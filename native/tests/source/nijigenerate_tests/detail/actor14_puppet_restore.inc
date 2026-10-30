// Existing regression.d1529-1571; include inside regression::detail after helpers.
void testPuppetMemoryRestoreDirtyState() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::AddNodeCommand;
    using nijigenerate::commands::node::node::SetNodeNameCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::project;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    Slice<std::uint8_t> snapshot(nijigenerate::io::ngWriteProjectPuppetMemory(
        *ngRequireObject(incActivePuppet())));
    auto originalPath = ngRequireObject(incActiveProject())->path;
    incActionInvalidateSavedState();
    ngRestorePuppetMemory(snapshot);
    require(incActionIsModified(), RetainedString::fromStaticLiteral("Snapshot rollback must retain unsaved changes"));
    require(ngRequireObject(incActiveProject())->path == originalPath,
        RetainedString::fromStaticLiteral("Snapshot rollback must retain the project path"));
    incActionMarkSaved();
    ngRestorePuppetMemory(snapshot);
    require(!incActionIsModified(), RetainedString::fromStaticLiteral("Restoring an unchanged clean snapshot must remain clean"));
    resetCase();
    auto originalPuppet = incActivePuppet();
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(originalPuppet);
    auto node = ngRequireObject(constructSourceCommand<AddNodeCommand>(
        RetainedString::fromStaticLiteral("Node"))->run(ctx))->created[0];
    ctx->nodes(Context::Nodes{node});
    struct BeforeCheckpointArgument {
        operator Slice<RetainedString>() && {
            return Slice<RetainedString>{RetainedString::fromStaticLiteral("before-checkpoint")};
        }
    };
    require(ngRequireObject(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{},
        BeforeCheckpointArgument{})->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("Pre-rig edit should succeed"));
    Slice<std::uint8_t> checkpoint(nijigenerate::io::ngWriteProjectPuppetMemory(
        *ngRequireObject(originalPuppet)));
    incActionPushGroup();
    struct PartialStageArgument {
        operator Slice<RetainedString>() && {
            return Slice<RetainedString>{RetainedString::fromStaticLiteral("partial-stage")};
        }
    };
    require(ngRequireObject(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{},
        PartialStageArgument{})->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("Partial stage edit should succeed"));
    ngRestorePuppetMemory(checkpoint);
    incActionPopGroup();
    require(findDirectNode(incActivePuppet(), "before-checkpoint") != nullptr,
        RetainedString::fromStaticLiteral("Rollback should restore checkpoint content"));
    incActionUndo();
    require(incActivePuppet() == originalPuppet && ngRequireObject(node)->getName() == "before-checkpoint",
        RetainedString::fromStaticLiteral("Undoing checkpoint rollback must restore the original object identities and undo partial edits"));
    incActionUndo();
    require(ngRequireObject(node)->getName() != "before-checkpoint",
        RetainedString::fromStaticLiteral("Pre-rig undo history must remain usable after rollback"));
    incActionRedo();
    incActionRedo();
    require(incActivePuppet() != originalPuppet && findDirectNode(incActivePuppet(), "before-checkpoint") != nullptr,
        RetainedString::fromStaticLiteral("Redo must restore the replacement model and checkpoint content"));
    ctx->puppet(incActivePuppet());
    ctx->nodes(Context::Nodes{findDirectNode(ctx->puppet(), "before-checkpoint")});
    struct LatestSavedArgument {
        operator Slice<RetainedString>() && {
            return Slice<RetainedString>{RetainedString::fromStaticLiteral("latest-saved")};
        }
    };
    require(ngRequireObject(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{},
        LatestSavedArgument{})->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("New saved content should differ from checkpoint"));
    incActionMarkSaved();
    ngRestorePuppetMemory(checkpoint);
    require(incActionIsModified(),
        RetainedString::fromStaticLiteral("Restoring different content must not mark an older checkpoint saved"));
}
