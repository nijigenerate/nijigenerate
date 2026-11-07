#pragma once

#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::commands::depth::bone::detail {

template<class Allocate> decltype(auto) allocateBoneConcatenation(Allocate&& allocate) {
    try {
        return std::forward<Allocate>(allocate)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}

inline compat::RetainedString concatenateBoneStrings(
        std::initializer_list<std::string_view> parts) {
    return allocateBoneConcatenation([&] {
        std::string result;
        for (auto part : parts) result.append(part);
        return compat::RetainedString(std::move(result));
    });
}

} // namespace nijigenerate::commands::depth::bone::detail
