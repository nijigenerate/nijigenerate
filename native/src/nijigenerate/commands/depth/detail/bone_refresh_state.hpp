#pragma once

#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <param/parameter.hpp>
#include <cstddef>
#include <memory>

namespace nijigenerate::commands::depth::bone::detail {

extern thread_local std::shared_ptr<ext::nodes::ExDepthRigRoot> lastDepthBoneDirtyRoot;
extern thread_local std::shared_ptr<nicxlive::core::param::Parameter> lastDepthBoneDirtyParameter;
extern thread_local compat::Vec2u lastDepthBoneDirtyKeypoint;
extern thread_local std::shared_ptr<actions::AsyncGroupAction> depthBoneRefreshActionSink;
extern thread_local std::size_t depthBoneRefreshSuppressed;

} // namespace nijigenerate::commands::depth::bone::detail
