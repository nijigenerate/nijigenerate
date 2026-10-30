#pragma once

#include "bone_constants.hpp"
#include "bone_key_projection.hpp"
#include <nijigenerate/commands/command_table.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/actions/depthbone_mutation.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>

namespace nijigenerate::commands { class Command; }
namespace nijigenerate::actions { class GroupAction; class AsyncGroupAction; }
namespace nijigenerate::ext::nodes { class ExDepthRigRoot; }
namespace nicxlive::core::nodes { class Node; }
namespace nicxlive::core::param { class Parameter; }

namespace nijigenerate::commands::depth::bone {

enum class DepthBoneCommand : std::int32_t {
    CreateDepthRigRoot = 0,
    AddDepthBone = 1,
    AddStandardDepthSkeleton = 2,
    AddStandardDepthParameters = 3,
    FitDepthRigRootZToDepth = 4,
    FitDepthBoneZToDepth = 5,
    SetDepthBoneRest = 6,
    SetDepthBoneConstraint = 7,
    ListDepthBones = 8,
    AddDepthBoneSource = 9,
    RemoveDepthBoneSource = 10,
    ListDepthBoneSources = 11,
    SetDepthBoneSourceSettings = 12,
    SetDepthBoneInfluenceRule = 13,
    GetDepthBoneInfluenceRule = 14,
    PreviewDepthBoneInfluence = 15,
    PreviewDepthBoneDeform = 16,
    ApplyDepthBoneDeform = 17
};

enum class DepthBoneDirtyScope : std::int32_t {
    Keypoint = 0,
    AllKeypoints = 1,
};

using DepthBoneCommandMap = ::nijigenerate::commands::CommandTableStorage<DepthBoneCommand>;
extern thread_local DepthBoneCommandMap commands;

// Explicit shared-module startup, before any action/mutation producer is used.
void ngInitDepthBoneModule();

std::uint64_t ngCreateDepthBoneSourceSettingsMergeSession();
void ngBeginDepthBoneSourceSettingsMerge(std::uint64_t session, compat::RetainedString property);
void ngEndDepthBoneSourceSettingsMerge();

bool ngDepthRigNodeCurrentScaledDepth(std::shared_ptr<nicxlive::core::nodes::Node> node,
    float& localTranslationZ, float& rootDepth);
std::size_t ngDepthRigRootFittableDepthBoneCount(std::shared_ptr<ext::nodes::ExDepthRigRoot> root);
bool ngFitDepthRigNodeTranslationZToCurrentDepth(std::shared_ptr<nicxlive::core::nodes::Node> node);

void ngBeginDepthBoneRefreshActionSink(std::shared_ptr<actions::AsyncGroupAction> sink);
void ngEndDepthBoneRefreshActionSink(std::shared_ptr<actions::AsyncGroupAction> sink);

void ngMarkDepthBoneDirty(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    compat::RetainedStringSlice reason, DepthBoneDirtyScope dirtyScope = DepthBoneDirtyScope::Keypoint,
    std::uint32_t targetUuid = 0, bool settleBeforeDispatch = false);
void ngMarkDepthBoneDirtyForTarget(std::shared_ptr<nicxlive::core::nodes::Node> changedNode,
    compat::RetainedStringSlice reason);
void ngMarkDepthBoneDirtyForArmedParameter(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    compat::RetainedStringSlice reason, DepthBoneDirtyScope dirtyScope = DepthBoneDirtyScope::Keypoint);
void ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    compat::RetainedStringSlice reason);
void ngDepthBoneMutationChanged(actions::depthboneinvalidation::DepthBoneMutation mutation);

void ngFlushDepthBoneDirty();
bool ngFlushDepthBoneDirtyForFrame();
bool ngHasPendingDepthBoneRefresh();
void ngFlushDepthBoneDirtyImmediate();
bool ngHasPendingDepthBoneRefreshForSink(std::shared_ptr<actions::GroupAction> sink);
std::size_t ngPendingDepthBoneRefreshWorkForSink(std::shared_ptr<actions::GroupAction> sink);

}

#include "bone_commands.hpp"
#include "bone_effective_pivots.hpp"
#include "bone_gpu_queries.hpp"
#include "bone_packet_build.hpp"
