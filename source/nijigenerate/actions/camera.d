module nijigenerate.actions.camera;

import i18n;
import nijigenerate.actions;
import nijigenerate.ext.nodes.excamera : ExCamera;
import nijilive;
import std.format;

class CameraResizeAction : Action {
public:
    ExCamera camera;
    Transform oldTransform;
    Transform newTransform;
    vec2 oldViewport;
    vec2 newViewport;

    this(ExCamera camera, Transform oldTransform, vec2 oldViewport, Transform newTransform, vec2 newViewport) {
        this.camera = camera;
        this.oldTransform = oldTransform;
        this.oldViewport = oldViewport;
        this.newTransform = newTransform;
        this.newViewport = newViewport;
        notify();
    }

    private void apply(Transform transform, vec2 viewport) {
        camera.localTransform = transform;
        camera.setViewport(viewport);
        camera.transformChanged();
        notify();
    }

    private void notify() {
        camera.notifyChange(camera, NotifyReason.AttributeChanged);
        camera.notifyChange(camera, NotifyReason.Transformed);
    }

    void rollback() {
        apply(oldTransform, oldViewport);
    }

    void redo() {
        apply(newTransform, newViewport);
    }

    string describe() {
        return _("Resized camera %s").format(camera.name);
    }

    string describeUndo() {
        return _("Undo resize camera %s").format(camera.name);
    }

    string getName() {
        return "CameraResize";
    }

    bool merge(Action other) {
        if (!canMerge(other)) return false;
        auto resize = cast(CameraResizeAction)other;
        newTransform = resize.newTransform;
        newViewport = resize.newViewport;
        return true;
    }

    bool canMerge(Action other) {
        auto resize = cast(CameraResizeAction)other;
        return resize !is null && resize.camera.uuid == camera.uuid;
    }
}
