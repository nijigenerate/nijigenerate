#pragma once

#include "../framework_schema.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <string>

namespace nijigenerate::autorig::framework::detail {

inline bool hasPort(compat::Slice<AutoRigPortSpec> ports, compat::RetainedStringSlice id) {
    for (const auto port : ports) if (port.id == id) return true;
    return false;
}

inline AutoRigValueKind portKind(compat::Slice<AutoRigPortSpec> ports, compat::RetainedStringSlice id) {
    for (const auto port : ports) if (port.id == id) return port.kind;
    throw compat::ThrowableException(compat::RetainedString("Unknown AutoRig port: " + std::string(id.view())));
}

} // namespace nijigenerate::autorig::framework::detail
