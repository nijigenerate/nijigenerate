/*
    Depth mesh editor target state.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.mesheditor.node;

import bindbc.opengl;
import nijigenerate;
import nijigenerate.actions;
import nijigenerate.core.actionstack;
import nijigenerate.core.dbg;
import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.renderer;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import std.algorithm : clamp, max, min, sort, uniq;
import std.array : array;
import std.format : format;
import std.math : abs, ceil, cmp, round;
import std.stdio : writefln;

enum DepthDisplayPlaneSize = 2.9f;
enum DepthDisplayZScale = 0.42f;

class DepthMeshEditorOne {
private:
    GridDeformer target;
    DepthMappedNode depthMapped;
    Texture texture;
    GLuint textureFbo;
    int textureWidth;
    int textureHeight;
    vec2 minPoint = vec2(0);
    vec2 maxPoint = vec2(1);
    ushort[] indices;
    package(nijigenerate.viewport.depth) vec2[] projectedPoints;
    ptrdiff_t selectedVertex = -1;
    bool loggedEmptyVertices;
    bool loggedInvalidTopology;
    bool loggedTextureUnavailable;
    bool loggedFramebufferIncomplete;
    bool loggedDepthLengthMismatch;
    bool loggedFirstDraw;
    bool loggedOffscreenDraw;

    void log(string message) {
        writefln("[DepthEdit] %s: %s", target is null ? "(null)" : target.name, message);
    }

    float roundDepth(float value) {
        return cast(float)(round(value * 1000.0f) / 1000.0f);
    }

    float[] sortedUnique(float[] values) {
        sort(values);
        return values.uniq.array;
    }

    void rebuildTopology() {
        indices.length = 0;
        auto verts = target.vertices.toArray();
        if (verts.length == 0) {
            if (!loggedEmptyVertices) {
                log("topology skipped: GridDeformer has no vertices");
                loggedEmptyVertices = true;
            }
            return;
        }

        float[] xs;
        float[] ys;
        foreach (v; verts) {
            xs ~= v.x;
            ys ~= v.y;
        }
        xs = sortedUnique(xs);
        ys = sortedUnique(ys);
        if (xs.length < 2 || ys.length < 2 || xs.length * ys.length != verts.length) {
            if (!loggedInvalidTopology) {
                log("topology skipped: invalid grid vertices vertices=%s xs=%s ys=%s xs*ys=%s".format(
                    verts.length, xs.length, ys.length, xs.length * ys.length));
                loggedInvalidTopology = true;
            }
            return;
        }

        ushort[ulong] lookup;
        foreach (i, v; verts) {
            size_t xi;
            size_t yi;
            foreach (j, x; xs) if (x == v.x) { xi = j; break; }
            foreach (j, y; ys) if (y == v.y) { yi = j; break; }
            lookup[yi * xs.length + xi] = cast(ushort)i;
        }

        foreach (y; 0 .. ys.length - 1) {
            foreach (x; 0 .. xs.length - 1) {
                auto k0 = y * xs.length + x;
                auto k1 = y * xs.length + x + 1;
                auto k2 = (y + 1) * xs.length + x;
                auto k3 = (y + 1) * xs.length + x + 1;
                auto p0 = k0 in lookup;
                auto p1 = k1 in lookup;
                auto p2 = k2 in lookup;
                auto p3 = k3 in lookup;
                if (p0 is null || p1 is null || p2 is null || p3 is null) {
                    if (!loggedInvalidTopology) {
                        log("topology skipped cell: missing grid vertex at x=%s y=%s".format(x, y));
                        loggedInvalidTopology = true;
                    }
                    continue;
                }
                auto i0 = *p0;
                auto i1 = *p1;
                auto i2 = *p2;
                auto i3 = *p3;
                indices ~= [i0, i1, i3, i0, i3, i2];
            }
        }
        log("topology ready: vertices=%s xs=%s ys=%s triangles=%s".format(
            verts.length, xs.length, ys.length, indices.length / 3));
    }

    void rebuildTexture() {
        auto verts = target.vertices.toArray();
        if (verts.length == 0) {
            if (!loggedEmptyVertices) {
                log("texture skipped: GridDeformer has no vertices");
                loggedEmptyVertices = true;
            }
            return;
        }

        minPoint = verts[0];
        maxPoint = verts[0];
        foreach (v; verts[1 .. $]) {
            minPoint.x = min(minPoint.x, v.x);
            minPoint.y = min(minPoint.y, v.y);
            maxPoint.x = max(maxPoint.x, v.x);
            maxPoint.y = max(maxPoint.y, v.y);
        }

        textureWidth = max(1, cast(int)ceil(maxPoint.x - minPoint.x));
        textureHeight = max(1, cast(int)ceil(maxPoint.y - minPoint.y));

        if (texture !is null && (texture.width != textureWidth || texture.height != textureHeight)) {
            texture.dispose();
            texture = null;
        }
        if (texture is null) {
            texture = new Texture(textureWidth, textureHeight, 4, false, false);
        }
        if (textureFbo == 0) {
            glGenFramebuffers(1, &textureFbo);
        }
        log("texture ready: size=%sx%s fbo=%s".format(textureWidth, textureHeight, textureFbo));
    }

    Node[] vertexModeDrawableChildren() {
        Node[] subParts;

        void findSubDrawable(Node n) {
            if (n.coverOthers()) {
                foreach (child; n.children)
                    findSubDrawable(child);
            }
            if (auto c = cast(Composite)n) {
                if (c.propagateMeshGroup) {
                    subParts ~= c;
                }
            } else if (auto d = cast(Drawable)n) {
                subParts ~= d;
                foreach (child; n.children)
                    findSubDrawable(child);
            }
        }

        findSubDrawable(target);

        import std.algorithm.sorting : sort;
        import std.algorithm.mutation : SwapStrategy;
        sort!((a, b) => cmp(a.zSort, b.zSort) > 0, SwapStrategy.stable)(subParts);
        return subParts;
    }

    void renderTextureLikeVertexMode() {
        if (texture is null || textureFbo == 0) {
            if (!loggedTextureUnavailable) {
                log("offscreen skipped: texture or FBO is unavailable texture=%s fbo=%s".format(texture !is null, textureFbo));
                loggedTextureUnavailable = true;
            }
            return;
        }

        GLint prevDrawFbo;
        GLint prevReadFbo;
        GLint[4] prevViewport;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
        glGetIntegerv(GL_VIEWPORT, prevViewport.ptr);

        glBindFramebuffer(GL_FRAMEBUFFER, textureFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.getTextureId(), 0);
        glDrawBuffers(1, [GL_COLOR_ATTACHMENT0].ptr);
        auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
            if (!loggedFramebufferIncomplete) {
                log("offscreen skipped: framebuffer incomplete status=%s textureId=%s size=%sx%s".format(
                    fboStatus, texture.getTextureId(), textureWidth, textureHeight));
                loggedFramebufferIncomplete = true;
            }
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cast(GLuint)prevDrawFbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, cast(GLuint)prevReadFbo);
            glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            return;
        }
        glViewport(0, 0, textureWidth, textureHeight);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        inPushViewport(textureWidth, textureHeight);
        auto offscreenCamera = inGetCamera();
        offscreenCamera.scale = vec2(1, 1);
        offscreenCamera.position = vec2(
            -minPoint.x - cast(float)textureWidth * 0.5f,
            -minPoint.y - cast(float)textureHeight * 0.5f
        );
        offscreenCamera.rotation = 0;

        mat4 transform = target.transform.matrix.inverse;
        target.setOneTimeTransform(&transform);
        scope(exit) {
            target.setOneTimeTransform(null);
            inPopViewport();
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cast(GLuint)prevDrawFbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, cast(GLuint)prevReadFbo);
            glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            if (prevDrawFbo != 0) {
                glDrawBuffers(3, [GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2].ptr);
            }
        }

        auto parts = vertexModeDrawableChildren();
        if (!loggedOffscreenDraw) {
            log("offscreen draw: children=%s textureId=%s viewport=%sx%s".format(
                parts.length, texture.getTextureId(), textureWidth, textureHeight));
            loggedOffscreenDraw = true;
        }
        foreach (part; parts) {
            part.drawOne();
        }
    }

public:
    float[] depths;
    float[] baseDepths;

    this(GridDeformer target) {
        this.target = target;
        this.depthMapped = cast(DepthMappedNode)target;
        log("editor created: type=%s depthMapped=%s vertices=%s deformation=%s".format(
            typeid(target).toString(), depthMapped !is null, target.vertices.length, target.deformation.length));
        resetFromTarget();
        rebuildTopology();
        rebuildTexture();
    }

    ~this() {
        dispose();
    }

    GridDeformer getTarget() {
        return target;
    }

    vec2[] getVertices() {
        return target.vertices.toArray();
    }

    vec2 localVertex(size_t index) {
        auto vertices = getVertices();
        return index < vertices.length ? vertices[index] : vec2(0);
    }

    vec2 snapLocalPoint(vec2 point) {
        auto vertices = getVertices();
        if (vertices.length == 0) return point;
        auto best = vertices[0];
        auto bestDistance = (point - best).length();
        foreach (v; vertices[1 .. $]) {
            auto distance = (point - v).length();
            if (distance < bestDistance) {
                best = v;
                bestDistance = distance;
            }
        }
        return best;
    }

    ptrdiff_t nearestLocalVertexIndex(vec2 point) {
        auto vertices = getVertices();
        if (vertices.length == 0) return -1;
        ptrdiff_t best = 0;
        auto bestDistance = (point - vertices[0]).length();
        foreach (i, v; vertices[1 .. $]) {
            auto distance = (point - v).length();
            if (distance < bestDistance) {
                best = cast(ptrdiff_t)i + 1;
                bestDistance = distance;
            }
        }
        return best;
    }

    vec2 projectedVertex(size_t index) {
        return index < projectedPoints.length ? projectedPoints[index] : vec2(0);
    }

    float depthDisplayScale() {
        auto size = maxPoint - minPoint;
        return max(1.0f, max(size.x, size.y) * (DepthDisplayZScale / DepthDisplayPlaneSize));
    }

    vec2 depthViewToModel(vec2 point, ref DepthCamera3D depthCamera, float depth = 0.0f) {
        return unprojectDepthPoint(point, -depth * depthDisplayScale(), depthCamera);
    }

    vec2 modelToDepthView(vec2 point, float depth, ref DepthCamera3D depthCamera) {
        return projectDepthPoint(point, -depth * depthDisplayScale(), depthCamera);
    }

    vec2 localToWorld(vec2 point) {
        return point;
    }

    vec2 projectLocalPoint(vec2 point, float depth, ref DepthCamera3D depthCamera) {
        return modelToDepthView(point, depth, depthCamera);
    }

    vec2 displayWorldToLocal(vec2 point, ref DepthCamera3D depthCamera, float depth = 0.0f) {
        return depthViewToModel(point, depthCamera, depth);
    }

    vec2 worldToLocal(vec2 point) {
        return point;
    }

    float[] copyEditorDepths() {
        return depths.dup;
    }

    Node targetNode() {
        return cast(Node)target;
    }

    void replaceEditorDepths(float[] values) {
        depths = values.dup;
        if (depths.length != target.vertices.length) {
            depths.length = target.vertices.length;
        }
    }

    void resetWorkingDepths() {
        replaceEditorDepths(baseDepths);
    }

    void clearBaseDepths() {
        baseDepths.length = target.vertices.length;
        baseDepths[] = 0;
        replaceEditorDepths(baseDepths);
    }

    void dispose() {
        if (texture !is null) {
            texture.dispose();
            texture = null;
        }
        if (textureFbo != 0) {
            glDeleteFramebuffers(1, &textureFbo);
            textureFbo = 0;
        }
    }

    void resetFromTarget() {
        depths = depthMapped !is null ? depthMapped.copyDepths() : null;
        if (depths is null || depths.length != target.vertices.length) {
            log("depths initialized: sourceLength=%s targetVertices=%s".format(
                depths is null ? -1 : cast(long)depths.length, target.vertices.length));
            depths.length = target.vertices.length;
            depths[] = 0;
        } else {
            log("depths loaded: length=%s".format(depths.length));
        }
        baseDepths = depths.dup;
    }

    void applyToTarget() {
        if (depthMapped is null) {
            log("apply skipped: target is not DepthMappedNode");
            return;
        }
        size_t nonZero;
        float minDepth = depths.length ? depths[0] : 0;
        float maxDepth = depths.length ? depths[0] : 0;
        ptrdiff_t firstNonZero = -1;
        foreach (i, value; depths) {
            if (value < minDepth) minDepth = value;
            if (value > maxDepth) maxDepth = value;
            if (abs(value) > 0.000001f) {
                nonZero++;
                if (firstNonZero < 0) firstNonZero = cast(ptrdiff_t)i;
            }
        }
        log("apply depths: length=%s vertices=%s nonZero=%s min=%s max=%s firstNonZero=%s".format(
            depths.length, target.vertices.length, nonZero, minDepth, maxDepth, firstNonZero));
        auto action = new DepthMappedChangeAction(target);
        depthMapped.replaceDepths(depths);
        auto saved = depthMapped.copyDepths();
        log("apply replaceDepths done: savedLength=%s".format(saved is null ? -1 : cast(long)saved.length));
        action.updateNewState();
        incActionPush(action);
        target.notifyChange(target, NotifyReason.AttributeChanged);
    }

    float getDepth(size_t index) {
        return index < depths.length ? depths[index] : 0;
    }

    float depthAtLocalPoint(vec2 point) {
        auto vertices = getVertices();
        if (vertices.length == 0 || depths.length == 0) return 0;
        size_t bestIndex;
        auto bestDistance = (point - vertices[0]).length();
        foreach (i, v; vertices[1 .. $]) {
            auto distance = (point - v).length();
            if (distance < bestDistance) {
                bestIndex = i + 1;
                bestDistance = distance;
            }
        }
        return bestIndex < depths.length ? depths[bestIndex] : 0;
    }

    void setDepth(size_t index, float value) {
        if (index >= depths.length) return;
        depths[index] = roundDepth(clamp(value, -2.0f, 2.0f));
    }

    void addDepth(size_t index, float value) {
        setDepth(index, getDepth(index) + value);
    }

    ptrdiff_t nearestProjectedVertex(vec2 point, float radius) {
        ptrdiff_t best = -1;
        float bestDistance = radius;
        foreach (i, projected; projectedPoints) {
            auto distance = (projected - point).length();
            if (distance < bestDistance) {
                best = cast(ptrdiff_t)i;
                bestDistance = distance;
            }
        }
        return best;
    }

    void selectVertex(ptrdiff_t index) {
        selectedVertex = index;
    }

    void draw(Camera viewportCamera, ref DepthCamera3D depthCamera, DepthTextureMeshRenderer renderer) {
        auto vertices = target.vertices.toArray();
        if (!loggedFirstDraw) {
            log("draw enter: vertices=%s depths=%s indices=%s texture=%s fbo=%s cameraScale=%s cameraPos=%s".format(
                vertices.length,
                depths.length,
                indices.length,
                texture !is null,
                textureFbo,
                viewportCamera.scale,
                viewportCamera.position));
            loggedFirstDraw = true;
        }
        if (vertices.length == 0) {
            if (!loggedEmptyVertices) {
                log("draw skipped: GridDeformer has no vertices");
                loggedEmptyVertices = true;
            }
            return;
        }
        if (vertices.length != depths.length) {
            if (!loggedDepthLengthMismatch) {
                log("draw skipped: depths length mismatch vertices=%s depths=%s".format(vertices.length, depths.length));
                loggedDepthLengthMismatch = true;
            }
            return;
        }
        if (indices.length == 0 && !loggedInvalidTopology) {
            log("draw skipped texture mesh: no indices");
            loggedInvalidTopology = true;
        }
        renderTextureLikeVertexMode();

        vec2[] projected;
        vec2[] uvs;
        Vec3Array points;
        projected.length = vertices.length;
        uvs.length = vertices.length;
        points.length = vertices.length;

        vec2 size = maxPoint - minPoint;
        if (size.x == 0) size.x = 1;
        if (size.y == 0) size.y = 1;

        foreach (i, v; vertices) {
            vec2 projectedLocal = modelToDepthView(v, depths[i], depthCamera);
            projected[i] = projectedLocal;
            uvs[i] = vec2(
                (v.x - minPoint.x) / size.x,
                1.0f - ((v.y - minPoint.y) / size.y)
            );
            points[i] = vec3(projected[i].x, projected[i].y, 0);
        }
        projectedPoints = projected.dup;

        renderer.draw(texture, projected, uvs, indices, viewportCamera);

        Vec3Array gridLines;
        float[] xs;
        float[] ys;
        foreach (v; vertices) {
            xs ~= v.x;
            ys ~= v.y;
        }
        xs = sortedUnique(xs);
        ys = sortedUnique(ys);
        if (xs.length >= 2 && ys.length >= 2 && xs.length * ys.length == vertices.length) {
            ushort[ulong] lookup;
            foreach (i, v; vertices) {
                size_t xi;
                size_t yi;
                foreach (j, x; xs) if (x == v.x) { xi = j; break; }
                foreach (j, y; ys) if (y == v.y) { yi = j; break; }
                lookup[yi * xs.length + xi] = cast(ushort)i;
            }

            void appendGridLine(ulong key0, ulong key1) {
                auto p0 = key0 in lookup;
                auto p1 = key1 in lookup;
                if (p0 is null || p1 is null) return;
                auto i0 = *p0;
                auto i1 = *p1;
                if (i0 < points.length && i1 < points.length) {
                    gridLines ~= points[i0];
                    gridLines ~= points[i1];
                }
            }

            foreach (y; 0 .. ys.length) {
                foreach (x; 0 .. xs.length - 1) {
                    appendGridLine(y * xs.length + x, y * xs.length + x + 1);
                }
            }
            foreach (x; 0 .. xs.length) {
                foreach (y; 0 .. ys.length - 1) {
                    appendGridLine(y * xs.length + x, (y + 1) * xs.length + x);
                }
            }
        }
        if (gridLines.length > 0) {
            inDbgSetBuffer(gridLines);
            inDbgDrawLines(vec4(0.55, 0.55, 0.55, 0.7));
        }
    }
}
