#pragma once

#include "nijigenerate/commands/command_scope_transition.hpp"
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>

#include <concepts>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace nijigenerate::commands {

void ngRunAutoRigEditorAction(compat::SharedDelegate<void()> action);

enum class CommandJsonSchema : std::int32_t {
    unspecified = 0,
    overlayObjects = 1,
    depthOperation = 2,
    depthOperations = 3,
};
enum class CommandShortcutPolicy : std::int32_t {
    visible = 0,
    hidden = 1,
};
enum class CommandMcpExposure : std::int32_t {
    tool = 0,
    hidden = 1,
};
enum class CommandGuiDisplay : std::int32_t {
    none = 0,
    window = 1,
    dialog = 2,
    modal = 3,
    popup = 4,
    confirm = 5,
    dialogOutput = 6,
    dialogWindow = 7,
    layout = 8,
};
enum class CommandIrreversibleEffect : std::int32_t {
    none = 0,
    fileWrite = 1,
    importData = 2,
    projectReset = 3,
    configEdit = 4,
    apply = 5,
    keyframeEdit = 6,
    bindingEdit = 7,
    structuralEdit = 8,
    create = 9,
    deleteObject = 10,
    rename = 11,
    textureRegenerate = 12,
    repair = 13,
    layoutReset = 14,
};
struct ShortcutVisibility { CommandShortcutPolicy value{}; };
struct McpExposureMeta { CommandMcpExposure value{}; };
struct GuiDisplayMeta { CommandGuiDisplay value{}; };
struct IrreversibleEffectMeta { CommandIrreversibleEffect value{}; };
struct PresentationOnly {};
template<class... Scopes> struct CommandScopes {};

inline constexpr ShortcutVisibility ShortcutVisible{CommandShortcutPolicy::visible};
inline constexpr ShortcutVisibility ShortcutHidden{CommandShortcutPolicy::hidden};
inline constexpr McpExposureMeta McpTool{CommandMcpExposure::tool};
inline constexpr McpExposureMeta McpHidden{CommandMcpExposure::hidden};
inline constexpr GuiDisplayMeta GuiWindow{CommandGuiDisplay::window};
inline constexpr GuiDisplayMeta GuiDialog{CommandGuiDisplay::dialog};
inline constexpr GuiDisplayMeta GuiModal{CommandGuiDisplay::modal};
inline constexpr GuiDisplayMeta GuiPopup{CommandGuiDisplay::popup};
inline constexpr GuiDisplayMeta GuiConfirm{CommandGuiDisplay::confirm};
inline constexpr GuiDisplayMeta GuiDialogOutput{CommandGuiDisplay::dialogOutput};
inline constexpr GuiDisplayMeta GuiDialogWindow{CommandGuiDisplay::dialogWindow};
inline constexpr GuiDisplayMeta GuiLayout{CommandGuiDisplay::layout};
inline constexpr IrreversibleEffectMeta EffectFileWrite{CommandIrreversibleEffect::fileWrite};
inline constexpr IrreversibleEffectMeta EffectImport{CommandIrreversibleEffect::importData};
inline constexpr IrreversibleEffectMeta EffectProjectReset{CommandIrreversibleEffect::projectReset};
inline constexpr IrreversibleEffectMeta EffectConfigEdit{CommandIrreversibleEffect::configEdit};
inline constexpr IrreversibleEffectMeta EffectApply{CommandIrreversibleEffect::apply};
inline constexpr IrreversibleEffectMeta EffectKeyframeEdit{CommandIrreversibleEffect::keyframeEdit};
inline constexpr IrreversibleEffectMeta EffectBindingEdit{CommandIrreversibleEffect::bindingEdit};
inline constexpr IrreversibleEffectMeta EffectStructuralEdit{CommandIrreversibleEffect::structuralEdit};
inline constexpr IrreversibleEffectMeta EffectCreate{CommandIrreversibleEffect::create};
inline constexpr IrreversibleEffectMeta EffectDelete{CommandIrreversibleEffect::deleteObject};
inline constexpr IrreversibleEffectMeta EffectRename{CommandIrreversibleEffect::rename};
inline constexpr IrreversibleEffectMeta EffectTextureRegenerate{CommandIrreversibleEffect::textureRegenerate};
inline constexpr IrreversibleEffectMeta EffectRepair{CommandIrreversibleEffect::repair};
inline constexpr IrreversibleEffectMeta EffectLayoutReset{CommandIrreversibleEffect::layoutReset};


struct CommandMetadata;

// Explicit specializations supply the original fully qualified D type name.
template<class T> struct CommandScopeTypeName;

class CommandScope : public std::enable_shared_from_this<CommandScope> {
public:
    virtual ~CommandScope() = 0;
    virtual compat::RetainedString id() const { return typeName(); }
    virtual compat::RetainedString description() const { return compat::RetainedString::emptyLiteral(); }
    virtual compat::Slice<CommandScopeTransition> completionCommands() const { return {}; }
    virtual compat::Slice<CommandScopeTransition> cancellationCommands() const { return {}; }
    virtual compat::RetainedString typeName() const;
    virtual bool accepts(const CommandScope* activeScope) const { return this == activeScope; }
    virtual bool permits(const CommandMetadata& metadata) const;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<CommandScope> other);
    virtual int opCmp(std::shared_ptr<CommandScope> other);
    compat::RetainedString sourceClassInfoName() const;
protected:
    virtual void prepareSourceClassInfo() const;
};

class NormalCommandScope final : public CommandScope {
public:
    compat::RetainedString id() const override { return compat::RetainedString::fromStaticLiteral("normal"); }
    compat::RetainedString description() const override { return compat::RetainedString::fromStaticLiteral("Normal editor command scope"); }
    compat::RetainedString typeName() const override;
protected:
    void prepareSourceClassInfo() const override;
};
class GlobalCommandScope final : public CommandScope {
public:
    compat::RetainedString id() const override { return compat::RetainedString::fromStaticLiteral("global"); }
    compat::RetainedString description() const override { return compat::RetainedString::fromStaticLiteral("Commands available in every active scope"); }
    compat::RetainedString typeName() const override;
    bool accepts(const CommandScope* activeScope) const override { return activeScope != nullptr; }
protected:
    void prepareSourceClassInfo() const override;
};
template<> struct CommandScopeTypeName<NormalCommandScope> {
    static constexpr std::string_view value = "nijigenerate.commands.base.NormalCommandScope";
};
template<> struct CommandScopeTypeName<GlobalCommandScope> {
    static constexpr std::string_view value = "nijigenerate.commands.base.GlobalCommandScope";
};

// One canonical AA handle retains source key descriptors and stored scope handles.
using CommandScopeMap = core::selector::SelectorMap<compat::RetainedStringSlice,
    std::shared_ptr<CommandScope>, core::selector::SelectorStringHash, core::selector::SelectorStringEqual>;
struct CommandMetadata {
    bool shortcutRunnable = true;
    bool mcpExposed = true;
    CommandGuiDisplay guiDisplay = CommandGuiDisplay::none;
    CommandIrreversibleEffect irreversibleEffect = CommandIrreversibleEffect::none;
    CommandScopeMap scopes;
    bool presentationOnly = false;
};

namespace detail {
CommandScopeMap& registeredCommandScopes();
}
template<std::derived_from<CommandScope> S> std::shared_ptr<S> ngCommandScope() {
    const auto key = compat::RetainedString::fromStaticView(CommandScopeTypeName<S>::value);
    try {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(S), key);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    auto& registered = detail::registeredCommandScopes();
    if (const auto found = registered.find(key); found != registered.end()) {
        return std::dynamic_pointer_cast<S>(found->second);
    }
    auto scope = std::allocate_shared<S>(nicxlive::core::d_runtime::SourceAllocator<S>{});
    registered[key] = scope;
    return scope;
}

class CommandScopeRegistration final : public std::enable_shared_from_this<CommandScopeRegistration> {
public:
    explicit CommandScopeRegistration(std::shared_ptr<CommandScope> registeredScope);
    virtual ~CommandScopeRegistration();
    CommandScopeRegistration(const CommandScopeRegistration&) = delete;
    CommandScopeRegistration& operator=(const CommandScopeRegistration&) = delete;
    void close();
    bool isActive() const { return active_; }
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<CommandScopeRegistration> other);
    virtual int opCmp(std::shared_ptr<CommandScopeRegistration> other);
    compat::RetainedString sourceClassInfoName() const;
private:
    std::shared_ptr<CommandScope> registeredScope_;
    bool active_ = false;
};
std::shared_ptr<CommandScopeRegistration> ngPushCommandScope(std::shared_ptr<CommandScope> commandScope);
std::shared_ptr<CommandScope> ngCurrentCommandScope();
compat::Slice<std::shared_ptr<CommandScope>> ngCommandScopeStackSnapshot();

} // namespace nijigenerate::commands

namespace nicxlive::core::d_runtime {
// These are the defining declarations in nijigenerate.commands.base.
#define NG_COMMAND_SCOPE_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.base." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.base." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.base." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.base." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_COMMAND_SCOPE_SOURCE_NAMES(CommandScope)
NG_COMMAND_SCOPE_SOURCE_NAMES(NormalCommandScope)
NG_COMMAND_SCOPE_SOURCE_NAMES(GlobalCommandScope)
NG_COMMAND_SCOPE_SOURCE_NAMES(CommandScopeRegistration)
#undef NG_COMMAND_SCOPE_SOURCE_NAMES

#define NG_COMMAND_VALUE_SOURCE_NAMES(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.base." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.base." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_COMMAND_VALUE_SOURCE_NAMES(ShortcutVisibility)
NG_COMMAND_VALUE_SOURCE_NAMES(McpExposureMeta)
NG_COMMAND_VALUE_SOURCE_NAMES(GuiDisplayMeta)
NG_COMMAND_VALUE_SOURCE_NAMES(IrreversibleEffectMeta)
NG_COMMAND_VALUE_SOURCE_NAMES(PresentationOnly)
NG_COMMAND_VALUE_SOURCE_NAMES(CommandScopeTransition)
NG_COMMAND_VALUE_SOURCE_NAMES(CommandMetadata)
#undef NG_COMMAND_VALUE_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

#include <nijigenerate/core/typed_format.hpp>

namespace nijigenerate::core::translation {

template<> struct FormatEnumTraits<commands::CommandJsonSchema> {
    inline static constexpr std::string_view name = "CommandJsonSchema";
    inline static constexpr std::array members{
        FormatEnumMember<commands::CommandJsonSchema>{commands::CommandJsonSchema::unspecified, "unspecified"},
        FormatEnumMember<commands::CommandJsonSchema>{commands::CommandJsonSchema::overlayObjects, "overlayObjects"},
        FormatEnumMember<commands::CommandJsonSchema>{commands::CommandJsonSchema::depthOperation, "depthOperation"},
        FormatEnumMember<commands::CommandJsonSchema>{commands::CommandJsonSchema::depthOperations, "depthOperations"}
    };
};
template<> struct FormatEnumTraits<commands::CommandShortcutPolicy> {
    inline static constexpr std::string_view name = "CommandShortcutPolicy";
    inline static constexpr std::array members{
        FormatEnumMember<commands::CommandShortcutPolicy>{commands::CommandShortcutPolicy::visible, "visible"},
        FormatEnumMember<commands::CommandShortcutPolicy>{commands::CommandShortcutPolicy::hidden, "hidden"}
    };
};
template<> struct FormatEnumTraits<commands::CommandMcpExposure> {
    inline static constexpr std::string_view name = "CommandMcpExposure";
    inline static constexpr std::array members{
        FormatEnumMember<commands::CommandMcpExposure>{commands::CommandMcpExposure::tool, "tool"},
        FormatEnumMember<commands::CommandMcpExposure>{commands::CommandMcpExposure::hidden, "hidden"}
    };
};
template<> struct FormatEnumTraits<commands::CommandGuiDisplay> {
    inline static constexpr std::string_view name = "CommandGuiDisplay";
    inline static constexpr std::array members{
        FormatEnumMember<commands::CommandGuiDisplay>{commands::CommandGuiDisplay::none, "none"},
        FormatEnumMember<commands::CommandGuiDisplay>{commands::CommandGuiDisplay::window, "window"},
        FormatEnumMember<commands::CommandGuiDisplay>{commands::CommandGuiDisplay::dialog, "dialog"},
        FormatEnumMember<commands::CommandGuiDisplay>{commands::CommandGuiDisplay::modal, "modal"},
        FormatEnumMember<commands::CommandGuiDisplay>{commands::CommandGuiDisplay::popup, "popup"},
        FormatEnumMember<commands::CommandGuiDisplay>{commands::CommandGuiDisplay::confirm, "confirm"},
        FormatEnumMember<commands::CommandGuiDisplay>{commands::CommandGuiDisplay::dialogOutput, "dialogOutput"},
        FormatEnumMember<commands::CommandGuiDisplay>{commands::CommandGuiDisplay::dialogWindow, "dialogWindow"},
        FormatEnumMember<commands::CommandGuiDisplay>{commands::CommandGuiDisplay::layout, "layout"}
    };
};
template<> struct FormatEnumTraits<commands::CommandIrreversibleEffect> {
    inline static constexpr std::string_view name = "CommandIrreversibleEffect";
    inline static constexpr std::array members{
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::none, "none"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::fileWrite, "fileWrite"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::importData, "importData"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::projectReset, "projectReset"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::configEdit, "configEdit"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::apply, "apply"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::keyframeEdit, "keyframeEdit"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::bindingEdit, "bindingEdit"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::structuralEdit, "structuralEdit"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::create, "create"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::deleteObject, "deleteObject"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::rename, "rename"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::textureRegenerate, "textureRegenerate"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::repair, "repair"},
        FormatEnumMember<commands::CommandIrreversibleEffect>{commands::CommandIrreversibleEffect::layoutReset, "layoutReset"}
    };
};

template<> struct FormatAggregateTraits<commands::ShortcutVisibility> {
    using Value = commands::ShortcutVisibility;
    inline static constexpr std::string_view typeName = "ShortcutVisibility";
    static constexpr auto fields() {
        return std::tuple{ngFormatAggregateMember("value", offsetof(Value, value), &Value::value)};
    }
};

template<> struct FormatAggregateTraits<commands::McpExposureMeta> {
    using Value = commands::McpExposureMeta;
    inline static constexpr std::string_view typeName = "McpExposureMeta";
    static constexpr auto fields() {
        return std::tuple{ngFormatAggregateMember("value", offsetof(Value, value), &Value::value)};
    }
};

template<> struct FormatAggregateTraits<commands::GuiDisplayMeta> {
    using Value = commands::GuiDisplayMeta;
    inline static constexpr std::string_view typeName = "GuiDisplayMeta";
    static constexpr auto fields() {
        return std::tuple{ngFormatAggregateMember("value", offsetof(Value, value), &Value::value)};
    }
};

template<> struct FormatAggregateTraits<commands::IrreversibleEffectMeta> {
    using Value = commands::IrreversibleEffectMeta;
    inline static constexpr std::string_view typeName = "IrreversibleEffectMeta";
    static constexpr auto fields() {
        return std::tuple{ngFormatAggregateMember("value", offsetof(Value, value), &Value::value)};
    }
};

template<> struct FormatAggregateTraits<commands::PresentationOnly> {
    inline static constexpr std::string_view typeName = "PresentationOnly";
    static constexpr auto fields() { return std::tuple{}; }
};

template<> struct FormatAggregateTraits<commands::CommandScopeTransition> {
    using Value = commands::CommandScopeTransition;
    inline static constexpr std::string_view typeName = "CommandScopeTransition";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("commandId", offsetof(Value, commandId), &Value::commandId),
            ngFormatAggregateMember("description", offsetof(Value, description), &Value::description)};
    }
};

template<> struct FormatAggregateTraits<commands::CommandMetadata> {
    using Value = commands::CommandMetadata;
    inline static constexpr std::string_view typeName = "CommandMetadata";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("shortcutRunnable", offsetof(Value, shortcutRunnable), &Value::shortcutRunnable),
            ngFormatAggregateMember("mcpExposed", offsetof(Value, mcpExposed), &Value::mcpExposed),
            ngFormatAggregateMember("guiDisplay", offsetof(Value, guiDisplay), &Value::guiDisplay),
            ngFormatAggregateMember("irreversibleEffect", offsetof(Value, irreversibleEffect), &Value::irreversibleEffect),
            ngFormatAggregateMember("scopes", offsetof(Value, scopes), &Value::scopes),
            ngFormatAggregateMember("presentationOnly", offsetof(Value, presentationOnly), &Value::presentationOnly)};
    }
};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::commands::CommandScope>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::commands::CommandScope>> {};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::commands::NormalCommandScope>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::commands::NormalCommandScope>> {};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::commands::GlobalCommandScope>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::commands::GlobalCommandScope>> {};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::commands::CommandScopeRegistration>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::commands::CommandScopeRegistration>> {};
}
