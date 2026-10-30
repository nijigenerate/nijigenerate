#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::autorig::framework::detail {

inline bool safeId(compat::RetainedStringSlice id) {
    const auto value = id.view();
    if (value.size() == 0 || value == "." || value == "..") return false;
    for (unsigned char ch : value)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) return false;
    return true;
}

inline bool safeFileName(compat::RetainedStringSlice name) {
    const auto value = name.view();
    if (value.size() == 0 || value == "." || value == ".." ||
        value[value.size() - 1] == '.' || value[value.size() - 1] == ' ') return false;
    for (unsigned char ch : value)
        if (ch < ' ' || ch == '/' || ch == '\\' || ch == ':' || ch == '*' ||
            ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') return false;
    return true;
}

} // namespace nijigenerate::autorig::framework::detail
