/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.windows.welcome;
import nijigenerate.widgets.label;
import nijigenerate.widgets.dummy;
import nijigenerate.widgets.button;
import nijigenerate.windows.base;
import nijigenerate.core;
import nijigenerate.core.i18n;
import std.string;
import std.file : FileException;
import nijigenerate.utils.link;
import i18n;
import nijilive;
import nijigenerate.ver;
import nijigenerate.io;
import nijigenerate;
import nijigenerate.config;
import nijigenerate.widgets.dialog;

import nijigenerate.widgets.shadow;

class WelcomeWindow : Window {
private:
    int step = 1;
    Texture banner;
    Texture bannerLogo;
    ImVec2 origWindowPadding;
    bool firstFrame = true;
    bool changesRequiresRestart;
    
    // Temporary variables for setup
    int tmpUIScale;

    ImVec2 uiSize;
    ImDrawList* shadowDrawList;

protected:
    override
    void onBeginUpdate() {
        flags |= ImGuiWindowFlags.NoResize;
        flags |= ImGuiWindowFlags.NoDecoration;

        ImVec2 wpos = ImVec2(
            igGetMainViewport().Pos.x+(igGetMainViewport().Size.x/2),
            igGetMainViewport().Pos.y+(igGetMainViewport().Size.y/2),
        );

        igSetNextWindowPos(wpos, ImGuiCond.Always, ImVec2(0.5, 0.5));
        igSetNextWindowSize(uiSize, ImGuiCond.Appearing);
        igSetNextWindowSizeConstraints(uiSize, uiSize);
        origWindowPadding = igGetStyle().WindowPadding;
        igPushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2(0, 0));
        igPushStyleVar(ImGuiStyleVar.WindowRounding, 10);
        super.onBeginUpdate();
    }

    override
    void onEndUpdate() {
        igPopStyleVar(2);
        super.onEndUpdate();
    }

    override
    void onUpdate() {
        auto window = igGetCurrentWindow();
        incRenderWindowShadow(
            shadowDrawList,
            window.OuterRectClipped
        );
        
        // Fix styling for subwindows
        igPushStyleVar(ImGuiStyleVar.WindowPadding, origWindowPadding);
        auto windowViewport = igGetWindowViewport();
        windowViewport.Flags |= ImGuiViewportFlags.TopMost;
        windowViewport.Flags |= ImGuiViewportFlags.NoDecoration;
        windowViewport.Flags |= ImGuiViewportFlags.NoTaskBarIcon;
        

        ImVec2 origin;
        igGetCursorStartPos(&origin);
        if (igBeginChild("##BANNER", ImVec2(0, 200))) {
            igPushStyleColor(ImGuiCol.Text, 0xFFFFFFFF);
                ImVec2 spos;
                igGetCursorScreenPos(&spos); 
                
                // Background
                ImDrawList_AddImageRounded(
                    igGetWindowDrawList(), 
                    cast(void*)banner.getTextureId(),
                    ImVec2(spos.x+1, spos.y+1),
                    ImVec2(spos.x+511, spos.y+199),
                    ImVec2(0, 0),
                    ImVec2(1, 1),
                    0xFFFFFFFF,
                    10,
                    ImDrawFlags.RoundCornersTop
                );

                //Logo
                igSetCursorPos(ImVec2(0, 0));
                igImage(cast(void*)bannerLogo.getTextureId(), ImVec2(bannerLogo.width/5, bannerLogo.height/5));

                // Version String
                ImVec2 vsSize = incMeasureString(INC_VERSION);
                igSetCursorPos(ImVec2(512-(vsSize.x+12), 12));
                incTextShadowed(INC_VERSION);
                
                // Banner Artist Name
                string artistString = INC_BANNER_ARTIST_NAME;
                vsSize = incMeasureString(artistString);
                igSetCursorPos(ImVec2(512-(vsSize.x+12), 200-(vsSize.y+8)));
                incTextBordered(artistString);

                if (igIsItemHovered()) {
                    igSetMouseCursor(ImGuiMouseCursor.Hand);
                }

                // Clicking artist link sends you to the artist's page
                if (igIsItemClicked()) {
                    incOpenLink(INC_BANNER_ARTIST_PAGE);
                }
            igPopStyleColor();
        } 
        igEndChild();
        
        igIndent();
        if (igBeginChild("##CONFIG_AREA", ImVec2(-4, 0), false, ImGuiWindowFlags.NoScrollbar)) {
            ImVec2 avail = incAvailableSpace();
            igPushTextWrapPos(avail.x);
            switch(step) {

                // SETUP PAGE
                case 0:
                    incDummy(ImVec2(0, 4));

                    incTextShadowed(_("Quick Setup"));
                    igNewLine();

                    incDummy(ImVec2(avail.x/6, 64));
                    igSameLine(0, 0);
                    igBeginGroup();
                        igPushItemWidth(avail.x/3);
                            auto comboFlags = 
                                ImGuiComboFlags.NoArrowButton | 
                                ImGuiComboFlags.HeightLargest;
                            
                            if(igBeginCombo(__("Language"), incLocaleCurrentName().toStringz, comboFlags)) {
                                if (igSelectable("English")) {
                                    incLocaleSet(null);
                                    changesRequiresRestart = true;
                                }
                                foreach(entry; incLocaleGetEntries()) {
                                    if (igSelectable(entry.humanNameC)) {
                                        incLocaleSet(entry.code);
                                        changesRequiresRestart = true;
                                    }
                                }
                                igEndCombo();
                            }

                            if(igBeginCombo(__("Color Theme"), incGetDarkMode() ? __("Dark") : __("Light"), comboFlags)) {
                                if (igSelectable(__("Dark"), incGetDarkMode())) incSetDarkMode(true);
                                if (igSelectable(__("Light"), !incGetDarkMode())) incSetDarkMode(false);

                                igEndCombo();
                            }

                            version (UseUIScaling) {
                                version(OSX) {

                                    // macOS follows Retina scaling, skip showing this
                                } else {
                                    if (igInputInt(__("UI Scale"), &tmpUIScale, 25, 50, ImGuiInputTextFlags.EnterReturnsTrue)) {
                                        tmpUIScale = clamp(tmpUIScale, 100, 200);
                                        incSetUIScale(cast(float)tmpUIScale/100.0);
                                    }
                                }
                            }

                            if (changesRequiresRestart) {
                                igNewLine();
                                igPushTextWrapPos(avail.x/1.15);
                                    incTextColored(
                                        ImVec4(0.8, 0.2, 0.2, 1), 
                                        _("nijigenerate needs to be restarted for some changes to take effect.")
                                    );
                                igPopTextWrapPos();
                            }
                        igPopItemWidth();
                    igEndGroup();

                    // Move down to where we want our button
                    incDummy(ImVec2(0, -32));

                    // Move button to the right
                    incDummy(ImVec2(-64, 24));
                    igSameLine(0, 0);
                    if (incButtonColored(__("Next"), ImVec2(64, 24))) {
                        incSettingsSet!bool("hasDoneQuickSetup", true);
                        step++;
                    }
                    break;

                // WELCOME PAGE
                case 1:
                    incDummy(ImVec2(0, 4));

                    // Left hand side
                    if (igBeginChild("##LHS", ImVec2((avail.x-8)/2, 0), false, ImGuiWindowFlags.NoScrollbar)) {
                        incTextShadowed(_("Create Project"));
                        incDummy(ImVec2(0, 2));
                        igIndent();
                            if (incTextLinkWithIcon("", _("New..."))) {
                                incNewProject();
                                this.close();
                            }

                            if (incTextLinkWithIcon("", _("Open..."))) {
                                const TFD_Filter[] filters = [
                                    { ["*.inx"], "nijigenerate Project (*.inx)" }
                                ];

                                string file = incShowOpenDialog(filters, _("Open..."));
                                if (file) {
                                    // FileException should handle in incOpenProject, so we don't write try/catch here
                                    if (incOpenProject(file))
                                        this.close();
                                }
                            }


                            if (incTextLinkWithIcon("", _("Import..."))) {
                                igOpenPopup("IMPORT_OPTIONS");
                            }
                            
                            if (igBeginPopup("IMPORT_OPTIONS")) {
                                if(igMenuItem(__("Import PSD..."))) {
                                    if (incImportShowPSDDialog()) {
                                        this.close();
                                    }
                                }

                                if(igMenuItem(__("Import KRA..."))) {
                                    if (incImportShowKRADialog()) {
                                        this.close();
                                    }
                                }

                                igEndPopup();
                            }

                        igUnindent();

                        incDummy(ImVec2(0, 6));
                        incTextShadowed(_("Recent Projects"));
                        incDummy(ImVec2(0, 2));
                        igIndent();
                            if (incGetPrevProjects().length > 0) {
                                foreach(i, recent; incGetPrevProjects()) {
                                    if (i >= 4) break;

                                    import std.path : baseName;
                                    if (incTextLinkWithIcon("", recent.baseName)) {
                                        // FileException should handle in incOpenProject, so we don't write try/catch here
                                        if (incOpenProject(recent))
                                            this.close();
                                    }
                                }
                            } else {
                                incTextShadowed("No recent projects...");
                            }
                        igUnindent();
                    }
                    igEndChild();

                    igSameLine(0, 4);

                    // Right hand side
                    if (igBeginChild("##RHS", ImVec2((avail.x-8)/2, 0), false, ImGuiWindowFlags.NoScrollbar)) {
                        incTextShadowed(_("On the Web"));
                        incDummy(ImVec2(0, 2));
                        igIndent();

                            static if (INC_INFO_WEBSITE_URI.length > 0) {
                                if (incTextLinkWithIcon("", _("Website"))) {
                                    incOpenLink(INC_INFO_WEBSITE_URI);
                                }
                            }

                            static if (INC_INFO_DOCS_URI.length > 0) {
                                if (incTextLinkWithIcon("", _("Documentation"))) {
                                    incOpenLink(INC_INFO_DOCS_URI);
                                }
                            }
                            /*                            
                            static if (INC_INFO_DISCORD_URI.length > 0) {
                                if (incTextLinkWithIcon("", _("Join our Discord"))) {
                                    incOpenLink(INC_INFO_DISCORD_URI);
                                }
                            }
                            */

                        igUnindent();
                    }
                    igEndChild();
                    break;

                default:
                    this.close();
                    break;
            }
            igPopTextWrapPos(); 
        }
        igEndChild();
        igUnindent();
        igPopStyleVar();
    }

    override
    void onClose() {
        if (step > 0) incSettingsSet!bool("hasDoneQuickSetup", true);
        incDestroyWindowDrawList(shadowDrawList);
    }

public:
    this() {
        super(_("nijigenerate Start"));

        auto bannerTex = ShallowTexture(cast(ubyte[])import("ui/banner.png"));
        banner = new Texture(bannerTex);
        banner.setAnisotropy(1.5);

        auto bannerLogoTex = ShallowTexture(cast(ubyte[])import("ui/banner-logo.png"));   
        inTexPremultiply(bannerLogoTex.data); 
        bannerLogo = new Texture(bannerLogoTex);

        if (!incSettingsGet!bool("hasDoneQuickSetup", false)) step = 0;

        // Load UI scale
        tmpUIScale = cast(int)(incGetUIScale()*100);

        uiSize = ImVec2(
            512, 
            384
        );

        shadowDrawList = incCreateWindowDrawList();
    }
}
