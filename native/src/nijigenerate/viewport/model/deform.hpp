#pragma once
#include <nijigenerate/viewport/base.hpp>
#include <memory>
#include <compat/object_metadata.hpp>

namespace nicxlive::core::param { struct Vec2u; }
namespace nijigenerate::ext::nodes { class ExDepthBone; class ExDepthRigRoot; }
namespace nijigenerate::viewport::common::mesheditor { class IncMeshEditor; }

namespace nijigenerate::viewport::model::deform {

class DeformationViewport : public base::Viewport,
    public std::enable_shared_from_this<DeformationViewport> {
public:
    std::shared_ptr<common::mesheditor::IncMeshEditor> editor;
    std::shared_ptr<nicxlive::core::param::Parameter> parameter;

    void draw(std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    virtual std::shared_ptr<ext::nodes::ExDepthRigRoot> findDepthRoot(std::shared_ptr<ext::nodes::ExDepthBone> bone);
    virtual compat::Slice<std::shared_ptr<ext::nodes::ExDepthRigRoot>> findDepthRoots();
    void drawTools() override;
    void drawOptions() override;
    void update(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    void selectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> nodes) override;
    void withdraw() override;
    void armedParameterChanged(std::shared_ptr<nicxlive::core::param::Parameter> parameter) override;
    virtual void paramValueChanged();
    virtual void syncEditor(std::shared_ptr<nicxlive::core::param::Parameter> parameter,
                    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> targets);
    virtual void syncEditor(std::shared_ptr<nicxlive::core::param::Parameter> parameter,
                    nicxlive::core::param::Vec2u keyPoint,
                    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> targets);
    void menu() override;
private:
    void prepareSourceClassInfo() override {
        base::Viewport::prepareSourceClassInfo();
        base::Viewport::registerSourceClassInfo<DeformationViewport>(
            "nijigenerate.viewport.model.deform.DeformationViewport");
    }
};

std::shared_ptr<DeformationViewport> ngDeformationViewport();
void ngViewportNodeDeformNotifyParamValueChanged();
std::shared_ptr<common::mesheditor::IncMeshEditor> ngViewportModelDeformGetEditor();
std::shared_ptr<common::mesheditor::IncMeshEditor> ngViewportModelDeformSyncEditor(
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, nicxlive::core::param::Vec2u keyPoint,
    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> targets);

}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::deform::DeformationViewport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.model.deform.DeformationViewport)";
    inline static constexpr char unqualifiedArgument[] = "(DeformationViewport)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.model.deform.DeformationViewport";
    inline static constexpr char unqualifiedElement[] = "DeformationViewport";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::model::deform::DeformationViewport> {
    inline static constexpr char className[] = "nijigenerate.viewport.model.deform.DeformationViewport";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.model.deform.DeformationViewport)";
};
}
