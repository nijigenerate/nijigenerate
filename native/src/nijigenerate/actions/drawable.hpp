#pragma once
#include <nijigenerate/core/typed_format.hpp>
#include "package.hpp"
#include <nodes/drawable.hpp>
#include <param/parameter.hpp>
#include <param/binding.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <memory>
#include <tuple>
#include <utility>

namespace nijigenerate::actions::drawable {
class DrawableChangeAction : public GroupAction, public LazyBoundAction {
    void copy(nicxlive::core::nodes::MeshData& source, nicxlive::core::nodes::MeshData& destination);
    struct SourceAllocationTag {};
    explicit DrawableChangeAction(SourceAllocationTag);
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(ActionText name, std::shared_ptr<nicxlive::core::nodes::Drawable> self);
protected:
    virtual void prepareSourceClassInfo();
public:
    std::shared_ptr<DrawableChangeAction> shared_from_this() {
        return std::dynamic_pointer_cast<DrawableChangeAction>(GroupAction::shared_from_this());
    }
    std::shared_ptr<const DrawableChangeAction> shared_from_this() const {
        return std::dynamic_pointer_cast<const DrawableChangeAction>(GroupAction::shared_from_this());
    }
    std::weak_ptr<DrawableChangeAction> weak_from_this() noexcept {
        return std::dynamic_pointer_cast<DrawableChangeAction>(GroupAction::weak_from_this().lock());
    }
    std::weak_ptr<const DrawableChangeAction> weak_from_this() const noexcept {
        return std::dynamic_pointer_cast<const DrawableChangeAction>(GroupAction::weak_from_this().lock());
    }
    std::shared_ptr<nicxlive::core::nodes::Drawable> self;
    ActionText name;
    nicxlive::core::nodes::MeshData mesh;
    bool undoable = false;

    DrawableChangeAction(ActionText name, std::shared_ptr<nicxlive::core::nodes::Drawable> self);
    template<class Arguments> static std::shared_ptr<DrawableChangeAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DrawableChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<DrawableChangeAction>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    DrawableChangeAction(const DrawableChangeAction&) = delete;
    DrawableChangeAction& operator=(const DrawableChangeAction&) = delete;
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
};
} // namespace nijigenerate::actions::drawable

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::actions::drawable::DrawableChangeAction> {
    inline static constexpr char className[]="nijigenerate.actions.drawable.DrawableChangeAction";
    inline static constexpr char templateArgumentName[]="(nijigenerate.actions.drawable.DrawableChangeAction)";
};

template<> struct SourceTypeSpellings<::nijigenerate::actions::drawable::DrawableChangeAction> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return {
            RetainedString::fromStaticLiteral("(nijigenerate.actions.drawable.DrawableChangeAction)"),
            RetainedString::fromStaticLiteral("(DrawableChangeAction)"),
            RetainedString::fromStaticLiteral("nijigenerate.actions.drawable.DrawableChangeAction"),
            RetainedString::fromStaticLiteral("DrawableChangeAction")};
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::drawable::DrawableChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::drawable::DrawableChangeAction>> {};

} // namespace nijigenerate::core::translation
