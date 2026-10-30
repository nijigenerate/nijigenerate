#pragma once
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <compat/source_allocator.hpp>
#include <cstdint>
#include <charconv>
#include <limits>
#include <memory>
#include <initializer_list>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nijigenerate::autorig::deterministic::native_detail {
template<class T> using NativeVector = std::vector<T, nicxlive::core::d_runtime::SourceAllocator<T>>;

inline compat::Slice<std::uint8_t> allocateNativeBytes(std::size_t count) {
    if (!count) return {};
    // No user element or iterator is invoked by this actual primitive buffer.
    try {
        return compat::Slice<std::uint8_t>(std::vector<std::uint8_t>(count));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline std::string allocateNativeText(std::size_t count, char initial) {
    try {
        return std::string(count,initial);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline compat::RetainedString retainNativeText(std::string bytes) {
    try {
        return compat::RetainedString(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline compat::RetainedString nativeText(std::initializer_list<std::string_view> parts) {
    // Arguments are already evaluated. This boundary contains only byte copies
    // and the canonical immutable string's actual backing allocation.
    try {
        std::string bytes;
        for (const auto part : parts) bytes.append(part);
        return compat::RetainedString(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class Integer> compat::RetainedString nativeNumber(Integer value) {
    char bytes[std::numeric_limits<Integer>::digits10+3];
    const auto result=std::to_chars(bytes,bytes+sizeof(bytes),value);
    return nativeText({std::string_view(bytes,static_cast<std::size_t>(result.ptr-bytes))});
}

inline compat::RetainedString nativeCaptureStem(std::string_view name) {
    try {
        std::string stem(name);
        std::size_t position=0;
        while ((position=stem.find("::",position))!=std::string::npos) {
            stem.replace(position,2,"-");
            ++position;
        }
        return compat::RetainedString(std::move(stem));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline compat::RetainedString copyNativeMessage(compat::RetainedString message) {
    // Source POD idup calls _d_newarrayU, which returns null for length zero.
    if (message.empty()) return {};
    try {
        return compat::RetainedString(std::string(message.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T, class... Args> std::shared_ptr<T> createNativeObject(Args&&... arguments) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(arguments)...);
}
}
