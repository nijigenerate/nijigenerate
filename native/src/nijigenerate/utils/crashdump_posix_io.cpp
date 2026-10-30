#include "crashdump_posix_io_internal.hpp"
#include <unistd.h>

namespace nijigenerate::utils::crashdump::detail {

void writeAll(int fd, std::string_view text) noexcept {
    if (fd < 0 || text.empty()) return;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto wrote = ::write(fd, text.data() + pos, text.size() - pos);
        if (wrote <= 0) return;
        pos += static_cast<std::size_t>(wrote);
    }
}

void writeInt(int fd, std::int64_t value) noexcept {
    char buffer[32];
    std::size_t pos = sizeof(buffer);
    const bool negative = value < 0;
    auto magnitude = static_cast<std::uint64_t>(value);
    if (negative) magnitude = std::uint64_t{0} - magnitude;
    do {
        buffer[--pos] = static_cast<char>('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude);
    if (negative) buffer[--pos] = '-';
    writeAll(fd, std::string_view(buffer + pos, sizeof(buffer) - pos));
}

} // namespace nijigenerate::utils::crashdump::detail
