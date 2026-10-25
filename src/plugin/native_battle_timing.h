#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace cntiming {
// Reports retain native fixedPlayTime simulation seconds. UI seconds are the
// effective elapsed time at the pinned original FAST multiplier (2.0).
struct Policy {
    std::int32_t version=0,display_seconds=0,simulation_seconds=0;
    std::int32_t speed_numerator=1,speed_denominator=1,boss_offset_seconds=0,force_end_seconds=300;
};
inline Policy configured(const std::string& mode,std::int32_t round,std::int32_t boss_offset=150){
    const bool standard=mode=="mode_single_funny";
    if(!standard&&mode!="mode_single_normal"&&mode!="mode_single_hard"&&mode!="mode_single_abyss")
        throw std::runtime_error("private battle timing mode invalid");
    if(round<1||round>(standard?9:15)||boss_offset<0||boss_offset>3600)
        throw std::runtime_error("private battle timing round or offset invalid");
    const bool boss=standard?round==9:round>=14;
    static constexpr std::int32_t durations[]={45,45,55,55,55,70,85,85,95,95,115,115,115};
    const auto display=boss?120:durations[round-1];
    return {1,display,display*2,2,1,boss?boss_offset:0,display+(boss?boss_offset:0)};
}
inline Policy legacy(bool boss){return {0,boss?150:300,boss?150:300,1,1,boss?150:0,300};}
inline std::int32_t overtime(const Policy& policy,std::int32_t elapsed){
    if(elapsed<0||elapsed>86400||policy.speed_numerator<1||policy.speed_denominator<1)
        throw std::runtime_error("private battle elapsed time invalid");
    return std::max<std::int32_t>(0,elapsed-policy.simulation_seconds)*policy.speed_denominator/policy.speed_numerator;
}
inline std::uint64_t ui_ticks(const Policy& policy,std::int32_t elapsed){
    if(elapsed<0||elapsed>86400||policy.speed_numerator<1||policy.speed_denominator<1)
        throw std::runtime_error("private battle elapsed time invalid");
    return std::uint64_t(elapsed)*10000000ULL*std::uint64_t(policy.speed_denominator)/std::uint64_t(policy.speed_numerator);
}
}
