// Faithful body of regression.d3102-3168; include once outside namespaces.
#include "root_texture_reload.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_rmdir.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/part.hpp>
#include <texture.hpp>
#include <array>
#include <cstddef>
#include <cstdint>

namespace nijigenerate_tests::regression::detail {
void writeRegressionPng(nijigenerate::compat::RetainedString path, std::uint8_t r, std::uint8_t g,
    std::uint8_t b, std::int32_t width, std::int32_t height, std::uint8_t a);

void testPartTextureReloadFixture() {
    using namespace nijigenerate::compat::phobos;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ThrowableException;
    using nicxlive::core::ShallowTexture;
    using nicxlive::core::Texture;
    using nicxlive::core::nodes::TextureUsage;
    resetCase();

    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-texture-reload")};
    const auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array albedoSegments{fixtureDir, RetainedString::fromStaticLiteral("albedo.png")};
        const auto albedoPath = ngBuildPath(albedoSegments);
        const std::array replacementSegments{fixtureDir, RetainedString::fromStaticLiteral("replacement.png")};
        const auto replacementPath = ngBuildPath(replacementSegments);
        const std::array emissiveSegments{fixtureDir, RetainedString::fromStaticLiteral("emissive.png")};
        const auto emissivePath = ngBuildPath(emissiveSegments);
        const std::array mismatchSegments{fixtureDir, RetainedString::fromStaticLiteral("mismatched.png")};
        const auto mismatchedPath = ngBuildPath(mismatchSegments);
        writeRegressionPng(albedoPath, 255, 0, 0, 2, 2, 255);
        writeRegressionPng(replacementPath, 0, 255, 0, 4, 3, 255);
        writeRegressionPng(emissivePath, 0, 0, 255, 4, 3, 255);
        writeRegressionPng(mismatchedPath, 0, 0, 0, 1, 1, 255);

        auto part = newMeshPart(RetainedString::fromStaticLiteral("texture-reload-part"));
        nijigenerate::project::incActivePuppet()->populateTextureSlots();
        require(nijigenerate::project::incActivePuppet()->textureSlots.size() == 1,
            RetainedString::fromStaticLiteral("initial Part fixture should populate one texture slot"));
        constexpr auto albedo = static_cast<std::size_t>(TextureUsage::Albedo);
        constexpr auto emissiveUsage = static_cast<std::size_t>(TextureUsage::Emissive);
        auto oldTexture = part->textures[albedo];
        nijigenerate::compat::ngRunWithScopeExit([&] {
            auto replacement = ShallowTexture(replacementPath, 4);
            part->textures[albedo] = Texture::createSourceDeferred([&] { return replacement; });
            nijigenerate::project::incActivePuppet()->rescanNodes();
            nijigenerate::project::incActivePuppet()->populateTextureSlots();
            nijigenerate::project::incActivePuppet()->updateTextureState();
            require(part->textures[albedo] != oldTexture,
                RetainedString::fromStaticLiteral("texture reload should replace the Part albedo texture instance"));
            require(part->textures[albedo]->width() == 4 && part->textures[albedo]->height() == 3,
                RetainedString::fromStaticLiteral("texture reload should use dimensions from the replacement image"));
            require(nijigenerate::project::incActivePuppet()->textureSlots.size() == 1 &&
                nijigenerate::project::incActivePuppet()->textureSlots[0] == part->textures[albedo],
                RetainedString::fromStaticLiteral(
                    "texture reload should repopulate puppet texture slots with the replacement texture"));

            auto emissive = ShallowTexture(emissivePath, 3);
            require(emissive.width == part->textures[albedo]->width() &&
                emissive.height == part->textures[albedo]->height(),
                RetainedString::fromStaticLiteral(
                    "matching auxiliary texture fixture should be accepted by size validation"));
            part->textures[emissiveUsage] = Texture::createSourceDeferred([&] { return emissive; });
            nijigenerate::project::incActivePuppet()->populateTextureSlots();
            require(nijigenerate::project::incActivePuppet()->textureSlots.size() == 2,
                RetainedString::fromStaticLiteral(
                    "adding a matching emissive texture should add another texture slot"));

            auto mismatched = ShallowTexture(mismatchedPath, 3);
            require(mismatched.width != part->textures[albedo]->width() ||
                mismatched.height != part->textures[albedo]->height(),
                RetainedString::fromStaticLiteral(
                    "mismatched auxiliary texture fixture should be rejected by size validation"));
            bool missingRejected = false;
            try {
                const std::array missingSegments{fixtureDir, RetainedString::fromStaticLiteral("missing.png")};
                auto missing = ShallowTexture(ngBuildPath(missingSegments), 4);
                [[maybe_unused]] auto unused = Texture::createSourceDeferred([&] { return missing; });
            } catch (const ThrowableException&) {
                missingRejected = true;
            }
            require(missingRejected, RetainedString::fromStaticLiteral(
                "loading a missing texture source should throw instead of replacing the texture slot"));
        }, [&] {
            for (const auto texture : part->textures) {
                if (texture) texture->dispose();
            }
            if (oldTexture) oldTexture->dispose();
        });
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}
} // namespace nijigenerate_tests::regression::detail
