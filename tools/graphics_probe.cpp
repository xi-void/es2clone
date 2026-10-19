#include "original_graphics.hpp"
#include <fstream>
#include <iostream>
#include <string>

int main(int argc,char** argv) {
    if(argc<3 || argc>4) {
        std::cerr<<"Usage: graphics_probe BITMAP DPL_PALETTE [first-frame.ppm]\n"
                 <<"       graphics_probe --indices-json BITMAP\n";return 2;
    }
    try {
        if(argc==3 && std::string(argv[1])=="--indices-json") {
            // Offline consumers need exact indices, not palette-expanded colors.
            const auto frames=siege::original::decodeBitmaps(siege::original::readFile(argv[2]));
            const auto& frame=frames.front();
            std::cout<<"{\"frame_count\":"<<frames.size()<<",\"width\":"<<frame.width
                     <<",\"height\":"<<frame.height<<",\"indices\":[";
            for(size_t i=0;i<frame.indices.size();++i) {
                if(i)std::cout<<',';
                std::cout<<unsigned(frame.indices[i]);
            }
            std::cout<<"]}\n";
            return 0;
        }
        auto palette=siege::original::decodePalette(siege::original::readFile(argv[2]));
        auto frames=siege::original::decodeBitmaps(siege::original::readFile(argv[1]));
        std::cout<<frames.size()<<" frames; first "<<frames.front().width<<'x'<<frames.front().height<<'\n';
        if(argc==4) {
            // Inspection output is opaque: transparency is specific to cockpit use.
            std::ofstream out(argv[3],std::ios::binary);
            const auto& frame=frames.front();out<<"P6\n"<<frame.width<<' '<<frame.height<<"\n255\n";
            for(auto index:frame.indices)out.write(reinterpret_cast<const char*>(palette[index].data()),3);
            if(!out)throw std::runtime_error("Could not write preview");
        }
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
