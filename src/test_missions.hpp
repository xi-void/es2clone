#pragma once
#include "simulation.hpp"

namespace siege::testMissions {
// Local engine test content; no original campaign resources or audio assets.
// 600 x 600 m open range, with five 100 m route legs (including insertion -> WP1).
inline constexpr Mission waypointRange{
    "Waypoint Test Range","NAVIGATION TEST",
    "Visit five navigation waypoints in order. No enemies. Each arrival advances the route.",0,0
};
inline constexpr MissionLayout waypointRangeLayout{
    {0,0,200}, {{{0,0,100},{0,0,0},{100,0,0},{100,0,-100},{0,0,-100}}}, {},5,true,300
};
inline constexpr Mission modelRange{
    "HERC Model Test Range","MODEL / COMBAT TEST",
    "Inspect a human Tomahawk and Cybrid Achilles. Both targets are live. Destroy them, then extract.",2,0.25f
};
inline constexpr MissionLayout modelRangeLayout{
    {0,0,45}, {{{0,0,-30},{0,0,-30},{0,0,-30}}}, {0,0,-110},0,true,300
};
}
