#pragma once
#include "simulation.hpp"
#include <array>
#include <string>
#include <vector>

namespace siege {
constexpr int audioRate=48000;
enum class AudioBus { Music, Weapons, Machinery, Interface, Count };
using AudioVolumes=std::array<float,4>;
constexpr AudioVolumes defaultVolumes{0.22f,0.8f,1.0f,0.55f};
struct DriveSound { float power=0,speed=0,load=0,mass=65,turbo=0; };
struct MixerStats { uint64_t frames=0,audibleFrames=0,assetEvents=0,fallbackEvents=0,musicFrames=0,startupCues=0; float peak=0; bool finite=true; };

class AudioAssets;
class MusicStream;
// Assets are generated once. The callback mixes bounded voices without allocations.
class AudioMixer {
public:
    AudioMixer();
    void trigger(const SoundEvent& event);
    void configure(const AudioAssets* value,MusicStream* stream=nullptr){assets=value;music=stream;}
    void setMusicActive(bool value){musicActive=value;}
    void render(float* stereo,int frames) noexcept;
    void reset();
    void setDrive(DriveSound value) { drive=value; }
    void setMuted(bool value) { muted=value; }
    void setSuspended(bool value) { suspended=value; }
    void setVolumes(AudioVolumes value);
    MixerStats statistics() const { return stats; }
    void clearStatistics() { stats={}; }
    size_t sampleBytes() const;
    int activeVoices() const;
private:
    struct Voice { const std::vector<float>* clip=nullptr; float position=0,left=0,right=0; AudioBus bus=AudioBus::Machinery; int channels=1; bool world=true; };
    std::array<std::vector<float>,int(Event::Count)*6> clips;
    std::array<Voice,32> voices{};
    std::array<float,2048> sine{};
    AudioVolumes volumes=defaultVolumes,levels=defaultVolumes;
    DriveSound drive;
    MixerStats stats;
    float reactorPhase=0,servoPhase=0,turbinePhase=0,power=0,motion=0,load=0,turbo=0,volume=0,worldVolume=1,pressure=0;
    const AudioAssets* assets=nullptr;
    MusicStream* music=nullptr;
    std::array<size_t,3> loopPositions{};
    std::array<unsigned,int(Event::Count)> selections{};
    float duck=1,duckHold=0,musicLevel=0,limiter=1;
    bool musicActive=false;
    bool muted=false,suspended=false;
    unsigned variation=0;
    uint32_t random=71623;
    float oscillator(float& phase,float frequency) noexcept;
};
bool writeWave(const std::string& path,const std::vector<float>& stereo);
}
