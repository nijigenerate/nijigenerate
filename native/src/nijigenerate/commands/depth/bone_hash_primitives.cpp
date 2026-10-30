#include "detail/bone_hash.hpp"
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cmath>
#include <limits>
#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#elif defined(_M_ARM64) && !defined(__clang__)
#include <arm64_neon.h>
#endif

namespace nijigenerate::commands::depth::bone::detail {

#if !defined(_M_X64) && !defined(__x86_64__)
std::int64_t convertDepthBoneQuantizedFloatToLong(float value) {
#if defined(__aarch64__) || defined(_M_ARM64)
#if defined(__GNUC__) || defined(__clang__)
    // FCVTZS Xd, Sn truncates directly from the original binary32 operand.
    // Keep the actual instruction's FPSR/trap and invalid-input behavior;
    // a C++ floating-to-integral cast would instead permit undefined behavior.
    std::int64_t result;
    __asm__ volatile("fcvtzs %x0, %s1" : "=r"(result) : "w"(value));
    return result;
#else
    // MSVC exposes the ACLE scalar signed64 conversion through arm64_neon.h.
    // Binary32 values widen exactly to binary64, including the infinities
    // produced by the source multiplication. Do not change FPCR or clamp.
    // https://arm-software.github.io/acle/neon_intrinsics/advsimd.html
    return vcvtd_s64_f64(static_cast<double>(value));
#endif
#elif defined(__i386__) || defined(_M_IX86)
    // FISTP uses the x87 rounding-control field. Save every other control bit,
    // request truncation, and restore the original control word after popping
    // the actual signed64 result. This does not require the SSE3 FISTTP ISA.
    std::uint16_t originalControl;
    std::uint16_t truncatingControl;
    std::int64_t result;
#if defined(_MSC_VER) && !defined(__clang__)
    __asm fnstcw originalControl
    truncatingControl = static_cast<std::uint16_t>(originalControl | 0x0c00u);
    __asm {
        fldcw truncatingControl
        fld value
        fistp result
        fldcw originalControl
    }
#else
    __asm__ volatile("fnstcw %0" : "=m"(originalControl));
    truncatingControl = static_cast<std::uint16_t>(originalControl | 0x0c00u);
    __asm__ volatile(
        "fldcw %2\n\t"
        "flds %3\n\t"
        "fistpq %0\n\t"
        "fldcw %1"
        : "=m"(result)
        : "m"(originalControl), "m"(truncatingControl), "m"(value)
        : "st", "memory");
#endif
    return result;
#else
    // The standard fenv provider supplies the same truncating conversion on
    // other targets. llrint preserves the implementation's actual invalid and
    // inexact behavior, including an invalid result outside the integer range.
    // N1570 7.6p8, 7.6.3.2, 7.12.9.5 and F.10.6.5. FE_TOWARDZERO denotes a
    // rounding mode supported by this implementation's get/set functions.
    static_assert(std::numeric_limits<long long>::digits == 63);
    const auto originalRounding = std::fegetround();
    const auto originalErrno = errno;
    if (originalRounding != FE_TOWARDZERO) std::fesetround(FE_TOWARDZERO);
    const auto result = std::llrint(value);
    if (originalRounding != FE_TOWARDZERO) std::fesetround(originalRounding);
    errno = originalErrno;
    return result;
#endif
}
#endif

std::uint64_t hashMix(std::uint64_t seed, std::uint64_t value) {
    seed ^= value + UINT64_C(0x9e3779b97f4a7c15) + (seed << 6) + (seed >> 2);
    return seed;
}

std::uint64_t hashBool(std::uint64_t seed, bool value) {
    return hashMix(seed, value ? UINT64_C(1) : UINT64_C(0));
}

std::uint64_t hashInt(std::uint64_t seed, std::int64_t value) {
    return hashMix(seed, static_cast<std::uint64_t>(value));
}

std::uint64_t hashFloat(std::uint64_t seed, float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559 &&
        std::numeric_limits<float>::radix == 2 && std::numeric_limits<float>::digits == 24 &&
        std::numeric_limits<float>::max_exponent == 128 && std::numeric_limits<float>::min_exponent == -125);
    // Phobos isFinite reads exponent bits, including for a signaling NaN.
    const auto exponent = std::bit_cast<std::uint32_t>(value) & UINT32_C(0x7f800000);
    if (exponent == UINT32_C(0x7f800000)) return hashMix(seed, UINT64_C(0x7ff80000));
    const float quantized = value * 1000000.0f;
#if defined(_M_X64) || defined(__x86_64__)
    // Actual CVTTSS2SI conversion includes the hardware invalid-range behavior.
    // Its equivalence to the selected D compiler still requires observation.
    const auto integer = static_cast<std::int64_t>(_mm_cvttss_si64(_mm_set_ss(quantized)));
#else
    const auto integer = convertDepthBoneQuantizedFloatToLong(quantized);
#endif
    return hashInt(seed, integer);
}

std::uint64_t hashVec2(std::uint64_t seed, nicxlive::core::math::Vec2 value) {
    seed = hashFloat(seed, value.x);
    return hashFloat(seed, value.y);
}

std::uint64_t hashVec3(std::uint64_t seed, nicxlive::core::math::Vec3 value) {
    seed = hashFloat(seed, value.x);
    seed = hashFloat(seed, value.y);
    return hashFloat(seed, value.z);
}

std::uint64_t hashMatrix(std::uint64_t seed, nicxlive::core::math::Mat4 matrix) {
    for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) seed = hashFloat(seed, matrix[x][y]);
    }
    return seed;
}

std::uint64_t hashStringValue(std::uint64_t seed, compat::RetainedStringSlice value) {
    for (unsigned char byte : value.view()) seed = hashMix(seed, static_cast<std::uint64_t>(byte));
    return seed;
}

} // namespace nijigenerate::commands::depth::bone::detail
