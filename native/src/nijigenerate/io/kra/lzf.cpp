// Independent kra-d lzf.d. BSD-2-Clause: kra.LICENSE.
#include "document.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <bit>
#include <limits>

namespace nijigenerate::io::kra {
compat::Slice<std::uint8_t> lzfDecompress(compat::Slice<std::uint8_t> input,
    std::uint64_t length, std::int32_t maxout) {
    (void)length; // The original accepts but does not use this argument.
    if (maxout < 0) throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    compat::Slice<std::uint8_t> output; output.resize(static_cast<std::size_t>(maxout));
    const auto source = compat::RetainedString::fromStaticLiteral("kra/lzf.d");
    const auto inputAt = [&](std::int32_t index, std::size_t line) {
        const auto i = static_cast<std::size_t>(index);
        if (i >= input.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, input.size(), source, line);
        return input.data()[i];
    };
    const auto outputAt = [&](std::int32_t index, std::size_t line) -> std::uint8_t& {
        const auto i = static_cast<std::size_t>(index);
        if (i >= output.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, output.size(), source, line);
        return output.data()[i];
    };
    const auto slice = [&](compat::Slice<std::uint8_t> value, std::size_t first,
            std::size_t last, std::size_t line) {
        if (first > last || last > value.size())
            throw compat::ngStaticError<compat::ArraySliceError>(first, last, value.size(), source, line);
        return value.slice(first, last);
    };
    std::int32_t ip = 0, op = 0;
    const std::uint64_t limit = input.size() - 1;
    const auto wrap = [](std::uint32_t value) { return std::bit_cast<std::int32_t>(value); };
    while (static_cast<std::uint64_t>(ip) < limit) {
        const std::uint32_t ctrl = inputAt(ip, 15) + 1u;
        const std::uint32_t offset = (inputAt(ip, 16) & 31u) << 8;
        std::uint32_t count = inputAt(ip, 17) >> 5;
        ip = wrap(static_cast<std::uint32_t>(ip) + 1u);
        if (ctrl < 33) {
            auto dst = slice(output, static_cast<std::size_t>(op),
                static_cast<std::size_t>(static_cast<std::uint32_t>(op) + ctrl), 22);
            const auto src = slice(input, static_cast<std::size_t>(ip),
                static_cast<std::size_t>(static_cast<std::uint32_t>(ip) + ctrl), 22);
            for (std::size_t i = 0; i < ctrl; ++i) dst[i] = src[i];
            ip = wrap(static_cast<std::uint32_t>(ip) + ctrl); op = wrap(static_cast<std::uint32_t>(op) + ctrl);
        } else {
            --count;
            std::int32_t reference = wrap(static_cast<std::uint32_t>(op) - offset - 1u);
            if (count == 6) { count += inputAt(ip, 35); ip = wrap(static_cast<std::uint32_t>(ip) + 1u); }
            reference = wrap(static_cast<std::uint32_t>(reference) - inputAt(ip, 37));
            ip = wrap(static_cast<std::uint32_t>(ip) + 1u);
            if (static_cast<std::uint32_t>(op) + count + 3u > static_cast<std::uint32_t>(maxout) || reference < 0) return {};
            const auto copy = [&](std::size_t line) {
                auto& destination = outputAt(op, line);
                const auto value = outputAt(reference, line);
                destination = value;
                op = wrap(static_cast<std::uint32_t>(op) + 1u); reference = wrap(static_cast<std::uint32_t>(reference) + 1u);
            };
            copy(43); copy(44); copy(45);
            for (; count > 0; --count) copy(49);
        }
    }
    return slice(output, 0, static_cast<std::size_t>(op), 53);
}
}
