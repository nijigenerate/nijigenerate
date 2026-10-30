// Independent source/nijigenerate/windows/rename.d port. BSD-2-Clause.
#include "rename.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/nullable_string.hpp>
#include <array>
#include <limits>
#include <stdexcept>
#include <new>
#include <source_location>

namespace nijigenerate::windows::rename {
namespace {
template<class Operation> auto allocateString(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedStringSlice hiddenTerminator(std::string_view value) {
    auto result = allocateString([&] {
        auto bytes = std::string(value);
        bytes.push_back('\0');
        return compat::RetainedStringSlice{compat::RetainedString(std::move(bytes))};
    });
    return result.slice(0, result.size() - 1);
}
compat::RetainedString retainTitle(std::string value) {
    return allocateString([&] { return compat::RetainedString(std::move(value)); });
}
} // namespace
void RenameWindow::apply() {
    const auto receiver = weak_from_this().lock();
    if (sdkOutputDescriptor) {
        *sdkOutputDescriptor = text.empty() ? nicxlive::core::common::NullableString{}
            : allocateString([&] { return nicxlive::core::common::NullableString(std::string(text.view())); });
    } else if (stringOutput) *stringOutput = allocateString([&] { return std::string(text.view()); });
    else {
        if (!outputDescriptor) compat::ngFailNullAccess(true);
        *outputDescriptor = text.empty() ? compat::RetainedString{}
            : allocateString([&] { return compat::RetainedString(std::string(text.view())); });
    }
    close();
}
void RenameWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    ImGui::SetNextWindowSize(ImVec2(320, 120), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 120),
        ImVec2(std::numeric_limits<float>::max(), 120));
    base::Window::onBeginUpdate();
}
void RenameWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    using namespace widgets;
    using core::translation::ngTranslateRetained;
    using core::translation::ngTranslateCString;
    const float measured = ngMeasureString(compat::RetainedStringSlice{
        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Rename"))}).x;
    const float maximum = std::numeric_limits<float>::max();
    const float doneLength = measured < 64 ? 64 : (maximum < measured ? maximum : measured);
    auto available = ngAvailableSpace();
    ngDummy(ImVec2(0, available.y / 5));
    ImGui::Indent(16);
    available = ngAvailableSpace();
    if (ngInputText("RENAME", available.x - 16, text, ImGuiInputTextFlags_EnterReturnsTrue)) apply();
    ImGui::Unindent(16);
    available = ngAvailableSpace();
    ngDummy(ImVec2(0, -24));
    ngDummy(ImVec2(available.x - (doneLength + 8), 20));
    ImGui::SameLine(0, 0);
    // Preserve the source's second apply even if Enter already closed the window.
    if (ngButtonColored(ngTranslateCString("Rename").data(), ImVec2(doneLength + 8, 20))) apply();
}
void RenameWindow::initialize(compat::RetainedString& destination) {
    using namespace core::translation;
    const auto pattern = ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Rename %s..."));
    const std::array<TypedFormatArgument, 1> arguments{ngFormatArgument(destination)};
    base::Window::initialize(retainTitle(ngFormatTyped(pattern.view(), arguments)));
    outputDescriptor = &destination;
    text = hiddenTerminator(destination.view());
}
void RenameWindow::initialize(std::shared_ptr<compat::RetainedString> destination) {
    if (!destination) throw std::invalid_argument("RenameWindow requires a retained string descriptor");
    initialize(*destination);
    // Existing native callers may explicitly retain the descriptor's real owner.
    output = std::move(destination);
}
void RenameWindow::initialize(std::shared_ptr<std::string> destination) {
    if (!destination) throw std::invalid_argument("RenameWindow requires retained string storage");
    using namespace core::translation;
    const auto pattern=ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Rename %s..."));
    const std::array<TypedFormatArgument,1> arguments{ngFormatArgument(*destination)};
    base::Window::initialize(retainTitle(ngFormatTyped(pattern.view(),arguments)));
    stringOutput=std::move(destination);
    text = hiddenTerminator(*stringOutput);
}
void RenameWindow::initialize(nicxlive::core::common::NullableString& destination) {
    using namespace core::translation;
    const auto pattern = ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Rename %s..."));
    const std::array<TypedFormatArgument, 1> arguments{ngFormatArgument(destination)};
    base::Window::initialize(retainTitle(ngFormatTyped(pattern.view(), arguments)));
    sdkOutputDescriptor = &destination;
    // Restore callbacks may replace the descriptor; duplicate its current value.
    text = hiddenTerminator(destination.view());
}
void RenameWindow::initialize(std::shared_ptr<nicxlive::core::common::NullableString> destination) {
    if (!destination) throw std::invalid_argument("RenameWindow requires a retained SDK string descriptor");
    initialize(*destination);
    // Retain the real containing Parameter/Node owner through its aliasing handle.
    sdkOutput = std::move(destination);
}
}
