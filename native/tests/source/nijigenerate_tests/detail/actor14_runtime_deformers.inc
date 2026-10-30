// Existing regression.d20113-20166; include inside regression::detail after helpers.
void testGridDeformerRuntimeInterpolationContracts() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::math::Mat4;
    using nicxlive::core::d_runtime::SourceAllocator;
    resetCase();
    class RuntimeGridDeformer : public GridDeformer {
    public:
        struct SourceReceiverTag {};
        explicit RuntimeGridDeformer(SourceReceiverTag) : GridDeformer(DeferredConstruction{}) {}
        void initializeSource(std::shared_ptr<Node> parent) { constructGridOwned(std::move(parent)); }
        void primeRuntime() {
            nicxlive::core::RenderContext ctx;
            runPreProcessTask(ctx);
        }
    };
    auto grid = std::allocate_shared<RuntimeGridDeformer>(SourceAllocator<RuntimeGridDeformer>{},
        RuntimeGridDeformer::SourceReceiverTag{});
    grid->initializeSource(ngRequireObject(nijigenerate::project::incActivePuppet())->root);
    grid->setName(NullableString::fromStaticLiteral("runtime-grid"));
    grid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}, Vec2{10, 10}}));
    grid->getDeformation() = Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 0}, Vec2{10, 0}});
    grid->primeRuntime();
    auto part = newMeshPart(RetainedString::fromStaticLiteral("runtime-grid-child"));
    part->localTransform.translation = Vec3{0, 0, 0};
    auto vertices = Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{5, 5}, Vec2{10, 10}});
    auto baseDeformation = Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{0, 0}, Vec2{0, 0}});
    auto matrix = Mat4::identity();
    auto unchangedInput = baseDeformation.dup();
    auto unchanged = grid->deformChildren(part, vertices, unchangedInput, &matrix);
    require(unchanged.deformation.size() == vertices.size(),
        RetainedString::fromStaticLiteral("GridDeformer runtime should preserve child deformation length"));
    require(unchanged.changed,
        RetainedString::fromStaticLiteral("GridDeformer runtime should report changed output when grid deformation exists"));
    require(nearVec2(unchanged.deformation[0], Vec2{0, 0}),
        RetainedString::fromStaticLiteral("GridDeformer runtime should keep the left edge unchanged"));
    require(nearVec2(unchanged.deformation[1], Vec2{5, 0}),
        RetainedString::fromStaticLiteral("GridDeformer runtime should bilinearly interpolate center X offset"));
    require(nearVec2(unchanged.deformation[2], Vec2{10, 0}),
        RetainedString::fromStaticLiteral("GridDeformer runtime should apply full right-edge X offset"));
    grid->getDeformation().fill(Vec2{0, 0});
    auto baselineInput = baseDeformation.dup();
    auto baseline = grid->deformChildren(part, vertices, baselineInput, &matrix);
    require(baseline.deformation.size() == baseDeformation.size(),
        RetainedString::fromStaticLiteral("GridDeformer baseline should preserve deformation length"));
    require(!baseline.changed,
        RetainedString::fromStaticLiteral("GridDeformer baseline should report unchanged output with zero deformation"));
    require(nearVec2Array(baseline.deformation, baseDeformation),
        RetainedString::fromStaticLiteral("GridDeformer baseline should not add offsets"));
}
