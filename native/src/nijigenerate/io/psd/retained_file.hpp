#pragma once
// Required owned-stream subset of current Phobos File. BSL-1.0 notice:
// native/port/licenses/phobos-psd-file.txt.
#include <cstdint>
#include <cstdio>
#include <memory>
#include <span>
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/compat/phobos_stdio.hpp"

namespace nijigenerate::io::psd {
using RetainedFileError = providers::mcp::stdio::ErrnoException;

// A real retained CRT stream, including explicit close invalidation of aliases.
// The named constructor is the required File(name, "rb") subset.
// Other open modes, reopening, borrowed streams and popen remain separate APIs.
class RetainedFile {
public:
    RetainedFile() = default;
    explicit RetainedFile(compat::RetainedString name);
    RetainedFile(const RetainedFile& other);
    RetainedFile(RetainedFile&& other) noexcept;
    RetainedFile& operator=(RetainedFile other);
    ~RetainedFile() noexcept(false);

    static RetainedFile temporary();
    bool isOpen() const noexcept;
    compat::RetainedString name() const noexcept { return name_; }
    void detach();
    void close();
    void flush();
    void seek(std::int64_t offset, int origin = SEEK_SET);
    std::uint64_t tell() const;
    std::uint64_t size();
    std::span<std::uint8_t> rawRead(std::span<std::uint8_t> buffer);
    void rawWrite(std::span<const std::uint8_t> buffer);

private:
    struct Control;
    void closeHandles();
    std::shared_ptr<Control> owner_;
    // Phobos stores this descriptor per File value, outside its shared Impl.
    compat::RetainedString name_;
};
} // namespace nijigenerate::io::psd
