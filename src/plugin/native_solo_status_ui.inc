// Private SINGLE presentation only. Keep the native WAIT_FINISH state and flow.
#pragma once
inline constexpr CN_Method CN_SOLO_STATUS_VALUE={"Torappu.UI.AutoChess.Battle.AutoChessBattleUIBottomTipsPanel","OnValueChanged",0x602bfd1,0x2818ad0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModelProperty"}};
using NativeSoloStatusValue=void(*)(void*,void*,const void*);
NativeSoloStatusValue nsui_old_value=nullptr;
std::atomic<unsigned> nsui_overrides{0},nsui_failures{0};

void nsui_value(void* self,void* property,const void* info){
    RuntimeLease lease(runtime_gate);
    if(!lease||closing.load()){lease.release();nsui_old_value(self,property,info);return;}
    // The original consumer renders its configured text synchronously. Keep its
    // animation, visibility, cached enum and exception semantics intact.
    nsui_old_value(self,property,info);
    if(!can_read()||!private_view.load()||!cncombat::mode_allowed(nb_selected_mode))return;
    try{
        if(!self||!property||api.il2cpp_object_get_class(self)!=cls(CN_SOLO_STATUS_VALUE.type)||
           api.il2cpp_object_get_class(property)!=cls("Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModelProperty"))return;
        auto getter=method(api.il2cpp_object_get_class(property),"get_Value",0);uint32_t impl=0;
        if(!getter||(api.il2cpp_method_get_flags(getter,&impl)&16)||
           !matches_type(api.il2cpp_method_get_return_type(getter),"Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModel"))
            throw std::runtime_error("private solo status property getter mismatch");
        TemporaryRoots roots;auto view=roots.retain(invoke(getter,property));
        if(!npb_hud_owner(view))return;
        npb_field(view,"<bottomTipsViewModel>k__BackingField","Torappu.UI.AutoChess.Battle.AutoChessBattleUIBottomTipsViewModel");
        auto model=roots.retain(field_object(view,"<bottomTipsViewModel>k__BackingField"));
        if(!model||api.il2cpp_object_get_class(model)!=cls("Torappu.UI.AutoChess.Battle.AutoChessBattleUIBottomTipsViewModel"))
            throw std::runtime_error("private solo status model missing");
        auto show_field=npb_field(model,"<showType>k__BackingField","Torappu.UI.AutoChess.Battle.AutoChessBattleUIBottomTipsShowType");
        int32_t show_type=0;api.il2cpp_field_get_value(model,show_field,&show_type);
        if(show_type!=3)return; // Pinned CN WAIT_FINISH; never alter native state.
        npb_field(self,"_txtTip","UnityEngine.UI.Text");auto label=roots.retain(field_object(self,"_txtTip"));
        if(!label||api.il2cpp_object_get_class(label)!=cls("UnityEngine.UI.Text"))
            throw std::runtime_error("private solo status text component missing");
        auto setter=method(api.il2cpp_object_get_class(label),"set_text",1);
        if(!setter||(api.il2cpp_method_get_flags(setter,&impl)&16)||
           !matches_type(api.il2cpp_method_get_return_type(setter),"System.Void")||
           !matches_type(api.il2cpp_method_get_param(setter,0),"System.String"))
            throw std::runtime_error("private solo status text setter mismatch");
        auto message=roots.retain(string_new("等待本轮结算"));void* args[]={message};invoke(setter,label,args);
        if(nsui_overrides.fetch_add(1)==0)report("private-single-wait-finish-label-adapted");
    }catch(const std::exception& error){
        if(nsui_failures.fetch_add(1)==0)report((std::string("private-single-wait-finish-label-failed: ")+error.what()).c_str());
    }
}
std::string native_solo_status_ui_status_json(){
    return "{\"overrides\":"+std::to_string(nsui_overrides.load())+",\"failures\":"+std::to_string(nsui_failures.load())+"}";
}
void native_solo_status_ui_setup(){hook(CN_SOLO_STATUS_VALUE,nsui_value,nsui_old_value);}
