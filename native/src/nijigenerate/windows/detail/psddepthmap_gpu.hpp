#pragma once
#include "../psddepthmap_complete_values.hpp"
#include <shader.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
#include <texture.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
namespace nijigenerate::windows::psddepthmap::detail {
class PsdDepth3DAdjustGpuRenderer final : public std::enable_shared_from_this<PsdDepth3DAdjustGpuRenderer> {
    std::shared_ptr<nicxlive::core::Texture> texture;
    unsigned fbo = 0, depthBuffer = 0, vao = 0, vbo = 0, ibo = 0;
    std::shared_ptr<nicxlive::core::Shader> shader;
    int viewportSizeUniform = -1, centerUniform = -1, yawPitchUniform = -1, zoomPanUniform = -1;
    int depthScaleUniform = -1, layerDepthTransformUniform = -1, textureUniform = -1;
    int width = 0, height = 0;
    void ensureShader();
    void ensureBuffers();
    bool ensureTarget(int width, int height);
    std::shared_ptr<nicxlive::core::Texture> renderScoped(int width, int height,
        const void* context, void (*invoke)(const void*));
public:
    PsdDepth3DAdjustGpuRenderer() = default;
    virtual ~PsdDepth3DAdjustGpuRenderer() = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<PsdDepth3DAdjustGpuRenderer> other);
    virtual int opCmp(std::shared_ptr<PsdDepth3DAdjustGpuRenderer> other);
    PsdDepth3DAdjustGpuRenderer(const PsdDepth3DAdjustGpuRenderer&) = delete;
    PsdDepth3DAdjustGpuRenderer& operator=(const PsdDepth3DAdjustGpuRenderer&) = delete;
    PsdDepth3DAdjustGpuRenderer(PsdDepth3DAdjustGpuRenderer&&) = delete;
    PsdDepth3DAdjustGpuRenderer& operator=(PsdDepth3DAdjustGpuRenderer&&) = delete;
    // The real render body invokes this borrowed context synchronously and never stores it.
    template<class DrawLayers>
        requires (std::is_object_v<std::remove_reference_t<DrawLayers>> &&
            !std::is_volatile_v<std::remove_reference_t<DrawLayers>> &&
            std::is_invocable_r_v<void, DrawLayers&>)
    std::shared_ptr<nicxlive::core::Texture> render(int width, int height, DrawLayers&& drawLayers) {
        using Callback = std::remove_reference_t<DrawLayers>;
        return renderScoped(width, height, std::addressof(drawLayers), [](const void* context) {
            auto& callback = *const_cast<Callback*>(static_cast<const Callback*>(context));
            if constexpr (std::is_pointer_v<Callback> ||
                std::is_same_v<std::remove_cv_t<Callback>, std::function<void()>>) {
                if (!callback) compat::ngFailNullAccess(false);
            }
            std::invoke(callback);
        });
    }
    std::shared_ptr<nicxlive::core::Texture> render(int width, int height, std::nullptr_t) {
        return renderScoped(width, height, nullptr, nullptr);
    }
    bool capturePreview(int& width, int& height, compat::Slice<std::uint8_t>& rgba);
    void drawLayer(std::shared_ptr<nicxlive::core::Texture> texture, PsdDepth3DAdjustMesh& mesh,
        float centerX, float centerY, float yaw, float pitch, float zoom, nicxlive::core::math::Vec2 pan,
        float depthDisplayScale, PsdDepthLayerEdit bakedTransform, PsdDepthLayerEdit currentTransform);
    void dispose();
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::detail::PsdDepth3DAdjustGpuRenderer> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.windows.psddepthmap.PsdDepth3DAdjustGpuRenderer)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepth3DAdjustGpuRenderer)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.windows.psddepthmap.PsdDepth3DAdjustGpuRenderer";
    inline static constexpr char unqualifiedElement[] = "PsdDepth3DAdjustGpuRenderer";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::psddepthmap::detail::PsdDepth3DAdjustGpuRenderer> {
    inline static constexpr char className[] = "nijigenerate.windows.psddepthmap.PsdDepth3DAdjustGpuRenderer";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.windows.psddepthmap.PsdDepth3DAdjustGpuRenderer)";
};
}
