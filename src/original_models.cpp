#include "original_models.hpp"
#include <limits>
#include <stdexcept>

namespace siege::original {
namespace {
void require(bool b,const char* message) { if(!b)throw std::runtime_error(message); }
struct Reader {
    const Bytes& b;size_t at=0,end;
    uint32_t u(unsigned n=2) {
        require(at<=end&&n<=end-at,"Truncated DTS record");
        uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(b[at++])<<(i*8);return v;
    }
    int s() {return int16_t(u());}
    Vec3 point() {float x=float(s()),y=float(s()),z=float(s());return {x,y,z};}
    void skip(size_t n) {require(at<=end&&n<=end-at,"Truncated DTS table");at+=n;}
    std::vector<uint16_t> words(size_t n) {
        require(n<65536&&n<=(end-at)/2,"Invalid DTS table length");
        std::vector<uint16_t> v;v.reserve(n);while(n--)v.push_back(uint16_t(u()));return v;
    }
};
struct Parser {
    HercShape shape;
    unsigned objects=0;
    void record(Reader& outer,int depth=0,bool visible=true) {
        require(depth<32&&++objects<50000,"Excessive DTS nesting or object count");
        uint32_t tag=outer.u(4),size=outer.u(4);
        require(size<=outer.end-outer.at,"Invalid DTS object length");
        Reader r{outer.b,outer.at,outer.at+size};outer.at=r.end;
        if(tag==0x1e0002) { // ANAnimList
            unsigned count=r.u();require(count<=64,"Excessive DTS sequence count");
            for(unsigned i=0;i<count;++i)sequence(r);
            count=r.u();r.skip(size_t(count)*8); // transition state machine retained in source
            count=r.u();require(count>0&&count<=8192,"Invalid DTS transform count");
            for(unsigned i=0;i<count;++i){auto rotation=r.point()*(2*pi/65536);auto translation=r.point();shape.transforms.push_back({rotation,translation});}
            shape.defaults=r.words(r.u());require(!shape.defaults.empty()&&shape.defaults.size()<=64,"Invalid DTS bone count");
            shape.parents.assign(shape.defaults.size(),-2);
            count=r.u();require(count<=shape.defaults.size(),"Invalid DTS relation count");
            for(unsigned i=0;i<count;++i) {
                int parent=r.s(),child=r.s();int bones=int(shape.defaults.size());
                require(child>=0&&child<bones&&parent>=-1&&parent<bones&&shape.parents[child]==-2,"Invalid DTS relation");
                shape.parents[child]=parent;
            }
            // Reduced LODs omit the non-rendered ground-motion node 0.
            if(shape.parents[0]==-2)shape.parents[0]=-1;
            for(auto p:shape.parents)require(p>=-1,"Incomplete DTS hierarchy");
        } else {
            int bone=r.s(),id=r.s();r.skip(8); // radius and center are bounds, not translations
            if(tag==0x140014||tag==0x14000a) {
                unsigned ni=r.u(),np=r.u(),nc=r.u(),nf=r.u();
                require(np<=8192&&nc%4==0&&nf<=4096,"Invalid DTS mesh counts");
                auto indices=r.words(ni);std::vector<Vec3> points;points.reserve(np);
                for(unsigned i=0;i<np;++i)points.push_back(r.point());
                std::vector<std::array<uint16_t,8>> surfaces(nc/4);
                for(auto& s:surfaces)for(auto& v:s)v=uint16_t(r.u());
                ModelMesh mesh;mesh.bone=bone;mesh.id=id;
                for(unsigned i=0;i<nf;++i) {
                    unsigned type=r.u(4),bytes=r.u(4);size_t start=r.at;
                    require(type==0x140001||type==0x140002||type==0x140003||type==0x140009||type==0x14000f,"Unsupported DTS polygon");
                    unsigned normal=r.u(),center=r.u(),count=r.u(),first=r.u();
                    require(normal<points.size()&&center<points.size()&&count>=2&&count<=64&&first+count<=indices.size(),"Invalid DTS polygon indices");
                    ModelFace face;face.textured=type==0x14000f;
                    if(type!=0x140001) {
                        unsigned material=r.u();require(material%4==0&&material/4<surfaces.size(),"Invalid DTS surface reference");
                        face.material=surfaces[material/4][0];face.flags=surfaces[material/4][1];
                    }
                    if(type==0x140009)r.u(); // source Gouraud normal-list; render flat original facets
                    require(r.at-start==bytes,"Unsupported DTS polygon payload");
                    // Reverse the recorded winding, as in the verified ES2 exporter.
                    for(unsigned v=0;v<count;++v){auto index=indices[first+count-1-v];require(index<points.size(),"Invalid DTS vertex reference");face.vertices.push_back(points[index]);}
                    face.normal=normalized(points[normal]);
                    if(length(face.normal)<0.9f&&count>=3)
                        face.normal=normalized(cross(face.vertices[1]-face.vertices[0],face.vertices[2]-face.vertices[0]));
                    if(face.textured)require(count==3||count==4,"Unsupported textured DTS polygon");
                    // Plain TSPoly objects are nonvisual markers. Cell variant 0 is
                    // textured; 1 is its untextured duplicate; 2 is a hit/debris proxy.
                    if(visible&&type!=0x140001&&count>=3)mesh.faces.push_back(std::move(face));
                }
                if(tag==0x14000a){auto n=r.u();r.skip(size_t(n)*10);}
                // Original named attachment/camera planes are not armour. These
                // IDs are the head/camera/weapon locators in the ES2 bone table.
                bool locator=id==8||id==9||id==10||id==11||id==66||id==77;
                if(!mesh.faces.empty()&&!locator)shape.meshes.push_back(std::move(mesh));
            } else if(tag==0x140007||tag==0x140008||tag==0x14000b||tag==0x14000c||tag==0x140015||tag==0x1e0003) {
                unsigned count=r.u();require(count<=4096,"Excessive DTS part count");
                for(unsigned i=0;i<count;++i)record(r,depth+1,visible&&(tag!=0x14000b||i==0));
                if(tag==0x140015){auto n=r.u();r.skip(size_t(n)*16);}
                else if(tag==0x14000b)r.u();
                else if(tag==0x14000c)r.skip(r.end-r.at);
                else if(tag==0x140008||tag==0x1e0003){auto nt=r.u(),ns=r.u();r.words(ns);r.words(nt);if(tag==0x1e0003)record(r,depth+1,false);}
            } else require(tag==0x140005,"Unsupported DTS part type");
        }
        require(r.at==r.end,"Unconsumed DTS record data");
    }
    void sequence(Reader& outer) {
        auto tag=outer.u(4),bytes=outer.u(4);require(tag==0x1e0001||tag==0x1e0004,"Unsupported DTS sequence");
        require(bytes<=outer.end-outer.at,"Truncated DTS sequence");Reader r{outer.b,outer.at,outer.at+bytes};outer.at=r.end;
        ModelSequence s;s.duration=uint16_t(r.u());r.skip(4);auto frames=r.u();require(frames>0&&frames<=1024,"Invalid DTS frame count");
        unsigned duration=0;
        for(unsigned f=0;f<frames;++f){auto tick=r.u();require(tick>0,"Zero-length DTS frame");s.ticks.push_back(uint16_t(tick));duration+=tick;r.skip(4);}
        require(duration==s.duration,"DTS frame durations do not match sequence");
        s.parts=r.words(r.u());s.indices=r.words(s.parts.size()*frames);
        require(r.at==r.end,"Unconsumed DTS sequence");shape.sequences.push_back(std::move(s));
    }
};
ModelMatrix matrix(const ModelTransform& t) {
    ModelMatrix x,y,z,translation;
    float c=std::cos(t.rotation.x),s=std::sin(t.rotation.x);x.v[5]=c;x.v[6]=s;x.v[9]=-s;x.v[10]=c;
    c=std::cos(t.rotation.y);s=std::sin(t.rotation.y);y.v[0]=c;y.v[2]=-s;y.v[8]=s;y.v[10]=c;
    c=std::cos(t.rotation.z);s=std::sin(t.rotation.z);z.v[0]=c;z.v[1]=s;z.v[4]=-s;z.v[5]=c;
    translation.v[12]=t.translation.x;translation.v[13]=t.translation.y;translation.v[14]=t.translation.z;
    return translation*z*y*x;
}
std::vector<ModelMatrix> hierarchy(const HercShape& shape,const std::vector<ModelTransform>& local) {
    std::vector<ModelMatrix> out(local.size());std::vector<bool> done(local.size());
    auto visit=[&](auto&& self,int i)->void {if(done[i])return;int parent=shape.parents[i];if(parent>=0){self(self,parent);out[i]=out[parent]*matrix(local[i]);}else out[i]=matrix(local[i]);done[i]=true;};
    for(size_t i=0;i<local.size();++i)visit(visit,int(i));
    return out;
}
ModelTransform sample(const HercShape& shape,int bone,double phase) {
    const auto& seq=shape.sequences[1];auto part=std::find(seq.parts.begin(),seq.parts.end(),bone);
    if(part==seq.parts.end())return shape.transforms[shape.defaults[bone]];
    float tick=float(phase-std::floor(phase))*seq.duration;size_t frame=0;
    while(frame+1<seq.ticks.size()&&tick>=seq.ticks[frame]){tick-=seq.ticks[frame];++frame;}
    size_t p=size_t(part-seq.parts.begin()),next=(frame+1)%seq.ticks.size();float t=tick/seq.ticks[frame];
    auto a=shape.transforms[seq.indices[frame*seq.parts.size()+p]],b=shape.transforms[seq.indices[next*seq.parts.size()+p]];
    return {a.rotation+(b.rotation-a.rotation)*t,a.translation+(b.translation-a.translation)*t};
}
}
Vec3 ModelMatrix::point(Vec3 p) const {return {v[0]*p.x+v[4]*p.y+v[8]*p.z+v[12],v[1]*p.x+v[5]*p.y+v[9]*p.z+v[13],v[2]*p.x+v[6]*p.y+v[10]*p.z+v[14]};}
ModelMatrix ModelMatrix::operator*(const ModelMatrix& b) const {
    ModelMatrix out;out.v.fill(0);for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)out.v[c*4+r]+=v[k*4+r]*b.v[c*4+k];return out;
}
HercShape decodeHercShape(const Bytes& b) {
    require(b.size()<=4*1024*1024,"DTS exceeds model size limit");Reader r{b,0,b.size()};HercShape result;
    unsigned lod=0;
    while(r.at<r.end){Parser p;p.record(r);auto& s=p.shape;
        require((lod>0||(s.defaults.size()==12&&s.sequences.size()==8))&&!s.meshes.empty(),"Unsupported HERC rig (proof pair requires 12 bones / 8 sequences)");
        for(auto v:s.defaults)require(v<s.transforms.size(),"Invalid DTS default transform");
        for(size_t i=0;i<s.parents.size();++i){int bone=int(i);unsigned steps=0;while(bone>=0){require(++steps<=s.parents.size(),"Cyclic DTS hierarchy");bone=s.parents[bone];}}
        for(const auto& seq:s.sequences){for(auto v:seq.parts)require(v<s.defaults.size(),"Invalid DTS animated bone");for(auto v:seq.indices)require(v<s.transforms.size(),"Invalid DTS keyframe transform");}
        for(const auto& mesh:s.meshes)require(mesh.bone>=0&&mesh.bone<int(s.defaults.size()),"Invalid DTS mesh transform");
        if(lod==0)result=std::move(s);
        require(++lod<=16,"Excessive DTS detail levels");
    }
    require(lod>0,"Empty DTS shape");result.detailLevels=lod;
    auto pose=result.pose(Gait{});float inf=std::numeric_limits<float>::infinity();result.minimum={inf,inf,inf};result.maximum={-inf,-inf,-inf};
    for(const auto& m:result.meshes)for(const auto& f:m.faces)for(auto v:f.vertices){v=pose[m.bone].point(v);result.minimum={std::min(result.minimum.x,v.x),std::min(result.minimum.y,v.y),std::min(result.minimum.z,v.z)};result.maximum={std::max(result.maximum.x,v.x),std::max(result.maximum.y,v.y),std::max(result.maximum.z,v.z)};}
    return result;
}
std::vector<ModelMatrix> HercShape::pose(const Gait& gait,float torso,float pitch,float leftGround,float rightGround) const {
    std::vector<ModelTransform> local;for(auto i:defaults)local.push_back(transforms[i]);
    float blend=clamp(gait.blend,0,1);
    // Source walk: left swing frames 0..3, right swing 4..7. Remap each leg's
    // stance/swing intervals to the existing simulation's 60/40 contact cycle.
    // No independent animation clock and no sound-event changes.
    for(size_t i=1;i<local.size();++i) {
        int side=(i==2||i==5||i==6||i==9)?-1:(i==3||i==7||i==8||i==10)?1:0;
        double p=gait.phase+(side>0?0.5:0);p-=std::floor(p);
        double source=p<0.6?0.375+p/0.6*0.625:(p-0.6)/0.4*0.375;
        auto animated=sample(*this,int(i),source+(side>0?0.5:0));
        local[i].rotation+=(animated.rotation-local[i].rotation)*blend;
        local[i].translation+=(animated.translation-local[i].translation)*blend;
    }
    local[4].rotation.z-=torso;local[11].rotation.x+=pitch;
    auto matrices=hierarchy(*this,local);
    for(int side:{-1,1}) {
        float low=std::numeric_limits<float>::infinity();
        for(const auto& m:meshes)if(m.bone==(side<0?5:7)||m.bone==(side<0?6:8))
            for(const auto& f:m.faces)for(auto v:f.vertices)low=std::min(low,matrices[m.bone].point(v).z);
        require(std::isfinite(low),"Unsupported HERC rig: missing sole geometry");
        float ground=(side<0?leftGround:rightGround)/worldScale;
        // Keep planted sole geometry on the same ground used by locomotion. The
        // original keyframes contain small sole penetrations, especially Achilles.
        float correction=gait.foot(side).planted?ground-low:std::max(0.0f,ground-low);
        local[side<0?2:3].translation.z+=correction;
    }
    return hierarchy(*this,local);
}
ModelPalette decodeModelPalette(const Bytes& b) {
    ModelPalette out;out.colors=decodePalette(b);Reader r{b,1036,b.size()};auto count=r.u(4);
    require(count==256,"Unsupported model palette remap count");
    // WORLD0 contains 256 counted palette-index ramps; this consumes its entire
    // tail exactly. Keep these original colors instead of exporter RGB guesses.
    for(unsigned i=0;i<count;++i){auto n=r.u();require(n>0&&n<=256,"Invalid model palette ramp");auto ramp=r.words(n);for(auto index:ramp)require(index<256,"Invalid model palette remap");out.ramps.push_back(std::move(ramp));}
    require(r.at==r.end,"Unsupported model palette tail");return out;
}
std::array<uint8_t,3> ModelPalette::surface(uint16_t index,uint16_t flags) const {
    require(index<256,"Invalid model color");
    require(flags==0||flags==1024,"Unsupported model color flags");
    return colors[flags==1024?ramps.at(index).back():index];
}
HercAsset loadHercAsset(const std::filesystem::path& dir,const std::string& name) {
    require(name=="TOMAHAWK"||name=="ACHILLES","HERC not yet verified");
    HercAsset out;out.shape=decodeHercShape(readFile(dir/(name+".DTS")));
    auto dat=readFile(dir/(name+".DAT"));require(dat.size()==216,"Unsupported HERC definition");
    Reader r{dat,148,dat.size()};auto skin=r.u();require(skin==(name=="TOMAHAWK"?1:3),"Unexpected HERC skin binding");
    out.textures=decodeBitmaps(readFile(dir/(skin==1?"MEDIUM.DBA":"ENEMY.DBA")));
    out.palette=decodeModelPalette(readFile(dir/"WORLD0.DPL"));
    for(const auto& m:out.shape.meshes)for(const auto& f:m.faces){if(f.textured)require(f.material<out.textures.size(),"Missing original model texture");else out.palette.surface(f.material,f.flags);}
    return out;
}
}
