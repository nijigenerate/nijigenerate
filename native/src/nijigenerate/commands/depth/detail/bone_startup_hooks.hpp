#pragma once

#include "../bone.hpp"
#include <nijigenerate/actions/depthbone_mutation_hook.hpp>
#include <nijigenerate/actions/package.hpp>

namespace nijigenerate::commands::depth::bone {

namespace detail {
std::shared_ptr<actions::AsyncGroupAction> claimDepthBoneAsyncGroupAction(actions::ActionPtr action);
}

} // namespace nijigenerate::commands::depth::bone
