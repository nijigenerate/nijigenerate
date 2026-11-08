#include "detail/psddepthmap_gpu.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/backend/active.hpp>
#include <nodes/common.hpp>
#include <algorithm>
#include <bit>
#include <array>
#include <string>
#include <utility>

namespace nijigenerate::windows::psddepthmap::detail {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec4;
namespace { template<class F> struct Exit { F fn; ~Exit() noexcept(false) { fn(); } }; }
namespace {
void preparePsdDepthGpuClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<PsdDepth3DAdjustGpuRenderer>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        ngRegisterObjectTypeNames<PsdDepth3DAdjustGpuRenderer>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(PsdDepth3DAdjustGpuRenderer), name);
        return true;
    }();
    (void)registered;
}
}
// Inherited object.Object defaults; Digital Mars, BSL-1.0.
compat::RetainedString PsdDepth3DAdjustGpuRenderer::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    preparePsdDepthGpuClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t PsdDepth3DAdjustGpuRenderer::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool PsdDepth3DAdjustGpuRenderer::opEquals(std::shared_ptr<PsdDepth3DAdjustGpuRenderer> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int PsdDepth3DAdjustGpuRenderer::opCmp(std::shared_ptr<PsdDepth3DAdjustGpuRenderer>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    preparePsdDepthGpuClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = helpers::allocatePsdDepthMemory([&] {
        std::string text = "need opCmp for class ";
        text += className.view();
        return compat::RetainedString(std::move(text));
    });
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool PsdDepth3DAdjustGpuRenderer::ensureTarget(int w,int h) {
    w=std::max(1,w); h=std::max(1,h);
    if(texture && (width!=w || height!=h)) { texture->dispose(); texture.reset(); }
    width=w; height=h;
    if(!texture) texture=nicxlive::core::Texture::createSource(w,h,4,false,false);
    return bool(texture);
}
std::shared_ptr<nicxlive::core::Texture> PsdDepth3DAdjustGpuRenderer::renderScoped(
    int w,int h,const void* context,void (*invoke)(const void*)) {
    const auto receiver=weak_from_this().lock();
    if(!ensureTarget(w,h)) return {};
    auto& renderer=backend::selected::backend();
    renderer.pushTarget(texture->backendId(),{.08f,.09f,.10f,1});
    try { if(!invoke) compat::ngFailNullAccess(false); invoke(context); }
    catch(...) { renderer.popTarget(); throw; }
    renderer.popTarget(); return texture;
}
bool PsdDepth3DAdjustGpuRenderer::capturePreview(int& w,int& h,compat::Slice<uint8_t>& rgba) {
    const auto receiver=weak_from_this().lock(); w=0; h=0; rgba={};
    if(!texture || width<=0 || height<=0) return false;
    auto pixels=backend::selected::backend().readTexture(texture->backendId());
    rgba.resize(pixels.size());
    // Preserve the preview's document-row order, independent of render targets.
    const auto rowBytes=size_t(width)*4;
    for(int y=0;y<height;++y) std::copy_n(pixels.data()+size_t(y)*rowBytes,rowBytes,rgba.data()+size_t(y)*rowBytes);
    w=width; h=height; return true;
}
void PsdDepth3DAdjustGpuRenderer::drawLayer(std::shared_ptr<nicxlive::core::Texture> layerTexture,
    PsdDepth3DAdjustMesh& mesh,float centerX,float centerY,float yaw,float pitch,float zoom,
    Vec2 pan,float depthDisplayScale,PsdDepthLayerEdit baked,PsdDepthLayerEdit current) {
    const auto receiver=weak_from_this().lock();
    if(!layerTexture || mesh.vertexData.empty() || mesh.indices.empty()) return;
    using Vec3=nicxlive::core::math::Vec3;
    using Mat4=nicxlive::core::math::Mat4;
    const auto cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    std::vector<Vec3> positions; std::vector<Vec2> uvs;
    positions.reserve(mesh.vertexData.size()/5); uvs.reserve(mesh.vertexData.size()/5);
    for(size_t i=0;i+4<mesh.vertexData.size();i+=5) {
        auto x=mesh.vertexData[i]-centerX,y=mesh.vertexData[i+1]-centerY,depth=mesh.vertexData[i+2];
        if(std::abs(baked.zScale)>1e-6f) depth=(depth-baked.zOffset)*(current.zScale/baked.zScale)+current.zOffset;
        else depth+=current.zOffset-baked.zOffset;
        auto z=-depth*depthDisplayScale,rx=x*cy+z*sy,rz=-x*sy+z*cy;
        auto ry=y*cp-rz*sp,cameraDepth=y*sp+rz*cp;
        auto sx=float(width)*.5f+rx*zoom+pan.x,syScreen=float(height)*.5f+ry*zoom+pan.y;
        positions.push_back({sx/width*2-1,syScreen/height*2-1,
            std::clamp(cameraDepth/std::max(depthDisplayScale*2,1.f),-1.f,1.f)});
        uvs.push_back({mesh.vertexData[i+3],mesh.vertexData[i+4]});
    }
    backend::selected::backend().drawMesh(layerTexture->backendId(),positions,uvs,
        {mesh.indices.data(),mesh.indices.size()},Mat4::identity(),{1,1,1,1},{},true);
}
void PsdDepth3DAdjustGpuRenderer::dispose() {
    const auto receiver=weak_from_this().lock();
    if(texture) { texture->dispose(); texture.reset(); }
}
}
