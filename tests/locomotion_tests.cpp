#include "simulation.hpp"
#include <cstdlib>
#include <iostream>
using namespace siege;
namespace {
constexpr float dt=1.0f/120;
int checks=0;
void expect(bool value,const char* message){++checks;if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
void tick(Combat& game,float seconds,Controls input={}){for(int i=0;i<int(seconds*120);++i){game.update(dt,input);game.events.clear();}}
void ready(Combat& game,int type=0){game.reset(type);while(!game.ready())game.update(dt,{});for(auto& e:game.enemies)e.alive=false;game.spawned=missions[0].enemies;game.events.clear();}
bool isStep(Event e){return e==Event::FootLeft||e==Event::FootRight;}
}
int main(){
    Combat game;game.reset();Vec3 initial=game.player.pos,enemy=game.enemies[0].pos;
    std::vector<Event> startup;for(const auto& event:game.events)startup.push_back(event.type);game.events.clear();
    while(!game.ready()){
        game.fire(0);game.update(dt,{1,1,1,true,true,true});
        for(const auto& event:game.events)startup.push_back(event.type);game.events.clear();
    }
    expect(startup==std::vector<Event>{Event::ReactorIgnition,Event::SystemInit,Event::MechanicalActivate,Event::ReactorReady},"startup plays four ordered cues exactly once");
    expect(length(game.player.pos-initial)==0&&length(game.enemies[0].pos-enemy)==0&&game.time==0,"startup holds player, enemies and mission clock");
    expect(game.projectiles.empty()&&game.beams.empty()&&game.player.gait.contacts==0,"startup interlocks weapons and gait");
    game.update(dt,{});expect(game.events.empty(),"online transition does not repeat startup cues");
    game.reset();expect(game.startupTime==0&&game.events.size()==1&&game.events[0].type==Event::ReactorIgnition,"restart resets startup and its cues");

    // Every event is compared to the rendered foot state on both sides of the tick.
    for(float speed:{2.0f,9.0f,20.0f,28.8f}){
        Gait gait;int previousSide=0,contacts=0,lifts=0;bool sync=true,alternates=true,bounded=true;
        for(int frame=0;frame<120*15;++frame){
            auto left=gait.foot(-1),right=gait.foot(1);std::array<GaitTransition,4> transitions{};
            int count=gait.advance(dt,speed,0,0,18,65,transitions);
            for(int i=0;i<count;++i){auto e=transitions[i];auto before=e.side<0?left:right;auto after=gait.foot(e.side);
                if(e.contact){++contacts;sync&=!before.planted&&after.planted&&after.offset.y==0;alternates&=e.side!=previousSide;previousSide=e.side;}
                else {++lifts;sync&=before.planted&&!after.planted;}
            }
            bounded&=std::abs(gait.bounce)<0.0651f&&std::abs(gait.sway)<0.0251f&&std::abs(gait.bodyLift())<0.14f;
        }
        expect(sync&&contacts>2&&lifts>=contacts,"foot impacts and hydraulic lift cues match foot trajectories");
        expect(alternates&&gait.contacts==unsigned(contacts),"left and right contacts alternate without duplicate events");
        expect(bounded,"suspension stays within centimetres at all speeds");
        auto before=gait.contacts;for(int frame=0;frame<120*3;++frame){std::array<GaitTransition,4> events{};gait.advance(dt,0,0,0,18,65,events);}
        expect(gait.contacts-before<=1&&gait.foot(-1).planted&&gait.foot(1).planted,"stop completes at most one raised foot placement");
        expect(std::abs(gait.bounce)<0.0001f&&std::abs(gait.sway)<0.0001f,"suspension settles when stationary");
    }
    Gait idle;std::array<GaitTransition,4> events{};int idleEvents=0;
    for(int i=0;i<2400;++i)idleEvents+=idle.advance(dt,0,0,0,13,65,events);
    expect(idleEvents==0&&idle.contacts==0&&length(idle.displacement())==0,"stationary machine has no footsteps or camera shake");
    unsigned slowSteps=0,fastSteps=0;
    for(int fast=0;fast<2;++fast){Gait gait;for(int i=0;i<1200;++i)gait.advance(dt,fast?20.8f:13,0,0,13,65,events);(fast?fastSteps:slowSteps)=gait.contacts;}
    expect(fastSteps>slowSteps*1.3f&&fastSteps<slowSteps*1.65f,"turbo cadence follows distance and lengthened strides");
    for(Controls input:std::array<Controls,3>{{{-1,0,0},{0,0,1},{0,1,0}}}){
        ready(game);tick(game,5,input);expect(game.player.gait.contacts>=2,"reverse, strafe and pivot all produce foot contacts");
    }
    ready(game);game.player.pos={-130,terrainHeight(-130,-110),-110};tick(game,9,{1});
    auto contacts=game.player.gait.contacts;tick(game,4,{1});
    expect(game.player.gait.contacts==contacts&&game.player.gait.speed<0.12f,"walking into a solid wall does not play phantom footsteps");
    ready(game);game.player.pos.z=-battlefieldLimit;tick(game,7,{1});contacts=game.player.gait.contacts;tick(game,4,{1});
    expect(game.player.gait.contacts==contacts,"world boundary stops cadence after settling");
    Combat heavy,light;ready(heavy,1);ready(light,2);tick(heavy,1,{1});tick(light,1,{1});
    expect(light.player.speed>heavy.player.speed*2,"chassis mass governs acceleration");
    Gait heavyGait,lightGait;heavyGait.phase=lightGait.phase=0.499;
    heavyGait.advance(dt,8,0,0,13,100,events);lightGait.advance(dt,8,0,0,13,40,events);
    expect(std::abs(heavyGait.bounce)>std::abs(lightGait.bounce)*1.7f,"heavy chassis transfers more contact force into suspension");

    ready(game);tick(game,5,{1});float cruise=game.player.speed,previousSpeed=cruise,previousPower=0;int engages=0,releases=0;bool smooth=true;
    for(int frame=0;frame<120*3;++frame){
        game.update(dt,{1,0,0,false,false,true});
        smooth&=game.player.speed>=previousSpeed&&game.player.speed-previousSpeed<0.05f&&game.player.turboPower>=previousPower&&game.player.turboPower-previousPower<0.008f;
        previousSpeed=game.player.speed;previousPower=game.player.turboPower;
        for(auto e:game.events)if(e.type==Event::TurboEngage)++engages;game.events.clear();
    }
    expect(smooth&&engages==1&&game.player.speed>cruise*1.5f,"turbo spools and accelerates smoothly with a single engagement cue");
    game.update(dt,{1});for(auto e:game.events)if(e.type==Event::TurboRelease)++releases;game.events.clear();
    expect(game.player.turboPower>0.98f&&!game.player.turboEngaged,"disengaging turbo retains turbine energy for spool-down");
    for(int frame=0;frame<360;++frame){game.update(dt,{1});for(auto e:game.events)if(e.type==Event::TurboRelease)++releases;game.events.clear();}
    expect(releases==1&&game.player.turboPower==0&&std::abs(game.player.speed-cruise)<0.1f,"turbo winds down to normal cruising speed once");
    ready(game);game.update(dt,{0,0,0,false,false,true});
    expect(game.player.turboEngaged&&game.player.turboPower>0&&game.player.gait.contacts==0,"reactor can spool up while standing without phantom steps");
    game.player.heat=100;game.player.overheated=true;game.events.clear();game.update(dt,{1,0,0,false,false,true});
    expect(!game.player.turboEngaged&&game.events[0].type==Event::TurboRelease,"overheat disengages turbo audibly");
    ready(game);bool soundMatches=true;unsigned sounds=0;
    for(int frame=0;frame<1200;++frame){game.update(dt,{1});for(auto e:game.events)if(isStep(e.type)){
        int side=e.type==Event::FootLeft?-1:1;++sounds;soundMatches&=game.player.gait.foot(side).planted&&e.pan*side>0&&e.mass==game.player.mass;
    }game.events.clear();}
    expect(soundMatches&&sounds==game.player.gait.contacts,"combat sends each player contact with correct side and mass");
    Vec3 aim=game.aim(),origin=game.eye();game.player.gait.bounce=-0.04f;game.player.gait.sway=0.02f;
    expect(length(game.cameraEye()-origin)>0.01f&&std::abs(game.cameraRoll())>0&&length(game.aim()-aim)==0&&length(game.eye()-origin)==0,"cockpit moves with suspension while commanded gun aim stays stable");
    std::cout<<"PASS: "<<checks<<" locomotion/startup/turbo checks\n";
}
