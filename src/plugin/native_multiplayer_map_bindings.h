#pragma once
#include "native_multiplayer_map_types.h"
namespace mixedhelp {
struct BindingDraft {Method id;const char* owner;const char* name;uint32_t token;uint64_t rva;bool isStatic;const char* result;uint32_t count;const char* parameters[7];};
inline const BindingDraft BINDING_DRAFT[]={
{Method::MapWidth,"Torappu.MapData","get_width",0x6007b5a,0x241d330ULL,false,"System.Int32",0,{}},
{Method::MapHeight,"Torappu.MapData","get_height",0x6007b5b,0x241d300ULL,false,"System.Int32",0,{}},
{Method::MapDataItem,"Torappu.MapData","get_Item",0x6007b5c,0x241d270ULL,false,"Torappu.TileData",2,{"System.Int32","System.Int32"}},
{Method::TileClone,"Torappu.TileData","DeepClone",0x6007b58,0x242f6c0ULL,false,"Torappu.TileData",0,{}},
{Method::MapImport,"Torappu.Battle.Map","ImportData",0x6011181,0x87e720ULL,false,"System.Void",6,{"Torappu.MapData","Torappu.Battle.BattleFactory","System.Boolean","System.Boolean","System.Boolean","System.Boolean"}},
{Method::MapInit,"Torappu.Battle.Map","Init",0x6011169,0x87f240ULL,false,"System.Void",4,{"Torappu.MapData","Torappu.LevelData","System.Collections.Generic.IList`1<Torappu.GridPosition>","System.String"}},
{Method::MapGetTile,"Torappu.Battle.Map","GetTile",0x60111be,0x87dbc0ULL,false,"Torappu.Battle.Tile",1,{"Torappu.GridPosition"}},
{Method::MapGraphicHeight,"Torappu.Battle.MapGraphic","GetTileHeight",0x60111e3,0x878ce0ULL,false,"System.Single",1,{"Torappu.TileData+HeightType"}},
{Method::GraphicAttach,"Torappu.Battle.MapGraphic","AttachToMap",0x60111e2,0x878380ULL,false,"System.Boolean",1,{"System.Boolean"}},
{Method::GraphicUpdatePosition,"Torappu.Battle.TileGraphic","UpdateGridPosition",0x60114a5,0x8bb860ULL,false,"System.Void",1,{"UnityEngine.Transform"}},
{Method::GraphicGetPosition,"Torappu.Battle.TileGraphic","get_gridPos",0x601149f,0x8bbb60ULL,false,"Torappu.GridPosition",0,{}},
{Method::RegisterGraphic,"Torappu.Battle.Map","RegisterSubMapGraphics",0x60111c9,0x87fda0ULL,false,"System.Boolean",2,{"System.String","Torappu.Battle.MapGraphic"}},
{Method::IsLeftNormal,"Torappu.Battle.AutoChess.AutoChessMapAreaManager","IsLeftNormalMap",0x6013857,0xb66a80ULL,false,"System.Boolean",1,{"Torappu.GridPosition"}},
{Method::IsRightNormal,"Torappu.Battle.AutoChess.AutoChessMapAreaManager","IsRightNormalMap",0x6013858,0xb66da0ULL,false,"System.Boolean",1,{"Torappu.GridPosition"}},
{Method::ArrayClone,"System.Array","Clone",0x6000e24,0x3cf2560ULL,false,"System.Object",0,{}},
{Method::ArrayGetOne,"System.Array","GetValue",0x6000e93,0x5eeb700ULL,false,"System.Object",1,{"System.Int32"}},
{Method::ArraySetOne,"System.Array","SetValue",0x6000e96,0x5eed540ULL,false,"System.Void",2,{"System.Object","System.Int32"}},
{Method::ArraySetTwo,"System.Array","SetValue",0x6000e97,0x5eed1d0ULL,false,"System.Void",3,{"System.Object","System.Int32","System.Int32"}},
{Method::ArrayLength,"System.Array","GetLength",0x6000e8a,0x5eeb640ULL,false,"System.Int32",1,{"System.Int32"}},
{Method::InstantiateParent,"UnityEngine.Object","Instantiate",0x6000a4b,0x6b62fc0ULL,true,"UnityEngine.Object",3,{"UnityEngine.Object","UnityEngine.Transform","System.Boolean"}},
{Method::GetComponent,"UnityEngine.GameObject","GetComponent",0x6000993,0x6b5be00ULL,false,"UnityEngine.Component",1,{"System.Type"}},
{Method::GetGameObject,"UnityEngine.Component","get_gameObject",0x6000956,0x6b59d80ULL,false,"UnityEngine.GameObject",0,{}},
{Method::SetActive,"UnityEngine.GameObject","SetActive",0x60009c0,0x6b5c530ULL,false,"System.Void",1,{"System.Boolean"}},
{Method::GetAnchor,"Torappu.Battle.Map","get_anchorTransform",0x601115c,0x885d20ULL,false,"UnityEngine.Transform",0,{}},
{Method::GetGraphicContainer,"Torappu.Battle.Map","get_graphicContainer",0x601115e,0x8868e0ULL,false,"UnityEngine.Transform",0,{}},
{Method::SubgraphicEnable,"Torappu.Battle.MapSubGraphicHolder","EnableSubGraphic",0x60111eb,0x8797f0ULL,false,"System.Void",1,{"System.String"}},
{Method::SubgraphicDisable,"Torappu.Battle.MapSubGraphicHolder","DisableSubGraphic",0x60111ec,0x879510ULL,false,"System.Void",1,{"System.String"}},
{Method::Destroy,"UnityEngine.Object","Destroy",0x6000a52,0x6b61b60ULL,true,"System.Void",1,{"UnityEngine.Object"}},
{Method::GetFactory,"Torappu.Battle.BattleController","get_factory",0x600fbfb,0x6415d0ULL,false,"Torappu.Battle.BattleFactory",0,{}},
{Method::GetPrimaryGraphic,"Torappu.Battle.Map","get_graphic",0x6011161,0x8869a0ULL,false,"Torappu.Battle.MapGraphic",0,{}},
{Method::GraphicActiveEnabled,"UnityEngine.Behaviour","get_isActiveAndEnabled",0x6000952,0x6b59040ULL,false,"System.Boolean",0,{}},
};
}
