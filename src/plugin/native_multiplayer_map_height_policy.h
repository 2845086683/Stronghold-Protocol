#pragma once
#include "native_multiplayer_map_types.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <memory>

namespace mixedheight {
using Object=::mixedhelp::Object;using Position=::mixedhelp::Position;using Context=::mixedhelp::Context;
struct V2{float x,y;};struct V3{float x,y,z;};
static_assert(sizeof(V2)==8&&sizeof(V3)==12&&sizeof(Position)==8,"fixed CN Win64 value ABI");
enum class Consumer{Farthest,Nearest,Sector,EntityEffect,LocalEffect,Count};
struct Capture {
    Consumer kind;Object self=nullptr,entity=nullptr,map=nullptr,selectedMap=nullptr,selectedTile=nullptr,transform=nullptr;
    Context context;Position grid{};V2 wall{},direction{},point{};unsigned mask=0;bool consumed=false,invalid=false;
};
// These callbacks are supplied by the concrete native map owner. graphicFor
// must use the original logical GridPosition/frame ownership, never a guessed
// left/right X boundary. fromVector must invoke the original native converter.
struct Host {
    std::function<Context()> current;
    std::function<bool(Object)> containsMap;
    std::function<Object(Object,Position)> graphicFor;
    std::function<float(Object,int32_t)> graphicHeight;
    std::function<Position(V2)> fromVector;
    std::function<void()> abortManaged;
    std::function<void(const std::string&)> failed;
};
class Router {
    Host host;
public:
    static inline thread_local Capture* active=nullptr;
    explicit Router(Host value):host(std::move(value)){
        if(!host.current||!host.containsMap||!host.graphicFor||!host.graphicHeight||!host.fromVector||!host.abortManaged||!host.failed)throw std::runtime_error("combat-height host incomplete");
    }
    const Host& callbacks()const{return host;}
    bool scope(Capture& c,Consumer kind,Object self,Object entity){
        c.kind=kind;c.self=self;c.entity=entity;c.context=host.current();c.map=c.context.map;
        return c.context.privateHelp&&c.context.controller&&c.map&&host.containsMap(c.map);
    }
    // Capture functions perform no managed calls/allocation/throw. They run
    // after the complete original getter and only accept its exact native
    // return address inside a protected consumer.
    static void local(Position grid,V2 value,uintptr_t caller)noexcept{
        auto c=active;if(!c)return;
        if((c->kind==Consumer::Farthest&&(caller==0xd5ccb6||caller==0xd5ccd2))||
           (c->kind==Consumer::Sector&&(caller==0xd7c342||caller==0xd7c35e))){
            const unsigned bit=(caller==0xd5ccb6||caller==0xd7c342)?1:2;
            if(c->mask&&!(c->grid==grid))c->invalid=true;c->grid=grid;c->mask|=bit;return;
        }
        if(c->kind!=Consumer::Nearest)return;
        switch(caller){
            case 0xd76541:c->wall.x=value.x;c->mask|=1;break;
            case 0xd76552:c->direction.x=value.x;c->mask|=2;break;
            case 0xd7657a:c->wall.y=value.y;c->mask|=4;break;
            case 0xd7658b:c->direction.y=value.y;c->mask|=8;break;
        }
    }
    static void tile(Object map,Position grid,Object tile,uintptr_t caller)noexcept{
        auto c=active;if(!c||c->kind!=Consumer::Nearest||caller!=0xd76415)return;
        c->selectedMap=map;c->selectedTile=tile;c->grid=grid;c->mask|=16;
    }
    static void entity(Object self,const V3& value,uintptr_t caller)noexcept{
        auto c=active;if(!c||c->kind!=Consumer::EntityEffect)return;
        if(caller==0x1198c54||caller==0x1198c67){
            if(self!=c->entity)c->invalid=true;
            if(caller==0x1198c54){c->point.x=value.x;c->mask|=1;}else{c->point.y=value.y;c->mask|=2;}
        }
    }
    static void transform(Object self,const V3& value,uintptr_t caller)noexcept{
        auto c=active;if(!c||c->kind!=Consumer::LocalEffect)return;
        if(caller==0x1199072){c->transform=self;c->point.x=value.x;c->mask|=1;}
        else if(caller==0x1199098){if(!c->transform||c->transform!=self)c->invalid=true;c->point.y=value.y;c->mask|=2;}
    }
    bool try_route(Object map,int32_t type,uintptr_t caller,float& result){
        auto c=active;if(!c)return false;
        bool expected=false;int32_t expectedType=-1;
        switch(c->kind){
            case Consumer::Farthest:expected=caller==0xd5cd28||caller==0xd5cd51;expectedType=caller==0xd5cd28?1:0;break;
            case Consumer::Sector:expected=caller==0xd7c3b8||caller==0xd7c3e1;expectedType=caller==0xd7c3b8?1:0;break;
            case Consumer::Nearest:expected=caller==0xd7643e||caller==0xd765b2;expectedType=caller==0xd765b2?0:-1;break;
            case Consumer::EntityEffect:expected=caller==0x1198c91;expectedType=1;break;
            case Consumer::LocalEffect:expected=caller==0x11990c2;expectedType=1;break;
            default:break;
        }
        // An unrelated nested query or the unproved Lua wrapper never consumes
        // this route. A recognized query with missing evidence is an error.
        if(!expected)return false;
        if(!::mixedhelp::same_context(c->context,host.current())||!host.containsMap(c->map))throw std::runtime_error("combat-height current scene changed");
        if(c->map!=map)return false;
        if(c->consumed||c->invalid||!host.containsMap(map)||
           (expectedType!=-1&&type!=expectedType))throw std::runtime_error("combat-height route ownership/type changed");
        Position position=c->grid;
        if(c->kind==Consumer::Farthest||c->kind==Consumer::Sector){
            if(c->mask!=3)throw std::runtime_error("combat-height missing original selected grid");
        }else if(c->kind==Consumer::Nearest&&caller==0xd7643e){
            if(!(c->mask&16)||c->selectedMap!=map||!c->selectedTile)throw std::runtime_error("combat-height missing selected wall Tile");
        }else{
            V2 point=c->point;
            if(c->kind==Consumer::Nearest){
                if(c->mask!=15)throw std::runtime_error("combat-height missing wall-edge components");
                // Only reconstruct the exact XY already being assembled by
                // d765b5..d765d7 for owner lookup. Original movement selection,
                // collision, height assignment and executor remain untouched.
                point={c->wall.x+c->direction.x*0.5f,c->wall.y+c->direction.y*0.5f};
            }else if(c->mask!=3)throw std::runtime_error("combat-height missing original Effect XY");
            if(!std::isfinite(point.x)||!std::isfinite(point.y))throw std::runtime_error("combat-height nonfinite original XY");
            position=host.fromVector(point);
        }
        auto graphic=host.graphicFor(map,position);if(!graphic)throw std::runtime_error("combat-height no position graphic");
        result=host.graphicHeight(graphic,type);if(!std::isfinite(result))throw std::runtime_error("combat-height native graphic returned nonfinite height");
        c->consumed=true;return true;
    }
};
}
