#pragma once
#include "synthesis.hpp"
#include "audio_assets.hpp"
#include "music_stream.hpp"
#include <SDL.h>

namespace siege {
class Audio {
public:
    bool init(const std::string& directory="",const std::string& config="");
    ~Audio(){shutdown();}
    void setMusicActive(bool value);
    const AudioAssets& assetInfo() const {return assets;}
    AudioVolumes configuredVolumes(AudioVolumes saved) const;
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
    AudioAssets assets;
    MusicStream music;
    AudioMixer mixer;
    std::vector<float> capture;
    size_t captured=0;
    static void callback(void* self,Uint8* stream,int length);
};
}
