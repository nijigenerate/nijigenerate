#include "source_function_origin.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::compat {
namespace {
template<class Operation> auto allocateText(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}

void appendScope(std::string& text, const SourceFunctionScope& scope) {
    text.append(scope.identifier);
    if (!scope.isTemplateInstance) return;
    text.push_back('!');
    const bool parenthesized = scope.arguments.size() != 1 ||
        scope.arguments[0].form == SourceFunctionArgumentForm::Parenthesized;
    if (parenthesized) text.push_back('(');
    for (std::size_t i = 0; i < scope.arguments.size(); ++i) {
        if (i) text.append(", ");
        text.append(scope.arguments[i].spelling);
    }
    if (parenthesized) text.push_back(')');
}

void appendAttributes(std::string& text, const SourceFunctionAttributes& attributes) {
    if (!attributes.qualifiers.empty()) {
        text.push_back(' ');
        text.append(attributes.qualifiers);
    }
    constexpr std::array<std::string_view, 8> names{
        "pure", "nothrow", "@nogc", "@property", "ref", "return", "scope", "@live"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (attributes.flags & (std::uint16_t{1} << i)) {
            text.push_back(' ');
            text.append(names[i]);
        }
    }
    switch (attributes.trust) {
    case SourceFunctionTrust::Default: break;
    case SourceFunctionTrust::System: text.append(" @system"); break;
    case SourceFunctionTrust::Trusted: text.append(" @trusted"); break;
    case SourceFunctionTrust::Safe: text.append(" @safe"); break;
    }
}

std::string functionNameText(std::span<const SourceFunctionScope> scopes) {
    std::string name;
    for (std::size_t i = 0; i < scopes.size(); ++i) {
        if (i) name.push_back('.');
        appendScope(name, scopes[i]);
    }
    return name;
}

void appendTail(std::string& text, const SourceFunctionOrigin& origin) {
    text.append("\n- - -\nError in function\n");
    text.append(origin.function.view());
    text.append("\n- - -\nFunction prototype\n");
    text.append(origin.prototype.view());
    text.append("\n_____");
}
}

SourceFunctionOwnedArgument ngMirStringFunctionPredicateArgument() {
    return {RetainedString::fromStaticLiteral("stringFun"), SourceFunctionArgumentForm::Parenthesized};
}

SourceFunctionOwnedArgument ngMirStaticOpCallPredicateArgument() {
    return {RetainedString::fromStaticLiteral("opCall"), SourceFunctionArgumentForm::Parenthesized};
}

SourceFunctionOrigin ngRenderSourceFunctionOrigin(const SourceFunctionDeclaration& declaration) {
    // All metadata callbacks must complete before this pure byte-storage path.
    return allocateText([&] {
        auto name = functionNameText(declaration.scopes);
        std::string prototype;
        switch (declaration.linkage) {
        case SourceFunctionLinkage::D: break;
        case SourceFunctionLinkage::C: prototype.append("extern (C) "); break;
        case SourceFunctionLinkage::Cpp: prototype.append("extern (C++) "); break;
        case SourceFunctionLinkage::Windows: prototype.append("extern (Windows) "); break;
        case SourceFunctionLinkage::System: prototype.append("extern (System) "); break;
        case SourceFunctionLinkage::ObjectiveC:
            prototype.append("extern (Objective-C) ");
            if (declaration.objectiveCStatic) prototype.append("static ");
            break;
        }
        if (!declaration.returnType.empty()) {
            prototype.append(declaration.returnType);
            prototype.push_back(' ');
        }
        prototype.append(name);
        prototype.push_back('(');
        for (std::size_t i = 0; i < declaration.parameters.size(); ++i) {
            if (i) prototype.append(", ");
            prototype.append(declaration.parameters[i]);
        }
        prototype.push_back(')');
        appendAttributes(prototype, declaration.attributes);
        return SourceFunctionOrigin{RetainedString(std::move(name)), RetainedString(std::move(prototype))};
    });
}

RetainedString ngRenderSourceFunctionName(std::span<const SourceFunctionScope> scopes) {
    return allocateText([&] { return RetainedString(functionNameText(scopes)); });
}

RetainedString ngMirSlicePopFrontFunctionName(const RetainedString& sliceQualifiedElement,
    const SourceFunctionTemplateArgument& dimension) {
    const std::array scopes{
        SourceFunctionScope{sliceQualifiedElement.view(), false, {}},
        SourceFunctionScope{"mir_slice", false, {}},
        SourceFunctionScope{"popFront", true, std::span{&dimension, std::size_t{1}}},
        SourceFunctionScope{"popFront", false, {}}};
    return ngRenderSourceFunctionName(scopes);
}

RetainedString ngMirSlicePopFrontErrorMessage(const RetainedString& sliceQualifiedElement,
    const SourceFunctionTemplateArgument& dimension) {
    const auto name = ngMirSlicePopFrontFunctionName(sliceQualifiedElement, dimension);
    return allocateText([&] {
        std::string message(name.view());
        message.append(": length!");
        message.append(dimension.spelling);
        message.append(" should be greater than 0.");
        return RetainedString(std::move(message));
    });
}

SourceFunctionAttributes ngMirAnyShapeCheckEntryAttributes() {
    // Mir any's first statement expands the tokens before semantic3 finalizes
    // inference. initInferAttributes sets fd flags, not these TypeFunction bits.
    return SourceFunctionAttributes{0, SourceFunctionTrust::Default, {}};
}

SourceFunctionOrigin ngMirAnyFunctionOrigin(const SourceFunctionTemplateArgument& predicate,
    std::span<const SourceFunctionTemplateArgument> sliceTypes,
    std::span<const std::string_view> parameters, const SourceFunctionAttributes& attributes) {
    const std::array scopes{
        SourceFunctionScope{"mir", false, {}},
        SourceFunctionScope{"algorithm", false, {}},
        SourceFunctionScope{"iteration", false, {}},
        SourceFunctionScope{"any", true, std::span{&predicate, std::size_t{1}}},
        SourceFunctionScope{"any", true, sliceTypes},
        SourceFunctionScope{"any", false, {}}};
    return ngRenderSourceFunctionOrigin(SourceFunctionDeclaration{
        scopes, "bool", parameters, attributes, SourceFunctionLinkage::D, false});
}

RetainedString ngMirFunctionErrorTail(const SourceFunctionOrigin& origin) {
    return allocateText([&] {
        std::string text;
        appendTail(text, origin);
        return RetainedString(std::move(text));
    });
}

RetainedString ngMirShapeErrorMessage(const SourceFunctionOrigin& origin) {
    return allocateText([&] {
        std::string text = "all slices must have the same shape";
        appendTail(text, origin);
        return RetainedString(std::move(text));
    });
}
} // namespace nijigenerate::compat
