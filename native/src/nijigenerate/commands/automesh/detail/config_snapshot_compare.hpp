#pragma once

#include "config_snapshot.hpp"

namespace nijigenerate::commands::automesh::config::detail {

inline bool _sameAutoMeshConfig(AutoMeshConfigSnapshot a, AutoMeshConfigSnapshot b) {
    return a.simple == b.simple && a.advanced == b.advanced;
}

} // namespace nijigenerate::commands::automesh::config::detail
