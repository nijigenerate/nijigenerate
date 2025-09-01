module nijigenerate.core.input;
import nijigenerate.core.dpi;
import nijigenerate.core.settings; // settings for macOS Ctrl/Super swap
import nijilive.core;
import nijilive.math;
import bindbc.imgui;
import bindbc.sdl;
import std.algorithm;
import std.string : startsWith, split;

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
        default: return ImGuiKey.None;
    }
}

// ===== Compile-time helper to format shortcuts per OS =====
private string _kImpl(string spec)()
{
    // spec uses '-' as separator, e.g., "Super-X", "Ctrl-Shift-Z"
    string res;
    bool first = true;
    size_t i = 0, start = 0;
    while (i <= spec.length) {
        if (i == spec.length || spec[i] == '-') {
            auto tok = spec[start .. i];
            string mapped;
            // Map canonical token to OS-specific label
            if (tok == "Super") {
                version (OSX) mapped = "\ueae7"; // ⌘
                else mapped = "Super";
            } else if (tok == "Alt") {
                version (OSX) mapped = "\ueae8"; // ⌥
                else mapped = "Alt";
            } else if (tok == "Ctrl") {
                version (OSX) mapped = "\ueae6"; // ^
                else mapped = "Ctrl";
            } else if (tok == "Shift") {
                // Keep text label for now (could map to ⇧ on macOS later)
                mapped = "Shift";
            } else {
                mapped = tok; // key token like "N", "F5", etc.
            }
            if (!first) res ~= "+";
            res ~= mapped;
            first = false;
            start = i + 1;
        }
        ++i;
    }
    return res;
}

template _K(string spec)
{
    enum _K = _kImpl!spec();
}

// OS-specific modifier tokens used for parsing
private enum string ngTokSuper = _kImpl!"Super"();
private enum string ngTokAlt   = _kImpl!"Alt"();
private enum string ngTokCtrl  = _kImpl!"Ctrl"();
private enum string ngTokShift = _kImpl!"Shift"();

// Public helper to format a shortcut string for the current OS labels
// Example: ngFormatShortcut(true,false,true,false,"K") => "Ctrl+Shift+K" (Windows/Linux) or "^+⇧+K" (macOS mapping)
string ngFormatShortcut(bool ctrl, bool alt, bool shift, bool superKey, string key)
{
    // On macOS, optionally swap displayed labels for Ctrl/Super
    version (OSX)
    {
        if (incSettingsGet!bool("MacSwapCmdCtrl", false))
        {
            auto tmp = ctrl; ctrl = superKey; superKey = tmp;
        }
    }

    string res;
    if (ctrl)  res ~= (res.length ? "+" : "") ~ ngTokCtrl;
    if (alt)   res ~= (res.length ? "+" : "") ~ ngTokAlt;
    if (shift) res ~= (res.length ? "+" : "") ~ ngTokShift;
    if (superKey) res ~= (res.length ? "+" : "") ~ ngTokSuper;
    if (key.length)
        res ~= (res.length ? "+" : "") ~ key;
    return res;
}

// Parse a shortcut string produced by _K and test against current IO state
bool incShortcut(string s, bool repeat=false) {
    auto io = igGetIO();

    // Split into tokens on '+'; last token is the key, others are modifiers
    string[] parts;
    size_t start = 0;
    for (size_t i = 0; i <= s.length; ++i) {
        if (i == s.length || s[i] == '+') {
            parts ~= s[start .. i];
            start = i + 1;
        }
    }
    if (parts.length == 0) return false;

    bool needCtrl = false, needAlt = false, needShift = false, needSuper = false;
    foreach (i, tok; parts[0 .. $-1]) {
        if (tok == ngTokCtrl) needCtrl = true;
        else if (tok == ngTokAlt) needAlt = true;
        else if (tok == ngTokShift) needShift = true;
        else if (tok == ngTokSuper) needSuper = true;
        else {
            // Unknown token in modifiers; reject
            return false;
        }
    }

    // On macOS, optionally swap Ctrl and Super matching
    version (OSX)
    {
        if (incSettingsGet!bool("MacSwapCmdCtrl", false))
        {
            auto tmp = needCtrl; needCtrl = needSuper; needSuper = tmp;
        }
    }

    // Enforce exact modifier match
    if (!(io.KeyCtrl == needCtrl && io.KeyAlt == needAlt && io.KeyShift == needShift && io.KeySuper == needSuper))
        return false;

    auto keyTok = parts[$-1];
    ImGuiKey scancode = incKeyScancode(keyTok);
    if (scancode == ImGuiKey.None) return false;
    return igIsKeyPressed(scancode, repeat);
}

// Public helpers to expose current OS-specific modifier labels for UI
string ngModifierLabelCtrl() { return ngTokCtrl; }
string ngModifierLabelSuper() { return ngTokSuper; }
