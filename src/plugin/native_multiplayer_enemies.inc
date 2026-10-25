// Original Init also invokes LOCAL random replacement, even with MULTI data.
// Reuse the verified server-list initialization used by independent simulation.
using NMEInit=void(*)(void*,void*,const void*);
NMEInit nme_original_init=nullptr;
std::atomic<unsigned> nme_initializations{0},nme_failures{0};
bool native_multiplayer_battle_buff_templates_scope(){
 if(!native_multiplayer_entry_active()||!nmd_scope(native_multiplayer_entry_mode()))return false;
 auto manager=nb_manager(),centre=nb_centre();if(!manager||!centre)return false;
 auto bridge=field_object(manager,"dataBridge");
 return act(field_object(centre,"<actId>k__BackingField"))&&field_object(manager,"dataCenter")==centre&&
     bridge&&api.il2cpp_object_get_class(bridge)==cls("Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer");
}
void native_multiplayer_enemy_init(void* self,void* data,const void* info){
 RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
 if(!nmd_scope(native_multiplayer_entry_mode())){lease.release();nme_original_init(self,data,info);return;}
 try{
  nb_main_thread();TemporaryRoots roots;roots.retain(self);roots.retain(data);
  auto manager=roots.retain(nb_manager()),centre=roots.retain(nb_centre());
  auto bridge=roots.retain(field_object(manager,"dataBridge"));
  if(!manager||!centre||!act(field_object(centre,"<actId>k__BackingField"))||
     self!=field_object(manager,"levelEnemyManager")||!bridge||
     api.il2cpp_object_get_class(bridge)!=cls("Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer")||
     !data||api.il2cpp_object_get_class(data)!=cls("Torappu.Battle.AutoChess.SceneRoundEnemyData")||
     data!=nb_container_model(native_multiplayer_dispatch_scene(),"roundEnemyData"))
   throw std::runtime_error("private MULTI enemy initialization owner/data mismatch");
  PrivateDataScope scope(true);const char* step="MULTI enemies";
  native_multiplayer_battle_buff_templates_preflight();
  nb_initialize_server_enemies(self,step,data);++nme_initializations;
 }catch(const std::exception& ex){++nme_failures;nb_error("multiplayer-enemy-initialization",ex);}
}
void native_multiplayer_enemies_setup(){
 const CN_Method spec={"Torappu.Battle.AutoChess.AutoChessLevelEnemyManager","Init",0x601382f,0xb636f0ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.SceneRoundEnemyData"}};
 auto selected=bind_method(spec);void* pointer=nullptr;
 if(!read(selected,pointer)||reinterpret_cast<uintptr_t>(pointer)-reinterpret_cast<uintptr_t>(runtime)!=spec.rva)
  throw std::runtime_error("private MULTI enemy Init RVA differs");
 std::vector<native_mixed_help::HookInstall> created;
 native_mixed_help::create_exact(selected,native_multiplayer_enemy_init,nme_original_init,created);
}
