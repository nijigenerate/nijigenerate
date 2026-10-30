#pragma once

#include "initializer_body.hpp"
#include "map_initialization.hpp"
#include <nijigenerate/commands/all_command_maps.hpp>
#include <array>
#include <tuple>
#include <utility>

namespace nijigenerate::commands {

namespace detail {

// These are the actual source import groups. Availability is only a lookup
// precondition; one matching declaration still requires its real body producer.
// Different D import groups are not silently merged into a best-match overload.
template<class Key> consteval std::size_t importedInitializerMatches() {
    return std::size_t{requires { ::nijigenerate::commands::ngInitCommands<Key>(); }}
        + requires { binding::ngInitCommands<Key>(); }
        + requires { node::node::ngInitCommands<Key>(); }
        + requires { parameter::animedit::ngInitCommands<Key>(); }
        + requires { parameter::group::ngInitCommands<Key>(); }
        + requires { parameter::param::ngInitCommands<Key>(); }
        + requires { parameter::paramedit::ngInitCommands<Key>(); }
        + requires { parameter::prop::ngInitCommands<Key>(); }
        + requires { puppet::file::ngInitCommands<Key>(); }
        + requires { puppet::edit::ngInitCommands<Key>(); }
        + requires { puppet::view::ngInitCommands<Key>(); }
        + requires { puppet::tool::ngInitCommands<Key>(); }
        + requires { viewport::control::ngInitCommands<Key>(); }
        + requires { viewport::palette::ngInitCommands<Key>(); }
        + requires { mesheditor::tool::ngInitCommands<Key>(); }
        + requires { view::panel::ngInitCommands<Key>(); }
        + requires { node::dynamic::ngInitCommands<Key>(); }
        + requires { inspector::apply_node::ngInitCommands<Key>(); }
        + requires { node::mask::ngInitCommands<Key>(); }
        + requires { node::welding::ngInitCommands<Key>(); }
        + requires { node::simplephysics::ngInitCommands<Key>(); }
        + requires { automesh::dynamic::ngInitCommands<Key>(); }
        + requires { automesh::config::ngInitCommands<Key>(); }
        + requires { vertex::define_mesh::ngInitCommands<Key>(); }
        + requires { model::set_deform_binding::ngInitCommands<Key>(); }
        + requires { depth::bone::ngInitCommands<Key>(); }
        + requires { depth::editor::ngInitCommands<Key>(); }
        + requires { depth::map::ngInitCommands<Key>(); }
        + requires { depth::psd_dialog::ngInitCommands<Key>(); };
}

} // namespace detail

namespace detail {

template<class Key> consteval bool dynamicInitializerSupported() {
    if constexpr (!requires { automesh::dynamic::ngInitCommands<Key>(); }) return false;
    else {
        static_assert(HasInitializerBody<Key>, "Dynamic initializer has no production body producer");
        return InitializerBody<Key>::supported();
    }
}

template<class Key> consteval bool configInitializerSupported() {
    if constexpr (!requires { automesh::config::ngInitCommands<Key>(); }) return false;
    else {
        static_assert(HasInitializerBody<Key>, "Config initializer has no production body producer");
        return InitializerBody<Key>::supported();
    }
}

} // namespace detail

// Each lookup has its own source import scope. A successful lookup additionally
// evaluates the operations shared with that initializer's production entry.
// Missing production representation is an error, never a fabricated false.
template<class Alias, class Key, int Index>
struct CTInitScan {
    inline static constexpr bool genericBodyProvided = detail::HasInitializerBody<Key>;
    inline static constexpr bool _hasDyn = detail::dynamicInitializerSupported<Key>();
    inline static constexpr bool _hasCfg = detail::configInitializerSupported<Key>();
};

template<class Alias, class Key, int Index>
    requires (detail::importedInitializerMatches<Key>() != 1 || detail::HasInitializerBody<Key>)
struct CTInitScan<Alias, Key, Index> {
    inline static constexpr bool genericBodyProvided = detail::HasInitializerBody<Key>;
    inline static constexpr bool _hasGen = [] {
        if constexpr (detail::importedInitializerMatches<Key>() != 1) return false;
        else return detail::InitializerBody<Key>::supported();
    }();
    inline static constexpr bool _hasDyn = detail::dynamicInitializerSupported<Key>();
    inline static constexpr bool _hasCfg = detail::configInitializerSupported<Key>();
};

template<detail::ExistingMapBinding Binding, int Index>
using CTInitScanInvoke = CTInitScan<Binding, detail::KeyTypeOfCommandMapBinding<Binding>, Index>;

namespace detail {

struct InitializerScanResult {
    bool generic;
    bool dynamic;
    bool config;
};

template<class Maps> struct InitializerScanSequence;

template<class... Binding> struct InitializerScanSequence<std::tuple<Binding...>> {
    inline static constexpr bool bodiesProvided =
        (HasInitializerBody<KeyTypeOfCommandMapBinding<Binding>> && ...);
    template<std::size_t... Index>
    static auto scans(std::index_sequence<Index...>) ->
        std::tuple<CTInitScanInvoke<Binding, static_cast<int>(Index)>...>;
    using type = decltype(scans(std::index_sequence_for<Binding...>{}));

    template<std::size_t... Index>
    static consteval auto evaluate(std::index_sequence<Index...>) {
        // Reference every enum initializer even when its result is false. A
        // tuple of scan types alone would not force these member definitions.
        return std::array<InitializerScanResult, sizeof...(Binding)>{
            InitializerScanResult{
                CTInitScanInvoke<Binding, static_cast<int>(Index)>::_hasGen,
                CTInitScanInvoke<Binding, static_cast<int>(Index)>::_hasDyn,
                CTInitScanInvoke<Binding, static_cast<int>(Index)>::_hasCfg}...};
    }
    static consteval auto evaluate() { return evaluate(std::index_sequence_for<Binding...>{}); }
};

} // namespace detail

using AllCommandInitializerScans = typename detail::InitializerScanSequence<AllCommandMaps>::type;
inline constexpr auto allCommandInitializerScanResults =
    detail::InitializerScanSequence<AllCommandMaps>::evaluate();

} // namespace nijigenerate::commands
