#pragma once

#include <functional>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {

// The defining declaration owns exact original text/scope and the actual
// translated native body. This executes C++ code; it does not interpret Source
// text, infer names from a pointer, or verify an invented declaration's origin.
template<class Declaration, auto NativeExpression> struct TranslatedSourceExpression {
    static_assert(requires { Declaration::sourceExpression; Declaration::sourceModule; },
        "A translated expression needs its genuine declaring text and scope");
    using SourceDeclaration = Declaration;

    template<class... Arguments> static constexpr decltype(auto) invoke(Arguments&&... arguments) {
        if constexpr ((std::is_pointer_v<decltype(NativeExpression)> &&
                std::is_function_v<std::remove_pointer_t<decltype(NativeExpression)>>) ||
                std::is_member_function_pointer_v<decltype(NativeExpression)>)
            static_assert(NativeExpression != nullptr, "A translated expression needs its actual native body");
        static_assert(std::is_invocable_v<decltype(NativeExpression), Arguments&&...>,
            "A translated expression body must accept its actual lowered Source arguments");
        if constexpr (std::is_invocable_v<decltype(NativeExpression), Arguments&&...>)
            return std::invoke(NativeExpression, std::forward<Arguments>(arguments)...);
    }
    template<class... Arguments> constexpr decltype(auto) operator()(Arguments&&... arguments) const {
        return invoke(std::forward<Arguments>(arguments)...);
    }
};

} // namespace nijigenerate::compat
