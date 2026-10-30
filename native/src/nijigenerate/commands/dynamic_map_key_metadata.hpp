#pragma once
#include "detail/map_validation.hpp"
#include "node/dynamic_keys.hpp"
#include "view/panel_key.hpp"
#include "automesh/dynamic_key.hpp"
#include "automesh/config_keys.hpp"
namespace nijigenerate::commands::detail {
#define NG_COMMAND_KEY_NAME(Type, Name) \
    template<> struct CommandMapKeyMetadata<Type> { \
        inline static constexpr std::string_view typeName = Name; \
    };
NG_COMMAND_KEY_NAME(node::dynamic::AddNodeKey, "AddNodeKey")
NG_COMMAND_KEY_NAME(node::dynamic::InsertNodeKey, "InsertNodeKey")
NG_COMMAND_KEY_NAME(node::dynamic::ConvertToKey, "ConvertToKey")
NG_COMMAND_KEY_NAME(view::panel::PanelKey, "PanelKey")
NG_COMMAND_KEY_NAME(automesh::dynamic::AutoMeshKey, "AutoMeshKey")
NG_COMMAND_KEY_NAME(automesh::config::AutoMeshGetConfigKey, "AutoMeshGetConfigKey")
NG_COMMAND_KEY_NAME(automesh::config::AutoMeshSetConfigKey, "AutoMeshSetConfigKey")
NG_COMMAND_KEY_NAME(automesh::config::AutoMeshSetSimpleKey, "AutoMeshSetSimpleKey")
NG_COMMAND_KEY_NAME(automesh::config::AutoMeshSetAdvancedKey, "AutoMeshSetAdvancedKey")
NG_COMMAND_KEY_NAME(automesh::config::AutoMeshSetPresetKey, "AutoMeshSetPresetKey")
#undef NG_COMMAND_KEY_NAME
}
