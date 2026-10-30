#pragma once
#include "additional_tables.hpp"
#include "node/dynamic.hpp"
#include "all_map_key_metadata.hpp"
#include "dynamic_map_key_metadata.hpp"
#include "binding/binding_table.hpp"
#include "node/node_table.hpp"
#include "parameter/group_table.hpp"
#include "parameter/param_table.hpp"
#include "parameter/paramedit_table.hpp"
#include "parameter/prop_table.hpp"
#include "puppet/file.hpp"
#include "puppet/edit_table.hpp"
#include "puppet/tool_table.hpp"
#include "viewport/control_table.hpp"
#include "viewport/palette_table.hpp"
#include "mesheditor/tool_commands.hpp"
#include "view/panel_command_map.hpp"
#include "inspector/apply_node_table.hpp"
#include "node/mask.hpp"
#include "node/welding.hpp"
#include "node/simplephysics.hpp"
#include "automesh/dynamic_registry.hpp"
#include "automesh/config_tables.hpp"
#include "vertex/define_mesh.hpp"
#include "model/set_deform_binding.hpp"
#include "depth/bone.hpp"
#include "depth/editor_table.hpp"
#include "depth/map.hpp"
#include "depth/psd_dialog.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <nijigenerate/core/typed_format_quote.hpp>

namespace nijigenerate::commands {
// Resolve TLS objects in the calling thread. Never retain another thread's address.
template<class F> void ngForEachCommandMap(F&& fn) {
    fn(scopeCommands);
    fn(binding::commands);
    fn(node::node::commands);
    fn(parameter::animedit::commands);
    fn(parameter::group::commands);
    fn(parameter::param::commands);
    fn(parameter::paramedit::commands);
    fn(parameter::prop::commands);
    fn(puppet::file::commands);
    fn(puppet::edit::commands);
    fn(puppet::view::commands);
    fn(puppet::tool::commands);
    fn(viewport::control::commands);
    fn(viewport::palette::commands);
    fn(mesheditor::tool::selectToolModeCommands);
    fn(view::panel::togglePanelCommands);
    fn(node::dynamic::addNodeCommands);
    fn(node::dynamic::insertNodeCommands);
    fn(node::dynamic::convertNodeCommands);
    fn(inspector::apply_node::commands);
    fn(node::mask::commands);
    fn(node::welding::commands);
    fn(node::simplephysics::commands);
    fn(automesh::dynamic::autoMeshApplyCommands);
    fn(automesh::config::commands);
    fn(automesh::config::autoMeshGetConfigCommands);
    fn(automesh::config::autoMeshSetConfigCommands);
    fn(automesh::config::autoMeshTypedCommands);
    fn(vertex::define_mesh::commands);
    fn(model::set_deform_binding::commands);
    fn(depth::bone::commands);
    fn(depth::editor::commands);
    fn(depth::map::commands);
    fn(depth::psd_dialog::commands);
}

template<class K> std::string_view ngCommandKeyTypeName() {
    return detail::CommandMapKeyMetadata<K>::typeName;
}

namespace detail {
// Only pure local string work belongs here; evaluate Source callbacks first.
template<class F> std::string ngCommandKeyStringAllocation(F&& operation) {
    try {
        return std::forward<F>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

template<class K> std::string ngCommandKeyName(const K& key) {
    if constexpr (std::is_enum_v<K>) {
        return detail::ngCommandKeyStringAllocation([&] {
            for (auto member : detail::CommandMapKeyMetadata<K>::members)
                if (member.value == key) return std::string(member.name);
            return "cast(" + std::string(ngCommandKeyTypeName<K>()) + ")" +
                std::to_string(static_cast<std::underlying_type_t<K>>(key));
        });
    } else if constexpr (requires { key.toString(); }) {
        decltype(auto) value = key.toString();
        const auto view = value.view();
        return detail::ngCommandKeyStringAllocation([&] { return std::string(view); });
    } else {
        const auto id = key.id.view();
        return detail::ngCommandKeyStringAllocation([&] {
            return std::string(ngCommandKeyTypeName<K>()) + "(" +
                core::translation::detail::formatQuotedString(id) + ")";
        });
    }
}
template<class K> std::string ngCommandIdFromKey(K key) {
    using namespace automesh::config;
    // Source K k copies the key descriptor before any conversion callback.
    // Retain its owners, without cloning the descriptor's shared element data.
    const auto stripPrefix = [](std::string_view value, std::string_view prefix) {
        return value.starts_with(prefix) ? value.substr(prefix.size()) : value;
    };
    const auto stripSuffix = [](std::string_view value, std::string_view suffix) {
        return value.ends_with(suffix) ? value.substr(0, value.size() - suffix.size()) : value;
    };
    const auto lowerLeading = [](std::string_view value) {
        // Source duplicates even an unchanged nonempty value, then applies
        // std.ascii.toLower to the first code unit only.
        std::string result(value);
        if (!result.empty() && result[0] >= 'A' && result[0] <= 'Z') result[0] += 'a' - 'A';
        return result;
    };
    if constexpr (std::same_as<K, AutoMeshGetConfigKey> || std::same_as<K, AutoMeshSetConfigKey> ||
                  std::same_as<K, AutoMeshSetSimpleKey> || std::same_as<K, AutoMeshSetAdvancedKey> ||
                  std::same_as<K, AutoMeshSetPresetKey>) {
        const auto id = key.id.view();
        return detail::ngCommandKeyStringAllocation([&] {
            if constexpr (std::same_as<K, AutoMeshGetConfigKey>) return "AutoMesh.GetConfig." + std::string(id);
            else if constexpr (std::same_as<K, AutoMeshSetConfigKey>) return "AutoMesh.SetConfig." + std::string(id);
            else if constexpr (std::same_as<K, AutoMeshSetSimpleKey>) return "AutoMesh.SetSimple." + std::string(id);
            else if constexpr (std::same_as<K, AutoMeshSetAdvancedKey>) return "AutoMesh.SetAdvanced." + std::string(id);
            else return "AutoMesh.SetPreset." + std::string(id);
        });
    } else {
        // User-defined key conversion must not enter the allocation-only catch.
        auto name = ngCommandKeyName(key);
        return detail::ngCommandKeyStringAllocation([&] {
            if constexpr (std::same_as<K, node::dynamic::AddNodeKey>) return "Node.Add." + std::move(name);
            else if constexpr (std::same_as<K, node::dynamic::InsertNodeKey>) return "Node.Insert." + std::move(name);
            else if constexpr (std::same_as<K, node::dynamic::ConvertToKey>) return "Node.ConvertTo." + std::move(name);
            else if constexpr (std::same_as<K, automesh::dynamic::AutoMeshKey>) return "AutoMesh.Apply." + std::move(name);
            else if constexpr (std::same_as<K, AutoMeshConfigCommand>)
                return "AutoMesh." + std::string(stripPrefix(name, "AutoMesh"));
            else if constexpr (std::same_as<K, AutoMeshTypedCommand>) {
                if (name.starts_with("AutoMeshSetSimple_"))
                    return "AutoMesh.SetSimple." + lowerLeading(
                        stripSuffix(stripPrefix(name, "AutoMeshSetSimple_"), "AutoMeshProcessor"));
                else if (name.starts_with("AutoMeshSetAdvanced_"))
                    return "AutoMesh.SetAdvanced." + lowerLeading(
                        stripSuffix(stripPrefix(name, "AutoMeshSetAdvanced_"), "AutoMeshProcessor"));
                else if (name.starts_with("AutoMeshSetPreset_"))
                    return "AutoMesh.SetPreset." + lowerLeading(
                        stripSuffix(stripPrefix(name, "AutoMeshSetPreset_"), "AutoMeshProcessor"));
                return "AutoMesh." + name;
            } else if constexpr (std::same_as<K, inspector::apply_node::InspectorNodeApplyCommand>)
                return "Inspector.Apply." + std::move(name);
            else if constexpr (std::same_as<K, view::panel::PanelKey>) return "Panel.Toggle." + std::move(name);
            else if constexpr (std::string_view{detail::CommandMapKeyMetadata<K>::typeName} == "AnimeditCommand")
                return "Animation.Add" + std::string(stripPrefix(name, "AddAnimation"));
            else return std::string(ngCommandKeyTypeName<K>()) + "." + std::move(name);
        });
    }
}
}
