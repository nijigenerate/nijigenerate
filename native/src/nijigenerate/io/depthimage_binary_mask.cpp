#include "depthimage.hpp"
#include "depthimage_internal_types.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>

#include <algorithm>
#include <new>
#include <stdexcept>

namespace nijigenerate::io::depthimage {
namespace {

template<class Output, class Input> Output maskFromDepthPixels(Input depthPixels) {
    auto mask = detail::initialByteArray<Output>(depthPixels.size());
    for (std::size_t i = 0; i < mask.size(); ++i) mask[i] = depthPixels[i] > 0 ? 1 : 0;
    return mask;
}

template<class Output, class Input> Output applyBinaryMaskToDepthPixels(Input depthPixels, Input mask) {
    const auto length = std::min(depthPixels.size(), mask.size());
    Output result;
    if constexpr (std::same_as<Output, compat::Slice<std::uint8_t>>) {
        if (!depthPixels.empty()) {
            try {
                result = Output(depthPixels.begin(), depthPixels.end());
            } catch (const compat::OutOfMemoryError&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            }
        }
    } else {
        result.resize(depthPixels.size());
        for (std::size_t i = 0; i < depthPixels.size(); ++i) result[i] = depthPixels[i];
    }
    for (std::size_t i = 0; i < length; ++i) if (mask[i] == 0) result[i] = 0;
    for (std::size_t i = length; i < result.size(); ++i) result[i] = 0;
    return result;
}
} // namespace

std::vector<std::uint8_t> ngDepthDrawMaskFromDepthPixels(std::span<const std::uint8_t> depthPixels) {
    return maskFromDepthPixels<std::vector<std::uint8_t>>(depthPixels);
}

compat::Slice<std::uint8_t> ngDepthDrawMaskFromDepthPixelsSource(compat::ConstSlice<std::uint8_t> depthPixels) {
    return maskFromDepthPixels<compat::Slice<std::uint8_t>>(depthPixels);
}

std::vector<std::uint8_t> ngDepthDrawApplyBinaryMaskToDepthPixels(
        std::span<const std::uint8_t> depthPixels, std::span<const std::uint8_t> mask) {
    return applyBinaryMaskToDepthPixels<std::vector<std::uint8_t>>(depthPixels, mask);
}

compat::Slice<std::uint8_t> ngDepthDrawApplyBinaryMaskToDepthPixelsSource(
        compat::ConstSlice<std::uint8_t> depthPixels, compat::ConstSlice<std::uint8_t> mask) {
    return applyBinaryMaskToDepthPixels<compat::Slice<std::uint8_t>>(depthPixels, mask);
}

} // namespace nijigenerate::io::depthimage
