#include "colorbleed.hpp"
#include "colorbleed_detail.hpp"
#include "colorbleed_texdata.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::core::colorbleed {
namespace {
std::shared_ptr<detail::Mask> createMask(std::span<const std::uint8_t> bytes) {
    return std::allocate_shared<detail::Mask>(
        nicxlive::core::d_runtime::SourceAllocator<detail::Mask>{}, bytes);
}
}

void incColorBleedPixels(nicxlive::core::ShallowTexture* texture, int maxIterations) {
    texture = compat::ngRequireObject(texture);
    const int width = texture->width, height = texture->height;
    detail::ShallowTexData textureData{texture->data};
    auto mask = createMask(
        std::span<const std::uint8_t>(textureData.bytes().data(), textureData.bytes().size()));
    int iterations = 0, lastPending = -1;
    while (mask->getPendingSize() > 0) {
        if (iterations >= maxIterations) break;
        lastPending = mask->getPendingSize();
        detail::executeIteration(textureData.pixels(), mask.get(), width, height);
        ++iterations;
        if (mask->getPendingSize() == lastPending) break;
    }
    texture->data = textureData.bytes();
}
void incColorBleedPixels(nicxlive::core::Texture& texture, int maxIterations) {
    const int width = texture.width(), height = texture.height();
    detail::TextureTexData textureData{texture.getSourceTextureData()};
    auto mask = createMask(
        std::span<const std::uint8_t>(textureData.bytes().data(), textureData.bytes().size()));
    int iterations = 0, lastPending = -1;
    while (mask->getPendingSize() > 0) {
        if (iterations >= maxIterations) break;
        lastPending = mask->getPendingSize();
        detail::executeIteration(textureData.pixels(), mask.get(), width, height);
        ++iterations;
        if (mask->getPendingSize() == lastPending) break;
    }
    texture.setData(textureData.bytes());
}
void incColorBleedPixels(std::shared_ptr<nicxlive::core::Texture> texture, int maxIterations) {
    // Retain the actual class receiver across its virtual dimension/data calls.
    incColorBleedPixels(*compat::ngRequireObject(texture), maxIterations);
}
}
