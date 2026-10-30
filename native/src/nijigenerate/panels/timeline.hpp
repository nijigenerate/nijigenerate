#pragma once
#include "package.hpp"
#include <animation/animation.hpp>
#include <nijigenerate/widgets/timeline.hpp>

namespace nijigenerate::panels::timeline {
void incAnimationTimelineUpdate(nicxlive::core::Animation& animation);
float incAnimationGetTimelineWidth();
compat::Slice<float> incAnimationGetTrackHeights();
void ngInitializeTimelinePanelModule();
class TimelinePanel : public Panel {
    float scroll=0,hscroll=0,zoom=1;
    float widgetHeight=std::numeric_limits<float>::quiet_NaN();
    void drawHeaders();
    void drawTracks();
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(TimelinePanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.timeline.TimelinePanel"));
            return true;
        }();
        (void)registered;
    }
    void onBeginUpdate() override;
    void onEndUpdate() override;
    void onUpdate() override;
public:
    TimelinePanel();
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::timeline::TimelinePanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.timeline.TimelinePanel)";
    inline static constexpr char unqualifiedArgument[] = "(TimelinePanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.timeline.TimelinePanel";
    inline static constexpr char unqualifiedElement[] = "TimelinePanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::timeline::TimelinePanel> {
    inline static constexpr char className[] = "nijigenerate.panels.timeline.TimelinePanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.timeline.TimelinePanel)";
};
}
