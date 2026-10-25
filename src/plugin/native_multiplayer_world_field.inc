// A field is a native Map/MapGraphic inside the existing Unity battle scene.
// Changing its authored source must not create another BattleController/loader.
#pragma once
void native_multiplayer_clear_preparation_dummies(){
 nb_main_thread();
 // Dummy HUDs are keyed by Tile references. Finish them while the old view
 // and its Tile table are still installed, so the original Remove event can
 // hide/recycle rank and SP widgets before a view/map replaces those keys.
 static const CN_Method finish={"Torappu.Battle.AutoChess.AutoChessDummyManager","FinishAllDummyImmediately",0x60137ee,0xb5a060ULL,0,false,"System.Void",{}};
 auto selected=bind_method(finish);auto dummy=field_object(nb_manager(),"dummyManager");
 if(!dummy||api.il2cpp_object_get_class(dummy)!=cls("Torappu.Battle.AutoChess.AutoChessDummyManager"))throw std::runtime_error("native view dummy manager owner missing");
 invoke(selected,dummy);
}
namespace native_world_field {
// Read-only snapshots at authored map handoffs, never a new per-frame hook.
// Tile.Start / MapArea._ShowTile own native gate effects; MapGraphic attachment
// is a separate lifecycle and must not be used as evidence that a gate exists.
struct GateTrace {
 std::mutex mutex;std::vector<cnjson::Value> records;std::string firstException;
 uint32_t dataRoot=0;std::vector<mixedhelp::Position> gates;uint64_t sequence=0;
 void clear(){if(dataRoot)hold_free(dataRoot);dataRoot=0;gates.clear();std::lock_guard<std::mutex> lock(mutex);records.clear();firstException.clear();sequence=0;}
};
GateTrace gateTrace;
// Keep route evidence after scene teardown; these counters never write render state.
std::atomic<unsigned> sceneGraphicBorrows{0}, firstHelpSceneAdoptions{0};
cnjson::Value trace_object(){cnjson::Value v;v.kind=cnjson::Value::Object;return v;}
cnjson::Value trace_text(const std::string& s){cnjson::Value v;v.kind=cnjson::Value::String;v.string=s;return v;}
cnjson::Value trace_number(int64_t n){cnjson::Value v;v.kind=cnjson::Value::Integer;v.integer=n;return v;}
cnjson::Value trace_bool(bool b){cnjson::Value v;v.kind=cnjson::Value::Boolean;v.boolean=b;return v;}
cnjson::Value trace_identity(void* p){return trace_text(std::to_string(reinterpret_cast<uintptr_t>(p)));}
void trace_exception(const std::string& error)noexcept{try{std::lock_guard<std::mutex> lock(gateTrace.mutex);if(gateTrace.firstException.empty())gateTrace.firstException=error.substr(0,512);}catch(...){}}
struct Cached {
 mixedhelp::Stage source;void* root=nullptr,*graphic=nullptr;
};
struct Owner {
 void* controller=nullptr,*map=nullptr,*originalGraphic=nullptr;
 std::string originalStage,stage,level;uint64_t epoch=0;
 std::vector<uint32_t> roots,currentRoots;std::map<std::string,Cached> sources;std::map<std::string,cnfield::Source> sourceProofs;
 void clear(){gateTrace.clear();for(auto h:currentRoots)hold_free(h);for(auto h:roots)hold_free(h);currentRoots.clear();roots.clear();sources.clear();sourceProofs.clear();controller=map=originalGraphic=nullptr;originalStage.clear();stage.clear();level.clear();++epoch;}
 void* keep(void* value,bool current=false){if(!value)throw std::runtime_error("native field retained source is null");auto h=hold_new(value,false);if(!h||hold_get(h)!=value){if(h)hold_free(h);throw std::runtime_error("native field source root failed");}(current?currentRoots:roots).push_back(h);return value;}
};
Owner owner;
void trace(const char* phase)noexcept{
 try{
  nb_main_thread();if(!nff_adapter)return;auto controller=nmo_controller(),map=nff_map(controller);
  // All callers are existing private field callbacks. Refuse a stale native
  // owner before dereferencing its map or performing any diagnostic invokes.
  if(!controller||!map||(owner.controller&&(owner.controller!=controller||owner.map!=map)))return;
  TemporaryRoots roots;roots.retain(controller);roots.retain(map);auto data=roots.retain(field_object(map,"m_data"));if(!data)return;
  if(!gateTrace.dataRoot||hold_get(gateTrace.dataRoot)!=data){
   std::vector<mixedhelp::Position> positions;auto width=nff_adapter->integer(nff_adapter->invoke(mixedhelp::Method::MapWidth,data,{})),height=nff_adapter->integer(nff_adapter->invoke(mixedhelp::Method::MapHeight,data,{}));
   if(width!=21||height!=19)throw std::runtime_error("gate trace refused non-catalog map dimensions");
   for(int32_t row=0;row<height;++row)for(int32_t col=0;col<width;++col){auto tile=roots.retain(nff_adapter->invoke(mixedhelp::Method::MapDataItem,data,{mixedhelp::Arg::i32(row),mixedhelp::Arg::i32(col)}));auto key=text(field_object(tile,"tileKey"));if(key=="tile_start"||key=="tile_end"||key=="tile_flystart"||key=="tile_start_cooperate"||key=="tile_end_cooperate"){if(positions.size()>=16)throw std::runtime_error("gate trace exceeds fixed functional-tile bound");positions.push_back({row,col});}}
   auto retained=hold_new(data,false);if(!retained||hold_get(retained)!=data){if(retained)hold_free(retained);throw std::runtime_error("gate trace could not retain its current map data");}
   if(gateTrace.dataRoot)hold_free(gateTrace.dataRoot);gateTrace.dataRoot=retained;gateTrace.gates=std::move(positions);
  }
  static const CN_Method hidden={"Torappu.Battle.Tile","get_isHidden",0x6011411,0x8c5e60ULL,0,false,"System.Boolean",{}};
  static const CN_Method holder={"Torappu.Battle.Tile","get_hasEffectHolder",0x601141e,0x8c5960ULL,0,false,"System.Boolean",{}};
  auto hiddenMethod=bind_method(hidden),holderMethod=bind_method(holder);auto record=trace_object();record.object["phase"]=trace_text(phase?phase:"");record.object["controller"]=trace_identity(controller);record.object["map"]=trace_identity(map);record.object["mapData"]=trace_identity(data);record.object["mapGraphic"]=trace_identity(field_object(map,"_graphic"));record.object["levelData"]=trace_identity(field_object(controller,"m_levelData"));record.object["stage"]=trace_text(owner.stage);record.object["fieldEpoch"]=trace_number(owner.epoch);cnjson::Value samples;samples.kind=cnjson::Value::Array;
  for(auto pos:gateTrace.gates){auto tile=roots.retain(nff_adapter->invoke(mixedhelp::Method::MapGetTile,map,{mixedhelp::Arg::grid(pos)}));if(!tile)throw std::runtime_error("gate trace current map is missing a functional tile");auto sample=trace_object();sample.object["row"]=trace_number(pos.row);sample.object["col"]=trace_number(pos.col);sample.object["tile"]=trace_identity(tile);sample.object["tileKey"]=trace_text(text(field_object(field_object(tile,"_data"),"tileKey")));sample.object["effectKey"]=trace_text(text(field_object(tile,"_effect")));sample.object["effectHolder"]=trace_identity(field_object(tile,"m_effectHolder"));sample.object["hasEffectHolder"]=trace_bool(nff_adapter->boolean(invoke(holderMethod,tile)));sample.object["isHidden"]=trace_bool(nff_adapter->boolean(invoke(hiddenMethod,tile)));samples.array.push_back(std::move(sample));}
  record.object["gates"]=std::move(samples);std::lock_guard<std::mutex> lock(gateTrace.mutex);record.object["sequence"]=trace_number(++gateTrace.sequence);if(gateTrace.records.size()>=16)gateTrace.records.erase(gateTrace.records.begin());gateTrace.records.push_back(std::move(record));
 }catch(const std::exception& error){trace_exception(std::string(phase?phase:"")+": "+error.what());}catch(...){trace_exception(std::string(phase?phase:"")+": native gate trace failed");}
}
std::string trace_json(){std::lock_guard<std::mutex> lock(gateTrace.mutex);auto value=trace_object();value.object["readOnly"]=trace_bool(true);value.object["perFrameHook"]=trace_bool(false);value.object["sceneGraphicBorrows"]=trace_number(sceneGraphicBorrows.load());value.object["firstHelpSceneAdoptions"]=trace_number(firstHelpSceneAdoptions.load());value.object["firstException"]=trace_text(gateTrace.firstException);value.object["samples"].kind=cnjson::Value::Array;value.object["samples"].array=gateTrace.records;return cnjson::dump(value);}
static const CN_Method METHODS[]={
 {"Torappu.Battle.AutoChess.AutoChessGameManager","_FinishAllUnits",0x60136b6,0xb45860ULL,0,false,"System.Void",{}},
 {"Torappu.Battle.AutoChess.AutoChessMapAreaManager","Start",0x6013863,0xb67140ULL,0,false,"System.Void",{}},
 {"Torappu.Battle.Map","_InitTilesAndWidgets",0x6011198,0x884540ULL,2,false,"System.Void",{"Torappu.MapData","System.Collections.Generic.IList`1<Torappu.GridPosition>"}},
 {"Torappu.Battle.Map","_InitRoutesAndPassableMaps",0x6011199,0x883ab0ULL,2,false,"System.Void",{"Torappu.RouteData[]","Torappu.RouteData[]"}},
 {"Torappu.Battle.AutoChess.AutoChessDummyManager","FinishAllDummyImmediately",0x60137ee,0xb5a060ULL,0,false,"System.Void",{}},
 {"Torappu.Battle.AutoChess.AutoChessEnemyPreviewManager","Unload",0x6013815,0xb60420ULL,0,false,"System.Void",{}},
 {"Torappu.Battle.AutoChess.AutoChessEnemyPreviewManager",".ctor",0x601381b,0xb61e50ULL,0,false,"System.Void",{}},
};
void require_field(void* object,const char* name,const char* expected){if(!object||!matches_type(api.il2cpp_field_get_type(profile_field(api.il2cpp_object_get_class(object),name)),expected))throw std::runtime_error("native continuous field declaration differs");}
void adopt(void* controller,void* map){
 if(owner.controller==controller&&owner.map==map)return;
 owner.clear();if(!controller||!map||controller!=nmo_controller()||nff_map(controller)!=map)throw std::runtime_error("native continuous field owner is unavailable");
 nff_ensure_adapter();for(const auto& spec:METHODS)bind_method(spec);
 require_field(controller,"m_levelData","Torappu.LevelData");require_field(controller,"m_mapData","Torappu.MapData");
 require_field(map,"_graphic","Torappu.Battle.MapGraphic");require_field(map,"m_data","Torappu.MapData");
 TemporaryRoots roots;auto inout=roots.retain(invoke(method(cls("Torappu.Battle.BattleInOut"),"get_instance",0),nullptr));auto input=roots.retain(invoke(nmo_method(0x600f5cc),inout));
 auto info=roots.retain(native_battle_boxed_field(input,"stageInfo",roots)),level=roots.retain(field_object(input,"levelData"));
 const auto stage=text(field_object(info,"stageId"));if(stage.empty()||!level||native_multiplayer_resolve_stage_path(stage,text(field_object(level,"levelId")),CN_PRIVATE_LEVELS).empty())throw std::runtime_error("native continuous field lacks its signed initial source");
 owner.controller=owner.keep(controller);owner.map=owner.keep(map);owner.originalGraphic=owner.keep(field_object(map,"_graphic"));owner.originalStage=owner.stage=stage;owner.level=native_multiplayer_stage_path(stage,CN_PRIVATE_LEVELS);
}
Cached& cache(const mixedhelp::Stage& input){
 if(input.stageId.empty()||input.levelId.empty()||input.graphicSha256.size()!=64||!input.levelData||!input.mapData||!input.graphicPrefab||native_multiplayer_resolve_stage_path(input.stageId,input.levelId,CN_PRIVATE_LEVELS).empty())throw std::runtime_error("native continuous field source is outside the signed catalog");
 auto found=owner.sources.find(input.stageId);if(found!=owner.sources.end()){if(found->second.source.graphicSha256!=input.graphicSha256||found->second.source.levelId!=input.levelId)throw std::runtime_error("native continuous field cached source changed");return found->second;}
 if(owner.sources.size()>=8)throw std::runtime_error("native continuous field cache exceeds catalog");
 Cached value;value.source=input;for(auto p:{input.levelData,input.mapData,input.graphicPrefab})owner.keep(p);
 if(input.stageId==owner.originalStage){value.graphic=owner.originalGraphic;value.root=owner.keep(nff_adapter->invoke(mixedhelp::Method::GetGameObject,value.graphic,{}));}
 else{
  auto container=owner.keep(nff_adapter->invoke(mixedhelp::Method::GetGraphicContainer,owner.map,{}));
  value.root=owner.keep(nff_adapter->invoke(mixedhelp::Method::InstantiateParent,nullptr,{mixedhelp::Arg::ref(input.graphicPrefab),mixedhelp::Arg::ref(container),mixedhelp::Arg::flag(false)}));
  auto type=owner.keep(nff_adapter->type_object("Torappu.Battle.MapGraphic"));value.graphic=owner.keep(nff_adapter->invoke(mixedhelp::Method::GetComponent,value.root,{mixedhelp::Arg::ref(type)}));
  nff_adapter->unbind_scene_lighting(value.root);nff_adapter->invoke(mixedhelp::Method::SetActive,value.root,{mixedhelp::Arg::flag(false)});
 }
 return owner.sources.emplace(input.stageId,std::move(value)).first->second;
}
void* clone_map(void* source){
 nff_adapter->validate_map_schema(source);auto copy=owner.keep(nff_adapter->allocate("Torappu.MapData"),true);nff_adapter->copy_fields(copy,source);
 auto matrix=owner.keep(nff_adapter->invoke(mixedhelp::Method::ArrayClone,field_object(source,"map"),{}),true);
 auto oldTiles=field_object(source,"tiles");const auto count=nff_adapter->array_size(oldTiles);if(count>32768)throw std::runtime_error("native continuous field tile table exceeds bound");
 auto tiles=owner.keep(nff_adapter->array("Torappu.TileData",count),true);
 for(size_t i=0;i<count;++i){auto row=owner.keep(nff_adapter->invoke(mixedhelp::Method::ArrayGetOne,oldTiles,{mixedhelp::Arg::i32(int32_t(i))}),true);auto clone=owner.keep(nff_adapter->invoke(mixedhelp::Method::TileClone,row,{}),true);if(clone==row)throw std::runtime_error("native continuous field TileData clone aliases catalog");nff_adapter->invoke(mixedhelp::Method::ArraySetOne,tiles,{mixedhelp::Arg::ref(clone),mixedhelp::Arg::i32(int32_t(i))});}
 nff_adapter->field(copy,"map",matrix);nff_adapter->field(copy,"tiles",tiles);
 for(const char* key:{"blockEdges","effects","tags","layerRects"})if(auto value=field_object(source,key))nff_adapter->field(copy,key,owner.keep(nff_adapter->invoke(mixedhelp::Method::ArrayClone,value,{}),true));
 return copy;
}
struct Terrain {
 static constexpr const char* fields[]={"levelPredefineData","m_originPredefinedTokens","m_predefinedTokens","m_originPredefinedChars","m_predefinedChars","originLevelRunes"};
 std::array<void*,6> values{};
 void install(void* model)const{for(size_t i=0;i<values.size();++i)set_object_field(model,profile_field(api.il2cpp_object_get_class(model),fields[i]),values[i]);}
};
Terrain terrain_snapshot(void* model,TemporaryRoots& roots){
 require_field(model,"levelPredefineData","Torappu.LevelData+PredefinedData");
 for(const char* key:{"m_originPredefinedTokens","m_predefinedTokens","m_originPredefinedChars","m_predefinedChars"})require_field(model,key,"System.Collections.Generic.List`1<Torappu.LevelData+PredefinedData+PredefinedCharacter>");
 require_field(model,"originLevelRunes","System.Collections.Generic.List`1<Torappu.LegacyInLevelRuneData>");
 Terrain result;for(size_t i=0;i<result.values.size();++i)result.values[i]=roots.retain(field_object(model,Terrain::fields[i]));return result;
}
Terrain clone_terrain(void* model,void* level){
 // Model.Init seeds origins from its initial LevelData exactly once. Later
 // Prepare only _ResetPredefines() copies those origins. An in-scene source
 // switch must replace that source too, or terrain effects enumerate the last
 // player's platforms/tokens despite displaying the new Map/MapGraphic.
 Terrain result;auto source=field_object(level,"predefines");
 if(!source)throw std::runtime_error("native continuous field target predefines absent");
 auto data=owner.keep(object_new(api.il2cpp_object_get_class(source)),true);invoke(nff_method(0x6007b42),data);copy_native_fields(data,source);result.values[0]=data;
 std::map<void*,void*> rows;
 for(size_t k=0;k<2;++k){
  const char* key=k?"characterInsts":"tokenInsts";auto original=field_object(source,key);
  if(!original)throw std::runtime_error("native continuous field target predefine array absent");
  const auto count=nff_adapter->array_size(original);if(count>4096)throw std::runtime_error("native continuous field predefine count exceeds bound");
  auto array=owner.keep(nff_adapter->array("Torappu.LevelData+PredefinedData+PredefinedCharacter",count),true);
  const size_t index=1+k*2;auto originalList=field_object(model,Terrain::fields[index]);if(!originalList)throw std::runtime_error("native continuous field original predefine list absent");auto listClass=api.il2cpp_object_get_class(originalList);
  auto origins=owner.keep(object_new(listClass),true),current=owner.keep(object_new(listClass),true);
  invoke(method(listClass,".ctor",0),origins);invoke(method(listClass,".ctor",0),current);auto add=method(listClass,"Add",1);
  for(size_t i=0;i<count;++i){
   auto row=nff_adapter->invoke(mixedhelp::Method::ArrayGetOne,original,{mixedhelp::Arg::i32(int32_t(i))});
   if(!row)throw std::runtime_error("native continuous field target predefined row absent");
   auto& clone=rows[row];if(!clone){clone=owner.keep(object_new(api.il2cpp_object_get_class(row)),true);invoke(nff_method(0x6007b49),clone);copy_native_fields(clone,row);}
   nff_adapter->invoke(mixedhelp::Method::ArraySetOne,array,{mixedhelp::Arg::ref(clone),mixedhelp::Arg::i32(int32_t(i))});
   void* args[]={clone};invoke(add,origins,args);invoke(add,current,args);
  }
  set_object_field(data,profile_field(api.il2cpp_object_get_class(data),key),array);result.values[index]=origins;result.values[index+1]=current;
 }
 for(const char* key:{"tokenCards","characterCards"})if(auto cards=field_object(source,key))set_object_field(data,profile_field(api.il2cpp_object_get_class(data),key),owner.keep(nff_adapter->invoke(mixedhelp::Method::ArrayClone,cards,{}),true));
 auto originalRunes=field_object(model,"originLevelRunes");if(!originalRunes)throw std::runtime_error("native continuous field original Rune list absent");auto runesClass=api.il2cpp_object_get_class(originalRunes);auto runes=owner.keep(object_new(runesClass),true);invoke(method(runesClass,".ctor",0),runes);
 if(auto original=field_object(level,"runes")){const auto count=nff_adapter->array_size(original);if(count>4096)throw std::runtime_error("native continuous field Rune count exceeds bound");auto add=method(runesClass,"Add",1);for(size_t i=0;i<count;++i){void* args[]={nff_adapter->invoke(mixedhelp::Method::ArrayGetOne,original,{mixedhelp::Arg::i32(int32_t(i))})};invoke(add,runes,args);}}
 result.values[5]=runes;set_object_field(level,profile_field(api.il2cpp_object_get_class(level),"predefines"),data);return result;
}
void import(const mixedhelp::Stage& input,bool force){
 nb_main_thread();auto controller=nmo_controller(),map=nff_map(controller);adopt(controller,map);if(!force&&owner.stage==input.stageId)return;
 auto& target=cache(input);TemporaryRoots roots;
 auto oldGraphic=roots.retain(field_object(map,"_graphic")),oldData=roots.retain(field_object(controller,"m_mapData")),oldLevel=roots.retain(field_object(controller,"m_levelData"));
 auto oldRoot=roots.retain(nff_adapter->invoke(mixedhelp::Method::GetGameObject,oldGraphic,{}));auto model=roots.retain(field_object(nb_centre(),"battleData"));
 if(!model)throw std::runtime_error("native continuous field has no battle model");require_field(model,"<levelData>k__BackingField","Torappu.LevelData");
 auto oldModelLevel=roots.retain(field_object(model,"<levelData>k__BackingField"));auto oldTerrain=terrain_snapshot(model,roots);
 std::vector<uint32_t> previous;previous.swap(owner.currentRoots);
 try{
  auto data=clone_map(target.source.mapData);auto level=owner.keep(object_new(api.il2cpp_object_get_class(target.source.levelData)),true);invoke(nff_method(0x6007b1a),level);copy_native_fields(level,target.source.levelData);set_object_field(level,profile_field(api.il2cpp_object_get_class(level),"mapData"),data);auto terrain=clone_terrain(model,level);
  // The native preview caches spawn grids once per manager, not per MapData.
  // Retire its coroutine/enemies against the old field, then let a fresh native
  // preview initialize lazily from the new field. Resetting only m_inited would
  // append to its old grid dictionaries. The battle manager itself is retained.
  auto manager=roots.retain(nb_manager());require_field(manager,"enemyPreviewer","Torappu.Battle.AutoChess.AutoChessEnemyPreviewManager");
  auto preview=roots.retain(field_object(manager,"enemyPreviewer"));
  if(!preview||api.il2cpp_object_get_class(preview)!=cls("Torappu.Battle.AutoChess.AutoChessEnemyPreviewManager"))throw std::runtime_error("native continuous field preview owner missing");
  auto nextPreview=roots.retain(object_new(api.il2cpp_object_get_class(preview)));invoke(bound_methods.at(0x601381b),nextPreview);
  invoke(bound_methods.at(0x6013815),preview);
  // Only a real authored field change finishes units. Ordinary board, shop,
  // ready, clock and peer-status deltas never enter this function.
  native_multiplayer_clear_preparation_dummies();
  invoke(bound_methods.at(0x60136b6),nb_manager());
  // FinishWithNoReason uses native reason 7, which DestroyEntityToDummy turns
  // into dead-like dummies as well (only reasons 10/11 are excluded). Retire
  // those teardown-only dummies while their old Tile keys still exist. Real
  // battle knockdowns never enter this authored-field replacement path.
  native_multiplayer_clear_preparation_dummies();
  trace("own-after-unit-retirement");
  nff_adapter->invoke(mixedhelp::Method::SetActive,oldRoot,{mixedhelp::Arg::flag(false)});set_object_field(map,profile_field(api.il2cpp_object_get_class(map),"_graphic"),target.graphic);
  nff_adapter->invoke(mixedhelp::Method::SetActive,target.root,{mixedhelp::Arg::flag(true)});
  set_object_field(controller,profile_field(api.il2cpp_object_get_class(controller),"m_levelData"),level);set_object_field(controller,profile_field(api.il2cpp_object_get_class(controller),"m_mapData"),data);set_object_field(map,profile_field(api.il2cpp_object_get_class(map),"m_data"),data);set_object_field(model,profile_field(api.il2cpp_object_get_class(model),"<levelData>k__BackingField"),level);terrain.install(model);
  auto factory=roots.retain(nff_adapter->invoke(mixedhelp::Method::GetFactory,controller,{}));if(!factory)throw std::runtime_error("native continuous field factory unavailable");
  nff_adapter->invoke(mixedhelp::Method::MapImport,map,{mixedhelp::Arg::ref(data),mixedhelp::Arg::ref(factory),mixedhelp::Arg::flag(true),mixedhelp::Arg::flag(false),mixedhelp::Arg::flag(true),mixedhelp::Arg::flag(false)});
  trace("own-after-import");
  // Only authored MapGraphic meshes render the floor. Entry/exit gates are
  // separate Tile effects, preserved by ClearAllGraphic and generated by
  // Tile.Start / MapArea._ShowTile. The adapter recycles old effects first.
  // ImportData's final attachGraphic=false preserves the authored graphic
  // owner. As in the proven mixed-map pipeline, explicitly bind every graphic
  // to the newly imported native Tiles before any view/prepare can consume it.
  if(!nff_adapter->boolean(nff_adapter->invoke(mixedhelp::Method::GraphicAttach,target.graphic,{mixedhelp::Arg::flag(true)})))throw std::runtime_error("native continuous field graphic failed to attach to imported tiles");
  // ImportData creates Tiles, but does not rebind MapWidgets/listeners or
  // rebuild routes onto those new Tiles. These are the same two native steps
  // called by Map.Init, without resetting battle RNG or creating a new scene.
  // In particular the original red/blue gates must survive Help -> own field.
  void* widgetArgs[]={data,nullptr};invoke(bound_methods.at(0x6011198),map,widgetArgs);
  auto routes=roots.retain(field_object(level,"routes")),extraRoutes=roots.retain(field_object(level,"extraRoutes"));
  void* routeArgs[]={routes,extraRoutes};invoke(bound_methods.at(0x6011199),map,routeArgs);
  auto area=roots.retain(field_object(nb_manager(),"mapAreaManager"));invoke(bound_methods.at(0x6013863),area);
  trace("own-after-native-area-start");
  // Tile.Init runs native Rune/graphic initialization; map-area restoration
  // also re-enables authored groups. Bind the final active original graphics
  // to those initialized Tiles, just as the Help pipeline does after Map.Init.
  if(!nff_adapter->boolean(nff_adapter->invoke(mixedhelp::Method::GraphicAttach,target.graphic,{mixedhelp::Arg::flag(true)})))throw std::runtime_error("native continuous field final graphic failed to attach to initialized tiles");
  trace("own-after-final-graphic-attach");
  if(nmo_controller()!=controller||nff_map(controller)!=map||field_object(map,"m_data")!=data||field_object(map,"_graphic")!=target.graphic)throw std::runtime_error("native continuous field import lost its owner");
  set_object_field(manager,profile_field(api.il2cpp_object_get_class(manager),"enemyPreviewer"),nextPreview);
  owner.stage=input.stageId;owner.level=native_multiplayer_stage_path(input.stageId,CN_PRIVATE_LEVELS);++owner.epoch;nmo_stage=owner.stage;
  for(auto h:previous)hold_free(h);
 }catch(...){
  try{throw;}catch(const std::exception& error){trace_exception(std::string("own-import: ")+error.what());}catch(...){trace_exception("own-import: native field import failed");}
  // Preserve the exact prior native references. A partial map failure remains
  // a failed preparation proof; it never falls through to battle_loader.
  set_object_field(controller,profile_field(api.il2cpp_object_get_class(controller),"m_levelData"),oldLevel);set_object_field(controller,profile_field(api.il2cpp_object_get_class(controller),"m_mapData"),oldData);set_object_field(map,profile_field(api.il2cpp_object_get_class(map),"m_data"),oldData);set_object_field(map,profile_field(api.il2cpp_object_get_class(map),"_graphic"),oldGraphic);
  set_object_field(model,profile_field(api.il2cpp_object_get_class(model),"<levelData>k__BackingField"),oldModelLevel);
  oldTerrain.install(model);
  try{nff_adapter->invoke(mixedhelp::Method::SetActive,target.root,{mixedhelp::Arg::flag(false)});nff_adapter->invoke(mixedhelp::Method::SetActive,oldRoot,{mixedhelp::Arg::flag(true)});}catch(...){}
  for(auto h:owner.currentRoots)hold_free(h);owner.currentRoots=std::move(previous);throw;
 }
}
mixedhelp::Stage source(const cnfield::Source& source,int32_t seat){
 const CN_Asset* asset=nullptr;for(const auto& row:CN_ASSETS)if(source.assetKey==row.alias){asset=&row;break;}
 if(!asset||asset->scene||source.assetKey!=asset->asset)throw std::runtime_error("native continuous field graphic is outside the fixed profile");
 const CN_Bundle* bundle=nullptr;for(const auto& row:CN_BUNDLES)if(std::strcmp(row.name,asset->bundle)==0){bundle=&row;break;}if(!bundle||source.graphicHash!=bundle->hash)throw std::runtime_error("native continuous field graphic hash differs");
 const auto pathValue=native_multiplayer_stage_path(source.stageId,CN_PRIVATE_LEVELS);
 if(pathValue.empty()||!owner.controller||owner.controller!=nmo_controller()||owner.map!=nff_map(owner.controller))throw std::runtime_error("native continuous field source owner differs");
 const auto cached=owner.sources.find(source.stageId);const auto proof=owner.sourceProofs.find(source.stageId);
 if(cached!=owner.sources.end()&&proof!=owner.sourceProofs.end()){
  const auto& signedSource=proof->second;const auto& stored=cached->second.source;
  if(signedSource.assetKey!=source.assetKey||signedSource.graphicHash!=source.graphicHash||signedSource.mapHash!=source.mapHash||signedSource.predefinesHash!=source.predefinesHash||stored.stageId!=source.stageId||stored.graphicSha256!=source.graphicHash||native_multiplayer_resolve_stage_path(stored.stageId,stored.levelId,CN_PRIVATE_LEVELS)!=pathValue||!stored.mapData||!stored.levelData||!stored.graphicPrefab)throw std::runtime_error("native continuous field cached source proof differs");
  // Reuse only immutable authored assets owned by this live controller/map.
  // The caller still imports the target field and runs its scoped terrain
  // executor, including same-stage specialEffects/terrain changes.
  auto result=stored;result.seat=seat;return result;
 }
 TemporaryRoots roots;auto path=roots.retain(string_new(pathValue.c_str()));void* args[]={path};auto level=roots.retain(invoke(nmo_method(0x60131d7),nullptr,args));auto map=roots.retain(field_object(level,"mapData"));auto key=roots.retain(string_new(source.assetKey.c_str()));auto prefab=roots.retain(load_asset(key));
 if(!level||!map||native_multiplayer_resolve_stage_path(source.stageId,text(field_object(level,"levelId")),CN_PRIVATE_LEVELS)!=pathValue)throw std::runtime_error("native continuous field loaded source identity differs");
 if(!prefab||!api.il2cpp_class_is_assignable_from(cls("UnityEngine.GameObject"),api.il2cpp_object_get_class(prefab)))throw std::runtime_error("native continuous field graphic is not a prefab");
 owner.sourceProofs[source.stageId]=source;
 return {seat,source.stageId,text(field_object(level,"levelId")),source.graphicHash,map,level,prefab};
}
}
bool native_multiplayer_world_field_info(void* controller,void* map,std::string& stage,std::string& level){auto& owner=native_world_field::owner;if(owner.controller!=controller||owner.map!=map||owner.stage.empty())return false;stage=owner.stage;level=owner.level;return true;}
uint64_t native_multiplayer_world_field_epoch(){return native_world_field::owner.epoch;}
void* native_multiplayer_world_field_scene_graphic(void* controller,void* map,const std::string& stage){
 nb_main_thread();
 if(!controller||!map||controller!=nmo_controller()||nff_map(controller)!=map)return nullptr;
 auto& value=native_world_field::owner;
 const bool first=value.controller!=controller||value.map!=map;
 // Initial native loading does not call import(). First Help must register
 // that live scene before testing its render ownership; otherwise an empty
 // cache selects an isolated prefab and loses the scene's baked lightmaps.
 // adopt validates the immutable, signed loader input and only retains roots.
 native_world_field::adopt(controller,map);
 // A cached prefab from a different scene has no native lightmap table. Do
 // not mistake it for the original streamed scene when returning from a view.
 if(value.controller!=controller||value.map!=map||value.originalStage!=stage||!value.originalGraphic||field_object(map,"_graphic")!=value.originalGraphic)return nullptr;
 ++native_world_field::sceneGraphicBorrows;if(first)++native_world_field::firstHelpSceneAdoptions;
 return value.originalGraphic;
}
void native_multiplayer_world_field_switch(const cnjson::Value& raw,int32_t seat,bool force){auto source=cnfield::source(raw);auto controller=nmo_controller(),map=nff_map(controller);native_world_field::adopt(controller,map);auto stage=native_world_field::source(source,seat);native_world_field::import(stage,force);}
void native_multiplayer_world_field_restore(const mixedhelp::Stage& stage){native_world_field::import(stage,true);}
void native_multiplayer_world_field_reset(){native_world_field::owner.clear();}
