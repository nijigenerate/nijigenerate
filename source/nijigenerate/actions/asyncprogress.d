module nijigenerate.actions.asyncprogress;

import bindbc.imgui : ImGuiIO, ImVec2, igProgressBar, igText;
import nijigenerate.actions : AsyncGroupAction, AsyncGroupActionEvent;
import nijigenerate.api.mcp.task : ngMcpEnqueueAction;
import nijigenerate.widgets.notification : NotificationPopup;
import std.algorithm.comparison : max, min;
import std.string : toStringz;

/**
    Presents progress for any AsyncGroupAction.

    The action owns progress state and cancellation semantics. An optional work
    counter can represent pipelines which split one scheduled operation into
    internal tasks. The optional pump advances one slice of subsystem work on
    the main thread; independently running tasks may omit both callbacks and
    update the action directly.
*/
class AsyncGroupActionProgress {
public:
    alias PumpHandler = void delegate();
    alias WorkCountHandler = size_t delegate();

private:
    AsyncGroupAction action;
    string title;
    string completedMessage;
    string canceledMessage;
    WorkCountHandler workCount;
    PumpHandler pump;
    ulong observerId;
    ulong popupId;
    ulong popupGeneration;
    ulong notifiedCompletionGeneration;
    size_t observedTotal;
    bool pumpQueued;
    bool disposed;

public:
    this(
        AsyncGroupAction action,
        string title,
        string completedMessage = null,
        string canceledMessage = null,
        WorkCountHandler workCount = null,
        PumpHandler pump = null,
    ) {
        this.action = action;
        this.title = title;
        this.completedMessage = completedMessage;
        this.canceledMessage = canceledMessage;
        this.workCount = workCount;
        this.pump = pump;
        if (action is null) return;
        observerId = action.addObserver(&onActionEvent);
        if (action.isApplied && action.pendingAsyncCount > 0) activate();
    }

    bool isActive() const {
        return !disposed && action !is null && action.isApplied &&
            remainingWork > 0;
    }

    bool isVisible() const {
        return !disposed && popupId != 0 && NotificationPopup.instance().isOpen(popupId);
    }

    void dispose() {
        if (disposed) return;
        disposed = true;
        if (action !is null) action.removeObserver(observerId);
        observerId = 0;
        closePopup();
        action = null;
        workCount = null;
        pump = null;
    }

private:
    void onActionEvent(AsyncGroupAction source, AsyncGroupActionEvent event) {
        if (disposed || source !is action) return;
        final switch (event) {
            case AsyncGroupActionEvent.Scheduled:
            case AsyncGroupActionEvent.Running:
            case AsyncGroupActionEvent.Progressed:
                if (remainingWork > 0) activate();
                break;
            case AsyncGroupActionEvent.Completed:
                if (remainingWork > 0) activate();
                else complete();
                break;
            case AsyncGroupActionEvent.Failed:
                closePopup();
                break;
            case AsyncGroupActionEvent.Canceled:
                cancel();
                break;
            case AsyncGroupActionEvent.Redone:
                closePopup();
                break;
        }
    }

    void activate() {
        if (!isActive) return;
        auto generation = action.generation;
        if (popupId == 0 || popupGeneration != generation) {
            closePopup();
            popupGeneration = generation;
            observedTotal = max(cast(size_t)1, remainingWork);
            auto self = this;
            popupId = NotificationPopup.instance().popup((ImGuiIO* io) {
                self.draw();
            }, -1);
        }
        schedulePump();
    }

    void draw() {
        if (disposed || action is null || !action.isApplied ||
            popupGeneration != action.generation || remainingWork == 0) {
            closePopup();
            return;
        }
        auto remaining = remainingWork;
        observedTotal = max(observedTotal, action.completedAsyncCount + remaining);
        auto total = observedTotal;
        auto completed = total > remaining ? total - remaining : 0;
        completed = min(completed, total);
        float ratio = total > 0 ? cast(float)completed / cast(float)total : 0.0f;
        igText(title.toStringz);
        igProgressBar(ratio, ImVec2(320, 0));
    }

    void schedulePump() {
        if (pump is null || pumpQueued || !isActive) return;
        pumpQueued = true;
        auto self = this;
        auto generation = action.generation;
        ngMcpEnqueueAction({
            self.pumpStep(generation);
        });
    }

    void pumpStep(ulong generation) {
        pumpQueued = false;
        if (disposed || action is null) return;
        if (!action.isApplied || action.generation != generation) {
            schedulePump();
            return;
        }
        if (remainingWork == 0) {
            complete();
            return;
        }
        try {
            pump();
        } catch (Throwable throwable) {
            closePopup();
            throw throwable;
        }
        if (remainingWork == 0) complete();
        else schedulePump();
    }

    void complete() {
        auto generation = action is null ? 0 : action.generation;
        closePopup();
        if (completedMessage.length == 0 || generation == notifiedCompletionGeneration) return;
        notifiedCompletionGeneration = generation;
        NotificationPopup.instance().popup(completedMessage, 3);
    }

    void cancel() {
        closePopup();
        if (canceledMessage.length > 0) NotificationPopup.instance().popup(canceledMessage, 3);
    }

    void closePopup() {
        if (popupId == 0) return;
        NotificationPopup.instance().close(popupId);
        popupId = 0;
        popupGeneration = 0;
    }

    size_t remainingWork() const {
        if (action is null) return 0;
        return workCount is null ? action.pendingAsyncCount : workCount();
    }
}
