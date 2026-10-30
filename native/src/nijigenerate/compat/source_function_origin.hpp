#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>

namespace nijigenerate::compat {

struct SourceMirAbi32;
struct SourceMirAbi64;

// These categories describe resolved Source arguments, never native type names.
enum class SourceFunctionArgumentForm {
    Parenthesized,
    SingleBasicTypeOrIdentifier,
    SingleLiteralExpression,
};

struct SourceFunctionTemplateArgument {
    std::string_view spelling;
    SourceFunctionArgumentForm form;
};

struct SourceFunctionOwnedArgument {
    RetainedString spelling;
    SourceFunctionArgumentForm form;
    SourceFunctionTemplateArgument view() const { return {spelling.view(), form}; }
};

// Call only after the genuine Source naryFun branch has selected this target.
// These return the declared symbol identifiers used by frontend objectToBuffer.
SourceFunctionOwnedArgument ngMirStringFunctionPredicateArgument();
SourceFunctionOwnedArgument ngMirStaticOpCallPredicateArgument();

template<class SourceAbi>
    requires (std::same_as<SourceAbi, SourceMirAbi32> || std::same_as<SourceAbi, SourceMirAbi64>)
SourceFunctionOwnedArgument ngMirZeroDimensionFunctionArgument() {
    if constexpr (std::same_as<SourceAbi, SourceMirAbi64>)
        return {RetainedString::fromStaticLiteral("0LU"), SourceFunctionArgumentForm::SingleLiteralExpression};
    else
        return {RetainedString::fromStaticLiteral("0u"), SourceFunctionArgumentForm::SingleLiteralExpression};
}

struct SourceFunctionScope {
    std::string_view identifier;
    bool isTemplateInstance;
    std::span<const SourceFunctionTemplateArgument> arguments;
};

enum class SourceFunctionTrust { Default, System, Trusted, Safe };
enum class SourceFunctionLinkage { D, C, Cpp, Windows, System, ObjectiveC };
enum class SourceFunctionAttribute : std::uint16_t {
    Pure = 1u << 0,
    Nothrow = 1u << 1,
    Nogc = 1u << 2,
    Property = 1u << 3,
    Ref = 1u << 4,
    ExplicitReturn = 1u << 5,
    ExplicitScope = 1u << 6,
    Live = 1u << 7,
};

struct SourceFunctionAttributes {
    // Supply the Source function type's attributes at special-token expansion.
    // Later completed inference is not a substitute for that semantic snapshot.
    // Return/scope bits describe explicit, not inferred, attributes.
    std::uint16_t flags;
    SourceFunctionTrust trust;
    std::string_view qualifiers;
    SourceFunctionAttributes(std::uint16_t flags, SourceFunctionTrust trust,
        std::string_view qualifiers) : flags(flags), trust(trust), qualifiers(qualifiers) {}
};

struct SourceFunctionDeclaration {
    std::span<const SourceFunctionScope> scopes;
    std::string_view returnType;
    // Each entry is the actual resolved parameter declaration rendered by the
    // defining mapping, including tuple/type/storage/name/default information.
    std::span<const std::string_view> parameters;
    SourceFunctionAttributes attributes;
    SourceFunctionLinkage linkage;
    bool objectiveCStatic;
};

struct SourceFunctionOrigin {
    RetainedString function;
    RetainedString prototype;
};

// A specialization belongs at the actual Source function's defining mapping.
// Missing identity/attributes are an absent producer, not an empty origin.
template<class DeclarationIdentity> struct SourceFunctionOriginTraits;

SourceFunctionOrigin ngRenderSourceFunctionOrigin(const SourceFunctionDeclaration& declaration);
RetainedString ngRenderSourceFunctionName(std::span<const SourceFunctionScope> scopes);
RetainedString ngMirSlicePopFrontFunctionName(const RetainedString& sliceQualifiedElement,
    const SourceFunctionTemplateArgument& dimension);
RetainedString ngMirSlicePopFrontErrorMessage(const RetainedString& sliceQualifiedElement,
    const SourceFunctionTemplateArgument& dimension);

template<class SourceSliceIdentity, class SourceAbi>
    requires (std::same_as<SourceAbi, SourceMirAbi32> || std::same_as<SourceAbi, SourceMirAbi64>)
RetainedString ngMirSliceZeroPopFrontErrorMessage() {
    using namespace ::nicxlive::core::d_runtime;
    const auto slice = ngSourceTypeSpelling<SourceSliceIdentity>(SourceTypeSpelling::QualifiedElement);
    const auto dimension = ngMirZeroDimensionFunctionArgument<SourceAbi>();
    return ngMirSlicePopFrontErrorMessage(slice, dimension.view());
}
// Maps the actual nested declaration in mir.algorithm.iteration.any. Arguments
// are resolved Source metadata; no native callable signature is substituted.
SourceFunctionOrigin ngMirAnyFunctionOrigin(const SourceFunctionTemplateArgument& predicate,
    std::span<const SourceFunctionTemplateArgument> sliceTypes,
    std::span<const std::string_view> parameters, const SourceFunctionAttributes& attributes);
SourceFunctionAttributes ngMirAnyShapeCheckEntryAttributes();

// The Source types are explicit defining identities, not deduced native view
// carriers. Shape diagnostics instantiate this branch only for multiple slices.
template<class... SourceSlices> requires (sizeof...(SourceSlices) > 1)
SourceFunctionOrigin ngMirAnyTypedFunctionOrigin(const SourceFunctionTemplateArgument& predicate,
    std::span<const std::string_view> parameters, const SourceFunctionAttributes& attributes) {
    using namespace ::nicxlive::core::d_runtime;
    const std::array<RetainedString, sizeof...(SourceSlices)> names{
        ngSourceTypeSpelling<SourceSlices>(SourceTypeSpelling::UnqualifiedElement)...};
    std::array<SourceFunctionTemplateArgument, sizeof...(SourceSlices)> arguments;
    for (std::size_t i = 0; i < names.size(); ++i) {
        arguments[i] = {names[i].view(), SourceFunctionArgumentForm::Parenthesized};
    }
    // All owners above survive the synchronous builder. With at least two
    // template arguments, frontend parenthesization does not elide parentheses.
    return ngMirAnyFunctionOrigin(predicate, arguments, parameters, attributes);
}

// This is the special-token expansion at the first statement of the selected
// Mir any body, before final attribute and parameter-scope inference.
template<class... SourceSlices> requires (sizeof...(SourceSlices) > 1)
SourceFunctionOrigin ngMirAnyShapeCheckFunctionOrigin(const SourceFunctionTemplateArgument& predicate) {
    using namespace ::nicxlive::core::d_runtime;
    const std::array<RetainedString, sizeof...(SourceSlices)> names{
        ngSourceTypeSpelling<SourceSlices>(SourceTypeSpelling::UnqualifiedElement)...};
    std::array<SourceFunctionTemplateArgument, sizeof...(SourceSlices)> arguments;
    std::array<std::string_view, sizeof...(SourceSlices)> parameters;
    for (std::size_t i = 0; i < names.size(); ++i) {
        arguments[i] = {names[i].view(), SourceFunctionArgumentForm::Parenthesized};
        parameters[i] = names[i].view();
    }
    return ngMirAnyFunctionOrigin(predicate, arguments, parameters, ngMirAnyShapeCheckEntryAttributes());
}
RetainedString ngMirFunctionErrorTail(const SourceFunctionOrigin& origin);
RetainedString ngMirShapeErrorMessage(const SourceFunctionOrigin& origin);

template<class... SourceSlices> requires (sizeof...(SourceSlices) > 1)
RetainedString ngMirStringPredicateShapeErrorMessage() {
    const auto predicate = ngMirStringFunctionPredicateArgument();
    const auto origin = ngMirAnyShapeCheckFunctionOrigin<SourceSlices...>(predicate.view());
    return ngMirShapeErrorMessage(origin);
}

template<class... SourceSlices> requires (sizeof...(SourceSlices) > 1)
RetainedString ngMirStaticOpCallPredicateShapeErrorMessage() {
    const auto predicate = ngMirStaticOpCallPredicateArgument();
    const auto origin = ngMirAnyShapeCheckFunctionOrigin<SourceSlices...>(predicate.view());
    return ngMirShapeErrorMessage(origin);
}

template<class DeclarationIdentity>
    requires requires {
        { SourceFunctionOriginTraits<DeclarationIdentity>::origin() } -> std::same_as<SourceFunctionOrigin>;
    }
SourceFunctionOrigin ngSourceFunctionOrigin() {
    return SourceFunctionOriginTraits<DeclarationIdentity>::origin();
}

} // namespace nijigenerate::compat
