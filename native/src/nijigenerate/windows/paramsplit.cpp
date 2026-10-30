// Copyright 2020-2024 Inochi2D/nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "paramsplit.hpp"
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/array_range_error.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <puppet.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::windows::paramsplit {
namespace {
// Only passive built-in byte/array/cache storage belongs inside this boundary.
template<class Operation> decltype(auto) allocateSplitMemory(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

void setMappingPayload(const char* type, std::uint32_t key) {
    static_assert(sizeof(void*) >= sizeof(key));
    // Preserve the authored pointer-sized payload without reading past the key.
    // Only its uint prefix is consumed; trailing bytes carry no mapping data.
    std::array<std::byte, sizeof(void*)> payload{};
    std::memcpy(payload.data(), &key, sizeof(key));
    ImGui::SetDragDropPayload(type, payload.data(), payload.size(), ImGuiCond_Always);
}
std::uint32_t ngBindingNodeUUID(BindingRef binding) {
    const auto node = compat::ngRequireObject(binding)->getNode();
    return node ? node->getUUID() : compat::ngRequireObject(binding)->getNodeUUID();
}
class ParameterSplitBindingsAction : public actions::Action,
    public std::enable_shared_from_this<ParameterSplitBindingsAction> {
    std::size_t idx = 0;
    ParameterRef param, newParam;
    Bindings originalBindings, oldParamBindings, newParamBindings;
    bool hasNewParam() {
        const auto puppet = compat::ngRequireObject(project::incActivePuppet());
        const auto parameters = puppet->parameters;
        const auto needle = newParam;
        for (const auto candidate : parameters)
            if (nicxlive::core::ngResourceObjectEquals(candidate, needle)) return true;
        return false;
    }
    void insertNewParam() {
        if (hasNewParam()) return;
        auto insertIndex = idx + 1;
        if (insertIndex > compat::ngRequireObject(project::incActivePuppet())->parameters.size())
            insertIndex = compat::ngRequireObject(project::incActivePuppet())->parameters.size();
        const auto puppet = compat::ngRequireObject(project::incActivePuppet());
        auto& parameters = puppet->parameters;
        allocateSplitMemory([&] { parameters.insertValue(insertIndex, newParam); });
    }
    template<std::size_t N> actions::ActionText describeText(const char (&key)[N]) {
        const auto keepAlive = weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
        const auto nameReceiver = compat::ngRequireObject(param);
        const auto name = nameReceiver->getName();
        const std::array<core::translation::TypedFormatArgument, 1> values{core::translation::ngFormatArgument(name)};
        auto formatted = core::translation::ngFormatTyped(pattern.view(), values);
        try {
            return compat::RetainedString(std::move(formatted));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
public:
    ParameterSplitBindingsAction(const ParameterSplitBindingsAction&) = delete;
    ParameterSplitBindingsAction& operator=(const ParameterSplitBindingsAction&) = delete;
    ParameterSplitBindingsAction(ParameterSplitBindingsAction&&) = delete;
    ParameterSplitBindingsAction& operator=(ParameterSplitBindingsAction&&) = delete;
    ParameterSplitBindingsAction(std::size_t index, ParameterRef parameter, ParameterRef next,
        Bindings original, Bindings oldBindings, Bindings newBindings)
        : idx(index), param(std::move(parameter)), newParam(std::move(next)),
          originalBindings(std::move(original)), oldParamBindings(std::move(oldBindings)),
          newParamBindings(std::move(newBindings)) {}
    void rollback() override {
        const auto keepAlive = weak_from_this().lock();
        compat::ngRequireObject(project::incActivePuppet())->removeParameter(newParam);
        compat::ngRequireObject(param)->bindings = originalBindings;
        compat::ngRequireObject(newParam)->bindings = {};
    }
    void redo() override {
        const auto keepAlive = weak_from_this().lock();
        compat::ngRequireObject(param)->bindings = oldParamBindings;
        compat::ngRequireObject(newParam)->bindings = newParamBindings;
        insertNewParam();
    }
    actions::ActionText describe() override { return describeText("Split parameter %s"); }
    actions::ActionText describeUndo() override { return describeText("Parameter %s split was cancelled"); }
    actions::ActionText getName() override { return compat::RetainedString::fromStaticLiteral("this"); }
    bool merge(actions::ActionPtr) override { return false; }
    bool canMerge(actions::ActionPtr) override { return false; }
    virtual compat::RetainedString toString() {
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    int opCmp(actions::ActionPtr other) override {
        prepareSourceClassInfo();
        return actions::Action::opCmp(std::move(other));
    }
private:
    static void prepareSourceClassInfo() {
        static const bool registered = [] {
            allocateSplitMemory([] {
                nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterSplitBindingsAction),
                    compat::RetainedString::fromStaticLiteral(
                        "nijigenerate.windows.paramsplit.ParameterSplitBindingsAction"));
            });
            return true;
        }();
        (void)registered;
    }
};
}
ParameterRef ngSplitParameterBindings(std::size_t idx, ParameterRef param,
        compat::Slice<std::uint32_t> takeNodeUUIDs, bool pushAction) {
    const auto next = nicxlive::core::param::Parameter::createSourceDeferred([&] {
        const auto name = compat::ngRequireObject(param)->getName();
        const auto suffix = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral(" (Split)"));
        return allocateSplitMemory([&] {
            std::string splitBytes;
            splitBytes.reserve(name.size() + suffix.size());
            splitBytes.append(name.view());
            splitBytes.append(suffix.view());
            return nicxlive::core::common::NullableString(std::move(splitBytes));
        });
    }, [&] { return compat::ngRequireObject(param)->isVec2; });
    for (std::size_t axis = 0; axis < param->axisPoints.size(); ++axis) {
        auto& destination = next->axisPoints[axis];
        const auto points = param->axisPoints[axis];
        destination = allocateSplitMemory([&] { return points.duplicate(); });
    }
    Bindings oldBindings, newBindings;
    const auto bindings = param->bindings;
    for (auto binding : bindings) {
        const auto uuid = ngBindingNodeUUID(binding);
        if (std::find(takeNodeUUIDs.begin(), takeNodeUUIDs.end(), uuid) != takeNodeUUIDs.end())
            allocateSplitMemory([&] { newBindings.push_back(binding); });
        else allocateSplitMemory([&] { oldBindings.push_back(binding); });
    }
    if (newBindings.empty()) return nullptr;
    const auto currentBindings = param->bindings;
    const auto originalBindings = allocateSplitMemory([&] { return currentBindings.duplicate(); });
    const auto action = std::allocate_shared<ParameterSplitBindingsAction>(
        nicxlive::core::d_runtime::SourceAllocator<ParameterSplitBindingsAction>{},
        idx, param, next, originalBindings, oldBindings, newBindings);
    action->redo();
    if (pushAction) core::actionstack::incActionPush(action);
    return next;
}
void ParamSplitWindow::buildMapping() {
    const auto keepAlive = weak_from_this().lock();
    auto bindings = compat::ngRequireObject(param)->bindings;
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        auto& binding = bindings[index]; const auto uuid = ngBindingNodeUUID(binding);
        if (!mappings.contains(uuid)) {
            ParamMapping value{index, {}, compat::ngRequireObject(binding)->getNode(), false};
            mappings[uuid] = std::move(value);
        }
        auto& mappedBindings = mappingAt(uuid, 141).bindings;
        allocateSplitMemory([&] { mappedBindings.push_back(binding); });
    }
}
void ParamSplitWindow::apply() {
    const auto keepAlive = weak_from_this().lock(); compat::Slice<std::uint32_t> take;
    for (const auto& [uuid, mapping] : mappings) if (mapping.take) take.append(uuid);
    ngSplitParameterBindings(idx, param, take); close();
}
void ParamSplitWindow::oldBindingsList() {
    const auto keepAlive = weak_from_this().lock();
    const auto count = allocateSplitMemory([&] { return mappings.keys(); }).size();
    for (std::size_t i = 0; i < count; ++i) {
        const auto uuid = mappingKeyAt(i, 159);
        auto& mapping = mappingAt(mappingKeyAt(i, 160), 160);
        if (mapping.take) continue;
        {
            const auto nameReceiver = compat::ngRequireObject(mapping.node);
            const auto name = nameReceiver->getName();
            const compat::SourceCString label{compat::RetainedStringSlice(compat::RetainedString(name))};
            compat::detail::ngSourceCStringPostconditions(name.view(), label.data());
            ImGui::Selectable(label.data());
        }
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            setMappingPayload("__OLD_TO_NEW", uuid);
            const auto nameReceiver = compat::ngRequireObject(mapping.node);
            widgets::ngText(compat::RetainedStringSlice(compat::RetainedString(nameReceiver->getName())));
            ImGui::EndDragDropSource();
        }
    }
}
void ParamSplitWindow::newBindingsList() {
    const auto keepAlive = weak_from_this().lock();
    const auto count = allocateSplitMemory([&] { return mappings.keys(); }).size();
    for (std::size_t i = 0; i < count; ++i) {
        const auto uuid = mappingKeyAt(i, 176);
        auto& mapping = mappingAt(mappingKeyAt(i, 177), 177);
        if (!mapping.take) continue;
        {
            const auto nameReceiver = compat::ngRequireObject(mapping.node);
            const auto name = nameReceiver->getName();
            const compat::SourceCString label{compat::RetainedStringSlice(compat::RetainedString(name))};
            compat::detail::ngSourceCStringPostconditions(name.view(), label.data());
            ImGui::Selectable(label.data());
        }
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            setMappingPayload("__NEW_TO_OLD", uuid);
            const auto nameReceiver = compat::ngRequireObject(mapping.node);
            widgets::ngText(compat::RetainedStringSlice(compat::RetainedString(nameReceiver->getName())));
            ImGui::EndDragDropSource();
        }
    }
}
void ParamSplitWindow::onBeginUpdate() {
    const auto keepAlive = weak_from_this().lock();
    const float maximum = std::numeric_limits<float>::max();
    ImGui::SetNextWindowSizeConstraints(ImVec2(640, 480), ImVec2(maximum, maximum));
    base::Window::onBeginUpdate();
}
void ParamSplitWindow::onUpdate() {
    const auto keepAlive = weak_from_this().lock();
    const auto space = widgets::ngAvailableSpace();
    const float gap = 8, width = space.x / 2, height = space.y - 24;
    ImGui::BeginGroup();
    if (ImGui::BeginChild("###OldParam", ImVec2(width, height))) {
        if (ImGui::BeginListBox("###ItemListOld", ImVec2(width - gap, height))) {
            oldBindingsList(); ImGui::EndListBox();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const auto payload = ImGui::AcceptDragDropPayload("__NEW_TO_OLD")) {
                std::uint32_t key;
                std::memcpy(&key, compat::ngRequireObject(payload->Data), sizeof(key));
                mappingAt(key, 216).take = false;
                ImGui::EndDragDropTarget(); ImGui::EndChild(); ImGui::EndGroup(); return;
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::EndChild(); ImGui::SameLine(0, gap);
    if (ImGui::BeginChild("###NewParam", ImVec2(width, height))) {
        if (ImGui::BeginListBox("###ItemListNew", ImVec2(width, height))) { newBindingsList(); ImGui::EndListBox(); }
    }
    ImGui::EndChild();
    if (ImGui::BeginDragDropTarget()) {
        if (const auto payload = ImGui::AcceptDragDropPayload("__OLD_TO_NEW")) {
            std::uint32_t key;
            std::memcpy(&key, compat::ngRequireObject(payload->Data), sizeof(key));
            mappingAt(key, 243).take = true;
            ImGui::EndDragDropTarget(); ImGui::EndGroup(); return;
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::EndGroup(); ImGui::BeginGroup();
    widgets::ngDummy(ImVec2(-64, 24)); ImGui::SameLine(0, 0);
    const auto label = core::translation::ngTranslateCString("Apply");
    if (widgets::ngButtonColored(label.data(), ImVec2(64, 24))) apply();
    ImGui::EndGroup();
}
void ParamSplitWindow::initialize(std::size_t index, ParameterRef parameter) {
    idx = index; param = std::move(parameter); buildMapping();
    base::Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Split Parameter")));
}
std::uint32_t ParamSplitWindow::mappingKeyAt(std::size_t index, std::size_t sourceLine) {
    const auto keys = allocateSplitMemory([&] { return mappings.keys(); });
    if (index < keys.size()) return keys[index];
    throw ::nicxlive::core::d_runtime::ngStaticError<::nicxlive::core::d_runtime::ArrayIndexError>(
        index, keys.size(), compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/windows/paramsplit.d"), sourceLine);
}
ParamMapping& ParamSplitWindow::mappingAt(std::uint32_t key, std::size_t sourceLine) {
    const auto found = mappings.find(key);
    if (found != mappings.end()) return found->second;
    throw ::nicxlive::core::d_runtime::ngStaticError<::nicxlive::core::d_runtime::RangeError>(
        "source/nijigenerate/windows/paramsplit.d", sourceLine);
}
}
