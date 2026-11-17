#pragma once
#include "contracts.hpp"
#include "native_commands.hpp"
#include "native_mapping.hpp"
#include "native_numbers.hpp"
#include "native_processor.hpp"
#include <nijigenerate/autorig/framework.hpp>
#include <span>
#include <vector>
namespace nijigenerate::actions { class AsyncGroupAction; }
namespace nijigenerate::autorig::deterministic::native_detail {
using Json=core::settings::SettingsJsonValue;
using Task=std::shared_ptr<framework::AutoRigTaskContext>;
using NodeRef=std::shared_ptr<nicxlive::core::nodes::Node>;
using PuppetRef=std::shared_ptr<nicxlive::core::Puppet>;
using ParameterRef=std::shared_ptr<nicxlive::core::param::Parameter>;
Json reviewSettings(Task);
Json reviewModel(Task);
std::shared_ptr<commands::Context> editorContext(compat::Slice<NodeRef> nodes={});
ParameterRef parameterByName(PuppetRef,framework::Text);
Json verifySourceFrames(Json,Task,bool collectFailures=false);
Json renderFrame(Task,framework::Text,Json cameraFrame={});
Json renderValidation(Json,Task);
compat::RetainedString editorSignature(PuppetRef);
void settleDepthRefresh(Task);
Json observeModel(Json,Task);
Json prepareSourceGroups(Json,Task);
Json meshParts(Json,Json,Task,const std::shared_ptr<actions::AsyncGroupAction>&);
Json registerSourceUV(Json,Task,const std::shared_ptr<actions::AsyncGroupAction>&);
Json prepareFeatureComposites(Json,Json,Task);
Json registerDomainParents(Json,Json,Task);
Json buildRig(Json,Json,Task);
Json applyControls(Json,Task);
Json weldShoulders(Json,Task);
struct FixedFootPlan { framework::Text modelDigest; Json report; };
struct CheekPlan { framework::Text modelDigest; Json report; };
struct RearCapOperation {
    std::uint32_t part;
    std::array<unsigned, 2> key;
    compat::Slice<float> offsets;
};
struct RearCapPlan {
    framework::Text modelDigest;
    Json silhouettes;
    std::vector<RearCapOperation> operations;
};
RearCapPlan planRearCap(Json,Json,Task);
Json applyRearCap(const RearCapPlan&,Task);
Json applyFixedFootCorrections(Json,Json,Task);
Json applyCheekCorrections(Json,Json,Task);
Json validateDepthInputs(Json,Json,Task);
Json bakeAngles(Json,Task);
Json verifyRig(Json,Json,Task);
Json applyPhysics(Json,Json,Task);
}
namespace nijigenerate::autorig::deterministic::native {
core::settings::SettingsJsonValue ngRigNativeStage(compat::RetainedStringSlice stage,
    core::settings::SettingsJsonValue state,core::settings::SettingsJsonValue program,
    compat::ConstSlice<std::uint8_t> model,std::shared_ptr<framework::AutoRigTaskContext> task);
}
