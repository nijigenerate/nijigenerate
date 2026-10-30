#pragma once

#include <cstddef>
#include <array>
#include <memory>
#include <string>
#include <nodes/node.hpp>
#include <texture.hpp>
#include <puppet.hpp>
#include "nijigenerate/compat/signal.hpp"
#include "nijigenerate/compat/retained_string_slice.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/core/animation/player.hpp"
#include "nijigenerate/core/asyncderivedupdate.hpp"
#include "nijigenerate/core/settings_string.hpp"
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>

namespace nicxlive::core::nodes { class Drawable; }

namespace nijigenerate::project {
using nicxlive::core::Puppet;
using nicxlive::core::Texture;
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Drawable;
using nicxlive::core::param::Parameter;
using core::animation::AnimationPlaybackRef;
using core::asyncderivedupdate::AsyncDerivedUpdateScopeId;

enum class EditMode { ModelEdit = 1, VertexEdit = 2, AnimEdit = 4, ModelTest = 8, DepthEdit = 16, ALL = 31 };
enum class ModelEditSubMode { Layout, Deform };
extern thread_local EditMode editMode_;
extern thread_local bool incShowVertices;
extern thread_local bool incShowBounds;
extern thread_local bool incShowOrientation;
extern thread_local bool ngShowDepthBones;

class Project : public std::enable_shared_from_this<Project> {
public:
    Project();
    virtual ~Project() = default;
    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;
    Project(Project&&) = delete;
    Project& operator=(Project&&) = delete;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Project> other);
    virtual int opCmp(std::shared_ptr<Project> other);
    AsyncDerivedUpdateScopeId derivedUpdateScope;
    std::shared_ptr<Puppet> puppet;
    compat::Slice<std::shared_ptr<Texture>> textures;
    compat::RetainedStringSlice path;
    struct NodeEvent : compat::Signal<compat::Slice<std::shared_ptr<Node>>> {};
    struct ParamEvent : compat::Signal<std::shared_ptr<Parameter>> {};
    struct EditModeEvent : compat::Signal<EditMode> {};
    struct AnimationEvent : compat::Signal<AnimationPlaybackRef> {};
    struct CameraFocusEvent : compat::Signal<float, nicxlive::core::math::Vec2> {};
    NodeEvent SelectionChanged;
    ParamEvent ArmedParameterChanged;
    ParamEvent ParameterSelectionChanged;
    EditModeEvent EditModeChanging;
    EditModeEvent EditModeChanged;
    AnimationEvent AnimationChanged;
    CameraFocusEvent CameraFocused;
protected:
    virtual void prepareSourceClassInfo();
};

using ProjectCallback = void (*)(std::shared_ptr<Project>);
using PuppetCallback = void (*)(std::shared_ptr<Puppet>);
extern thread_local compat::Slice<ProjectCallback> newProjectCallbacks;
extern thread_local compat::Slice<PuppetCallback> loadCallbacks;
extern thread_local compat::Slice<PuppetCallback> saveCallbacks;
extern thread_local compat::Slice<PuppetCallback> postSaveCallbacks;

void incRegisterLoadFunc(PuppetCallback callback);
void incRegisterSaveFunc(PuppetCallback callback);
void ngRegisterPostSaveFunc(PuppetCallback callback);
void ngRegisterProjectCallback(ProjectCallback callback);

std::shared_ptr<Puppet> incActivePuppet();
std::shared_ptr<Project> incActiveProject();
void incNewProject();
bool incOpenProject(compat::RetainedStringSlice path);
bool incOpenProject(compat::RetainedStringSlice mainPath, compat::RetainedStringSlice backupPath);
bool incSaveProject(compat::RetainedStringSlice path,
    compat::RetainedStringSlice autosaveStamp = compat::RetainedString::emptyLiteral());
void ngPruneProjectAnimationLanes(std::shared_ptr<Puppet> puppet);
void incClearImguiData();
void incResetRootNode(std::shared_ptr<Puppet>& puppet);
compat::RetainedStringSlice incProjectPath();
compat::Slice<core::settings::SettingsString> incGetPrevProjects();
void incAddPrevProject(core::settings::SettingsString path);
std::shared_ptr<Parameter> incArmedParameter();
std::size_t incArmedParameterIdx();
compat::Slice<std::shared_ptr<Node>>& incSelectedNodes();
compat::Slice<std::shared_ptr<Parameter>>& incSelectedParams();
compat::Slice<std::shared_ptr<Drawable>>& incDrawables();
std::shared_ptr<Node>& incSelectedNode();
std::shared_ptr<Parameter> incSelectedParam();
void incSelectNode(std::shared_ptr<Node> node = nullptr);
void incSelectNodes(compat::Slice<std::shared_ptr<Node>> nodes);
void incSelectParam(std::shared_ptr<Parameter> parameter = nullptr);
void incSelectParams(compat::Slice<std::shared_ptr<Parameter>> parameters);
void incAddSelectNode(std::shared_ptr<Node> node);
void incAddSelectParam(std::shared_ptr<Parameter> parameter);
void incRemoveSelectParam(std::shared_ptr<Parameter> parameter);
void incRemoveSelectNode(std::shared_ptr<Node> node);
bool incNodeInSelection(std::shared_ptr<Node> node);
bool incParamInSelection(std::shared_ptr<Parameter> parameter);
EditMode incEditMode();
void incSetEditMode(EditMode mode, bool unselect = true);
void incFocusCamera(std::shared_ptr<Node> node);
void incFocusCamera(std::shared_ptr<Node> node, nicxlive::core::math::Vec2 position);
ModelEditSubMode ngModelEditSubMode();
void incArmParameter(std::size_t i, std::shared_ptr<Parameter>& param);
void incDisarmParameter();
void incInitAnimationPlayer(std::shared_ptr<Puppet> puppet);
std::shared_ptr<core::animation::AnimationPlayer>& incAnimationPlayerGet();
AnimationPlaybackRef incAnimationGet();
void incAnimationChange(compat::RetainedStringSlice name);
bool ngAnimationCreateOrUpdate(compat::RetainedStringSlice name, nicxlive::core::Animation animation,
    compat::RetainedStringSlice originalName = compat::RetainedString::emptyLiteral());
bool ngAnimationDelete(compat::RetainedStringSlice name);
void incAnimationKeyframeAdd(std::shared_ptr<Parameter>& parameter, int axis, float value);
bool incAnimationKeyframeRemove(std::shared_ptr<Parameter>& parameter, int axis);
compat::Slice<compat::RetainedString> incAnimationKeysGet();
void incAnimationRender();
void incAnimationUpdate();
void incSelectAll();
void incImportFolder(compat::RetainedStringSlice folder);
void incImportINP(compat::RetainedStringSlice file);
void ngRestorePuppetMemory(compat::Slice<std::uint8_t> data);
void incExportINP(compat::RetainedStringSlice file);
void incRegenerateMipmaps();
void incRebleedTextures();
std::shared_ptr<Project> ngCreateHeadlessRegressionProject();
} // namespace nijigenerate::project

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::project::EditMode> {
    using Enum = ::nijigenerate::project::EditMode;
    inline static constexpr std::string_view name = "EditMode";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::ModelEdit, "ModelEdit"},
        FormatEnumMember<Enum>{Enum::VertexEdit, "VertexEdit"},
        FormatEnumMember<Enum>{Enum::AnimEdit, "AnimEdit"},
        FormatEnumMember<Enum>{Enum::ModelTest, "ModelTest"},
        FormatEnumMember<Enum>{Enum::DepthEdit, "DepthEdit"},
        FormatEnumMember<Enum>{Enum::ALL, "ALL"}};
};
template<> struct FormatEnumTraits<::nijigenerate::project::ModelEditSubMode> {
    using Enum = ::nijigenerate::project::ModelEditSubMode;
    inline static constexpr std::string_view name = "ModelEditSubMode";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Layout, "Layout"},
        FormatEnumMember<Enum>{Enum::Deform, "Deform"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
#define NG_PROJECT_TYPE_ORIGINS(Type, Qualified, Unqualified, Reference) \
template<> struct SourceTypeSpellings<::nijigenerate::project::Type> { \
    inline static constexpr bool isNativeObject = Reference; \
    inline static constexpr char qualifiedArgument[] = "(" Qualified ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" Unqualified ")"; \
    inline static constexpr char qualifiedElement[] = Qualified; \
    inline static constexpr char unqualifiedElement[] = Unqualified; \
};
NG_PROJECT_TYPE_ORIGINS(Project, "nijigenerate.project.Project", "Project", true)
NG_PROJECT_TYPE_ORIGINS(EditMode, "nijigenerate.project.EditMode", "EditMode", false)
NG_PROJECT_TYPE_ORIGINS(ModelEditSubMode, "nijigenerate.project.ModelEditSubMode", "ModelEditSubMode", false)
NG_PROJECT_TYPE_ORIGINS(Project::NodeEvent, "nijigenerate.project.Project.NodeEvent", "NodeEvent", false)
NG_PROJECT_TYPE_ORIGINS(Project::ParamEvent, "nijigenerate.project.Project.ParamEvent", "ParamEvent", false)
NG_PROJECT_TYPE_ORIGINS(Project::EditModeEvent,
    "nijigenerate.project.Project.EditModeEvent", "EditModeEvent", false)
NG_PROJECT_TYPE_ORIGINS(Project::AnimationEvent,
    "nijigenerate.project.Project.AnimationEvent", "AnimationEvent", false)
NG_PROJECT_TYPE_ORIGINS(Project::CameraFocusEvent,
    "nijigenerate.project.Project.CameraFocusEvent", "CameraFocusEvent", false)
#undef NG_PROJECT_TYPE_ORIGINS
template<> struct SourceObjectTypeNames<::nijigenerate::project::Project> {
    inline static constexpr char className[] = "nijigenerate.project.Project";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.project.Project)";
};
}

#include <nijigenerate/core/typed_format.hpp>

namespace nijigenerate::core::translation {
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::project::Project>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::project::Project>> {};
}

// Actual source public imports. Keep declarations above available to modules
// that include the project header back through their own canonical interfaces.
#include <nijigenerate/ver.hpp>
#include <nijigenerate/atlas/package.hpp>
#include <nijigenerate/io/package.hpp>
