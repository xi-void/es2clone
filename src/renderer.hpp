#pragma once
#include "simulation.hpp"
#include "synthesis.hpp"
#include "radar.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <GL/gl.h>
#include <array>
#include <string>
#include <vector>

namespace siege {
struct Color { float r,g,b,a=1; };
enum class Screen { Menu, Hangar, Playing, Paused, Controls, Debrief, AudioSettings };
enum class Action { None, Launch, Hangar, Back, Controls, Quit, Sound, Fullscreen, PrevChassis, NextChassis, PrevMission, NextMission, Resume, Retry, Next, ShieldUpgrade, WeaponUpgrade,
    AudioSettings,MusicDown,MusicUp,WeaponsDown,WeaponsUp,MachineryDown,MachineryUp,InterfaceDown,InterfaceUp };
struct Button { float x,y,w,h; Action action; };
struct UIState {
    Screen screen=Screen::Menu,returnScreen=Screen::Menu;
    int chassis=0,mission=0,bank=0,shieldUpgrade=0,weaponUpgrade=0,completed=0;
    bool muted=false,fullscreen=false;
    AudioVolumes volumes=defaultVolumes;
    float mouseX=-1,mouseY=-1,time=0;
    std::string notice;
    float noticeTime=0;
    float radarMeters=defaultRadarRange;
};
struct Glyph { float u=0,v=0,w=0,h=0,advance=0; };
struct Font { GLuint texture=0; float size=0,height=0; std::array<Glyph,128> glyphs{}; };
class Renderer {
public:
    bool init(const std::string& assetPath);
    bool loadOriginalCockpit(const std::string& directory);
    void destroy();
    void render(int width,int height,const Combat& game,const UIState& ui);
    bool screenshot(const std::string& path,int width,int height);
    Action hit(float x,float y) const;
    std::vector<Button> buttons;
private:
    Font display,mono;
    GLuint terrainList=0,sceneryList=0,groundTexture=0;
    GLuint originalCockpitTexture=0;
    int pixelWidth=1440,pixelHeight=900;
    float viewAspect=1.6f;
    float uiWidth=1440,uiHeight=900,strokeScale=1;
    float matrix[16]{},projection[16]{};
    const UIState* state=nullptr;
    bool loadFont(Font& font,const std::string& path,int size);
    void begin2D(float width=1440,float height=900);
    void color(Color c);
    void rect(float x,float y,float w,float h,Color c);
    void outline(float x,float y,float w,float h,Color c,float thickness=1);
    void line(float x,float y,float xx,float yy,Color c,float thickness=1);
    void circle(float x,float y,float radius,Color c,bool fill=false,int segments=48);
    float textWidth(const std::string& value,float size,bool bold=false,float spacing=0);
    void text(float x,float y,const std::string& value,float size,Color c,bool bold=false,float spacing=0);
    void centered(float x,float y,const std::string& value,float size,Color c,bool bold=false);
    void button(float x,float y,float w,float h,const std::string& label,Action action,bool primary=false,bool disabled=false);
    void bar(float x,float y,float w,float h,float amount,Color c);
    void sky(float time);
    void world(const Combat& game,bool menu,float time);
    void camera(Vec3 eye,Vec3 center,float fov,float roll=0);
    void makeTerrain();
    void box(Vec3 pos,Vec3 size,Color c);
    void cylinder(Vec3 pos,float radius,float height,Color c,int sides=8);
    void mountain(float x,float z,float radius,float height,Color c,int seed);
    void mech(Vec3 pos,float yaw,const Gait& gait,int type,bool friendly,float damage=0,float death=0);
    void strut(Vec3 a,Vec3 b,float width,float depth,Color c);
    void bevel(float x,float y,float w,float h,float rim,Color face);
    void schematic(float x,float y,float scale,float armor,float legs,float stroke=2);
    void shadow(Vec3 pos,float radius);
    bool project(Vec3 pos,float& x,float& y) const;
    void menu(const UIState& ui);
    void hangar(const UIState& ui);
    void hud(const Combat& game,const UIState& ui);
    void originalHud(const Combat& game,const UIState& ui);
    void radar(const Combat& game,float x,float y,float size);
    void marquee(const Combat& game,float x,float y,float width,float size);
    void controls(const UIState& ui);
    void pause(const UIState& ui);
    void audioSettings(const UIState& ui);
    void debrief(const Combat& game,const UIState& ui);
    void topbar(const UIState& ui);
};
}
