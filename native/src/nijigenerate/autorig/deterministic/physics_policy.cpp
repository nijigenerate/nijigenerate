#include "detail/physics.hpp"
#include "detail/physics_resources.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace nijigenerate::autorig::deterministic::physics {
using namespace detail;
namespace {
char32_t takeRoleScalar(std::string_view text, std::size_t& position) {
    using Cursor = compat::phobos::detail::UnicodeCursor<std::string_view, char>;
    const auto scalar = Cursor(text.substr(position)).front();
    position += scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
    return scalar;
}
bool isRoleSeparator(char32_t c) {
    return (c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0 || c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
        c == 0x205f || c == 0x3000 || c == ':' || c == '-';
}
compat::RetainedStringSlice replaceRoleSeparators(compat::RetainedStringSlice input,
        std::source_location source = std::source_location::current()) {
    const auto text = input.view();
    // The fixed source charset has a one-byte ShiftOr prefix. Decode only its
    // possible leading bytes, then the live greedy run and its first nonmember.
    constexpr std::string_view leads = "\t\n\v\f\r :-\xc2\xe1\xe2\xe3";
    try {
        std::string result;
        bool changed = false;
        std::size_t search = 0, copied = 0;
        for (;;) {
            const auto begin = text.find_first_of(leads, search);
            if (begin == text.npos) break;
            auto after = begin;
            if (!isRoleSeparator(takeRoleScalar(text, after))) {
                search = after;
                continue;
            }
            auto end = after;
            while (after < text.size()) {
                const auto next = after;
                if (!isRoleSeparator(takeRoleScalar(text, after))) {
                    end = next;
                    break;
                }
                end = after;
            }
            result.append(text.substr(copied, begin - copied));
            result += '_';
            copied = end;
            search = after;
            changed = true;
        }
        if (!changed) return input;
        result.append(text.substr(copied));
        return compat::RetainedStringSlice{compat::RetainedString(std::move(result))};
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
compat::RetainedStringSlice collapseRoleUnderscores(compat::RetainedStringSlice input,
        std::source_location source = std::source_location::current()) {
    const auto text = input.view();
    const auto first = text.find("__");
    if (first == text.npos) return input;
    try {
        std::string result;
        std::size_t begin = first, copied = 0;
        for (;;) {
            auto end = begin + 2;
            while (end < text.size() && text[end] == '_') ++end;
            auto after = end;
            if (after < text.size()) takeRoleScalar(text, after);
            result.append(text.substr(copied, begin - copied));
            result += '_';
            copied = end;
            begin = text.find("__", after);
            if (begin == text.npos) break;
        }
        result.append(text.substr(copied));
        return compat::RetainedStringSlice{compat::RetainedString(std::move(result))};
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
Slice<compat::RetainedStringSlice> splitRoleTokens(compat::RetainedStringSlice input,
        std::source_location source = std::source_location::current()) {
    if (input.empty()) return {};
    const auto text = input.view();
    try {
        std::vector<compat::RetainedStringSlice> tokens;
        for (std::size_t start = 0;;) {
            const auto end = text.find('_', start);
            tokens.emplace_back(input.slice(start, end == text.npos ? input.size() : end));
            if (end == text.npos) break;
            start = end + 1;
        }
        return Slice<compat::RetainedStringSlice>(std::move(tokens));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
compat::RetainedStringSlice replaceRoleCaptureNames(compat::RetainedStringSlice input,
        std::source_location source = std::source_location::current()) {
    const auto pattern = input.view();
    constexpr std::string_view needle = "(?P<side>", replacement = "(?<side>";
    auto position = pattern.find(needle);
    if (position == pattern.npos) return input;
    try {
        std::string result;
        std::size_t copied = 0;
        do {
            result.append(pattern.substr(copied, position - copied));
            result.append(replacement);
            copied = position + needle.size();
            position = pattern.find(needle, copied);
        } while (position != pattern.npos);
        result.append(pattern.substr(copied));
        return compat::RetainedStringSlice{compat::RetainedString(std::move(result))};
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
bool roleHasSidePlaceholder(const Json& rule, std::source_location source = std::source_location::current()) {
    const auto owner = rule.sourceIndexValue("owner").as_string();
    const auto chart = rule.sourceIndexValue("chart").as_string();
    try {
        std::string combined(owner.view());
        combined.append(chart.view());
        return combined.find("{side}") != combined.npos;
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
}
Json ngRigPhysicsPolicy() {
    return autorig::json::detail::parseJSON(
        compat::RetainedString::fromStaticView(policySource()));
}
Json detail::currentRole(compat::RetainedStringSlice name) {
    auto specification=autorig::json::detail::parseJSON(
        compat::RetainedString::fromStaticView(materialRolesSource()));
    while(!name.empty() && name.view().back()=='\0')name=name.slice(0,name.size()-1);
    auto normalizedText=[&] {
        const auto value=io::imageexport::detail::lowerExtension(name);
        return replaceRoleSeparators(compat::RetainedStringSlice{value});
    }();
    normalizedText=collapseRoleUnderscores(normalizedText);
    while(!normalizedText.empty()&&normalizedText.view().front()=='_')
        normalizedText=normalizedText.slice(1,normalizedText.size());
    while(!normalizedText.empty()&&normalizedText.view().back()=='_')
        normalizedText=normalizedText.slice(0,normalizedText.size()-1);
    const auto normalizedView=normalizedText.view();
    bool decorator=false;
    for(auto prefix:specification.sourceIndexValue("decorator_prefixes").sourceArrayValue())
        decorator|=normalizedView.starts_with(prefix.as_string().view());
    Slice<compat::RetainedStringSlice> tags;
    for(auto token:splitRoleTokens(normalizedText)){
        const compat::RetainedStringSlice tag=token.view()=="left"?
            compat::RetainedStringSlice{compat::RetainedString::fromStaticLiteral("l")}:
            token.view()=="right"?
            compat::RetainedStringSlice{compat::RetainedString::fromStaticLiteral("r")}:token;
        if((tag.view()=="l"||tag.view()=="r")&&!contains(tags,tag))tags.append(tag);
    }
    Array candidates;
    for(auto rule:specification.sourceIndexValue(decorator?"receivers":"rules").sourceArrayValue()){
        const auto pattern=replaceRoleCaptureNames(
            compat::RetainedStringSlice{rule.sourceIndexValue("pattern").as_string()});
        if(!physicsRegexSearch(normalizedText,pattern))continue;
        bool needsSide=get(rule,"side",false).as_bool()||roleHasSidePlaceholder(rule);
        if(needsSide&&tags.size()!=1)return {};
        auto usage=decorator?jsonLiteral("decoration"):rule.sourceIndexValue("usage").as_string();
        candidates.append(object({{jsonLiteral("usage"),Json(usage)}}));
    }return candidates.size()==1?candidates[0]:Json{};
}
}
