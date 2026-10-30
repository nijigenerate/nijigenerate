#include "contracts_file.hpp"

#include <nijigenerate/autorig/deterministic/detail/sha256.hpp>
#include <nijigenerate/io/psd/retained_file.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <array>
#include <utility>

namespace nijigenerate::autorig::deterministic::contracts {

compat::RetainedString ngRigFileDigest(compat::RetainedString path) {
    io::psd::RetainedFile file(std::move(path));
    compat::RetainedString result;
    compat::ngRunWithScopeExit([&] {
        detail::Sha256 digest;
        {
            // ByChunkImpl retains its own File descriptor and primes before iteration.
            auto rangeFile = file;
            auto buffer = detail::allocateContractStorage([] {
                return std::make_unique<std::uint8_t[]>(64 * 1024);
            });
            std::span<std::uint8_t> chunk(buffer.get(), 64 * 1024);
            chunk = rangeFile.rawRead(chunk);
            if (chunk.empty()) rangeFile.detach();
            while (rangeFile.isOpen()) {
                digest.put(chunk);
                // Phobos reuses the previous returned slice, including short reads.
                chunk = rangeFile.rawRead(chunk);
                if (chunk.empty()) rangeFile.detach();
            }
        }
        const auto bytes = digest.finish();
        constexpr char digits[] = "0123456789abcdef";
        std::array<char, 64> hex{};
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            hex[2 * i] = digits[bytes[i] >> 4];
            hex[2 * i + 1] = digits[bytes[i] & 15];
        }
        result = detail::copyContractMessage(std::string_view(hex.data(), hex.size()));
    }, [&] { file.detach(); });
    return result;
}

} // namespace nijigenerate::autorig::deterministic::contracts
