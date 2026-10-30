#include "detail/native_stage.hpp"
#include "native_state.hpp"
#include "observation.hpp"
#include "review.hpp"
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/commands/depth/detail/bone_action_scope.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/io/project_writer.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
namespace nijigenerate::autorig::deterministic::native {
using namespace native_detail;
using namespace evidence::detail;
Json ngRigNativeStage(compat::RetainedStringSlice stageValue,Json state,Json program,
    compat::ConstSlice<std::uint8_t> model,Task task) {
    const auto stage=stageValue.view();
    ngRigCheckpoint(task);
    if(stage=="observe-model")state=observeModel(state,task);
    else {
        enforceNative(!model.empty(),"Missing committed model snapshot",2047);
        enforceNative(bytesDigest(model.view()).view()==indexNative(state,"model_sha256").as_string().view(),
            "Model snapshot hash mismatch",2048);
        task->runOnMainThread([&] {
            auto current=project::incActivePuppet();
            enforceNative(current&&current->root->getUUID()==uuid(indexNative(state,"rootId")),
                "Active model changed since the previous AutoRig stage",2051);
            auto signature=editorSignature(current);
            if(signature.view()!=indexNative(state,"editor_signature").as_string().view()) {
                auto expected=liveRigSignatures.find(task->sessionId());
                enforceNative(expected!=liveRigSignatures.end()&&signature==expected->second,
                    "Editor model changed since the previous AutoRig stage; regenerate its observation",2056);
                project::ngRestorePuppetMemory(compat::Slice<std::uint8_t>(model.begin(),model.end()));
            }
        });
    }
    if(stage=="verify-saved-rig") {auto result=verifyRig(state,program,task);task->publishJson("review",review::ngRigReviewCompact(result));return result;}
    auto before=reviewModel(task);std::shared_ptr<actions::AsyncGroupAction> owner;
    task->runOnMainThread([&] {
        owner=createNativeObject<actions::AsyncGroupAction>();core::actionstack::incActionPushGroup(owner);
        commands::depth::bone::ngBeginDepthBoneRefreshActionSink(owner);owner->beginCheckpointRecording();
    });
    auto cleanup=[&] {task->runOnMainThread([&] {owner->endCheckpointRecording();commands::depth::bone::ngEndDepthBoneRefreshActionSink(owner);core::actionstack::incActionPopGroup();});};
    Json returned;
    compat::ngRunWithScopeExit([&] {
      bool bodySucceeded=false;
      compat::ngRunWithScopeExit([&] {
        if(stage=="mesh-parts")state=meshParts(state,program,task);
        else if(stage=="prepare-shoulders")assignNative(state,"shoulder_pairs",observation::ngRigPrepareShoulders(state,program,task));
        else if(stage=="prepare-source-groups")state=prepareSourceGroups(state,task);
        else if(stage=="register-source-uv")state=registerSourceUV(state,task);
        else if(stage=="prepare-feature-composites")state=prepareFeatureComposites(state,program,task);
        else if(stage=="compile-domain-layout")state=registerDomainParents(state,program,task);
        else if(stage=="build-native-rig")state=buildRig(state,program,task);
        else if(stage=="weld-shoulders")state=weldShoulders(state,task);
        else if(stage=="validate-depth-inputs")state=validateDepthInputs(state,program,task);
        else if(stage=="bake-depth-angles")state=bakeAngles(state,task);
        else if(stage=="apply-rig-controls")state=applyControls(state,task);
        else if(stage=="apply-shape-corrections")state=applyCheekCorrections(state,program,task);
        else if(stage=="apply-secondary-physics")state=applyPhysics(state,program,task);
        else enforceNativeLazy(stage=="observe-model",[&] {
            return nativeText({"Unknown native rig stage: ",stage});
        },2101);
        ngRigCheckpoint(task);settleDepthRefresh(task);compat::Slice<std::uint8_t> snapshot;
        task->runOnMainThread([&] {
            auto puppet=project::incActivePuppet();puppet->root->notifyChange(puppet->root,nicxlive::core::nodes::NotifyReason::StructureChanged);
            snapshot=compat::Slice<std::uint8_t>(io::ngWriteProjectPuppetMemory(*project::incActivePuppet()));
            assignNative(state,"editor_signature",core::settings::SettingsString(editorSignature(project::incActivePuppet())));
            const auto session = task->sessionId();
            const auto signature = indexNative(state,"editor_signature").as_string().retainedString();
            liveRigSignatures[session]=signature;
        });
        assignNative(state,"model_sha256",bytesDigest({snapshot.data(),snapshot.size()}));
        assignNative(state,"completed_stage",core::settings::SettingsString(stageValue));
        Json details=core::settings::SettingsJsonObject{};
        for(auto key:{"prepared_groups","feature_composites","origin_composites","source_group_grids","shoulder_pairs","source_uv_registration","depth_validation","shape_corrections","fixed_foot_corrections"})
            if(auto value=state.sourceObjectRef().if_contains(key))assignNative(details,
                compat::RetainedString::fromStaticView(key),review::ngRigReviewCompact(*value));
        auto resultReview=objectEvidence({{stringLiteralEvidence("stage"),core::settings::SettingsString(stageValue)},
            {stringLiteralEvidence("changes"),review::ngRigReviewChanges(before,reviewModel(task))},{stringLiteralEvidence("details"),details}});
        if(stage!="observe-model")task->publishJson("review",resultReview);task->publishBlob("model",snapshot);
        // Capture the D return value before running any scope guards.
        returned=state;
        bodySucceeded=true;
      },[&] {
        if(!bodySucceeded&&!model.empty())task->runOnMainThread([&] {
            project::ngRestorePuppetMemory(compat::Slice<std::uint8_t>(model.begin(),model.end()));
            const auto session = task->sessionId();
            const auto signature = editorSignature(project::incActivePuppet());
            liveRigSignatures[session]=signature;
        });
      });
      // A success-handler failure reaches scope(exit), not the earlier rollback.
      task->runOnMainThread([&]{owner->retainCompletedResults();});
    },cleanup);
    return returned;
}
}
