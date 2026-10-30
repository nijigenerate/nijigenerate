#include "command_browser_helpers.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/widgets/toggle.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/controller.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <cfloat>
namespace nijigenerate::windows::command_browser {
using namespace detail;
using project::incActivePuppet;
void CommandBrowserWindow::drawInputsTable(CommandInfo& info) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!ImGui::BeginTable("inputs",4,ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) return;
    for (auto label : {"Name","Type","Description","Value"}) {
        const auto translated = core::translation::ngTranslateCString(label);
        ImGui::TableSetupColumn(translated.data());
    }
    ImGui::TableHeadersRow();
    // Source foreach captures the slice descriptor before invoking widgets.
    // A callback may replace info.inputs without releasing this active range.
    const auto inputs = info.inputs;
    for (auto arg : inputs) {
        ImGui::PushID(sourceCString(arg.name).data());
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(sourceCString(arg.name).data());
        ImGui::TableSetColumnIndex(1); ImGui::Text(sourceCString(arg.typeName).data());
        ImGui::TableSetColumnIndex(2); textWrappedInColumn(arg.desc.empty() ? staticText("-") : arg.desc);
        ImGui::TableSetColumnIndex(3);
        if (!arg.enumValues.empty()) {
            if (!argValues.contains(arg.name)) argValues[arg.name] = arg.enumValues.empty() ? emptyText() : arg.enumValues[0];
            auto current = requiredBrowserEntry(argValues,arg.name,617)->second;
            auto label = concat(staticText("##enum_"),arg.name);
            const auto comboLabel = sourceCString(label);
            const auto comboCurrent = sourceCString(current.empty() ? staticText("-") : current);
            if (ImGui::BeginCombo(comboLabel.data(),comboCurrent.data())) {
                for (auto option : arg.enumValues) {
                    bool selected = option == current;
                    if (ImGui::Selectable(sourceCString(option).data(),selected)) {
                        argValues[arg.name] = option; current = option;
                    }
                }
                ImGui::EndCombo();
            }
        } else if (arg.isBool) {
            if (!argValues.contains(arg.name)) argValues[arg.name] = staticText("false");
            bool value = false; parseBool(requiredBrowserEntry(argValues,arg.name,631)->second,value);
            if (widgets::ngToggleSwitch(concat(staticText("##b_"),arg.name).view(),value,ImVec2(38,20)))
                argValues[arg.name] = staticText(value ? "true" : "false");
        } else if (arg.isFloat) {
            if (!argValues.contains(arg.name)) argValues[arg.name] = emptyText();
            float value = 0;
            const bool hasValue = !requiredBrowserEntry(argValues,arg.name,639)->second.empty() &&
                parseFloat(requiredBrowserEntry(argValues,arg.name,639)->second,value);
            if (!hasValue) { value = 0; argValues[arg.name] = staticText("0"); }
            if (widgets::ngDragFloat(concat(staticText("##f_"),arg.name).view(),&value,0.1f,-FLT_MAX,FLT_MAX,"%.4f",
                    ImGuiSliderFlags_NoRoundToFormat)) argValues[arg.name] = format("%s",value);
        } else if (arg.isIntegral) {
            if (!argValues.contains(arg.name)) argValues[arg.name] = emptyText();
            int value = 0;
            const bool hasValue = !requiredBrowserEntry(argValues,arg.name,649)->second.empty() &&
                parseInteger(requiredBrowserEntry(argValues,arg.name,649)->second.view(),value);
            if (!hasValue) { value = 0; argValues[arg.name] = staticText("0"); }
            if (ImGui::InputInt(sourceCString(concat(staticText("##i_"),arg.name)).data(),&value))
                argValues[arg.name] = format("%s",value);
        } else if (arg.isNodeResourceArray) {
            if (!argNodeSelections.contains(arg.name)) argNodeSelections[arg.name] = Nodes();
            {
                auto selections = argNodeSelections;
                const auto entry = requiredBrowserEntry(selections,arg.name,658).retainValue();
                const auto puppet = incActivePuppet();
                renderResourcePicker(arg.name,*entry,puppet,arg.acceptsNodeResource);
            }
            argValues[arg.name] = resourceSelectionIds(requiredBrowserEntry(argNodeSelections,arg.name,659)->second);
        } else if (arg.typeName.view() == "Parameter[]") {
            if (!argParamSelections.contains(arg.name)) argParamSelections[arg.name] = Parameters();
            {
                auto selections = argParamSelections;
                const auto entry = requiredBrowserEntry(selections,arg.name,663).retainValue();
                const auto puppet = incActivePuppet();
                renderResourcePicker(arg.name,*entry,puppet);
            }
            argValues[arg.name] = resourceSelectionIds(requiredBrowserEntry(argParamSelections,arg.name,664)->second);
        } else if (arg.typeName.view() == "ParameterBinding[]") {
            if (!argBindingSelections.contains(arg.name)) argBindingSelections[arg.name] = Bindings();
            {
                auto selections = argBindingSelections;
                const auto entry = requiredBrowserEntry(selections,arg.name,668).retainValue();
                const auto puppet = incActivePuppet();
                renderResourcePicker(arg.name,*entry,puppet);
            }
            argValues[arg.name] = resourceSelectionIds(requiredBrowserEntry(argBindingSelections,arg.name,669)->second);
        } else if (arg.isNodeResource) {
            Nodes selected;
            if (auto found = argNodeSelections.find(arg.name); found != argNodeSelections.end()) selected = found->second;
            if (selected.isNull()) selected = Nodes();
            if (selected.size() > 1) selected = selected.slice(0,1);
            if (renderResourcePicker(arg.name,selected,incActivePuppet(),arg.acceptsNodeResource)) {
                if (selected.size() > 1) selected = selected.slice(0,1);
                argNodeSelections[arg.name] = selected;
            }
            argValues[arg.name] = resourceSelectionIds(selected);
        } else if (arg.typeName.view() == "Parameter") {
            Parameters selected;
            if (auto found = argParamSelections.find(arg.name); found != argParamSelections.end()) selected = found->second;
            if (selected.isNull()) selected = Parameters();
            if (selected.size() > 1) selected = selected.slice(0,1);
            if (renderResourcePicker(arg.name,selected,incActivePuppet())) {
                if (selected.size() > 1) selected = selected.slice(0,1);
                argParamSelections[arg.name] = selected;
            }
            argValues[arg.name] = !selected.empty() && selected[0]
                ? format("%s",compat::ngRequireObject(selected[0])->getUUID()) : emptyText();
        } else if (arg.typeName.view() == "ParameterBinding") {
            Bindings selected;
            if (auto found = argBindingSelections.find(arg.name); found != argBindingSelections.end()) selected = found->second;
            if (selected.isNull()) selected = Bindings();
            if (selected.size() > 1) selected = selected.slice(0,1);
            if (renderResourcePicker(arg.name,selected,incActivePuppet())) {
                if (selected.size() > 1) selected = selected.slice(0,1);
                argBindingSelections[arg.name] = selected;
            }
            argValues[arg.name] = resourceSelectionIds(selected);
        } else {
            if (!argValues.contains(arg.name)) argValues[arg.name] = emptyText();
            const auto key = format("arg_%s",arg.name);
            // Source fromStringz(array).idup uses a bounded byte search,
            // unlike trimAtNul's decoded countUntil scalar-step contract.
            auto nameBytes = arg.name.view();
            const auto zero = nameBytes.find('\0');
            if (zero != std::string_view::npos) nameBytes = nameBytes.substr(0, zero);
            const auto label = nameBytes.empty() ? BrowserText{} : text(nameBytes);
            auto values = argValues;
            const auto entry = requiredBrowserEntry(values,arg.name,705).retainValue();
            input(key,label,*entry);
        }
        ImGui::PopID();
    }
    auto contextRow = [&](const char* name,const char* type,const char* description) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(name);
        ImGui::TableSetColumnIndex(1); ImGui::Text(type);
        ImGui::TableSetColumnIndex(2);
        const auto translated = core::translation::ngTranslateCString(description);
        ImGui::Text(translated.data());
        ImGui::TableSetColumnIndex(3);
    };
    contextRow("context.nodes","Node[]","Select nodes (optional)");
    if (renderResourcePicker(staticText("ctx_nodes"),ctxNodesSel,incActivePuppet())) contextDirty = true;
    contextRow("context.parameters","Parameter[]","Select parameters (optional)");
    if (renderResourcePicker(staticText("ctx_params"),ctxParamsSel,incActivePuppet())) contextDirty = true;
    contextRow("context.armedParameters","Parameter[]","Select armed parameters (optional)");
    if (renderResourcePicker(staticText("ctx_armed"),ctxArmedSel,incActivePuppet())) contextDirty = true;
    contextRow("context.bindings","ParameterBinding[]","Select bindings (optional)");
    if (renderResourcePicker(staticText("ctx_bindings"),ctxBindingsSel,incActivePuppet())) contextDirty = true;
    contextRow("context.keyPoint","[uint,uint]","Key point (pick via controller), optional");
    Parameters parameters = !ctxParamsSel.empty() ? ctxParamsSel : !ctxArmedSel.empty() ? ctxArmedSel : ctxParamsSel;
    if (parameters.empty() && !argParamSelections.empty()) {
        for (const auto& [key,selection] : argParamSelections) {
            if (!selection.empty()) { parameters = Parameters{selection[0]}; break; }
        }
    }
    const auto translatedNone = core::translation::ngTranslateCString("- (none) -");
    const auto noneLabel = text(std::string_view(translatedNone.data()));
    auto currentLabel = ctxKeyPoint.empty() ? noneLabel : ctxKeyPoint;
    if (ImGui::Button(sourceCString(currentLabel).data())) ImGui::OpenPopup("ctx_kp_popup");
    if (ImGui::BeginPopup("ctx_kp_popup")) {
        if (parameters.empty() || !parameters[0]) ImGui::Text(core::translation::ngTranslateCString("No Parameter selected").data());
        else {
            static thread_local int parameterIndex = 0;
            if (parameterIndex >= std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(parameters.size()))) parameterIndex = 0;
            if (parameters.size() > 1) {
                if (ImGui::BeginCombo("##kp_param_combo",sourceCString(ext::nodes::ngRetainNodeName(
                        compat::ngRequireObject(compat::ngArrayAt(parameters,
                            static_cast<std::size_t>(parameterIndex)))->getName())).data())) {
                    for (std::size_t i = 0; i < parameters.size(); ++i) {
                        auto parameter = parameters[i];
                        if (!parameter) continue;
                        bool selected = i == static_cast<std::size_t>(parameterIndex);
                        if (ImGui::Selectable(sourceCString(ext::nodes::ngRetainNodeName(parameter->getName())).data(),selected))
                            parameterIndex = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i));
                    }
                    ImGui::EndCombo();
                }
            } else ImGui::Text(sourceCString(ext::nodes::ngRetainNodeName(
                compat::ngRequireObject(compat::ngArrayAt(parameters,
                    static_cast<std::size_t>(parameterIndex)))->getName())).data());
            auto parameter = compat::ngRequireObject(
                compat::ngArrayAt(parameters, static_cast<std::size_t>(parameterIndex)));
            ImVec2 size = parameter->isVec2 ? ImVec2(240,160) : ImVec2(240,64);
            auto previous = parameter->value;
            if (widgets::ngController("ctx_kp_ctrl",parameter,size,true)) {
                auto keypoint = compat::ngRequireObject(parameter)->findClosestKeypoint();
                ctxKeyPoint = format("%s,%s",keypoint.x,keypoint.y); contextDirty = true;
            }
            if (!parameter) compat::ngFailNullAccess(true);
            parameter->value = previous;
            ImGui::Separator(); ImGui::Text(core::translation::ngTranslateCString("Click on controller to pick keypoint.").data());
        }
        ImGui::EndPopup();
    }
    ImGui::EndTable();
}
}
