#pragma once
#include <nijigenerate/windows/base.hpp>
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <array>
#include <limits>
#include <string_view>
#include <utility>
#include <optional>
namespace nijigenerate::windows::settings {
class SettingsPane {
    compat::RetainedString value_;
public:
    SettingsPane();
    explicit SettingsPane(compat::RetainedString value) : value_(std::move(value)) {}
    SettingsPane(const SettingsPane&) = default;
    SettingsPane& operator=(const SettingsPane&) = default;
    SettingsPane(SettingsPane&& other) noexcept : value_(other.value_) {}
    SettingsPane& operator=(SettingsPane&& other) noexcept { value_ = other.value_; return *this; }
    std::string_view view() const noexcept { return value_.view(); }
    compat::RetainedString retainedString() const { return value_; }
    operator compat::RetainedString() const { return value_; }
    friend bool operator==(const SettingsPane& left, const SettingsPane& right) noexcept {
        return left.value_ == right.value_;
    }
    friend bool operator==(const SettingsPane& left, const compat::RetainedString& right) noexcept {
        return left.value_ == right;
    }
    friend bool operator==(const compat::RetainedString& left, const SettingsPane& right) noexcept {
        return left == right.value_;
    }
    static const SettingsPane LookAndFeel, Viewport, Accessibility, Rendering,
        FileHandling, Shortcuts, Integration;
};
inline const SettingsPane SettingsPane::LookAndFeel{compat::RetainedString::fromStaticLiteral("Look and Feel")};
inline const SettingsPane SettingsPane::Viewport{compat::RetainedString::fromStaticLiteral("Viewport")};
inline const SettingsPane SettingsPane::Accessibility{compat::RetainedString::fromStaticLiteral("Accessbility")};
inline const SettingsPane SettingsPane::Rendering{compat::RetainedString::fromStaticLiteral("Rendering")};
inline const SettingsPane SettingsPane::FileHandling{compat::RetainedString::fromStaticLiteral("File Handling")};
inline const SettingsPane SettingsPane::Shortcuts{compat::RetainedString::fromStaticLiteral("Shortcuts")};
inline const SettingsPane SettingsPane::Integration{compat::RetainedString::fromStaticLiteral("Integration")};
inline SettingsPane::SettingsPane() : value_(LookAndFeel.value_) {}
extern thread_local bool incIsSettingsOpen;
bool ngIsSettingsOpen();
class SettingsWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    bool generalTabOpen = true, otherTabOpen = true, changesRequiresRestart = false;
    int tmpUIScale = 0;
    float targetUIScale = std::numeric_limits<float>::quiet_NaN();
    SettingsPane settingsPane = SettingsPane::LookAndFeel;
    bool capturingShortcut = false;
    std::shared_ptr<commands::Command> capturingCommand;
    bool capturingRepeat = false;
    compat::RetainedString capturingPreview;
    std::array<char, 256> shortcutFilterBuffer{};
    compat::RetainedString shortcutFilter = compat::RetainedString::emptyLiteral();
    void initialize();
    void beginSection(const char* title);
    void endSection();
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
    void onClose() override;
    template<class Table> bool hasMatchingCommands(Table& table);
    template<class Table> void renderCommandTable(Table& table, const char* title);
    virtual void captureShortcutUI();
    virtual void drawShortcutsPane();
public:
    SettingsWindow() = default;
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::windows::settings::SettingsPane> {
    using Value = ::nijigenerate::windows::settings::SettingsPane;
    struct Member { Value value; std::string_view name; std::string_view stringValue; };
    inline static constexpr std::string_view name = "SettingsPane";
    inline static const std::array members{
        Member{Value::LookAndFeel, "LookAndFeel", "Look and Feel"},
        Member{Value::Viewport, "Viewport", "Viewport"},
        Member{Value::Accessibility, "Accessibility", "Accessbility"},
        Member{Value::Rendering, "Rendering", "Rendering"},
        Member{Value::FileHandling, "FileHandling", "File Handling"},
        Member{Value::Shortcuts, "Shortcuts", "Shortcuts"},
        Member{Value::Integration, "Integration", "Integration"}};
};
template<> struct FormatStringEnumTraits<::nijigenerate::windows::settings::SettingsPane> {
    using Value = ::nijigenerate::windows::settings::SettingsPane;
    static compat::RetainedString raw(const Value& value) { return value.retainedString(); }
    static std::string_view typeName() { return "SettingsPane"; }
    static std::optional<std::string_view> memberName(const Value& value) {
        for (const auto& member : FormatEnumTraits<Value>::members)
            if (value.view() == member.stringValue) return member.name;
        return std::nullopt;
    }
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::settings::SettingsPane> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.settings.SettingsPane)";
    inline static constexpr char unqualifiedArgument[] = "(SettingsPane)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.settings.SettingsPane";
    inline static constexpr char unqualifiedElement[] = "SettingsPane";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::detail {
template<class T> requires (std::same_as<std::remove_cv_t<T>, ::nijigenerate::windows::settings::SettingsPane> &&
    !std::is_volatile_v<T>)
struct CommandResultInitialValue<T> {
    static T get() { return ::nijigenerate::windows::settings::SettingsPane::LookAndFeel; }
};
} // namespace nijigenerate::commands::detail

#include "settings_templates.hpp"
