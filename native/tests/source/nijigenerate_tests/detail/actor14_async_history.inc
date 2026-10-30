// Existing regression.d17816-18066; include inside regression::detail after helpers.
void testAsyncActionGroupUndoRedo() {
    using namespace nijigenerate::actions;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::actions::asyncprogress::AsyncGroupActionProgress;
    using nijigenerate::actions::node::detail::createNodeAction;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using NameAction = nijigenerate::actions::node::NodeValueChangeAction<Node, NullableString>;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto newName = []<std::size_t N, class Next>(std::shared_ptr<Node> node,
        const char (&previous)[N], Next next) {
        return createNodeAction<NameAction>([&](NameAction* storage) {
            auto label = RetainedString::fromStaticLiteral("name");
            auto target = node;
            auto old = NullableString::fromStaticLiteral(previous);
            auto value = next();
            auto pointer = std::shared_ptr<NullableString>(node, &node->name);
            ::new (static_cast<void*>(storage)) NameAction(label, target, old, value, pointer);
        });
    };
    const auto newAsync = [](auto buildArguments) {
        return createNodeAction<AsyncGroupAction>([&](AsyncGroupAction* storage) {
            auto arguments = buildArguments();
            ::new (static_cast<void*>(storage)) AsyncGroupAction(arguments);
        });
    };
    const auto newEmptyAsync = [&] { return newAsync([] { return ActionSlice<ActionPtr>{}; }); };
    resetCase();
    auto primaryNode = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto derivedNode = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    primaryNode->setName(NullableString::fromStaticLiteral("primary-before"));
    derivedNode->setName(NullableString::fromStaticLiteral("derived-before"));
    primaryNode->setName(NullableString::fromStaticLiteral("primary-after"));
    auto primary = newName(primaryNode, "primary-before", [&] { return primaryNode->name; });
    auto owner = newAsync([&] { return ActionSlice<ActionPtr>{primary}; });
    auto observedCell = std::allocate_shared<Slice<AsyncGroupActionEvent>>(SourceAllocator<Slice<AsyncGroupActionEvent>>{});
    auto& observedEvents = *observedCell;
    auto observerId = owner->addObserver([observedCell](std::shared_ptr<AsyncGroupAction>, AsyncGroupActionEvent event) {
        observedCell->append(event);
    });
    owner->markAsyncScheduled(2);
    owner->markAsyncRunning();
    owner->markAsyncFinished();
    check(owner->totalAsyncCount() == 2 && owner->completedAsyncCount() == 1 && owner->pendingAsyncCount() == 1,
        "async group should expose subsystem-independent progress counters");
    check(observedEvents.size() >= 3 && observedEvents[observedEvents.size() - 1] == AsyncGroupActionEvent::Progressed,
        "async group observers should receive incremental progress events");
    auto firstGeneration = owner->generation();
    derivedNode->setName(NullableString::fromStaticLiteral("derived-after"));
    auto derived = newName(derivedNode, "derived-before", [&] { return derivedNode->name; });
    check(owner->addCompletedAsyncAction(firstGeneration, derived),
        "completed asynchronous action should attach to its owner generation");
    check(owner->completedAsyncCount() == owner->totalAsyncCount() &&
        observedEvents[observedEvents.size() - 1] == AsyncGroupActionEvent::Completed,
        "async group completion should finish generic progress and notify observers");
    incActionPush(owner);
    incActionUndo();
    check(primaryNode->name == "primary-before" && derivedNode->name == "derived-before",
        "async group undo should revert completed output before its primary operation");
    check(owner->state() == AsyncGroupActionState::Undone, "async group should retain an explicit undone state");
    check(owner->pendingAsyncCount() == 0 && owner->totalAsyncCount() == 0 &&
        observedEvents[observedEvents.size() - 1] == AsyncGroupActionEvent::Canceled,
        "undo should cancel generic progress immediately without waiting for worker cleanup");
    check(!owner->addCompletedAsyncAction(firstGeneration, derived),
        "completion from the canceled generation must be rejected after undo");
    incActionRedo();
    check(primaryNode->name == "primary-after" && derivedNode->name == "derived-before",
        "async group redo should replay the primary operation but not stale derived output");
    check(owner->generation() != firstGeneration, "async group redo should use a new asynchronous generation");
    check(observedEvents[observedEvents.size() - 1] == AsyncGroupActionEvent::Redone,
        "async group redo should notify progress observers before fresh work is scheduled");
    owner->markAsyncScheduled();
    incActionMarkSaved();
    check(!incActionIsModified(),
        "saving while redo-owned asynchronous work is pending should capture the current primary state");
    derivedNode->setName(NullableString::fromStaticLiteral("derived-redone"));
    auto redoneDerived = newName(derivedNode, "derived-before", [&] { return derivedNode->name; });
    check(owner->addCompletedAsyncAction(owner->generation(), redoneDerived),
        "redo generation should accept freshly completed asynchronous output");
    check(incActionIsModified(),
        "derived output completed after save must invalidate the saved snapshot without moving the history pointer");
    owner->retainCompletedResults();
    incActionUndo();
    check(primaryNode->name == "primary-before" && derivedNode->name == "derived-before",
        "second undo should revert asynchronously regenerated output and the primary operation");
    owner->removeObserver(observerId);
    incActionRedo();
    check(primaryNode->name == "primary-after" && derivedNode->name == "derived-redone",
        "completed checkpoint redo should restore the original derived action output");
    incActionUndo();
    incActionRedo();
    check(derivedNode->name == "derived-redone", "completed checkpoint output should survive repeated undo and redo");
    auto inner = newAsync([&] { return ActionSlice<ActionPtr>{primary}; });
    inner->markAsyncScheduled();
    check(inner->addCompletedAsyncAction(inner->generation(), redoneDerived),
        "nested checkpoint should attach its completed output");
    auto outer = newAsync([&] {
        auto group = createNodeAction<GroupAction>([&](GroupAction* storage) {
            auto arguments = ActionSlice<ActionPtr>{inner};
            ::new (static_cast<void*>(storage)) GroupAction(arguments);
        });
        return ActionSlice<ActionPtr>{group};
    });
    outer->retainCompletedResults();
    outer->rollback();
    outer->redo();
    check(inner->replaysCompletedResults() && derivedNode->name == "derived-redone",
        "sealing a task checkpoint should preserve nested asynchronous output identities");
    auto outputOnly = newEmptyAsync();
    outputOnly->markAsyncScheduled();
    check(outputOnly->addCompletedAsyncAction(outputOnly->generation(), redoneDerived),
        "output-only checkpoint should attach its completed output");
    check(!outputOnly->empty(), "completed generated output must not be discarded as an empty history group");
    outputOnly->retainCompletedResults();
    incActionPush(outputOnly);
    incActionUndo();
    incActionRedo();
    check(derivedNode->name == "derived-redone", "output-only checkpoint should support undo and redo");
    auto ordered = newEmptyAsync();
    ordered->beginCheckpointRecording();
    auto orderedNode = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    orderedNode->setName(NullableString::fromStaticLiteral("first"));
    ordered->addAction(newName(orderedNode, "before", [] { return NullableString::fromStaticLiteral("first"); }));
    auto nested = newEmptyAsync();
    ordered->addAction(nested);
    nested->markAsyncScheduled();
    orderedNode->setName(NullableString::fromStaticLiteral("generated"));
    auto nestedGeneration = nested->generation();
    auto nestedOutput = newName(orderedNode, "first", [] { return NullableString::fromStaticLiteral("generated"); });
    check(nested->addCompletedAsyncAction(nestedGeneration, nestedOutput),
        "nested writeback should be recorded in the enclosing checkpoint");
    orderedNode->setName(NullableString::fromStaticLiteral("last"));
    ordered->addAction(newName(orderedNode, "generated", [] { return NullableString::fromStaticLiteral("last"); }));
    ordered->retainCompletedResults();
    ordered->endCheckpointRecording();
    ordered->rollback();
    check(orderedNode->name == "before", "checkpoint undo should reverse interleaved writeback and primary edits");
    ordered->redo();
    check(orderedNode->name == "last", "checkpoint redo must not replay earlier generated output after later edits");
    auto firstCheckpoint = newEmptyAsync();
    auto secondCheckpoint = newEmptyAsync();
    auto firstOutput = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto secondOutput = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    firstOutput->setName(NullableString::fromStaticLiteral("first-after"));
    secondOutput->setName(NullableString::fromStaticLiteral("second-after"));
    firstCheckpoint->markAsyncScheduled();
    secondCheckpoint->markAsyncScheduled();
    auto firstCheckpointGeneration = firstCheckpoint->generation();
    auto firstCheckpointOutput = newName(firstOutput, "first-before", [&] { return firstOutput->name; });
    firstCheckpoint->addCompletedAsyncAction(firstCheckpointGeneration, firstCheckpointOutput);
    auto secondCheckpointGeneration = secondCheckpoint->generation();
    auto secondCheckpointOutput = newName(secondOutput, "second-before", [&] { return secondOutput->name; });
    secondCheckpoint->addCompletedAsyncAction(secondCheckpointGeneration, secondCheckpointOutput);
    firstCheckpoint->retainCompletedResults();
    secondCheckpoint->retainCompletedResults();
    check(!firstCheckpoint->canMerge(secondCheckpoint) && !firstCheckpoint->merge(secondCheckpoint),
        "retained checkpoints must preserve their independent replay sequences");
    secondCheckpoint->rollback();
    firstCheckpoint->rollback();
    check(firstOutput->name == "first-before" && secondOutput->name == "second-before",
        "separate retained checkpoints must undo both generated outputs");
    firstCheckpoint->redo();
    secondCheckpoint->redo();
    check(firstOutput->name == "first-after" && secondOutput->name == "second-after",
        "separate retained checkpoints must redo both generated outputs");
    auto earlierOwner = newEmptyAsync();
    earlierOwner->markAsyncScheduled(2);
    firstOutput->setName(NullableString::fromStaticLiteral("early-after"));
    auto earlyGeneration = earlierOwner->generation();
    auto earlyOutput = newName(firstOutput, "early-before", [&] { return firstOutput->name; });
    earlierOwner->addCompletedAsyncAction(earlyGeneration, earlyOutput);
    auto partialCheckpoint = newEmptyAsync();
    partialCheckpoint->beginCheckpointRecording();
    secondOutput->setName(NullableString::fromStaticLiteral("late-after"));
    auto lateGeneration = earlierOwner->generation();
    auto lateOutput = newName(secondOutput, "late-before", [&] { return secondOutput->name; });
    earlierOwner->addCompletedAsyncAction(lateGeneration, lateOutput);
    partialCheckpoint->retainCompletedResults();
    partialCheckpoint->endCheckpointRecording();
    partialCheckpoint->rollback();
    check(firstOutput->name == "early-after" && secondOutput->name == "late-before",
        "a checkpoint must undo only the async completion it recorded");
    partialCheckpoint->redo();
    check(secondOutput->name == "late-after", "a delegated completion must support checkpoint redo");
    partialCheckpoint->rollback();
    earlierOwner->rollback();
    check(firstOutput->name == "early-before" && secondOutput->name == "late-before",
        "undoing the older owner must still undo its nondelegated completion");
    earlierOwner->redo();
    check(firstOutput->name == "early-before" && secondOutput->name == "late-before",
        "ordinary redo must not restore discarded async completions");
    resetCase();
    primaryNode = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    primaryNode->setName(NullableString::fromStaticLiteral("progress-before"));
    primaryNode->setName(NullableString::fromStaticLiteral("progress-after"));
    primary = newName(primaryNode, "progress-before", [&] { return primaryNode->name; });
    owner = newAsync([&] { return ActionSlice<ActionPtr>{primary}; });
    owner->markAsyncScheduled();
    auto progress = AsyncGroupActionProgress::ngCreateDerivedSource([&] {
        return std::tuple{owner, RetainedString::fromStaticLiteral("Running async test")};
    });
    check(progress->isActive(), "generic async progress should activate for pending work");
    check(progress->isVisible(), "generic async progress should create a visible notification");
    incActionPush(owner);
    incActionUndo();
    check(!progress->isActive(), "generic async progress should close immediately when its action is undone");
    check(!progress->isVisible(), "undo should remove the generic progress notification itself");
    incActionRedo();
    owner->markAsyncScheduled();
    check(progress->isActive(), "generic async progress should reactivate for work rescheduled by redo");
    owner->markAsyncFinished();
    check(!progress->isActive(), "generic async progress should close when rescheduled work completes");
    progress->dispose();
    owner = newEmptyAsync();
    owner->markAsyncScheduled();
    auto failedToken = owner->asyncToken();
    progress = AsyncGroupActionProgress::ngCreateDerivedSource([&] {
        return std::tuple{owner, RetainedString::fromStaticLiteral("Failing async test"),
            RetainedString::fromStaticLiteral("Unexpected success")};
    });
    check(progress->isActive() && progress->isVisible(),
        "generic async progress should display scheduled work before failure");
    owner->markAsyncFailed();
    check(owner->state() == AsyncGroupActionState::Failed && owner->pendingAsyncCount() == 0 &&
        failedToken.canceled() && !progress->isActive() && !progress->isVisible(),
        "generic async failure must settle pending work, reject stale output, and close progress without success");
    check(!owner->addCompletedAsyncAction(failedToken.generation(), primary),
        "a failed asynchronous generation must reject later completion callbacks");
    progress->dispose();
    resetCase();
    primaryNode = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    derivedNode = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    primaryNode->setName(NullableString::fromStaticLiteral("merge-primary-0"));
    derivedNode->setName(NullableString::fromStaticLiteral("merge-derived-0"));
    primaryNode->setName(NullableString::fromStaticLiteral("merge-primary-1"));
    auto firstPrimary = newName(primaryNode, "merge-primary-0", [&] { return primaryNode->name; });
    auto firstOwner = newAsync([&] { return ActionSlice<ActionPtr>{firstPrimary}; });
    derivedNode->setName(NullableString::fromStaticLiteral("merge-derived-1"));
    auto firstDerived = newName(derivedNode, "merge-derived-0", [&] { return derivedNode->name; });
    firstOwner->addCompletedAsyncAction(firstOwner->generation(), firstDerived);
    incActionPush(firstOwner);
    primaryNode->setName(NullableString::fromStaticLiteral("merge-primary-2"));
    auto secondPrimary = newName(primaryNode, "merge-primary-1", [&] { return primaryNode->name; });
    auto secondOwner = newAsync([&] { return ActionSlice<ActionPtr>{secondPrimary}; });
    derivedNode->setName(NullableString::fromStaticLiteral("merge-derived-2"));
    auto secondDerived = newName(derivedNode, "merge-derived-1", [&] { return derivedNode->name; });
    secondOwner->addCompletedAsyncAction(secondOwner->generation(), secondDerived);
    incActionPush(secondOwner);
    check(incActionHistory().size() == 1, "compatible async groups should merge into one history entry");
    incActionUndo();
    check(primaryNode->name == "merge-primary-0" && derivedNode->name == "merge-derived-0",
        "merged async group undo should roll back every completed derived generation and the original edit");
    incActionRedo();
    check(primaryNode->name == "merge-primary-2" && derivedNode->name == "merge-derived-0",
        "merged async group redo should restore only the final primary value and reschedule derived work");
}
