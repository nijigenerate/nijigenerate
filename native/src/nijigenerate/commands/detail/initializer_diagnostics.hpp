#pragma once

#include "initializer_scan.hpp"
#include <nijigenerate/viewport/vertex/automesh/meta.hpp>
#include <string_view>

namespace nijigenerate::commands {

inline constexpr bool _autoMeshKeyInitVisible =
    detail::dynamicInitializerSupported<automesh::dynamic::AutoMeshKey>();
inline constexpr bool _autoMeshGuardEnabled = false;

namespace detail {

template<class Key>
[[deprecated("[CT][Warn] AutoMeshKey initializer not visible (skipped compile-time guard)")]]
consteval bool autoMeshInitializerWarning() { return true; }

template<class Key>
[[deprecated("[CT][Warn] AutoMeshTypedCommand initializer not visible at this stage (will init explicitly at runtime)")]]
consteval bool autoMeshTypedInitializerWarning() { return true; }

template<std::size_t Size> struct InitializerDiagnosticText {
    char bytes[Size + 1]{};
    consteval explicit InitializerDiagnosticText(std::string_view text) {
        for (std::size_t index = 0; index < Size; ++index) bytes[index] = text[index];
    }
};

template<std::size_t Index, class Processor, auto ProcessorId>
[[deprecated("[CT][AutoMeshKey][Fail] ApplyAutoMeshPT instantiation failed; see processor index/type/id")]]
consteval bool autoMeshProcessorWarning() { return true; }

template<class Key, class Processors> struct AutoMeshInitBreakdown;

template<class Key, class... Processor>
struct AutoMeshInitBreakdown<Key, compat::TypeSequence<Processor...>> {
    template<std::size_t Index, class PT> static consteval bool inspect() {
        if constexpr (!requires { sizeof(automesh::dynamic::ApplyAutoMeshPT<PT>); }) {
            // Preserve the actual metadata dependency of the source diagnostic.
            constexpr auto id = viewport::vertex::automesh::meta::AMProcInfo<PT>::id;
            return autoMeshProcessorWarning<Index, PT, InitializerDiagnosticText<id.size()>{id}>();
        } else return true;
    }
    template<std::size_t... Index>
    static consteval bool inspectAll(std::index_sequence<Index...>) {
        constexpr std::array<bool, sizeof...(Processor)> results{inspect<Index, Processor>()...};
        bool supported = true;
        for (const auto result : results) supported = supported && result;
        return supported;
    }
    static consteval bool inspectAll() {
        return inspectAll(std::index_sequence_for<Processor...>{});
    }
};

template<class Key, bool GuardEnabled> consteval bool diagnoseAutoMeshInitializer() {
    if constexpr (!dynamicInitializerSupported<Key>()) {
        if constexpr (GuardEnabled) {
            static_assert(AutoMeshInitBreakdown<Key,
                viewport::vertex::automesh::AutoMeshProcessorTypes>::inspectAll());
            static_assert(dynamicInitializerSupported<Key>(),
                "[CT][Guard] AutoMeshKey initializer not visible");
        } else return autoMeshInitializerWarning<Key>();
    }
    return true;
}

template<class Key> consteval bool diagnoseTypedInitializer() {
    constexpr bool genericVisible = [] {
        if constexpr (importedInitializerMatches<Key>() != 1) return false;
        else {
            static_assert(HasInitializerBody<Key>, "Typed initializer has no production body producer");
            return InitializerBody<Key>::supported();
        }
    }();
    if constexpr (!genericVisible && !configInitializerSupported<Key>())
        return autoMeshTypedInitializerWarning<Key>();
    return true;
}

inline constexpr bool autoMeshInitializerDiagnostic = diagnoseAutoMeshInitializer<
    automesh::dynamic::AutoMeshKey, _autoMeshGuardEnabled>();
inline constexpr bool autoMeshTypedInitializerDiagnostic =
    diagnoseTypedInitializer<automesh::config::AutoMeshTypedCommand>();

} // namespace detail
} // namespace nijigenerate::commands
