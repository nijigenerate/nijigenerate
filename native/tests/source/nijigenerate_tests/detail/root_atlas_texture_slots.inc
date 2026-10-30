// Faithful body of regression.d2287-2348; include once outside namespaces.
#include "root_atlas_texture_slots.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/atlas/packer.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/part.hpp>
#include <texture.hpp>
#include <cstddef>
#include <memory>

namespace nijigenerate_tests::regression::detail {
bool rectsOverlap(nicxlive::core::math::Vec4i a, nicxlive::core::math::Vec4i b);

void testRenderAtlasPackerTextureSlots() {
    using nijigenerate::atlas::packer::TexturePacker;
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::SourceTextureBytes;
    using nicxlive::core::Texture;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec2i;
    using nicxlive::core::math::Vec4i;
    using nicxlive::core::nodes::TextureUsage;
    resetCase();

    auto packer = std::allocate_shared<TexturePacker>(SourceAllocator<TexturePacker>{}, Vec2i{64, 64});
    auto a = packer->packTexture(Vec2i{16, 12});
    auto b = packer->packTexture(Vec2i{24, 18});
    auto c = packer->packTexture(Vec2i{8, 8});
    require(a.z == 16 && a.w == 12,
        RetainedString::fromStaticLiteral("render atlas packer should preserve first allocation size"));
    require(b.z == 24 && b.w == 18,
        RetainedString::fromStaticLiteral("render atlas packer should preserve second allocation size"));
    require(c.z == 8 && c.w == 8,
        RetainedString::fromStaticLiteral("render atlas packer should preserve third allocation size"));
    require(!rectsOverlap(a, b) && !rectsOverlap(a, c) && !rectsOverlap(b, c),
        RetainedString::fromStaticLiteral("render atlas packer should not overlap live allocations"));
    packer->remove(b);
    auto reused = packer->packTexture(Vec2i{24, 18});
    require(reused.z == 24 && reused.w == 18, RetainedString::fromStaticLiteral(
        "render atlas packer should accept a same-sized allocation after removal"));
    packer->clear();
    require(packer->packTexture(Vec2i{64, 64}) == Vec4i{0, 0, 64, 64},
        RetainedString::fromStaticLiteral("render atlas packer clear should invalidate all previous allocations"));

    auto sharedTexture = Texture::createDataSourceDeferred(
        [] { return SourceTextureBytes{255, 0, 0, 255}; }, [] { return 1; }, [] { return 1; },
        [] { return 4; }, [] { return 4; }, [] { return false; }, [] { return false; });
    auto replacementTexture = Texture::createDataSourceDeferred(
        [] { return SourceTextureBytes{0, 255, 0, 255}; }, [] { return 1; }, [] { return 1; },
        [] { return 4; }, [] { return 4; }, [] { return false; }, [] { return false; });
    auto extraTexture = Texture::createDataSourceDeferred(
        [] { return SourceTextureBytes{0, 0, 255, 255}; }, [] { return 1; }, [] { return 1; },
        [] { return 4; }, [] { return 4; }, [] { return false; }, [] { return false; });
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto partA = newMeshPart(RetainedString::fromStaticLiteral("atlas-slot-a"));
        auto partB = newMeshPart(RetainedString::fromStaticLiteral("atlas-slot-b"));
        constexpr auto albedo = static_cast<std::size_t>(TextureUsage::Albedo);
        constexpr auto emissive = static_cast<std::size_t>(TextureUsage::Emissive);
        auto originalA = partA->textures[albedo];
        auto originalB = partB->textures[albedo];
        nijigenerate::compat::ngRunWithScopeExit([&] {
            originalA->dispose();
            originalB->dispose();
            originalA.reset();
            originalB.reset();
            partA->textures[albedo] = sharedTexture;
            partB->textures[albedo] = sharedTexture;
            nijigenerate::project::incActivePuppet()->populateTextureSlots();
            require(nijigenerate::project::incActivePuppet()->textureSlots.size() == 1,
                RetainedString::fromStaticLiteral("texture slot population should deduplicate shared texture instances"));
            require(nijigenerate::project::incActivePuppet()->getTextureSlotIndexFor(sharedTexture) == 0,
                RetainedString::fromStaticLiteral("shared texture should be discoverable in slot zero"));

            partB->textures[albedo] = replacementTexture;
            nijigenerate::project::incActivePuppet()->populateTextureSlots();
            require(nijigenerate::project::incActivePuppet()->textureSlots.size() == 2,
                RetainedString::fromStaticLiteral("texture slot population should include replacement texture"));
            require(nijigenerate::project::incActivePuppet()->getTextureSlotIndexFor(sharedTexture) >= 0,
                RetainedString::fromStaticLiteral("original shared texture should remain while referenced"));
            require(nijigenerate::project::incActivePuppet()->getTextureSlotIndexFor(replacementTexture) >= 0,
                RetainedString::fromStaticLiteral("replacement texture should receive a slot"));

            partA->textures[emissive] = extraTexture;
            nijigenerate::project::incActivePuppet()->populateTextureSlots();
            require(nijigenerate::project::incActivePuppet()->textureSlots.size() == 3,
                RetainedString::fromStaticLiteral("texture slot population should include auxiliary usage textures"));
            partA->textures[albedo].reset();
            nijigenerate::project::incActivePuppet()->populateTextureSlots();
            require(nijigenerate::project::incActivePuppet()->getTextureSlotIndexFor(sharedTexture) < 0,
                RetainedString::fromStaticLiteral("texture slot rebuild should invalidate unreferenced textures"));
            require(nijigenerate::project::incActivePuppet()->textureSlots.size() == 2,
                RetainedString::fromStaticLiteral("texture slot rebuild should keep only currently referenced textures"));
        }, [&] {
            for (auto& slot : partA->textures) slot.reset();
            for (auto& slot : partB->textures) slot.reset();
            if (originalA) originalA->dispose();
            if (originalB) originalB->dispose();
        });
    }, [&] {
        sharedTexture->dispose();
        replacementTexture->dispose();
        extraTexture->dispose();
    });
}
} // namespace nijigenerate_tests::regression::detail
