module nijigenerate.core.input;
import nijigenerate.core;
import nijilive.core;
import nijilive.math;
import bindbc.imgui;
import bindbc.sdl;
import std.algorithm;
import std.array;

private {
    vec2 mpos;
    ImGuiIO* io;
}

/**
    Begins a UI input pass
*/
void incInputPoll() {
    io = igGetIO();
}

vec2 WorldToViewport(float x, float y, Camera camera = null) {
    if (camera is null)
        camera = inGetCamera();
    vec2 camPos = camera.position;
    vec2 camScale = camera.scale;
    vec2 camCenter = camera.getCenterOffset();
    float uiScale = incGetUIScale();

    return (
        mat3.scaling(uiScale, uiScale,1).inverse()
        * mat3.scaling(camScale.x, camScale.y, 1) 
        * mat3.translation(camPos.x+camCenter.x, camPos.y+camCenter.y, 0) 
        * vec3(x, y, 1)
    ).xy;
}

/**
    Sets the mouse within the viewport
*/
void incInputSetViewportMouse(float x, float y) {
    vec2 camPos = inGetCamera().position;
    vec2 camScale = inGetCamera().scale;
    vec2 camCenter = inGetCamera().getCenterOffset();
    float uiScale = incGetUIScale();

    mpos = (
        mat3.translation(
            camPos.x+camCenter.x, 
            camPos.y+camCenter.y,
            0
        ) * 
        mat3.scaling(
            camScale.x, 
            camScale.y,
            1
        ).inverse() *
        mat3.scaling(
            uiScale, 
            uiScale, 
            1
        ) *
        vec3(x, y, 1)
    ).xy;
}

/**
    Gets the position of the mouse in the viewport
*/
vec2 incInputGetMousePosition() {
    return mpos;
}

/**
    Gets whether a mouse button is down
*/
bool incInputIsMouseDown(int idx) {
    return io.MouseDown[idx];
}

/**
    Gets whether a mouse button is down
*/
bool incInputIsMouseClicked(ImGuiMouseButton idx) {
    return igIsMouseClicked(idx, false);
}

/**
    Gets whether a mouse button is down
*/
bool incInputIsMouseReleased(ImGuiMouseButton idx) {
    return igIsMouseReleased(idx);
}

/**
    Gets whether a right click popup menu is requested by the user
*/
bool incInputIsPopupRequested() {
    return 
        !incInputIsDragRequested() &&  // User can drag camera, make sure they aren't doing that 
        incInputIsMouseReleased(ImGuiMouseButton.Right); // Check mouse button released
}

/**
    Gets whether the user has requested to drag something
*/
bool incInputIsDragRequested(ImGuiMouseButton btn = ImGuiMouseButton.Right) {
    ImVec2 dragDelta;
    igGetMouseDragDelta(&dragDelta, btn);
    return abs(dragDelta.x) > 0.1f && abs(dragDelta.y) > 0.1f;
}

/**
    Gets whether a key is held down
*/
bool incInputIsKeyDown(ImGuiKey key) {
    return igIsKeyDown(key);
}

/**
    Gets whether a key is held down
*/
bool incInputIsKeyUp(ImGuiKey key) {
    return !incInputIsKeyDown(key);
}

/**
    Gets whether a key is held down
*/
bool incInputIsKeyPressed(ImGuiKey key) {
    return igIsKeyPressed(key);
}

ImGuiKey incKeyScancode(string c) {
    switch (c) {
        case "A": return ImGuiKey.A;
        case "B": return ImGuiKey.B;
        case "C": return ImGuiKey.C;
        case "D": return ImGuiKey.D;
        case "E": return ImGuiKey.E;
        case "F": return ImGuiKey.F;
        case "G": return ImGuiKey.G;
        case "H": return ImGuiKey.H;
        case "I": return ImGuiKey.I;
        case "J": return ImGuiKey.J;
        case "K": return ImGuiKey.K;
        case "L": return ImGuiKey.L;
        case "M": return ImGuiKey.M;
        case "N": return ImGuiKey.N;
        case "O": return ImGuiKey.O;
        case "P": return ImGuiKey.P;
        case "Q": return ImGuiKey.Q;
        case "R": return ImGuiKey.R;
        case "S": return ImGuiKey.S;
        case "T": return ImGuiKey.T;
        case "U": return ImGuiKey.U;
        case "V": return ImGuiKey.V;
        case "W": return ImGuiKey.W;
        case "X": return ImGuiKey.X;
        case "Y": return ImGuiKey.Y;
        case "Z": return ImGuiKey.Z;
        case "0": return ImGuiKey.n0;
        case "1": return ImGuiKey.n1;
        case "2": return ImGuiKey.n2;
        case "3": return ImGuiKey.n3;
        case "4": return ImGuiKey.n4;
        case "5": return ImGuiKey.n5;
        case "6": return ImGuiKey.n6;
        case "7": return ImGuiKey.n7;
        case "8": return ImGuiKey.n8;
        case "9": return ImGuiKey.n9;
        case "F1": return ImGuiKey.F1;
        case "F2": return ImGuiKey.F2;
        case "F3": return ImGuiKey.F3;
        case "F4": return ImGuiKey.F4;
        case "F5": return ImGuiKey.F5;
        case "F6": return ImGuiKey.F6;
        case "F7": return ImGuiKey.F7;
        case "F8": return ImGuiKey.F8;
        case "F9": return ImGuiKey.F9;
        case "F10": return ImGuiKey.F10;
        case "F11": return ImGuiKey.F11;
        case "F12": return ImGuiKey.F12;
        case "Left": return ImGuiKey.LeftArrow;
        case "Right": return ImGuiKey.RightArrow;
        case "Up": return ImGuiKey.UpArrow;
        case "Down": return ImGuiKey.DownArrow;
        case "Esc": return ImGuiKey.Escape;
        case "Space": return ImGuiKey.Space;
        case "Enter": return ImGuiKey.Enter;
        case "Tab": return ImGuiKey.Tab;
        case "Backspace": return ImGuiKey.Backspace;
        case "Delete": return ImGuiKey.Delete;
        case "Insert": return ImGuiKey.Insert;
        case "Ctrl": return ImGuiKey.ModCtrl;
        case "Alt": return ImGuiKey.ModAlt;
        case "Shift": return ImGuiKey.ModShift;
        default: return ImGuiKey.None;
    }
}

/**
    checks if a shortcut is pressed,
    the function can make sure the shortcut excludes the modifier keys
*/
bool incShortcut(string s, bool repeat=false) {
    auto io = igGetIO();

    if(io.KeyCtrl && io.KeyAlt) return false;

    if (startsWith(s, "Ctrl+Shift+")) {
        if (!(io.KeyCtrl && !io.KeyAlt && io.KeyShift)) return false;
        s = s[11..$];
    }
    if (startsWith(s, "Ctrl+")) {
        if (!(io.KeyCtrl && !io.KeyAlt && !io.KeyShift)) return false;
        s = s[5..$];
    }
    if (startsWith(s, "Alt+")) {
        if (!(!io.KeyCtrl && io.KeyAlt && !io.KeyShift)) return false;
        s = s[4..$];
    }
    if (startsWith(s, "Shift+")) {
        if (!(!io.KeyCtrl && !io.KeyAlt && io.KeyShift)) return false;
        s = s[6..$];
    }

    ImGuiKey scancode = incKeyScancode(s);
    if (scancode == ImGuiKey.None) return false;

    return igIsKeyPressed(scancode, repeat);
}

/**
    key pressed may not work if the key is a modifier key
    so we need to check if the key is a modifier key
    this function not check right or left modifier key
    use incIsModifierKeyLR for that
*/
bool incIsModifierKey(ImGuiKey key) {
    switch (key) {
        case ImGuiKey.ModCtrl:
        case ImGuiKey.ModAlt:
        case ImGuiKey.ModShift:
        case ImGuiKey.ModSuper:
            return true;
        default:
            return false;
    }
}

bool incIsModifierKeyLR(ImGuiKey key) {
    switch (key) {
        case ImGuiKey.LeftCtrl:
        case ImGuiKey.RightCtrl:
        case ImGuiKey.LeftAlt:
        case ImGuiKey.RightAlt:
        case ImGuiKey.LeftShift:
        case ImGuiKey.RightShift:
        case ImGuiKey.LeftSuper:
        case ImGuiKey.RightSuper:
            return true;
        default:
            return false;
    }
}

ImGuiKey[] incStringToKeys(string s) {
    string[] keys = s.split("+");
    ImGuiKey[] result;
    foreach (key; keys) {
        ImGuiKey scancode = incKeyScancode(key);
        if (scancode == ImGuiKey.None)
            throw new Exception("Invalid key: " ~ key);
        result ~= scancode;
    }

    return result;
}