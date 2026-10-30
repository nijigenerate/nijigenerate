// Copyright (c) 2017 - 2025 Jonathan M Davis. Distributed under LICENSE.dxml.
// KRA's concrete parseDOM!simpleXML closure, adapted from dxml 0.4.5.
#include "xml.hpp"
#include "xml_graphical.hpp"
#include <nijigenerate/compat/phobos_index_sort.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <common/unicode_lowercase.hpp>
#include <algorithm>
#include <bit>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
namespace nijigenerate::io::kra::xml {
XMLParsingException::XMLParsingException(const char* message, TextPos position, std::size_t offset,
        std::source_location source)
    : XMLParsingException(compat::RetainedString::fromBorrowedView(message), position, offset,
          compat::RetainedString::fromBorrowedView(source.file_name()), source.line()) {}
XMLParsingException::XMLParsingException(compat::RetainedString message, TextPos position, std::size_t offset,
        compat::RetainedString sourceFile, std::size_t sourceLine)
    : ThrowableException(compat::RetainedString("[" + std::to_string(position.line) + ":" +
          std::to_string(position.col) + "]: " + std::string(message.view())),
          std::move(sourceFile), sourceLine),
      pos(position), offset(offset) {}
namespace {
bool space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
bool xmlChar(std::uint32_t c) {
    return c == 9 || c == 10 || c == 13 || (c >= 32 && c <= 0xd7ff) ||
        (c >= 0xe000 && c <= 0xfffd) || (c >= 0x10000 && c <= 0x10ffff);
}
bool nameStart(std::uint32_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == ':' || c == '_' ||
        (c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xf6) || (c >= 0xf8 && c <= 0x2ff) ||
        (c >= 0x370 && c <= 0x37d) || (c >= 0x37f && c <= 0x1fff) ||
        (c >= 0x200c && c <= 0x200d) || (c >= 0x2070 && c <= 0x218f) ||
        (c >= 0x2c00 && c <= 0x2fef) || (c >= 0x3001 && c <= 0xd7ff) ||
        (c >= 0xf900 && c <= 0xfdcf) || (c >= 0xfdf0 && c <= 0xfffd) ||
        (c >= 0x10000 && c <= 0xeffff);
}
bool nameChar(std::uint32_t c) {
    return nameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == 0xb7 ||
        (c >= 0x300 && c <= 0x36f) || (c >= 0x203f && c <= 0x2040);
}
class Parser {
    compat::RetainedString owner;
    std::string_view input;
    std::size_t pos{};
    TextPos positionAt(std::size_t offset) const {
        TextPos position;
        for (std::size_t i = 0; i < offset; ++i) {
            if (input[i] == '\n') {
                position.line = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(position.line) + 1u);
                position.col = 1;
            } else {
                position.col = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(position.col) + 1u);
            }
        }
        return position;
    }
    [[noreturn]] void fail(const char* message) const {
        throw XMLParsingException(message, positionAt(pos), pos);
    }
    [[noreturn]] void failAt(compat::RetainedString message, std::size_t offset,
            std::size_t sourceLine) const {
        throw XMLParsingException(std::move(message), positionAt(offset), offset,
            compat::RetainedString::fromStaticLiteral("dxml/parser.d"), sourceLine);
    }
    bool starts(std::string_view token) const { return input.substr(pos).starts_with(token); }
    void require(std::string_view token) {
        if (!starts(token)) fail("Unexpected XML token");
        pos += token.size();
    }
    bool whitespace() {
        auto begin = pos;
        while (pos < input.size() && space(input[pos])) ++pos;
        return pos != begin;
    }
    compat::RetainedString copy(std::size_t first, std::size_t last) const {
        return owner.subslice(first, last);
    }
    // dxml takeName uses Phobos decodeFront with UseReplacementDchar.yes.
    std::uint32_t decodeName(std::size_t& cursor, std::size_t limit) const {
        if (cursor == limit) fail("Unexpected end of XML name");
        nicxlive::core::common::unicode::ReplacementUtf8Cursor decoder{input.substr(cursor, limit - cursor)};
        const auto scalar = decoder.take();
        cursor += decoder.offset;
        return scalar;
    }
    [[noreturn]] void invalidName(std::uint32_t scalar, std::size_t begin,
            std::size_t end, std::size_t sourceLine) const {
        char digits[8];
        const auto formatted = std::to_chars(digits, digits + sizeof(digits), scalar, 16);
        std::string hex(digits, formatted.ptr);
        for (auto& digit : hex) if (digit >= 'a' && digit <= 'f') digit -= 'a' - 'A';
        if (hex.size() < 2) hex.insert(hex.begin(), '0');
        const auto shown = detail::isGraphical(scalar) ?
            "'" + std::string(input.substr(begin, end - begin)) + "' (0x" + hex + ")" : "0x" + hex;
        failAt(compat::RetainedString("Name contains invalid character: " + shown), begin, sourceLine);
    }
    compat::RetainedString name(char delimiter = 0,
            std::size_t limit = std::numeric_limits<std::size_t>::max()) {
        limit = (std::min)(limit, input.size());
        auto begin = pos, next = pos;
        const auto first = decodeName(next, limit);
        if (!nameStart(first)) invalidName(first, pos, next, 5821);
        pos = next;
        while (pos < limit) {
            if (space(input[pos]) || (delimiter && input[pos] == delimiter)) break;
            next = pos;
            const auto scalar = decodeName(next, limit);
            if (!nameChar(scalar)) invalidName(scalar, pos, next, 5846);
            pos = next;
        }
        return copy(begin, pos);
    }
    void reference(bool attribute) {
        const auto ampPosition = pos;
        const auto failReference = [&]() -> void {
            if (attribute) failAt(compat::RetainedString::fromStaticLiteral(
                "& is only legal in an attribute value as part of a reference, and this parser only supports entity "
                "references if they're predefined by the spec. This is not a valid character reference or one of the "
                "predefined entity references."), ampPosition, 6065);
            failAt(compat::RetainedString::fromStaticLiteral(
                "& is only legal in an EntitType.text entity as part of a reference, and this parser only supports "
                "entity references if they're predefined by the spec. This is not a valid character reference or "
                "one of the predefined entity references."), ampPosition, 6487);
        };
        require("&");
        for (auto token : {"amp;", "apos;", "quot;", "lt;", "gt;"}) {
            if (starts(token)) { pos += std::string_view(token).size(); return; }
        }
        if (!starts("#")) failReference();
        ++pos;
        unsigned radix = 10;
        if (starts("x")) { radix = 16; ++pos; }
        auto begin = pos;
        std::uint32_t value = 0;
        while (pos < input.size() && input[pos] != ';') {
            auto c = input[pos++];
            unsigned digit = c >= '0' && c <= '9' ? c - '0' :
                radix == 16 && c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                radix == 16 && c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99;
            if (digit >= radix || value > (0x10ffff - digit) / radix) failReference();
            value = value * radix + digit;
        }
        if (pos == begin || !xmlChar(value) || !starts(";")) failReference();
        ++pos;
    }
    void until(std::string_view delimiter) {
        auto end = input.find(delimiter, pos);
        if (end == input.npos)
            failAt(compat::RetainedString("Failed to find: " + std::string(delimiter)), pos, 5591);
        pos = end + delimiter.size();
    }
    void comment() {
        require("<!--");
        until("--");
        if (!starts(">")) failAt(compat::RetainedString::fromStaticLiteral(
            "Comments cannot contain -- and cannot be terminated by --->"), pos, 2002);
        ++pos;
    }
    void pi() { require("<?"); until("?>"); }
    void doctype() {
        require("<!DOCTYPE");
        if (!whitespace()) failAt(compat::RetainedString::fromStaticLiteral(
            "Whitespace must follow <!DOCTYPE"), pos, 1953);
        for (;;) {
            const auto next = input.find_first_of("\"'[>", pos);
            if (next == input.npos) failAt(compat::RetainedString::fromStaticLiteral(
                "Prematurely reached end of document"), input.size(), 5633);
            pos = next;
            const auto c = input[pos++];
            if (c == '\'' || c == '"') { until(std::string_view(&c, 1)); continue; }
            if (c == '>') return;
            for (;;) {
                if (pos == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
                    "Prematurely reached end of document"), pos, 2451);
                const auto inner = input.find_first_of("\"']", pos);
                if (inner == input.npos) failAt(compat::RetainedString::fromStaticLiteral(
                    "Prematurely reached end of document"), input.size(), 5633);
                pos = inner;
                const auto quote = input[pos++];
                if (quote != ']') { until(std::string_view(&quote, 1)); continue; }
                whitespace();
                if (!starts(">")) failAt(compat::RetainedString::fromStaticLiteral(
                    "Incorrectly terminated <!DOCTYPE> section."), pos, 2472);
                ++pos;
                return;
            }
        }
    }
    void misc() {
        for (;;) {
            whitespace();
            if (starts("<!--")) comment();
            else if (starts("<?")) pi();
            else return;
        }
    }
    void validChar(bool attribute) {
        const auto begin = pos;
        using Cursor = compat::phobos::detail::UnicodeCursor<std::string_view, char>;
        try {
            const auto scalar = Cursor(input.substr(pos)).front();
            if (!xmlChar(scalar)) {
                char digits[8];
                const auto formatted = std::to_chars(digits, digits + sizeof(digits), scalar, 16);
                failAt(compat::RetainedString("Character is not legal in an XML File: 0x" +
                    std::string(digits, formatted.ptr)), begin,
                    attribute ? (scalar < 128 ? 6136 : 6156) : (scalar < 128 ? 6557 : 6578));
            }
            pos += scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
        } catch (const compat::UTFException&) {
            failAt(compat::RetainedString::fromStaticLiteral("Invalid Unicode character"),
                begin, attribute ? 6161 : 6583);
        }
    }
    std::shared_ptr<Entity> element() {
        require("<");
        // dxml first captures the complete start-tag text, skipping quoted >.
        // Its missing-delimiter/quote errors precede attribute validation.
        const auto tagBegin = pos;
        auto tagEnd = pos;
        for (; tagEnd < input.size() && input[tagEnd] != '>'; ++tagEnd) {
            if (input[tagEnd] != '\'' && input[tagEnd] != '"') continue;
            const auto quotePosition = tagEnd;
            const auto next = input.find(input[tagEnd], tagEnd + 1);
            if (next == input.npos) failAt(compat::RetainedString::fromStaticLiteral(
                "Failed to find matching quote"), quotePosition, 5548);
            tagEnd = next;
        }
        if (tagEnd == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
            "Failed to find: >"), tagBegin, 5591);
        if (tagEnd == tagBegin) failAt(compat::RetainedString::fromStaticLiteral(
            "Tag missing name"), tagBegin, 2586);
        if (input[tagBegin] == '/') failAt(compat::RetainedString::fromStaticLiteral(
            "Invalid end tag"), tagBegin, 2588);
        const auto tagContentEnd = input[tagEnd - 1] == '/' ? tagEnd - 1 : tagEnd;
        auto result = std::make_shared<Entity>();
        result->name = name(0, tagContentEnd);
        std::vector<std::size_t> attributePositions;
        const auto checkAttributes = [&] {
            if (result->attributes.size() < 2) return;
            std::vector<std::size_t> order(result->attributes.size());
            for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
            const auto less = [&](std::size_t left, std::size_t right) {
                // Taken is takeExactly(byCodeUnit(string)): cmp reads raw code units.
                return result->attributes[left][0].view() < result->attributes[right][0].view();
            };
            // Original Attribute is a string descriptor plus two int32 coordinates.
            compat::phobos_index_sort::sort<2 * sizeof(void*) + 2 * sizeof(std::int32_t)>(order, less,
                "Failed to sort range of type Attribute[]");
            for (std::size_t i = 1; i < order.size(); ++i) {
                if (result->attributes[order[i - 1]][0].view() == result->attributes[order[i]][0].view())
                    failAt(compat::RetainedString::fromStaticLiteral("Duplicate attribute name"),
                        attributePositions[order[i]], 3067);
            }
        };
        for (;;) {
            bool separated = whitespace();
            if (starts("/>")) { pos += 2; checkAttributes(); return result; }
            if (starts(">")) { ++pos; checkAttributes(); break; }
            if (!separated) failAt(compat::RetainedString::fromStaticLiteral(
                "Whitespace missing before attribute name"), pos, 2638);
            const auto attributePosition = pos;
            auto attr = name('=', tagContentEnd);
            whitespace();
            if (pos >= tagEnd) failAt(compat::RetainedString::fromStaticLiteral(
                "Prematurely reached end of document"), pos, 2644);
            if (!starts("=")) failAt(compat::RetainedString::fromStaticLiteral("= missing"), pos, 2646);
            ++pos;
            whitespace();
            if (pos >= tagEnd) failAt(compat::RetainedString::fromStaticLiteral(
                "Prematurely reached end of document"), pos, 5980);
            if (input[pos] != '\'' && input[pos] != '"') failAt(compat::RetainedString::fromStaticLiteral(
                "Expected quoted text"), pos, 6175);
            const auto quotePosition = pos;
            auto quote = input[pos++];
            auto begin = pos;
            while (pos < input.size() && input[pos] != quote) {
                if (starts("<")) failAt(compat::RetainedString::fromStaticLiteral(
                    "< is not legal in an attribute name"), pos, 6116);
                if (starts("&")) reference(true); else validChar(true);
            }
            auto value = copy(begin, pos);
            if (pos == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
                "Unterminated attribute value"), quotePosition, 5997);
            ++pos;
            result->attributes.append(std::array<compat::RetainedString, 2>{attr, value});
            attributePositions.push_back(attributePosition);
        }
        for (;;) {
            if (pos == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
                "Prematurely reached end of document"), pos, 2790);
            auto begin = pos;
            whitespace();
            if (pos == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
                "Prematurely reached end of document"), pos, 2793);
            if (starts("<") && pos + 1 == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
                "Prematurely reached end of document"), pos + 1, 2815);
            if (starts("</")) {
                pos += 2;
                const auto namePosition = pos;
                if (pos == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
                    "Unterminated end tag"), pos, 2721);
                const auto endName = name('>');
                whitespace();
                if (!starts(">")) failAt(compat::RetainedString::fromStaticLiteral(
                    "There can only be whitespace between an end tag's name and the >"), pos, 2730);
                ++pos;
                if (endName.view() != result->name.view()) failAt(compat::RetainedString(
                    "Name of end tag </" + std::string(endName.view()) +
                    "> does not match corresponding start tag <" + std::string(result->name.view()) + ">"),
                    namePosition, 3034);
                return result;
            }
            if (starts("<!--")) { comment(); continue; }
            if (starts("<?")) { pi(); continue; }
            if (starts("<![CDATA[")) {
                pos += 9;
                auto textBegin = pos;
                const auto end = input.find("]]>", pos);
                if (end == input.npos) failAt(compat::RetainedString::fromStaticLiteral(
                    "Failed to find: ]]>"), textBegin, 5591);
                while (pos < end) validChar(false);
                auto text = std::make_shared<Entity>(); text->type = EntityType::cdata;
                text->text = copy(textBegin, pos); pos += 3; result->children.append(text); continue;
            }
            if (starts("<!")) failAt(compat::RetainedString::fromStaticLiteral(
                "Expected Comment or CDATA section"), pos + 1, 2928);
            if (starts("<")) { result->children.append(element()); continue; }
            pos = begin;
            const auto textEnd = input.find('<', begin);
            if (textEnd == input.npos) failAt(compat::RetainedString::fromStaticLiteral(
                "Failed to find: <"), begin, 5591);
            while (pos < textEnd) {
                if (starts("]]>")) failAt(compat::RetainedString::fromStaticLiteral(
                    "]]> is not legal in EntityType.text"), pos, 6537);
                if (starts("&")) reference(false); else validChar(false);
            }
            if (pos + 1 == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
                "Prematurely reached end of document"), pos + 1, 2803);
            auto text = std::make_shared<Entity>(); text->type = EntityType::text;
            text->text = copy(begin, pos); result->children.append(text);
        }
    }
public:
    explicit Parser(compat::RetainedString text) : owner(std::move(text)), input(owner.view()) {}
    std::shared_ptr<Entity> parse() {
        bool ws = whitespace();
        if (starts("<?xml")) {
            if (ws) failAt(compat::RetainedString::fromStaticLiteral(
                "Cannot have whitespace before the <?xml...?> declaration"), 0, 1880);
            auto after = pos + 5;
            if (after == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
                "Prematurely reached end of document"), after, 1881);
            if (space(input[after]) || input[after] == '?') { pos = after; until("?>"); }
        }
        misc();
        bool sawDoctype = false;
        if (starts("<!DOCTYPE")) { doctype(); misc(); sawDoctype = true; }
        if (pos == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
            "Prematurely reached end of document"), pos, 1927);
        if (!starts("<")) failAt(compat::RetainedString::fromStaticLiteral("Expected <"), pos, 1929);
        if (pos + 1 == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
            "Prematurely reached end of document"), pos + 1, 1931);
        if (starts("<!")) {
            if (sawDoctype && starts("<!DOCTYPE")) failAt(compat::RetainedString::fromStaticLiteral(
                "Only one <!DOCTYPE ...> declaration allowed per XML document"), pos + 1, 1963);
            const auto message = sawDoctype ? compat::RetainedString::fromStaticLiteral("Expected Comment") :
                compat::RetainedString::fromStaticLiteral("Expected Comment or DOCTYPE section");
            failAt(message, pos + 1, sawDoctype ? 1966 : 1957);
        }
        auto root = std::make_shared<Entity>();
        root->children.append(element());
        misc();
        if (pos != input.size()) {
            if (!starts("<")) failAt(compat::RetainedString::fromStaticLiteral("Expected <"), pos, 2965);
            if (pos + 1 == input.size()) failAt(compat::RetainedString::fromStaticLiteral(
                "Prematurely reached end of document"), pos + 1, 2967);
            if (starts("<!")) failAt(compat::RetainedString::fromStaticLiteral("Expected Comment"), pos + 1, 2983);
            failAt(compat::RetainedString::fromStaticLiteral("Must be a comment or PI"), pos + 1, 2993);
        }
        return root;
    }
};
}
std::shared_ptr<Entity> parseSimpleXML(compat::RetainedString text) { return Parser(std::move(text)).parse(); }
}
