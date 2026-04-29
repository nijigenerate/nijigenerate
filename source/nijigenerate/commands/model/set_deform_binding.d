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
import std.math : PI, cos, sin;
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

private CommandResult ngResolveSetDeformBindingParameter(Context ctx, out Parameter param) {
    param = null;

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

    return ngPrepareSetDeformBindingContext(ctx, param);
}

private ParameterBinding ngFindBindingByTarget(Parameter param, Resource target, string bindingName) {
    if (param is null || target is null) return null;

    auto direct = param.getBinding(target, bindingName);
    if (direct !is null) return direct;

    foreach (binding; param.bindings) {
        if (binding is null) continue;
        if (binding.getName() != bindingName) continue;
        if (binding.getNodeUUID() == target.uuid) {
            if (binding.getTarget().target !is target)
                binding.setTarget(target, bindingName);
            return binding;
        }
    }

    return null;
}

private bool ngGetDeformBaseVertices(Node node, out Vec2Array vertices) {
    if (auto drawable = cast(Drawable)node) {
        vertices = drawable.getMesh().vertices.dup;
        return vertices.length > 0;
    }
    if (auto deformable = cast(Deformable)node) {
        vertices = deformable.vertices.dup;
        return vertices.length > 0;
    }
    return false;
}

private vec2 ngVerticesCenter(ref Vec2Array vertices) {
    vec2 minV = vertices[0];
    vec2 maxV = vertices[0];
    foreach (v; vertices) {
        if (v.x < minV.x) minV.x = v.x;
        if (v.y < minV.y) minV.y = v.y;
        if (v.x > maxV.x) maxV.x = v.x;
        if (v.y > maxV.y) maxV.y = v.y;
    }
    return (minV + maxV) / 2;
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
        auto contextResult = ngResolveSetDeformBindingParameter(ctx, param);
        if (!contextResult.succeeded) return contextResult;

        // Resolve targets
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        if (ns.length == 0) return CommandResult(false, "No target nodes");

        bool anyChanged = false;
        ParameterBinding[] allCreated;
        {
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

/**
    Set deformation binding by applying a local transform to target vertices.

    - translation: [x,y] local offset in model units; defaults to [0,0].
    - scale: [x,y] scale around pivot; defaults to [1,1].
    - rotationDegrees: counter-clockwise rotation around pivot.
    - pivot: [x,y] local pivot; defaults to each target's vertex bounds center.
 */
@ShortcutHidden
@EffectBindingEdit
class SetTRSBindingCommand : ExCommand!(
    TW!(float[], "translation",     "Local translation [x,y] added to every vertex; default [0,0]."),
    TW!(float[], "scale",           "Local scale [x,y] around pivot; default [1,1]."),
    TW!(float,   "rotationDegrees", "Counter-clockwise rotation in degrees around pivot; default 0."),
    TW!(float[], "pivot",           "Optional local pivot [x,y]. If omitted, each target uses its vertex bounds center.")
) {
    this(float[] translation = null, float[] scale = null, float rotationDegrees = 0, float[] pivot = null) {
        super(
            _("Set TRS Binding"),
            _("Set deform binding offsets at current keypoint from translation, scale, and rotation."),
            translation,
            scale,
            rotationDegrees,
            pivot
        );
    }

    override bool runnable(Context ctx) {
        bool hasParam = (ctx.hasArmedParameters && ctx.armedParameters.length > 0) || (ctx.hasParameters && ctx.parameters.length > 0) || (incArmedParameter() !is null);
        if (!hasParam) return false;
        Node[] nodes = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        foreach (node; nodes) {
            Vec2Array vertices;
            if (ngGetDeformBaseVertices(node, vertices)) return true;
        }
        return false;
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "No applicable parameters or nodes");

        vec2 translationValue;
        vec2 scaleValue;
        vec2 pivotValue;
        string error;
        if (!ngResolveVec2Arg(translation, vec2(0, 0), translationValue, "translation", error)) return CommandResult(false, error);
        if (!ngResolveVec2Arg(scale, vec2(1, 1), scaleValue, "scale", error)) return CommandResult(false, error);
        bool hasExplicitPivot = !(pivot is null) && pivot.length > 0;
        if (!ngResolveVec2Arg(pivot, vec2(0, 0), pivotValue, "pivot", error)) return CommandResult(false, error);

        Parameter param;
        auto contextResult = ngResolveSetDeformBindingParameter(ctx, param);
        if (!contextResult.succeeded) return contextResult;

        Node[] nodes = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        if (nodes.length == 0) return CommandResult(false, "No target nodes");

        float radians = rotationDegrees * PI / 180.0f;
        float c = cos(radians);
        float s = sin(radians);

        bool anyChanged = false;
        ParameterBinding[] allCreated;

        {
            vec2u kp = ctx.hasKeyPoint ? ctx.keyPoint : param.findClosestKeypoint();
            DeformationParameterBinding[] deformBindings;
            Vec2Array[] offsetsByBinding;
            ParameterBinding[] newlyAdded;

            foreach (node; nodes) {
                Vec2Array baseVertices;
                if (!ngGetDeformBaseVertices(node, baseVertices)) continue;

                auto binding = ngFindBindingByTarget(param, node, "deform");
                bool wasCreated = false;
                vec2 center = hasExplicitPivot ? pivotValue : ngVerticesCenter(baseVertices);
                Vec2Array offsets;
                offsets.length = baseVertices.length;
                foreach (i, v; baseVertices) {
                    vec2 p = v - center;
                    p = vec2(p.x * scaleValue.x, p.y * scaleValue.y);
                    vec2 rotated = vec2(p.x * c - p.y * s, p.x * s + p.y * c);
                    vec2 transformed = center + rotated + translationValue;
                    offsets[i] = transformed - v;
                }

                if (binding is null) {
                    if (ngAllOffsetsZero(offsets)) continue;
                    binding = param.getOrAddBinding(node, "deform");
                    wasCreated = binding !is null;
                }

                auto deformBinding = cast(DeformationParameterBinding)binding;
                if (deformBinding is null) continue;
                if (wasCreated) newlyAdded ~= deformBinding;

                deformBindings ~= deformBinding;
                offsetsByBinding ~= offsets;
            }

            if (deformBindings.length == 0)
                return CommandResult(false, "No bindings updated");

            auto group = new GroupAction();
            foreach (binding; newlyAdded) {
                group.addAction(new ParameterBindingAddAction(param, binding));
                allCreated ~= binding;
            }

            auto action = new ParameterChangeBindingsValueAction(_("Set TRS Binding"), param, cast(ParameterBinding[])deformBindings, cast(int)kp.x, cast(int)kp.y);
            foreach (i, binding; deformBindings) {
                binding.update(kp, offsetsByBinding[i]);
                if (auto target = cast(Deformable)binding.getTarget().target)
                    target.updateDeform();
            }
            action.updateNewState();
            group.addAction(action);

            if (!group.empty()) {
                incActionPush(group);
                paramPointChanged(param);
                anyChanged = true;
            }
        }

        incViewportNodeDeformNotifyParamValueChanged();
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
