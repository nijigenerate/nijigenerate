#pragma once

#include "source_function_origin.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nijigenerate::compat {
namespace source_static_detail {

template<std::size_t Capacity> struct Text {
    std::array<char, Capacity + 1> bytes{};
    std::size_t length = 0;

    constexpr void append(char value) {
        if (length == Capacity) throw "Source declaration text capacity exceeded";
        bytes[length++] = value;
    }
    constexpr void append(std::string_view value) {
        for (const auto byte : value) append(byte);
    }
    constexpr void decimal(std::size_t value) {
        if (value >= 10) decimal(value / 10);
        append(static_cast<char>('0' + value % 10));
    }
    constexpr std::string_view view() const { return {bytes.data(), length}; }
};

template<class Abi> concept SourceAbi =
    std::same_as<Abi, SourceMirAbi32> || std::same_as<Abi, SourceMirAbi64>;

template<SourceAbi Abi, std::size_t Capacity>
constexpr void zeroDimension(Text<Capacity>& text) {
    if constexpr (std::same_as<Abi, SourceMirAbi64>) text.append("0LU");
    else text.append("0u");
}

template<std::size_t Rank, SourceAbi Abi>
consteval auto contiguousName(std::string_view iterator) {
    Text<96> result;
    result.append("mir_slice!(");
    result.append(iterator);
    result.append(", ");
    result.decimal(Rank);
    if constexpr (std::same_as<Abi, SourceMirAbi64>) result.append("LU");
    else result.append('u');
    result.append(", mir_slice_kind.contiguous)");
    return result;
}

template<SourceAbi Abi, std::size_t Capacity>
consteval auto popFront(const Text<Capacity>& type) {
    Text<256> result;
    result.append("mir.ndslice.slice.");
    result.append(type.view());
    result.append(".mir_slice.popFront!");
    zeroDimension<Abi>(result);
    result.append(".popFront: length!");
    zeroDimension<Abi>(result);
    result.append(" should be greater than 0.");
    return result;
}

template<std::size_t Capacity>
constexpr void argument(Text<Capacity>& text, const SourceFunctionTemplateArgument& value) {
    const bool parenthesized = value.form == SourceFunctionArgumentForm::Parenthesized;
    if (parenthesized) text.append('(');
    text.append(value.spelling);
    if (parenthesized) text.append(')');
}

template<class Alias, class... Declarations> requires (sizeof...(Declarations) > 1)
consteval auto shapeError() {
    // Three copies of each actual Slice spelling occur below. Include the real
    // defining alias's length, separators and fixed headings in the capacity.
    constexpr auto predicate = Alias::sourceArgument();
    constexpr auto nameBytes = (Declarations::unqualifiedName().length + ... + std::size_t{0});
    constexpr auto capacity = nameBytes * 3 + sizeof...(Declarations) * 8
        + predicate.spelling.size() * 2 + 256;
    const std::array names{Declarations::unqualifiedName()...};
    Text<capacity> function;
    function.append("mir.algorithm.iteration.any!");
    argument(function, predicate);
    function.append(".any!(");
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i) function.append(", ");
        function.append(names[i].view());
    }
    function.append(").any");

    Text<capacity> result;
    result.append("all slices must have the same shape\n- - -\nError in function\n");
    result.append(function.view());
    result.append("\n- - -\nFunction prototype\nbool ");
    result.append(function.view());
    result.append('(');
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i) result.append(", ");
        result.append(names[i].view());
    }
    // This exact first statement precedes TypeFunction inference completion.
    // Its resolved tuple parameters have no names or explicit storage classes.
    result.append(")\n_____");
    return result;
}
}

// A declaration key for the actual Image.sliced<T> path and its contiguous
// rank transforms. It is never inferred from an arbitrary native view carrier.
template<class T, std::size_t Rank, source_static_detail::SourceAbi Abi>
    requires (Rank > 0 && Rank < 255 && (std::same_as<T, std::uint8_t>
        || std::same_as<T, std::uint16_t> || std::same_as<T, float>))
struct StaticMirImageSliceDeclaration {
    static consteval auto unqualifiedName() {
        if constexpr (std::same_as<T, std::uint8_t>)
            return source_static_detail::contiguousName<Rank, Abi>("ubyte*");
        else if constexpr (std::same_as<T, std::uint16_t>)
            return source_static_detail::contiguousName<Rank, Abi>("ushort*");
        else return source_static_detail::contiguousName<Rank, Abi>("float*");
    }
    static consteval auto popFrontMessage() {
        return source_static_detail::popFront<Abi>(unqualifiedName());
    }
};

// These are the explicit outputs at distancetransform.d76 and their actual
// contiguous front/flattened rank reductions. Keep the original output defaults.
// Identity does not come from the native output aggregate or its storage.
template<source_static_detail::SourceAbi Abi, std::size_t Rank = 2>
    requires (Rank > 0 && Rank <= 2)
struct StaticDistanceTransformDistanceSliceDeclaration {
    static consteval auto unqualifiedName() {
        return source_static_detail::contiguousName<Rank, Abi>("float*");
    }
    static consteval auto popFrontMessage() {
        return source_static_detail::popFront<Abi>(unqualifiedName());
    }
};

template<source_static_detail::SourceAbi Abi, std::size_t Rank = 3>
    requires (Rank > 0 && Rank <= 3)
struct StaticDistanceTransformNearestSliceDeclaration {
    static consteval auto unqualifiedName() {
        return source_static_detail::contiguousName<Rank, Abi>("int*");
    }
    static consteval auto popFrontMessage() {
        return source_static_detail::popFront<Abi>(unqualifiedName());
    }
};

// Defining consumers select these only after the genuine Source naryFun branch
// has chosen its stringFun or static opCall alias. No C++ callable is classified.
struct StaticMirStringFunctionAlias {
    static consteval SourceFunctionTemplateArgument sourceArgument() {
        return {"stringFun", SourceFunctionArgumentForm::Parenthesized};
    }
};
struct StaticMirOpCallAlias {
    static consteval SourceFunctionTemplateArgument sourceArgument() {
        return {"opCall", SourceFunctionArgumentForm::Parenthesized};
    }
};

// Other genuine symbolic/value aliases supply their own constexpr
// sourceArgument() record. An absent defining record remains an absent producer.
template<class Alias, class... Declarations> requires (sizeof...(Declarations) > 1)
RetainedString ngStaticMirAnyShapeErrorMessage() noexcept {
    static constexpr auto message = source_static_detail::shapeError<Alias, Declarations...>();
    return RetainedString::fromStaticView(message.view());
}

template<class Declaration>
RetainedString ngStaticMirSlicePopFrontErrorMessage() noexcept {
    static constexpr auto message = Declaration::popFrontMessage();
    return RetainedString::fromStaticView(message.view());
}

} // namespace nijigenerate::compat
