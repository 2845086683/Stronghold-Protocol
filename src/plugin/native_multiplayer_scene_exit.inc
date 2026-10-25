// Native terminal consumption for an acknowledged private voluntary scene quit.
// The party remains on the server. This must not enter Server.Disconnect: the
// private BattleServer intentionally has no socket/m_net for it to disconnect.
#pragma once
bool native_multiplayer_entry_voluntary_exit(){
    if(!native_multiplayer_entry_active()||native_multi_entry_scene_exit_key.empty())return false;
    const auto origin=preparation_view::session(native_multi_entry_snapshot);auto seat=native_multi_entry_field(native_multi_entry_snapshot,"seat");
    if(!origin.valid()||origin.party||!seat||seat->kind!=cnjson::Value::Integer)return false;
    return native_multi_entry_scene_exit_key=="scene-quit:"+origin.room+":"+std::to_string(origin.match)+":"+std::to_string(seat->integer)+":"+std::to_string(native_multi_entry_service_epoch);
}
void native_multi_entry_scene_exit(const cnjson::Value& snapshot){
    native_battle_launch_require_ui();if(!native_multiplayer_entry_active())return;
    const auto found=native_multi_entry_field(snapshot,"nativeSceneExit");
    if(!found||found->kind!=cnjson::Value::Object)throw std::runtime_error("private scene quit ACK lacks its terminal contract");
    const auto& exit=*found;const auto origin=preparation_view::session(native_multi_entry_snapshot);
    const auto incoming=preparation_view::session(snapshot);const auto seat=native_multi_entry_field(snapshot,"seat"),reason=native_multi_entry_field(exit,"reason"),exitSeat=native_multi_entry_field(exit,"seat"),match=native_multi_entry_field(exit,"matchSequence");
    const auto kind=native_multi_entry_string(exit,"kind");
    if(!origin.valid()||origin.party||!(origin==incoming)||native_multi_entry_string(exit,"roomId")!=origin.room||(kind!="voluntary"&&kind!="completed")||(kind=="completed"&&native_multi_entry_string(snapshot,"phase")!="result")||!reason||reason->kind!=cnjson::Value::Integer||reason->integer!=7||!seat||seat->kind!=cnjson::Value::Integer||seat->integer<0||seat->integer>3||!exitSeat||exitSeat->kind!=cnjson::Value::Integer||exitSeat->integer!=seat->integer||!match||match->kind!=cnjson::Value::Integer||match->integer!=origin.match)
        throw std::runtime_error("private scene quit ACK belongs to another session or reason");
    const uint64_t epoch=native_multi_entry_service_epoch;
    const auto key="scene-quit:"+origin.room+":"+std::to_string(origin.match)+":"+std::to_string(seat->integer)+":"+std::to_string(epoch);
    if(native_multi_entry_scene_exit_key==key)return;
    if(!native_multi_entry_scene_quit_pending)throw std::runtime_error("private scene quit was not requested by this native owner");
    auto mode=native_multi_entry_retain(native_multiplayer_entry_mode());auto server=native_multi_entry_retain(field_object(mode.get(),"m_battleSvr"));
    if(!mode||!server)throw std::runtime_error("private scene quit battle server owner missing");
    auto done=[origin,epoch,mode,server](){
        if(!native_multiplayer_entry_active()||epoch!=native_multi_entry_service_epoch||!(preparation_view::session(native_multi_entry_snapshot)==origin)||native_multiplayer_entry_mode()!=mode.get()||field_object(mode.get(),"m_battleSvr")!=server.get())throw std::runtime_error("private scene quit completion lost its original native owner");
        // Pinned CN control flow: _HandlePlayerKicked sets SERVER_KICK=3 and
        // reason, then its synchronous OnConnectionLost(0) boxes m_endInfo,
        // emits SCENE_LOST=10 and resets native battle data. Invoke that exact
        // terminal chain without the unused socket transport in between.
        PrivateDataScope private_scope(true);NB_ProjectionScope projection_scope;
        int32_t exception=3,reason=7,lost=0;void* set_args[]={&exception,&reason};
        invoke(bound_methods.at(CN_MULTI_ENTRY_BATTLE_EXCEPTION.token),server.get(),set_args);
        void* lost_args[]={&lost};invoke(bound_methods.at(CN_MULTI_ENTRY_BATTLE_LOST.token),server.get(),lost_args);
    };
    native_multi_entry_scene_exit_key=key;native_multi_entry_scene_quit_pending=false;
    // Stop future polling as soon as the terminal ACK is accepted, before a
    // possibly asynchronous original scene retirement drains its last task.
    native_multiplayer_transport_detach(origin);
    if(!native_multiplayer_preparation_view_retire(origin,std::move(done),key)){native_multi_entry_scene_exit_key.clear();throw std::runtime_error("private scene quit lacks its retiring original scene owner");}
}
