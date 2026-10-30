#include "command_browser_resources.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/utils.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <functional>
namespace nijigenerate::windows::command_browser {
using utils::incTypeIdToIcon;
namespace detail {
namespace {
detail::BrowserClassMap<BindingRef,std::uint32_t> gBindingIds;
struct BindingIdHash {
    std::size_t operator()(std::uint32_t value) const noexcept { return value; }
};
nicxlive::core::common::ResourceKeyMap<std::uint32_t,BindingRef,
    BindingIdHash,std::equal_to<std::uint32_t>> gBindingById;
}
std::uint32_t ensureCommandBrowserBindingId(BindingRef binding) {
    auto found = gBindingIds.find(binding);
    if (found != gBindingIds.end()) return found->second;
    const auto id = gBindingNextId++;
    gBindingIds[binding] = id; gBindingById[id] = binding; return id;
}
BindingRef findBindingByCommandBrowserId(std::uint32_t id) {
    auto puppet = project::incActivePuppet();
    if (!puppet) return {};
    const auto parameters = puppet->parameters;
    for (auto param : parameters) {
        const auto bindings = compat::ngRequireObject(param)->bindings;
        for (auto binding : bindings)
            if (ensureCommandBrowserBindingId(binding) == id) return binding;
    }
    return {};
}
}
namespace {
template<class... T> std::string format(std::string_view pattern,T&&... args) {
    std::array<core::translation::TypedFormatArgument,sizeof...(T)> values{
        core::translation::ngFormatArgument(std::forward<T>(args))...};
    return core::translation::ngFormatTyped(pattern,values);
}
}
void renderNodeTree(NodeRef node, NodeRef& chosen, bool (*accepts)(NodeRef)) {
    if (!node) return;
    const bool selectable = !accepts || accepts(node);
    ImGuiTreeNodeFlags flags = node->children.empty()
        ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen : ImGuiTreeNodeFlags_None;
    flags |= ImGuiTreeNodeFlags_DefaultOpen;
    const auto icon = incTypeIdToIcon(node->typeId());
    const auto name = node->getName();
    const auto uuid = node->getUUID();
    const auto label = detail::retainBrowserText(format("%s %s##node_%s",icon,name,uuid));
    if (ImGui::TreeNodeEx(detail::sourceCString(label).data(),flags)) {
        if (selectable && ImGui::IsItemClicked()) chosen = node;
        if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
            const auto children = node->children;
            for (auto child : children) renderNodeTree(child,chosen,accepts);
            ImGui::TreePop();
        }
    }
}
void renderParameterTree(PuppetRef puppet, ParameterRef& chosen) {
    if (!puppet) return;
    const auto parameters = puppet->parameters;
    for (auto param : parameters) {
        ImGuiTreeNodeFlags flags = !compat::ngRequireObject(param)->bindings.empty() ? ImGuiTreeNodeFlags_None
            : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
        const auto icon = incTypeIdToIcon("Parameter");
        const auto name = compat::ngRequireObject(param)->getName();
        const auto uuid = compat::ngRequireObject(param)->getUUID();
        const auto label = detail::retainBrowserText(format("%s %s##param_%s",icon,name,uuid));
        if (ImGui::TreeNodeEx(detail::sourceCString(label).data(),flags)) {
            if (ImGui::IsItemClicked()) chosen = param;
            if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) ImGui::TreePop();
        }
    }
}
void renderBindingTree(PuppetRef puppet, BindingRef& chosen) {
    if (!puppet) return;
    const auto parameters = puppet->parameters;
    for (auto param : parameters) {
        if (compat::ngRequireObject(param)->bindings.empty()) continue;
        const auto icon = incTypeIdToIcon("Parameter");
        const auto name = compat::ngRequireObject(param)->getName();
        const auto uuid = compat::ngRequireObject(param)->getUUID();
        const auto label = detail::retainBrowserText(format("%s %s##param_binding_%s",icon,name,uuid));
        if (ImGui::TreeNodeEx(detail::sourceCString(label).data(),ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto bindings = compat::ngRequireObject(param)->bindings;
            for (auto binding : bindings) {
                auto id = detail::ensureCommandBrowserBindingId(binding);
                const auto icon = incTypeIdToIcon("Binding");
                const auto name = compat::ngRequireObject(binding)->getNameDescriptor();
                const auto bindingLabel = detail::retainBrowserText(format("%s %s##binding_%s",icon,name,id));
                if (ImGui::TreeNodeEx(detail::sourceCString(bindingLabel).data(),
                    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen))
                    if (ImGui::IsItemClicked()) chosen = binding;
            }
            ImGui::TreePop();
        }
    }
}
BrowserText labelFor(NodeRef value) {
    if (!value) {
        const auto translated = core::translation::ngTranslateCString("- (none) -");
        const auto bytes = std::string_view(translated.data());
        return detail::allocateBrowserMemory([&] { return BrowserText(compat::RetainedString(bytes)); });
    }
    return ext::nodes::ngRetainNodeName(value->getName());
}
BrowserText labelFor(ParameterRef value) {
    if (!value) {
        const auto translated = core::translation::ngTranslateCString("- (none) -");
        const auto bytes = std::string_view(translated.data());
        return detail::allocateBrowserMemory([&] { return BrowserText(compat::RetainedString(bytes)); });
    }
    return ext::nodes::ngRetainNodeName(value->getName());
}
BrowserText labelFor(BindingRef value) {
    if (!value) {
        const auto translated = core::translation::ngTranslateCString("- (none) -");
        const auto bytes = std::string_view(translated.data());
        return detail::allocateBrowserMemory([&] { return BrowserText(compat::RetainedString(bytes)); });
    }
    return ext::nodes::ngRetainNodeName(value->getNameDescriptor());
}
}
