// Only the personal result receives a display copy keyed for its SDK lookup.
// Team identity, original response, authentication and SDK state stay intact.
#pragma once
inline constexpr CN_Method NMR_PERSONAL_LOAD={"Torappu.UI.AutoChess.AutoChessSettleGamePersonalViewModel","LoadData",0x602b7e4,0x2786710ULL,4,false,"System.Void",{"Torappu.ActAutoChessData","Torappu.PlayerActivity+PlayerActAutoChessActivity","Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo","Torappu.ActAutoChessModeType"}};
using NMRPersonalLoad=void(*)(void*,void*,void*,void*,int32_t,const void*);
NMRPersonalLoad nmr_old_personal_load=nullptr;
void nmr_personal_load(void* self,void* actData,void* playerData,void* summary,int32_t mode,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    auto response=nb_retained("native-multiplayer-result-response");
    if(!nms_presenting_result||mode!=2||!response||summary!=field_object(response,"gameSettleData")){
        lease.release();nmr_old_personal_load(self,actData,playerData,summary,mode,info);return;
    }
    try{
        nb_main_thread();auto view=nms_view();
        if(!nms_return_matches(view)||nms_personal_sdk_uid.empty())throw std::runtime_error("private personal result identity unavailable");
        auto copied=nb_at(view,"nativeSettleGameInfo");auto& team=copied.object.at("teamInfo");unsigned matched=0;int64_t trophies=-1;
        for(auto& member:team.array)if(nb_string(member,"uid")==nms_result_uid){
            member.object["uid"]=json_text(nms_personal_sdk_uid);trophies=nb_number(nb_at(member,"card"),"trophyNum");++matched;
        }
        if(matched!=1)throw std::runtime_error("private personal result recipient differs");
        if(trophies<0||trophies>2147483647)throw std::runtime_error("private trophy count invalid");
        auto activity=cnjson::parse(activity_json);
        if(nb_number(activity,"trophyNum")!=trophies){activity.object["trophyNum"]=nb_integer(trophies);refresh_private_activity(cnjson::dump(activity));}
        // Refresh the same independent activity object used by subsequent
        // room/main-page loads; do not modify the official PlayerData model.
        playerData=player();
        TemporaryRoots roots;auto native=build_value(api.il2cpp_class_get_type(cls("Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo")),copied,roots).object;
        if(!native)throw std::runtime_error("private personal result copy unavailable");
        // LoadData inlines _GetSelfBattleBriefInfo; patching that helper alone
        // misses this path. The original model computes text, voice and rounds
        // from the copied recipient row using its unchanged native logic.
        nmr_old_personal_load(self,actData,playerData,native,mode,info);
    }catch(const std::exception& ex){nms_failure("personal-result",ex);}
}
void native_multiplayer_result_personal_setup(){hook(NMR_PERSONAL_LOAD,nmr_personal_load,nmr_old_personal_load);}
