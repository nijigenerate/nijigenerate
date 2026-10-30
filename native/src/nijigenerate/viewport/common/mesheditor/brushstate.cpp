#include "brushstate.hpp"
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::viewport::common::mesheditor::brushstate {
namespace {
using nicxlive::core::nodes::BlendMode;
// Authored __gshared state remains process-wide without an additional lock policy.
std::shared_ptr<TeacherPart> brushTeacherPart;
bool brushTeacherPartVisibilityOverridden = false;
bool brushTeacherPartPreviousEnabled = false;
BlendMode brushTeacherPartPreviousBlendMode = BlendMode::Normal;
bool brushTeacherPartBlendModeOverridden = false;

void enforceTeacherPartBlendMode(std::shared_ptr<TeacherPart> part) {
    if (!part) return;
    if (part->blendMode != BlendMode::Difference) {
        brushTeacherPartPreviousBlendMode = part->blendMode;
        brushTeacherPartBlendModeOverridden = true;
        part->blendMode = BlendMode::Difference;
    }
}
}
std::shared_ptr<TeacherPart> ngBrushGetTeacherPart() {
    enforceTeacherPartBlendMode(brushTeacherPart);
    return brushTeacherPart;
}
void ngBrushSetTeacherPart(std::shared_ptr<TeacherPart> part) {
    if (part == brushTeacherPart) {
        if (part) {
            const bool currentlyEnabled = part->getEnabled();
            if (!currentlyEnabled) {
                brushTeacherPartPreviousEnabled = false;
                part->setEnabled(true);
                brushTeacherPartVisibilityOverridden = true;
            }
            enforceTeacherPartBlendMode(part);
            core::ngViewportSetTemporaryBackgroundColor(0, 0, 0, 1);
        } else {
            core::ngViewportClearTemporaryBackgroundColor();
        }
        return;
    }
    if (brushTeacherPart) {
        if (brushTeacherPartVisibilityOverridden) {
            const auto receiver = brushTeacherPart;
            receiver->setEnabled(brushTeacherPartPreviousEnabled);
        }
        if (brushTeacherPartBlendModeOverridden)
            compat::ngRequireObject(brushTeacherPart)->blendMode = brushTeacherPartPreviousBlendMode;
    }
    brushTeacherPartVisibilityOverridden = false;
    brushTeacherPartBlendModeOverridden = false;
    brushTeacherPart = part;
    if (part) {
        brushTeacherPartPreviousEnabled = part->getEnabled();
        if (!brushTeacherPartPreviousEnabled) {
            part->setEnabled(true);
            brushTeacherPartVisibilityOverridden = true;
        }
        enforceTeacherPartBlendMode(part);
        core::ngViewportSetTemporaryBackgroundColor(0, 0, 0, 1);
    } else {
        core::ngViewportClearTemporaryBackgroundColor();
    }
}
void ngBrushClearTeacherPart() { ngBrushSetTeacherPart({}); }
bool ngBrushHasTeacherPart() {
    return static_cast<bool>(brushTeacherPart);
}
}
