#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <math/types.hpp>
#include <math/mat4.hpp>
#include <math/veca.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::viewport::common::mesheditor::brushes::base {

class Brush {
public:
    // Native polymorphic ownership support; no authored D finalizer is added.
    virtual ~Brush() = default;
    virtual ::nijigenerate::compat::RetainedString name() = 0;
    virtual bool isInside(::nicxlive::core::math::Vec2 center, ::nicxlive::core::math::Vec2 pos) = 0;
    virtual float weightAt(::nicxlive::core::math::Vec2 center, ::nicxlive::core::math::Vec2 pos) = 0;
    virtual ::nijigenerate::compat::Slice<float> weightsAt(
        ::nicxlive::core::math::Vec2 center, ::nicxlive::core::math::Vec2Array positions) = 0;
    virtual void draw(::nicxlive::core::math::Vec2 center, ::nicxlive::core::math::Mat4 transform) = 0;
    virtual bool configure() = 0;
};

} // namespace nijigenerate::viewport::common::mesheditor::brushes::base

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::brushes::base::Brush> {
    // The Source declaration is an interface reference, not a value record.
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesheditor.brushes.base.Brush)";
    inline static constexpr char unqualifiedArgument[] = "(Brush)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesheditor.brushes.base.Brush";
    inline static constexpr char unqualifiedElement[] = "Brush";
};
}
