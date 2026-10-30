#pragma once
#include <cstdint>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/project.hpp>
#include <compat/object_metadata.hpp>
namespace nijigenerate::ext::nodes { class ExDepthBone; class ExDepthRigRoot; }
namespace nijigenerate::viewport::model {
enum class ViewporMenuSortMode : std::int32_t { ZSort, SizeSort };
extern thread_local ViewporMenuSortMode incViewportModelMenuSortMode;
class ModelLayoutViewport : public base::Viewport {
public:
    void selectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> nodes) override;
    void withdraw() override;
    virtual std::shared_ptr<ext::nodes::ExDepthRigRoot> findDepthRoot(std::shared_ptr<ext::nodes::ExDepthBone> bone);
    virtual compat::Slice<std::shared_ptr<ext::nodes::ExDepthRigRoot>> findDepthRoots();
    void draw(std::shared_ptr<nicxlive::core::math::Camera> camera) override;
private:
    void prepareSourceClassInfo() override {
        base::Viewport::prepareSourceClassInfo();
        base::Viewport::registerSourceClassInfo<ModelLayoutViewport>("nijigenerate.viewport.model.ModelLayoutViewport");
    }
};
class ModelViewport : public base::DelegationViewport {
    project::ModelEditSubMode activeSubMode;
    std::shared_ptr<base::Viewport> createSubView(project::ModelEditSubMode mode);
    void syncSubView();
public:
    ModelViewport();
    std::uint32_t asyncDerivedUpdateViewportChannel() override;
    void draw(std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    void drawTools() override;
    void drawOptions() override;
    void drawConfirmBar() override;
    void menu() override;
    void menuOpening() override;
    bool hasMenu() override;
    void armedParameterChanged(std::shared_ptr<nicxlive::core::param::Parameter> parameter) override;
private:
    void prepareSourceClassInfo() override {
        base::DelegationViewport::prepareSourceClassInfo();
        base::Viewport::registerSourceClassInfo<ModelViewport>("nijigenerate.viewport.model.ModelViewport");
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::ViewporMenuSortMode> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.model.ViewporMenuSortMode)";
    inline static constexpr char unqualifiedArgument[] = "(ViewporMenuSortMode)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.model.ViewporMenuSortMode";
    inline static constexpr char unqualifiedElement[] = "ViewporMenuSortMode";
};

template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::ModelLayoutViewport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.model.ModelLayoutViewport)";
    inline static constexpr char unqualifiedArgument[] = "(ModelLayoutViewport)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.model.ModelLayoutViewport";
    inline static constexpr char unqualifiedElement[] = "ModelLayoutViewport";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::model::ModelLayoutViewport> {
    inline static constexpr char className[] = "nijigenerate.viewport.model.ModelLayoutViewport";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.model.ModelLayoutViewport)";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::ModelViewport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.model.ModelViewport)";
    inline static constexpr char unqualifiedArgument[] = "(ModelViewport)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.model.ModelViewport";
    inline static constexpr char unqualifiedElement[] = "ModelViewport";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::model::ModelViewport> {
    inline static constexpr char className[] = "nijigenerate.viewport.model.ModelViewport";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.model.ModelViewport)";
};
} // namespace nicxlive::core::d_runtime
