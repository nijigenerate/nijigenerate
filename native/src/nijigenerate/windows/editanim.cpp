#include "editanim.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/unicode_strip.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <limits>
#include <cmath>
#include <array>
#include <utility>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::windows::editanim {
using compat::RetainedString;
using core::translation::ngTranslateRetained;
using core::translation::ngTranslateCString;
namespace {
template<class Operation> auto allocateString(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
RetainedString retainFormatted(std::string value) {
    return allocateString([&] { return RetainedString(std::move(value)); });
}
template<std::size_t N> RetainedString text(const char (&literal)[N]) {
    return ngTranslateRetained(RetainedString::fromStaticLiteral(literal));
}
template<class... Args> std::string format(std::string_view text, const Args&... args) {
    using namespace core::translation;
    const std::array<TypedFormatArgument, sizeof...(Args)> values{ngFormatArgument(args)...};
    return ngFormatTyped(text, values);
}
void error(const char* tag, core::translation::TranslatedCString title, RetainedString body) {
    using namespace widgets::dialog;
    ngDialog(DialogTag::borrowed(tag), DialogTag::retained(title.data(), title.owner),
        DialogText(std::move(body)));
}
template<class T> const T& sourceIndex(const compat::Slice<T>& values, std::size_t index,
                                    std::size_t line) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            RetainedString::fromStaticLiteral("source/nijigenerate/windows/editanim.d"), line);
    return values[index];
}
}
void EditAnimationWindow::apply() {
    const auto receiver = weak_from_this().lock();
    if (compat::ngStripUnicode(name.view()).empty()) {
        const auto title = ngTranslateCString("Invalid Name");
        auto body = text("Name can not be empty!");
        error("ERR_NAME_INVALID", title, std::move(body)); return;
    }
    const auto keys = compat::ngRequireObject(project::incActivePuppet())->getAnimations().keys();
    for (auto key : keys) {
        if ((isNew && key.view() == name.view()) ||
            (!isNew && originalName.view() != name.view() && key.view() == name.view())) {
            const auto title = ngTranslateCString("Invalid Name");
            const auto pattern = text("%s is already taken! Please chose another animation name.");
            error("ERR_NAME_TAKEN", title, retainFormatted(format(pattern.view(), name)));
            return;
        }
    }
    if (frameRateOption + 1 == fpsOptions.size())
        newAnim.timestep = static_cast<float>(1.0 / framerate);
    else newAnim.timestep = static_cast<float>(1.0 / sourceIndex(fpsOptionsFR, frameRateOption, 56));
    project::ngAnimationCreateOrUpdate(name, newAnim,
        isNew ? RetainedString::emptyLiteral() : originalName);
    close();
}
void EditAnimationWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    ImGui::SetNextWindowSize(ImVec2(480,320), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(480,320), ImVec2(std::numeric_limits<float>::max(),320));
    base::Window::onBeginUpdate();
}
void EditAnimationWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    using namespace widgets;
    auto available = ngAvailableSpace();
    ImGui::Indent(16);
    available = ngAvailableSpace();
    if (ngInputText("NAME", available.x - 16, name, ImGuiInputTextFlags_EnterReturnsTrue)) apply();
    ImGui::Unindent(16);
    ngBeginCategory(ngTranslateCString("Options").data(), IncCategoryFlags::NoCollapse);
    ngCheckbox(ngTranslateCString("Additive").data(), &newAnim.additive);
    ImGui::InputFloat(ngTranslateCString("Weight").data(), &newAnim.animationWeight);
    if (ImGui::DragInt(ngTranslateCString("Frames").data(), &newAnim.length, 1, 1, std::numeric_limits<int>::max()))
        newAnim.leadOut = newAnim.length;
    ImGui::DragInt(ngTranslateCString("Lead In").data(), &newAnim.leadIn, 1, 0, newAnim.length);
    ImGui::DragInt(ngTranslateCString("Lead Out").data(), &newAnim.leadOut, 1, 0, newAnim.length);
    const auto comboTitle = ngTranslateCString("Framerate");
    const auto preview = sourceIndex(fpsOptions, frameRateOption, 97);
    if (ImGui::BeginCombo(comboTitle.data(), preview.data())) {
        const auto count = fpsOptions.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto option = sourceIndex(fpsOptions, i, 99);
            if (ImGui::Selectable(option.data(), i == frameRateOption)) frameRateOption = i;
        }
        ImGui::EndCombo();
    }
    float timestep = static_cast<float>(1.0 / sourceIndex(fpsOptionsFR, frameRateOption, 107));
    if (frameRateOption + 1 == fpsOptions.size()) {
        ImGui::InputFloat(ngTranslateCString("Custom Framerate").data(), &framerate);
        timestep = static_cast<float>(1.0 / framerate);
    }
    const float time = timestep * newAnim.length;
    const float seconds = static_cast<float>(compat::ngFloatToInt32(time));
    const float milliseconds = static_cast<float>(compat::ngFloatToInt32((time - seconds) * 1000));
    ngText(compat::RetainedStringSlice(retainFormatted(
        format(text("%ss %sms").view(), seconds, milliseconds))));
    ngEndCategory();
    auto button = isNew ? text("Create") : text("Save");
    auto measured = ngMeasureString(compat::RetainedStringSlice(button)).x;
    const float maximum = std::numeric_limits<float>::max();
    const float length = measured < 64 ? 64 : (maximum < measured ? maximum : measured);
    available = ngAvailableSpace();
    ngDummy(ImVec2(0,-24)); ngDummy(ImVec2(available.x - (length + 8),20));
    ImGui::SameLine(0,0);
    const compat::SourceCString buttonCString{button};
    compat::detail::ngSourceCStringPostconditions(button.view(), buttonCString.data());
    if (ngButtonColored(buttonCString.data(), ImVec2(length + 8,20))) apply();
}
void EditAnimationWindow::setupFPS() {
    using Option = core::translation::TranslatedCString;
    compat::Slice<Option> options{Option{}, Option{}, Option{}, Option{}, Option{}};
    options[0] = ngTranslateCString("120 FPS");
    options[1] = ngTranslateCString("60 FPS");
    options[2] = ngTranslateCString("30 FPS");
    options[3] = ngTranslateCString("25 FPS");
    options[4] = ngTranslateCString("Custom Framrate");
    fpsOptions = std::move(options);
    fpsOptionsFR = {120,60,30,25,60};
}
void EditAnimationWindow::initialize() {
    base::Window::initialize(text("New Animation..."));
    isNew = true; newAnim.animationWeight = 1; newAnim.additive = false;
    newAnim.leadIn = 0; newAnim.leadOut = 0; newAnim.length = 100; setupFPS();
}
void EditAnimationWindow::initialize(nicxlive::core::Animation animation, RetainedString input) {
    base::Window::initialize(retainFormatted(format(text("Edit %s...").view(), input)));
    isNew = false;
    originalName = input.empty() ? RetainedString{}
        : allocateString([&] { return RetainedString(input.view()); });
    name = allocateString([&] {
        auto terminated = std::string(input.view());
        terminated.push_back('\0');
        return RetainedString(std::move(terminated)).subslice(0, input.size());
    });
    newAnim = animation; setupFPS();
}
}
