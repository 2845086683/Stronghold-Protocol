// Keep the native input normalization/toast flow. Only a private CN room-button
// invocation may accept our six-digit server code before JoinTeam is reached.
#pragma once
inline constexpr CN_Method NRI_JOIN_CLICK={"Torappu.UI.AutoChess.AutoChessRoomSelectState","EventOnJoinTeam",0x602b755,0x2781d00ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NRI_CHECK_CODE={"Torappu.UI.AutoChess.AutoChessUtil","CheckIsTeamIdLegal",0x602b55d,0x2760ae0ULL,1,true,"System.Boolean",{"System.String"}};
using NRI_Click=void(*)(void*,const void*);using NRI_Check=bool(*)(void*,const void*);
NRI_Click nri_old_click=nullptr;NRI_Check nri_old_check=nullptr;
thread_local bool nri_private_input=false;
std::atomic<unsigned> nri_accepted{0};
void nri_join_click(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    const bool target=self&&can_read()&&private_view.load()&&unity_ui_thread.load()==GetCurrentThreadId()&&act(field_object(self,"m_actId"));
    struct Scope{bool previous;explicit Scope(bool value):previous(nri_private_input){nri_private_input=value;}~Scope(){nri_private_input=previous;}} scope(target);
    lease.release();nri_old_click(self,info);
}
bool nri_check_code(void* code,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;
    if(nri_private_input&&can_read()&&private_view.load()&&unity_ui_thread.load()==GetCurrentThreadId()){
        const auto value=text(code);
        if(value.size()==6&&value.find_first_not_of("0123456789")==std::string::npos){++nri_accepted;return true;}
    }
    lease.release();return nri_old_check(code,info);
}
void native_private_room_input_setup(){hook(NRI_JOIN_CLICK,nri_join_click,nri_old_click);hook(NRI_CHECK_CODE,nri_check_code,nri_old_check);}
