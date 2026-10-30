// Existing regression.d1655-1700; include inside regression::detail after helpers.
void testAutoMeshParentCancellation() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::viewport::vertex::automesh::grid::GridAutoMeshProcessor;
    using nijigenerate::viewport::common::mesh::IncMesh;
    using nijigenerate::commands::automesh::dynamic::ApplyAutoMeshPT;
    using nijigenerate::commands::CommandResult;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nicxlive::core::nodes::Deformable;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::core::thread_context::Thread;
    resetCase();
    class CancellationProbe : public GridAutoMeshProcessor {
    public:
        std::atomic<std::int32_t> calls{0};
        std::shared_ptr<IncMesh> autoMesh(std::shared_ptr<Deformable>, std::shared_ptr<IncMesh> mesh,
            bool = false, float = 0, bool = false, float = 0) override {
            calls.store(1, std::memory_order_seq_cst);
            nijigenerate::core::tasks::TaskFiber::yield();
            calls.store(2, std::memory_order_seq_cst);
            return mesh;
        }
    };
    auto processor = std::allocate_shared<CancellationProbe>(SourceAllocator<CancellationProbe>{});
    auto part = newMeshPart(RetainedString::fromStaticLiteral("cancel-probe"));
    auto context = std::allocate_shared<Context>(SourceAllocator<Context>{});
    context->puppet(nijigenerate::project::incActivePuppet());
    context->nodes(Context::Nodes{part});
    auto command = constructSourceCommand<ApplyAutoMeshPT<GridAutoMeshProcessor>>(processor,
        [processor] { return processor->calls.load(std::memory_order_seq_cst) > 0; });
    auto pending = command->run(context);
    // Native lowering of the original delegate's shared captured local frame.
    struct CapturedFrame {
        std::atomic<bool> done{false};
        std::shared_ptr<CommandResult> completed;
    };
    auto frame = std::allocate_shared<CapturedFrame>(SourceAllocator<CapturedFrame>{});
    auto waiter = Thread::createSourceDeferred([&] {
        return [pending, frame] {
            frame->completed = ngRequireObject(pending)->waitForCompletion();
            frame->done.store(true, std::memory_order_seq_cst);
        };
    });
    waiter->start();
    for (int index = 0; index < 500; ++index) {
        nijigenerate::api::mcp::task::ngMcpProcessQueue();
        if (frame->done.load(std::memory_order_seq_cst)) break;
        Thread::sleep(std::chrono::milliseconds{10});
    }
    require(frame->done.load(std::memory_order_seq_cst),
        RetainedString::fromStaticLiteral("Canceled AutoMesh must finish without waiting for the mesh computation"));
    waiter->join();
    require(!ngRequireObject(frame->completed)->succeeded &&
        frame->completed->message.view().find("canceled") != std::string_view::npos,
        RetainedString::fromStaticLiteral("Parent cancellation must be propagated to the AutoMesh result"));
    require(processor->calls.load(std::memory_order_seq_cst) == 1,
        RetainedString::fromStaticLiteral("Canceled AutoMesh must not resume its suspended computation"));
    require(part->getVertices().size() == 3,
        RetainedString::fromStaticLiteral("Canceled AutoMesh must not apply queued mesh changes"));
}
