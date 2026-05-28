/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.windows.paramsplit;
import nijigenerate.windows.base;
import nijigenerate.core;
import nijigenerate.widgets.dummy;
import nijigenerate.widgets.label;
import nijigenerate.widgets.button;
import nijigenerate;
import nijigenerate.actions;
import nijigenerate.core.actionstack;
import std.string;
import nijigenerate.utils.link;
import nijilive;
import i18n;
import std.algorithm.searching : canFind, countUntil;
import std.array : insertInPlace;

struct ParamMapping {
    size_t idx;
    ParameterBinding[] bindings;
    nijilive.core.Resource node;
    bool take;
}

private uint ngBindingNodeUUID(ParameterBinding binding) {
    auto node = binding.getNode();
    if (node !is null)
        return node.uuid;
    return binding.getNodeUUID();
}

private class ParameterSplitBindingsAction : Action {
private:
    size_t idx;
    Parameter param;
    Parameter newParam;
    ParameterBinding[] originalBindings;
    ParameterBinding[] oldParamBindings;
    ParameterBinding[] newParamBindings;

    bool hasNewParam() {
        return incActivePuppet().parameters.countUntil(newParam) >= 0;
    }

    void insertNewParam() {
        if (hasNewParam()) return;

        auto insertIndex = idx + 1;
        if (insertIndex > incActivePuppet().parameters.length)
            insertIndex = incActivePuppet().parameters.length;
        incActivePuppet().parameters.insertInPlace(insertIndex, newParam);
    }

public:
    this(size_t idx, Parameter param, Parameter newParam, ParameterBinding[] originalBindings, ParameterBinding[] oldParamBindings, ParameterBinding[] newParamBindings) {
        this.idx = idx;
        this.param = param;
        this.newParam = newParam;
        this.originalBindings = originalBindings;
        this.oldParamBindings = oldParamBindings;
        this.newParamBindings = newParamBindings;
    }

    void rollback() {
        incActivePuppet().removeParameter(newParam);
        param.bindings = originalBindings;
        newParam.bindings = [];
    }

    void redo() {
        param.bindings = oldParamBindings;
        newParam.bindings = newParamBindings;
        insertNewParam();
    }

    string describe() {
        return _("Split parameter %s").format(param.name);
    }

    string describeUndo() {
        return _("Parameter %s split was cancelled").format(param.name);
    }

    string getName() {
        return this.stringof;
    }

    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}

Parameter ngSplitParameterBindings(size_t idx, Parameter param, uint[] takeNodeUUIDs, bool pushAction = true) {
    Parameter newParam = new Parameter(param.name~_(" (Split)"), param.isVec2);
    foreach(axis; 0..param.axisPoints.length) {
        newParam.axisPoints[axis] = param.axisPoints[axis].dup;
    }

    ParameterBinding[] oldParamBindings;
    ParameterBinding[] newParamBindings;
    foreach(binding; param.bindings) {
        if (takeNodeUUIDs.canFind(ngBindingNodeUUID(binding)))
            newParamBindings ~= binding;
        else
            oldParamBindings ~= binding;
    }

    if (newParamBindings.length == 0)
        return null;

    auto originalBindings = param.bindings.dup;
    auto action = new ParameterSplitBindingsAction(idx, param, newParam, originalBindings, oldParamBindings, newParamBindings);
    action.redo();
    if (pushAction)
        incActionPush(action);
    return newParam;
}

class ParamSplitWindow : Window {
private:
    size_t idx;
    Parameter param;
    ParamMapping[uint] mappings;

    void buildMapping() {
        foreach(i, ref binding; param.bindings) {
            auto nodeUuid = ngBindingNodeUUID(binding);
            if (nodeUuid !in mappings) {
                mappings[nodeUuid] = ParamMapping(
                    i,
                    [],
                    binding.getNode(),
                    false
                );
            }
            mappings[nodeUuid].bindings ~= binding;
        }
    }

    void apply() {
        uint[] takeNodeUUIDs;
        foreach(nodeUuid, ref mappingNode; mappings) {
            if (mappingNode.take)
                takeNodeUUIDs ~= nodeUuid;
        }
        ngSplitParameterBindings(idx, param, takeNodeUUIDs);

        this.close();
    }

    void oldBindingsList() {

        foreach(k; 0..mappings.keys.length) {
            auto key = mappings.keys[k];
            auto mapping = &mappings[mappings.keys[k]];

            if (mapping.take) continue;

            igSelectable(mapping.node.name.toStringz);
            if(igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
                igSetDragDropPayload("__OLD_TO_NEW", cast(void*)&key, (&key).sizeof, ImGuiCond.Always);
                incText(mapping.node.name);
                igEndDragDropSource();
            }
        }
    }

    void newBindingsList() {
        
        foreach(k; 0..mappings.keys.length) {
            auto key = mappings.keys[k];
            auto mapping = &mappings[mappings.keys[k]];
            if (!mapping.take) continue;
            
            igSelectable(mapping.node.name.toStringz);
            if(igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
                igSetDragDropPayload("__NEW_TO_OLD", cast(void*)&key, (&key).sizeof, ImGuiCond.Always);
                incText(mapping.node.name);
                igEndDragDropSource();
            }
        }
    }

protected:

    override
    void onBeginUpdate() {
        igSetNextWindowSizeConstraints(ImVec2(640, 480), ImVec2(float.max, float.max));
        super.onBeginUpdate();
    }

    override
    void onUpdate() {
        ImVec2 space = incAvailableSpace();
        float gapspace = 8;
        float childWidth = (space.x/2);
        float childHeight = space.y-(24);

        igBeginGroup();
            if (igBeginChild("###OldParam", ImVec2(childWidth, childHeight))) {
                if (igBeginListBox("###ItemListOld", ImVec2(childWidth-gapspace, childHeight))) {
                    oldBindingsList();
                    igEndListBox();
                }
                
                if(igBeginDragDropTarget()) {
                    const(ImGuiPayload)* payload = igAcceptDragDropPayload("__NEW_TO_OLD");
                    if (payload !is null) {
                        uint mappingName = *cast(uint*)payload.Data;
                        
                        mappings[mappingName].take = false;

                        igEndDragDropTarget();
                        igEndChild();
                        igEndGroup();
                        return;
                    }
                    igEndDragDropTarget();
                }
            }
            igEndChild();

            igSameLine(0, gapspace);

            if (igBeginChild("###NewParam", ImVec2(childWidth, childHeight))) {
                if (igBeginListBox("###ItemListNew", ImVec2(childWidth, childHeight))) {
                    newBindingsList();
                    igEndListBox();
                }
            }
            igEndChild();

            if(igBeginDragDropTarget()) {
                const(ImGuiPayload)* payload = igAcceptDragDropPayload("__OLD_TO_NEW");
                if (payload !is null) {
                    uint mappingName = *cast(uint*)payload.Data;
                    
                    mappings[mappingName].take = true;

                    igEndDragDropTarget();
                    igEndGroup();
                    return;
                }
                igEndDragDropTarget();
            }
        igEndGroup();

        igBeginGroup();
            incDummy(ImVec2(-64, 24));
            igSameLine(0, 0);
            if (incButtonColored(__("Apply"), ImVec2(64, 24))) {
                this.apply();
            }
        igEndGroup();
    }

public:
    this(size_t idx, Parameter param) {
        this.idx = idx;
        this.param = param;
        this.buildMapping();
        super(_("Split Parameter"));
    }
}
