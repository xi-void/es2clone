#pragma once
#include "simulation.hpp"

namespace siege {
constexpr float defaultRadarRange=800;
inline float radarRange(float meters) { return std::isfinite(meters)?clamp(meters,200,2400):defaultRadarRange; }
struct RadarPoint { float x,y; bool inRange; };
// Heading-up planar coordinates: +x right, -y forward. Visibility is deliberately
// separate from projection so terrain occlusion / sensor damage can be added later.
inline RadarPoint radarPoint(Vec3 position,const Player& player,float range) {
    Vec3 d=position-player.pos;
    float x=(d.x*std::cos(player.yaw)+d.z*std::sin(player.yaw))/radarRange(range);
    float y=(-d.x*std::sin(player.yaw)+d.z*std::cos(player.yaw))/radarRange(range);
    float distance=std::hypot(x,y);
    return {x/std::max(1.0f,distance),y/std::max(1.0f,distance),distance<=1};
}
}
