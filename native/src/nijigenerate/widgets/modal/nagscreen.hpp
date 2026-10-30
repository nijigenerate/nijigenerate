/*
    Copyright (c) 2024, Inochi2D Project
    Copyright (c) 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    Authors: Luna Nielsen
    Altered C++ declaration port from the independent project D module.
*/
#pragma once
#include "nijigenerate/widgets/modal/base.hpp"
#include "nijigenerate/widgets/markdown.hpp"
#include <cstdint>
#include <limits>

namespace nijigenerate::widgets::modal {
class Nagscreen : public Modal {
private:
    ImVec2 uiSize{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
    ImVec2 origWindowPadding{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
    ImDrawList* shadowDrawList = nullptr;
    std::int64_t startTime = 0;
    std::int32_t timeout = 0;
    compat::RetainedStringSlice message{};
    markdown::MarkdownConfig cfg{};

protected:
    void onBeginUpdate() override;
    void onEndUpdate() override;
    void onUpdate() override;
    void prepareSourceClassInfo() override {
        Modal::prepareSourceClassInfo();
        registerSourceClassInfo<Nagscreen>("nijigenerate.widgets.modal.nagscreen.Nagscreen");
    }

public:
    Nagscreen(compat::RetainedString title, compat::RetainedStringSlice message,
        std::int32_t timeout, float baseHeight = 384.0f);
    ~Nagscreen() override = default;
};
}
namespace nijigenerate::widgets::modal::nagscreen {
using ::nijigenerate::widgets::modal::Nagscreen;
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::modal::Nagscreen> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.modal.nagscreen.Nagscreen)";
    inline static constexpr char unqualifiedArgument[] = "(Nagscreen)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.modal.nagscreen.Nagscreen";
    inline static constexpr char unqualifiedElement[] = "Nagscreen";
};
template<> struct SourceObjectTypeNames<::nijigenerate::widgets::modal::Nagscreen> {
    inline static constexpr char className[] = "nijigenerate.widgets.modal.nagscreen.Nagscreen";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.widgets.modal.nagscreen.Nagscreen)";
};
}
