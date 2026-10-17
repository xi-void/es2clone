#include "renderer.hpp"
#include <iostream>

namespace siege {
void Renderer::destroyOriginalModels() {
    for(auto& model:originalHercs){if(model.lists)glDeleteLists(model.lists,GLsizei(model.asset.shape.meshes.size()));if(!model.textures.empty())glDeleteTextures(GLsizei(model.textures.size()),model.textures.data());model=OriginalHerc{};}
}
bool Renderer::loadOriginalModels(const std::string& directory) {
    destroyOriginalModels();int loaded=0;
    for(size_t i=0;i<originalHercs.size();++i) {
        const char* name=i==0?"TOMAHAWK":"ACHILLES";auto& model=originalHercs[i];
        try {
            model.asset=original::loadHercAsset(directory,name);
            model.textures.resize(model.asset.textures.size());glGenTextures(GLsizei(model.textures.size()),model.textures.data());
            glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            for(size_t t=0;t<model.textures.size();++t) {
                const auto& bitmap=model.asset.textures[t];auto rgba=bitmap.rgba(model.asset.palette.colors);
                glBindTexture(GL_TEXTURE_2D,model.textures[t]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,bitmap.width,bitmap.height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            }
            model.lists=glGenLists(GLsizei(model.asset.shape.meshes.size()));if(!model.lists)throw std::runtime_error("OpenGL model allocation failed");
            for(size_t m=0;m<model.asset.shape.meshes.size();++m) {
                glNewList(model.lists+GLuint(m),GL_COMPILE);
                for(const auto& face:model.asset.shape.meshes[m].faces) {
                    if(face.textured){glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,model.textures[face.material]);glColor3f(1,1,1);}
                    else {glDisable(GL_TEXTURE_2D);auto rgb=model.asset.palette.surface(face.material,face.flags);glColor3ub(rgb[0],rgb[1],rgb[2]);}
                    glNormal3f(face.normal.x,face.normal.y,face.normal.z);
                    // Fixed corner mapping from TSTexture4Poly: no modern UV set.
                    constexpr float quad[4][2]={{0,0},{1,0},{1,1},{0,1}};
                    constexpr int tri[3]={2,1,0};
                    glBegin(GL_TRIANGLE_FAN);
                    for(size_t v=0;v<face.vertices.size();++v){if(face.textured){auto uv=quad[face.vertices.size()==3?tri[v]:v];glTexCoord2f(uv[0],1-uv[1]);}auto p=face.vertices[v];glVertex3f(p.x,p.y,p.z);}
                    glEnd();
                }
                glEndList();
            }
            if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("OpenGL original model upload failed");
            std::cout<<"Original HERC: "<<name<<" ("<<model.asset.shape.meshes.size()<<" mesh groups, "<<model.asset.shape.detailLevels<<" source detail levels)\n";++loaded;
        } catch(const std::exception& e) {
            std::cerr<<"Original HERC "<<name<<": "<<e.what()<<"; using procedural fallback\n";
            if(model.lists)glDeleteLists(model.lists,GLsizei(model.asset.shape.meshes.size()));
            if(!model.textures.empty())glDeleteTextures(GLsizei(model.textures.size()),model.textures.data());
            model=OriginalHerc{};
        }
    }
    glDisable(GL_TEXTURE_2D);return loaded==2;
}
bool Renderer::originalMech(const Gait& gait,HercAppearance appearance,float torso,float pitch,Vec3 pos,float yaw,float size,bool flat) {
    int i=appearance==HercAppearance::Tomahawk?0:appearance==HercAppearance::Achilles?1:-1;
    if(i<0||!originalHercs[i].lists)return false;
    const auto& model=originalHercs[i];auto pose=model.asset.shape.pose(gait,torso,pitch);
    if(!flat) {
        float ground[2]{};
        for(int foot=0;foot<2;++foot) {
            Vec3 sole{0,0,1e9f};
            for(const auto& mesh:model.asset.shape.meshes)if(mesh.bone==(foot==0?5:7)||mesh.bone==(foot==0?6:8))
                for(const auto& face:mesh.faces)for(auto p:face.vertices){p=pose[mesh.bone].point(p);if(p.z<sole.z)sole=p;}
            float x=sole.x*original::HercShape::worldScale*size,z=-sole.y*original::HercShape::worldScale*size;
            float wx=pos.x+x*std::cos(yaw)-z*std::sin(yaw),wz=pos.z+x*std::sin(yaw)+z*std::cos(yaw);
            ground[foot]=(terrainHeight(wx,wz)-pos.y)/size;
        }
        pose=model.asset.shape.pose(gait,torso,pitch,ground[0],ground[1]);
    }
    glPushMatrix();constexpr float scale=original::HercShape::worldScale;glScalef(scale,scale,scale);glRotatef(-90,1,0,0);
    for(size_t m=0;m<model.asset.shape.meshes.size();++m){glPushMatrix();glMultMatrixf(pose[model.asset.shape.meshes[m].bone].v.data());glCallList(model.lists+GLuint(m));glPopMatrix();}
    glPopMatrix();glDisable(GL_TEXTURE_2D);return true;
}
}
