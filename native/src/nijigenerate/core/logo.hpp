#pragma once
#include <memory>

namespace nicxlive::core { class Texture; }
namespace nijigenerate::core {
using LogoTextureHandle = std::shared_ptr<nicxlive::core::Texture>;
void ngInitLogo();
LogoTextureHandle ngGetLogo();
LogoTextureHandle ngGetLogoI2D();
LogoTextureHandle ngGetAda();
LogoTextureHandle ngGetGrid();
}
