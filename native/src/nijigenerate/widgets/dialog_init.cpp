#include "nijigenerate/widgets/dialog.hpp"
#include "nijigenerate/widgets/detail/dialog_state.hpp"
#include "nijigenerate/resources/embedded.hpp"

#include <texture.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <span>
#include <string_view>

namespace nijigenerate::widgets::dialog {
namespace {
nicxlive::core::ShallowTexture decode(std::string_view name) {
    const auto bytes = resources::ngEmbeddedResource(name);
    if (!bytes) throw compat::ThrowableException(
        compat::RetainedString::fromStaticLiteral("Missing embedded dialog image"));
    nicxlive::core::ShallowTexture result(std::span<const std::uint8_t>(bytes->data(), bytes->size()));
    nicxlive::core::inTexPremultiply(result.data);
    return result;
}
}

void ngInitDialogs() {
    const auto info = decode("ui/ui-info.png");
    const auto warning = decode("ui/ui-warning.png");
    const auto error = decode("ui/ui-error.png");
    detail::adaTextures = compat::Slice<std::shared_ptr<nicxlive::core::Texture>>{
        nicxlive::core::Texture::createSource(info),
        nicxlive::core::Texture::createSource(warning),
        nicxlive::core::Texture::createSource(error),
    };
}
} // namespace nijigenerate::widgets::dialog
