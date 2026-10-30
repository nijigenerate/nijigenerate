#include "common.hpp"
#include "nijigenerate/compat/scope_exit.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <nodes/projectable.hpp>
#include <algorithm>
#include <bit>

namespace nijigenerate::viewport::vertex::automesh::common {
namespace {
using ::nijigenerate::core::cv::image::ngCreateImage;
using ::nijigenerate::core::cv::image::ngCreateImageDeferred;
using ImageFormat = ::nijigenerate::core::cv::image::ImageFormat;
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Mat4;

thread_local AlphaInputCache* gAutoMeshAlphaInputCache = nullptr;

std::int32_t pixelCount(const AlphaInput& input) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(input.w) *
        static_cast<std::uint32_t>(input.h));
}

std::size_t alphaOffset(std::int32_t index) {
    const auto offset = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index) * 4u + 3u);
    return static_cast<std::size_t>(offset);
}

template<class Bytes>
decltype(auto) byteAt(Bytes& bytes, std::size_t offset, std::size_t line) {
    if (offset >= bytes.size()) throw compat::ngStaticError<compat::ArrayIndexError>(offset, bytes.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/vertex/automesh/common.d"), line);
    return bytes[offset];
}

void clearImage(const AlphaInput& input) {
    auto bytes = input.img->data();
    for (auto& byte : bytes) byte = 0;
}

void copyAlpha(const AlphaInput& input, const std::uint8_t* alpha, std::size_t line) {
    const auto count = pixelCount(input);
    auto bytes = input.img->data();
    for (std::int32_t index = 0; index < count; ++index) {
        auto& destination = byteAt(bytes, alphaOffset(index), line);
        if (!alpha) compat::ngFailNullAccess(false);
        destination = alpha[index];
    }
}

Vec2 projectPosition(const Mat4& matrix, Vec2 position) {
    const float values[]{position.x, position.y, 0.0f, 1.0f};
    float result[4]{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            result[row] = nicxlive::core::math::detail::inmathAccumulate(result[row],
                nicxlive::core::math::detail::inmathProduct(matrix[row][column], values[column]));
        }
    }
    return {result[0], result[1]};
}
} // namespace

void setAutoMeshAlphaInputCache(AlphaInputCache* cache) {
    gAutoMeshAlphaInputCache = cache;
}

void clearAutoMeshAlphaInputCache(AlphaInputCache* cache) {
    if (gAutoMeshAlphaInputCache == cache) gAutoMeshAlphaInputCache = nullptr;
}

AlphaInput alphaInputFromProvider(std::shared_ptr<IAlphaProvider> provider) {
    AlphaInput input;
    input.w = compat::ngRequireObject(provider)->width();
    input.h = provider->height();
    input.fromProvider = true;
    const auto bounds = provider->boundsWorld();
    input.providerWorldCenter = {bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f};
    return input;
}

AlphaInput alphaInputFromProviderWithImage(std::shared_ptr<IAlphaProvider> provider) {
    auto input = alphaInputFromProvider(provider);
    if (input.w > 0 && input.h > 0) {
        input.img = ngCreateImage<>(input.w, input.h, ImageFormat::IF_RGB_ALPHA);
        clearImage(input);
        const auto* alpha = provider->alphaPtr();
        copyAlpha(input, alpha, 46);
    }
    return input;
}

AlphaInput getAlphaInput(Target target) {
    AlphaInput input;
    if (target && gAutoMeshAlphaInputCache) {
        const auto key = target->getUUID();
        const auto cached = compat::ngRequireObject(gAutoMeshAlphaInputCache)->findRetained(key);
        if (cached) return *cached;
        return input;
    }
    if (auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(target)) {
        const auto textures = part->textures;
        if (textures.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, textures.size(),
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/vertex/automesh/common.d"), 60);
        const auto texture = textures[0];
        if (!texture) return input;
        const auto data = texture->getSourceTextureData();
        input.img = ngCreateImageDeferred<>([&] { return texture->width(); },
            [&] { return texture->height(); }, [] { return ImageFormat::IF_RGB_ALPHA; });
        input.w = texture->width();
        input.h = texture->height();
        clearImage(input);
        auto bytes = input.img->data();
        if (data.size() == bytes.size()) {
            if (!bytes.empty()) std::copy(data.begin(), data.end(), bytes.begin());
        } else {
            const auto count = pixelCount(input);
            for (std::int32_t index = 0; index < count; ++index) {
                const auto offset = alphaOffset(index);
                auto& destination = byteAt(bytes, offset, 71);
                destination = byteAt(data, offset, 71);
            }
        }
        input.fromProvider = false;
    } else {
        const auto provider = DeformableAlphaProvider::createSource(target);
        const auto populate = [&] {
            input.w = provider->width();
            input.h = provider->height();
            if (input.w <= 0 || input.h <= 0) return;
            input.img = ngCreateImage<>(input.w, input.h, ImageFormat::IF_RGB_ALPHA);
            clearImage(input);
            const auto* alpha = provider->alphaPtr();
            copyAlpha(input, alpha, 83);
            const auto bounds = provider->boundsWorld();
            input.providerWorldCenter = {bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f};
            input.fromProvider = true;
        };
        compat::ngRunWithScopeExit(populate, [&] { provider->dispose(); });
    }
    return input;
}

Vec2 alphaImageCenter(AlphaInput input) {
    return {static_cast<float>(input.w / 2), static_cast<float>(input.h / 2)};
}

void mapImageCenteredMeshToTargetLocal(Mesh& mesh, Target target, AlphaInput input) {
    if (input.fromProvider) {
        const auto inverse = compat::ngRequireObject(target)->transform().matrix().inverse();
        const auto vertices = compat::ngRequireObject(mesh)->vertices;
        for (auto* vertex : vertices) {
            compat::ngRequireObject(vertex);
            const Vec2 world{vertex->position.x + input.providerWorldCenter.x,
                vertex->position.y + input.providerWorldCenter.y};
            vertex->position = projectPosition(inverse, world);
        }
    } else if (auto projectable = std::dynamic_pointer_cast<nicxlive::core::nodes::Projectable>(target)) {
        const auto vertices = compat::ngRequireObject(mesh)->vertices;
        for (auto* vertex : vertices) {
            compat::ngRequireObject(vertex);
            vertex->position.x += projectable->textureOffset.x;
            vertex->position.y += projectable->textureOffset.y;
        }
    }
}

} // namespace nijigenerate::viewport::vertex::automesh::common
