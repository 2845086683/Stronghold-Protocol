#pragma once
#include "native_multiplayer_map_types.h"
namespace mixedhelp::concrete {
struct LoadPlan:Plan {
    bool singleOwner=false;
    // Only supplied when the live scene still owns this exact authored map.
    // Borrow its render graph, including baked lighting and scene effects.
    Object sceneGraphic=nullptr;
    LoadPlan()=default;
    LoadPlan(const Plan& value):Plan(value){}
};
class Pipeline {
public:
    enum class State { Idle,Preparing,Imported,Attached,Ready,AwaitingOwnReload,Failed };
    struct Graphic {Source source;int seat;Object root=nullptr,graphic=nullptr,holder=nullptr;std::string key;std::vector<Object> kept;bool registered=false,borrowed=false;};
private:
    Native& api;Roots roots;LoadPlan plan;Frame frame;State state=State::Idle;
    Object composed=nullptr;std::vector<Object> clonedTiles;std::vector<Graphic> graphics;
    Identity restoreIdentity;
    bool mutated=false;std::string error;
    Object call(Method id,Object self,std::vector<Arg> args={}){guard();return api.invoke(id,self,args);}
    void guard(){api.main_thread();if(!same_context(api.current_context(),plan.context))throw std::runtime_error("mixed-map stale owner/controller/generation");}
    int32_t number(Method id,Object self){return api.integer(call(id,self));}
    Object get_array(Object array,size_t index){return call(Method::ArrayGetOne,array,{Arg::i32(int32_t(index))});}
    void set_array(Object array,size_t index,Object value){call(Method::ArraySetOne,array,{Arg::ref(value),Arg::i32(int32_t(index))});}
    const Stage& stage(Source value)const{return plan.singleOwner?plan.left:value==Source::Left?plan.left:value==Source::Right?plan.right:plan.neutral;}
    void require_stage(const Stage& value){
        if(value.stageId.empty()||value.levelId.empty()||value.graphicSha256.size()!=64||!value.mapData||!value.levelData||!value.graphicPrefab)
            throw std::runtime_error("mixed-map stage or pinned graphic missing");
        static const std::set<std::string> allowed={"act1autochess_m01","act1autochess_m02","act1autochess_m03","act1autochess_m04","act2autochess_m01","act2autochess_m02","act2autochess_m03","act2autochess_m04"};
        if(!allowed.count(value.stageId))throw std::runtime_error("mixed-map stage outside the eight catalog sources");
    }
    void pin_stage(const Stage& value){roots.add(value.mapData);roots.add(value.levelData);roots.add(value.graphicPrefab);}
    void require_empty_metadata(Object map){
        // The eight pinned catalog MapData sources have no blocked edges or map-
        // wide effects/tags/layers. Unknown nonempty metadata is rejected,
        // never discarded or assigned a guessed edge direction/owner.
        for(const auto* name:{"blockEdges","effects","tags","layerRects"}){auto value=api.field(map,name);if(value&&api.array_size(value))throw std::runtime_error(std::string("mixed-map unreviewed nonempty MapData metadata: ")+name);}
    }
    Object clone_map(const Frame& useFrame,bool selfOnly){
        const auto source=selfOnly?plan.self.mapData:plan.neutral.mapData;
        auto result=roots.add(api.allocate("Torappu.MapData"));if(result==source)throw std::runtime_error("mixed-map MapData clone aliases source");
        api.copy_fields(result,source);auto oldMatrix=api.field(source,"map");
        auto matrix=roots.add(call(Method::ArrayClone,oldMatrix));if(matrix==oldMatrix)throw std::runtime_error("mixed-map matrix clone aliases source");
        auto tiles=roots.add(api.array("Torappu.TileData",useFrame.owners.size()));
        clonedTiles.clear();clonedTiles.reserve(useFrame.owners.size());
        for(int32_t row=0;row<useFrame.height;++row)for(int32_t col=0;col<useFrame.width;++col){
            const Position pos{row,col};const auto input=selfOnly?plan.self.mapData:stage(useFrame.at(pos)).mapData;
            auto original=roots.add(call(Method::MapDataItem,input,{Arg::i32(row),Arg::i32(col)}));
            auto copy=roots.add(call(Method::TileClone,original));if(copy==original)throw std::runtime_error("mixed-map TileData.DeepClone aliases source");
            const auto index=clonedTiles.size();if(index>32767)throw std::runtime_error("mixed-map Int16 tile table overflow");
            clonedTiles.push_back(copy);set_array(tiles,index,copy);
            auto boxed=roots.add(api.short_box(int16_t(index)));
            call(Method::ArraySetTwo,matrix,{Arg::ref(boxed),Arg::i32(useFrame.height-1-row),Arg::i32(col)});
        }
        api.field(result,"map",matrix);api.field(result,"tiles",tiles);
        // Null/empty arrays are independently cloned to avoid writing through
        // references owned by a cached source MapData in later native code.
        for(const auto* name:{"blockEdges","effects","tags","layerRects"})if(auto value=api.field(source,name))api.field(result,name,roots.add(call(Method::ArrayClone,value)));
        return result;
    }
    void instantiate_graphics(){
        const auto container=roots.add(call(Method::GetGraphicContainer,plan.context.map));
        if(plan.sceneGraphic){
            if(call(Method::GetPrimaryGraphic,plan.context.map)!=plan.sceneGraphic||
               plan.left.stageId!=plan.neutral.stageId||plan.right.stageId!=plan.neutral.stageId||plan.self.stageId!=plan.neutral.stageId)
                throw std::runtime_error("mixed-map live scene graphic proof differs");
            Graphic value;value.source=Source::Neutral;value.seat=-1;value.borrowed=true;
            value.graphic=roots.add(plan.sceneGraphic);value.root=roots.add(call(Method::GetGameObject,value.graphic));
            value.holder=roots.add(call(Method::GetComponent,value.root,{Arg::ref(roots.add(api.type_object("Torappu.Battle.MapSubGraphicHolder")))}));
            graphics.push_back(value);return;
        }
        for(const auto source:{Source::Neutral,Source::Left,Source::Right}){
            if(plan.singleOwner&&source!=Source::Neutral)continue;
            const auto& input=stage(source);Graphic value;value.source=source;value.seat=source==Source::Neutral?-1:input.seat;
            if(plan.singleOwner)value.seat=input.seat;
            value.key=registration_key(plan.context.identity,source,value.seat);
            value.root=roots.add(call(Method::InstantiateParent,nullptr,{Arg::ref(input.graphicPrefab),Arg::ref(container),Arg::flag(false)}));
            if(value.root==input.graphicPrefab||std::any_of(graphics.begin(),graphics.end(),[&](const Graphic& old){return old.root==value.root;}))throw std::runtime_error("mixed-map Instantiate did not create a unique private instance");
            graphics.push_back(value);auto& output=graphics.back();
            output.graphic=roots.add(call(Method::GetComponent,output.root,{Arg::ref(roots.add(api.type_object("Torappu.Battle.MapGraphic")))}));
            output.holder=roots.add(call(Method::GetComponent,output.root,{Arg::ref(roots.add(api.type_object("Torappu.Battle.MapSubGraphicHolder")))}));
            call(Method::SetActive,output.root,{Arg::flag(false)});
        }
    }
    void select_graphics(){
        const auto anchor=roots.add(call(Method::GetAnchor,plan.context.map));
        for(auto& output:graphics){
            // Apply the authored Help hidden groups before final filtering;
            // enabling a group afterwards could reactivate excluded tiles.
            // _SwitchToHelpBattleMap -> _UpdateMapGraphic -> native holder:
            // false calls DisableSubGraphic; true calls EnableSubGraphic.
            call(Method::SubgraphicDisable,output.holder,{Arg::text("HIDDEN@1")});
            call(plan.singleOwner?Method::SubgraphicEnable:Method::SubgraphicDisable,output.holder,{Arg::text("HIDDEN@2")});
            call(Method::SubgraphicEnable,output.holder,{Arg::text("HIDDEN@3")});
            auto original=api.field(output.graphic,"_graphics");if(!original)throw std::runtime_error("mixed-map graphic tile array missing");
            const auto count=api.array_size(original);if(count>4096)throw std::runtime_error("mixed-map graphic count bound");
            std::set<size_t> positions;
            for(size_t index=0;index<count;++index){auto graphic=roots.add(get_array(original,index));
                if(!output.borrowed&&api.field(graphic,"_tile"))throw std::runtime_error("mixed-map prefab retained a source Tile");
                if(output.borrowed)api.field(graphic,"_tile",nullptr); // MapImport retired that Tile generation.
                call(Method::GraphicUpdatePosition,graphic,{Arg::ref(anchor)});const auto pos=api.position(call(Method::GraphicGetPosition,graphic));
                if(!positions.insert(frame.logical(pos)).second)throw std::runtime_error("mixed-map evidenced graphic source contains duplicate TileGraphic positions");
                if(output.borrowed||frame.at(pos)==output.source)output.kept.push_back(graphic);
                else call(Method::SetActive,roots.add(call(Method::GetGameObject,graphic)),{Arg::flag(false)});
            }
            auto filtered=roots.add(api.array("Torappu.Battle.TileGraphic",output.kept.size()));
            for(size_t i=0;i<output.kept.size();++i)set_array(filtered,i,output.kept[i]);
            if(!output.borrowed){api.field(output.graphic,"_graphics",filtered);api.prune_rendering(output.root,output.kept,output.source==Source::Neutral);}
            // These exact keys/flags are the existing Help switch policy.
            // Call the holder on every owned instance; submap registration
            // does not make the original manager enumerate those holders.
            call(Method::SetActive,output.root,{Arg::flag(true)});
            if(!api.boolean(call(Method::GraphicAttach,output.graphic,{Arg::flag(true)})))throw std::runtime_error("mixed-map native AttachToMap failed");
            if(!output.borrowed){
                if(!api.boolean(call(Method::RegisterGraphic,plan.context.map,{Arg::text(output.key),Arg::ref(output.graphic)})))throw std::runtime_error("mixed-map native subgraphic registration collision");
                output.registered=true;
            }
        }
    }
    std::vector<Object> imported_tiles(){
        std::vector<Object> result;std::set<Object> unique;
        for(int32_t row=0;row<frame.height;++row)for(int32_t col=0;col<frame.width;++col){const Position p{row,col};
            auto tile=call(Method::MapGetTile,plan.context.map,{Arg::grid(p)});
            if(!tile||!unique.insert(tile).second||api.field(tile,"_data")!=clonedTiles.at(frame.logical(p)))throw std::runtime_error("mixed-map imported Tile/data binding differs");
            result.push_back(roots.add(tile));
        }
        return result;
    }
    void verify_tiles_and_graphics(const std::vector<Object>& imported){
        std::set<Object> cached;std::map<Object,size_t> inputs;
        for(int32_t row=0;row<frame.height;++row)for(int32_t col=0;col<frame.width;++col){const Position p{row,col};
            inputs.emplace(clonedTiles.at(frame.logical(p)),frame.logical(p));
            for(const auto* source:{&plan.neutral,&plan.left,&plan.right,&plan.self})cached.insert(call(Method::MapDataItem,source->mapData,{Arg::i32(row),Arg::i32(col)}));
        }
        for(int32_t row=0;row<frame.height;++row)for(int32_t col=0;col<frame.width;++col){const Position p{row,col};
            auto tile=call(Method::MapGetTile,plan.context.map,{Arg::grid(p)});
            if(!tile||tile!=imported.at(frame.logical(p))||call(Method::MapDataItem,composed,{Arg::i32(row),Arg::i32(col)})!=clonedTiles.at(frame.logical(p)))throw std::runtime_error("mixed-map native Tile/input ownership changed");
            // Map.Init keeps the imported Tile objects. Tile.Init then calls
            // RuneManager.PreprocessTile, which may DeepClone its TileData and
            // writes the returned object to Tile._data. Keep that original
            // engine result; it need not equal the pre-Init MapData reference.
            auto data=api.field(tile,"_data");auto input=inputs.find(data);
            if(!data||cached.count(data)||(input!=inputs.end()&&input->second!=frame.logical(p)))throw std::runtime_error("mixed-map native Rune TileData aliases a foreign source/cell");
        }
        for(const auto& output:graphics)for(auto graphic:output.kept){const auto pos=api.position(call(Method::GraphicGetPosition,graphic));
            const auto actual=frame.at(pos);auto expected=call(Method::MapGetTile,plan.context.map,{Arg::grid(pos)});auto bound=api.field(graphic,"_tile");
            // AttachToMap(true), 0x87884a..0x878855, deliberately skips a
            // TileGraphic unless Behaviour.isActiveAndEnabled. Authored hidden
            // groups remain in _graphics: their unbound null _tile is normal.
            // Every active graphic must bind this exact imported Tile; even
            // an inactive graphic may never retain a foreign cell/old Tile.
            const bool active=api.boolean(call(Method::GraphicActiveEnabled,graphic));
            if((!output.borrowed&&actual!=output.source)||!expected||(active&&bound!=expected)||(!active&&bound&&bound!=expected))
                throw std::runtime_error("mixed-map TileGraphic owner/new Tile mismatch row="+std::to_string(pos.row)+" col="+std::to_string(pos.col)+
                    " source="+std::to_string(int(output.source))+" frame="+std::to_string(int(actual))+" active="+(active?"1":"0")+
                    " bound="+(bound?(bound==expected?std::string("expected"):std::string("foreign")):std::string("null"))+" expected="+(expected?"imported":"null"));
        }
    }
    void cleanup_owned(){
        api.main_thread();
        // Do not use guard here: a stale controller still owns its old private
        // instances, but must never cause removal of a new generation's value.
        bool changed=false;std::string cleanupError;
        for(auto it=graphics.rbegin();it!=graphics.rend();++it){
            if(it->borrowed)continue; // Native scene lifetime, never our clone.
            try{if(it->registered&&!api.unregister_if_owned(plan.context.map,it->key,it->graphic))changed=true;}
            catch(const std::exception& ex){if(cleanupError.empty())cleanupError=ex.what();}
            it->registered=false;
            // OnDisable writes global scene shader settings. Run it before
            // restoring the own scene, not in Unity's deferred Destroy pass.
            try{if(it->root){api.invoke(Method::SetActive,it->root,{Arg::flag(false)});api.invoke(Method::Destroy,nullptr,{Arg::ref(it->root)});it->root=nullptr;}}
            catch(const std::exception& ex){if(cleanupError.empty())cleanupError=ex.what();}
        }
        graphics.clear();
        if(!cleanupError.empty())throw std::runtime_error("mixed-map cleanup failed: "+cleanupError);
        if(changed)throw std::runtime_error("mixed-map registered value changed; new owner preserved");
    }
    void request_restore(const std::string& reason){
        restoreIdentity=api.restore_target(plan.context.identity);
        if(restoreIdentity.room!=plan.context.identity.room||restoreIdentity.run!=plan.context.identity.run||restoreIdentity.round<plan.context.identity.round||restoreIdentity.generation<plan.context.identity.generation)throw std::runtime_error("mixed-map authenticated restore target regressed or changed room/run");
        state=State::AwaitingOwnReload;api.request_own_reload({plan.context.identity,restoreIdentity,plan.selfSeat,plan.self,reason});
    }
    void failed(const std::exception& failure){
        error=failure.what();state=State::Failed;bool same=false;
        try{same=same_context(api.current_context(),plan.context);cleanup_owned();}catch(const std::exception& cleanup){error+="; cleanup: ";error+=cleanup.what();}
        if(mutated&&same){try{request_restore(error);}catch(const std::exception& restore){error+="; restore: ";error+=restore.what();state=State::Failed;roots.clear();}}
        else roots.clear();
    }
public:
    explicit Pipeline(Native& value):api(value),roots(value){}
    State status()const{return state;}
    const std::string& failure()const{return error;}
    const Frame& layout()const{return frame;}
    Object map_data()const{return composed;}
    bool ready()const{return state==State::Ready;}
    Object prepare(const LoadPlan& input){
        api.main_thread();
        if(state==State::Ready&&same_context(input.context,plan.context)&&input.planHash==plan.planHash)return composed;
        if(state!=State::Idle)throw std::runtime_error("mixed-map previous generation requires own-field restoration first");
        plan=input;state=State::Preparing;error.clear();mutated=false;
        try{
            guard();if(!plan.context.privateHelp||plan.selfSeat<0||plan.selfSeat>3||plan.left.seat<0||plan.right.seat<0||plan.left.seat>3||plan.right.seat>3||(!plan.singleOwner&&plan.left.seat==plan.right.seat)||(plan.singleOwner&&plan.left.seat!=plan.right.seat)||plan.self.seat!=plan.selfSeat||plan.routeIdentity.empty()||plan.planHash.empty())throw std::runtime_error("mixed-map invalid authenticated plan");
            for(const auto* inputStage:{&plan.neutral,&plan.left,&plan.right,&plan.self}){require_stage(*inputStage);pin_stage(*inputStage);require_empty_metadata(inputStage->mapData);}
            roots.add(plan.context.controller);roots.add(plan.context.map);roots.add(plan.areaManager);
            const auto width=number(Method::MapWidth,plan.neutral.mapData),height=number(Method::MapHeight,plan.neutral.mapData);
            for(const auto* inputStage:{&plan.left,&plan.right,&plan.self})if(number(Method::MapWidth,inputStage->mapData)!=width||number(Method::MapHeight,inputStage->mapData)!=height)throw std::runtime_error("mixed-map source dimensions differ from template");
            if(plan.singleOwner){
                if(width!=21||height!=19||plan.routeIdentity!="Activities/ACT1AUTOCHESS/level_act1autochess_escaped_single")throw std::runtime_error("mixed-map single owner requires the original escaped_single frame and routes");
                frame={width,height,std::vector<Source>(size_t(width)*size_t(height),Source::Neutral)};
            }else frame=common_frame(width,height,[&](Position p){return std::make_pair(api.boolean(call(Method::IsLeftNormal,plan.areaManager,{Arg::grid(p)})),api.boolean(call(Method::IsRightNormal,plan.areaManager,{Arg::grid(p)})));});
            composed=clone_map(frame,false);instantiate_graphics();return composed;
        }catch(const std::exception& ex){failed(ex);return nullptr;}
    }
    bool commit(Object postRuneMap,Object escapedLevel,const std::function<void(Object,Object)>& originalMapInit){
        try{
            guard();if(state!=State::Preparing||postRuneMap!=composed||escapedLevel!=plan.neutral.levelData)throw std::runtime_error("mixed-map commit is not the prepared post-Rune owner");
            if(number(Method::MapWidth,composed)!=frame.width||number(Method::MapHeight,composed)!=frame.height)throw std::runtime_error("mixed-map Rune changed the frame dimensions");
            require_empty_metadata(composed);
            // Preserve original Rune preprocessing: adopt its resulting cell
            // references instead of cloning source data a second time.
            clonedTiles.clear();
            for(int32_t row=0;row<frame.height;++row)for(int32_t col=0;col<frame.width;++col){
                auto tile=roots.add(call(Method::MapDataItem,composed,{Arg::i32(row),Arg::i32(col)}));
                for(const auto* source:{&plan.neutral,&plan.left,&plan.right,&plan.self})if(tile==call(Method::MapDataItem,source->mapData,{Arg::i32(row),Arg::i32(col)}))throw std::runtime_error("mixed-map Rune aliased cached source TileData");
                clonedTiles.push_back(tile);
            }
            auto primary=roots.add(call(Method::GetPrimaryGraphic,plan.context.map));
            auto primaryRoot=roots.add(call(Method::GetGameObject,primary));
            mutated=true;if(!plan.sceneGraphic)call(Method::SetActive,primaryRoot,{Arg::flag(false)});
            // Clear factory TileGraphic meshes: the authored scene already
            // supplies the ground. ClearAllGraphic does not clear Tile._effect
            // or its holder; gate particles keep their original lifecycle.
            call(Method::MapImport,plan.context.map,{Arg::ref(composed),Arg::ref(factory()),Arg::flag(true),Arg::flag(false),Arg::flag(true),Arg::flag(false)});state=State::Imported;
            select_graphics();state=State::Attached;
            const auto imported=imported_tiles();
            guard();originalMapInit(composed,escapedLevel);guard();
            // Rebind after the original per-Tile Rune/widgets initialization.
            // Re-importing here would discard its initialized Tile objects.
            for(auto& output:graphics)if(!api.boolean(call(Method::GraphicAttach,output.graphic,{Arg::flag(true)})))throw std::runtime_error("mixed-map final native AttachToMap failed");
            verify_tiles_and_graphics(imported);
            if(api.field(plan.context.controller,"m_mapData")!=composed)throw std::runtime_error("mixed-map controller did not retain the post-Rune MapData");
            state=State::Ready;return true;
        }catch(const std::exception& ex){failed(ex);return false;}
    }
    bool owns(Object map,Object data)const{return state==State::Preparing&&map==plan.context.map&&data==composed;}
    bool contains_map(Object map)const{return map==plan.context.map&&(state==State::Preparing||state==State::Imported||state==State::Attached||state==State::Ready);}
    void original_load_entering(){guard();if(state!=State::Preparing)throw std::runtime_error("mixed-map original loader entered outside preparation");mutated=true;}
    void use_level(Object level){guard();if(state!=State::Preparing||!level||level==plan.neutral.levelData)throw std::runtime_error("mixed-map working LevelData must be a private clone");plan.neutral.levelData=roots.add(level);}
    const LoadPlan& active_plan()const{return plan;}
    const std::vector<Graphic>& owned_graphics()const{return graphics;}
    void abort_before_commit(const std::exception& ex){failed(ex);}
    Object factory(){auto value=call(Method::GetFactory,plan.context.controller);if(!value)throw std::runtime_error("mixed-map BattleFactory missing");return value;}
    Object height_graphic(Object map,Object data,Position pos){
        guard();if(!mutated||map!=plan.context.map||clonedTiles.at(frame.logical(pos))!=data)throw std::runtime_error("mixed-map height scope does not own this Tile.SetData");
        return graphic_for(map,pos);
    }
    Object graphic_for(Object map,Position pos){
        guard();if(!mutated||!contains_map(map))throw std::runtime_error("mixed-map height route has no active owner");
        if(plan.sceneGraphic)return plan.sceneGraphic;
        const auto source=frame.at(pos);for(const auto& value:graphics)if(value.source==source)return value.graphic;
        throw std::runtime_error("mixed-map position has no live graphic height owner");
    }
    void finish(){guard();if(state!=State::Ready)throw std::runtime_error("mixed-map finish requires completed composition");state=State::Failed;try{cleanup_owned();}catch(const std::exception& ex){error=ex.what();throw;}request_restore("help completed; rebuild own native field");}
    void abandon_stale(){
        api.main_thread();if(same_context(api.current_context(),plan.context))throw std::runtime_error("mixed-map abandon requires an obsolete controller/generation");
        state=State::Failed;try{cleanup_owned();}catch(const std::exception& ex){error=ex.what();}
        roots.clear();clonedTiles.clear();composed=nullptr;
        // No restore request is sent into the newer live Unity context.
    }
    void on_own_reload(const Identity& identity,Object newController,Object newMap,const std::string& stageId,bool composite){
        api.main_thread();const auto live=api.current_context();if(state!=State::AwaitingOwnReload||!(identity==restoreIdentity)||!newController||!newMap||newController==plan.context.controller||composite||stageId!=plan.self.stageId||live.controller!=newController||live.map!=newMap||live.privateHelp||!(live.identity==identity))throw std::runtime_error("mixed-map own-field reload acknowledgement mismatch");
        roots.clear();clonedTiles.clear();composed=nullptr;graphics.clear();state=State::Idle;mutated=false;
    }
    void on_own_import(const Identity& identity,Object controller,Object map,const std::string& stageId){
        api.main_thread();const auto live=api.current_context();
        if(state!=State::AwaitingOwnReload||!(identity==restoreIdentity)||!controller||!map||controller!=plan.context.controller||map!=plan.context.map||stageId!=plan.self.stageId||live.controller!=controller||live.map!=map||live.privateHelp||!(live.identity==identity))throw std::runtime_error("mixed-map in-scene own-field import acknowledgement mismatch");
        auto data=api.field(controller,"m_mapData"),level=api.field(controller,"m_levelData");
        if(!data||!level||data==composed||data==plan.self.mapData||api.field(map,"m_data")!=data||api.field(level,"mapData")!=data)throw std::runtime_error("mixed-map own-field import lacks a fresh native map/level binding");
        roots.clear();clonedTiles.clear();composed=nullptr;graphics.clear();state=State::Idle;mutated=false;
    }
};
}
