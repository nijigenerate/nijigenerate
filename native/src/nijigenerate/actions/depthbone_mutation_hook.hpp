#pragma once

namespace nijigenerate::actions::depthboneinvalidation {

struct DepthBoneMutation;
using DepthBoneMutationHook = void (*)(DepthBoneMutation mutation);
extern DepthBoneMutationHook ngDepthBoneMutationHook;

} // namespace nijigenerate::actions::depthboneinvalidation
