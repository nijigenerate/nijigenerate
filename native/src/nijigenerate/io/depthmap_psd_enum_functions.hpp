#pragma once

#include <nijigenerate/io/depthmap_psd_types.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <cstddef>
#include <source_location>
#include <string>
#include <utility>

namespace nijigenerate::io::depthmap_psd {

class PsdDepthEnumParseError : public compat::ThrowableException {
public:
    explicit PsdDepthEnumParseError(std::string text,
        std::source_location source = std::source_location::current());
    PsdDepthEnumParseError(std::string text, compat::RetainedString sourceFile, std::size_t sourceLine);
};

// A checked final-switch failure is not an ordinary recoverable Exception.
class PsdDepthEnumSwitchError : public compat::SwitchError {
public:
    explicit PsdDepthEnumSwitchError(std::source_location source = std::source_location::current())
        : SwitchError(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"), source) {}
    PsdDepthEnumSwitchError(compat::RetainedString sourceFile, std::size_t sourceLine)
        : SwitchError(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            sourceFile, sourceLine) {}
    PsdDepthEnumSwitchError(nicxlive::core::d_runtime::SourceErrorViewTag tag,
            const compat::Throwable& source) noexcept : SwitchError(tag, source) {}
};

PsdDepthConvolution ngPsdDepthConvolutionFromString(compat::RetainedString value);
PsdDepthChannel ngPsdDepthChannelFromString(compat::RetainedString value);
PsdDepthMissingPolicy ngPsdDepthMissingPolicyFromString(compat::RetainedString value);
compat::RetainedString ngPsdDepthConvolutionName(PsdDepthConvolution value);
compat::RetainedString ngPsdDepthChannelName(PsdDepthChannel value);
compat::RetainedString ngPsdDepthMissingPolicyName(PsdDepthMissingPolicy value);
compat::RetainedString ngPsdDepthCompositionModeName(PsdDepthCompositionMode value);
compat::RetainedString ngPsdDepthCompositeSourceKindName(PsdDepthCompositeSourceKind value);

}
