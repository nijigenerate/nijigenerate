// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "renderer.hpp"
#include "mesheditor/node_stable_sort.hpp"
#include <nijigenerate/backend/active.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <compat/source_allocator.hpp>
#include <nodes/deformable.hpp>
#include <nodes/composite.hpp>
#include <nodes/drawable.hpp>
#include <texture.hpp>
#include <runtime_state.hpp>
#include <algorithm>
#include <array>
namespace nijigenerate::viewport::depth {
using Vec2=nicxlive::core::math::Vec2;
using Vec3=nicxlive::core::math::Vec3;
using Mat4=nicxlive::core::math::Mat4;
void DepthTextureMeshRenderer::drawSolid(compat::Slice<Vec2> positions,compat::Slice<uint16_t> indices,
    std::shared_ptr<nicxlive::core::math::Camera> camera,nicxlive::core::math::Vec4 color) {
    std::vector<Vec3> vertices; vertices.reserve(positions.size());
    for(auto p:positions) vertices.push_back({p.x,p.y,0});
    std::vector<uint32_t> triangles(indices.begin(),indices.end());
    backend::selected::backend().drawMesh(0,vertices,{},triangles,camera->matrix(),color);
}
void DepthTextureMeshRenderer::draw(std::shared_ptr<nicxlive::core::Texture> texture,compat::Slice<Vec2> positions,
    compat::Slice<Vec2> uvs,compat::Slice<uint16_t> indices,std::shared_ptr<nicxlive::core::math::Camera> camera) {
    if(!texture || positions.empty() || positions.size()!=uvs.size() || indices.empty()) return;
    std::vector<Vec3> vertices; vertices.reserve(positions.size());
    for(auto p:positions) vertices.push_back({p.x,p.y,0});
    std::vector<uint32_t> triangles(indices.begin(),indices.end());
    backend::selected::backend().drawMesh(texture->backendId(),vertices,{uvs.data(),uvs.size()},triangles,camera->matrix());
}
bool DepthTargetOffscreenTextureRenderer::rebuildTexture(Target target) {
    const auto vertices = compat::ngRequireObject(target)->getVertices().toArray(); if (vertices.empty()) return false;
    minPoint = maxPoint = vertices[0];
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        const auto value = vertices[i];
        minPoint.x = std::min(minPoint.x, value.x); minPoint.y = std::min(minPoint.y, value.y);
        maxPoint.x = std::max(maxPoint.x, value.x); maxPoint.y = std::max(maxPoint.y, value.y);
    }
    sourceSize = {std::max(1.f, maxPoint.x - minPoint.x), std::max(1.f, maxPoint.y - minPoint.y)};
    const auto maxSize=backend::selected::context().maxTextureSize();
    if (!ngDepthTargetOffscreenTextureSize(minPoint, maxPoint, maxSize, textureWidth, textureHeight, texturePixelScale)) return false;
    if (texture && (compat::ngRequireObject(texture)->width() != textureWidth
            || compat::ngRequireObject(texture)->height() != textureHeight)) {
        compat::ngRequireObject(texture)->dispose();
        texture.reset();
    }
    if (!texture) texture = nicxlive::core::Texture::createSource(textureWidth, textureHeight, 4, false, false);
    return bool(texture);
}
compat::Slice<DepthTargetOffscreenTextureRenderer::Node> DepthTargetOffscreenTextureRenderer::drawableChildren(Target target) {
    compat::Slice<Node> parts; if (!target) return parts;
    const auto visit = [&](const auto& recurse, Node node) -> void {
        if (compat::ngRequireObject(node)->coverOthers()) {
            const auto children = node->children;
            for (auto child : children) recurse(recurse, child);
        }
        if (auto composite = std::dynamic_pointer_cast<nicxlive::core::nodes::Composite>(node)) {
            if (composite->propagateMeshGroup) parts.append(composite);
        } else if (auto drawable = std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node)) {
            parts.append(drawable);
            const auto children = node->children;
            for (auto child : children) recurse(recurse, child);
        }
    };
    visit(visit, target);
    mesheditor::node::detail::phobosStableNodeSort(parts, [](Node a, Node b) {
        const auto left = compat::ngRequireObject(a)->zSort();
        const auto right = compat::ngRequireObject(b)->zSort();
        return left > right;
    });
    return parts;
}
DepthTargetOffscreenTextureRenderer::~DepthTargetOffscreenTextureRenderer() noexcept(false) { dispose(); }
void DepthTargetOffscreenTextureRenderer::dispose() {
    const auto keepAlive=weak_from_this().lock();
    if (texture) { compat::ngRequireObject(texture)->dispose(); texture.reset(); }
}
std::shared_ptr<nicxlive::core::Texture> DepthTargetOffscreenTextureRenderer::render(Target target) {
    const auto keepAlive = weak_from_this().lock(); if (!target || !rebuildTexture(target)) return {};
    auto& renderer=backend::selected::backend();
    renderer.pushTarget(texture->backendId());
    nicxlive::core::inPushViewport(textureWidth, textureHeight);
    const auto camera = nicxlive::core::inGetCameraHandle();
    compat::ngRequireObject(camera)->scale = texturePixelScale;
    camera->position = {-minPoint.x - sourceSize.x * .5f, -minPoint.y - sourceSize.y * .5f};
    camera->rotation = 0;
    auto transform = std::allocate_shared<Mat4>(nicxlive::core::d_runtime::SourceAllocator<Mat4>{},
        core::math::source_matrix::inverse(target->transform().matrix()));
    target->setOneTimeTransform(transform);
    std::shared_ptr<nicxlive::core::Texture> result;
    compat::ngRunWithScopeExit([&] {
        for (auto part : drawableChildren(target)) compat::ngRequireObject(part)->drawOne();
        result=texture;
    },[&] {
        target->setOneTimeTransform({}); nicxlive::core::inPopViewport();
        renderer.popTarget();
    });
    return result;
}
}
