#pragma once

#include "resource.hpp"
#include "tokenizer.hpp"
#include "treestore_sort.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <bit>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

namespace nijigenerate::core::selector {
template<bool addNonIncluded = true> class TreeStore_
    : public std::enable_shared_from_this<TreeStore_<addNonIncluded>> {
    template<class Value> using ResourceMap = nicxlive::core::common::PolicyKeyMap<ResourcePtr, Value,
        nicxlive::core::common::ObjectClassKeyPolicy<ResourcePtr>>;
    template<class Array> static SelectorArray<ResourcePtr> arrayCopy(const Array& values) {
        if (values.empty()) return {};
        try {
            return SelectorArray<ResourcePtr>(std::vector<ResourcePtr>(values.begin(), values.end()));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    static compat::RetainedString className() {
        // LDC ClassInfo uses DMD toPrettyChars(true). A single bool expression
        // prints !true/!false, followed by the eponymous class identifier.
        if constexpr (addNonIncluded)
            return compat::RetainedString::fromStaticLiteral(
                "nijigenerate.core.selector.treestore.TreeStore_!true.TreeStore_");
        else
            return compat::RetainedString::fromStaticLiteral(
                "nijigenerate.core.selector.treestore.TreeStore_!false.TreeStore_");
    }
    compat::RetainedString dynamicClassName() const {
        if (typeid(*this) == typeid(TreeStore_)) return className();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
public:
    TreeStore_() = default;
    TreeStore_(const TreeStore_&) = delete;
    TreeStore_& operator=(const TreeStore_&) = delete;
    TreeStore_(TreeStore_&&) = delete;
    TreeStore_& operator=(TreeStore_&&) = delete;
    virtual ~TreeStore_() = default;

    // Inherited object.d defaults (Digital Mars, BSL-1.0); full retained notice:
    // nicxlive/core/licenses/druntime-associative-array.txt.
    virtual compat::RetainedString toString() {
        [[maybe_unused]] const auto receiver = this->weak_from_this().lock();
        return dynamicClassName();
    }
    virtual std::size_t toHash() {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<TreeStore_> other) {
        return nicxlive::core::common::ngClassObjectIdentity(other) == dynamic_cast<const void*>(this);
    }
    virtual int opCmp(std::shared_ptr<TreeStore_>) {
        [[maybe_unused]] const auto receiver = this->weak_from_this().lock();
        const auto name = dynamicClassName();
        compat::RetainedString message;
        try {
            std::string bytes = "need opCmp for class ";
            bytes += name.view();
            message = compat::RetainedString(std::move(bytes));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        throw compat::ThrowableException(std::move(message),
            compat::RetainedString::fromStaticLiteral("object.d"), 195);
    }

    SelectorArray<ResourcePtr> nodes;
    ResourceMap<SelectorArray<ResourcePtr>> children;
    SelectorArray<ResourcePtr> roots;
    ResourceMap<bool> nodeIncluded;

    virtual void reset() {
        [[maybe_unused]] const auto receiver = this->weak_from_this().lock();
        children.clear();
        nodes.clear();
        nodeIncluded.clear();
        roots.clear();
    }

    virtual void setResources(SelectorArray<ResourcePtr> nodes_) {
        [[maybe_unused]] const auto receiver = this->weak_from_this().lock();
        nodes = std::move(nodes_);
        roots.clear();
        children.clear();
        nodeIncluded.clear();
        ResourceMap<ResourcePtr> parentMap;
        ResourceMap<bool> rootMap;
        ResourceMap<ResourceMap<bool>> childMap;
        const auto includedNodes = nodes;
        for (const auto n : includedNodes) nodeIncluded[n] = true;

        const auto addToMap = [&](auto&& recurse, ResourcePtr res, std::int32_t level) -> void {
            if (parentMap.contains(res)) return;
            if constexpr (addNonIncluded) {
                auto source = compat::ngRequireObject(res)->source;
                while (source) {
                    if (!source->source) break;
                    if (nodeIncluded.contains(source) || source->explicitSelection) break;
                    source = source->source;
                }
                if (source) {
                    parentMap[res] = source;
                    (void)childMap[source]; // Source require initializes the actual null AA value cell.
                    const auto slot = childMap[source];
                    auto& childrenForSource = static_cast<ResourceMap<bool>&>(slot);
                    childrenForSource[res] = true;
                    const auto nextLevel = std::bit_cast<std::int32_t>(
                        std::bit_cast<std::uint32_t>(level) + 1u);
                    recurse(recurse, source, nextLevel);
                } else rootMap[res] = true;
            } else {
                auto source = compat::ngRequireObject(res)->source;
                ResourcePtr parentInSet;
                while (source) {
                    if (nodeIncluded.contains(source)) { parentInSet = source; break; }
                    source = source->source;
                }
                if (parentInSet) {
                    parentMap[res] = parentInSet;
                    (void)childMap[parentInSet];
                    const auto slot = childMap[parentInSet];
                    auto& childrenForParent = static_cast<ResourceMap<bool>&>(slot);
                    childrenForParent[res] = true;
                } else rootMap[res] = true;
            }
        };
        const auto mappedNodes = nodes;
        for (const auto res : mappedNodes) addToMap(addToMap, res, 0);
        if constexpr (addNonIncluded) {
            const auto childKeys = childMap.keys();
            for (const auto res : childKeys) {
                if (!parentMap.contains(res)) {
                    rootMap[res] = true;
                } else {
                    const auto parent = parentMap.find(res);
                    if (parent == parentMap.end()) throw compat::ngStaticError<compat::RangeError>(
                        compat::RetainedString::fromStaticLiteral(
                            "source/nijigenerate/core/selector/treestore.d"), 87);
                    if (!parent->second) rootMap[res] = true;
                }
            }
        }
        auto rootKeys = rootMap.keys();
        const auto less = [](const ResourcePtr& a, const ResourcePtr& b) {
            const auto left = compat::ngRequireObject(a)->index;
            return left < compat::ngRequireObject(b)->index;
        };
        treestore_sort_detail::Sorter sorter(less);
        sorter.sort(std::span<ResourcePtr>(rootKeys.data(), rootKeys.size()));
        roots = arrayCopy(rootKeys);
        for (const auto& pair : childMap) {
            const auto parent = pair.first;
            const auto childSet = pair.second;
            auto childKeys = childSet.keys();
            sorter.sort(std::span<ResourcePtr>(childKeys.data(), childKeys.size()));
            auto sorted = arrayCopy(childKeys);
            children[parent] = std::move(sorted);
        }
    }
};
// D class references retain one object; copying a handle must not copy its fields.
template<bool addNonIncluded = true>
using TreeStoreReference = std::shared_ptr<TreeStore_<addNonIncluded>>;
using TreeStore = TreeStore_<>;
namespace treestore {
using ::nijigenerate::core::selector::TreeStore_;
using ::nijigenerate::core::selector::TreeStoreReference;
using ::nijigenerate::core::selector::TreeStore;
}
}
