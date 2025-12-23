module nijigenerate.viewport.common.mesheditor.brushes.base;
import inmath;
import nijilive.math : Vec2Array;

interface Brush {
    string name();
    bool isInside(vec2 center, vec2 pos);
    float weightAt(vec2 center, vec2 pos);
    float[] weightsAt(vec2 center, Vec2Array positions);
    void draw(vec2 center, mat4 transform);
    bool configure();
}
