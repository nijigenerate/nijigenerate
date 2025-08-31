module nijigenerate.commands.parameter.prop;

import nijigenerate.commands.base;
import nijigenerate.ext;
import nijigenerate.actions;
import nijigenerate.core;
import nijigenerate.viewport.model.deform : incViewportNodeDeformNotifyParamValueChanged;
import i18n;
import nijilive; // vec2, Parameter
import nijilive.core.param.binding : ValueParameterBinding, ParameterParameterBinding, DeformationParameterBinding;

// Name only
class SetParameterNameCommand : ExCommand!(TW!(string, "newName", "New parameter name")) {
    this(string newName) { super(_("Set Parameter Name"), newName); }
    override bool shortcutRunnable() { return false; }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters) return;
        auto param = ctx.parameters[0];

        // Validate name uniqueness within active puppet
        auto ex = cast(ExPuppet) (ctx.hasPuppet ? ctx.puppet : null);
        if (ex !is null) {
            auto fparam = ex.findParameter(newName);
            if (fparam !is null && fparam.uuid != param.uuid) {
                // Name already taken; do nothing
                return;
            }
        }

        // Push name change as action
        // No action push for name (pointer to property not supported); follow existing behavior
        param.name = newName;
        param.makeIndexable();
    }
}

// Apply min/max and axis breakpoints (normalized values) together
class ApplyParameterPropsAxesCommand : ExCommand!(
    TW!(float[2], "min",   "New min (x,y) as [2]"),
    TW!(float[2], "max",   "New max (x,y) as [2]"),
    TW!(float[],  "axisX", "Normalized X breakpoints (including endpoints)"),
    TW!(float[],  "axisY", "Normalized Y breakpoints (including endpoints; empty for 1D)")
) {
    this(float[2] min, float[2] max, float[] axisX, float[] axisY) {
        super(_("Apply Parameter Axes + Props"), min, max, axisX, axisY);
    }
    override bool shortcutRunnable() { return false; }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters) return;
        auto param = ctx.parameters[0];

        // Convert to vec2 for internal use
        vec2 vmin = vec2(min[0], min[1]);
        vec2 vmax = vec2(max[0], max[1]);

        // Snapshot old axis points and binding data before structural changes
        float[] oldXs = param.axisPoints[0].dup;
        float[] oldYs = param.axisPoints[1].dup;
        struct SnapF { ValueParameterBinding b; float[][] vals; bool[][] set; }
        struct SnapP { ParameterParameterBinding b; float[][] vals; bool[][] set; }
        struct SnapD { DeformationParameterBinding b; Deformation[][] vals; bool[][] set; }
        SnapF[] snapsF; SnapP[] snapsP; SnapD[] snapsD;
        foreach (binding; param.bindings) {
            if (auto vb = cast(ValueParameterBinding)binding) {
                float[][] vals; vals.length = oldXs.length; foreach (x; 0..oldXs.length) { vals[x].length = oldYs.length; foreach (y; 0..oldYs.length) vals[x][y] = vb.getValue(vec2u(cast(uint)x, cast(uint)y)); }
                bool[][] set; set.length = oldXs.length; foreach (x; 0..oldXs.length) set[x] = vb.getIsSet()[x].dup;
                snapsF ~= SnapF(vb, vals, set);
            } else if (auto pb = cast(ParameterParameterBinding)binding) {
                float[][] vals; vals.length = oldXs.length; foreach (x; 0..oldXs.length) { vals[x].length = oldYs.length; foreach (y; 0..oldYs.length) vals[x][y] = pb.getValue(vec2u(cast(uint)x, cast(uint)y)); }
                bool[][] set; set.length = oldXs.length; foreach (x; 0..oldXs.length) set[x] = pb.getIsSet()[x].dup;
                snapsP ~= SnapP(pb, vals, set);
            } else if (auto db = cast(DeformationParameterBinding)binding) {
                Deformation[][] vals; vals.length = oldXs.length; foreach (x; 0..oldXs.length) { vals[x].length = oldYs.length; foreach (y; 0..oldYs.length) vals[x][y] = db.getValue(vec2u(cast(uint)x, cast(uint)y)); }
                bool[][] set; set.length = oldXs.length; foreach (x; 0..oldXs.length) set[x] = db.getIsSet()[x].dup;
                snapsD ~= SnapD(db, vals, set);
            }
        }

        // Apply min/max via actions per component for proper undo/redo
        auto prevMin = param.min; auto prevMax = param.max;
        param.min = vmin; param.max = vmax;
        if (prevMin.x != param.min.x)
            incActionPush(new ParameterValueChangeAction!float("min X", param, prevMin.x, param.min.x, &param.min.vector[0]));
        if (param.isVec2 && prevMin.y != param.min.y)
            incActionPush(new ParameterValueChangeAction!float("min Y", param, prevMin.y, param.min.y, &param.min.vector[1]));
        if (prevMax.x != param.max.x)
            incActionPush(new ParameterValueChangeAction!float("max X", param, prevMax.x, param.max.x, &param.max.vector[0]));
        if (param.isVec2 && prevMax.y != param.max.y)
            incActionPush(new ParameterValueChangeAction!float("max Y", param, prevMax.y, param.max.y, &param.max.vector[1]));

        // Helper to rewrite axis points to provided normalized list
        void applyAxis(uint axis, float[] targetVals) {
            import std.algorithm : sort;
            import std.math : abs;
            // Ensure ascending and presence of endpoints
            targetVals.sort();
            if (targetVals.length < 2) return;

            // Current points
            float[] curVals;
            foreach (v; param.axisPoints[axis]) curVals ~= v;
            size_t curN = curVals.length;
            size_t tarN = targetVals.length;

            // Match endpoints implicitly
            // Build matching between mid points by nearest neighbor
            bool[] curMatched; curMatched.length = curN;
            bool[] tarMatched; tarMatched.length = tarN;
            if (curN >= 1 && tarN >= 1) { curMatched[0] = true; tarMatched[0] = true; }
            if (curN >= 2 && tarN >= 2) { curMatched[curN-1] = true; tarMatched[tarN-1] = true; }

            // For each target mid-point, pick nearest unmatched current mid-point
            for (size_t tj = 1; tj + 1 < tarN; ++tj) {
                float t = targetVals[tj];
                float bestDist = float.infinity;
                size_t bestIdx = size_t.max;
                for (size_t ci = 1; ci + 1 < curN; ++ci) {
                    if (curMatched[ci]) continue;
                    float d = abs(curVals[ci] - t);
                    if (d < bestDist) { bestDist = d; bestIdx = ci; }
                }
                if (bestIdx != size_t.max) {
                    curMatched[bestIdx] = true;
                    tarMatched[tj] = true;
                }
            }

            // Delete unmatched current mid-points (ascending index)
            if (curN > 2) {
                size_t i = 1;
                while (i + 1 < curN) {
                    if (!curMatched[i]) {
                        param.deleteAxisPoint(axis, cast(uint)i);
                        // Update arrays after deletion
                        curVals = curVals[0 .. i] ~ curVals[i+1 .. $];
                        curMatched = curMatched[0 .. i] ~ curMatched[i+1 .. $];
                        curN--;
                        // Do not increment i, since next element shifted into i
                        continue;
                    }
                    i++;
                }
            }

            // Insert unmatched target mid-points
            for (size_t tj = 1; tj + 1 < tarN; ++tj) {
                if (!tarMatched[tj]) {
                    param.insertAxisPoint(axis, targetVals[tj]);
                }
            }

            // Now lengths should match; set positions to exact target values in order
            // Refresh current length
            curN = param.axisPoints[axis].length;
            // Sanity: rely on engine keeping axisPoints sorted; assign by index
            for (size_t i = 1; i + 1 < curN && i + 1 < tarN; ++i) {
                param.axisPoints[axis][i] = targetVals[i];
            }
        }

        // Apply breakpoints
        if (axisX.length >= 2) applyAxis(0, axisX);
        if (param.isVec2 && axisY.length >= 2) applyAxis(1, axisY);

        // Remap all binding values by nearest neighbor in normalized space
        // Use tolerance based on half of old cell size so that far points become unset
        float[] newXs = param.axisPoints[0];
        float[] newYs = param.axisPoints[1];
        auto nearestIndex = (const float[] arr, float v) {
            size_t best = 0; float bestd = float.infinity;
            foreach (i, a; arr) { float d = a - v; if (d < 0) d = -d; if (d < bestd) { bestd = d; best = i; } }
            return best;
        };
        auto halfCell = (const float[] arr) {
            float[] hw; hw.length = arr.length;
            if (arr.length == 0) return hw;
            foreach (i, a; arr) {
                float leftGap = i > 0 ? a - arr[i-1] : (arr.length > 1 ? arr[1] - a : 1.0f);
                float rightGap = i+1 < arr.length ? arr[i+1] - a : (arr.length > 1 ? a - arr[i-1] : 1.0f);
                float gap = leftGap < rightGap ? leftGap : rightGap;
                hw[i] = gap * 0.5f;
            }
            return hw;
        };
        float[] halfX = halfCell(oldXs);
        float[] halfY = halfCell(oldYs);
        // Helper for one-to-one nearest matching between old and new grid points
        struct Pair { size_t xi, yi, ox, oy; float d; }
        auto buildPairs = (const float[] xsN, const float[] ysN, const float[] xsO, const float[] ysO) {
            Pair[] pairs;
            foreach (xi; 0..xsN.length) foreach (yi; 0..ysN.length) {
                foreach (ox; 0..xsO.length) foreach (oy; 0..ysO.length) {
                    float dx = xsN[xi] - xsO[ox]; if (dx < 0) dx = -dx;
                    float dy = ysN[yi] - ysO[oy]; if (dy < 0) dy = -dy;
                    pairs ~= Pair(xi, yi, ox, oy, dx+dy);
                }
            }
            import std.algorithm : sort;
            sort!((a,b)=>a.d<b.d)(pairs);
            return pairs;
        };
        auto assignMatchesF = (ref SnapF s) {
            s.b.clear();
            size_t NX = newXs.length, NY = newYs.length, OX = oldXs.length, OY = oldYs.length;
            bool[] matchedNew; matchedNew.length = NX*NY;
            bool[] matchedOld; matchedOld.length = OX*OY;
            auto LNi = (size_t x, size_t y){ return x*NY + y; };
            auto LOi = (size_t x, size_t y){ return x*OY + y; };
            auto pairs = buildPairs(newXs, newYs, oldXs, oldYs);
            foreach (p; pairs) {
                size_t iN = LNi(p.xi, p.yi);
                size_t iO = LOi(p.ox, p.oy);
                if (!matchedNew[iN] && !matchedOld[iO]) {
                    matchedNew[iN] = true; matchedOld[iO] = true;
                    if (s.set[p.ox][p.oy]) s.b.setValue(vec2u(cast(uint)p.xi, cast(uint)p.yi), s.vals[p.ox][p.oy]);
                    else s.b.unset(vec2u(cast(uint)p.xi, cast(uint)p.yi));
                }
            }
            s.b.reInterpolate();
        };
        auto assignMatchesP = (ref SnapP s) {
            s.b.clear();
            size_t NX = newXs.length, NY = newYs.length, OX = oldXs.length, OY = oldYs.length;
            bool[] matchedNew; matchedNew.length = NX*NY;
            bool[] matchedOld; matchedOld.length = OX*OY;
            auto LNi = (size_t x, size_t y){ return x*NY + y; };
            auto LOi = (size_t x, size_t y){ return x*OY + y; };
            auto pairs = buildPairs(newXs, newYs, oldXs, oldYs);
            foreach (p; pairs) {
                size_t iN = LNi(p.xi, p.yi);
                size_t iO = LOi(p.ox, p.oy);
                if (!matchedNew[iN] && !matchedOld[iO]) {
                    matchedNew[iN] = true; matchedOld[iO] = true;
                    if (s.set[p.ox][p.oy]) s.b.setValue(vec2u(cast(uint)p.xi, cast(uint)p.yi), s.vals[p.ox][p.oy]);
                    else s.b.unset(vec2u(cast(uint)p.xi, cast(uint)p.yi));
                }
            }
            s.b.reInterpolate();
        };
        auto assignMatchesD = (ref SnapD s) {
            s.b.clear();
            size_t NX = newXs.length, NY = newYs.length, OX = oldXs.length, OY = oldYs.length;
            bool[] matchedNew; matchedNew.length = NX*NY;
            bool[] matchedOld; matchedOld.length = OX*OY;
            auto LNi = (size_t x, size_t y){ return x*NY + y; };
            auto LOi = (size_t x, size_t y){ return x*OY + y; };
            auto pairs = buildPairs(newXs, newYs, oldXs, oldYs);
            foreach (p; pairs) {
                size_t iN = LNi(p.xi, p.yi);
                size_t iO = LOi(p.ox, p.oy);
                if (!matchedNew[iN] && !matchedOld[iO]) {
                    matchedNew[iN] = true; matchedOld[iO] = true;
                    if (s.set[p.ox][p.oy]) s.b.setValue(vec2u(cast(uint)p.xi, cast(uint)p.yi), s.vals[p.ox][p.oy]);
                    else s.b.unset(vec2u(cast(uint)p.xi, cast(uint)p.yi));
                }
            }
            s.b.reInterpolate();
        };
        foreach (i, ref s; snapsF) assignMatchesF(s);
        foreach (i, ref s; snapsP) assignMatchesP(s);
        foreach (i, ref s; snapsD) assignMatchesD(s);

        // Notify after remap
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

enum ParamPropCommand {
    SetParameterName,
    ApplyParameterPropsAxes,
}

Command[ParamPropCommand] commands;

void ngInitCommands(T)() if (is(T == ParamPropCommand))
{
    // NOTE: Commands with ExCommand template args require explicit default args for registration
    mixin(registerCommand!(ParamPropCommand.SetParameterName, ""));
    mixin(registerCommand!(ParamPropCommand.ApplyParameterPropsAxes, [0f, 0f], [1f, 1f], new float[](0), new float[](0)));
}
