#include "font_add_data.hpp"
#include <nijigenerate/core/font_init_state.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace nijigenerate::core::font::detail {

void _incAddFontData(compat::RetainedString name, compat::Slice<std::uint8_t>& data,
                     float size, const ImWchar* ranges, ImVec2 offset) {
    static_assert(sizeof(int) == sizeof(std::int32_t) && std::numeric_limits<int>::digits == 31);
    auto* cfg = IM_NEW(ImFontConfig)();
    cfg->FontBuilderFlags = 1 << 9;
    cfg->FontDataOwnedByAtlas = false;
    cfg->MergeMode = !compat::ngRequireObject(atlas)->Fonts.empty();
    cfg->GlyphOffset = offset;
    cfg->OversampleH = 3;
    cfg->OversampleV = 2;

    std::array<char, 40> nameData;
    std::memset(nameData.data(), 0xff, nameData.size());
    if (name.size() > nameData.size()) {
        throw compat::ArraySliceError(0, name.size(), nameData.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/font.d"), 38);
    }
    if (!name.empty()) std::memcpy(nameData.data(), name.data(), name.size());
    static_assert(sizeof(cfg->Name) == 40);
    std::memcpy(cfg->Name, nameData.data(), nameData.size());

    const auto narrowed = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(data.size()));
    compat::ngRequireObject(atlas)->AddFontFromMemoryTTF(static_cast<void*>(data.data()), static_cast<int>(narrowed),
                              size, cfg, ranges);
}

} // namespace nijigenerate::core::font::detail
