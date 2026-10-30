#pragma once

#include "optimum_detail.hpp"
#include "nijigenerate/core/typed_format.hpp"
#include "nijigenerate/compat/phobos_stdio.hpp"
#include <compat/source_allocator.hpp>
#include <utility>

namespace nijigenerate::viewport::vertex::automesh::optimum::detail::diagnostics {
using core::translation::TypedFormatArgument;
using core::translation::FormatRange;

template<class Reader> void contourReader(FormatRange& range, Reader&& reader) {
    // These fresh ranges store only already-formed descriptor captures.
    // Actual reads and formatting execute later, outside this allocation catch.
    try {
        range.read = std::forward<Reader>(reader);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

// inmath Vector.toString formats its actual integer coordinate array with %s.
// Retain the genuine contour descriptors and expose them to the canonical
// formatter as nested ranges, rather than replacing CV points or copying models.
inline TypedFormatArgument contourArgument(Contours contours) {
    auto outer = std::allocate_shared<FormatRange>(nicxlive::core::d_runtime::SourceAllocator<FormatRange>{});
    outer->length = contours.size();
    contourReader(*outer, [contours](std::size_t i) -> TypedFormatArgument {
        const auto contour = contours[i];
        auto points = std::allocate_shared<FormatRange>(nicxlive::core::d_runtime::SourceAllocator<FormatRange>{});
        points->length = contour.size();
        contourReader(*points, [contour](std::size_t j) -> TypedFormatArgument {
            const auto point = contour[j];
            auto coordinates = std::allocate_shared<FormatRange>(
                nicxlive::core::d_runtime::SourceAllocator<FormatRange>{});
            coordinates->length = 2;
            contourReader(*coordinates, [point](std::size_t component) -> TypedFormatArgument {
                if (component > 1) throw std::out_of_range("Contour diagnostic component");
                return core::translation::ngFormatArgument(component == 0 ? point.x : point.y);
            });
            return coordinates;
        });
        return points;
    });
    return outer;
}
inline TypedFormatArgument argument(TypedFormatArgument value) { return value; }
template<class T> TypedFormatArgument argument(T value) { return core::translation::ngFormatArgument(value); }

template<class... Arguments>
void write(bool newline, std::string_view pattern, Arguments... arguments) {
    const std::array<TypedFormatArgument, sizeof...(Arguments)> values{argument(std::move(arguments))...};
    nicxlive::core::d_runtime::phobos::stdio::standardOutput().withTextWriter(
        [&](nicxlive::core::d_runtime::phobos::stdio::FileTextWriter& writer) {
            core::translation::ngFormatTypedTo(
                [&writer](std::string_view bytes) { writer.put(bytes); }, pattern, values);
            if (newline) writer.put(static_cast<unsigned char>('\n'));
        });
}
template<class... Arguments> void print(std::string_view pattern, Arguments... arguments) {
    write(false, pattern, std::move(arguments)...);
}
template<class... Arguments> void println(std::string_view pattern, Arguments... arguments) {
    write(true, pattern, std::move(arguments)...);
}
} // namespace nijigenerate::viewport::vertex::automesh::optimum::detail::diagnostics
