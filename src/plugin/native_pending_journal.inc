// Included inside namespace cn, before native_battle_data.inc. Files are local
// bounded JSON only. All business/request validation lives at the caller seam.
#pragma once
std::atomic<unsigned long> npj_temporary_sequence{0};
const char* native_pending_game_assembly_hash(){
    for(const auto& file:CN_FILES)if(std::wcscmp(file.path,L"GameAssembly.dll")==0)return file.hash;
    throw std::runtime_error("private journal game assembly pin unavailable");
}
struct NPJ_Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit NPJ_Handle(HANDLE v):value(v){}
    ~NPJ_Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    NPJ_Handle(const NPJ_Handle&)=delete;NPJ_Handle& operator=(const NPJ_Handle&)=delete;
};
void npj_check_directory(const std::wstring& path,bool create){
    if(create&&!CreateDirectoryW(path.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)throw std::runtime_error("private journal directory unavailable");
    DWORD flags=GetFileAttributesW(path.c_str());
    if(flags==INVALID_FILE_ATTRIBUTES||!(flags&FILE_ATTRIBUTE_DIRECTORY)||(flags&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("private journal directory is missing or redirected");
}
std::wstring npj_root(bool create){
    wchar_t base[32768]{};DWORD count=GetEnvironmentVariableW(L"LOCALAPPDATA",base,32768);
    if(!count||count>=32768)throw std::runtime_error("private journal LOCALAPPDATA unavailable");
    std::wstring root(base,count);if(root.size()<3||root[1]!=L':'||(root[2]!=L'\\'&&root[2]!=L'/')||root.find(L"..")!=std::wstring::npos)throw std::runtime_error("private journal base path invalid");
    // The base itself is also checked: no junction/symlink may redirect the
    // dedicated owned subtree to a game file or an unrelated directory.
    npj_check_directory(root,false);
    for(const wchar_t* part:{L"StrongholdProtocol",L"state",L"CN-native",L"pending"}){root+=L"\\";root+=part;
        if(!create&&GetFileAttributesW(root.c_str())==INVALID_FILE_ATTRIBUTES){auto error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND)return L"";}
        npj_check_directory(root,create);
    }return root;
}
std::wstring npj_path(const char* kind,const std::string& run,bool create){
    if(!kind||!cnpj::kind(kind)||!cnpj::run_id(run))throw std::runtime_error("private journal file identity invalid");
    auto root=npj_root(create);if(root.empty())return root;std::string name=std::string(kind)+"-"+run+"-pending.json";return root+L"\\"+std::wstring(name.begin(),name.end());
}
std::string native_pending_read(const char* kind,const std::string& run){
    auto path=npj_path(kind,run,false);if(path.empty())return {};
    NPJ_Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr));
    if(file.value==INVALID_HANDLE_VALUE){auto error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND)return {};throw std::runtime_error("private journal read unavailable");}
    BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};
    if(!GetFileInformationByHandle(file.value,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))||!GetFileSizeEx(file.value,&size)||size.QuadPart<1||size.QuadPart>static_cast<LONGLONG>(cnpj::max_bytes))throw std::runtime_error("private journal file type or size invalid");
    std::string bytes(std::size_t(size.QuadPart),'\0');DWORD used=0;
    if(!ReadFile(file.value,bytes.data(),DWORD(bytes.size()),&used,nullptr)||used!=bytes.size())throw std::runtime_error("private journal read incomplete");return bytes;
}
void native_pending_write(const char* kind,const std::string& run,const std::string& bytes){
    if(bytes.empty()||bytes.size()>cnpj::max_bytes)throw std::runtime_error("private journal write size invalid");
    cnjson::parse(bytes);auto path=npj_path(kind,run,true);
    DWORD prior=GetFileAttributesW(path.c_str());if(prior!=INVALID_FILE_ATTRIBUTES&&(prior&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))throw std::runtime_error("private journal target redirected");
    auto temp=path+L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++npj_temporary_sequence);
    bool complete=false;{
        NPJ_Handle file(CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));
        if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("private journal temporary file unavailable");
        DWORD written=0;complete=WriteFile(file.value,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file.value);
    }
    if(!complete){DeleteFileW(temp.c_str());throw std::runtime_error("private journal durable write failed");}
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temp.c_str());throw std::runtime_error("private journal atomic commit failed");}
}
void native_pending_erase(const char* kind,const std::string& run,const std::string& expected_bytes){
    if(expected_bytes.empty())throw std::runtime_error("private journal erase requires captured content");
    auto current=native_pending_read(kind,run);if(current.empty())return;
    if(current!=expected_bytes)throw std::runtime_error("private journal changed before acknowledgement");
    auto path=npj_path(kind,run,false);if(path.empty()||!DeleteFileW(path.c_str()))throw std::runtime_error("private journal acknowledged file removal failed");
}
