// Existing regression.d19830-19929; include inside regression::detail after helpers.
template<class T> class OneTimePhaseProbe : public T {
    using Node = nicxlive::core::nodes::Node;
    using Grid = nicxlive::core::nodes::GridDeformer;
    using Path = nicxlive::core::nodes::PathDeformer;
    using Vec2Array = nicxlive::core::math::Vec2Array;
    using Mat4 = nicxlive::core::math::Mat4;
    using Context = nicxlive::core::RenderContext;
    using String = nijigenerate::compat::RetainedString;
    static constexpr bool direct = std::is_same_v<T, Grid> || std::is_same_v<T, Path>;
public:
    nijigenerate::compat::Slice<String> phases;
    OneTimePhaseProbe() requires direct : T(typename T::DeferredConstruction{}) {}
    OneTimePhaseProbe() requires (!direct) : T() {}
    void initializeSource(std::shared_ptr<Node> parent) {
        if constexpr (std::is_same_v<T, Grid>) this->constructGridOwned(std::move(parent));
        else if constexpr (std::is_same_v<T, Path>) this->constructPathOwned(std::move(parent), Path::CurveType::Spline);
        else this->constructSource(std::move(parent));
    }
    auto filterCounts() {
        return std::tuple{this->preProcessFilters.size(), this->postProcessFilters.size()};
    }
    void addPostFilters() {
        for (std::size_t stage = 0; stage < 3; ++stage) {
            typename Node::FilterHook hook;
            hook.stage = static_cast<int>(stage);
            hook.func = [](std::shared_ptr<Node>, Vec2Array, Vec2Array, Mat4*) {
                return std::tuple{Vec2Array{}, static_cast<Mat4*>(nullptr), false};
            };
            this->postProcessFilters.push_back(hook);
        }
    }
protected:
    void runBeginTask(Context& ctx) override {
        phases.append(String::fromStaticLiteral("init"));
        T::runBeginTask(ctx);
    }
    void runPreProcessTask(Context& ctx) override {
        phases.append(String::fromStaticLiteral("pre"));
        T::runPreProcessTask(ctx);
    }
    void runDynamicTask(Context& ctx) override {
        phases.append(String::fromStaticLiteral("dynamic"));
        T::runDynamicTask(ctx);
    }
    void runPostTaskImpl(std::size_t priority, Context& ctx) override {
        // std.conv.toChars/core.internal.string.unsignedToTempString, as used by to!string.
        char buffer[64];
        std::size_t first = sizeof(buffer);
        auto value = priority;
        do {
            std::uint32_t digit;
            if (value < 10) { digit = static_cast<std::uint32_t>(value); value = 0; }
            else { digit = static_cast<std::uint32_t>(value % 10); value /= 10; }
            buffer[--first] = static_cast<char>(digit + '0');
        } while (value);
        auto output = allocateSourceStringBytes(sizeof(buffer) - first);
        for (std::size_t i = 0; i < output.size(); ++i) output[i] = buffer[first + i];
        auto number = String::retainedView(output.retentionOwner(), output.data(), output.size());
        phases.append(concatenateSourceStrings({String::fromStaticLiteral("post"), number}));
        T::runPostTaskImpl(priority, ctx);
    }
    void runFinalTask(Context& ctx) override {
        phases.append(String::fromStaticLiteral("final"));
        T::runFinalTask(ctx);
    }
};

template<class T> auto oneTimeFilterSnapshot(bool dynamic, bool postFilters,
    nijigenerate::compat::Slice<std::uint64_t> selected = {},
    nijigenerate::compat::Slice<std::uint64_t> mirrored = {}, bool captureSecond = false) {
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::PathDeformer;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::TaskScheduler;
    using nicxlive::core::TaskOrder;
    using nicxlive::core::TaskKind;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformer;
    resetCase();
    auto root = ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    auto filter = std::allocate_shared<OneTimePhaseProbe<T>>(SourceAllocator<OneTimePhaseProbe<T>>{});
    filter->initializeSource(root);
    auto target = std::allocate_shared<OneTimePhaseProbe<GridDeformer>>(
        SourceAllocator<OneTimePhaseProbe<GridDeformer>>{});
    target->initializeSource(root);
    auto vertices = Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}, Vec2{10, 10}});
    target->rebuffer(vertices.dup());
    std::shared_ptr<OneTimePhaseProbe<GridDeformer>> second;
    if (captureSecond) {
        second = std::allocate_shared<OneTimePhaseProbe<GridDeformer>>(
            SourceAllocator<OneTimePhaseProbe<GridDeformer>>{});
        second->initializeSource(root);
        second->rebuffer(vertices.dup());
    }
    Vec2Array offsets;
    if constexpr (std::is_base_of_v<GridDeformer, T>) {
        filter->rebuffer(vertices.dup());
        filter->switchDynamic(dynamic);
        offsets = Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{4, 2}, Vec2{0, 0}, Vec2{4, 2}});
    } else {
        filter->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{10, 0}}));
        offsets = Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{0, 5}});
    }
    if (postFilters) filter->addPostFilters();
    auto originalFilters = target->filterCounts();
    auto parameterCount = ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.size();
    filter->captureTarget(target);
    if (second) filter->captureTarget(second);
    if constexpr (std::is_same_v<T, OneTimeDeformer<GridDeformer>> ||
                  std::is_same_v<T, OneTimeDeformer<PathDeformer>>) {
        filter->setSelectedVertices(target, selected, mirrored);
    }
    auto scheduler = std::allocate_shared<TaskScheduler>(SourceAllocator<TaskScheduler>{});
    nicxlive::core::RenderContext ctx;
    for (int frame = 0; frame < 2; ++frame) {
        scheduler->clearTasks();
        ngRequireObject(root)->registerRenderTasks(scheduler);
        scheduler->addTask(TaskOrder::Parameters, TaskKind::Parameters,
            [filter, offsets](nicxlive::core::RenderContext&) { filter->getDeformation() = offsets.dup(); });
        scheduler->execute(ctx);
    }
    auto result = std::tuple{filter->phases.duplicate(), target->getDeformation().dup(),
        second ? second->getDeformation().dup() : Vec2Array{}};
    require(!nearVec2Array(target->getDeformation(),
        Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{0, 0}, Vec2{0, 0}, Vec2{0, 0}})),
        RetainedString::fromStaticLiteral("captured target fixture must exercise a nonzero grid/path deformation"));
    require(target->parentRef() == root && nearVec2Array(target->getVertices(), vertices),
        RetainedString::fromStaticLiteral("one-time updates must preserve target hierarchy and base vertices"));
    require(ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.size() == parameterCount,
        RetainedString::fromStaticLiteral("render task registration must not add parameters"));
    filter->releaseTarget(target);
    if (second) filter->releaseTarget(second);
    require(target->filterCounts() == originalFilters && filter->children.size() == 0,
        RetainedString::fromStaticLiteral("release must remove only the temporary filter hooks"));
    scheduler->clearTasks();
    ngRequireObject(root)->registerRenderTasks(scheduler);
    scheduler->execute(ctx);
    require(nearVec2Array(target->getDeformation(),
        Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{0, 0}, Vec2{0, 0}, Vec2{0, 0}})),
        RetainedString::fromStaticLiteral("released targets must return to their unfiltered deformation"));
    return result;
}
