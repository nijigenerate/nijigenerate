module nijigenerate.viewport.common.mesheditor.brushstate;

import nijilive.core.nodes.part : Part;

alias TeacherPart = Part;

private __gshared TeacherPart brushTeacherPart;

TeacherPart incBrushGetTeacherPart() {
    return brushTeacherPart;
}

void incBrushSetTeacherPart(TeacherPart part) {
    brushTeacherPart = part;
}

void incBrushClearTeacherPart() {
    brushTeacherPart = null;
}

bool incBrushHasTeacherPart() {
    return brushTeacherPart !is null;
}
