// Private zero-based owner adaptation at the native squad lookup boundary.
// The original model keeps every conversion, skill/module choice and cache.
// Include after native_preparation_tokens.inc and the MULTI observation helpers.
#pragma once
static const CN_Method NMSQ_METHODS[]={
{"Torappu.Battle.DataCenter.AutoChessDataCenter","GetChessSquad",0x6013474,0xb0fdc0ULL,2,false,"Torappu.Battle.AutoChess.ChessSquad",{"System.String","System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessSquadModel","GetChessSquad",0x6013942,0xbab820ULL,2,false,"Torappu.Battle.AutoChess.ChessSquad",{"System.String","System.Int32"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","get_playerViewIndex",0x601343f,0xb14700ULL,0,false,"System.Int32",{}},
};
using NMSQGet=void*(*)(void*,void*,int32_t,const void*);
NMSQGet nmsq_original_get=nullptr;
std::atomic<unsigned> nmsq_zero_lookups{0},nmsq_failures{0};

bool nmsq_zero_owner(void* centre){
    if(!native_multiplayer_entry_active()||centre!=nb_centre()||!npt_scene())return false;
    const auto& all=nb_at(nb_at(nb_at(nb_at(nmo_snapshot,"dtoProjection"),"allStateSync"),"payload"),"allStateSyncData");
    const auto& players=nb_at(all,"staticPlayers");
    if(players.kind!=cnjson::Value::Array||players.array.size()>4)throw std::runtime_error("multiplayer squad static players invalid");
    std::set<int64_t> seats;
    for(const auto& row:players.array){auto seat=nb_number(row,"index");if(seat<0||seat>3||!seats.insert(seat).second)throw std::runtime_error("multiplayer squad static owner invalid");}
    return seats.count(0)!=0;
}
void* nmsq_get(void* self,void* chess_id,int32_t player,const void* info){
    RuntimeLease lease(runtime_gate);
    if(!lease||closing.load()||!can_read()||unity_ui_thread.load()!=GetCurrentThreadId()){lease.release();return nmsq_original_get(self,chess_id,player,info);}
    bool owned=false;
    try{
        owned=native_multiplayer_entry_active()&&self==nb_centre();
        if((player==0||player==-1)&&owned&&nmsq_zero_owner(self)){
            int32_t owner=player;
            if(player==-1){
                // Native default callers pass -1, not 0. Resolve the actual
                // current view without treating its valid seat zero as unset.
                auto boxed=invoke(bound_methods.at(0x601343f),self);
                if(!boxed||!read(unbox(boxed),owner))throw std::runtime_error("multiplayer squad native view index unavailable");
            }
            if(owner==0){
                TemporaryRoots roots;roots.retain(self);roots.retain(chess_id);
                auto field=profile_field(api.il2cpp_object_get_class(self),"m_chessSquadModel");
                if((api.il2cpp_field_get_flags(field)&16)||!matches_type(api.il2cpp_field_get_type(field),"Torappu.Battle.AutoChess.AutoChessSquadModel"))throw std::runtime_error("multiplayer squad model field contract changed");
                auto model=roots.retain(field_object(self,"m_chessSquadModel"));
                if(!model||api.il2cpp_object_get_class(model)!=cls("Torappu.Battle.AutoChess.AutoChessSquadModel"))throw std::runtime_error("multiplayer squad model unavailable");
                void* args[]={chess_id,&owner};
                auto squad=invoke(bound_methods.at(0x6013942),model,args);
                if(squad&&api.il2cpp_object_get_class(squad)!=cls("Torappu.Battle.AutoChess.ChessSquad"))throw std::runtime_error("multiplayer squad model result contract changed");
                ++nmsq_zero_lookups;return squad;
            }
        }
    }catch(const std::exception& error){++nmsq_failures;nb_error("multiplayer-squad-owner",error);if(owned)return nullptr;}
    lease.release();return nmsq_original_get(self,chess_id,player,info);
}
void native_multiplayer_squad_owner_setup(){
    hook(NMSQ_METHODS[0],nmsq_get,nmsq_original_get);
    bind_method(NMSQ_METHODS[1]);bind_method(NMSQ_METHODS[2]);
}
