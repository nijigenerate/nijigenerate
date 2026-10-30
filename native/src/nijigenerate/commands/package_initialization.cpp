#include "package.hpp"
#include "all_command_maps.hpp"
#include "detail/map_initialization.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::commands {
namespace {
#ifdef CMD_LOG
template<class Map> struct CommandAATypeName;
template<class Value> struct CommandValueTypeName;

template<> struct CommandValueTypeName<Command> {
    inline static constexpr std::string_view name = "Command";
};
template<bool Expose> struct CommandValueTypeName<node::node::AddNodeCommandT<Expose>> {
    inline static constexpr std::string_view name =
        Expose ? "AddNodeCommandT!true" : "AddNodeCommandT!false";
};
template<bool Expose> struct CommandValueTypeName<node::node::InsertNodeCommandT<Expose>> {
    inline static constexpr std::string_view name =
        Expose ? "InsertNodeCommandT!true" : "InsertNodeCommandT!false";
};
template<bool Expose> struct CommandValueTypeName<node::dynamic::ConvertNodeToCommand<Expose>> {
    inline static constexpr std::string_view name =
        Expose ? "ConvertNodeToCommand!true" : "ConvertNodeToCommand!false";
};

// Selected LDC 1.41.0 hdrgen prints VALUE[KEY] and a lone bool argument as !false.
// Both components refer to actual source declarations, never C++ RTTI names.
template<class Key, class Value> struct CommandValueAATypeName {
    inline static constexpr auto bytes = [] {
        constexpr auto valueType = CommandValueTypeName<Value>::name;
        constexpr auto key = detail::CommandMapKeyMetadata<Key>::typeName;
        std::array<char, valueType.size() + key.size() + 2> result{};
        std::size_t position = 0;
        for (char value : valueType) result[position++] = value;
        result[position++] = '[';
        for (char value : key) result[position++] = value;
        result[position] = ']';
        return result;
    }();
};

template<class Key, class Value, class Hash, class Equal>
struct CommandAATypeName<CommandMapStorage<Key, std::shared_ptr<Value>, Hash, Equal>>
    : CommandValueAATypeName<Key, Value> {};

// The model module already specializes its original map onto this same kernel.
template<class Key, class Value, class Hash, class Equal>
struct CommandAATypeName<core::selector::SelectorMap<Key, std::shared_ptr<Value>, Hash, Equal>>
    : CommandValueAATypeName<Key, Value> {};

template<> struct CommandAATypeName<view::panel::PanelCommandMap>
    : CommandValueAATypeName<view::panel::PanelKey, Command> {};
template<> struct CommandAATypeName<automesh::dynamic::AutoMeshApplyCommandMap>
    : CommandValueAATypeName<automesh::dynamic::AutoMeshKey, Command> {};
#endif

template<detail::ExistingMapBinding Binding>
void initializeMap() {
    using Map = typename Binding::map_type;
    using Key = detail::KeyTypeOfCommandMapBinding<Binding>;
    using Metadata = detail::CommandMapKeyMetadata<Key>;
#ifdef CMD_LOG
    const auto mapType = compat::RetainedString::fromStaticView(
        std::string_view(CommandAATypeName<Map>::bytes.data(), CommandAATypeName<Map>::bytes.size()));
    const auto writeLog = [](std::string_view pattern, auto... values) {
        providers::mcp::stdio::standardOutput().withTextWriter(
            [&](providers::mcp::stdio::FileTextWriter& writer) {
                const std::array args{core::translation::ngFormatArgument(values)...};
                core::translation::ngFormatTypedTo(
                    [&](std::string_view part) { writer.put(part); }, pattern, args);
                writer.put(static_cast<unsigned char>('\n'));
            });
    };
    writeLog("[CMD] init begin: AA=%s key=%s", mapType,
        compat::RetainedString::fromStaticView(Metadata::typeName));
#endif
    detail::ngInitCommandMap<Binding>();
    auto& map = Binding::get();
    std::size_t count = 0;
    for ([[maybe_unused]] const auto& entry : map) ++count;
    // Use the same genuine validator after the original prevalidation count.
    detail::ngValidateCommandMap<Binding>();
#ifdef CMD_LOG
    writeLog("[CMD] init end:   AA=%s count=%s", mapType, count);
#endif
}
}

void ngInitAllCommands() {
    // The comma fold sequences the original aliases, resolving each real TLS map at use.
    []<std::size_t... Index>(std::index_sequence<Index...>) {
        (initializeMap<std::tuple_element_t<Index, AllCommandMaps>>(), ...);
    }(std::make_index_sequence<std::tuple_size_v<AllCommandMaps>>{});
}
}
