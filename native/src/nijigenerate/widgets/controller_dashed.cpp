#include <nijigenerate/widgets/controller.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::widgets {

void ngDrawLineDashed(ImDrawList* list, ImVec2 from, ImVec2 to, ImU32 color,
                      float thickness, int segments, float lineScale) {
    if ((color >> 24) == 0) return;

    ImVec2 direction{(to.x - from.x) / segments, (to.y - from.y) / segments};
    bool on = true;
    ImVec2 points[2];
    for (int index = 0; index < segments; ++index) {
        points[index % 2] = ImVec2{from.x + direction.x * index, from.y + direction.y * index};
        if (index != 0 && index % 2 == 0) {
            if (on) {
                compat::ngRequireObject(list)->PathLineTo(ImVec2{points[0].x - direction.x * lineScale,
                                        points[0].y - direction.y * lineScale});
                compat::ngRequireObject(list)->PathLineTo(ImVec2{points[1].x + direction.x * lineScale,
                                        points[1].y + direction.y * lineScale});
                compat::ngRequireObject(list)->PathStroke(color, ImDrawFlags_None, thickness);
            }
            on = !on;
        }
    }
    compat::ngRequireObject(list)->PathClear();
}

} // namespace nijigenerate::widgets
