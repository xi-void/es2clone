#include "original_graphics.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace siege::original;
namespace {
int checks=0;
void expect(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
void put(Bytes& b,size_t at,uint32_t value,size_t length=4) {for(size_t i=0;i<length;++i)b[at+i]=uint8_t(value>>(8*i));}
Bytes bitmap(uint16_t w,uint16_t h,uint8_t color=0) {
    size_t size=21+size_t(w)*h;Bytes b(size+(size&1),0);
    put(b,0,0x28000e);put(b,4,13+size_t(w)*h);put(b,8,h,2);put(b,10,w,2);b[12]=8;put(b,15,size_t(w)*h);
    std::fill(b.begin()+21,b.begin()+size,color);return b;
}
Bytes array(const std::vector<Bytes>& frames) {
    Bytes b(12,0);put(b,0,0x280001);put(b,8,frames.size());
    for(const auto& frame:frames)b.insert(b.end(),frame.begin(),frame.end());
    put(b,4,b.size()-8);return b;
}
void rejects(const std::function<void()>& test,const char* message) {
    bool rejected=false;try{test();}catch(const std::runtime_error&){rejected=true;}expect(rejected,message);
}
}
int main(int argc,char** argv) {
    try {
        Bytes pal(1040,0);put(pal,0,0x28000f);put(pal,4,1032);put(pal,8,256);
        pal[12+7*4]=63;pal[13+7*4]=24;pal[14+7*4]=1;
        auto p=decodePalette(pal);expect(p[7]==std::array<uint8_t,3>{252,96,4},"Six-bit RGB channel order and expansion");
        auto raw=bitmap(3,2,7);raw[21]=0;
        auto decoded=decodeBitmaps(raw);expect(decoded.size()==1&&decoded[0].width==3&&decoded[0].height==2,"Standalone DBM dimensions");
        expect(decoded[0].indices==Bytes({0,7,7,7,7,7}),"Packed pixels start at byte 21 with no per-row padding");
        auto rgba=decoded[0].rgba(p,true);expect(rgba[3]==0&&rgba[7]==255&&rgba[4]==252,"Cockpit index zero transparency preserves colored pixels");
        expect(decoded[0].rgba(p,false)[3]==255,"Inspection can retain opaque black");
        auto frames=array({raw,bitmap(1,1,3),bitmap(2,2,5)});
        auto images=decodeBitmaps(frames);expect(images.size()==3&&images[1].indices[0]==3&&images[2].indices.size()==4,"Array frame alignment for odd and even payloads");
        for(size_t length=0;length<raw.size();++length) {
            Bytes truncated(raw.begin(),raw.begin()+length);
            rejects([&]{decodeBitmaps(truncated);},"Truncated bitmap rejected");
        }
        for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{0,0},{4,0},{8,0},{10,65535},{12,16},{13,1},{15,999},{19,1}}) {
            auto bad=raw;put(bad,entry.first,entry.second,entry.first==8||entry.first==10||entry.first==19?2:entry.first==12||entry.first==13?1:4);
            rejects([&]{decodeBitmaps(bad);},"Unsupported/malformed bitmap rejected");
        }
        for(uint32_t count:{0u,4097u,0xffffffffu}){auto bad=frames;put(bad,8,count);rejects([&]{decodeBitmaps(bad);},"Unbounded array count rejected");}
        auto extra=raw;extra.push_back(0);rejects([&]{decodeBitmaps(extra);},"Unexpected trailing bytes rejected");
        for(size_t offset:{size_t(0),size_t(4),size_t(8)}){auto bad=pal;put(bad,offset,0);rejects([&]{decodePalette(bad);},"Invalid palette header rejected");}
        auto bad=pal;bad[12]=64;rejects([&]{decodePalette(bad);},"Unknown palette precision rejected");
        bad=pal;bad.resize(1039);rejects([&]{decodePalette(bad);},"Truncated palette rejected");
        rejects([&]{readFile("/definitely-absent/openearthsiege-resource");},"Missing resource reported");
        if(argc>1) {
            auto root=std::filesystem::path(argv[1]);
            p=decodePalette(readFile(root/"dpl/COCKPIT.DPL"));
            int originals=0;
            for(const auto& file:std::filesystem::directory_iterator(root/"hb0")) {
                auto f=decodeBitmaps(readFile(file.path()));
                expect(f.size()==1&&f[0].width==640&&f[0].height==480,"Actual original HB0 dimensions");
                auto pixels=f[0].rgba(p,true);expect(pixels.size()==640*480*4,"Actual original RGBA conversion");++originals;
            }
            expect(originals==9,"All nine supplied cockpit frames decoded");
        }
        std::cout<<"PASS: "<<checks<<" original graphics checks\n";
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
