#include "retained_file.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cerrno>
#include <new>
#include <string>
#include <utility>
#include <sys/stat.h>
#ifdef _WIN32
#include <compat/phobos_file_read.hpp>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace nijigenerate::io::psd {
struct RetainedFile::Control {
    std::FILE* stream;
    explicit Control(std::FILE* value) : stream(value) {}
    ~Control() { if (stream) std::fclose(stream); }
};
namespace {
[[noreturn]] void failFile(int error, std::string_view operation) {
    throw RetainedFileError(error, operation);
}
}
RetainedFile::RetainedFile(compat::RetainedString name) : name_(std::move(name)) {
#ifdef _WIN32
    const auto path = nicxlive::core::d_runtime::file_read_detail::wide(name_);
    auto* stream = _wfopen(path.c_str(), L"rb");
#else
    const std::string path(name_.view());
    auto* stream = std::fopen(path.c_str(), "rb");
#endif
    if (!stream) failFile(errno, name_.view());
    try { owner_ = std::make_shared<Control>(stream); }
    catch (const std::bad_alloc&) {
        std::fclose(stream);
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
RetainedFile::RetainedFile(const RetainedFile& other) = default;
RetainedFile::RetainedFile(RetainedFile&& other) noexcept = default;
RetainedFile& RetainedFile::operator=(RetainedFile other) {
    owner_.swap(other.owner_);
    std::swap(name_, other.name_);
    return *this;
}
RetainedFile::~RetainedFile() noexcept(false) { closeHandles(); }
RetainedFile RetainedFile::temporary() {
    RetainedFile result;
    auto* stream = std::tmpfile();
    if (!stream) failFile(errno, "tmpfile");
    try { result.owner_ = std::make_shared<Control>(stream); }
    catch (const std::bad_alloc&) {
        std::fclose(stream);
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return result;
}
bool RetainedFile::isOpen() const noexcept { return owner_ && owner_->stream; }
void RetainedFile::closeHandles() {
    if (owner_ && owner_.use_count() == 1) close();
    owner_.reset();
}
void RetainedFile::detach() { closeHandles(); }
void RetainedFile::close() {
    if (!isOpen()) return;
    auto* stream = std::exchange(owner_->stream, nullptr);
    if (std::fclose(stream) != 0) failFile(errno, "fclose");
}
void RetainedFile::flush() {
    if (!isOpen()) failFile(EBADF, "fflush");
    if (std::fflush(owner_->stream) != 0) failFile(errno, "fflush");
}
void RetainedFile::seek(std::int64_t offset, int origin) {
    if (!isOpen()) failFile(EBADF, "fseek");
#ifdef _WIN32
    const int result = _fseeki64(owner_->stream, offset, origin);
#else
    const int result = fseeko(owner_->stream, static_cast<off_t>(offset), origin);
#endif
    if (result != 0) failFile(errno, "fseek");
}
std::uint64_t RetainedFile::tell() const {
    if (!isOpen()) failFile(EBADF, "ftell");
#ifdef _WIN32
    const auto result = _ftelli64(owner_->stream);
#else
    const auto result = ftello(owner_->stream);
#endif
    if (result < 0) failFile(errno, "ftell");
    return static_cast<std::uint64_t>(result);
}
std::uint64_t RetainedFile::size() {
    if (!isOpen()) failFile(EBADF, "fstat");
#ifdef _WIN32
    struct _stat64 status;
    const int result = _fstat64(_fileno(owner_->stream), &status);
#else
    struct stat status;
    const int result = fstat(fileno(owner_->stream), &status);
#endif
    if (result != 0) failFile(errno, "fstat");
    return static_cast<std::uint64_t>(status.st_size);
}
std::span<std::uint8_t> RetainedFile::rawRead(std::span<std::uint8_t> buffer) {
    if (!isOpen()) failFile(EBADF, "fread");
    const auto count = std::fread(buffer.data(), 1, buffer.size(), owner_->stream);
    if (count != buffer.size() && std::ferror(owner_->stream)) failFile(errno, "fread");
    return buffer.first(count);
}
void RetainedFile::rawWrite(std::span<const std::uint8_t> buffer) {
    if (!isOpen()) failFile(EBADF, "fwrite");
    if (std::fwrite(buffer.data(), 1, buffer.size(), owner_->stream) != buffer.size())
        failFile(errno ? errno : EIO, "fwrite");
}
}
