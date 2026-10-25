// Research-only candidate, included in namespace cn after help and prep view API.
// Original IL2CPP instance ABI: bool(self, handler, MethodInfo*).
#pragma once

static const CN_Method NMPO_OPERATION={
    "Torappu.Battle.AutoChess.AutoChessPlayerOpManager","_DoOperation",
    0x6013829,0xb69ba0ULL,1,false,"System.Boolean",
    {"Torappu.Battle.AutoChess.AutoChessGameModeTileOperationHandlerBase"}};

bool native_multiplayer_preparation_view_may_write_board();
using NMPOOperation=bool(*)(void*,void*,const void*);
NMPOOperation nmpo_original_operation=nullptr;

bool nmpo_operation(void* self,void* handler,const void* info){
    RuntimeLease lease(runtime_gate);
    // Do not inspect managed objects once the runtime gate has closed.
    if(!lease||closing.load())return false;
    bool private_entry=false;
    try{
        private_entry=native_multiplayer_entry_active();
        if(private_entry&&nmh_scope()){
            auto manager=nb_manager();
            if(self&&manager&&
               api.il2cpp_object_get_class(self)==cls("Torappu.Battle.AutoChess.AutoChessPlayerOpManager")&&
               field_object(manager,"playerOpManager")==self&&
               !native_multiplayer_preparation_view_may_write_board())return false;
        }
    }catch(const std::exception& ex){
        nb_error("multiplayer-preparation-operation",ex);
        if(private_entry)return false;
    }
    // Keep the protected original's managed exception / return semantics.
    lease.release();return nmpo_original_operation(self,handler,info);
}

void native_multiplayer_preparation_view_operation_setup(){
    // Create only. The existing setup(enable) owns global hook activation.
    hook(NMPO_OPERATION,nmpo_operation,nmpo_original_operation);
}
