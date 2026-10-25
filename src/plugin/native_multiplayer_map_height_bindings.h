#pragma once
#include <cstdint>
namespace mixedheight {
enum class Id {Farthest,Nearest,Sector,EntityEffect,LocalEffect,GridLocal,MapItem,EntityPosition,TransformPosition,FromVector,Count};
struct Binding {Id id;const char* owner;const char* name;uint32_t token;uintptr_t rva;const char* result;uint32_t count;bool isStatic;const char* parameters[2];};
inline constexpr Binding BINDINGS[]={
 {Id::Farthest,"Torappu.Battle.Projectiles.FarthestPointMovement","_SetTargetPosAndDirection",0x6015370,0xd5c7d0,"System.Void",2,false,{"Torappu.Battle.ILocatable","Torappu.Battle.ILocatable"}},
 {Id::Nearest,"Torappu.Battle.Projectiles.NearestHighlandWallBottomMovement","_SetTargetPos",0x6015441,0xd75d90,"System.Void",2,false,{"Torappu.Battle.ILocatable","Torappu.Battle.ILocatable"}},
 {Id::Sector,"Torappu.Battle.Projectiles.SectorFarthestPointMovement","_SetTargetPosAndDirection",0x601548f,0xd7c170,"System.Void",2,false,{"Torappu.Battle.ILocatable","Torappu.Battle.ILocatable"}},
 {Id::EntityEffect,"Torappu.Battle.Effects.Effect","_InitLocationFromEntity",0x60189bf,0x1197f10,"System.Void",1,false,{"Torappu.Battle.Entity"}},
 {Id::LocalEffect,"Torappu.Battle.Effects.Effect","_InitLocation",0x60189c2,0x1198f60,"System.Void",1,false,{"System.Boolean"}},
 {Id::GridLocal,"Torappu.GridPosition","get_asLocalPosition",0x6006d88,0x2104010,"UnityEngine.Vector2",0,false,{}},
 {Id::MapItem,"Torappu.Battle.Map","get_Item",0x601115f,0x885c70,"Torappu.Battle.Tile",1,false,{"Torappu.GridPosition"}},
 {Id::EntityPosition,"Torappu.Battle.VisualObject","get_mapPositionV3",0x601302c,0xad4200,"UnityEngine.Vector3",0,false,{}},
 {Id::TransformPosition,"UnityEngine.Transform","get_localPosition",0x6000b92,0x6b74fb0,"UnityEngine.Vector3",0,false,{}},
 {Id::FromVector,"Torappu.GridPosition","FromVectorPosition",0x6006d90,0x2103900,"Torappu.GridPosition",1,true,{"UnityEngine.Vector2"}},
};
}
