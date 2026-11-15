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
inline bool section(compat::Slice<std::uint8_t> data, const std::array<std::uint8_t, 8>& expected) {
    return data.size() >= expected.size() && std::equal(expected.begin(), expected.end(), data.begin());
}
}
inline nicxlive::core::ShallowTexture ngDecodeProjectTexture(std::span<const std::uint8_t> bytes, int channels = 0) {
    return nicxlive::core::ShallowTexture(bytes, channels);
}

template<class T> std::shared_ptr<T> ngLoadProjectJson(std::string_view json) {
    return nicxlive::core::fmt::inLoadTypedPuppetJson<T>(json);
}
template<class T> std::shared_ptr<T> ngLoadProjectInp(compat::Slice<std::uint8_t> buffer) {
    using namespace nicxlive::core;
    using Buffer = common::SharedArray<std::uint8_t>;
    const auto owner = buffer.retentionOwner();
    return fmt::inLoadPuppetFromMemory<T>(owner
        ? Buffer::retainedView(owner, buffer.data(), buffer.size())
        : Buffer::staticView(buffer.data(), buffer.size()));
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
