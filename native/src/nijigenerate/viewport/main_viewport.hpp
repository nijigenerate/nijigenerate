#pragma once
#include "base.hpp"
#include <nijigenerate/project.hpp>
#include <compat/source_allocator.hpp>
#include <concepts>
#include <memory>
#include <new>
#include <utility>

namespace nijigenerate::viewport::base {
class MainViewport : public DelegationViewport, public compat::SignalReceiver,
    public std::enable_shared_from_this<MainViewport> {
    bool selectionDeferred = false;
    bool parameterDeferred = false;
    struct DifferenceAggregationRegion { int x = 0, y = 0, width = 0, height = 0; };
    bool prepareDifferenceAggregation(std::shared_ptr<nicxlive::core::math::Camera> camera);
    static bool computeDifferenceAggregationRegion(std::shared_ptr<nicxlive::core::nodes::Node> node,
        std::shared_ptr<nicxlive::core::math::Camera> camera, int width, int height,
        DifferenceAggregationRegion& region);
protected:
    MainViewport() = default;
    void constructSource();
public:
    // Native constructors initialize fields only. A derived logical constructor calls
    // MainViewport::constructSource() at its source super() point, after ownership exists.
    // Private derived constructors may grant friendship to MainViewport.
    template<class Derived, class... Args> requires std::derived_from<Derived, MainViewport>
    static std::shared_ptr<Derived> ngCreateDerived(Args&&... args) {
        nicxlive::core::d_runtime::SourceAllocator<Derived> allocator;
        auto* storage = allocator.allocate(1);
        try {
            ::new (static_cast<void*>(storage)) Derived();
        } catch (...) {
            allocator.deallocate(storage, 1);
            throw;
        }
        auto result = std::shared_ptr<Derived>(storage, [allocator](Derived* object) mutable {
            std::destroy_at(object);
            allocator.deallocate(object, 1);
        }, allocator);
        result->constructSource(std::forward<Args>(args)...);
        return result;
    }
    static std::shared_ptr<MainViewport> create();
    void selectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> nodes) override;
    void armedParameterChanged(std::shared_ptr<nicxlive::core::param::Parameter> parameter) override;
    using DelegationViewport::draw;
    using DelegationViewport::update;
    virtual void draw();
    virtual void poll();
    virtual void update(bool localOnly = false, bool presentationOnly = false);
    virtual void onEditModeChanged(project::EditMode mode);
private:
    void prepareSourceClassInfo() override {
        DelegationViewport::prepareSourceClassInfo();
        Viewport::registerSourceClassInfo<MainViewport>("nijigenerate.viewport.base.MainViewport");
    }
};
std::shared_ptr<MainViewport> ngViewport();
void ngPresentTemporaryViewport(std::shared_ptr<Viewport> next);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::base::MainViewport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.base.MainViewport)";
    inline static constexpr char unqualifiedArgument[] = "(MainViewport)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.base.MainViewport";
    inline static constexpr char unqualifiedElement[] = "MainViewport";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::base::MainViewport> {
    inline static constexpr char className[] = "nijigenerate.viewport.base.MainViewport";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.base.MainViewport)";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::base::MainViewport::DifferenceAggregationRegion> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.base.MainViewport.DifferenceAggregationRegion)";
    inline static constexpr char unqualifiedArgument[] = "(DifferenceAggregationRegion)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.base.MainViewport.DifferenceAggregationRegion";
    inline static constexpr char unqualifiedElement[] = "DifferenceAggregationRegion";
};
}
