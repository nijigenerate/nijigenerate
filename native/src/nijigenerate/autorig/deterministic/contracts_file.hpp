#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::autorig::deterministic::contracts {

namespace detail {
// Storage operations only; serializers, numeric providers, File and user calls stay outside.
template<class Operation> auto allocateContractStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}
inline compat::RetainedString copyContractMessage(std::string_view message) {
    return allocateContractStorage([&] { return compat::RetainedString(message); });
}
}

compat::RetainedString ngRigFileDigest(compat::RetainedString path);

} // namespace nijigenerate::autorig::deterministic::contracts
