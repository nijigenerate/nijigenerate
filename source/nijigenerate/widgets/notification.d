module nijigenerate.widgets.notification;

import bindbc.imgui;
import nijigenerate.widgets.button;
import std.string;

class NotificationPopup {
private:
    float statusTime;
    bool infinite = false;
    bool visible = false;
    char* messagez;
    void delegate(ImGuiIO* io) callback = null;

protected:
    static NotificationPopup instance_ = null;
public:
    this() { }

    static NotificationPopup instance() {
        if (instance_ is null) {
            instance_ = new NotificationPopup();
        }
        return instance_;
    }

    string status() { return cast(string)(messagez.fromStringz); }
    void popup(string text, float duration) {
        messagez = cast(char*)text.toStringz;
        statusTime = duration;
        visible = true;
        infinite = statusTime < 0;
    }

    void popup(void delegate(ImGuiIO*) _callback, float duration) {
        callback = _callback;
        messagez = null;
        statusTime = duration;
        visible = true;
        infinite = statusTime < 0;
    }

    void onUpdate() {
        if (visible) {
            auto io = igGetIO();
            auto viewportSize = ImVec2(io.DisplaySize.x, io.DisplaySize.y);

            igSetNextWindowPos(ImVec2(viewportSize.x * 0.5f - 150, 0), ImGuiCond.Always, ImVec2(0.5f, 0.0f));

            ImGuiWindowFlags flags = ImGuiWindowFlags.NoTitleBar | ImGuiWindowFlags.NoResize |
                                    ImGuiWindowFlags.NoMove | ImGuiWindowFlags.NoSavedSettings |
                                    ImGuiWindowFlags.NoScrollbar;
            // For callback-driven content, allow auto-resize to fit widgets (e.g., dropdowns)
            if (callback) {
                flags |= ImGuiWindowFlags.AlwaysAutoResize;
            } else {
                ImVec2 textSize;
                igCalcTextSize(&textSize, messagez);
                igSetNextWindowSize(ImVec2(textSize.x + 100, 50), ImGuiCond.Always);
            }

            ImVec4 lightGreen = ImVec4(0.67f, 0.75f, 0.63f, 1.00f); // 画像の明るい緑色に基づく
            igPushStyleColor(ImGuiCol.WindowBg, lightGreen);

            if (igBegin("##NotificationPopup", null, flags))
            {
                if (callback) {
                    // For callback-driven content (e.g., dropdown lists), rely on ESC/outside-click to close.
                    // Avoid placing a close button on the same row to prevent layout issues.
                    callback(io);
                } else if (messagez) {
                    igText(messagez);
                    igSameLine();
                    if (incButtonColored("\ue5cd", ImVec2(20, 20))){
                        visible = false;
                    }
                }
            }

            igEnd();
            igPopStyleColor();
            statusTime -= igGetIO().DeltaTime;
            if (!infinite && statusTime < 0) {
                visible = false;
            }
        }

    }

    // Close and clear the popup immediately
    void close() {
        visible = false;
        infinite = false;
        statusTime = 0;
        messagez = null;
        callback = null;
    }
}
