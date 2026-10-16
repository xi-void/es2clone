#include "synthesis.hpp"
#include "audio_assets.hpp"
#include "music_stream.hpp"
#include <fstream>

namespace siege {
namespace {
struct Noise {
    uint32_t state;
    float low=0,mid=0,high=0;
    float next(){state^=state<<13;state^=state>>17;state^=state<<5;return float(state&65535)/32768-1;}
};
float envelope(float t,float attack,float decay) {return (1-std::exp(-t/attack))*std::exp(-t/decay);}
float smooth(float x) {x=clamp(x,0,1);return x*x*(3-2*x);}
void follow(float& value,float target,float rate){value=std::abs(target-value)<1e-6f?target:value+(target-value)*rate;}
bool footEvent(Event e){return e==Event::FootLeft||e==Event::FootRight;}
bool hydraulicEvent(Event e){return e==Event::HydraulicLeft||e==Event::HydraulicRight;}
bool weightedEvent(Event e){return footEvent(e)||hydraulicEvent(e)||e==Event::ReactorIgnition||e==Event::MechanicalActivate;}
AudioBus busFor(Event e) {
    if(e==Event::Laser||e==Event::Cannon||e==Event::Missile||e==Event::Impact||e==Event::Explosion)return AudioBus::Weapons;
    if(e==Event::Lock||e==Event::Wave||e==Event::Win||e==Event::Lose||e==Event::Overheat||e==Event::DamageWarning)return AudioBus::Interface;
    return AudioBus::Machinery;
}

std::vector<float> generate(Event event,int weight,int variant) {
    const float masses[]={40,65,100};float mass=masses[weight],pitch=std::sqrt(65/mass);
    bool right=event==Event::FootRight||event==Event::HydraulicRight;
    float detune=(right?1.035f:0.975f)*(variant?1.018f:0.988f);
    float duration=footEvent(event)?1.55f:hydraulicEvent(event)?(variant?0.36f:0.62f):0.35f;
    if(event==Event::ReactorIgnition)duration=2.1f;
    if(event==Event::SystemInit)duration=1.45f;
    if(event==Event::MechanicalActivate)duration=1.65f;
    if(event==Event::ReactorReady)duration=1;
    if(event==Event::TurboEngage||event==Event::TurboRelease||event==Event::ReactorShutdown)duration=1.4f;
    if(event==Event::Explosion||event==Event::Win||event==Event::Lose)duration=1.15f;
    std::vector<float> result(size_t(duration*audioRate));
    Noise noise{uint32_t(2624+int(event)*313+weight*71+variant*13)};
    float dcInput=0,dcOutput=0;
    for(size_t i=0;i<result.size();++i) {
        float t=float(i)/audioRate,n=noise.next();
        noise.low+=0.018f*(n-noise.low);noise.mid+=0.10f*(n-noise.mid);noise.high+=0.35f*(n-noise.high);
        float sample=0;
        if(footEvent(event)) {
            float fundamental=clamp(47*pitch,36,62)*detune;
            float phase=2*pi*(fundamental*t+11*0.035f*(1-std::exp(-t/0.035f)));
            float impact=std::sin(phase)*envelope(t,0.004f,0.18f)*0.65f;
            float structure=(std::sin(2*pi*31*detune*t)*0.24f+std::sin(2*pi*74*detune*t)*0.17f)*envelope(t,0.012f,0.48f);
            float plate=0;
            for(float frequency:{137.0f,219.0f,351.0f,583.0f})
                plate+=std::sin(2*pi*frequency*detune*t)*envelope(t,0.008f,0.11f+27/frequency)*0.025f;
            float debris=(noise.mid-noise.low)*envelope(t,0.003f,0.075f)*0.25f;
            float release=t>0.055f?(noise.high-noise.mid)*envelope(t-0.055f,0.03f,0.14f)*0.035f:0;
            sample=impact+structure+plate+debris+release;
        } else if(hydraulicEvent(event)) {
            float e=std::sin(pi*t/duration);e*=e;
            float motor=std::sin(2*pi*(112*pitch*t+62*t*t))*0.020f;
            float gears=std::sin(2*pi*241*detune*t)*0.011f;
            sample=(motor*(variant?1.65f:1)+gears+(noise.mid-noise.low)*(variant?0.24f:0.14f))*e;
        } else if(event==Event::ReactorIgnition) {
            float spool=smooth(t/1.4f),end=1-smooth((t-1.6f)/0.5f);
            float resonance=std::sin(2*pi*(23*t+9*t*t))*0.15f+std::sin(2*pi*68*t)*0.045f;
            sample=(resonance+noise.low*0.65f)*spool*end;
            if(t>0.28f)sample+=std::sin(2*pi*39*pitch*(t-0.28f))*envelope(t-0.28f,0.02f,0.42f)*0.28f;
            if(t<0.2f)sample+=(noise.mid-noise.low)*envelope(t,0.002f,0.035f)*0.16f;
        } else if(event==Event::SystemInit) {
            for(float at:{0.03f,0.26f,0.59f,0.94f})if(t>=at){float q=t-at;sample+=(noise.mid*0.16f+std::sin(2*pi*173*q)*0.055f)*envelope(q,0.002f,0.055f);}
            sample+=std::sin(2*pi*97*t)*0.012f*std::sin(pi*t/duration);
        } else if(event==Event::MechanicalActivate) {
            float e=smooth(t/0.18f)*(1-smooth((t-1.3f)/0.35f));
            sample=((noise.mid-noise.low)*0.22f+std::sin(2*pi*(85*pitch*t+23*t*t))*0.025f)*e;
            for(float at:{0.12f,0.74f,1.30f})if(t>=at){float q=t-at;sample+=(std::sin(2*pi*53*pitch*q)*0.22f+noise.low*0.22f)*envelope(q,0.008f,0.16f);}
        } else if(event==Event::ReactorReady) {
            sample=std::sin(2*pi*62*t)*envelope(t,0.025f,0.3f)*0.12f;
            if(t>0.12f)sample+=(noise.mid-noise.low)*envelope(t-0.12f,0.004f,0.08f)*0.14f;
        } else if(event==Event::TurboEngage||event==Event::TurboRelease||event==Event::ReactorShutdown) {
            bool engage=event==Event::TurboEngage;
            float e=smooth(t/0.10f)*(1-smooth((t-0.9f)/0.5f));
            float phase=engage?(95*t+115*t*t):(420*t-100*t*t);
            sample=(std::sin(2*pi*phase)*0.046f+std::sin(2*pi*48*t)*0.085f+(noise.mid-noise.low)*0.12f)*e;
            if(engage)sample+=std::sin(2*pi*67*t)*envelope(t,0.008f,0.10f)*0.13f;
        } else {
            float frequency=event==Event::Laser?920:event==Event::Lock?1320:event==Event::Overheat?650:85;
            bool metallic=event==Event::Cannon||event==Event::Missile||event==Event::Explosion||event==Event::Impact;
            float decay=event==Event::Explosion?0.38f:event==Event::Missile?0.22f:0.09f;
            if(metallic)sample=(noise.mid*0.8f+std::sin(2*pi*frequency*t)*0.26f)*envelope(t,0.003f,decay);
            else sample=std::sin(2*pi*(frequency*t-frequency*0.5f*t*t))*envelope(t,0.006f,0.12f)*0.12f;
        }
        float filtered=sample-dcInput+0.9974f*dcOutput;dcInput=sample;dcOutput=filtered;
        result[i]=filtered*smooth(float(result.size()-i-1)/(audioRate*0.025f));
    }
    return result;
}
}
AudioMixer::AudioMixer() {
    for(size_t i=0;i<sine.size();++i)sine[i]=std::sin(float(i)*2*pi/sine.size());
    for(int e=0;e<int(Event::Count);++e) {
        int weights=weightedEvent(Event(e))?3:1,variants=(footEvent(Event(e))||hydraulicEvent(Event(e)))?2:1;
        for(int w=0;w<weights;++w)for(int v=0;v<variants;++v)clips[size_t(e*6+w*2+v)]=generate(Event(e),w,v);
    }
}
void AudioMixer::reset() {
    voices={};drive={};power=motion=load=turbo=pressure=0;
    reactorPhase=servoPhase=turbinePhase=0;variation=0;stats={};loopPositions={};selections={};duck=1;duckHold=0;limiter=1;
}
void AudioMixer::setVolumes(AudioVolumes value){for(size_t i=0;i<volumes.size();++i)volumes[i]=clamp(value[i],0,1);}
size_t AudioMixer::sampleBytes() const {size_t bytes=0;for(const auto& clip:clips)bytes+=clip.size()*sizeof(float);return bytes;}
int AudioMixer::activeVoices() const {int count=0;for(const auto& v:voices)if(v.clip)++count;return count;}
void AudioMixer::trigger(const SoundEvent& event) {
    if(event.type==Event::Count||event.strength<=0)return;
    int weight=weightedEvent(event.type)?(event.mass<50?0:event.mass>85?2:1):0;
    int variant=footEvent(event.type)?int((variation++/2)%2):hydraulicEvent(event.type)&&event.activity>0.85f?1:0;
    const std::vector<float>* clip=&clips[size_t(int(event.type)*6+weight*2+variant)];
    int channels=1;AudioBus bus=busFor(event.type);float mappedGain=1;bool imported=false;
    bool boot=assets&&assets->startup&&event.type==Event::ReactorIgnition;
    if(assets){
        const auto& choices=assets->events[int(event.type)];
        // Track one is the user's complete boot sequence. Avoid doubling it with
        // the old generated startup cues; explicitly mapped cues remain available.
        if(assets->startup&&choices.empty()&&(event.type==Event::SystemInit||event.type==Event::MechanicalActivate||event.type==Event::ReactorReady))return;
        if(boot){clip=&assets->startup->samples;channels=2;bus=AudioBus::Music;imported=true;++stats.startupCues;}
        else if(!choices.empty()){
            clip=&choices[selections[int(event.type)]++%choices.size()]->samples;channels=2;imported=true;
        }
        mappedGain=assets->config.gains[int(event.type)];
    }
    if(clip->empty())return;
    Voice* slot=&voices[0];float least=1e9f;
    for(auto& voice:voices) {
        if(!voice.clip){slot=&voice;break;}
        float remaining=(float(voice.clip->size())-voice.position)/voice.channels;
        if(remaining<least){least=remaining;slot=&voice;}
    }
    float pan=clamp(event.pan,-0.9f,0.9f),gain=clamp(event.strength,0,1.4f)*mappedGain;
    float scale=channels==2?1.0f:0.5f;
    *slot={clip,0,gain*std::sqrt((1-pan)*scale),gain*std::sqrt((1+pan)*scale),bus,channels,bus!=AudioBus::Interface&&event.type!=Event::ReactorShutdown};
    if(imported)++stats.assetEvents;else ++stats.fallbackEvents;
    if(footEvent(event.type)||event.type==Event::Explosion)duck=std::min(duck,0.72f);
    if(event.type==Event::Overheat||event.type==Event::DamageWarning||event.type==Event::Lose||event.type==Event::Wave){
        duckHold=std::max(duckHold,std::max(2.0f,float(clip->size())/channels/audioRate));
    }

}
float AudioMixer::oscillator(float& phase,float frequency) noexcept {
    phase+=frequency*float(sine.size())/audioRate;
    if(phase>=sine.size())phase-=float(sine.size());
    int at=int(phase);float fraction=phase-at;
    return sine[size_t(at)]+(sine[size_t((at+1)%int(sine.size()))]-sine[size_t(at)])*fraction;
}
void AudioMixer::render(float* stereo,int frames) noexcept {
    float weight=std::sqrt(65/clamp(drive.mass,40,100));
    for(int frame=0;frame<frames;++frame) {
        float target=muted?0:1;
        volume+=clamp(target-volume,-1.0f/576,1.0f/576);
        worldVolume+=clamp((suspended?0.0f:1.0f)-worldVolume,-1.0f/576,1.0f/576);
        for(size_t i=0;i<levels.size();++i)follow(levels[i],volumes[i],0.002f);
        float left=0,right=0;
        if(!suspended||worldVolume>0) {
            follow(power,drive.power,0.00018f);
            follow(motion,drive.speed,0.00035f);
            follow(load,drive.load,0.0003f);
            follow(turbo,drive.turbo,0.00007f);
            float reactor=oscillator(reactorPhase,(33+4*power)*weight)*(0.033f+0.010f*load)*power;
            float servo=oscillator(servoPhase,(112+motion*70+load*25)*weight)*motion*0.014f*power;
            random^=random<<13;random^=random>>17;random^=random<<5;
            float noise=float(random&65535)/32768-1;
            pressure+=0.045f*(noise-pressure);
            float pump=pressure*(0.018f+0.03f*load)*motion*power;
            float turbine=oscillator(turbinePhase,(180+turbo*250)*weight)*0.028f*turbo*power;
            float resonance=pressure*0.065f*turbo*power;
            float loopLeft=0,loopRight=0;
            float* layers[]={&reactor,&servo,&turbine};
            float gains[]={0.20f*power,0.18f*motion*power,0.24f*turbo*power};
            for(int i=0;i<3;++i)if(assets&&assets->loops[i]){
                const auto& samples=assets->loops[i]->samples;size_t count=samples.size()/2;
                size_t at=loopPositions[i]%count;
                // Short wrap crossfade prevents a hard edge without retuning the recording.
                size_t fade=std::min<size_t>(240,count/4);float l=samples[at*2],r=samples[at*2+1];
                if(fade&&at>=count-fade){float t=float(at-(count-fade))/fade;size_t first=at-(count-fade);l=l*(1-t)+samples[first*2]*t;r=r*(1-t)+samples[first*2+1]*t;}
                loopPositions[i]=at+1==count?fade:at+1;
                loopLeft+=l*gains[i];loopRight+=r*gains[i];*layers[i]=0;
                if(i==1)pump=0;
            }
            float gain=levels[int(AudioBus::Machinery)]*worldVolume;
            left=(reactor+servo+pump+turbine+resonance+loopLeft)*gain;
            right=(reactor+servo+pump+turbine+resonance+loopRight)*gain;
        }
        duckHold=std::max(0.0f,duckHold-1.0f/audioRate);
        float duckTarget=duckHold>0?(assets?assets->config.warningDuck:0.32f):1.0f;
        follow(duck,duckTarget,duckTarget<duck?0.002f:0.000045f);
        for(auto& v:voices)if(v.clip) {
            if(suspended&&worldVolume<=0&&v.world)continue;
            size_t at=size_t(v.position);
            if(at+size_t(v.channels)>v.clip->size()){v.clip=nullptr;continue;}
            float gain=levels[int(v.bus)]*(v.world?worldVolume:1);
            if(v.bus==AudioBus::Music)gain*=duck;
            left+=(*v.clip)[at]*v.left*gain;
            right+=(*v.clip)[at+(v.channels==2?1:0)]*v.right*gain;
            v.position+=float(v.channels);
        }
        follow(musicLevel,musicActive?1.0f:0.0f,0.0002f);
        if(music&&(musicActive||musicLevel>1e-5f)){
            float l=0,r=0;if(music->pop(l,r))++stats.musicFrames;
            float gain=levels[int(AudioBus::Music)]*duck*musicLevel;
            left+=l*gain;right+=r*gain;
        }
        // Linked stereo limiter: unity gain below the ceiling, so original
        // recordings retain their timbre. Instant attack, gentle release.
        float peak=std::max(std::abs(left),std::abs(right));
        float limit=peak>0.97f?0.97f/peak:1.0f;
        if(limit<limiter)limiter=limit;else follow(limiter,limit,0.00005f);
        left*=limiter*volume;right*=limiter*volume;
        stereo[frame*2]=left;stereo[frame*2+1]=right;
        ++stats.frames;if(std::abs(left)+std::abs(right)>1e-6f)++stats.audibleFrames;
        stats.peak=std::max(stats.peak,std::max(std::abs(left),std::abs(right)));
        if(!std::isfinite(left)||!std::isfinite(right))stats.finite=false;
    }
}
bool writeWave(const std::string& path,const std::vector<float>& stereo) {
    std::ofstream out(path,std::ios::binary);if(!out)return false;
    auto u16=[&](uint16_t v){out.put(char(v));out.put(char(v>>8));};
    auto u32=[&](uint32_t v){u16(uint16_t(v));u16(uint16_t(v>>16));};
    uint32_t bytes=uint32_t(stereo.size()*2);
    out.write("RIFF",4);u32(36+bytes);out.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(audioRate);u32(audioRate*4);u16(4);u16(16);out.write("data",4);u32(bytes);
    for(float sample:stereo)u16(uint16_t(int16_t(clamp(sample,-1,1)*32767)));
    return bool(out);
}
}
