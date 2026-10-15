#pragma once
#include "simulation.hpp"
#include <SDL.h>
#include <array>
#include <atomic>

namespace siege {
class Audio {
public:
    bool init();
    void shutdown();
    void play(Event event);
    void setMuted(bool value) { muted=value; }
    void setEngine(float value) { engine=value; }
private:
    struct Voice { float phase=0,remaining=0,duration=0,frequency=0,volume=0; int kind=0; };
    SDL_AudioDeviceID device=0;
    std::array<Voice,24> voices{};
    std::atomic<bool> muted{false};
    std::atomic<float> engine{0};
    float enginePhase=0;
    uint32_t noise=9182;
    static void callback(void* self,Uint8* stream,int length);
    void mix(float* stream,int count);
};
}
