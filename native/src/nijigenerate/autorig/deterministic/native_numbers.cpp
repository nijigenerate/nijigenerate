#include "native_numbers.hpp"
#include "detail/native_memory.hpp"
#include "contracts_json.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <cmath>
#include <limits>

namespace nijigenerate::autorig::deterministic::native_detail {

std::uint32_t uuid(core::settings::SettingsJsonValue value) {
    const double number = contracts::ngRigNumber(value);
    // Bounds must short-circuit before the floating-to-integer conversion.
    if (!(number >= 0 && number <= std::numeric_limits<std::uint32_t>::max() &&
        number == static_cast<std::uint32_t>(number)))
        throw compat::ThrowableException(copyNativeMessage(compat::RetainedString::fromStaticLiteral("Invalid native rig UUID")),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),210);
    return static_cast<std::uint32_t>(number);
}

compat::Slice<float> nativeNumbers(core::settings::SettingsJsonValue value) {
    compat::Slice<float> result;
    // D materializes ngRigNumbers completely before entering the float loop.
    const auto numbers = contracts::ngRigNumbers(value);
    for (const double number : numbers) {
        if (!(std::abs(number) <= std::numeric_limits<float>::max()))
            throw compat::ThrowableException(
                copyNativeMessage(compat::RetainedString::fromStaticLiteral("Rig coordinate exceeds native range")),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),217);
        result.append(static_cast<float>(number));
    }
    return result;
}

} // namespace nijigenerate::autorig::deterministic::native_detail
