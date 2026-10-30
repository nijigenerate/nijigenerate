// Copyright (c) 2017 - 2025 Jonathan M Davis. Distributed under LICENSE.dxml.
#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <source_location>
namespace nijigenerate::io::kra::xml {
enum class EntityType { elementStart, text, cdata };
struct Entity {
    EntityType type{EntityType::elementStart};
    compat::RetainedString name, text;
    compat::Slice<std::array<compat::RetainedString, 2>> attributes;
    compat::Slice<std::shared_ptr<Entity>> children;
};
struct TextPos {
    std::int32_t line = 1;
    std::int32_t col = 1;
};
class XMLParsingException : public compat::ThrowableException {
public:
    XMLParsingException(const char* message, TextPos position, std::size_t offset,
        std::source_location source = std::source_location::current());
    XMLParsingException(compat::RetainedString message, TextPos position, std::size_t offset,
        compat::RetainedString sourceFile, std::size_t sourceLine);
    std::string_view ngThrowableTypeName() const noexcept override { return "dxml.parser.XMLParsingException"; }
    TextPos pos;
    std::size_t offset;
};
// The simpleXML DOM preserves reference spelling and omits whitespace-only text.
std::shared_ptr<Entity> parseSimpleXML(compat::RetainedString text);
}
