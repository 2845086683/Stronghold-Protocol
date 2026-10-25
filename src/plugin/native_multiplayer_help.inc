// Included inside namespace cn after native_battle_data.inc.
// Exact pinned CN declarations; no raw managed field offsets or map displacement.
#pragma once

// The existing generated CN_Method profile stores up to six arguments. Keep
// that profile intact and bind this seven-argument native overload separately.
struct NMHSpawnMethod {const char* type;const char* name;uint32_t token;uint64_t rva;unsigned parameters;bool is_static;const char* result;const char* arguments[7];};
static const NMHSpawnMethod NMH_SPAWN={"Torappu.Battle.Deck","SpawnCharacterOrToken",0x60100e1,0x6cbb00ULL,7,false,"System.Boolean",{"System.UInt32","Torappu.SharedConsts+Direction","Torappu.Battle.Tile","System.Boolean","System.Boolean","System.Boolean","System.Boolean"}};
static const CN_Method NMH_METHODS[]={
{"Torappu.Battle.BattleController","GetDeck",0x600fbef,0x625320ULL,1,false,"Torappu.Battle.Deck",{"Torappu.PlayerSide"}},
{"Torappu.Battle.Deck+CharacterCard","CheckBuildable",0x6010210,0x6d6360ULL,4,false,"System.Boolean",{"Torappu.Battle.Tile","System.Boolean","System.Boolean","System.Boolean"}},
{"Torappu.Battle.AutoChess.AutoChessDummyManager","FinishDummyByTile",0x60137ef,0xb5a450ULL,1,false,"System.Void",{"Torappu.Battle.Tile"}},
{"Torappu.Battle.Deck+Card","get_state",0x6010113,0x6c28d0ULL,0,false,"Torappu.Battle.Deck+Card+State",{}},
{"Torappu.Battle.Deck+Card","get_data",0x6010123,0x6c0fa0ULL,0,false,"Torappu.Battle.BattleCharacterData",{}},
{"Torappu.Battle.Deck+Card","get_playerSide",0x6010127,0x6c1df0ULL,0,false,"Torappu.PlayerSide",{}},
{"Torappu.Battle.Deck+Card","ResetRespawnTime",0x601014e,0x6bd650ULL,1,false,"System.Void",{"System.Boolean"}},
{"Torappu.Battle.Deck+Card","ChangeStateManually",0x6010173,0x6b95f0ULL,1,false,"System.Void",{"Torappu.Battle.Deck+Card+State"}},
{"Torappu.Battle.Deck+CharacterCard","get_cardPolicy",0x601020c,0x6d6850ULL,0,false,"Torappu.Battle.Deck+Card+CardPolicy",{}},
{"Torappu.Battle.Map","GetTile",0x60111be,0x87dbc0ULL,1,false,"Torappu.Battle.Tile",{"Torappu.GridPosition"}},
{"Torappu.Battle.Unit","get_isDummy",0x6012f2b,0xad2c00ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.AutoChess.AutoChessBattleUtil","FindCard",0x6013523,0xb24910ULL,1,true,"Torappu.Battle.Deck+Card",{"System.UInt32"}},
{"Torappu.Battle.AutoChess.AutoChessBattleUtil","TryGetCardUidByInstId",0x6013525,0xb28870ULL,3,true,"System.Boolean",{"System.Int32","System.Boolean","System.UInt32&"}},
{"Torappu.Battle.AutoChess.AutoChessDummyManager","GetCharacter",0x60137d0,0xb5a710ULL,1,false,"Torappu.Battle.Character",{"Torappu.Battle.Tile"}},
{"Torappu.Battle.AutoChess.AutoChessDummyManager","GetTile",0x60137d4,0xb5a970ULL,1,false,"Torappu.Battle.Tile",{"System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessDummyManager","CreateDeadLikeDummy",0x60137e5,0xb598f0ULL,3,false,"Torappu.Battle.Character",{"Torappu.Battle.BattleCharacterData","Torappu.Battle.Tile","Torappu.SharedConsts+Direction"}},
{"Torappu.Battle.BattleController","get_map",0x600fbe8,0x6431d0ULL,0,false,"Torappu.Battle.Map",{}},
{"Torappu.Battle.Tile","GetCharacter",0x601143a,0x8bdfe0ULL,0,false,"Torappu.Battle.Character",{}},
{"Torappu.Battle.UnitManager","GetFirstCharacterByUid",0x6012fe9,0xac2480ULL,1,false,"Torappu.Battle.Character",{"System.UInt32"}},
{"Torappu.Battle.Character","OnFinish",0x6012902,0xa2d9f0ULL,1,false,"System.Void",{"Torappu.Battle.Entity+FinishReason"}},
{"Torappu.Battle.Entity","get_unfinished",0x6010353,0x708ee0ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInHelpBattle","_ApplyChessStatus",0x6013d2f,0xbfc420ULL,0,false,"System.Void",{}},
{"Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInHelpBattle","_ApplyChessStatus",0x6013d30,0xbfc0a0ULL,3,false,"System.Void",{"Torappu.Battle.Character","System.Int32","Torappu.Battle.AutoChess.HelpBattleData"}},
{"Torappu.Battle.AutoChess.AutoChessMapAreaManager","ConvertChessPositionInfoToDefault",0x6013860,0xb66390ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.ChessPositionInfo"}},
{"Torappu.Battle.AutoChess.AutoChessMapAreaManager","ConvertChessPositionInfoToRightMap",0x6013862,0xb66560ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.ChessPositionInfo"}},
{"Torappu.Battle.AutoChess.AutoChessBattleUtil","TryGetCharacterInstId",0x601351b,0xb288d0ULL,2,true,"System.Boolean",{"Torappu.Battle.Character","System.Int32&"}},
{"Torappu.Battle.Character","get_isToken",0x601285d,0xa3b530ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.Character","get_data",0x6012883,0xa39dd0ULL,0,false,"Torappu.Battle.BattleCharacterData",{}},
{"Torappu.Battle.Entity","get_originTile",0x601034d,0x707fc0ULL,0,false,"Torappu.Battle.Tile",{}},
{"Torappu.Battle.Entity","get_hpRatio",0x6010387,0x704c20ULL,0,false,"Torappu.FP",{}},
{"Torappu.Battle.Entity","get_spRatio",0x601038c,0x708510ULL,0,false,"Torappu.FP",{}},
{"Torappu.Battle.Entity","get_maxSp",0x601038a,0x7075d0ULL,0,false,"System.Int32",{}},
{"Torappu.Battle.Entity","ApplyModifier",0x601046a,0x6efd80ULL,2,false,"System.Boolean",{"Torappu.Battle.Modifier&","System.Boolean"}},
{"Torappu.Battle.Entity","FinishWithNoReason",0x6010415,0x6f3610ULL,0,false,"System.Void",{}},
{"Torappu.Battle.VisualObject","get_gridPosition",0x6013030,0xad4090ULL,0,false,"Torappu.GridPosition",{}},
{"Torappu.Battle.BattleController","get_unitManager",0x600fbf7,0x644510ULL,0,false,"Torappu.Battle.UnitManager",{}},
{"Torappu.FP","AsFloat",0x6000323,0x66fae20ULL,0,false,"System.Single",{}},
{"Torappu.FP","op_Implicit",0x600031c,0x66fdad0ULL,1,true,"Torappu.FP",{"System.Single"}},
{"Torappu.FP","op_Implicit",0x6000321,0x66fdac0ULL,1,true,"Torappu.FP",{"System.Int32"}},
{"Torappu.FP","op_Subtraction",0x60002fd,0x66fba10ULL,2,true,"Torappu.FP",{"Torappu.FP","Torappu.FP"}},
{"Torappu.FP","op_Multiply",0x6000301,0x66fb870ULL,2,true,"Torappu.FP",{"Torappu.FP","Torappu.FP"}},
{"Torappu.Battle.Modifier","NewSp",0x6011561,0x8b2670ULL,3,true,"Torappu.Battle.Modifier",{"Torappu.FP","Torappu.Battle.Entity","Torappu.Battle.Entity"}},
{"Torappu.Battle.Modifier","SetSharedFlag",0x601154f,0x8b2e40ULL,1,false,"System.Void",{"System.Int32"}},
{"UnityEngine.Mathf","RoundToInt",0x6000814,0x6b4c250ULL,1,true,"System.Int32",{"System.Single"}},
};

void* native_multiplayer_dispatch_bridge();
void* native_multiplayer_dispatch_scene();
using NMHApplyStatus=void(*)(void*,void*,int32_t,void*,const void*);
NMHApplyStatus nmh_original_apply=nullptr;
std::mutex nmh_mutex;
std::string nmh_room,nmh_phase,nmh_error;
int32_t nmh_seat=-1,nmh_round=0;
using NMHKey=std::array<int32_t,3>; // seat, owner instId, canonical preparation position.
std::map<NMHKey,int32_t> nmh_carry;
std::set<NMHKey> nmh_expected;
std::atomic<unsigned> nmh_captured{0},nmh_restored{0},nmh_failures{0};
std::atomic<unsigned> nmh_native_only{0};
std::atomic<bool> nmh_ready{false},nmh_failed{false};
std::atomic<bool> nmh_carry_failed{false};

void* nmh_method(uint32_t token){return bound_methods.at(token);}
int32_t nmh_int(void* object,const char* name){
    if(!object)throw std::runtime_error("multiplayer help integer object missing");
    auto field=profile_field(api.il2cpp_object_get_class(object),name);
    if(!matches_type(api.il2cpp_field_get_type(field),"System.Int32"))throw std::runtime_error("multiplayer help integer field type mismatch");
    int32_t value=0;api.il2cpp_field_get_value(object,field,&value);return value;
}
uint32_t nmh_uint(void* object,const char* name){
    if(!object)throw std::runtime_error("multiplayer help card data missing");
    auto field=profile_field(api.il2cpp_object_get_class(object),name);
    if(!matches_type(api.il2cpp_field_get_type(field),"System.UInt32"))throw std::runtime_error("multiplayer help card field type mismatch");
    uint32_t value=0;api.il2cpp_field_get_value(object,field,&value);return value;
}
template<class T>T nmh_result(void* object){T value{};if(!object||!read(unbox(object),value))throw std::runtime_error("multiplayer help native scalar unavailable");return value;}
bool nmh_scope(){
    if(closing.load()||!can_read()||unity_ui_thread.load()!=GetCurrentThreadId())return false;
    if(!native_multiplayer_dispatch_scene())return false;
    auto bridge=native_multiplayer_dispatch_bridge();auto centre=nb_centre();auto manager=nb_manager();
    return bridge&&centre&&manager&&act(field_object(centre,"<actId>k__BackingField"))&&
        field_object(manager,"dataCenter")==centre&&field_object(manager,"dataBridge")==bridge&&
        api.il2cpp_object_get_class(bridge)==cls("Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer");
}
int32_t nmh_encode(const cnjson::Value& pos){
    auto x=nb_number(pos,"x"),y=nb_number(pos,"y");
    if(x<0||x>65535||y<0||y>32767)throw std::runtime_error("multiplayer help preparation coordinate invalid");
    return int32_t((uint32_t(y)<<16)|uint32_t(x));
}
cnjson::Value nmh_position(int32_t encoded){
    auto value=nb_object();value.object["x"]=nb_integer(encoded&65535);value.object["y"]=nb_integer(encoded>>16);return value;
}
int32_t nmh_default_origin(void* character,TemporaryRoots& roots){
    // The original spawn tile survives character movement. Reuse the native
    // map area's conversion on a fresh position object, never a fixed +column.
    auto tile=roots.retain(invoke(nmh_method(0x601034d),character));
    if(!tile)throw std::runtime_error("multiplayer TOKEN origin tile unavailable");
    auto grid=roots.retain(invoke(nmh_method(0x6013030),tile));
    auto row=nmh_int(grid,"row"),col=nmh_int(grid,"col");
    if(row<0||row>32767||col<0||col>65535)throw std::runtime_error("multiplayer TOKEN origin grid invalid");
    auto value=nb_object();value.object["instId"]=nb_integer(0);value.object["chessType"]=nb_integer(3);
    value.object["position"]=nb_integer((uint32_t(row)<<16)|uint32_t(col));value.object["direction"]=nb_integer(0);
    auto position=build_value(api.il2cpp_class_get_type(cls("Torappu.Battle.AutoChess.ChessPositionInfo")),value,roots).object;
    auto area=field_object(nb_manager(),"mapAreaManager");if(!area)throw std::runtime_error("multiplayer native map area unavailable");
    void* args[]={position};invoke(nmh_method(0x6013860),area,args);
    auto encoded=nmh_int(position,"position");if(encoded<=0)throw std::runtime_error("multiplayer TOKEN canonical origin is outside battlefield");return encoded;
}
template<class T>T nmh_dictionary(void* dictionary,int32_t key){
    if(!dictionary)throw std::runtime_error("multiplayer TOKEN runtime index unavailable");
    auto lookup=method(api.il2cpp_object_get_class(dictionary),"TryGetValue",2);T result{};
    void* args[]={&key,&result};if(!nmh_result<uint8_t>(invoke(lookup,dictionary,args)))throw std::runtime_error("multiplayer TOKEN runtime identity unknown");return result;
}
NMHKey nmh_key(void* character,int32_t inst,TemporaryRoots& roots,uint32_t* card_uid=nullptr){
    auto indexer=field_object(nb_centre(),"indexer");
    auto seat=nmh_dictionary<int32_t>(field_object(indexer,"instId2PlayerIndex"),inst);
    auto card=nmh_dictionary<uint32_t>(field_object(indexer,"runtimeToken2CardDict"),inst);
    auto data=roots.retain(invoke(nmh_method(0x6012883),character));
    if(!card||nmh_uint(data,"uniqueId")!=card)throw std::runtime_error("multiplayer TOKEN card and preparation owner do not match");
    if(seat<0||seat>3||inst<0)throw std::runtime_error("multiplayer TOKEN owner outside room");
    if(card_uid)*card_uid=card;return {seat,inst,nmh_default_origin(character,roots)};
}
// Capture before native lifecycle teardown with the same validated identity,
// origin conversion, FP conversion and rounding as prepared TOKEN carry.
struct NMHTerminalSample { uint32_t card=0;int32_t tech=0; };
using NMHFinish=void(*)(void*,int32_t,const void*);
NMHFinish nmh_original_finish=nullptr;
std::string nmh_run;
int32_t nmh_match_sequence=0;
uint32_t nmh_terminal_controller_root=0;
std::map<NMHKey,NMHTerminalSample> nmh_terminal;
std::atomic<unsigned> nmh_terminal_samples{0};

void nmh_carry_clear_locked();
extern cnjson::Value nmh_carry_first_failure;

// Caller holds nmh_mutex and runs on the Unity thread.
void nmh_terminal_clear_locked(){
    nmh_terminal.clear();
    if(nmh_terminal_controller_root){hold_free(nmh_terminal_controller_root);nmh_terminal_controller_root=0;}
}
void native_multiplayer_help_reset(){
    nb_main_thread();std::lock_guard<std::mutex> lock(nmh_mutex);nmh_terminal_clear_locked();nmh_carry_clear_locked();
    nmh_room.clear();nmh_phase.clear();nmh_run.clear();nmh_match_sequence=0;nmh_seat=-1;nmh_round=0;
    nmh_carry.clear();nmh_expected.clear();nmh_ready=false;nmh_failed=false;nmh_carry_failed=false;nmh_error.clear();nmh_carry_first_failure={};
}
void nmh_terminal_bind_controller(void* controller){
    if(!controller)throw std::runtime_error("multiplayer TOKEN controller unavailable");
    std::lock_guard<std::mutex> lock(nmh_mutex);
    if(nmh_terminal_controller_root&&hold_get(nmh_terminal_controller_root)==controller)return;
    auto root=hold_new(controller,false);if(!root)throw std::runtime_error("multiplayer TOKEN controller root unavailable");
    nmh_terminal_clear_locked();nmh_terminal_controller_root=root;
}
bool nmh_prepared_identity(void* unit,const std::set<NMHKey>& expected,int32_t seat,TemporaryRoots& roots,NMHKey& key,uint32_t& card){
    if(!unit||!api.il2cpp_class_is_assignable_from(cls("Torappu.Battle.Character"),api.il2cpp_object_get_class(unit))||
       !nmh_result<uint8_t>(invoke(nmh_method(0x601285d),unit)))return false;
    int32_t inst=-1;void* inst_args[]={unit,&inst};
    if(!nmh_result<uint8_t>(invoke(nmh_method(0x601351b),nullptr,inst_args)))return false;
    auto indexer=field_object(nb_centre(),"indexer");auto tokens=field_object(indexer,"runtimeToken2CardDict");
    void* lookup_args[]={&inst,&card};
    if(!tokens||!nmh_result<uint8_t>(invoke(method(api.il2cpp_object_get_class(tokens),"TryGetValue",2),tokens,lookup_args)))return false;
    auto data=roots.retain(invoke(nmh_method(0x6012883),unit));
    if(!card||nmh_uint(data,"uniqueId")!=card)return false;
    key=nmh_key(unit,inst,roots,&card);return key[0]==seat&&expected.count(key);
}
int32_t nmh_terminal_sp(void* unit,TemporaryRoots& roots){
    auto fp=roots.retain(invoke(nmh_method(0x601038c),unit));
    auto ratio=nmh_result<float>(invoke(nmh_method(0x6000323),unbox(fp)));ratio*=100.0f;
    void* args[]={&ratio};auto tech=nmh_result<int32_t>(invoke(nmh_method(0x6000814),nullptr,args));
    if(tech<0||tech>100)throw std::runtime_error("multiplayer TOKEN SP ratio outside contract");return tech;
}
void nmh_terminal_record(void* character){
    std::set<NMHKey> expected;int32_t seat;
    {std::lock_guard<std::mutex> lock(nmh_mutex);if(!nmh_ready.load()||nmh_phase!="battle"||nmh_run.empty())return;expected=nmh_expected;seat=nmh_seat;}
    if(expected.empty())return;
    TemporaryRoots roots;NMHKey key{};uint32_t card=0;
    if(!nmh_prepared_identity(character,expected,seat,roots,key,card))return;
    auto controller=roots.retain(invoke(method(cls("Torappu.Battle.BattleController"),"get_instance",0),nullptr));
    nmh_terminal_bind_controller(controller);auto tech=nmh_terminal_sp(character,roots);
    {std::lock_guard<std::mutex> lock(nmh_mutex);
        if(!nmh_ready.load()||nmh_phase!="battle"||!nmh_expected.count(key)||hold_get(nmh_terminal_controller_root)!=controller)
            throw std::runtime_error("multiplayer TOKEN terminal scope changed");
        nmh_terminal[key]={card,tech};++nmh_terminal_samples;}
}
void nmh_character_finish(void* self,int32_t reason,const void* info){
    RuntimeLease lease(runtime_gate);
    try{if(lease&&nmh_scope())nmh_terminal_record(self);}
    catch(const std::exception& error){
        nmh_failed=true;++nmh_failures;
        try{std::lock_guard<std::mutex> lock(nmh_mutex);nmh_error=error.what();nb_error("multiplayer-token-terminal",error);}catch(...){}
    }catch(...){
        nmh_failed=true;++nmh_failures;
        try{std::lock_guard<std::mutex> lock(nmh_mutex);nmh_error="unknown multiplayer TOKEN terminal capture failure";}catch(...){}
    }
    // Never consume lifecycle teardown, even for foreign objects, shutdown,
    // failed identity reads, failed allocation, or an unknown C++ exception.
    lease.release();nmh_original_finish(self,reason,info);
}

cnjson::Value nmh_capture_live_and_terminal(){
    nb_main_thread();if(!nmh_scope())throw std::runtime_error("multiplayer TOKEN capture outside private native bridge");
    std::set<NMHKey> expected;int32_t seat;
    {std::lock_guard<std::mutex> lock(nmh_mutex);if(!nmh_ready.load()||nmh_phase!="battle"||nmh_run.empty())throw std::runtime_error("multiplayer TOKEN capture outside own battle");expected=nmh_expected;seat=nmh_seat;}
    auto result=nb_array();if(expected.empty())return result;
    TemporaryRoots roots;auto controller=roots.retain(invoke(method(cls("Torappu.Battle.BattleController"),"get_instance",0),nullptr));
    nmh_terminal_bind_controller(controller);
    std::map<NMHKey,NMHTerminalSample> samples;
    {std::lock_guard<std::mutex> lock(nmh_mutex);samples=nmh_terminal;}
    auto units=roots.retain(invoke(nmh_method(0x600fbf7),controller));
    auto characters=roots.retain(field_object(units,"<characters>k__BackingField"));
    if(!characters)throw std::runtime_error("multiplayer native characters unavailable");auto type=api.il2cpp_object_get_class(characters);
    auto count=nmh_result<int32_t>(invoke(method(type,"get_count",0),characters));
    if(count<0||count>4096)throw std::runtime_error("multiplayer TOKEN unit count outside bound");
    std::set<NMHKey> alive;
    for(int32_t index=0;index<count;++index){void* args[]={&index};auto unit=roots.retain(invoke(method(type,"get_Item",1),characters,args));
        // A finished object may remain in the collection until native cleanup;
        // its cleared SP must not override the prefix's terminal observation.
        if(!unit||!api.il2cpp_class_is_assignable_from(cls("Torappu.Battle.Character"),api.il2cpp_object_get_class(unit)))continue;
        if(!nmh_result<uint8_t>(invoke(nmh_method(0x6010353),unit)))continue;
        NMHKey key{};uint32_t card=0;if(!nmh_prepared_identity(unit,expected,seat,roots,key,card))continue;
        if(!alive.insert(key).second)throw std::runtime_error("multiplayer TOKEN origin identity is ambiguous");
        // A currently observable instance is authoritative over older finishes
        // at the same preparation origin. Periodic live values are not cached.
        samples[key]={card,nmh_terminal_sp(unit,roots)};
    }
    auto cards=field_object(field_object(nb_centre(),"indexer"),"runtimeToken2CardDict");
    for(const auto& item:samples){const auto& key=item.first;const auto& sample=item.second;
        if(key[0]!=seat||!expected.count(key))continue;
        int32_t inst=key[1];uint32_t current_card=0;void* args[]={&inst,&current_card};
        if(cards&&nmh_result<uint8_t>(invoke(method(api.il2cpp_object_get_class(cards),"TryGetValue",2),cards,args))&&current_card!=sample.card)
            throw std::runtime_error("multiplayer TOKEN cached card identity changed");
        auto row=nb_object();row.object["instId"]=nb_integer(key[1]);row.object["position"]=nmh_position(key[2]);
        row.object["tech"]=nb_integer(sample.tech);row.object["cardUid"]=nb_integer(sample.card);result.array.push_back(std::move(row));++nmh_captured;
    }
    // An unobserved/un-finished expected token remains absent and unresolved.
    return result;
}

#include "native_multiplayer_help_carry.inc"

void native_multiplayer_help_set_carry(const cnjson::Value& snapshot){
    nb_main_thread();std::lock_guard<std::mutex> lock(nmh_mutex);
    auto room=nb_string(snapshot,"roomId");auto phase=nb_string(snapshot,"phase");
    auto seat=nb_number(snapshot,"seat"),round=nb_number(snapshot,"round");
    if(seat<0||seat>3)throw std::runtime_error("multiplayer help recipient outside room");
    std::set<NMHKey> expected;std::map<NMHKey,int32_t> carry;
    auto self=snapshot.object.find("selfState");
    if(self!=snapshot.object.end()&&self->second.kind==cnjson::Value::Object){
        auto tokens=self->second.object.find("nativeTokenPositions");
        if(tokens!=self->second.object.end()){
            if(tokens->second.kind!=cnjson::Value::Array||tokens->second.array.size()>128)throw std::runtime_error("multiplayer preparation TOKEN layout invalid");
            for(const auto& token:tokens->second.array){auto pos=nb_number(token,"position");if(pos>0)expected.insert({seat,nb_number(token,"instId"),pos});}
        }
    }
    if(phase=="help"){
        const auto& plan=nb_at(snapshot,"jointDefense");const auto& segments=nb_at(plan,"segments");
        if(nb_number(plan,"round")!=round||nb_true(plan,"allowBondLayerGrowth")||segments.kind!=cnjson::Value::Array||segments.array.empty()||segments.array.size()>2)
            throw std::runtime_error("multiplayer help plan round/segments/layer contract invalid");
        for(const auto& segment:segments.array){auto owner=nb_number(segment,"seat");const auto& deployments=nb_at(segment,"deployments");
            if(owner<0||owner>3||deployments.kind!=cnjson::Value::Array||deployments.array.size()>128)throw std::runtime_error("multiplayer help deployment identity/list invalid");
            for(const auto& unit:deployments.array){if(nb_number(unit,"chessType")!=3)continue;
                auto key=NMHKey{owner,nb_number(unit,"instId"),nmh_encode(nb_at(unit,"position"))};
                const auto& status=nb_at(unit,"carry");
                if(status.kind!=cnjson::Value::Object)throw std::runtime_error("multiplayer help TOKEN terminal SP is unresolved");
                auto tech=nb_number(status,"tech");if(tech<0||tech>100||!carry.emplace(key,tech).second)throw std::runtime_error("multiplayer help TOKEN terminal SP identity invalid");
            }
        }
    }
    auto sequence=nb_number(snapshot,"matchSequence");if(sequence<1)throw std::runtime_error("multiplayer TOKEN match identity invalid");
    std::string run;if(self!=snapshot.object.end()&&self->second.kind==cnjson::Value::Object)run=nb_string(self->second,"runId");
    if(phase=="battle"&&run.empty())throw std::runtime_error("multiplayer TOKEN run identity missing");
    auto carry_plan=nmh_make_carry_plan(snapshot,phase,room,run,sequence,round,seat);
    bool changed=room!=nmh_room||round!=nmh_round||seat!=nmh_seat||sequence!=nmh_match_sequence||run!=nmh_run;
    if(changed){nmh_failed=false;nmh_error.clear();}
    if(changed||phase!="battle"||expected!=nmh_expected)nmh_terminal_clear_locked();
    nmh_run=std::move(run);nmh_match_sequence=sequence;
    nmh_room=std::move(room);nmh_phase=std::move(phase);nmh_round=round;nmh_seat=seat;
    nmh_expected=std::move(expected);nmh_carry=std::move(carry);nmh_accept_carry_locked(std::move(carry_plan));nmh_ready=true;
}
cnjson::Value native_multiplayer_help_capture_tokens(){return nmh_capture_live_and_terminal();}
void nmh_restore_sp(void* character,int32_t tech,TemporaryRoots& roots){
    float ratio=float(tech)/100.0f;void* float_args[]={&ratio};auto desired=roots.retain(invoke(nmh_method(0x600031c),nullptr,float_args));
    auto current=roots.retain(invoke(nmh_method(0x601038c),character));void* sub_args[]={unbox(desired),unbox(current)};
    auto delta=roots.retain(invoke(nmh_method(0x60002fd),nullptr,sub_args));
    auto max_sp=nmh_result<int32_t>(invoke(nmh_method(0x601038a),character));
    if(max_sp<=0){nmh_ratio(character,0x601038c,roots);return;}
    void* int_args[]={&max_sp};auto scale=roots.retain(invoke(nmh_method(0x6000321),nullptr,int_args));
    void* mul_args[]={unbox(delta),unbox(scale)};auto amount=roots.retain(invoke(nmh_method(0x6000301),nullptr,mul_args));
    void* modifier_args[]={unbox(amount),nullptr,character};auto modifier=roots.retain(invoke(nmh_method(0x6011561),nullptr,modifier_args));
    int32_t flag=0x80;void* flag_args[]={&flag};invoke(nmh_method(0x601154f),unbox(modifier),flag_args);
    uint8_t force=0;void* apply_args[]={unbox(modifier),&force};
    invoke(nmh_method(0x601046a),character,apply_args);
    // ApplyModifier owns clamping and skill/buff vetoes. A legal native no-op
    // is not a failed field load; never overwrite the engine's resulting SP.
    nmh_ratio(character,0x601038c,roots);
}
void nmh_apply_status(void* self,void* character,int32_t inst,void* help_data,const void* info){
    auto reentry=nmh_apply_reentry;
    if(reentry&&reentry->self==self&&reentry->character==character&&reentry->inst==inst&&reentry->help==help_data){
        nmh_apply_reentry=nullptr;nmh_original_apply(self,character,inst,help_data,info);return;
    }
    RuntimeLease lease(runtime_gate);
    if(!lease||!nmh_scope()){lease.release();nmh_original_apply(self,character,inst,help_data,info);return;}
    auto failure=nb_object();failure.object["instId"]=nb_integer(inst);
    try{
        bool carry_phase=false;{std::lock_guard<std::mutex> lock(nmh_mutex);carry_phase=nmh_ready.load()&&nmh_phase=="help";}
        if(!carry_phase){lease.release();nmh_original_apply(self,character,inst,help_data,info);return;}
        {std::lock_guard<std::mutex> lock(nmh_mutex);if(nmh_carry_failed.load())return;}
        TemporaryRoots roots;roots.retain(self);roots.retain(character);roots.retain(help_data);
        const bool token=nmh_result<uint8_t>(invoke(nmh_method(0x601285d),character))!=0;NMHKey key{};
        failure.object["isToken"]=nb_boolean(token);
        auto controller=nmh_carry_controller(roots);NMHCarryTarget target;uint64_t serial;
        bool declared_inst=false;
        {std::lock_guard<std::mutex> lock(nmh_mutex);
            if(nmh_failed.load()||nmh_phase!="help"||nmh_carry_stamp.empty())throw std::runtime_error("multiplayer carry plan not active");
            if(!nmh_carry_controller_root||hold_get(nmh_carry_controller_root)!=controller)throw std::runtime_error("multiplayer carry controller not owned by prepared plan");
            if(!nmh_carry_task_root||hold_get(nmh_carry_task_root)!=self)throw std::runtime_error("multiplayer carry task not owned by current start iterator");
            for(const auto& row:nmh_carry_targets)if(row.first[1]==inst&&row.second.token==token)declared_inst=true;
            serial=nmh_carry_serial;}
        int32_t actual=-1;void* inst_args[]={character,&actual};
        if(!nmh_result<uint8_t>(invoke(nmh_method(0x601351b),nullptr,inst_args))||actual!=inst)throw std::runtime_error("multiplayer character runtime instId mismatch");
        auto owners=field_object(field_object(nb_centre(),"indexer"),"instId2PlayerIndex");
        if(!owners)throw std::runtime_error("multiplayer carry owner index unavailable");
        int32_t owner=-1;void* owner_args[]={&inst,&owner};
        const bool indexed=nmh_result<uint8_t>(invoke(method(api.il2cpp_object_get_class(owners),"TryGetValue",2),owners,owner_args))!=0;
        if(indexed){if(owner<0||owner>3)throw std::runtime_error("multiplayer carry owner outside room");failure.object["ownerSeat"]=nb_integer(owner);}
        bool expected=false;
        {std::lock_guard<std::mutex> lock(nmh_mutex);
            if(nmh_carry_serial!=serial)throw std::runtime_error("multiplayer carry context changed during identity lookup");
            for(const auto& row:nmh_carry_targets)if(indexed&&row.first[0]==owner&&row.first[1]==inst&&row.second.token==token)expected=true;}
        if(!indexed&&declared_inst)throw std::runtime_error("multiplayer declared carry unit has no owner index");
        if(indexed&&declared_inst&&!expected)throw std::runtime_error("multiplayer declared carry unit owner differs from plan");
        if(!expected){
            // Native _ApplyChessStatus() enumerates ALL UnitManager Characters
            // (6013d2f/bfc638..bfc669), including engine-created summons and
            // units outside the authenticated pre-deployed defender subset.
            // Keep the original treatment for those units. They never gain a
            // custom HP/SP restore or satisfy any declared target's readiness.
            NMHApplyProof proof;proof.character=character;
            nmh_apply_original_protected(self,character,inst,help_data,proof);++nmh_native_only;return;
        }
        key=token?nmh_key(character,inst,roots):NMHKey{owner,inst,0};
        failure.object["canonicalPosition"]=nb_integer(key[2]);
        {std::lock_guard<std::mutex> lock(nmh_mutex);auto found=nmh_carry_targets.find(key);
            if(nmh_carry_serial!=serial)throw std::runtime_error("multiplayer carry context changed before target lookup");
            if(found==nmh_carry_targets.end()||found->second.token!=token)throw std::runtime_error("multiplayer carry unit not in authenticated defender plan");
            // A recycled dead-like dummy can retain the same identity. Never
            // run withdrawal or HP/SP a second time in this exact attempt.
            if(nmh_carry_applied.count(key))return;
            target=found->second;}
        if(token){nmh_restore_sp(character,target.tech,roots);++nmh_restored;}
        else{
            nmh_check_operator_status(help_data,inst,target,roots);NMHApplyProof proof;proof.character=character;
            nmh_apply_original_protected(self,character,inst,help_data,proof);
            failure.object["hpCalls"]=nb_integer(proof.hpCalls);failure.object["spCalls"]=nb_integer(proof.spCalls);failure.object["finishCalls"]=nb_integer(proof.finishCalls);
            if(proof.invalid)throw std::runtime_error("multiplayer original carry changed target or force flag");
            if(target.retreat){
                // Native DestroyEntityToDummy recycles this Character before
                // creating its dead-like dummy. The pool may already reuse
                // the same address, so get_unfinished here observes a new
                // lifetime, not whether the original retreat succeeded.
                // The protected native call and exact call-site proof are
                // the completion contract; leave resurrection to the engine.
                if(proof.finishCalls!=1||proof.hpCalls||proof.spCalls)throw std::runtime_error("multiplayer original forced retreat call incomplete");
            }else{
                if(proof.hpCalls!=1||proof.spCalls!=1||proof.finishCalls)throw std::runtime_error("multiplayer original HP/SP carry calls incomplete");
                // The authenticated DTO above is the input contract. Original
                // ApplyModifier runs buffs, HP caps and skill SP rules; its
                // output need not equal that input ratio. Requiring equality
                // aborts the entire Help field after a valid native restore.
                const auto hp=nmh_ratio(character,0x6010387,roots),sp=nmh_ratio(character,0x601038c,roots);
                failure.object["actualHpPermille"]=nb_integer(int64_t(hp*1000));failure.object["actualSpPermille"]=nb_integer(int64_t(sp*1000));
                failure.object["hpAccepted"]=nb_boolean(proof.hpAccepted);failure.object["spAccepted"]=nb_boolean(proof.spAccepted);
            }
        }
        {std::lock_guard<std::mutex> lock(nmh_mutex);
            if(nmh_carry_serial!=serial||nmh_phase!="help"||!nmh_carry_targets.count(key)||!nmh_carry_controller_root||hold_get(nmh_carry_controller_root)!=controller)throw std::runtime_error("multiplayer carry context changed before completion");
            nmh_carry_applied.insert(key);}
    }catch(const std::exception& error){
        nmh_carry_record_failure(error.what(),failure);
        try{nb_error("multiplayer-help-unit-carry",error);}catch(...){}
    }catch(...){nmh_carry_record_failure("unknown multiplayer carry failure",failure);}
}
using NMHApplyAll=void(*)(void*,const void*);
NMHApplyAll nmh_original_apply_all=nullptr;
void nmh_apply_all(void* self,const void* info){
    RuntimeLease lease(runtime_gate);
    if(!lease||!nmh_scope()){lease.release();nmh_original_apply_all(self,info);return;}
    bool owned=false;{std::lock_guard<std::mutex> lock(nmh_mutex);owned=nmh_ready&&nmh_phase=="help";}
    if(!owned){lease.release();nmh_original_apply_all(self,info);return;}
    auto context=nb_object();auto samples=nb_array();
    try{
        TemporaryRoots roots;roots.retain(self);auto controller=nmh_carry_controller(roots);
        {std::lock_guard<std::mutex> lock(nmh_mutex);
            if(nmh_carry_failed||!nmh_carry_controller_root||hold_get(nmh_carry_controller_root)!=controller||
               !nmh_carry_task_root||hold_get(nmh_carry_task_root)!=self)
                throw std::runtime_error("multiplayer carry enumeration owner differs");}
        auto help=roots.retain(invoke(method(api.il2cpp_object_get_class(nb_centre()),"get_helpBattleData",0),nb_centre()));
        auto units=roots.retain(invoke(nmh_method(0x600fbf7),controller));
        auto characters=roots.retain(field_object(units,"<characters>k__BackingField"));
        if(!characters||!help)throw std::runtime_error("multiplayer carry enumeration source missing");
        auto type=api.il2cpp_object_get_class(characters);
        const auto count=nmh_result<int32_t>(invoke(method(type,"get_count",0),characters));
        if(count<0||count>4096)throw std::runtime_error("multiplayer carry enumeration exceeds bound");
        std::vector<std::pair<void*,int32_t>> captured;
        // Match native 6013d2f's Character/instId selection, but capture the
        // references before any callback can swap-remove or pool an entry.
        // This runs synchronously; GC roots do not extend a battle lifetime.
        for(int32_t i=0;i<count;++i){void* args[]={&i};auto unit=roots.retain(invoke(method(type,"get_Item",1),characters,args));
            if(!unit||!api.il2cpp_class_is_assignable_from(cls("Torappu.Battle.Character"),api.il2cpp_object_get_class(unit)))continue;
            int32_t inst=-1;void* idArgs[]={unit,&inst};
            if(!nmh_result<uint8_t>(invoke(nmh_method(0x601351b),nullptr,idArgs)))continue;
            captured.emplace_back(unit,inst);auto row=nb_object();row.object["instId"]=nb_integer(inst);
            // Diagnostic getters must not turn a usable native selection
            // into a failed field. They are never used as carry filters.
            try{
                row.object["dummy"]=nb_boolean(nmh_result<uint8_t>(invoke(method(api.il2cpp_object_get_class(unit),"get_isDummy",0),unit))!=0);
                row.object["unfinished"]=nb_boolean(nmh_result<uint8_t>(invoke(nmh_method(0x6010353),unit))!=0);
            }catch(...){row.object["stateUnavailable"]=nb_boolean(true);}
            samples.array.push_back(row);
        }
        {std::lock_guard<std::mutex> lock(nmh_mutex);if(nmh_carry_before_apply.kind!=cnjson::Value::Array)nmh_carry_before_apply=samples;}
        for(const auto& unit:captured){
            // The existing executor revalidates native identity and attempt,
            // and owns the original HP/SP/withdrawal callbacks unchanged.
            nmh_apply_status(self,unit.first,unit.second,help,nullptr);
            if(nmh_carry_failed.load())return;
        }
    }catch(const std::exception& error){context.object["beforeApply"]=samples;nmh_carry_record_failure(error.what(),context);}
    catch(...){context.object["beforeApply"]=samples;nmh_carry_record_failure("unknown multiplayer carry enumeration failure",context);}
}
#include "native_multiplayer_help_respawn.inc"

void native_multiplayer_help_complete_pending(){
    nb_main_thread();if(!nmh_scope())throw std::runtime_error("multiplayer carry completion outside private world");
    if(native_multiplayer_help_carry_ready())return;
    TemporaryRoots roots;auto controller=nmh_carry_controller(roots);
    void* task=nullptr;size_t bound=0;uint64_t serial=0;
    {std::lock_guard<std::mutex> lock(nmh_mutex);
        if(nmh_phase!="help"||nmh_failed||nmh_carry_failed||!nmh_carry_task_root||
           !nmh_carry_controller_root||hold_get(nmh_carry_controller_root)!=controller)
            throw std::runtime_error("multiplayer carry completion lost its start owner");
        task=roots.retain(hold_get(nmh_carry_task_root));bound=nmh_carry_targets.size();serial=nmh_carry_serial;}
    auto help=roots.retain(invoke(method(api.il2cpp_object_get_class(nb_centre()),"get_helpBattleData",0),nb_centre()));
    // Native _ApplyChessStatus enumerates UnitManager's mutable UnorderedArray.
    // DestroyEntityToDummy recycles a downed unit immediately; removal can move
    // an unvisited unit behind that enumerator. Complete only authenticated,
    // still-live targets missed by that pass, through the SAME per-unit hook.
    // Restart after every application because it may recycle another unit.
    for(size_t pass=0;pass<bound;++pass){
        bool applied=false;auto units=roots.retain(invoke(nmh_method(0x600fbf7),controller));
        auto characters=roots.retain(field_object(units,"<characters>k__BackingField"));
        if(!characters)throw std::runtime_error("multiplayer carry characters unavailable");
        auto type=api.il2cpp_object_get_class(characters);
        const auto count=nmh_result<int32_t>(invoke(method(type,"get_count",0),characters));
        if(count<0||count>4096)throw std::runtime_error("multiplayer carry character count outside bound");
        for(int32_t index=0;index<count;++index){
            void* args[]={&index};auto unit=roots.retain(invoke(method(type,"get_Item",1),characters,args));
            if(!unit||!api.il2cpp_class_is_assignable_from(cls("Torappu.Battle.Character"),api.il2cpp_object_get_class(unit))||
               !nmh_result<uint8_t>(invoke(nmh_method(0x6010353),unit))||
               nmh_result<uint8_t>(invoke(method(api.il2cpp_object_get_class(unit),"get_isDummy",0),unit)))continue;
            int32_t inst=-1;void* idArgs[]={unit,&inst};
            if(!nmh_result<uint8_t>(invoke(nmh_method(0x601351b),nullptr,idArgs)))continue;
            int32_t owner=-1;auto owners=field_object(field_object(nb_centre(),"indexer"),"instId2PlayerIndex");
            if(!owners)throw std::runtime_error("multiplayer carry completion owner index unavailable");
            void* ownerArgs[]={&inst,&owner};
            if(!nmh_result<uint8_t>(invoke(method(api.il2cpp_object_get_class(owners),"TryGetValue",2),owners,ownerArgs)))continue;
            const bool token=nmh_result<uint8_t>(invoke(nmh_method(0x601285d),unit))!=0;
            bool declared=false;
            {std::lock_guard<std::mutex> lock(nmh_mutex);if(serial!=nmh_carry_serial)throw std::runtime_error("multiplayer carry completion plan changed");
                for(const auto& row:nmh_carry_targets)if(row.first[0]==owner&&row.first[1]==inst&&row.second.token==token)declared=true;}
            if(!declared)continue;
            const auto key=token?nmh_key(unit,inst,roots):NMHKey{owner,inst,0};
            {std::lock_guard<std::mutex> lock(nmh_mutex);if(!nmh_carry_targets.count(key)||nmh_carry_applied.count(key))continue;}
            nmh_apply_status(task,unit,inst,help,nullptr);
            {std::lock_guard<std::mutex> lock(nmh_mutex);if(nmh_carry_failed||serial!=nmh_carry_serial||!nmh_carry_applied.count(key))return;}
            applied=true;break;
        }
        if(!applied)break;
    }
    nmh_complete_downed(task,controller,help,serial,roots);
    if(!native_multiplayer_help_carry_ready()){
        auto context=nb_object();auto missing=nb_array();
        {std::lock_guard<std::mutex> lock(nmh_mutex);for(const auto& row:nmh_carry_targets)if(!nmh_carry_applied.count(row.first)){
            auto unit=nb_object();unit.object["seat"]=nb_integer(row.first[0]);unit.object["instId"]=nb_integer(row.first[1]);
            unit.object["position"]=nb_integer(row.first[2]);unit.object["retreat"]=nb_boolean(row.second.retreat);missing.array.push_back(unit);}}
        context.object["missingUnits"]=missing;
        const auto error="multiplayer help carry units are not all applied: "+cnjson::dump(missing);
        nmh_carry_record_failure(error,context);throw std::runtime_error(error);
    }
}
void native_multiplayer_help_require_no_failure(){std::lock_guard<std::mutex> lock(nmh_mutex);if(nmh_failed.load()||nmh_carry_failed.load())throw std::runtime_error("multiplayer help carry failed: "+nmh_error);}
void native_multiplayer_help_require_ready(){
    native_multiplayer_help_require_no_failure();bool help=false;{std::lock_guard<std::mutex> lock(nmh_mutex);help=nmh_phase=="help";}
    if(help&&!native_multiplayer_help_carry_ready())throw std::runtime_error("multiplayer help carry units are not all applied");
}
cnjson::Value native_multiplayer_help_status(){
    std::lock_guard<std::mutex> lock(nmh_mutex);auto value=nb_object();value.object["snapshotReady"]=nb_boolean(nmh_ready.load());
    value.object["failed"]=nb_boolean(nmh_failed.load()||nmh_carry_failed.load());value.object["snapshotFailed"]=nb_boolean(nmh_failed.load());value.object["carryFailed"]=nb_boolean(nmh_carry_failed.load());value.object["capturedTokens"]=nb_integer(nmh_captured.load());value.object["restoredTokens"]=nb_integer(nmh_restored.load());
    value.object["expectedCarryUnits"]=nb_integer(nmh_carry_targets.size());value.object["appliedCarryUnits"]=nb_integer(nmh_carry_applied.size());value.object["carryGeneration"]=nb_integer(nmh_carry_generation);
    value.object["firstCarryFailure"]=nmh_carry_first_failure;
    value.object["nativeOnlyUnits"]=nb_integer(nmh_native_only.load());
    value.object["downedRespawnRestores"]=nb_integer(nmh_downed_restores.load());
    value.object["livingCarryRestores"]=nb_integer(nmh_living_restores.load());
    value.object["lastAbsentCarry"]=nmh_absent_last;
    value.object["failures"]=nb_integer(nmh_failures.load());value.object["lastError"]=json_text(nmh_error);value.object["nativeValidated"]=nb_boolean(false);return value;
}
void native_multiplayer_help_setup(){
    bind_method(NMH_SPAWN);
    for(const auto& spec:NMH_METHODS){if(spec.token==0x6013d2f)hook(spec,nmh_apply_all,nmh_original_apply_all);else if(spec.token==0x6013d30)hook(spec,nmh_apply_status,nmh_original_apply);else if(spec.token==0x6012902)hook(spec,nmh_character_finish,nmh_original_finish);else if(spec.token==0x601046a)hook(spec,nmh_carry_modifier,nmh_original_modifier);else if(spec.token==0x6010415)hook(spec,nmh_carry_force_finish,nmh_original_force_finish);}
    for(const auto& spec:NMH_METHODS)bind_method(spec);
}
