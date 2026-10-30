#pragma once
#include "base.hpp"
#include <limits>

namespace nicxlive::core { class Texture; }

namespace nijigenerate::windows::welcome {
class WelcomeWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    int step = 1;
    std::shared_ptr<nicxlive::core::Texture> banner, bannerLogo;
    ImVec2 origWindowPadding{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    bool firstFrame = true;
    bool changesRequiresRestart = false;
    int tmpUIScale = 0;
    ImVec2 uiSize{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    ImDrawList* shadowDrawList = nullptr;
    void initialize();
protected:
    void onBeginUpdate() override;
    void onEndUpdate() override;
    void onUpdate() override;
    void onClose() override;
public:
    WelcomeWindow() = default;
};
}
