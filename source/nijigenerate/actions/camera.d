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
    Node[] children;
    Transform[uint] oldChildTransforms;
    Transform[uint] newChildTransforms;

    this(
        ExCamera camera,
        Transform oldTransform,
        vec2 oldViewport,
        Transform newTransform,
        vec2 newViewport,
        Node[] children = null,
        Transform[uint] oldChildTransforms = null,
        Transform[uint] newChildTransforms = null
    ) {
        this.camera = camera;
        this.oldTransform = oldTransform;
        this.oldViewport = oldViewport;
        this.newTransform = newTransform;
        this.newViewport = newViewport;
        this.children = children;
        this.oldChildTransforms = oldChildTransforms;
        this.newChildTransforms = newChildTransforms;
        notify();
    }

    private void apply(Transform transform, vec2 viewport, Transform[uint] childTransforms) {
        camera.localTransform = transform;
        camera.setViewport(viewport);
        camera.transformChanged();
        foreach (child; children) {
            if (auto childTransform = child.uuid in childTransforms) {
                child.localTransform = *childTransform;
                child.localTransform.update();
                child.transformChanged();
                child.notifyChange(child, NotifyReason.AttributeChanged);
                child.notifyChange(child, NotifyReason.Transformed);
            }
        }
        notify();
    }

    private void notify() {
        camera.notifyChange(camera, NotifyReason.AttributeChanged);
        camera.notifyChange(camera, NotifyReason.Transformed);
    }

    void rollback() {
        apply(oldTransform, oldViewport, oldChildTransforms);
    }

    void redo() {
        apply(newTransform, newViewport, newChildTransforms);
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
        children = resize.children;
        newChildTransforms = resize.newChildTransforms;
        return true;
    }

    bool canMerge(Action other) {
        auto resize = cast(CameraResizeAction)other;
        return resize !is null && resize.camera.uuid == camera.uuid;
    }
}
