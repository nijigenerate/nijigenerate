#pragma once

#include "source_slice_identity.hpp"
#include <nijigenerate/core/cv/image.hpp>
#include <cstddef>
#include <concepts>

namespace nijigenerate::compat {

struct SourceMirSliceValueContext {
    RetainedString dimensionArgument;
    RetainedString kindArgument;
};

struct SourceMirSliceValueOrigins {
    SourceMirSliceValueContext qualifiedArgument;
    SourceMirSliceValueContext unqualifiedArgument;
    SourceMirSliceValueContext qualifiedElement;
    SourceMirSliceValueContext unqualifiedElement;
};

// These describe the selected Source ABI, independently of the native carrier.
// Defining consumers must select one explicitly; there is no sizeof fallback.
struct SourceMirAbi32 {};
struct SourceMirAbi64 {};

template<class Abi> concept KnownSourceMirAbi =
    std::same_as<Abi, SourceMirAbi32> || std::same_as<Abi, SourceMirAbi64>;

template<std::size_t Dimensions, SourceMirSliceKind Kind, class SourceAbi>
struct SourceMirSliceValueOrigin;

// Source slice.d669 constrains the dimension/kind domain. Actual selected
// IntegerExp rendering is supplied by formal printer receipt 81804.
template<std::size_t Dimensions, SourceMirSliceKind Kind, KnownSourceMirAbi SourceAbi>
    requires (Dimensions > 0 && Dimensions < 255
        && (Kind == SourceMirSliceKind::universal || Kind == SourceMirSliceKind::canonical
            || Kind == SourceMirSliceKind::contiguous)
        && !(Kind == SourceMirSliceKind::canonical && Dimensions == 1))
struct SourceMirSliceValueOrigin<Dimensions, Kind, SourceAbi> {
    static SourceMirSliceValueOrigins get() {
        try {
            std::string dimension = std::to_string(Dimensions);
            if constexpr (std::same_as<SourceAbi, SourceMirAbi64>) dimension.append("LU");
            else dimension.push_back('u');
            const auto kind = [] {
                if constexpr (Kind == SourceMirSliceKind::universal)
                    return RetainedString::fromStaticLiteral("mir_slice_kind.universal");
                else if constexpr (Kind == SourceMirSliceKind::canonical)
                    return RetainedString::fromStaticLiteral("mir_slice_kind.canonical");
                else return RetainedString::fromStaticLiteral("mir_slice_kind.contiguous");
            }();
            const SourceMirSliceValueContext context{RetainedString(std::move(dimension)), kind};
            // IntegerExp uses the enum's own declared name even under fullQual.
            return {context, context, context, context};
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
    }
};

template<core::cv::image::ImageDataElement T, class SourceAbi,
    class ValueOrigin = SourceMirSliceValueOrigin<3, SourceMirSliceKind::contiguous, SourceAbi>>
struct ImageSliceSourceOrigin {
    static SourceSliceTypeOrigin get()
        requires requires { ValueOrigin::get(); }
    {
        using namespace nicxlive::core::d_runtime;
        // image.d data/sliced and formal current-source receipt 75385 establish
        // mutable T[] for all three supported T, including const receivers.
        // Mir sliced's dynamic-array branch selects that array's element pointer.
        const auto qualifiedPixel = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedElement);
        const auto unqualifiedPixel = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedElement);
        const auto values = ValueOrigin::get();
        try {
            const auto context = [](const RetainedString& pixel, const SourceMirSliceValueContext& value) {
                std::string iterator(pixel.view());
                iterator.push_back('*');
                // Three explicit dimensions and no labels come from image.d100.
                // The complete Slice value itself has no added qualifier wrapper.
                return SourceSliceTypeContext{RetainedString(std::move(iterator)),
                    value.dimensionArgument, value.kindArgument, {},
                    RetainedString::emptyLiteral(), RetainedString::emptyLiteral()};
            };
            return {context(qualifiedPixel, values.qualifiedArgument),
                context(unqualifiedPixel, values.unqualifiedArgument),
                context(qualifiedPixel, values.qualifiedElement),
                context(unqualifiedPixel, values.unqualifiedElement)};
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
    }
};

// Distinct Source identity keys avoid assigning an Image-origin spelling to
// another producer which happens to reuse the same native view carrier.
template<core::cv::image::ImageDataElement T, KnownSourceMirAbi SourceAbi>
using ImageSliceSourceIdentity = SourceSliceDeclarationIdentity<ImageSliceSourceOrigin<T, SourceAbi>>;

} // namespace nijigenerate::compat
