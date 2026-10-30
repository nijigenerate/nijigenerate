/*
    Markdown support, declaration port from the independently authored D module.
    This is an altered source version, not the original imgui_markdown library.

    Copyright (c) 2019 Juliette Foucaut and Doug Binks

    This software is provided 'as-is', without any express or implied
    warranty. In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgement in the product documentation would be
    appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/
#pragma once

#include <nijigenerate/core/typed_format_enum.hpp>
#include "nijigenerate/compat/retained_string_slice.hpp"
#include <compat/object_metadata.hpp>
#include <imgui.h>
#include <array>
#include <cstdint>

namespace nijigenerate::widgets::markdown {
struct MarkdownLinkCallbackData {
    compat::RetainedStringSlice text{};
    compat::RetainedStringSlice link{};
    void* userData = nullptr;
    bool isImage = false;
};
struct MarkdownTooltipCallbackData {
    MarkdownLinkCallbackData linkData{};
    compat::RetainedStringSlice linkIcon{};
};
struct MarkdownImageData {
    bool isValid = false;
    bool useLinkCallback = false;
    ImTextureID userTextureId = 0;
    ImVec2 size{100.0f, 100.0f};
    ImVec2 uv0{0.0f, 0.0f};
    ImVec2 uv1{1.0f, 1.0f};
    ImVec4 tint_col{1.0f, 1.0f, 1.0f, 1.0f};
    ImVec4 border_col{0.0f, 0.0f, 0.0f, 0.0f};
};
enum class MarkdownFormatType : std::int32_t {
    NormalText = 0,
    Heading = 1,
    UnorderedList = 2,
    Link = 3,
    Emphasis = 4,
};
struct MarkdownFormatInfo;
struct MarkdownHeadingFormat;
struct MarkdownConfig;

using MarkdownLinkCallback = void(*)(MarkdownLinkCallbackData data);
using MarkdownImageCallback = MarkdownImageData(*)(MarkdownLinkCallbackData data);
using MarkdownTooltipCallback = void(*)(MarkdownTooltipCallbackData data);
using MarkdownFormatCallback = void(*)(MarkdownFormatInfo& info, bool start);
using incMarkdownLinkCallback = MarkdownLinkCallback;
using incMarkdownImageCallback = MarkdownImageCallback;
using incMarkdownTooltipCallback = MarkdownTooltipCallback;
using incMarkdownFormatCallback = MarkdownFormatCallback;

namespace detail {
// The default address is real; its body is accepted separately from fields.
void formatDefault(MarkdownFormatInfo& info, bool start);
}

struct MarkdownHeadingFormat {
    float scale = 1.0f;
    bool separator = true;
};

struct MarkdownConfig {
    static constexpr std::int32_t NUMHEADINGS = 3;
    MarkdownLinkCallback linkCallback = nullptr;
    MarkdownTooltipCallback tooltipCallback = nullptr;
    MarkdownImageCallback imageCallback = nullptr;
    compat::RetainedStringSlice linkIcon{compat::RetainedString::emptyLiteral()};
    std::array<MarkdownHeadingFormat, NUMHEADINGS> headingFormats{};
    void* userData = nullptr;
    MarkdownFormatCallback formatCallback = &detail::formatDefault;
};

struct MarkdownFormatInfo {
    MarkdownFormatType type = MarkdownFormatType::NormalText;
    std::int32_t level = 0;
    bool itemHovered = false;
    MarkdownConfig config;
};

// The implementation retains the input slices for callbacks and per-thread hover state.
void ngMarkdown(compat::RetainedStringSlice text, MarkdownConfig& config);
} // namespace nijigenerate::widgets::markdown

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::widgets::markdown::MarkdownFormatType> {
    using Enum = ::nijigenerate::widgets::markdown::MarkdownFormatType;
    inline static constexpr std::string_view name = "MarkdownFormatType";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::NormalText, "NormalText"},
        FormatEnumMember<Enum>{Enum::Heading, "Heading"},
        FormatEnumMember<Enum>{Enum::UnorderedList, "UnorderedList"},
        FormatEnumMember<Enum>{Enum::Link, "Link"},
        FormatEnumMember<Enum>{Enum::Emphasis, "Emphasis"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
#define NG_MARKDOWN_VALUE_SOURCE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::widgets::markdown::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.markdown." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.markdown." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_MARKDOWN_VALUE_SOURCE_ORIGIN(MarkdownFormatType);
NG_MARKDOWN_VALUE_SOURCE_ORIGIN(MarkdownLinkCallbackData);
NG_MARKDOWN_VALUE_SOURCE_ORIGIN(MarkdownTooltipCallbackData);
NG_MARKDOWN_VALUE_SOURCE_ORIGIN(MarkdownImageData);
NG_MARKDOWN_VALUE_SOURCE_ORIGIN(MarkdownFormatInfo);
NG_MARKDOWN_VALUE_SOURCE_ORIGIN(MarkdownHeadingFormat);
NG_MARKDOWN_VALUE_SOURCE_ORIGIN(MarkdownConfig);
#undef NG_MARKDOWN_VALUE_SOURCE_ORIGIN
} // namespace nicxlive::core::d_runtime
