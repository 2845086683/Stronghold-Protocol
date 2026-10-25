// Private preparation-only target restrictions. All native drag/replace,
// animation, inventory and battle execution remain in the original engine.
// Include after native_preparation_tokens.inc and native_shop_free_refresh.inc.
#pragma once
inline constexpr CN_Method NITR_BUILDABLE={"Torappu.Battle.UI.AutoChessDragState","_CheckDummyBuildable",0x6018ee5,0x11eda00ULL,4,false,"System.Boolean",{"Torappu.Battle.Tile","Torappu.Battle.Tile","Torappu.Battle.AutoChess.ChessInst","Torappu.Battle.Character"}};
inline constexpr CN_Method NITR_EQUIP={"Torappu.Battle.UI.AutoChessDragState","_EquipCharacter",0x6018ee1,0x11eef90ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NITR_BUFFS={"Torappu.Battle.AutoChess.AutoChessBattleUtil","GetEquipEffectBuffs",0x60134f5,0xb272f0ULL,1,true,"System.Collections.Generic.List`1<Torappu.ActAutoChessData+ActAutoChessBuffInfoData>",{"System.String"}};
inline constexpr CN_Method NITR_NORMAL={"Torappu.Battle.AutoChess.AutoChessBattleUtil","GetChessNormalId",0x6013535,0xb26b00ULL,1,true,"System.String",{"System.String"}};
inline constexpr const char* NITR_TRANSFER_EFFECT="use_equip_recruit_new_char_and_give_char_to_player_most_bond";
using NativeItemBuildable=uint8_t(*)(void*,void*,void*,void*,void*,const void*);
using NativeItemEquip=void(*)(void*,const void*);
NativeItemBuildable nitr_old_buildable=nullptr;
NativeItemEquip nitr_old_equip=nullptr;
std::atomic<bool> nitr_setup_ready{false},nitr_failed{false};
std::atomic<unsigned> nitr_blocked_preview{0},nitr_blocked_drop{0},nitr_failures{0};

bool nitr_scene(void* drag){
    // npt_scene binds the pinned native manager/data/bridge to this activity;
    // nsfr_scene additionally proves the current, writable own preparation
    // board (including the MULTI room/match/seat/run presentation receipt).
    return npt_scene()&&nsfr_scene()&&drag&&api.il2cpp_object_get_class(drag)==cls(NITR_EQUIP.type);
}
void* nitr_field(void* object,const char* name,const char* type){
    if(!object)throw std::runtime_error("private item target object missing");
    auto field=profile_field(api.il2cpp_object_get_class(object),name);
    if((api.il2cpp_field_get_flags(field)&16)||!matches_type(api.il2cpp_field_get_type(field),type))
        throw std::runtime_error(std::string("private item target field contract: ")+name);
    return field;
}
int32_t nitr_kind(void* object){
    int32_t result=-1;api.il2cpp_field_get_value(object,nitr_field(object,"chessType","Torappu.AutoChessItemType"),&result);return result;
}
void* nitr_collection_method(void* collection,const char* name,int count,const char* result,const char* parameter=nullptr){
    auto selected=method(api.il2cpp_object_get_class(collection),name,count);uint32_t impl=0;
    if(!selected||(api.il2cpp_method_get_flags(selected,&impl)&16)||
       !matches_type(api.il2cpp_method_get_return_type(selected),result)||
       (parameter&&!matches_type(api.il2cpp_method_get_param(selected,0),parameter)))
        throw std::runtime_error("private item target collection contract");
    return selected;
}
bool nitr_forbidden(void* drag,void* tile,void* item,TemporaryRoots& roots){
    // Ordinary operator/magic placement is the hot path. Inspect only its
    // already-live native type before asking for any private snapshot proof.
    if(!tile||!item||api.il2cpp_object_get_class(item)!=cls("Torappu.Battle.AutoChess.ChessInst")||nitr_kind(item)!=1||!nitr_scene(drag))return false;
    roots.retain(drag);roots.retain(tile);roots.retain(item);
    auto position=roots.retain(invoke(nb_method(0x6013030),tile));
    if(!position)throw std::runtime_error("private item target position missing");
    void* target_args[]={unbox(position)};
    auto target=roots.retain(invoke(nb_method(0x60134fe),nullptr,target_args));
    if(!target||api.il2cpp_object_get_class(target)!=cls("Torappu.Battle.AutoChess.ChessInst")||nitr_kind(target)!=0)return false;
    nitr_field(target,"chessId","System.String");
    auto target_id=roots.retain(field_object(target,"chessId"));void* normal_args[]={target_id};
    auto normal=roots.retain(invoke(nb_method(NITR_NORMAL.token),nullptr,normal_args));
    if(!normal||text(normal).empty())throw std::runtime_error("private item target base chess missing");
    auto data=roots.retain(field_object(nb_centre(),"<actData>k__BackingField"));
    nitr_field(data,"diyChessDict","System.Collections.Generic.Dictionary`2<System.String,Torappu.RarityRank>");
    auto diy=roots.retain(field_object(data,"diyChessDict"));
    if(!diy)throw std::runtime_error("private item target DIY dictionary missing");
    void* diy_args[]={normal};uint8_t is_diy=0;
    auto boxed=roots.retain(invoke(nitr_collection_method(diy,"ContainsKey",1,"System.Boolean","System.String"),diy,diy_args));
    if(!boxed||!read(unbox(boxed),is_diy))throw std::runtime_error("private item target DIY result missing");
    if(!is_diy)return false;
    nitr_field(item,"chessId","System.String");
    auto item_id=roots.retain(field_object(item,"chessId"));void* item_args[]={item_id};
    auto effects=roots.retain(invoke(nb_method(NITR_BUFFS.token),nullptr,item_args));
    if(!effects)throw std::runtime_error("private item target effects missing");
    int32_t count=0;
    if(!read(unbox(invoke(nitr_collection_method(effects,"get_Count",0,"System.Int32"),effects)),count)||count<0||count>128)
        throw std::runtime_error("private item target effect count invalid");
    auto get=nitr_collection_method(effects,"get_Item",1,"Torappu.ActAutoChessData+ActAutoChessBuffInfoData","System.Int32");
    for(int32_t index=0;index<count;++index){
        void* args[]={&index};auto effect=roots.retain(invoke(get,effects,args));
        nitr_field(effect,"key","System.String");
        if(text(field_object(effect,"key"))==NITR_TRANSFER_EFFECT)return nitr_scene(drag);
    }
    return false;
}
void nitr_failure(const std::exception& error){
    // Preview is called every frame. Keep the first useful failure without
    // flooding logs; the server still validates the same effect before writes.
    if(nitr_failed.exchange(true))return;
    ++nitr_failures;nb_error("native-item-target-rule",error);
}
uint8_t nitr_buildable(void* self,void* tile,void* begin,void* item,void* dummy,const void* info){
    RuntimeLease lease(runtime_gate);
    auto result=nitr_old_buildable(self,tile,begin,item,dummy,info);
    if(!result||!lease||closing.load())return result;
    try{TemporaryRoots roots;if(nitr_forbidden(self,tile,item,roots)){++nitr_blocked_preview;return 0;}}
    catch(const std::exception& error){nitr_failure(error);}
    return result;
}
void nitr_equip(void* self,const void* info){
    RuntimeLease lease(runtime_gate);
    if(lease&&!closing.load())try{
        if(nitr_scene(self)){
            TemporaryRoots roots;nitr_field(self,"m_dragContext","Torappu.Battle.UI.AutoChessDragState+TileDragContext");
            auto context=roots.retain(field_object(self,"m_dragContext"));
            nitr_field(context,"currentInst","Torappu.Battle.AutoChess.ChessInst");nitr_field(context,"currentTile","Torappu.Battle.Tile");
            if(nitr_forbidden(self,field_object(context,"currentTile"),field_object(context,"currentInst"),roots)){
                // Native _EquipCharacter also returns without an operation for
                // an invalid build condition. Its caller owns normal cleanup.
                ++nitr_blocked_drop;return;
            }
        }
    }catch(const std::exception& error){nitr_failure(error);}
    nitr_old_equip(self,info);
}
std::string native_item_target_rules_status_json(){
    return "{\"setup\":"+std::string(nitr_setup_ready.load()?"true":"false")+",\"blockedPreviews\":"+std::to_string(nitr_blocked_preview.load())+
        ",\"blockedDrops\":"+std::to_string(nitr_blocked_drop.load())+",\"failures\":"+std::to_string(nitr_failures.load())+"}";
}
void native_item_target_rules_setup(){
    struct FieldSpec{const char* type;const char* name;const char* value;size_t offset;};
    const FieldSpec fields[]={
        {"Torappu.Battle.AutoChess.ChessInst","chessId","System.String",24},
        {"Torappu.Battle.AutoChess.ChessInst","chessType","Torappu.AutoChessItemType",52},
        {"Torappu.ActAutoChessData","diyChessDict","System.Collections.Generic.Dictionary`2<System.String,Torappu.RarityRank>",56},
        {"Torappu.ActAutoChessData+ActAutoChessBuffInfoData","key","System.String",16},
        {"Torappu.Battle.UI.AutoChessDragState","m_dragContext","Torappu.Battle.UI.AutoChessDragState+TileDragContext",168},
        {"Torappu.Battle.UI.AutoChessDragState+TileDragContext","currentInst","Torappu.Battle.AutoChess.ChessInst",32},
        {"Torappu.Battle.UI.AutoChessDragState+TileDragContext","currentTile","Torappu.Battle.Tile",56},
    };
    for(const auto& spec:fields){auto type=cls(spec.type);if(!type)throw std::runtime_error("private item target class missing");
        auto field=profile_field(type,spec.name);
        if((api.il2cpp_field_get_flags(field)&16)||api.il2cpp_field_get_offset(field)!=spec.offset||!matches_type(api.il2cpp_field_get_type(field),spec.value))
            throw std::runtime_error("private item target setup field contract");
    }
    bind_method(NITR_BUFFS);bind_method(NITR_NORMAL);
    hook(NITR_BUILDABLE,nitr_buildable,nitr_old_buildable);hook(NITR_EQUIP,nitr_equip,nitr_old_equip);nitr_setup_ready=true;
}
