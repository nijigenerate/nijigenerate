module nijigenerate.io.psdlayers;

import psd : Layer, LayerFlags, LayerType;
import std.array : join;

struct PsdLayerGroupState {
    string path;
    bool visible = true;
    float opacity = 1.0f;
}

PsdLayerGroupState[] ngPsdLayerGroupStates(const(Layer)[] layers) {
    PsdLayerGroupState[] states;
    states.length = layers.length;
    string[] pathSegments;
    bool[] visibility;
    float[] opacity;

    foreach (i, ref layer; layers) {
        final switch (layer.type) {
            case LayerType.OpenFolder:
            case LayerType.ClosedFolder:
                pathSegments ~= layer.name;
                visibility ~= (visibility.length == 0 || visibility[$-1]) &&
                    (layer.flags & LayerFlags.Visible) == 0;
                opacity ~= (opacity.length == 0 ? 1.0f : opacity[$-1]) *
                    cast(float)layer.opacity / 255.0f;
                break;
            case LayerType.SectionDivider:
                if (pathSegments.length > 0) {
                    pathSegments.length--;
                    visibility.length--;
                    opacity.length--;
                }
                break;
            case LayerType.Any:
                states[i].path = pathSegments.length > 0 ? "/" ~ pathSegments.join("/") : "";
                states[i].visible = visibility.length == 0 || visibility[$-1];
                states[i].opacity = opacity.length == 0 ? 1.0f : opacity[$-1];
                break;
        }
    }
    return states;
}
