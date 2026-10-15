#include "simulation.hpp"
#include <limits>

namespace siege {
const Chassis chassis[3] = {
    {"NOMAD", "MEDIUM / MULTIROLE",110,100,13,10,1.0f,65},
    {"BASTION", "HEAVY / ASSAULT",165,130,9,14,1.15f,100},
    {"STRIDER", "LIGHT / RECON",80,80,18,8,0.95f,40}
};
const Mission missions[3] = {
    {"DUST TO DUST","CINDER BASIN", "Eliminate the Cybrid patrol. Recover salvage. Reach extraction.",4,0.75f},
    {"BROKEN SIGNAL","RELAY STATION 09", "Break through the relay defenders and secure the extraction zone.",6,1.0f},
    {"LAST LIGHT","THE ASH FRONT", "Destroy the assault force. The resistance is counting on you.",8,1.25f}
};
float terrainHeight(float x,float z) {
    // Keep the combat arena gently rolling; mountains are separate scenery.
    float ridges=(std::max(0.0f,std::abs(x)-145)*0.24f+std::max(0.0f,std::abs(z)-300)*0.27f);
    ridges*=0.85f+0.28f*std::sin(x*0.016f+z*0.009f)+0.18f*std::cos(z*0.023f);
    return std::sin(x*0.022f)*std::cos(z*0.018f)*1.9f + std::sin(z*0.044f+x*0.012f)*0.65f+ridges;
}
const std::vector<Obstacle>& obstacles() {
    static const std::vector<Obstacle> result=[] {
        std::vector<Obstacle> objects;
        for(int i=0;i<5;++i) {
            float x=-130+i*20.0f,z=-140-(i%2)*38.0f,h=9+(i%3)*5.0f;
            objects.push_back({{x,terrainHeight(x,z)+h/2,z},{7.5f,h/2+0.5f,10}});
        }
        objects.push_back({{-105,terrainHeight(-105,-220)+10,-220},{5,10,5}});
        for(int i=0;i<9;++i)objects.push_back({{80+i*13.0f,1,-185},{4.5f,1.3f,2.5f}});
        return objects;
    }();
    return result;
}
bool solidAt(Vec3 position,float radius) {
    for(const auto& obstacle:obstacles()) {
        Vec3 d=position-obstacle.center;
        if(std::abs(d.x)<obstacle.halfSize.x+radius&&std::abs(d.y)<obstacle.halfSize.y+radius&&std::abs(d.z)<obstacle.halfSize.z+radius)return true;
    }
    return false;
}
static void resolveMovement(Vec3& position,Vec3 previous,float radius) {
    // Sliding along solid structures feels better than stopping both axes.
    Vec3 probe=position+Vec3{0,3,0};
    if(!solidAt(probe,radius))return;
    probe.x=previous.x;
    if(!solidAt(probe,radius)){position.x=previous.x;return;}
    probe.x=position.x;probe.z=previous.z;
    if(!solidAt(probe,radius)){position.z=previous.z;return;}
    position.x=previous.x;position.z=previous.z;
}
float Combat::random(float low,float high) { return std::uniform_real_distribution<float>(low,high)(rng); }
Vec3 Combat::eye() const { return player.pos+Vec3{0,8.2f,0}; }
Vec3 Combat::aim() const { return direction(player.yaw+player.torso,player.pitch); }
Vec3 Combat::cameraEye() const {
    Vec3 offset=player.gait.displacement();
    return eye()+direction(player.yaw+pi/2)*offset.x+Vec3{0,offset.y,0}+direction(player.yaw)*offset.z;
}
Vec3 Combat::cameraAim() const {
    // The guns retain their commanded direction; camera suspension is cosmetic.
    return direction(player.yaw+player.torso,player.pitch+player.gait.bounce*0.018f);
}
float Combat::cameraRoll() const { return player.gait.sway*0.06f; }
StartupStage Combat::startupStage() const {
    if(startupTime<2.1f)return StartupStage::Ignition;
    if(startupTime<3.55f)return StartupStage::Systems;
    if(startupTime<5.2f)return StartupStage::Hydraulics;
    return StartupStage::Online;
}
void Combat::announce(const std::string& text,float seconds) { message=text; messageTime=seconds; }
void Combat::reset(int selectedChassis,int selectedMission,int shieldUpgrades,int weaponUpgrades) {
    *this=Combat{};
    chassisIndex=std::clamp(selectedChassis,0,2); mission=std::clamp(selectedMission,0,2);
    const auto& c=chassis[chassisIndex];
    player.maxArmor=player.armor=c.armor;
    player.maxShield=player.shield=c.shield+std::clamp(shieldUpgrades,0,3)*20;
    player.maxSpeed=c.speed; player.missiles=c.missiles;player.mass=c.mass;
    player.damage=c.damage*(1+std::clamp(weaponUpgrades,0,3)*0.12f);
    player.pos.y=terrainHeight(player.pos.x,player.pos.z);
    extraction.y=terrainHeight(extraction.x,extraction.z);
    spawnWave();
    events.clear();events.emplace_back(Event::ReactorIgnition,1,0,c.mass);
    announce("REACTOR // Containment established. Ignition sequence.",startupDuration);
}
void Combat::spawnWave() {
    ++wave;
    int count=std::min(3,missions[mission].enemies-spawned);
    for(int i=0;i<count;++i) {
        Enemy e;
        e.id=spawned++; e.type=(e.id+mission)%3;
        e.pos={float(i-1)*52+random(-10,10),0,-35.0f-float(i)*36};
        if(wave>1) e.pos=player.pos+direction(player.yaw+(i-1)*0.55f)*(140+i*25.0f);
        e.pos.x=clamp(e.pos.x,-270,270); e.pos.z=clamp(e.pos.z,-280,240);
        e.pos.y=terrainHeight(e.pos.x,e.pos.z);
        e.maxArmor=e.armor=72+e.type*22+mission*10;
        e.maxShield=e.shield=25+e.type*12;
        e.legs=50+e.type*12; e.cooldown=3+i*1.3f;
        e.yaw=pi;
        enemies.push_back(e);
    }
    events.push_back(Event::Wave);
}
void Combat::explode(Vec3 pos,int count) {
    for(int i=0;i<count;++i) {
        float life=random(0.5f,2.0f);
        particles.push_back({pos,{random(-13,13),random(3,20),random(-13,13)},life,life,random(0.25f,1.2f),i%3});
    }
}
void Combat::damageEnemy(int index,float damage,bool legs) {
    if(index<0||index>=int(enemies.size())||!enemies[index].alive) return;
    auto& e=enemies[index];
    e.sinceHit=0; e.flash=0.13f;
    float absorbed=std::min(e.shield,damage); e.shield-=absorbed; damage-=absorbed;
    if(legs) e.legs-=damage; else e.armor-=damage;
    hitFlash=0.18f;
    if(e.armor<=0||e.legs<=0) {
        e.alive=false; e.legKill=e.legs<=0; ++kills;
        int value=e.legKill?50:30;
        pickups.push_back({e.pos,value,false});
        explode(e.pos+Vec3{0,6,0}); events.push_back(Event::Explosion);
        announce(e.legKill?"LEG DISABLED // Intact chassis. 50 salvage available.":"TARGET DESTROYED // 30 salvage available.",3);
        if(target==index) target=-1;
    } else events.push_back(Event::Impact);
}
void Combat::damagePlayer(float amount) {
    if(phase!=Phase::Active) return;
    player.sinceHit=0;
    float shieldDamage=std::min(player.shield,amount);
    player.shield-=shieldDamage; player.armor=std::max(0.0f,player.armor-(amount-shieldDamage));
    damageFlash=0.45f; events.push_back(Event::Impact);
    if(player.armor<=0) { phase=Phase::Lost; events.push_back(Event::Lose); announce("HERC LOST // Signal terminated.",100); }
}
void Combat::selectTarget() {
    int start=target;
    for(int n=1;n<=int(enemies.size());++n) {
        int i=(start+n)%int(enemies.size());
        if(enemies[i].alive) { target=i; events.push_back(Event::Lock); return; }
    }
    target=-1;
}
void Combat::fire(int weapon) {
    if(phase!=Phase::Active||!ready()||player.overheated) return;
    bool missile=weapon==2;
    if((missile?player.missileCooldown:player.cooldown)>0) return;
    if(missile&&player.missiles<=0) { announce("MISSILE RACK EMPTY // Switch to laser or autocannon.",2); return; }
    Vec3 origin=eye(),dir=aim();
    Vec3 muzzle=origin+cross(dir,{0,1,0})*2.6f+Vec3{0,-1.6f,0}+dir*3;
    float damage=weapon==0?26.0f:weapon==1?44.0f:88.0f;
    player.heat+=weapon==0?14:weapon==1?10:19;
    if(missile) {
        --player.missiles; player.missileCooldown=1.15f;
        int lock=-1; float best=0.9f;
        for(int i=0;i<int(enemies.size());++i) if(enemies[i].alive) {
            float alignment=dot(normalized(enemies[i].pos+Vec3{0,7,0}-origin),dir);
            if(alignment>best) {best=alignment;lock=i;}
        }
        if(target>=0&&target<int(enemies.size())&&enemies[target].alive&&dot(normalized(enemies[target].pos+Vec3{0,7,0}-origin),dir)>0.7f) lock=target;
        projectiles.push_back({muzzle,dir*78,6,lock,true,damage*player.damage});
        events.push_back(Event::Missile);
    } else {
        player.cooldown=weapon==0?0.3f:0.7f;
        float nearest=650; int hit=-1; bool legHit=false;
        for(int i=0;i<int(enemies.size());++i) if(enemies[i].alive) {
            for(int part=0;part<2;++part) {
                Vec3 center=enemies[i].pos+Vec3{0,part==0?8.5f:2.7f,0};
                float distance=dot(center-origin,dir);
                float radius=part==0?4.7f:3.0f;
                float radial=length(origin+dir*distance-center);
                if(distance>0&&distance<nearest&&radial<radius) { nearest=distance; hit=i; legHit=part==1; }
            }
        }
        Vec3 end=origin+dir*nearest;
        // Terrain stops shots aimed into the ground before they reach a target.
        for(float d=2;d<nearest;d+=2) { Vec3 q=origin+dir*d; if(q.y<terrainHeight(q.x,q.z)||solidAt(q)) {end=q;hit=-1;break;} }
        float life=weapon==0?0.13f:0.09f;
        beams.push_back({muzzle,end,life,life,weapon});
        if(hit>=0) {damageEnemy(hit,damage*player.damage,legHit);target=enemies[hit].alive?hit:-1;}
        events.push_back(weapon==0?Event::Laser:Event::Cannon);
    }
    if(player.heat>=100) { player.heat=100; player.overheated=true; announce("REACTOR OVERHEAT // Weapons cooling.",3); events.push_back(Event::Overheat); }
}
void Combat::update(float dt,const Controls& input) {
    if(phase!=Phase::Active) return;
    dt=clamp(dt,0,0.05f);
    if(!ready()) {
        float previous=startupTime;
        startupTime=std::min(startupDuration,startupTime+dt);
        const float moments[]={2.1f,3.55f,5.2f};
        const Event cues[]={Event::SystemInit,Event::MechanicalActivate,Event::ReactorReady};
        for(int i=0;i<3;++i)if(previous<moments[i]&&startupTime>=moments[i])events.emplace_back(cues[i],1,0,player.mass);
        if(ready())announce("COMMAND // All systems online. Weapons free.",5);
        // Enemies and mission time wait for deployment; pause freezes startup too.
        return;
    }
    time+=dt;
    player.cooldown=std::max(0.0f,player.cooldown-dt);
    player.missileCooldown=std::max(0.0f,player.missileCooldown-dt);
    player.sinceHit+=dt; player.heat=std::max(0.0f,player.heat-dt*12);
    hitFlash=std::max(0.0f,hitFlash-dt); damageFlash=std::max(0.0f,damageFlash-dt); messageTime=std::max(0.0f,messageTime-dt);
    if(player.overheated&&player.heat<42) player.overheated=false;
    if(player.sinceHit>5) player.shield=std::min(player.maxShield,player.shield+dt*9);
    bool boosting=input.boost&&!player.overheated;
    if(boosting!=player.turboEngaged) {
        events.emplace_back(boosting?Event::TurboEngage:Event::TurboRelease,0.8f,0,player.mass);
        player.turboEngaged=boosting;
    }
    player.turboPower+=clamp((boosting?1.0f:0.0f)-player.turboPower,-dt*0.65f,dt*0.85f);
    if(boosting) { player.heat=std::min(100.0f,player.heat+dt*24); if(player.heat>=100) {player.overheated=true;events.push_back(Event::Overheat);} }
    float desired=input.throttle*player.maxSpeed*(1+player.turboPower*0.6f)*(input.throttle<0?0.55f:1);
    float acceleration=3.2f*65/player.mass*(1+player.turboPower*0.35f);
    float braking=(input.throttle*player.speed<=0)?1.6f:1;
    player.speed+=clamp(desired-player.speed,-acceleration*braking*dt,acceleration*braking*dt);
    float desiredSide=input.strafe*player.maxSpeed*0.48f;
    player.sideSpeed+=clamp(desiredSide-player.sideSpeed,-acceleration*dt,acceleration*dt);
    float turnSpeed=0.65f*std::sqrt(65/player.mass);
    player.turnRate+=clamp(input.turn*turnSpeed-player.turnRate,-dt*1.2f,dt*1.2f);
    player.yaw=std::remainder(player.yaw+player.turnRate*dt,2*pi);
    Vec3 previousPlayer=player.pos;
    player.pos+=direction(player.yaw)*(player.speed*dt);
    player.pos+=direction(player.yaw+pi/2)*(player.sideSpeed*dt);
    player.pos.x=clamp(player.pos.x,-310,310); player.pos.z=clamp(player.pos.z,-310,260);
    resolveMovement(player.pos,previousPlayer,3.5f);
    player.pos.y=terrainHeight(player.pos.x,player.pos.z);
    Vec3 motion=dt>0?(player.pos-previousPlayer)*(1/dt):Vec3{};motion.y=0;
    std::array<GaitTransition,4> transitions{};
    int count=player.gait.advance(dt,dot(motion,direction(player.yaw)),dot(motion,direction(player.yaw+pi/2)),
                                  player.turnRate,player.maxSpeed,player.mass,transitions);
    for(int i=0;i<count;++i) {
        const auto& step=transitions[i];
        Event event=step.contact?(step.side<0?Event::FootLeft:Event::FootRight):(step.side<0?Event::HydraulicLeft:Event::HydraulicRight);
        events.emplace_back(event,step.strength*(1+player.turboPower*0.12f),step.side*0.3f,player.mass,player.gait.speed/player.maxSpeed);
    }
    if(input.fire) fire(player.weapon);
    if(input.missile) fire(2);
    int alive=0;
    for(auto& e:enemies) {
        e.flash=std::max(0.0f,e.flash-dt);
        if(!e.alive) {e.deathTime+=dt;continue;}
        ++alive; e.sinceHit+=dt;
        if(e.sinceHit>8) e.shield=std::min(e.maxShield,e.shield+dt*3);
        Vec3 delta=player.pos-e.pos; delta.y=0;
        float distance=length(delta); e.yaw=std::atan2(delta.x,-delta.z);
        float move=distance>90?6.0f+e.type*1.4f:distance<42?-3.0f:0.0f;
        Vec3 forward=normalized(delta);
        Vec3 lateral=direction(e.yaw+pi/2)*(std::sin(time*0.45f+e.id*2)*2.8f);
        Vec3 previousEnemy=e.pos;
        e.pos+=(forward*move+lateral)*dt;
        for(const auto& other:enemies)if(other.alive&&other.id!=e.id) {
            Vec3 separation=e.pos-other.pos;separation.y=0;float separationDistance=length(separation);
            if(separationDistance>0.01f&&separationDistance<10)e.pos+=separation*((10-separationDistance)/separationDistance*dt*2);
        }
        e.pos.x=clamp(e.pos.x,-290,290); e.pos.z=clamp(e.pos.z,-290,245);
        resolveMovement(e.pos,previousEnemy,3.5f);
        e.pos.y=terrainHeight(e.pos.x,e.pos.z);
        Vec3 enemyMotion=dt>0?(e.pos-previousEnemy)*(1/dt):Vec3{};enemyMotion.y=0;
        int enemySteps=e.gait.advance(dt,dot(enemyMotion,direction(e.yaw)),dot(enemyMotion,direction(e.yaw+pi/2)),0,
                                     chassis[e.type].speed,chassis[e.type].mass,transitions);
        for(int i=0;i<enemySteps;++i)if(transitions[i].contact&&distance<85) {
            float attenuation=0.20f*std::pow(1-distance/85,2);
            float pan=dot(normalized(e.pos-player.pos),direction(player.yaw+player.torso+pi/2));
            events.emplace_back(transitions[i].side<0?Event::FootLeft:Event::FootRight,attenuation*transitions[i].strength,pan*0.8f,chassis[e.type].mass);
        }
        e.cooldown-=dt;
        if(e.cooldown<=0&&distance<240) {
            Vec3 start=e.pos+Vec3{0,8,0};
            Vec3 predicted=eye()+direction(player.yaw)*(player.speed*distance/105.0f)+Vec3{random(-4,4),random(-1.5f,1.5f),random(-4,4)};
            Vec3 v=normalized(predicted-start)*65;
            projectiles.push_back({start,v,5,-1,false,(11+e.type*3)*missions[mission].enemyPower});
            e.cooldown=random(2.4f,4.4f)/missions[mission].enemyPower;
        }
    }
    for(auto& p:projectiles) {
        p.life-=dt;
        if(p.friendly&&p.target>=0&&p.target<int(enemies.size())&&enemies[p.target].alive) {
            Vec3 desiredVelocity=normalized(enemies[p.target].pos+Vec3{0,7,0}-p.pos)*88;
            p.velocity+=(desiredVelocity-p.velocity)*std::min(1.0f,dt*5);
        }
        p.pos+=p.velocity*dt;
        if(p.friendly) {
            for(int i=0;i<int(enemies.size());++i) if(enemies[i].alive&&length(p.pos-(enemies[i].pos+Vec3{0,7,0}))<5.5f) {
                damageEnemy(i,p.damage); explode(p.pos,12); p.life=-1;break;
            }
        } else if(length(p.pos-eye())<4.5f) {damagePlayer(p.damage);p.life=-1;}
        if(p.pos.y<terrainHeight(p.pos.x,p.pos.z)||solidAt(p.pos)) {explode(p.pos,5);p.life=-1;}
    }
    projectiles.erase(std::remove_if(projectiles.begin(),projectiles.end(),[](auto& p){return p.life<=0;}),projectiles.end());
    for(auto& b:beams) b.life-=dt;
    beams.erase(std::remove_if(beams.begin(),beams.end(),[](auto& b){return b.life<=0;}),beams.end());
    for(auto& p:particles) {p.life-=dt;p.pos+=p.velocity*dt;p.velocity.y-=16*dt;}
    particles.erase(std::remove_if(particles.begin(),particles.end(),[](auto& p){return p.life<=0;}),particles.end());
    for(auto& p:pickups) if(!p.collected&&length(p.pos-player.pos)<19) {
        p.collected=true; salvage+=p.value; player.armor=std::min(player.maxArmor,player.armor+12);
        announce("SALVAGE RECOVERED // +"+std::to_string(p.value)+" alloy. Field repairs +12.",3);
        events.push_back(Event::Lock);
    }
    if(alive==0&&!extractionOpen) {
        waveDelay+=dt;
        if(spawned<missions[mission].enemies&&waveDelay>3) {spawnWave();waveDelay=0;announce("COMMAND // Reinforcements approaching. Check your radar.",5);}
        else if(kills==missions[mission].enemies) {extractionOpen=true;announce("SECTOR CLEAR // Reach the green extraction beacon.",8);events.push_back(Event::Wave);}
    }
    if(phase==Phase::Active&&extractionOpen&&length(player.pos-extraction)<24) {
        phase=Phase::Won; salvage+=75+mission*25; events.push_back(Event::Win); announce("MISSION COMPLETE // Welcome home, pilot.",100);
    }
}
}
