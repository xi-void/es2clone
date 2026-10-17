#pragma once
#include "original_graphics.hpp"
#include "simulation.hpp"
#include <filesystem>

namespace siege::original {
// Early 3Space ANShape, not the later Torque DTS format. Source coordinates are
// retained (Z up, +Y forward); the renderer performs the single basis conversion.
struct ModelTransform { Vec3 rotation,translation; };
struct ModelMatrix {
    std::array<float,16> v{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    Vec3 point(Vec3 p) const;
    ModelMatrix operator*(const ModelMatrix& b) const;
};
struct ModelFace {
    std::vector<Vec3> vertices;
    Vec3 normal;
    uint16_t material=0,flags=0;
    bool textured=false;
};
struct ModelMesh { int bone=-1,id=0; std::vector<ModelFace> faces; };
struct ModelSequence {
    std::vector<uint16_t> ticks,parts,indices;
    uint16_t duration=0;
};
struct HercShape {
    std::vector<ModelMesh> meshes;
    std::vector<ModelTransform> transforms;
    std::vector<uint16_t> defaults;
    std::vector<int> parents;
    std::vector<ModelSequence> sequences;
    unsigned detailLevels=0;
    Vec3 minimum,maximum;
    // Shared world scale for both proof models: fits the existing collision and
    // camera envelope without changing their geometry or relative proportions.
    static constexpr float worldScale=0.00625f;
    std::vector<ModelMatrix> pose(const Gait& gait,float torso=0,float pitch=0,
                                 float leftGround=0,float rightGround=0) const;
};
struct ModelPalette {
    Palette colors;
    std::vector<std::vector<uint16_t>> ramps;
    std::array<uint8_t,3> surface(uint16_t index,uint16_t flags) const;
};
struct HercAsset { HercShape shape; ModelPalette palette; std::vector<Bitmap> textures; };
HercShape decodeHercShape(const Bytes& bytes);
ModelPalette decodeModelPalette(const Bytes& bytes);
HercAsset loadHercAsset(const std::filesystem::path& directory,const std::string& name);
}
