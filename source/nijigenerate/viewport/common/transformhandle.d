module nijigenerate.viewport.common.transformhandle;

import nijigenerate.actions;
import nijigenerate.ext.nodes.excamera : ExCamera;
import nijilive;
import std.algorithm : max, min;
import std.math : abs;

abstract class ViewportTransformHandleAdapter {
    abstract vec4 bounds(Node node);
    abstract vec2 scaleHandleValue(Node node);
    abstract void beginScale(Node node);
    abstract void applyScale(Node node, vec2 scale);
    abstract void endScale(Node node, vec2 previousScale, ref Action[string] actions);
}

private class DefaultViewportTransformHandleAdapter : ViewportTransformHandleAdapter {
    override vec4 bounds(Node node) {
        vec4 result = node.getCombinedBounds();
        vec4 ownBounds;
        if (selfMeshBounds(node, ownBounds)) {
            unionBounds(result, ownBounds);
        }
        return result;
    }

    override vec2 scaleHandleValue(Node node) {
        return node.localTransform.scale;
    }

    override void beginScale(Node node) {
    }

    override void applyScale(Node node, vec2 scale) {
        node.localTransform.scale = scale;
        node.notifyChange(node, NotifyReason.AttributeChanged);
    }

    override void endScale(Node node, vec2 previousScale, ref Action[string] actions) {
        if (node.localTransform.scale.vector[0] != previousScale.x) {
            actions["X"] = new NodeValueChangeAction!(Node, float)(
                "X",
                node,
                previousScale.x,
                node.localTransform.scale.vector[0],
                &node.localTransform.scale.vector[0]
            );
        }
        if (node.localTransform.scale.vector[1] != previousScale.y) {
            actions["Y"] = new NodeValueChangeAction!(Node, float)(
                "Y",
                node,
                previousScale.y,
                node.localTransform.scale.vector[1],
                &node.localTransform.scale.vector[1]
            );
        }
    }
}

private class CameraViewportTransformHandleAdapter : ViewportTransformHandleAdapter {
private:
    ExCamera activeCamera;
    Transform oldTransform;
    vec2 oldViewport;
    Node[] children;
    Transform[uint] oldChildTransforms;
    Transform[uint] childWorldTransforms;

    ExCamera camera(Node node) {
        auto result = cast(ExCamera)node;
        assert(result !is null);
        return result;
    }

    void ensureActive(ExCamera camera) {
        if (activeCamera is camera) return;
        beginScale(camera);
    }

public:
    override vec4 bounds(Node node) {
        auto camera = camera(node);
        auto vpHalfSize = camera.getViewport() / 2;
        vec2[4] corners = [
            vec2(-vpHalfSize.x, -vpHalfSize.y),
            vec2(+vpHalfSize.x, -vpHalfSize.y),
            vec2(-vpHalfSize.x, +vpHalfSize.y),
            vec2(+vpHalfSize.x, +vpHalfSize.y),
        ];
        auto matrix = camera.transform.matrix;
        vec2 first = (matrix * vec4(corners[0], 0, 1)).xy;
        vec4 result = vec4(first.xyxy);
        foreach (corner; corners[1 .. $]) {
            vec2 point = (matrix * vec4(corner, 0, 1)).xy;
            result.x = min(result.x, point.x);
            result.y = min(result.y, point.y);
            result.z = max(result.z, point.x);
            result.w = max(result.w, point.y);
        }
        return result;
    }

    override vec2 scaleHandleValue(Node node) {
        return node.localTransform.scale;
    }

    override void beginScale(Node node) {
        activeCamera = camera(node);
        oldTransform = activeCamera.localTransform;
        oldViewport = activeCamera.getViewport();
        children = activeCamera.children.dup;
        oldChildTransforms.clear();
        childWorldTransforms.clear();
        foreach (child; children) {
            oldChildTransforms[child.uuid] = child.localTransform;
            childWorldTransforms[child.uuid] = child.transform();
        }
    }

    override void applyScale(Node node, vec2 scale) {
        auto camera = camera(node);
        ensureActive(camera);

        auto oldScale = oldTransform.scale;
        auto factor = vec2(
            scale.x / nonZeroScale(oldScale.x),
            scale.y / nonZeroScale(oldScale.y)
        );

        camera.localTransform = oldTransform;
        camera.localTransform.scale = vec2(scaleSign(scale.x), scaleSign(scale.y));
        camera.localTransform.update();
        camera.setViewport(vec2(oldViewport.x * abs(factor.x), oldViewport.y * abs(factor.y)));
        camera.transformChanged();

        foreach (child; children) {
            if (auto worldTransform = child.uuid in childWorldTransforms) {
                restoreWorldTransformUnder(child, camera, *worldTransform);
                child.notifyChange(child, NotifyReason.AttributeChanged);
                child.notifyChange(child, NotifyReason.Transformed);
            }
        }

        camera.notifyChange(camera, NotifyReason.AttributeChanged);
    }

    override void endScale(Node node, vec2 previousScale, ref Action[string] actions) {
        auto camera = camera(node);
        ensureActive(camera);
        if (camera.localTransform.scale == oldTransform.scale && camera.getViewport() == oldViewport) return;

        Transform[uint] newChildTransforms;
        foreach (child; children) {
            newChildTransforms[child.uuid] = child.localTransform;
        }

        actions["CameraResize"] = new CameraResizeAction(
            camera,
            oldTransform,
            oldViewport,
            camera.localTransform,
            camera.getViewport(),
            children,
            oldChildTransforms,
            newChildTransforms
        );
        activeCamera = null;
    }
}

private {
    ViewportTransformHandleAdapter defaultAdapter;
    ViewportTransformHandleAdapter cameraAdapter;
    Node lastAdapterNode;
    ViewportTransformHandleAdapter lastAdapter;
}

ViewportTransformHandleAdapter ngViewportTransformHandleAdapter(Node node) {
    if (node is lastAdapterNode && lastAdapter !is null) {
        return lastAdapter;
    }

    lastAdapterNode = node;
    lastAdapter = createAdapter(node);
    return lastAdapter;
}

private ViewportTransformHandleAdapter createAdapter(Node node) {
    if (defaultAdapter is null) {
        defaultAdapter = new DefaultViewportTransformHandleAdapter();
        cameraAdapter = new CameraViewportTransformHandleAdapter();
    }
    if (cast(ExCamera)node) return cameraAdapter;
    return defaultAdapter;
}

private void unionBounds(ref vec4 target, vec4 candidate) {
    target = vec4(
        min(target.x, candidate.x),
        min(target.y, candidate.y),
        max(target.z, candidate.z),
        max(target.w, candidate.w)
    );
}

private bool selfMeshBounds(Node node, out vec4 result) {
    if (auto drawable = cast(Drawable)node) {
        if (drawable.vertices.length == 0) return false;

        auto matrix = drawable.meshOverlayMatrix();
        auto points = drawable.meshOverlayPoints();
        if (points.length == 0) return false;

        vec2 first = (matrix * vec4(points[0], 0, 1)).xy;
        result = vec4(first.xyxy);
        foreach (i; 1 .. points.length) {
            vec2 point = (matrix * vec4(points[i], 0, 1)).xy;
            result.x = min(result.x, point.x);
            result.y = min(result.y, point.y);
            result.z = max(result.z, point.x);
            result.w = max(result.w, point.y);
        }
        return true;
    }

    if (auto deformable = cast(Deformable)node) {
        auto vertices = deformable.vertices;
        if (vertices.length == 0) return false;

        auto matrix = deformable.getDynamicMatrix();
        auto deformCount = deformable.deformation.length;
        vec2 first = vertices[0].toVector();
        if (deformCount > 0) first += deformable.deformation[0].toVector();
        first = (matrix * vec4(first, 0, 1)).xy;
        result = vec4(first.xyxy);

        foreach (i; 1 .. vertices.length) {
            vec2 point = vertices[i].toVector();
            if (i < deformCount) point += deformable.deformation[i].toVector();
            point = (matrix * vec4(point, 0, 1)).xy;
            result.x = min(result.x, point.x);
            result.y = min(result.y, point.y);
            result.z = max(result.z, point.x);
            result.w = max(result.w, point.y);
        }
        return true;
    }

    return false;
}

private float scaleSign(float value) {
    return value < 0 ? -1.0f : 1.0f;
}

private float nonZeroScale(float value) {
    return abs(value) < 0.0001f ? 1.0f : value;
}

private Transform rootTransformFor(Node child) {
    auto puppet = child.puppet();
    if (puppet !is null && puppet.root !is null)
        return puppet.root.localTransform;
    return Transform(vec3(0, 0, 0));
}

private void restoreWorldTransformUnder(Node child, Node parent, Transform worldTransform) {
    Transform parentTransform = child.lockToRoot()
        ? rootTransformFor(child)
        : (parent is null ? Transform(vec3(0, 0, 0)) : parent.transform());

    auto localWithOffsetTranslation = Node.getRelativePosition(parentTransform.matrix, worldTransform.matrix);
    auto localWithOffsetRotation = worldTransform.rotation - parentTransform.rotation;
    auto localWithOffsetScale = vec2(
        worldTransform.scale.x / nonZeroScale(parentTransform.scale.x),
        worldTransform.scale.y / nonZeroScale(parentTransform.scale.y)
    );

    child.localTransform.translation = localWithOffsetTranslation - vec3(
        child.getValue("transform.t.x"),
        child.getValue("transform.t.y"),
        child.getValue("transform.t.z")
    );
    child.localTransform.rotation = localWithOffsetRotation - vec3(
        child.getValue("transform.r.x"),
        child.getValue("transform.r.y"),
        child.getValue("transform.r.z")
    );
    child.localTransform.scale = vec2(
        localWithOffsetScale.x / nonZeroScale(child.getValue("transform.s.x")),
        localWithOffsetScale.y / nonZeroScale(child.getValue("transform.s.y"))
    );
    child.localTransform.update();
    child.transformChanged();
}
