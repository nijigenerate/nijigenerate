/*
    Depth texture mesh renderer.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.renderer;

import bindbc.opengl;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.common.targetview;
import nijilive;
import std.algorithm : max, min, sort;
import std.array : array;
import std.math : ceil, cos, sin;

enum float DepthRenderPi = 3.14159265358979323846f;

struct DepthTargetRenderMesh {
    vec2[] positions;
    vec2[] uvs;
    ushort[] indices;
}

struct DepthTargetRenderLine {
    vec2 p0;
    vec2 p1;
}

struct DepthTargetRenderPoint {
    vec2 point;
    float size;
}

class DepthTargetRenderer {
    DepthTargetRenderMesh buildMesh(
        vec2[] vertices,
        const(float)[] depths,
        ushort[] indices,
        vec2 minPoint,
        vec2 maxPoint,
        float depthDisplayScale,
        ref DepthCamera3D depthCamera
    ) {
        DepthTargetRenderMesh mesh;
        if (vertices.length == 0 || vertices.length != depths.length) return mesh;

        mesh.positions.length = vertices.length;
        mesh.uvs.length = vertices.length;
        mesh.indices = indices.dup;

        auto size = maxPoint - minPoint;
        if (size.x == 0) size.x = 1;
        if (size.y == 0) size.y = 1;

        foreach (i, vertex; vertices) {
            mesh.positions[i] = projectDepthPoint(vertex, -depths[i] * depthDisplayScale, depthCamera);
            mesh.uvs[i] = vec2(
                (vertex.x - minPoint.x) / size.x,
                1.0f - ((vertex.y - minPoint.y) / size.y)
            );
        }
        return mesh;
    }

    DepthTargetRenderMesh buildMesh(DepthTargetView target, ref DepthCamera3D depthCamera) {
        DepthTargetRenderMesh mesh;
        if (target is null) return mesh;

        auto vertices = target.getVertices();
        auto depths = target.copyWorkingDepths();
        mesh = buildMesh(
            vertices,
            depths,
            target.getIndices(),
            target.boundsMin(),
            target.boundsMax(),
            target.depthDisplayScale(),
            depthCamera
        );
        target.projectedPoints = mesh.positions.dup;
        return mesh;
    }

    DepthTargetRenderLine[] buildGridLines(vec2[] vertices, vec2[] projected) {
        DepthTargetRenderLine[] lines;
        if (vertices.length == 0 || projected.length != vertices.length) return lines;

        float[] xs;
        float[] ys;
        foreach (vertex; vertices) {
            xs ~= vertex.x;
            ys ~= vertex.y;
        }
        xs = sortedUnique(xs);
        ys = sortedUnique(ys);
        if (xs.length < 2 || ys.length < 2 || xs.length * ys.length != vertices.length) return lines;

        ushort[ulong] lookup;
        foreach (i, vertex; vertices) {
            size_t xi;
            size_t yi;
            foreach (j, x; xs) if (x == vertex.x) { xi = j; break; }
            foreach (j, y; ys) if (y == vertex.y) { yi = j; break; }
            lookup[yi * xs.length + xi] = cast(ushort)i;
        }

        void appendLine(ulong key0, ulong key1) {
            auto p0 = key0 in lookup;
            auto p1 = key1 in lookup;
            if (p0 is null || p1 is null) return;
            if (*p0 >= projected.length || *p1 >= projected.length) return;
            lines ~= DepthTargetRenderLine(projected[*p0], projected[*p1]);
        }

        foreach (y; 0 .. ys.length) {
            foreach (x; 0 .. xs.length - 1) appendLine(y * xs.length + x, y * xs.length + x + 1);
        }
        foreach (x; 0 .. xs.length) {
            foreach (y; 0 .. ys.length - 1) appendLine(y * xs.length + x, (y + 1) * xs.length + x);
        }
        return lines;
    }

    DepthTargetRenderLine[] buildGridLines(DepthTargetView target, ref DepthCamera3D depthCamera) {
        if (target is null) return null;
        auto mesh = buildMesh(target, depthCamera);
        return buildGridLines(target.getVertices(), mesh.positions);
    }

    DepthTargetRenderLine buildLine(
        vec2 p0,
        vec2 p1,
        float depth,
        float depthDisplayScale,
        ref DepthCamera3D depthCamera
    ) {
        return DepthTargetRenderLine(
            projectDepthPoint(p0, -depth * depthDisplayScale, depthCamera),
            projectDepthPoint(p1, -depth * depthDisplayScale, depthCamera)
        );
    }

    DepthTargetRenderLine buildDepthLine(
        vec2 p0,
        float depth0,
        vec2 p1,
        float depth1,
        float depthDisplayScale,
        ref DepthCamera3D depthCamera
    ) {
        return DepthTargetRenderLine(
            projectDepthPoint(p0, -depth0 * depthDisplayScale, depthCamera),
            projectDepthPoint(p1, -depth1 * depthDisplayScale, depthCamera)
        );
    }

    DepthTargetRenderPoint buildPoint(
        vec2 point,
        float depth,
        float depthDisplayScale,
        ref DepthCamera3D depthCamera,
        float size = 1.0f
    ) {
        return DepthTargetRenderPoint(
            projectDepthPoint(point, -depth * depthDisplayScale, depthCamera),
            size
        );
    }

    DepthTargetRenderLine[] buildEllipseLines(
        vec2 center,
        float radiusX,
        float radiusY,
        float angleDeg,
        float depth,
        float depthDisplayScale,
        ref DepthCamera3D depthCamera,
        int segments = 48
    ) {
        DepthTargetRenderLine[] lines;
        if (segments <= 0) return lines;
        auto angle = angleDeg * DepthRenderPi / 180.0f;
        auto ux = vec2(cos(angle), sin(angle));
        auto uy = vec2(-sin(angle), cos(angle));
        foreach (i; 0 .. segments) {
            auto a0 = cast(float)i / cast(float)segments * 2.0f * DepthRenderPi;
            auto a1 = cast(float)(i + 1) / cast(float)segments * 2.0f * DepthRenderPi;
            auto l0 = center + ux * (cos(a0) * radiusX) + uy * (sin(a0) * radiusY);
            auto l1 = center + ux * (cos(a1) * radiusX) + uy * (sin(a1) * radiusY);
            lines ~= buildLine(l0, l1, depth, depthDisplayScale, depthCamera);
        }
        return lines;
    }

    DepthTargetRenderLine[] buildPlaneLines(
        vec2 minPoint,
        vec2 maxPoint,
        float depth,
        float depthDisplayScale,
        ref DepthCamera3D depthCamera
    ) {
        return [
            buildLine(vec2(minPoint.x, minPoint.y), vec2(maxPoint.x, minPoint.y), depth, depthDisplayScale, depthCamera),
            buildLine(vec2(maxPoint.x, minPoint.y), vec2(maxPoint.x, maxPoint.y), depth, depthDisplayScale, depthCamera),
            buildLine(vec2(maxPoint.x, maxPoint.y), vec2(minPoint.x, maxPoint.y), depth, depthDisplayScale, depthCamera),
            buildLine(vec2(minPoint.x, maxPoint.y), vec2(minPoint.x, minPoint.y), depth, depthDisplayScale, depthCamera),
        ];
    }

    DepthTargetRenderLine[] buildRangeLines(
        vec2 minPoint,
        vec2 maxPoint,
        float minDepth,
        float maxDepth,
        float depthDisplayScale,
        ref DepthCamera3D depthCamera
    ) {
        auto centerX = (minPoint.x + maxPoint.x) * 0.5f;
        return [
            buildLine(vec2(minPoint.x, minPoint.y), vec2(maxPoint.x, minPoint.y), minDepth, depthDisplayScale, depthCamera),
            buildLine(vec2(minPoint.x, maxPoint.y), vec2(maxPoint.x, maxPoint.y), maxDepth, depthDisplayScale, depthCamera),
            buildLine(vec2(centerX, minPoint.y), vec2(centerX, maxPoint.y), minDepth, depthDisplayScale, depthCamera),
            buildLine(vec2(centerX, minPoint.y), vec2(centerX, maxPoint.y), maxDepth, depthDisplayScale, depthCamera),
        ];
    }

private:
    float[] sortedUnique(float[] values) {
        sort(values);
        float[] result;
        foreach (value; values) {
            if (result.length == 0 || result[$ - 1] != value) result ~= value;
        }
        return result;
    }
}

class DepthTextureMeshRenderer {
private:
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
    Shader shader;
    Shader solidShader;
    int mvpUniform = -1;
    int textureUniform = -1;
    int solidMvpUniform = -1;
    int solidColorUniform = -1;
    bool loggedEnsure;
    bool loggedDrawSuccess;
    bool loggedDrawSkip;
    bool loggedGlError;

    void ensureBuffers() {
        if (vao != 0) return;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);
    }

    void ensure() {
        ensureBuffers();
        if (shader !is null) return;
        shader = new Shader(
            q{
#version 330
layout(location = 0) in vec2 vert;
layout(location = 1) in vec2 uv;
out vec2 fragUv;
uniform mat4 mvp;
void main() {
    fragUv = uv;
    gl_Position = mvp * vec4(vert.xy, 0.0, 1.0);
}
},
            q{
#version 330
in vec2 fragUv;
out vec4 color;
uniform sampler2D tex;
void main() {
    color = texture(tex, fragUv);
}
}
        );
        mvpUniform = shader.getUniformLocation("mvp");
        textureUniform = shader.getUniformLocation("tex");
        loggedEnsure = true;
    }

    void ensureSolid() {
        ensureBuffers();
        if (solidShader !is null) return;
        solidShader = new Shader(
            q{
#version 330
layout(location = 0) in vec2 vert;
uniform mat4 mvp;
void main() {
    gl_Position = mvp * vec4(vert.xy, 0.0, 1.0);
}
},
            q{
#version 330
out vec4 color;
uniform vec4 meshColor;
void main() {
    color = meshColor;
}
}
        );
        solidMvpUniform = solidShader.getUniformLocation("mvp");
        solidColorUniform = solidShader.getUniformLocation("meshColor");
    }

public:
    void drawSolid(vec2[] positions, ushort[] indices, Camera viewportCamera, vec4 color) {
        if (positions.length == 0 || indices.length == 0) {
            loggedDrawSkip = true;
            return;
        }

        ensureSolid();

        GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean blendEnabled = glIsEnabled(GL_BLEND);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, positions.length * vec2.sizeof, positions.ptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.length * ushort.sizeof, indices.ptr, GL_DYNAMIC_DRAW);

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        inSetBlendMode(BlendMode.Normal);

        solidShader.use();
        solidShader.setUniform(solidMvpUniform, viewportCamera.matrix());
        solidShader.setUniform(solidColorUniform, color);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, false, vec2.sizeof, null);
        glDrawElements(GL_TRIANGLES, cast(int)indices.length, GL_UNSIGNED_SHORT, null);
        auto glError = glGetError();
        if (glError != GL_NO_ERROR && !loggedGlError) {
            loggedGlError = true;
        } else if (glError == GL_NO_ERROR && !loggedDrawSuccess) {
            loggedDrawSuccess = true;
        }
        glDisableVertexAttribArray(0);

        if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        glBindVertexArray(0);
    }

    void draw(Texture texture, vec2[] positions, vec2[] uvs, ushort[] indices, Camera viewportCamera) {
        if (texture is null || positions.length == 0 || positions.length != uvs.length || indices.length == 0) {
            loggedDrawSkip = true;
            return;
        }

        ensure();

        GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean blendEnabled = glIsEnabled(GL_BLEND);

        float[] vertexData;
        vertexData.length = positions.length * 4;
        foreach (i, position; positions) {
            vertexData[i * 4 + 0] = position.x;
            vertexData[i * 4 + 1] = position.y;
            vertexData[i * 4 + 2] = uvs[i].x;
            vertexData[i * 4 + 3] = uvs[i].y;
        }

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexData.length * float.sizeof, vertexData.ptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.length * ushort.sizeof, indices.ptr, GL_DYNAMIC_DRAW);

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        inSetBlendMode(BlendMode.Normal);

        shader.use();
        shader.setUniform(mvpUniform, viewportCamera.matrix());
        shader.setUniform(textureUniform, 0);
        texture.bind(0);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 2, GL_FLOAT, false, float.sizeof * 4, null);
        glVertexAttribPointer(1, 2, GL_FLOAT, false, float.sizeof * 4, cast(void*)(float.sizeof * 2));
        glDrawElements(GL_TRIANGLES, cast(int)indices.length, GL_UNSIGNED_SHORT, null);
        auto glError = glGetError();
        if (glError != GL_NO_ERROR && !loggedGlError) {
            loggedGlError = true;
        } else if (glError == GL_NO_ERROR && !loggedDrawSuccess) {
            loggedDrawSuccess = true;
        }
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);

        if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        glBindVertexArray(0);
    }
}

class DepthTargetOffscreenTextureRenderer {
private:
    Texture texture;
    GLuint textureFbo;
    int textureWidth;
    int textureHeight;
    vec2 minPoint = vec2(0);
    vec2 maxPoint = vec2(1);

    bool rebuildTexture(Deformable target) {
        auto verts = target.vertices.toArray();
        if (verts.length == 0) return false;

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
        return texture !is null && textureFbo != 0;
    }

public:
    static Node[] drawableChildren(Deformable target) {
        Node[] subParts;
        if (target is null) return subParts;

        void findSubDrawable(Node n) {
            if (n.coverOthers()) {
                foreach (child; n.children) findSubDrawable(child);
            }
            if (auto c = cast(Composite)n) {
                if (c.propagateMeshGroup) subParts ~= c;
            } else if (auto d = cast(Drawable)n) {
                subParts ~= d;
                foreach (child; n.children) findSubDrawable(child);
            }
        }

        findSubDrawable(target);
        sort!((a, b) => a.zSort > b.zSort)(subParts);
        return subParts;
    }

    ~this() {
        dispose();
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

    Texture render(Deformable target) {
        if (target is null || !rebuildTexture(target)) return null;

        GLint prevDrawFbo;
        GLint prevReadFbo;
        GLint[4] prevViewport;
        GLboolean prevDepthEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean prevCullEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean prevBlendEnabled = glIsEnabled(GL_BLEND);
        GLint prevBlendEquationRgb;
        GLint prevBlendEquationAlpha;
        GLint prevBlendSrcRgb;
        GLint prevBlendDstRgb;
        GLint prevBlendSrcAlpha;
        GLint prevBlendDstAlpha;
        GLfloat[4] prevClearColor;
        GLint maxDrawBuffers;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
        glGetIntegerv(GL_VIEWPORT, prevViewport.ptr);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &prevBlendEquationRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &prevBlendEquationAlpha);
        glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, prevClearColor.ptr);
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        GLenum[] prevDrawBuffers;
        prevDrawBuffers.length = maxDrawBuffers > 0 ? cast(size_t)maxDrawBuffers : 1;
        foreach (i; 0 .. prevDrawBuffers.length) {
            GLint drawBuffer;
            glGetIntegerv(cast(GLenum)(GL_DRAW_BUFFER0 + i), &drawBuffer);
            prevDrawBuffers[i] = cast(GLenum)drawBuffer;
        }
        while (prevDrawBuffers.length > 1 && prevDrawBuffers[$ - 1] == GL_NONE) {
            prevDrawBuffers.length--;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, textureFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.getTextureId(), 0);
        glDrawBuffers(1, [GL_COLOR_ATTACHMENT0].ptr);
        auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cast(GLuint)prevDrawFbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, cast(GLuint)prevReadFbo);
            glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            return null;
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
            if (prevDepthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (prevCullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            if (prevBlendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            glBlendEquationSeparate(cast(GLenum)prevBlendEquationRgb, cast(GLenum)prevBlendEquationAlpha);
            glBlendFuncSeparate(
                cast(GLenum)prevBlendSrcRgb,
                cast(GLenum)prevBlendDstRgb,
                cast(GLenum)prevBlendSrcAlpha,
                cast(GLenum)prevBlendDstAlpha
            );
            glClearColor(prevClearColor[0], prevClearColor[1], prevClearColor[2], prevClearColor[3]);
            glDrawBuffers(cast(GLsizei)prevDrawBuffers.length, prevDrawBuffers.ptr);
        }

        foreach (part; drawableChildren(target)) {
            part.drawOne();
        }
        return texture;
    }
}
