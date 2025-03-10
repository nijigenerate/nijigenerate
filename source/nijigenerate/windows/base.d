module nijigenerate.windows.base;

import nijigenerate.core;
import nijigenerate.windows.welcome;
import bindbc.imgui;
import nijigenerate.widgets;
import std.string;
import std.conv;
import i18n;

version(NoUIScaling) private ImGuiWindowClass* windowClass;

private uint spawnCount = 0;

/**
    A Widget
*/
abstract class Window {
private:
    string name_;
    bool visible = true;
    bool disabled;
    int spawnedId;
    const(char)* imName;

protected:
    bool onlyOne;
    bool drewWindow;
    ImGuiWindowFlags flags;

    abstract void onUpdate();

    void onBeginUpdate() {
        if (imName is null) this.setTitle(name);
        version(NoUIScaling) {
            igSetNextWindowClass(windowClass);
            drewWindow = incBegin(
                imName,
                &visible, 
                incIsWayland() ? flags : flags | ImGuiWindowFlags.NoDecoration
            );
        } else version(UseUIScaling) {
            drewWindow = incBegin(
                imName,
                &visible, 
                flags
            );
        }
    }
    
    void onEndUpdate() {
        incEnd();
    }

    void onClose() { }

    void unclose() {
        this.visible = true;
    }

public:


    /**
        Constructs a frame
    */
    this(string name) {
        this.name_ = name;
        this.restore();
    }

    final void close() {
        this.visible = false;
    }

    final string name() {
        return name_;
    }

    final void setTitle(string title) {
        this.name_ = title;
        imName = "%s###%s".format(name_, spawnedId).toStringz;
    }

    /**
        Draws the frame
    */
    final void update() {
        if (!disabled && !visible) return;

        igPushItemFlag(ImGuiItemFlags.Disabled, disabled);
            this.onBeginUpdate();
                this.onUpdate();
            this.onEndUpdate();
        igPopItemFlag();

        if (disabled && !visible) visible = true;
    }

    ImVec2 getPosition() {
        ImVec2 pos;
        igGetWindowPos(&pos);
        return pos;
    }

    void disable() {
        this.flags = ImGuiWindowFlags.NoDocking | 
            ImGuiWindowFlags.NoCollapse | 
            ImGuiWindowFlags.NoNav | 
            ImGuiWindowFlags.NoMove |
            ImGuiWindowFlags.NoScrollWithMouse |
            ImGuiWindowFlags.NoScrollbar;
        disabled = true;
    }

    void restore() {
        disabled = false;
        this.flags = ImGuiWindowFlags.NoDocking | ImGuiWindowFlags.NoCollapse | ImGuiWindowFlags.NoSavedSettings;

        version(NoUIScaling) {
            windowClass = ImGuiWindowClass_ImGuiWindowClass();
            windowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags.NoDecoration | ImGuiViewportFlags.NoTaskBarIcon;
            windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags.NoAutoMerge;
        }
    }
    
    /**
        Gets whether the window is visible.
    */
    final
    bool isVisible() {
        return visible;
    }
}

private {
    Window[] windowStack;
    Window[] windowList;
}

/**
    Pushes window to stack
*/
void incPushWindow(Window window) {
    window.spawnedId = spawnCount++;
    
    // Only allow one instance of the window
    if (window.onlyOne) {
        foreach(win; windowStack) {
            if (win.name == window.name) return;
        }
    }

    if (windowStack.length > 0) {
        windowStack[$-1].disable();
    }

    windowStack ~= window;
}

/**
    Pushes window to stack
*/
void incPushWindowList(Window window) {
    window.spawnedId = spawnCount++;

    // Only allow one instance of the window
    if (window.onlyOne) {
        foreach(win; windowList) {
            if (win.name == window.name) return;
        }
    }

    windowList ~= window;
}

/**
    Pop window from Window List
*/
void incPopWindowList(Window window) {
    import std.algorithm.searching : countUntil;
    import std.algorithm.mutation : remove;

    ptrdiff_t i = windowList.countUntil(window);
    if (i != -1) {
        window.onClose();
        if (windowList.length == 1) windowList.length = 0;
        else windowList = windowList.remove(i);
    }
}

/**
    Pop window from Window List
*/
void incPopWindowListAll() {
    foreach(window; windowList) {
        window.onClose();
        window.visible = false;
    }
    windowList.length = 0;
}

/**
    Pops a window
*/
void incPopWindow() {
    if (windowStack.length > 0) {
        windowStack[$-1].onClose();
        windowStack.length--;

        // The size of the stack has changed, make sure we
        // have anything to restore
        if (windowStack.length > 0) windowStack[$-1].restore();
    }
}

/**
    Update windows
*/
void incUpdateWindows() {
    int id = 0;
    foreach(window; windowStack) {
        window.update();
        if (!window.visible) incPopWindow();
    }
    
    Window[] closedWindows;
    foreach(window; windowList) {
        window.update();
        if (!window.visible) closedWindows ~= window;
    }

    foreach(window; closedWindows) {
        incPopWindowList(window);
    }
}

/**
    Gets top window
*/
Window incGetTopWindow() {
    return windowStack.length > 0 ? windowStack[$-1] : null;
}

/**
    Pops the welcome window.
*/
void incPopWelcomeWindow() {
    import std.algorithm.mutation : remove;
    if (windowStack) {
        foreach(i; 0..windowStack.length) {
            if (auto ww = cast(WelcomeWindow)windowStack[i]) {
                windowStack = windowStack.remove(i);
                return;
            }
        }
    }
}

/**
    Gets whether the welcome window is on top
*/
bool incWelcomeWindowOnTop() {
    return cast(WelcomeWindow)incGetTopWindow() !is null;
}

/**
    Gets the length of the window stack
*/
size_t incGetWindowStackSize() {
    return windowStack.length;
}

/**
    Gets the length of the window list
*/
size_t incGetWindowListSize() {
    return windowList.length;
}

/**
    Gets the amount of floating windows open
*/
size_t incGetWindowsOpen() {
    return windowStack.length+windowList.length;
}
