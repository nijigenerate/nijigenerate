module nijigenerate.commands.model.set_deform_binding;

import nijigenerate.commands.base;
import nijigenerate.commands.binding.base : paramPointChanged;
import nijigenerate.actions.parameter; // ParameterChangeBindingsValueAction
import nijigenerate.actions;           // GroupAction, ParameterBindingAddAction
import nijigenerate.core.actionstack : incActionPush;
import nijigenerate.ext.param;
import nijigenerate.project : EditMode, incActivePuppet, incSelectedNodes, incArmedParameter, incArmParameter, incEditMode, incSetEditMode;
import nijigenerate.viewport.model.deform : incViewportNodeDeformNotifyParamValueChanged;
import nijilive; // Parameter, Node, Drawable, Deformable
import nijilive.math : vec2, vec2u;
import std.algorithm : map, filter;
import std.algorithm.searching : countUntil;
import std.array : array;
import std.conv : to;
import std.exception : enforce;
import i18n;

private bool ngFindSetDeformBindingParameterIndex(Parameter param, out size_t index) {
    if (param is null) return false;

    if (auto exParam = cast(ExParameter)param) {
        if (auto parent = exParam.getParent()) {
            auto found = parent.children.countUntil(param);
            if (found >= 0) {
                index = cast(size_t)found;
                return true;
            }
        }
    }

    auto puppet = incActivePuppet();
    if (puppet !is null) {
        auto found = puppet.parameters.countUntil(param);
        if (found >= 0) {
            index = cast(size_t)found;
            return true;
        }
    }
    return false;
}

private CommandResult ngPrepareSetDeformBindingContext(Context ctx, Parameter param) {
    if (param is null)
        return CommandResult(false, "No parameter resolved");

    size_t index;
    if (!ngFindSetDeformBindingParameterIndex(param, index))
        return CommandResult(false, "Parameter not found in active puppet");

    if (incEditMode() != EditMode.ModelEdit)
        incSetEditMode(EditMode.ModelEdit, false);

    if (ctx.hasKeyPoint && ctx.hasExplicitKeyPoint) {
        if (ctx.keyPoint.x >= param.axisPointCount(0) || ctx.keyPoint.y >= param.axisPointCount(1))
            return CommandResult(false, "keyPoint is out of range for parameter");

        param.value = param.getKeypointValue(ctx.keyPoint);
    } else {
        param.value = param.getClosestKeypointValue();
    }

    paramPointChanged(param);
    incArmParameter(index, param);
    incViewportNodeDeformNotifyParamValueChanged();

    return CommandResult(true);
}

/**
    Set Deformation binding at current keypoint using provided offsets.

    - values: flattened [dx0,dy0, dx1,dy1, ...]
    Applies to the first armed Parameter (or ctx.parameters[0] if provided),
    and to selected nodes that already have a deform binding.
 */
@ShortcutHidden
@EffectBindingEdit
class SetDeformBindingCommand : ExCommand!(
    TW!(string,  "bindingName", "Binding name. Must be 'deform'."),
    TW!(float[], "values",      "Flattened deformation offsets: [dx0,dy0, dx1,dy1, ...]")
) {
    this(string bname, float[] vals) {
        super(_("Set Deform Binding"), _("Set deform binding offsets at current keypoint."), bname, vals);
    }

    override bool runnable(Context ctx) {
        // Need a parameter context and at least one deformable node
        bool hasParam = (ctx.hasArmedParameters && ctx.armedParameters.length > 0) || (ctx.hasParameters && ctx.parameters.length > 0) || (incArmedParameter() !is null);
        if (!hasParam) return false;
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        foreach (n; ns) if (cast(Drawable)n || cast(Deformable)n) return true;
        return false;
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "No applicable parameters or nodes");

        // No-op if values not provided
        if (values is null || values.length == 0) return CommandResult(false, "No values provided");
        if (bindingName != "deform") return CommandResult(false, "SetDeformBinding only supports bindingName='deform'");
        if (values.length < 2 || values.length % 2 != 0) return CommandResult(false, "Deform values must be flattened [dx,dy]* offsets");

        // Resolve parameter to operate on
        Parameter[] params;
        if (ctx.hasArmedParameters && ctx.armedParameters.length > 0) params = ctx.armedParameters;
        else if (ctx.hasParameters && ctx.parameters.length > 0) params = ctx.parameters;
        else if (auto p = incArmedParameter()) params = [p];
        if (params.length == 0) return CommandResult(false, "No parameters resolved");

        auto contextResult = ngPrepareSetDeformBindingContext(ctx, params[0]);
        if (!contextResult.succeeded) return contextResult;

        // Resolve targets
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        if (ns.length == 0) return CommandResult(false, "No target nodes");

        bool anyChanged = false;
        ParameterBinding[] allCreated;
        // For each parameter, set binding at target keypoint
        foreach (param; params) {
            // Determine keypoint
            vec2u kp = ctx.hasKeyPoint ? ctx.keyPoint : param.findClosestKeypoint();

            // Prepare sets and track new bindings to add undo/redo entries
            DeformationParameterBinding[] deformBindings;
            ParameterBinding[]            newlyAdded;

            // Pre-parse deform offsets if applicable
            Vec2Array offsets;
            offsets.length = values.length / 2;
            foreach (i; 0 .. offsets.length) offsets[i] = vec2(values[i*2], values[i*2 + 1]);

            // Collect or create bindings for each selected node. Skip incompatible
            // targets instead of dropping all valid updates for the parameter.
            foreach (n; ns) {
                // Try resolve existing binding first
                auto existing = param.getBinding(n, bindingName);
                bool wasNull = (existing is null);
                ParameterBinding b = existing;

                // Branch by concrete binding type, not by name
                if (auto db = cast(DeformationParameterBinding)b) {
                    // Only valid on deformable/drawable nodes with matching vertex counts.
                    size_t expected = 0;
                    if (auto d = cast(Drawable)n) expected = d.getMesh().vertices.length;
                    else if (auto df = cast(Deformable)n) expected = df.vertices.length;
                    else continue;
                    if (expected != offsets.length) continue;
                    deformBindings ~= db;
                    // existing deform binding case; wasNull is false here
                } else {
                    // Create only the canonical deform binding for deformable/drawable targets.
                    size_t expected = 0;
                    if (auto d = cast(Drawable)n) expected = d.getMesh().vertices.length;
                    else if (auto df = cast(Deformable)n) expected = df.vertices.length;
                    else continue;
                    if (expected != offsets.length) continue;

                    b = param.getOrAddBinding(n, "deform");
                    if (auto ndb = cast(DeformationParameterBinding)b) {
                        deformBindings ~= ndb;
                        newlyAdded ~= ndb;
                    } else {
                        continue;
                    }
                }
            }

            if (deformBindings.length == 0) continue;

            // Build grouped action with optional add-binding entries
            auto group = new GroupAction();
            foreach (nb; newlyAdded) {
                group.addAction(new ParameterBindingAddAction(param, nb));
                allCreated ~= nb;
            }
            if (deformBindings.length > 0) {
                auto action = new ParameterChangeBindingsValueAction(_("Set Deform Binding"), param, cast(ParameterBinding[])deformBindings, cast(int)kp.x, cast(int)kp.y);
                foreach (b; deformBindings) b.update(kp, offsets);
                action.updateNewState();
                group.addAction(action);
            }
            if (!group.empty()) {
                incActionPush(group);
                paramPointChanged(param);
                anyChanged = true;
            }
        }

        // Refresh deformation viewport/editor state
        incViewportNodeDeformNotifyParamValueChanged();
        if (allCreated.length > 0) {
            return new CreateResult!ParameterBinding(anyChanged, allCreated, anyChanged ? "" : "No bindings updated");
        }
        return CommandResult(anyChanged, anyChanged ? "" : "No bindings updated");
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
