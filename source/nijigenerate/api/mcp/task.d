module nijigenerate.api.mcp.task;

import core.thread : Thread;
import core.sync.mutex : Mutex;
import core.sync.condition : Condition;

// Queue and synchronization
// Simple queue item representing a command to run on the main thread
private struct EnqueuedCommand { void delegate() action; }


private __gshared EnqueuedCommand[] gQueue;
private __gshared Mutex gQueueMutex;

void ngMcpInitTask() {
    if (gQueueMutex is null) gQueueMutex = new Mutex();
}

// Enqueue a command run request
void ngMcpEnqueueAction(void delegate() action)
{
    synchronized (gQueueMutex) gQueue ~= EnqueuedCommand(action);
}

// Public: process pending queue items on the main thread
void ngMcpProcessQueue() {
    if (gQueueMutex is null) return;
    EnqueuedCommand[] items;
    synchronized (gQueueMutex) {
        if (gQueue.length == 0) return;
        items = gQueue;
        gQueue.length = 0;
    }

    foreach (item; items) {
        if (item.action !is null) {
            try item.action(); catch (Exception) {}
        }
    }
}

T ngRunInMainThread(T)(T delegate() action) {
    auto lock = new Mutex();
    auto cond = new Condition(lock);
    bool done = false;
    Exception captured;
    static if (!is(T: void)) T result;

    ngMcpEnqueueAction({
        static if (!is(T: void)) T result2;
        try {
            if (action !is null) {
                static if (!is(T: void))
                    result2 = action();
                else
                    action();
            }
            synchronized (lock) {
                static if (!is(T: void)) result = result2;
                done = true;
                cond.notify();
            }
        } catch (Exception e) {
            // Preserve exception and still notify waiters to avoid deadlock/timeouts
            synchronized (lock) {
                captured = e;
                done = true;
                cond.notify();
            }
        }
    });
    synchronized (lock) { 
        while (!done) cond.wait(); 
    }
    if (captured !is null) throw captured;
    static if (!is(T: void))
        return result;
}
