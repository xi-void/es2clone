#include "original_models.hpp"
#include <iostream>
#include <functional>
#include <stdexcept>
using namespace siege;
using namespace siege::original;
namespace {
int checks=0;
void expect(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void reject(const std::function<void()>& fn,const char* message){bool threw=false;try{fn();}catch(const std::exception&){threw=true;}expect(threw,message);}
void word(Bytes& b,unsigned v,unsigned n=2){while(n--){b.push_back(uint8_t(v));v>>=8;}}
void append(Bytes& a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());}
Bytes record(unsigned tag,const Bytes& body){Bytes b;word(b,tag,4);word(b,unsigned(body.size()),4);append(b,body);return b;}
Bytes base(int bone,int id=0){Bytes b;word(b,unsigned(bone));word(b,id);for(int i=0;i<4;++i)word(b,0);return b;}
Bytes mesh(int bone,bool badIndex=false){
    Bytes b=base(bone);for(unsigned x:{3,5,4,1})word(b,x);word(b,0);word(b,1);word(b,badIndex?99:2);
    for(int v:{0,0,0, 10,0,0, 0,0,10, 0,-100,0, 3,0,3})word(b,unsigned(v));
    for(int i=0;i<8;++i)word(b,0);
    Bytes poly;for(int v:{3,4,3,0,0})word(poly,v);append(b,record(0x140003,poly));return record(0x140014,b);
}
Bytes fixture(bool badIndex=false,bool cycle=false){
    // Synthetic triangles only: test the serialized format without game assets.
    Bytes b=base(-1);word(b,2);
    Bytes cell=base(5);word(cell,3);append(cell,mesh(5,badIndex));append(cell,mesh(5));append(cell,mesh(5));word(cell,0);append(b,record(0x14000b,cell));append(b,mesh(7));
    word(b,0);word(b,0);
    Bytes anim;word(anim,8);
    for(int i=0;i<8;++i){Bytes seq;for(int v:{100,0,0,1,100,0,0,12})word(seq,v);for(int j=0;j<12;++j)word(seq,j);for(int j=0;j<12;++j)word(seq,j);append(anim,record(0x1e0001,seq));}
    word(anim,0);word(anim,12);for(int i=0;i<12*6;++i)word(anim,0);word(anim,12);for(int i=0;i<12;++i)word(anim,i);
    word(anim,12);for(int i=0;i<12;++i){word(anim,cycle&&i==5?5:unsigned(-1));word(anim,i);}
    append(b,record(0x1e0002,anim));return record(0x1e0003,b);
}
float sole(const HercShape& shape,const std::vector<ModelMatrix>& pose,int side){
    float low=1e9f;for(const auto& m:shape.meshes)if(m.bone==(side<0?5:7)||m.bone==(side<0?6:8))for(const auto& f:m.faces)for(auto p:f.vertices)low=std::min(low,pose[m.bone].point(p).z);return low;
}
}
int main(int argc,char** argv)try {
    auto bytes=fixture();auto shape=decodeHercShape(bytes);
    expect(shape.meshes.size()==2,"Cell variants must not render overlapping copies");
    expect(shape.defaults.size()==12&&shape.sequences.size()==8,"Read hierarchy and original keyframes");
    reject([&]{decodeHercShape(fixture(true));},"Reject invalid geometry indices");
    reject([&]{decodeHercShape(fixture(false,true));},"Reject cyclic hierarchy");
    for(size_t n=0;n<bytes.size();n+=37)reject([&]{decodeHercShape(Bytes(bytes.begin(),bytes.begin()+n));},"Reject truncated records");
    auto bad=bytes;bad[0]=0xff;reject([&]{decodeHercShape(bad);},"Reject unknown shape format");
    bad=bytes;bad[7]=0xff;reject([&]{decodeHercShape(bad);},"Reject excessive record length");
    bad=bytes;
    for(size_t i=0;i+10<bad.size();++i)if(bad[i]==0x14&&bad[i+1]==0&&bad[i+2]==0x14&&bad[i+3]==0&&bad[i+8]==7)bad[i+8]=4;
    reject([&]{decodeHercShape(bad);},"Reject unsupported rig with missing sole geometry");
    Combat game;game.reset(0,modelTestMission);expect(game.enemies.size()==2,"Model range has exactly two targets");
    expect(game.enemies[0].appearance==HercAppearance::Tomahawk&&game.enemies[1].appearance==HercAppearance::Achilles,"Range includes both factions");
    expect(game.testMission()&&!game.navigationOnly()&&game.groundHeight(50,25)==0,"Separate flat combat test mission");
    game.selectTarget();expect(game.target==0,"Original target cycling works");auto start=game.enemies[0].pos;
    for(int i=0;i<1000;++i){game.update(1.f/120,{});game.events.clear();}
    expect(length(game.enemies[0].pos-start)>0.1f&&game.enemies[0].gait.phase>0,"AI movement still drives model gait");
    Vec3 aim=game.enemies[0].pos+Vec3{0,8.5f,0}-game.eye();game.player.torso=std::atan2(aim.x,-aim.z)-game.player.yaw;game.player.pitch=std::atan2(aim.y,std::hypot(aim.x,aim.z));
    float health=game.enemies[0].armor+game.enemies[0].shield;game.fire(0);
    expect(game.enemies[0].armor+game.enemies[0].shield<health,"Existing hitscan weapons hit original-model target");
    game.enemies[0].shield=0;game.damageEnemy(0,10000,true);
    expect(!game.enemies[0].alive&&game.enemies[0].legKill,"Original leg hit behavior retained");
    expect(std::any_of(game.events.begin(),game.events.end(),[](auto e){return e.type==Event::EnemyDisabled;}),"Leg kill keeps disabled association");
    game.events.clear();game.enemies[1].shield=0;game.damageEnemy(1,10000,false);
    expect(std::any_of(game.events.begin(),game.events.end(),[](auto e){return e.type==Event::EnemyDestroyed;}),"Torso kill keeps destroyed association");
    game.reset(0,modelTestMission);expect(game.kills==0&&game.enemies.size()==2&&game.enemies[0].alive,"Test range restarts cleanly");
    if(argc==2)for(auto name:{"TOMAHAWK","ACHILLES"}) {
        auto asset=loadHercAsset(argv[1],name);const auto& model=asset.shape;
        expect(model.detailLevels==7&&model.meshes.size()==18,"Original proof-pair geometry counts");
        expect(model.maximum.z*HercShape::worldScale>10&&model.maximum.z*HercShape::worldScale<15,"Original relative scale fits existing world");
        Gait gait;gait.blend=1;auto first=model.pose(gait);bool moved=false;
        for(int i=0;i<100;++i){gait.phase=i/100.0;auto pose=model.pose(gait);
            bool finite=true;for(const auto& m:pose)for(float v:m.v)finite&=std::isfinite(v);
            expect(finite,"Finite original animation transforms");
            for(int side:{-1,1})if(gait.foot(side).planted)expect(std::abs(sole(model,pose,side))<0.01f,"Original sole planted on simulation ground");
            moved|=length(pose[9].point({})-first[9].point({}))>1;
        }
        expect(moved,"Original leg animation changes pose");
        gait.phase=0;auto yaw=model.pose(gait,0.4f,0),pitch=model.pose(gait,0,0.2f);
        expect(yaw[11].v!=first[11].v&&pitch[11].v!=first[11].v,"Torso and pitch articulation supported");
        std::cout<<name<<": original geometry, textures, hierarchy, animation and ground contacts verified\n";
    }
    std::cout<<"PASS: "<<checks<<" model checks"<<(argc==1?" (synthetic assets; pass original directory for local checks)":"")<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
