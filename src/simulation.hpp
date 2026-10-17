#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace siege {
constexpr float pi = 3.14159265358979323846f;
struct Vec3 {
    float x=0, y=0, z=0;
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
    Vec3& operator+=(Vec3 b) { x+=b.x; y+=b.y; z+=b.z; return *this; }
};
inline float dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float length(Vec3 a) { return std::sqrt(dot(a,a)); }
inline Vec3 normalized(Vec3 a) { float l=length(a); return l>0.00001f?a*(1/l):Vec3{}; }
inline Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float clamp(float x,float lo,float hi) { return std::clamp(x,lo,hi); }
inline Vec3 direction(float yaw,float pitch=0) { return {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)}; }
float terrainHeight(float x,float z);
constexpr float battlefieldLimit=1200;
struct MissionLayout { Vec3 insertion; std::array<Vec3,3> sectors; Vec3 extraction; };
extern const MissionLayout missionLayouts[3];
struct Landmark { Vec3 pos; const char* name; float height; };
extern const std::array<Landmark,4> landmarks;
struct Obstacle { Vec3 center,halfSize; };
const std::vector<Obstacle>& obstacles();
bool solidAt(Vec3 position,float radius=0);
struct Chassis { const char* name; const char* role; float armor,shield,speed; int missiles; float damage,mass; };
extern const Chassis chassis[3];
struct Mission { const char* name; const char* region; const char* brief; int enemies; float enemyPower; };
extern const Mission missions[3];
enum class Phase { Active, Won, Lost };
enum class Event { Laser, Cannon, Missile, Impact, Explosion, Lock, Wave, Win, Lose, Overheat,
    FootLeft, FootRight, HydraulicLeft, HydraulicRight, ReactorIgnition, SystemInit, MechanicalActivate, ReactorReady,
    TurboEngage, TurboRelease, ReactorShutdown, DamageWarning, EnemyDestroyed, EnemyDisabled, Count };
struct SoundEvent {
    Event type;
    float strength=1,pan=0,mass=65,activity=0;
    SoundEvent(Event event,float intensity=1,float position=0,float tons=65,float motion=0)
        : type(event),strength(intensity),pan(position),mass(tons),activity(motion) {}
};
// One phase defines both rendered foot trajectories and touchdown/liftoff events.
// A foot is planted for 60% of its stride; the remaining 40% is its swing.
struct FootPose { Vec3 offset; bool planted=true; };
struct GaitTransition { int side=0; bool contact=false; float strength=0; };
struct Gait {
    double phase=0;
    float travel=0,blend=0,forward=1,sideways=0,pivot=0,speed=0,load=0;
    float bounce=0,bounceVelocity=0,sway=0,swayVelocity=0,settleStrength=0;
    unsigned contacts=0;
    FootPose foot(int side) const;
    float bodyLift() const;
    Vec3 displacement() const;
    // No allocation: a 50 ms tick cannot cross more than four gait boundaries.
    int advance(float dt,float forwardSpeed,float sideSpeed,float turnRate,float maxSpeed,float mass,
                std::array<GaitTransition,4>& transitions);
};
constexpr float startupDuration=6.2f;
enum class StartupStage { Ignition, Systems, Hydraulics, Online };
struct Controls { float throttle=0,turn=0,strafe=0; bool fire=false,missile=false,boost=false; };
struct Player {
    Vec3 pos{0,0,105};
    float yaw=0,torso=0,pitch=0,speed=0,armor=110,shield=100,heat=0;
    float maxArmor=110,maxShield=100,maxSpeed=13,damage=1;
    float cooldown=0,missileCooldown=0,sinceHit=100;
    float sideSpeed=0,turnRate=0,mass=65,turboPower=0;
    Gait gait;
    int missiles=10,weapon=0;
    bool overheated=false,turboEngaged=false;
};
struct Enemy {
    int id=0,type=0;
    Vec3 pos;
    float yaw=0,armor=100,maxArmor=100,shield=45,maxShield=45,legs=65;
    float cooldown=0,sinceHit=0,flash=0,deathTime=0;
    Gait gait;
    bool alive=true,legKill=false;
};
struct Projectile { Vec3 pos,velocity; float life=0; int target=-1; bool friendly=false; float damage=0; };
struct Beam { Vec3 a,b; float life=0,maxLife=0; int kind=0; };
struct Particle { Vec3 pos,velocity; float life=0,maxLife=0,size=0; int kind=0; };
struct Pickup { Vec3 pos; int value=0; bool collected=false; };
struct CockpitMessage { std::string text; float elapsed=0; };
struct Combat {
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<Beam> beams;
    std::vector<Particle> particles;
    std::vector<Pickup> pickups;
    std::vector<SoundEvent> events;
    Phase phase=Phase::Active;
    int mission=0,chassisIndex=0,kills=0,spawned=0,wave=0,salvage=0,target=-1;
    float time=0,waveDelay=0,hitFlash=0,damageFlash=0,messageTime=0,startupTime=0;
    std::string message;
    std::deque<CockpitMessage> statusMessages;
    Vec3 extraction{0,0,-225};
    bool extractionOpen=false;
    std::mt19937 rng{2624};
    void reset(int selectedChassis=0,int selectedMission=0,int shieldUpgrades=0,int weaponUpgrades=0);
    void update(float dt,const Controls& input);
    void fire(int weapon);
    void damageEnemy(int index,float damage,bool legs=false);
    void damagePlayer(float amount);
    void selectTarget();
    void spawnWave();
    void explode(Vec3 pos,int count=32);
    void announce(const std::string& text,float seconds=4);
    void status(const std::string& text);
    Vec3 eye() const;
    Vec3 cameraEye() const;
    Vec3 cameraAim() const;
    float cameraRoll() const;
    bool ready() const { return startupTime>=startupDuration; }
    StartupStage startupStage() const;
    float insertionAltitude() const;
    Vec3 navigationPoint() const;
    Vec3 aim() const;
    float random(float low,float high);
};
}
