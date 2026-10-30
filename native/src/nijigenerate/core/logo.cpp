#include "nijigenerate/core/logo.hpp"
#include "nijigenerate/resources/embedded.hpp"
#include <texture.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::core {
namespace {
// Unshared D module variables have thread-local storage.
thread_local LogoTextureHandle logoI2D;
thread_local LogoTextureHandle logo;
thread_local LogoTextureHandle ada;
thread_local LogoTextureHandle grid;

nicxlive::core::ShallowTexture decode(std::string_view name) {
    const auto resource = resources::ngEmbeddedResource(name);
    if (!resource) throw std::runtime_error("Missing embedded logo resource");
    return nicxlive::core::ShallowTexture(std::span<const std::uint8_t>{
        reinterpret_cast<const std::uint8_t*>(resource->data()), resource->size()});
}
}

void ngInitLogo() {
    // Assignment is deliberately sequential: later failure preserves earlier replacements.
    auto texture = decode("logo.png");
    nicxlive::core::inTexPremultiply(texture.data);
    logoI2D = nicxlive::core::Texture::createSource(texture);
    texture = decode("icon.png");
    nicxlive::core::inTexPremultiply(texture.data);
    logo = nicxlive::core::Texture::createSource(texture);
    texture = decode("ui/ui-ada.png");
    nicxlive::core::inTexPremultiply(texture.data);
    ada = nicxlive::core::Texture::createSource(texture);
    texture = decode("ui/grid.png");
    nicxlive::core::inTexPremultiply(texture.data);
    grid = nicxlive::core::Texture::createSource(texture);
    {
        const auto target = compat::ngRequireObject(grid);
        target->setFiltering(nicxlive::core::Filtering::Nearest);
    }
    {
        const auto target = compat::ngRequireObject(grid);
        target->setWrapping(nicxlive::core::Wrapping::Repeat);
    }
}
LogoTextureHandle ngGetLogo() { return logo; }
LogoTextureHandle ngGetLogoI2D() { return logoI2D; }
LogoTextureHandle ngGetAda() { return ada; }
LogoTextureHandle ngGetGrid() { return grid; }
}
