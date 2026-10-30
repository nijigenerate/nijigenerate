#pragma once

#include <cstdint>
#include <string_view>

namespace nijigenerate::utils::crashdump::detail {

void writeAll(int fd, std::string_view text) noexcept;
void writeInt(int fd, std::int64_t value) noexcept;

} // namespace nijigenerate::utils::crashdump::detail
