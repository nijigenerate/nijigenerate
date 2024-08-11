module nijigenerate.core.traits;

import nijilive;
import nijilive.math;

enum VertexDisplayType {
    None,
    DrawOne,
    DrawSubParts,
}

struct Traits(T) {
    T instance;

    VertexDisplayType showSubParts() { return VertexDisplayType.None; }
    bool mustShowAsSubParts() { return false; }
}

struct Traits(T: Drawable) {
    VertexDisplayType showSubParts() { return VertexDisplayType.DrawOne; }
}

struct Traits(T: MeshGroup) {
    T instance;

    VertexDisplayType showSubParts() { return VertexDisplayType.DrawSubParts; }
}

struct Traits(T: BezierDeformer) {
    T instance;

    VertexDisplayType showSubParts() { return VertexDisplayType.DrawSubParts; }
}