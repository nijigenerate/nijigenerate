#pragma once
#include <string_view>

namespace nijigenerate::config {
inline constexpr std::string_view INC_BANNER_ARTIST_NAME = "seagetch";
inline constexpr std::string_view INC_BANNER_ARTIST_PAGE = "https://twitter.com/seagetch";
inline constexpr std::string_view INC_BUG_REPORT_URI =
    "https://github.com/nijigenerate/nijigenerate/issues/new?assignees=&labels=bug&template=bug-report.yml&title=%5BBUG%5D";
inline constexpr std::string_view INC_FEATURE_REQ_URI =
    "https://github.com/nijigenerate/nijigenerate/issues/new?assignees=&labels=enhancement&template=feature_request.yml&title=%5BFeature+Request%5D";
inline constexpr std::string_view INC_INFO_WEBSITE_URI = "https://github.com/nijigenerate/";
inline constexpr std::string_view INC_INFO_DOCS_URI = "https://docs.github.com/nijigenerate/";
inline constexpr std::string_view INC_INFO_DISCORD_URI = "";
inline constexpr bool INC_INFO_SHOW_DONATE_LINKS = true;
}
