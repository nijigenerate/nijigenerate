#include "base.hpp"
#include "welcome.hpp"

#include <nijigenerate/api/mcp/task.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/window_platform.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/detail/plain_array_remove.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/widgets/window.hpp>
#include <imgui_internal.h>
#include <bit>
#include <cstdint>
#include <array>
#include <string>
#include <typeinfo>
#include <new>
#include <source_location>
#include <stdexcept>

#if defined(NG_USE_UI_SCALING) && defined(NG_NO_UI_SCALING)
#error "Window requires mutually exclusive UI scaling versions"
#endif

namespace nijigenerate::windows::base {
namespace {
#if defined(NG_NO_UI_SCALING)
thread_local ImGuiWindowClass* windowClass = nullptr;
#endif
thread_local std::uint32_t spawnCount = 0;
thread_local compat::Slice<WindowReference> windowStack;
thread_local compat::Slice<WindowReference> windowList;

bool objectEquals(WindowReference left, WindowReference right) {
    if (left == right) return true;
    if (!left || !right) return false;
    if (!left->opEquals(right)) return false;
    return typeid(*left) == typeid(*right) || right->opEquals(left);
}

compat::Slice<WindowReference> removeAt(compat::Slice<WindowReference> windows, std::size_t index) {
    return compat::detail::ngRemovePlainArrayStable(windows, index);
}
} // namespace

// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString Window::toString() {
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

std::size_t Window::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}

int Window::opCmp(WindowReference) {
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(className.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

void Window::initialize(compat::RetainedString name) {
    name_ = std::move(name);
    restore();
}

void Window::onBeginUpdate() {
    if (!imName) setTitle(name());
    bool* open = api::mcp::task::ngMcpExternalCommandsBlocked() ? nullptr : &visible;
#if defined(NG_NO_UI_SCALING)
    ImGui::SetNextWindowClass(windowClass);
    drewWindow = widgets::ngBegin(imName, open,
        core::ngIsWayland() ? flags : flags | ImGuiWindowFlags_NoDecoration);
#elif defined(NG_USE_UI_SCALING)
    drewWindow = widgets::ngBegin(imName, open, flags);
#else
    (void)open;
#endif
}

void Window::onEndUpdate() { widgets::ngEnd(); }
void Window::onClose() {}
void Window::unclose() { visible = true; }

void Window::close() {
    if (api::mcp::task::ngMcpExternalCommandsBlocked()) return;
    visible = false;
}

compat::RetainedString Window::name() { return name_; }

void Window::setTitle(compat::RetainedString title) {
    name_ = std::move(title);
    const std::array<core::translation::TypedFormatArgument,2> arguments{
        core::translation::ngFormatArgument(name_),core::translation::ngFormatArgument(spawnedId)};
    const auto decorated = core::translation::ngFormatTyped("%s###%s",arguments);
    compat::SourceCString next{std::string_view(decorated)};
    compat::detail::ngSourceCStringPostconditions(decorated, next.data());
    imNameStorage_=std::move(next);
    imName = imNameStorage_->data();
}

void Window::update() {
    auto owner = weak_from_this().lock();
    if (!disabled && !visible) return;
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, disabled);
    onBeginUpdate();
    ImGui::BeginDisabled(api::mcp::task::ngMcpExternalCommandsBlocked());
    onUpdate();
    ImGui::EndDisabled();
    onEndUpdate();
    ImGui::PopItemFlag();
    if (disabled && !visible) visible = true;
}

ImVec2 Window::getPosition() { return ImGui::GetWindowPos(); }

void Window::disable() {
    flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
    disabled = true;
}

void Window::restore() {
    disabled = false;
    flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
#if defined(NG_NO_UI_SCALING)
    // The D module replaces its TLS pointer on every restore without destroying it.
    windowClass = IM_NEW(ImGuiWindowClass)();
    windowClass->ViewportFlagsOverrideClear = ImGuiViewportFlags_NoDecoration | ImGuiViewportFlags_NoTaskBarIcon;
    windowClass->ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
#endif
}

bool Window::isVisible() { return visible; }
bool Window::opEquals(const WindowReference& other) { return this == other.get(); }

void incPushWindow(WindowReference window) {
    static_assert(sizeof(int) == sizeof(std::int32_t));
    const auto id=std::bit_cast<std::int32_t>(spawnCount++);
    if(!window) compat::ngFailNullAccess(true);
    window->spawnedId = id;
    if (window->onlyOne) {
        const auto windows = windowStack;
        for (auto existing : windows) {
            const auto existingName = compat::ngRequireObject(existing)->name();
            const auto incomingName = compat::ngRequireObject(window)->name();
            if (existingName == incomingName) return;
        }
    }
    if (!windowStack.empty()) compat::ngRequireObject(windowStack[windowStack.size() - 1])->disable();
    windowStack.append(std::move(window));
}

void incPushWindowList(WindowReference window) {
    const auto id=std::bit_cast<std::int32_t>(spawnCount++);
    if(!window) compat::ngFailNullAccess(true);
    window->spawnedId = id;
    if (window->onlyOne) {
        const auto windows = windowList;
        for (auto existing : windows) {
            const auto existingName = compat::ngRequireObject(existing)->name();
            const auto incomingName = compat::ngRequireObject(window)->name();
            if (existingName == incomingName) return;
        }
    }
    windowList.append(std::move(window));
}

void incPopWindowList(WindowReference window) {
    const auto searchList = windowList;
    for (std::size_t i = 0; i < searchList.size(); ++i) {
        if (!objectEquals(searchList[i], window)) continue;
        compat::ngRequireObject(window)->onClose();
        if (windowList.size() == 1) windowList.resize(0);
        else windowList = removeAt(windowList, i);
        return;
    }
}

void incPopWindowListAll() {
    auto iteration = windowList;
    for (auto window : iteration) {
        compat::ngRequireObject(window)->onClose();
        if(!window) compat::ngFailNullAccess(true);
        window->visible = false;
    }
    windowList.resize(0);
}

void incPopWindow() {
    if (windowStack.empty()) return;
    auto top = windowStack[windowStack.size() - 1];
    compat::ngRequireObject(top)->onClose();
    windowStack.resize(windowStack.size() - 1);
    if (!windowStack.empty()) compat::ngRequireObject(windowStack[windowStack.size() - 1])->restore();
}

void detail::removeFirstStackWindow(bool (*matches)(const WindowReference&)) {
    for (std::size_t i = 0; i < windowStack.size(); ++i) {
        if (matches(windowStack[i])) {
            windowStack = removeAt(windowStack, i);
            return;
        }
    }
}

void incUpdateWindows() {
    auto stackIteration = windowStack;
    for (auto window : stackIteration) {
        compat::ngRequireObject(window)->update();
        if (!window->visible) incPopWindow();
    }
    compat::Slice<WindowReference> closedWindows;
    auto listIteration = windowList;
    for (auto window : listIteration) {
        compat::ngRequireObject(window)->update();
        if (!window->visible) closedWindows.append(window);
    }
    for (auto window : closedWindows) incPopWindowList(std::move(window));
}

WindowReference incGetTopWindow() {
    return windowStack.empty() ? nullptr : windowStack[windowStack.size() - 1];
}

void incPopWelcomeWindow() {
    detail::removeFirstStackWindow([](const WindowReference& window) {
        return std::dynamic_pointer_cast<welcome::WelcomeWindow>(window) != nullptr;
    });
}

bool incWelcomeWindowOnTop() {
    return std::dynamic_pointer_cast<welcome::WelcomeWindow>(incGetTopWindow()) != nullptr;
}

std::size_t incGetWindowStackSize() { return windowStack.size(); }
std::size_t incGetWindowListSize() { return windowList.size(); }
std::size_t incGetWindowsOpen() { return windowStack.size() + windowList.size(); }

} // namespace nijigenerate::windows::base
