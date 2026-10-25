#pragma once
namespace mixedhelp::concrete {
struct ExtraBinding {const char* id;const char* owner;const char* name;uint32_t token;uint64_t rva;bool isStatic;const char* result;uint32_t count;const char* parameters[7];};
inline const ExtraBinding EXTRA_BINDINGS[]={
{"TileSetDataHook","Torappu.Battle.Tile","SetData",0x601144a,0x8c2f00ULL,false,"System.Void",4,{"Torappu.TileData","Torappu.GridPosition","Torappu.Battle.Map","Torappu.MapLayer"}},
{"MapHeightHook","Torappu.Battle.Map","GetTileHeight",0x6011184,0x87d9e0ULL,false,"System.Single",1,{"Torappu.TileData+HeightType"}},
{"ControllerLoadSeam","Torappu.Battle.BattleController","LoadGameWithRune",0x600fd2c,0x627330ULL,false,"System.Void",7,{"System.Collections.Generic.List`1<Torappu.Battle.BattlePlayerData>","Torappu.LevelData","Torappu.MapData","Torappu.LevelData+Difficulty","Torappu.IRuneDataHolder","Torappu.LevelData+Options&","Torappu.Battle.Runes.Rune+RuneLevelExtraOutput&"}},
{"ArrayRank","System.Array","get_Rank",0x6000e88,0x5eeb680ULL,false,"System.Int32",0,{}},
{"GameObjectChildren","UnityEngine.GameObject","GetComponentsInChildren",0x60009a5,0x6b5be50ULL,false,"UnityEngine.Component[]",2,{"System.Type","System.Boolean"}},
{"ComponentTransform","UnityEngine.Component","get_transform",0x6000955,0x6b59e30ULL,false,"UnityEngine.Transform",0,{}},
{"TransformIsChildOf","UnityEngine.Transform","IsChildOf",0x6000be4,0x6b72ad0ULL,false,"System.Boolean",1,{"UnityEngine.Transform"}},
{"RendererEnable","UnityEngine.Renderer","set_enabled",0x60003e2,0x6b40240ULL,false,"System.Void",1,{"System.Boolean"}},
{"BehaviourEnable","UnityEngine.Behaviour","set_enabled",0x6000951,0x6b59080ULL,false,"System.Void",1,{"System.Boolean"}},
{"RendererBakedIndex","UnityEngine.Renderer","set_lightmapIndex",0x6000416,0x6b40370ULL,false,"System.Void",1,{"System.Int32"}},
{"RendererRealtimeIndex","UnityEngine.Renderer","set_realtimeLightmapIndex",0x6000418,0x6b405c0ULL,false,"System.Void",1,{"System.Int32"}},
{"GameObjectActive","UnityEngine.GameObject","get_activeInHierarchy",0x60009c2,0x6b5c940ULL,false,"System.Boolean",0,{}},
{"TileHidden","Torappu.Battle.Tile","get_isHidden",0x6011411,0x8c5e60ULL,false,"System.Boolean",0,{}},
{"TileGenerateEffect","Torappu.Battle.Tile","GenerateTileEffect",0x601146d,0x8bda70ULL,false,"System.Void",0,{}},
{"EffectFinished","Torappu.Battle.Effects.Effect","get_isFinished",0x6018994,0x119a920ULL,false,"System.Boolean",0,{}},
{"EffectFinish","Torappu.Battle.Effects.Effect","FinishMe",0x60189a7,0x1195c30ULL,false,"System.Void",1,{"System.Boolean"}},
{"ListDictLookup","Torappu.ListDict`2","TryGetValue",0x60001a8,0ULL,false,"System.Boolean",2,{"T2096","T2097&"}},
{"ListDictRemove","Torappu.ListDict`2","Remove",0x60001a7,0ULL,false,"System.Boolean",1,{"T2096"}},
};
}
