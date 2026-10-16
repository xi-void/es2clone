#include "audio.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <numeric>
using namespace siege;
namespace {
int checks=0;
void expect(bool value,const char* message){++checks;if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
std::vector<float> render(AudioMixer& mixer,float seconds){std::vector<float> out(size_t(seconds*audioRate)*2);mixer.render(out.data(),int(out.size()/2));return out;}
double energy(const std::vector<float>& x,int channel=-1){double total=0;for(size_t i=channel<0?0:size_t(channel);i<x.size();i+=channel<0?1:2)total+=double(x[i])*x[i];return total/(x.size()/(channel<0?1:2));}
float peak(const std::vector<float>& x){float p=0;for(float v:x)p=std::max(p,std::abs(v));return p;}
void isolate(AudioMixer& mixer,AudioBus bus){mixer.reset();AudioVolumes v{};v[int(bus)]=1;mixer.setVolumes(v);render(mixer,0.3f);}
std::vector<float> eventAudio(AudioMixer& mixer,SoundEvent event){isolate(mixer,AudioBus::Machinery);mixer.trigger(event);return render(mixer,1.8f);}
bool waitFrames(Audio& audio,uint64_t count){uint64_t target=audio.statistics().frames+count;Uint32 began=SDL_GetTicks();while(audio.statistics().frames<target&&SDL_GetTicks()-began<3000)SDL_Delay(5);return audio.statistics().frames>=target;}
}
int main(){
    auto began=std::chrono::steady_clock::now();AudioMixer mixer;
    expect(mixer.sampleBytes()<16*1024*1024,"all procedural assets fit within 16 MiB");
    for(int value=0;value<int(Event::Count);++value){
        mixer.reset();mixer.setVolumes({0,1,1,1});render(mixer,0.3f);mixer.trigger(SoundEvent(Event(value)));
        auto out=render(mixer,0.6f);expect(peak(out)>0.01f&&mixer.statistics().finite,"every sound event renders finite audible PCM");
    }
    auto left=eventAudio(mixer,{Event::FootLeft,1,-0.3f,65}),right=eventAudio(mixer,{Event::FootRight,1,0.3f,65});
    expect(energy(left,0)>energy(left,1)*1.5&&energy(right,1)>energy(right,0)*1.5,"left and right impacts have distinct stereo placement");
    // Filter energy, rather than testing specific implementation frequencies.
    double total=0,bass=0,mean=0;float low=0;
    for(size_t i=0;i<left.size();i+=2){float x=(left[i]+left[i+1])*0.5f;low+=0.01945f*(x-low);total+=x*x;bass+=low*low;mean+=x;}
    expect(bass/total>0.70,"foot impacts carry most energy below 150 Hz");
    expect(std::abs(mean/(left.size()/2))<0.0002,"impact has negligible DC offset");
    double difference=0;for(size_t i=0;i<left.size();i+=2)difference+=std::abs(left[i]-right[i+1]);
    expect(difference/(left.size()/2)>0.005,"left and right have distinct mechanical timbres, beyond panning");
    auto light=eventAudio(mixer,{Event::FootLeft,1,0,40}),heavy=eventAudio(mixer,{Event::FootLeft,1,0,100});
    expect(light!=heavy,"chassis masses use different resonant impact assets");
    auto normal=eventAudio(mixer,{Event::FootLeft,1,-0.3f,65,0.5f}),turbo=eventAudio(mixer,{Event::FootLeft,1,-0.3f,65,1.6f});
    expect(normal==turbo,"turbo never changes impact sample playback speed or pitch");
    auto hydraulic=eventAudio(mixer,{Event::HydraulicLeft,1,-0.3f,65,0.5f}),fastHydraulic=eventAudio(mixer,{Event::HydraulicLeft,1,-0.3f,65,1.6f});
    expect(hydraulic!=fastHydraulic&&energy(fastHydraulic)>energy(hydraulic)*0.8,"high-speed hydraulics use shorter, stronger independently synthesized motion");
    for(int bus=1;bus<4;++bus){
        const Event events[]={Event::Lock,Event::Cannon,Event::FootLeft,Event::Lock};
        isolate(mixer,AudioBus(bus));if(bus)mixer.trigger(events[bus]);
        auto audible=render(mixer,0.2f);expect(energy(audible)>1e-6,"each bus independently produces output");
        mixer.reset();AudioVolumes v{1,1,1,1};v[bus]=0;mixer.setVolumes(v);render(mixer,0.3f);
        // Silence the music too when testing suppression of an event bus.
        if(bus){v[0]=0;mixer.setVolumes(v);render(mixer,0.3f);mixer.trigger(events[bus]);}
        auto silent=render(mixer,0.2f);expect(peak(silent)<0.00001f,"each volume control independently suppresses its own bus");
    }
    AudioMixer reference,continuity;AudioVolumes musicOnly{1,0,0,0};reference.setVolumes(musicOnly);continuity.setVolumes(musicOnly);
    render(reference,1);render(continuity,1);continuity.setSuspended(true);continuity.reset();
    expect(render(reference,2)==render(continuity,2),"music remains sample-continuous across pause and mission reset");
    continuity.setMuted(true);render(continuity,0.1f);expect(peak(render(continuity,0.1f))==0,"master mute fades completely to silence");
    continuity.setMuted(false);expect(peak(render(continuity,0.2f))==0,"missing soundtrack stays silent; no generated replacement music");
    isolate(mixer,AudioBus::Machinery);mixer.setDrive({1,0,0,65,0});render(mixer,1);auto idle=render(mixer,1);
    mixer.setDrive({1,1.6f,1,65,1});render(mixer,1);auto highPower=render(mixer,1);
    expect(energy(highPower)>energy(idle)*1.5,"high-power turbine, servo and resonance layers differ from idle reactor hum");
    mixer.setSuspended(true);render(mixer,0.1f);expect(peak(render(mixer,0.1f))<1e-6,"pause silences world machinery");
    mixer.setSuspended(false);render(mixer,0.1f);expect(energy(render(mixer,0.1f))>1e-5,"resume restores the reactor without restarting music");
    mixer.setVolumes({1,1,1,1});render(mixer,0.3f);mixer.clearStatistics();
    for(int i=0;i<100;++i)mixer.trigger({i%2?Event::FootLeft:Event::Explosion,1.4f,(i%3-1)*0.8f,100,1.6f});
    expect(mixer.activeVoices()==32,"combat voice pool is bounded");
    auto stress=render(mixer,3);expect(mixer.statistics().finite&&peak(stress)<0.98f,"saturated weapons/music/machinery mix remains finite without clipping");
    double meanStress=std::accumulate(stress.begin(),stress.end(),0.0)/stress.size();
    expect(std::abs(meanStress)<0.005,"high-load mix does not develop a DC bias");
    // Repeatable CPU workload; report timing instead of a flaky machine-specific limit.
    auto benchmark=std::chrono::steady_clock::now();std::array<float,1024> buffer{};
    for(int block=0;block<3000;++block){if(block%20==0)for(int i=0;i<4;++i)mixer.trigger({Event::FootLeft,0.7f});mixer.render(buffer.data(),512);}
    auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-benchmark).count();
    std::cout<<"Mixer: "<<mixer.sampleBytes()/1048576.0<<" MiB assets; "<<32/elapsed<<"x realtime for layered mix\n";

    expect(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0,"SDL audio subsystem initializes");Audio audio;
    expect(audio.init(),"SDL opens stereo floating-point 48 kHz callback device");
    std::cout<<"SDL audio driver: "<<SDL_GetCurrentAudioDriver()<<'\n';
    audio.setVolumes({0,0,1,0});expect(waitFrames(audio,16000),"SDL callback runs at the requested mix rate");
    audio.clearStatistics();audio.play({Event::FootLeft,1,-0.3f});expect(waitFrames(audio,12000),"SDL callback consumes a contact event");
    auto stats=audio.statistics();expect(stats.finite&&stats.audibleFrames>10000&&stats.peak>0.1f&&stats.peak<0.98f,"actual SDL callback contains finite audible mechanical PCM");
    audio.reset();audio.setVolumes({1,1,1,1});audio.setDrive({1,1.5f,0.8f,65,1});audio.play(Event::TurboEngage);
    audio.play(Event::Cannon);audio.play(Event::Lock);audio.play({Event::FootRight,1,0.3f});audio.clearStatistics();
    expect(waitFrames(audio,12000),"SDL simultaneously plays weapon, interface and turbo layers");stats=audio.statistics();
    expect(stats.finite&&stats.audibleFrames>stats.frames*0.98&&stats.peak<0.98f,"SDL effects and machinery are audible without clipping");
    audio.setMuted(true);waitFrames(audio,2400);audio.clearStatistics();waitFrames(audio,2400);
    expect(audio.statistics().peak==0,"SDL master mute reaches digital silence");
    audio.shutdown();SDL_Quit();
    std::cout<<"PASS: "<<checks<<" DSP and SDL callback checks in "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count()<<" s\n";
}
