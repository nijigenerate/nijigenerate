#include "detail/native_stage.hpp"
#include "native_state.hpp"
#include "observation.hpp"
#include "controls.hpp"
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
    if (stage == "plan-fixed-foot" || stage == "plan-rear-hair" || stage == "plan-cheek") {
        const auto digest = nativeString(indexNative(state, "model_sha256"));
        const auto baked = state.sourceObjectRef().if_contains("correction_base_sha256");
        enforceNative(baked && digest.view() == baked->as_string().view(),
            "Angle bake evidence is missing", 2042);
        if (stage == "plan-fixed-foot")
            task->publishEvidence("plan", FixedFootPlan{digest,
                controls::ngRigCompileFixedFootCorrections(state, program, task)});
        else if (stage == "plan-rear-hair")
            task->publishEvidence("plan", planRearCap(state, program, task));
        else {
            auto report = controls::ngRigCompileCheekCorrections(state, program, task);
            if (indexNative(report, "applicable").as_bool()) {
                const auto skin = uuid(indexNative(report, "skin"));
                if (task->eligibleTargets() && !task->eligibleTargets()->contains(skin)) {
                    task->recordUnit({{skin}, framework::AutoRigTaskState::Blocked,
                        "The skin Part lacks validated face-ridge evidence"});
                    report = objectEvidence({{stringLiteralEvidence("applicable"), false}});
                } else {
                    const auto model = framework::Text(std::to_string(uuid(indexNative(state, "rootId"))));
                    framework::AutoRigTargetAssociations associations{model, {}};
                    NativeIntegerMap<std::uint32_t, bool> seen;
                    for (const auto& operation : indexNative(report, "operations").sourceArrayValue()) {
                        const auto id = uuid(indexNative(operation, "part"));
                        if (seen.contains(id)) continue;
                        seen[id] = true;
                        associations.members.push_back({"face/cheek", framework::Text(std::string(model.view()) + "/face/cheek"), id});
                    }
                    task->publishEvidence("target-associations", std::move(associations));
                }
            }
            task->publishEvidence("plan", CheekPlan{digest, report});
        }
        return state;
    }
    if (stage == "prepare-shoulders" || stage == "detect-layered-ears" || stage == "detect-hip-attachments") {
        const auto pairs = stage == "prepare-shoulders" ? observation::ngRigPrepareShoulders(state, program, task)
            : stage == "detect-layered-ears" ? observation::ngRigPrepareLayeredEars(state, task)
            : observation::ngRigPrepareHipAttachments(state, program, task);
        const auto modelId = framework::Text(std::to_string(uuid(indexNative(state, "rootId"))));
        const framework::Text tag = stage == "prepare-shoulders" ? "body/shoulder" :
            stage == "detect-layered-ears" ? "head/ear" : "body/hip";
        framework::AutoRigTargetAssociations associations{modelId, {}};
        for (const auto& pair : pairs.sourceArrayValue()) {
            if (!indexNative(pair, "matching").as_bool()) continue;
            const auto source = uuid(indexNative(pair, "source"));
            const auto target = uuid(indexNative(pair, "target"));
            const auto unit = framework::Text(std::string(modelId.view()) + "/" + std::string(tag.view()) +
                "/" + std::to_string(source) + "/" + std::to_string(target));
            associations.members.push_back({tag, unit, source});
            associations.members.push_back({tag, unit, target});
        }
        task->publishEvidence("pairs", pairs);
        task->publishEvidence("target-associations", std::move(associations));
        return state;
    }
    const auto requireCorrectionBase = [&](const framework::Text& digest) {
        const auto correctionBase = state.sourceObjectRef().if_contains("correction_base_sha256");
        enforceNative(correctionBase && correctionBase->as_string().view() == digest.view(),
            "Correction evidence was collected for a different baked model", 2070);
    };
    std::shared_ptr<const FixedFootPlan> footPlan;
    std::shared_ptr<const RearCapPlan> rearPlan;
    std::shared_ptr<const CheekPlan> cheekPlan;
    if (stage == "apply-fixed-foot") {
        footPlan = task->inputEvidence<FixedFootPlan>("plan");
        requireCorrectionBase(footPlan->modelDigest);
    } else if (stage == "apply-rear-hair") {
        rearPlan = task->inputEvidence<RearCapPlan>("plan");
        requireCorrectionBase(rearPlan->modelDigest);
    } else if (stage == "apply-shape-corrections") {
        cheekPlan = task->inputEvidence<CheekPlan>("plan");
        requireCorrectionBase(cheekPlan->modelDigest);
    }
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
            if (const auto scope = task->targetScope())
                enforceNative(scope->modelIdentity.view() == std::to_string(current->root->getUUID()) &&
                    signature.view() == indexNative(state, "editor_signature").as_string().view(),
                    "Scoped AutoRig requires a checkpoint matching the current editor model", 2053);
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
        if (stage == "mesh-parts") {
            if (task->hasInput("shoulder-pairs"))
                assignNative(state, "shoulder_pairs", *task->inputEvidence<Json>("shoulder-pairs"));
            core::settings::SettingsJsonArray surfaces;
            for (const auto port : {"ear-pairs", "hip-pairs"})
                if (task->hasInput(port))
                    for (const auto& pair : task->inputEvidence<Json>(port)->sourceArrayValue()) surfaces.append(pair);
            assignNative(state, "surface_pairs", surfaces);
            state = meshParts(state, program, task, owner);
        }
        else if(stage=="prepare-source-groups")state=prepareSourceGroups(state,task);
        else if(stage=="register-source-uv")state=registerSourceUV(state,task,owner);
        else if(stage=="prepare-feature-composites")state=prepareFeatureComposites(state,program,task);
        else if(stage=="compile-domain-layout")state=registerDomainParents(state,program,task);
        else if(stage=="build-native-rig")state=buildRig(state,program,task);
        else if(stage=="weld-shoulders")state=weldShoulders(state,task);
        else if(stage=="validate-depth-inputs")state=validateDepthInputs(state,program,task);
        else if(stage=="bake-depth-angles")state=bakeAngles(state,task);
        else if(stage=="apply-rig-controls")state=applyControls(state,task);
        else if (footPlan) state = applyFixedFootCorrections(state, footPlan->report, task);
        else if (rearPlan) {
            if (task->targetScope() || task->eligibleTargets()) {
                RearCapPlan selected{rearPlan->modelDigest, core::settings::SettingsJsonArray{}, {}};
                for (const auto& operation : rearPlan->operations)
                    if (task->acceptsTarget(operation.part)) selected.operations.push_back(operation);
                auto silhouettes = selected.silhouettes.sourceArrayValue();
                for (const auto& silhouette : rearPlan->silhouettes.sourceArrayValue())
                    if (task->acceptsTarget(uuid(indexNative(silhouette, "part")))) silhouettes.append(silhouette);
                selected.silhouettes = silhouettes;
                assignNative(state, "rear_hair_silhouettes", applyRearCap(selected, task));
            } else assignNative(state, "rear_hair_silhouettes", applyRearCap(*rearPlan, task));
        }
        else if (cheekPlan) state = applyCheekCorrections(state,
            Json(cheekPlan->report.sourceObjectRef().duplicate()), task);
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
        if (stage == "bake-depth-angles")
            assignNative(state, "correction_base_sha256", indexNative(state, "model_sha256"));
        assignNative(state,"completed_stage",core::settings::SettingsString(stageValue));
        Json details=core::settings::SettingsJsonObject{};
        for(auto key:{"prepared_groups","feature_composites","origin_composites","source_group_grids","shoulder_pairs","surface_pairs","source_uv_registration","depth_validation","shape_corrections","fixed_foot_corrections"})
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
