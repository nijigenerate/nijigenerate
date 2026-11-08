// Copyright (c) 2020-2023 Inochi2D Project, 2024 nijigenerate Project. BSD-2-Clause.
#include "atlas.hpp"
#include <common/object_key_policy.hpp>
#include <string>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/backend/vulkan/backend.hpp>
#include <math/veca.hpp>
#include <algorithm>
#include <bit>
#include <memory>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <source_location>
#include <new>
#include <stdexcept>

namespace nijigenerate::atlas::atlas {
namespace {
std::unique_ptr<int[]> allocatePackedIndices(std::size_t count) {
    try {
        return std::make_unique<int[]>(count);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
void prepareAtlasClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Atlas),
            nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("nijigenerate.atlas.atlas.Atlas"));
        return true;
    }();
    (void)registered;
}
}
// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0;
// retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
nicxlive::core::d_runtime::RetainedString Atlas::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Atlas::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Atlas::opEquals(std::shared_ptr<Atlas> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int Atlas::opCmp(std::shared_ptr<Atlas>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
void Atlas::prepareSourceClassInfo() { prepareAtlasClassInfo(); }
bool ngAtlasObjectEquals(std::shared_ptr<Atlas> left, std::shared_ptr<Atlas> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right), &prepareAtlasClassInfo);
}
namespace {
thread_local std::shared_ptr<Texture> currCanvas;
void setCanvas(const std::shared_ptr<Texture>& canvas) { currCanvas=canvas; }
void renderToTexture(const std::shared_ptr<Texture>& texture,Rect where,Rect uv,
    BlendMode mode=BlendMode::Normal,float opacity=1) {
    using namespace nicxlive::core::math;
    const auto width=currCanvas->width(),height=currCanvas->height();
    const float l=where.x,r=l+where.w,t=where.y,b=t+where.h;
    const std::array<Vec3,4> positions{{{l,t,0},{l,b,0},{r,t,0},{r,b,0}}};
    const std::array<Vec2,4> uvs{{{uv.x,uv.y},{uv.x,uv.y+uv.h},{uv.x+uv.w,uv.y},{uv.x+uv.w,uv.y+uv.h}}};
    const std::array<uint32_t,6> indices{0,1,2,2,1,3};
    // A positive-height Vulkan offscreen viewport maps clip -Y to its top row.
    const auto projection=Mat4::orthographic(0,width,0,height,-1,1);
    auto& renderer=backend::vulkan::backend();
    renderer.pushTarget(currCanvas->backendId(),{},false);
    try { renderer.drawMesh(texture->backendId(),positions,uvs,indices,projection,{1,1,1,opacity},{},false,mode); }
    catch(...) { renderer.popTarget(); throw; }
    renderer.popTarget();
}
}
void incInitAtlassing() {}
void ngShutdownAtlassing() { currCanvas.reset(); }
std::shared_ptr<Atlas> Atlas::create(std::size_t atlasSize, int padding, float scale) {
    return ngCreateDerived<Atlas>(atlasSize, padding, scale);
}
Atlas::Atlas(std::size_t atlasSize, int padding, float scale) {
    prepareSourceClassInfo();
    initialize(atlasSize, padding, scale);
}
void Atlas::initialize(std::size_t atlasSize, int padding, float scale) {
    this->padding = padding;
    this->scale = scale;
    resize(atlasSize);
    const auto initialized = allocatePackedIndices(packedIndices.size());
    std::copy_n(initialized.get(), packedIndices.size(), packedIndices.begin());
}
bool Atlas::pack(std::shared_ptr<Part> p, float fscale) {
    const auto lifetime = weak_from_this().lock();
    const auto mesh = compat::ngRequireObject(p)->nicxlive::core::nodes::Drawable::getMesh();
    float startX = 0, startY = 0, endX = 0, endY = 0;
    fscale *= scale;
    float x = 1, y = 1, z = 0, w = 0;
    for (const auto uv : mesh.uvs) {
        if (uv.x < x) x = uv.x;
        if (uv.y < y) y = uv.y;
        if (uv.x > z) z = uv.x;
        if (uv.y > w) w = uv.y;
    }
    if (x < 0) { startX = -x; x = 0; }
    if (y < 0) { startY = -y; y = 0; }
    if (z > 1) { endX = z - 1; z = 1; }
    if (w > 1) { endY = w - 1; w = 1; }
    const Rect uvRect{x, y, z-x, w-y};
    const Rect bounding{x-startX, y-startY, z-x+startX+endX, w-y+startY+endY};
    const auto paddedSize = [this](float extent) {
        const auto paddingTwice = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(padding)*2u);
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(compat::ngFloatToInt32(extent))
            + static_cast<std::uint32_t>(paddingTwice));
    };
    const auto sizeWidthOwner = p->textures[0];
    const auto sizeWidth = compat::ngRequireObject(sizeWidthOwner)->width();
    const auto packedWidth = paddedSize((sizeWidth*bounding.w)*fscale);
    const auto sizeHeightOwner = p->textures[0];
    const auto sizeHeight = compat::ngRequireObject(sizeHeightOwner)->height();
    const packer::Vec2i size{packedWidth, paddedSize((sizeHeight*bounding.h)*fscale)};
    const auto packerOwner = packer;
    const auto packed = compat::ngRequireObject(packerOwner)->packTexture(size);
    if (packed == packer::Vec4i{}) return false;
    const float ax = packed.x, ay = packed.y, aw = packed.z, ah = packed.w;
    const auto startWidthOwner = p->textures[0];
    startX *= compat::ngRequireObject(startWidthOwner)->width()*fscale;
    const auto startHeightOwner = p->textures[0];
    startY *= compat::ngRequireObject(startHeightOwner)->height()*fscale;
    const auto endWidthOwner = p->textures[0];
    endX *= compat::ngRequireObject(endWidthOwner)->width()*fscale;
    const auto endHeightOwner = p->textures[0];
    endY *= compat::ngRequireObject(endHeightOwner)->height()*fscale;
    for (std::size_t i = 0; i < p->textures.size(); ++i) {
        auto& texture = p->textures[i];
        if (texture) {
            setCanvas(textures[i]);
            packedIndices[i] = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(packedIndices[i]) + 1u);
            const auto paddingTwice = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(padding)*2u);
            const Rect where{ax+startX+padding, ay+startY+padding,
                aw-paddingTwice-startX-endX, ah-paddingTwice-startY-endY};
            const Rect mapping{ax+padding, ay+padding, aw-paddingTwice, ah-paddingTwice};
            const auto uuid = p->getUUID();
            mappings.set(uuid, mapping);
            renderToTexture(texture, where, uvRect);
        }
    }
    return true;
}
void Atlas::renderOnTop(std::uint32_t index, std::shared_ptr<Texture> texture, Rect where,
    Rect uvs, BlendMode mode, float opacity) {
    const auto lifetime = weak_from_this().lock();
    if (texture && index < textures.size()) {
        setCanvas(textures[index]);
        renderToTexture(texture, where, uvs, mode, opacity);
    }
}
void Atlas::resize(std::size_t size) {
    const auto lifetime = weak_from_this().lock();
    packer = std::allocate_shared<packer::TexturePacker>(
        nicxlive::core::d_runtime::SourceAllocator<packer::TexturePacker>{},
        packer::Vec2i{static_cast<int>(size), static_cast<int>(size)});
    for (std::size_t i = 0; i < textures.size(); ++i) {
        if (const auto texture = textures[i]) texture->dispose();
        textures[i] = Texture::createSource(static_cast<int>(size), static_cast<int>(size), i == 0 ? 4 : 3);
        auto& renderer=backend::vulkan::backend();
        renderer.pushTarget(textures[i]->backendId()); renderer.popTarget();
    }
}
void Atlas::finalize() {
    const auto lifetime = weak_from_this().lock();
    for (auto texture : textures) compat::ngRequireObject(texture)->genMipmap();
}
void Atlas::clear() {
    const auto lifetime = weak_from_this().lock();
    mappings.clear();
    const auto packerOwner = packer;
    compat::ngRequireObject(packerOwner)->clear();
    const auto initialized = allocatePackedIndices(packedIndices.size());
    std::copy_n(initialized.get(), packedIndices.size(), packedIndices.begin());
}
}
