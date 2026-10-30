// Existing regression.d19793-19831; include inside regression::detail after helpers.
void testOneTimeDeformerSchedulesCapturedTargetsOnce() {
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::PathDeformer;
    using nicxlive::core::TaskScheduler;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::compat::ngRequireObject;
    class CountingNode : public Node {
    public:
        std::size_t registrations{};
        void initializeSource(std::shared_ptr<Node> parent) { constructWithParent(std::move(parent)); }
        void registerRenderTasks(std::shared_ptr<TaskScheduler> scheduler) override {
            registrations++;
            Node::registerRenderTasks(scheduler);
        }
    };
    auto runForType = []<class T>() {
        resetCase();
        auto root = ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        auto target = GridDeformer::createSourceDeferred([&] { return root; });
        auto child = std::allocate_shared<CountingNode>(SourceAllocator<CountingNode>{});
        child->initializeSource(target);
        using Filter = nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformer<T>;
        auto filter = Filter::createSource([&] { return std::tuple{root}; });
        filter->captureTarget(target);
        auto scheduler = std::allocate_shared<TaskScheduler>(SourceAllocator<TaskScheduler>{});
        ngRequireObject(root)->registerRenderTasks(scheduler);
        require(child->registrations == 1, nijigenerate::compat::RetainedString::fromStaticLiteral(
            "captured descendants must register render tasks only once"));
        require(target->parentRef() == root, nijigenerate::compat::RetainedString::fromStaticLiteral(
            "one-time capture must preserve the target's original parent"));
        require(filter->children.size() == 1 && filter->children[0] == target,
            nijigenerate::compat::RetainedString::fromStaticLiteral(
                "task registration must restore the captured targets for deformation and baking"));
        scheduler->clearTasks();
        ngRequireObject(root)->registerRenderTasks(scheduler);
        require(child->registrations == 2, nijigenerate::compat::RetainedString::fromStaticLiteral(
            "rebuilding tasks must not duplicate captured descendants"));
        filter->releaseTarget(target);
        require(filter->children.size() == 0, nijigenerate::compat::RetainedString::fromStaticLiteral(
            "captured targets must remain releasable after scheduling"));
    };
    runForType.template operator()<GridDeformer>();
    runForType.template operator()<PathDeformer>();
}
