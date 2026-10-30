#include "project.hpp"
#include "project_animation_detail.hpp"
#include <nodes/drawable.hpp>
#include <param/parameter.hpp>
#include <resource.hpp>
#include "nijigenerate/ext/package.hpp"
#include "nijigenerate/core/settings.hpp"
#include "nijigenerate/core/actionstack.hpp"
#include "nijigenerate/core/window.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/windows/base.hpp"
#include "nijigenerate/io/autosave.hpp"
#include "nijigenerate/backend/runtime_debug_flags.hpp"
#include "nijigenerate/compat/switch_error.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include <compat/object_metadata.hpp>
#include <common/object_key_policy.hpp>
#include <array>
#include <runtime_state.hpp>
#include <limits>
#include <new>
#include <stdexcept>

namespace nijigenerate::project {
thread_local compat::Slice<ProjectCallback> newProjectCallbacks;
thread_local compat::Slice<PuppetCallback> loadCallbacks;
thread_local compat::Slice<PuppetCallback> saveCallbacks;
thread_local compat::Slice<PuppetCallback> postSaveCallbacks;

void incRegisterLoadFunc(PuppetCallback callback) { loadCallbacks.append(callback); }
void incRegisterSaveFunc(PuppetCallback callback) { saveCallbacks.append(callback); }
void ngRegisterPostSaveFunc(PuppetCallback callback) { postSaveCallbacks.append(callback); }
void ngRegisterProjectCallback(ProjectCallback callback) { newProjectCallbacks.append(callback); }

namespace {
thread_local std::shared_ptr<Project> activeProject;
thread_local compat::Slice<std::shared_ptr<Node>> selectedNodes;
thread_local compat::Slice<std::shared_ptr<nicxlive::core::nodes::Drawable>> drawables;
thread_local std::shared_ptr<Parameter> armedParam;
thread_local compat::Slice<std::shared_ptr<Parameter>> selectedParams;
thread_local std::size_t armedParamIdx = 0;
thread_local std::shared_ptr<core::animation::AnimationPlayer> incAnimationPlayer;
thread_local AnimationPlaybackRef incAnimationCurrent;

// Used Phobos reference-range removeStable/popExactly bodies. BSL-1.0;
// Andrei Alexandrescu and Phobos contributors: port/licenses/phobos-selection-remove.txt.
template<class T> compat::Slice<std::shared_ptr<T>> dropSelectionFront(
        compat::Slice<std::shared_ptr<T>> range, std::size_t count) {
    const auto length = range.size();
#ifndef NDEBUG
    if (count > length) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
        compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
    if (count > length) throw compat::ngStaticError<compat::ArraySliceError>(count, length, length,
        compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
    return range.slice(count, length);
}

template<class T> compat::Slice<std::shared_ptr<T>> dropSelectionBack(
        compat::Slice<std::shared_ptr<T>> range, std::size_t count) {
    const auto length = range.size();
#ifndef NDEBUG
    if (count > length) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
        compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2153);
#endif
    const auto end = length - count;
    if (end > length) throw compat::ngStaticError<compat::ArraySliceError>(0, end, length,
        compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2156);
    return range.slice(0, end);
}

template<class T> compat::Slice<std::shared_ptr<T>> removeSelectionAt(
        compat::Slice<std::shared_ptr<T>> range, std::size_t position) {
    auto source = dropSelectionFront(range, position);
    auto target = dropSelectionFront(range, position);
    source = dropSelectionFront(source, 1);
    const auto result = dropSelectionBack(range, 1);
    // For source class references move copies the handle and leaves its slot intact.
    for (std::size_t i = 0; i < source.size(); ++i) target.data()[i] = source.data()[i];
    return result;
}

} // namespace

namespace detail {

void installAnimationPlayer(std::shared_ptr<core::animation::AnimationPlayer> player) {
    incAnimationPlayer = std::move(player);
    incAnimationCurrent.reset();
}

AnimationLanes ngCloneAnimationLanes(AnimationLanes lanes) {
    try {
        auto result = lanes.duplicate();
        for (std::size_t index = 0; index < result.size(); ++index)
            result[index].frames = lanes[index].frames.duplicate();
        return result;
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}

Animation ngCloneAnimation(Animation animation) {
    animation.lanes = ngCloneAnimationLanes(animation.lanes);
    return animation;
}

void ngRestoreAnimationLanes(Animation* animation, AnimationLanes lanes) {
    auto restoredLanes = ngCloneAnimationLanes(lanes);
    if (!animation) compat::ngFailNullAccess(true);
    AnimationLanes installedLanes;
    {
        // Release old lane owners only after publishing the complete descriptor
        // and retaining the actual foreach backing; destruction may reenter.
        [[maybe_unused]] const auto previousLanes = animation->lanes;
        animation->lanes = std::move(restoredLanes);
        installedLanes = animation->lanes;
    }
    for (auto& lane : installedLanes) lane.updateFrames();
}

} // namespace detail

namespace {
using detail::AnimationLane;
using detail::AnimationLanes;

[[maybe_unused]] bool ngAnimationLaneTargetIsValid(std::shared_ptr<Puppet> puppet, AnimationLane& lane) {
    if (!lane.paramRef || !lane.paramRef->targetParam) return false;
    auto parameter = lane.paramRef->targetParam;
    const auto sourcePuppet = compat::ngRequireObject(puppet);
    const auto uuid = parameter->getUUID();
    return sourcePuppet->findParameter(uuid) == parameter;
}

[[maybe_unused]] void ngPruneInvalidAnimationLanes(std::shared_ptr<Puppet> puppet) {
    const auto sourcePuppet = compat::ngRequireObject(puppet);
    auto animations = sourcePuppet->getAnimations();
    for (auto& [name, animation] : animations) {
        AnimationLanes validLanes;
        auto lanes = animation.lanes;
        for (auto& lane : lanes) {
            if (!ngAnimationLaneTargetIsValid(puppet, lane)) continue;
            try {
                validLanes.push_back(lane);
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
            }
        }
        animation.lanes = validLanes;
    }
}

}
// D enum.init is its first member, not the integer zero.
thread_local EditMode editMode_ = EditMode::ModelEdit;
thread_local bool incShowVertices = true;
thread_local bool incShowBounds = true;
thread_local bool incShowOrientation = true;
thread_local bool ngShowDepthBones = true;

Project::Project() {
    derivedUpdateScope = core::asyncderivedupdate::incAsyncDerivedUpdateCreateScope();
}
void Project::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Project),
            compat::RetainedString::fromStaticLiteral("nijigenerate.project.Project"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString Project::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Project::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Project::opEquals(std::shared_ptr<Project> other) {
    return nicxlive::core::common::ngClassObjectIdentity(other) == dynamic_cast<const void*>(this);
}
int Project::opCmp(std::shared_ptr<Project>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = detail::ngAllocateProjectStorage([&] {
        return compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    });
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
void ngPruneProjectAnimationLanes(std::shared_ptr<Puppet> puppet) { ngPruneInvalidAnimationLanes(std::move(puppet)); }
void incNewProject() {
    windows::base::incPopWelcomeWindow();
    if (activeProject && !activeProject->path.empty()) io::autosave::incReleaseLockfile();
    incClearImguiData();
    if (activeProject) {
        activeProject->path = compat::RetainedString::emptyLiteral();
        core::asyncderivedupdate::incAsyncDerivedUpdateClearScope(
            compat::ngRequireObject(activeProject)->derivedUpdateScope);
    }
    editMode_ = EditMode::ModelEdit;
    windows::base::incPopWindowListAll();
    activeProject = std::allocate_shared<Project>(nicxlive::core::d_runtime::SourceAllocator<Project>{});
    compat::ngRequireObject(activeProject)->puppet = ext::ExPuppet::create();
    auto callbacks = newProjectCallbacks;
    for (const auto callback : callbacks) {
        if (!callback) compat::ngFailNullAccess(false);
        callback(activeProject);
    }
    incInitAnimationPlayer(compat::ngRequireObject(activeProject)->puppet);
    incFocusCamera(compat::ngRequireObject(compat::ngRequireObject(activeProject)->puppet)->root);
    incSelectNode(nullptr);
    incDisarmParameter();
    nicxlive::core::dbg::inDbgDrawMeshVertexPoints = true;
    nicxlive::core::dbg::inDbgDrawMeshOutlines = true;
    nicxlive::core::dbg::inDbgDrawMeshOrientation = true;
    core::actionstack::incActionClearHistory();
    core::ngSetWindowTitle(compat::RetainedString(core::translation::ngTranslate("New Project")));
    io::autosave::startAutosaveTimer();
}
std::shared_ptr<Puppet> incActivePuppet() { return activeProject ? activeProject->puppet : nullptr; }
std::shared_ptr<Project> incActiveProject() { return activeProject; }
void incResetRootNode(std::shared_ptr<Puppet>& puppet) {
    auto root = compat::ngRequireObject(puppet)->root;
    if (!root) compat::ngFailNullAccess(true);
    root->localTransform.translation = nicxlive::core::math::Vec3{0.0f, 0.0f, 0.0f};
    root->localTransform.rotation = nicxlive::core::math::Vec3{0.0f, 0.0f, 0.0f};
    root->localTransform.scale = nicxlive::core::math::Vec2{1.0f, 1.0f};
}
compat::RetainedStringSlice incProjectPath() {
    return activeProject ? activeProject->path :
        compat::RetainedStringSlice(compat::RetainedString::emptyLiteral());
}
compat::Slice<core::settings::SettingsString> incGetPrevProjects() {
    return core::settings::incSettingsGet<core::settings::SettingsStringArray>(
        compat::RetainedString::fromStaticLiteral("prev_projects"));
}
void incAddPrevProject(core::settings::SettingsString path) {
    auto projects = incGetPrevProjects();
    for (std::size_t index = 0; index < projects.size(); ++index) {
        if (projects[index].view() != path.view()) continue;
        for (std::size_t next = index + 1; next < projects.size(); ++next)
            projects[next - 1] = projects[next];
        projects = projects.slice(0, projects.size() - 1);
        break;
    }

    const auto duplicatedPath = [&path] {
        try {
            return path.empty() ? core::settings::SettingsString{} :
                core::settings::SettingsString(std::string(path.view()));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    }();
    auto recent = [&] {
        try {
            std::vector<core::settings::SettingsString> combined;
            combined.reserve(projects.size() + 1);
            combined.push_back(duplicatedPath);
            for (const auto& project : projects) combined.push_back(project);
            return core::settings::SettingsStringArray(std::move(combined));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 877);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 877);
        }
    }();
    if (recent.size() > 10) recent.resize(10);

    core::settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("prev_projects"), recent);
    core::settings::incSettingsSave();
}
std::shared_ptr<Parameter> incArmedParameter() {
    return editMode_ == EditMode::ModelEdit ? armedParam : nullptr;
}
std::size_t incArmedParameterIdx() { return editMode_ == EditMode::ModelEdit ? armedParamIdx : 0; }
compat::Slice<std::shared_ptr<Node>>& incSelectedNodes() { return selectedNodes; }
compat::Slice<std::shared_ptr<Parameter>>& incSelectedParams() { return selectedParams; }
compat::Slice<std::shared_ptr<Drawable>>& incDrawables() { return drawables; }
std::shared_ptr<Node>& incSelectedNode() {
    return selectedNodes.empty() ? compat::ngRequireObject(incActivePuppet())->root : selectedNodes[0];
}
std::shared_ptr<Parameter> incSelectedParam() {
    return selectedParams.empty() ? std::shared_ptr<Parameter>{} : selectedParams[0];
}
void incSelectNode(std::shared_ptr<Node> node) {
    if (!node) selectedNodes.resize(0);
    else selectedNodes = compat::Slice<std::shared_ptr<Node>>{node};
    auto emittingProject = activeProject;
    compat::ngRequireObject(emittingProject)->SelectionChanged.emit(selectedNodes);
}
void incSelectNodes(compat::Slice<std::shared_ptr<Node>> nodes) {
    selectedNodes = nodes;
    auto emittingProject = activeProject;
    compat::ngRequireObject(emittingProject)->SelectionChanged.emit(selectedNodes);
}
void incSelectParam(std::shared_ptr<Parameter> parameter) {
    selectedParams.resize(0);
    if (parameter) selectedParams = compat::Slice<std::shared_ptr<Parameter>>{parameter};
    auto emittingProject = activeProject;
    compat::ngRequireObject(emittingProject)->ParameterSelectionChanged.emit(incSelectedParam());
}
void incSelectParams(compat::Slice<std::shared_ptr<Parameter>> parameters) {
    selectedParams = parameters;
    auto emittingProject = activeProject;
    compat::ngRequireObject(emittingProject)->ParameterSelectionChanged.emit(incSelectedParam());
}
void incAddSelectNode(std::shared_ptr<Node> node) {
    const auto selection = selectedNodes;
    for (std::size_t index = 0; index < selection.size(); ++index) {
        const auto selected = selection[index];
        if (nicxlive::core::ngResourceObjectEquals(selected, node)) return;
    }
    selectedNodes.append(node);
    auto emittingProject = activeProject;
    compat::ngRequireObject(emittingProject)->SelectionChanged.emit(selectedNodes);
}
void incAddSelectParam(std::shared_ptr<Parameter> parameter) {
    const auto selection = selectedParams;
    for (std::size_t index = 0; index < selection.size(); ++index) {
        const auto selected = selection[index];
        if (nicxlive::core::ngResourceObjectEquals(selected, parameter)) return;
    }
    selectedParams.append(parameter);
    auto emittingProject = activeProject;
    compat::ngRequireObject(emittingProject)->ParameterSelectionChanged.emit(incSelectedParam());
}
void incRemoveSelectNode(std::shared_ptr<Node> node) {
    const auto originalSelection = selectedNodes;
    for (std::size_t index = 0; index < originalSelection.size(); ++index) {
        const auto selected = originalSelection[index];
        const auto targetUuid = compat::ngRequireObject(node)->getUUID();
        const auto selectedUuid = compat::ngRequireObject(selected)->getUUID();
        if (targetUuid == selectedUuid) {
            selectedNodes = removeSelectionAt(selectedNodes, index);
            auto emittingProject = activeProject;
            compat::ngRequireObject(emittingProject)->SelectionChanged.emit(selectedNodes);
        }
    }
}
void incRemoveSelectParam(std::shared_ptr<Parameter> parameter) {
    const auto originalSelection = selectedParams;
    for (std::size_t index = 0; index < originalSelection.size(); ++index) {
        const auto selected = originalSelection[index];
        if (!selected || !parameter) continue;
        const auto selectedUuid = selected->getUUID();
        const auto targetUuid = parameter->getUUID();
        if (selectedUuid == targetUuid) {
            selectedParams = removeSelectionAt(selectedParams, index);
            auto emittingProject = activeProject;
            compat::ngRequireObject(emittingProject)->ParameterSelectionChanged.emit(incSelectedParam());
            break;
        }
    }
}
bool incNodeInSelection(std::shared_ptr<Node> node) {
    const auto selection = selectedNodes;
    for (std::size_t index = 0; index < selection.size(); ++index) {
        const auto selected = selection[index];
        if (!selected) continue;
        const auto targetUuid = compat::ngRequireObject(node)->getUUID();
        const auto selectedUuid = selected->getUUID();
        if (targetUuid == selectedUuid) return true;
    }
    return false;
}
bool incParamInSelection(std::shared_ptr<Parameter> parameter) {
    const auto selection = selectedParams;
    for (std::size_t index = 0; index < selection.size(); ++index) {
        const auto selected = selection[index];
        if (!selected) continue;
        const auto targetUuid = compat::ngRequireObject(parameter)->getUUID();
        const auto selectedUuid = selected->getUUID();
        if (targetUuid == selectedUuid) return true;
    }
    return false;
}
EditMode incEditMode() { return editMode_; }
void incFocusCamera(std::shared_ptr<Node> node) {
    if (!node) return;
    auto bounds = node->getCombinedBounds();
    if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) {
        drawable->updateBounds();
        const float nan = std::numeric_limits<float>::quiet_NaN();
        bounds = drawable->bounds.value_or(std::array{nan, nan, nan, nan});
    }
    const float x = bounds[0] + (bounds[2] - bounds[0]) * 0.5f;
    const float y = bounds[1] + (bounds[3] - bounds[1]) * 0.5f;
    incFocusCamera(node, {-x, -y});
}
void incFocusCamera(std::shared_ptr<Node> node, nicxlive::core::math::Vec2 position) {
    if (!node) return;
    int width, height;
    nicxlive::core::inGetViewport(width, height);
    auto nt = node->transform();
    const auto bounds = node->getCombinedBounds();
    float x = bounds[2] - bounds[0], y = bounds[3] - bounds[1];
    if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const auto db = drawable->bounds.value_or(std::array{nan, nan, nan, nan});
        x = db[2] - db[0]; y = db[3] - db[1];
    } else {
        nt.translation = {bounds[0] + (bounds[2] - bounds[0]) * 0.5f,
            bounds[1] + (bounds[3] - bounds[1]) * 0.5f, 0};
    }
    const float largestViewport = static_cast<float>(std::min(width, height));
    const float largestBounds = x < y ? y : x;
    const float factor = largestViewport / largestBounds;
    const double zoom = std::clamp(static_cast<double>(factor) * 0.90, 0.1, 2.5);
    const auto emitting = compat::ngRequireObject(activeProject);
    emitting->CameraFocused.emit(static_cast<float>(zoom), position);
}
void incSetEditMode(EditMode mode, bool unselect) {
    using namespace core::actionstack;
    if (const auto emitting = activeProject) emitting->EditModeChanging.emit(editMode_);
    switch (mode) {
    case EditMode::VertexEdit: {
        const std::array allowed{ActionStackScopeUnit::VertexEdit};
        ngGuardActionStackScopes(allowed);
        break;
    }
    case EditMode::DepthEdit: {
        const std::array allowed{ActionStackScopeUnit::DepthEdit};
        ngGuardActionStackScopes(allowed);
        break;
    }
    case EditMode::ModelEdit:
    case EditMode::AnimEdit:
    case EditMode::ModelTest:
    case EditMode::ALL: ngGuardActionStackScopes(); break;
    default:
        throw compat::ngStaticError<compat::SwitchError>(
            "No appropriate switch clause found", "source/nijigenerate/project.d", 889);
    }
    if (const auto parameter = armedParam) {
        const auto value = parameter->getClosestKeypointValue(parameter->value);
        compat::ngRequireObject(armedParam)->value = value;
    }
    if (unselect) incSelectNode(nullptr);
    if (mode != EditMode::ModelEdit) {
        const auto project = compat::ngRequireObject(activeProject);
        const auto puppet = compat::ngRequireObject(project->puppet);
        const auto traversalRoot = puppet->root;
        auto found = puppet->findNodesType<Drawable>(traversalRoot);
        drawables = compat::Slice<std::shared_ptr<Drawable>>::retainedView(
            found.retentionOwner(), found.data(), found.size());
    }
    editMode_ = mode;
    if (const auto emitting = activeProject) emitting->EditModeChanged.emit(editMode_);
    if (incAnimationPlayer) {
        incAnimationPlayer->stopAll(true);
        incAnimationPlayer->destroyAll();
    }
    incAnimationCurrent.reset();
}
ModelEditSubMode ngModelEditSubMode() {
    return incArmedParameter() ? ModelEditSubMode::Deform : ModelEditSubMode::Layout;
}
void incArmParameter(std::size_t i, std::shared_ptr<Parameter>& param) {
    armedParam = param;
    armedParamIdx = i;
    {
        // Retain the emitting owner while callbacks may replace the active Project.
        auto emittingProject = activeProject;
        compat::ngRequireObject(emittingProject)->ArmedParameterChanged.emit(armedParam);
    }
    auto puppet = incActivePuppet();
    compat::ngRequireObject(puppet)->enableDrivers = false;
    if (!puppet->getDrivers().empty()) puppet->resetDrivers();
}
void incDisarmParameter() {
    armedParam.reset();
    armedParamIdx = 0;
    {
        // Retain the emitting owner while callbacks may replace the active Project.
        auto emittingProject = activeProject;
        compat::ngRequireObject(emittingProject)->ArmedParameterChanged.emit(armedParam);
    }
    auto puppet = incActivePuppet();
    compat::ngRequireObject(puppet)->enableDrivers = true;
    if (!puppet->getDrivers().empty()) puppet->resetDrivers();
}
void incInitAnimationPlayer(std::shared_ptr<Puppet>) {
    // The D implementation intentionally reads the active project, not its argument.
    incAnimationPlayer = core::animation::AnimationPlayer::create(compat::ngRequireObject(activeProject)->puppet);
    incAnimationCurrent.reset();
}
std::shared_ptr<core::animation::AnimationPlayer>& incAnimationPlayerGet() { return incAnimationPlayer; }
AnimationPlaybackRef incAnimationGet() { return incAnimationCurrent; }
void incAnimationChange(compat::RetainedStringSlice name) {
    {
        const auto player = compat::ngRequireObject(incAnimationPlayer);
        player->stopAll(true);
    }
    {
        const auto player = compat::ngRequireObject(incAnimationPlayer);
        incAnimationCurrent = player->createOrGet(name);
    }
    const auto emittingProject = compat::ngRequireObject(activeProject);
    emittingProject->AnimationChanged.emit(incAnimationCurrent);
}
std::shared_ptr<Project> ngCreateHeadlessRegressionProject() {
    if (activeProject) core::asyncderivedupdate::incAsyncDerivedUpdateClearScope(activeProject->derivedUpdateScope);
    activeProject = std::allocate_shared<Project>(nicxlive::core::d_runtime::SourceAllocator<Project>{});
    activeProject->puppet = ext::ExPuppet::create();
    incInitAnimationPlayer(activeProject->puppet);
    selectedNodes.resize(0);
    selectedParams.resize(0);
    armedParam.reset();
    armedParamIdx = 0;
    editMode_ = EditMode::ModelEdit;
    return activeProject;
}
} // namespace nijigenerate::project
