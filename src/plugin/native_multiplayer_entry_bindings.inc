// Pinned CN room entry ABI. Runtime bind_method rechecks all tokens/signatures.
#pragma once
inline constexpr CN_Method CN_MULTI_ENTRY_JOIN={"Torappu.UI.AutoChess.AutoChessPrepareController","JoinTeam",0x602b3dd,0x271fc30ULL,1,false,"System.Void",{"System.String"}};
inline constexpr CN_Method CN_MULTI_ENTRY_RESPONSE={"Torappu.UI.AutoChess.AutoChessPrepareController","_OnJoinTeamRespHandle",0x602b3e1,0x2727230ULL,2,false,"System.Void",{"Torappu.UI.AutoChess.AutoChessServiceCommonResultType","Torappu.UI.AutoChess.AutoChessTeamInfo"}};
inline constexpr CN_Method CN_MULTI_ENTRY_START_GAME={"Torappu.UI.AutoChess.AutoChessPrepareController","_EventOnRoomStartGame",0x602b40f,0x2723570ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_DESTROY={"Torappu.UI.AutoChess.AutoChessPrepareController","OnDestroy",0x602b3ca,0x2720270ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_SERVER_START={"Torappu.UI.AutoChess.Server.AutoChessTeamServer","Start",0x602becd,0x280a8e0ULL,1,false,"System.Void",{"Torappu.SocketNetwork.SvrCom.CommonJoinEntry"}};
inline constexpr CN_Method CN_MULTI_ENTRY_MODE_REQUEST={"Torappu.UI.AutoChess.Server.AutoChessServiceMode","SendRequest",0x602bd21,0x27f1ff0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoChessServiceRequest"}};
inline constexpr CN_Method CN_MULTI_ENTRY_TEAM_FILL={"Torappu.UI.AutoChess.Server.AutoChessServiceTeamInfo","Fill",0x602bcf7,0x27f3c90ULL,2,false,"System.Void",{"System.String","Torappu.UI.AutoChess.Server.AutoChessTeamStatus"}};
inline constexpr CN_Method CN_MULTI_ENTRY_TEAM_CHANGED={"Torappu.UI.AutoChess.Server.AutoChessServiceMode","OnTeamStatusChanged",0x602bd2b,0x27f1d10ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_SERVICE_TIME={"Torappu.UI.AutoChess.Server.AutoChessService","get_currentTime",0x602bcdd,0x27f4eb0ULL,0,false,"System.DateTime",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_MODE_TIME={"Torappu.UI.AutoChess.Server.AutoChessServiceMode","get_currentTime",0x602bd14,0x27f2fd0ULL,0,false,"System.DateTime",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_TIMESTAMP={"Torappu.DateTimeUtil","TimeStampToDateTime",0x600016d,0x66e0340ULL,1,true,"System.DateTime",{"System.Int64"}};
inline constexpr CN_Method CN_MULTI_ENTRY_LEAVE_DONE={"Torappu.UI.AutoChess.AutoChessPrepareController","_HandleTeamLeaveRet",0x602b3e8,0x2725f90ULL,1,false,"System.Void",{"System.Object"}};
inline constexpr CN_Method CN_MULTI_ENTRY_LEAVE_PAGE={"Torappu.UI.AutoChess.AutoChessPrepareController","_LeavePreparePage",0x602b423,0x2726710ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_SERVICE_DISPOSE={"Torappu.UI.AutoChess.Server.AutoChessService","OnDispose",0x602bce1,0x27f3ef0ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_PREPARE_UPDATE={"Torappu.UI.AutoChess.AutoChessPrepareController","Update",0x602b3c9,0x2721910ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_ROOM_REFRESH={"Torappu.UI.AutoChess.AutoChessRoomViewModel","RefreshData",0x602b73d,0x27839d0ULL,2,false,"Torappu.UI.AutoChess.AutoChessRoomViewModel+RefreshDataResult",{"Torappu.UI.AutoChess.AutoChessPrepareModel","System.Collections.Generic.List`1<System.Collections.Generic.KeyValuePair`2<System.String,System.String>>"}};
inline constexpr CN_Method CN_MULTI_ENTRY_BACK_TO_TEAM={"Torappu.UI.AutoChess.Server.AutoChessServiceMode","_SwitchBackToTeam1",0x602bd26,0x27f2d00ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_SCENE_READY={"Torappu.UI.AutoChess.Server.AutoChessServiceMode","TriggerEvent",0x602bd2a,0x27f2270ULL,2,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoChessServiceEvent","System.Object"}};
inline constexpr CN_Method CN_MULTI_ENTRY_RECONNECT_DONE={"Torappu.UI.AutoChess.AutoChessPrepareController","_ReconnectBattleDone",0x602b3e0,0x2727ab0ULL,1,false,"System.Void",{"System.Boolean"}};
inline constexpr CN_Method CN_MULTI_ENTRY_LEAVE_REQUEST={"Torappu.UI.AutoChess.AutoChessPrepareController","_SendLeaveTeamReq",0x602b41a,0x2729640ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_MULTI_ENTRY_BATTLE_EXCEPTION={"Torappu.UI.AutoChess.Server.AutoChessBattleServer","_SetException",0x602bea9,0x27fab10ULL,2,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoChessBattleServer+AutoChessBattleException","System.Int32"}};
inline constexpr CN_Method CN_MULTI_ENTRY_BATTLE_LOST={"Torappu.UI.AutoChess.Server.AutoChessBattleServer","OnConnectionLost",0x602bea7,0x27f8d30ULL,1,false,"System.Void",{"Torappu.SocketNetwork.ServerBase.Server+NetLostType"}};
