#pragma once

#include "binding.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <param/parameter.hpp>
#include <algorithm>
#include <type_traits>
#include <vector>

namespace nijigenerate::actions::binding::detail {

template<class Array> decltype(auto) bindingArrayAt(Array& values, std::size_t index, std::size_t line) {
    const auto length = values.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/binding.d"), line);
    return values[index];
}

// Keep this overload visible before the dependent matrix element calls.
inline nicxlive::core::param::DeformSlot cloneActionValue(nicxlive::core::param::DeformSlot value) {
    nicxlive::core::param::DeformSlot cloned;
    try {
        // The concrete float-lane duplicate does not invoke user constructors.
        cloned.vertexOffsets = value.vertexOffsets.dup();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return cloned;
}

template<class T> std::shared_ptr<std::vector<T>> reserveBindingValues(std::size_t count) {
    auto storage = std::allocate_shared<std::vector<T>>(
        nicxlive::core::d_runtime::SourceAllocator<std::vector<T>>{});
    try {
        // This vector is empty, so reserve cannot copy or move an arbitrary T.
        storage->reserve(count);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return storage;
}

inline nicxlive::core::common::SharedArray<bool> bindingFlags(std::size_t count) {
    try {
        return nicxlive::core::common::SharedArray<bool>(count, false);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T> nicxlive::core::common::SharedArray<T> duplicateBindingArray(
        nicxlive::core::common::SharedArray<T> source) {
    if (source.empty()) return {};
    if constexpr (std::is_same_v<T, bool>) {
        auto target = bindingFlags(source.size());
        std::copy(source.begin(), source.end(), target.begin());
        return target;
    } else {
        auto storage = reserveBindingValues<T>(source.size());
        // Element copies run outside the memory-only boundary. No default T
        // construction or additional T assignment is introduced by the buffer.
        for (const auto& value : source) storage->push_back(value);
        return nicxlive::core::common::SharedArray<T>(std::move(storage));
    }
}

template<class T>
auto duplicate(nicxlive::core::common::SharedArray<nicxlive::core::common::SharedArray<T>>& source) {
    const auto initialRows = source;
    auto target = duplicateBindingArray(initialRows);
    const auto sourceRows = source;
    for (std::size_t i = 0; i < sourceRows.size(); ++i) {
        const auto row = bindingArrayAt(sourceRows, i, 37);
        auto& destinationRow = bindingArrayAt(target, i, 38);
        destinationRow = duplicateBindingArray(row);
        for (std::size_t j = 0; j < row.size(); ++j) {
            auto value = bindingArrayAt(row, j, 39);
            auto& destination = bindingArrayAt(bindingArrayAt(target, i, 40), j, 40);
            const auto cloned = cloneActionValue(value);
            destination = cloned;
        }
    }
    return target;
}

template<class T>
void copy(nicxlive::core::common::SharedArray<nicxlive::core::common::SharedArray<T>>& source,
          nicxlive::core::common::SharedArray<nicxlive::core::common::SharedArray<T>>& target) {
    // Phobos zip retains both outer descriptors and uses their shorter length.
    const auto sourceRows = source;
    const auto targetRows = target;
    const auto count = std::min(sourceRows.size(), targetRows.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto sourceRow = bindingArrayAt(sourceRows, i, 47);
        auto targetRow = bindingArrayAt(targetRows, i, 47);
        for (std::size_t j = 0; j < sourceRow.size(); ++j) {
            auto value = bindingArrayAt(sourceRow, j, 48);
            auto& destination = bindingArrayAt(targetRow, j, 49);
            const auto cloned = cloneActionValue(value);
            destination = cloned;
        }
    }
}

} // namespace nijigenerate::actions::binding::detail
