/*
    Copyright © 2026, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.widgets.asyncderivedupdatedetails;

import bindbc.imgui;
import i18n;
import nijigenerate.core.asyncderivedupdate;
import nijigenerate.widgets.button : incButtonColored;
import nijigenerate.widgets.label : incTextColored, incTextLabel;
import nijigenerate.widgets.tooltip : incTooltip;
import std.algorithm.sorting : sort;
import std.format : format;
import std.string : toStringz;

private ImVec4 stateColor(AsyncDerivedUpdateState state) {
    final switch (state) {
    case AsyncDerivedUpdateState.Detected: return ImVec4(1.0f, 0.55f, 0.1f, 1.0f);
    case AsyncDerivedUpdateState.Queued: return ImVec4(0.72f, 0.35f, 1.0f, 1.0f);
    case AsyncDerivedUpdateState.Running: return ImVec4(0.1f, 0.9f, 1.0f, 1.0f);
    case AsyncDerivedUpdateState.Applied: return ImVec4(0.2f, 1.0f, 0.35f, 1.0f);
    case AsyncDerivedUpdateState.Stale: return ImVec4(1.0f, 0.45f, 0.1f, 1.0f);
    case AsyncDerivedUpdateState.Failed: return ImVec4(1.0f, 0.1f, 0.15f, 1.0f);
    case AsyncDerivedUpdateState.Canceled: return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
    }
}

private string stateLabel(AsyncDerivedUpdateState state) {
    final switch (state) {
    case AsyncDerivedUpdateState.Detected: return _("DETECTED");
    case AsyncDerivedUpdateState.Queued: return _("WAIT");
    case AsyncDerivedUpdateState.Running: return _("RUN");
    case AsyncDerivedUpdateState.Applied: return _("OK");
    case AsyncDerivedUpdateState.Stale: return _("STALE");
    case AsyncDerivedUpdateState.Failed: return _("ERROR");
    case AsyncDerivedUpdateState.Canceled: return _("CANCELED");
    }
}

private string progressText(ref const AsyncDerivedUpdateSnapshot snapshot) {
    if (snapshot.expectedUnits == 0) return null;
    return " %s/%s".format(snapshot.appliedUnits, snapshot.expectedUnits);
}

/** Draw details only for registered derived updates; explicit tools are absent. */
void drawAsyncDerivedUpdateDetailsUi(
    ref const AsyncDerivedUpdateSnapshot[] snapshots,
) {
    size_t detected;
    size_t queued;
    size_t running;
    size_t applied;
    size_t stale;
    size_t failed;
    size_t canceled;
    size_t detailCount;
    foreach (ref const snapshot; snapshots) {
        if ((snapshot.display & cast(uint)AsyncDerivedUpdateDisplay.Details) == 0)
            continue;
        detailCount++;
        final switch (snapshot.state) {
        case AsyncDerivedUpdateState.Detected: detected++; break;
        case AsyncDerivedUpdateState.Queued: queued++; break;
        case AsyncDerivedUpdateState.Running: running++; break;
        case AsyncDerivedUpdateState.Applied: applied++; break;
        case AsyncDerivedUpdateState.Stale: stale++; break;
        case AsyncDerivedUpdateState.Failed: failed++; break;
        case AsyncDerivedUpdateState.Canceled: canceled++; break;
        }
    }
    if (detailCount == 0) return;

    auto summary = _("Updates  wait:%s  run:%s  ok:%s  stale:%s  error:%s").format(
        queued + detected, running, applied, stale + canceled, failed);
    auto summaryState = failed > 0 ? AsyncDerivedUpdateState.Failed :
        (running > 0 ? AsyncDerivedUpdateState.Running :
        (queued + detected > 0 ? AsyncDerivedUpdateState.Queued :
        (stale + canceled > 0 ? AsyncDerivedUpdateState.Stale :
        AsyncDerivedUpdateState.Applied)));
    if (incButtonColored(
        summary.toStringz,
        ImVec2(0, 26),
        stateColor(summaryState))) {
        igOpenPopup("AsyncDerivedUpdateDetails");
    }
    incTooltip(_("Updates derived asynchronously from another edit. Click for details."));

    if (igBeginPopup("AsyncDerivedUpdateDetails")) {
        incTextLabel(_("Derived Updates"));
        size_t[] detailIndices;
        foreach (i, ref const snapshot; snapshots) {
            if ((snapshot.display & cast(uint)AsyncDerivedUpdateDisplay.Details) != 0)
                detailIndices ~= i;
        }
        detailIndices.sort!((a, b) =>
            snapshots[a].label < snapshots[b].label);
        foreach (i; detailIndices) {
            ref const snapshot = snapshots[i];
            auto operation = snapshot.origin.operationName.length
                ? snapshot.origin.operationName : _("Update");
            incTextColored(
                stateColor(snapshot.state),
                "%s  %s%s".format(
                    snapshot.label,
                    stateLabel(snapshot.state),
                    progressText(snapshot)));
            incTextLabel(_("Operation: %s").format(operation));
            if (snapshot.reason.length)
                incTextLabel(_("Reason: %s").format(snapshot.reason));
            if (snapshot.retryCount > 0)
                incTextLabel(_("Retry: %s").format(snapshot.retryCount));
            if (snapshot.detail.length)
                incTextLabel(_("Detail: %s").format(snapshot.detail));
            igSeparator();
        }
        igEndPopup();
    }
}
