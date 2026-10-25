// The original HTTP success continuation reads LevelData before publishing
// BattleInOut.input. Validate its still-live native Cache, not the prior input.
bool native_multiplayer_pending_level_scope(){
    TemporaryRoots roots;
    auto controller=cls("Torappu.BattleStartController"),cache_class=cls("Torappu.BattleStartController+Cache");
    auto field=profile_field(controller,"m_cache");uint32_t alignment=0;
    if(!(api.il2cpp_field_get_flags(field)&16)||!matches_type(api.il2cpp_field_get_type(field),"Torappu.BattleStartController+Cache")||
       !api.il2cpp_class_is_valuetype(cache_class)||api.il2cpp_class_value_size(cache_class,&alignment)!=704)
        throw std::runtime_error("private multiplayer start cache ABI mismatch");
    std::array<uint8_t,704> raw{};api.il2cpp_field_static_get_value(field,raw.data());
    auto cache=roots.retain(value_box(cache_class,raw.data()));if(!field_object(cache,"handler"))return false;
    auto param=native_battle_boxed_field(cache,"param",roots);
    auto config=roots.retain(field_object(param,"customizeStartService"));
    if(!config||api.il2cpp_object_get_class(config)!=cls("Torappu.UI.AutoChess.AutoChessStartMultiBattleServiceConfig"))return false;
    const auto snapshot=native_multiplayer_transport_snapshot();
    const auto room=cnmp::text(snapshot,"roomId");
    if(room.empty()||room!=native_multi_entry_room||!native_multiplayer_phase_can_launch(cnmp::text(snapshot,"phase"))||
       text(field_object(config,"m_activityId"))!="act2autochess"||text(field_object(config,"m_sceneId"))!=room)return false;
    auto meta=native_battle_boxed_field(param,"actMeta",roots);
    if(text(field_object(meta,"activityId"))!="act2autochess")return false;
    const auto stage=text(field_object(param,"stageId"));
    return stage==cnmp::text(cnmp::at(snapshot,"selfState"),"stageId")||native_multiplayer_preparation_view_allows_initial_stage(snapshot,stage);
}
