module nijigenerate.viewport.common.mesheditor.brushstate;

import nijilive.core.nodes.part : Part, BlendMode;
import nijigenerate.core.window : incViewportSetTemporaryBackgroundColor, incViewportClearTemporaryBackgroundColor;

alias TeacherPart = Part;

private __gshared TeacherPart brushTeacherPart;
private __gshared bool brushTeacherPartVisibilityOverridden;
private __gshared bool brushTeacherPartPreviousEnabled;
private __gshared BlendMode brushTeacherPartPreviousBlendMode;
private __gshared bool brushTeacherPartBlendModeOverridden;

TeacherPart incBrushGetTeacherPart() {
    enforceTeacherPartBlendMode(brushTeacherPart);
    return brushTeacherPart;
}

void incBrushSetTeacherPart(TeacherPart part) {
    if (part is brushTeacherPart) {
        if (part !is null) {
            bool currentlyEnabled = part.getEnabled();
            if (!currentlyEnabled) {
                brushTeacherPartPreviousEnabled = false;
                part.setEnabled(true);
                brushTeacherPartVisibilityOverridden = true;
            }
            enforceTeacherPartBlendMode(part);
            incViewportSetTemporaryBackgroundColor(0.0f, 0.0f, 0.0f, 1.0f);
        } else {
            incViewportClearTemporaryBackgroundColor();
        }
        return;
    }

    if (brushTeacherPart !is null) {
        if (brushTeacherPartVisibilityOverridden) {
            brushTeacherPart.setEnabled(brushTeacherPartPreviousEnabled);
        }
        if (brushTeacherPartBlendModeOverridden) {
            brushTeacherPart.blendingMode = brushTeacherPartPreviousBlendMode;
        }
    }
    brushTeacherPartVisibilityOverridden = false;
    brushTeacherPartBlendModeOverridden = false;

    brushTeacherPart = part;

    if (part !is null) {
        brushTeacherPartPreviousEnabled = part.getEnabled();
        if (!brushTeacherPartPreviousEnabled) {
            part.setEnabled(true);
            brushTeacherPartVisibilityOverridden = true;
        }
        enforceTeacherPartBlendMode(part);
        incViewportSetTemporaryBackgroundColor(0.0f, 0.0f, 0.0f, 1.0f);
    } else {
        incViewportClearTemporaryBackgroundColor();
    }
}

private void enforceTeacherPartBlendMode(TeacherPart part) {
    if (part is null)
        return;

    if (part.blendingMode != BlendMode.Difference) {
        brushTeacherPartPreviousBlendMode = part.blendingMode;
        brushTeacherPartBlendModeOverridden = true;
        part.blendingMode = BlendMode.Difference;
    }
}

void incBrushClearTeacherPart() {
    incBrushSetTeacherPart(null);
}

bool incBrushHasTeacherPart() {
    return brushTeacherPart !is null;
}
