#pragma once
#include "nijigenerate/compat/phobos_string_hash.hpp"
#include "nijigenerate/compat/retained_string.hpp"

namespace nijigenerate::commands::view::panel {
struct PanelKey {
    compat::RetainedString name;
    compat::RetainedString toString() const noexcept { return name; }
    std::size_t toHash() const noexcept { return compat::ngPhobosStringHash(name.view()); }
    bool opEquals(const PanelKey& rhs) const noexcept { return name == rhs.name; }
    friend bool operator==(const PanelKey& left, const PanelKey& right) noexcept { return left.opEquals(right); }
};
}
