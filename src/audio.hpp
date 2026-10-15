#pragma once
#include "synthesis.hpp"
#include <SDL.h>

namespace siege {
class Audio {
public:
    bool init();
    void shutdown();
    void play(const SoundEvent& event);
    void reset();
    void setMuted(bool value);
    void setSuspended(bool value);
    void setDrive(DriveSound value);
    void setVolumes(AudioVolumes value);
    MixerStats statistics();
    void clearStatistics();
    bool startCapture(float seconds);
    bool saveCapture(const std::string& path);
private:
    SDL_AudioDeviceID device=0;
    AudioMixer mixer;
    std::vector<float> capture;
    size_t captured=0;
    static void callback(void* self,Uint8* stream,int length);
};
}
