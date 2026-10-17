#include "renderer.hpp"
#include "original_graphics.hpp"
#include "audio.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace siege;
namespace fs=std::filesystem;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::vector<unsigned char> pixels(int width,int height){
    std::vector<unsigned char> out(size_t(width)*height*3);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,out.data());return out;
}
int main(int argc,char** argv)try {
    if(argc!=4){std::cerr<<"Usage: battlefield_render ASSETS ORIGINAL_GRAPHICS OUTPUT_DIRECTORY\n";return 2;}
    require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)==0,SDL_GetError());require(TTF_Init()==0,TTF_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    auto* window=SDL_CreateWindow("Battlefield regression",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    require(window,SDL_GetError());auto context=SDL_GL_CreateContext(window);require(context,SDL_GetError());
    Renderer renderer;require(renderer.init(argv[1]),"renderer init");
    require(renderer.loadOriginalCockpit(argv[2]),"original artwork required for this optional local test");
    auto frame=original::decodeBitmaps(original::readFile(fs::path(argv[2])/"TOMAHAWK.HB0"))[0];
    fs::path out=argv[3];fs::create_directories(out);UIState ui;ui.screen=Screen::Playing;
    Combat game;
    auto ready=[&](){game.reset();while(!game.ready())game.update(1.0f/120,{});game.events.clear();game.statusMessages.clear();game.messageTime=0;};
    auto capture=[&](const std::string& name,int w,int h){renderer.render(w,h,game,ui);require(renderer.screenshot((out/(name+"-"+std::to_string(w)+"x"+std::to_string(h)+".ppm")).string(),w,h),"capture failed");require(glGetError()==GL_NO_ERROR,"OpenGL state error");};
    for(auto size:{std::pair{1920,1080},std::pair{1280,720}}){
        auto [w,h]=size;SDL_SetWindowSize(window,w,h);
        game.reset();game.startupTime=1.5f;game.statusMessages.front().elapsed=1.5f;capture("insertion",w,h);
        game.startupTime=4;game.statusMessages.front().elapsed=4;capture("touchdown",w,h);
        ready();game.selectTarget();capture("combat",w,h);
        for(bool legs:{false,true}){ready();game.enemies[0].shield=0;game.damageEnemy(0,1000,legs);game.statusMessages.front().elapsed=1;capture(legs?"disabled":"destroyed",w,h);}
        ready();game.player.yaw=pi/2;game.player.torso=0.4f;ui.radarMeters=400;capture("radar-east",w,h);ui.radarMeters=800;
        ready();for(int i=0;i<600;++i){game.update(1.0f/120,{1,0,0,false,false,true});game.events.clear();}capture("turbo-motion",w,h);
        ready();game.enemies.clear();capture("pillar-baseline",w,h);auto base=pixels(w,h);
        for(float side:{-1.0f,1.0f}){Enemy enemy;enemy.pos=game.player.pos+Vec3{side*40,0,-70};game.enemies.push_back(enemy);}
        capture("pillar-occlusion",w,h);auto target=pixels(w,h);
        // Compare the opaque interiors of both original pillars, away from filtered
        // cutout boundaries. Neither world models nor projected brackets may paint there.
        int checked=0;float scale=float(h)/480;int left=(w-h*4/3)/2;
        for(int sy=70;sy<290;++sy)for(int sx=3;sx<637;++sx){
            if(sx>160&&sx<480)continue;
            bool opaque=true;for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)opaque&=frame.indices[size_t(sy+dy)*640+sx+dx]!=0;
            if(!opaque)continue;
            int px=left+int((sx+0.5f)*scale),py=h-1-int((sy+0.5f)*scale);size_t at=(size_t(py)*w+px)*3;
            require(std::equal(base.begin()+at,base.begin()+at+3,target.begin()+at),"target brackets leak onto original pillar");++checked;
        }
        require(checked>500,"pillar assertion needs substantial opaque coverage");
        std::cout<<w<<'x'<<h<<": nine scenes; "<<checked<<" opaque pillar samples unchanged\n";
    }
    Audio audio;require(audio.init((fs::path(argv[1])/"audio").string()),"SDL audio init");
    for(bool legs:{false,true}){
        ready();audio.reset();audio.setVolumes({0,0,0,1});SDL_Delay(100);audio.clearStatistics();audio.startCapture(3);
        game.enemies[0].shield=0;game.damageEnemy(0,1000,legs);for(auto event:game.events)audio.play(event);
        SDL_Delay(2500);auto stats=audio.statistics();
        require(stats.assetEvents==2&&stats.fallbackEvents==0&&stats.audibleFrames>10000&&stats.finite&&stats.peak<1,"kill simulation did not reach SDL as original speech + explosion");
        require(audio.saveCapture((out/(legs?"disabled-sdl.wav":"destroyed-sdl.wav")).string()),"audio capture failed");
    }
    audio.shutdown();renderer.destroy();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();
    std::cout<<"PASS: battlefield rendering and simulation-to-SDL kill notifications\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
