/*
    nijilive Part extended with layer information
    previously Inochi2D Part extended with layer information

    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.ext.nodes.excamera;
import nijilive.core.nodes.part;
import nijilive.core.nodes;
import nijilive.core;
import nijilive.fmt.serialize;
//import std.stdio : writeln;
import nijilive.math;
import nijigenerate.core.dbg;
import std.math : abs, isFinite, round;

@TypeId("Camera")
class ExCamera : Node {
protected:
    vec2 viewport = vec2(1920, 1080);

    override
    void serializeSelfImpl(ref InochiSerializer serializer, bool recursive = true, SerializeNodeFlags flags = SerializeNodeFlags.All) {
        super.serializeSelfImpl(serializer, recursive, flags);
        if (flags & SerializeNodeFlags.State) {
            serializer.putKey("viewport");
            serializer.serializeValue(viewport.vector);
        }
    }

    override
    SerdeException deserializeFromFghj(Fghj data) {
        auto err = super.deserializeFromFghj(data);
        if (err) return err;

        if (!data["viewport"].isEmpty) data["viewport"].deserializeValue(viewport.vector);
        viewport = normalizeViewport(viewport);
        return null;
    }

    override
    string typeId() {
        return "Camera";
    }

    /**
        Initial bounds size
    */
    override
    vec4 getInitialBoundsSize() {
        auto tr = transform;
        auto vpHalfSize = (viewport/2)*transform.scale;
        vec3 topLeft = vec3(
            tr.translation.x-vpHalfSize.x, 
            tr.translation.y-vpHalfSize.y,
            0
        );
        vec3 bottomRight = vec3(
            tr.translation.x+vpHalfSize.x, 
            tr.translation.y+vpHalfSize.y,
            0
        );

        return vec4(topLeft.xy, bottomRight.xy);
    }

    override
    void drawBounds() {
        auto tr = transform;
        auto vpHalfSize = viewport/2;

        vec3 topLeft = vec3(-vpHalfSize.x, -vpHalfSize.y, 0);
        vec3 bottomRight = vec3(+vpHalfSize.x, +vpHalfSize.y, 0);
        vec3 topRight = vec3(bottomRight.x, topLeft.y, 0);
        vec3 bottomLeft = vec3(topLeft.x, bottomRight.y, 0);

        inDbgSetBuffer([
            topLeft, topRight,
            topRight, bottomRight,
            bottomRight, bottomLeft,
            bottomLeft, topLeft
        ]);

        inDbgLineWidth(2);
        inDbgDrawLines(vec4(0, 0, 0, 1), transform.matrix);
        inDbgLineWidth(1);
        inDbgDrawLines(vec4(1, 1, 1, 1), transform.matrix);
    }

public:
    this() { super(); }
    this(Node parent) { super(parent); }
    this(vec2 viewport) { 
        super();
        this.viewport = normalizeViewport(viewport);
    }

    /**
        Gets nijilive camera for this camera
    */
    Camera getCamera() {
        vec2 scale = transform().scale;

        Camera cam = new Camera();
        cam.position = transform().translation.xy*vec2(-1, -1);
        cam.rotation = -transform().rotation.z;
        cam.scale = vec2(1/scale.x, 1/scale.y);
        return cam;
    }

    /**
        Gets the viewport for this camera
    */
    ref vec2 getViewport() {
        return viewport;
    }

    void setViewport(vec2 value) {
        viewport = normalizeViewport(value);
    }

    void foldScaleIntoViewport() {
        auto scale = localTransform.scale;
        if (scale.x == 1 && scale.y == 1) return;

        auto childWorldTransforms = captureChildWorldTransforms();
        setViewport(vec2(viewport.x * abs(scale.x), viewport.y * abs(scale.y)));
        localTransform.scale = vec2(scaleSign(scale.x), scaleSign(scale.y));
        localTransform.update();
        transformChanged();
        restoreChildWorldTransforms(childWorldTransforms);
    }

    private static vec2 normalizeViewport(vec2 value) {
        return vec2(evenDimension(value.x), evenDimension(value.y));
    }

    private static float evenDimension(float value) {
        float size = abs(value);
        if (!isFinite(size)) return 2;

        int rounded = cast(int)round(size);
        if (rounded < 2) return 2;
        if (rounded % 2 != 0) rounded++;
        return cast(float)rounded;
    }

    private static float scaleSign(float value) {
        return value < 0 ? -1.0f : 1.0f;
    }

    private Transform[uint] captureChildWorldTransforms() {
        Transform[uint] result;
        foreach (child; children) {
            result[child.uuid] = child.transform();
        }
        return result;
    }

    private void restoreChildWorldTransforms(Transform[uint] childWorldTransforms) {
        foreach (child; children) {
            if (auto worldTransform = child.uuid in childWorldTransforms) {
                restoreWorldTransformUnder(child, this, *worldTransform);
                child.notifyChange(child, NotifyReason.AttributeChanged);
                child.notifyChange(child, NotifyReason.Transformed);
            }
        }
    }

    private static float nonZeroScale(float value) {
        return abs(value) < 0.0001f ? 1.0f : value;
    }

    private static Transform rootTransformFor(Node child) {
        auto puppet = child.puppet();
        if (puppet !is null && puppet.root !is null)
            return puppet.root.localTransform;
        return Transform(vec3(0, 0, 0));
    }

    private static void restoreWorldTransformUnder(Node child, Node parent, Transform worldTransform) {
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

}

void incRegisterExCamera() {
    inRegisterNodeType!ExCamera();
}

bool incVerifyCameraSizeShowWarning(ref ExCamera selectedCamera) {
    import nijigenerate.widgets;
    import i18n;
    if (selectedCamera.getViewport().x % 2 != 0 || selectedCamera.getViewport().y % 2 != 0) {
        incTextColored(ImVec4(1, 0, 0, 1), _("Warning: Camera size must be divisible by 2"));
        return true;
    }
    return false;
}
