#pragma once
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <load_diagnostics.hpp>
#include <serde_reader.hpp>
#include <fmt/fmt.hpp>
#include <nijigenerate/io/imagefmt/reader.hpp>
#include <new>
#include <source_location>
#include <type_traits>

namespace nicxlive::core::d_runtime { class FileException; }
namespace nijigenerate::compat { using ::nicxlive::core::d_runtime::FileException; }
namespace nijigenerate::io::project_reader {
// Compatibility spelling of the one real std.file.FileException provider.
// Its platform implementation is included by throwing/catching translation units.
using ProjectFileError = compat::FileException;
compat::Slice<std::uint8_t> ngReadProjectFile(compat::RetainedStringSlice path);
[[noreturn]] void ngThrowProjectFileError(compat::RetainedStringSlice path, std::uint32_t code,
    std::source_location origin = std::source_location::current());
namespace detail {
inline constexpr auto sourceFile = "nijilive/fmt/package.d";
inline void enforce(bool value, const char* message, std::size_t line) {
    if (!value) throw compat::ThrowableException(compat::RetainedString(message),
        compat::RetainedString::fromBorrowedView(sourceFile), line);
}
struct BufferReader {
    compat::Slice<std::uint8_t> bytes;
    std::size_t offset = 0;
    compat::Slice<std::uint8_t> take(std::size_t count, std::size_t line) {
        const auto start = offset;
        offset += count;
        if (start > offset || offset > bytes.size())
            throw compat::ngStaticError<nicxlive::core::d_runtime::ArraySliceError>(
                start, offset, bytes.size(), compat::RetainedString::fromBorrowedView(sourceFile), line);
        return bytes.slice(start, offset);
    }
    std::uint8_t byte(std::size_t line) {
        const auto index = offset++;
        if (index >= bytes.size())
            throw compat::ngStaticError<nicxlive::core::d_runtime::ArrayIndexError>(
                index, bytes.size(), compat::RetainedString::fromBorrowedView(sourceFile), line);
        return bytes.data()[index];
    }
    std::uint32_t word(std::size_t line) {
        auto data = take(4, line);
        return (std::uint32_t(data[0]) << 24) | (std::uint32_t(data[1]) << 16) |
            (std::uint32_t(data[2]) << 8) | data[3];
    }
};
inline bool section(compat::Slice<std::uint8_t> data, const std::array<std::uint8_t, 8>& expected) {
    return data.size() >= expected.size() && std::equal(expected.begin(), expected.end(), data.begin());
}
inline std::string string(compat::Slice<std::uint8_t> bytes) {
    return bytes.empty() ? std::string{} : std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
inline std::vector<std::uint8_t> vector(compat::Slice<std::uint8_t> bytes) {
    return bytes.empty() ? std::vector<std::uint8_t>{} : std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}
inline compat::RetainedString retainedString(compat::Slice<std::uint8_t> bytes) {
    const auto owner = bytes.retentionOwner();
    const auto data = reinterpret_cast<const char*>(bytes.data());
    if (owner && data) return compat::RetainedString::retainedView(owner, data, bytes.size());
    // A nonnull ownerless Slice is the existing process-static input contract.
    return compat::RetainedString::fromBorrowedView(std::string_view(data, bytes.size()));
}
inline nicxlive::core::common::SharedArray<std::uint8_t> retainedBytes(compat::Slice<std::uint8_t> bytes) {
    using Bytes = nicxlive::core::common::SharedArray<std::uint8_t>;
    if (auto owner = bytes.retentionOwner()) return Bytes::retainedView(std::move(owner), bytes.data(), bytes.size());
    return Bytes::staticView(bytes.data(), bytes.size());
}
[[noreturn]] inline void throwReturnedSerde(const nicxlive::core::serde::ReadError& error) {
    using namespace nicxlive::core::serde::source_exceptions;
    if (const auto object = dynamic_cast<const FghjSerdeException*>(error.get())) throw *object;
    if (const auto object = dynamic_cast<const EmptyFghjException*>(error.get())) throw *object;
    throw *compat::ngRequireObject(error.object());
}
}
inline nicxlive::core::ShallowTexture ngDecodeProjectTexture(std::span<const std::uint8_t> bytes, int channels = 0) {
    return nicxlive::core::ShallowTexture(bytes, channels);
}

template<class T> std::shared_ptr<T> ngLoadProjectJson(std::string_view json) {
    static_assert(std::is_same_v<decltype(T::create()), std::shared_ptr<T>>,
        "A Source-derived Puppet must supply its actual retained construction factory");
    nicxlive::core::serde::inLastLoadDiagnostics = {};
    auto document = nicxlive::core::serde::parseTypedJson(json);
    auto puppet = T::create();
    if (const auto error = puppet->deserializeFromFghj(document.root()))
        detail::throwReturnedSerde(error);
    return puppet;
}
template<class T> std::shared_ptr<T> ngLoadProjectInp(compat::Slice<std::uint8_t> buffer) {
    using namespace nicxlive::core;
    detail::BufferReader reader{buffer};
    fmt::inpModeFlag() = true;
    detail::enforce(detail::section(buffer, fmt::MAGIC_BYTES), "Invalid data format for INP puppet", 81);
    reader.offset = 8;
    const auto jsonLength = reader.word(86);
    const auto json = detail::retainedString(reader.take(jsonLength, 88));
    detail::enforce(detail::section(reader.take(8, 90), fmt::TEX_SECTION),
        "Expected Texture Blob section, got nothing!", 90);
    // The selected editor profile is the original InDoesRender configuration.
    inBeginTextureLoading();
    const auto count = reader.word(98);
    common::SharedArray<std::shared_ptr<Texture>> slots;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto length = reader.word(104);
        [[maybe_unused]] const auto textureType = reader.byte(106);
        if (length == 0) inAddTextureBinary(ShallowTexture(SourceTextureBytes{}, 0, 0, 4));
        else {
            const auto bytes = reader.take(length, 109);
            inAddTextureBinary(ngDecodeProjectTexture({bytes.data(), bytes.size()}));
        }
        slots.push_back(inGetLatestTexture());
    }
    auto puppet = ngLoadProjectJson<T>(json.view());
    puppet->textureSlots = slots;
    puppet->updateTextureState();
    inEndTextureLoading();
    if (buffer.size() >= reader.offset + 8 && detail::section(reader.take(8, 139), fmt::EXT_SECTION)) {
        const auto sectionCount = reader.word(141);
        for (std::uint32_t i = 0; i < sectionCount; ++i) {
            const auto nameLength = reader.word(148);
            const auto name = detail::retainedString(reader.take(nameLength, 149));
            const auto payloadLength = reader.word(153);
            const auto payload = detail::retainedBytes(reader.take(payloadLength, 156));
            puppet->extData[name] = payload;
        }
    }
    return puppet;
}
template<class T> std::shared_ptr<T> ngLoadProjectPuppet(compat::RetainedStringSlice file) {
    try {
        auto bytes = ngReadProjectFile(file);
        const auto path = file.view();
        const auto extension = path.substr(platform::ngAutosaveStripExtension(path).size());
        if (extension == ".inp") {
            detail::enforce(detail::section(bytes, nicxlive::core::fmt::MAGIC_BYTES),
                "Invalid data format for INP puppet", 43);
            return ngLoadProjectInp<T>(bytes);
        }
        if (extension == ".inx") {
            detail::enforce(detail::section(bytes, nicxlive::core::fmt::MAGIC_BYTES),
                "Invalid data format for nijigenerate INX", 47);
            return ngLoadProjectInp<T>(bytes);
        }
        throw compat::ThrowableException(compat::RetainedString("Invalid file format of " +
            std::string(extension) + " at path " + std::string(path)),
            compat::RetainedString::fromBorrowedView(detail::sourceFile), 51);
    } catch (const compat::ThrowableException&) {
        nicxlive::core::inEndTextureLoading<false>();
        throw;
    }
}
}
