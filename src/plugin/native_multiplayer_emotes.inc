// Pinned CN panel ABI. Only the owned private activity reads the detached
// PlayerData view; the original panel still owns assets, paging and cooldown.
#pragma once
const CN_Method NME_PANEL={"Torappu.UI.Emoticon.EmoticonPanelBaseModel","ShowPanelLoadData",0x6024d98,0x1ff0960ULL,4,false,"System.Void",{"Torappu.UI.Emoticon.IEmoticonCustomConfig","Torappu.ValueBundle","Torappu.EmojiSceneType","Torappu.UI.Emoticon.GOPositionHolder"}};
using NME_Show=void(*)(void*,void*,void*,int32_t,void*,const void*);
NME_Show nme_old_show=nullptr;
void nme_show(void* self,void* config,void* bundle,int32_t scene,void* position,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    const bool owned=unity_ui_thread.load()==GetCurrentThreadId()&&can_read()&&scene==6&&config&&
        api.il2cpp_object_get_class(config)==cls("Torappu.UI.AutoChess.Battle.AutoChessEmojiController+EmojiConfig")&&
        nmd_scope(native_multiplayer_entry_mode());
    if(!owned){lease.release();nme_old_show(self,config,bundle,scene,position,info);return;}
    PrivateDataScope scope(true);nme_old_show(self,config,bundle,scene,position,info);
}
void native_multiplayer_emotes_setup(){
    uint32_t alignment=0;auto bundle=cls("Torappu.ValueBundle");
    if(!bundle||!api.il2cpp_class_is_valuetype(bundle)||api.il2cpp_class_value_size(bundle,&alignment)!=32)
        throw std::runtime_error("private emote ValueBundle ABI mismatch");
    nbb_field(cls("Torappu.PlayerDataModel"),"emoticon","Torappu.PlayerEmoticon",376);
    nbb_field(cls("Torappu.PlayerEmoticon"),"unlockTheme","System.Collections.Generic.List`1<System.String>",16);
    hook(NME_PANEL,nme_show,nme_old_show);
}
