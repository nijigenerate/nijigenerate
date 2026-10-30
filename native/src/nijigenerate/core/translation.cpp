// Translation behavior ported from i18n-d 1.0.2 (Copyright (c) 2020, Luna).
// BSD-2-Clause notice: native/port/licenses/i18n-d.txt.
#include "translation.hpp"
#include "i18n_mo.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <common/string_key_map.hpp>
#include <common/unicode_lowercase.hpp>
#include <compat/phobos_file_read.hpp>
#include <compat/source_allocator.hpp>
#include <cstdint>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::core::translation {
namespace {
namespace mo = i18n::detail;
using Language = mo::CatalogMOFile;
struct LookupEntry {
    compat::RetainedString target;
    compat::SourceCString cTarget;
    explicit LookupEntry(compat::RetainedString value)
        : target(std::move(value)),
          cTarget(target.view()) {
        compat::detail::ngSourceCStringPostconditions(target.view(), cTarget.data());
    }
};
thread_local std::shared_ptr<const Language> loadedLanguage;
using LookupTable = nicxlive::core::common::StringKeyMap<std::shared_ptr<const LookupEntry>>;
thread_local LookupTable lookup;
thread_local bool gettextFormat = false;

compat::RetainedString languageExtension(compat::RetainedString file) {
    const auto bytes = file.view();
#if defined(_WIN32)
    const auto separator = bytes.find_last_of("/\\:");
#else
    const auto separator = bytes.find_last_of('/');
#endif
    const auto dot = bytes.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0 ||
            (separator != std::string_view::npos && dot <= separator + 1)) return {};
    return file.subslice(dot, file.size());
}

std::uint32_t readWord(std::span<const std::uint8_t> data, std::uint64_t offset) {
    return mo::catalogWord(data, offset);
}
std::string_view readEntry(std::span<const std::uint8_t> data, std::uint64_t descriptor, std::size_t line) {
    const auto length = readWord(data, descriptor);
    const auto offset = readWord(data, descriptor + 4);
    return mo::catalogSlice(data, offset, length, line);
}
} // namespace

void ngClearLanguage() {
    lookup.clear();
    loadedLanguage.reset();
}

bool ngLoadLanguage(compat::RetainedString file) {
    if (file.empty()) {
        ngClearLanguage();
        return true;
    }
    lookup.clear();
    const auto originalExtension = languageExtension(file);
    // This concrete Unicode operation has no caller-supplied callbacks; only
    // its passive output backing failures need the native allocation bridge.
    const auto extension = mo::allocateMoStorage([&] {
        return nicxlive::core::common::unicode::toLower(originalExtension);
    });
    if (extension.view() != ".mo") {
        const auto message = mo::allocateMoStorage([&] {
            return compat::RetainedString("i18n: Invalid file format " + std::string(originalExtension.view()));
        });
        throw compat::ThrowableException(message,
            compat::RetainedString::fromStaticLiteral("i18n/tr.d"), 88);
    }
    try {
        auto language = mo::loadRetainedCatalogMO(nicxlive::core::d_runtime::ngReadBinaryFile(file));
        // tr.d publishes the loaded MO before generating cTargets. If that
        // allocation fails, the cleared lookup falls back to this actual MO.
        loadedLanguage = language;
        LookupTable entries;
        const auto requiredLanguage = compat::ngRequireObject(language);
        for (std::uint64_t index = 0; index < requiredLanguage->count; ++index) {
            const auto targetDescriptor = requiredLanguage->targets + index * 8;
            if (readWord(requiredLanguage->data, targetDescriptor) == 0) continue;
            const auto source = readEntry(requiredLanguage->data, requiredLanguage->sources + index * 8, 125);
            const auto target = readEntry(requiredLanguage->data, targetDescriptor, 126);
            entries.setRetained(compat::RetainedString::retainedView(language->dataOwner,
                    source.data(), source.size()),
                std::allocate_shared<LookupEntry>(nicxlive::core::d_runtime::SourceAllocator<LookupEntry>{},
                    compat::RetainedString::retainedView(language->dataOwner, target.data(), target.size())));
        }
        lookup = std::move(entries);
        gettextFormat = true;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ngLoadLanguage(const std::string& file) {
    // Native compatibility borrows only for this synchronous call. Authored
    // callers use the retained overload and carry their genuine source owner.
    return ngLoadLanguage(compat::RetainedString::fromBorrowedView(file));
}
bool ngLoadLanguage(const char* file) {
    return ngLoadLanguage(compat::RetainedString::fromBorrowedView(
        file ? std::string_view(file) : std::string_view{}));
}

std::string ngTranslate(std::string_view text) {
    if (const auto found = lookup.findRetained(text))
        return mo::allocateMoStorage([&] { return std::string((*found)->target.view()); });
    // A failed load clears the map but preserves the previous MO fallback.
    if (gettextFormat && loadedLanguage) {
        const auto language = loadedLanguage;
        for (std::uint64_t index = 0; index < language->count; ++index) {
            if (readEntry(language->data, language->sources + index * 8, 81) != text) continue;
            const auto descriptor = language->targets + index * 8;
            if (readWord(language->data, descriptor) == 0) break;
            const auto translated = readEntry(language->data, descriptor, 87);
            return mo::allocateMoStorage([&] { return std::string(translated); });
        }
    }
    return mo::allocateMoStorage([&] { return std::string(text); });
}
compat::RetainedString ngTranslateRetained(compat::RetainedString text) {
    // Preserve the existing lookup/loader types and their error behavior. This
    // counted path reads the same immutable key/value cells without allocating
    // a temporary key, cloning bytes or borrowing a C-string terminator length.
    if (const auto found = lookup.findRetained(text.view())) return (*found)->target;
    const auto language = loadedLanguage;
    if (gettextFormat && language) {
        for (std::uint64_t index = 0; index < language->count; ++index) {
            if (readEntry(language->data, language->sources + index * 8, 81) != text.view()) continue;
            const auto descriptor = language->targets + index * 8;
            if (readWord(language->data, descriptor) == 0) break;
            const auto target = readEntry(language->data, descriptor, 87);
            return compat::RetainedString::retainedView(language->dataOwner, target.data(), target.size());
        }
    }
    return text;
}
TranslatedCString ngTranslateCString(std::string_view text) {
    // tr.d __ first selects the independently owned cTargets[0] toStringz
    // copy. Do not substitute a fresh temporary string or the MO pointer here.
    if (const auto found = lookup.findRetained(text)) return {(*found)->cTarget.data(), *found};
    const auto language = loadedLanguage;
    if (gettextFormat && language) {
        for (std::uint64_t index = 0; index < language->count; ++index) {
            if (readEntry(language->data, language->sources + index * 8, 101) != text) continue;
            const auto descriptor = language->targets + index * 8;
            if (readWord(language->data, descriptor) == 0) break;
            const auto offset = readWord(language->data, descriptor + 4);
            if (offset >= language->data.size())
                throw compat::ngStaticError<compat::ArrayIndexError>(offset, language->data.size(),
                    compat::RetainedString::fromStaticLiteral("i18n/mo.d"), 107);
            return {reinterpret_cast<const char*>(language->data.data() + offset), language->dataOwner};
        }
    }
    auto fallback = std::allocate_shared<compat::SourceCString>(
        nicxlive::core::d_runtime::SourceAllocator<compat::SourceCString>{}, text);
    compat::detail::ngSourceCStringPostconditions(text, fallback->data());
    return {fallback->data(), std::move(fallback)};
}
} // namespace nijigenerate::core::translation
