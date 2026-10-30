#include "resource.hpp"
#include "parameters.hpp"
#include "nodes_menu.hpp"
#include <nijigenerate/core/selector/query.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/dragdrop.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <timing.hpp>
#include <unordered_set>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <source_location>
#include <new>
#include <stdexcept>
#include <compat/source_allocator.hpp>

namespace nijigenerate::panels::resource {
using namespace project;
using namespace core::selector;
using namespace widgets;
using core::translation::ngTranslateCString;
using core::translation::ngTranslateRetained;
namespace {
thread_local compat::RetainedString resourcePanelPath=compat::RetainedString::fromStaticLiteral(
    "com.github.nijigenerate.nijigenerate.ResourcePanel");
thread_local std::shared_ptr<ResourcePanel> singleton;
compat::RetainedString concatenate(compat::RetainedString left,std::string_view right) {
    // D's concatenation runtime leaves the result null when total length is zero.
    if(left.empty() && right.empty()) return {};
    try {
        auto bytes=std::string(left.view());
        bytes.append(right);
        return compat::RetainedString(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::Slice<ResourcePtr> asSlice(const SelectorArray<ResourcePtr>& values) {
    if(values.isNull()) return {};
    auto owner=std::allocate_shared<SelectorArray<ResourcePtr>>(
        nicxlive::core::d_runtime::SourceAllocator<SelectorArray<ResourcePtr>>{},values);
    auto data=owner->data();
    auto length=owner->size();
    return compat::Slice<ResourcePtr>::retainedView(std::move(owner),data,length);
}
}
void ngInitializeResourcePanelModule() {
    thread_local bool initialized=false;
    if(!initialized) {
        singleton=std::allocate_shared<ResourcePanel>(nicxlive::core::d_runtime::SourceAllocator<ResourcePanel>{});
        incAddPanel(singleton);initialized=true;
    }
}
void incLoadResourcePanel(std::shared_ptr<nicxlive::core::Puppet> puppet) {
    const auto found=compat::ngRequireObject(puppet)->extData.find(resourcePanelPath);
    if(found!=puppet->extData.end() && !found->second.empty()) {
        const auto& bytes=found->second;
        auto data=nicxlive::core::serde::parseTypedJson({reinterpret_cast<const char*>(bytes.data()),bytes.size()});
        if(singleton) {
            const auto config=std::allocate_shared<ResourcePanelConfig>(
                nicxlive::core::d_runtime::SourceAllocator<ResourcePanelConfig>{});
            (void)config->deserializeFromFghj(data.root());
        }
    }
}
void incDumpResourcePanelPath(std::shared_ptr<nicxlive::core::Puppet> puppet) {
    if(singleton) {
        const auto serializer=std::allocate_shared<nicxlive::core::serde::TypedJsonWriter>(
            nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::serde::TypedJsonWriter>{});
        const auto config=std::allocate_shared<ResourcePanelConfig>(
            nicxlive::core::d_runtime::SourceAllocator<ResourcePanelConfig>{});
        serializer->serializeValue(*config);
        serializer->flush();
        const auto& bytes=serializer->str();
        // Source casts the appender's mutable char buffer without copying it.
        // The actual nonconst writer owns these bytes through the retained view.
        auto* data=reinterpret_cast<std::uint8_t*>(const_cast<char*>(bytes.data()));
        auto retained=nicxlive::core::PuppetExtData::Bytes::retainedView(serializer,data,bytes.size());
        compat::ngRequireObject(puppet)->extData[resourcePanelPath]=std::move(retained);
    }
}
void ngInitResourcePanel() { incRegisterSaveFunc(&incDumpResourcePanelPath); }
core::settings::SettingsJsonValue ngResourcePanelReadback() {
    using Json=core::settings::SettingsJsonValue;
    core::settings::SettingsJsonObject result;
    if(compat::ngRequireObject(singleton)->history.empty()) { result["initialized"]=false;return result; }
    const auto view=singleton->history[singleton->historyIndex];
    auto selector=std::make_shared<Selector>();
    const auto command=compat::ngRequireObject(view)->command;
    const auto query=concatenate(command,singleton->armedParameter?", Binding:active":"");
    selector->build(SelectorString(query));
    std::unordered_set<std::uint32_t> expected,displayed;
    for(auto resource:selector->run()) if(compat::ngRequireObject(resource)->type()==ResourceType::Node) expected.insert(resource->uuid());
    std::uint64_t stale=0,duplicates=0,missing=0;
    const auto nodes=compat::ngRequireObject(view->store)->nodes;
    for(auto resource:nodes) if(compat::ngRequireObject(resource)->type()==ResourceType::Node) {
        if(displayed.contains(resource->uuid())) ++duplicates;
        displayed.insert(resource->uuid());auto node=to<nicxlive::core::nodes::Node>(resource);
        if(!singleton->activePuppet || singleton->activePuppet->findNodeById(compat::ngRequireObject(node)->getUUID())!=node) ++stale;
    }
    for(auto uuid:expected) if(!displayed.contains(uuid)) ++missing;
    result["initialized"]=true;result["history_index"]=singleton->historyIndex;
    result["history_count"]=static_cast<std::uint64_t>(singleton->history.size());
    result["filter"]=core::settings::SettingsString(view->command);
    result["expected_nodes"]=static_cast<std::uint64_t>(expected.size());
    result["displayed_nodes"]=static_cast<std::uint64_t>(displayed.size());
    result["missing_nodes"]=missing;result["stale_nodes"]=stale;result["duplicate_nodes"]=duplicates;
    result["observing_current_root"]=singleton->activePuppet && singleton->observedRoot==singleton->activePuppet->root;
    return result;
}
void ResourcePanelConfig::serialize(nicxlive::core::serde::TypedJsonWriter& serializer) const {
    if(!singleton || singleton->history.empty()) return;
    const auto output=std::dynamic_pointer_cast<output::IconTreeOutput>(compat::ngRequireObject(singleton->history[0])->output);
    if(output) {
        const auto state=serializer.structBegin();serializer.putKey("nextInHorizontal");
        const auto first=serializer.listBegin();
        std::vector<std::uint32_t> keys;for(const auto& entry:output->layout()) keys.push_back(entry.first);
        for(auto uuid:keys) if(output->layout()[uuid].nextInHorizontal) { serializer.elemBegin();serializer.serializeValue(uuid); }
        serializer.listEnd(first);serializer.putKey("folded");const auto second=serializer.listBegin();
        keys.clear();for(const auto& entry:output->layout()) keys.push_back(entry.first);
        for(auto uuid:keys) if(output->layout()[uuid].folded) { serializer.elemBegin();serializer.serializeValue(uuid); }
        serializer.listEnd(second);serializer.structEnd(state);
    }
}
nicxlive::core::serde::ReadError ResourcePanelConfig::deserializeFromFghj(nicxlive::core::serde::TypedJsonView data) {
    using namespace nicxlive::core::serde;
    if(data.isMissing()) return {};
    if(compat::ngRequireObject(singleton)->history.empty()) return {};
    auto view=singleton->history[0];auto output=std::dynamic_pointer_cast<widgets::output::IconTreeOutput>(compat::ngRequireObject(view)->output);
    auto horizontal=data.findFirst("nextInHorizontal");
    if(!horizontal.isMissing() && horizontal.kind()==JsonKind::Array) for(auto element:horizontal.elements()) {
        std::uint32_t uuid=0;(void)deserializeValue(element,uuid);compat::ngRequireObject(output)->layout()[uuid].nextInHorizontal=true;
    }
    auto folded=data.findFirst("folded");
    if(!folded.isMissing() && folded.kind()==JsonKind::Array) for(auto element:folded.elements()) {
        std::uint32_t uuid=0;(void)deserializeValue(element,uuid);compat::ngRequireObject(output)->layout()[uuid].folded=true;
    }
    return {};
}
ResourcePanel::ResourcePanel():Panel(compat::RetainedString::fromStaticLiteral("Resources"),
    ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Resources")),false) {}
void ResourcePanel::execFilter(std::shared_ptr<View> view,bool refreshViews) {
    auto selector=std::allocate_shared<Selector>(nicxlive::core::d_runtime::SourceAllocator<Selector>{});
    const auto command=compat::ngRequireObject(view)->command;
    const auto query=concatenate(command,armedParameter?", Binding:active":"");
    selector->build(SelectorString(query));
    auto nodes=selector->run();
    if(!view->store) view->store=std::allocate_shared<TreeStore>(
        nicxlive::core::d_runtime::SourceAllocator<TreeStore>{});
    view->store->setResources(nodes);
    if(!view->output) view->output=std::allocate_shared<output::IconTreeOutput>(
        nicxlive::core::d_runtime::SourceAllocator<output::IconTreeOutput>{},view->store,shared_from_this());
    if(refreshViews && views) views->refresh(asSlice(nodes));
}
void ResourcePanel::notifyChange(std::shared_ptr<nicxlive::core::nodes::Node>,nicxlive::core::nodes::NotifyReason reason) {
    if(reason==nicxlive::core::nodes::NotifyReason::StructureChanged) forceUpdatePreview=true;
}
void ResourcePanel::onUpdate() {
    auto owner=shared_from_this();parameters::incRunPendingParameterUiCommand();
    auto current=incActivePuppet();auto root=current?current->root:nullptr;
    if(!nicxlive::core::ngPuppetObjectEquals(current,activePuppet) || root!=observedRoot) {
        if(observedRoot) observedRoot->removeNotifyListener(
            nicxlive::core::nodes::NodeNotifyListener::fromMethod<&ResourcePanel::notifyChange>(owner));
        activePuppet=incActivePuppet();observedRoot=root;
        if(observedRoot) observedRoot->addNotifyListener(
            nicxlive::core::nodes::NodeNotifyListener::fromMethod<&ResourcePanel::notifyChange>(owner));
        const auto entries=history;for(auto item:entries) compat::ngRequireObject(compat::ngRequireObject(item)->output)->reset();
        if(views) views->reset();if(activePuppet) incLoadResourcePanel(activePuppet);forceUpdatePreview=true;
    }
    if(!nicxlive::core::ngResourceObjectEquals(incArmedParameter(),armedParameter)) {
        armedParameter=incArmedParameter();forceUpdatePreview=true;
    }
    if(!views) views=std::allocate_shared<output::ViewOutput>(
        nicxlive::core::d_runtime::SourceAllocator<output::ViewOutput>{},owner);
    if(history.empty()) {
        history.append(std::allocate_shared<View>(nicxlive::core::d_runtime::SourceAllocator<View>{},
            compat::RetainedString::fromStaticLiteral("*"),nullptr));
        execFilter(history[history.size()-1]);
    }
    if(forceUpdatePreview) {
        const auto entries=history;for(auto view:entries) execFilter(view,false);
        views->refresh(asSlice(compat::ngRequireObject(compat::ngRequireObject(history[historyIndex])->store)->nodes));forceUpdatePreview=false;
    }
    float scroll=0;const auto available=ngAvailableSpace();const auto position=ImGui::GetCursorScreenPos();
    const ImRect rectangle(position,{position.x+available.x,position.y+available.y});
    ngBeginDragDropFake();
    auto payload=ImGui::AcceptDragDropPayload("_PUPPETNTREE",ImGuiDragDropFlags_AcceptPeekOnly|ImGuiDragDropFlags_SourceAllowNullID);
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left) && payload && payload->Data) {
        const auto mouse=ImGui::GetMousePos();
        if(mouse.x>rectangle.Min.x && mouse.x<rectangle.Max.x) {
            const float speed=static_cast<float>((4*256)*nicxlive::core::deltaTime());
            if(mouse.y<rectangle.Min.y+32 && mouse.y>=rectangle.Min.y) scroll=-speed;
            if(mouse.y>rectangle.Max.y-32 && mouse.y<=rectangle.Max.y) scroll=speed;
        }
    }
    ngEndDragDropFake();
    if(ImGui::BeginChild("ShellMain",{0,-34},ImGuiChildFlags_None)) {
        auto* window=ImGui::GetCurrentWindow();ImGui::SetScrollY(compat::ngRequireObject(window)->Scroll.y+scroll);
        compat::ngRequireObject(compat::ngRequireObject(history[historyIndex])->output)->onUpdate();
    }
    ImGui::EndChild();const auto spacing=ImGui::GetStyle().ItemSpacing;ImGui::GetStyle().ItemSpacing={0,0};
    if(ImGui::BeginPopup("###AddResource")) {
        ImGui::TextColored({0.7f,0.5f,0.5f,1},ngTranslateCString("New Node").data());ImGui::Separator();ImGui::Dummy({0,6});
        panels::nodes::ngAddNodeMenu();
        ImGui::TextColored({0.7f,0.5f,0.5f,1},ngTranslateCString("New Parameter").data());ImGui::Separator();ImGui::Dummy({0,6});
        parameters::incParameterMenuContents(compat::ngRequireObject(incActivePuppet())->parameters);ImGui::EndPopup();
    }
    if(ngButtonColored("\uE145",{30,30})) ImGui::OpenPopup("###AddResource");
    ImGui::GetStyle().ItemSpacing=spacing;views->onUpdate();
}
void ResourcePanel::addCommand(compat::RetainedString command) {
    const auto previous=compat::ngRequireObject(history[history.size()-1])->command;
    const auto combined=concatenate(previous,command.view());
    compat::ngRequireObject(history[history.size()-1])->command=combined;
    forceUpdatePreview=true;
}
void ResourcePanel::setCommand(compat::RetainedString command) {
    history.append(std::allocate_shared<View>(nicxlive::core::d_runtime::SourceAllocator<View>{},
        std::move(command),nullptr));++historyIndex;forceUpdatePreview=true;
}
void ResourcePanel::addPopup(ResourcePtr resource) { compat::ngRequireObject(views)->addResources({std::move(resource)}); }
ResourcePanelConfig::ResourcePanelConfig() { prepareSourceClassInfo(); }

// object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void View::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(View),
            compat::RetainedString::fromStaticLiteral("nijigenerate.panels.resource.View"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString View::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t View::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool View::opEquals(std::shared_ptr<View> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int View::opCmp(std::shared_ptr<View>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

// object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void ResourcePanelConfig::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ResourcePanelConfig),
            compat::RetainedString::fromStaticLiteral("nijigenerate.panels.resource.ResourcePanelConfig"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString ResourcePanelConfig::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t ResourcePanelConfig::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool ResourcePanelConfig::opEquals(std::shared_ptr<ResourcePanelConfig> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int ResourcePanelConfig::opCmp(std::shared_ptr<ResourcePanelConfig>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

}
