#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <tuple>

#if defined(NG_REGRESSION_SMOKE)
namespace nijigenerate::regression_smoke {

struct RegressionSmokeOptions {
    bool enabled = false;
    bool computerUse = false;
    compat::RetainedString scenario;
    int frames = 6;
    int frameDelayMs = 0;
};

RegressionSmokeOptions ngParseRegressionSmokeOptions(compat::Slice<compat::RetainedString> args);
void ngRegressionSmokeFail(compat::RetainedString message);
bool ngRegressionSmokeFailed();
compat::RetainedString ngRegressionSmokeFailureMessage();
void ngSetupRegressionSmokeScenario(compat::RetainedString scenario);

} // namespace nijigenerate::regression_smoke

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::regression_smoke::RegressionSmokeOptions> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.regression_smoke.RegressionSmokeOptions)";
    inline static constexpr char unqualifiedArgument[] = "(RegressionSmokeOptions)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.regression_smoke.RegressionSmokeOptions";
    inline static constexpr char unqualifiedElement[] = "RegressionSmokeOptions";
};
}
namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::regression_smoke::RegressionSmokeOptions> {
    using Value = ::nijigenerate::regression_smoke::RegressionSmokeOptions;
    inline static constexpr std::string_view typeName = "RegressionSmokeOptions";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("enabled",offsetof(Value,enabled),&Value::enabled),
            ngFormatAggregateMember("computerUse",offsetof(Value,computerUse),&Value::computerUse),
            ngFormatAggregateMember("scenario",offsetof(Value,scenario),&Value::scenario),
            ngFormatAggregateMember("frames",offsetof(Value,frames),&Value::frames),
            ngFormatAggregateMember("frameDelayMs",offsetof(Value,frameDelayMs),&Value::frameDelayMs)};
    }
};
}
#endif
