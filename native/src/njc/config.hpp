#pragma once

#include "config_errors.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <compat/object_metadata.hpp>
#include <cstdint>

namespace njc::config {

using Text = nijigenerate::compat::RetainedStringSlice;
struct Endpoint {
    Text host{nijigenerate::compat::RetainedString::fromStaticLiteral("127.0.0.1")};
    std::uint16_t port = 8088;
    Text path{nijigenerate::compat::RetainedString::fromStaticLiteral("/mcp")};
    Text bearer;
};
struct Options {
    Endpoint endpoint;
    nijigenerate::compat::Slice<Text> command;
    bool help = false;
    bool rawJson = false;
    bool summary = false;
};

Options parseOptions(nijigenerate::compat::Slice<nijigenerate::compat::RetainedStringSlice> args);
void applyUrl(Endpoint& endpoint, nijigenerate::compat::RetainedStringSlice url);
nijigenerate::compat::RetainedStringSlice usage();

} // namespace njc::config

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::njc::config::Endpoint> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(njc.config.Endpoint)";
    inline static constexpr char unqualifiedArgument[] = "(Endpoint)";
    inline static constexpr char qualifiedElement[] = "njc.config.Endpoint";
    inline static constexpr char unqualifiedElement[] = "Endpoint";
};
template<> struct SourceTypeSpellings<::njc::config::Options> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(njc.config.Options)";
    inline static constexpr char unqualifiedArgument[] = "(Options)";
    inline static constexpr char qualifiedElement[] = "njc.config.Options";
    inline static constexpr char unqualifiedElement[] = "Options";
};
} // namespace nicxlive::core::d_runtime
