/*
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.widgets.modal.nagscreen;
import bindbc.sdl;
import nijigenerate.widgets.label;
import nijigenerate.widgets.markdown;
import nijigenerate.widgets.dummy;
import nijigenerate.widgets.modal;
import nijigenerate.widgets.button;
import nijigenerate.core;
import nijigenerate.core.i18n;
import std.string;
import nijigenerate.utils.link;
import i18n;
import nijilive;
import nijigenerate.ver;
import nijigenerate.io;
import nijigenerate;
import nijigenerate.config;

class Nagscreen : Modal {
private:

    ImVec2 uiSize;
    ImVec2 origWindowPadding;
    ImDrawList* shadowDrawList;

    // Timeout in seconds
    long startTime;
    int timeout;

    // Message
    string message;

    MarkdownConfig cfg;

protected:
    override
    void onBeginUpdate() {

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
        long currTime = cast(long)igGetTime();
        long passedTime = currTime-startTime;
        long timeLeft = timeout-passedTime;
        
        // Fix styling for subwindows
        igPushStyleVar(ImGuiStyleVar.WindowPadding, origWindowPadding);
        auto windowViewport = igGetWindowViewport();
        windowViewport.Flags |= ImGuiViewportFlags.TopMost;
        windowViewport.Flags |= ImGuiViewportFlags.NoDecoration;
        windowViewport.Flags |= ImGuiViewportFlags.NoTaskBarIcon;
        

        ImVec2 origin;
        igGetCursorStartPos(&origin);

        igIndent();
            if (igBeginChild("##BODY", ImVec2(-4, 0), false, ImGuiWindowFlags.NoScrollbar)) {
                ImVec2 avail = incAvailableSpace();
                igPushTextWrapPos(avail.x);

                    igSetCursorPosY(16);

                    igSetWindowFontScale(1.8);
                        ImVec2 size = incMeasureString(_(title));

                        incDummy(ImVec2((avail.x/2)-(size.x/2), size.y));
                        igSameLine(0, 0);
                        incTextShadowed(_(title));
                    igSetWindowFontScale(1);
                    igNewLine();

                    incDummy(ImVec2(16, 64));
                    igSameLine(0, 0);

                    ImVec2 availB = incAvailableSpace();
                    
                    igPushStyleColor(ImGuiCol.Button, ImVec4(0.176, 0.447, 0.698, 1));
                    igPushStyleColor(ImGuiCol.ButtonHovered, ImVec4(0.313, 0.521, 0.737, 1));
                        igBeginGroup();
                            igPushItemWidth(availB.x-16);
                                incMarkdown(_(message), cfg);
                            igPopItemWidth();
                        igEndGroup();
                    igPopStyleColor();
                    igPopStyleColor();

                    // Move down to where we want our button
                    incDummy(ImVec2(0, -40));

                    // Move button to the center
                    incDummy(ImVec2((avail.x/2)-32, 24));
                    igSameLine(0, 0);

                    const(char)* btnText;
                    if (timeLeft > 0) {
                        btnText = "(%s)".format(timeLeft).toStringz;
                    } else {
                        btnText = __("Close");
                    }

                    igBeginDisabled(timeLeft > 0);
                        if (incButtonColored(btnText, ImVec2(64, 24))) {
                            incModalCloseTop();
                        }
                    igEndDisabled();
                igPopTextWrapPos(); 
            }
            igEndChild();
        igUnindent();
        igPopStyleVar();
    }


public:
    this(string title, string msg, int timeout, float baseHeight = 384) {
        super(title, false);
        this.message = msg;
        this.timeout = timeout;

        cfg.headingFormats[0] = MarkdownHeadingFormat(2, true);
        cfg.headingFormats[1] = MarkdownHeadingFormat(1.5, false);
        cfg.headingFormats[2] = MarkdownHeadingFormat(1.2, false);
        cfg.linkCallback = (MarkdownLinkCallbackData data) {
            incOpenLink(data.link);
        };

        startTime = cast(long)igGetTime();

        // Load UI size
        uiSize = ImVec2(
            512, 
            baseHeight
        );
    }
}