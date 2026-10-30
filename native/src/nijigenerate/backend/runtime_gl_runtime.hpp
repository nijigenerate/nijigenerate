#pragma once
// Independent nijilive OpenGL runtime.d, BSD-2-Clause.
// Full notice: native/port/licenses/nijilive-difference-aggregation.txt.
#include <shader.hpp>
#include "runtime_gl_shader.hpp"
#include <common/resource_key_map.hpp>
#include <common/shared_array.hpp>
#include <compat/phobos_string_hash.hpp>
#include <runtime_state.hpp>
#include <array>
#include <span>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
namespace nijigenerate::backend::runtime_gl {
struct PostProcessingShader {
private:
    using String = nicxlive::core::d_runtime::RetainedString;
    struct KeyHash {
        std::size_t operator()(const String& value) const noexcept {
            return nicxlive::core::d_runtime::ngPhobosStringHash(value.view());
        }
    };
    struct KeyEqual {
        bool operator()(const String& left, const String& right) const noexcept {
            return left.view() == right.view();
        }
    };
    nicxlive::core::common::ResourceKeyMap<String, int, KeyHash, KeyEqual> uniformCache;
public:
    std::shared_ptr<nicxlive::core::Shader> shader;
    PostProcessingShader() = default;
    explicit PostProcessingShader(std::shared_ptr<nicxlive::core::Shader> shader);
    unsigned getUniform(String name);
    bool hasUniform(String name) const;
};
// The source exposes this TLS vector's actual three-element array to widgets.
using nicxlive::core::inSceneAmbientLight;
void oglInitRenderer();
void oglReleaseRenderer();
void oglBeginScene();
void oglEndScene();
void oglPostProcessScene();
void oglBeginComposite();
void oglEndComposite();
void oglAddBasicLightingPostProcess();
nicxlive::core::common::SharedArray<PostProcessingShader>& oglGetPostProcessingStack();
void oglDrawScene(nicxlive::core::math::Vec4 area);
void oglPrepareCompositeScene();
unsigned oglGetFramebuffer();
unsigned oglGetRenderImage();
unsigned oglGetCompositeImage();
unsigned oglGetCompositeFramebuffer();
unsigned oglGetBlendFramebuffer();
unsigned oglGetMainAlbedo();
unsigned oglGetMainEmissive();
unsigned oglGetMainBump();
unsigned oglGetCompositeEmissive();
unsigned oglGetCompositeBump();
unsigned oglGetBlendAlbedo();
unsigned oglGetBlendEmissive();
unsigned oglGetBlendBump();
void oglRebindActiveTargets();
void oglSwapMainCompositeBuffers();
void oglResizeViewport(int width, int height);
void oglDumpViewport(std::vector<std::uint8_t>& destination, int width, int height);
void oglDumpViewport(std::span<std::uint8_t> destination, int width, int height);
void oglDumpViewport(nicxlive::core::common::SharedArray<std::uint8_t>& destination, int width, int height);
}
