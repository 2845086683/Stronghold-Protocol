#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace cnlaunch {
struct SceneEpoch {
 std::uint64_t epoch=0,managerEpoch=0,completedEpoch=0;
 std::uint32_t controllerRoot=0,iteratorRoot=0;
 void* manager=nullptr;bool pending=false;
};
struct SceneTransition {SceneEpoch before;std::uint32_t managerRoot=0;bool complete=false,entered=false,consumed=false;};
inline constexpr std::size_t object_header_size=16;
inline constexpr std::size_t start_param_size=64;
inline constexpr std::size_t controller_param_size=680;
inline constexpr std::int32_t local_mode=0;

// The CN Win64 static method receives this value indirectly. These are raw
// managed-reference bits, never owned pointers or boxed field offsets.
struct alignas(8) RawStartBattleParam {
    std::uint64_t act_id=0;
    std::uint64_t stage_id=0;
    std::uint64_t scene_id=0;
    std::uint64_t mode_id=0;
    std::uint64_t stage_name=0;
    std::uint64_t stage_code=0;
    std::uint64_t loading_pic_id=0;
    std::int32_t mode_type=0;
    std::int32_t multi_mode_sub_type=0;
};

// BattleStartController.Param has a different ABI from StartBattleParam.
// Runtime metadata validation and field access remain in the native bridge.
struct alignas(8) ControllerRaw {
    std::array<std::uint8_t,controller_param_size> bytes{};
};

// AutoChessService.Start/Reconnect are static: RCX points at this indirect
// value, RDX carries AutoChessServiceParam and R8 carries hidden MethodInfo.
struct alignas(8) CommonJoinEntryRaw {
    std::uint64_t id=0;
    std::uint64_t svr_address=0;
    std::uint64_t svr_token=0;
    std::uint64_t on_join_done=0;
};

constexpr bool target_activity(std::string_view activity_id) noexcept {
    return activity_id=="act2autochess";
}

constexpr bool is_private_local_launch(std::string_view activity_id,std::int32_t mode_type) noexcept {
    return target_activity(activity_id)&&mode_type==local_mode;
}

static_assert(std::is_standard_layout_v<RawStartBattleParam>);
static_assert(std::is_trivially_copyable_v<RawStartBattleParam>);
static_assert(sizeof(RawStartBattleParam)==start_param_size);
static_assert(sizeof(ControllerRaw)==controller_param_size);
static_assert(std::is_standard_layout_v<CommonJoinEntryRaw>);
static_assert(std::is_trivially_copyable_v<CommonJoinEntryRaw>);
static_assert(sizeof(CommonJoinEntryRaw)==32);
static_assert(offsetof(CommonJoinEntryRaw,on_join_done)==24);
}
