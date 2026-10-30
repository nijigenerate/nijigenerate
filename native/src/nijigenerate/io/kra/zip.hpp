#pragma once
// Read-only Phobos std.zip adaptation. Copyright D Language Foundation 2000-2009.
// Boost Software License 1.0; see native/port/licenses/phobos-kra-zip.txt.
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
namespace nijigenerate::io::kra {
class ZipException : public compat::ThrowableException {
public:
    using ThrowableException::ThrowableException;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.zip.ZipException"; }
};
class ZlibException : public compat::ThrowableException {
public:
    explicit ZlibException(int error);
    std::string_view ngThrowableTypeName() const noexcept override { return "std.zlib.ZlibException"; }
};
struct ZipEntry {
    compat::Slice<std::uint8_t> compressedData, expandedData;
    std::uint32_t offset{}, compressedSize{}, expandedSize{}, crc{};
    std::uint16_t flags{}, method{};
};
class ZipArchive {
public:
    explicit ZipArchive(compat::Slice<std::uint8_t> bytes);
    void expand(std::shared_ptr<ZipEntry> entry);
    std::unordered_map<std::string, std::shared_ptr<ZipEntry>> directory;
private:
    compat::Slice<std::uint8_t> bytes_;
    void bounds(std::uint64_t offset, std::uint64_t count, std::size_t sourceLine) const;
    std::uint16_t u16(std::uint64_t offset) const;
    std::uint32_t u32(std::uint64_t offset) const;
    std::uint64_t u64(std::uint64_t offset) const;
};
}
