#include "package.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::viewport::depth::tools {
namespace {
thread_local compat::Slice<DepthToolRef> depthTools;
template<class T> std::shared_ptr<T> allocateDepthTool() {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{});
}
// Consume the same real defining types' independent metadata records.
[[maybe_unused]] const bool builtinDepthToolClassInfo = [] {
    const auto registerType = []<class T> {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<T>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        ngRegisterObjectTypeNames<T>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(T), name);
    };
    registerType.template operator()<directdepth::DepthDirectDepthTool>();
    registerType.template operator()<ring::DepthRingTool>();
    registerType.template operator()<attachedpoint::DepthAttachedPointTool>();
    registerType.template operator()<plane::DepthPlaneTool>();
    return true;
}();
}
void ngRegisterDepthEditTool(DepthToolRef tool) {
    if (tool) tool->sourceOwner_ = tool;
    // D foreach retains the evaluated array descriptor and copies each handle.
    // A virtual mode call can register another tool or alter an escaped slice.
    const auto registeredTools = depthTools;
    for (auto registered : registeredTools) {
        const auto registeredReceiver = compat::ngRequireObject(registered);
        const auto registeredMode = registeredReceiver->mode();
        const auto candidateReceiver = compat::ngRequireObject(tool);
        const auto candidateMode = candidateReceiver->mode();
        if (registeredMode == candidateMode) return;
    }
    depthTools.append(std::move(tool));
}
compat::Slice<DepthToolRef> ngDepthEditTools() {
    if (depthTools.empty()) {
        ngRegisterDepthEditTool(allocateDepthTool<directdepth::DepthDirectDepthTool>());
        ngRegisterDepthEditTool(allocateDepthTool<ring::DepthRingTool>());
        ngRegisterDepthEditTool(allocateDepthTool<attachedpoint::DepthAttachedPointTool>());
        ngRegisterDepthEditTool(allocateDepthTool<plane::DepthPlaneTool>());
    }
    return depthTools;
}
DepthToolRef ngDepthEditTool(DepthToolMode mode) {
    for (auto tool : ngDepthEditTools()) {
        const auto receiver = compat::ngRequireObject(tool);
        if (receiver->mode() == mode) return tool;
    }
    return nullptr;
}
}
