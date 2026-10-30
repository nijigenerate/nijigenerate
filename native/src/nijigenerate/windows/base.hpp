#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/windows/detail/source_class_info.hpp>
#include <compat/source_allocator.hpp>
#include <imgui.h>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <new>
#include <source_location>
#include <tuple>
#include <type_traits>

namespace nijigenerate::windows::base {

class Window;
using WindowReference = std::shared_ptr<Window>;

template<class T, class... Args> std::shared_ptr<T> ngCreateWindow(Args&&... args);
void incPushWindow(WindowReference window);
void incPushWindowList(WindowReference window);
void incPopWindowList(WindowReference window);
void incPopWindowListAll();
void incPopWindow();
void incUpdateWindows();
WindowReference incGetTopWindow();
void incPopWelcomeWindow();
bool incWelcomeWindowOnTop();
namespace detail {
void removeFirstStackWindow(bool (*matches)(const WindowReference&));
// Native call frame only: references to live producers, no Source object identity.
template<class... Producers> struct DeferredWindowArguments {
    std::tuple<Producers...> producers;
};
template<class T> inline constexpr bool isDeferredWindowArguments = false;
template<class... Producers>
inline constexpr bool isDeferredWindowArguments<DeferredWindowArguments<Producers...>> = true;
}
std::size_t incGetWindowStackSize();
std::size_t incGetWindowListSize();
std::size_t incGetWindowsOpen();

class Window : public std::enable_shared_from_this<Window> {
    template<class T, class... Args> friend std::shared_ptr<T> ngCreateWindow(Args&&...);
    friend void incPushWindow(WindowReference);
    friend void incPushWindowList(WindowReference);
    friend void incPopWindowList(WindowReference);
    friend void incPopWindowListAll();
    friend void incPopWindow();
    friend void incUpdateWindows();

    compat::RetainedString name_;
    bool visible = true;
    bool disabled = false;
    int spawnedId = 0;
    std::optional<compat::SourceCString> imNameStorage_;
    const char* imName = nullptr;

protected:
    bool onlyOne = false;
    bool drewWindow = false;
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;

    // A logical D constructor runs after the concrete C++ object is initialized,
    // so restore can dispatch virtually before the derived logical body.
    Window() = default;
    void initialize(compat::RetainedString name);
    virtual void onUpdate() = 0;
    virtual void onBeginUpdate();
    virtual void onEndUpdate();
    virtual void onClose();
    virtual void unclose();

public:
    virtual ~Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    virtual void close() final;
    virtual compat::RetainedString name() final;
    virtual void setTitle(compat::RetainedString title) final;
    virtual void update() final;
    virtual ImVec2 getPosition();
    virtual void disable();
    virtual void restore();
    virtual bool isVisible() final;
    // Window inherits the overridable Object equality hook in the D source.
    virtual bool opEquals(const WindowReference& other);
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(WindowReference other);
};

// Concrete translations expose their logical constructor as initialize and
// friend this factory; no virtual callbacks run in a C++ base constructor.
template<class T, class... Args> std::shared_ptr<T> ngCreateWindow(Args&&... args) {
    static_assert(std::derived_from<T, Window>);
    detail::ngRegisterSourceWindowClass<T>();
    auto window = std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{});
    if constexpr (sizeof...(Args) == 1 &&
            (detail::isDeferredWindowArguments<std::remove_cvref_t<Args>> && ...)) {
        auto&& frame = std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...));
        auto arguments = std::apply([](auto&&... producers) {
            // Each Source argument finishes before the next producer is invoked.
            return std::tuple<std::decay_t<std::invoke_result_t<decltype(producers)>>...>{
                std::invoke(std::forward<decltype(producers)>(producers))...};
        }, std::move(frame.producers));
        std::apply([&](auto&&... values) {
            window->initialize(std::forward<decltype(values)>(values)...);
        }, std::move(arguments));
    } else window->initialize(std::forward<Args>(args)...);
    return window;
}

template<class T, class... Producers>
std::shared_ptr<T> ngCreateWindowDeferred(Producers&&... producers) {
    // Keep producer references only; argument evaluation belongs after allocation.
    return ngCreateWindow<T>(detail::DeferredWindowArguments<Producers&&...>{
        std::forward_as_tuple(std::forward<Producers>(producers)...)});
}

} // namespace nijigenerate::windows::base
