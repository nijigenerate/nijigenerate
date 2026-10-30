#pragma once
#include <nijigenerate/core/typed_format.hpp>

#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/ext/nodes/exdepthops.hpp>
#include <nodes/deformable.hpp>
#include <param/binding_impl.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <memory>
#include <type_traits>
#include <tuple>
#include <utility>

namespace nicxlive::core::nodes { struct MeshData; }

namespace nijigenerate::actions::deformable {

class DeformableChangeAction : public GroupAction, public LazyBoundAction {
    using Deformable = nicxlive::core::nodes::Deformable;
    using MeshData = nicxlive::core::nodes::MeshData;

    void copy(MeshData& source, MeshData& destination);

public:
    std::shared_ptr<DeformableChangeAction> shared_from_this() {
        return std::dynamic_pointer_cast<DeformableChangeAction>(GroupAction::shared_from_this());
    }
    std::shared_ptr<const DeformableChangeAction> shared_from_this() const {
        return std::dynamic_pointer_cast<const DeformableChangeAction>(GroupAction::shared_from_this());
    }
    std::weak_ptr<DeformableChangeAction> weak_from_this() noexcept {
        return std::dynamic_pointer_cast<DeformableChangeAction>(GroupAction::weak_from_this().lock());
    }
    std::weak_ptr<const DeformableChangeAction> weak_from_this() const noexcept {
        return std::dynamic_pointer_cast<const DeformableChangeAction>(GroupAction::weak_from_this().lock());
    }
    struct DeformableState {
        compat::Slice<nicxlive::core::math::Vec2> vertices;
        compat::Slice<float> depths;
        bool hasDepths = false;
        compat::Slice<ext::nodes::ExDepthOp> depthOperations;
        compat::Slice<float> depthOperationBaseDepths;
        bool hasDepthOperations = false;
    };
    std::shared_ptr<Deformable> self;
    ActionText name;
    DeformableState state;
    bool undoable = false;

    DeformableChangeAction(ActionText name, std::shared_ptr<Deformable> self);
    template<class Arguments> static std::shared_ptr<DeformableChangeAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DeformableChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<DeformableChangeAction>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    DeformableChangeAction(const DeformableChangeAction&) = delete;
    DeformableChangeAction& operator=(const DeformableChangeAction&) = delete;
    DeformableChangeAction(DeformableChangeAction&&) = delete;
    DeformableChangeAction& operator=(DeformableChangeAction&&) = delete;
    void updateNewState() override;
    void clear() override;
    virtual void addBinding(std::shared_ptr<nicxlive::core::param::Parameter> parameter,
        std::shared_ptr<nicxlive::core::param::ParameterBinding> binding);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr other) override;
    bool canMerge(ActionPtr other) override;
    compat::RetainedString toString() override;

protected:
    virtual void prepareSourceClassInfo();
private:
    struct SourceAllocationTag {};
    explicit DeformableChangeAction(SourceAllocationTag);
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(ActionText name, std::shared_ptr<Deformable> self);
    DeformableState captureState();
    void applyState(DeformableState& state);
    void notifyGeometryChanged();
};

class GridDeformerDefineAction : public LazyBoundAction,
        public std::enable_shared_from_this<GridDeformerDefineAction> {
    using Deformable = nicxlive::core::nodes::Deformable;
    using Binding = nicxlive::core::param::DeformationParameterBinding;
    using BindingValues = decltype(std::declval<Binding&>().values);
    using BindingIsSet = std::remove_reference_t<decltype(std::declval<Binding&>().getIsSet())>;

    struct BindingState {
        std::shared_ptr<Binding> binding;
        BindingValues values;
        BindingIsSet isSet;
    };
    struct State {
        compat::Slice<nicxlive::core::math::Vec2> vertices;
        compat::Slice<float> depths;
        bool hasDepths = false;
        compat::Slice<ext::nodes::ExDepthOp> depthOperations;
        compat::Slice<float> depthOperationBaseDepths;
        bool hasDepthOperations = false;
        compat::Slice<BindingState> bindings;
    };

    std::shared_ptr<Deformable> self;
    ActionText name;
    State oldState;
    State newState;

    static BindingValues dupValues(BindingValues values);
    static BindingIsSet dupIsSet(BindingIsSet values);
    State captureState();
    void applyState(State& state);
    void notifyGeometryChanged();

    struct SourceAllocationTag {};
    explicit GridDeformerDefineAction(SourceAllocationTag);
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(ActionText name, std::shared_ptr<Deformable> self);
protected:
    virtual void prepareSourceClassInfo();
public:
    GridDeformerDefineAction(ActionText name, std::shared_ptr<Deformable> self);
    template<class Arguments> static std::shared_ptr<GridDeformerDefineAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<GridDeformerDefineAction>(
            nicxlive::core::d_runtime::SourceAllocator<GridDeformerDefineAction>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    GridDeformerDefineAction(const GridDeformerDefineAction&) = delete;
    GridDeformerDefineAction& operator=(const GridDeformerDefineAction&) = delete;
    GridDeformerDefineAction(GridDeformerDefineAction&&) = delete;
    GridDeformerDefineAction& operator=(GridDeformerDefineAction&&) = delete;
    void updateNewState() override;
    void clear() override;
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr other) override;
    bool canMerge(ActionPtr other) override;
    virtual compat::RetainedString toString();
};

} // namespace nijigenerate::actions::deformable

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::actions::deformable::DeformableChangeAction> {
    inline static constexpr char className[]="nijigenerate.actions.deformable.DeformableChangeAction";
    inline static constexpr char templateArgumentName[]="(nijigenerate.actions.deformable.DeformableChangeAction)";
};
template<> struct SourceObjectTypeNames<::nijigenerate::actions::deformable::GridDeformerDefineAction> {
    inline static constexpr char className[]="nijigenerate.actions.deformable.GridDeformerDefineAction";
    inline static constexpr char templateArgumentName[]="(nijigenerate.actions.deformable.GridDeformerDefineAction)";
};

#define NG_DEFORMABLE_ACTION_TYPE_ORIGIN(Type, Qualified, Unqualified, Object) \
    template<> struct SourceTypeSpellings<::nijigenerate::actions::deformable::Type> { \
        inline static constexpr bool isNativeObject=Object; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(" Qualified ")"), \
                RetainedString::fromStaticLiteral("(" Unqualified ")"), \
                RetainedString::fromStaticLiteral(Qualified),RetainedString::fromStaticLiteral(Unqualified)}; \
        } \
    }
NG_DEFORMABLE_ACTION_TYPE_ORIGIN(DeformableChangeAction,
    "nijigenerate.actions.deformable.DeformableChangeAction","DeformableChangeAction",true);
NG_DEFORMABLE_ACTION_TYPE_ORIGIN(GridDeformerDefineAction,
    "nijigenerate.actions.deformable.GridDeformerDefineAction","GridDeformerDefineAction",true);
NG_DEFORMABLE_ACTION_TYPE_ORIGIN(DeformableChangeAction::DeformableState,
    "nijigenerate.actions.deformable.DeformableChangeAction.DeformableState","DeformableState",false);
NG_DEFORMABLE_ACTION_TYPE_ORIGIN(GridDeformerDefineAction::BindingState,
    "nijigenerate.actions.deformable.GridDeformerDefineAction.BindingState","BindingState",false);
NG_DEFORMABLE_ACTION_TYPE_ORIGIN(GridDeformerDefineAction::State,
    "nijigenerate.actions.deformable.GridDeformerDefineAction.State","State",false);
#undef NG_DEFORMABLE_ACTION_TYPE_ORIGIN
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::deformable::DeformableChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::deformable::DeformableChangeAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::deformable::GridDeformerDefineAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::deformable::GridDeformerDefineAction>> {};

} // namespace nijigenerate::core::translation
