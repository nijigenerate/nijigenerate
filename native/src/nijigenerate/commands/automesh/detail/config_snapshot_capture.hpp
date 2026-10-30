#pragma once
#include "config_snapshot.hpp"
#include <nijigenerate/viewport/vertex/automesh/meta.hpp>
#include <memory>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::commands::automesh::config::detail {

inline AutoMeshConfigSnapshot _autoMeshConfigSnapshot(
        std::shared_ptr<viewport::vertex::automesh::meta::IAutoMeshReflect> reflect) {
    // Braced initializer clauses preserve the source Simple-then-Advanced calls.
    // No null filtering, exception translation or cloning is authored here.
    return {compat::ngRequireObject(reflect)->values(compat::RetainedString::fromStaticLiteral("Simple")),
            compat::ngRequireObject(reflect)->values(compat::RetainedString::fromStaticLiteral("Advanced"))};
}

} // namespace nijigenerate::commands::automesh::config::detail
