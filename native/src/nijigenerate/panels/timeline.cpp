#include "timeline.hpp"
#include <nijigenerate/widgets/toolbar.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/signal.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <timing.hpp>
#include <imgui_internal.h>
#include <cmath>
#include <bit>
#include <array>
#include <algorithm>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::panels::timeline {
using namespace project;
using namespace widgets;
using namespace widgets::timeline;
using core::translation::ngTranslateCString;
using core::translation::ngTranslateRetained;
using compat::ngRequireObject;
namespace {
thread_local float timelineWidth=static_cast<float>(defaultHeaderWidth);
thread_local compat::Slice<float> trackHeights;
class AnimationListener : public compat::SignalReceiver,
    public std::enable_shared_from_this<AnimationListener> {
protected:
    virtual void prepareSourceClassInfo();
public:
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AnimationListener> other);
    virtual int opCmp(std::shared_ptr<AnimationListener> other);
    virtual void onAnimationChanged(AnimationPlaybackRef animation) {
        incAnimationTimelineUpdate(*ngRequireObject(ngRequireObject(animation)->animation()));
    }
};
}
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::timeline::AnimationListener> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.timeline.AnimationListener)";
    inline static constexpr char unqualifiedArgument[] = "(AnimationListener)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.timeline.AnimationListener";
    inline static constexpr char unqualifiedElement[] = "AnimationListener";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::timeline::AnimationListener> {
    inline static constexpr char className[] = "nijigenerate.panels.timeline.AnimationListener";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.timeline.AnimationListener)";
};
}

namespace nijigenerate::panels::timeline {
namespace {
template<class Operation> auto allocateBytes(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedStringSlice retainedText(std::string text) {
    return allocateBytes([&] {
        return compat::RetainedStringSlice(compat::RetainedString(std::move(text)));
    });
}
// Inherited Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void registerAnimationListenerType() {
    static const bool registered = allocateBytes([] {
        using namespace nicxlive::core::d_runtime;
        ngRegisterObjectClassName(typeid(AnimationListener),
            RetainedString::fromStaticLiteral("nijigenerate.panels.timeline.AnimationListener"));
        ngRegisterObjectTypeNames<AnimationListener>(
            RetainedString::fromStaticLiteral(SourceObjectTypeNames<AnimationListener>::className),
            RetainedString::fromStaticLiteral(SourceObjectTypeNames<AnimationListener>::templateArgumentName));
        ngPrepareSourceTypeSpellings<AnimationListener>();
        return true;
    });
    (void)registered;
}
void AnimationListener::prepareSourceClassInfo() { registerAnimationListenerType(); }
compat::RetainedString AnimationListener::toString() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t AnimationListener::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool AnimationListener::opEquals(std::shared_ptr<AnimationListener> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int AnimationListener::opCmp(std::shared_ptr<AnimationListener>) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    auto message = allocateBytes([&] {
        auto bytes = std::string("need opCmp for class ");
        bytes += name.view();
        return compat::RetainedString(std::move(bytes));
    });
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
thread_local std::shared_ptr<AnimationListener> listener;
void connectProject(std::shared_ptr<Project> project) {
    ngRequireObject(project)->AnimationChanged.connect(
        Project::AnimationEvent::Slot::bind<&AnimationListener::onAnimationChanged>(*ngRequireObject(listener)));
}
template<class T> T clamp(T value,T low,T high) { return value<low?low:(value>high?high:value); }
std::int32_t integer(float value) {
    if(!std::isfinite(value) || value< -2147483648.0f || value>=2147483648.0f)
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
}
void ngInitializeTimelinePanelModule() {
    thread_local bool initialized=false;
    if(!initialized) {
        registerAnimationListenerType();
        listener=std::allocate_shared<AnimationListener>(
            nicxlive::core::d_runtime::SourceAllocator<AnimationListener>{});
        ngRegisterProjectCallback(&connectProject);
        ngInitializePanelModule<TimelinePanel>();initialized=true;
    }
}
void incAnimationTimelineUpdate(nicxlive::core::Animation& animation) {
    trackHeights.resize(animation.lanes.size(),std::numeric_limits<float>::quiet_NaN());
    const auto lanes=animation.lanes;
    for(std::size_t i=0;i<lanes.size();++i) trackHeights[i]=minTrackHeight;
}
float incAnimationGetTimelineWidth() { return timelineWidth; }
compat::Slice<float> incAnimationGetTrackHeights() { return trackHeights; }
TimelinePanel::TimelinePanel():Panel(compat::RetainedString::fromStaticLiteral("Timeline"),
    ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Timeline")),false) {
    flags|=ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;activeModes=EditMode::AnimEdit;
}
void TimelinePanel::drawHeaders() {
    float width=incAnimationGetTimelineWidth();const auto background=ImGui::GetStyle().Colors[ImGuiCol_ChildBg];
    ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0.25f});
    if(ImGui::BeginChild("HEADERS_ROOT",{width,widgetHeight},ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
        auto* window=ImGui::GetCurrentWindow();scroll=clamp(scroll,0.0f,ImGui::GetScrollMaxY());ImGui::SetScrollY(scroll);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,background);
        if(incAnimationGet()) {
            const auto count=ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size();
            for(std::size_t i=0;i<count;++i) {
                auto playback=incAnimationGet();auto lanes=ngRequireObject(ngRequireObject(playback)->animation())->lanes;
                auto& lane=lanes[i];ImGui::PushID(static_cast<int>(i));
                if(ImGui::BeginPopup("###OPTIONS")) {
                    if(ImGui::BeginMenu(ngTranslateCString("Interpolation").data())) {
                        using Mode=nicxlive::core::common::InterpolateMode;
                        if(ImGui::MenuItem(ngTranslateCString("Nearest").data(),nullptr,lane.interpolation==Mode::Nearest)) lane.interpolation=Mode::Nearest;
                        if(ImGui::MenuItem(ngTranslateCString("Stepped").data(),nullptr,lane.interpolation==Mode::Stepped)) lane.interpolation=Mode::Stepped;
                        if(ImGui::MenuItem(ngTranslateCString("Linear").data(),nullptr,lane.interpolation==Mode::Linear)) lane.interpolation=Mode::Linear;
                        if(ImGui::MenuItem(ngTranslateCString("Cubic").data(),nullptr,lane.interpolation==Mode::Cubic)) lane.interpolation=Mode::Cubic;
                        ImGui::EndMenu();
                    }
                    if(ImGui::BeginMenu(ngTranslateCString("Merge Mode").data())) {
                        using Mode=nicxlive::core::param::ParamMergeMode;
                        ImGui::BeginDisabled(true);
                        if(ImGui::MenuItem(ngTranslateCString("Additive").data(),nullptr,lane.mergeMode==Mode::Additive)) lane.mergeMode=Mode::Additive;
                        if(ImGui::MenuItem(ngTranslateCString("Multiplicative").data(),nullptr,lane.mergeMode==Mode::Multiplicative)) lane.mergeMode=Mode::Multiplicative;
                        ImGui::EndDisabled();
                        if(ImGui::MenuItem(ngTranslateCString("Forced").data(),nullptr,lane.mergeMode==Mode::Forced)) lane.mergeMode=Mode::Forced;
                        ImGui::EndMenu();
                    }
                    if(ImGui::MenuItem(ngTranslateCString("Delete").data())) {
                        auto destinationOwner=ngRequireObject(incAnimationGet());
                        auto* destination=ngRequireObject(destinationOwner->animation());
                        auto source=ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes;
                        destination->lanes=source.removed(i);
                        ImGui::EndPopup();ImGui::PopID();ImGui::PopStyleColor();ImGui::EndChild();ImGui::PopStyleColor();return;
                    }
                    ImGui::EndPopup();
                }
                auto heights=incAnimationGetTrackHeights();ngAnimationLaneHeader(lane,width,heights[i]);
                ImGui::OpenPopupOnItemClick("###OPTIONS",ImGuiPopupFlags_MouseButtonRight);ImGui::PopID();
            }
        }
        ImGui::PopStyleColor();[[maybe_unused]] const float height=ngRequireObject(window)->ClipRect.Min.y-ngRequireObject(window)->ClipRect.Max.y;
        ngHeaderResizer(width,false);
    }
    ImGui::EndChild();ImGui::PopStyleColor();
}
void TimelinePanel::drawTracks() {
    ImGui::SameLine(0,0);ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0.033f});
    if(ImGui::BeginChild("LANES_ROOT",{0,widgetHeight},ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
        [[maybe_unused]] auto* window=ImGui::GetCurrentWindow();ImGui::SetScrollY(scroll);
        hscroll=clamp(hscroll,0.0f,ImGui::GetScrollMaxX());ImGui::SetScrollX(hscroll);
        if(incAnimationGet()) {
            ngBeginTimelinePlayhead(*ngRequireObject(ngRequireObject(incAnimationGet())->animation()),zoom);
            auto playback=incAnimationGet();auto lanes=ngRequireObject(ngRequireObject(playback)->animation())->lanes;
            for(std::size_t i=0;i<lanes.size();++i) {
                auto& lane=lanes[i];float frame=NAN,offset=NAN;
                ngTimelineLane(lane,*ngRequireObject(ngRequireObject(incAnimationGet())->animation()),zoom,static_cast<int>(i),&frame,&offset);
                if(frame> -1) {
                    const int target=integer(std::round(frame));
                    if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) { ngRequireObject(incAnimationGet())->seek(target);incAnimationRender(); }
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        auto parameter=ngRequireObject(lane.paramRef)->targetParam;const auto axis=ngRequireObject(lane.paramRef)->targetAxis;
                        const auto value=ngRequireObject(parameter)->unmapAxis(axis,offset);
                        if(!incAnimationKeyframeRemove(parameter,axis)) incAnimationKeyframeAdd(parameter,axis,value);
                    }
                }
            }
            auto animationOwner=ngRequireObject(incAnimationGet());
            auto* endAnimation=ngRequireObject(animationOwner->animation());
            const auto endZoom=zoom;
            const auto endFrame=ngRequireObject(incAnimationGet())->hframe();
            ngEndTimelinePlayhead(*endAnimation,endZoom,endFrame);
        }
    }
    ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopStyleVar();
}
void TimelinePanel::onBeginUpdate() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,0);
    Panel::onBeginUpdate();
}
void TimelinePanel::onEndUpdate() { Panel::onEndUpdate();ImGui::PopStyleVar(2); }
void TimelinePanel::onUpdate() {
    const bool editing=incEditMode()==EditMode::AnimEdit;auto animation=incAnimationGet();
    ImGui::PushID("TopBar");ImGui::BeginDisabled(!editing);
    if(ngBeginInnerToolbar(24)) {
        ImGui::BeginDisabled(!animation);auto player=incAnimationPlayerGet();
        if(ngToolbarButton(ngRequireObject(player)->snapToFramerate?"\356\242\231":"\356\242\230",32)) ngRequireObject(player)->snapToFramerate=!ngRequireObject(player)->snapToFramerate;
        ngTooltip(compat::RetainedStringSlice(ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Lock playback to animation framerate"))));
        if(ngToolbarButton("\356\201\207",32)) ngRequireObject(animation)->stop(ImGui::IsKeyDown(ImGuiKey_LeftShift)||ImGui::IsKeyDown(ImGuiKey_RightShift));
        if(ngToolbarButton(animation && !(!ngRequireObject(animation)->playing()||ngRequireObject(animation)->paused())?"\356\200\264":"\356\200\267",32)) {
            if(!ngRequireObject(animation)->playing()||ngRequireObject(animation)->paused()) ngRequireObject(animation)->play(true);else ngRequireObject(animation)->pause();
        }
        ImGui::EndDisabled();
    }
    ngEndInnerToolbar();ImGui::EndDisabled();ImGui::PopID();widgetHeight=ngAvailableSpace().y-24;
    if(editing) {
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            const auto& io=ImGui::GetIO();
            if((io.KeyMods&ImGuiMod_Shift)==ImGuiMod_Shift) {
                const float delta=static_cast<float>((io.MouseWheel*1024*zoom)*nicxlive::core::deltaTime());
                // Source version(osx) is case-sensitive and is not declared by
                // any supplied DUB configuration; the builtin is OSX.
                hscroll-=delta;
            } else if((io.KeyMods&ImGuiMod_Ctrl)==ImGuiMod_Ctrl) {
                const float delta=static_cast<float>((io.MouseWheel*2*zoom)*nicxlive::core::deltaTime());
                zoom=static_cast<float>(clamp(static_cast<double>(zoom+delta),timelineMinZoom,static_cast<double>(timelineMaxZoom)));
            } else {
                const float delta=static_cast<float>((io.MouseWheel*1024)*nicxlive::core::deltaTime());
                scroll-=delta;
            }
        }
        drawHeaders();drawTracks();
    } else ngDummy({0,widgetHeight-3});
    ImGui::PushID("BottomBar");
    if(ngBeginInnerToolbar(24,false,false)) {
        int seconds=0,milliseconds=0,frame=1,frames=1,width=0;
        if(animation) {
            seconds=ngRequireObject(animation)->seconds();milliseconds=ngRequireObject(animation)->miliseconds();frame=ngRequireObject(animation)->frame();frames=ngRequireObject(animation)->frames();
            width=allocateBytes([&] { return std::max(1,static_cast<int>(std::to_string(frames).size())); });
        }
        const auto pattern=allocateBytes([&] {
            return "%0"+std::to_string(width)+"d/%0"+std::to_string(width)+"d";
        });
        const std::array frameArguments{core::translation::ngFormatArgument(frame),
            core::translation::ngFormatArgument(frames)};
        ngToolbarText(retainedText(core::translation::ngFormatTyped(pattern,frameArguments)));
        ngToolbarSeparator();
        const std::array timeArguments{core::translation::ngFormatArgument(seconds),
            core::translation::ngFormatArgument(milliseconds)};
        ngToolbarText(retainedText(core::translation::ngFormatTyped("%ss %sms",timeArguments)));
    }
    ngEndInnerToolbar();ImGui::PopID();
    if(!editing) {
        ngLabelOver(compat::RetainedStringSlice(ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Not in Animation Edit mode..."))),{0,0},true);
        return;
    }
}
}
