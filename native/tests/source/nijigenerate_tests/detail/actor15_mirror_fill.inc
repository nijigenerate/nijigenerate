// Complete existing regression.d13334-13389 only.
void testParameterKeyframeMirrorFillCommands() {
    using Text = nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Vec2u;
    using nicxlive::core::nodes::Node;
    using nijigenerate::ext::ExParameter;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::binding;
    namespace actions = nijigenerate::core::actionstack;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto node2D = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    auto param2D = new2DParameter(text("MirrorFill2D"));
    auto binding2D = newValueBinding(param2D, node2D, text("transform.t.x"), false);
    binding2D->setValue(Vec2u{0, 1}, 11.0f);
    binding2D->setValue(Vec2u{1, 0}, 13.0f);
    binding2D->setValue(Vec2u{0, 0}, 17.0f);
    actions::incActionClearHistory();
    auto horizontalCtx = bindingContext(param2D, binding2D, Vec2u{2, 1});
    check(constructSourceCommand<SetFromHorizontalMirrorCommand>()->run(horizontalCtx)->succeeded,
        "SetFromHorizontalMirrorCommand should succeed");
    check(near(binding2D->getValue(Vec2u{2, 1}), -11.0f),
        "horizontal mirror-fill should copy mirrored X source and flip transform.t.x sign");
    actions::incActionUndo();
    check(!binding2D->isSet(Vec2u{2, 1}), "undo horizontal mirror-fill should restore unset destination");
    actions::incActionRedo();
    check(near(binding2D->getValue(Vec2u{2, 1}), -11.0f), "redo horizontal mirror-fill should restore mirrored destination");
    actions::incActionClearHistory();
    auto verticalCtx = bindingContext(param2D, binding2D, Vec2u{1, 2});
    check(constructSourceCommand<SetFromVerticalMirrorCommand>()->run(verticalCtx)->succeeded,
        "SetFromVerticalMirrorCommand should succeed");
    check(near(binding2D->getValue(Vec2u{1, 2}), 13.0f),
        "vertical mirror-fill should copy mirrored Y source without flipping transform.t.x sign");
    actions::incActionUndo();
    check(!binding2D->isSet(Vec2u{1, 2}), "undo vertical mirror-fill should restore unset destination");
    actions::incActionRedo();
    check(near(binding2D->getValue(Vec2u{1, 2}), 13.0f), "redo vertical mirror-fill should restore mirrored destination");
    actions::incActionClearHistory();
    auto diagonalCtx = bindingContext(param2D, binding2D, Vec2u{2, 2});
    check(constructSourceCommand<SetFromDiagonalMirrorCommand>()->run(diagonalCtx)->succeeded,
        "SetFromDiagonalMirrorCommand should succeed");
    const bool diagonalCorrect = near(binding2D->getValue(Vec2u{2, 2}), -17.0f);
    // std.conv.toStr uses default FormatSpec/formatValue for this float.
    const auto diagnosticValue = binding2D->getValue(Vec2u{2, 2});
    const std::array<format::TypedFormatArgument, 1> arguments{format::ngFormatArgument(diagnosticValue)};
    auto rendered = format::ngFormatTyped("%s", arguments);
    auto diagnosticText = nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(rendered)); });
    auto diagonalMessage = concatenateSourceStrings({
        text("diagonal mirror-fill should copy diagonal source and flip transform.t.x sign; got "), diagnosticText});
    require(diagonalCorrect, diagonalMessage);
    actions::incActionUndo();
    check(!binding2D->isSet(Vec2u{2, 2}), "undo diagonal mirror-fill should restore unset destination");
    actions::incActionRedo();
    check(near(binding2D->getValue(Vec2u{2, 2}), -17.0f), "redo diagonal mirror-fill should restore mirrored destination");
    auto node1D = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    auto param1D = ExParameter::createSourceDeferred(
        [] { return nicxlive::core::common::NullableString::fromStaticLiteral("MirrorFill1D"); }, [] { return false; });
    param1D->min = {-1, 0};
    param1D->max = {1, 0};
    param1D->insertAxisPoint(0, 0.5f);
    nijigenerate::project::incActivePuppet()->parameters.push_back(param1D);
    auto binding1D = newValueBinding(param1D, node1D, text("transform.t.x"), false);
    binding1D->setValue(Vec2u{0, 0}, 19.0f);
    actions::incActionClearHistory();
    auto oneDCtx = bindingContext(param1D, binding1D, Vec2u{2, 0});
    check(constructSourceCommand<SetFrom1DMirrorCommand>()->run(oneDCtx)->succeeded, "SetFrom1DMirrorCommand should succeed");
    check(near(binding1D->getValue(Vec2u{2, 0}), -19.0f),
        "1D mirror-fill should copy mirrored source and flip transform.t.x sign");
    actions::incActionUndo();
    check(!binding1D->isSet(Vec2u{2, 0}), "undo 1D mirror-fill should restore unset destination");
    actions::incActionRedo();
    check(near(binding1D->getValue(Vec2u{2, 0}), -19.0f), "redo 1D mirror-fill should restore mirrored destination");
}
