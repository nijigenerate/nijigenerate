// Complete existing regression.d1702-1740 only.
void testWeldingRuntimeDeformation() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::actions::node;
    namespace actions = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto equalIndices = [](Indices left, Indices right) {
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) if (left[i] != right[i]) return false;
        return true;
    };
    resetCase();
    auto drawable = newMeshPart(text("weld-runtime-drawable"));
    auto target = newMeshPart(text("weld-runtime-target"));
    drawable->deformation[0] = Vec2{8, 0};
    drawable->deformation[1] = Vec2{8, 0};
    drawable->deformation[2] = Vec2{8, 0};
    target->deformation[0] = Vec2{0, 0};
    target->deformation[1] = Vec2{0, 0};
    target->deformation[2] = Vec2{0, 0};
    Indices indices{0, 1, 2};
    actions::incActionPush(std::allocate_shared<DrawableAddWeldingAction>(SourceAllocator<DrawableAddWeldingAction>{},
        drawable, target, indices, 0.25f));
    check(drawable->weldedLinks.size() == 1 && target->weldedLinks.size() == 1,
        "welding runtime setup should create reciprocal links");
    check(equalIndices(drawable->weldedLinks[0].indices, indices), "source welding should preserve explicit index mapping");
    check(equalIndices(target->weldedLinks[0].indices, indices), "counter welding should map target vertices back to source vertices");
    check(near(drawable->weldedLinks[0].weight, 0.25f), "source welding weight should be preserved");
    check(near(target->weldedLinks[0].weight, 0.75f), "counter welding weight should be inverse of source weight");
    nicxlive::core::RenderContext ctx;
    drawable->runPostTask2(ctx);
    target->runPostTask2(ctx);
    check(nearVec2(drawable->deformation[0], Vec2{2, 0}) && nearVec2(drawable->deformation[1], Vec2{2, 0}) &&
        nearVec2(drawable->deformation[2], Vec2{2, 0}),
        "welding runtime should pull source deformation toward the weighted shared position");
    check(nearVec2(target->deformation[0], Vec2{2, 0}) && nearVec2(target->deformation[1], Vec2{2, 0}) &&
        nearVec2(target->deformation[2], Vec2{2, 0}),
        "welding runtime should push target deformation toward the weighted shared position");
    actions::incActionUndo();
    check(drawable->weldedLinks.empty() && target->weldedLinks.empty(), "undo welding setup should remove reciprocal links");
    drawable->deformation[0] = Vec2{8, 0};
    drawable->deformation[1] = Vec2{8, 0};
    drawable->deformation[2] = Vec2{8, 0};
    target->deformation[0] = Vec2{0, 0};
    target->deformation[1] = Vec2{0, 0};
    target->deformation[2] = Vec2{0, 0};
    drawable->runPostTask2(ctx);
    target->runPostTask2(ctx);
    check(nearVec2(drawable->deformation[0], Vec2{8, 0}) && nearVec2(target->deformation[0], Vec2{0, 0}),
        "undo welding setup should remove reciprocal post-process behavior");
}

void testWeldingUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::actions::node;
    namespace actions = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto equalIndices = [](Indices left, Indices right) {
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) if (left[i] != right[i]) return false;
        return true;
    };
    resetCase();
    auto drawable = newMeshPart(text("drawable"));
    auto target = newMeshPart(text("target"));
    Indices indices{0, 1, 2};
    actions::incActionPush(std::allocate_shared<DrawableAddWeldingAction>(SourceAllocator<DrawableAddWeldingAction>{},
        drawable, target, indices, 0.25f));
    check(drawable->weldedLinks.size() == 1, "welding add should create link");
    check(target->weldedLinks.size() == 1, "welding add should create counter link");
    check(near(drawable->weldedLinks[0].weight, 0.25f), "welding add should set weight");
    check(near(target->weldedLinks[0].weight, 0.75f), "welding add should set counter weight");
    Indices changedIndices{2, 1, 0};
    actions::incActionPush(DrawableChangeWeldingAction::create(drawable, target, changedIndices, 0.6f));
    check(equalIndices(drawable->weldedLinks[0].indices, changedIndices), "welding change should update indices");
    check(near(drawable->weldedLinks[0].weight, 0.6f), "welding change should update weight");
    check(near(target->weldedLinks[0].weight, 0.4f), "welding change should update counter weight");
    actions::incActionUndo();
    check(equalIndices(drawable->weldedLinks[0].indices, indices), "welding change undo should restore indices");
    check(near(drawable->weldedLinks[0].weight, 0.25f), "welding change undo should restore weight");
    actions::incActionRedo();
    check(equalIndices(drawable->weldedLinks[0].indices, changedIndices), "welding change redo should restore indices");
    check(near(drawable->weldedLinks[0].weight, 0.6f), "welding change redo should restore weight");
    actions::incActionPush(std::allocate_shared<DrawableRemoveWeldingAction>(SourceAllocator<DrawableRemoveWeldingAction>{},
        drawable, target));
    check(drawable->weldedLinks.empty(), "welding remove should remove link");
    check(target->weldedLinks.empty(), "welding remove should remove counter link");
    actions::incActionUndo();
    check(drawable->weldedLinks.size() == 1, "welding remove undo should restore link");
    check(target->weldedLinks.size() == 1, "welding remove undo should restore counter link");
    actions::incActionRedo();
    check(drawable->weldedLinks.empty(), "welding remove redo should remove link again");
    check(target->weldedLinks.empty(), "welding remove redo should remove counter link again");
}
