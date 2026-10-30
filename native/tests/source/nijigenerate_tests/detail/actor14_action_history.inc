// Existing regression.d1850-1882,17728-17748,18179-18257,19348-19417.
// Include once inside the original module-private namespace.
void testActionHistoryIndexAndModifiedState() {
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using NameAction = nijigenerate::actions::node::NodeValueChangeAction<Node, NullableString>;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto node = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    node->setName(NullableString::fromStaticLiteral("A"));
    incActionMarkSaved();
    require(!incActionIsModified(), RetainedString::fromStaticLiteral("empty history marked saved should not be modified"));
    node->setName(NullableString::fromStaticLiteral("B"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = node;
        const auto previous = NullableString::fromStaticLiteral("A");
        const auto next = node->getName();
        const auto pointer = std::shared_ptr<NullableString>(node, &node->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    require(incActionHistory().size() == 1, RetainedString::fromStaticLiteral("first action should enter history"));
    require(incActionIndex() == 1, RetainedString::fromStaticLiteral("action index should point after first action"));
    require(incActionIsModified(), RetainedString::fromStaticLiteral("pushed action should mark project modified"));
    node->setName(NullableString::fromStaticLiteral("C"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("sort");
        const auto target = node;
        const auto previous = NullableString::fromStaticLiteral("B");
        const auto next = node->getName();
        const auto pointer = std::shared_ptr<NullableString>(node, &node->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    require(incActionHistory().size() == 2, RetainedString::fromStaticLiteral("second non-merged action should enter history"));
    require(incActionIndex() == 2, RetainedString::fromStaticLiteral("action index should point after second action"));
    incActionSetIndex(0);
    require(node->getName() == NullableString::fromStaticLiteral("A"), RetainedString::fromStaticLiteral("setting action index to zero should undo all actions"));
    require(incActionIndex() == 0, RetainedString::fromStaticLiteral("action index should be zero after full undo"));
    require(!incActionIsModified(), RetainedString::fromStaticLiteral("returning to saved index should clear modified state"));
    incActionSetIndex(1);
    require(node->getName() == NullableString::fromStaticLiteral("B"), RetainedString::fromStaticLiteral("setting action index to one should replay first action only"));
    require(incActionIndex() == 1, RetainedString::fromStaticLiteral("action index should be one after partial replay"));
    require(incActionIsModified(), RetainedString::fromStaticLiteral("partial replay past saved state should be modified"));
    incActionSetIndex(2);
    require(node->getName() == NullableString::fromStaticLiteral("C"), RetainedString::fromStaticLiteral("setting action index to history end should replay all actions"));
    require(incActionIndex() == 2, RetainedString::fromStaticLiteral("action index should be at history end"));
    incActionMarkSaved();
    require(!incActionIsModified(), RetainedString::fromStaticLiteral("marking current index saved should clear modified state"));
    incActionUndo();
    require(incActionIsModified(), RetainedString::fromStaticLiteral("undoing away from saved index should mark modified"));
    incActionClearHistory();
    require(incActionHistory().size() == 0, RetainedString::fromStaticLiteral("clear history should remove actions"));
    require(incActionIndex() == 0, RetainedString::fromStaticLiteral("clear history should reset action index"));
    require(!incActionIsModified(), RetainedString::fromStaticLiteral("clear history should reset saved state"));
}

void testActionMergeSemantics() {
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using NameAction = nijigenerate::actions::node::NodeValueChangeAction<Node, NullableString>;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto node = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    node->setName(NullableString::fromStaticLiteral("A"));
    incActionMarkSaved();
    node->setName(NullableString::fromStaticLiteral("B"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = node;
        const auto previous = NullableString::fromStaticLiteral("A");
        const auto next = node->getName();
        const auto pointer = std::shared_ptr<NullableString>(node, &node->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    node->setName(NullableString::fromStaticLiteral("C"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = node;
        const auto previous = NullableString::fromStaticLiteral("B");
        const auto next = node->getName();
        const auto pointer = std::shared_ptr<NullableString>(node, &node->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    require(incActionHistory().size() == 1, RetainedString::fromStaticLiteral("same node property edits should merge into one undo entry"));
    require(incActionIndex() == 1, RetainedString::fromStaticLiteral("merged edit should keep one active history entry"));
    incActionUndo();
    require(node->getName() == NullableString::fromStaticLiteral("A"), RetainedString::fromStaticLiteral("undo merged edit should restore the original value"));
    require(!incActionIsModified(), RetainedString::fromStaticLiteral("undo merged edit back to saved state should clear modified flag"));
    incActionRedo();
    require(node->getName() == NullableString::fromStaticLiteral("C"), RetainedString::fromStaticLiteral("redo merged edit should restore the final merged value"));
    node->setName(NullableString::fromStaticLiteral("D"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("sort");
        const auto target = node;
        const auto previous = NullableString::fromStaticLiteral("C");
        const auto next = node->getName();
        const auto pointer = std::shared_ptr<NullableString>(node, &node->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    require(incActionHistory().size() == 2, RetainedString::fromStaticLiteral("different action names should split undo entries"));
    incActionUndo();
    require(node->getName() == NullableString::fromStaticLiteral("C"), RetainedString::fromStaticLiteral("undo split edit should restore previous value"));
    node->setName(NullableString::fromStaticLiteral("E"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("visibility");
        const auto target = node;
        const auto previous = NullableString::fromStaticLiteral("C");
        const auto next = node->getName();
        const auto pointer = std::shared_ptr<NullableString>(node, &node->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    require(incActionHistory().size() == 2 && incActionIndex() == 2,
        RetainedString::fromStaticLiteral("pushing after undo should truncate redo history and append the new action"));
    incActionRedo();
    require(node->getName() == NullableString::fromStaticLiteral("E"), RetainedString::fromStaticLiteral("redo after truncation should not restore the discarded action"));
}

void testNodeNameUndoMergesPerNode() {
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using NameAction = nijigenerate::actions::node::NodeValueChangeAction<Node, NullableString>;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto nodeA = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto nodeB = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    nodeA->setName(NullableString::fromStaticLiteral("A"));
    nodeB->setName(NullableString::fromStaticLiteral("B"));
    const auto oldA = nodeA->getName();
    nodeA->setName(NullableString::fromStaticLiteral("A1"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = nodeA;
        const auto previous = oldA;
        const auto next = nodeA->getName();
        const auto pointer = std::shared_ptr<NullableString>(nodeA, &nodeA->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    nodeA->setName(NullableString::fromStaticLiteral("A12"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = nodeA;
        const auto previous = NullableString::fromStaticLiteral("A1");
        const auto next = nodeA->getName();
        const auto pointer = std::shared_ptr<NullableString>(nodeA, &nodeA->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    require(incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("successive name edits on the same node should merge"));
    const auto oldB = nodeB->getName();
    nodeB->setName(NullableString::fromStaticLiteral("B1"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = nodeB;
        const auto previous = oldB;
        const auto next = nodeB->getName();
        const auto pointer = std::shared_ptr<NullableString>(nodeB, &nodeB->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    require(incActionHistory().size() == 2,
        RetainedString::fromStaticLiteral("name edits on different nodes must not merge"));
    incActionUndo();
    require(nodeB->getName() == NullableString::fromStaticLiteral("B"),
        RetainedString::fromStaticLiteral("undo should restore the second node name"));
    require(nodeA->getName() == NullableString::fromStaticLiteral("A12"),
        RetainedString::fromStaticLiteral("undo of node B must not affect node A"));
    incActionUndo();
    require(nodeA->getName() == NullableString::fromStaticLiteral("A"),
        RetainedString::fromStaticLiteral("merged node A name undo should restore the original value"));
    incActionRedo();
    require(nodeA->getName() == NullableString::fromStaticLiteral("A12"),
        RetainedString::fromStaticLiteral("merged node A name redo should restore the final value"));
}

void testActionStackScopeGuard() {
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using NameAction = nijigenerate::actions::node::NodeValueChangeAction<Node, NullableString>;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    require(ngActionStackLevel() == 0, RetainedString::fromStaticLiteral("action stack should start at root level"));
    auto vertexScope = ngOpenActionStackScope(ActionStackScopeUnit::VertexEdit);
    require(vertexScope->isActive(), RetainedString::fromStaticLiteral("VertexEdit scope should open"));
    require(ngActionStackScopeActive(ActionStackScopeUnit::VertexEdit),
        RetainedString::fromStaticLiteral("VertexEdit scope should be registered"));
    require(ngActionStackLevel() == 1, RetainedString::fromStaticLiteral("VertexEdit scope should push one action stack level"));
    {
        const nijigenerate::compat::Slice<ActionStackScopeUnit> allowed{ActionStackScopeUnit::VertexEdit};
        ngGuardActionStackScopes(std::span<const ActionStackScopeUnit>{allowed.data(), allowed.size()});
    }
    require(vertexScope->isActive(), RetainedString::fromStaticLiteral("guard should keep allowed VertexEdit scope"));
    require(ngActionStackLevel() == 1, RetainedString::fromStaticLiteral("guard should not pop allowed scope"));
    auto depthScope = ngOpenActionStackScope(ActionStackScopeUnit::DepthEdit);
    require(depthScope->isActive(), RetainedString::fromStaticLiteral("DepthEdit nested scope should open"));
    require(ngActionStackLevel() == 2, RetainedString::fromStaticLiteral("nested DepthEdit scope should push another level"));
    {
        const nijigenerate::compat::Slice<ActionStackScopeUnit> allowed{ActionStackScopeUnit::VertexEdit};
        ngGuardActionStackScopes(std::span<const ActionStackScopeUnit>{allowed.data(), allowed.size()});
    }
    require(vertexScope->isActive(), RetainedString::fromStaticLiteral("guard should preserve allowed outer scope"));
    require(!depthScope->isActive(), RetainedString::fromStaticLiteral("guard should close disallowed nested DepthEdit scope"));
    require(!ngActionStackScopeActive(ActionStackScopeUnit::DepthEdit),
        RetainedString::fromStaticLiteral("closed DepthEdit scope should be unregistered"));
    require(ngActionStackLevel() == 1, RetainedString::fromStaticLiteral("guard should restore stack level after closing nested scope"));
    auto oneTimeScope = ngOpenActionStackScope(ActionStackScopeUnit::OneTimeDeform);
    require(oneTimeScope->isActive(), RetainedString::fromStaticLiteral("OneTimeDeform nested scope should open"));
    require(ngActionStackLevel() == 2, RetainedString::fromStaticLiteral("OneTimeDeform nested scope should push another level"));
    ngGuardActionStackScopes();
    require(!vertexScope->isActive(), RetainedString::fromStaticLiteral("empty guard should close outer VertexEdit scope"));
    require(!oneTimeScope->isActive(), RetainedString::fromStaticLiteral("closing outer scope should also close nested OneTimeDeform scope"));
    require(ngActionStackLevel() == 0, RetainedString::fromStaticLiteral("empty guard should restore root action stack level"));
    auto rootNode = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    rootNode->setName(NullableString::fromStaticLiteral("before-root-action"));
    rootNode->setName(NullableString::fromStaticLiteral("after-root-action"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = rootNode;
        const auto previous = NullableString::fromStaticLiteral("before-root-action");
        const auto next = rootNode->getName();
        const auto pointer = std::shared_ptr<NullableString>(rootNode, &rootNode->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    auto clearScope = ngOpenActionStackScope(ActionStackScopeUnit::VertexEdit);
    require(ngActionStackLevel() == 1 && clearScope->isActive(),
        RetainedString::fromStaticLiteral("current-level clear fixture should own a nested VertexEdit stack"));
    incActionPushGroup();
    auto discardedNode = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    discardedNode->setName(NullableString::fromStaticLiteral("before-clear"));
    discardedNode->setName(NullableString::fromStaticLiteral("discarded-change"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = discardedNode;
        const auto previous = NullableString::fromStaticLiteral("before-clear");
        const auto next = discardedNode->getName();
        const auto pointer = std::shared_ptr<NullableString>(discardedNode, &discardedNode->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    incActionClearHistory(ActionStackClear::CurrentLevel);
    require(clearScope->isActive() && ngActionStackScopeActive(ActionStackScopeUnit::VertexEdit) &&
        ngActionStackLevel() == 1 && ngActionStackGroupDepth() == 1 && incActionHistory().size() == 0,
        RetainedString::fromStaticLiteral("clearing current history must preserve ownership of an open asynchronous group"));
    auto postClearNode = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    postClearNode->setName(NullableString::fromStaticLiteral("before-post-clear"));
    postClearNode->setName(NullableString::fromStaticLiteral("after-post-clear"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = postClearNode;
        const auto previous = NullableString::fromStaticLiteral("before-post-clear");
        const auto next = postClearNode->getName();
        const auto pointer = std::shared_ptr<NullableString>(postClearNode, &postClearNode->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    incActionPopGroup();
    require(ngActionStackGroupDepth() == 0 && incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("the original group owner must still close and retain only writeback performed after the clear"));
    incActionUndo();
    require(postClearNode->getName() == NullableString::fromStaticLiteral("before-post-clear") &&
        discardedNode->getName() == NullableString::fromStaticLiteral("discarded-change"),
        RetainedString::fromStaticLiteral("undo after a current-level clear must affect only actions recorded after the clear"));
    incActionRedo();
    clearScope->close();
    require(!clearScope->isActive() && ngActionStackLevel() == 0 && incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("closing a cleared edit scope must restore the root action history"));
    incActionUndo();
    require(rootNode->getName() == NullableString::fromStaticLiteral("before-root-action"),
        RetainedString::fromStaticLiteral("root history must remain undoable after clearing and closing an edit scope"));
}

void testActionGroupUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using NameAction = nijigenerate::actions::node::NodeValueChangeAction<Node, NullableString>;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto nodeA = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto nodeB = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    nodeA->setName(NullableString::fromStaticLiteral("A"));
    nodeB->setName(NullableString::fromStaticLiteral("B"));
    incActionPushGroup();
    nodeA->setName(NullableString::fromStaticLiteral("A1"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = nodeA;
        const auto previous = NullableString::fromStaticLiteral("A");
        const auto next = nodeA->getName();
        const auto pointer = std::shared_ptr<NullableString>(nodeA, &nodeA->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    nodeB->setName(NullableString::fromStaticLiteral("B1"));
    incActionPush(nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
        const auto label = RetainedString::fromStaticLiteral("name");
        const auto target = nodeB;
        const auto previous = NullableString::fromStaticLiteral("B");
        const auto next = nodeB->getName();
        const auto pointer = std::shared_ptr<NullableString>(nodeB, &nodeB->name);
        ::new (static_cast<void*>(storage)) NameAction(label, target, previous, next, pointer);
    }));
    incActionPopGroup();
    require(incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("grouped edits should create one undo entry"));
    incActionUndo();
    require(nodeA->getName() == NullableString::fromStaticLiteral("A") &&
        nodeB->getName() == NullableString::fromStaticLiteral("B"),
        RetainedString::fromStaticLiteral("group undo should restore all grouped edits"));
    incActionRedo();
    require(nodeA->getName() == NullableString::fromStaticLiteral("A1") &&
        nodeB->getName() == NullableString::fromStaticLiteral("B1"),
        RetainedString::fromStaticLiteral("group redo should restore all grouped edits"));
}
