#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include <nodes/node.hpp>
#include <memory>

namespace nijigenerate::commands::depth::bone::detail {

// Original final class identity, independent of Node inheritance. The child
// callback and genuine bound-delegate listener bridge remain separate gates.
class DepthBoneEffectivePivotObserver final {
public:
    DepthBoneEffectivePivotObserver() = default;
    DepthBoneEffectivePivotObserver(const DepthBoneEffectivePivotObserver&) = delete;
    DepthBoneEffectivePivotObserver& operator=(const DepthBoneEffectivePivotObserver&) = delete;
    void onChange(std::shared_ptr<nicxlive::core::nodes::Node> target,
        nicxlive::core::nodes::NotifyReason reason);
};

extern thread_local std::shared_ptr<DepthBoneEffectivePivotObserver> depthBoneEffectivePivotObserver;

} // namespace nijigenerate::commands::depth::bone::detail
