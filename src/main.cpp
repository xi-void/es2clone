#include "audio.hpp"
#include "renderer.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs=std::filesystem;
using namespace siege;

struct Options {
    bool smoke=false,mute=false;
    int frames=0;
    std::string screenshot,scene="menu",saveDir,audioDir,audioConfig,audioCapture;
};
static void help() {
    std::cout<<"Earthsiege II: Resistance - native Linux mech combat\n\n"
             <<"Usage: earthsiege [--mute] [--save-dir PATH] [--audio-dir PATH] [--audio-config FILE] [--audio-capture FILE.wav]\n"
             <<"       earthsiege --smoke-test [--scene menu|hangar|startup|combat|pause|controls|audio|debrief|flow]\n"
             <<"                  [--frames N] [--screenshot image.ppm] [--save-dir PATH]\n\n"
             <<"W/S move; A/D turn; Q/E strafe; mouse aims torso; C centers torso\n"
             <<"Left mouse/Space fire; right mouse missile; 1/2/3 weapon; T/Tab target\n"
             <<"Shift turbo; Escape pause; F1 controls; F2 audio mixer; M mute; F11 fullscreen\n";
}
static std::string dataDirectory() {
    const char* xdg=std::getenv("XDG_DATA_HOME");
    if(xdg&&*xdg) return (fs::path(xdg)/"earthsiege-resistance").string();
    const char* home=std::getenv("HOME");
    if(home&&*home)return (fs::path(home)/".local/share/earthsiege-resistance").string();
    return "save";
}
static void load(UIState& ui,const std::string& directory) {
    std::ifstream file(fs::path(directory)/"pilot.sav");
    std::string version;int bank=0,shield=0,weapon=0,completed=0,chassis=0,mission=0,mute=0;
    if(file>>version>>bank>>shield>>weapon>>completed>>chassis>>mission>>mute&&(version=="ESR1"||version=="ESR2")) {
        ui.bank=std::clamp(bank,0,1000000);ui.shieldUpgrade=std::clamp(shield,0,3);ui.weaponUpgrade=std::clamp(weapon,0,3);
        ui.completed=std::clamp(completed,0,3);ui.chassis=std::clamp(chassis,0,2);ui.mission=std::clamp(mission,0,2);ui.muted=mute!=0;
        if(version=="ESR2")for(float& volume:ui.volumes){float value;if(file>>value&&std::isfinite(value))volume=clamp(value,0,1);}
    }
}
static bool save(const UIState& ui,const std::string& directory) {
    std::error_code error;fs::create_directories(directory,error);if(error)return false;
    fs::path target=fs::path(directory)/"pilot.sav",temporary=fs::path(directory)/"pilot.sav.tmp";
    std::ofstream file(temporary);file<<"ESR2\n"<<ui.bank<<' '<<ui.shieldUpgrade<<' '<<ui.weaponUpgrade<<' '<<ui.completed<<' '<<ui.chassis<<' '<<ui.mission<<' '<<int(ui.muted)<<'\n';
    for(float volume:ui.volumes)file<<volume<<' ';
    file<<'\n';file.close();if(!file)return false;
    fs::rename(temporary,target,error);return !error;
}
int main(int argc,char** argv) {
    Options options;
    for(int i=1;i<argc;++i){std::string arg=argv[i];auto value=[&](){if(i+1>=argc)throw std::runtime_error("Missing value for "+arg);return std::string(argv[++i]);};
        try {
            if(arg=="--help"||arg=="-h"){help();return 0;}
            else if(arg=="--smoke-test")options.smoke=true;
            else if(arg=="--mute")options.mute=true;
            else if(arg=="--frames")options.frames=std::max(1,std::stoi(value()));
            else if(arg=="--screenshot")options.screenshot=value();
            else if(arg=="--scene")options.scene=value();
            else if(arg=="--audio-capture")options.audioCapture=value();
            else if(arg=="--audio-dir")options.audioDir=value();
            else if(arg=="--audio-config")options.audioConfig=value();
            else if(arg=="--save-dir")options.saveDir=value();
            else throw std::runtime_error("Unknown argument: "+arg);
        } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
    }
    if(options.scene!="menu"&&options.scene!="hangar"&&options.scene!="startup"&&options.scene!="combat"&&options.scene!="pause"&&options.scene!="controls"&&options.scene!="audio"&&options.scene!="debrief"&&options.scene!="flow"){std::cerr<<"Unknown scene\n";return 2;}
    if(!options.smoke&&options.scene!="menu"){std::cerr<<"--scene requires --smoke-test\n";return 2;}
    if(options.smoke&&options.frames==0)options.frames=120;
    if(options.saveDir.empty())options.saveDir=options.smoke?(fs::temp_directory_path()/"earthsiege-smoke").string():dataDirectory();
    // Audio is optional; video initialization must not depend on an audio device.
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_TIMER)!=0){std::cerr<<"SDL video initialization failed: "<<SDL_GetError()<<'\n';return 1;}
    if(TTF_Init()!=0){std::cerr<<"Font initialization failed: "<<TTF_GetError()<<'\n';SDL_Quit();return 1;}
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    int width=1440,height=900;
    if(!options.smoke){SDL_Rect usable{};if(SDL_GetDisplayUsableBounds(0,&usable)==0){width=std::min(width,usable.w-80);height=std::min(height,usable.h-100);}width=std::max(width,960);height=std::max(height,600);}
    Uint32 flags=SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(options.smoke?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN);
    SDL_Window* window=SDL_CreateWindow("Earthsiege II: Resistance",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,flags);
    if(!window){std::cerr<<"Could not create the native window: "<<SDL_GetError()<<'\n';TTF_Quit();SDL_Quit();return 1;}
    SDL_SetWindowMinimumSize(window,960,600);
    SDL_GLContext context=SDL_GL_CreateContext(window);
    if(!context){std::cerr<<"OpenGL 2.1 context failed: "<<SDL_GetError()<<'\n';SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();return 1;}
    SDL_GL_SetSwapInterval(options.smoke?0:1);
    std::cout<<"Renderer: "<<glGetString(GL_RENDERER)<<" / OpenGL "<<glGetString(GL_VERSION)<<'\n';
    char* base=SDL_GetBasePath();fs::path executable=base?fs::path(base):fs::current_path();SDL_free(base);
    fs::path assets=executable/"assets";
    if(!fs::exists(assets/"fonts/Display.ttf"))assets=executable/"../share/earthsiege/assets";
    if(!fs::exists(assets/"fonts/Display.ttf"))assets=fs::current_path()/"assets";
    Renderer renderer;
    if(!renderer.init(assets.string())){renderer.destroy();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();return 1;}
    fs::path audioRoot=options.audioDir.empty()?assets/"audio":fs::path(options.audioDir);
    if(options.audioDir.empty()&&fs::exists(executable/"../assets/audio/audio.ini"))audioRoot=executable/"../assets/audio";
    Audio audio;
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)!=0||!audio.init(audioRoot.string(),options.audioConfig))std::cerr<<"Audio unavailable; continuing without sound: "<<SDL_GetError()<<'\n';
    UIState ui;if(!options.smoke)load(ui,options.saveDir);if(options.mute||(options.smoke&&options.audioCapture.empty()))ui.muted=true;ui.volumes=audio.configuredVolumes(ui.volumes);audio.setMuted(ui.muted);audio.setVolumes(ui.volumes);audio.setSuspended(true);
    if(!options.audioCapture.empty())audio.startCapture(120);
    Combat game;game.reset();bool running=true,banked=false,mouseDown=false,rightDown=false;
    auto notify=[&](const std::string& message){ui.notice=message;ui.noticeTime=4;};
    auto persist=[&](){if(!save(ui,options.saveDir))notify("Could not save pilot data. Check the save directory.");};
    auto capture=[&](bool enabled){mouseDown=rightDown=false;if(!options.smoke){if(SDL_SetRelativeMouseMode(enabled?SDL_TRUE:SDL_FALSE)!=0&&enabled)notify("Mouse capture unavailable. Use arrow keys to aim.");SDL_ShowCursor(enabled?SDL_DISABLE:SDL_ENABLE);}};
    auto launch=[&](){audio.reset();game.reset(ui.chassis,ui.mission,ui.shieldUpgrade,ui.weaponUpgrade);banked=false;ui.screen=Screen::Playing;capture(true);persist();};
    auto act=[&](Action action){
        if(action!=Action::None)audio.play(Event::Lock);
        switch(action){
        case Action::Launch:case Action::Retry:launch();break;
        case Action::Next:ui.mission=(ui.mission+1)%3;launch();break;
        case Action::Hangar:ui.screen=Screen::Hangar;capture(false);audio.reset();audio.play(Event::ReactorShutdown);break;
        case Action::Back:if(ui.screen==Screen::Controls||ui.screen==Screen::AudioSettings)ui.screen=ui.returnScreen;else ui.screen=Screen::Menu;break;
        case Action::Controls:ui.returnScreen=ui.screen;ui.screen=Screen::Controls;capture(false);break;
        case Action::AudioSettings:ui.returnScreen=ui.screen;ui.screen=Screen::AudioSettings;capture(false);break;
        case Action::Quit:running=false;break;
        case Action::Sound:ui.muted=!ui.muted;audio.setMuted(ui.muted);persist();break;
        case Action::Fullscreen:ui.fullscreen=!ui.fullscreen;if(SDL_SetWindowFullscreen(window,ui.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0)!=0){ui.fullscreen=!ui.fullscreen;notify("Fullscreen unavailable on this display.");}break;
        case Action::PrevChassis:ui.chassis=(ui.chassis+2)%3;persist();break;
        case Action::NextChassis:ui.chassis=(ui.chassis+1)%3;persist();break;
        case Action::PrevMission:ui.mission=(ui.mission+2)%3;persist();break;
        case Action::NextMission:ui.mission=(ui.mission+1)%3;persist();break;
        case Action::Resume:ui.screen=Screen::Playing;capture(true);break;
        case Action::ShieldUpgrade:{int cost=80+ui.shieldUpgrade*40;if(ui.shieldUpgrade<3&&ui.bank>=cost){ui.bank-=cost;++ui.shieldUpgrade;persist();notify("Shield capacitor upgraded across all HERCs.");}break;}
        case Action::WeaponUpgrade:{int cost=100+ui.weaponUpgrade*50;if(ui.weaponUpgrade<3&&ui.bank>=cost){ui.bank-=cost;++ui.weaponUpgrade;persist();notify("Weapon amplifier upgraded across all HERCs.");}break;}
        case Action::MusicDown:case Action::MusicUp:case Action::WeaponsDown:case Action::WeaponsUp:
        case Action::MachineryDown:case Action::MachineryUp:case Action::InterfaceDown:case Action::InterfaceUp:{
            int control=int(action)-int(Action::MusicDown),bus=control/2;
            ui.volumes[size_t(bus)]=clamp(ui.volumes[size_t(bus)]+(control%2?0.1f:-0.1f),0,1);
            audio.setVolumes(ui.volumes);persist();break;
        }
        case Action::None:break;
        }
    };
    if(options.smoke){
        if(options.scene=="flow"){ui.bank=300;options.frames=std::max(options.frames,32);}
        if(options.scene=="combat"||options.scene=="pause"||options.scene=="debrief"){game.reset();while(!game.ready())game.update(1.0f/120,{});game.events.clear();ui.screen=Screen::Playing;}
        if(options.scene=="hangar")ui.screen=Screen::Hangar;
        if(options.scene=="startup")ui.screen=Screen::Playing;
        if(options.scene=="audio")ui.screen=Screen::AudioSettings;
        if(options.scene=="pause")ui.screen=Screen::Paused;
        if(options.scene=="controls")ui.screen=Screen::Controls;
        if(options.scene=="debrief"){ui.screen=Screen::Debrief;game.phase=Phase::Won;game.kills=4;game.salvage=195;game.time=126;ui.bank=195;}
    }
    uint64_t previous=SDL_GetPerformanceCounter();double frequency=double(SDL_GetPerformanceFrequency());int frame=0,exitCode=0;float accumulated=0;
    float pausedTime=0;
    while(running){
        uint64_t now=SDL_GetPerformanceCounter();float dt=options.smoke&&options.audioCapture.empty()?1.0f/60:std::min(float(double(now-previous)/frequency),0.1f);previous=now;
        ui.time+=dt;ui.noticeTime=std::max(0.0f,ui.noticeTime-dt);
        if(options.smoke&&options.scene=="flow") {
            auto check=[&](bool result,const char* name){if(!result){std::cerr<<"UI flow FAIL: "<<name<<'\n';exitCode=1;running=false;}};
            auto click=[&](int x,int y){SDL_Event e{};e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;e.button.x=x*width/1440;e.button.y=y*height/900;SDL_PushEvent(&e);};
            auto key=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=code;SDL_PushEvent(&e);};
            switch(frame) {
            case 1:click(490,700);break;
            case 2:check(ui.screen==Screen::Hangar,"menu to hangar");click(520,310);break;
            case 3:check(ui.chassis==1,"select chassis");click(460,645);break;
            case 4:check(ui.shieldUpgrade==1&&ui.bank==220,"buy shield upgrade");click(460,710);break;
            case 5:check(ui.weaponUpgrade==1&&ui.bank==120,"buy weapon upgrade");click(460,810);break;
            case 6:check(ui.screen==Screen::Menu,"hangar back");click(540,535);break;
            case 7:check(ui.mission==1,"select mission");key(SDLK_RETURN);break;
            case 8:check(ui.screen==Screen::Playing&&game.chassisIndex==1&&game.mission==1&&game.player.maxShield==150,"deploy chosen loadout");key(SDLK_2);break;
            case 9:check(game.player.weapon==1,"weapon hotkey");key(SDLK_ESCAPE);pausedTime=game.startupTime;break;
            case 10:check(ui.screen==Screen::Paused&&game.startupTime==pausedTime,"pause freezes startup and simulation");key(SDLK_F1);break;
            case 11:check(ui.screen==Screen::Controls,"field manual from pause");key(SDLK_ESCAPE);break;
            case 12:check(ui.screen==Screen::Paused,"manual returns to pause");key(SDLK_RETURN);break;
            case 13:check(ui.screen==Screen::Playing&&game.startupTime>pausedTime,"resume startup");key(SDLK_ESCAPE);break;
            case 14:click(700,550);break;
            case 15:check(ui.screen==Screen::Playing&&game.kills==0&&game.player.missiles==14,"restart mission");key(SDLK_ESCAPE);break;
            case 16:click(700,620);break;
            case 17:check(ui.screen==Screen::Hangar,"return to hangar");{UIState restored;load(restored,options.saveDir);check(restored.bank==120&&restored.chassis==1&&restored.mission==1&&restored.shieldUpgrade==1&&restored.weaponUpgrade==1,"pilot save round trip");}key(SDLK_F2);break;
            case 18:check(ui.screen==Screen::AudioSettings,"audio settings hotkey");click(835,314);break;
            case 19:check(ui.volumes[0]<defaultVolumes[0],"independent music volume");{UIState restored;load(restored,options.saveDir);check(std::abs(restored.volumes[0]-ui.volumes[0])<0.001f&&restored.volumes[2]==1,"volume persistence");}key(SDLK_ESCAPE);break;
            default:break;
            }
        }
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)running=false;
            if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_FOCUS_LOST&&ui.screen==Screen::Playing&&!options.smoke){ui.screen=Screen::Paused;capture(false);}
            if(event.type==SDL_MOUSEMOTION){
                int ww,wh;SDL_GetWindowSize(window,&ww,&wh);ui.mouseX=float(event.motion.x)/std::max(1,ww)*1440;ui.mouseY=float(event.motion.y)/std::max(1,wh)*900;
                if(ui.screen==Screen::Playing){game.player.torso=clamp(game.player.torso+event.motion.xrel*0.0024f,-1.4f,1.4f);game.player.pitch=clamp(game.player.pitch-event.motion.yrel*0.0020f,-0.65f,0.5f);}
            }
            if(event.type==SDL_MOUSEBUTTONDOWN){if(ui.screen==Screen::Playing){if(event.button.button==SDL_BUTTON_LEFT)mouseDown=true;if(event.button.button==SDL_BUTTON_RIGHT)rightDown=true;}}
            if(event.type==SDL_MOUSEBUTTONUP){
                if(event.button.button==SDL_BUTTON_LEFT)mouseDown=false;
                if(event.button.button==SDL_BUTTON_RIGHT)rightDown=false;
                if(ui.screen!=Screen::Playing&&event.button.button==SDL_BUTTON_LEFT){int ww,wh;SDL_GetWindowSize(window,&ww,&wh);act(renderer.hit(float(event.button.x)/std::max(1,ww)*1440,float(event.button.y)/std::max(1,wh)*900));}
            }
            if(event.type==SDL_KEYDOWN&&!event.key.repeat){auto key=event.key.keysym.sym;
                if(key==SDLK_F11)act(Action::Fullscreen);
                else if(key==SDLK_m)act(Action::Sound);
                else if(key==SDLK_F2){if(ui.screen==Screen::AudioSettings)act(Action::Back);else{if(ui.screen==Screen::Playing){ui.screen=Screen::Paused;capture(false);}act(Action::AudioSettings);}}
                else if(key==SDLK_F1){if(ui.screen==Screen::Controls)act(Action::Back);else {if(ui.screen==Screen::Playing){ui.screen=Screen::Paused;capture(false);}act(Action::Controls);}}
                else if(key==SDLK_ESCAPE){if(ui.screen==Screen::Playing){ui.screen=Screen::Paused;capture(false);}else if(ui.screen==Screen::Paused)act(Action::Resume);else if(ui.screen==Screen::Controls||ui.screen==Screen::Hangar||ui.screen==Screen::AudioSettings)act(Action::Back);else if(ui.screen==Screen::Debrief)ui.screen=Screen::Menu;else running=false;}
                else if(ui.screen==Screen::Playing){if(key>=SDLK_1&&key<=SDLK_3)game.player.weapon=int(key-SDLK_1);if(key==SDLK_t||key==SDLK_TAB)game.selectTarget();if(key==SDLK_c){game.player.torso=0;game.player.pitch=0;}}
                else if(ui.screen==Screen::Menu||ui.screen==Screen::Hangar){if(key==SDLK_RETURN)act(Action::Launch);if(key==SDLK_h)act(Action::Hangar);if(key==SDLK_LEFT)act(ui.screen==Screen::Hangar?Action::PrevChassis:Action::PrevMission);if(key==SDLK_RIGHT)act(ui.screen==Screen::Hangar?Action::NextChassis:Action::NextMission);}
                else if(ui.screen==Screen::Paused&&key==SDLK_RETURN)act(Action::Resume);
                else if(ui.screen==Screen::Debrief&&key==SDLK_RETURN)act(game.phase==Phase::Won?Action::Next:Action::Retry);
            }
        }
        bool simulated=ui.screen==Screen::Playing;
        if(simulated){
            const Uint8* keys=SDL_GetKeyboardState(nullptr);Controls input;
            input.throttle=float(keys[SDL_SCANCODE_W])-keys[SDL_SCANCODE_S];input.turn=float(keys[SDL_SCANCODE_D])-keys[SDL_SCANCODE_A];input.strafe=float(keys[SDL_SCANCODE_E])-keys[SDL_SCANCODE_Q];input.boost=keys[SDL_SCANCODE_LSHIFT]||keys[SDL_SCANCODE_RSHIFT];input.fire=mouseDown||keys[SDL_SCANCODE_SPACE];input.missile=rightDown;
            game.player.torso=clamp(game.player.torso+(float(keys[SDL_SCANCODE_RIGHT])-keys[SDL_SCANCODE_LEFT])*dt,-1.4f,1.4f);
            game.player.pitch=clamp(game.player.pitch+(float(keys[SDL_SCANCODE_UP])-keys[SDL_SCANCODE_DOWN])*dt*0.65f,-0.65f,0.5f);
            if(options.smoke){input.throttle=frame<100?0.5f:0;input.fire=frame%70<20;
                if(!options.audioCapture.empty()){
                    float t=game.startupTime+game.time;input.throttle=t>=7&&t<19?1:0;input.boost=t>=11&&t<16;input.turn=t>=10?0.4f:0;
                }
            }
            accumulated+=dt;
            while(accumulated>=1.0f/120){game.update(1.0f/120,input);accumulated-=1.0f/120;}
            if(game.phase!=Phase::Active){ui.screen=Screen::Debrief;capture(false);if(game.phase==Phase::Won&&!banked){ui.bank+=game.salvage;ui.completed=std::max(ui.completed,ui.mission+1);banked=true;persist();}}
        } else {accumulated=0;}
        audio.setSuspended(ui.screen!=Screen::Playing);
        bool missionScreen=ui.screen==Screen::Playing||ui.screen==Screen::Paused||
            ((ui.screen==Screen::Controls||ui.screen==Screen::AudioSettings)&&ui.returnScreen==Screen::Paused);
        audio.setMusicActive(missionScreen?game.ready():audio.assetInfo().config.menuMusic);
        audio.setDrive({clamp(game.startupTime/2.1f,0,1),clamp(game.player.gait.speed/game.player.maxSpeed,0,1.6f),std::abs(game.player.gait.load),game.player.mass,game.player.turboPower});
        if(simulated)for(const auto& event:game.events)audio.play(event);
        game.events.clear();
        SDL_GL_GetDrawableSize(window,&width,&height);renderer.render(width,height,game,ui);
        ++frame;
        if(options.smoke&&!options.audioCapture.empty()&&ui.time>=21)running=false;
        if(options.frames>0&&frame>=options.frames){
            if(!options.screenshot.empty()&&!renderer.screenshot(options.screenshot,width,height)){std::cerr<<"Screenshot write failed\n";exitCode=1;}
            GLenum error=glGetError();if(error!=GL_NO_ERROR){std::cerr<<"OpenGL error: "<<error<<'\n';exitCode=1;}
            running=false;
        }
        SDL_GL_SwapWindow(window);
        if(options.smoke&&!options.audioCapture.empty()){
            double spent=double(SDL_GetPerformanceCounter()-now)/frequency;
            if(spent<1.0/60)SDL_Delay(Uint32((1.0/60-spent)*1000));
        }
        if(!options.smoke&&SDL_GL_GetSwapInterval()==0)SDL_Delay(8);
    }
    if(!options.smoke)persist();
    if(!options.audioCapture.empty()){
        auto stats=audio.statistics();bool saved=audio.saveCapture(options.audioCapture);
        std::cout<<"In-game audio: "<<stats.assetEvents<<" file events, "<<stats.fallbackEvents<<" fallback events, "<<stats.musicFrames<<" music frames, "<<stats.startupCues<<" boot cues, peak "<<stats.peak<<", finite "<<stats.finite<<", captured "<<saved<<'\n';
        if(!saved||!stats.finite||!stats.audibleFrames)exitCode=1;
    }
    capture(false);audio.shutdown();renderer.destroy();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();
    if(options.smoke)std::cout<<"Native render smoke test: "<<(exitCode==0?"PASS":"FAIL")<<" ("<<frame<<" frames, "<<options.scene<<")\n";
    return exitCode;
}
