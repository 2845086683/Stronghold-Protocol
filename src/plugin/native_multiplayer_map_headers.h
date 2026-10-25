#pragma once
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <type_traits>
#include "native_multiplayer_map_bindings.h"
#include "native_multiplayer_map_extra_bindings.h"
#include "native_multiplayer_map_pipeline.h"
#include "native_multiplayer_map_height_policy.h"
#include "native_multiplayer_map_height_bindings.h"
namespace mixedhelp {
static_assert(sizeof(Position)==8&&std::is_trivially_copyable<Position>::value,"fixed CN GridPosition Win64 ABI");
using MapInitNative=void(*)(Object,Object,Object,Object,Object,const void*);
using TileSetDataNative=void(*)(Object,Object,Position,Object,int32_t,const void*);
using MapHeightNative=float(*)(Object,int32_t,const void*);
}
