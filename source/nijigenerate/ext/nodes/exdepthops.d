/*
    Shared extension for optional depth edit operation definitions.

    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.ext.nodes.exdepthops;

import nijilive.core.nodes;
import nijilive.fmt.serialize;
import nijilive.math;

enum ExDepthOpType {
    AttachedPoint,
    Ring,
    Plane,
}

struct ExDepthOp {
    ExDepthOpType type;

    size_t index;
    vec2 p0;
    vec2 p1;
    vec2 center;

    float amount;
    float width;
    float hardness;
    float p0Angle = 180.0f;
    float p1Angle = 0.0f;

    float radiusX;
    float radiusY;
    float angle;
    float targetDepth;
    float flattenStrength;

    string typeName() const {
        final switch (type) {
            case ExDepthOpType.AttachedPoint: return "attached-point";
            case ExDepthOpType.Ring: return "ring";
            case ExDepthOpType.Plane: return "plane";
        }
    }

    void serialize(S)(ref S serializer) const {
        auto state = serializer.structBegin();
        serializer.putKey("type");
        serializer.putValue(typeName());

        final switch (type) {
            case ExDepthOpType.AttachedPoint:
                serializer.putKey("index");
                serializer.serializeValue(index);
                serializer.putKey("amount");
                serializer.serializeValue(amount);
                break;

            case ExDepthOpType.Ring:
                serializer.putKey("p0");
                p0.serialize(serializer);
                serializer.putKey("p1");
                p1.serialize(serializer);
                serializer.putKey("amount");
                serializer.serializeValue(amount);
                serializer.putKey("width");
                serializer.serializeValue(width);
                serializer.putKey("hardness");
                serializer.serializeValue(hardness);
                serializer.putKey("p0Angle");
                serializer.serializeValue(p0Angle);
                serializer.putKey("p1Angle");
                serializer.serializeValue(p1Angle);
                break;

            case ExDepthOpType.Plane:
                serializer.putKey("center");
                center.serialize(serializer);
                serializer.putKey("radiusX");
                serializer.serializeValue(radiusX);
                serializer.putKey("radiusY");
                serializer.serializeValue(radiusY);
                serializer.putKey("angle");
                serializer.serializeValue(angle);
                serializer.putKey("targetDepth");
                serializer.serializeValue(targetDepth);
                serializer.putKey("flattenStrength");
                serializer.serializeValue(flattenStrength);
                break;
        }

        serializer.structEnd(state);
    }

    SerdeException deserializeFromFghj(Fghj data) {
        string typeString;
        if (auto exc = data["type"].deserializeValue(typeString)) return exc;

        switch (typeString) {
            case "attached-point":
                type = ExDepthOpType.AttachedPoint;
                if (!data["index"].isEmpty) {
                    if (auto exc = data["index"].deserializeValue(index)) return exc;
                }
                if (!data["amount"].isEmpty) {
                    if (auto exc = data["amount"].deserializeValue(amount)) return exc;
                }
                return null;

            case "ring":
                type = ExDepthOpType.Ring;
                if (!data["p0"].isEmpty) p0.deserialize(data["p0"]);
                if (!data["p1"].isEmpty) p1.deserialize(data["p1"]);
                if (!data["amount"].isEmpty) {
                    if (auto exc = data["amount"].deserializeValue(amount)) return exc;
                }
                if (!data["width"].isEmpty) {
                    if (auto exc = data["width"].deserializeValue(width)) return exc;
                }
                if (!data["hardness"].isEmpty) {
                    if (auto exc = data["hardness"].deserializeValue(hardness)) return exc;
                }
                if (!data["p0Angle"].isEmpty) {
                    if (auto exc = data["p0Angle"].deserializeValue(p0Angle)) return exc;
                }
                if (!data["p1Angle"].isEmpty) {
                    if (auto exc = data["p1Angle"].deserializeValue(p1Angle)) return exc;
                }
                return null;

            case "plane":
                type = ExDepthOpType.Plane;
                if (!data["center"].isEmpty) center.deserialize(data["center"]);
                if (!data["radiusX"].isEmpty) {
                    if (auto exc = data["radiusX"].deserializeValue(radiusX)) return exc;
                }
                if (!data["radiusY"].isEmpty) {
                    if (auto exc = data["radiusY"].deserializeValue(radiusY)) return exc;
                }
                if (!data["angle"].isEmpty) {
                    if (auto exc = data["angle"].deserializeValue(angle)) return exc;
                }
                if (!data["targetDepth"].isEmpty) {
                    if (auto exc = data["targetDepth"].deserializeValue(targetDepth)) return exc;
                }
                if (!data["flattenStrength"].isEmpty) {
                    if (auto exc = data["flattenStrength"].deserializeValue(flattenStrength)) return exc;
                }
                return null;

            default:
                return new SerdeException("unknown depth operation type: " ~ typeString);
        }
    }
}

interface DepthOperationMappedNode {
    ExDepthOp[] copyDepthOps();
    void replaceDepthOps(ExDepthOp[] values);
}

mixin template ExDepthOperated() {
public:
    ExDepthOp[] depthOps;

    ExDepthOp[] copyDepthOps() {
        return depthOps.dup;
    }

    void replaceDepthOps(ExDepthOp[] values) {
        depthOps = values.dup;
    }

    void copyDepthOpsFrom(Node src) {
        if (auto operated = cast(DepthOperationMappedNode)src) {
            depthOps = operated.copyDepthOps();
        } else {
            depthOps = null;
        }
    }

    void serializeDepthOps(ref InochiSerializer serializer) {
        if (depthOps.length == 0) return;

        serializer.putKey("depth-ops");
        serializer.serializeValue(depthOps);
    }

    SerdeException deserializeDepthOps(Fghj data) {
        if (data["depth-ops"].isEmpty) return null;
        if (data["depth-ops"].kind == Fghj.Kind.null_) {
            depthOps = null;
            return null;
        }

        depthOps.length = 0;
        foreach (entry; data["depth-ops"].byElement) {
            ExDepthOp op;
            if (auto exc = op.deserializeFromFghj(entry)) return exc;
            depthOps ~= op;
        }
        return null;
    }
}
