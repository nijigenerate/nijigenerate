#pragma once
#include "base.hpp"
#include <nijigenerate/viewport/common/transformhandle.hpp>
#include <math/camera.hpp>
#include <limits>

namespace nijigenerate::viewport::base {
enum class LockedOrientation { None, Horizontal, Vertical };
class DraggingOnHandle : public std::enable_shared_from_this<DraggingOnHandle> {
    static constexpr float initialFloat = std::numeric_limits<float>::quiet_NaN();
public:
    using Vec2 = nicxlive::core::math::Vec2;
    bool dragged = false;
    Vec2 dragOrigin{initialFloat, initialFloat}, origValue{initialFloat, initialFloat};
    Vec2 prevPos{initialFloat, initialFloat}, prevValue{initialFloat, initialFloat};
    common::transformhandle::ActionMap actions;
    LockedOrientation locked = LockedOrientation::None;
    DraggingOnHandle(Vec2 origin = {0, 0}, Vec2 value = {0, 0}) {
        dragged = true;
        dragOrigin = origin;
        prevPos = origin;
        origValue = value;
        prevValue = value;
        locked = LockedOrientation::None;
    }
    virtual ~DraggingOnHandle() = default;
    DraggingOnHandle(const DraggingOnHandle&) = delete;
    DraggingOnHandle& operator=(const DraggingOnHandle&) = delete;
    virtual void lockOrientation(LockedOrientation orientation);
    virtual void commitActions();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DraggingOnHandle> other);
    virtual int opCmp(std::shared_ptr<DraggingOnHandle> other);
};
bool ngDraggingOnHandleObjectEquals(std::shared_ptr<DraggingOnHandle> left,
    std::shared_ptr<DraggingOnHandle> right);
namespace detail {
void viewportMovement(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera);
void viewportZoomToMouse(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera, float uiScale);
void viewportZoomToCenter(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera, float uiScale);
}
}

namespace nicxlive::core::d_runtime {
#define NG_VIEWPORT_CONTROL_ORIGINS(Type, Reference) \
template<> struct SourceTypeSpellings<::nijigenerate::viewport::base::Type> { \
    inline static constexpr bool isNativeObject = Reference; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.base." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.base." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
};
NG_VIEWPORT_CONTROL_ORIGINS(LockedOrientation, false)
NG_VIEWPORT_CONTROL_ORIGINS(DraggingOnHandle, true)
#undef NG_VIEWPORT_CONTROL_ORIGINS
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::base::DraggingOnHandle> {
    inline static constexpr char className[] = "nijigenerate.viewport.base.DraggingOnHandle";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.base.DraggingOnHandle)";
};
}
