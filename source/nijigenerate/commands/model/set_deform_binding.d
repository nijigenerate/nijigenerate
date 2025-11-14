module nijigenerate.commands.model.set_deform_binding;

import nijigenerate.commands.base;
import nijigenerate.actions.parameter; // ParameterChangeBindingsValueAction
import nijigenerate.actions;           // GroupAction, ParameterBindingAddAction
import nijigenerate.core.actionstack : incActionPush;
import nijigenerate.project : incSelectedNodes, incArmedParameter;
import nijigenerate.viewport.model.deform : incViewportNodeDeformNotifyParamValueChanged;
import nijilive; // Parameter, Node, Drawable, Deformable
import nijilive.math : vec2, vec2u;
import std.algorithm : map, filter;
import std.array : array;
import std.conv : to;
import std.exception : enforce;
import i18n;

/**
    Set Deformation binding at current keypoint using provided offsets.

    - values: flattened [dx0,dy0, dx1,dy1, ...]
    Applies to the first armed Parameter (or ctx.parameters[0] if provided),
    and to selected nodes that already have a deform binding.
*/
class SetDeformBindingCommand : ExCommand!(
    TW!(string,  "bindingName", "Binding name (e.g., 'deform' or value name)"),
    TW!(float[], "values",      "Flattened values: deform=[dx,dy]*, other=[v]")
) {
    this(string bname, float[] vals) {
        super(_("Set Binding"), _("Set binding value(s) at current keypoint."), bname, vals);
    }

    override bool runnable(Context ctx) {
        // Need a parameter context and at least one deformable node
        bool hasParam = (ctx.hasArmedParameters && ctx.armedParameters.length > 0) || (ctx.hasParameters && ctx.parameters.length > 0) || (incArmedParameter() !is null);
        if (!hasParam) return false;
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        foreach (n; ns) if (cast(Drawable)n || cast(Deformable)n) return true;
        return false;
    }

    // Not available via shortcut (requires external data)
    override bool shortcutRunnable() { return false; }

    override void run(Context ctx) {
        if (!runnable(ctx)) return;

        // No-op if values not provided
        if (values is null || values.length == 0) return;

        // Resolve parameter to operate on
        Parameter[] params;
        if (ctx.hasArmedParameters && ctx.armedParameters.length > 0) params = ctx.armedParameters;
        else if (ctx.hasParameters && ctx.parameters.length > 0) params = ctx.parameters;
        else if (auto p = incArmedParameter()) params = [p];
        if (params.length == 0) return;

        // Resolve targets
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        if (ns.length == 0) return;

        // For each parameter, set binding at target keypoint
        foreach (param; params) {
            // Determine keypoint
            vec2u kp = ctx.hasKeyPoint ? ctx.keyPoint : param.findClosestKeypoint();

            // Prepare sets and track new bindings to add undo/redo entries
            DeformationParameterBinding[] deformBindings;
            ValueParameterBinding[]       valueBindings;
            ParameterBinding[]            newlyAdded;

            // Pre-parse deform offsets if applicable
            Vec2Array offsets;
            bool hasOffsets = (values.length >= 2) && (values.length % 2 == 0);
            if (hasOffsets) {
                offsets.length = values.length / 2;
                foreach (i; 0 .. offsets.length) offsets[i] = vec2(values[i*2], values[i*2 + 1]);
            }

            // Collect or create bindings for each selected node, then dispatch by concrete type
            bool deformMismatch = false;
            foreach (n; ns) {
                // Try resolve existing binding first
                auto existing = param.getBinding(n, bindingName);
                bool wasNull = (existing is null);
                ParameterBinding b = existing;

                // Branch by concrete binding type, not by name
                if (auto db = cast(DeformationParameterBinding)b) {
                    // Only valid on deformable/drawable nodes and with even-length offsets
                    if (!hasOffsets) { deformMismatch = true; continue; }
                    size_t expected = 0;
                    if (auto d = cast(Drawable)n) expected = d.getMesh().vertices.length;
                    else if (auto df = cast(Deformable)n) expected = df.vertices.length;
                    else { deformMismatch = true; continue; }
                    if (expected != offsets.length) { deformMismatch = true; continue; }
                    deformBindings ~= db;
                    // existing deform binding case; wasNull is false here
                } else if (auto vb = cast(ValueParameterBinding)b) {
                    // Expect single scalar value
                    if (values.length != 1) continue;
                    valueBindings ~= vb;
                    // existing value binding case
                } else {
                    // No existing binding; decide if we should create one
                    if (hasOffsets) {
                        // Targeting a deform-style update; only create for deformable/drawable with matching counts
                        size_t expected = 0;
                        if (auto d = cast(Drawable)n) expected = d.getMesh().vertices.length;
                        else if (auto df = cast(Deformable)n) expected = df.vertices.length;
                        else { deformMismatch = true; continue; }
                        if (expected != offsets.length) { deformMismatch = true; continue; }

                        // Create binding and verify type
                        b = param.getOrAddBinding(n, bindingName);
                        if (auto ndb = cast(DeformationParameterBinding)b) {
                            deformBindings ~= ndb;
                            newlyAdded ~= ndb;
                        } else {
                            // created wrong type; treat as mismatch
                            deformMismatch = true;
                            continue;
                        }
                    } else if (values.length == 1) {
                        // Value-style; only create if node supports the parameter
                        auto node = cast(Node)n;
                        if (!node || !node.hasParam(bindingName)) continue;
                        b = param.getOrAddBinding(n, bindingName);
                        if (auto nvb = cast(ValueParameterBinding)b) {
                            valueBindings ~= nvb;
                            newlyAdded ~= nvb;
                        }
                    } else {
                        // Neither deform nor valid value input
                        continue;
                    }
                }
            }

            // If any deform target mismatches, do nothing for this parameter
            if (deformMismatch) continue;

            // Build grouped action with optional add-binding entries
            auto group = new GroupAction();
            foreach (nb; newlyAdded) {
                group.addAction(new ParameterBindingAddAction(param, nb));
            }
            if (deformBindings.length > 0) {
                auto action = new ParameterChangeBindingsValueAction(_("Set Deform Binding"), param, cast(ParameterBinding[])deformBindings, cast(int)kp.x, cast(int)kp.y);
                foreach (b; deformBindings) b.update(kp, offsets);
                action.updateNewState();
                group.addAction(action);
            }
            if (valueBindings.length > 0) {
                auto action = new ParameterChangeBindingsValueAction(_("Set Binding"), param, cast(ParameterBinding[])valueBindings, cast(int)kp.x, cast(int)kp.y);
                float v = values[0];
                foreach (b; valueBindings) b.setValue(kp, v);
                action.updateNewState();
                group.addAction(action);
            }
            if (!group.empty()) incActionPush(group);
        }

        // Refresh deformation viewport/editor state
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

enum ModelCommand {
    SetDeformBinding,
}

Command[ModelCommand] commands;

void ngInitCommands(T)() if (is(T == ModelCommand))
{
    // Register with benign defaults; actual args supplied at call-time
    mixin(registerCommand!(ModelCommand.SetDeformBinding, "deform", null));
}
