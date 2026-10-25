// The pinned CN _AppendBattlePlayerRunes accepts seat zero on its all-player
// path, but uses owner > 0 on its partial-player path (ba114d..ba114f).
// Adapt that zero-based private-seat mismatch once per original holder rebuild.
// Native factory, side mapping and _AddPackedRuneData retain all Rune behavior.
#pragma once
unsigned nmmr_patch_zero_owner_runes(void* self,TemporaryRoots& roots){
    auto centre=roots.retain(nb_centre());auto players=roots.retain(field_object(centre,"m_battlePlayerIndex"));
    if(!players)return 0;const auto count=nmo_count(players);if(count<2)return 0;
    if(count>2)throw std::runtime_error("multiplayer Rune field has more than two active defenders");
    std::set<int32_t> active;
    for(int32_t i=0;i<count;++i){auto player=nmh_result<int32_t>(nmo_item(players,i));if(player<0||player>3||!active.insert(player).second)throw std::runtime_error("multiplayer Rune active seat invalid");}
    if(!active.count(0))return 0;
    auto effects=roots.retain(field_object(centre,"<battleSpecialEffects>k__BackingField"));if(!effects)return 0;
    auto right=roots.retain(field_object(centre,"m_rightMapPlayerIndex"));if(!right)throw std::runtime_error("multiplayer Rune native right-field owners missing");
    auto contains=[&](void* set,int32_t seat){void* args[]={&seat};return nmh_result<uint8_t>(invoke(method(api.il2cpp_object_get_class(set),"Contains",1),set,args))!=0;};
    std::map<int32_t,std::string> applied;std::set<void*> fresh;unsigned added=0;
    for(int32_t i=0,total=nmo_count(effects);i<total;++i){
        auto effect=roots.retain(nmo_item(effects,i));if(!effect||api.il2cpp_object_get_class(effect)!=cls("Torappu.Battle.AutoChess.BattleSpecialEffect"))throw std::runtime_error("multiplayer Rune special effect class differs");
        auto targets=roots.retain(field_object(effect,"affectedUidIndex"));if(!targets)throw std::runtime_error("multiplayer Rune special effect targets missing");
        if(!contains(targets,0))continue;bool all=true;for(auto seat:active)if(!contains(targets,seat)){all=false;break;}
        if(all)continue; // The original all-covered branch already includes seat zero.
        int32_t instance=0;api.il2cpp_field_get_value(effect,profile_field(api.il2cpp_object_get_class(effect),"effectInstId"),&instance);
        const auto id=text(field_object(effect,"effectId"));if(instance<0||id.empty())throw std::runtime_error("multiplayer Rune special effect identity missing");
        auto prior=applied.find(instance);if(prior!=applied.end()){if(prior->second!=id)throw std::runtime_error("multiplayer Rune effect instance conflicts");continue;}applied.emplace(instance,id);
        auto key=roots.retain(string_new(id.c_str()));void* factory[]={key};auto pack=roots.retain(invoke(nb_method(0x60138ca),self,factory));
        if(!pack)continue; // Original factory returns null for non-Rune effects.
        if(api.il2cpp_object_get_class(pack)!=cls("Torappu.RuneTable+PackedRuneData")||text(field_object(pack,"id"))!=id||!fresh.insert(pack).second)throw std::runtime_error("multiplayer Rune factory did not return a fresh matching pack");
        uint8_t isRight=contains(right,0)?1:0;void* sideArgs[]={&isRight};auto side=nmh_result<int32_t>(invoke(nb_method(0x601353e),nullptr,sideArgs));
        int32_t owner=0;void* add[]={pack,&side,&owner};invoke(nb_method(0x60138cd),self,add);++added;
    }
    return added;
}
