#pragma once
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>
#include <memory>

namespace nicxlive::core::param { class ParameterBinding; }
namespace nicxlive::core::nodes { class Node; }
namespace nijigenerate::commands { class Command; class Context; }

namespace nijigenerate::core::shortcut {
struct ActionEntry {
    compat::RetainedString shortcut;
    std::shared_ptr<commands::Command> command;
    bool repeat = false;
};

} // namespace nijigenerate::core::shortcut

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::shortcut::ActionEntry> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.shortcut.base.ActionEntry)";
    inline static constexpr char unqualifiedArgument[] = "(ActionEntry)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.shortcut.base.ActionEntry";
    inline static constexpr char unqualifiedElement[] = "ActionEntry";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::shortcut {
void ngRegisterShortcut(compat::RetainedString shortcut, std::shared_ptr<commands::Command> command,
                        bool repeat = false);
void ngClearShortcutFor(std::shared_ptr<commands::Command> command);
compat::Slice<ActionEntry> ngListShortcuts();
compat::RetainedString ngShortcutFor(std::shared_ptr<commands::Command> command);
std::shared_ptr<commands::Context> ngBuildExecutionContext();
void incHandleShortcuts();
void ngSaveShortcutsToSettings();
void ngLoadShortcutsFromSettings();

using ParamPointProvider = compat::Vec2u (*)();
using SelectedBindings = compat::Slice<std::shared_ptr<::nicxlive::core::param::ParameterBinding>>;
using SelectedNodes = compat::Slice<std::shared_ptr<::nicxlive::core::nodes::Node>>;
using SelectedBindingsProvider = SelectedBindings (*)();
using SelectedNodesProvider = SelectedNodes (*)();
namespace detail {
inline bool shortcutCaptureActive = false;
inline thread_local ParamPointProvider paramPointProvider = nullptr;
inline thread_local SelectedBindingsProvider selectedBindingsProvider = nullptr;
inline SelectedNodesProvider selectedNodesProvider = nullptr;
}
inline void ngSetShortcutCapture(bool capturing) {
    detail::shortcutCaptureActive = capturing;
}
inline bool ngIsShortcutCapture() {
    return detail::shortcutCaptureActive;
}
inline void ngSetParamPointProvider(ParamPointProvider provider) {
    detail::paramPointProvider = provider;
}
inline void ngSetSelectedBindingsProvider(SelectedBindingsProvider provider) {
    detail::selectedBindingsProvider = provider;
}
inline void ngSetSelectedNodesProvider(SelectedNodesProvider provider) {
    detail::selectedNodesProvider = provider;
}
} // namespace nijigenerate::core::shortcut
