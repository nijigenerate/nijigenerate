// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/io/psd/parser.hpp"

namespace nijigenerate::io::psd {
const BlendingMode BlendingMode::PassThrough{compat::RetainedString::fromStaticLiteral("pass")};
const BlendingMode BlendingMode::Normal{compat::RetainedString::fromStaticLiteral("norm")};
const BlendingMode BlendingMode::Dissolve{compat::RetainedString::fromStaticLiteral("diss")};
const BlendingMode BlendingMode::Darken{compat::RetainedString::fromStaticLiteral("dark")};
const BlendingMode BlendingMode::Multiply{compat::RetainedString::fromStaticLiteral("mul ")};
const BlendingMode BlendingMode::ColorBurn{compat::RetainedString::fromStaticLiteral("idiv")};
const BlendingMode BlendingMode::LinearBurn{compat::RetainedString::fromStaticLiteral("lbrn")};
const BlendingMode BlendingMode::DarkerColor{compat::RetainedString::fromStaticLiteral("dkCl")};
const BlendingMode BlendingMode::Lighten{compat::RetainedString::fromStaticLiteral("lite")};
const BlendingMode BlendingMode::Screen{compat::RetainedString::fromStaticLiteral("scrn")};
const BlendingMode BlendingMode::ColorDodge{compat::RetainedString::fromStaticLiteral("div ")};
const BlendingMode BlendingMode::LinearDodge{compat::RetainedString::fromStaticLiteral("lddg")};
const BlendingMode BlendingMode::LighterColor{compat::RetainedString::fromStaticLiteral("lgCl")};
const BlendingMode BlendingMode::Overlay{compat::RetainedString::fromStaticLiteral("over")};
const BlendingMode BlendingMode::SoftLight{compat::RetainedString::fromStaticLiteral("sLit")};
const BlendingMode BlendingMode::HardLight{compat::RetainedString::fromStaticLiteral("hLit")};
const BlendingMode BlendingMode::VividLight{compat::RetainedString::fromStaticLiteral("vLit")};
const BlendingMode BlendingMode::LinearLight{compat::RetainedString::fromStaticLiteral("lLit")};
const BlendingMode BlendingMode::PinLight{compat::RetainedString::fromStaticLiteral("pLit")};
const BlendingMode BlendingMode::HardMix{compat::RetainedString::fromStaticLiteral("hMix")};
const BlendingMode BlendingMode::Difference{compat::RetainedString::fromStaticLiteral("diff")};
const BlendingMode BlendingMode::Exclusion{compat::RetainedString::fromStaticLiteral("smud")};
const BlendingMode BlendingMode::Subtract{compat::RetainedString::fromStaticLiteral("fsub")};
const BlendingMode BlendingMode::Divide{compat::RetainedString::fromStaticLiteral("fdiv")};
const BlendingMode BlendingMode::Hue{compat::RetainedString::fromStaticLiteral("hue ")};
const BlendingMode BlendingMode::Saturation{compat::RetainedString::fromStaticLiteral("sat ")};
const BlendingMode BlendingMode::Color{compat::RetainedString::fromStaticLiteral("colr")};
const BlendingMode BlendingMode::Luminosity{compat::RetainedString::fromStaticLiteral("lum ")};
void Layer::extractLayerImage() { extractLayer(*this); }
} // namespace nijigenerate::io::psd
