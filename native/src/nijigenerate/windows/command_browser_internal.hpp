#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/windows/command_browser_resource_traits.hpp>
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <compat/phobos_string_hash.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <cstdint>
#include <memory>
#include <concepts>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nicxlive::core::nodes { class Node; }

namespace nijigenerate::windows::command_browser {

// The D open flag has ordinary per-thread storage, independently of the caches.
extern thread_local bool incIsCommandBrowserOpen;

namespace detail {
using BrowserText = compat::RetainedStringSlice;

template<class Allocate> auto allocateBrowserMemory(Allocate allocate) {
    try {
        return allocate();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>();
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>();
    }
}

inline compat::SourceCString sourceCString(BrowserText value) {
    compat::SourceCString result(value);
    compat::detail::ngSourceCStringPostconditions(value.view(), result.data());
    return result;
}

inline BrowserText retainBrowserText(std::string&& value) {
    return allocateBrowserMemory([&] {
        return BrowserText(compat::RetainedString(std::move(value)));
    });
}

template<std::integral T> BrowserText browserDecimal(T value) {
    return allocateBrowserMemory([&] {
        return BrowserText(compat::RetainedString(std::to_string(value)));
    });
}

inline BrowserText joinBrowserTexts(compat::Slice<BrowserText> values, std::string_view separator) {
    return allocateBrowserMemory([&] {
        std::string result;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i) result.append(separator);
            result.append(values[i].view());
        }
        return BrowserText(compat::RetainedString(std::move(result)));
    });
}
struct BrowserTextHash {
    std::size_t operator()(const BrowserText& value) const noexcept {
        return nicxlive::core::d_runtime::ngPhobosStringHash(value.view());
    }
};
struct BrowserTextEqual {
    bool operator()(const BrowserText& query, const BrowserText& stored) const noexcept {
        return query.view() == stored.view();
    }
};
template<class V> using BrowserTextMap = nicxlive::core::common::ResourceKeyMap<
    BrowserText, V, BrowserTextHash, BrowserTextEqual>;
template<class K, class V> using BrowserClassMap = nicxlive::core::common::PolicyKeyMap<
    K, V, nicxlive::core::common::ObjectClassKeyPolicy<K>>;
using RawArguments = BrowserTextMap<BrowserText>;
template<class Map> auto requiredBrowserEntry(Map& values, const BrowserText& key, std::size_t sourceLine) {
    auto found = values.find(key);
    if (found == values.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/command_browser.d"), sourceLine);
    return found;
}
using CommandRef = std::shared_ptr<commands::Command>;

struct CommandArgInfo {
    BrowserText name;
    BrowserText typeName;
    BrowserText desc;
    bool hidden = false;
    compat::Slice<BrowserText> enumValues;
    bool isIntegral = false;
    bool isFloat = false;
    bool isBool = false;
    bool isNodeResource = false;
    bool isNodeResourceArray = false;
    bool (*acceptsNodeResource)(std::shared_ptr<::nicxlive::core::nodes::Node>) = nullptr;
};

struct CommandInfo {
    BrowserText id;
    BrowserText category;
    CommandRef cmd;
    compat::Slice<CommandArgInfo> inputs;
    BrowserText outputType;
    bool isResource = false;
    BrowserText resourceType;
    BrowserText resourceChange;
    bool isExResult = false;
    BrowserText exResultType;
    void (*applyArgs)(CommandRef, RawArguments) = nullptr;
    void (*captureDefaults)(CommandRef, RawArguments&) = nullptr;
    compat::Slice<BrowserText> resourceArgs;
};

// The source counter is process-wide (__gshared), not an atomic/TLS identifier.
extern std::uint32_t gBindingNextId;

compat::RetainedStringSlice trimAtNul(compat::RetainedStringSlice text);
void textWrappedInColumn(const char* text);
// Uses the actual retained toStringz body for the selected Phobos library profile.
void textWrappedInColumn(compat::RetainedStringSlice text);
} // namespace detail

} // namespace nijigenerate::windows::command_browser
