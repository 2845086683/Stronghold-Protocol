// Included by native_battle_data.inc inside cn, after native DTO helpers.
// All display uses the original OnRevBroadcast -> PluginEvent 1003 -> native
// parser/panel. This adapter owns no UI, socket, official request or layer tick.
#pragma once
inline constexpr CN_Method NBB_RECEIVE={"Torappu.Battle.DataCenter.AutoChessDataCenter","OnRevBroadcast",0x6013496,0xb10de0ULL,1,false,"System.Void",{"System.Object"}};
inline constexpr CN_Method NBB_BOSS_RATIO={"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel","get_bossHpRatio",0x60138ef,0xba7030ULL,0,false,"System.Single",{}};
std::mutex nbb_identity_lock;
cnjson::Value nbb_identity;
std::atomic<unsigned> nbb_identity_reads{0},nbb_identity_failures{0},nbb_delivered{0},nbb_duplicates{0},nbb_rejected{0},nbb_boss_delivered{0};
std::atomic<uint64_t> nbb_identity_last_read{0};std::atomic<uintptr_t> nbb_identity_last_model{0};
std::atomic<bool> nbb_identity_available{false};
std::string nbb_error;
cnbroadcast::Delivery nbb_delivery;
std::vector<cnbroadcast::Entry> nbb_catalog;
unsigned nbb_pending_boss=0;int32_t nbb_pending_round=0;
bool nbb_boss_dirty=false;
std::string nbb_pending_run;
thread_local const cnjson::Value* nbb_current_reply=nullptr;
thread_local bool nbb_current_reply_consumed=false;
thread_local unsigned nbb_terminal_delivery_depth=0;
struct NB_BroadcastReplyScope {
    const cnjson::Value* previous;bool previous_consumed;
    explicit NB_BroadcastReplyScope(const cnjson::Value& reply):previous(nbb_current_reply),previous_consumed(nbb_current_reply_consumed){nbb_current_reply=&reply;nbb_current_reply_consumed=false;}
    ~NB_BroadcastReplyScope(){nbb_current_reply=previous;nbb_current_reply_consumed=previous_consumed;}
};

void nbb_field(void* c,const char* name,const char* type,size_t offset=SIZE_MAX){
    auto f=profile_field(c,name);if((api.il2cpp_field_get_flags(f)&16)||!matches_type(api.il2cpp_field_get_type(f),type)||(offset!=SIZE_MAX&&api.il2cpp_field_get_offset(f)!=offset))throw std::runtime_error(std::string("broadcast field contract: ")+name);
}
void native_broadcast_observe_identity(void* real_model){
    // Called with the untouched return from old_model, before context_model
    // clones the activity view. No singleton factory, rawData or SDK is read.
    const auto now=GetTickCount64();const auto previous=nbb_identity_last_read.load();
    if(nbb_identity_available.load()&&nbb_identity_last_model.load()==reinterpret_cast<uintptr_t>(real_model)&&previous&&now-previous<5000)return;
    nbb_identity_last_model.store(reinterpret_cast<uintptr_t>(real_model));nbb_identity_last_read.store(now);
    try{
        cnjson::Value card=nb_object();
        if(real_model){
            if(api.il2cpp_object_get_class(real_model)!=cls("Torappu.PlayerDataModel"))throw std::runtime_error("broadcast identity real model type");
            auto status=field_object(real_model,"status");if(status&&api.il2cpp_object_get_class(status)==cls("Torappu.PlayerStatus")){
                auto name=text(field_object(status,"nickName")),number=text(field_object(status,"nickNumber"));
                if(!name.empty()&&name.size()<=256&&!number.empty()&&number.size()<=32){
                    card.object["nickName"]=json_text(name);card.object["nickNumber"]=json_text(number);
                    for(const auto key:{"secretary","secretarySkinId"}){auto s=text(field_object(status,key));if(!s.empty()&&s.size()<=256)card.object[key]=json_text(s);}
                    int32_t level=0;uint8_t skin_sp=0;api.il2cpp_field_get_value(status,profile_field(api.il2cpp_object_get_class(status),"level"),&level);api.il2cpp_field_get_value(status,profile_field(api.il2cpp_object_get_class(status),"secretarySkinSp"),&skin_sp);
                    if(level>=1&&level<=1000)card.object["level"]=nb_integer(level);card.object["secretarySkinSp"]=nb_boolean(skin_sp!=0);
                    auto style=field_object(real_model,"playerNameCardStyle");
                    if(style&&api.il2cpp_object_get_class(style)==cls("Torappu.PlayerNameCardStyle")){
                        auto skin=field_object(style,"skin");
                        if(skin&&api.il2cpp_object_get_class(skin)==cls("Torappu.PlayerNameCardSkin")){
                            auto selected=field_object(skin,"selected");auto skin_id=text(selected);
                            if(!skin_id.empty()&&skin_id.size()<=256){
                                int32_t skin_template=0;auto templates=field_object(skin,"tmpl");
                                if(templates){auto lookup=method(api.il2cpp_object_get_class(templates),"TryGetValue",2);uint32_t flags=0;
                                    if(!lookup||(api.il2cpp_method_get_flags(lookup,&flags)&16)||!matches_type(api.il2cpp_method_get_return_type(lookup),"System.Boolean"))throw std::runtime_error("display name card template lookup contract");
                                    void* args[]={selected,&skin_template};uint8_t found=0;
                                    if(!read(unbox(invoke(lookup,templates,args)),found))throw std::runtime_error("display name card template lookup result");
                                    if(!found)skin_template=0;
                                }
                                if(skin_template<0)throw std::runtime_error("display name card template is negative");
                                card.object["nameCardSkinId"]=json_text(skin_id);card.object["nameCardSkinTmpl"]=nb_integer(skin_template);
                            }
                        }
                    }
                    auto avatar=field_object(status,"avatar");if(avatar&&api.il2cpp_object_get_class(avatar)==cls("Torappu.AvatarInfo")){
                        int32_t type=0;api.il2cpp_field_get_value(avatar,profile_field(api.il2cpp_object_get_class(avatar),"type"),&type);auto id=text(field_object(avatar,"id"));
                        const char* types[]={"NONE","ASSISTANT","ICON","DEFAULT"};if(type>=0&&type<=3&&id.size()<=256){card.object["avatarType"]=json_text(types[type]);card.object["avatarId"]=json_text(id);}
                    }
                }
            }
        }
        std::lock_guard<std::mutex> lock(nbb_identity_lock);nbb_identity=std::move(card);nbb_identity_available=nbb_identity.object.count("nickName")!=0;++nbb_identity_reads;
    }catch(...){std::lock_guard<std::mutex> lock(nbb_identity_lock);nbb_identity=nb_object();nbb_identity_available=false;++nbb_identity_failures;}
}
cnjson::Value native_broadcast_cached_identity(){
    std::lock_guard<std::mutex> lock(nbb_identity_lock);
    return nbb_identity_available.load()?nbb_identity:nb_object();
}
bool native_broadcast_project_settlement(cnjson::Value& summary,const std::string& self_uid){
    // The caller has already bound the authoritative result to its native self
    // UID. Display-only cached profile fields never change that result proof,
    // another player's card, titles, score, rounds, or any persisted account.
    if(self_uid.empty())return false;
    auto teams=summary.object.find("teamInfo");if(teams==summary.object.end()||teams->second.kind!=cnjson::Value::Array)return false;
    cnjson::Value card;{std::lock_guard<std::mutex> lock(nbb_identity_lock);card=nbb_identity;}
    if(card.kind!=cnjson::Value::Object||!card.object.count("nickName")||!card.object.count("nickNumber"))return false;
    cnjson::Value* self=nullptr;
    for(auto& row:teams->second.array){
        if(row.kind!=cnjson::Value::Object)continue;auto uid=row.object.find("uid");
        if(uid==row.object.end()||uid->second.kind!=cnjson::Value::String||uid->second.string!=self_uid)continue;
        if(self)return false;self=&row;
    }
    if(!self)return false;auto target=self->object.find("card");if(target==self->object.end()||target->second.kind!=cnjson::Value::Object)return false;
    auto projected=target->second;
    for(const auto& mapping:std::initializer_list<std::pair<const char*,const char*>>{
        {"nickName","nickname"},{"nickNumber","nicknameNumber"},{"level","level"},
        {"secretary","secretary"},{"secretarySkinId","secretarySkinId"},{"secretarySkinSp","secretarySkinSp"},
        {"avatarType","avatarType"},{"avatarId","avatarId"},{"nameCardSkinId","nameCardSkinId"},{"nameCardSkinTmpl","nameCardSkinTmpl"}}){
        auto value=card.object.find(mapping.first);if(value!=card.object.end())projected.object[mapping.second]=value->second;
    }
    target->second=std::move(projected);return true;
}
void native_broadcast_project_identity(cnjson::Value& all){
    // Only local player zero is replaced. Preserve the separate original
    // nickname/number fields so native parser formatting never doubles #0000.
    auto players=all.object.find("staticPlayers");if(players==all.object.end()||players->second.kind!=cnjson::Value::Array)return;
    cnjson::Value card;{std::lock_guard<std::mutex> lock(nbb_identity_lock);card=nbb_identity;}
    for(auto& player:players->second.array){if(nb_number(player,"index")!=0)continue;
        auto target=player.object.find("cardInfo");if(target!=player.object.end()&&target->second.kind==cnjson::Value::Object&&card.kind==cnjson::Value::Object)
            for(const auto& mapping:std::initializer_list<std::pair<const char*,const char*>>{
                {"nickName","nickName"},{"nickNumber","nickNumber"},{"level","level"},
                {"secretary","secretary"},{"secretarySkinId","secretarySkinId"},{"secretarySkinSp","secretarySkinSp"},
                {"avatarType","avatarType"},{"avatarId","avatarId"},{"nameCardSkinId","nameCardSkin"},{"nameCardSkinTmpl","nameCardSkinTmpl"}}){
                auto value=card.object.find(mapping.first);if(value!=card.object.end())target->second.object[mapping.second]=value->second;
            }
        // PrepareModel already cached selfUid during ordinary login. Its
        // existing object is optional; never initialize the SDK to get a UID.
        auto prepare=nb_retained("native-private-prepare-model");if(prepare&&api.il2cpp_object_get_class(prepare)==cls("Torappu.UI.AutoChess.AutoChessPrepareModel")){
            auto getter=method(api.il2cpp_object_get_class(prepare),"get_selfUid",0);uint32_t flags=0;
            if(getter&&!(api.il2cpp_method_get_flags(getter,&flags)&16)&&matches_type(api.il2cpp_method_get_return_type(getter),"System.String")){
                auto uid=text(invoke(getter,prepare));if(!uid.empty()&&uid.size()<=128)player.object["uid"]=json_text(uid);
            }
        }
    }
}
int32_t nbb_count(void* list,int32_t maximum){
    int32_t n=0;if(!list||!read(unbox(invoke(method(api.il2cpp_object_get_class(list),"get_Count",0),list)),n)||n<0||n>maximum)throw std::runtime_error("broadcast native list bound");return n;
}
void* nbb_item(void* list,int32_t index,TemporaryRoots& roots){void* args[]={&index};auto item=roots.retain(invoke(method(api.il2cpp_object_get_class(list),"get_Item",1),list,args));if(!item)throw std::runtime_error("broadcast native item missing");return item;}
void nbb_load_catalog(){
    if(!nbb_catalog.empty())return;TemporaryRoots roots;
    auto table=roots.retain(invoke(method(cls("Torappu.ActivityDB"),"get_data",0),nullptr));
    auto common=roots.retain(field_object(table,"autoChessData"));if(!common||api.il2cpp_object_get_class(common)!=cls("Torappu.AutoChessData"))throw std::runtime_error("broadcast native common data missing");
    auto list=roots.retain(field_object(common,"broadcastList"));std::vector<cnbroadcast::Entry> candidate;
    for(int32_t i=0,n=nbb_count(list,64);i<n;++i){auto row=nbb_item(list,i,roots);cnbroadcast::Entry entry;entry.id=text(field_object(row,"id"));api.il2cpp_field_get_value(row,profile_field(api.il2cpp_object_get_class(row),"type"),&entry.type);
        if(entry.type==3){auto params=roots.retain(field_object(row,"paramList"));if(nbb_count(params,8)!=1)throw std::runtime_error("boss broadcast native threshold count");auto raw=text(nbb_item(params,0,roots));size_t consumed=0;entry.threshold=std::stod(raw,&consumed);if(consumed!=raw.size())throw std::runtime_error("boss broadcast native threshold text");}
        candidate.push_back(std::move(entry));
    }
    cnbroadcast::validate_catalog(candidate);nbb_catalog=std::move(candidate);
}
bool nbb_owner(){
    if(!can_read()||closing.load()||!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||nb_run_id.empty()||nb_run_id!=nbb_delivery.run||nb_string(nb_state,"runId")!=nb_run_id||nb_string(nb_state,"activityId")!="act2autochess")return false;
    const auto phase=nb_string(nb_state,"phase");if(phase=="paused"||phase=="loading")return false;
    if(phase=="result"&&(!nbb_terminal_delivery_depth||!nbb_current_reply||nb_string(nb_at(*nbb_current_reply,"state"),"runId")!=nb_run_id||nb_number(nb_at(*nbb_current_reply,"state"),"revision")!=nb_number(nb_state,"revision")))return false;
    auto gm=nb_manager(),dc=nb_centre();return gm&&dc&&field_object(gm,"dataCenter")==dc&&text(field_object(dc,"<actId>k__BackingField"))=="act2autochess";
}
void nbb_dispatch(const cnjson::Value& payload){
    nb_main_thread();if(!nbb_owner())throw std::runtime_error("broadcast native scene owner changed");TemporaryRoots roots;PrivateDataScope private_scope(true);NB_ProjectionScope receive_scope;
    auto message=build_value(api.il2cpp_class_get_type(cls("Torappu.AutoChessBroadcast")),payload,roots).object;
    if(!message||api.il2cpp_object_get_class(message)!=cls("Torappu.AutoChessBroadcast"))throw std::runtime_error("broadcast native DTO conversion");
    void* args[]={message};invoke(bound_methods.at(NBB_RECEIVE.token),nb_centre(),args);++nbb_delivered;
}
void native_broadcast_session(const std::string& run){nbb_delivery.session(run,nb_number(nb_state,"revision"));nbb_pending_boss=0;nbb_boss_dirty=false;nbb_pending_round=0;nbb_pending_run.clear();}
void native_broadcast_detach(){nbb_pending_boss=0;nbb_boss_dirty=false;nbb_pending_round=0;nbb_pending_run.clear();}
void native_broadcast_receive(const cnjson::Value& reply){
    try{
        if(nbb_current_reply==&reply&&nbb_current_reply_consumed)return;
        const auto& state=nb_at(reply,"state");auto revision=nb_number(state,"revision");
        if(nb_string(state,"runId")!=nbb_delivery.run||revision<nbb_delivery.high_revision)throw std::runtime_error("broadcast stale response revision");
        nbb_delivery.high_revision=std::max(nbb_delivery.high_revision,revision);
        auto incoming=reply.object.find("nativeBroadcasts");if(incoming==reply.object.end())return;
        if(incoming->second.kind!=cnjson::Value::Array||incoming->second.array.size()>64)throw std::runtime_error("broadcast response batch invalid");
        if(incoming->second.array.empty())return;
        if(!nb_true(reply,"ok")||!nbb_owner())throw std::runtime_error("broadcast requires a successful live command receipt");
        if(nb_string(state,"runId")!=nb_run_id||revision!=nb_number(nb_state,"revision"))throw std::runtime_error("broadcast response revision mismatch");
        const auto& receipt=nb_at(reply,"nativeBroadcastReceipt");
        auto first_round=nb_number(receipt,"fromRound"),last_round=nb_number(receipt,"toRound"),from_revision=nb_number(receipt,"fromRevision");
        if(nb_string(receipt,"runId")!=nb_run_id||nb_number(receipt,"toRevision")!=revision||from_revision<0||from_revision>=revision||last_round!=nb_number(state,"round")||first_round<0||first_round>last_round||last_round-first_round>1)throw std::runtime_error("broadcast command delta receipt invalid");
        nbb_load_catalog();std::vector<cnbroadcast::Event> events;
        for(const auto& value:incoming->second.array)events.push_back(cnbroadcast::parse(value,nbb_catalog,nb_run_id,first_round,last_round));
        for(const auto& event:events){if(!nbb_delivery.unseen(event)){++nbb_duplicates;continue;}
            // Single simulation has one real participant. A future room DTO
            // must explicitly supply the recipient before its gift can arrive.
            if(event.index!=0)throw std::runtime_error("broadcast single player index mismatch");
            // Record dispatch intent before entering arbitrary original event
            // listeners. An ambiguous managed exception must not spam replay.
            nbb_delivery.accepted(event);nbb_dispatch(event.payload);
        }
    }catch(const std::exception& ex){++nbb_rejected;nbb_error=ex.what();}
}
void native_broadcast_before_state_change(int32_t state){
    if(state!=7||!nbb_current_reply||nbb_current_reply_consumed)return;
    // Only the exact successful apply-message stack may temporarily deliver
    // terminal transaction events. A result GET or a later tick gets no scope.
    struct TerminalDeliveryScope{TerminalDeliveryScope(){++nbb_terminal_delivery_depth;}~TerminalDeliveryScope(){--nbb_terminal_delivery_depth;}} terminal_scope;
    native_broadcast_receive(*nbb_current_reply);nbb_current_reply_consumed=true;
}
void native_broadcast_boss_progress(void* status){
    try{
        if(!nbb_owner()||nb_string(nb_state,"phase")!="boss"||!status||status!=field_object(nb_centre(),"battleStatus"))return;
        nb_main_thread();auto round=nb_number(nb_state,"round");if(nbb_delivery.mask(round)==7)return;
        // A rapid volley may emit hundreds of HP callbacks. Only mark the
        // existing HUD dirty here; read/box its final ratio once per safe tick.
        if(nbb_pending_run!=nb_run_id||nbb_pending_round!=round)nbb_pending_boss=0;
        nbb_boss_dirty=true;nbb_pending_round=round;nbb_pending_run=nb_run_id;
    }catch(const std::exception& ex){++nbb_rejected;nbb_error=ex.what();}
}
void native_broadcast_tick(){
    if((!nbb_pending_boss&&!nbb_boss_dirty)||nb_projection_depth||nb_command_in_flight||closing.load())return;
    try{
        nb_main_thread();if(!nbb_owner()||nbb_pending_run!=nb_run_id||nbb_pending_round!=nb_number(nb_state,"round")||nb_string(nb_state,"phase")!="boss"){native_broadcast_detach();return;}
        if(nbb_boss_dirty){
            nbb_boss_dirty=false;nbb_load_catalog();TemporaryRoots roots;float ratio=0;
            auto status=field_object(nb_centre(),"battleStatus"),boxed=roots.retain(invoke(bound_methods.at(NBB_BOSS_RATIO.token),status));
            if(!boxed||!read(unbox(boxed),ratio))throw std::runtime_error("broadcast native boss HUD ratio missing");
            nbb_pending_boss|=cnbroadcast::crossed(nbb_catalog,double(ratio),nbb_delivery.mask(nbb_pending_round));
        }
        unsigned bit=1;for(const auto& row:nbb_catalog)if(row.type==3){
            if((nbb_pending_boss&bit)&&!(nbb_delivery.mask(nbb_pending_round)&bit)){
                auto payload=nb_object();payload.object["uIdx"]=nb_integer(0);payload.object["broadcastId"]=json_text(row.id);payload.object["strParams"]=nb_array();payload.object["intParams"]=nb_array();
                nbb_delivery.boss_accepted(nbb_pending_round,bit);nbb_dispatch(payload);++nbb_boss_delivered;
            }
            nbb_pending_boss&=~bit;bit<<=1;
        }
    }catch(const std::exception& ex){++nbb_rejected;nbb_error=ex.what();}
}
unsigned native_broadcast_boss_mask(){return nbb_delivery.run==nb_run_id?nbb_delivery.mask(nb_number(nb_state,"round")):0;}
cnjson::Value native_broadcast_status(bool details){
    auto value=nb_object();value.object["delivered"]=nb_integer(nbb_delivered.load());value.object["bossDelivered"]=nb_integer(nbb_boss_delivered.load());value.object["duplicateReceipts"]=nb_integer(nbb_duplicates.load());value.object["rejected"]=nb_integer(nbb_rejected.load());value.object["identityCacheReads"]=nb_integer(nbb_identity_reads.load());value.object["identityCacheFailures"]=nb_integer(nbb_identity_failures.load());
    {std::lock_guard<std::mutex> lock(nbb_identity_lock);value.object["cachedRealDisplayIdentity"]=nb_boolean(nbb_identity.object.count("nickName")!=0);}
    if(details){value.object["catalogRows"]=nb_integer(nbb_catalog.size());value.object["pendingBossMask"]=nb_integer(nbb_pending_boss);value.object["bossHudDirty"]=nb_boolean(nbb_boss_dirty);value.object["lastError"]=json_text(nbb_error);}return value;
}
void native_broadcast_setup(){
    bind_method(NBB_RECEIVE);bind_method(NBB_BOSS_RATIO);
    for(const auto& row:std::initializer_list<std::tuple<const char*,const char*,const char*,size_t>>{
        {"Torappu.AutoChessBroadcast","uIdx","System.Int32",16},{"Torappu.AutoChessBroadcast","broadcastId","System.String",24},{"Torappu.AutoChessBroadcast","strParams","System.Collections.Generic.List`1<System.String>",32},{"Torappu.AutoChessBroadcast","intParams","System.Collections.Generic.List`1<System.Int32>",40},
        {"Torappu.PlayerDataModel","status","Torappu.PlayerStatus",32},{"Torappu.PlayerStatus","nickName","System.String",16},{"Torappu.PlayerStatus","nickNumber","System.String",24},{"Torappu.PlayerStatus","level","System.Int32",72},{"Torappu.PlayerStatus","secretary","System.String",160},{"Torappu.PlayerStatus","secretarySkinId","System.String",168},{"Torappu.PlayerStatus","secretarySkinSp","System.Boolean",176},{"Torappu.PlayerStatus","avatar","Torappu.AvatarInfo",232},{"Torappu.AvatarInfo","type","Torappu.PlayerAvatarType",16},{"Torappu.AvatarInfo","id","System.String",24},
        {"Torappu.PlayerDataModel","playerNameCardStyle","Torappu.PlayerNameCardStyle",416},{"Torappu.PlayerNameCardStyle","skin","Torappu.PlayerNameCardSkin",24},{"Torappu.PlayerNameCardSkin","selected","System.String",16},{"Torappu.PlayerNameCardSkin","tmpl","System.Collections.Generic.Dictionary`2<System.String,System.Int32>",32},
        {"Torappu.AutoChessData","broadcastList","System.Collections.Generic.List`1<Torappu.AutoChessData+AutoChessBroadcastData>",168},{"Torappu.AutoChessData+AutoChessBroadcastData","id","System.String",16},{"Torappu.AutoChessData+AutoChessBroadcastData","type","Torappu.AutoChessBroadcastType",36},{"Torappu.AutoChessData+AutoChessBroadcastData","paramList","System.Collections.Generic.List`1<System.String>",40}
    })nbb_field(cls(std::get<0>(row)),std::get<1>(row),std::get<2>(row),std::get<3>(row));
}
