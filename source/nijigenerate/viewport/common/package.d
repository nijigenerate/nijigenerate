/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.

    Author: Asahi Lina
*/
module nijigenerate.viewport.common;
import nijilive;

Vec3Array incCreateCircleBuffer(vec2 origin, vec2 radii, uint segments)
{
    Vec3Array lines;

    void addPoint(ulong i) {
        float theta = i * 2 * PI / segments;
        vec2 pt = origin + vec2(radii.x * sin(theta), radii.y * cos(theta));
        lines ~= vec3(pt.x, pt.y, 0);
    }
    foreach(i; 0..segments) {
        addPoint(i);
        addPoint(i + 1);
    }

    return lines;
}

Vec3Array incCreateRectBuffer(vec2 from, vec2 to) {
    return Vec3Array([
        vec3(from.x, from.y, 0),
        vec3(to.x, from.y, 0),
        vec3(to.x, from.y, 0),
        vec3(to.x, to.y, 0),
        vec3(to.x, to.y, 0),
        vec3(from.x, to.y, 0),
        vec3(from.x, to.y, 0),
        vec3(from.x, from.y, 0),
    ]);
}

Vec3Array incCreateLineBuffer(vec2 from, vec2 to) {
    return Vec3Array([
        vec3(from.x, from.y, 0),
        vec3(to.x, to.y, 0),
    ]);
}
