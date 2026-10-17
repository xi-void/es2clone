#include "original_graphics.hpp"
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
    if(argc<3 || argc>4) {
        std::cerr<<"Usage: graphics_probe BITMAP DPL_PALETTE [first-frame.ppm]\n";return 2;
    }
    try {
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
