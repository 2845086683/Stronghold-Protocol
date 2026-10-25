// Policy header goes before namespace cn; include this after data.inc. The
// projection-depth predicate is supplied by the private data bridge. No native
// DataCenter or official service object is created or mutated here.
#pragma once
bool native_battle_data_projecting();

inline constexpr CN_Method NPI_CONVERT_ID={"Torappu.Battle.AutoChess.DataConverter","GetChessIdByIdentifier",0x6013678,0xb37160ULL,1,true,"System.String",{"System.Int32"}};
inline constexpr CN_Method NPI_ACT_DATA={"Torappu.UI.AutoChess.AutoChessUtil","GetActAutoChessData",0x602b516,0x2761300ULL,1,true,"Torappu.ActAutoChessData",{"System.String"}};
using NPIConvert=void*(*)(int32_t,const void*);NPIConvert npi_old_convert=nullptr;
std::mutex npi_mutex;
cnidentifier::Index npi_index;
std::string npi_last_error;
std::atomic<unsigned> npi_reads{0},npi_failures{0},npi_builds{0};
std::atomic<bool> npi_setup{false};
thread_local std::string npi_projection_error;
thread_local bool npi_building=false;

// A nested Fill must not erase an unresolved identifier from its outer Fill.
// Completion consumes the latch when it rejects the typed snapshot, allowing a
// subsequent explicit conversion retry to begin cleanly.
void native_private_identifier_reset_projection_error(){}
void native_private_identifier_require_projection_success(){if(!npi_projection_error.empty()){auto error=std::move(npi_projection_error);npi_projection_error.clear();throw std::runtime_error(error);}}
void npi_scan(void* table,cnidentifier::Index& index,TemporaryRoots& roots){
    if(!table)throw std::runtime_error("private chess identifier table missing");
    auto c=api.il2cpp_object_get_class(table);int32_t count=0;
    if(!read(unbox(invoke(method(c,"get_Count",0),table)),count)||count<1||count>4096)throw std::runtime_error("private chess identifier table size invalid");
    auto keys=roots.retain(invoke(method(c,"get_Keys",0),table));
    auto iterator=roots.retain(invoke(method(api.il2cpp_object_get_class(keys),"GetEnumerator",0),keys));
    if(!iterator)throw std::runtime_error("private chess identifier iterator missing");
    auto ic=api.il2cpp_object_get_class(iterator);auto move=method(ic,"MoveNext",0),current=method(ic,"get_Current",0),item=method(c,"get_Item",1);
    for(int32_t position=0;position<count;++position){
        uint8_t next=0;if(!read(unbox(invoke(move,instance_argument(iterator))),next)||!next)throw std::runtime_error("private chess identifier iterator ended early");
        auto key=roots.retain(invoke(current,instance_argument(iterator)));void* args[]={key};
        auto row=roots.retain(invoke(item,table,args));if(!row)throw std::runtime_error("private chess identifier row missing");
        int32_t identifier=0;api.il2cpp_field_get_value(row,profile_field(api.il2cpp_object_get_class(row),"identifier"),&identifier);
        index.add(identifier,text(field_object(row,"chessId")),text(key));
    }
}
std::string npi_lookup(int32_t identifier){
    if(npi_building)throw std::runtime_error("recursive private chess identifier construction");
    struct Guard{Guard(){npi_building=true;}~Guard(){npi_building=false;}} guard;
    TemporaryRoots roots;void* args[]={roots.retain(string_new("act2autochess"))};
    auto data=roots.retain(invoke(bound_methods.at(NPI_ACT_DATA.token),nullptr,args));
    if(!data)throw std::runtime_error("private act2 chess definitions unavailable");
    auto source=nb_retained("private-identifier-act-data");
    if(source!=data){
        cnidentifier::Index fresh;npi_scan(field_object(data,"charChessDataDict"),fresh,roots);npi_scan(field_object(data,"trapChessDataDict"),fresh,roots);
        // Both native definition maps were decoded before publishing any new
        // reverse lookup. A malformed/duplicate identifier cannot leave a
        // partially constructed map available to the next converter request.
        protect("private-identifier-act-data",data);
        {std::lock_guard<std::mutex> lock(npi_mutex);npi_index=std::move(fresh);++npi_builds;}
    }
    std::lock_guard<std::mutex> lock(npi_mutex);return npi_index.find(identifier);
}
void* npi_convert(int32_t identifier,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;
    const bool projecting=native_battle_data_projecting();
    const bool on_ui=can_read()&&unity_ui_thread.load()==GetCurrentThreadId();
    if(!projecting||!private_data_depth){lease.release();return npi_old_convert(identifier,method_info);}
    try{
        if(!cnidentifier::scope(projecting,private_data_depth,on_ui))throw std::runtime_error("private chess identifier conversion outside observed Unity UI thread");
        if(!npi_setup.load())throw std::runtime_error("private chess identifier setup missing");
        auto result=npi_lookup(identifier);++npi_reads;return string_new(result.c_str());
    }catch(const std::exception& ex){
        ++npi_failures;if(npi_projection_error.empty())npi_projection_error=ex.what();
        {std::lock_guard<std::mutex> lock(npi_mutex);npi_last_error=ex.what();}
        // Original converters treat null as unresolved and may silently skip
        // the item. nb_fill must call require_projection_success after invoke
        // so a missing private definition rejects the entire typed snapshot.
        return nullptr;
    }
}
std::string native_private_identifier_status_json(){
    std::unique_lock<std::mutex> lock(npi_mutex,std::try_to_lock);if(!lock.owns_lock())return "{\"busy\":true}";
    return "{\"setup\":"+std::string(npi_setup.load()?"true":"false")+",\"entries\":"+std::to_string(npi_index.size())+",\"reads\":"+std::to_string(npi_reads.load())+
        ",\"builds\":"+std::to_string(npi_builds.load())+",\"failures\":"+std::to_string(npi_failures.load())+",\"lastError\":"+quote(npi_last_error)+"}";
}
void native_private_identifier_setup(){
    hook(NPI_CONVERT_ID,npi_convert,npi_old_convert);bind_method(NPI_ACT_DATA);
    auto data=cls("Torappu.ActAutoChessData");if(!data)throw std::runtime_error("private chess identifier data class missing");
    const char* fields[]={"charChessDataDict","trapChessDataDict"};
    const char* types[]={"System.Collections.Generic.Dictionary`2<System.String,Torappu.ActAutoChessData+ActAutoChessCharChessData>","System.Collections.Generic.Dictionary`2<System.String,Torappu.ActAutoChessData+ActAutoChessTrapChessData>"};
    const size_t offsets[]={40,88};
    for(size_t i=0;i<2;++i){auto field=profile_field(data,fields[i]);if(api.il2cpp_field_get_offset(field)!=offsets[i]||!matches_type(api.il2cpp_field_get_type(field),types[i]))throw std::runtime_error("private chess identifier dictionary contract mismatch");}
    for(const char* type:{"Torappu.ActAutoChessData+ActAutoChessCharChessData","Torappu.ActAutoChessData+ActAutoChessTrapChessData"}){
        auto c=cls(type);if(!c)throw std::runtime_error("private chess identifier row class missing");
        auto id=profile_field(c,"identifier"),key=profile_field(c,"chessId");
        if(api.il2cpp_field_get_offset(id)!=24||!matches_type(api.il2cpp_field_get_type(id),"System.Int32")||api.il2cpp_field_get_offset(key)!=16||!matches_type(api.il2cpp_field_get_type(key),"System.String"))throw std::runtime_error("private chess identifier row contract mismatch");
    }
    npi_setup.store(true);
}
