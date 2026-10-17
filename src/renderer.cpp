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
void Renderer::destroy() {destroyOriginalModels();if(originalCockpitTexture)glDeleteTextures(1,&originalCockpitTexture);if(display.texture)glDeleteTextures(1,&display.texture);if(mono.texture)glDeleteTextures(1,&mono.texture);if(groundTexture)glDeleteTextures(1,&groundTexture);if(terrainList)glDeleteLists(terrainList,1);if(sceneryList)glDeleteLists(sceneryList,1);}
void Renderer::begin2D(float width,float height) {
    uiWidth=width;uiHeight=height;
    GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
    strokeScale=width==640?float(viewport[3])/height:1;
    glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,width,height,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
}
void Renderer::rect(float x,float y,float w,float h,Color c) {color(c);glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();}
void Renderer::outline(float x,float y,float w,float h,Color c,float thickness) {glLineWidth(std::max(1.0f,thickness*strokeScale));color(c);glBegin(GL_LINE_LOOP);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();glLineWidth(1);}
void Renderer::line(float x,float y,float xx,float yy,Color c,float thickness) {glLineWidth(std::max(1.0f,thickness*strokeScale));color(c);glBegin(GL_LINES);glVertex2f(x,y);glVertex2f(xx,yy);glEnd();glLineWidth(1);}
void Renderer::circle(float x,float y,float radius,Color c,bool fill,int segments) {glLineWidth(std::max(1.0f,strokeScale));color(c);glBegin(fill?GL_TRIANGLE_FAN:GL_LINE_LOOP);if(fill)glVertex2f(x,y);for(int i=0;i<=segments;++i){float a=float(i)/segments*pi*2;glVertex2f(x+std::cos(a)*radius,y+std::sin(a)*radius);}glEnd();glLineWidth(1);}
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
void Renderer::sky(float) {
    begin2D();glBegin(GL_QUADS);color({0.57f,0.025f,0.012f});glVertex2f(0,0);glVertex2f(1440,0);color({0.80f,0.065f,0.015f});glVertex2f(1440,650);glVertex2f(0,650);glEnd();
    rect(0,650,1440,250,{0.80f,0.065f,0.015f});
}
void Renderer::camera(Vec3 eye,Vec3 center,float fov,float roll) {
    glMatrixMode(GL_PROJECTION);glLoadIdentity();
    double near=0.3,top=near*std::tan(fov*pi/360.0);double aspect=viewAspect;
    glFrustum(-top*aspect,top*aspect,-top,top,near,3600);
    glGetFloatv(GL_PROJECTION_MATRIX,projection);
    Vec3 forward=normalized(center-eye),side=normalized(cross(forward,{0,1,0})),up=cross(side,forward);
    Vec3 rolled=side*std::cos(roll)+up*std::sin(roll);up=up*std::cos(roll)-side*std::sin(roll);side=rolled;
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
    auto point=[&](int i,int j){float u=i/6.0f,v=j/6.0f,r=std::hypot(u,v);
        float ridge=std::pow(std::max(0.0f,1-r),0.72f)*(0.78f+0.18f*std::sin(u*11+seed)+0.14f*std::cos(v*13-seed));
        float px=x+u*radius,pz=z+v*radius;return Vec3{px,terrainHeight(px,pz)+ridge*height,pz};};
    auto tri=[&](Vec3 p,Vec3 q,Vec3 r){Vec3 n=normalized(cross(q-p,r-p));glNormal3f(n.x,n.y,n.z);for(Vec3 v:{p,q,r}){glTexCoord2f(v.x*0.055f,v.z*0.055f);vertex(v);}};
    color(c);glBegin(GL_TRIANGLES);
    for(int i=-6;i<6;++i)for(int j=-6;j<6;++j){Vec3 a=point(i,j),b=point(i+1,j),c=point(i+1,j+1),d=point(i,j+1);tri(a,d,b);tri(b,d,c);}
    glEnd();
}
void Renderer::makeTerrain() {
    std::mt19937 rng{2624};auto random=[&](float low,float high){return std::uniform_real_distribution<float>(low,high)(rng);};
    // Seamless cellular basalt and incandescent fissures, generated locally.
    std::vector<unsigned char> texture(256*256*3);
    auto hash=[](int x,int y){uint32_t n=uint32_t((x+80)%8)*73856093u^uint32_t((y+80)%8)*19349663u;n^=n>>13;n*=1274126177u;return float(n&65535)/65535;};
    for(int y=0;y<256;++y)for(int x=0;x<256;++x){
        float px=x/32.0f,py=y/32.0f,near=100,next=100;
        for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i){int cx=int(px)+i,cy=int(py)+j;float dx=cx+hash(cx,cy)-px,dy=cy+hash(cy,cx)-py,d=dx*dx+dy*dy;if(d<near){next=near;near=d;}else if(d<next)next=d;}
        float crack=next-near,grain=random(0.60f,1.20f),rock=(0.22f+std::sqrt(near)*0.17f)*grain;
        bool lava=crack<0.027f&&hash(int(px),int(py))>0.42f;
        float glow=lava?clamp(1-crack/0.03f,0,1):0;
        Color c=lava?Color{0.72f+glow*0.28f,0.08f+glow*0.28f,0.012f}:Color{rock*0.77f,rock*0.90f,rock};
        size_t at=size_t(y*256+x)*3;texture[at]=uint8_t(c.r*255);texture[at+1]=uint8_t(c.g*255);texture[at+2]=uint8_t(c.b*255);
    }
    glGenTextures(1,&groundTexture);glBindTexture(GL_TEXTURE_2D,groundTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_GENERATE_MIPMAP,GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,256,256,0,GL_RGB,GL_UNSIGNED_BYTE,texture.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    terrainList=glGenLists(1);glNewList(terrainList,GL_COMPILE);glBegin(GL_TRIANGLES);
    for(int x=-1680;x<1680;x+=24)for(int z=-1680;z<1680;z+=24) {
        Vec3 a{float(x),terrainHeight(float(x),float(z)),float(z)},b{float(x+24),terrainHeight(float(x+24),float(z)),float(z)},c{float(x+24),terrainHeight(float(x+24),float(z+24)),float(z+24)},d{float(x),terrainHeight(float(x),float(z+24)),float(z+24)};
        float shade=random(0.87f,1.07f);Color base{shade,shade,shade};
        auto tri=[&](Vec3 p,Vec3 q,Vec3 r){Vec3 n=normalized(cross(q-p,r-p));color(base);glNormal3f(n.x,n.y,n.z);for(Vec3 v:{p,q,r}){glTexCoord2f(v.x*0.055f,v.z*0.055f);vertex(v);}};tri(a,d,b);tri(b,d,c);
    }
    glEnd();glEndList();
    sceneryList=glGenLists(1);glNewList(sceneryList,GL_COMPILE);
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,groundTexture);
    for(int i=0;i<55;++i){float a=float(i)*2*pi/55;mountain(std::cos(a)*random(1850,2100),std::sin(a)*random(1850,2100),random(180,320),random(100,220),{0.78f,0.83f,0.90f},i);}
    glDisable(GL_TEXTURE_2D);
    for(int i=0;i<150;++i){float x=random(-1350,1350),z=random(-1350,1350);if(std::abs(x)<85&&z>-170&&z<150)continue;float s=random(1,5);glPushMatrix();glTranslatef(x,terrainHeight(x,z),z);glRotatef(random(0,360),0,1,0);glRotatef(random(-15,15),0,0,1);box({0,s*0.3f,0},{s*1.8f,s,s*1.3f},{0.30f,0.29f,0.24f});glPopMatrix();}
    // A ruined relay outpost on the western edge of the battlefield.
    for(int i=0;i<5;++i){float x=-130+i*20.0f,z=-140-(i%2)*38.0f,y=terrainHeight(x,z),h=9+(i%3)*5.0f;box({x,y+h/2,z},{14,h,19},{0.28f,0.30f,0.27f});box({x,y+h,z},{15,1,20},{0.18f,0.22f,0.21f});for(int j=0;j<4;++j)box({x-5+j*3.2f,y+h*0.65f,z+9.56f},{1.6f,1.3f,0.1f},{0.10f,0.16f,0.15f});}
    cylinder({-105,terrainHeight(-105,-220),-220},5,20,{0.25f,0.29f,0.26f});
    cylinder({-105,terrainHeight(-105,-220)+20,-220},0.7f,28,{0.25f,0.28f,0.25f});
    box({-105,terrainHeight(-105,-220)+38,-220},{18,0.6f,0.8f},{0.23f,0.28f,0.25f});box({-105,terrainHeight(-105,-220)+43,-220},{12,0.6f,0.8f},{0.23f,0.28f,0.25f});
    for(int i=0;i<9;++i){float x=780+i*13.0f;box({x,terrainHeight(x,-185)+1,-185},{9,2.6f,5},{0.33f,0.32f,0.25f});}
    for(size_t i=0;i<landmarks.size();++i)if(i!=1) {
        auto p=landmarks[i].pos;float y=terrainHeight(p.x,p.z),h=landmarks[i].height;
        cylinder({p.x,y,p.z},5,20,{0.25f,0.29f,0.26f});
        cylinder({p.x,y+20,p.z},0.7f,h-20,{0.25f,0.28f,0.25f});
        box({p.x,y+h-7,p.z},{18,0.6f,0.8f},{0.23f,0.28f,0.25f});
        box({p.x,y+h-2,p.z},{12,0.6f,0.8f},{0.23f,0.28f,0.25f});
        box({p.x,y+h,p.z},{1.6f,1.6f,1.6f},{0.8f,0.26f,0.08f});
        for(int j=0;j<3;++j){float x=p.x+22+j*22,z=p.z+20,base=terrainHeight(x,z);box({x,base+6,z},{16,12,20},{0.28f,0.30f,0.27f});}
    }
    glEndList();
}
void Renderer::shadow(Vec3 p,float radius) {
    glDisable(GL_LIGHTING);color({0.025f,0.04f,0.035f,0.3f});glBegin(GL_TRIANGLE_FAN);glVertex3f(p.x,terrainHeight(p.x,p.z)+0.08f,p.z);for(int i=0;i<=24;++i){float a=i*2*pi/24,x=p.x+std::cos(a)*radius,z=p.z+std::sin(a)*radius*0.65f;glVertex3f(x,terrainHeight(x,z)+0.08f,z);}glEnd();glEnable(GL_LIGHTING);
}
void Renderer::strut(Vec3 a,Vec3 b,float width,float depth,Color c) {
    Vec3 delta=b-a,mid=(a+b)*0.5f;float distance=length(delta);if(distance<0.01f)return;
    glPushMatrix();glTranslatef(mid.x,mid.y,mid.z);
    Vec3 axis=cross({0,1,0},delta);
    if(length(axis)>0.001f)glRotatef(std::acos(clamp(delta.y/distance,-1,1))*180/pi,axis.x,axis.y,axis.z);
    box({0,0,0},{width,distance,depth},c);glPopMatrix();
}
void Renderer::mech(Vec3 pos,float yaw,const Gait& gait,int type,bool friendly,float damage,float death,HercAppearance appearance,bool flat,float torso,float pitch) {
    shadow(pos,9);glPushMatrix();glTranslatef(pos.x,pos.y,pos.z);glRotatef(-yaw*180/pi,0,1,0);
    float size=type==1?1.12f:type==2?0.88f:1.0f;glScalef(size,size,size);
    if(death>0){glTranslatef(0,-std::min(death,1.0f)*4.3f,0);glRotatef(std::min(death,1.0f)*68,1,0,0);}
    if(appearance==HercAppearance::Automatic)appearance=friendly?chassis[type].appearance:type==0?HercAppearance::Achilles:HercAppearance::Prototype;
    if(originalMech(gait,appearance,torso,pitch,pos,yaw,size,flat)){glPopMatrix();return;}
    Color armor=friendly?Color{0.35f,0.40f,0.34f}:Color{0.38f,0.32f,0.28f};
    Color edge=friendly?Color{0.49f,0.53f,0.43f}:Color{0.46f,0.40f,0.33f};
    Color dark{0.12f,0.16f,0.15f},steel{0.33f,0.37f,0.34f},accent=friendly?orange:Color{0.68f,0.19f,0.12f};
    if(damage>0)armor={0.95f,0.72f,0.43f};
    for(int side:{-1,1}) {
        Vec3 foot=gait.foot(side).offset;
        float wx=pos.x+(foot.x*std::cos(yaw)-foot.z*std::sin(yaw))*size;
        float wz=pos.z+(foot.x*std::sin(yaw)+foot.z*std::cos(yaw))*size;
        foot.y+=(flat?0:terrainHeight(wx,wz)-pos.y)/size;
        Vec3 ankle=foot+Vec3{0,1.15f,0},hip{side*2.25f,6.5f+gait.bodyLift(),0};
        Vec3 delta=ankle-hip;
        float bend=std::sqrt(std::max(0.05f,3.5f*3.5f-dot(delta,delta)*0.25f));
        Vec3 knee=(ankle+hip)*0.5f+normalized(cross({1,0,0},delta))*bend;
        strut(hip,knee,1.9f,2.0f,armor);strut(knee,ankle,1.8f,1.9f,edge);
        box(knee,{2.6f,1.5f,2.7f},armor);box(knee+Vec3{0,0,-1.38f},{1.9f,0.38f,0.1f},accent);
        strut(knee+Vec3{side*1.08f,0,0.6f},ankle+Vec3{side*1.08f,0,0.6f},0.22f,0.22f,steel);
        box(foot+Vec3{0,0.55f,-0.8f},{2.8f,1.1f,4.0f},dark);box(foot+Vec3{0,1.2f,-1.2f},{2.5f,0.6f,2.8f},armor);
        for(int j=0;j<3;++j)box(foot+Vec3{-0.8f+j*0.8f,1.1f,-2.83f},{0.4f,0.2f,0.08f},steel);
    }
    cylinder({0,5.3f,0},2.0f,1.8f,dark);
    glPushMatrix();glTranslatef(0,gait.bodyLift(),0);
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
    else {Vec3 eye=game.cameraEye();camera(eye,eye+game.cameraAim(),65,game.cameraRoll());}
    glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    const float ambient[]={0.40f,0.43f,0.50f,1},sun[]={0.90f,0.86f,0.85f,1},position[]={-0.4f,0.85f,0.6f,0};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightfv(GL_LIGHT0,GL_DIFFUSE,sun);glLightfv(GL_LIGHT0,GL_POSITION,position);
    glEnable(GL_FOG);const float fog[]={0.24f,0.20f,0.22f,1};glFogfv(GL_FOG_COLOR,fog);glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,350);glFogf(GL_FOG_END,2300);
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,groundTexture);
    if(!menuMode&&missionLayouts[game.mission].flat) {
        // Same ground material; the test range has no relief or scenery obstacles.
        color({1,1,1});glNormal3f(0,1,0);glBegin(GL_QUADS);
        glTexCoord2f(-60,-60);glVertex3f(-1680,0,-1680);glTexCoord2f(-60,60);glVertex3f(-1680,0,1680);
        glTexCoord2f(60,60);glVertex3f(1680,0,1680);glTexCoord2f(60,-60);glVertex3f(1680,0,-1680);
        glEnd();glDisable(GL_TEXTURE_2D);
    } else {glCallList(terrainList);glDisable(GL_TEXTURE_2D);glCallList(sceneryList);}
    if(menuMode) {Vec3 pos{11,terrainHeight(11,0),0};mech(pos,pi+0.65f+std::sin(time*0.12f)*0.12f,Gait{},state->chassis,true);}
    else {
        for(const auto& e:game.enemies) mech(e.pos,e.yaw,e.gait,e.type,false,e.flash,e.deathTime,e.appearance,missionLayouts[game.mission].flat);
        glDisable(GL_LIGHTING);
        for(const auto& p:game.pickups) if(!p.collected) {
            glPushMatrix();glTranslatef(p.pos.x,p.pos.y+2.0f+std::sin(time*2)*0.5f,p.pos.z);glRotatef(time*35,0,1,0);box({0,0,0},{1.3f,1.3f,1.3f},mint);glPopMatrix();
        }
        glDepthMask(GL_FALSE);
        if(game.extractionOpen||game.navigationOnly()) {
            auto point=game.navigationPoint();
            color({0.25f,0.9f,0.58f,0.12f});glBegin(GL_QUAD_STRIP);for(int i=0;i<=48;++i){float a=i*2*pi/48;glVertex3f(point.x+std::cos(a)*22,point.y+0.1f,point.z+std::sin(a)*22);glVertex3f(point.x+std::cos(a)*22,point.y+30,point.z+std::sin(a)*22);}glEnd();
            glLineWidth(2);color(mint);glBegin(GL_LINE_LOOP);for(int i=0;i<48;++i){float a=i*2*pi/48;glVertex3f(point.x+std::cos(a)*22,point.y+0.2f,point.z+std::sin(a)*22);}glEnd();
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
    x=(clip[0]/clip[3]*0.5f+0.5f)*uiWidth;y=(-clip[1]/clip[3]*0.5f+0.5f)*uiHeight;
    return x>0&&x<uiWidth&&y>0&&y<uiHeight;
}
void Renderer::topbar(const UIState& ui) {
    line(48,92,1392,92,{0.62f,0.66f,0.59f,0.22f});
    line(52,38,66,65,orange,4);line(66,65,80,38,orange,4);line(58,52,74,52,orange,3);
    text(99,31,"OPENEARTHSIEGE 2",22,cream,true,1);text(100,59,"COMPATIBILITY PROTOTYPE",10,muted,false,1.3f);
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
    text(84,518,"OPERATION 0"+std::to_string(ui.mission+1)+" / 0"+std::to_string(missionCount),11,orange,false,1);
    text(84,545,missions[ui.mission].name,28,cream,true);
    bool route=missionLayouts[ui.mission].routeWaypoints>0;
    text(84,585,missions[ui.mission].region+std::string("  /  ")+(route?"5 WAYPOINTS":std::to_string(missions[ui.mission].enemies)+" HOSTILES"),11,muted);
    text(84,610,route?"FOLLOW WP1 THROUGH WP5. NO COMBAT.":"ELIMINATE. SALVAGE. EXTRACT.",11,mint);
    button(474,518,37,32,"<",Action::PrevMission);button(523,518,37,32,">",Action::NextMission);
    button(64,669,330,62,"DEPLOY HERC    >",Action::Launch,true);button(408,669,170,62,"HANGAR",Action::Hangar);
    text(65,750,"ENTER  Deploy   H  Hangar   F1  Manual   F2  Audio",11,muted);
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
    text(944,735,num(chassis[ui.chassis].mass)+" TONNES // READY",12,mint,false,1.2f);text(944,764,"LASER / AUTOCANNON / MISSILES",11,cream);
}
void Renderer::radar(const Combat& game,float x,float y,float size) {
    constexpr Color cyan{0.13f,0.77f,0.95f},green{0.18f,0.95f,0.12f},dim{0.25f,0.6f,0.65f,0.45f};
    float scale=size/90,r=size*0.40f,cx=x+size/2,cy=y+size/2;
    // An open windshield sensor display, with a faint range ring rather than a new console.
    circle(cx,cy,r,dim,false,48);circle(cx,cy,r/2,{dim.r,dim.g,dim.b,0.18f},false,32);
    auto point=[&](Vec3 p){return radarPoint(p,game.player,state->radarMeters);};
    auto diamond=[&](RadarPoint p,Color c,float radius){float px=cx+p.x*r,py=cy+p.y*r;
        line(px,py-radius,px+radius,py,c);line(px+radius,py,px,py+radius,c);
        line(px,py+radius,px-radius,py,c);line(px-radius,py,px,py-radius,c);};
    if(!game.navigationOnly())for(const auto& landmark:landmarks){auto p=point(landmark.pos);if(p.inRange)outline(cx+p.x*r-1.5f*scale,cy+p.y*r-1.5f*scale,3*scale,3*scale,dim);}
    if(!game.navigationOnly()){auto exit=point(game.extraction);diamond(exit,game.extractionOpen?green:dim,3*scale);}
    if(!game.extractionOpen)diamond(point(game.navigationPoint()),{0.98f,0.68f,0.13f},3.7f*scale);
    for(size_t i=0;i<game.enemies.size();++i)if(game.enemies[i].alive) {
        auto p=point(game.enemies[i].pos);if(!p.inRange)continue;
        float px=cx+p.x*r,py=cy+p.y*r;
        circle(px,py,2.4f*scale,{0.02f,0.05f,0.07f},true,8);
        circle(px,py,1.4f*scale,cyan,true,8);
        if(int(i)==game.target)outline(px-3*scale,py-3*scale,6*scale,6*scale,green);
    }
    // Forward is always up; the thin ray is the independent torso / weapon heading.
    line(cx,cy,cx+std::sin(game.player.torso)*10*scale,cy-std::cos(game.player.torso)*10*scale,green);
    color({0.93f,0.94f,0.90f});glBegin(GL_TRIANGLES);
    glVertex2f(cx,cy-3*scale);glVertex2f(cx-2.5f*scale,cy+2*scale);glVertex2f(cx+2.5f*scale,cy+2*scale);glEnd();
    float north=game.player.yaw;
    centered(cx-std::sin(north)*(r+4*scale),cy-std::cos(north)*(r+4*scale)-3*scale,"N",6*scale,cyan);
    centered(cx,y+size-3*scale,num(state->radarMeters)+" M",6.5f*scale,cyan);
}
void Renderer::marquee(const Combat& game,float x,float y,float width,float size) {
    if(game.statusMessages.empty())return;
    const auto& message=game.statusMessages.front();
    while(size>6&&textWidth(message.text,size)>width-8)size-=0.25f;
    float w=textWidth(message.text,size),at=x+(width-w)/2,t=message.elapsed;
    if(t<0.6f)at+=(width+w)/2*(1-t/0.6f);
    if(t>5.6f)at-=(width+w)/2*((t-5.6f)/0.6f);
    GLint viewport[4],previous[4];glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_SCISSOR_BOX,previous);
    bool clipped=glIsEnabled(GL_SCISSOR_TEST);glEnable(GL_SCISSOR_TEST);
    glScissor(viewport[0]+int(x/uiWidth*viewport[2]),viewport[1]+int((uiHeight-y-size-3)/uiHeight*viewport[3]),
              int(width/uiWidth*viewport[2]),int((size+4)/uiHeight*viewport[3]));
    text(at,y,message.text,size,{0.13f,0.77f,0.95f});
    glScissor(previous[0],previous[1],previous[2],previous[3]);if(!clipped)glDisable(GL_SCISSOR_TEST);
}
void Renderer::bevel(float x,float y,float w,float h,float rim,Color face) {
    rect(x,y,w,h,{0.055f,0.045f,0.05f});
    color({0.53f,0.50f,0.54f});glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w-rim,y+rim);glVertex2f(x+rim,y+rim);glEnd();
    color({0.38f,0.35f,0.37f});glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+rim,y+rim);glVertex2f(x+rim,y+h-rim);glVertex2f(x,y+h);glEnd();
    color({0.15f,0.12f,0.14f});glBegin(GL_QUADS);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x+w-rim,y+h-rim);glVertex2f(x+w-rim,y+rim);glEnd();
    rect(x+rim,y+rim,w-rim*2,h-rim*2,face);
    line(x+rim,y+rim,x+w-rim,y+rim,{0.68f,0.65f,0.72f},2);
    line(x+rim,y+rim,x+rim,y+h-rim,{0.51f,0.49f,0.54f},2);
}
void Renderer::schematic(float x,float y,float scale,float armor,float legs,float stroke) {
    auto part=[&](float px,float py,float w,float h,float health){Color c=health>0.3f?Color{0.15f,0.9f,0.12f}:red;outline(x+px*scale,y+py*scale,w*scale,h*scale,c,stroke);outline(x+(px+3)*scale,y+(py+3)*scale,(w-6)*scale,(h-6)*scale,c,stroke*0.5f);};
    part(34,0,24,20,armor);part(23,23,46,37,armor);part(0,19,19,37,armor);part(73,19,19,37,armor);
    part(29,63,15,30,legs);part(49,63,15,30,legs);part(27,96,17,34,legs);part(49,96,17,34,legs);
    line(x+19*scale,y+32*scale,x+23*scale,y+32*scale,{0.2f,1,0.1f},stroke);line(x+69*scale,y+32*scale,x+73*scale,y+32*scale,{0.2f,1,0.1f},stroke);
}
void Renderer::hud(const Combat& game,const UIState& ui) {
    const auto& p=game.player;
    constexpr Color cyan{0.13f,0.77f,0.95f},green{0.18f,0.95f,0.12f},lcd{0.025f,0.03f,0.028f},label{0.67f,0.72f,0.62f},purple{0.41f,0.42f,0.75f};
    // Structural canopy rails: broad metal faces, recessed channels and lit seals.
    for(int side:{-1,1}) {
        auto point=[&](float x,float y){glVertex2f(side<0?x:1440-x,y);};
        const float top[]={0,24,49,79,105,119,128},bottom[]={213,238,266,296,322,334,341};
        const Color tones[]={{0.08f,0.055f,0.07f},{0.25f,0.21f,0.24f},{0.39f,0.40f,0.42f},{0.23f,0.22f,0.25f},{0.43f,0.39f,0.44f},{0.66f,0.66f,0.91f}};
        for(int i=0;i<6;++i){color(tones[i]);glBegin(GL_QUADS);point(top[i],0);point(top[i+1],0);point(bottom[i+1],588);point(bottom[i],596);glEnd();}
    }
    float heading=std::fmod((p.yaw+p.torso)*180/pi+3600,360.0f);
    rect(502,13,436,7,{0.03f,0.05f,0.04f});rect(647,14,146,5,{0.08f,0.38f,0.16f});line(720+p.torso*60,6,720+p.torso*60,29,cyan,3);
    for(int i=-20;i<=20;++i){float x=720+i*13;rect(x,55,3,i%5==0?8:3,cyan);if(i%10==0)centered(x,69,num(std::fmod(heading+i+360,360)),18,cyan);}
    std::string objective=(game.extractionOpen?"EXTRACTION: ":"WAYPOINT "+std::to_string(game.wave)+": ")+num(length(game.navigationPoint()-p.pos))+" M.";
    centered(720,100,objective,16,cyan);
    radar(game,245,160,170);
    marquee(game,410,126,620,16);
    for(int i=0;i<int(game.enemies.size());++i) {
        const auto& e=game.enemies[i];if(!e.alive)continue;float x,y;
        if(project(e.pos+Vec3{0,7,0},x,y)&&y<570&&y>130){float s=clamp(2100/length(e.pos-p.pos),12,45);Color c=game.target==i?green:Color{0.14f,0.74f,0.13f,0.65f};outline(x-s,y-s,s*2,s*2,c,1.5f);if(game.target==i){line(x-s-10,y,x+s+10,y,c);line(x,y-s-10,x,y+s+10,c);}}
    }
    float rx=720,ry=450;project(game.eye()+game.aim()*300,rx,ry);
    circle(rx,ry,23,game.hitFlash>0?green:cream,false,32);
    line(rx-36,ry,rx-17,ry,cream,2);line(rx+17,ry,rx+36,ry,cream,2);line(rx,ry-36,rx,ry-17,cream,2);line(rx,ry+17,rx,ry+36,cream,2);
    if(game.extractionOpen||game.navigationOnly()){float x,y;if(project(game.navigationPoint()+Vec3{0,9,0},x,y)){outline(x-13,y-13,26,26,green,2);centered(x,y-35,game.extractionOpen?"EXTRACT":"WP "+std::to_string(game.wave),14,green);}}
    text(369,555,"SPEED:",16,label);text(446,555,num(std::hypot(p.speed,p.sideSpeed)*3.6f)+" KPH",16,cyan);
    int seconds=int(game.time);text(904,555,"TIME:",16,label);text(965,555,(seconds/60<10?"0":"")+std::to_string(seconds/60)+":"+(seconds%60<10?"0":"")+std::to_string(seconds%60),16,cyan);
    // Contact impulses move only the close console by a fraction of a pixel.
    glPushMatrix();glTranslatef(p.gait.sway*8,p.gait.bounce*10,0);
    color({0.16f,0.13f,0.15f});glBegin(GL_POLYGON);glVertex2f(0,900);glVertex2f(58,633);glVertex2f(327,588);glVertex2f(623,588);glVertex2f(646,607);glVertex2f(796,607);glVertex2f(818,588);glVertex2f(1113,588);glVertex2f(1384,633);glVertex2f(1440,900);glEnd();
    line(334,588,623,588,{0.68f,0.67f,0.94f},5);line(623,588,646,607,{0.68f,0.67f,0.94f},5);line(646,607,796,607,{0.68f,0.67f,0.94f},5);line(796,607,818,588,{0.68f,0.67f,0.94f},5);line(818,588,1106,588,{0.68f,0.67f,0.94f},5);
    bevel(391,619,658,281,18,{0.18f,0.16f,0.18f});bevel(45,645,352,255,15,{0.18f,0.15f,0.17f});bevel(1048,645,347,255,15,{0.18f,0.15f,0.17f});
    bevel(433,598,180,29,7,{0.61f,0.61f,0.90f});bevel(827,598,180,29,7,{0.61f,0.61f,0.90f});
    bevel(69,668,305,180,6,lcd);
    const char* weapons[]={"LAS 200","ATC 35","MISSILE","TURBO SHIFT"};
    for(int i=0;i<4;++i){float y=682+i*38;rect(81,y-1,281,31,i==p.weapon?Color{0.23f,0.21f,0.22f}:Color{0.13f,0.11f,0.12f});if(i==p.weapon)outline(80,y-2,283,33,cream,2);text(89,y,i==3?" ":std::to_string(i+1),19,i==p.weapon?green:label);rect(112,y+1,9,25,i==3?purple:red);text(132,y,weapons[i],i==3?12:17,i==p.weapon?cream:label);if(i==2)text(287,y,std::to_string(p.missiles),18,label);else{float fill=i==3?p.turboPower:1;bar(282,y+7,65,13,fill,i==3?orange:purple);}}
    bevel(85,858,124,29,3,purple);centered(147,862,"1 / 2 / 3",13,cream);bevel(223,858,136,29,3,purple);centered(291,862,"WEAPONS",13,cream);
    const char* keys[]={"T","1","2","3","C","F1"};
    for(int i=0;i<6;++i){float y=654+i*38;bevel(414,y,55,32,3,i==0?Color{0.20f,0.56f,0.16f}:purple);centered(441,y+5,keys[i],15,cream);}
    bevel(479,649,548,238,7,lcd);text(498,663,"TARGET",17,cream);outline(817,661,190,30,{0.26f,0.27f,0.26f},3);centered(912,665,"T / TAB SELECT",13,label);line(490,702,1013,702,{0.28f,0.30f,0.27f},2);
    const Enemy* target=game.target>=0&&game.target<int(game.enemies.size())&&game.enemies[game.target].alive?&game.enemies[game.target]:nullptr;
    text(499,719,"TARGET:",15,label);text(499,746,target?chassis[target->type].name:"NO LOCK",20,target?red:label);
    text(499,786,"STATUS:",15,label);text(499,810,target?(target->legs<30?"LEG DAMAGE":"TRACKING"):"SCANNING",16,target?green:cyan);
    text(499,850,"DIST:",15,label);text(578,850,target?num(length(target->pos-p.pos))+" M":"----",16,red);
    schematic(872,719,1.08f,target?target->armor/target->maxArmor:1,target?target->legs/75:1);
    bevel(1071,667,298,183,8,lcd);bevel(1086,681,72,150,8,{0.12f,0.12f,0.12f});
    for(int i=0;i<25;++i)line(1100,695+i*4.7f,1141,695+i*4.7f,{0.36f,0.36f,0.35f});
    float throttle=clamp(std::abs(p.speed)/(p.maxSpeed*1.6f),0,1);bevel(1093,801-throttle*107,58,16,4,{0.52f,0.52f,0.53f});
    text(1176,683,"ARMOR",11,label);centered(1286,679,num(p.armor),21,cream);schematic(1197,714,0.55f,p.armor/p.maxArmor,1);bar(1263,714,78,9,p.armor/p.maxArmor,green);bar(1263,741,78,9,p.shield/p.maxShield,cyan);text(1262,764,"SHIELD",10,label);text(1262,785,num(p.shield),23,cyan);
    bevel(1081,858,126,29,3,purple);centered(1144,862,"HEAT",13,cream);bar(1219,861,133,18,p.heat/100,p.overheated?red:cyan);
    glPopMatrix();
    if(p.overheated)centered(720,506,"REACTOR OVERHEAT",17,orange);
    else if(p.turboPower>0.1f)centered(720,506,p.turboEngaged?"TURBO ENGAGED":"TURBINE SPOOL-DOWN",13,cyan);
    if(game.messageTime>0&&game.ready()){rect(358,535,725,18,{0,0,0,0.45f});centered(720,536,game.message,10,cyan);}
    if(!game.ready()){
        bevel(482,204,476,279,13,{0.025f,0.035f,0.035f});centered(720,221,game.insertionAltitude()>0?"AIR-DROP INSERTION":"TOUCHDOWN // INITIALIZING",20,cyan);centered(720,248,game.insertionAltitude()>0?"ALTITUDE: "+num(game.insertionAltitude())+" M":"GROUND CONTACT CONFIRMED",13,label);
        const char* stages[]={"REACTOR IGNITION","SYSTEM DIAGNOSTICS","HYDRAULIC PRESSURE","ACTUATORS ONLINE"};
        int stage=int(game.startupStage());for(int i=0;i<4;++i){float y=276+i*37;text(508,y,stages[i],16,i<=stage?cream:label);text(864,y,i<stage?"OK":i==stage?"...":"--",16,i<stage?green:cyan);}
        bar(508,440,424,7,game.startupTime/startupDuration,cyan);
    }
    if(game.damageFlash>0)outline(1,1,1438,898,{1,0.15f,0.03f,game.damageFlash},6);
    text(22,26,"ESC PAUSE",9,label);text(1301,26,ui.muted?"MUTED":"F2 AUDIO",9,label);
}
void Renderer::audioSettings(const UIState& ui) {
    rect(0,0,1440,900,{0.018f,0.03f,0.03f,0.95f});text(350,156,"// AUDIO SYSTEMS",12,orange,false,1);text(346,198,"BALANCE THE MACHINE.",39,cream,true);
    const char* names[]={"MUSIC","WEAPONS","MACHINERY","INTERFACE"};
    const char* descriptions[]={"Continuous industrial ambient score","Weapons, hits and explosions","Foot impacts, hydraulics, reactor and turbine","Targeting and system alerts"};
    for(int i=0;i<4;++i){float y=300+i*91.0f;text(350,y,names[i],17,cream,true);text(350,y+29,descriptions[i],11,muted);bar(578,y+7,180,12,ui.volumes[i],mint);text(770,y,std::to_string(int(ui.volumes[i]*100+0.5f))+"%",14,mint);button(818,y-4,40,36,"-",Action(int(Action::MusicDown)+i*2));button(871,y-4,40,36,"+",Action(int(Action::MusicUp)+i*2));}
    text(350,689,"Music remains below the machinery. M toggles master mute.",12,muted);button(350,748,300,56,"BACK",Action::Back,true);button(674,748,237,56,ui.muted?"UNMUTE":"MUTE ALL",Action::Sound);
}
void Renderer::controls(const UIState&) {
    rect(0,0,1440,900,{0.018f,0.035f,0.032f,0.94f});text(200,96,"// PILOT FIELD MANUAL",12,orange,false,1);text(196,133,"KNOW YOUR MACHINE.",47,cream,true);
    const char* keys[]={"W / S", "A / D", "Q / E", "MOUSE", "C", "SHIFT", "LMB / SPACE", "RMB", "1 / 2 / 3", "T / TAB", "ESC", "M / F11"};
    const char* actions[]={"Forward / reverse", "Turn chassis", "Strafe left / right", "Aim torso and weapons", "Center torso and aim", "Turbo reactor (generates heat)", "Fire selected weapon", "Launch guided missile", "Select weapon", "Cycle hostile target", "Pause / release mouse", "Mute audio / fullscreen"};
    for(int i=0;i<12;++i){float x=i<6?200:760,y=235+(i%6)*52.0f;text(x,y,keys[i],14,mint);text(x+160,y,actions[i],13,cream);line(x,y+35,x+475,y+35,border);}
    text(200,588,"FIGHT SMART. COME HOME.",19,cream,true);
    text(200,628,"Shields recharge after 5 seconds without a hit. Overheated weapons must cool.",13,muted);
    text(200,658,"Aim for legs to preserve salvage. Walk near green crates for alloy and repairs.",13,muted);
    text(200,688,"Clear all waves, then reach the green extraction beacon to bank your salvage.",13,muted);
    text(200,718,"F2 opens independent music, weapons, machinery and interface volumes.",13,muted);
    text(200,748,"[ / ] adjusts radar range. Diamond: waypoint / extraction; squares: relays.",13,muted);
    button(200,788,260,57,"BACK TO OPERATIONS",Action::Back,true);
}
void Renderer::pause(const UIState&) {
    rect(0,0,1440,900,{0.015f,0.03f,0.028f,0.86f});text(505,209,"// SYSTEMS ON STANDBY",12,orange,false,1);text(502,250,"HOLD POSITION.",47,cream,true);text(506,317,"Simulation paused. Your HERC is safe.",13,muted);
    button(975,382,245,52,"F2  AUDIO MIXER",Action::AudioSettings);button(505,382,430,58,"RESUME MISSION",Action::Resume,true);button(505,456,430,52,"FIELD MANUAL",Action::Controls);button(505,524,430,52,"RESTART MISSION",Action::Retry);button(505,592,430,52,"RETURN TO HANGAR",Action::Hangar);button(505,660,430,52,"EXIT TO DESKTOP",Action::Quit);
}
void Renderer::debrief(const Combat& game,const UIState& ui) {
    bool won=game.phase==Phase::Won,route=game.navigationOnly();rect(0,0,1440,900,{0.02f,0.04f,0.035f,0.91f});
    text(400,161,route?"// NAVIGATION TEST":won?"// EXTRACTION CONFIRMED":"// HERC SIGNAL LOST",12,won?mint:orange,false,1);text(395,205,route&&won?"TEST COMPLETE.":won?"WELCOME HOME.":"NOT THE END.",61,cream,true);
    text(400,289,route&&won?"Five waypoints reached. Ready for another run.":won?"One more piece of Earth belongs to us.":"The resistance needs you. Refit and try again.",15,muted);
    line(400,345,1040,345,border);
    const char* labels[]={"OPERATION", route?"WAYPOINTS REACHED":"HOSTILES DESTROYED", "MISSION TIME", "SALVAGE BANKED", "TOTAL RESERVE"};
    std::string values[]={missions[game.mission].name,route?std::to_string(won?5:game.wave-1)+" / 5":std::to_string(game.kills)+" / "+std::to_string(missions[game.mission].enemies),num(game.time/60,1)+" MIN",won&&!game.testMission()?std::to_string(game.salvage)+" ALLOY":"0 ALLOY",std::to_string(ui.bank)+" ALLOY"};
    for(int i=0;i<5;++i){float y=378+i*47.0f;text(400,y,labels[i],12,muted);text(774,y,values[i],14,i>=3?orange:cream,true);}
    button(400,660,306,61,game.testMission()?"RESTART TEST   >":won?"NEXT OPERATION   >":"RETRY MISSION   >",won&&!game.testMission()?Action::Next:Action::Retry,true);button(726,660,314,61,"RETURN TO HANGAR",Action::Hangar);button(400,744,640,44,"EXIT TO DESKTOP",Action::Quit);
}
void Renderer::render(int width,int height,const Combat& game,const UIState& ui) {
    pixelWidth=std::max(1,width);pixelHeight=std::max(1,height);state=&ui;buttons.clear();glViewport(0,0,pixelWidth,pixelHeight);
    bool original=originalCockpitTexture && (ui.screen==Screen::Playing||ui.screen==Screen::Paused||ui.screen==Screen::Debrief);
    viewAspect=float(pixelWidth)/pixelHeight;
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(original) {
        // Preserve the source art's 4:3 proportions; projection and overlay share this viewport.
        int h=std::min(pixelHeight,pixelWidth*3/4),w=h*4/3;
        h=std::max(h,1);w=std::max(w,1);viewAspect=float(w)/h;
        glViewport((pixelWidth-w)/2,(pixelHeight-h)/2,w,h);
    }
    sky(ui.time);
    bool inMenu=ui.screen==Screen::Menu||ui.screen==Screen::Hangar||(ui.screen==Screen::Controls&&(ui.returnScreen==Screen::Menu||ui.returnScreen==Screen::Hangar));
    world(game,inMenu,ui.time);begin2D();
    if(ui.screen==Screen::Menu)menu(ui);
    else if(ui.screen==Screen::Hangar)hangar(ui);
    else if(ui.screen==Screen::Controls)controls(ui);
    else if(ui.screen==Screen::AudioSettings)audioSettings(ui);
    else {
        if(original){begin2D(640,480);originalHud(game,ui);}else hud(game,ui);
        // Menus keep their existing full-window layout and mouse coordinates.
        glViewport(0,0,pixelWidth,pixelHeight);begin2D();
        if(ui.screen==Screen::Paused)pause(ui);else if(ui.screen==Screen::Debrief)debrief(game,ui);
    }
    if(ui.noticeTime>0){rect(380,100,680,36,panel);centered(720,109,ui.notice,12,mint);}
}
bool Renderer::screenshot(const std::string& path,int width,int height) {
    std::vector<unsigned char> pixels(size_t(width)*height*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream out(path,std::ios::binary);if(!out)return false;out<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;--y)out.write(reinterpret_cast<char*>(pixels.data()+size_t(y)*width*3),width*3);
    return bool(out);
}
}
