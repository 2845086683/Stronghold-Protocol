// Include inside cn after report helpers. This observer never handles an
// exception: every path returns CONTINUE_SEARCH. Its binary records describe
// first-chance exceptions and are not evidence that an exception was fatal.
// install/shutdown serialize normal lifecycle work; scope(true) is authorized
// only by the UI caller after exact current private-scene identity validation.
#pragma once

constexpr unsigned NFC_MAX_RECORDS=8,NFC_STACK_WORDS=64,NFC_MAX_MODULES=8;
static_assert(std::atomic<bool>::is_always_lock_free&&std::atomic<unsigned>::is_always_lock_free&&std::atomic<DWORD>::is_always_lock_free,"fault observer atomics must not acquire runtime locks");
struct NFCModule { uint64_t base,size;uint32_t kind,reserved; };
struct NFCHeader {
    char magic[16],version[16],meaning[32];uint32_t schema,header_bytes,record_bytes,pid;
    uint64_t process_filetime,capture_filetime;uint32_t max_records,stack_words,module_count,context_bytes;
    NFCModule modules[NFC_MAX_MODULES];
};
struct NFCRecord {
    char magic[8];uint32_t bytes,sequence,first_chance,fatal_confirmed;
    uint32_t pid,tid,code,flags,parameter_count,context_read_ok,stack_read_bytes,stack_address_count;
    uint64_t exception_address,exception_parameters[EXCEPTION_MAXIMUM_PARAMETERS];
    // Register values and module-relative candidate return addresses only.
    // No managed strings, request bodies, account data or raw stack bytes.
    CONTEXT context;uint64_t stack_addresses[NFC_STACK_WORDS];
};
std::mutex nfc_lifecycle_mutex;
std::atomic<bool> nfc_accepting{false},nfc_scoped{false},nfc_stopping{false};
// A zero owner is idle. Win32 thread IDs are nonzero. This single lock-free
// gate covers recursion and concurrency before any handler memory/file access;
// it never needs per-thread TLS expansion or first-use heap allocation.
std::atomic<DWORD> nfc_owner_thread{0};
std::atomic<DWORD> nfc_transport_thread{0};
void native_fault_capture_transport_scope(bool entered){nfc_transport_thread.store(entered?GetCurrentThreadId():0,std::memory_order_release);}
std::atomic<unsigned> nfc_inflight{0},nfc_record_count{0},nfc_written{0},nfc_error{0};
HANDLE nfc_file=INVALID_HANDLE_VALUE,nfc_process=nullptr;PVOID nfc_registration=nullptr;
NFCHeader nfc_header{};NFCRecord nfc_record{};
uint64_t nfc_stack_scratch[NFC_STACK_WORDS]{};std::wstring nfc_file_path;

bool nfc_candidate_code(DWORD code){
    return code==EXCEPTION_ACCESS_VIOLATION||code==EXCEPTION_IN_PAGE_ERROR||code==EXCEPTION_ILLEGAL_INSTRUCTION||
        code==EXCEPTION_PRIV_INSTRUCTION||code==EXCEPTION_INT_DIVIDE_BY_ZERO||code==EXCEPTION_INT_OVERFLOW;
}
bool nfc_module_address(uint64_t address){
    for(unsigned i=0;i<nfc_header.module_count;++i){const auto& m=nfc_header.modules[i];if(address>=m.base&&address-m.base<m.size)return true;}
    return false;
}
struct NFCCallback {
    DWORD saved_error=GetLastError(),thread=GetCurrentThreadId();bool entered=false,owner_owned=false;
    NFCCallback(){
        DWORD expected=0;
        if(!thread||!nfc_owner_thread.compare_exchange_strong(expected,thread,std::memory_order_acq_rel))return;
        owner_owned=true;
        if(!nfc_accepting.load(std::memory_order_acquire))return;
        nfc_inflight.fetch_add(1,std::memory_order_acq_rel);
        if(nfc_accepting.load(std::memory_order_acquire))entered=true;
        else nfc_inflight.fetch_sub(1,std::memory_order_release);
    }
    ~NFCCallback(){
        if(entered)nfc_inflight.fetch_sub(1,std::memory_order_release);
        if(owner_owned)nfc_owner_thread.store(0,std::memory_order_release);
        SetLastError(saved_error);
    }
};
LONG CALLBACK nfc_handler(EXCEPTION_POINTERS* supplied){
    NFCCallback callback;if(!callback.entered)return EXCEPTION_CONTINUE_SEARCH;
    RuntimeLease lease(runtime_gate);
    const bool scoped=nfc_scoped.load(std::memory_order_acquire)||nfc_transport_thread.load(std::memory_order_acquire)==callback.thread;
    if(!lease||closing.load()||!active.load()||!scoped)return EXCEPTION_CONTINUE_SEARCH;
    EXCEPTION_POINTERS pointers{};EXCEPTION_RECORD fault{};SIZE_T copied=0;
    if(!supplied||!ReadProcessMemory(nfc_process,supplied,&pointers,sizeof(pointers),&copied)||copied!=sizeof(pointers)||
       !pointers.ExceptionRecord||!pointers.ContextRecord||
       !ReadProcessMemory(nfc_process,pointers.ExceptionRecord,&fault,sizeof(fault),&copied)||copied!=sizeof(fault)||
       !nfc_candidate_code(fault.ExceptionCode))return EXCEPTION_CONTINUE_SEARCH;
    // A simultaneous observer write is dropped, never waited for. All buffers
    // are preallocated, so no handler path needs a heap, managed API or mutex.
    if(nfc_record_count.load()>=NFC_MAX_RECORDS)return EXCEPTION_CONTINUE_SEARCH;
    std::memset(&nfc_record,0,sizeof(nfc_record));
    std::memcpy(nfc_record.magic,"NFCREC1",7);nfc_record.bytes=sizeof(nfc_record);
    nfc_record.sequence=nfc_record_count.fetch_add(1)+1;nfc_record.first_chance=1;nfc_record.fatal_confirmed=0;
    nfc_record.pid=nfc_header.pid;nfc_record.tid=callback.thread;nfc_record.code=fault.ExceptionCode;nfc_record.flags=fault.ExceptionFlags;
    nfc_record.exception_address=reinterpret_cast<uintptr_t>(fault.ExceptionAddress);
    nfc_record.parameter_count=fault.NumberParameters<EXCEPTION_MAXIMUM_PARAMETERS?fault.NumberParameters:EXCEPTION_MAXIMUM_PARAMETERS;
    for(unsigned i=0;i<nfc_record.parameter_count;++i)nfc_record.exception_parameters[i]=fault.ExceptionInformation[i];
    copied=0;
    if(ReadProcessMemory(nfc_process,pointers.ContextRecord,&nfc_record.context,sizeof(CONTEXT),&copied)&&copied==sizeof(CONTEXT)){
        nfc_record.context_read_ok=1;copied=0;
        // Bound the read to 512 bytes. Persist only addresses inside cached
        // code images; arbitrary stack contents are never written to disk.
        const BOOL ok=ReadProcessMemory(nfc_process,reinterpret_cast<const void*>(nfc_record.context.Rsp),nfc_stack_scratch,sizeof(nfc_stack_scratch),&copied);
        if(ok||copied){
            if(copied>sizeof(nfc_stack_scratch))copied=sizeof(nfc_stack_scratch);
            nfc_record.stack_read_bytes=static_cast<uint32_t>(copied);
            for(unsigned i=0;i<copied/sizeof(uint64_t);++i)if(nfc_module_address(nfc_stack_scratch[i]))
                nfc_record.stack_addresses[nfc_record.stack_address_count++]=nfc_stack_scratch[i];
        }
    }
    if(!closing.load()&&active.load()&&nfc_accepting.load(std::memory_order_acquire)&&
       (nfc_scoped.load(std::memory_order_acquire)||nfc_transport_thread.load(std::memory_order_acquire)==callback.thread)){
        DWORD written=0;if(!WriteFile(nfc_file,&nfc_record,sizeof(nfc_record),&written,nullptr)||written!=sizeof(nfc_record))nfc_error.store(4);else ++nfc_written;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
void nfc_cache_module(HMODULE image,uint32_t kind){
    if(!image||nfc_header.module_count>=NFC_MAX_MODULES)return;
    for(unsigned i=0;i<nfc_header.module_count;++i)if(nfc_header.modules[i].base==reinterpret_cast<uintptr_t>(image))return;
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),image,&dos,sizeof(dos),&copied)||copied!=sizeof(dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0||dos.e_lfanew>0x100000)return;
    const auto at=reinterpret_cast<const unsigned char*>(image)+dos.e_lfanew;
    if(!ReadProcessMemory(GetCurrentProcess(),at,&nt,sizeof(nt),&copied)||copied!=sizeof(nt)||nt.Signature!=IMAGE_NT_SIGNATURE||
       nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||!nt.OptionalHeader.SizeOfImage)return;
    nfc_header.modules[nfc_header.module_count++]={reinterpret_cast<uintptr_t>(image),nt.OptionalHeader.SizeOfImage,kind,0};
}
void native_fault_capture_install(){
    std::lock_guard<std::mutex> lock(nfc_lifecycle_mutex);
    if(nfc_stopping.load()||closing.load()||nfc_registration)return;
    wchar_t base[32768]{};const DWORD used=GetEnvironmentVariableW(L"LOCALAPPDATA",base,32768);
    if(!used||used>=32768){nfc_error.store(2);return;}
    const auto dest=std::wstring(base)+L"\\StrongholdProtocol\\captures\\CN-native";
    for(size_t i=3;i<dest.size();++i)if(dest[i]==L'\\')CreateDirectoryW(dest.substr(0,i).c_str(),nullptr);CreateDirectoryW(dest.c_str(),nullptr);
    FILETIME now{},created{},exited{},kernel{},user{};GetSystemTimeAsFileTime(&now);GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);
    const uint64_t stamp=(uint64_t(now.dwHighDateTime)<<32)|now.dwLowDateTime;
    nfc_file_path=dest+L"\\native-first-chance-0.1.115-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(stamp)+L".bin";
    nfc_file=CreateFileW(nfc_file_path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(nfc_file==INVALID_HANDLE_VALUE){nfc_error.store(2);return;}
    std::memset(&nfc_header,0,sizeof(nfc_header));std::memcpy(nfc_header.magic,"CN-NATIVE-FAULT",15);std::memcpy(nfc_header.version,"0.1.115",8);
    std::memcpy(nfc_header.meaning,"FIRST_CHANCE_NOT_FATAL",sizeof("FIRST_CHANCE_NOT_FATAL"));nfc_header.schema=1;nfc_header.header_bytes=sizeof(nfc_header);nfc_header.record_bytes=sizeof(nfc_record);nfc_header.pid=GetCurrentProcessId();
    nfc_header.capture_filetime=stamp;nfc_header.process_filetime=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
    nfc_header.max_records=NFC_MAX_RECORDS;nfc_header.stack_words=NFC_STACK_WORDS;nfc_header.context_bytes=sizeof(CONTEXT);
    nfc_cache_module(GetModuleHandleW(nullptr),1);nfc_cache_module(runtime,2);nfc_cache_module(GetModuleHandleW(L"UnityPlayer.dll"),3);nfc_cache_module(module,4);
    nfc_cache_module(GetModuleHandleW(L"ntdll.dll"),5);nfc_cache_module(GetModuleHandleW(L"kernel32.dll"),6);nfc_cache_module(GetModuleHandleW(L"KernelBase.dll"),7);
    DWORD written=0;
    if(!WriteFile(nfc_file,&nfc_header,sizeof(nfc_header),&written,nullptr)||written!=sizeof(nfc_header)||nfc_stopping.load()||closing.load()){
        CloseHandle(nfc_file);nfc_file=INVALID_HANDLE_VALUE;nfc_error.store(2);return;
    }
    nfc_process=GetCurrentProcess();nfc_registration=AddVectoredExceptionHandler(1,nfc_handler);
    if(!nfc_registration){CloseHandle(nfc_file);nfc_file=INVALID_HANDLE_VALUE;nfc_error.store(3);return;}
    if(nfc_stopping.load()||closing.load()){
        RemoveVectoredExceptionHandler(nfc_registration);nfc_registration=nullptr;CloseHandle(nfc_file);nfc_file=INVALID_HANDLE_VALUE;
        return;
    }
    nfc_accepting.store(true,std::memory_order_release);
    // shutdown sets stopping before waiting for our lifecycle mutex. Recheck
    // after publication as well so its earlier revoke cannot be overwritten.
    if(nfc_stopping.load()||closing.load()){
        nfc_accepting.store(false,std::memory_order_release);RemoveVectoredExceptionHandler(nfc_registration);nfc_registration=nullptr;
        while(nfc_inflight.load(std::memory_order_acquire))Sleep(0);
        CloseHandle(nfc_file);nfc_file=INVALID_HANDLE_VALUE;
    }
}
void native_fault_capture_scope(bool exact_private_scene){
    nfc_scoped.store(exact_private_scene&&!closing.load()&&active.load()&&!nfc_stopping.load()&&nfc_accepting.load(std::memory_order_acquire),std::memory_order_release);
}
void native_fault_capture_shutdown(){
    // Host first revokes scope, closes/drains runtime_gate and waits for its
    // setup worker. No handler takes this mutex; draining owns no RuntimeLease.
    nfc_stopping.store(true);nfc_scoped.store(false,std::memory_order_release);nfc_accepting.store(false,std::memory_order_release);
    std::lock_guard<std::mutex> lock(nfc_lifecycle_mutex);
    if(nfc_registration){RemoveVectoredExceptionHandler(nfc_registration);nfc_registration=nullptr;}
    while(nfc_inflight.load(std::memory_order_acquire))Sleep(0);
    if(nfc_file!=INVALID_HANDLE_VALUE){CloseHandle(nfc_file);nfc_file=INVALID_HANDLE_VALUE;}
}
// Normal report thread only. Never called by the exception handler.
std::string native_fault_capture_status_json(){
    std::unique_lock<std::mutex> lock(nfc_lifecycle_mutex,std::try_to_lock);
    if(!lock.owns_lock())return "{\"busy\":true}";
    const bool installed=nfc_registration&&nfc_accepting.load(std::memory_order_acquire);
    const bool armed=installed&&(nfc_scoped.load(std::memory_order_acquire)||nfc_transport_thread.load(std::memory_order_acquire))&&active.load()&&!closing.load()&&!nfc_stopping.load();
    return "{\"installed\":"+std::string(installed?"true":"false")+",\"armed\":"+(armed?"true":"false")+
        ",\"records\":"+std::to_string(nfc_written.load())+",\"recordAttempts\":"+std::to_string(nfc_record_count.load())+
        ",\"maxRecords\":8,\"classification\":\"first-chance-not-fatal\",\"errorCode\":"+std::to_string(nfc_error.load())+",\"path\":"+quote(utf8(nfc_file_path))+"}";
}
