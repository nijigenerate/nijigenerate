#pragma once

// SHA-256 subset adapted from current Phobos std.digest.sha.
// Copyright Kai Nacke 2012. BSL-1.0: native/port/licenses/phobos-rig-sha256.txt.
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace nijigenerate::autorig::deterministic::contracts::detail {

class Sha256 {
public:
    void put(std::span<const std::uint8_t> input) noexcept {
        auto index = static_cast<std::size_t>((count_ >> 3) & 63);
        count_ += static_cast<std::uint64_t>(input.size()) * 8;
        const auto partLength = 64 - index;
        std::size_t offset = 0;
        if (input.size() >= partLength) {
            std::copy_n(input.data(), partLength, buffer_.data() + index);
            transform(buffer_.data());
            offset = partLength;
            while (input.size() - offset >= 64) {
                transform(input.data() + offset);
                offset += 64;
            }
            index = 0;
        }
        if (input.size() != offset)
            std::copy_n(input.data() + offset, input.size() - offset, buffer_.data() + index);
    }

    std::array<std::uint8_t, 32> finish() noexcept {
        const auto bits = count_;
        const auto index = static_cast<std::size_t>((count_ >> 3) & 63);
        const auto padLength = index < 56 ? 56 - index : 120 - index;
        std::array<std::uint8_t, 64> padding{0x80};
        put(std::span<const std::uint8_t>(padding.data(), padLength));
        std::array<std::uint8_t, 8> length{};
        for (std::size_t i = 0; i < 8; ++i)
            length[i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
        put(length);
        std::array<std::uint8_t, 32> result{};
        for (std::size_t i = 0; i < 8; ++i)
            for (std::size_t j = 0; j < 4; ++j)
                result[4 * i + j] = static_cast<std::uint8_t>(state_[i] >> (24 - 8 * j));
        *this = Sha256{};
        return result;
    }

private:
    static std::uint32_t small0(std::uint32_t x) noexcept {
        return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
    }
    static std::uint32_t small1(std::uint32_t x) noexcept {
        return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10);
    }
    void transform(const std::uint8_t* block) noexcept {
        std::array<std::uint32_t, 16> words;
        auto a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        auto e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (std::size_t i = 0; i < 64; ++i) {
            if (i < 16) {
                const auto* bytes = block + i * 4;
                words[i] = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                    (static_cast<std::uint32_t>(bytes[1]) << 16) |
                    (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
            } else {
                words[i & 15] = small1(words[(i - 2) & 15]) + words[(i - 7) & 15] +
                    small0(words[(i - 15) & 15]) + words[i & 15];
            }
            const auto sigma1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const auto choose = g ^ (e & (f ^ g));
            const auto first = h + sigma1 + choose + constants_[i] + words[i & 15];
            const auto sigma0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const auto majority = (a & b) | (c & (a ^ b));
            const auto second = sigma0 + majority;
            h = g; g = f; f = e; e = d + first;
            d = c; c = b; b = a; a = first + second;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
        words.fill(0);
    }

    inline static constexpr std::array<std::uint32_t, 64> constants_{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    std::array<std::uint32_t, 8> state_{
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    std::uint64_t count_{};
    std::array<std::uint8_t, 64> buffer_{};
};

} // namespace nijigenerate::autorig::deterministic::contracts::detail
