#pragma once
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cstddef>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::autorig::json::detail {
template<std::size_t N> compat::RetainedString copyArtifactMessage(const char (&message)[N]) {
    try {
        return compat::RetainedString(std::string_view(message, N - 1));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

// Private bindings used by the platform conversion for owned AutoRig JSON.
void* createNumericLocale(int category, const char* name);
void freeNumericLocale(void* locale);
double parseLocaleDouble(const char* text, char** end, void* locale);

// Actual module version branches; the non-Windows branch creates no CRT locale.
void* createArtifactNumericLocale();
void freeArtifactNumericLocale(void* locale);
double restoreArtifactDouble(compat::RetainedStringSlice token, void* locale);

}
