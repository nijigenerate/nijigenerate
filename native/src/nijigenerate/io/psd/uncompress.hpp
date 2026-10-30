#pragma once
// Phobos std.zlib.UnCompress, Copyright The D Language Foundation 2000-2011.
// BSL-1.0; full license and source notice: port/licenses/phobos-kra-zip.txt.
#include <nijigenerate/compat/const_slice.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <zlib.h>

namespace nijigenerate::io::psd::detail {

enum class HeaderFormat { deflate, gzip, determineFromData };

// Production decoder used by the selected vendor rle.d translation.
// Each returned descriptor owns its output independently of the decoder.
class UnCompress {
    z_stream stream_{};
    int initialized_ = 0;
    int done_ = 0;
    bool inputEnded_ = false;
    std::size_t destinationBufferSize_ = 0;
    HeaderFormat format_ = HeaderFormat::deflate;
    [[noreturn]] void error(int status);
public:
    explicit UnCompress(std::uint32_t destinationBufferSize);
    explicit UnCompress(HeaderFormat format = HeaderFormat::determineFromData);
    virtual ~UnCompress() noexcept;
    UnCompress(const UnCompress&) = delete;
    UnCompress& operator=(const UnCompress&) = delete;
    UnCompress(UnCompress&&) = delete;
    UnCompress& operator=(UnCompress&&) = delete;
    virtual compat::ConstSlice<std::uint8_t> uncompress(std::span<const std::uint8_t> input);
    virtual compat::Slice<std::uint8_t> flush();
    virtual bool empty() const;
};

} // namespace nijigenerate::io::psd::detail
