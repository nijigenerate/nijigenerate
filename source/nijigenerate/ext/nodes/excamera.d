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

        setViewport(vec2(viewport.x * scale.x, viewport.y * scale.y));
        localTransform.scale = vec2(1, 1);
        localTransform.update();
        transformChanged();
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
