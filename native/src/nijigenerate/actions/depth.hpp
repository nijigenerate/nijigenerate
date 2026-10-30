#pragma once

#include "package.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include "../compat/retained_string_slice.hpp"
#include "../ext/nodes/exdepthops.hpp"
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <memory>
#include <tuple>
#include <utility>

namespace nicxlive::core::nodes { class Node; }
namespace nijigenerate::ext::nodes { class DepthMappedNode; }

namespace nijigenerate::actions::depth {

class DepthMappedChangeAction : public LazyBoundAction,
    public std::enable_shared_from_this<DepthMappedChangeAction> {
private:
    std::shared_ptr<nicxlive::core::nodes::Node> node;
    std::shared_ptr<ext::nodes::DepthMappedNode> depthMapped;
    compat::Slice<float> oldDepths;
    compat::Slice<float> newDepths;
    compat::RetainedStringSlice reason;

    compat::Slice<float> capture();
    void apply(compat::Slice<float> depths);
    struct SourceAllocationTag {};
    explicit DepthMappedChangeAction(SourceAllocationTag);
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(std::shared_ptr<nicxlive::core::nodes::Node> node,
        compat::RetainedStringSlice reason = compat::RetainedString::fromStaticLiteral("Depth Map"));
protected:
    virtual void prepareSourceClassInfo();

public:
    explicit DepthMappedChangeAction(std::shared_ptr<nicxlive::core::nodes::Node> node,
        compat::RetainedStringSlice reason = compat::RetainedString::fromStaticLiteral("Depth Map"));
    template<class Arguments> static std::shared_ptr<DepthMappedChangeAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DepthMappedChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<DepthMappedChangeAction>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    DepthMappedChangeAction(const DepthMappedChangeAction&) = delete;
    DepthMappedChangeAction& operator=(const DepthMappedChangeAction&) = delete;
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

class DepthOperationMappedChangeAction : public LazyBoundAction,
    public std::enable_shared_from_this<DepthOperationMappedChangeAction> {
private:
    struct State {
        compat::Slice<ext::nodes::ExDepthOp> operations;
        compat::Slice<float> baseDepths;
    };
    std::shared_ptr<nicxlive::core::nodes::Node> node;
    std::shared_ptr<ext::nodes::DepthOperationMappedNode> depthOperated;
    State oldState;
    State newState;

    State capture();
    void apply(State state);
    struct SourceAllocationTag {};
    explicit DepthOperationMappedChangeAction(SourceAllocationTag);
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(std::shared_ptr<nicxlive::core::nodes::Node> node);
protected:
    virtual void prepareSourceClassInfo();

public:
    explicit DepthOperationMappedChangeAction(std::shared_ptr<nicxlive::core::nodes::Node> node);
    template<class Arguments>
    static std::shared_ptr<DepthOperationMappedChangeAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DepthOperationMappedChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<DepthOperationMappedChangeAction>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    DepthOperationMappedChangeAction(const DepthOperationMappedChangeAction&) = delete;
    DepthOperationMappedChangeAction& operator=(const DepthOperationMappedChangeAction&) = delete;
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

std::shared_ptr<DepthOperationMappedChangeAction> ngClearDepthOperationsChangeAction(
    std::shared_ptr<nicxlive::core::nodes::Node> node);

} // namespace nijigenerate::actions::depth

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::actions::depth::DepthMappedChangeAction> {
    inline static constexpr char className[]="nijigenerate.actions.depth.DepthMappedChangeAction";
    inline static constexpr char templateArgumentName[]="(nijigenerate.actions.depth.DepthMappedChangeAction)";
};
template<> struct SourceObjectTypeNames<::nijigenerate::actions::depth::DepthOperationMappedChangeAction> {
    inline static constexpr char className[]="nijigenerate.actions.depth.DepthOperationMappedChangeAction";
    inline static constexpr char templateArgumentName[]="(nijigenerate.actions.depth.DepthOperationMappedChangeAction)";
};

#define NG_DEPTH_ACTION_TYPE_ORIGIN(Type, Qualified, Unqualified, Object) \
    template<> struct SourceTypeSpellings<::nijigenerate::actions::depth::Type> { \
        inline static constexpr bool isNativeObject=Object; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(" Qualified ")"), \
                RetainedString::fromStaticLiteral("(" Unqualified ")"), \
                RetainedString::fromStaticLiteral(Qualified),RetainedString::fromStaticLiteral(Unqualified)}; \
        } \
    }
NG_DEPTH_ACTION_TYPE_ORIGIN(DepthMappedChangeAction,
    "nijigenerate.actions.depth.DepthMappedChangeAction","DepthMappedChangeAction",true);
NG_DEPTH_ACTION_TYPE_ORIGIN(DepthOperationMappedChangeAction,
    "nijigenerate.actions.depth.DepthOperationMappedChangeAction","DepthOperationMappedChangeAction",true);
NG_DEPTH_ACTION_TYPE_ORIGIN(DepthOperationMappedChangeAction::State,
    "nijigenerate.actions.depth.DepthOperationMappedChangeAction.State","State",false);
#undef NG_DEPTH_ACTION_TYPE_ORIGIN
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::depth::DepthMappedChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::depth::DepthMappedChangeAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::depth::DepthOperationMappedChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::depth::DepthOperationMappedChangeAction>> {};

} // namespace nijigenerate::core::translation
