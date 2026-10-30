// Existing regression.d12632-12678 and 12813-12815; include inside regression::detail after helpers.
std::size_t runCompositeMatrixParameterPresets(nijigenerate::compat::RetainedString onlyId) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::parameter::param::Add1DParameterCommand;
    using nijigenerate::commands::parameter::param::Add2DParameterCommand;
    using nijigenerate::commands::parameter::prop::SetParameterNameCommand;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::common::NullableString;
    using namespace nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    std::size_t executed = 0;
    struct ParamRangeCase { std::int32_t min; std::int32_t max; RetainedString slug; };
    auto rangeCases = Slice<ParamRangeCase>{
        {0, 1, text("0-1")}, {-1, 1, text("minus-1-1")}, {-2, 2, text("minus-2-2")},
        {0, 2, text("0-2")}, {-5, 5, text("minus-5-5")}};
    for (bool is2D : Slice<bool>{false, true}) {
        for (auto range : rangeCases) {
            auto dim = is2D ? text("2d") : text("1d");
            auto caseId = concatenateSourceStrings({text("parameter.composite-preset-matrix."), dim, text("."), range.slug});
            if (onlyId.size() && onlyId != caseId) continue;
            resetCase();
            auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
            ctx->puppet(nijigenerate::project::incActivePuppet());
            auto result = is2D ? constructSourceCommand<Add2DParameterCommand>(range.min, range.max)->run(ctx)
                : constructSourceCommand<Add1DParameterCommand>(range.min, range.max)->run(ctx);
            check(ngRequireObject(result)->succeeded && result->created.size() == 1, "matrix parameter preset should create parameter");
            auto param = result->created[0];
            auto node = Node::createSourceDeferred([] { return ngRequireObject(nijigenerate::project::incActivePuppet())->root; });
            auto binding = newValueBinding(param, node, text("transform.t.x"), true);
            const auto key = is2D ? nijigenerate::compat::Vec2u{
                static_cast<std::uint32_t>(param->axisPoints[0].size() - 1),
                static_cast<std::uint32_t>(param->axisPoints[1].size() - 1)}
                : nijigenerate::compat::Vec2u{static_cast<std::uint32_t>(param->axisPoints[0].size() - 1), 0};
            ngRequireObject(binding)->setValue(key, 9.0f);
            ctx->parameters(Context::Parameters{param});
            check(ngRequireObject(constructSourceCommand<SetParameterNameCommand>(text("MatrixParam"))->run(ctx))->succeeded,
                "matrix parameter rename should succeed");
            incActionUndo();
            check(ngRequireObject(param)->getName() != NullableString::fromStaticLiteral("MatrixParam") && binding->isSet(key),
                "matrix undo parameter rename should preserve binding");
            incActionRedo();
            check(param->getName() == NullableString::fromStaticLiteral("MatrixParam") && binding->isSet(key),
                "matrix redo parameter rename should preserve binding");
            ++executed;
        }
    }
    const std::int32_t expected = onlyId.size() ? 1 : 10;
    const bool countMatches = executed == static_cast<std::size_t>(expected);
    // Selected std.conv.toChars/unsignedToTempString: both authored inputs are nonnegative.
    const auto decimal = []<class T>(T input) {
        std::array<char, std::is_signed_v<T> ? 11 : 20> buffer;
        auto first = buffer.size();
        auto value = static_cast<std::uint64_t>(input);
        do {
            std::uint32_t digit;
            if (value < 10) { digit = static_cast<std::uint32_t>(value); value = 0; }
            else { digit = static_cast<std::uint32_t>(value % 10); value /= 10; }
            buffer[--first] = static_cast<char>('0' + digit);
        } while (value);
        auto result = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(result.data(), buffer.data() + first, result.size());
        return RetainedString::retainedView(result.retentionOwner(), result.data(), result.size());
    };
    const auto expectedText = decimal(expected);
    const auto executedText = decimal(executed);
    require(countMatches, concatenateSourceStrings({text("parameter composite scenario should execute exactly "), expectedText,
        text(" preset cases, got "), executedText}));
    return executed;
}

void testCompositeMatrixParameterPresetsScenario() {
    runCompositeMatrixParameterPresets();
}

// Existing regression.d12605-12630 and 12809-12811.
std::size_t runCompositeMatrixNodeTypes(nijigenerate::compat::RetainedString onlyId) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::SetNodeNameCommand;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    ensureRegressionNodeTypesRegistered();
    std::size_t executed = 0;
    for (auto className : regressionNodeMenuTypes) {
        auto slug = scenarioSlug(className.view());
        auto caseId = concatenateSourceStrings({text("node.composite-type-matrix."), slug});
        if (onlyId.size() && onlyId != caseId) continue;
        resetCase();
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(nijigenerate::project::incActivePuppet());
        auto result = nijigenerate::commands::node::dynamic::ensureAddNodeCommand(className)->run(ctx);
        const bool createdOne = ngRequireObject(result)->succeeded && result->created.size() == 1;
        require(createdOne, concatenateSourceStrings({text("matrix AddNode should create "), className}));
        auto node = result->created[0];
        ctx->nodes(Context::Nodes{node});
        struct Names {
            RetainedString className;
            operator Slice<RetainedString>() const {
                return Slice<RetainedString>{concatenateSourceStrings({
                    RetainedString::fromStaticLiteral("Matrix "), className})};
            }
        };
        // Convert the original array argument only after the actual receiver allocation.
        auto rename = std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{}, Names{className});
        require(ngRequireObject(rename->run(ctx))->succeeded, text("matrix node rename should succeed"));
        incActionUndo();
        auto undoActual = ngRequireObject(node)->getName();
        auto undoName = concatenateSourceStrings({text("Matrix "), className});
        const bool undone = undoActual != undoName.toNullableString();
        require(undone, concatenateSourceStrings({text("matrix undo rename should restore "), className}));
        incActionRedo();
        auto redoActual = node->getName();
        auto redoName = concatenateSourceStrings({text("Matrix "), className});
        const bool redone = redoActual == redoName.toNullableString();
        require(redone, concatenateSourceStrings({text("matrix redo rename should restore "), className}));
        ++executed;
    }
    const std::size_t expected = onlyId.size() ? 1 : regressionNodeMenuTypes.size();
    const bool countMatches = executed == expected;
    // Selected std.conv unsignedToTempString for the original size_t values.
    const auto decimal = [](std::size_t value) {
        std::array<char, 20> buffer;
        auto first = buffer.size();
        do {
            const auto digit = value % 10;
            value /= 10;
            buffer[--first] = static_cast<char>('0' + digit);
        } while (value);
        auto result = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(result.data(), buffer.data() + first, result.size());
        return RetainedString::retainedView(result.retentionOwner(), result.data(), result.size());
    };
    auto expectedText = decimal(expected);
    auto executedText = decimal(executed);
    require(countMatches, concatenateSourceStrings({text("node-type composite scenario should execute exactly "),
        expectedText, text(" node menu types, got "), executedText}));
    return executed;
}

void testCompositeMatrixNodeTypesScenario() {
    runCompositeMatrixNodeTypes();
}

// Existing regression.d12680-12723 and 12817-12819.
std::size_t runCompositeMatrixDepthOps(nijigenerate::compat::RetainedString onlyId) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::depth::map::DepthMapCommand;
    using nijigenerate::ext::nodes::ExGridDeformer;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::d_runtime::SourceAllocator;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    using Object = nijigenerate::core::settings::SettingsJsonObject;
    using namespace nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    std::size_t executed = 0;
    for (auto opType : Slice<RetainedString>{text("attached-point"), text("ring"), text("plane")}) {
        auto caseId = concatenateSourceStrings({text("depth.composite-operation-matrix."), opType});
        if (onlyId.size() && onlyId != caseId) continue;
        resetCase();
        auto grid = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        grid->setName(concatenateSourceStrings({text("matrix-depth-"), opType}).toNullableString());
        grid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}, Vec2{10, 10}}));
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        check(ngRequireObject(ngCmd<DepthMapCommand::SetDepths>(ctx, grid,
            Slice<float>{0.0f, 0.1f, 0.2f, 0.3f}))->succeeded, "matrix depth should set depths");
        Object fields;
        if (opType == text("attached-point")) {
            fields["index"] = Json(1);
            fields["amount"] = Json(0.4);
        } else if (opType == text("ring")) {
            fields["p0"] = jsonVec2Value(0, 0);
            fields["p1"] = jsonVec2Value(10, 0);
            fields["amount"] = Json(0.5);
            fields["width"] = Json(5.0);
            fields["hardness"] = Json(0.25);
        } else {
            fields["p0"] = jsonVec2Value(0, 0);
            fields["p1"] = jsonVec2Value(10, 0);
            fields["p2"] = jsonVec2Value(0, 10);
            fields["z0"] = Json(0.0);
            fields["z1"] = Json(0.25);
            fields["z2"] = Json(-0.25);
        }
        const bool added = ngRequireObject(ngCmd<DepthMapCommand::AddDepthOp>(
            ctx, grid, depthCommandOp(opType, fields), -1))->succeeded;
        require(added, concatenateSourceStrings({text("matrix depth should add op "), opType}));
        check(grid->copyDepthOps().size() == 1, "matrix depth should store one op");
        incActionUndo();
        check(grid->copyDepthOps().size() == 0, "matrix depth undo should remove op");
        incActionRedo();
        check(grid->copyDepthOps().size() == 1, "matrix depth redo should restore op");
        ++executed;
    }
    const std::int32_t expected = onlyId.size() ? 1 : 3;
    const bool countMatches = executed == static_cast<std::size_t>(expected);
    const auto decimal = []<class T>(T input) {
        std::array<char, std::is_signed_v<T> ? 11 : 20> buffer;
        auto first = buffer.size();
        auto value = static_cast<std::uint64_t>(input);
        do {
            const auto digit = value % 10;
            value /= 10;
            buffer[--first] = static_cast<char>('0' + digit);
        } while (value);
        auto result = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(result.data(), buffer.data() + first, result.size());
        return RetainedString::retainedView(result.retentionOwner(), result.data(), result.size());
    };
    auto expectedText = decimal(expected);
    auto executedText = decimal(executed);
    require(countMatches, concatenateSourceStrings({text("depth-op composite scenario should execute exactly "),
        expectedText, text(" depth operation cases, got "), executedText}));
    return executed;
}

void testCompositeMatrixDepthOpsScenario() {
    runCompositeMatrixDepthOps();
}

// Existing regression.d12725-12739 and 12821-12823.
std::size_t runCompositeMatrixAutoMeshProcessors(nijigenerate::compat::RetainedString onlyId) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    std::size_t executed = 0;
    for (auto processorId : Slice<RetainedString>{text("grid"), text("contour"), text("skeleton"), text("optimum")}) {
        auto caseId = concatenateSourceStrings({text("automesh.composite-processor-matrix-scenario."), processorId});
        if (onlyId.size() && onlyId != caseId) continue;
        resetCase();
        auto mesh = runAutoMeshOnAlphaPart(processorId);
        auto label = concatenateSourceStrings({text("matrix-"), processorId});
        const auto minimum = processorId == text("skeleton") ? 2 : 3;
        requireAutoMeshOutput(label, mesh, minimum);
        ++executed;
    }
    const std::int32_t expected = onlyId.size() ? 1 : 4;
    const bool countMatches = executed == static_cast<std::size_t>(expected);
    const auto decimal = []<class T>(T input) {
        std::array<char, std::is_signed_v<T> ? 11 : 20> buffer;
        auto first = buffer.size();
        auto value = static_cast<std::uint64_t>(input);
        do {
            const auto digit = value % 10;
            value /= 10;
            buffer[--first] = static_cast<char>('0' + digit);
        } while (value);
        auto result = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(result.data(), buffer.data() + first, result.size());
        return RetainedString::retainedView(result.retentionOwner(), result.data(), result.size());
    };
    auto expectedText = decimal(expected);
    auto executedText = decimal(executed);
    require(countMatches, concatenateSourceStrings({text("automesh composite scenario should execute exactly "),
        expectedText, text(" processor cases, got "), executedText}));
    return executed;
}

void testCompositeMatrixAutoMeshProcessorsScenario() {
    runCompositeMatrixAutoMeshProcessors();
}

// Existing regression.d12741-12804 and 12825-12827.
std::size_t runCompositeMatrixSimplePhysicsSettings(nijigenerate::compat::RetainedString onlyId) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::node::simplephysics;
    using namespace nijigenerate::core::actionstack;
    using nicxlive::core::nodes::SimplePhysics;
    using nicxlive::core::nodes::SourcePhysicsModel;
    using nicxlive::core::nodes::SourceParamMapMode;
    using nicxlive::core::d_runtime::SourceAllocator;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto formatted = [](std::string_view pattern, float value) {
        const std::array<format::TypedFormatArgument, 1> args{format::ngFormatArgument(value)};
        auto rendered = format::ngFormatTyped(pattern, args);
        return nijigenerate::platform::detail::ngPathAllocation([&] { return RetainedString(std::move(rendered)); });
    };
    std::size_t executed = 0;
    resetCase();
    auto physics = SimplePhysics::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{physics});
    check(constructSourceCommand<SetSimplePhysicsModelTypeCommand>(SourcePhysicsModel{SourcePhysicsModel::SpringPendulum})
        ->run(ctx)->succeeded, "matrix physics model should apply");
    check(constructSourceCommand<SetSimplePhysicsMapModeCommand>(SourceParamMapMode{SourceParamMapMode::XY})
        ->run(ctx)->succeeded, "matrix physics map should apply");
    check(constructSourceCommand<SetSimplePhysicsLocalOnlyCommand>(true)->run(ctx)->succeeded,
        "matrix physics localOnly should apply");
    for (auto value : Slice<float>{0.5f, 2.0f, 4.0f}) {
        auto number = formatted("%.1f", value);
        auto slug = scenarioSlug(number.view());
        auto id = concatenateSourceStrings({text("simplephysics.composite-settings-matrix.gravity-"), slug});
        if (onlyId.size() && onlyId != id) continue;
        check(constructSourceCommand<SetSimplePhysicsGravityCommand>(value)->run(ctx)->succeeded,
            "matrix physics gravity should apply");
        incActionUndo();
        incActionRedo();
        check(near(physics->getGravity(), value), "matrix physics gravity redo should restore value");
        ++executed;
    }
    for (auto value : Slice<float>{50.0f, 125.0f, 300.0f}) {
        auto number = formatted("%.0f", value);
        auto slug = scenarioSlug(number.view());
        auto id = concatenateSourceStrings({text("simplephysics.composite-settings-matrix.length-"), slug});
        if (onlyId.size() && onlyId != id) continue;
        check(constructSourceCommand<SetSimplePhysicsLengthCommand>(value)->run(ctx)->succeeded,
            "matrix physics length should apply");
        incActionUndo();
        incActionRedo();
        check(near(physics->getLength(), value), "matrix physics length redo should restore value");
        ++executed;
    }
    for (auto value : Slice<float>{0.25f, 1.5f, 5.0f}) {
        auto number = value == 0.25f ? text("0-25") : formatted("%.1f", value);
        auto slug = scenarioSlug(number.view());
        auto id = concatenateSourceStrings({text("simplephysics.composite-settings-matrix.frequency-"), slug});
        if (onlyId.size() && onlyId != id) continue;
        check(constructSourceCommand<SetSimplePhysicsFrequencyCommand>(value)->run(ctx)->succeeded,
            "matrix physics frequency should apply");
        incActionUndo();
        incActionRedo();
        check(near(physics->getFrequency(), value), "matrix physics frequency redo should restore value");
        ++executed;
    }
    for (auto value : Slice<float>{0.1f, 0.5f, 0.9f}) {
        auto number = formatted("%.1f", value);
        auto slug = scenarioSlug(number.view());
        auto id = concatenateSourceStrings({text("simplephysics.composite-settings-matrix.angle-damping-"), slug});
        if (onlyId.size() && onlyId != id) continue;
        check(constructSourceCommand<SetSimplePhysicsAngleDampingCommand>(value)->run(ctx)->succeeded,
            "matrix physics angle damping should apply");
        incActionUndo();
        incActionRedo();
        check(near(physics->getAngleDamping(), value), "matrix physics angle damping redo should restore value");
        ++executed;
    }
    for (auto value : Slice<float>{0.1f, 0.5f, 0.9f}) {
        auto number = formatted("%.1f", value);
        auto slug = scenarioSlug(number.view());
        auto id = concatenateSourceStrings({text("simplephysics.composite-settings-matrix.length-damping-"), slug});
        if (onlyId.size() && onlyId != id) continue;
        check(constructSourceCommand<SetSimplePhysicsLengthDampingCommand>(value)->run(ctx)->succeeded,
            "matrix physics length damping should apply");
        incActionUndo();
        incActionRedo();
        check(near(physics->getLengthDamping(), value), "matrix physics length damping redo should restore value");
        ++executed;
    }
    const std::int32_t expected = onlyId.size() ? 1 : 15;
    const bool countMatches = executed == static_cast<std::size_t>(expected);
    const auto decimal = []<class T>(T input) {
        std::array<char, std::is_signed_v<T> ? 11 : 20> buffer;
        auto first = buffer.size();
        auto value = static_cast<std::uint64_t>(input);
        do {
            const auto digit = value % 10;
            value /= 10;
            buffer[--first] = static_cast<char>('0' + digit);
        } while (value);
        auto result = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(result.data(), buffer.data() + first, result.size());
        return RetainedString::retainedView(result.retentionOwner(), result.data(), result.size());
    };
    auto expectedText = decimal(expected);
    auto executedText = decimal(executed);
    require(countMatches, concatenateSourceStrings({text("simplephysics composite scenario should execute exactly "),
        expectedText, text(" setting cases, got "), executedText}));
    return executed;
}

void testCompositeMatrixSimplePhysicsSettingsScenario() {
    runCompositeMatrixSimplePhysicsSettings();
}

// Existing regression.d12829-12843.
void testCompositeCaseMatrixBreadth() {
    using nijigenerate::compat::RetainedString;
    ensureRegressionNodeTypesRegistered();
    std::size_t executed = 0;
    executed += runCompositeMatrixGridModes();
    executed += runCompositeMatrixNodeTypes();
    executed += runCompositeMatrixParameterPresets();
    executed += runCompositeMatrixDepthOps();
    executed += runCompositeMatrixAutoMeshProcessors();
    executed += runCompositeMatrixSimplePhysicsSettings();
    const auto expected = 24 + regressionNodeMenuTypes.size() + 10 + 3 + 4 + 15;
    const bool countMatches = executed == expected;
    const auto decimal = [](std::size_t value) {
        std::array<char, 20> buffer;
        auto first = buffer.size();
        do {
            const auto digit = value % 10;
            value /= 10;
            buffer[--first] = static_cast<char>('0' + digit);
        } while (value);
        auto result = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(result.data(), buffer.data() + first, result.size());
        return RetainedString::retainedView(result.retentionOwner(), result.data(), result.size());
    };
    const auto expectedText = decimal(expected);
    const auto executedText = decimal(executed);
    require(countMatches, concatenateSourceStrings({RetainedString::fromStaticLiteral(
        "composite matrix should execute exact generated case count "), expectedText,
        RetainedString::fromStaticLiteral(", got "), executedText}));
    require(generatedCompositeCaseScenarios().size() == expected, RetainedString::fromStaticLiteral(
        "generated composite scenario catalog should match executable matrix count"));
}
