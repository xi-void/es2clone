#include "audio.hpp"
#include <cstdlib>
#include <iostream>
using namespace siege;
// Runs the same 120 Hz simulation and mixer used by the game, offline or through
// a real SDL callback. Optional locally imported recordings use the same mappings.
int main(int argc,char** argv){
    std::string output="mech-audio-demo.wav",audioDir;bool play=false,machineryOnly=false;int type=0;
    for(int i=1;i<argc;++i){std::string arg=argv[i];
        if(arg=="--play")play=true;
        else if(arg=="--machinery-only")machineryOnly=true;
        else if(arg=="--audio-dir"&&i+1<argc)audioDir=argv[++i];
        else if(arg=="--output"&&i+1<argc)output=argv[++i];
        else if(arg=="--chassis"&&i+1<argc)type=std::clamp(std::atoi(argv[++i]),0,2);
        else {std::cerr<<"Usage: audio_preview [--audio-dir assets/audio] [--output FILE.wav] [--play] [--machinery-only] [--chassis 0|1|2]\n";return 2;}
    }
    constexpr int seconds=28,framesPerTick=audioRate/120;
    Combat game;game.reset(type);for(auto& enemy:game.enemies)enemy.alive=false;game.spawned=missions[0].enemies;
    AudioVolumes volumes=defaultVolumes;if(machineryOnly)volumes={0,0,1,0};
    AudioMixer mixer;mixer.setVolumes(volumes);Audio audio;AudioAssets assets;MusicStream music;
    if(!audioDir.empty()&&!play){assets.load(audioDir);music.start(assets.playlist,assets.config.random,assets.config.crossfade);mixer.configure(&assets,&music);}
    if(play){
        if(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)!=0||!audio.init(audioDir)){std::cerr<<"SDL audio failed: "<<SDL_GetError()<<'\n';return 1;}
        audio.setVolumes(volumes);audio.startCapture(seconds+1);
        std::cout<<"Playing via SDL "<<SDL_GetCurrentAudioDriver()<<"; recording the actual callback to "<<output<<'\n';
    }
    std::vector<float> recording(size_t(seconds)*audioRate*2);unsigned left=0,right=0;Uint64 began=play?SDL_GetPerformanceCounter():0;
    double frequency=play?double(SDL_GetPerformanceFrequency()):1;
    std::cout<<"0-6.2 s startup; 7-13 s walk; 13-19 s turbo; 19-23 s spool-down; 23-28 s stop\n";
    for(int frame=0;frame<seconds*120;++frame){
        float time=float(frame)/120;Controls input;input.throttle=time>=7&&time<23?1:0;input.boost=time>=13&&time<19;
        // Curve through the open basin to avoid running into the arena boundary.
        input.turn=time>=11&&time<23?0.4f:0;
        game.update(1.0f/120,input);
        DriveSound drive{clamp(game.startupTime/2.1f,0,1),clamp(game.player.gait.speed/game.player.maxSpeed,0,1.6f),std::abs(game.player.gait.load),game.player.mass,game.player.turboPower};
        if(play){audio.setDrive(drive);audio.setMusicActive(game.ready());}
        else {mixer.setDrive(drive);mixer.setMusicActive(game.ready());
            if(game.ready()&&!assets.playlist.empty())for(int wait=0;wait<100&&music.buffered()<size_t(framesPerTick);++wait)SDL_Delay(1);
        }
        for(const auto& event:game.events){if(play)audio.play(event);else mixer.trigger(event);if(event.type==Event::FootLeft)++left;if(event.type==Event::FootRight)++right;}game.events.clear();
        if(play){double target=double(frame+1)/120;while(double(SDL_GetPerformanceCounter()-began)/frequency<target)SDL_Delay(1);}
        else mixer.render(recording.data()+size_t(frame)*framesPerTick*2,framesPerTick);
    }
    bool saved=play?audio.saveCapture(output):writeWave(output,recording);MixerStats stats=play?audio.statistics():mixer.statistics();
    if(play){audio.shutdown();SDL_Quit();}
    std::cout<<"Contacts L/R: "<<left<<'/'<<right<<"; original/file events: "<<stats.assetEvents<<"; fallback events: "<<stats.fallbackEvents<<"; music frames: "<<stats.musicFrames<<"; boot cues: "<<stats.startupCues<<"; PCM peak: "<<stats.peak<<"; "<<(saved&&stats.finite?"saved ":"FAILED ")<<output<<'\n';
    return saved&&stats.finite&&stats.audibleFrames>0?0:1;
}
