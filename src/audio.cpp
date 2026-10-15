#include "audio.hpp"
#include <cstring>

namespace siege {
bool Audio::init() {
    SDL_AudioSpec wanted{},actual{};
    wanted.freq=audioRate;wanted.format=AUDIO_F32SYS;wanted.channels=2;wanted.samples=512;
    wanted.callback=callback;wanted.userdata=this;
    // SDL converts to the hardware format if necessary. Keep mixer stereo/48 kHz.
    device=SDL_OpenAudioDevice(nullptr,0,&wanted,&actual,SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if(device)SDL_PauseAudioDevice(device,0);
    return device!=0;
}
void Audio::shutdown(){if(device)SDL_CloseAudioDevice(device);device=0;}
void Audio::play(const SoundEvent& event){if(!device)return;SDL_LockAudioDevice(device);mixer.trigger(event);SDL_UnlockAudioDevice(device);}
void Audio::reset(){if(!device)return;SDL_LockAudioDevice(device);mixer.reset();SDL_UnlockAudioDevice(device);}
void Audio::setMuted(bool value){if(!device)return;SDL_LockAudioDevice(device);mixer.setMuted(value);SDL_UnlockAudioDevice(device);}
void Audio::setSuspended(bool value){if(!device)return;SDL_LockAudioDevice(device);mixer.setSuspended(value);SDL_UnlockAudioDevice(device);}
void Audio::setDrive(DriveSound value){if(!device)return;SDL_LockAudioDevice(device);mixer.setDrive(value);SDL_UnlockAudioDevice(device);}
void Audio::setVolumes(AudioVolumes value){if(!device)return;SDL_LockAudioDevice(device);mixer.setVolumes(value);SDL_UnlockAudioDevice(device);}
MixerStats Audio::statistics(){if(!device)return {};SDL_LockAudioDevice(device);auto stats=mixer.statistics();SDL_UnlockAudioDevice(device);return stats;}
void Audio::clearStatistics(){if(!device)return;SDL_LockAudioDevice(device);mixer.clearStatistics();SDL_UnlockAudioDevice(device);}
bool Audio::startCapture(float seconds){
    if(!device||seconds<=0||seconds>120)return false;
    std::vector<float> buffer(size_t(seconds*audioRate)*2);
    SDL_LockAudioDevice(device);capture.swap(buffer);captured=0;SDL_UnlockAudioDevice(device);return true;
}
bool Audio::saveCapture(const std::string& path){
    if(!device)return false;
    SDL_LockAudioDevice(device);std::vector<float> buffer(capture.begin(),capture.begin()+captured);capture.clear();captured=0;SDL_UnlockAudioDevice(device);
    return !buffer.empty()&&writeWave(path,buffer);
}
void Audio::callback(void* self,Uint8* stream,int length){
    auto& audio=*static_cast<Audio*>(self);int frames=length/int(sizeof(float)*2);
    audio.mixer.render(reinterpret_cast<float*>(stream),frames);
    size_t count=std::min(size_t(frames)*2,audio.capture.size()-audio.captured);
    if(count){std::memcpy(audio.capture.data()+audio.captured,stream,count*sizeof(float));audio.captured+=count;}
}
}
