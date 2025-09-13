module nijigenerate.widgets.notification;

import bindbc.imgui;
import nijigenerate.widgets.button;
import std.string;

class NotificationPopup {
private:
    struct Item {
        float remaining;
        bool infinite;
        char* messagez;
        void delegate(ImGuiIO* io) callback;
    }
    Item[] items; // stack: older at front, newest at back

protected:
    static NotificationPopup instance_ = null;
public:
    this() { }

    static NotificationPopup instance() {
        if (instance_ is null) instance_ = new NotificationPopup();
        return instance_;
    }

    // Return latest message text for status bar; empty if none or callback-only
    string status() {
        foreach_reverse (it; items) {
            if (it.messagez !is null) return cast(string)(it.messagez.fromStringz);
        }
        return "";
    }

    // Push a text notification
    void popup(string text, float duration) {
        Item it;
        it.messagez = cast(char*)text.toStringz;
        it.remaining = duration;
        it.infinite = duration < 0;
        it.callback = null;
        items ~= it;
    }

    // Push an interactive notification
    void popup(void delegate(ImGuiIO*) _callback, float duration) {
        Item it;
        it.messagez = null;
        it.remaining = duration;
        it.infinite = duration < 0;
        it.callback = _callback;
        items ~= it;
    }

    void onUpdate() {
        if (items.length == 0) return;

        auto io = igGetIO();
        ImVec2 basePos = ImVec2(io.DisplaySize.x * 0.5f, 8);
        float yOffset = 0;
        ImVec4 lightGreen = ImVec4(0.67f, 0.75f, 0.63f, 1.00f);

        // Draw from newest (back) to oldest (front), stacking downward
        foreach_reverse (idx, it; items) {
            ImGuiWindowFlags flags = ImGuiWindowFlags.NoTitleBar | ImGuiWindowFlags.NoResize |
                                     ImGuiWindowFlags.NoMove | ImGuiWindowFlags.NoSavedSettings |
                                     ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.AlwaysAutoResize;

            // Position this item
            igSetNextWindowPos(ImVec2(basePos.x, basePos.y + yOffset), ImGuiCond.Always, ImVec2(0.5f, 0.0f));

            igPushStyleColor(ImGuiCol.WindowBg, lightGreen);
            import std.conv : to;
            auto winLabel = "##NotificationPopup" ~ idx.to!string;
            if (igBegin(winLabel.toStringz, null, flags)) {
                if (it.callback !is null) {
                    it.callback(io);
                } else if (it.messagez !is null) {
                    igText(it.messagez);
                    igSameLine();
                    if (incButtonColored("\ue5cd", ImVec2(20, 20))) {
                        // Mark as expired immediately
                        items[idx].remaining = 0;
                    }
                }
            }
            // Measure height used to stack next item below
            ImVec2 curSize;
            igGetWindowSize(&curSize);
            yOffset += curSize.y + 6; // gap between popups
            igEnd();
            igPopStyleColor();
        }

        // Update timers and cull expired
        float dt = igGetIO().DeltaTime;
        Item[] kept;
        foreach (it; items) {
            if (!it.infinite) it.remaining -= dt;
            if (it.infinite || it.remaining > 0) kept ~= it;
        }
        items = kept;
    }

    // Close all popups immediately
    void close() {
        items.length = 0;
    }
}
