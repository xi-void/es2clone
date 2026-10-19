#include "audio.hpp"
#include <cstdlib>
#include <iostream>
using namespace siege;
namespace {
int checks=0;
void expect(bool ok,const char* message){++checks;if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
int arrivals(const Combat& game) {
    return int(std::count_if(game.events.begin(),game.events.end(),[](auto e){return e.type==Event::WaypointReached;}));
}
void ready(Combat& game) {
    game.reset(0,2);game.startupTime=startupDuration;
    // Keep a fixed sector marker, with no AI fire or automatic mission completion.
    game.enemies.clear();game.spawned=missions[game.mission].enemies;game.events.clear();
}
void place(Combat& game,Vec3 point,float distance) {
    game.player.pos=point+Vec3{0,0,distance};
    game.player.pos.y=game.groundHeight(game.player.pos.x,game.player.pos.z);
    game.player.yaw=0;game.player.speed=game.player.sideSpeed=game.player.turnRate=0;
}
void enter(Combat& game) {
    place(game,game.navigationPoint(),waypointArrivalRadius+0.25f);
    game.player.speed=13;
    game.update(0.05f,{1});
    expect(length(game.player.pos-game.navigationPoint())<waypointArrivalRadius,"movement crosses the arrival boundary");
    game.player.speed=0;
}
void tick(Combat& game,int frames,Controls input={}){for(int i=0;i<frames;++i)game.update(1.0f/120,input);}
float renderRms(AudioMixer& mixer) {
    std::vector<float> samples(audioRate*2);mixer.render(samples.data(),audioRate);
    double sum=0;for(float v:samples)sum+=v*v;
    return float(std::sqrt(sum/samples.size()));
}
}
int main(int argc,char** argv) {
    // Reproduce the runtime failure with the real defenders present. Previously
    // navigation followed a defender and fired ~96 m short of the sector anchor.
    Combat patrol;patrol.reset();
    while(!patrol.ready())patrol.update(1.0f/120,{});
    patrol.events.clear();Vec3 anchor=missionLayouts[0].sectors[0];anchor.y=patrol.groundHeight(anchor.x,anchor.z);
    bool stable=true,premature=false;int patrolArrivals=0;
    for(int frame=0;frame<120*40&&patrol.phase==Phase::Active;++frame) {
        patrol.update(1.0f/120,{length(patrol.player.pos-anchor)>26?1.0f:0.15f});
        stable&=length(patrol.navigationPoint()-anchor)<0.001f;
        for(auto event:patrol.events)if(event.type==Event::WaypointReached){++patrolArrivals;premature|=length(patrol.player.pos-anchor)>=waypointArrivalRadius;}
        patrol.events.clear();if(patrolArrivals)break;
    }
    expect(stable,"mission waypoint remains fixed while actual enemy AI moves");
    expect(!premature&&patrolArrivals==1,"normal campaign movement reports the sector arrival, never a nearby enemy");

    // Exercise the actual five-waypoint mission, from spawn through completion,
    // without teleporting, setting speed/yaw, clearing enemies or advancing stages.
    for(int chassis=0;chassis<3;++chassis) {
        Combat range;range.reset(chassis,waypointTestMission);
        expect(range.navigationOnly()&&range.wave==1&&range.enemies.empty()&&!range.extractionOpen,"test mission starts with WP1 and no combat objectives");
        expect(length(range.navigationPoint()-range.player.pos)==100,"WP1 is immediately active 100 meters ahead");
        bool flat=true,open=true;
        for(int x=-300;x<=300;x+=25)for(int z=-300;z<=300;z+=25){flat&=range.groundHeight(x,z)==0;open&=!range.blockedAt({float(x),3,float(z)},4);}
        expect(flat&&open,"test range ground and collision agree on a flat open map");
        while(!range.ready())range.update(1.0f/120,{});
        range.events.clear();tick(range,240);
        expect(arrivals(range)==0&&range.wave==1&&range.phase==Phase::Active,"stationary startup cannot announce or complete any waypoint");
        int reached=0;bool ordered=true,once=true,physical=true;float hold=0;
        for(int frame=0;frame<120*180&&range.phase==Phase::Active;++frame) {
            Vec3 goal=range.navigationPoint(),before=range.player.pos;int active=range.wave;
            Vec3 delta=goal-before;float heading=std::remainder(std::atan2(delta.x,-delta.z)-range.player.yaw,2*pi);
            Controls input;
            if(hold<=0){input.turn=clamp(heading*4,-1,1);input.throttle=std::abs(heading)<0.35f?1:0;}else hold-=1.0f/120;
            range.update(1.0f/120,input);
            int n=arrivals(range);once&=n<=1;
            if(n){
                ++reached;ordered&=active==reached&&range.wave==std::min(5,reached+1);
                physical&=length(before-goal)>=waypointArrivalRadius&&length(range.player.pos-goal)<waypointArrivalRadius;
                hold=2; // Normal braking, then a stationary interval after each arrival.
            }
            range.events.clear();
        }
        expect(ordered&&once&&physical&&reached==5,"normal movement produces exactly one physical arrival for each ordered waypoint");
        expect(range.phase==Phase::Won&&range.wave==5&&range.enemies.empty()&&range.salvage==0,"WP5 alone completes the range without combat rewards");
        tick(range,600);expect(arrivals(range)==0,"remaining at completed WP5 cannot repeat speech");
        range.reset(chassis,waypointTestMission);
        expect(range.wave==1&&!range.waypointArrivalReported&&range.phase==Phase::Active&&arrivals(range)==0,"range can restart with an unreported WP1");
    }
    Combat game;ready(game);
    place(game,game.navigationPoint(),waypointArrivalRadius+0.25f);
    game.selectTarget();for(int i=0;i<10;++i)(void)game.navigationPoint();tick(game,120);
    expect(arrivals(game)==0,"target selection, HUD queries and waiting outside are silent");
    game.player.speed=13;game.update(0,{1});
    expect(arrivals(game)==0,"a paused or zero-duration tick cannot report arrival");
    enter(game);expect(arrivals(game)==1,"first physical arrival emits one notification");
    tick(game,1200);
    expect(arrivals(game)==1,"standing inside for ten seconds cannot retrigger");
    place(game,game.navigationPoint(),5);
    for(int i=0;i<20;++i)tick(game,30,{i%2?1.0f:-1.0f,0,i%2?0.2f:-0.2f});
    expect(arrivals(game)==1,"repeated forward/reverse/strafe movement inside is silent");
    place(game,game.navigationPoint(),30);tick(game,1);enter(game);
    expect(arrivals(game)==1,"leaving and reentering the same active waypoint stays latched");

    // Use normal wave progression to activate successive sector markers.
    for(int completed=1;completed<=2;++completed) {
        game.player.speed=game.player.sideSpeed=0;game.kills=game.spawned=completed*3;
        game.waveDelay=3;game.update(0.05f,{});
        expect(game.wave==completed+1,"cleared patrol advances to the next active waypoint");
        expect(arrivals(game)==completed,"activating the next waypoint does not play arrival");
        game.enemies.clear();game.spawned=missions[game.mission].enemies;
        enter(game);expect(arrivals(game)==completed+1,"each sequential waypoint has its own arrival latch");
    }
    game.kills=missions[game.mission].enemies;game.update(0.05f,{});
    expect(game.extractionOpen&&arrivals(game)==3,"unlocking extraction alone is silent");
    enter(game);
    expect(arrivals(game)==4&&game.phase==Phase::Won,"entering extraction reports arrival before mission completion");
    tick(game,120);expect(arrivals(game)==4,"completed mission cannot repeat the final arrival");
    ready(game);enter(game);expect(arrivals(game)==1,"mission restart clears the previous arrival latch");

    ready(game);
    place(game,missionLayouts[game.mission].sectors[1],0);game.update(0.05f,{});
    game.spawned=3;game.spawnWave();game.enemies.clear();game.spawned=missions[game.mission].enemies;
    tick(game,120);tick(game,120,{0.1f});
    expect(arrivals(game)==0,"a new waypoint activated around the HERC is silent even with movement inside");
    enter(game);expect(arrivals(game)==1,"that waypoint reports a subsequent genuine entry");

    ready(game);place(game,game.navigationPoint(),0);
    Enemy enemy;enemy.id=10;enemy.cooldown=1000;enemy.pos=game.player.pos+Vec3{0,0,-10};
    game.enemies.push_back(enemy);game.selectTarget();tick(game,1);
    expect(arrivals(game)==0,"a defender moving onto a parked HERC is not waypoint arrival");
    enter(game);expect(arrivals(game)==1,"entry uses the same fixed navigation marker as the HUD");
    enemy.id=11;enemy.pos=game.player.pos+Vec3{0,0,-5};game.enemies.push_back(enemy);
    game.selectTarget();enter(game);
    expect(arrivals(game)==1,"switching nearest defender or selected target never rearms the patrol waypoint");

    ready(game);game.extractionOpen=true;
    game.extraction={-130,terrainHeight(-130,-150.5f),-150.5f};
    place(game,game.extraction,24.1f);Vec3 before=game.player.pos;
    game.player.speed=13;game.update(0.05f,{1});
    expect(game.player.pos.z==before.z&&arrivals(game)==0,"blocked movement cannot report entry through a structure");
    game.reset();place(game,game.navigationPoint(),waypointArrivalRadius+0.25f);
    game.player.speed=13;game.update(0.05f,{1});
    expect(arrivals(game)==0,"deployment interlock prevents arrival");
    game.startupTime=startupDuration;game.phase=Phase::Lost;game.update(0.05f,{1});
    expect(arrivals(game)==0,"destroyed HERC cannot reach a waypoint");

    AudioMixer fallback;fallback.setVolumes({0,0,0,defaultVolumes[3]});renderRms(fallback);
    fallback.trigger(Event::WaypointReached);
    expect(renderRms(fallback)>0.001f&&fallback.statistics().finite,"missing original voice has an audible finite notification fallback");

    // Optional real-asset integration: no proprietary files needed for the default suite.
    if(argc>1) {
        expect(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0,"SDL initializes");
        Audio audio;expect(audio.init(argv[1]),"SDL opens the supplied original audio assets");
        const auto& assets=audio.assetInfo();
        expect(assets.config.events[int(Event::WaypointReached)]=="sfx/original/VOL/SIMVOICE.VOL/SIMVOICE/CVM_0031.WAV"&&
               assets.events[int(Event::WaypointReached)].size()==1,"verified original CVM_0031 recording is loaded");
        expect(assets.config.events[int(Event::EnemyDestroyed)]=="sfx/original/VOL/SIMVOICE.VOL/SIMVOICE/CVM_0050.WAV"&&
               assets.config.events[int(Event::EnemyDisabled)]=="sfx/original/VOL/SIMVOICE.VOL/SIMVOICE/CVM_0051.WAV",
               "existing destroyed and disabled associations remain intact");
        AudioMixer mixer;mixer.configure(&assets);mixer.setVolumes({0,0,0,0});renderRms(mixer);
        mixer.trigger(Event::WaypointReached);
        expect(renderRms(mixer)<1e-6f,"existing warning/interface volume can silence waypoint speech");
        mixer.reset();mixer.setVolumes({0,0,0,defaultVolumes[3]});renderRms(mixer);
        mixer.trigger(Event::WaypointReached);float normal=renderRms(mixer);
        mixer.reset();mixer.setVolumes({0,0,0,defaultVolumes[3]/2});renderRms(mixer);
        mixer.trigger(Event::WaypointReached);float quiet=renderRms(mixer);
        expect(normal>0.01f&&std::abs(quiet/normal-0.5f)<0.02f,"speech follows the existing interface volume without a new gain");
        audio.setVolumes({0,0,0,defaultVolumes[3]});SDL_Delay(150);audio.clearStatistics();
        ready(game);enter(game);
        for(auto event:game.events)audio.play(event);
        if(argc>2)expect(audio.startCapture(2),"start local SDL callback capture");
        SDL_Delay(1800);auto stats=audio.statistics();
        expect(stats.assetEvents>=1&&stats.audibleFrames>10000&&stats.finite&&stats.peak>0&&stats.peak<1,
               "real movement event plays the original voice through SDL at the current interface volume");
        if(argc>2)expect(audio.saveCapture(argv[2]),"save local SDL callback capture");
        std::cout<<"Original arrival SDL: mapped="<<stats.assetEvents<<" audible="<<stats.audibleFrames<<" peak="<<stats.peak<<'\n';
        game.events.clear();tick(game,120);for(auto event:game.events)audio.play(event);SDL_Delay(80);
        expect(audio.statistics().assetEvents==stats.assetEvents,"remaining in the radius sends no further original voice to SDL");
        audio.shutdown();SDL_Quit();
    }
    std::cout<<"PASS: "<<checks<<" waypoint checks\n";
}
