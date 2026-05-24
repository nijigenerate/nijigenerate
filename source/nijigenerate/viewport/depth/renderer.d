/*
    Depth texture mesh renderer.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.renderer;

import bindbc.opengl;
import nijilive;

class DepthTextureMeshRenderer {
private:
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
    Shader shader;
    int mvpUniform = -1;
    int textureUniform = -1;
    bool loggedEnsure;
    bool loggedDrawSuccess;
    bool loggedDrawSkip;
    bool loggedGlError;

    void ensure() {
        if (shader !is null) return;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);

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

public:
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
