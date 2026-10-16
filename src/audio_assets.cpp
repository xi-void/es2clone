#include "audio_assets.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>
#include <set>

namespace siege {
namespace fs=std::filesystem;
namespace {
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
std::string lower(std::string s){for(char& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
bool supported(const fs::path& p){auto s=lower(p.extension().string());return s==".wav"||s==".mp3"||s==".flac"||s==".ogg";}
bool safe(const fs::path& p){if(p.empty()||p.is_absolute())return false;for(const auto& part:p)if(part=="..")return false;return true;}
const char* names[]={"laser","cannon","missile","impact","explosion","lock","wave","win","lose","overheat",
 "foot_left","foot_right","hydraulic_left","hydraulic_right","reactor_ignition","system_init","mechanical_activate","reactor_ready","turbo_engage","turbo_release","reactor_shutdown","damage_warning"};
static_assert(sizeof(names)/sizeof(*names)==size_t(Event::Count));
}
AudioDecoder::~AudioDecoder(){if(stream)SDL_FreeAudioStream(stream);if(file)sf_close(file);}
bool AudioDecoder::open(const fs::path& path){
    if(stream)SDL_FreeAudioStream(stream);
    if(file)sf_close(file);
    stream=nullptr;file=nullptr;info={};ended=false;total=0;
    file=sf_open(path.c_str(),SFM_READ,&info);
    if(!file||info.channels<1||info.channels>2||info.samplerate<8000||info.samplerate>192000||info.frames<=0)return false;
    stream=SDL_NewAudioStream(AUDIO_F32SYS,Uint8(info.channels),info.samplerate,AUDIO_F32SYS,2,audioRate);
    total=int64_t(std::llround(double(info.frames)*audioRate/info.samplerate));
    return stream!=nullptr;
}
int AudioDecoder::read(float* stereo,int frames){
    if(!stream)return 0;
    int done=0;
    while(done<frames){
        int got=SDL_AudioStreamGet(stream,stereo+done*2,(frames-done)*int(2*sizeof(float)));
        if(got<0)break;
        done+=got/int(2*sizeof(float));
        if(done==frames||ended)break;
        std::array<float,8192> input{};
        sf_count_t count=sf_readf_float(file,input.data(),4096);
        if(count<=0){SDL_AudioStreamFlush(stream);ended=true;}
        else if(SDL_AudioStreamPut(stream,input.data(),int(count*info.channels*sizeof(float)))<0)break;
    }
    for(int i=0;i<done*2;++i)if(!std::isfinite(stereo[i]))stereo[i]=0;
    return done;
}
const char* AudioAssets::eventName(Event e){return int(e)>=0&&e<Event::Count?names[int(e)]:"invalid";}
void AudioAssets::readConfig(const fs::path& path){
    std::ifstream in(path);if(!in)return;
    std::string line,section;int number=0;
    while(std::getline(in,line)){
        ++number;line=trim(line);if(line.empty()||line[0]=='#'||line[0]==';')continue;
        if(line.front()=='['&&line.back()==']'){section=lower(trim(line.substr(1,line.size()-2)));continue;}
        auto at=line.find('=');if(at==std::string::npos)continue;
        auto key=lower(trim(line.substr(0,at))),value=trim(line.substr(at+1));
        try {
            auto numeric=[&](float low,float high){size_t used=0;float v=std::stof(value,&used);if(used!=value.size()||!std::isfinite(v)||v<low||v>high)throw std::runtime_error("range");return v;};
            if(section=="volumes"){
                const char* buses[]={"music","effects","machinery","warning"};
                for(int i=0;i<4;++i)if(key==buses[i]&&!value.empty())config.overrides[i]=numeric(0,1);
            } else if(section=="music"){
                if(key=="mode"){if(value!="random"&&value!="sequential")throw std::runtime_error("mode");config.random=value=="random";}
                else if(key=="menus"){if(value!="true"&&value!="false")throw std::runtime_error("bool");config.menuMusic=value=="true";}
                else if(key=="crossfade_seconds")config.crossfade=numeric(0,5);
                else if(key=="warning_duck")config.warningDuck=numeric(0,1);
                else if(key=="startup_track")config.startupTrack=value;
            } else if(section=="events"||section=="gains"){
                for(int i=0;i<int(Event::Count);++i)if(key==names[i]){if(section=="events")config.events[i]=value;else config.gains[i]=numeric(0,2);}
            } else if(section=="loops"){
                const char* loops[]={"reactor","servo","turbo"};for(int i=0;i<3;++i)if(key==loops[i])config.loops[i]=value;
            }
        } catch(const std::exception&){std::cerr<<"Audio: invalid setting at "<<path<<':'<<number<<'\n';}
    }
}
std::shared_ptr<AudioClip> AudioAssets::loadClip(const std::string& relative){
    if(!safe(relative))return {};
    auto found=cache.find(relative);if(found!=cache.end())return found->second;
    std::shared_ptr<AudioClip> clip;AudioDecoder decoder;
    if(decoder.open(root/relative)){
        clip=std::make_shared<AudioClip>();std::array<float,8192> buffer{};int n;
        while((n=decoder.read(buffer.data(),4096))>0){
            if(clip->samples.size()+size_t(n)*2>size_t(audioRate)*30*2||bytes+(clip->samples.size()+size_t(n)*2)*sizeof(float)>64*1024*1024){clip.reset();break;}
            clip->samples.insert(clip->samples.end(),buffer.begin(),buffer.begin()+n*2);
        }
        if(clip&&clip->samples.empty())clip.reset();
        if(clip)bytes+=clip->samples.size()*sizeof(float);
    }
    std::error_code error;
    if(!clip&&fs::exists(root/relative,error))std::cerr<<"Audio: cannot decode bounded effect "<<relative<<"; using fallback\n";
    cache[relative]=clip;return clip;
}
void AudioAssets::load(const fs::path& directory,const fs::path& configFile){
    root=directory;config=AudioConfig{};events={};loops={};startup.reset();playlist.clear();cache.clear();bytes=0;
    readConfig(root/"audio.ini");readConfig(root/"local.ini");if(!configFile.empty())readConfig(configFile);
    for(int i=0;i<int(Event::Count);++i){
        std::istringstream variants(config.events[i]);std::string name;
        while(std::getline(variants,name,'|'))if(auto clip=loadClip(trim(name)))events[i].push_back(clip);
        if(events[i].empty())if(auto clip=loadClip(std::string("fallback/")+names[i]+".wav"))events[i].push_back(clip);
    }
    for(int i=0;i<3;++i)loops[i]=loadClip(config.loops[i]);
    std::error_code ec;auto music=root/"music";
    if(fs::is_directory(music,ec)){
        fs::recursive_directory_iterator it(music,fs::directory_options::skip_permission_denied,ec),end;
        while(it!=end&&!ec){
            auto name=it->path().filename().string();bool hidden=!name.empty()&&(name[0]=='.'||name=="__MACOSX");
            if(hidden&&it->is_directory(ec))it.disable_recursion_pending();
            if(!hidden&&it->is_regular_file(ec)&&supported(it->path()))playlist.push_back(it->path());
            it.increment(ec);
        }
    }
    std::sort(playlist.begin(),playlist.end());
    fs::path startupPath;
    if(config.startupTrack=="auto"){
        for(const auto& p:playlist){auto name=lower(p.filename().string());
            if(name.find("001_")==0||name.find("01 ")==0||name.find("01_")==0||name.find("01-")==0){startupPath=p;break;}}
    } else if(safe(config.startupTrack))startupPath=root/config.startupTrack;
    if(!startupPath.empty()){
        startup=loadClip(startupPath.lexically_relative(root).generic_string());
        playlist.erase(std::remove(playlist.begin(),playlist.end(),startupPath),playlist.end());
    }
    size_t mapped=0;for(const auto& entry:events)if(!entry.empty())++mapped;
    std::cout<<"Audio assets: "<<mapped<<" mapped events, "<<playlist.size()<<" music tracks, startup "<<(startup?"loaded":"fallback")<<", "<<bytes/1048576.0<<" MiB PCM\n";
}
}
