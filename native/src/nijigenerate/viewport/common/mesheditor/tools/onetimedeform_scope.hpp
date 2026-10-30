#pragma once
#include "onetimedeform.hpp"
#include <nijigenerate/core/actionstack.hpp>
#include <common/resource_key_map.hpp>
#include <resource.hpp>
#include <param/binding_impl.hpp>
#include <puppet.hpp>
#include <type_traits>
#include <utility>

namespace nijigenerate::viewport::common::mesheditor::tools::onetimedeform::detail {
using FilterRef = std::shared_ptr<nicxlive::core::nodes::NodeFilter>;
using DeformableRef = std::shared_ptr<nicxlive::core::nodes::Deformable>;
using PuppetRef = std::shared_ptr<nicxlive::core::Puppet>;
using ParameterRef = std::shared_ptr<nicxlive::core::param::Parameter>;
using BindingRef = std::shared_ptr<nicxlive::core::param::DeformationParameterBinding>;
using Vec2u = nicxlive::core::param::Vec2u;
using Values = decltype(nicxlive::core::param::DeformationParameterBinding::values);
using IsSet = std::remove_reference_t<decltype(
    std::declval<nicxlive::core::param::DeformationParameterBinding&>().getIsSet())>;
using ScopeRef = std::shared_ptr<core::actionstack::ActionStackScope>;
struct DeformableHash {
    std::size_t operator()(const DeformableRef& value) const {
        return nicxlive::core::ngResourceInterfaceHash(value);
    }
};
struct DeformableEqual {
    bool operator()(const DeformableRef& query, const DeformableRef& stored) const {
        return nicxlive::core::ngResourceObjectEquals(query, stored);
    }
};
using InitialDeformationMap = nicxlive::core::common::ResourceKeyMap<DeformableRef, Vec2Array,
    DeformableHash, DeformableEqual>;
class OneTimeDeformScope final : public std::enable_shared_from_this<OneTimeDeformScope> {
public:
    OneTimeDeformScope() = default;
    virtual ~OneTimeDeformScope() = default;
    OneTimeDeformScope(const OneTimeDeformScope&) = delete;
    OneTimeDeformScope& operator=(const OneTimeDeformScope&) = delete;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<OneTimeDeformScope> other);
    virtual int opCmp(std::shared_ptr<OneTimeDeformScope> other);
    FilterRef filter;
    EditorRef vertImpl, defImpl;
    std::int32_t vertActionId = 0, defActionId = 0;
    SubToolMode currentMode = SubToolMode::Vertex;
    bool switchModeConsumed = false;
    std::int32_t cachedPeekFrame = -1, cachedPeekAction = 0;
    SubToolMode cachedPeekMode = SubToolMode::Select;
    std::int32_t cachedUpdateFrame = -1, cachedUpdateAction = 0;
    SubToolMode cachedUpdateMode = SubToolMode::Select;
    bool cachedUpdateResult = false, cachedUpdateChanged = false;
    struct SaveBindingBackup {
        BindingRef binding;
        bool bindingAdded = false;
        Values values;
        IsSet isSet;
        // Native bindings use non-owning references; retain the original D object graph during save callbacks.
        ParameterRef parameterOwner;
        DeformableRef targetOwner;
    };
    struct SaveTargetFit { DeformableRef target; BindingRef binding; Vec2Array desired, initial; };
    NodeRef detachedFilterParent;
    std::size_t detachedFilterOffset = 0;
    compat::Slice<SaveBindingBackup> saveBackups;
    ScopeRef actionScope;
    bool explicitScopeClose = false;
    InitialDeformationMap initialTargetDeformations;
    NodeRef filterNode();
    bool active() const { return bool(filter); }
    void begin();
    void closeActionScope();
    void onActionScopeClosed();
    void forceFinalize(NodeRef target);
    template<class T> bool initialize(NodeRef target, NodeRef current = {});
    template<class T> void setup();
    Values copyValues(Values source);
    IsSet copyIsSet(IsSet source);
    BindingRef backupTargetBinding(ParameterRef parameter, DeformableRef target);
    void detachFilterNodeForSave(PuppetRef puppet);
    void flushCurrentSubToolForSave();
    bool applyVertexToolToFilter();
    bool hasTargetChanges();
    void detachForSave(PuppetRef puppet);
    void restoreAfterSave(PuppetRef puppet);
    void cleanupTarget(EditorRef editor, bool allTargets);
    void invalidatePeekCache();
    void resetCurrentTool(EditorRef editor);
    void leaveSubTool(SubToolMode current, bool recordTransientAction = true);
    template<class T> void enterSubTool(SubToolMode next);
    template<class T> void ensureSubToolReady(SubToolMode next);
    void seedViewFromBinding();
    void normalizeDeformationBinding(BindingRef binding, std::size_t length);
    float squaredError(Vec2Array lhs, Vec2Array rhs);
    Vec2Array evaluateWithCandidate(ParameterRef parameter, BindingRef binding, Vec2u keypoint, Vec2Array candidate);
    Vec2Array fitOffsetsForFinalResult(ParameterRef parameter, BindingRef binding, Vec2u keypoint,
        DeformableRef target, Vec2Array initial, Vec2Array desired);
    void applyToTargetsWithFitting(ParameterRef parameter, Vec2u keypoint);
};
struct FilterHash {
    std::size_t operator()(const FilterRef& value) const {
        return nicxlive::core::ngResourceInterfaceHash(
            std::dynamic_pointer_cast<nicxlive::core::Resource>(value));
    }
};
struct FilterEqual {
    bool operator()(const FilterRef& query, const FilterRef& stored) const {
        return nicxlive::core::ngResourceInterfaceKeyEquals(
            std::dynamic_pointer_cast<nicxlive::core::Resource>(query),
            std::dynamic_pointer_cast<nicxlive::core::Resource>(stored));
    }
};
using FilterEditorMap = nicxlive::core::common::ResourceKeyMap<FilterRef, EditorRef, FilterHash, FilterEqual>;
extern thread_local FilterEditorMap gFilterEditors;
extern thread_local std::shared_ptr<OneTimeDeformScope> activeScope;
std::shared_ptr<OneTimeDeformScope> ngOneTimeDeformScope();
std::shared_ptr<OneTimeDeformScope> ngActiveOneTimeDeformScope();
void ngDetachOneTimeDeformFilterForSave(PuppetRef puppet);
void ngRestoreOneTimeDeformFilterAfterSave(PuppetRef puppet);
void ngOnOneTimeDeformScopeClosed(core::actionstack::ActionStackScopeUnit unit);
void ngEnsureOneTimeDeformCallbacks();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::detail::OneTimeDeformScope> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformScope";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformScope)";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::detail::OneTimeDeformScope> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformScope)";
    inline static constexpr char unqualifiedArgument[] = "(OneTimeDeformScope)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformScope";
    inline static constexpr char unqualifiedElement[] = "OneTimeDeformScope";
};
}
#include "onetimedeform.tpp"
