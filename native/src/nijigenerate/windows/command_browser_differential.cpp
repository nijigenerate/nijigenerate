#include "command_browser_enum_traits.hpp"
#include "command_browser_value_traits.hpp"
#include "command_browser_result_traits.hpp"
#include <nijigenerate/commands/all_command_maps.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <algorithm>
#include <array>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#ifdef NG_COMMAND_BROWSER_DIFFERENTIAL
namespace nijigenerate::windows::command_browser {
using namespace detail;
namespace {
void cbDiffCollectNodes(NodeRef node, Nodes& nodes) {
    if (!node) return;
    nodes.append(node);
    const auto children = node->children;
    for (auto child : children) cbDiffCollectNodes(child, nodes);
}
BrowserText cbDiffJoinValues(Texts values) {
    return concat(concat(staticText("["), join(values, ",")), staticText("]"));
}
template<class T> BrowserText cbDiffValue(T value) {
    if constexpr (isNodeResource<T>)
        return value ? concat(staticText("Node:"), browserDecimal(value->getUUID())) : staticText("null");
    else if constexpr (isExactParameterResource<T>)
        return value ? concat(staticText("Parameter:"), browserDecimal(value->getUUID())) : staticText("null");
    else if constexpr (isExactBindingResource<T>)
        return value ? concat(staticText("ParameterBinding:"), BrowserText(value->getNameDescriptor()))
                     : staticText("null");
    else if constexpr (ResourceArrayType<T>::isDynamicArray && isResourceArgument<T>) {
        Texts values;
        for (auto item : value) values.append(cbDiffValue(item));
        return cbDiffJoinValues(values);
    } else return browserValueString(value);
}
template<class T> bool cbDiffRawValue(BrowserText& raw, Nodes nodes, Parameters params, Bindings bindings) {
    if constexpr (std::same_as<T,BrowserText> || std::same_as<T,compat::RetainedString> || std::same_as<T,std::string>) {
        raw = staticText("command-browser-differential"); return true;
    } else if constexpr (isBrowserEnum<T>) {
        if constexpr (BrowserEnumTraits<T>::members.size() == 0) return false;
        else { raw = browserEnumLiteral<T>(0); return true; }
    } else if constexpr (std::same_as<T,bool>) { raw = staticText("true"); return true; }
    else if constexpr (isBrowserIntegral<T>) { raw = staticText("7"); return true; }
    else if constexpr (std::floating_point<T>) { raw = staticText("1.25"); return true; }
    else if constexpr (isNodeResource<T>) {
        T found;
        for (auto node : nodes) {
            if (auto casted = std::dynamic_pointer_cast<typename T::element_type>(node)) {
                found = casted;
                break;
            }
        }
        if (!found) return false;
        raw = browserDecimal(found->getUUID());
        return true;
    } else if constexpr (isNodeResourceArray<T>) {
        using E = NodeResourceElement<T>;
        Texts ids;
        for (auto node : nodes) {
            if (auto found = std::dynamic_pointer_cast<typename E::element_type>(node))
                ids.append(browserDecimal(found->getUUID()));
            if (ids.size() >= 2) break;
        }
        if (ids.empty()) return false;
        raw = join(ids, ","); return true;
    } else if constexpr (isExactParameterResource<T>) {
        if (params.empty()) return false;
        raw = browserDecimal(compat::ngRequireObject(params[0])->getUUID());
        return true;
    } else if constexpr (isExactBindingResource<T>) {
        if (bindings.empty()) return false;
        raw = browserDecimal(ensureCommandBrowserBindingId(bindings[0]));
        return true;
    } else if constexpr (isResourceArgument<T>) {
        using E = typename ResourceArrayType<T>::Element;
        Texts ids;
        if constexpr (isExactParameterResource<E>) {
            for (std::size_t i=0; i<params.size() && i<(ResourceArrayType<T>::isDynamicArray ? 2u : 1u); ++i)
                ids.append(browserDecimal(compat::ngRequireObject(params[i])->getUUID()));
        } else {
            for (std::size_t i=0; i<bindings.size() && i<(ResourceArrayType<T>::isDynamicArray ? 2u : 1u); ++i)
                ids.append(browserDecimal(ensureCommandBrowserBindingId(bindings[i])));
        }
        if (ids.empty()) return false;
        raw = join(ids, ","); return true;
    } else return false;
}
// These Source expressions contain only manifest field/type strings and are
// folded before runtime. Retain their actual bytes without extra allocations.
template<class F, int Kind> BrowserText cbDiffFieldConstant() {
    static constexpr std::string_view separator = Kind == 0 ? "=" : Kind == 1 ? "=<exception:" : ":";
    static constexpr std::string_view type = Kind == 2 ? F::typeName : std::string_view{};
    static constexpr auto bytes = [] {
        std::array<char, F::fieldName.size() + separator.size() + type.size()> result{};
        std::size_t offset = 0;
        for (const auto part : std::array{std::string_view(F::fieldName), separator, type})
            for (char byte : part) result[offset++] = byte;
        return result;
    }();
    return staticText(std::string_view(bytes.data(),bytes.size()));
}

template<class C> BrowserText cbDiffCapture(const std::shared_ptr<C>& instance) {
    Texts fields;
    if constexpr (!std::same_as<BrowserStorage<C>,void>) {
        std::apply([&](const auto&... descriptors) {
            ([&] {
                using F = std::remove_cvref_t<decltype(descriptors)>;
                try {
                    auto value = F::get(*instance);
                    fields.append(concat(cbDiffFieldConstant<F,0>(),cbDiffValue<typename F::Type>(value)));
                }
                catch (const compat::ThrowableException& error) {
                    const auto message = BrowserText(error.msg);
                    fields.append(concat(concat(cbDiffFieldConstant<F,1>(),message),staticText(">")));
                }
            }(), ...);
        },commands::CommandFields<BrowserStorage<C>>::descriptors);
    }
    return join(fields,";");
}
void sortText(Texts& values) {
    // Sort the original array; equal byte strings have indistinguishable order.
    if (values.size() < 2) return;
    std::sort(values.begin(),values.end(),[](const auto& a,const auto& b){return a.view()<b.view();});
}
template<class C> void cbDiffAppendCommand(Texts& lines, BrowserText id, CommandRef command,
    CommandInfo info, Nodes nodes, Parameters params, Bindings bindings) {
    auto instance = std::dynamic_pointer_cast<C>(command);
    if (!instance) { lines.append(concat(id,staticText("\tcast-failed"))); return; }
    RawArguments values;
    Texts unsupported;
    if constexpr (!std::same_as<BrowserStorage<C>,void>) {
        std::apply([&](const auto&... descriptors) {
            ([&] {
                using F = std::remove_cvref_t<decltype(descriptors)>;
                if constexpr (!F::hidden) {
                    BrowserText raw;
                    if (cbDiffRawValue<typename F::Type>(raw,nodes,params,bindings)) values[staticText(F::fieldName)]=raw;
                    else unsupported.append(cbDiffFieldConstant<F,2>());
                }
            }(), ...);
        },commands::CommandFields<BrowserStorage<C>>::descriptors);
    }
    auto before=cbDiffCapture(instance);
    if(info.applyArgs) info.applyArgs(command,values);
    auto after=cbDiffCapture(instance);
    BrowserText cleared;
    if(info.applyArgs && !values.empty()) {
        RawArguments emptyValues;
        for(const auto& [key,value]:values) emptyValues[key]=emptyText();
        info.applyArgs(command,emptyValues);
        cleared=cbDiffCapture(instance);
    }
    Texts pairs;
    for(const auto& [key,value]:values) pairs.append(concat(concat(key,staticText("=")),value));
    sortText(pairs);sortText(unsupported);
    // Materialize each Source concatenation in its authored left-to-right order.
    auto line = concat(id, staticText("\targs="));
    line = concat(line, join(pairs,";"));
    line = concat(line, staticText("\tunsupported="));
    line = concat(line, join(unsupported,";"));
    line = concat(line, staticText("\tbefore="));
    line = concat(line, before);
    line = concat(line, staticText("\tafter="));
    line = concat(line, after);
    line = concat(line, staticText("\tcleared="));
    line = concat(line, cleared);
    lines.append(line);
}
}
BrowserText ngCommandBrowserDifferentialReport() {
    rebuildCommandInfos();
    auto puppet=project::incActivePuppet();
    Nodes nodes;Parameters params;Bindings bindings;
    if(puppet) {
        cbDiffCollectNodes(puppet->root,nodes);
        const auto sourceParameters = puppet->parameters;
        if (!sourceParameters.empty()) {
            params = allocateBrowserMemory([&] {
                return Parameters(std::vector<ParameterRef>(sourceParameters.begin(),sourceParameters.end()));
            });
        }
        for(auto param:puppet->parameters)
            for(auto binding:compat::ngRequireObject(param)->bindings) bindings.append(binding);
    }
    Texts lines;
    commands::ngForEachCommandMap([&](const auto& table) {
        using Map = std::remove_cvref_t<decltype(table)>;
        using Entry = std::remove_cvref_t<decltype(*std::declval<const Map&>().begin())>;
        using Key = std::remove_cv_t<typename Entry::first_type>;
        using Value = typename Entry::second_type;
        const auto current = table;
        for (auto [key, command] : current) {
            const auto found = gCommandInfosByCmd.find(command);
            if (found == gCommandInfosByCmd.end()) continue;
            const auto id = retainBrowserText(commands::ngCommandIdFromKey(key));
            if constexpr (std::is_enum_v<Key>) {
                using Metadata = commands::detail::CommandMapKeyMetadata<Key>;
                // Source static foreach visits every declared enum member and
                // emits nothing for a stored value absent from EnumMembers.
                [&]<std::size_t... Index>(std::index_sequence<Index...>) {
                    ([&] {
                        constexpr auto member = Metadata::members[Index].value;
                        if (key != member) return;
                        if constexpr (requires { typename commands::CommandClassFor<member>::type; }) {
                            using C = typename commands::CommandClassFor<member>::type;
                            cbDiffAppendCommand<C>(lines,id,command,found->second,nodes,params,bindings);
                        } else lines.append(concat(id,staticText("\tno-concrete-type")));
                    }(), ...);
                }(std::make_index_sequence<Metadata::members.size()>{});
            } else {
                using C = typename Value::element_type;
                cbDiffAppendCommand<C>(lines,id,command,found->second,nodes,params,bindings);
            }
        }
    });
    sortText(lines);
    return join(lines,"\n");
}
}
#endif
