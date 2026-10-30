#pragma once

#include "typed_format.hpp"
#include <common/shared_array.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>

namespace nijigenerate::core::translation {

template<class T>
TypedFormatArgument ngFormatArgument(nicxlive::core::common::SharedArray<T> values) {
    auto range = [] {
        try { return std::make_shared<FormatRange>(); }
        catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    range->length = values.size();
    range->sourceTypeName=FormatSourceTypeName<nicxlive::core::common::SharedArray<T>>::get();
    if constexpr (!FormatArrayElement<T>) {
        range->characterBits = static_cast<unsigned>(sizeof(T) * CHAR_BIT);
        range->characterView = std::basic_string_view<T>(values.data(), values.size());
    }
    // The actual runtime SharedArray<bool> owns bool[] cells. This adapter does
    // not route them through vector<bool> or make an eager array of arguments.
    try {
        range->read = [values = std::move(values)](std::size_t index) mutable {
            if constexpr (compat::SourceFixedArray<std::remove_cv_t<T>>::value)
                return ngFormatFixedArrayReferenceArgument(values[index], values.retentionOwner());
            else if constexpr (HasCustomFormat<T> || HasAggregateFormatMetadata<std::remove_cv_t<T>>)
                return ngFormatValueReferenceArgument(values[index], values.retentionOwner());
            else return ngFormatArgument(values[index]);
        };
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return range;
}

} // namespace nijigenerate::core::translation

