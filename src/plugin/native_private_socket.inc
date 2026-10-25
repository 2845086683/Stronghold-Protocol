// CN Lua hotfixes can call Server.SendMsg without ServiceMode.SendRequest.
// Retain native request/DTO objects and use the same private dispatchers at the
// last send boundary. nmd_server_scope verifies thread, activity, mode and the
// exact team/battle server object; every other server keeps its original body.
#pragma once
inline constexpr CN_Method NPS_PROTOCOL={"Torappu.SocketNetwork.ServerBase.Server","SendMsg",0x600889c,0x2b5e2c0ULL,1,false,"System.Void",{"Torappu.SocketNetwork.ServerBase.Protocol"}};
inline constexpr CN_Method NPS_REQUEST={"Torappu.SocketNetwork.ServerBase.Server","SendMsg",0x600889e,0x2b5e410ULL,1,false,"System.Void",{"Torappu.SocketNetwork.Msg.RequestHandler"}};
using NPS_Send=void(*)(void*,void*,const void*);
NPS_Send nps_old_protocol=nullptr,nps_old_request=nullptr;
std::atomic<unsigned> nps_protocols{0},nps_requests{0};
void nps_protocol(void* server,void* protocol,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!nmd_server_scope(server)){lease.release();nps_old_protocol(server,protocol,info);return;}
    try{
        nb_main_thread();if(!protocol)throw std::runtime_error("private socket protocol is null");
        ++nps_protocols;
        if(native_multiplayer_entry_request(protocol)||native_multiplayer_dispatch_request(native_multiplayer_entry_mode(),protocol))return;
        throw std::runtime_error("private socket protocol has no connected handler");
    }catch(const std::exception& error){nmd_failed(error);}
}
void nps_request(void* server,void* request,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!nmd_server_scope(server)){lease.release();nps_old_request(server,request,info);return;}
    ++nps_requests;nmd_request(native_multiplayer_entry_mode(),request,info);
}
void native_private_socket_setup(){
    hook(NPS_PROTOCOL,nps_protocol,nps_old_protocol);hook(NPS_REQUEST,nps_request,nps_old_request);
}
