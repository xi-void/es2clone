#include "audio.hpp"
#include <cmath>
#include <cstring>

namespace siege {
bool Audio::init() {
    SDL_AudioSpec wanted{},actual{};
    wanted.freq=48000; wanted.format=AUDIO_F32SYS; wanted.channels=1; wanted.samples=1024;
    wanted.callback=callback; wanted.userdata=this;
    device=SDL_OpenAudioDevice(nullptr,0,&wanted,&actual,0);
    if(device) SDL_PauseAudioDevice(device,0);
    return device!=0;
}
void Audio::shutdown() { if(device) SDL_CloseAudioDevice(device);device=0; }
void Audio::play(Event event) {
    if(!device) return;
    Voice v;
    switch(event) {
    case Event::Laser: v={0,0.18f,0.18f,920,0.13f,0};break;
    case Event::Cannon: v={0,0.24f,0.24f,110,0.23f,1};break;
    case Event::Missile: v={0,0.5f,0.5f,210,0.16f,1};break;
    case Event::Explosion: v={0,0.9f,0.9f,65,0.28f,1};break;
    case Event::Impact: v={0,0.15f,0.15f,95,0.15f,1};break;
    case Event::Step: v={0,0.12f,0.12f,48,0.10f,1};break;
    case Event::Lock: v={0,0.14f,0.14f,1320,0.09f,2};break;
    case Event::Wave: v={0,0.45f,0.45f,460,0.09f,2};break;
    case Event::Win: v={0,1.3f,1.3f,440,0.13f,3};break;
    case Event::Lose: v={0,1.3f,1.3f,220,0.13f,0};break;
    case Event::Overheat: v={0,0.6f,0.6f,650,0.12f,2};break;
    }
    SDL_LockAudioDevice(device);
    auto* slot=&voices[0];
    for(auto& voice:voices) { if(voice.remaining<=0) {slot=&voice;break;} if(voice.remaining<slot->remaining) slot=&voice; }
    *slot=v;
    SDL_UnlockAudioDevice(device);
}
void Audio::callback(void* self,Uint8* stream,int length) { static_cast<Audio*>(self)->mix(reinterpret_cast<float*>(stream),length/int(sizeof(float))); }
void Audio::mix(float* stream,int count) {
    float motor=engine.load(); bool silent=muted.load();
    for(int i=0;i<count;++i) {
        enginePhase+=2*pi*(35+motor*25)/48000;
        if(enginePhase>2*pi) enginePhase-=2*pi;
        float sample=(std::sin(enginePhase)+std::sin(enginePhase*2)*0.3f)*0.025f*motor;
        for(auto& v:voices) if(v.remaining>0) {
            float t=1-v.remaining/v.duration,envelope=std::pow(1-t,1.8f);
            float frequency=v.frequency*(v.kind==0?1-t*0.8f:v.kind==3?1+std::floor(t*4)*0.25f:1);
            v.phase+=2*pi*frequency/48000;
            if(v.phase>2*pi) v.phase-=2*pi;
            noise^=noise<<13; noise^=noise>>17; noise^=noise<<5;
            float random=float(noise&65535)/32768-1;
            float wave=v.kind==1?random*0.55f+std::sin(v.phase)*0.45f:std::sin(v.phase);
            if(v.kind==2) wave*=std::sin(t*30)>0?1:0.1f;
            sample+=wave*envelope*v.volume; v.remaining-=1.0f/48000;
        }
        stream[i]=silent?0:std::tanh(sample);
    }
}
}
