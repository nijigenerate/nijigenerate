#pragma once
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <memory>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <utility>
#include <tuple>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::viewport::depth::mesheditor::editor { class DepthMeshEditor; }
namespace nijigenerate::viewport::depth::mesheditor::node { class DepthMeshEditorOne; }
namespace nijigenerate::viewport::depth::tools::operation { class DepthOperation; }

namespace nijigenerate::viewport::depth::mesheditor::action {
class DepthOperationListChangeAction : public actions::LazyBoundAction,
    public std::enable_shared_from_this<DepthOperationListChangeAction> {
    using Editor = editor::DepthMeshEditor;
    using EditorRef = std::shared_ptr<node::DepthMeshEditorOne>;
    using Operations = compat::Slice<std::shared_ptr<tools::operation::DepthOperation>>;
    friend class editor::DepthMeshEditor;
    std::shared_ptr<Editor> editor;
    std::weak_ptr<Editor> pendingEditor;
    std::shared_ptr<Editor> retainedEditor() const;
    static std::shared_ptr<DepthOperationListChangeAction> createPending(
        std::shared_ptr<Editor> editor, EditorRef target);
    static std::shared_ptr<DepthOperationListChangeAction> retainForPublication(
        std::shared_ptr<DepthOperationListChangeAction> action);
    EditorRef target;
    Operations oldOperations, newOperations;
    struct SourceAllocationTag {};
    explicit DepthOperationListChangeAction(SourceAllocationTag) {}
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(std::shared_ptr<Editor> editor, EditorRef target);
public:
    DepthOperationListChangeAction(std::shared_ptr<Editor> editor, EditorRef target);
    template<class Arguments>
    static std::shared_ptr<DepthOperationListChangeAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DepthOperationListChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<DepthOperationListChangeAction>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    DepthOperationListChangeAction(const DepthOperationListChangeAction&) = delete;
    DepthOperationListChangeAction& operator=(const DepthOperationListChangeAction&) = delete;
    EditorRef targetEditor() const { return target; }
    static std::shared_ptr<DepthOperationListChangeAction> create(std::shared_ptr<Editor> editor, EditorRef target) {
        return createSource([&] { return std::tuple{std::move(editor), std::move(target)}; });
    }
    void updateNewState() override;
    void clear() override {}
    void rollback() override;
    void redo() override;
    actions::ActionText describe() override;
    actions::ActionText describeUndo() override;
    actions::ActionText getName() override;
    bool merge(actions::ActionPtr) override { return false; }
    bool canMerge(actions::ActionPtr) override { return false; }
    virtual compat::RetainedString toString();
    int opCmp(actions::ActionPtr other) override;
};
class DepthEditorDepthChangeAction : public actions::LazyBoundAction,
    public std::enable_shared_from_this<DepthEditorDepthChangeAction> {
    using Editor = editor::DepthMeshEditor;
    using EditorRef = std::shared_ptr<node::DepthMeshEditorOne>;
    using Operations = compat::Slice<std::shared_ptr<tools::operation::DepthOperation>>;
    std::shared_ptr<Editor> editor;
    EditorRef target;
    compat::Slice<float> oldDepths, oldBaseDepths;
    Operations oldOperations;
    bool oldDirectDepthDirty = false;
    compat::Slice<float> newDepths, newBaseDepths;
    Operations newOperations;
    bool newDirectDepthDirty = false;
    struct SourceAllocationTag {};
    explicit DepthEditorDepthChangeAction(SourceAllocationTag) {}
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(std::shared_ptr<Editor> editor, EditorRef target);
public:
    DepthEditorDepthChangeAction(std::shared_ptr<Editor> editor, EditorRef target);
    template<class Arguments>
    static std::shared_ptr<DepthEditorDepthChangeAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DepthEditorDepthChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<DepthEditorDepthChangeAction>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    DepthEditorDepthChangeAction(const DepthEditorDepthChangeAction&) = delete;
    DepthEditorDepthChangeAction& operator=(const DepthEditorDepthChangeAction&) = delete;
    static std::shared_ptr<DepthEditorDepthChangeAction> create(std::shared_ptr<Editor> editor, EditorRef target) {
        return createSource([&] { return std::tuple{std::move(editor), std::move(target)}; });
    }
    void updateNewState() override;
    void clear() override {}
    void rollback() override;
    void redo() override;
    actions::ActionText describe() override;
    actions::ActionText describeUndo() override;
    actions::ActionText getName() override;
    bool merge(actions::ActionPtr) override { return false; }
    bool canMerge(actions::ActionPtr) override { return false; }
    virtual compat::RetainedString toString();
    int opCmp(actions::ActionPtr other) override;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::depth::mesheditor::action::DepthOperationListChangeAction> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.mesheditor.action.DepthOperationListChangeAction)";
    inline static constexpr char unqualifiedArgument[] = "(DepthOperationListChangeAction)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.mesheditor.action.DepthOperationListChangeAction";
    inline static constexpr char unqualifiedElement[] = "DepthOperationListChangeAction";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::depth::mesheditor::action::DepthOperationListChangeAction> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.mesheditor.action.DepthOperationListChangeAction";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.mesheditor.action.DepthOperationListChangeAction)";
};
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::depth::mesheditor::action::DepthEditorDepthChangeAction> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.mesheditor.action.DepthEditorDepthChangeAction)";
    inline static constexpr char unqualifiedArgument[] = "(DepthEditorDepthChangeAction)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.mesheditor.action.DepthEditorDepthChangeAction";
    inline static constexpr char unqualifiedElement[] = "DepthEditorDepthChangeAction";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::depth::mesheditor::action::DepthEditorDepthChangeAction> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.mesheditor.action.DepthEditorDepthChangeAction";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.mesheditor.action.DepthEditorDepthChangeAction)";
};
}
