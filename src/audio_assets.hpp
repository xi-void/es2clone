#pragma once
#include "synthesis.hpp"
#include <SDL.h>
#include <sndfile.h>
#include <filesystem>
#include <memory>
#include <map>

namespace siege {
struct AudioClip { std::vector<float> samples; }; // Interleaved stereo, 48 kHz; no pitch changes.
class AudioDecoder {
public:
    ~AudioDecoder();
    bool open(const std::filesystem::path& path);
    int read(float* stereo,int frames);
    // Estimate only: embedded artwork may inflate some MP3 lengths.
    int64_t totalFrames() const { return total; }
private:
    SNDFILE* file=nullptr;
    SDL_AudioStream* stream=nullptr;
    SF_INFO info{};
    bool ended=false;
    int64_t total=0;
};
struct AudioConfig {
    AudioVolumes overrides{-1,-1,-1,-1};
    bool random=false,menuMusic=false;
    float crossfade=0.75f,warningDuck=0.32f;
    std::string startupTrack="auto";
    std::array<std::string,int(Event::Count)> events{};
    std::array<float,int(Event::Count)> gains{};
    std::array<std::string,3> loops{};
    AudioConfig(){gains.fill(1);}
};
class AudioAssets {
public:
    void load(const std::filesystem::path& directory,const std::filesystem::path& configFile={});
    AudioConfig config;
    std::array<std::vector<std::shared_ptr<AudioClip>>,int(Event::Count)> events;
    std::array<std::shared_ptr<AudioClip>,3> loops;
    std::shared_ptr<AudioClip> startup;
    std::vector<std::filesystem::path> playlist;
    size_t bytes=0;
    static const char* eventName(Event event);
private:
    std::filesystem::path root;
    std::map<std::string,std::shared_ptr<AudioClip>> cache;
    void readConfig(const std::filesystem::path& path);
    std::shared_ptr<AudioClip> loadClip(const std::string& relative);
};
}
