#pragma once

#include "source_image_slice_identity.hpp"
#include <cstddef>
#include <cstdint>

namespace nijigenerate::compat {

// Explicit Source pointer-iterator declaration. Instantiating this identity is
// a defining mapping, not a deduction from a native storage or view class.
template<class Element, std::size_t Dimensions, SourceMirSliceKind Kind, class SourceAbi>
struct DeclaredPointerSliceSourceOrigin {
    using ValueOrigin = SourceMirSliceValueOrigin<Dimensions, Kind, SourceAbi>;

    static SourceSliceTypeOrigin get() requires requires { ValueOrigin::get(); } {
        using namespace nicxlive::core::d_runtime;
        const auto qualifiedElement = ngSourceTypeSpelling<Element>(SourceTypeSpelling::QualifiedElement);
        const auto unqualifiedElement = ngSourceTypeSpelling<Element>(SourceTypeSpelling::UnqualifiedElement);
        const auto values = ValueOrigin::get();
        try {
            const auto context = [](const RetainedString& element, const SourceMirSliceValueContext& value) {
                std::string iterator(element.view());
                iterator.push_back('*');
                return SourceSliceTypeContext{RetainedString(std::move(iterator)),
                    value.dimensionArgument, value.kindArgument, {},
                    RetainedString::emptyLiteral(), RetainedString::emptyLiteral()};
            };
            return {context(qualifiedElement, values.qualifiedArgument),
                context(unqualifiedElement, values.unqualifiedArgument),
                context(qualifiedElement, values.qualifiedElement),
                context(unqualifiedElement, values.unqualifiedElement)};
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
    }
};

// Both types are written explicitly in the actual distanceTransform signature
// (distancetransform.d76), independently of C++ DistanceOutput's rank/storage.
template<KnownSourceMirAbi SourceAbi>
using DistanceTransformDistanceSliceSourceIdentity = SourceSliceDeclarationIdentity<
    DeclaredPointerSliceSourceOrigin<float, 2, SourceMirSliceKind::contiguous, SourceAbi>>;

template<KnownSourceMirAbi SourceAbi>
using DistanceTransformNearestSliceSourceIdentity = SourceSliceDeclarationIdentity<
    DeclaredPointerSliceSourceOrigin<std::int32_t, 3, SourceMirSliceKind::contiguous, SourceAbi>>;

} // namespace nijigenerate::compat
