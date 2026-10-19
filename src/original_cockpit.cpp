#include "renderer.hpp"
#include "original_graphics.hpp"
#include <filesystem>
#include <iostream>

namespace siege {
bool Renderer::loadOriginalCockpit(const std::string& directory) {
    namespace fs=std::filesystem;
    try {
        auto root=fs::path(directory);
        if(!fs::exists(root/"TOMAHAWK.HB0") && !fs::exists(root/"COCKPIT.DPL"))return false;
        auto palette=original::decodePalette(original::readFile(root/"COCKPIT.DPL"));
        auto frames=original::decodeBitmaps(original::readFile(root/"TOMAHAWK.HB0"));
        if(frames.size()!=1 || frames[0].width!=640 || frames[0].height!=480)
            throw std::runtime_error("Expected the verified 640x480 Tomahawk cockpit");
        auto pixels=frames[0].rgba(palette,true);
        // Premultiply the binary-alpha cutout before filtering; transparent RGB must not
        // produce a dark fringe when interpolation blends an edge with the windshield.
        for(size_t i=0;i<pixels.size();i+=4)if(!pixels[i+3])pixels[i]=pixels[i+1]=pixels[i+2]=0;
        glGenTextures(1,&originalCockpitTexture);glBindTexture(GL_TEXTURE_2D,originalCockpitTexture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,640,480,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        std::cout<<"Original cockpit: TOMAHAWK.HB0 + COCKPIT.DPL (640x480)\n";
        return true;
    } catch(const std::exception& e) {
        std::cerr<<"Original cockpit unavailable; using prototype artwork: "<<e.what()<<'\n';return false;
    }
}

void Renderer::originalHud(const Combat& game,const UIState&) {
    constexpr Color cyan{0.13f,0.77f,0.95f},green{0.18f,0.95f,0.12f},white{0.94f,0.94f,0.94f},
        grey{0.66f,0.71f,0.64f},red{0.92f,0.12f,0.05f},purple{0.55f,0.55f,0.92f},black{0.07f,0.07f,0.07f};
    const auto& p=game.player;
    auto number=[](float value){return std::to_string(int(std::round(value)));};
    auto projectOriginal=[&](Vec3 point,float& x,float& y) {
        return project(point,x,y);
    };

    // Windshield symbology uses the same projection as the 3D view, keeping gun aim stable.
    float heading=std::fmod((p.yaw+p.torso)*180/pi+3600,360.0f);
    rect(229,8,182,4,black);rect(289,9,62,2,green);line(320+p.torso*27,5,320+p.torso*27,17,cyan);
    for(int i=-20;i<=20;++i){float x=320+i*5.8f;rect(x,27,1,i%5==0?4:1,cyan);if(i%10==0)centered(x,34,number(std::fmod(heading+i+360,360)),9,cyan);}
    Vec3 waypoint=game.navigationPoint()-p.pos;
    float bearing=std::remainder(std::atan2(waypoint.x,-waypoint.z)-p.yaw-p.torso,2*pi);
    float marker=320+clamp(bearing*180/pi*5.8f,-116,116);
    color(green);glBegin(GL_QUADS);glVertex2f(marker,15);glVertex2f(marker+4,19);glVertex2f(marker,23);glVertex2f(marker-4,19);glEnd();
    centered(320,49,(game.extractionOpen?"EXTRACTION: ":"WAYPOINT "+std::to_string(game.wave)+": ")+number(length(game.navigationPoint()-p.pos))+" M",8,cyan);
    radar(game,145,85,90);
    marquee(game,175,65,290,9);
    for(int i=0;i<int(game.enemies.size());++i)if(game.enemies[i].alive) {
        const auto& e=game.enemies[i];Vec3 d=e.pos-p.pos;
        float x,y;
        if(projectOriginal(e.pos+Vec3{0,7,0},x,y)&&y>70&&y<290) {
            float size=clamp(930/length(d),6,20);outline(x-size,y-size,size*2,size*2,green);
            if(i==game.target){line(x-size-4,y,x+size+4,y,green);line(x,y-size-4,x,y+size+4,green);}
        }
    }
    float rx=320,ry=240;projectOriginal(game.eye()+game.aim()*300,rx,ry);
    Color aimColor=game.hitFlash>0?green:white;
    circle(rx,ry,11,aimColor,false,32);line(rx-18,ry,rx-8,ry,aimColor);line(rx+8,ry,rx+18,ry,aimColor);
    line(rx,ry-18,rx,ry-8,aimColor);line(rx,ry+8,rx,ry+18,aimColor);
    if(game.extractionOpen||game.navigationOnly()){float x,y;if(projectOriginal(game.navigationPoint()+Vec3{0,9,0},x,y)){outline(x-6,y-6,12,12,green);centered(x,y-17,game.extractionOpen?"EXTRACT":"WP "+std::to_string(game.wave),8,green);}}
    text(199,291,"SPEED: "+number(std::hypot(p.speed,p.sideSpeed)*3.6f)+" KPH",8,cyan);
    int seconds=int(game.time);text(365,291,"TIME: "+std::to_string(seconds/60)+":"+(seconds%60<10?"0":"")+std::to_string(seconds%60),8,cyan);

    // The unchanged contact-driven suspension moves the original frame and its instruments together.
    // Coefficients are the prototype console's existing offsets converted to source pixels.
    glPushMatrix();glTranslatef(p.gait.sway*8*640/1440,p.gait.bounce*10*480/900,0);
    // Draw the frame over projected marks: the artwork itself masks pillars and consoles.
    glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,originalCockpitTexture);color({1,1,1});
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(1,0);glVertex2f(640,0);
    glTexCoord2f(1,1);glVertex2f(640,480);glTexCoord2f(0,1);glVertex2f(0,480);
    glEnd();glDisable(GL_TEXTURE_2D);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // Temporary live instruments placed within the source artwork. GAU/HFN/MFD semantics
    // are a separate compatibility step; do not fabricate original equipment or bindings.
    const char* weapons[]={"LAS 200","ATC 35","MISSILE","TURBO"};
    for(int i=0;i<4;++i) {
        float y=351+i*18;
        rect(54,y,112,14,black);
        if(i==p.weapon)outline(54.5f,y+0.5f,111,13,white,0.65f);
        text(57,y+1,i<3?std::to_string(i+1):" ",8,i==p.weapon?green:grey);
        text(68,y+1,weapons[i],8,white);
        if(i==2)text(145,y+1,std::to_string(p.missiles),8,white);
        else bar(136,y+3,25,7,i==3?p.turboPower:1,purple);
    }
    rect(54,423,112,13,black);text(57,424,"HEAT",8,grey);bar(101,426,60,6,p.heat/100,p.overheated?red:cyan);
    const char* keys[]={"T","1","2","3","C","F1"};
    for(int i=0;i<6;++i)centered(223,356+i*20,keys[i],9,purple);
    const Enemy* target=game.target>=0&&game.target<int(game.enemies.size())&&game.enemies[game.target].alive?&game.enemies[game.target]:nullptr;
    text(250,360,"TARGET",9,white);text(348,360,"T / TAB",8,grey);line(246,375,429,375,grey);
    text(250,384,target?chassis[target->type].name:"NO LOCK",10,target?red:grey);
    text(250,407,"STATUS",9,grey);text(250,422,target?(target->legs<30?"LEG DAMAGE":"TRACKING"):"SCANNING",8,green);
    text(250,449,"DIST",8,grey);text(280,449,target?number(length(target->pos-p.pos))+" M":"----",8,red);
    schematic(383,391,0.47f,target?target->armor/target->maxArmor:1,target?target->legs/75:1,0.6f);
    float throttle=clamp(std::abs(p.speed)/(p.maxSpeed*1.6f),0,1);
    rect(477,431-throttle*70,20,5,white);
    rect(548,374,36,69,black);centered(566,377,number(p.armor),10,white);
    bar(551,396,30,8,p.armor/p.maxArmor,green);bar(551,411,30,8,p.shield/p.maxShield,cyan);
    centered(566,427,number(p.shield),10,cyan);
    glPopMatrix();

    if(p.overheated)centered(320,265,"REACTOR OVERHEAT",10,red);
    else if(p.turboPower>0.1f)centered(320,265,p.turboEngaged?"TURBO ENGAGED":"TURBINE SPOOL-DOWN",8,cyan);
    if(game.messageTime>0&&game.ready()) {
        float size=8;while(size>6&&textWidth(game.message,size)>310)size-=0.25f;
        centered(320,278,game.message,size,cyan);
    }
    if(!game.ready()) {
        rect(250,103,220,142,black);outline(250,103,220,142,purple);
        centered(360,112,game.insertionAltitude()>0?"AIR-DROP INSERTION":"TOUCHDOWN // INITIALIZING",10,cyan);
        centered(360,127,game.insertionAltitude()>0?"ALTITUDE: "+number(game.insertionAltitude())+" M":"GROUND CONTACT CONFIRMED",8,grey);
        const char* stages[]={"REACTOR IGNITION","SYSTEM DIAGNOSTICS","HYDRAULIC PRESSURE","ACTUATORS ONLINE"};
        int stage=int(game.startupStage());
        for(int i=0;i<4;++i){text(262,149+i*18,stages[i],8,i<=stage?white:grey);text(438,149+i*18,i<stage?"OK":i==stage?"..":"--",8,green);}
        bar(262,231,196,3,game.startupTime/startupDuration,cyan);
    }
    if(game.damageFlash>0)outline(1,1,638,478,{1,0.15f,0.05f,clamp(game.damageFlash,0,1)},3);
}
}
