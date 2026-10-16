#include "audio.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
using namespace siege;
namespace fs=std::filesystem;
namespace {
int checks=0;
void expect(bool ok,const char* message){++checks;if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
void put(const fs::path& p,const std::string& s){std::ofstream f(p);f<<s;}
bool waitBuffered(MusicStream& music,size_t frames){for(int i=0;i<2000;++i){if(music.buffered()>=frames)return true;SDL_Delay(1);}return false;}
float rms(const std::vector<float>& samples){double sum=0;for(float v:samples)sum+=v*v;return float(std::sqrt(sum/samples.size()));}
std::vector<float> render(AudioMixer& m,int frames){std::vector<float> out(size_t(frames)*2);m.render(out.data(),frames);return out;}
}
int main(int argc,char** argv){
    expect(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0,"SDL initialization");
    auto root=fs::temp_directory_path()/("es2-audio-test-"+std::to_string(SDL_GetPerformanceCounter()));
    fs::create_directories(root/"music/__MACOSX");fs::create_directories(root/"sfx");fs::create_directories(root/"fallback");
    // Deliberately simple finite signals are temporary decoder/mixer fixtures,
    // never installed as game music.
    std::vector<float> tone(audioRate*2);for(size_t i=0;i<tone.size()/2;++i){tone[i*2]=0.25f;tone[i*2+1]=0.125f;}
    expect(writeWave((root/"sfx/step.wav").string(),tone),"write fixture");
    writeWave((root/"music/001_boot.wav").string(),tone);
    writeWave((root/"music/002_a.wav").string(),tone);
    writeWave((root/"music/003_b.wav").string(),tone);
    put(root/"music/000_broken.mp3","not an mp3");put(root/"music/__MACOSX/._junk.mp3","metadata");
    put(root/"audio.ini","[volumes]\nmusic=0.6\neffects=nan\n[music]\nmode=sequential\ncrossfade_seconds=0.15\n[events]\nfoot_left=sfx/step.wav\nfoot_right=sfx/missing.wav\nlaser=../outside.wav\n[loops]\nreactor=sfx/step.wav\n");
    put(root/"local.ini","[volumes]\nmachinery=0.45\n[events]\nfoot_right=sfx/step.wav\n");
    AudioAssets assets;assets.load(root);
    expect(assets.startup&&assets.playlist.size()==3,"numbered startup excluded; hidden metadata skipped");
    expect(assets.events[int(Event::FootLeft)][0]==assets.events[int(Event::FootRight)][0],"duplicate effects share cached PCM");
    expect(assets.events[int(Event::Laser)].empty(),"path traversal is rejected and falls back");
    expect(assets.config.overrides[0]==0.6f&&assets.config.overrides[1]==-1&&assets.config.overrides[2]==0.45f,"plain config validation and local overrides");
    AudioDecoder decoder;expect(decoder.open(root/"sfx/step.wav"),"PCM decoder opens");
    std::vector<float> decoded(audioRate*2+1000);int count=decoder.read(decoded.data(),audioRate+500);
    expect(count==audioRate,"decode preserves full duration and exact EOF");
    expect(std::abs(decoded[1000]-0.25f)<0.0001f&&std::abs(decoded[1001]-0.125f)<0.0001f,"decode preserves stereo levels without normalization");
    // Encode temporary fixtures in each supported codec, then use the actual
    // runtime decoder and resampler (44.1 kHz mono -> 48 kHz stereo).
    const int formats[]={SF_FORMAT_WAV|SF_FORMAT_PCM_16,SF_FORMAT_FLAC|SF_FORMAT_PCM_16,
                         SF_FORMAT_OGG|SF_FORMAT_VORBIS,SF_FORMAT_MPEG|SF_FORMAT_MPEG_LAYER_III};
    const char* extensions[]={"wav","flac","ogg","mp3"};
    for(int f=0;f<4;++f){
        SF_INFO info{};info.samplerate=44100;info.channels=1;info.format=formats[f];
        auto path=root/(std::string("codec.")+extensions[f]);auto file=sf_open(path.c_str(),SFM_WRITE,&info);
        expect(file!=nullptr,"codec fixture encoder available");std::vector<float> signal(44100);
        for(size_t i=0;i<signal.size();++i)signal[i]=0.25f*std::sin(2*pi*220*float(i)/44100);
        expect(sf_writef_float(file,signal.data(),signal.size())==44100,"codec fixture encoded");sf_close(file);
        AudioDecoder codec;expect(codec.open(path),"runtime codec opens fixture");
        std::vector<float> output(audioRate*3);int frames=codec.read(output.data(),audioRate*3/2);
        expect(std::abs(frames-audioRate)<512,"codec preserves duration through resampling");
        output.resize(size_t(frames)*2);expect(rms(output)>0.15f&&rms(output)<0.20f,"codec retains audio level");
    }
    MusicStream music;music.start(assets.playlist,false,0.15f);
    expect(waitBuffered(music,80000),"worker skips corrupt file and fills bounded ring");
    float minimum=1,maximum=0;int frames=0;bool available=true;
    while(frames<audioRate*4){
        available=waitBuffered(music,512)&&available;
        for(int i=0;i<512;++i){float l,r;available=music.pop(l,r)&&available;minimum=std::min(minimum,l);maximum=std::max(maximum,l);++frames;}
    }
    expect(available,"ring refills and supplies every frame across transitions");
    expect(minimum>0.24f&&maximum<0.26f,"linear crossfade keeps correlated tracks continuous without silence or overshoot");
    AudioMixer mixer;mixer.configure(&assets,&music);mixer.setMusicActive(true);mixer.setVolumes({1,0,0,0});render(mixer,12000);
    expect(waitBuffered(music,12000),"stream ready for bus test");
    auto plain=render(mixer,12000);expect(rms(plain)>0.1f,"supplied soundtrack reaches music bus");
    mixer.trigger(Event::Overheat);expect(waitBuffered(music,12000),"warning test buffer");auto ducked=render(mixer,12000);
    expect(rms(ducked)<rms(plain)*0.5f,"important warning ducks music even with warning bus muted");
    mixer.setVolumes({0,1,1,1});mixer.setMusicActive(false);render(mixer,12000);mixer.trigger(Event::FootLeft);
    expect(rms(render(mixer,12000))>0.1f&&mixer.statistics().assetEvents>0,"imported footsteps coexist with independently muted music");
    mixer.reset();mixer.trigger(Event::ReactorIgnition);expect(mixer.statistics().startupCues==1,"level ignition selects supplied track one");
    mixer.trigger(Event::SystemInit);expect(mixer.statistics().fallbackEvents==0,"boot cue replaces generated startup sequence");
    music.stop();
    MusicStream shuffled;shuffled.start({root/"music/000_broken.mp3",root/"music/002_a.wav",root/"music/missing.ogg"},true,0.15f);
    bool continuous=true;
    for(int turn=0;turn<8;++turn){
        continuous=waitBuffered(shuffled,30000)&&continuous;
        for(int i=0;i<30000;++i){float l,r;continuous=shuffled.pop(l,r)&&l>0.24f&&continuous;}
    }
    expect(continuous,"random playback continues through reshuffles with only one decodable track");shuffled.stop();
    Audio audio;expect(audio.init(root.string()),"SDL opens with actual file assets");audio.setVolumes({0.3f,1,1,1});audio.setMusicActive(true);audio.setDrive({1,1.4f,0.8f,65,1});
    audio.play({Event::FootLeft,1,-0.3f});audio.play(Event::Cannon);audio.play(Event::TurboEngage);SDL_Delay(350);
    auto stats=audio.statistics();expect(stats.assetEvents==1&&stats.fallbackEvents==2&&stats.musicFrames>0,"actual SDL callback mixes file effects, fallbacks and streaming music");
    expect(stats.finite&&stats.peak<=0.97f&&stats.audibleFrames>10000,"SDL file playback is finite audible and bounded");audio.shutdown();
    if(argc>1){
        AudioAssets original;original.load(argv[1]);
        expect(original.startup&&original.playlist.size()==9,"supplied remaster boot and nine background tracks decode");
        expect(!original.events[int(Event::FootLeft)].empty()&&original.loops[0],"ISO footstep and engine loaded");
        Audio actual;expect(actual.init(argv[1]),"SDL opens original assets");actual.setMusicActive(true);actual.setDrive({1,1,0.5f,65,0.8f});
        for(Event event:{Event::FootLeft,Event::FootRight,Event::Laser,Event::Cannon,Event::Missile,Event::Explosion,Event::Lock})actual.play(event);
        SDL_Delay(400);auto originalStats=actual.statistics();
        expect(originalStats.assetEvents==7&&originalStats.musicFrames>0&&originalStats.audibleFrames>10000&&originalStats.finite,"seven original game effects and supplied MP3 reach SDL callback");actual.shutdown();
    }
    fs::remove_all(root);SDL_Quit();std::cout<<"PASS: "<<checks<<" asset/stream/SDL checks\n";
}
