#pragma once
#include "nijigenerate/windows/base.hpp"
#include "nijigenerate/widgets/markdown.hpp"
#include <math/types.hpp>
#include <limits>

namespace nijigenerate::windows::about {
class AboutWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    static constexpr int ADA_SIZE = 332;
    static constexpr int ADA_SIZE_PARTIAL = ADA_SIZE / 6;
    nicxlive::core::math::Vec2 ada_float{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    widgets::markdown::MarkdownConfig cfg;
    void initialize();
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
public:
    AboutWindow() = default;
};
} // namespace nijigenerate::windows::about
