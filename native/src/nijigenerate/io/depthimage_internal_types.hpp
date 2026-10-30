#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

// Module-private data counterparts, intentionally absent from depthimage.hpp.
namespace nijigenerate::io::depthimage::detail {
inline void appendIndexArray(compat::Slice<std::size_t>& values, std::size_t value) {
    try {
        values.append(value);
    } catch (const compat::OutOfMemoryError&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}

// Source initializers, element sizes and runtime branches are supplied by each
// primitive-array caller; native sizeof(T) does not define a Source layout.
template<class T> compat::Slice<T> initialPrimitiveArray(std::size_t count, const T& initialValue,
    std::size_t sourceElementBytes, std::size_t overflowLine, std::size_t allocationLine) {
    if (count == 0) return {};
    if (count > std::numeric_limits<std::size_t>::max() / sourceElementBytes) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), overflowLine);
    }
    try {
        return compat::Slice<T>(std::vector<T>(count, initialValue));
    } catch (const compat::OutOfMemoryError&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), allocationLine);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), allocationLine);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), allocationLine);
    }
}

// These callers assign a length to a null byte array. Construct the complete
// zero-initialized backing before publishing its retained descriptor.
template<class Output> Output initialByteArray(std::size_t count) {
    if constexpr (std::is_same_v<Output, compat::Slice<std::uint8_t>>) {
        if (count == 0) return {};
        try {
            return Output(std::vector<std::uint8_t>(count));
        } catch (const compat::OutOfMemoryError&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 877);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 877);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 877);
        }
    } else {
        Output result;
        result.resize(count);
        return result;
    }
}

inline compat::RetainedString copyEnforceMessage(std::string_view message) {
    if (message.empty()) return compat::RetainedString::fromStaticLiteral("Enforcement failed");
    try {
        return compat::RetainedString(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}

struct DepthDrawConvolutionSample {
    double value = std::numeric_limits<double>::quiet_NaN();
    bool valid = false;
};
struct TupleIndexValue {
    std::size_t index = 0;
    std::uint8_t value = 0;
};
struct DepthDrawHistogramIntegral {
    compat::Slice<compat::Slice<std::uint32_t>> bins;
    std::int32_t stride = 0;
};
struct DepthDrawHistogramStats {
    bool valid = false;
    std::int32_t p50 = 0;
    std::int32_t p65 = 0;
    std::int32_t p80 = 0;
};
struct DepthDrawMaskedDepthEstimate {
    bool valid = false;
    double depth = std::numeric_limits<double>::quiet_NaN();
    double confidence = std::numeric_limits<double>::quiet_NaN();
    double weight = std::numeric_limits<double>::quiet_NaN();
};
struct DepthDrawMaskedSparseGridDepths {
    compat::Slice<double> grid;
    compat::Slice<std::uint8_t> values;
};
} // namespace nijigenerate::io::depthimage::detail

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_IMAGE_PRIVATE_VALUE_ORIGIN(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::io::depthimage::detail::Type> { \
    inline static constexpr bool isNativeObject=false; \
    inline static constexpr char qualifiedArgument[]="(nijigenerate.io.depthimage." #Type ")"; \
    inline static constexpr char unqualifiedArgument[]="(" #Type ")"; \
    inline static constexpr char qualifiedElement[]="nijigenerate.io.depthimage." #Type; \
    inline static constexpr char unqualifiedElement[]=#Type; \
};
NG_DEPTH_IMAGE_PRIVATE_VALUE_ORIGIN(DepthDrawConvolutionSample)
NG_DEPTH_IMAGE_PRIVATE_VALUE_ORIGIN(TupleIndexValue)
NG_DEPTH_IMAGE_PRIVATE_VALUE_ORIGIN(DepthDrawHistogramIntegral)
NG_DEPTH_IMAGE_PRIVATE_VALUE_ORIGIN(DepthDrawHistogramStats)
NG_DEPTH_IMAGE_PRIVATE_VALUE_ORIGIN(DepthDrawMaskedDepthEstimate)
NG_DEPTH_IMAGE_PRIVATE_VALUE_ORIGIN(DepthDrawMaskedSparseGridDepths)
#undef NG_DEPTH_IMAGE_PRIVATE_VALUE_ORIGIN
} // namespace nicxlive::core::d_runtime
