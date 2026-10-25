#pragma once
#include "native_multiplayer_map_policy.h"
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <set>
#include <utility>

namespace mixedhelp {
using Object=void*;
enum class Method {
    MapWidth,MapHeight,MapDataItem,TileClone,MapImport,MapInit,MapGetTile,MapGraphicHeight,
    GraphicAttach,GraphicUpdatePosition,GraphicGetPosition,RegisterGraphic,
    IsLeftNormal,IsRightNormal,ArrayClone,ArrayGetOne,ArraySetOne,ArraySetTwo,ArrayLength,
    InstantiateParent,GetComponent,GetGameObject,SetActive,GetAnchor,GetGraphicContainer,
    SubgraphicEnable,SubgraphicDisable,Destroy,GetFactory,GetPrimaryGraphic,GraphicActiveEnabled
};
struct Arg {
    enum class Kind { Object,Int32,Boolean,Position,String } kind=Kind::Object;
    Object object=nullptr;int32_t integer=0;bool boolean=false;Position position;std::string string;
    static Arg ref(Object value){Arg a;a.object=value;return a;}
    static Arg i32(int32_t value){Arg a;a.kind=Kind::Int32;a.integer=value;return a;}
    static Arg flag(bool value){Arg a;a.kind=Kind::Boolean;a.boolean=value;return a;}
    static Arg grid(Position value){Arg a;a.kind=Kind::Position;a.position=value;return a;}
    static Arg text(std::string value){Arg a;a.kind=Kind::String;a.string=std::move(value);return a;}
};
struct Context { Identity identity;Object controller=nullptr,map=nullptr;bool privateHelp=false; };
inline bool same_context(const Context& a,const Context& b){return a.identity==b.identity&&a.controller==b.controller&&a.map==b.map&&a.privateHelp==b.privateHelp;}
struct Stage {
    int seat=-1;std::string stageId,levelId,graphicSha256;Object mapData=nullptr,levelData=nullptr,graphicPrefab=nullptr;
};
struct Plan {
    Context context;int selfSeat=-1;Stage neutral,left,right,self;
    Object areaManager=nullptr;std::string routeIdentity,planHash;
};
struct RestoreRequest {Identity source,target;int selfSeat=-1;Stage self;std::string reason;};

// The boundary is reflection/runtime_invoke, not a replacement battle engine.
// Every Method has an exact pinned declaration in bindings.json. Object refs
// passed to invoke are retained by the candidate until the generation ends.
struct Native {
    virtual ~Native()=default;
    virtual void main_thread()=0;
    virtual Context current_context()=0;
    virtual Object invoke(Method,Object,const std::vector<Arg>&)=0;
    virtual Object allocate(const char*)=0;
    virtual void copy_fields(Object,Object)=0;
    virtual Object field(Object,const char*)=0;
    virtual void field(Object,const char*,Object)=0;
    virtual Object array(const char*,size_t)=0;
    virtual size_t array_size(Object)=0;
    virtual int32_t integer(Object)=0;
    virtual bool boolean(Object)=0;
    virtual float floating(Object)=0;
    virtual Position position(Object)=0;
    virtual Object short_box(int16_t)=0;
    virtual Object type_object(const char*)=0;
    virtual uint32_t retain(Object)=0;
    virtual Object retained(uint32_t)=0;
    virtual void release(uint32_t)=0;
    // Exact ListDict<string,MapGraphic>.TryGetValue / Remove pair. No removal
    // is allowed unless the current value equals this generation's instance.
    virtual bool unregister_if_owned(Object,const std::string&,Object)=0;
    virtual Identity restore_target(const Identity&)=0;
    // Keep each side's authored renderer subtree beneath selected tile
    // graphics. Only the neutral template retains shared decorations.
    virtual void prune_rendering(Object,const std::vector<Object>&,bool)=0;
    // Integration hands this to the existing own-field BattleLoader path;
    // it must not mark ready until on_own_reload validates the new context.
    virtual void request_own_reload(const RestoreRequest&)=0;
};

class Roots {
    Native& api;std::vector<uint32_t> handles;
public:
    explicit Roots(Native& value):api(value){}
    Roots(const Roots&)=delete;Roots& operator=(const Roots&)=delete;
    Object add(Object value){if(!value)throw std::runtime_error("mixed-map null managed root");const auto h=api.retain(value);if(!h||api.retained(h)!=value)throw std::runtime_error("mixed-map GC root retention failed");handles.push_back(h);return value;}
    void clear(){for(auto i=handles.rbegin();i!=handles.rend();++i)api.release(*i);handles.clear();}
    ~Roots(){clear();}
};

}
