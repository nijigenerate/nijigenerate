#pragma once

#include "types_status_level.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <cstddef>
#include <tuple>
#include <compat/object_metadata.hpp>

namespace nijigenerate::api::acp::types {

struct Position {
    std::size_t line{};
    std::size_t character{};
    bool operator==(const Position&) const = default;
};

struct Range {
    Position start;
    Position end;
    bool operator==(const Range&) const = default;
};

struct TextEdit {
    Range range;
    compat::RetainedStringSlice newText;
    bool operator==(const TextEdit&) const = default;
};

struct WorkspaceEdit {
    compat::RetainedStringSlice uri;
    compat::Slice<TextEdit> edits;
    bool operator==(const WorkspaceEdit& other) const {
        if (uri != other.uri || edits.size() != other.edits.size()) return false;
        for (std::size_t i = 0; i < edits.size(); ++i) {
            if (edits[i] != other.edits[i]) return false;
        }
        return true;
    }
};

struct Document {
    compat::RetainedStringSlice uri;
    compat::RetainedStringSlice languageId;
    compat::RetainedStringSlice text;
    std::size_t version_{};
    bool operator==(const Document&) const = default;
};

struct StatusNotification {
    compat::RetainedStringSlice title;
    compat::RetainedStringSlice message;
    StatusLevel level{};
    bool operator==(const StatusNotification&) const = default;
};

} // namespace nijigenerate::api::acp::types

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::api::acp::types::Position> {
    using Value=::nijigenerate::api::acp::types::Position;
    inline static constexpr std::string_view typeName="Position";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("line",offsetof(Value,line),&Value::line),
            ngFormatAggregateMember("character",offsetof(Value,character),&Value::character)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::api::acp::types::Range> {
    using Value=::nijigenerate::api::acp::types::Range;
    inline static constexpr std::string_view typeName="Range";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("start",offsetof(Value,start),&Value::start),
            ngFormatAggregateMember("end",offsetof(Value,end),&Value::end)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::api::acp::types::TextEdit> {
    using Value=::nijigenerate::api::acp::types::TextEdit;
    inline static constexpr std::string_view typeName="TextEdit";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("range",offsetof(Value,range),&Value::range),
            ngFormatAggregateMember("newText",offsetof(Value,newText),&Value::newText)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::api::acp::types::WorkspaceEdit> {
    using Value=::nijigenerate::api::acp::types::WorkspaceEdit;
    inline static constexpr std::string_view typeName="WorkspaceEdit";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("uri",offsetof(Value,uri),&Value::uri),
            ngFormatAggregateMember("edits",offsetof(Value,edits),&Value::edits)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::api::acp::types::Document> {
    using Value=::nijigenerate::api::acp::types::Document;
    inline static constexpr std::string_view typeName="Document";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("uri",offsetof(Value,uri),&Value::uri),
            ngFormatAggregateMember("languageId",offsetof(Value,languageId),&Value::languageId),
            ngFormatAggregateMember("text",offsetof(Value,text),&Value::text),
            ngFormatAggregateMember("version_",offsetof(Value,version_),&Value::version_)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::api::acp::types::StatusNotification> {
    using Value=::nijigenerate::api::acp::types::StatusNotification;
    inline static constexpr std::string_view typeName="StatusNotification";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("title",offsetof(Value,title),&Value::title),
            ngFormatAggregateMember("message",offsetof(Value,message),&Value::message),
            ngFormatAggregateMember("level",offsetof(Value,level),&Value::level)};
    }
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::types::Position> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.api.acp.types.Position)"),
            RetainedString::fromStaticLiteral("(Position)"),
            RetainedString::fromStaticLiteral("nijigenerate.api.acp.types.Position"),
            RetainedString::fromStaticLiteral("Position")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::types::Range> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.api.acp.types.Range)"),
            RetainedString::fromStaticLiteral("(Range)"),
            RetainedString::fromStaticLiteral("nijigenerate.api.acp.types.Range"),
            RetainedString::fromStaticLiteral("Range")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::types::TextEdit> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.api.acp.types.TextEdit)"),
            RetainedString::fromStaticLiteral("(TextEdit)"),
            RetainedString::fromStaticLiteral("nijigenerate.api.acp.types.TextEdit"),
            RetainedString::fromStaticLiteral("TextEdit")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::types::WorkspaceEdit> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.api.acp.types.WorkspaceEdit)"),
            RetainedString::fromStaticLiteral("(WorkspaceEdit)"),
            RetainedString::fromStaticLiteral("nijigenerate.api.acp.types.WorkspaceEdit"),
            RetainedString::fromStaticLiteral("WorkspaceEdit")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::types::Document> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.api.acp.types.Document)"),
            RetainedString::fromStaticLiteral("(Document)"),
            RetainedString::fromStaticLiteral("nijigenerate.api.acp.types.Document"),
            RetainedString::fromStaticLiteral("Document")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::types::StatusNotification> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.api.acp.types.StatusNotification)"),
            RetainedString::fromStaticLiteral("(StatusNotification)"),
            RetainedString::fromStaticLiteral("nijigenerate.api.acp.types.StatusNotification"),
            RetainedString::fromStaticLiteral("StatusNotification")};
    }
};
} // namespace nicxlive::core::d_runtime
