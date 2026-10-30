#pragma once

#include <nijigenerate/commands/automesh/config.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/viewport/vertex/automesh/package.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta.hpp>

namespace nijigenerate::commands::automesh::config::detail {

template<class Key, class PT> consteval bool processorRegistrationSupported() {
    if constexpr (!requires {
        typename ConfigCommandTypes<PT>::Get;
        typename ConfigCommandTypes<PT>::Set;
        typename ConfigCommandTypes<PT>::Simple;
        typename ConfigCommandTypes<PT>::Advanced;
        typename ConfigCommandTypes<PT>::Preset;
        viewport::vertex::automesh::meta::AMSourceAttributes<PT>::attributes;
        viewport::vertex::automesh::meta::AMSourceAttributes<PT>::identifier;
    }) return false;
    else {
        using Types = ConfigCommandTypes<PT>;
        using Info = viewport::vertex::automesh::meta::AMProcInfo<PT>;
        if constexpr (!requires {
            typename std::integral_constant<std::size_t, Info::id.size()>;
        }) return false;
        else if constexpr (std::same_as<Key, AutoMeshGetConfigKey>)
            return ngRuntimeCommandRegistrationSupported<typename Types::Get,
                decltype(autoMeshGetConfigCommands), Key, compat::RetainedString>();
        else if constexpr (std::same_as<Key, AutoMeshSetConfigKey>)
            return ngRuntimeCommandRegistrationSupported<typename Types::Set,
                decltype(autoMeshSetConfigCommands), Key, compat::RetainedString, compat::RetainedString>();
        else if constexpr (std::same_as<Key, AutoMeshSetSimpleKey>)
            return ngRuntimeCommandRegistrationSupported<typename Types::Simple,
                decltype(autoMeshSetSimpleCommands), Key>();
        else if constexpr (std::same_as<Key, AutoMeshSetAdvancedKey>)
            return ngRuntimeCommandRegistrationSupported<typename Types::Advanced,
                decltype(autoMeshSetAdvancedCommands), Key>();
        else if constexpr (std::same_as<Key, AutoMeshSetPresetKey>)
            return ngRuntimeCommandRegistrationSupported<typename Types::Preset,
                decltype(autoMeshSetPresetCommands), Key>();
        else if constexpr (std::same_as<Key, AutoMeshTypedCommand>)
            return ngRuntimeCommandRegistrationSupported<typename Types::Simple, decltype(autoMeshTypedCommands), Key>()
                && ngRuntimeCommandRegistrationSupported<typename Types::Advanced, decltype(autoMeshTypedCommands), Key>()
                && ngRuntimeCommandRegistrationSupported<typename Types::Preset, decltype(autoMeshTypedCommands), Key>();
        else return false;
    }
}

template<class Key, class... PT>
consteval bool processorInitializationSupported(compat::TypeSequence<PT...>) {
    return (processorRegistrationSupported<Key, PT>() && ...);
}

template<class Key> consteval bool initializationSupported() {
    if constexpr (std::same_as<Key, AutoMeshConfigCommand>) {
        using Map = decltype(commands);
        // Optional no-argument attempts cannot fail the enclosing initializer.
        // These are the subsequent eight required registrations, with real args.
        return ngRuntimeCommandRegistrationSupported<AutoMeshListProcessorsCommand, Map, Key>()
            && ngRuntimeCommandRegistrationSupported<AutoMeshGetActiveCommand, Map, Key>()
            && ngRuntimeCommandRegistrationSupported<AutoMeshSetActiveCommand, Map, Key, compat::RetainedString>()
            && ngRuntimeCommandRegistrationSupported<AutoMeshApplyActiveCommand, Map, Key>()
            && ngRuntimeCommandRegistrationSupported<AutoMeshGetSchemaCommand, Map, Key, compat::RetainedString>()
            && ngRuntimeCommandRegistrationSupported<AutoMeshGetValuesCommand, Map, Key,
                compat::RetainedString, compat::RetainedString>()
            && ngRuntimeCommandRegistrationSupported<AutoMeshSetPresetCommand, Map, Key,
                compat::RetainedString, compat::RetainedString>()
            && ngRuntimeCommandRegistrationSupported<AutoMeshSetValuesCommand, Map, Key,
                compat::RetainedString, compat::RetainedString, compat::RetainedString>();
    } else return processorInitializationSupported<Key>(viewport::vertex::automesh::AutoMeshProcessorTypes{});
}

template<class Key, class PT, std::size_t Index>
    requires(processorRegistrationSupported<Key, PT>())
void registerProcessor() {
    using Types = ConfigCommandTypes<PT>;
    using Info = viewport::vertex::automesh::meta::AMProcInfo<PT>;
    if constexpr (std::same_as<Key, AutoMeshTypedCommand>) {
        // Original _genTypedEnumMembers emits Simple, Advanced, Preset per PT.
        auto simple = ngCreateRegisteredCommand<typename Types::Simple>();
        autoMeshTypedCommands[static_cast<Key>(3 * Index)] = simple;
        auto advanced = ngCreateRegisteredCommand<typename Types::Advanced>();
        autoMeshTypedCommands[static_cast<Key>(3 * Index + 1)] = advanced;
        auto preset = ngCreateRegisteredCommand<typename Types::Preset>();
        autoMeshTypedCommands[static_cast<Key>(3 * Index + 2)] = preset;
    } else {
        const Key key{compat::RetainedString::fromStaticView(Info::id)};
        if constexpr (std::same_as<Key, AutoMeshGetConfigKey>) {
            auto command = ngCreateRegisteredCommand<typename Types::Get>(
                compat::RetainedString::fromStaticLiteral("Simple"));
            autoMeshGetConfigCommands[key] = command;
        } else if constexpr (std::same_as<Key, AutoMeshSetConfigKey>) {
            auto command = ngCreateRegisteredCommand<typename Types::Set>(
                compat::RetainedString::fromStaticLiteral("Simple"), compat::RetainedString::fromStaticLiteral("{}"));
            autoMeshSetConfigCommands[key] = command;
        } else if constexpr (std::same_as<Key, AutoMeshSetSimpleKey>) {
            auto command = ngCreateRegisteredCommand<typename Types::Simple>();
            autoMeshSetSimpleCommands[key] = command;
        } else if constexpr (std::same_as<Key, AutoMeshSetAdvancedKey>) {
            auto command = ngCreateRegisteredCommand<typename Types::Advanced>();
            autoMeshSetAdvancedCommands[key] = command;
        } else if constexpr (std::same_as<Key, AutoMeshSetPresetKey>) {
            auto command = ngCreateRegisteredCommand<typename Types::Preset>();
            autoMeshSetPresetCommands[key] = command;
        }
    }
}

template<class Key, class... PT> requires((processorRegistrationSupported<Key, PT>() && ...))
void initializeProcessors(compat::TypeSequence<PT...>) {
    []<std::size_t... Index>(std::index_sequence<Index...>) {
        (registerProcessor<Key, PT, Index>(), ...);
    }(std::index_sequence_for<PT...>{});
}

} // namespace nijigenerate::commands::automesh::config::detail

namespace nijigenerate::commands::automesh::config {

template<class T> requires(detail::initializationSupported<T>())
void ngInitCommands();
template<> void ngInitCommands<AutoMeshConfigCommand>();
template<> void ngInitCommands<AutoMeshGetConfigKey>();
template<> void ngInitCommands<AutoMeshSetConfigKey>();
template<> void ngInitCommands<AutoMeshSetSimpleKey>();
template<> void ngInitCommands<AutoMeshSetAdvancedKey>();
template<> void ngInitCommands<AutoMeshSetPresetKey>();
template<> void ngInitCommands<AutoMeshTypedCommand>();

} // namespace nijigenerate::commands::automesh::config

namespace nijigenerate::commands::detail {

// These are the source overload's key constraints, not a success list. Each
// selected body recomputes its real construction/metadata/assignment domain.
template<class Key> requires (std::same_as<Key, automesh::config::AutoMeshConfigCommand> ||
    std::same_as<Key, automesh::config::AutoMeshGetConfigKey> ||
    std::same_as<Key, automesh::config::AutoMeshSetConfigKey> ||
    std::same_as<Key, automesh::config::AutoMeshSetSimpleKey> ||
    std::same_as<Key, automesh::config::AutoMeshSetAdvancedKey> ||
    std::same_as<Key, automesh::config::AutoMeshSetPresetKey> ||
    std::same_as<Key, automesh::config::AutoMeshTypedCommand>)
struct InitializerBody<Key> {
    static consteval bool supported() { return automesh::config::detail::initializationSupported<Key>(); }
    static void invoke() requires(supported()) { automesh::config::ngInitCommands<Key>(); }
};

} // namespace nijigenerate::commands::detail
