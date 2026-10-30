#pragma once
// i18n-d 1.0.2 mo.d, Copyright (c) 2020, Luna; BSD-2-Clause.
// Complete notice: native/port/licenses/i18n-d.txt.
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include <compat/source_allocator.hpp>
#include <common/shared_array.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::core::i18n::detail {

template<class Operation> decltype(auto) allocateMoStorage(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

class CatalogRangeError : public std::out_of_range {
public:
    CatalogRangeError() : std::out_of_range("Catalog byte slice out of range") {}
};

inline std::uint32_t catalogWord(std::span<const std::uint8_t> bytes, std::uint64_t offset) {
    // Table pointers outside the original allocation have no defined D result.
    // Keep the existing native guard, rather than adding a validation pass.
    if (offset > bytes.size() || bytes.size() - offset < 4) throw CatalogRangeError();
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + static_cast<std::size_t>(offset), sizeof(value));
    return value;
}

inline std::string_view catalogSlice(std::span<const std::uint8_t> bytes,
        std::uint32_t offset, std::uint32_t length, std::size_t line = 81) {
    const std::uint32_t end = offset + length;
    if (offset > end || end > bytes.size())
        throw compat::ngStaticError<compat::ArraySliceError>(offset, end, bytes.size(),
            compat::RetainedString::fromStaticLiteral("i18n/mo.d"), line);
    return {reinterpret_cast<const char*>(bytes.data() + offset), length};
}

inline compat::RetainedString catalogFormatMessage(const char* field, std::uint32_t value) {
    return allocateMoStorage([&] {
        static constexpr char digits[] = "0123456789abcdef";
        std::string text = std::string("Bad mo file ") + field + " 0x";
        for (int shift = 28; shift >= 0; shift -= 4) text += digits[(value >> shift) & 15];
        return compat::RetainedString(std::move(text));
    });
}

template<class Message>
void catalogEnforce(bool condition, Message&& makeMessage, std::size_t line) {
    if (condition) return;
    const auto message = std::forward<Message>(makeMessage)();
    const auto ownedMessage = message.empty() ?
        compat::RetainedString::fromStaticLiteral("Enforcement failed") :
        allocateMoStorage([&] { return compat::RetainedString(message.view()); });
    throw compat::ThrowableException(ownedMessage,
        compat::RetainedString::fromStaticLiteral("i18n/mo.d"), line);
}

template<class UInt> constexpr UInt catalogSwapEndian(UInt value) {
    UInt result = 0;
    for (std::size_t index = 0; index < sizeof(UInt); ++index) {
        result = static_cast<UInt>((result << 8) | (value & 0xff));
        value >>= 8;
    }
    return result;
}

// One actual MOFile, with the source header values and retained data descriptor.
// The three existing count/table spellings remain available to catalog callers.
struct CatalogMOFile {
    std::span<const std::uint8_t> data;
    std::uint32_t count{}, sources{}, targets{};
    std::uint32_t magic{}, revision{}, hashSize{};
    std::uintptr_t hashes{};
    std::shared_ptr<const void> dataOwner;
};

inline void initializeCatalogMO(CatalogMOFile& file, std::span<const std::uint8_t> bytes) {
    file.data = bytes;
    constexpr std::size_t headerSize = 24 + sizeof(void*);
    if (bytes.size() < headerSize)
        throw compat::ngStaticError<compat::ArraySliceError>(0, headerSize, bytes.size(),
            compat::RetainedString::fromStaticLiteral("i18n/mo.d"), 56);
    file.magic = catalogWord(bytes, 0);
    file.revision = catalogWord(bytes, 4);
    file.count = catalogWord(bytes, 8);
    file.sources = catalogWord(bytes, 12);
    file.targets = catalogWord(bytes, 16);
    file.hashSize = catalogWord(bytes, 20);
    std::memcpy(&file.hashes, bytes.data() + 24, sizeof(file.hashes));
    if (file.magic == 0xde120495u) {
        file.revision = catalogSwapEndian(file.revision);
        file.count = catalogSwapEndian(file.count);
        file.sources = catalogSwapEndian(file.sources);
        file.targets = catalogSwapEndian(file.targets);
        file.hashSize = catalogSwapEndian(file.hashSize);
        // Preserve the original assignment, including its use of magic.
        file.hashes = catalogSwapEndian(static_cast<std::uintptr_t>(file.magic));
    }
    const bool validMagic = file.magic == 0x950412deu;
    catalogEnforce(validMagic, [&] { return catalogFormatMessage("magic", file.magic); }, 66);
    const bool validRevision = file.revision == 0;
    catalogEnforce(validRevision, [&] { return catalogFormatMessage("revision", file.revision); }, 67);
}

inline std::unique_ptr<CatalogMOFile> loadCatalogMO(std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) return nullptr;
    auto file = allocateMoStorage([] { return std::make_unique<CatalogMOFile>(); });
    initializeCatalogMO(*file, bytes);
    return file;
}

inline std::shared_ptr<const CatalogMOFile> loadRetainedCatalogMO(
        nicxlive::core::common::SharedArray<std::uint8_t> bytes) {
    if (bytes.empty()) return {};
    auto file = std::allocate_shared<CatalogMOFile>(
        nicxlive::core::d_runtime::SourceAllocator<CatalogMOFile>{});
    file->dataOwner = bytes.retentionOwner();
    initializeCatalogMO(*file, {bytes.data(), bytes.size()});
    return file;
}

inline std::string_view queryCatalogMO(const CatalogMOFile* file, std::string_view text) {
    file = compat::ngRequireObject(file);
    const auto bytes = file->data;
    if (bytes.empty()) return text;
    for (std::uint32_t index = 0; index < file->count; ++index) {
        const auto sourceDescriptor = std::uint64_t(file->sources) + std::uint64_t(index) * 8;
        const auto sourceOffset = catalogWord(bytes, sourceDescriptor + 4);
        const auto sourceLength = catalogWord(bytes, sourceDescriptor);
        if (catalogSlice(bytes, sourceOffset, sourceLength, 81) != text) continue;
        const auto targetDescriptor = std::uint64_t(file->targets) + std::uint64_t(index) * 8;
        const auto targetLength = catalogWord(bytes, targetDescriptor);
        if (targetLength) {
            const auto targetOffset = catalogWord(bytes, targetDescriptor + 4);
            return catalogSlice(bytes, targetOffset, targetLength, 87);
        }
        return text;
    }
    return text;
}

} // namespace nijigenerate::core::i18n::detail
