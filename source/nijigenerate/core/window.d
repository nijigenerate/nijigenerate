module nijigenerate.core.window;

public import bindbc.sdl;
public import bindbc.opengl;
public import bindbc.imgui;
public import bindbc.imgui.ogl;
import nijigenerate.ver;
import std.string;
import std.exception;
import nijigenerate.core.settings;
import nijigenerate.core.logo;
import nijigenerate.core.path;
import nijigenerate.core.font;
import nijigenerate.core.dpi;
import nijigenerate.core.tasks;
import nijigenerate.widgets.dialog;
import nijigenerate.widgets.modal;
import nijigenerate.widgets.button;
import nijilive;
import nijilive.core.diff_collect : DifferenceEvaluationResult;
import nijigenerate.backend.gl;
import nijigenerate.io.autosave;
import nijigenerate.io.save;
import i18n;
import std.stdio : writefln;

version(OSX) {
    enum const(char)*[] SDL_VERSIONS = ["libSDL2.dylib", "libSDL2-2.0.dylib", "libSDL2-2.0.0.dylib"];
} else version(Windows) {
    enum const(char)*[] SDL_VERSIONS = ["SDL2.dll"];
} else {
    enum const(char)*[] SDL_VERSIONS = [
        "libSDL2-2.0.so.0",
        "libSDL2-2.0.so",
        "libSDL2.so",
        "/usr/local/lib/libSDL2-2.0.so.0",
        "/usr/local/lib/libSDL2-2.0.so",
        "/usr/local/lib/libSDL2.so",
    ];
}

version(linux) {
    import dportals;
}

version(Windows) {
    import core.sys.windows.windows;
    import core.sys.windows.winuser;

    // Windows 8.1+ DPI awareness context enum
    enum DPIAwarenessContext { 
        DPI_AWARENESS_CONTEXT_UNAWARE = 0,
        DPI_AWARENESS_CONTEXT_SYSTEM_AWARE = 1,
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE = 2,
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = 3
    }

    // Windows 8.1+ DPI awareness enum
    enum ProcessDPIAwareness { 
        PROCESS_DPI_UNAWARE = 0,
        PROCESS_SYSTEM_DPI_AWARE = 1,
        PROCESS_PER_MONITOR_DPI_AWARE = 2
    }


    void incSetWin32DPIAwareness() {
        void* userDLL, shcoreDLL;

        bool function() dpiAwareFunc8;
        HRESULT function(DPIAwarenessContext) dpiAwareFuncCtx81;
        HRESULT function(ProcessDPIAwareness) dpiAwareFunc81;

        userDLL = SDL_LoadObject("USER32.DLL");
        if (userDLL) {
            dpiAwareFunc8 = cast(typeof(dpiAwareFunc8)) SDL_LoadFunction(userDLL, "SetProcessDPIAware");
            dpiAwareFuncCtx81 = cast(typeof(dpiAwareFuncCtx81)) SDL_LoadFunction(userDLL, "SetProcessDpiAwarenessContext");
        }
        
        shcoreDLL = SDL_LoadObject("SHCORE.DLL");
        if (shcoreDLL) {
            dpiAwareFunc81 = cast(typeof(dpiAwareFunc81)) SDL_LoadFunction(shcoreDLL, "SetProcessDpiAwareness");
        }
        
        if (dpiAwareFuncCtx81) {
            dpiAwareFuncCtx81(DPIAwarenessContext.DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
            dpiAwareFuncCtx81(DPIAwarenessContext.DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else if (dpiAwareFunc81) {
            dpiAwareFunc81(ProcessDPIAwareness.PROCESS_PER_MONITOR_DPI_AWARE);
        } else if (dpiAwareFunc8) dpiAwareFunc8();


        // Unload the DLLs
        if (userDLL) SDL_UnloadObject(userDLL);
        if (shcoreDLL) SDL_UnloadObject(shcoreDLL);        
    }
}


package {
    SDL_GLContext gl_context;
    SDL_Window* window;
    ImGuiIO* io;
    bool done = false;
    ImGuiID viewportDock;
    bool firstFrame = true;
    // Current subtitle (model name) to display in window title
    string windowSubtitle;
    // Cached modified flag to avoid redundant title updates
    bool lastWindowModified = false;

    bool isDarkMode = false;
    string[] files;
    bool isWayland;
    bool isTilingWM;

    
    ImVec4[ImGuiCol.COUNT] incDarkModeColors;
    ImVec4[ImGuiCol.COUNT] incLightModeColors;
    bool viewportBackgroundOverrideActive;
    ImVec4 viewportBackgroundOverrideColor;
    ImVec4 viewportBackgroundStoredColor;

    SDL_Window* tryCreateWindow(string title, SDL_WindowFlags flags) {
        auto w = SDL_CreateWindow(
            title.toStringz, 
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            cast(uint)incSettingsGet!int("WinW", 1280), 
            cast(uint)incSettingsGet!int("WinH", 800), 
            flags
        );
        if (w) SDL_SetWindowMinimumSize(window, 960, 720);
        return w;
    }

}

bool incShowStatsForNerds;
bool ngDifferenceAggregationDebugEnabled;
size_t ngDifferenceAggregationTargetIndex;
size_t ngDifferenceAggregationResolvedIndex = size_t.max;
DifferenceEvaluationResult ngDifferenceAggregationResult;
bool ngDifferenceAggregationResultValid;
ulong ngDifferenceAggregationResultSerial;


bool incIsWayland() {
    return isWayland;
}

bool incIsTilingWM() {
    return isTilingWM;
}

/**
    Opens Window
*/
void incOpenWindow() {
    import std.process : environment;
    import std.string : fromStringz;

    switch(environment.get("XDG_SESSION_DESKTOP")) {
        case "i3":

        // Items beyond this point are just guesstimations.
        case "awesome":
        case "bspwm":
        case "dwm":
        case "echinus":
        case "euclid-wm":
        case "herbstluftwm":
        case "leftwm":
        case "notion":
        case "qtile":
        case "ratpoison":
        case "snapwm":
        case "stumpwm":
        case "subtle":
        case "wingo":
        case "wmfs":
        case "xmonad":
        case "wayfire":
        case "river":
        case "labwc":
            isTilingWM = true;
            break;
        
        default:
            isTilingWM = false;
            break;
    }


    // Load SDL2 in the order required for Steam
    foreach(ver; SDL_VERSIONS) {
        auto sdlSupport = loadSDL(ver);

        if (sdlSupport != SDLSupport.noLibrary && 
            sdlSupport != SDLSupport.badLibrary) break;
    }

    // Whomp whomp
    enforce(sdlSupport != SDLSupport.noLibrary, "SDL2 library not found!");
    enforce(sdlSupport != SDLSupport.badLibrary, "Bad SDL2 library found!");
    
    version(BindImGui_Dynamic) {
        auto imSupport = loadImGui();
        enforce(imSupport != ImGuiSupport.noLibrary, "cimgui library not found!");
    
        // HACK: For some reason this check fails on some macOS and Linux installations
        version(Windows) enforce(imSupport != ImGuiSupport.badLibrary, "Bad cimgui library found!");
    }

    int code = SDL_Init(SDL_INIT_EVERYTHING & ~SDL_INIT_AUDIO);
    enforce(
        code == 0,
        "Error initializing SDL2! %s".format(SDL_GetError().fromStringz)
    );

    // Do not disable the OS screensaver; allow it explicitly.
    SDL_EnableScreenSaver();

    version(Windows) {
        incSetWin32DPIAwareness();
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GLprofile.SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

    if (incSettingsGet!bool("WinMax", false)) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }

    // Don't make KDE freak out when nijigenerate opens
    if (!incSettingsGet!bool("DisableCompositor")) SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    debug string WIN_TITLE = "nijigenerate "~_("(Debug Mode)");
    else string WIN_TITLE = "nijigenerate "~INC_VERSION;
    
    window = tryCreateWindow(WIN_TITLE, flags);
    
    // On Linux we want to check whether the window was created under wayland or x11
    version(linux) {
        SDL_SysWMinfo info;
        SDL_GetWindowWMInfo(window, &info);
        isWayland = info.subsystem == SDL_SYSWM_TYPE.SDL_SYSWM_WAYLAND;
    }

    GLSupport support;
    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_DestroyWindow(window);

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GLprofile.SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        window = tryCreateWindow(WIN_TITLE, flags);
        gl_context = SDL_GL_CreateContext(window);
    }
    enforce(gl_context !is null, "Failed to create GL 3.2 or 3.1 core context!");
    SDL_GL_SetSwapInterval(1);

    // Load GL 3
    support = loadOpenGL();
    switch(support) {
        case GLSupport.noLibrary:
            throw new Exception("OpenGL library could not be loaded!");

        case GLSupport.noContext:
            throw new Exception("No valid OpenGL context was found!");

        default: break;
    }


    import std.string : fromStringz;
    version(Windows) {
        
        // Windows is heck when it comes to /SUBSYSTEM:windows
    } else {
        debug {
            writefln("GLInfo:\n\t%s\n\t%s\n\t%s\n\t%s\n\tgls=%s",
                glGetString(GL_VERSION).fromStringz,
                glGetString(GL_VENDOR).fromStringz,
                glGetString(GL_RENDERER).fromStringz,
                glGetString(GL_SHADING_LANGUAGE_VERSION).fromStringz,
                support
            );
        }
    }

    // Setup nijilive
    inInit(() { return igGetTime(); });
    
    incCreateContext();

    incInitLogo();

    // Set X11 window icon
    version(linux) {
        if (!isWayland) {
            auto tex = ShallowTexture(cast(ubyte[])import("icon.png"));
            SDL_SetWindowIcon(window, SDL_CreateRGBSurfaceWithFormatFrom(tex.data.ptr, tex.width, tex.height, 32, 4*tex.width,  SDL_PIXELFORMAT_RGBA32));
        }
    }

    // Load Settings
    incShowStatsForNerds = incSettingsCanGet("NerdStats") ? incSettingsGet!bool("NerdStats") : false;

    version(linux) {
        dpInit();
    }
}

void incCreateContext() {

    // Setup IMGUI
    auto ctx = igCreateContext(null);
    io = igGetIO();

    import std.file : exists;
    if (!exists(incGetAppImguiConfigFile())) {
        // TODO: Setup a base config
    }


    // Copy string out of GC memory to make sure it doesn't get yeeted before imgui exits.
    import core.stdc.stdlib : malloc;
    import core.stdc.string : memcpy;
    io.IniFilename = cast(char*)malloc(incGetAppImguiConfigFile().length+1);
    memcpy(cast(void*)io.IniFilename, toStringz(incGetAppImguiConfigFile), incGetAppImguiConfigFile().length+1);
    igLoadIniSettingsFromDisk(io.IniFilename);

    incSetDarkMode(incSettingsGet!bool("DarkMode", false));

    io.ConfigFlags |= ImGuiConfigFlags.DockingEnable;                               // Enable Docking
    io.ConfigWindowsResizeFromEdges = true;                                         // Enable Edge resizing
    version (OSX) io.ConfigMacOSXBehaviors = true;                                  // macOS Behaviours on macOS

    // Force C locale due to imgui removing support for setting decimal separator.
    import i18n.culture : i18nSetLocale;
    i18nSetLocale("C");

    // NOTE: Viewports break DPI scaling system, as such if Viewports is enabled
    // we will be disable DPI scaling.
    version(NoUIScaling) {
        if (!incIsTilingWM) io.ConfigFlags |= ImGuiConfigFlags.ViewportsEnable;         // Enable Viewports (causes freezes)
    } else {
        incInitDPIScaling();
    }

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;                         // Enable Keyboard Navigation
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    incGLBackendInit(null);

    // Setup font handling
    incInitFonts();

    incInitStyling();
    incInitDialogs();
    incResetClearColor();
}

/**
    Gets SDL Window Pointer
*/
SDL_Window* incGetWindowPtr() {
    return window;
}

void incRefreshWindowTitle() {
    import std.string : toStringz;
    if (windowSubtitle.length > 0) {
        string mark = incIsProjectModified() ? " *" : "";
        SDL_SetWindowTitle(window, ("nijigenerate - " ~ windowSubtitle ~ mark).toStringz);
    } else {
        SDL_SetWindowTitle(window, "nijigenerate");
    }
}

void incSetWindowTitle(string subtitle) {
    windowSubtitle = subtitle;
    // Update cached flag and refresh immediately
    lastWindowModified = incIsProjectModified();
    incRefreshWindowTitle();
}

// Checks modified state and refreshes title when it changes
void incUpdateWindowTitleTick() {
    bool modified = incIsProjectModified();
    if (modified != lastWindowModified) {
        lastWindowModified = modified;
        incRefreshWindowTitle();
    }
}

/**
    Finalizes everything by freeing imgui resources, etc.
*/
void incFinalize() {
    // Save settings
    igSaveIniSettingsToDisk(igGetIO().IniFilename);

    // Cleanup
    incGLBackendShutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(null);

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

/**
    Gets dockspace of the viewport
*/
ImGuiID incGetViewportDockSpace() {
    return viewportDock;
}




/**
    Initialize styling
*/
void incInitStyling() {
    //style.WindowBorderSize = 0;
    auto style = igGetStyle();

    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;

    /*
    style.WindowRounding = 4;
    style.ChildRounding = 0;
    style.FrameRounding = 3;
    style.PopupRounding = 6;
    style.ScrollbarRounding = 18;
    style.GrabRounding = 3;
    style.LogSliderDeadzone = 6;
    style.TabRounding = 6;

    style.IndentSpacing = 10;
    style.ItemSpacing.y = 3;
    style.FramePadding.y = 4;

    style.GrabMinSize = 13;
    style.ScrollbarSize = 14;
    style.ChildBorderSize = 1;
    */

    // Don't draw the silly roll menu
    style.WindowMenuButtonPosition = ImGuiDir.None;

    // macOS support
    version(OSX) style.WindowTitleAlign = ImVec2(0.5, 0.5);
    
    
    igStyleColorsDark(style);
    style.Colors[ImGuiCol.Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol.TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol.WindowBg]               = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
    style.Colors[ImGuiCol.ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol.PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    style.Colors[ImGuiCol.Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.16f);
    style.Colors[ImGuiCol.BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.16f);
    style.Colors[ImGuiCol.FrameBg]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol.FrameBgHovered]         = ImVec4(0.15f, 0.15f, 0.15f, 0.40f);
    style.Colors[ImGuiCol.FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 0.67f);
    style.Colors[ImGuiCol.TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    style.Colors[ImGuiCol.TitleBgActive]          = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol.TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    style.Colors[ImGuiCol.MenuBarBg]              = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    style.Colors[ImGuiCol.ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    style.Colors[ImGuiCol.ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    style.Colors[ImGuiCol.ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol.ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol.CheckMark]              = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
    style.Colors[ImGuiCol.SliderGrab]             = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol.SliderGrabActive]       = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol.Button]                 = ImVec4(0.39f, 0.39f, 0.39f, 0.40f);
    style.Colors[ImGuiCol.ButtonHovered]          = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
    style.Colors[ImGuiCol.ButtonActive]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol.Header]                 = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol.HeaderHovered]          = ImVec4(0.28f, 0.28f, 0.28f, 0.80f);
    style.Colors[ImGuiCol.HeaderActive]           = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
    style.Colors[ImGuiCol.Separator]              = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol.SeparatorHovered]       = ImVec4(0.29f, 0.29f, 0.29f, 0.78f);
    style.Colors[ImGuiCol.SeparatorActive]        = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
    style.Colors[ImGuiCol.ResizeGrip]             = ImVec4(0.35f, 0.35f, 0.35f, 0.00f);
    style.Colors[ImGuiCol.ResizeGripHovered]      = ImVec4(0.40f, 0.40f, 0.40f, 0.00f);
    style.Colors[ImGuiCol.ResizeGripActive]       = ImVec4(0.55f, 0.55f, 0.56f, 0.00f);
    style.Colors[ImGuiCol.Tab]                    = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol.TabHovered]             = ImVec4(0.34f, 0.34f, 0.34f, 0.80f);
    style.Colors[ImGuiCol.TabActive]              = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol.TabUnfocused]           = ImVec4(0.14f, 0.14f, 0.14f, 0.97f);
    style.Colors[ImGuiCol.TabUnfocusedActive]     = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
    style.Colors[ImGuiCol.DockingPreview]         = ImVec4(0.62f, 0.68f, 0.75f, 0.70f);
    style.Colors[ImGuiCol.DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol.PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    style.Colors[ImGuiCol.PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol.PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol.PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol.TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    style.Colors[ImGuiCol.TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    style.Colors[ImGuiCol.TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    style.Colors[ImGuiCol.TableRowBg]             = ImVec4(0.310f, 0.310f, 0.310f, 0.267f);
    style.Colors[ImGuiCol.TableRowBgAlt]          = ImVec4(0.463f, 0.463f, 0.463f, 0.267f);
    style.Colors[ImGuiCol.TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    style.Colors[ImGuiCol.DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    style.Colors[ImGuiCol.NavHighlight]           = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
    style.Colors[ImGuiCol.NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol.NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    style.Colors[ImGuiCol.ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    incDarkModeColors = style.Colors.dup;
    
    igStyleColorsLight(style);
    // Accent palette based on green and neutral tones
    ImVec4 accentGreen = ImVec4(0.36f, 0.45f, 0.35f, 1.00f); // primary green accent
    ImVec4 darkGreen = ImVec4(0.18f, 0.23f, 0.18f, 1.00f);   // darker green accent
    ImVec4 lightGreen = ImVec4(0.67f, 0.75f, 0.63f, 1.00f);  // lighter green accent
    ImVec4 black = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);       // near-black text
    ImVec4 white = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);       // pure white
    ImVec4 grey = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);        // medium grey
    ImVec4 lightGrey = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);   // light grey

    // Use dark text on light background
    style.Colors[ImGuiCol.Text] = black;

    // Background colors
    style.Colors[ImGuiCol.WindowBg] = ImVec4(0.97f, 0.975f, 0.97f, 1.00f);
    style.Colors[ImGuiCol.PopupBg] = ImVec4(0.97f, 0.975f, 0.97f, 1.00f);
    style.Colors[ImGuiCol.MenuBarBg] = ImVec4(0.97f, 0.975f, 0.97f, 1.00f);

    // Window title bar
    style.Colors[ImGuiCol.TitleBg] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol.TitleBgActive] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol.TitleBgCollapsed] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

    // Status/toolbar backgrounds and docking
    style.Colors[ImGuiCol.DockingPreview] = lightGrey; // docking preview background
    style.Colors[ImGuiCol.DockingEmptyBg] = white;     // empty docking area background

    // Progress bar colors
    style.Colors[ImGuiCol.PlotHistogram] = accentGreen;
    style.Colors[ImGuiCol.PlotHistogramHovered] = darkGreen;

    // Buttons
    style.Colors[ImGuiCol.Button] = accentGreen;
    style.Colors[ImGuiCol.ButtonHovered] = darkGreen;
    style.Colors[ImGuiCol.ButtonActive] = black;

    // Checkbox checkmark uses green accent
    style.Colors[ImGuiCol.CheckMark] = accentGreen;

    // InputText: white background with grey border
    style.Colors[ImGuiCol.FrameBg] = white;
    style.Colors[ImGuiCol.FrameBgHovered] = grey;
    style.Colors[ImGuiCol.FrameBgActive] = lightGreen;
    style.Colors[ImGuiCol.Border] = grey; // border color set to grey

    // Tabs
    style.Colors[ImGuiCol.Tab] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol.TabHovered] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol.TabActive] = ImVec4(0.97f, 0.975f, 0.97f, 1.00f);
    style.Colors[ImGuiCol.TabUnfocused] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol.TabUnfocusedActive] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);

    // List/selection headers
    style.Colors[ImGuiCol.Header] = grey;
    style.Colors[ImGuiCol.HeaderHovered] = lightGreen;
    style.Colors[ImGuiCol.HeaderActive] = darkGreen;

    // Scrollbar background same as window background
    style.Colors[ImGuiCol.ScrollbarBg] = white;

    // Scrollbar colors
    style.Colors[ImGuiCol.ScrollbarGrab] = grey;
    style.Colors[ImGuiCol.ScrollbarGrabHovered] = darkGreen;
    style.Colors[ImGuiCol.ScrollbarGrabActive] = black;

    // Slider colors
    style.Colors[ImGuiCol.SliderGrab] = accentGreen;
    style.Colors[ImGuiCol.SliderGrabActive] = darkGreen;

    // Table colors (header/border/rows) matching overall style
    // Header uses light grey like title bars; borders are grey; rows use subtle striping
    style.Colors[ImGuiCol.TableHeaderBg]     = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol.TableBorderStrong] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol.TableBorderLight]  = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    // Row background uses subtle stripes over white background
    style.Colors[ImGuiCol.TableRowBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol.TableRowBgAlt]     = ImVec4(0.95f, 0.95f, 0.95f, 0.45f);

    // Misc style parameters
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;

    // Spacing and alignments
    style.FramePadding = ImVec2(12.0f, 8.0f);        // padding inside widgets
    style.ItemSpacing = ImVec2(8.0f, 4.0f);          // spacing between items
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);     // spacing inside items
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);      // center button text
    style.DisplaySafeAreaPadding = ImVec2(10.0f, 10.0f); // extra margin around content
    ngButtonTextColor = ImVec4(1, 1, 1, 1);

    /*
    style.Colors[ImGuiCol.Border] = ImVec4(0.8, 0.8, 0.8, 0.5);
    style.Colors[ImGuiCol.BorderShadow] = ImVec4(0, 0, 0, 0.05);
    style.Colors[ImGuiCol.TitleBg] = ImVec4(0.902, 0.902, 0.902, 1);
    style.Colors[ImGuiCol.TitleBgActive] = ImVec4(0.98, 0.98, 0.98, 1);
    style.Colors[ImGuiCol.Separator] = ImVec4(0.86, 0.86, 0.86, 1);
    style.Colors[ImGuiCol.ScrollbarGrab] = ImVec4(0.68, 0.68, 0.68, 1);
    style.Colors[ImGuiCol.ScrollbarGrabActive] = ImVec4(0.68, 0.68, 0.68, 1);
    style.Colors[ImGuiCol.ScrollbarGrabHovered] = ImVec4(0.64, 0.64, 0.64, 1);
    style.Colors[ImGuiCol.FrameBg] = ImVec4(1, 1, 1, 1);
    style.Colors[ImGuiCol.FrameBgHovered] = ImVec4(0.78, 0.88, 1, 1);
    style.Colors[ImGuiCol.FrameBgActive] = ImVec4(0.76, 0.86, 1, 1);
    style.Colors[ImGuiCol.Button] = ImVec4(0.98, 0.98, 0.98, 1);
    style.Colors[ImGuiCol.ButtonHovered] = ImVec4(1, 1, 1, 1);
    style.Colors[ImGuiCol.ButtonActive] = ImVec4(0.8, 0.8, 0.8, 1);
    style.Colors[ImGuiCol.CheckMark] = ImVec4(0, 0, 0, 1);
    style.Colors[ImGuiCol.Tab] = ImVec4(0.98, 0.98, 0.98, 1);
    style.Colors[ImGuiCol.TabHovered] = ImVec4(1, 1, 1, 1);
    style.Colors[ImGuiCol.TabActive] = ImVec4(0.8, 0.8, 0.8, 1);
    style.Colors[ImGuiCol.TabUnfocused] = ImVec4(0.92, 0.92, 0.92, 1);
    style.Colors[ImGuiCol.TabUnfocusedActive] = ImVec4(0.88, 0.88, 0.88, 1);
    style.Colors[ImGuiCol.MenuBarBg] = ImVec4(0.863, 0.863, 0.863, 1);  
    style.Colors[ImGuiCol.PopupBg] = ImVec4(0.941, 0.941, 0.941, 1);  
    style.Colors[ImGuiCol.Header] = ImVec4(0.990, 0.990, 0.990, 1);  
    style.Colors[ImGuiCol.HeaderHovered] = ImVec4(1, 1, 1, 1);
    */
    incLightModeColors = style.Colors.dup;
    
    style.Colors = isDarkMode ? incDarkModeColors : incLightModeColors;
}

void incPushDarkColorScheme() {
    auto ctx = igGetCurrentContext();
    ctx.Style.Colors = incDarkModeColors;
}

void incPushLightColorScheme() {
    auto ctx = igGetCurrentContext();
    ctx.Style.Colors = incLightModeColors;
}

void incPopColorScheme() {
    auto ctx = igGetCurrentContext();
    ctx.Style.Colors = isDarkMode ? incDarkModeColors : incLightModeColors;
}

void incSetDarkMode(bool darkMode) {
    auto style = igGetStyle();
    style.Colors = darkMode ? incDarkModeColors : incLightModeColors;

    // Set Dark mode setting
    incSettingsSet("DarkMode", darkMode);
    isDarkMode = darkMode;
}

bool incGetDarkMode() {
    return isDarkMode;
}

/**
    Gets whether a frame should be processed
*/
bool incShouldProcess() {
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) == 0;
}


void incFinishFileDrag() {
    files.length = 0;
}

void incBeginLoopNoEv() {
    // Start the Dear ImGui frame
    incGLBackendNewFrame();
    ImGui_ImplSDL2_NewFrame();

    // Do our DPI pre-processing
    igNewFrame();
    incGLBackendBeginRender();

    version(linux) dpUpdate();

    // HACK: prevents the app freezing when files are drag and drop on the modal dialog.
    // freeze is caused by `igSetDragDropPayload()`, so we check if the modal is open.
    if (files.length > 0 && !incModalIsOpen()) {
        if (igBeginDragDropSource(ImGuiDragDropFlags.SourceExtern)) {
            igSetDragDropPayload("__PARTS_DROP", &files, files.sizeof);
            igBeginTooltip();
            foreach(file; files) {
                import nijigenerate.widgets.label : incText;
                incText(file);
            }
            igEndTooltip();
            igEndDragDropSource();
        }
    } else if (incModalIsOpen()) {
        // clean up the files array
        files.length = 0;
    }

    // Update window title dot when modified state toggles
    incUpdateWindowTitleTick();

    // Add docking space
    viewportDock = igDockSpaceOverViewport(null, ImGuiDockNodeFlags.NoDockingInCentralNode, null);
    if (!incSettingsCanGet("firstrun_complete")) {
        // Ensure Armed Parameters panel is visible on first run
        incSettingsSet("Armed Parameters.visible", true);
        incSetDefaultLayout();
        incSettingsSet("firstrun_complete", true);
    }

    // HACK: ImGui Crashes if a popup is rendered on the first frame, let's avoid that.
    if (firstFrame) firstFrame = false;
    else {
        // imgui can not igOpenPopup two popups at the same time, that causes a freeze
        // so we sperate the popups rendering
        if (incModalIsOpen())
            incModalRender();
        else
            incRenderDialogs();
    }

    incHandleDialogHandlers();
}

void incSetDefaultLayout() {
    import nijigenerate.panels;
    
    igDockBuilderRemoveNodeChildNodes(viewportDock);
    ImGuiID 
        dockMainID, dockIDNodes, dockIDInspector, dockIDHistory, dockIDParams,
        dockIDToolSettings, dockIDLoggerAndTextureSlots, dockIDTimeline, dockIDAnimList,
        dockIDArmedParams, dockIDResources;

    dockMainID = viewportDock;
    dockIDAnimList = igDockBuilderSplitNode(dockMainID, ImGuiDir.Left, 0.10f, null, &dockMainID);
    dockIDNodes = igDockBuilderSplitNode(dockMainID, ImGuiDir.Left, 0.10f, null, &dockMainID);
    dockIDInspector = igDockBuilderSplitNode(dockIDNodes, ImGuiDir.Down, 0.60f, null, &dockIDNodes);
    dockIDToolSettings = igDockBuilderSplitNode(dockMainID, ImGuiDir.Right, 0.10f, null, &dockMainID);
    dockIDHistory = igDockBuilderSplitNode(dockIDToolSettings, ImGuiDir.Down, 0.50f, null, &dockIDToolSettings);
    dockIDTimeline = igDockBuilderSplitNode(dockMainID, ImGuiDir.Down, 0.15f, null, &dockMainID);
    dockIDParams = igDockBuilderSplitNode(dockMainID, ImGuiDir.Left, 0.15f, null, &dockMainID);
    // Split Parameters area to place Armed Parameters above Parameters
    dockIDArmedParams = igDockBuilderSplitNode(dockIDParams, ImGuiDir.Up, 0.3f, null, &dockIDParams);
    dockIDResources = igDockBuilderSplitNode(dockMainID, ImGuiDir.Left, 0.20f, null, &dockMainID);

    igDockBuilderDockWindow("###Nodes", dockIDNodes);
    igDockBuilderDockWindow("###Inspector", dockIDInspector);
    igDockBuilderDockWindow("###Tool Settings", dockIDToolSettings);
    igDockBuilderDockWindow("###History", dockIDHistory);
    igDockBuilderDockWindow("###Scene", dockIDHistory);
    igDockBuilderDockWindow("###Timeline", dockIDTimeline);
    igDockBuilderDockWindow("###Animation List", dockIDAnimList);
    igDockBuilderDockWindow("###Logger", dockIDTimeline);
    igDockBuilderDockWindow("###Armed Parameters", dockIDArmedParams);
    igDockBuilderDockWindow("###Parameters", dockIDParams);
    igDockBuilderDockWindow("###Texture Slots", dockIDLoggerAndTextureSlots);
    igDockBuilderDockWindow("###Resources", dockIDResources);

    igDockBuilderFinish(viewportDock);
}

/**
    Begins the nijigenerate rendering loop
*/
void incBeginLoop() {
    SDL_Event event;

    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_QUIT:
                incExitSaveAsk();
                break;

            case SDL_DROPFILE:
                files ~= cast(string)event.drop.file.fromStringz;
                SDL_RaiseWindow(window);
                break;
            
            default: 
                incGLBackendProcessEvent(&event);
                break;
        }
    }

    incTaskUpdate();

    // Begin loop post-event
    incBeginLoopNoEv();
}

/**
    Ends the nijigenerate rendering loop
*/
void incEndLoop() {
    // incGLBackendEndRender();

    incCleanupDialogs();

    // Rendering
    igRender();
    glViewport(0, 0, cast(int)(io.DisplaySize.x*incGetUIScale), cast(int)(io.DisplaySize.y*incGetUIScale));
    glClearColor(0.5, 0.5, 0.5, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    incGLBackendRenderDrawData(igGetDrawData());

    if (io.ConfigFlags & ImGuiConfigFlags.ViewportsEnable) {
        SDL_Window* currentWindow = SDL_GL_GetCurrentWindow();
        SDL_GLContext currentCtx = SDL_GL_GetCurrentContext();
        igUpdatePlatformWindows();
        igRenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(currentWindow, currentCtx);
    }

    SDL_GL_SwapWindow(window);
}

/**
    Prints ImGui debug info
*/
void incDebugImGuiState(string msg, int indent = 0) {
    debug(imgui) {
        static int currentIndent = 0;

        string flag = "  ";
        if (indent > 0) {
            currentIndent += indent;
            flag = ">>";
        } else if (indent < 0) {
            flag = "<<";
        }

        //auto g = igGetCurrentContext();
        auto win = igGetCurrentWindow();
        writefln(
            "%s%s%s [%s]", ' '.repeat(currentIndent * 2), flag, msg,
            to!string(win.Name)
        );

        if (indent < 0) {
            currentIndent += indent;
            if (currentIndent < 0) {
                debug writeln("ERROR: dedented too far!");
                currentIndent = 0;
            }
        }
    }
}

/**
    Resets the clear color
*/
void incResetClearColor() {
    ImVec4 defaultColor = incGetDarkMode() ? ImVec4(0, 0, 0, 1) : ImVec4(1, 1, 1, 1);
    inSetClearColor(defaultColor.x, defaultColor.y, defaultColor.z, defaultColor.w);
    if (viewportBackgroundOverrideActive) {
        viewportBackgroundStoredColor = defaultColor;
        inSetClearColor(
            viewportBackgroundOverrideColor.x,
            viewportBackgroundOverrideColor.y,
            viewportBackgroundOverrideColor.z,
            viewportBackgroundOverrideColor.w,
        );
    }
}


bool incViewportHasTemporaryBackgroundColor() {
    return viewportBackgroundOverrideActive;
}

void incViewportGetBackgroundColor(ref ImVec4 color) {
    if (viewportBackgroundOverrideActive) {
        color = viewportBackgroundOverrideColor;
    } else {
        inGetClearColor(color.x, color.y, color.z, color.w);
    }
}

ImVec4 incViewportGetBackgroundColor() {
    ImVec4 color;
    incViewportGetBackgroundColor(color);
    return color;
}

/**
    Temporarily overrides the viewport background color until cleared.
*/
void incViewportSetTemporaryBackgroundColor(float r, float g, float b, float a = 1.0f) {
    if (!viewportBackgroundOverrideActive) {
        inGetClearColor(
            viewportBackgroundStoredColor.x,
            viewportBackgroundStoredColor.y,
            viewportBackgroundStoredColor.z,
            viewportBackgroundStoredColor.w,
        );
        viewportBackgroundOverrideActive = true;
    }
    viewportBackgroundOverrideColor = ImVec4(r, g, b, a);
    inSetClearColor(r, g, b, a);
}

void incViewportSetTemporaryBackgroundColor(ImVec4 color) {
    incViewportSetTemporaryBackgroundColor(color.x, color.y, color.z, color.w);
}

/**
    Restores the viewport background color to the stored value.
*/
void incViewportClearTemporaryBackgroundColor() {
    if (!viewportBackgroundOverrideActive)
        return;
    inSetClearColor(
        viewportBackgroundStoredColor.x,
        viewportBackgroundStoredColor.y,
        viewportBackgroundStoredColor.z,
        viewportBackgroundStoredColor.w,
    );
    viewportBackgroundOverrideActive = false;
}
/**
    Gets whether nijigenerate has requested the app to close
*/
bool incIsCloseRequested() {
    return done;
}

/**
    Exit nijigenerate
*/
void incExit() {
    done = true;

    int w, h;
    SDL_WindowFlags flags;
    flags = SDL_GetWindowFlags(window);
    SDL_GetWindowSize(window, &w, &h);
    incSettingsSet("WinW", w);
    incSettingsSet("WinH", h);
    incSettingsSet!bool("WinMax", (flags & SDL_WINDOW_MAXIMIZED) > 0);
    incReleaseLockfile();
}

