module nijigenerate.widgets.notification;

import bindbc.imgui;
import nijigenerate.widgets.button;
import std.string;

class NotificationPopup {
private:
    float statusTime;
    string message;
    char* messagez;

    static NotificationPopup instance_ = null;
public:
    this() { }

    static NotificationPopup instance() {
        if (instance_ is null) {
            instance_ = new NotificationPopup();
        }
        return instance_;
    }

    string status() { return message; }
    void popup(string text, float duration) {
        message = text;
        messagez = cast(char*)message.toStringz;
        statusTime = duration;
    }

    void onUpdate() {
        if (message) {
            auto io = igGetIO();
            auto viewportSize = ImVec2(io.DisplaySize.x, io.DisplaySize.y);

            igSetNextWindowPos(ImVec2(viewportSize.x * 0.5f - 150, 0), ImGuiCond.Always, ImVec2(0.5f, 0.0f));

            ImVec2 textSize;
            igCalcTextSize(&textSize, messagez);

            igSetNextWindowSize(ImVec2(textSize.x + 100, 50), ImGuiCond.Always);

            ImGuiWindowFlags flags = ImGuiWindowFlags.NoTitleBar | ImGuiWindowFlags.NoResize |
                                    ImGuiWindowFlags.NoMove | ImGuiWindowFlags.NoSavedSettings |
                                    ImGuiWindowFlags.NoScrollbar;

            ImVec4 lightGreen = ImVec4(0.67f, 0.75f, 0.63f, 1.00f); // 画像の明るい緑色に基づく
            igPushStyleColor(ImGuiCol.WindowBg, lightGreen);

            if (igBegin("##NotificationPopup", null, flags))
            {
                igText(messagez);
                igSameLine();
                if (incButtonColored("\ue5cd", ImVec2(20, 20))){
                    message = null;
                }
            }

            igEnd();
            igPopStyleColor();
            statusTime -= igGetIO().DeltaTime;
            if (statusTime < 0) {
                message = null;
            }
        }

    }
}