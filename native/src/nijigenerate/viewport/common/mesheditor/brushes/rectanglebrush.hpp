#pragma once
#include "base.hpp"
#include <limits>
#include <memory>
#include <cstddef>
#include <compat/object_metadata.hpp>

namespace nijigenerate::viewport::common::mesheditor::brushes::rectanglebrush {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec2Array;
using nicxlive::core::math::Mat4;
class RectangleBrush : public base::Brush, public std::enable_shared_from_this<RectangleBrush> {
public:
    float width = std::numeric_limits<float>::quiet_NaN();
    float height = std::numeric_limits<float>::quiet_NaN();
    float innerRatio = std::numeric_limits<float>::quiet_NaN();
    compat::RetainedString _name;
    RectangleBrush(compat::RetainedString name, float width, float height, float innerRatio);
    RectangleBrush(const RectangleBrush&) = delete;
    RectangleBrush& operator=(const RectangleBrush&) = delete;
    compat::RetainedString name() override;
    bool isInside(Vec2 center, Vec2 pos) override;
    float weightAt(Vec2 center, Vec2 pos) override;
    compat::Slice<float> weightsAt(Vec2 center, Vec2Array positions) override;
    void draw(Vec2 center, Mat4 transform) override;
    bool configure() override;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<RectangleBrush> other);
    virtual int opCmp(std::shared_ptr<RectangleBrush> other);
protected:
    virtual void prepareSourceClassInfo();
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::common::mesheditor::brushes::rectanglebrush::RectangleBrush> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.brushes.rectanglebrush.RectangleBrush)";
    inline static constexpr char unqualifiedArgument[] = "(RectangleBrush)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.brushes.rectanglebrush.RectangleBrush";
    inline static constexpr char unqualifiedElement[] = "RectangleBrush";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::common::mesheditor::brushes::rectanglebrush::RectangleBrush> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.brushes.rectanglebrush.RectangleBrush";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.brushes.rectanglebrush.RectangleBrush)";
};
}
