module nijigenerate.viewport.depth.draw.coordinate;

import nijigenerate.viewport.depth.common.targetview;
import nijigenerate.viewport.depth.draw.layer;
import nijilive;
import std.algorithm : max, min;

void ngDepthDrawLayerDocumentBounds(
    ref const(DepthDrawLayer) layer,
    out vec2 minPoint,
    out vec2 maxPoint
) {
    auto scaleX = layer.xyScale.x == 0.0f ? 1.0f : layer.xyScale.x;
    auto scaleY = layer.xyScale.y == 0.0f ? 1.0f : layer.xyScale.y;
    auto x0 = cast(float)layer.bounds.left + layer.xyOffset.x;
    auto y0 = cast(float)layer.bounds.top + layer.xyOffset.y;
    auto x1 = x0 + cast(float)layer.bounds.width * scaleX;
    auto y1 = y0 + cast(float)layer.bounds.height * scaleY;
    minPoint = vec2(min(x0, x1), min(y0, y1));
    maxPoint = vec2(max(x0, x1), max(y0, y1));
}

vec2 ngDepthDrawVertexDocumentPosition(
    DepthTargetView target,
    vec2 vertex,
    int documentWidth,
    int documentHeight
) {
    auto grid = target.getTarget();
    auto world = grid.transform.matrix * vec4(vertex, 0, 1);
    return vec2(
        world.x + cast(float)documentWidth / 2.0f,
        world.y + cast(float)documentHeight / 2.0f
    );
}

vec2 ngDepthDrawLayerPixelFromDocument(DepthDrawLayer layer, vec2 documentPoint) {
    auto scaleX = layer.xyScale.x == 0.0f ? 1.0f : layer.xyScale.x;
    auto scaleY = layer.xyScale.y == 0.0f ? 1.0f : layer.xyScale.y;
    auto raw = vec2(
        documentPoint.x - cast(float)layer.bounds.left,
        documentPoint.y - cast(float)layer.bounds.top
    );
    return vec2(
        (raw.x - layer.xyOffset.x) / scaleX,
        (raw.y - layer.xyOffset.y) / scaleY
    );
}

vec2 ngDepthDrawLayerPixelFromVertex(
    DepthTargetView target,
    DepthDrawLayer layer,
    vec2 vertex,
    int documentWidth,
    int documentHeight
) {
    auto documentPoint = ngDepthDrawVertexDocumentPosition(target, vertex, documentWidth, documentHeight);
    return ngDepthDrawLayerPixelFromDocument(layer, documentPoint);
}
