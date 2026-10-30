#pragma once

// mir-algorithm 3.22.4 mir/rc/context.d and mir/type_info.d.
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include "assert_error.hpp"

namespace nijigenerate::compat {
inline void ngMirRCAssert(bool condition, const char* file, unsigned line) {
#ifndef NDEBUG
    if (!condition) throw ngStaticError<AssertError>(
        RetainedString::fromStaticLiteral("Assertion failure"), RetainedString::fromBorrowedView(file), line);
#else
    (void)condition;
    (void)file;
    (void)line;
#endif
}
struct MirRCTypeInfo {
    void (*destructor)(void*);
    std::int32_t size;
};

struct MirRCContext {
    void (*deallocator)(MirRCContext*);
    const MirRCTypeInfo* typeInfo;
    std::atomic<std::size_t> counter;
    std::size_t length;
};

inline void ngMirRCDelete(MirRCContext& context) {
    ngMirRCAssert(context.deallocator != nullptr, "mir/rc/context.d", 75);
    if (context.typeInfo->destructor) {
        auto* element = reinterpret_cast<std::byte*>(&context + 1);
        auto remaining = context.length;
        ngMirRCAssert(remaining != 0, "mir/rc/context.d", 84);
        do {
            context.typeInfo->destructor(element);
            element += context.typeInfo->size;
        } while (--remaining);
    }
    if (context.counter.load())
        ngAssertUnreachable("Assertion failure", "mir/rc/context.d", 95);
    context.deallocator(&context);
}

inline void ngMirRCIncrease(MirRCContext& context) {
    if (context.counter.load()) context.counter.fetch_add(1);
}

inline void ngMirRCDecrease(MirRCContext& context) {
    if (context.counter.load() && context.counter.fetch_sub(1) == 1) ngMirRCDelete(context);
}

inline MirRCContext* ngMirRCCreate(const MirRCTypeInfo& type, std::size_t length,
    const void* payload = nullptr, bool initialize = true, bool deallocate = true) {
    ngMirRCAssert(length != 0, "mir/rc/context.d", 117);
    // Source size_t arithmetic and allocation failure stay distinct from callbacks.
    const auto bytes = length * static_cast<std::size_t>(type.size);
    void* allocation = std::malloc(sizeof(MirRCContext) + bytes);
    if (!allocation) return nullptr;
    auto* context = ::new (allocation) MirRCContext{
        [](MirRCContext* value) { std::free(value); }, &type, deallocate ? 1u : 0u, length};
    if (initialize) {
        auto* destination = reinterpret_cast<std::byte*>(context + 1);
        if (payload) {
            for (std::size_t i = 0; i < length; ++i) {
                std::memcpy(destination, payload, static_cast<std::size_t>(type.size));
                destination += type.size;
            }
        } else std::memset(destination, 0, bytes);
    }
    return context;
}
} // namespace nijigenerate::compat
