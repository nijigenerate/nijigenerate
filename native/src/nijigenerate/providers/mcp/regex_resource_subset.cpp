// Finite specialization of the original MCP resource-template expressions.
// This is not a general implementation of std.regex or SchemaBuilder.setPattern.
#include "resources.hpp"
#include "schema.hpp"

namespace nijigenerate::providers::mcp {
namespace {
struct UnsupportedPhobosRegexPattern {
    String pattern;
};
[[noreturn]] void unsupported(String expression) {throw UnsupportedPhobosRegexPattern{expression};}
}
namespace resources {
compat::Slice<compat::Slice<String>> ngPhobosResourceMatchAll(String input,String expression) {
    if(expression.view()!="\\{([^}]+)\\}") unsupported(expression);
    compat::Slice<compat::Slice<String>> matches;
    const auto value=input.view();std::size_t from=0;
    while(from<value.size()) {
        const auto begin=value.find('{',from);if(begin==std::string_view::npos) break;
        const auto end=value.find('}',begin+1);if(end==std::string_view::npos) break;
        if(end!=begin+1) matches.append(compat::Slice<String>{input.slice(begin,end+1),input.slice(begin+1,end)});
        from=end+1;
    }
    return matches;
}
String ngPhobosResourceReplaceAll(String input,String expression,String replacement) {
    if(expression.view()!="\\{uuid\\}"||replacement.view()!="([^/]+)") unsupported(expression);
    std::string result;const auto value=input.view();std::size_t from=0;
    for(;;) {
        const auto found=value.find("{uuid}",from);
        if(found==std::string_view::npos) {result.append(value.substr(from));break;}
        result.append(value.substr(from,found-from));result.append(replacement.view());from=found+6;
    }
    return text(result);
}
compat::Slice<String> ngPhobosResourceMatchFirst(String input,String expression) {
    constexpr std::string_view expected="^resource://nijigenerate/resources/([^/]+)$";
    constexpr std::string_view prefix="resource://nijigenerate/resources/";
    if(expression.view()!=expected) unsupported(expression);
    // No multiline flag: pinned parser emits Bof/Eof, not Bol/Eol. Eof requires
    // the actual end, and the negated slash class includes newline and NUL.
    if(!input.view().starts_with(prefix)||input.size()==prefix.size()) return {};
    const auto capture=input.slice(prefix.size(),input.size());
    for(std::size_t position=prefix.size();position<input.size();) {
        // The character class consumes decoded code points, not arbitrary bytes.
        // Use the existing pinned std.utf decoder and its UTFException carrier.
        if(core::selector::ngDecodeSelectorUtf8(input.view(),position)=='/') return {};
    }
    return {input,capture};
}
}
namespace schema {
bool ngPhobosSchemaPatternMatch(String, String expression) {
    // No nijigenerate MCP tool schema installs a regex pattern. Keep the wider
    // SDK surface explicitly incomplete instead of validating with a different engine.
    unsupported(expression);
}
}
}
