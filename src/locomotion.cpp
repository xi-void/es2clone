#include "simulation.hpp"

namespace siege {
namespace {
constexpr float stance=0.6f;
float approach(float value,float target,float amount) {
    return value+clamp(target-value,-amount,amount);
}
float localPhase(double phase,int side) {
    double p=phase+(side>0?0.5:0.0);
    return float(p-std::floor(p));
}
}
FootPose Gait::foot(int side) const {
    float p=localPhase(phase,side),along=0,lift=0;
    if(p<stance) along=p/stance-0.5f;
    else {
        float swing=(p-stance)/(1-stance);
        // Zero vertical velocity at landing; the impact is the transfer of load.
        float smooth=swing*swing*(3-2*swing);
        along=0.5f-smooth;
        lift=std::sin(pi*swing)*std::sin(pi*swing)*(0.65f+0.15f*blend);
    }
    float distance=along*travel;
    return {{side*2.25f-distance*sideways,lift,-0.4f+distance*(forward+side*pivot)},p<stance};
}
float Gait::bodyLift() const {
    return (1-std::cos(float(phase)*4*pi))*0.035f*blend+bounce;
}
Vec3 Gait::displacement() const {
    return {sway,bodyLift(),-clamp(load,-1.0f,1.0f)*0.025f};
}
int Gait::advance(float dt,float forwardSpeed,float sideSpeed,float turnRate,float maxSpeed,float mass,
                  std::array<GaitTransition,4>& transitions) {
    if(dt<=0)return 0;
    dt=std::min(dt,0.05f);
    float linear=std::hypot(forwardSpeed,sideSpeed);
    float actual=std::hypot(linear,std::abs(turnRate)*3.1f);
    float acceleration=(actual-speed)/dt;
    speed=actual;
    load+=(clamp(acceleration/5,-1,1)-load)*std::min(1.0f,dt*5);
    bool moving=actual>0.12f;
    float ratio=clamp(actual/maxSpeed,0,1.6f);
    float requestedTravel=4.6f+1.7f*ratio+mass*0.01f;
    float nextRate=moving?stance*actual/requestedTravel:0;
    int count=0;
    bool airborne=!foot(-1).planted||!foot(1).planted;
    if(moving) {
        blend=approach(blend,clamp(ratio*2,0.15f,1),dt*2);
        travel=approach(travel,requestedTravel,dt*12);
        // Blend direction changes so a reversal does not teleport a leg.
        float denominator=std::max(0.1f,actual);
        forward=approach(forward,forwardSpeed/denominator,dt*4);
        sideways=approach(sideways,sideSpeed/denominator,dt*4);
        pivot=approach(pivot,turnRate*3.1f/denominator,dt*4);
        settleStrength=0.48f+0.37f*std::min(ratio,1.3f)+0.12f*std::abs(load);
    } else if(airborne) {
        // Finish a raised foot's placement once. Do not loop steps against a wall.
        double nextContact=std::floor(phase*2+1.0000001)*0.5;
        nextRate=std::min(0.7f,float(nextContact-phase)/dt);
        settleStrength=0.32f;
    } else {
        blend=approach(blend,0,dt*2);
        travel=approach(travel,0,dt*6);
    }
    double previous=phase,next=phase+nextRate*dt;
    // Split on both touchdown and lift boundaries; events are derived from the
    // same 0/0.6 foot phases consumed by the renderer, not an independent timer.
    for(int side:{-1,1}) {
        double offset=side>0?0.5:0;
        for(int boundary=0;boundary<2;++boundary) {
            double localBoundary=boundary==0?0.0:double(stance);
            double at=std::floor(previous+offset-localBoundary)+1+localBoundary-offset;
            if(at<=next+1e-10&&count<int(transitions.size())) {
                // Land exactly on a crossed boundary. A rounded settling rate
                // must not leave the foot just before contact and retrigger it.
                next=std::max(next,at);
                bool contact=boundary==0;
                transitions[count++]={side,contact,settleStrength};
                if(contact) {
                    ++contacts;
                    float weight=clamp(mass/65,0.6f,1.6f);
                    bounceVelocity-=0.50f*settleStrength*weight;
                    swayVelocity+=side*0.13f*settleStrength*weight;
                }
            }
        }
    }
    phase=next;
    // Keep phase precise on long missions without changing the contact counter.
    if(phase>=1024)phase-=1024;
    // Damped suspension, ~3 Hz, with centimetres of travel. No random shaking.
    bounceVelocity+=(-340*bounce-23*bounceVelocity)*dt;
    bounce+=bounceVelocity*dt;
    swayVelocity+=(-180*sway-18*swayVelocity)*dt;
    sway+=swayVelocity*dt;
    bounce=clamp(bounce,-0.065f,0.065f);sway=clamp(sway,-0.025f,0.025f);
    return count;
}
}
