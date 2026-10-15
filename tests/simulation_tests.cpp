#include "simulation.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace siege;
int checks=0;
void expect(bool value,const std::string& message){++checks;if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
void tick(Combat& game,float seconds,Controls controls={}){for(int i=0;i<int(seconds*120);++i)game.update(1.0f/120,controls);}
void removeHostiles(Combat& game){for(auto& e:game.enemies){e.alive=false;}game.spawned=missions[game.mission].enemies;}
int main(){
    Combat game;game.reset();
    expect(game.enemies.size()==3&&game.phase==Phase::Active,"initial wave and active mission");
    expect(game.player.armor==110&&game.player.missiles==10,"Nomad loadout");
    game.reset(1,2,2,1);expect(game.player.armor==165&&game.player.shield==170&&game.player.missiles==14,"chassis and upgrades applied");
    game.reset();removeHostiles(game);float z=game.player.pos.z;tick(game,2,{1,0,0,false,false,false});expect(game.player.pos.z<z-25,"forward movement accelerates");
    tick(game,2);expect(std::abs(game.player.speed)<0.5f,"movement decelerates on release");
    tick(game,40,{1,0,0,false,false,false});expect(game.player.pos.z>=-310,"arena boundary");
    game.reset();removeHostiles(game);game.damagePlayer(130);expect(game.player.shield==0&&game.player.armor==80,"shields absorb damage before armor");
    tick(game,4);expect(game.player.shield==0,"shield regeneration has delay");tick(game,3);expect(game.player.shield>15,"shield regeneration resumes");
    game.damagePlayer(1000);expect(game.phase==Phase::Lost&&game.player.armor==0,"fatal damage ends mission");float time=game.time;tick(game,1);expect(game.time==time,"finished simulation is frozen");
    game.reset();game.enemies.resize(1);game.enemies[0].pos={0,terrainHeight(0,0),0};game.enemies[0].shield=0;game.enemies[0].cooldown=100;
    float armor=game.enemies[0].armor;game.fire(0);expect(game.enemies[0].armor<armor,"laser hits aimed torso");armor=game.enemies[0].armor;game.fire(0);expect(game.enemies[0].armor==armor,"laser obeys cooldown");
    game.player.torso=1.2f;tick(game,0.4f);game.fire(0);expect(game.enemies[0].armor==armor,"off-target shot misses");
    game.reset();game.enemies.resize(1);game.enemies[0].shield=0;game.damageEnemy(0,1000,true);expect(game.enemies[0].legKill&&game.kills==1&&game.pickups[0].value==50,"leg destruction preserves extra salvage");
    game.player.armor=50;game.player.pos=game.pickups[0].pos;game.update(0.01f,{});expect(game.salvage==50&&game.player.armor==62,"salvage pickup and field repair");game.update(0.01f,{});expect(game.salvage==50,"pickup collected once");
    game.reset();removeHostiles(game);game.player.heat=98;game.fire(0);expect(game.player.overheated,"high heat triggers reactor shutdown");size_t beams=game.beams.size();game.fire(1);expect(game.beams.size()==beams,"overheat blocks weapons");tick(game,5.1f);expect(!game.player.overheated,"reactor recovers after cooling");
    game.reset();game.player.missiles=0;game.fire(2);expect(game.projectiles.empty()&&game.player.missiles==0,"empty missile rack cannot fire");
    game.reset();game.enemies.resize(1);game.enemies[0].pos={0,terrainHeight(0,25),25};game.enemies[0].cooldown=100;float health=game.enemies[0].armor+game.enemies[0].shield;game.fire(2);expect(game.player.missiles==9&&game.projectiles.size()==1,"missile launches and consumes ammo");tick(game,2);expect(game.enemies[0].armor+game.enemies[0].shield<health,"guided missile reaches target");
    game.reset();game.selectTarget();expect(game.target==0,"target selects first hostile");game.enemies[1].alive=false;game.selectTarget();expect(game.target==2,"target cycling skips wrecks");game.enemies.clear();game.selectTarget();expect(game.target==-1,"target cycling handles empty battlefield");
    for(int mission=0;mission<3;++mission){
        game.reset(0,mission);int guard=0;
        while(game.kills<missions[mission].enemies&&++guard<10){for(int i=0;i<int(game.enemies.size());++i)game.damageEnemy(i,10000);tick(game,3.2f);}
        expect(game.kills==missions[mission].enemies&&game.extractionOpen,"all waves complete for mission "+std::to_string(mission));
        expect(game.phase==Phase::Active,"mission requires physical extraction");game.player.pos=game.extraction;game.update(0.01f,{});
        expect(game.phase==Phase::Won&&game.salvage>=75+mission*25,"extraction wins and awards mission bonus");
    }
    game.reset();expect(game.kills==0&&game.salvage==0&&game.phase==Phase::Active&&!game.extractionOpen&&game.projectiles.empty(),"restart clears prior mission state");
    game.reset();removeHostiles(game);tick(game,12,{1,0,0,false,false,true});expect(game.player.heat>0,"boost generates heat");
    expect(solidAt({-130,5,-140})&&!solidAt({0,8,0}),"structures occupy collision bounds");
    game.reset();removeHostiles(game);game.player.pos={-130,terrainHeight(-130,-110),-110};tick(game,3,{1,0,0,false,false,false});expect(game.player.pos.z>-128,"HERC stops at solid structure");
    game.reset();game.enemies.resize(1);game.enemies[0].pos={-130,terrainHeight(-130,-160),-160};game.enemies[0].cooldown=100;game.player.pos={-130,terrainHeight(-130,-110),-110};
    armor=game.enemies[0].armor+game.enemies[0].shield;game.fire(0);expect(game.enemies[0].armor+game.enemies[0].shield==armor,"structure blocks laser fire");
    // Exercise actual weapons, enemy AI, heat management, and travel to extraction.
    // This pilot aims accurately, but receives damage and follows normal rules.
    for(int mission=0;mission<3;++mission){
        game.reset(mission,mission);
        for(int frame=0;frame<120*240&&game.phase==Phase::Active;++frame){
            Controls input;int target=-1;float nearest=10000;
            for(int i=0;i<int(game.enemies.size());++i)if(game.enemies[i].alive){float d=length(game.enemies[i].pos-game.player.pos);if(d<nearest){nearest=d;target=i;}}
            Vec3 destination=game.extraction;
            if(target>=0)destination=game.enemies[target].pos;
            Vec3 delta=destination-game.player.pos;
            game.player.yaw=std::atan2(delta.x,-delta.z);game.player.torso=0;
            if(target>=0){
                Vec3 aim=normalized(destination+Vec3{0,8.5f,0}-game.eye());game.player.pitch=std::asin(aim.y);
                input.fire=game.player.heat<72;input.missile=game.player.heat<65&&game.player.missiles>0;
                input.throttle=nearest>85?0.7f:0;input.strafe=std::sin(game.time*0.7f)*0.8f;
                for(float distance=2;distance<nearest;distance+=2)if(solidAt(game.eye()+aim*distance)){input.strafe=1;input.throttle=0.3f;input.fire=false;input.missile=false;break;}
            }
            else {game.player.pitch=0;input.throttle=game.extractionOpen?1:0;}
            game.update(1.0f/120,input);game.events.clear();
        }
        if(game.phase!=Phase::Won){std::cerr<<"Pilot at "<<game.player.pos.x<<","<<game.player.pos.z<<'\n';for(const auto& enemy:game.enemies)if(enemy.alive)std::cerr<<"Survivor "<<enemy.id<<" at "<<enemy.pos.x<<","<<enemy.pos.z<<" HP "<<enemy.armor<<" SH "<<enemy.shield<<'\n';}
        expect(game.phase==Phase::Won,"armed pilot can complete mission "+std::to_string(mission)+" (kills "+std::to_string(game.kills)+", armor "+std::to_string(game.player.armor)+", time "+std::to_string(game.time)+")");
    }
    std::cout<<"PASS: "<<checks<<" simulation checks\n";
}
