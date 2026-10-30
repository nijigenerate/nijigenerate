// Altered independent D Markdown port; the original zlib notice is in markdown.hpp.
#include "nijigenerate/widgets/markdown.hpp"
#include "nijigenerate/widgets/markdown_internal.hpp"
#include "nijigenerate/widgets/dummy.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include <bit>
#include <cstdint>

namespace nijigenerate::widgets::markdown {
namespace {
using Slice = compat::RetainedStringSlice;
using Index = std::int32_t;
Index add(Index a, Index b) {
    return std::bit_cast<Index>(std::uint32_t(a) + std::uint32_t(b));
}
Index narrow(std::size_t value) { return std::bit_cast<Index>(static_cast<std::uint32_t>(value)); }
Slice range(Slice text, std::size_t first, std::size_t last, std::size_t sourceLine) {
    if (first > last || last > text.size())
        throw compat::ngStaticError<compat::ArraySliceError>(
            first, last, text.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/markdown.d"), sourceLine);
    return text.slice(first, last);
}
char at(Slice text, Index index, std::size_t sourceLine) {
    if (static_cast<std::size_t>(index) >= text.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(
            static_cast<std::size_t>(index), text.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/markdown.d"), sourceLine);
    return text.data()[index];
}
struct Line {
    bool isHeading = false, isEmphasis = false, isUnorderedListStart = false, isLeadingSpace = true;
    Index leadSpaceCount = 0, headingCount = 0, emphasisCount = 0;
    Index lineStart = 0, lineEnd = 0, lastRenderPosition = 0;
};
struct Link {
    enum class State : std::int32_t {
        NoLink, HasSquareBracketOpen, HasSquareBrackets, HasSquareBracketsRoundBracketOpen
    };
    State state = State::NoLink;
    Slice text{}, url{};
    bool isImage = false;
    Index numBracketsOpen = 0;
};
struct Emphasis {
    enum class State : std::int32_t { None, Left, Middle, Right };
    State state = State::None;
    Slice text{};
    std::uint8_t sym = 0xff; // Preserve the unsigned D char code-unit value and char.init bits.
};
class TextRegion {
    float indentX = 0;
public:
    void renderTextWrapped(Slice text, bool indentToHere) {
        const char* begin = text.data();
        const char* end = begin ? begin + text.size() : begin;
        const float scale = ImGui::GetIO().FontGlobalScale;
        float widthLeft = ngAvailableSpace().x;
        // The D source discards the wrap-position result. Preserve its full-slice rendering.
        ImGui::GetFont()->CalcWordWrapPositionA(scale, begin, end, widthLeft);
        ImGui::TextUnformatted(begin, end);
        if (indentToHere) {
            const float needed = ngAvailableSpace().x - widthLeft;
            if (needed) { ImGui::Indent(needed); indentX += needed; }
        }
        widthLeft = ngAvailableSpace().x;
        // sliceEnd already equals the complete slice end, so the D continuation loop never executes.
        (void)widthLeft;
    }
    void renderListTextWrapped(Slice text) {
        ImGui::Bullet(); ImGui::SameLine(); renderTextWrapped(text, true);
    }
    bool renderLinkText(Slice text, Link& link, Slice markdown, MarkdownConfig& cfg, Slice& hover) {
        hover.reset(); // D out parameter initialization occurs on every call.
        (void)markdown;
        MarkdownFormatInfo info;
        info.config = cfg; info.type = MarkdownFormatType::Link;
        cfg.formatCallback(info, true);
        ImGui::PushTextWrapPos(-1);
        const char* begin = text.data();
        const char* end = begin ? begin + text.size() : begin;
        ImGui::TextUnformatted(begin, end);
        ImGui::PopTextWrapPos();
        const bool itemHovered = ImGui::IsItemHovered();
        if (itemHovered) hover = link.text;
        const bool hovered = itemHovered || hover == link.text;
        info.itemHovered = hovered;
        cfg.formatCallback(info, false);
        if (hovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && cfg.linkCallback)
                cfg.linkCallback({link.text, link.url, cfg.userData, false});
            if (cfg.tooltipCallback) cfg.tooltipCallback({{link.text, link.url, cfg.userData, false}, cfg.linkIcon});
        }
        return itemHovered;
    }
    void renderLinkTextWrapped(Slice text, Link& link, Slice markdown, MarkdownConfig& cfg,
                               Slice& hover, bool indentToHere) {
        hover.reset(); // D out, not ref.
        const char* begin = text.data();
        const char* end = begin ? begin + text.size() : begin;
        const float scale = ImGui::GetIO().FontGlobalScale;
        float widthLeft = ngAvailableSpace().x;
        ImGui::GetFont()->CalcWordWrapPositionA(scale, begin, end, widthLeft);
        const bool hovered = renderLinkText(text, link, markdown, cfg, hover);
        if (indentToHere) {
            const float needed = ngAvailableSpace().x - widthLeft;
            if (needed) { ImGui::Indent(needed); indentX += needed; }
        }
        widthLeft = ngAvailableSpace().x;
        (void)widthLeft; // The source's continuation condition is false, as above.
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (!hovered && hover == link.text) hover.reset();
    }
    void resetIndent() {
        if (indentX > 0) ImGui::Unindent(indentX);
        indentX = 0;
    }
};
void renderLine(Slice markdown, Line& line, TextRegion& region, MarkdownConfig& cfg) {
    const Index indentStart = line.isUnorderedListStart ? 1 : 0;
    for (Index j = indentStart; j < line.leadSpaceCount / 2; j = add(j, 1)) ImGui::Indent();
    MarkdownFormatInfo info;
    info.config = cfg;
    const Index start = add(line.lastRenderPosition, 1);
    // D's wrapping start + (lineEnd - start) is exactly lineEnd.
    const Index end = line.lineEnd;
    if (line.isUnorderedListStart) {
        info.type = MarkdownFormatType::UnorderedList;
        cfg.formatCallback(info, true);
        region.renderListTextWrapped(range(markdown, add(start, 1), end, 337));
    } else if (line.isHeading || line.isEmphasis) {
        info.type = line.isHeading ? MarkdownFormatType::Heading : MarkdownFormatType::Emphasis;
        info.level = line.isHeading ? line.headingCount : line.emphasisCount;
        cfg.formatCallback(info, true);
        region.renderTextWrapped(range(markdown, add(start, 1), end, line.isHeading ? 343 : 349), false);
    } else {
        info.type = MarkdownFormatType::NormalText;
        cfg.formatCallback(info, true);
        region.renderTextWrapped(range(markdown, start, end, 354), false);
    }
    cfg.formatCallback(info, false);
    for (Index j = indentStart; j < line.leadSpaceCount / 2; j = add(j, 1)) ImGui::Unindent();
}
} // namespace

void ngMarkdown(Slice markdown, MarkdownConfig& cfg) {
    thread_local Slice linkHoverStart;
    (void)ImGui::GetStyle();
    Line line;
    Link link;
    Emphasis em;
    TextRegion region;
    char c = static_cast<char>(0xff);
    for (Index i = 0; static_cast<std::size_t>(i) < markdown.size(); i = add(i, 1)) {
        c = at(markdown, i, 373);
        if (line.isLeadingSpace) {
            if (c == ' ') { line.leadSpaceCount = add(line.leadSpaceCount, 1); continue; }
            line.isLeadingSpace = false;
            line.lastRenderPosition = add(i, -1);
            if (c == '*' && line.leadSpaceCount >= 1) {
                if (markdown.size() > static_cast<std::size_t>(add(i, 1)) && at(markdown, add(i, 1), 384) == ' ') {
                    line.isUnorderedListStart = true;
                    i = add(i, 1); line.lastRenderPosition = add(line.lastRenderPosition, 1);
                }
            } else if (c == '#') {
                line.headingCount = add(line.headingCount, 1);
                bool checking = true;
                Index j = i;
                while ((j = add(j, 1)), static_cast<std::size_t>(j) < markdown.size() && checking) {
                    c = at(markdown, j, 395);
                    switch (c) {
                    case '#': line.headingCount = add(line.headingCount, 1); break;
                    case ' ':
                        line.lastRenderPosition = add(j, -1); i = j;
                        line.isHeading = true; checking = false; break;
                    default: line.isHeading = false; checking = false; break;
                    }
                }
                if (line.isHeading) { em = {}; continue; }
            }
        }
        switch (link.state) {
        case Link::State::NoLink:
            if (c == '[' && !line.isHeading) {
                link.state = Link::State::HasSquareBracketOpen;
                link.text = range(markdown, static_cast<std::size_t>(add(i, 1)), markdown.size(), 426);
                if (i > 0 && at(markdown, add(i, -1), 427) == '!') link.isImage = true;
            }
            break;
        case Link::State::HasSquareBracketOpen:
            if (c == ']') {
                link.state = Link::State::HasSquareBrackets;
                link.text = range(markdown, static_cast<std::size_t>(link.text.data() - markdown.data()),
                                  static_cast<std::size_t>(i), 435);
            }
            break;
        case Link::State::HasSquareBrackets:
            if (c == '(') {
                link.state = Link::State::HasSquareBracketsRoundBracketOpen;
                link.url = range(markdown, static_cast<std::size_t>(add(i, 1)), markdown.size(), 441);
                link.numBracketsOpen = 1;
            }
            break;
        case Link::State::HasSquareBracketsRoundBracketOpen:
            if (c == '(') link.numBracketsOpen = add(link.numBracketsOpen, 1);
            else if (c == ')') link.numBracketsOpen = add(link.numBracketsOpen, -1);
            if (link.numBracketsOpen == 0) {
                em = {};
                line.lineEnd = add(narrow(static_cast<std::size_t>(link.text.data() - markdown.data())),
                                   link.isImage ? -2 : -1);
                renderLine(markdown, line, region, cfg);
                line.leadSpaceCount = 0;
                link.url = range(markdown, static_cast<std::size_t>(link.url.data() - markdown.data()),
                                 static_cast<std::size_t>(i), 457);
                line.isUnorderedListStart = false;
                ImGui::SameLine(0, 0);
                if (link.isImage) {
                    bool drawn = false;
                    [[maybe_unused]] bool useLinkCallback = false;
                    if (cfg.imageCallback) {
                        const auto image = cfg.imageCallback({link.text, link.url, cfg.userData, true});
                        // D reads useLinkCallback but does not gate the click callback on it.
                        useLinkCallback = image.useLinkCallback;
                        if (image.isValid) {
                            ImGui::Image(image.userTextureId, image.size, image.uv0, image.uv1,
                                         image.tint_col, image.border_col);
                            drawn = true;
                        }
                    }
                    if (!drawn) ImGui::Text("\xEB\xBB\xAF"); // Original fixed U+BEEF format, without arguments.
                    if (ImGui::IsItemHovered()) {
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && cfg.linkCallback)
                            cfg.linkCallback({link.text, link.url, cfg.userData, true});
                        if (cfg.tooltipCallback)
                            cfg.tooltipCallback({{link.text, link.url, cfg.userData, true}, cfg.linkIcon});
                    }
                } else region.renderLinkTextWrapped(link.text, link, markdown, cfg, linkHoverStart, false);
                ImGui::SameLine();
                link = {}; line.lastRenderPosition = i;
            }
            break;
        }
        // The source emphasis state switch has only a default arm.
        if (c == '\n') {
            line.lineEnd = i;
            if (em.state == Emphasis::State::Middle && line.emphasisCount >= 3 &&
                add(line.lineStart, line.emphasisCount) == i) ImGui::Separator();
            else renderLine(markdown, line, region, cfg);
            line = {}; em = {};
            line.lineStart = add(i, 1); line.lastRenderPosition = i;
            region.resetIndent(); link = {};
        }
    }
    if (em.state == Emphasis::State::Left && line.emphasisCount >= 3) ImGui::Separator();
    else if (markdown.size() && static_cast<std::size_t>(line.lineStart) < markdown.size() &&
             at(markdown, line.lineStart, 525) != 0) {
        line.lineEnd = narrow(markdown.size());
        if (at(markdown, add(line.lineEnd, -1), 527) == 0) line.lineEnd = add(line.lineEnd, -1);
        renderLine(markdown, line, region, cfg);
    }
}
} // namespace nijigenerate::widgets::markdown
