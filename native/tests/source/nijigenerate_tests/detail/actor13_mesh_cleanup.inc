// Existing regression.d1501-1527 only.
void testRejectedMeshResamplingGroupCleanup() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::SetNodeNameCommand;
    using nijigenerate::viewport::common::mesh::IncMesh;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::param::DeformationParameterBinding;
    using nicxlive::core::param::DeformSlot;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    resetCase();
    auto part = newMeshPart(text("rejected-resampling"));
    auto data = part->getMesh();
    data.indices = {};
    part->rebuffer(data);
    auto param = new2DParameter(text("Rejected resampling"));
    auto binding = std::dynamic_pointer_cast<DeformationParameterBinding>(
        param->createBinding(part, NullableString::fromStaticLiteral("deform"), false));
    param->addBinding(binding);
    ngRequireObject(binding)->setValue({0, 0}, DeformSlot(Vec2Array(
        SharedArray<Vec2>{Vec2{1, 2}, Vec2{3, 4}, Vec2{5, 6}})));
    const auto depth = ngActionStackGroupDepth();
    bool rejected = false;
    try {
        nijigenerate::core::math::mesh::applyMeshToTarget(part,
            Slice<Vec2>{Vec2{10, 10}, Vec2{20, 10}, Vec2{10, 20}}, static_cast<IncMesh*>(nullptr), false);
    } catch (const nijigenerate::compat::ThrowableException& error) {
        rejected = error.msg.view().find("No finite nondegenerate triangle") != std::string_view::npos;
    }
    require(rejected, text("Invalid source triangles must reject deformation resampling"));
    require(ngActionStackGroupDepth() == depth, text("Rejected mesh resampling must close its action group"));
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    ctx->nodes(Context::Nodes{part});
    struct Names {
        operator Slice<RetainedString>() const {
            return Slice<RetainedString>{RetainedString::fromStaticLiteral("after-rejection")};
        }
    };
    require(ngRequireObject(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{}, Names{})
        ->run(ctx))->succeeded, text("Subsequent edit should succeed"));
    incActionUndo();
    require(part->getName() == NullableString::fromStaticLiteral("rejected-resampling"),
        text("Subsequent edits must remain independently undoable"));
}
