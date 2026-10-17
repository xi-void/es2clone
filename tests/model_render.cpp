#include "renderer.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace siege {
struct ModelRenderTest {
    static void draw(Renderer& r,int w,int h,HercAppearance model,float angle,const Gait& gait,float torso=0) {
        glViewport(0,0,w,h);glClearColor(0.12f,0.14f,0.16f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        r.viewAspect=float(w)/h;r.camera({std::sin(angle)*26,12,std::cos(angle)*26},{0,6.5f,0},40);
        glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
        const float ambient[]={0.65f,0.65f,0.65f,1},sun[]={0.5f,0.5f,0.5f,1},light[]={-0.4f,0.8f,-0.5f,0};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightfv(GL_LIGHT0,GL_DIFFUSE,sun);glLightfv(GL_LIGHT0,GL_POSITION,light);glDisable(GL_FOG);
        r.box({0,-0.3f,0},{32,0.5f,32},{0.25f,0.28f,0.27f});
        r.mech({},0,gait,0,model==HercAppearance::Tomahawk,0,0,model,true,torso,0.1f);
    }
};
}
using namespace siege;
static void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
int main(int argc,char** argv)try {
    if(argc!=4){std::cerr<<"Usage: model_render ASSETS ORIGINAL_MODELS OUTPUT_DIRECTORY\n";return 2;}
    require(SDL_Init(SDL_INIT_VIDEO)==0,SDL_GetError());require(TTF_Init()==0,TTF_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    // Allocate the largest backing buffer first. Some SDL offscreen drivers do
    // not grow a hidden GL drawable immediately after SDL_SetWindowSize.
    auto* window=SDL_CreateWindow("Original HERC verification",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);require(window,SDL_GetError());
    auto context=SDL_GL_CreateContext(window);require(context,SDL_GetError());Renderer renderer;require(renderer.init(argv[1]),"Renderer init");require(renderer.loadOriginalModels(argv[2]),"Both original HERCs required");
    std::filesystem::create_directories(argv[3]);
    for(auto model:{HercAppearance::Tomahawk,HercAppearance::Achilles})for(int angle=0;angle<4;++angle)for(int frame=0;frame<4;++frame){
        Gait gait;gait.phase=frame/4.0;gait.blend=frame?1:0;
        ModelRenderTest::draw(renderer,960,720,model,pi+angle*pi/2,gait,frame?0.25f:0);
        auto file=std::filesystem::path(argv[3])/((model==HercAppearance::Tomahawk?"tomahawk":"achilles")+std::string("-view")+std::to_string(angle)+"-pose"+std::to_string(frame)+".ppm");
        require(renderer.screenshot(file.string(),960,720),"Screenshot failed");require(glGetError()==GL_NO_ERROR,"OpenGL model render error");
    }
    require(renderer.loadOriginalCockpit(std::filesystem::path(argv[2]).parent_path().string()),"Original cockpit required for integration capture");
    for(auto size:{std::pair{1920,1080},std::pair{1280,720}}) {
        auto [w,h]=size;SDL_SetWindowSize(window,w,h);Combat game;game.reset(0,modelTestMission);UIState ui;ui.mission=modelTestMission;
        auto capture=[&](const std::string& name){renderer.render(w,h,game,ui);auto path=std::filesystem::path(argv[3])/(name+"-"+std::to_string(w)+"x"+std::to_string(h)+".ppm");require(renderer.screenshot(path.string(),w,h),"Range capture failed");require(glGetError()==GL_NO_ERROR,"Range OpenGL error");};
        capture("range-selection");ui.screen=Screen::Playing;
        while(!game.ready())game.update(1.f/120,{});game.events.clear();game.selectTarget();capture("range-cockpit");
        for(int i=0;i<360;++i){game.update(1.f/120,{0.35f,0,0,false,false,true});game.events.clear();}
        capture("range-moving");
    }
    renderer.destroy();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();std::cout<<"PASS: 32 original HERC inspection renders + 6 mission integration captures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
