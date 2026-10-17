#include "original_graphics.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace siege::original {
namespace {
constexpr size_t maxFile=32*1024*1024, maxPixels=16*1024*1024;
constexpr uint32_t dbmTag=0x0028000e, dbaTag=0x00280001, dplTag=0x0028000f;
void require(bool valid, const char* message) {
    if(!valid) throw std::runtime_error(message);
}
uint32_t le(const Bytes& b, size_t offset, size_t length=4) {
    require(offset<=b.size() && length<=b.size()-offset,"Truncated original graphics resource");
    uint32_t value=0;
    for(size_t i=0;i<length;++i) value|=uint32_t(b[offset+i])<<(i*8);
    return value;
}
}
Bytes readFile(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f) throw std::runtime_error("Cannot open original graphics: "+path.string());
    auto length=f.tellg();
    require(length>=0 && uint64_t(length)<=maxFile,"Original graphics resource exceeds size limit");
    Bytes b(static_cast<size_t>(length));f.seekg(0);
    if(!f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size())))
        throw std::runtime_error("Cannot read original graphics: "+path.string());
    return b;
}
Palette decodePalette(const Bytes& b) {
    require(b.size()<=maxFile && le(b,0)==dplTag,"Unsupported DPL palette signature");
    require(le(b,4)==b.size()-8 && le(b,8)==256 && b.size()>=1040,"Invalid DPL size or color count");
    Palette palette{};
    // Four-byte RGB/flags entries begin at byte 12. Flags are not RGBA alpha.
    // This six-bit DAC expansion agrees with the reviewed Open Siege reader.
    for(size_t i=0;i<256;++i) for(size_t c=0;c<3;++c) {
        auto value=b[12+i*4+c];
        require(value<=63,"Unsupported DPL color precision");
        palette[i][c]=uint8_t(value*4);
    }
    // Additional shade/remap records are retained in the source, not interpreted.
    return palette;
}
std::vector<Bitmap> decodeBitmaps(const Bytes& b) {
    require(b.size()<=maxFile,"Original bitmap resource exceeds size limit");
    uint32_t tag=le(b,0),count=1;
    size_t offset=0,totalPixels=0;
    if(tag==dbaTag) {
        require(le(b,4)==b.size()-8,"Invalid DBA container length");
        count=le(b,8);offset=12;
        require(count>0 && count<=4096 && count<=(b.size()-12)/22,"Invalid DBA frame count");
    } else require(tag==dbmTag,"Unsupported original bitmap signature");
    std::vector<Bitmap> result;result.reserve(count);
    for(uint32_t frame=0;frame<count;++frame) {
        require(le(b,offset)==dbmTag,"Unsupported bitmap array member");
        uint32_t size=le(b,offset+4),height=le(b,offset+8,2),width=le(b,offset+10,2);
        require(le(b,offset+12,1)==8 && le(b,offset+13,2)==0 && le(b,offset+19,2)==0,
                "Unsupported DBM encoding or extra data");
        size_t pixels=size_t(width)*height;
        require(width>0 && height>0 && width<=4096 && height<=4096 && pixels<=maxPixels-totalPixels,
                "Invalid or excessive DBM dimensions");
        require(le(b,offset+15)==pixels && size==13+pixels,"Inconsistent DBM payload size");
        require(offset+21<=b.size() && pixels<=b.size()-(offset+21),"Truncated DBM pixels");
        Bitmap image;image.width=uint16_t(width);image.height=uint16_t(height);
        image.indices.assign(b.begin()+offset+21,b.begin()+offset+21+pixels);
        result.push_back(std::move(image));totalPixels+=pixels;
        offset+=21+pixels;
        // Each serialized object is padded to a two-byte boundary, not each row.
        if(offset&1) {require(offset<b.size(),"Missing DBM alignment byte");++offset;}
    }
    require(offset==b.size(),"Unexpected trailing bitmap data");
    return result;
}
Bytes Bitmap::rgba(const Palette& palette,bool transparentZero) const {
    require(indices.size()==size_t(width)*height && indices.size()<=maxPixels,"Invalid decoded bitmap");
    Bytes output(indices.size()*4);
    for(size_t i=0;i<indices.size();++i) {
        const auto& rgb=palette[indices[i]];
        std::copy(rgb.begin(),rgb.end(),output.begin()+i*4);
        output[i*4+3]=transparentZero && indices[i]==0 ? 0 : 255;
    }
    return output;
}
}
