// Existing regression.d19253-19318; include inside regression::detail after helpers.
void testViewportFlipPairCommands() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::compat::ngJsonStringify;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::viewport::control;
    using namespace nijigenerate::core::actionstack;
    const auto uuidText = [](std::uint32_t source) {
        // Used std.conv.toChars / core.internal.string.unsignedToTempString.
        char buffer[64];
        std::size_t first = sizeof(buffer);
        std::uint64_t value = source;
        do {
            std::uint32_t digit;
            if (value < 10) { digit = static_cast<std::uint32_t>(value); value = 0; }
            else { digit = static_cast<std::uint32_t>(value % 10); value /= 10; }
            buffer[--first] = static_cast<char>(digit + '0');
        } while (value);
        auto output = allocateSourceStringBytes(sizeof(buffer) - first);
        for (std::size_t i = 0; i < output.size(); ++i) output[i] = buffer[first + i];
        return RetainedString::retainedView(output.retentionOwner(), output.data(), output.size());
    };
    resetCase();
    auto puppet = nijigenerate::project::incActivePuppet();
    auto left = Node::createSourceDeferred([&] { return ngRequireObject(puppet)->root; });
    auto right = Node::createSourceDeferred([&] { return ngRequireObject(puppet)->root; });
    auto autoLeft = Node::createSourceDeferred([&] { return ngRequireObject(puppet)->root; });
    auto autoRight = Node::createSourceDeferred([&] { return ngRequireObject(puppet)->root; });
    left->setName(NullableString::fromStaticLiteral("Hand.L"));
    right->setName(NullableString::fromStaticLiteral("Hand.R"));
    autoLeft->setName(NullableString::fromStaticLiteral("Foot.L"));
    autoRight->setName(NullableString::fromStaticLiteral("Foot.R"));
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->puppet(puppet);
    auto add = constructSourceCommand<AddFlipPairCommand>();
    add->left = left;
    add->right = right;
    incActionClearHistory();
    require(ngRequireObject(add->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("AddFlipPairCommand should add an explicit pair"));
    require(incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("AddFlipPairCommand should push one undoable action"));
    auto listed = constructSourceCommand<ListFlipPairsCommand>()->run(ctx);
    require(ngRequireObject(listed)->succeeded,
        RetainedString::fromStaticLiteral("ListFlipPairsCommand should succeed after add"));
    auto listJson = ngJsonStringify(ngRequireObject(listed)->result);
    require(listJson.view().find(uuidText(left->getUUID()).view()) != std::string_view::npos &&
        listJson.view().find(uuidText(right->getUUID()).view()) != std::string_view::npos,
        RetainedString::fromStaticLiteral("listed flip pairs should include the explicit pair UUIDs"));
    incActionUndo();
    require(ngJsonStringify(ngRequireObject(constructSourceCommand<ListFlipPairsCommand>()->run(ctx))->result)
        .view().find(uuidText(left->getUUID()).view()) == std::string_view::npos,
        RetainedString::fromStaticLiteral("undo AddFlipPairCommand should remove the explicit pair"));
    incActionRedo();
    require(ngJsonStringify(ngRequireObject(constructSourceCommand<ListFlipPairsCommand>()->run(ctx))->result)
        .view().find(uuidText(left->getUUID()).view()) != std::string_view::npos,
        RetainedString::fromStaticLiteral("redo AddFlipPairCommand should restore the explicit pair"));
    auto autoAdd = constructSourceCommand<AutoAddFlipPairsCommand>();
    autoAdd->leftPattern = RetainedString::fromStaticLiteral(".L");
    autoAdd->rightPattern = RetainedString::fromStaticLiteral(".R");
    incActionClearHistory();
    require(ngRequireObject(autoAdd->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("AutoAddFlipPairsCommand should match named pairs"));
    require(incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("AutoAddFlipPairsCommand should push one undoable action"));
    auto autoJson = ngJsonStringify(ngRequireObject(constructSourceCommand<ListFlipPairsCommand>()->run(ctx))->result);
    require(autoJson.view().find(uuidText(autoLeft->getUUID()).view()) != std::string_view::npos &&
        autoJson.view().find(uuidText(autoRight->getUUID()).view()) != std::string_view::npos,
        RetainedString::fromStaticLiteral("auto-added flip pairs should include matched node UUIDs"));
    incActionUndo();
    require(ngJsonStringify(ngRequireObject(constructSourceCommand<ListFlipPairsCommand>()->run(ctx))->result)
        .view().find(uuidText(autoLeft->getUUID()).view()) == std::string_view::npos,
        RetainedString::fromStaticLiteral("undo AutoAddFlipPairsCommand should remove auto-added pairs"));
    incActionRedo();
    require(ngJsonStringify(ngRequireObject(constructSourceCommand<ListFlipPairsCommand>()->run(ctx))->result)
        .view().find(uuidText(autoLeft->getUUID()).view()) != std::string_view::npos,
        RetainedString::fromStaticLiteral("redo AutoAddFlipPairsCommand should restore auto-added pairs"));
    auto removePair = constructSourceCommand<RemoveFlipPairCommand>();
    removePair->left = left;
    removePair->right = right;
    incActionClearHistory();
    require(ngRequireObject(removePair->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("RemoveFlipPairCommand should remove explicit pair"));
    require(incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("RemoveFlipPairCommand should push one undoable action"));
    auto afterRemove = ngJsonStringify(ngRequireObject(constructSourceCommand<ListFlipPairsCommand>()->run(ctx))->result);
    require(afterRemove.view().find(uuidText(left->getUUID()).view()) == std::string_view::npos ||
        afterRemove.view().find(uuidText(right->getUUID()).view()) == std::string_view::npos,
        RetainedString::fromStaticLiteral("removed explicit pair should no longer be listed"));
    incActionUndo();
    require(ngJsonStringify(ngRequireObject(constructSourceCommand<ListFlipPairsCommand>()->run(ctx))->result)
        .view().find(uuidText(left->getUUID()).view()) != std::string_view::npos,
        RetainedString::fromStaticLiteral("undo RemoveFlipPairCommand should restore removed pair"));
    incActionRedo();
    require(ngJsonStringify(ngRequireObject(constructSourceCommand<ListFlipPairsCommand>()->run(ctx))->result)
        .view().find(uuidText(left->getUUID()).view()) == std::string_view::npos,
        RetainedString::fromStaticLiteral("redo RemoveFlipPairCommand should remove pair again"));
}
