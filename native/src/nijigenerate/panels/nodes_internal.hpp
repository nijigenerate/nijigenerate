#pragma once
#include "nodes_menu.hpp"
#include "nodes_icon.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/cmd.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/node/node.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <array>
#include <optional>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
namespace nijigenerate::panels::nodes::detail {
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
using ContextRef = std::shared_ptr<commands::Context>;
using commands::node::node::NodeCommand;
template<std::size_t N> inline compat::RetainedString tr(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(value));
}
template<std::size_t N> inline compat::RetainedString translatedNodeLabel(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(value));
}
template<class... T> compat::RetainedStringSlice format(compat::RetainedString pattern, T&&... value) {
    const std::array args{core::translation::ngFormatArgument(std::forward<T>(value))...};
    auto text = core::translation::ngFormatTyped(pattern.view(), args);
    try { return compat::RetainedStringSlice(compat::RetainedString(std::move(text))); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline compat::SourceCString cstring(std::string_view value) {
    compat::SourceCString result(value);
    compat::detail::ngSourceCStringPostconditions(value, result.data());
    return result;
}
inline auto nodeName(NodeRef node) {
    return compat::ngRequireObject(node)->getName();
}
inline std::uint32_t nodeUuid(NodeRef node) {
    return compat::ngRequireObject(node)->getUUID();
}
template<class Map, class ReadKey>
inline bool nodeFilterValue(Map values, ReadKey readKey, std::size_t sourceLine) {
    // An indexed AA read retains its descriptor before the virtual key getter.
    const auto uuid = readKey();
    const auto found = values.find(uuid);
    if (found != values.end()) return found->second;
    throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/nodes.d"), sourceLine);
}
inline ContextRef context(NodeRef n) {
    auto result = std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
    result->puppet(project::incActivePuppet()); result->nodes({n}); return result;
}
template<NodeCommand id> inline void run(ContextRef& ctx) {
    commands::ngCmd<id>(ctx);
}
template<std::size_t N>
inline bool input(const char (&id)[N], compat::RetainedString& value, std::optional<float> width = {}) {
    const compat::RetainedStringSlice key = compat::RetainedString::fromStaticLiteral(id);
    return width ? widgets::ngInputText(key, *width, value) : widgets::ngInputText(key, value);
}
}
