#pragma once

#include "nijigenerate/viewport/base.hpp"
#include <compat/object_metadata.hpp>
#include <cstdint>
#include <memory>

namespace nijigenerate::viewport::anim {

void ngViewportAnimOverlay();

class AnimationViewport : public base::Viewport, public std::enable_shared_from_this<AnimationViewport> {
protected:
    // This is distinct from the inherited Viewport flag, as in the source.
    bool alwaysUpdateMode{false};

public:
    std::uint32_t asyncDerivedUpdateViewportChannel() override;
    void draw(std::shared_ptr<nicxlive::core::math::Camera> camera) override;
private:
    void prepareSourceClassInfo() override {
        base::Viewport::prepareSourceClassInfo();
        base::Viewport::registerSourceClassInfo<AnimationViewport>("nijigenerate.viewport.anim.AnimationViewport");
    }
};

} // namespace nijigenerate::viewport::anim

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::anim::AnimationViewport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.anim.AnimationViewport)";
    inline static constexpr char unqualifiedArgument[] = "(AnimationViewport)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.anim.AnimationViewport";
    inline static constexpr char unqualifiedElement[] = "AnimationViewport";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::anim::AnimationViewport> {
    inline static constexpr char className[] = "nijigenerate.viewport.anim.AnimationViewport";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.anim.AnimationViewport)";
};
}
