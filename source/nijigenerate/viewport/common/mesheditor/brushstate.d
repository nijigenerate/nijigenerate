module nijigenerate.viewport.common.mesheditor.brushstate;

import nijilive.core.nodes.part : Part;
import nijigenerate.core.window : incViewportSetTemporaryBackgroundColor, incViewportClearTemporaryBackgroundColor;

alias TeacherPart = Part;

private __gshared TeacherPart brushTeacherPart;
private __gshared bool brushTeacherPartVisibilityOverridden;
private __gshared bool brushTeacherPartPreviousEnabled;

TeacherPart incBrushGetTeacherPart() {
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
            incViewportSetTemporaryBackgroundColor(0.0f, 0.0f, 0.0f, 1.0f);
        } else {
            incViewportClearTemporaryBackgroundColor();
        }
        return;
    }

    if (brushTeacherPart !is null && brushTeacherPartVisibilityOverridden) {
        brushTeacherPart.setEnabled(brushTeacherPartPreviousEnabled);
    }
    brushTeacherPartVisibilityOverridden = false;

    brushTeacherPart = part;

    if (part !is null) {
        brushTeacherPartPreviousEnabled = part.getEnabled();
        if (!brushTeacherPartPreviousEnabled) {
            part.setEnabled(true);
            brushTeacherPartVisibilityOverridden = true;
        }
        incViewportSetTemporaryBackgroundColor(0.0f, 0.0f, 0.0f, 1.0f);
    } else {
        incViewportClearTemporaryBackgroundColor();
    }
}

void incBrushClearTeacherPart() {
    incBrushSetTeacherPart(null);
}

bool incBrushHasTeacherPart() {
    return brushTeacherPart !is null;
}
