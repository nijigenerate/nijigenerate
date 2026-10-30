#pragma once

// Adapted from independent Phobos std.uni graphemeTransforms/getWidth.
// D Language Foundation, Boost 1.0; included data/layout retains the license.
#include "typed_format_grapheme_data.hpp"
#include <nijigenerate/compat/utf8.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <algorithm>

namespace nijigenerate::core::translation::detail {

enum class FormatGraphemeState { Start, CR, RI, L, V, LVT, Emoji, EmojiZWJ, Prepend, End };
enum class FormatGraphemeStep { Advance, Include, Exclude, Redo };

inline FormatGraphemeStep stepFormatGrapheme(FormatGraphemeState& state, char32_t ch) {
    using State = FormatGraphemeState;
    using Step = FormatGraphemeStep;
    const auto flags = formatGraphemeProperties(ch);
    const bool ri = ch >= 0x1f1e6 && ch <= 0x1f1ff;
    const bool l = (ch >= 4352 && ch < 4448) || (ch >= 43360 && ch < 43389);
    const bool v = (ch >= 4448 && ch < 4520) || (ch >= 55216 && ch < 55239);
    const bool t = (ch >= 4520 && ch < 4608) || (ch >= 55243 && ch < 55292);
    switch (state) {
    case State::Start:
        if ((flags & formatControl) || ch == '\n') return Step::Include;
        state = ch == '\r' ? State::CR : ri ? State::RI : l ? State::L :
            (flags & formatHangulLV) || v ? State::V : (flags & formatHangulLVT) || t ? State::LVT :
            (flags & formatPrepend) ? State::Prepend : (flags & formatPictographic) ? State::Emoji : State::End;
        return Step::Advance;
    case State::CR: return ch == '\n' ? Step::Include : Step::Exclude;
    case State::RI:
        state = State::End;
        return ri ? Step::Advance : Step::Redo;
    case State::L:
        if (l) return Step::Advance;
        if (v || (flags & formatHangulLV)) { state = State::V; return Step::Advance; }
        if (flags & formatHangulLVT) { state = State::LVT; return Step::Advance; }
        state = State::End;
        return Step::Redo;
    case State::V:
        if (v) return Step::Advance;
        if (t) { state = State::LVT; return Step::Advance; }
        state = State::End;
        return Step::Redo;
    case State::LVT:
        if (t) return Step::Advance;
        state = State::End;
        return Step::Redo;
    case State::Emoji:
        if (flags & formatExtend) return Step::Advance;
        if (ch == 0x200d) { state = State::EmojiZWJ; return Step::Advance; }
        state = State::End;
        return Step::Redo;
    case State::EmojiZWJ:
        state = State::Emoji;
        return (flags & formatPictographic) ? Step::Advance : Step::Redo;
    case State::Prepend:
        if ((flags & formatControl) || ch == '\r' || ch == '\n') return Step::Exclude;
        state = State::Start;
        return Step::Redo;
    case State::End:
        return !(flags & formatExtend) && !(flags & formatSpacingMark) && ch != 0x200d ?
            Step::Exclude : Step::Advance;
    }
    throw std::logic_error("Unknown grapheme state");
}

inline std::size_t formatDisplayWidth(std::string_view text) {
    // Preserve the source ASCII shortcut, including its CR/LF behavior.
    if (std::all_of(text.begin(), text.end(), [](unsigned char ch) { return ch <= 0x7f; }))
        return text.size();
    compat::phobos::detail::UnicodeCursor<std::string_view,char> cursor(text);
    std::size_t width = 0;
    while (!cursor.empty()) {
        ++width;
        auto state = FormatGraphemeState::Start;
        bool finished = false;
        while (!finished && !cursor.empty()) {
            const auto scalar = cursor.front();
            // Source redo reuses this front without another decode.
            bool redo;
            do {
                redo = false;
                switch (stepFormatGrapheme(state, scalar)) {
                case FormatGraphemeStep::Advance: cursor.popFront(); break;
                case FormatGraphemeStep::Include: cursor.popFront(); finished = true; break;
                case FormatGraphemeStep::Exclude: finished = true; break;
                case FormatGraphemeStep::Redo: redo = true; break;
                }
            } while (redo);
        }
    }
    return width;
}

} // namespace nijigenerate::core::translation::detail
