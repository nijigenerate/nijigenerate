module nijigenerate.viewport.depth.common.session;

import nijigenerate.actions.depth : DepthMappedChangeAction;
import nijigenerate.commands.depth.map : ngApplyDepthsChangeAction;
import nijigenerate.viewport.depth.camera : DepthCamera3D;
import nijigenerate.viewport.depth.common.targetview;
import nijilive;

DepthMappedChangeAction ngDepthViewWorkingDepthsChangeAction(DepthTargetView view, string reason) {
    if (view is null || view.getTarget() is null) return null;
    return ngApplyDepthsChangeAction(view.getTarget(), view.copyWorkingDepths(), reason);
}

class DepthViewSession {
private:
    float depthDisplayScale;

    ptrdiff_t findTargetIndex(ulong gridUuid) {
        foreach (i, view; targets) {
            if (view !is null && view.getTarget() !is null && view.getTarget().uuid == gridUuid) {
                return cast(ptrdiff_t)i;
            }
        }
        return -1;
    }

public:
    DepthCamera3D camera;
    DepthTargetView[] targets;
    ulong selectedGridUuid;

    DepthTargetView ensureTarget(Deformable target) {
        if (target is null) return null;
        if (auto existing = targetByGrid(target.uuid)) return existing;
        auto view = new DepthTargetView(target);
        view.setDepthDisplayScale(depthDisplayScale);
        targets ~= view;
        if (selectedGridUuid == 0) selectedGridUuid = target.uuid;
        return view;
    }

    void setTargets(Deformable[] nextTargets) {
        DepthTargetView[] nextViews;
        bool containsNext(ulong uuid) {
            foreach (view; nextViews) {
                if (view !is null && view.getTarget() !is null && view.getTarget().uuid == uuid) return true;
            }
            return false;
        }

        foreach (target; nextTargets) {
            if (target is null || containsNext(target.uuid)) continue;
            auto view = targetByGrid(target.uuid);
            if (view is null) view = new DepthTargetView(target);
            view.setDepthDisplayScale(depthDisplayScale);
            nextViews ~= view;
        }

        targets = nextViews;
        if (selectedGridUuid != 0 && targetByGrid(selectedGridUuid) !is null) return;
        selectedGridUuid = targets.length > 0 && targets[0] !is null && targets[0].getTarget() !is null
            ? targets[0].getTarget().uuid
            : 0;
    }

    void setDepthDisplayScale(float value) {
        depthDisplayScale = value > 0.0f ? value : 0.0f;
        foreach (view; targets) {
            if (view !is null) view.setDepthDisplayScale(depthDisplayScale);
        }
    }

    DepthTargetView targetByGrid(ulong gridUuid) {
        auto index = findTargetIndex(gridUuid);
        return index >= 0 ? targets[cast(size_t)index] : null;
    }

    bool selectTarget(ulong gridUuid) {
        if (targetByGrid(gridUuid) is null) return false;
        selectedGridUuid = gridUuid;
        return true;
    }

    DepthTargetView selectedTarget() {
        return targetByGrid(selectedGridUuid);
    }

    void removeTarget(ulong gridUuid) {
        auto index = findTargetIndex(gridUuid);
        if (index < 0) return;
        auto i = cast(size_t)index;
        targets = targets[0 .. i] ~ targets[i + 1 .. $];
        if (selectedGridUuid == gridUuid) {
            selectedGridUuid = targets.length > 0 && targets[0] !is null && targets[0].getTarget() !is null
                ? targets[0].getTarget().uuid
                : 0;
        }
    }

    void refreshGeometry() {
        foreach (view; targets) {
            if (view !is null) view.refreshGeometry();
        }
    }

    void resetWorkingDepths() {
        foreach (view; targets) {
            if (view !is null) view.resetWorkingDepths();
        }
    }

    void resetFromTargets() {
        foreach (view; targets) {
            if (view !is null) view.resetFromTarget();
        }
    }

    DepthMappedChangeAction workingDepthsChangeAction(ulong gridUuid, string reason) {
        return ngDepthViewWorkingDepthsChangeAction(targetByGrid(gridUuid), reason);
    }

    DepthMappedChangeAction selectedWorkingDepthsChangeAction(string reason) {
        return workingDepthsChangeAction(selectedGridUuid, reason);
    }

    void clear() {
        targets = null;
        selectedGridUuid = 0;
        depthDisplayScale = 0.0f;
    }
}
