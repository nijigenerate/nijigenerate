// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "node.hpp"
#include "../renderer.hpp"
#include "../common/vertex_copy.hpp"
#include <nijigenerate/actions/depth.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/typed_format_vec2.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/node_class_name.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/commands/depth/detail/bone_dirty_providers.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <nodes/grid_deformer.hpp>
#include <texture.hpp>
#include <runtime_state.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <cmath>
#include <algorithm>
#include <bit>
#include <utility>
#include <tuple>

#define NG_DEPTH_NODE_GL(name, type) ::nijigenerate::platform::ngGLBackendProcedure<type>(name)
namespace nijigenerate::viewport::depth::mesheditor::node {
namespace {
template<class T, class... Args> std::shared_ptr<T> allocateDepthNodeObject(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}
template<class... Args> std::string formatted(std::string_view pattern, Args&&... args) {
    const std::array<core::translation::TypedFormatArgument, sizeof...(Args)> values{
        core::translation::ngFormatArgument(std::forward<Args>(args))...};
    return core::translation::ngFormatTyped(pattern, values);
}
}
DepthMeshEditorOne::DepthMeshEditorOne(Grid value, bool buildRenderResources) {
    initializeSource(std::move(value), buildRenderResources);
}
void DepthMeshEditorOne::initializeSource(Grid value, bool buildRenderResources) {
    target = std::move(value);
    depthMapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(target);
    targetRenderer = allocateDepthNodeObject<DepthTargetRenderer>();
    if (buildRenderResources) offscreenTextureRenderer = allocateDepthNodeObject<DepthTargetOffscreenTextureRenderer>();
    const auto targetType = compat::ngNodeDynamicClassName(*compat::ngRequireObject(target));
    const bool hasDepthMapping = depthMapped != nullptr;
    const auto vertexCount = compat::ngRequireObject(target)->getVertices().size();
    const auto deformationCount = compat::ngRequireObject(target)->getDeformation().size();
    log(formatted("editor created: type=%s depthMapped=%s vertices=%s deformation=%s",
        targetType, hasDepthMapping, vertexCount, deformationCount));
    resetFromTarget(); rebuildTopology(); rebuildBounds();
}
DepthMeshEditorOne::~DepthMeshEditorOne() noexcept(false) { dispose(); }
void DepthMeshEditorOne::dispose() {
    const auto keepAlive = weak_from_this().lock();
    if (texture) {
        { const auto disposing = texture; disposing->dispose(); }
        texture.reset();
    }
    if (textureFbo) {
        NG_DEPTH_NODE_GL("glDeleteFramebuffers", PFNGLDELETEFRAMEBUFFERSPROC)(1, &textureFbo);
        textureFbo = 0;
    }
    if (offscreenTextureRenderer) {
        { const auto disposing = offscreenTextureRenderer; disposing->dispose(); }
        offscreenTextureRenderer.reset();
    }
}
void DepthMeshEditorOne::rebuildTexture() {
    const auto keepAlive = weak_from_this().lock();
    rebuildBounds();
    const auto vertices = common::targetview::detail::copyVertices(compat::ngRequireObject(target)->getVertices());
    if (vertices.empty()) {
        if (!loggedEmptyVertices) { log("texture skipped: GridDeformer has no vertices"); loggedEmptyVertices = true; }
        return;
    }
    textureWidth = std::max(1, compat::ngFloatToInt32(std::ceil(maxPoint.x - minPoint.x)));
    textureHeight = std::max(1, compat::ngFloatToInt32(std::ceil(maxPoint.y - minPoint.y)));
    if (texture && ([&] { const auto current = compat::ngRequireObject(texture); return current->width(); }() != textureWidth ||
                    [&] { const auto current = compat::ngRequireObject(texture); return current->height(); }() != textureHeight)) {
        { const auto disposing = compat::ngRequireObject(texture); disposing->dispose(); }
        texture.reset();
    }
    if (!texture) {
        texture = nicxlive::core::Texture::createSource(textureWidth, textureHeight, 4, false, false);
    }
    if (!textureFbo) NG_DEPTH_NODE_GL("glGenFramebuffers", PFNGLGENFRAMEBUFFERSPROC)(1, &textureFbo);
    log(formatted("texture ready: size=%sx%s fbo=%s", textureWidth, textureHeight, textureFbo));
}
void DepthMeshEditorOne::renderTextureLikeVertexMode() {
    const auto keepAlive = weak_from_this().lock();
    if (!texture || !textureFbo) {
        if (!loggedTextureUnavailable) {
            log(formatted("offscreen skipped: texture or FBO is unavailable texture=%s fbo=%s", bool(texture), textureFbo));
            loggedTextureUnavailable = true;
        }
        return;
    }
    const auto textureId = [&] {
        const auto receiver = compat::ngRequireObject(texture);
        return receiver->getTextureId();
    };
    GLint previousDraw = 0, previousRead = 0, previousViewport[4]{};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    const auto bind = NG_DEPTH_NODE_GL("glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC);
    bind(GL_FRAMEBUFFER, textureFbo);
    NG_DEPTH_NODE_GL("glFramebufferTexture2D", PFNGLFRAMEBUFFERTEXTURE2DPROC)(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId(), 0);
    const GLenum attachment = GL_COLOR_ATTACHMENT0;
    NG_DEPTH_NODE_GL("glDrawBuffers", PFNGLDRAWBUFFERSPROC)(1, &attachment);
    const auto status = NG_DEPTH_NODE_GL("glCheckFramebufferStatus", PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        if (!loggedFramebufferIncomplete) {
            const auto id = textureId();
            const auto width = textureWidth;
            const auto height = textureHeight;
            log(formatted("offscreen skipped: framebuffer incomplete status=%s textureId=%s size=%sx%s",
                status, id, width, height));
            loggedFramebufferIncomplete = true;
        }
        bind(GL_DRAW_FRAMEBUFFER, GLuint(previousDraw)); bind(GL_READ_FRAMEBUFFER, GLuint(previousRead));
        glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]); return;
    }
    glViewport(0, 0, textureWidth, textureHeight);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND);
    NG_DEPTH_NODE_GL("glBlendEquation", PFNGLBLENDEQUATIONPROC)(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    nicxlive::core::inPushViewport(textureWidth, textureHeight);
    const auto camera = nicxlive::core::inGetCameraHandle();
    if (!camera) compat::ngFailNullAccess(true);
    camera->scale = {1, 1};
    camera->position = {-minPoint.x - float(textureWidth) * .5f, -minPoint.y - float(textureHeight) * .5f};
    camera->rotation = 0;
    const auto transform = core::math::source_matrix::inverse(compat::ngRequireObject(target)->transform().matrix());
    const auto transformOwner = allocateDepthNodeObject<nicxlive::core::math::Mat4>(transform);
    compat::ngRequireObject(target)->setOneTimeTransform(transformOwner);
    compat::ngRunWithScopeExit([&] {
        const auto parts = vertexModeDrawableChildren();
        if (!loggedOffscreenDraw) {
            const auto count = parts.size();
            const auto id = textureId();
            const auto width = textureWidth;
            const auto height = textureHeight;
            log(formatted("offscreen draw: children=%s textureId=%s viewport=%sx%s",
                count, id, width, height));
            loggedOffscreenDraw = true;
        }
        for (auto part : parts) compat::ngRequireObject(part)->drawOne();
    }, [&] {
        compat::ngRequireObject(target)->setOneTimeTransform({}); nicxlive::core::inPopViewport();
        bind(GL_DRAW_FRAMEBUFFER, GLuint(previousDraw)); bind(GL_READ_FRAMEBUFFER, GLuint(previousRead));
        glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
        if (previousDraw != 0) {
            const GLenum attachments[]{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
            NG_DEPTH_NODE_GL("glDrawBuffers", PFNGLDRAWBUFFERSPROC)(3, attachments);
        }
    });
}
void DepthMeshEditorOne::applyToTarget() {
    const auto keepAlive = weak_from_this().lock();
    if (!depthMapped) { log("apply skipped: target is not DepthMappedNode"); return; }
    if (targetView) syncFromTargetView();
    std::size_t nonZero = 0;
    float minimum = depths.empty() ? 0 : depths[0];
    float maximum = depths.empty() ? 0 : depths[0];
    std::ptrdiff_t firstNonZero = -1;
    const auto currentDepths = depths;
    for (std::size_t i = 0; i < currentDepths.size(); ++i) {
        const auto value = currentDepths[i];
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
        if (std::abs(value) > .000001f) {
            ++nonZero;
            if (firstNonZero < 0) firstNonZero = std::ptrdiff_t(i);
        }
    }
    const auto depthCount = depths.size();
    const auto vertexCount = compat::ngRequireObject(target)->getVertices().size();
    log(formatted("apply depths: length=%s vertices=%s nonZero=%s min=%s max=%s firstNonZero=%s",
        depthCount, vertexCount, nonZero, minimum, maximum, firstNonZero));
    auto action = actions::depth::DepthMappedChangeAction::createSource([&] {
        return std::tuple{target, compat::RetainedString::fromStaticLiteral("Edit Depth Map")};
    });
    compat::ngRequireObject(depthMapped)->replaceDepths(depths);
    const auto saved = compat::ngRequireObject(depthMapped)->copyDepths();
    log(formatted("apply replaceDepths done: savedLength=%s", saved.isNull() ? std::int64_t(-1)
        : std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(saved.size()))));
    action->updateNewState();
    commands::depth::bone::ngMarkDepthBoneDirtyForTarget(target,
        compat::RetainedString::fromStaticLiteral("Edit Depth Map"));
    core::actionstack::incActionPush(action);
    const auto notifying = compat::ngRequireObject(target);
    notifying->notifyChange(target, nicxlive::core::nodes::NotifyReason::AttributeChanged);
}
void DepthMeshEditorOne::draw(std::shared_ptr<nicxlive::core::math::Camera> viewportCamera,
    DepthCamera3D& depthCamera, std::shared_ptr<DepthTextureMeshRenderer> renderer) {
    const auto keepAlive = weak_from_this().lock();
    const auto vertices = common::targetview::detail::copyVertices(compat::ngRequireObject(target)->getVertices());
    if (!loggedFirstDraw) {
        const auto vertexCount = vertices.size();
        const auto depthCount = depths.size();
        const auto indexCount = indices.size();
        const bool hasTextureRenderer = offscreenTextureRenderer != nullptr;
        const auto framebuffer = textureFbo;
        const auto cameraScale = compat::ngRequireObject(viewportCamera)->scale;
        const auto cameraPosition = compat::ngRequireObject(viewportCamera)->position;
        log(formatted("draw enter: vertices=%s depths=%s indices=%s texture=%s fbo=%s cameraScale=%s cameraPos=%s",
            vertexCount, depthCount, indexCount, hasTextureRenderer, framebuffer, cameraScale, cameraPosition));
        loggedFirstDraw = true;
    }
    if (vertices.empty()) {
        if (!loggedEmptyVertices) { log("draw skipped: GridDeformer has no vertices"); loggedEmptyVertices = true; }
        return;
    }
    if (vertices.size() != depths.size()) {
        if (!loggedDepthLengthMismatch) {
            log(formatted("draw skipped: depths length mismatch vertices=%s depths=%s", vertices.size(), depths.size()));
            loggedDepthLengthMismatch = true;
        }
        return;
    }
    if (indices.empty() && !loggedInvalidTopology) { log("draw skipped texture mesh: no indices"); loggedInvalidTopology = true; }
    if (!offscreenTextureRenderer) offscreenTextureRenderer = allocateDepthNodeObject<DepthTargetOffscreenTextureRenderer>();
    const auto targetTexture = compat::ngRequireObject(offscreenTextureRenderer)->render(target);
    const auto mesh = [&] {
        if (targetView) {
            const auto builder = compat::ngRequireObject(targetRenderer);
            const auto view = targetView;
            return builder->buildMesh(view, depthCamera);
        }
        const auto builder = compat::ngRequireObject(targetRenderer);
        const auto meshVertices = vertices;
        const auto meshDepths = depths;
        const auto meshIndices = indices;
        const auto meshMinimum = minPoint;
        const auto meshMaximum = maxPoint;
        const auto displayScale = depthDisplayScale();
        return builder->buildMesh(meshVertices, meshDepths, meshIndices,
            meshMinimum, meshMaximum, displayScale, depthCamera);
    }();
    projectedPoints = mesh.positions.duplicate();
    if (targetView) {
        const auto currentView = compat::ngRequireObject(targetView);
        auto& destination = currentView->projectedPoints;
        const auto source = projectedPoints;
        destination = source.duplicate();
    }
    compat::ngRequireObject(renderer)->draw(targetTexture, mesh.positions, mesh.uvs, mesh.indices, viewportCamera);
    nicxlive::core::math::Vec3Array lines;
    for (const auto line : compat::ngRequireObject(targetRenderer)->buildGridLines(vertices, mesh.positions)) {
        lines.append({line.p0.x, line.p0.y, 0}); lines.append({line.p1.x, line.p1.y, 0});
    }
    if (lines.size()) {
        core::dbg::inDbgSetBuffer(lines); core::dbg::inDbgDrawLines({.55f, .55f, .55f, .7f});
    }
}
}
#undef NG_DEPTH_NODE_GL
