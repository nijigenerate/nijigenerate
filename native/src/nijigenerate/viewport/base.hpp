#pragma once

#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/retained_string_slice.hpp"
#include <math/types.hpp>
#include <common/object_key_policy.hpp>
#include <compat/object_metadata.hpp>
#include <typeinfo>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

struct ImGuiIO;
struct ImDrawList;
struct ImVec2;
struct ImRect;
namespace nicxlive::core { struct Animation; }
namespace nicxlive::core::math { class Camera; }
namespace nicxlive::core::nodes { class Node; }
namespace nicxlive::core::param { class Parameter; }

namespace nijigenerate::viewport::base {

class MainViewport;
class DraggingOnHandle;
std::shared_ptr<MainViewport> ngViewport();
void ngPresentTemporaryViewport(std::shared_ptr<class Viewport> next);
void ngSetViewportAlwaysUpdate(bool value);
float ngGetMirrorX(float position);
float ngGetMirrorX2(float position);
void ngMirrorIO(ImGuiIO* result);
bool ngStartedDrag(int button);
void ngBeginDrag(int button);
void ngEndDrag(int button);
bool ngDragStartedInViewport(int button);
bool ngDragStartedOnHandle(int button, std::string_view name);
bool ngDragStartedOnHandle(int button, compat::RetainedStringSlice name);
void ngBeginDragInViewport(int button);
void ngBeginDragOnHandle(int button, std::string_view name, nicxlive::core::math::Vec2 previous = {0, 0});
void ngBeginDragOnHandle(int button, compat::RetainedStringSlice name,
    nicxlive::core::math::Vec2 previous = {0, 0});
bool ngGetDragOriginOnHandle(int button, std::string_view name, nicxlive::core::math::Vec2& position);
bool ngGetDragOriginOnHandle(int button, compat::RetainedStringSlice name, nicxlive::core::math::Vec2& position);
bool ngGetDragOrigValueOnHandle(int button, std::string_view name, nicxlive::core::math::Vec2& value);
bool ngGetDragOrigValueOnHandle(int button, compat::RetainedStringSlice name, nicxlive::core::math::Vec2& value);
bool ngGetDragPrevValueOnHandle(int button, std::string_view name, nicxlive::core::math::Vec2& value);
bool ngGetDragPrevValueOnHandle(int button, compat::RetainedStringSlice name, nicxlive::core::math::Vec2& value);
bool ngGetDragPrevPosOnHandle(int button, std::string_view name, nicxlive::core::math::Vec2& position);
bool ngGetDragPrevPosOnHandle(int button, compat::RetainedStringSlice name, nicxlive::core::math::Vec2& position);
void ngSetDragPrevPosOnHandle(int button, std::string_view name, nicxlive::core::math::Vec2 position);
void ngSetDragPrevPosOnHandle(int button, compat::RetainedStringSlice name, nicxlive::core::math::Vec2 position);
void ngSetDragPrevValueOnHandle(int button, std::string_view name, nicxlive::core::math::Vec2 value);
void ngSetDragPrevValueOnHandle(int button, compat::RetainedStringSlice name, nicxlive::core::math::Vec2 value);
void ngEndDragInViewport(int button);
void ngEndDragOnHandle(int button, std::string_view name);
void ngEndDragOnHandle(int button, compat::RetainedStringSlice name);
std::shared_ptr<DraggingOnHandle> ngGetDragOnHandleStatus(int button, std::string_view name);
std::shared_ptr<DraggingOnHandle> ngGetDragOnHandleStatus(int button, compat::RetainedStringSlice name);
void ngViewportTransformHandle();
void ngViewportReset();
compat::RetainedString ngGetViewportZoomMode();
bool ngSetViewportZoomMode(compat::RetainedString value);
template<std::size_t N> bool ngSetViewportZoomMode(const char (&value)[N]) {
    return ngSetViewportZoomMode(compat::RetainedString::fromStaticLiteral(value));
}
float ngGetViewportZoomSpeed();
bool ngSetViewportZoomSpeed(float value);
inline constexpr double viewportZoomMin = 0.05;
inline constexpr double viewportZoomMax = 12.0;
extern thread_local bool shouldPostProcess;
extern thread_local nicxlive::core::math::Vec2 viewportTargetPosition;
extern thread_local float viewportTargetZoom;
extern thread_local float viewportZoom;
extern thread_local float viewportTransformSnap;
extern thread_local bool shouldMirrorViewport;

class Viewport {
protected:
    bool alwaysUpdateMode{false};

public:
    Viewport() = default;
    virtual ~Viewport() = default;
    Viewport(const Viewport&) = delete;
    Viewport& operator=(const Viewport&) = delete;

    virtual void draw(std::shared_ptr<nicxlive::core::math::Camera> camera);
    virtual void drawTools();
    virtual void drawOptions();
    virtual void drawConfirmBar();
    virtual void drawOverlay(ImDrawList* drawList, ImVec2 viewportOrigin, ImRect viewportRect);
    virtual std::uint32_t asyncDerivedUpdateViewportChannel();
    virtual void update(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera);
    virtual void updatePresentation(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera);
    virtual void withdraw();
    virtual void present();
    virtual void menu();
    virtual void menuOpening();
    virtual void toolSettings();
    virtual bool hasMenu();
    virtual bool alwaysUpdate();
    virtual void alwaysUpdate(bool value);
    virtual void toolbar();
    virtual void selectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> selection);
    virtual void animationChanged(nicxlive::core::Animation animation);
    virtual void armedParameterChanged(std::shared_ptr<nicxlive::core::param::Parameter> parameter);
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Viewport> other);
    virtual int opCmp(std::shared_ptr<Viewport> other);
protected:
    template<class T, std::size_t N> static void registerSourceClassInfo(const char (&name)[N]) {
        static const bool registered = [&name] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T),
                compat::RetainedString::fromStaticLiteral(name));
            return true;
        }();
        (void)registered;
    }
    virtual void prepareSourceClassInfo();
};

class DelegationViewport : public Viewport {
protected:
    std::shared_ptr<Viewport> _subView;

public:
    void draw(std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    void drawTools() override;
    void drawOptions() override;
    void drawConfirmBar() override;
    std::uint32_t asyncDerivedUpdateViewportChannel() override;
    void drawOverlay(ImDrawList* drawList, ImVec2 origin, ImRect rect) override;
    void update(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    void updatePresentation(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    void withdraw() override;
    void present() override;
    void menu() override;
    void menuOpening() override;
    void toolSettings() override;
    void toolbar() override;
    bool hasMenu() override;
    void selectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> nodes) override;
    void animationChanged(nicxlive::core::Animation animation) override;
    void armedParameterChanged(std::shared_ptr<nicxlive::core::param::Parameter> parameter) override;
    virtual std::shared_ptr<Viewport> subView();
    virtual void subView(std::shared_ptr<Viewport> value);
protected:
    void prepareSourceClassInfo() override;
};

bool ngViewportObjectEquals(std::shared_ptr<Viewport> left, std::shared_ptr<Viewport> right);

} // namespace nijigenerate::viewport::base

namespace nicxlive::core::d_runtime {
#define NG_VIEWPORT_BASE_NAMES(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::viewport::base::Type> { \
    inline static constexpr bool isNativeObject = true; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.base." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.base." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
}; \
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::base::Type> { \
    inline static constexpr char className[] = "nijigenerate.viewport.base." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.base." #Type ")"; \
};
NG_VIEWPORT_BASE_NAMES(Viewport)
NG_VIEWPORT_BASE_NAMES(DelegationViewport)
#undef NG_VIEWPORT_BASE_NAMES
}
