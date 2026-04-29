module nijigenerate.commands.model.set_deform_binding;

import nijigenerate.commands.base;
import nijigenerate.commands.binding.base : paramPointChanged;
import nijigenerate.actions.parameter; // ParameterChangeBindingsValueAction
import nijigenerate.actions;           // GroupAction, ParameterBindingAddAction
import nijigenerate.core.actionstack : incActionPush;
import nijigenerate.ext.param;
import nijigenerate.project : EditMode, incActivePuppet, incSelectedNodes, incArmedParameter, incArmedParameterIdx, incArmParameter, incEditMode, incSetEditMode;
import nijigenerate.viewport.model.deform : incViewportNodeDeformNotifyParamValueChanged;
import nijilive; // Parameter, Node, Drawable, Deformable
import nijilive.math : vec2, vec2u;
import std.algorithm : map, filter;
import std.algorithm.searching : countUntil;
import std.array : array;
import std.conv : to;
import std.exception : enforce;
import std.math : PI;
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

private bool ngResolveParameterValueToKeyPoint(Parameter param, vec2 value, out vec2u keyPoint, out string message) {
    if (param is null) {
        message = "context.parameterValue requires a parameter";
        return false;
    }

    uint x = param.getClosestAxisPointIndex(0, param.mapAxis(0, value.x));
    uint y = param.getClosestAxisPointIndex(1, param.mapAxis(1, value.y));
    auto resolved = param.getKeypointValue(vec2u(x, y));

    import std.math : abs;
    enum float epsilon = 1e-5f;
    if (abs(resolved.x - value.x) > epsilon || abs(resolved.y - value.y) > epsilon) {
        message = "context.parameterValue does not match an existing key value";
        return false;
    }

    keyPoint = vec2u(x, y);
    return true;
}

private CommandResult ngPrepareSetDeformBindingContext(Context ctx, Parameter param, out vec2u keyPoint, out size_t paramIndex) {
    if (param is null)
        return CommandResult(false, "No parameter resolved");

    if (!ngFindSetDeformBindingParameterIndex(param, paramIndex))
        return CommandResult(false, "Parameter not found in active puppet");

    if (incEditMode() != EditMode.ModelEdit)
        incSetEditMode(EditMode.ModelEdit, false);

    if (ctx.hasParameterValue) {
        string message;
        if (!ngResolveParameterValueToKeyPoint(param, ctx.parameterValue, keyPoint, message))
            return CommandResult(false, message);

        param.value = param.getKeypointValue(keyPoint);
    } else if (ctx.hasKeyPoint && ctx.hasExplicitKeyPoint) {
        if (ctx.keyPoint.x >= param.axisPointCount(0) || ctx.keyPoint.y >= param.axisPointCount(1))
            return CommandResult(false, "keyPoint is out of range for parameter");
        keyPoint = ctx.keyPoint;
        param.value = param.getKeypointValue(keyPoint);
    } else {
        keyPoint = param.findClosestKeypoint();
        param.value = param.getKeypointValue(keyPoint);
    }

    return CommandResult(true);
}

private void ngFinalizeSetDeformBindingContext(Parameter param, size_t paramIndex) {
    if (param is null) return;
    paramPointChanged(param);
    if (incArmedParameter() !is param || incArmedParameterIdx() != paramIndex)
        incArmParameter(paramIndex, param);
    incViewportNodeDeformNotifyParamValueChanged();
}

private CommandResult ngResolveSetDeformBindingParameter(Context ctx, out Parameter param, out vec2u keyPoint, out size_t paramIndex) {
    param = null;
    paramIndex = 0;

    if (ctx.hasArmedParameters && ctx.armedParameters.length > 0) {
        if (ctx.armedParameters.length != 1)
            return CommandResult(false, "SetDeformBinding requires exactly one armed parameter");
        param = ctx.armedParameters[0];
    } else if (ctx.hasParameters && ctx.parameters.length > 0) {
        if (ctx.parameters.length != 1)
            return CommandResult(false, "SetDeformBinding requires exactly one parameter");
        param = ctx.parameters[0];
    } else {
        param = incArmedParameter();
    }

    if (param is null)
        return CommandResult(false, "No parameters resolved");

    return ngPrepareSetDeformBindingContext(ctx, param, keyPoint, paramIndex);
}

private ParameterBinding ngFindBindingByTarget(Parameter param, Resource target, string bindingName) {
    if (param is null || target is null) return null;

    auto direct = param.getBinding(target, bindingName);
    if (direct !is null) return direct;

    foreach (binding; param.bindings) {
        if (binding is null) continue;
        if (binding.getName() != bindingName) continue;
        auto bindingTarget = binding.getTarget().target;
        if (bindingTarget is null && binding.getNodeUUID() == target.uuid) {
            // Repair deserialized bindings only when the object reference is missing.
            // Do not retarget a live binding: duplicated UUIDs or cloned nodes would
            // otherwise make different targets share the same binding instance.
            binding.setTarget(target, bindingName);
            return binding;
        }
    }

    return null;
}

private bool ngResolveVec2Arg(float[] values, vec2 fallback, out vec2 result, string name, out string error) {
    if (values is null || values.length == 0) {
        result = fallback;
        return true;
    }
    if (values.length != 2) {
        error = name ~ " must be [x,y]";
        return false;
    }
    result = vec2(values[0], values[1]);
    return true;
}

private bool ngAllOffsetsZero(ref Vec2Array offsets) {
    foreach (offset; offsets) {
        if (offset.x != 0 || offset.y != 0)
            return false;
    }
    return true;
}

private ValueParameterBinding ngGetOrCreateValueBinding(Parameter param, Node node, string bindingName, GroupAction group, ref ParameterBinding[] allCreated) {
    if (param is null || node is null || !node.hasParam(bindingName)) return null;

    if (auto existing = cast(ValueParameterBinding)ngFindBindingByTarget(param, node, bindingName))
        return existing;

    auto created = cast(ValueParameterBinding)param.createBinding(node, bindingName);
    if (created is null) return null;

    param.addBinding(created);
    group.addAction(new ParameterBindingAddAction(param, created));
    allCreated ~= created;
    return created;
}

private bool ngSetTRSValueBinding(Parameter param, vec2u kp, Node[] nodes, string bindingName, float value, GroupAction group, ref ParameterBinding[] allCreated) {
    ValueParameterBinding[] bindings;
    foreach (node; nodes) {
        if (auto binding = ngGetOrCreateValueBinding(param, node, bindingName, group, allCreated))
            bindings ~= binding;
    }

    if (bindings.length == 0) return false;

    auto action = new ParameterBindingValueChangeAction!(float, ValueParameterBinding[])(bindingName, bindings, cast(int)kp.x, cast(int)kp.y);
    foreach (binding; bindings)
        binding.setValue(kp, value);
    action.updateNewState();
    group.addAction(action);
    return true;
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

        Parameter param;
        vec2u kp;
        size_t paramIndex;
        auto contextResult = ngResolveSetDeformBindingParameter(ctx, param, kp, paramIndex);
        if (!contextResult.succeeded) return contextResult;

        // Resolve targets
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        if (ns.length == 0) return CommandResult(false, "No target nodes");

        bool anyChanged = false;
        ParameterBinding[] allCreated;
        {
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
                auto existing = ngFindBindingByTarget(param, n, bindingName);
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
                    if (ngAllOffsetsZero(offsets)) continue;

                    b = param.getOrAddBinding(n, "deform");
                    if (auto ndb = cast(DeformationParameterBinding)b) {
                        deformBindings ~= ndb;
                        newlyAdded ~= ndb;
                    } else {
                        continue;
                    }
                }
            }

            if (deformBindings.length == 0)
                return CommandResult(false, "No bindings updated");

            // Build grouped action with optional add-binding entries
            auto group = new GroupAction();
            foreach (nb; newlyAdded) {
                group.addAction(new ParameterBindingAddAction(param, nb));
                allCreated ~= nb;
            }
            if (deformBindings.length > 0) {
                auto action = new ParameterChangeBindingsValueAction(_("Set Deform Binding"), param, cast(ParameterBinding[])deformBindings, cast(int)kp.x, cast(int)kp.y);
                foreach (b; deformBindings) {
                    b.update(kp, offsets);
                    if (auto target = cast(Deformable)b.getTarget().target)
                        target.updateDeform();
                }
                action.updateNewState();
                group.addAction(action);
            }
            if (!group.empty()) {
                incActionPush(group);
                anyChanged = true;
            }
        }

        // Refresh deformation viewport/editor state
        ngFinalizeSetDeformBindingContext(param, paramIndex);
        if (allCreated.length > 0) {
            return new CreateResult!ParameterBinding(anyChanged, allCreated, anyChanged ? "" : "No bindings updated");
        }
        return CommandResult(anyChanged, anyChanged ? "" : "No bindings updated");
    }
}

/**
    Set node transform value bindings at current keypoint.

    - translation: [x,y] writes transform.t.x and transform.t.y.
    - scale: [x,y] writes transform.s.x and transform.s.y.
    - rotationDegrees: writes transform.r.z in radians after degree conversion.
 */
@ShortcutHidden
@EffectBindingEdit
class SetTRSBindingCommand : ExCommand!(
    TW!(float[], "translation",     "Node transform translation binding [x,y]. Omit to leave unchanged."),
    TW!(float[], "scale",           "Node transform scale binding [x,y]. Omit to leave unchanged."),
    TW!(float,   "rotationDegrees", "Node transform Z rotation binding in degrees. Set applyRotation=true to write zero."),
    TW!(bool,    "applyRotation",   "Whether to write rotationDegrees even when it is zero.")
) {
    this(float[] translation = null, float[] scale = null, float rotationDegrees = 0, bool applyRotation = false) {
        super(
            _("Set TRS Binding"),
            _("Set node transform translation, rotation, and scale bindings at current keypoint."),
            translation,
            scale,
            rotationDegrees,
            applyRotation
        );
    }

    override bool runnable(Context ctx) {
        bool hasParam = (ctx.hasArmedParameters && ctx.armedParameters.length > 0) || (ctx.hasParameters && ctx.parameters.length > 0) || (incArmedParameter() !is null);
        if (!hasParam) return false;
        Node[] nodes = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        foreach (node; nodes) if (node !is null) return true;
        return false;
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "No applicable parameters or nodes");

        vec2 translationValue;
        vec2 scaleValue;
        string error;
        bool hasTranslation = !(translation is null) && translation.length > 0;
        bool hasScale = !(scale is null) && scale.length > 0;
        bool hasRotation = applyRotation || rotationDegrees != 0;

        if (!hasTranslation && !hasScale && !hasRotation)
            return CommandResult(false, "No TRS values provided");
        if (hasTranslation && !ngResolveVec2Arg(translation, vec2(0, 0), translationValue, "translation", error)) return CommandResult(false, error);
        if (hasScale && !ngResolveVec2Arg(scale, vec2(1, 1), scaleValue, "scale", error)) return CommandResult(false, error);

        Parameter param;
        vec2u kp;
        size_t paramIndex;
        auto contextResult = ngResolveSetDeformBindingParameter(ctx, param, kp, paramIndex);
        if (!contextResult.succeeded) return contextResult;

        Node[] nodes = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        if (nodes.length == 0) return CommandResult(false, "No target nodes");

        bool anyChanged = false;
        ParameterBinding[] allCreated;
        auto group = new GroupAction();

        if (hasTranslation) {
            anyChanged = ngSetTRSValueBinding(param, kp, nodes, "transform.t.x", translationValue.x, group, allCreated) || anyChanged;
            anyChanged = ngSetTRSValueBinding(param, kp, nodes, "transform.t.y", translationValue.y, group, allCreated) || anyChanged;
        }
        if (hasScale) {
            anyChanged = ngSetTRSValueBinding(param, kp, nodes, "transform.s.x", scaleValue.x, group, allCreated) || anyChanged;
            anyChanged = ngSetTRSValueBinding(param, kp, nodes, "transform.s.y", scaleValue.y, group, allCreated) || anyChanged;
        }
        if (hasRotation) {
            auto radians = rotationDegrees * PI / 180.0f;
            anyChanged = ngSetTRSValueBinding(param, kp, nodes, "transform.r.z", radians, group, allCreated) || anyChanged;
        }

        if (!anyChanged)
            return CommandResult(false, "No bindings updated");

        if (!group.empty()) {
            incActionPush(group);
        }

        ngFinalizeSetDeformBindingContext(param, paramIndex);
        if (allCreated.length > 0) {
            return new CreateResult!ParameterBinding(anyChanged, allCreated, anyChanged ? "" : "No bindings updated");
        }
        return CommandResult(anyChanged, anyChanged ? "" : "No bindings updated");
    }
}

enum ModelCommand {
    SetDeformBinding,
    SetTRSBinding,
}

Command[ModelCommand] commands;

void ngInitCommands(T)() if (is(T == ModelCommand))
{
    // Register with benign defaults; actual args supplied at call-time
    mixin(registerCommand!(ModelCommand.SetDeformBinding, "deform", null));
    mixin(registerCommand!(ModelCommand.SetTRSBinding));
}
