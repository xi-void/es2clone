#include "renderer.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace siege {
namespace {
constexpr Color cream{0.94f,0.91f,0.83f}, muted{0.59f,0.63f,0.60f}, orange{1,0.39f,0.19f}, mint{0.54f,0.85f,0.70f}, red{1,0.28f,0.22f};
constexpr Color panel{0.035f,0.055f,0.055f,0.94f}, border{0.29f,0.34f,0.31f,0.65f};
std::string num(float value,int digits=0) {std::ostringstream s;s<<std::fixed<<std::setprecision(digits)<<value;return s.str();}
void vertex(Vec3 p) {glVertex3f(p.x,p.y,p.z);}
}
void Renderer::color(Color c) {glColor4f(c.r,c.g,c.b,c.a);}
bool Renderer::loadFont(Font& font,const std::string& path,int size) {
    TTF_Font* source=TTF_OpenFont(path.c_str(),size);
    if(!source) {std::cerr<<"Font: "<<TTF_GetError()<<'\n';return false;}
    font.size=float(size);font.height=float(TTF_FontHeight(source));
    SDL_Surface* atlas=SDL_CreateRGBSurfaceWithFormat(0,1024,512,32,SDL_PIXELFORMAT_RGBA32);
    if(!atlas) {TTF_CloseFont(source);return false;}
    SDL_FillRect(atlas,nullptr,SDL_MapRGBA(atlas->format,255,255,255,0));
    for(int c=32;c<127;++c) {
        SDL_Surface* glyph=TTF_RenderGlyph_Blended(source,Uint16(c),{255,255,255,255});
        if(!glyph) continue;
        int x=((c-32)%16)*64,y=((c-32)/16)*80,advance=0;
        TTF_GlyphMetrics(source,Uint16(c),nullptr,nullptr,nullptr,nullptr,&advance);
        SDL_Rect dest{x,y,glyph->w,glyph->h};
        SDL_SetSurfaceBlendMode(glyph,SDL_BLENDMODE_NONE);SDL_BlitSurface(glyph,nullptr,atlas,&dest);
        font.glyphs[c]={float(x)/1024,float(y)/512,float(glyph->w),float(glyph->h),float(advance)};
        SDL_FreeSurface(glyph);
    }
    glGenTextures(1,&font.texture);glBindTexture(GL_TEXTURE_2D,font.texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1024,512,0,GL_RGBA,GL_UNSIGNED_BYTE,atlas->pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    SDL_FreeSurface(atlas);TTF_CloseFont(source);return true;
}
bool Renderer::init(const std::string& path) {
    if(!loadFont(display,path+"/fonts/Display.ttf",48)||!loadFont(mono,path+"/fonts/Mono.ttf",28)) return false;
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_NORMALIZE);glDisable(GL_CULL_FACE);
    makeTerrain();return true;
}
void Renderer::destroy() {if(display.texture)glDeleteTextures(1,&display.texture);if(mono.texture)glDeleteTextures(1,&mono.texture);if(terrainList)glDeleteLists(terrainList,1);if(sceneryList)glDeleteLists(sceneryList,1);}
void Renderer::begin2D() {
    glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1440,900,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
}
void Renderer::rect(float x,float y,float w,float h,Color c) {color(c);glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();}
void Renderer::outline(float x,float y,float w,float h,Color c,float thickness) {glLineWidth(thickness);color(c);glBegin(GL_LINE_LOOP);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();glLineWidth(1);}
void Renderer::line(float x,float y,float xx,float yy,Color c,float thickness) {glLineWidth(thickness);color(c);glBegin(GL_LINES);glVertex2f(x,y);glVertex2f(xx,yy);glEnd();glLineWidth(1);}
void Renderer::circle(float x,float y,float radius,Color c,bool fill,int segments) {color(c);glBegin(fill?GL_TRIANGLE_FAN:GL_LINE_LOOP);if(fill)glVertex2f(x,y);for(int i=0;i<=segments;++i){float a=float(i)/segments*pi*2;glVertex2f(x+std::cos(a)*radius,y+std::sin(a)*radius);}glEnd();}
float Renderer::textWidth(const std::string& value,float size,bool bold,float spacing) {const Font& font=bold?display:mono;float width=0;for(unsigned char c:value)width+=font.glyphs[c<128?c:'?'].advance*size/font.size+spacing;return width;}
void Renderer::text(float x,float y,const std::string& value,float size,Color c,bool bold,float spacing) {
    const Font& f=bold?display:mono;float scale=size/f.size;
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,f.texture);color(c);glBegin(GL_QUADS);
    for(unsigned char ch:value) {
        const auto& g=f.glyphs[ch<128?ch:'?'];float w=g.w*scale,h=g.h*scale;
        glTexCoord2f(g.u,g.v);glVertex2f(x,y);glTexCoord2f(g.u+g.w/1024,g.v);glVertex2f(x+w,y);
        glTexCoord2f(g.u+g.w/1024,g.v+g.h/512);glVertex2f(x+w,y+h);glTexCoord2f(g.u,g.v+g.h/512);glVertex2f(x,y+h);
        x+=g.advance*scale+spacing;
    }
    glEnd();glDisable(GL_TEXTURE_2D);
}
void Renderer::centered(float x,float y,const std::string& value,float size,Color c,bool bold) {text(x-textWidth(value,size,bold)/2,y,value,size,c,bold);}
void Renderer::button(float x,float y,float w,float h,const std::string& label,Action action,bool primary,bool disabled) {
    bool hover=state->mouseX>=x&&state->mouseX<x+w&&state->mouseY>=y&&state->mouseY<y+h&&!disabled;
    Color bg=primary?orange:panel;if(hover){bg.r=std::min(1.0f,bg.r+0.10f);bg.g+=0.08f;bg.b+=0.05f;}
    if(disabled)bg={0.10f,0.13f,0.12f,0.9f};
    rect(x,y,w,h,bg);outline(x,y,w,h,primary?orange:hover?cream:border);
    centered(x+w/2,y+h/2-10,label,16,disabled?muted:primary?Color{0.05f,0.065f,0.05f}:cream,true);
    if(!disabled)buttons.push_back({x,y,w,h,action});
}
void Renderer::bar(float x,float y,float w,float h,float amount,Color c) {rect(x,y,w,h,{0.16f,0.2f,0.18f,0.8f});rect(x,y,w*clamp(amount,0,1),h,c);for(float k=1;k<10;++k)line(x+w*k/10,y,x+w*k/10,y+h,panel);}
Action Renderer::hit(float x,float y) const {for(auto i=buttons.rbegin();i!=buttons.rend();++i)if(x>=i->x&&x<i->x+i->w&&y>=i->y&&y<i->y+i->h)return i->action;return Action::None;}
void Renderer::sky(float time) {
    begin2D();glBegin(GL_QUADS);color({0.10f,0.16f,0.18f});glVertex2f(0,0);glVertex2f(1440,0);color({0.65f,0.52f,0.37f});glVertex2f(1440,600);glVertex2f(0,600);glEnd();
    rect(0,600,1440,300,{0.65f,0.52f,0.37f});
    circle(1080,248,72,{0.95f,0.68f,0.38f,0.08f},true);circle(1080,248,58,{1.0f,0.75f,0.45f,0.14f},true);circle(1080,248,44,{1.0f,0.80f,0.52f,0.7f},true);
    for(int i=0;i<8;++i) {float y=150+i*37.0f;line(850+std::sin(i*4.0f)*100+std::sin(time*0.02f)*10,y,1440,y,{0.70f,0.63f,0.45f,0.055f},3);}
}
void Renderer::camera(Vec3 eye,Vec3 center,float fov) {
    glMatrixMode(GL_PROJECTION);glLoadIdentity();
    double near=0.3,top=near*std::tan(fov*pi/360.0);double aspect=double(pixelWidth)/pixelHeight;
    glFrustum(-top*aspect,top*aspect,-top,top,near,1800);
    glGetFloatv(GL_PROJECTION_MATRIX,projection);
    Vec3 forward=normalized(center-eye),side=normalized(cross(forward,{0,1,0})),up=cross(side,forward);
    float view[]={side.x,up.x,-forward.x,0,side.y,up.y,-forward.y,0,side.z,up.z,-forward.z,0,-dot(side,eye),-dot(up,eye),dot(forward,eye),1};
    glMatrixMode(GL_MODELVIEW);glLoadMatrixf(view);glGetFloatv(GL_MODELVIEW_MATRIX,matrix);
}
void Renderer::box(Vec3 p,Vec3 s,Color c) {
    color(c);glPushMatrix();glTranslatef(p.x,p.y,p.z);glScalef(s.x/2,s.y/2,s.z/2);
    glBegin(GL_QUADS);
    glNormal3f(0,1,0);glVertex3f(-1,1,-1);glVertex3f(-1,1,1);glVertex3f(1,1,1);glVertex3f(1,1,-1);
    glNormal3f(0,-1,0);glVertex3f(-1,-1,1);glVertex3f(-1,-1,-1);glVertex3f(1,-1,-1);glVertex3f(1,-1,1);
    glNormal3f(0,0,-1);glVertex3f(-1,-1,-1);glVertex3f(-1,1,-1);glVertex3f(1,1,-1);glVertex3f(1,-1,-1);
    glNormal3f(0,0,1);glVertex3f(1,-1,1);glVertex3f(1,1,1);glVertex3f(-1,1,1);glVertex3f(-1,-1,1);
    glNormal3f(-1,0,0);glVertex3f(-1,-1,1);glVertex3f(-1,1,1);glVertex3f(-1,1,-1);glVertex3f(-1,-1,-1);
    glNormal3f(1,0,0);glVertex3f(1,-1,-1);glVertex3f(1,1,-1);glVertex3f(1,1,1);glVertex3f(1,-1,1);
    glEnd();glPopMatrix();
}
void Renderer::cylinder(Vec3 p,float radius,float height,Color c,int sides) {
    color(c);glBegin(GL_QUADS);
    for(int i=0;i<sides;++i) {float a=i*2*pi/sides,b=(i+1)*2*pi/sides;glNormal3f(std::cos((a+b)/2),0,std::sin((a+b)/2));glVertex3f(p.x+std::cos(a)*radius,p.y,p.z+std::sin(a)*radius);glVertex3f(p.x+std::cos(b)*radius,p.y,p.z+std::sin(b)*radius);glVertex3f(p.x+std::cos(b)*radius,p.y+height,p.z+std::sin(b)*radius);glVertex3f(p.x+std::cos(a)*radius,p.y+height,p.z+std::sin(a)*radius);}
    glEnd();color({c.r*1.1f,c.g*1.1f,c.b*1.1f,c.a});glBegin(GL_TRIANGLE_FAN);glNormal3f(0,1,0);glVertex3f(p.x,p.y+height,p.z);for(int i=0;i<=sides;++i){float a=i*2*pi/sides;glVertex3f(p.x+std::cos(a)*radius,p.y+height,p.z+std::sin(a)*radius);}glEnd();
}
void Renderer::mountain(float x,float z,float radius,float height,Color c,int seed) {
    const int sides=7;glBegin(GL_TRIANGLES);
    for(int i=0;i<sides;++i){float a=i*2*pi/sides,b=(i+1)*2*pi/sides;Vec3 p{x+std::cos(a)*radius,0,z+std::sin(a)*radius},q{x+std::cos(b)*radius,0,z+std::sin(b)*radius},peak{x+std::sin(float(seed))*radius*0.3f,height,z};Vec3 n=normalized(cross(q-p,peak-p));color({c.r*(1+i*0.015f),c.g*(1+i*0.015f),c.b*(1+i*0.015f)});glNormal3f(n.x,n.y,n.z);vertex(p);vertex(q);vertex(peak);}
    glEnd();
}
void Renderer::makeTerrain() {
    std::mt19937 rng{2624};auto random=[&](float low,float high){return std::uniform_real_distribution<float>(low,high)(rng);};
    terrainList=glGenLists(1);glNewList(terrainList,GL_COMPILE);glBegin(GL_TRIANGLES);
    for(int x=-720;x<720;x+=18)for(int z=-720;z<720;z+=18) {
        Vec3 a{float(x),terrainHeight(float(x),float(z)),float(z)},b{float(x+18),terrainHeight(float(x+18),float(z)),float(z)},c{float(x+18),terrainHeight(float(x+18),float(z+18)),float(z+18)},d{float(x),terrainHeight(float(x),float(z+18)),float(z+18)};
        float shade=random(0.91f,1.07f);Color base{0.35f*shade,0.29f*shade,0.20f*shade};
        auto tri=[&](Vec3 p,Vec3 q,Vec3 r){Vec3 n=normalized(cross(q-p,r-p));color(base);glNormal3f(n.x,n.y,n.z);vertex(p);vertex(q);vertex(r);};tri(a,d,b);tri(b,d,c);
    }
    glEnd();glEndList();
    sceneryList=glGenLists(1);glNewList(sceneryList,GL_COMPILE);
    for(int i=0;i<55;++i){float a=float(i)*2*pi/55;mountain(std::cos(a)*random(650,820),std::sin(a)*random(650,820),random(75,190),random(70,220),{0.29f,0.29f,0.25f},i);}
    for(int i=0;i<150;++i){float x=random(-580,580),z=random(-580,580);if(std::abs(x)<85&&z>-170&&z<150)continue;float s=random(1,5);glPushMatrix();glTranslatef(x,terrainHeight(x,z),z);glRotatef(random(0,360),0,1,0);glRotatef(random(-15,15),0,0,1);box({0,s*0.3f,0},{s*1.8f,s,s*1.3f},{0.30f,0.29f,0.24f});glPopMatrix();}
    // A ruined relay outpost on the western edge of the battlefield.
    for(int i=0;i<5;++i){float x=-130+i*20.0f,z=-140-(i%2)*38.0f,y=terrainHeight(x,z),h=9+(i%3)*5.0f;box({x,y+h/2,z},{14,h,19},{0.28f,0.30f,0.27f});box({x,y+h,z},{15,1,20},{0.18f,0.22f,0.21f});for(int j=0;j<4;++j)box({x-5+j*3.2f,y+h*0.65f,z+9.56f},{1.6f,1.3f,0.1f},{0.10f,0.16f,0.15f});}
    cylinder({-105,terrainHeight(-105,-220),-220},5,20,{0.25f,0.29f,0.26f});
    cylinder({-105,20,-220},0.7f,28,{0.25f,0.28f,0.25f});
    box({-105,38,-220},{18,0.6f,0.8f},{0.23f,0.28f,0.25f});box({-105,43,-220},{12,0.6f,0.8f},{0.23f,0.28f,0.25f});
    for(int i=0;i<9;++i){float x=80+i*13.0f;box({x,1,-185},{9,2.6f,5},{0.33f,0.32f,0.25f});}
    glEndList();
}
void Renderer::shadow(Vec3 p,float radius) {
    glDisable(GL_LIGHTING);color({0.025f,0.04f,0.035f,0.3f});glBegin(GL_TRIANGLE_FAN);glVertex3f(p.x,terrainHeight(p.x,p.z)+0.08f,p.z);for(int i=0;i<=24;++i){float a=i*2*pi/24,x=p.x+std::cos(a)*radius,z=p.z+std::sin(a)*radius*0.65f;glVertex3f(x,terrainHeight(x,z)+0.08f,z);}glEnd();glEnable(GL_LIGHTING);
}
void Renderer::mech(Vec3 pos,float yaw,float walk,int type,bool friendly,float damage,float death) {
    shadow(pos,9);glPushMatrix();glTranslatef(pos.x,pos.y,pos.z);glRotatef(-yaw*180/pi,0,1,0);
    float size=type==1?1.12f:type==2?0.88f:1.0f;glScalef(size,size,size);
    if(death>0){glTranslatef(0,-std::min(death,1.0f)*4.3f,0);glRotatef(std::min(death,1.0f)*68,1,0,0);}
    Color armor=friendly?Color{0.35f,0.40f,0.34f}:Color{0.38f,0.32f,0.28f};
    Color edge=friendly?Color{0.49f,0.53f,0.43f}:Color{0.46f,0.40f,0.33f};
    Color dark{0.12f,0.16f,0.15f},steel{0.33f,0.37f,0.34f},accent=friendly?orange:Color{0.68f,0.19f,0.12f};
    if(damage>0)armor={0.95f,0.72f,0.43f};
    for(int side:{-1,1}) {
        float step=std::sin(walk+float(side)*pi/2),x=side*2.25f;
        glPushMatrix();glTranslatef(x,6.5f,0);glRotatef(step*19,1,0,0);
        box({0,-1.1f,0},{1.9f,3,2.1f},dark);box({0,-1.2f,-1},{2.25f,2.6f,0.8f},armor);
        box({0,-2.8f,-0.15f},{2.6f,1.5f,2.9f},edge);
        glTranslatef(0,-3.1f,0);glRotatef(-step*10,1,0,0);
        box({0,-1.15f,0.2f},{1.9f,2.9f,2},armor);box({0,-1.2f,-0.86f},{2.2f,2.2f,0.5f},edge);
        box({0,-0.6f,-1.15f},{1.8f,0.42f,0.12f},accent);
        cylinder({side*1.05f,-2,0.1f},0.19f,2.6f,steel);
        box({0,-2.6f,-0.7f},{2.8f,1.1f,4.0f},dark);box({0,-2.05f,-1.2f},{2.5f,0.6f,2.8f},armor);
        for(int j=0;j<3;++j)box({-0.8f+j*0.8f,-2.1f,-2.68f},{0.4f,0.2f,0.08f},steel);
        glPopMatrix();
    }
    cylinder({0,5.3f,0},2.0f,1.8f,dark);
    glPushMatrix();glTranslatef(0,std::sin(walk*2)*0.12f,0);
    box({0,8.5f,0},{5.9f,3.9f,3.9f},armor);
    box({0,10.2f,-0.15f},{5.3f,1.0f,4.3f},edge);
    box({0,7.4f,-2.05f},{4.6f,1.0f,0.45f},dark);
    for(int j=0;j<6;++j)box({-1.8f+j*0.72f,7.4f,-2.32f},{0.12f,0.75f,0.05f},steel);
    box({0,11.3f,-0.5f},{3.5f,1.6f,3.4f},armor);
    box({0,11.3f,-2.22f},{3.0f,0.72f,0.15f},dark);
    glDisable(GL_LIGHTING);box({0,11.4f,-2.32f},{2.75f,0.35f,0.1f},death>0?dark:friendly?Color{0.50f,0.91f,0.81f}:Color{1.0f,0.28f,0.13f});glEnable(GL_LIGHTING);
    box({-0.65f,9.1f,-2.04f},{0.55f,1.75f,0.12f},accent);box({0.15f,9.1f,-2.04f},{0.22f,1.75f,0.12f},accent);
    box({0,9,2.1f},{3.7f,2.9f,1.6f},dark);cylinder({-1.2f,9.8f,2.6f},0.5f,2.6f,steel);cylinder({1.2f,9.8f,2.6f},0.5f,2.6f,steel);
    cylinder({1.4f,11.8f,0.3f},0.065f,3.6f,steel);
    for(int side:{-1,1}) {
        box({side*3.4f,9.0f,0},{1.7f,1.5f,1.8f},dark);
        box({side*4.6f,9.5f,0},{2.5f,3.1f,3.5f},armor);
        box({side*4.6f,11.1f,0},{2.65f,0.55f,3.7f},edge);
        box({side*4.6f,10.1f,-1.82f},{2.1f,0.42f,0.10f},accent);
        if(side==1) {
            box({4.6f,8.4f,-0.8f},{1.8f,1.9f,3.5f},dark);
            glPushMatrix();glTranslatef(4.6f,8.4f,-1.4f);glRotatef(-90,1,0,0);cylinder({0,0,0},0.70f,3.7f,steel);cylinder({0,3.3f,0},0.85f,0.6f,dark);glPopMatrix();
            glDisable(GL_LIGHTING);box({4.6f,8.4f,-5.15f},{0.6f,0.6f,0.05f},friendly?mint:red);glEnable(GL_LIGHTING);
        } else {
            box({-4.6f,8.0f,-0.5f},{2,2.1f,2.4f},dark);
            for(int r=0;r<2;++r)for(int c=0;c<2;++c){glPushMatrix();glTranslatef(-5.1f+c*0.95f,7.55f+r*0.95f,-1.8f);glRotatef(-90,1,0,0);cylinder({0,0,0},0.32f,0.5f,steel,8);glPopMatrix();}
        }
        for(int k=0;k<2;++k)box({side*5.9f,9+k*1.0f,0},{0.13f,0.35f,0.8f},dark);
    }
    glPopMatrix();glPopMatrix();
}
void Renderer::world(const Combat& game,bool menuMode,float time) {
    glEnable(GL_DEPTH_TEST);glClear(GL_DEPTH_BUFFER_BIT);glDisable(GL_TEXTURE_2D);
    if(menuMode) camera({-14,11.8f,30},{-1,7.4f,0},42);
    else {Vec3 eye=game.eye();camera(eye,eye+game.aim(),65);}
    glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    const float ambient[]={0.42f,0.45f,0.43f,1},sun[]={0.95f,0.79f,0.58f,1},position[]={-0.4f,0.85f,0.6f,0};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightfv(GL_LIGHT0,GL_DIFFUSE,sun);glLightfv(GL_LIGHT0,GL_POSITION,position);
    glEnable(GL_FOG);const float fog[]={0.58f,0.49f,0.36f,1};glFogfv(GL_FOG_COLOR,fog);glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,100);glFogf(GL_FOG_END,850);
    glCallList(terrainList);glCallList(sceneryList);
    if(menuMode) {Vec3 pos{11,terrainHeight(11,0)+0.3f,0};mech(pos,pi+0.65f+std::sin(time*0.12f)*0.12f,0,state->chassis,true);}
    else {
        for(const auto& e:game.enemies) mech(e.pos,e.yaw,e.walk,e.type,false,e.flash,e.deathTime);
        glDisable(GL_LIGHTING);
        for(const auto& p:game.pickups) if(!p.collected) {
            glPushMatrix();glTranslatef(p.pos.x,p.pos.y+2.0f+std::sin(time*2)*0.5f,p.pos.z);glRotatef(time*35,0,1,0);box({0,0,0},{1.3f,1.3f,1.3f},mint);glPopMatrix();
        }
        glDepthMask(GL_FALSE);
        if(game.extractionOpen) {
            color({0.25f,0.9f,0.58f,0.12f});glBegin(GL_QUAD_STRIP);for(int i=0;i<=48;++i){float a=i*2*pi/48;glVertex3f(game.extraction.x+std::cos(a)*22,game.extraction.y+0.1f,game.extraction.z+std::sin(a)*22);glVertex3f(game.extraction.x+std::cos(a)*22,game.extraction.y+30,game.extraction.z+std::sin(a)*22);}glEnd();
            glLineWidth(2);color(mint);glBegin(GL_LINE_LOOP);for(int i=0;i<48;++i){float a=i*2*pi/48;glVertex3f(game.extraction.x+std::cos(a)*22,game.extraction.y+0.2f,game.extraction.z+std::sin(a)*22);}glEnd();
        }
        for(const auto& b:game.beams){glLineWidth(b.kind==0?4:3);Color c=b.kind==0?Color{0.40f,1,0.83f,b.life/b.maxLife}:Color{1,0.75f,0.28f,b.life/b.maxLife};color(c);glBegin(GL_LINES);vertex(b.a);vertex(b.b);glEnd();}
        for(const auto& p:game.projectiles) {Color c=p.friendly?Color{1,0.73f,0.27f}:Color{1,0.23f,0.08f};box(p.pos,{0.65f,0.65f,0.65f},c);glLineWidth(p.friendly?3:2);color(c);glBegin(GL_LINES);vertex(p.pos);vertex(p.pos-p.velocity*0.06f);glEnd();}
        for(const auto& p:game.particles){float t=p.life/p.maxLife;Color c=p.kind==0?Color{1,0.45f,0.1f,t}:p.kind==1?Color{1,0.8f,0.38f,t}:Color{0.18f,0.19f,0.17f,t};box(p.pos,{p.size,p.size,p.size},c);}
        glDepthMask(GL_TRUE);glLineWidth(1);
    }
    glDisable(GL_FOG);glDisable(GL_LIGHTING);
}
bool Renderer::project(Vec3 p,float& x,float& y) const {
    float v[]={p.x,p.y,p.z,1},view[4]{},clip[4]{};
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)view[r]+=matrix[c*4+r]*v[c];
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)clip[r]+=projection[c*4+r]*view[c];
    if(clip[3]<=0.1f)return false;
    x=(clip[0]/clip[3]*0.5f+0.5f)*1440;y=(-clip[1]/clip[3]*0.5f+0.5f)*900;
    return x>0&&x<1440&&y>0&&y<900;
}
void Renderer::topbar(const UIState& ui) {
    line(48,92,1392,92,{0.62f,0.66f,0.59f,0.22f});
    line(52,38,66,65,orange,4);line(66,65,80,38,orange,4);line(58,52,74,52,orange,3);
    text(99,31,"EARTHSIEGE II",22,cream,true,1);text(100,59,"RESISTANCE / HERC OPERATIONS",10,muted,false,1.3f);
    circle(1090,49,3,mint,true);text(1102,40,"SYSTEM ONLINE",11,mint);
    button(1230,30,65,36,ui.muted?"MUTED":"SOUND",Action::Sound);button(1308,30,82,36,"F11  [ ]",Action::Fullscreen);
}
void Renderer::menu(const UIState& ui) {
    // A dark translucent gradient keeps the live 3D hangar visible.
    glBegin(GL_QUADS);color({0.025f,0.045f,0.045f,0.97f});glVertex2f(0,0);glVertex2f(0,900);color({0.025f,0.045f,0.045f,0});glVertex2f(1090,900);glVertex2f(1090,0);glEnd();
    rect(0,798,1440,102,{0.025f,0.04f,0.04f,0.90f});topbar(ui);
    text(64,145,"// HUMAN RESISTANCE  /  2624",12,orange,false,1.7f);
    text(58,180,"EARTH",106,cream,true,1);text(58,281,"SIEGE",106,cream,true,1);
    rect(403,301,100,86,orange);text(429,297,"II",79,{0.045f,0.065f,0.055f},true);
    text(65,414,"THE MACHINES TOOK EARTH.",19,cream,true,0.7f);
    text(65,445,"Take it back. One hundred tons at a time.",14,muted);
    rect(64,500,514,145,{0.055f,0.075f,0.069f,0.88f});rect(64,500,3,145,orange);
    text(84,518,"OPERATION 0"+std::to_string(ui.mission+1)+" / 03",11,orange,false,1);
    text(84,545,missions[ui.mission].name,28,cream,true);
    text(84,585,missions[ui.mission].region+std::string("  /  ")+std::to_string(missions[ui.mission].enemies)+" HOSTILES",11,muted);
    text(84,610,"ELIMINATE. SALVAGE. EXTRACT.",11,mint);
    button(474,518,37,32,"<",Action::PrevMission);button(523,518,37,32,">",Action::NextMission);
    button(64,669,330,62,"DEPLOY HERC    >",Action::Launch,true);button(408,669,170,62,"HANGAR",Action::Hangar);
    text(65,750,"ENTER  Deploy     H  Hangar     F1  Field manual",11,muted);
    line(1000,626,1090,680,{0.68f,0.76f,0.66f,0.65f});line(1090,680,1345,680,{0.68f,0.76f,0.66f,0.65f});
    text(1105,693,"HERC / 0"+std::to_string(ui.chassis+1),11,mint,false,1);
    text(1103,715,chassis[ui.chassis].name,31,cream,true);text(1105,756,chassis[ui.chassis].role,10,cream);
    text(64,825,"01",26,orange,true);text(108,824,"PILOT THE MACHINE",13,cream,true);text(108,847,"Three HERC chassis. Your call.",11,muted);
    text(485,825,"02",26,orange,true);text(529,824,"BREAK THE LINE",13,cream,true);text(529,847,"Lasers, cannons, guided missiles.",11,muted);
    button(1010,824,190,40,"FIELD MANUAL",Action::Controls);button(1218,824,174,40,"EXIT",Action::Quit);
}
void Renderer::hangar(const UIState& ui) {
    glBegin(GL_QUADS);color({0.025f,0.045f,0.045f,0.98f});glVertex2f(0,0);glVertex2f(0,900);color({0.025f,0.045f,0.045f,0});glVertex2f(1010,900);glVertex2f(1010,0);glEnd();topbar(ui);
    text(64,126,"// RESISTANCE MOTOR POOL",12,orange,false,1);
    text(61,161,"MAKE IT YOURS.",51,cream,true);
    text(64,230,"Select a chassis. Upgrade. Survive.",14,muted);
    button(64,288,42,44,"<",Action::PrevChassis);text(127,283,chassis[ui.chassis].name,35,cream,true);button(503,288,42,44,">",Action::NextChassis);
    text(128,332,chassis[ui.chassis].role,12,mint);
    const auto& c=chassis[ui.chassis];
    const char* labels[]={"ARMOR", "SHIELDS", "TOP SPEED", "MISSILES"};float values[]={c.armor,c.shield+ui.shieldUpgrade*20,c.speed*3.6f,float(c.missiles)},maximum[]={170,190,110,14};
    for(int i=0;i<4;++i){float y=386+i*42.0f;text(64,y,labels[i],11,muted);bar(182,y+4,271,9,values[i]/maximum[i],i==1?mint:cream);text(474,y-3,num(values[i]),17,cream);}
    line(64,568,545,568,border);text(64,590,"SALVAGE RESERVE",11,muted);text(400,582,std::to_string(ui.bank)+" ALLOY",18,orange,true);
    int shieldCost=80+ui.shieldUpgrade*40,weaponCost=100+ui.weaponUpgrade*50;
    text(64,634,"SHIELD CAPACITOR  "+std::to_string(ui.shieldUpgrade)+"/3",12,cream);text(64,655,"+20 shield capacity / all HERCs",10,muted);
    button(384,627,161,43,ui.shieldUpgrade>=3?"MAX LEVEL":std::to_string(shieldCost)+" ALLOY  +",Action::ShieldUpgrade,false,ui.shieldUpgrade>=3||ui.bank<shieldCost);
    text(64,695,"WEAPON AMPLIFIER  "+std::to_string(ui.weaponUpgrade)+"/3",12,cream);text(64,716,"+12% weapon damage / all HERCs",10,muted);
    button(384,688,161,43,ui.weaponUpgrade>=3?"MAX LEVEL":std::to_string(weaponCost)+" ALLOY  +",Action::WeaponUpgrade,false,ui.weaponUpgrade>=3||ui.bank<weaponCost);
    button(64,783,294,59,"DEPLOY HERC    >",Action::Launch,true);button(374,783,171,59,"BACK",Action::Back);
    text(944,735,"READY FOR DEPLOYMENT",12,mint,false,1.2f);text(944,764,"LASER / AUTOCANNON / MISSILES",11,cream);
}
void Renderer::radar(const Combat& game,float x,float y,float size) {
    float cx=x+size/2,cy=y+size/2,r=size*0.45f;
    rect(x,y,size,size,{0.035f,0.075f,0.062f,0.97f});outline(x,y,size,size,border);
    for(int i=1;i<=3;++i)circle(cx,cy,r*i/3,{0.28f,0.47f,0.37f,0.35f});
    line(x+8,cy,x+size-8,cy,{0.28f,0.47f,0.37f,0.3f});line(cx,y+8,cx,y+size-8,{0.28f,0.47f,0.37f,0.3f});
    float angle=game.time*1.1f;line(cx,cy,cx+std::cos(angle)*r,cy+std::sin(angle)*r,{0.5f,0.9f,0.6f,0.45f});
    auto point=[&](Vec3 pos,Color c,float s){Vec3 d=pos-game.player.pos;float sine=std::sin(game.player.yaw),cosine=std::cos(game.player.yaw);float px=(d.x*cosine+d.z*sine)*r/250,py=(-d.x*sine+d.z*cosine)*r/250;float l=std::sqrt(px*px+py*py);if(l>r){px*=r/l;py*=r/l;}rect(cx+px-s/2,cy+py-s/2,s,s,c);};
    for(const auto& e:game.enemies)if(e.alive)point(e.pos,red,5);
    for(const auto& p:game.pickups)if(!p.collected)point(p.pos,mint,3);
    if(game.extractionOpen)point(game.extraction,mint,7);
    color(cream);glBegin(GL_TRIANGLES);glVertex2f(cx,cy-6);glVertex2f(cx-4,cy+4);glVertex2f(cx+4,cy+4);glEnd();
    text(x+7,y+5,"RADAR",9,mint);text(x+size-43,y+size-17,"250 M",8,muted);
}
void Renderer::hud(const Combat& game,const UIState& ui) {
    const auto& p=game.player;
    // Armored cockpit edges and lower instrument deck.
    color({0.035f,0.055f,0.052f,0.96f});glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(105,0);glVertex2f(52,670);glVertex2f(0,753);glVertex2f(1335,0);glVertex2f(1440,0);glVertex2f(1440,753);glVertex2f(1388,670);glEnd();
    line(105,0,52,670,{0.3f,0.34f,0.29f},4);line(1335,0,1388,670,{0.3f,0.34f,0.29f},4);
    rect(0,0,1440,60,panel);line(105,60,1335,60,border,2);
    text(28,20,"ES // RESISTANCE",15,cream,true);text(1116,22,"ESC  PAUSE    F1  HELP",11,muted);
    float heading=std::fmod((p.yaw+p.torso)*180/pi+3600,360.0f);
    for(int i=-4;i<=4;++i){float hx=720+i*64;line(hx,44,hx,52,mint);int h=int(heading+i*15+360)%360;text(hx-10,22,num(float(h)),10,muted);}
    line(720,52,720,67,orange,2);centered(720,75,num(heading)+" DEG",11,mint);
    text(115,100,"OP 0"+std::to_string(game.mission+1)+" / "+missions[game.mission].name,12,cream,true);
    text(115,124,game.extractionOpen?"REACH EXTRACTION":"ELIMINATE CYBRID FORCE",11,game.extractionOpen?mint:orange);
    text(115,149,std::to_string(game.kills)+" / "+std::to_string(missions[game.mission].enemies)+" TARGETS DESTROYED",10,muted);
    text(1192,101,"WAVE 0"+std::to_string(game.wave),12,cream);text(1192,124,num(game.time/60)+" MIN",10,muted);
    // Target brackets follow projected world coordinates.
    for(int i=0;i<int(game.enemies.size());++i) {
        const auto& e=game.enemies[i];if(!e.alive)continue;float x,y;
        if(project(e.pos+Vec3{0,7,0},x,y)&&y>85&&y<690){float distance=length(e.pos-p.pos),s=clamp(2600/distance,13,70);Color c=i==game.target?orange:Color{0.95f,0.59f,0.37f,0.7f};float l=s*0.32f;
            for(int a:{-1,1})for(int b:{-1,1}){line(x+a*s,y+b*s,x+a*(s-l),y+b*s,c,1.5f);line(x+a*s,y+b*s,x+a*s,y+b*(s-l),c,1.5f);}
            text(x-s,y-s-22,"CYBRID "+std::to_string(e.id+1),10,c);text(x+s+8,y-5,num(distance)+" M",10,c);
            if(i==game.target){bar(x-s,y+s+9,s*2,4,e.armor/e.maxArmor,orange);bar(x-s,y+s+17,s*2,3,e.shield/e.maxShield,mint);}
        }
    }
    if(game.extractionOpen){float x,y;if(project(game.extraction+Vec3{0,10,0},x,y)){outline(x-13,y-13,26,26,mint,2);centered(x,y-39,"EXTRACT / "+num(length(game.extraction-p.pos))+" M",12,mint);}}
    Color reticle=p.overheated?red:game.hitFlash>0?orange:mint;
    line(690,450,707,450,reticle,2);line(733,450,750,450,reticle,2);line(720,420,720,437,reticle,2);line(720,463,720,480,reticle,2);circle(720,450,3,reticle);
    if(game.hitFlash>0){line(708,438,713,443,orange,2);line(727,457,732,462,orange,2);line(708,462,713,457,orange,2);line(727,443,732,438,orange,2);}
    centered(720,497,p.overheated?"COOLING":p.weapon==0?"PULSE LASER":p.weapon==1?"35 MM AUTOCANNON":"GUIDED MISSILE",10,reticle);
    // Speed / heat ladder instruments at the edges of the windshield.
    for(int i=0;i<9;++i){float y=330+i*25.0f;line(121,y,133+(i%2==0?8:0),y,border);line(1299,y,1311+(i%2==0?8:0),y,border);}
    text(112,290,"KPH",10,muted);text(104,559,num(std::abs(p.speed)*3.6f),23,mint,true);
    text(1285,290,"HEAT",10,muted);text(1288,559,num(p.heat)+"%",20,p.heat>75?orange:mint,true);
    if(game.messageTime>0){rect(310,630,820,40,{0.025f,0.055f,0.05f,0.78f});centered(720,641,game.message,12,cream);}
    color({0.045f,0.065f,0.06f,1});glBegin(GL_POLYGON);glVertex2f(0,709);glVertex2f(250,691);glVertex2f(372,716);glVertex2f(1068,716);glVertex2f(1190,691);glVertex2f(1440,709);glVertex2f(1440,900);glVertex2f(0,900);glEnd();
    line(0,709,250,691,border,3);line(250,691,372,716,border,3);line(372,716,1068,716,border,3);line(1068,716,1190,691,border,3);line(1190,691,1440,709,border,3);
    radar(game,36,727,146);
    text(204,735,chassis[game.chassisIndex].name,19,cream,true);text(204,766,"ARMOR",10,muted);text(391,761,num(p.armor),15,cream);bar(204,788,222,8,p.armor/p.maxArmor,p.armor<p.maxArmor*0.3f?red:cream);
    text(204,812,"SHIELD",10,muted);text(391,807,num(p.shield),15,mint);bar(204,834,222,8,p.shield/p.maxShield,mint);
    text(204,857,"AUTO-REGEN / 5 SEC",9,muted);
    text(472,739,"WEAPON SYSTEMS",10,muted);
    const char* names[]={"PULSE LASER","AUTOCANNON","MISSILES"};
    for(int i=0;i<3;++i){float x=472+i*171;rect(x,765,155,81,p.weapon==i?Color{0.13f,0.19f,0.15f}:Color{0.06f,0.09f,0.08f});outline(x,765,155,81,p.weapon==i?mint:border);text(x+11,775,"0"+std::to_string(i+1),10,p.weapon==i?mint:muted);text(x+11,797,names[i],11,cream,true);text(x+11,822,i==2?std::to_string(p.missiles)+" ROUNDS":"UNLIMITED",9,muted);}
    text(474,859,"LMB / SPACE FIRE    RMB MISSILE    T TARGET",10,muted);
    text(1017,740,"REACTOR",10,muted);text(1153,734,num(p.heat)+"%",20,p.overheated?red:cream,true);bar(1017,774,193,9,p.heat/100,p.heat>75?orange:mint);
    text(1017,806,"SALVAGE",10,muted);text(1155,799,std::to_string(game.salvage),23,orange,true);text(1017,849,"Q / E  STRAFE",10,muted);text(1017,868,"SHIFT  BOOST",10,muted);
    text(1245,741,"TORSO",10,muted);circle(1285,805,35,border);line(1285,805,1285+std::sin(p.torso)*31,805-std::cos(p.torso)*31,mint,2);text(1245,856,"C  CENTER",9,muted);
    if(game.damageFlash>0){rect(0,0,1440,900,{0.85f,0.12f,0.03f,game.damageFlash*0.17f});outline(1,1,1438,898,{1,0.25f,0.1f,game.damageFlash},9);}
    if(ui.muted)text(1374,870,"MUTE",9,muted);
}
void Renderer::controls(const UIState&) {
    rect(0,0,1440,900,{0.018f,0.035f,0.032f,0.94f});text(200,96,"// PILOT FIELD MANUAL",12,orange,false,1);text(196,133,"KNOW YOUR MACHINE.",47,cream,true);
    const char* keys[]={"W / S", "A / D", "Q / E", "MOUSE", "C", "SHIFT", "LMB / SPACE", "RMB", "1 / 2 / 3", "T / TAB", "ESC", "M / F11"};
    const char* actions[]={"Forward / reverse", "Turn chassis", "Strafe left / right", "Aim torso and weapons", "Center torso and aim", "Boost (generates heat)", "Fire selected weapon", "Launch guided missile", "Select weapon", "Cycle hostile target", "Pause / release mouse", "Mute audio / fullscreen"};
    for(int i=0;i<12;++i){float x=i<6?200:760,y=235+(i%6)*52.0f;text(x,y,keys[i],14,mint);text(x+160,y,actions[i],13,cream);line(x,y+35,x+475,y+35,border);}
    text(200,588,"FIGHT SMART. COME HOME.",19,cream,true);
    text(200,628,"Shields recharge after 5 seconds without a hit. Overheated weapons must cool.",13,muted);
    text(200,658,"Aim for legs to preserve salvage. Walk near green crates for alloy and repairs.",13,muted);
    text(200,688,"Clear all waves, then reach the green extraction beacon to bank your salvage.",13,muted);
    text(200,718,"Guided missiles lock on enemies in front of you. Keep your target in view.",13,muted);
    button(200,788,260,57,"BACK TO OPERATIONS",Action::Back,true);
}
void Renderer::pause(const UIState&) {
    rect(0,0,1440,900,{0.015f,0.03f,0.028f,0.86f});text(505,209,"// SYSTEMS ON STANDBY",12,orange,false,1);text(502,250,"HOLD POSITION.",47,cream,true);text(506,317,"Simulation paused. Your HERC is safe.",13,muted);
    button(505,382,430,58,"RESUME MISSION",Action::Resume,true);button(505,456,430,52,"FIELD MANUAL",Action::Controls);button(505,524,430,52,"RESTART MISSION",Action::Retry);button(505,592,430,52,"RETURN TO HANGAR",Action::Hangar);button(505,660,430,52,"EXIT TO DESKTOP",Action::Quit);
}
void Renderer::debrief(const Combat& game,const UIState& ui) {
    bool won=game.phase==Phase::Won;rect(0,0,1440,900,{0.02f,0.04f,0.035f,0.91f});
    text(400,161,won?"// EXTRACTION CONFIRMED":"// HERC SIGNAL LOST",12,won?mint:orange,false,1);text(395,205,won?"WELCOME HOME.":"NOT THE END.",61,cream,true);
    text(400,289,won?"One more piece of Earth belongs to us.":"The resistance needs you. Refit and try again.",15,muted);
    line(400,345,1040,345,border);
    const char* labels[]={"OPERATION", "HOSTILES DESTROYED", "MISSION TIME", "SALVAGE BANKED", "TOTAL RESERVE"};
    std::string values[]={missions[game.mission].name,std::to_string(game.kills)+" / "+std::to_string(missions[game.mission].enemies),num(game.time/60,1)+" MIN",won?std::to_string(game.salvage)+" ALLOY":"0 ALLOY",std::to_string(ui.bank)+" ALLOY"};
    for(int i=0;i<5;++i){float y=378+i*47.0f;text(400,y,labels[i],12,muted);text(774,y,values[i],14,i>=3?orange:cream,true);}
    button(400,660,306,61,won?"NEXT OPERATION   >":"RETRY MISSION   >",won?Action::Next:Action::Retry,true);button(726,660,314,61,"RETURN TO HANGAR",Action::Hangar);button(400,744,640,44,"EXIT TO DESKTOP",Action::Quit);
}
void Renderer::render(int width,int height,const Combat& game,const UIState& ui) {
    pixelWidth=std::max(1,width);pixelHeight=std::max(1,height);state=&ui;buttons.clear();glViewport(0,0,pixelWidth,pixelHeight);
    glClearColor(0.06f,0.1f,0.09f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    sky(ui.time);
    bool inMenu=ui.screen==Screen::Menu||ui.screen==Screen::Hangar||(ui.screen==Screen::Controls&&(ui.returnScreen==Screen::Menu||ui.returnScreen==Screen::Hangar));
    world(game,inMenu,ui.time);begin2D();
    if(ui.screen==Screen::Menu)menu(ui);
    else if(ui.screen==Screen::Hangar)hangar(ui);
    else if(ui.screen==Screen::Controls)controls(ui);
    else {hud(game,ui);if(ui.screen==Screen::Paused)pause(ui);else if(ui.screen==Screen::Debrief)debrief(game,ui);}
    if(ui.noticeTime>0){rect(380,100,680,36,panel);centered(720,109,ui.notice,12,mint);}
}
bool Renderer::screenshot(const std::string& path,int width,int height) {
    std::vector<unsigned char> pixels(size_t(width)*height*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream out(path,std::ios::binary);if(!out)return false;out<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;--y)out.write(reinterpret_cast<char*>(pixels.data()+size_t(y)*width*3),width*3);
    return bool(out);
}
}
