// Complete original regression.d5376-5407 and 5409-5469.
// Include once inside regression::detail after require/resetCase/near.
void testSimplePhysicsParameterUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::SimplePhysics;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::param::Parameter;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::node::simplephysics;
    namespace actions = nijigenerate::core::actionstack;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto physics = SimplePhysics::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    physics->setName(NullableString::fromStaticLiteral("physics"));
    auto paramA = Parameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("PhysicsA"); }, [] { return false; });
    auto paramB = Parameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("PhysicsB"); }, [] { return false; });
    nijigenerate::project::incActivePuppet()->parameters.push_back(paramA);
    nijigenerate::project::incActivePuppet()->parameters.push_back(paramB);
    auto context = std::allocate_shared<Context>(SourceAllocator<Context>{});
    context->nodes(Context::Nodes{std::static_pointer_cast<Node>(physics)});
    auto setA = constructSourceCommand<SetSimplePhysicsParameterCommand>(paramA);
    check(setA->run(context)->succeeded, "setting SimplePhysics parameter A should succeed");
    check(physics->getParam() == paramA, "SimplePhysics parameter A should apply");
    auto setB = constructSourceCommand<SetSimplePhysicsParameterCommand>(paramB);
    check(setB->run(context)->succeeded, "setting SimplePhysics parameter B should succeed");
    check(physics->getParam() == paramB, "SimplePhysics parameter B should apply");
    check(actions::incActionHistory().size() == 2, "SimplePhysics parameter changes must not merge");
    actions::incActionUndo();
    check(physics->getParam() == paramA, "undo should restore the previous SimplePhysics parameter");
    auto clear = constructSourceCommand<ClearSimplePhysicsParameterCommand>();
    check(clear->run(context)->succeeded, "clearing SimplePhysics parameter should succeed");
    check(physics->getParam() == nullptr, "SimplePhysics parameter clear should apply");
    actions::incActionUndo();
    check(physics->getParam() == paramA, "undo clear should restore the previous SimplePhysics parameter");
}

void testSimplePhysicsSettingsUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::SimplePhysics;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::SourcePhysicsModel;
    using nicxlive::core::nodes::SourceParamMapMode;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::node::simplephysics;
    namespace actions = nijigenerate::core::actionstack;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto physics = SimplePhysics::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    physics->setName(NullableString::fromStaticLiteral("physics"));
    auto context = std::allocate_shared<Context>(SourceAllocator<Context>{});
    context->nodes(Context::Nodes{std::static_pointer_cast<Node>(physics)});
    check(constructSourceCommand<SetSimplePhysicsModelTypeCommand>(SourcePhysicsModel{SourcePhysicsModel::SpringPendulum})
        ->run(context)->succeeded, "setting physics model type should succeed");
    check(physics->getModelType() == SourcePhysicsModel::SpringPendulum, "physics model type should apply");
    actions::incActionUndo();
    check(physics->getModelType() == SourcePhysicsModel::Pendulum, "physics model type undo should restore old value");
    actions::incActionRedo();
    check(physics->getModelType() == SourcePhysicsModel::SpringPendulum, "physics model type redo should restore new value");
    check(constructSourceCommand<SetSimplePhysicsMapModeCommand>(SourceParamMapMode{SourceParamMapMode::XY})
        ->run(context)->succeeded, "setting physics map mode should succeed");
    check(physics->mapMode == SourceParamMapMode::XY, "physics map mode should apply");
    actions::incActionUndo();
    check(physics->mapMode == SourceParamMapMode::AngleLength, "physics map mode undo should restore old value");
    check(constructSourceCommand<SetSimplePhysicsLocalOnlyCommand>(true)->run(context)->succeeded,
        "setting localOnly should succeed");
    check(physics->localOnly, "physics localOnly should apply");
    actions::incActionUndo();
    check(!physics->localOnly, "physics localOnly undo should restore old value");
    check(constructSourceCommand<SetSimplePhysicsGravityCommand>(2.0f)->run(context)->succeeded, "setting gravity should succeed");
    check(near(physics->getGravity(), 2.0f), "physics gravity should apply");
    actions::incActionUndo();
    check(near(physics->getGravity(), 1.0f), "physics gravity undo should restore old value");
    check(constructSourceCommand<SetSimplePhysicsLengthCommand>(250.0f)->run(context)->succeeded, "setting length should succeed");
    check(near(physics->getLength(), 250.0f), "physics length should apply");
    actions::incActionUndo();
    check(near(physics->getLength(), 100.0f), "physics length undo should restore old value");
    check(constructSourceCommand<SetSimplePhysicsFrequencyCommand>(3.5f)->run(context)->succeeded, "setting frequency should succeed");
    check(near(physics->getFrequency(), 3.5f), "physics frequency should apply");
    actions::incActionUndo();
    check(near(physics->getFrequency(), 1.0f), "physics frequency undo should restore old value");
    check(constructSourceCommand<SetSimplePhysicsAngleDampingCommand>(0.2f)->run(context)->succeeded,
        "setting angle damping should succeed");
    check(near(physics->getAngleDamping(), 0.2f), "physics angle damping should apply");
    actions::incActionUndo();
    check(near(physics->getAngleDamping(), 0.5f), "physics angle damping undo should restore old value");
    check(constructSourceCommand<SetSimplePhysicsLengthDampingCommand>(0.3f)->run(context)->succeeded,
        "setting length damping should succeed");
    check(near(physics->getLengthDamping(), 0.3f), "physics length damping should apply");
    actions::incActionUndo();
    check(near(physics->getLengthDamping(), 0.5f), "physics length damping undo should restore old value");
    check(constructSourceCommand<SetSimplePhysicsOutputScaleXCommand>(1.7f)->run(context)->succeeded,
        "setting output scale X should succeed");
    check(near(physics->getOutputScale().x, 1.7f), "physics output scale X should apply");
    actions::incActionUndo();
    check(near(physics->getOutputScale().x, 1.0f), "physics output scale X undo should restore old value");
    check(constructSourceCommand<SetSimplePhysicsOutputScaleYCommand>(1.8f)->run(context)->succeeded,
        "setting output scale Y should succeed");
    check(near(physics->getOutputScale().y, 1.8f), "physics output scale Y should apply");
    actions::incActionUndo();
    check(near(physics->getOutputScale().y, 1.0f), "physics output scale Y undo should restore old value");
}

// Complete original regression.d5471-5511; these are authored fixture operations,
// not a second runner, new case or generated fixture asset.
void testSimplePhysicsSerializationRoundTrip() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::SimplePhysics;
    using nicxlive::core::nodes::SourcePhysicsModel;
    using nicxlive::core::nodes::SourceParamMapMode;
    using nicxlive::core::param::Parameter;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::puppet::file::SaveFileCommand;
    namespace paths = nijigenerate::compat::phobos;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto physics = SimplePhysics::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    physics->setName(NullableString::fromStaticLiteral("physics-serialized"));
    auto param = Parameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("PhysicsSerializedParam"); }, [] { return false; });
    nijigenerate::project::incActivePuppet()->parameters.push_back(param);
    physics->setParam(param);
    physics->setModelType(SourcePhysicsModel{SourcePhysicsModel::SpringPendulum});
    physics->mapMode = SourceParamMapMode{SourceParamMapMode::XY};
    physics->localOnly = true;
    physics->gravity = 2.25f;
    physics->length = 123.0f;
    physics->frequency = 4.5f;
    physics->angleDamping = 0.35f;
    physics->lengthDamping = 0.45f;
    physics->outputScale = {1.25f, 1.75f};
    nijigenerate::project::incActivePuppet()->root->build();
    const std::array fixtureSegments{paths::ngTempDirectory(), text("nijigenerate-regression")};
    auto fixtureDir = paths::ngBuildPath(fixtureSegments);
    paths::ngMkdirRecurse(fixtureDir.view());
    const std::array saveSegments{fixtureDir, text("simplephysics-roundtrip")};
    auto saveBase = paths::ngBuildPath(saveSegments);
    auto savePath = concatenateSourceStrings({saveBase, text(".inx")});
    check(constructSourceCommand<SaveFileCommand>(saveBase)->run(std::allocate_shared<Context>(SourceAllocator<Context>{}))
        ->succeeded, "SaveFileCommand should save SimplePhysics fixture");
    check(paths::ngFileExists(savePath.view()) && paths::ngIsFile(savePath.view()), "SimplePhysics INX fixture should exist");
    ensureRegressionNodeTypesRegistered();
    auto loadedPuppet = nijigenerate::io::project_reader::ngLoadProjectPuppet<nijigenerate::ext::ExPuppet>(savePath);
    auto loaded = std::dynamic_pointer_cast<SimplePhysics>(findNodeRecursive(loadedPuppet->root, "physics-serialized"));
    const bool loadedPresent = loaded != nullptr;
    const auto loadedMessage = concatenateSourceStrings({text("SimplePhysics node should load from INX; tree:\n"),
        nodeTreeSummary(loadedPuppet->root)});
    require(loadedPresent, loadedMessage);
    check(loaded->getParam() != nullptr && loaded->getParam()->getName().view() == "PhysicsSerializedParam",
        "SimplePhysics parameter reference should round-trip");
    check(loaded->getModelType() == SourcePhysicsModel::SpringPendulum, "SimplePhysics model type should round-trip");
    check(loaded->mapMode == SourceParamMapMode::XY, "SimplePhysics map mode should round-trip");
    check(loaded->localOnly, "SimplePhysics localOnly should round-trip");
    check(near(loaded->getGravity(), 2.25f), "SimplePhysics gravity should round-trip");
    check(near(loaded->getLength(), 123.0f), "SimplePhysics length should round-trip");
    check(near(loaded->getFrequency(), 4.5f), "SimplePhysics frequency should round-trip");
    check(near(loaded->getAngleDamping(), 0.35f), "SimplePhysics angle damping should round-trip");
    check(near(loaded->getLengthDamping(), 0.45f), "SimplePhysics length damping should round-trip");
    check(near(loaded->getOutputScale().x, 1.25f) && near(loaded->getOutputScale().y, 1.75f),
        "SimplePhysics output scale should round-trip");
}
