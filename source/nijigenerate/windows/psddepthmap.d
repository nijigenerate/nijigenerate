module nijigenerate.windows.psddepthmap;

import bindbc.opengl;
import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.commands;
import nijigenerate.commands.depth.map : PsdDepthComposedView, ngApplyPsdDepthImportResult,
    ngComposePsdDepthImportResult, ngComposePsdDepthTarget;
import nijigenerate.ext.nodes.exdepthmapped : DepthMappedNode;
import nijigenerate.ext.nodes.expart;
import nijigenerate.io : TFD_Filter, incShowImportDialog;
import nijigenerate.io.depthimage : DepthDrawAlphaDepthFocusedRule,
    ngDepthDrawDecodeGrayscaleDepthPixelsFromRgba, ngDepthDrawDetectAlphaDepthGaps,
    ngDepthDrawMedianFillDepth, ngDepthDrawSmoothGridDepthValues;
import nijigenerate.io.depthmap_psd;
import nijigenerate.io.depthsample : DepthSampleAggregate, DepthSampleChannel, DepthSampleConvolution,
    DepthSamplePoint, ngDepthSampleConvolve, ngDepthSampleMissingPoint, ngDepthSamplePixelDepth,
    ngDepthSampleValueToDepth01;
import nijigenerate.viewport.depth.camera : DepthCamera3D, projectDepthPoint, updateDepthCamera3D;
import nijigenerate.viewport.depth.common.targetview : DepthTargetView, ngDepthDisplayScaleForDocument,
    ngDepthDisplayScaleForTargetsInNodeSpace;
import nijigenerate.windows.base;
import nijigenerate.widgets;
import nijilive;
import nijilive.core.nodes.deformable : Deformable;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.nodes.deformer.path : PathDeformer;
import std.algorithm.comparison : max, min;
import std.algorithm : clamp;
import std.conv : to;
import std.exception : collectException;
import std.format : format;
import std.math : abs, round;
import std.string : join, toLower, toStringz;

struct PsdDepth3DAdjustGeometryStats {
    size_t layerPlanes;
    size_t depthRangeLines;
    size_t targetWireLines;
    size_t sampledPoints;
    size_t missingPoints;
}

private struct PsdDepthLayerEdit {
    float zOffset;
    float zScale = 1.0f;
    bool invert;
}

struct PsdDepth3DAdjustSample {
    int x;
    int y;
    ubyte depthByte;
    float depth;
    ubyte r;
    ubyte g;
    ubyte b;
    ubyte a;
}

struct PsdDepth3DAdjustMesh {
    float[] vertexData;
    uint[] indices;
}

private final class PsdDepth3DAdjustGpuRenderer {
private:
    Texture texture;
    GLuint fbo;
    GLuint depthBuffer;
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
    Shader shader;
    int viewportSizeUniform = -1;
    int centerUniform = -1;
    int yawPitchUniform = -1;
    int zoomPanUniform = -1;
    int depthScaleUniform = -1;
    int textureUniform = -1;
    int width;
    int height;

    void ensureShader() {
        if (shader !is null) return;
        shader = new Shader(
            q{
#version 330
layout(location = 0) in vec3 documentPosition;
layout(location = 1) in vec2 uv;
out vec2 fragUv;
uniform vec2 viewportSize;
uniform vec2 center;
uniform vec4 yawPitch;
uniform vec4 zoomPan;
uniform float depthDisplayScale;
void main() {
    float cy = yawPitch.x;
    float sy = yawPitch.y;
    float cp = yawPitch.z;
    float sp = yawPitch.w;
    float x = documentPosition.x - center.x;
    float y = documentPosition.y - center.y;
    float z = -documentPosition.z * depthDisplayScale;
    float rx = x * cy + z * sy;
    float rz = -x * sy + z * cy;
    float ry = y * cp - rz * sp;
    float cameraDepth = y * sp + rz * cp;
    vec2 screen = viewportSize * 0.5 + vec2(rx, ry) * zoomPan.x + zoomPan.yz;
    vec2 clip = vec2(screen.x / viewportSize.x * 2.0 - 1.0, 1.0 - screen.y / viewportSize.y * 2.0);
    float zClip = clamp(cameraDepth / max(depthDisplayScale * 2.0, 1.0), -1.0, 1.0);
    gl_Position = vec4(clip, zClip, 1.0);
    fragUv = uv;
}
},
            q{
#version 330
in vec2 fragUv;
out vec4 color;
uniform sampler2D tex;
void main() {
    color = texture(tex, fragUv);
    if (color.a < 0.01) discard;
}
}
        );
        viewportSizeUniform = shader.getUniformLocation("viewportSize");
        centerUniform = shader.getUniformLocation("center");
        yawPitchUniform = shader.getUniformLocation("yawPitch");
        zoomPanUniform = shader.getUniformLocation("zoomPan");
        depthScaleUniform = shader.getUniformLocation("depthDisplayScale");
        textureUniform = shader.getUniformLocation("tex");
    }

    void ensureBuffers() {
        if (vao == 0) glGenVertexArrays(1, &vao);
        if (vbo == 0) glGenBuffers(1, &vbo);
        if (ibo == 0) glGenBuffers(1, &ibo);
    }

    bool ensureTarget(int targetWidth, int targetHeight) {
        targetWidth = max(1, targetWidth);
        targetHeight = max(1, targetHeight);
        if (texture !is null && (width != targetWidth || height != targetHeight)) {
            texture.dispose();
            texture = null;
            if (depthBuffer != 0) {
                glDeleteRenderbuffers(1, &depthBuffer);
                depthBuffer = 0;
            }
        }
        width = targetWidth;
        height = targetHeight;
        if (texture is null) texture = new Texture(width, height, 4, false, false);
        if (fbo == 0) glGenFramebuffers(1, &fbo);
        if (depthBuffer == 0) {
            glGenRenderbuffers(1, &depthBuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        return texture !is null && fbo != 0 && depthBuffer != 0;
    }

public:
    Texture render(
        int targetWidth,
        int targetHeight,
        scope void delegate() drawLayers
    ) {
        if (!ensureTarget(targetWidth, targetHeight)) return null;
        ensureShader();
        ensureBuffers();

        GLint previousDrawFbo;
        GLint previousReadFbo;
        GLint[4] previousViewport;
        GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean blendEnabled = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFbo);
        glGetIntegerv(GL_VIEWPORT, previousViewport.ptr);
        scope(exit) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cast(GLuint)previousDrawFbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, cast(GLuint)previousReadFbo);
            glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
            if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            glBindVertexArray(0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.getTextureId(), 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
        glDrawBuffers(1, [GL_COLOR_ATTACHMENT0].ptr);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return null;

        glViewport(0, 0, width, height);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        inSetBlendMode(BlendMode.Normal);
        glClearColor(0.08f, 0.09f, 0.10f, 1.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawLayers();
        return texture;
    }

    void drawLayer(
        Texture layerTexture,
        ref PsdDepth3DAdjustMesh mesh,
        float centerX,
        float centerY,
        float yaw,
        float pitch,
        float zoom,
        vec2 pan,
        float depthDisplayScale
    ) {
        if (layerTexture is null || mesh.vertexData.length == 0 || mesh.indices.length == 0) return;
        import std.math : cos, sin;

        shader.use();
        shader.setUniform(viewportSizeUniform, vec2(cast(float)width, cast(float)height));
        shader.setUniform(centerUniform, vec2(centerX, centerY));
        shader.setUniform(yawPitchUniform, vec4(cos(yaw), sin(yaw), cos(pitch), sin(pitch)));
        shader.setUniform(zoomPanUniform, vec4(zoom, pan.x, pan.y, 0.0f));
        shader.setUniform(depthScaleUniform, depthDisplayScale);
        shader.setUniform(textureUniform, 0);
        layerTexture.bind(0);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertexData.length * float.sizeof, mesh.vertexData.ptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.length * uint.sizeof, mesh.indices.ptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 3, GL_FLOAT, false, float.sizeof * 5, null);
        glVertexAttribPointer(1, 2, GL_FLOAT, false, float.sizeof * 5, cast(void*)(float.sizeof * 3));
        glDrawElements(GL_TRIANGLES, cast(GLsizei)mesh.indices.length, GL_UNSIGNED_INT, null);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }

    void dispose() {
        if (texture !is null) {
            texture.dispose();
            texture = null;
        }
        if (depthBuffer != 0) {
            glDeleteRenderbuffers(1, &depthBuffer);
            depthBuffer = 0;
        }
        if (fbo != 0) {
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
        if (vbo != 0) {
            glDeleteBuffers(1, &vbo);
            vbo = 0;
        }
        if (ibo != 0) {
            glDeleteBuffers(1, &ibo);
            ibo = 0;
        }
        if (vao != 0) {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
    }
}

class PSDDepthMapWindow : Window {
private:
    string path;
    PsdDepthImportSettings settings;
    PsdDepthImportResult preview;
    PsdDepthComposedView composedPreview;
    string errorMessage;
    string lastApplyErrorMessage;
    bool previewDirty = true;
    bool onlyProblemLayers;
    ptrdiff_t selectedGridIndex;
    ptrdiff_t selected3DAdjustLayerIndex;
    bool initial3DAdjustTabSelected;
    Texture[string] originalPreviewTextures;
    Texture[string] depthMaskPreviewTextures;
    Texture[string] rawCompositePreviewTextures;
    PsdDepth3DAdjustSample[][string] threeDAdjustSamples;
    PsdDepth3DAdjustMesh[string] threeDAdjustMeshes;
    bool threeDAdjustSamplesPrepared;
    PsdDepth3DAdjustGpuRenderer threeDAdjustGpuRenderer;
    Texture threeDAdjustPreviewTexture;
    int threeDAdjustPreviewWidth;
    int threeDAdjustPreviewHeight;
    bool threeDAdjustPreviewDirty = true;
    DepthCamera3D threeDAdjustCamera;
    ulong threeDAdjustCameraTargetUuid;
    int threeDAdjustCameraLeft = int.min;
    int threeDAdjustCameraTop = int.min;
    int threeDAdjustCameraRight = int.min;
    int threeDAdjustCameraBottom = int.min;
    float threeDAdjustLastYaw;
    float threeDAdjustLastPitch;
    float threeDAdjustLastZoom;
    vec2 threeDAdjustLastPan;

    enum PreviewSize = 160f;
    enum PsdDepth3DAdjustMeshStep = 2;
    enum PsdDepth3DAdjustDepthScale = 0.22f;

    enum string[] ConvolutionNames = [
        "Nearest",
        "Box3x3",
        "Box5x5",
        "Gaussian3x3",
        "Gaussian5x5",
        "Median3x3",
        "Frontmost3x3",
        "Backmost3x3",
        "BoxCustom",
        "GaussianCustom",
        "MedianCustom",
        "FrontmostCustom",
        "BackmostCustom",
    ];

    enum string[] ChannelNames = [
        "AverageRGB",
        "R",
        "G",
        "B",
        "Luminance",
    ];

    enum string[] MissingPolicyNames = [
        "KeepExisting",
        "SetZero",
        "SetBack",
        "SkipGrid",
    ];

    void rebuildPreview() {
        disposePreviewTextures();
        auto previous = preview;
        preview = PsdDepthImportResult.init;
        composedPreview = PsdDepthComposedView.init;
        errorMessage = null;
        lastApplyErrorMessage = null;
        auto puppet = incActivePuppet();
        if (puppet is null) {
            errorMessage = _("No active puppet");
            previewDirty = false;
            return;
        }
        auto ex = collectException(preview = ngBuildPsdDepthsFromSource(puppet, path, settings));
        if (ex !is null) {
            errorMessage = ex.msg;
        } else {
            if (previous.composedLayers.length > 0) {
                ngPsdDepthApplyPreviousComposedLayerState(preview, previous);
            }
            string composeError;
            if (!ngComposePsdDepthImportResult(preview, composedPreview, composeError)) {
                errorMessage = composeError.length ? composeError : "PSD depth map composition failed";
            }
        }
        previewDirty = false;
    }

    void applyAlphaDepthGapFill() {
        if (previewDirty) rebuildPreview();
        if (errorMessage.length) return;

        static immutable DepthDrawAlphaDepthFocusedRule[] focusedRules = [
            DepthDrawAlphaDepthFocusedRule(17, 72, 0, 122, 132, 10, 18),
            DepthDrawAlphaDepthFocusedRule(6, 0, 0, 112, 126, 10, 20),
            DepthDrawAlphaDepthFocusedRule(6, 40, 0, 150, 170, 12, 24),
            DepthDrawAlphaDepthFocusedRule(10, 0, 0, 96, 120, 8, 16),
            DepthDrawAlphaDepthFocusedRule(14, 0, 44, 96, 156, 10, 20),
        ];

        foreach (layerIndex, ref layer; preview.composedLayers) {
            auto pixelCount = cast(size_t)max(0, layer.width * layer.height);
            if (pixelCount == 0 || layer.depthRgba.length != pixelCount * 4 ||
                layer.maskRgba.length != pixelCount * 4) continue;

            auto depth = ngDepthDrawDecodeGrayscaleDepthPixelsFromRgba(layer.depthRgba);
            ubyte[] mask;
            mask.length = pixelCount;
            foreach (i; 0 .. pixelCount) {
                auto offset = i * 4;
                mask[i] = layer.maskRgba[offset] != 0 && layer.maskRgba[offset + 3] != 0 ? 1 : 0;
            }
            auto detected = ngDepthDrawDetectAlphaDepthGaps(
                depth, mask, layer.width, layer.height, cast(int)layerIndex, focusedRules);
            auto filled = ngDepthDrawMedianFillDepth(
                depth, mask, detected.mask, layer.width, layer.height);
            foreach (i, value; filled.depth) {
                auto offset = i * 4;
                layer.depthRgba[offset + 0] = value;
                layer.depthRgba[offset + 1] = value;
                layer.depthRgba[offset + 2] = value;
                layer.depthRgba[offset + 3] = mask[i] && value > 0 ? 255 : 0;
            }
        }

        disposePreviewTextures();
        string composeError;
        if (!ngComposePsdDepthImportResult(preview, composedPreview, composeError)) {
            errorMessage = composeError.length ? composeError : "PSD depth map composition failed";
        }
        previewDirty = false;
    }

    void disposePreviewTextures() {
        foreach (key, texture; originalPreviewTextures) {
            if (texture !is null) texture.dispose();
        }
        foreach (key, texture; depthMaskPreviewTextures) {
            if (texture !is null) texture.dispose();
        }
        foreach (key, texture; rawCompositePreviewTextures) {
            if (texture !is null) texture.dispose();
        }
        if (threeDAdjustPreviewTexture !is null) threeDAdjustPreviewTexture.dispose();
        originalPreviewTextures = null;
        depthMaskPreviewTextures = null;
        rawCompositePreviewTextures = null;
        threeDAdjustSamples = null;
        threeDAdjustMeshes = null;
        threeDAdjustSamplesPrepared = false;
        if (threeDAdjustGpuRenderer !is null) threeDAdjustGpuRenderer.dispose();
        threeDAdjustGpuRenderer = null;
        threeDAdjustPreviewTexture = null;
        threeDAdjustPreviewWidth = 0;
        threeDAdjustPreviewHeight = 0;
        threeDAdjustPreviewDirty = true;
        threeDAdjustCameraLeft = int.min;
        threeDAdjustCameraTop = int.min;
        threeDAdjustCameraRight = int.min;
        threeDAdjustCameraBottom = int.min;
    }

    string layerCacheKey(string layerPath, ulong targetGridUuid) {
        return layerPath ~ "\0" ~ targetGridUuid.to!string;
    }

    PsdDepthComposedLayer* findLayerPreview(string layerPath, ulong targetGridUuid) {
        foreach (ref layerPreview; preview.composedLayers) {
            if (layerPreview.layerPath == layerPath && layerPreview.targetGridUuid == targetGridUuid) {
                return &layerPreview;
            }
        }
        return null;
    }

    PsdDepthComposedLayer* findComposedLayer(string layerPath, ulong targetGridUuid) {
        foreach (ref layer; preview.composedLayers) {
            if (layer.layerPath == layerPath && layer.targetGridUuid == targetGridUuid) return &layer;
        }
        return null;
    }

    PsdDepthLayerMapping* findMapping(string layerPath, ulong targetGridUuid) {
        foreach (ref mapping; preview.mappings) {
            if (mapping.layerPath == layerPath && mapping.targetGridUuid == targetGridUuid) return &mapping;
        }
        return null;
    }

    bool hasOtherMappings() {
        foreach (ref mapping; preview.mappings) {
            if (!mapping.matched || mapping.targetGridUuid == 0) return true;
        }
        foreach (ref layer; preview.composedLayers) {
            if (layer.targetGridUuid == 0) return true;
        }
        return false;
    }

    string firstOtherLayerPath() {
        foreach (ref mapping; preview.mappings) {
            if (mapping.matched && mapping.targetGridUuid != 0) continue;
            if (findLayerPreview(mapping.layerPath, mapping.targetGridUuid) !is null) return mapping.layerPath;
        }
        foreach (ref layer; preview.composedLayers) {
            if (layer.targetGridUuid != 0) continue;
            if (findLayerPreview(layer.layerPath, layer.targetGridUuid) !is null) return layer.layerPath;
        }
        return null;
    }

    Texture firstOtherPreviewTexture() {
        auto layerPath = firstOtherLayerPath();
        return layerPath.length ? layerPreviewTexture(layerPath, 0, true) : null;
    }

    bool isOthersSelected() {
        return selectedGridIndex == cast(ptrdiff_t)preview.grids.length;
    }

    string[] diagnostics() {
        string[] lines;
        if (errorMessage.length) {
            lines ~= _("Preview failed: %s").format(errorMessage);
            return lines;
        }
        if (lastApplyErrorMessage.length) {
            lines ~= _("GPU/apply failed: %s").format(lastApplyErrorMessage);
        }
        if (preview.compositionModeName.length) {
            lines ~= _("Composition: %s  %dx%d  color layers: %d  depth layers: %d  composed layers: %d  scale: %.3f").format(
                preview.compositionModeName,
                preview.compositionWidth,
                preview.compositionHeight,
                cast(int)preview.colorLayerCount,
                cast(int)preview.sourceDepthLayerCount,
                cast(int)preview.composedLayerCount,
                preview.globalDepthScale
            );
        }
        if (preview.colorSource.kindName.length || preview.depthSource.kindName.length) {
            lines ~= _("Sources: color=%s(%d) depth=%s(%d)").format(
                preview.colorSource.kindName.length ? preview.colorSource.kindName : "-",
                cast(int)preview.colorSource.layers.length,
                preview.depthSource.kindName.length ? preview.depthSource.kindName : "-",
                cast(int)preview.depthSource.layers.length
            );
        }
        foreach (diagnostic; preview.compositionDiagnostics) {
            auto subject = diagnostic.layerPath.length ? diagnostic.layerPath : diagnostic.layerName;
            if (subject.length) {
                lines ~= _("%s: %s").format(diagnostic.type, subject);
            } else if (diagnostic.message.length) {
                lines ~= _("%s: %s").format(diagnostic.type, diagnostic.message);
            } else {
                lines ~= diagnostic.type;
            }
        }
        if (preview.composedLayers.length == 0) {
            lines ~= _("No source layers were loaded from the selected depth source.");
        }
        if (preview.mappings.length > 0 && preview.grids.length == 0) {
            lines ~= _("No mapped targets were found. Remap a source layer to a GridDeformer or PathDeformer.");
        }
        foreach (mapping; preview.mappings) {
            if (mapping.status == "UnmatchedWithoutGrid" || mapping.status == "AmbiguousWithoutGrid") {
                lines ~= _("Unsupported target type for layer: %s").format(mapping.layerPath);
            } else if (mapping.status == "UnmatchedManualGrid") {
                lines ~= _("Mapped target is missing for layer: %s").format(mapping.layerPath);
            }
        }
        if (preview.gpuCompositionRequested) {
            lines ~= _("GPU Composition is selected. Apply must submit GPU work; CPU fallback is disabled.");
        }
        return lines;
    }

    void drawDiagnostics() {
        auto lines = diagnostics();
        foreach (line; lines) incText(line);
    }

    ubyte[] transformedDepthPreviewRgba(ref PsdDepthComposedLayer layerPreview) {
        ubyte[] rgba;
        if (layerPreview.width <= 0 || layerPreview.height <= 0) return rgba;
        auto pixelCount = cast(size_t)layerPreview.width * cast(size_t)layerPreview.height;
        rgba.length = pixelCount * 4;
        auto back = layerPreview.backDepth;
        auto front = layerPreview.frontDepth;
        auto minDepth = min(back, front);
        auto maxDepth = max(back, front);
        auto range = maxDepth - minDepth;
        if (range <= 0.000001f) range = 1.0f;
        foreach (i; 0 .. pixelCount) {
            auto index = i * 4;
            if (index + 3 >= layerPreview.maskRgba.length ||
                index + 3 >= layerPreview.colorRgba.length ||
                layerPreview.maskRgba[index] == 0 ||
                layerPreview.maskRgba[index + 3] == 0 ||
                layerPreview.colorRgba[index + 3] < 3) {
                continue;
            }
            float depth;
            auto x = cast(int)(i % cast(size_t)layerPreview.width);
            auto y = cast(int)(i / cast(size_t)layerPreview.width);
            if (!threeDAdjustDepthAt(layerPreview, x, y, depth)) continue;
            auto normalized = clamp((depth - minDepth) / range, 0.0f, 1.0f);
            auto gray = cast(ubyte)clamp(cast(int)(normalized * 255.0f + 0.5f), 0, 255);
            rgba[index + 0] = gray;
            rgba[index + 1] = gray;
            rgba[index + 2] = gray;
            rgba[index + 3] = 255;
        }
        return rgba;
    }

    Texture layerPreviewTexture(string layerPath, ulong targetGridUuid, bool depthMask) {
        auto layerPreview = findLayerPreview(layerPath, targetGridUuid);
        if (layerPreview is null) return null;
        auto key = layerCacheKey(layerPath, targetGridUuid);
        auto existing = depthMask ? key in depthMaskPreviewTextures : key in originalPreviewTextures;
        if (existing !is null) return *existing;

        auto rgba = depthMask ? transformedDepthPreviewRgba(*layerPreview) : layerPreview.colorRgba.dup;
        auto textureWidth = depthMask || layerPreview.width <= 0 ? layerPreview.width : layerPreview.width;
        auto textureHeight = depthMask || layerPreview.height <= 0 ? layerPreview.height : layerPreview.height;
        inTexPremultiply(rgba);
        auto texture = new Texture(rgba, textureWidth, textureHeight);
        if (depthMask) depthMaskPreviewTextures[key] = texture;
        else originalPreviewTextures[key] = texture;
        return texture;
    }

    Texture compositePreviewTexture(ref PsdDepthGridResult gridResult) {
        auto source = gridResult.rawCompositePreviewRgba;
        if (gridResult.grid is null || source.length == 0 ||
            gridResult.previewWidth <= 0 || gridResult.previewHeight <= 0) return null;

        auto key = gridResult.grid.uuid.to!string;
        auto existing = key in rawCompositePreviewTextures;
        if (existing) return *existing;

        auto rgba = source.dup;
        inTexPremultiply(rgba);
        auto texture = new Texture(rgba, gridResult.previewWidth, gridResult.previewHeight);
        rawCompositePreviewTextures[key] = texture;
        return texture;
    }

    void drawLayerPreviewTooltip(string layerPath, ulong targetGridUuid, bool depthMask) {
        auto layerPreview = findLayerPreview(layerPath, targetGridUuid);
        if (layerPreview is null) return;
        auto texture = layerPreviewTexture(layerPath, targetGridUuid, depthMask);
        if (texture is null) return;

        igBeginTooltip();
        incText(depthMask ? _("Depth Mask Preview") : _("Layer Preview"));
        incText("%s  %dx%d  (%d, %d)".format(
            layerPreview.layerName,
            layerPreview.width,
            layerPreview.height,
            layerPreview.left,
            layerPreview.top
        ));
        auto maxDimension = cast(float)(layerPreview.width > layerPreview.height ? layerPreview.width : layerPreview.height);
        auto scale = maxDimension > 0 ? min(PreviewSize / maxDimension, 1.0f) : 1.0f;
        igImage(
            cast(void*)texture.getTextureId(),
            ImVec2(cast(float)layerPreview.width * scale, cast(float)layerPreview.height * scale)
        );
        igEndTooltip();
    }

    void drawLayerPreviewHoverLabel(string label, string layerPath, ulong targetGridUuid, bool depthMask) {
        incText(label);
        if (igIsItemHovered()) drawLayerPreviewTooltip(layerPath, targetGridUuid, depthMask);
    }

    void drawLayerPreviewHoverText(string label, string layerPath, ulong targetGridUuid, bool depthMask) {
        incText(label.length ? label : "-");
        if (igIsItemHovered()) drawLayerPreviewTooltip(layerPath, targetGridUuid, depthMask);
    }

    ExPart matchedPart(ref PsdDepthLayerMapping mapping) {
        if (mapping.matchedNodeUuid == 0) return null;
        auto puppet = incActivePuppet();
        if (puppet is null || puppet.root is null) return null;
        foreach (part; puppet.findNodesType!ExPart(puppet.root)) {
            if (part.uuid == mapping.matchedNodeUuid) return part;
        }
        return null;
    }

    void drawMatchedNodeTooltip(ref PsdDepthLayerMapping mapping) {
        auto part = matchedPart(mapping);
        if (part is null || part.textures.length == 0 || part.textures[0] is null) return;

        auto texture = part.textures[0];
        igBeginTooltip();
        incText(_("Matched Node Preview"));
        incText(mapping.matchedNodeName);
        auto maxDimension = cast(float)max(texture.width, texture.height);
        auto scale = maxDimension > 0 ? min(PreviewSize / maxDimension, 1.0f) : 1.0f;
        igImage(
            cast(void*)texture.getTextureId(),
            ImVec2(cast(float)texture.width * scale, cast(float)texture.height * scale)
        );
        igEndTooltip();
    }

    void drawMatchedNodeText(ref PsdDepthLayerMapping mapping) {
        incText(mapping.matchedNodeName.length ? mapping.matchedNodeName : "-");
        if (igIsItemHovered()) drawMatchedNodeTooltip(mapping);
    }

    bool drawEnumCombo(string label, ref PsdDepthConvolution value) {
        auto current = ngPsdDepthConvolutionName(value);
        bool changed;
        if (igBeginCombo(label.toStringz, current.toStringz)) {
            foreach (name; ConvolutionNames) {
                bool selected = name == current;
                if (igSelectable(name.toStringz, selected)) {
                    value = ngPsdDepthConvolutionFromString(name);
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    bool drawEnumCombo(string label, ref PsdDepthMissingPolicy value) {
        auto current = ngPsdDepthMissingPolicyName(value);
        bool changed;
        if (igBeginCombo(label.toStringz, current.toStringz)) {
            foreach (name; MissingPolicyNames) {
                bool selected = name == current;
                if (igSelectable(name.toStringz, selected)) {
                    value = ngPsdDepthMissingPolicyFromString(name);
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    bool drawEnumCombo(string label, ref PsdDepthChannel value) {
        auto current = ngPsdDepthChannelName(value);
        bool changed;
        if (igBeginCombo(label.toStringz, current.toStringz)) {
            foreach (name; ChannelNames) {
                bool selected = name == current;
                if (igSelectable(name.toStringz, selected)) {
                    value = ngPsdDepthChannelFromString(name);
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    bool isCustomConvolution() {
        final switch (settings.convolution) {
            case PsdDepthConvolution.Nearest:
            case PsdDepthConvolution.Box3x3:
            case PsdDepthConvolution.Box5x5:
            case PsdDepthConvolution.Gaussian3x3:
            case PsdDepthConvolution.Gaussian5x5:
            case PsdDepthConvolution.Median3x3:
            case PsdDepthConvolution.Frontmost3x3:
            case PsdDepthConvolution.Backmost3x3:
                return false;
            case PsdDepthConvolution.BoxCustom:
            case PsdDepthConvolution.GaussianCustom:
            case PsdDepthConvolution.MedianCustom:
            case PsdDepthConvolution.FrontmostCustom:
            case PsdDepthConvolution.BackmostCustom:
                return true;
        }
    }

    Deformable[] currentTargets() {
        auto puppet = incActivePuppet();
        if (puppet is null || puppet.root is null) return null;
        Deformable[] targets;
        foreach (grid; puppet.findNodesType!GridDeformer(puppet.root)) targets ~= grid;
        foreach (path; puppet.findNodesType!PathDeformer(puppet.root)) targets ~= path;
        return targets;
    }

    string currentMappingLabel(ref PsdDepthLayerMapping mapping) {
        if (mapping.layerPath in settings.ignoredLayerPaths) return _("Ignore");
        if (auto overrideUuid = mapping.layerPath in settings.layerTargetGridUuidOverrides) {
            foreach (grid; currentTargets()) {
                if (grid.uuid.to!string == *overrideUuid) return grid.name.length ? grid.name : *overrideUuid;
            }
            return _("Missing Target");
        }
        return _("Auto");
    }

    bool drawManualMappingCombo(ref PsdDepthLayerMapping mapping) {
        bool changed;
        auto current = currentMappingLabel(mapping);
        if (igBeginCombo(("###map" ~ mapping.layerPath).toStringz, current.toStringz)) {
            bool autoSelected = !(mapping.layerPath in settings.ignoredLayerPaths) &&
                !(mapping.layerPath in settings.layerTargetGridUuidOverrides);
            if (igSelectable(_("Auto").toStringz, autoSelected)) {
                settings.ignoredLayerPaths.remove(mapping.layerPath);
                settings.layerTargetGridUuidOverrides.remove(mapping.layerPath);
                changed = true;
            }
            bool ignoreSelected = (mapping.layerPath in settings.ignoredLayerPaths) !is null;
            if (igSelectable(_("Ignore").toStringz, ignoreSelected)) {
                settings.layerTargetGridUuidOverrides.remove(mapping.layerPath);
                settings.ignoredLayerPaths[mapping.layerPath] = true;
                changed = true;
            }
            igSeparator();
            foreach (grid; currentTargets()) {
                auto uuid = grid.uuid.to!string;
                bool selected = false;
                if (auto overrideUuid = mapping.layerPath in settings.layerTargetGridUuidOverrides) {
                    selected = *overrideUuid == uuid;
                }
                auto label = grid.name.length ? grid.name : uuid;
                if (igSelectable(label.toStringz, selected)) {
                    settings.ignoredLayerPaths.remove(mapping.layerPath);
                    settings.layerTargetGridUuidOverrides[mapping.layerPath] = uuid;
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    void drawOptions() {
        bool changed;
        bool fillAlphaDepthGaps;
        incText(_("Color Source"));
        igSameLine();
        auto colorLabel = settings.colorSourcePath.length ? settings.colorSourcePath : _("<active target/art>");
        incText("%s".format(colorLabel));
        igSameLine();
        TFD_Filter[] colorFilters = [
            { ["*.png", "*.psd"], "Color source (*.png, *.psd)" },
            { ["*.png"], "Portable Network Graphics (*.png)" },
            { ["*.psd"], "Photoshop Document (*.psd)" }
        ];
        if (igButton(__("Select Color Source"))) {
            auto colorPath = incShowImportDialog(colorFilters, _("Select Color Source..."));
            if (colorPath.length) {
                settings.colorSourcePath = colorPath;
                changed = true;
            }
        }
        if (settings.colorSourcePath.length) {
            igSameLine();
            if (igButton(__("Clear Color Source"))) {
                settings.colorSourcePath = null;
                changed = true;
            }
        }
        changed = ngCheckbox(__("Invert Depth"), &settings.invert) || changed;
        incTooltip(_("Default: white is front and black is back."));
        changed = igDragFloat(__("Back Depth"), &settings.backDepth, 0.01f, -10.0f, 10.0f, "%.3f") || changed;
        changed = igDragFloat(__("Front Depth"), &settings.frontDepth, 0.01f, -10.0f, 10.0f, "%.3f") || changed;
        changed = igDragFloat(__("Depth Scale"), &settings.depthScale, 0.01f, 0.0f, 100.0f, "%.3f") || changed;
        incTooltip(_("Imported depth values are multiplied by this scale before applying."));
        changed = drawEnumCombo(_("Channel"), settings.channel) || changed;
        changed = drawEnumCombo(_("Sampling"), settings.convolution) || changed;
        if (isCustomConvolution()) {
            changed = igDragInt(__("Custom Radius"), &settings.customRadius, 0.1f, 1, 64) || changed;
        }
        changed = igDragFloat(__("Alpha Threshold"), &settings.alphaThreshold, 0.001f, 0.0f, 1.0f, "%.3f") || changed;
        changed = drawEnumCombo(_("Missing Vertex Pixel"), settings.missingPolicy) || changed;
        changed = ngCheckbox(__("Repair contour band"), &settings.repairContourBand) || changed;
        changed = ngCheckbox(__("Smooth wavy surface"), &settings.smoothWavySurface) || changed;
        fillAlphaDepthGaps = igButton(__("Fill alpha-depth gaps"));
        changed = ngCheckbox(__("GPU Composition"), &settings.useGpuComposition) || changed;
        incTooltip(_("When enabled, apply must use the GPU composition path. CPU fallback is treated as an error."));
        changed = ngCheckbox(__("Direct Grid Name Match"), &settings.matchDirectGridName) || changed;
        incTooltip(_("Also match PSD layer names directly against GridDeformer names."));
        changed = ngCheckbox(__("Only show problem layers"), &onlyProblemLayers) || changed;

        if (changed) previewDirty = true;
        if (fillAlphaDepthGaps) applyAlphaDepthGapFill();
    }

    void drawCompositePreviewTooltip(ref PsdDepthGridResult gridResult) {
        auto texture = compositePreviewTexture(gridResult);
        if (texture is null) return;

        igBeginTooltip();
        incText(_("Composite Depth Preview"));
        incText("%s  %dx%d  (%d, %d)".format(
            gridResult.grid !is null ? gridResult.grid.name : "-",
            gridResult.previewWidth,
            gridResult.previewHeight,
            gridResult.previewLeft,
            gridResult.previewTop
        ));
        auto maxDimension = cast(float)max(gridResult.previewWidth, gridResult.previewHeight);
        auto scale = maxDimension > 0 ? min(PreviewSize / maxDimension, 1.0f) : 1.0f;
        incText(_("Raw PSD Depth"));
        igImage(
            cast(void*)texture.getTextureId(),
            ImVec2(cast(float)gridResult.previewWidth * scale, cast(float)gridResult.previewHeight * scale)
        );
        igEndTooltip();
    }

    void ensureSelectedGridIndex() {
        auto other = hasOtherMappings();
        if (preview.grids.length == 0 && !other) {
            selectedGridIndex = -1;
            return;
        }
        auto maxIndex = cast(ptrdiff_t)preview.grids.length + (other ? 1 : 0);
        if (selectedGridIndex < 0 || selectedGridIndex >= maxIndex) {
            selectedGridIndex = 0;
        }
    }

    void drawGridList(float height) {
        ensureSelectedGridIndex();
        if (selectedGridIndex < 0) return;
        if (igBeginTable("###PsdDepthGridPreview", 3, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg | ImGuiTableFlags.ScrollY, ImVec2(0, height))) {
            igTableSetupColumn(__("Use"), ImGuiTableColumnFlags.WidthFixed, 46);
            igTableSetupColumn(__("Preview"), ImGuiTableColumnFlags.WidthFixed, 116);
            igTableSetupColumn(__("Target"));
            igTableHeadersRow();
            foreach (i, ref gridResult; preview.grids) {
                igTableNextRow();
                igTableNextColumn();
                drawGridEnabledCheckbox(gridResult.grid);
                igTableNextColumn();
                auto texture = compositePreviewTexture(gridResult);
                auto selected = selectedGridIndex == cast(ptrdiff_t)i;
                incTextureSlotUntitled(("###gridPreview" ~ i.to!string), texture, ImVec2(104, 104), 24, ImGuiWindowFlags.NoInputs, selected);
                if (igIsItemHovered()) drawCompositePreviewTooltip(gridResult);
                igTableNextColumn();
                auto label = "%s\n%s: %d  %s: %d\n%s: %d\n%s: %.3f  %s: %.3f\n%s".format(
                    gridResult.grid !is null ? gridResult.grid.name : "-",
                    _("Sampled"),
                    cast(int)gridResult.sampledVertices,
                    _("Missing"),
                    cast(int)gridResult.missingVertices,
                    "Coverage",
                    cast(int)gridResult.coverageSources,
                    _("Min"),
                    gridResult.minDepth,
                    _("Max"),
                    gridResult.maxDepth,
                    gridResult.skipped ? _("Skipped") : _("Will Apply")
                );
                if (igSelectable((label ~ "###gridRow" ~ i.to!string).toStringz, selected, ImGuiSelectableFlags.SpanAllColumns, ImVec2(0, 104))) {
                    selectedGridIndex = cast(ptrdiff_t)i;
                }
            }
            if (hasOtherMappings()) {
                igTableNextRow();
                igTableNextColumn();
                incText("-");
                igTableNextColumn();
                auto selected = isOthersSelected();
                auto texture = firstOtherPreviewTexture();
                incTextureSlotUntitled("###gridPreviewOthers", texture, ImVec2(104, 104), 24, ImGuiWindowFlags.NoInputs, selected);
                    if (igIsItemHovered()) {
                        auto layerPath = firstOtherLayerPath();
                        if (layerPath.length) drawLayerPreviewTooltip(layerPath, 0, true);
                    }
                igTableNextColumn();
                if (igSelectable((_("Others") ~ "\n" ~ _("Unmapped or ignored layers") ~ "###gridRowOthers").toStringz,
                    selected, ImGuiSelectableFlags.SpanAllColumns, ImVec2(0, 104))) {
                    selectedGridIndex = cast(ptrdiff_t)preview.grids.length;
                }
            }
            igEndTable();
        }
    }

    void drawGridEnabledCheckbox(Deformable grid) {
        if (grid is null) {
            incText("-");
            return;
        }

        auto key = grid.uuid.to!string;
        bool enabled = ngPsdDepthGridEnabled(settings, grid.uuid);
        if (ngCheckbox(("###useGrid" ~ key).toStringz, &enabled)) {
            if (enabled) {
                settings.disabledGridUuids.remove(key);
            } else {
                settings.disabledGridUuids[key] = true;
            }
            previewDirty = true;
        }
    }

    void drawComposedLayerEnabledCheckbox(string layerPath, ulong targetGridUuid) {
        auto layer = findComposedLayer(layerPath, targetGridUuid);
        if (layer is null) return;
        bool enabled = layer.enabled;
        auto widgetKey = "###useComposedLayer" ~ layerPath ~ targetGridUuid.to!string;
        if (ngCheckbox(widgetKey.toStringz, &enabled)) {
            layer.enabled = enabled;
            layer.visible = enabled;
            refreshAfter3DAdjustLayerChange(layerPath, targetGridUuid);
        }
    }

    void drawComposedLayerShowCheckbox(string layerPath, ulong targetGridUuid) {
        auto layer = findComposedLayer(layerPath, targetGridUuid);
        if (layer is null) return;
        bool enabled = layer.enabled && layer.visible;
        igPushID(("showComposedLayer" ~ layerPath ~ targetGridUuid.to!string).toStringz);
        scope(exit) igPopID();
        if (ngCheckbox(__("Show"), &enabled)) {
            layer.enabled = enabled;
            layer.visible = enabled;
            refreshAfter3DAdjustLayerChange(layerPath, targetGridUuid);
        }
    }

    void drawComposedLayerDepthEnabledCheckbox(string layerPath, ulong targetGridUuid) {
        auto layer = findComposedLayer(layerPath, targetGridUuid);
        if (layer is null) return;
        bool enabled = layer.depthEnabled;
        igPushID(("depthEnabledComposedLayer" ~ layerPath ~ targetGridUuid.to!string).toStringz);
        scope(exit) igPopID();
        if (ngCheckbox(__("Enable Depth"), &enabled)) {
            layer.depthEnabled = enabled;
            refreshAfter3DAdjustLayerChange(layerPath, targetGridUuid);
        }
        incTooltip(_("When disabled, use the nearest enabled depth below at each document coordinate."));
    }

    PsdDepthLayerEdit layerTransform(string layerPath, ulong targetGridUuid) {
        PsdDepthLayerEdit transform;
        if (auto layer = findComposedLayer(layerPath, targetGridUuid)) {
            transform.zOffset = layer.depthOffset;
            transform.zScale = layer.depthScale;
            transform.invert = layer.invert;
        }
        return transform;
    }

    void storeLayerTransform(string layerPath, ulong targetGridUuid, PsdDepthLayerEdit transform, bool changed) {
        if (!changed) return;
        if (auto layer = findComposedLayer(layerPath, targetGridUuid)) {
            layer.depthOffset = transform.zOffset;
            layer.depthScale = transform.zScale;
            layer.invert = transform.invert;
        }
        refreshAfter3DAdjustLayerChange(layerPath, targetGridUuid);
    }

    DepthSampleConvolution threeDAdjustSampleConvolution() {
        final switch (settings.convolution) {
        case PsdDepthConvolution.Nearest:
            return DepthSampleConvolution.Nearest;
        case PsdDepthConvolution.Box3x3:
            return DepthSampleConvolution.Box3x3;
        case PsdDepthConvolution.Box5x5:
            return DepthSampleConvolution.Box5x5;
        case PsdDepthConvolution.Gaussian3x3:
            return DepthSampleConvolution.Gaussian3x3;
        case PsdDepthConvolution.Gaussian5x5:
            return DepthSampleConvolution.Gaussian5x5;
        case PsdDepthConvolution.Median3x3:
            return DepthSampleConvolution.Median3x3;
        case PsdDepthConvolution.Frontmost3x3:
            return DepthSampleConvolution.Frontmost3x3;
        case PsdDepthConvolution.Backmost3x3:
            return DepthSampleConvolution.Backmost3x3;
        case PsdDepthConvolution.BoxCustom:
            return DepthSampleConvolution.BoxCustom;
        case PsdDepthConvolution.GaussianCustom:
            return DepthSampleConvolution.GaussianCustom;
        case PsdDepthConvolution.MedianCustom:
            return DepthSampleConvolution.MedianCustom;
        case PsdDepthConvolution.FrontmostCustom:
            return DepthSampleConvolution.FrontmostCustom;
        case PsdDepthConvolution.BackmostCustom:
            return DepthSampleConvolution.BackmostCustom;
        }
    }

    bool gridResultUsesLayer(ref PsdDepthGridResult gridResult, string layerPath) {
        foreach (ref layerMask; gridResult.layerMasks) {
            if (layerMask.layerPath == layerPath) return true;
        }
        return false;
    }

    bool layerTargetsGrid(string layerPath, ulong gridUuid) {
        if (gridUuid == 0) return false;
        if (auto composedLayer = findComposedLayer(layerPath, gridUuid)) {
            if (composedLayer.targetGridUuid == gridUuid) return true;
        }
        if (auto mapping = findMapping(layerPath, gridUuid)) {
            if (mapping.targetGridUuid == gridUuid) return true;
        }
        return false;
    }

    void disposeCompositePreviewTexture(ref PsdDepthGridResult gridResult) {
        if (gridResult.grid is null) return;
        auto key = gridResult.grid.uuid.to!string;
        if (auto texture = key in rawCompositePreviewTextures) {
            if (*texture !is null) (*texture).dispose();
            rawCompositePreviewTextures.remove(key);
        }
    }

    void disposeCompositePreviewTextures() {
        foreach (key, texture; rawCompositePreviewTextures) {
            if (texture !is null) texture.dispose();
        }
        rawCompositePreviewTextures = null;
    }

    void refreshGridResultDepths(ref PsdDepthGridResult gridResult) {
        if (gridResult.grid is null) return;
        PsdDepthGridResult composedGrid;
        string composeError;
        if (ngComposePsdDepthTarget(preview, gridResult, composedGrid, composeError)) {
            gridResult = composedGrid;
            disposeCompositePreviewTexture(gridResult);
            return;
        }
        errorMessage = composeError.length ? composeError : "PSD depth map composition failed";
    }

    void refreshPreviewDepthsForLayer(string layerPath, ulong targetGridUuid) {
        ptrdiff_t changedIndex = -1;
        foreach (i, ref layer; preview.composedLayers) {
            if (layer.layerPath == layerPath && layer.targetGridUuid == targetGridUuid) {
                changedIndex = cast(ptrdiff_t)i;
                break;
            }
        }
        foreach (ref gridResult; preview.grids) {
            if (gridResult.grid is null) continue;
            bool affected = gridResultUsesLayer(gridResult, layerPath) ||
                layerTargetsGrid(layerPath, gridResult.grid.uuid);
            if (!affected && changedIndex >= 0) {
                foreach (ref layerMask; gridResult.layerMasks) {
                    foreach (i, ref candidate; preview.composedLayers) {
                        if (candidate.layerPath != layerMask.layerPath ||
                            candidate.targetGridUuid != gridResult.grid.uuid) continue;
                        if (cast(ptrdiff_t)i > changedIndex && !candidate.depthEnabled) affected = true;
                        break;
                    }
                    if (affected) break;
                }
            }
            if (!affected) continue;
            refreshGridResultDepths(gridResult);
        }
    }

    void invalidate3DAdjustLayerCaches(string layerPath, ulong targetGridUuid) {
        auto key = layerCacheKey(layerPath, targetGridUuid);
        foreach (textureKey, texture; depthMaskPreviewTextures) {
            if (texture !is null) texture.dispose();
        }
        depthMaskPreviewTextures = null;
        if (auto texture = key in originalPreviewTextures) {
            if (*texture !is null) (*texture).dispose();
            originalPreviewTextures.remove(key);
        }
        // Stacking is a document-space dependency between every layer.
        // Changing one layer invalidates all prepared depths and meshes.
        threeDAdjustSamples = null;
        threeDAdjustMeshes = null;
        threeDAdjustSamplesPrepared = false;
        threeDAdjustPreviewDirty = true;
    }

    void refreshAfter3DAdjustLayerChange(string layerPath, ulong targetGridUuid) {
        invalidate3DAdjustLayerCaches(layerPath, targetGridUuid);
        refreshPreviewDepthsForLayer(layerPath, targetGridUuid);
    }

    void drawLayerZControls(string layerPath, ulong targetGridUuid) {
        igPushID(("layerZControls" ~ layerPath ~ targetGridUuid.to!string).toStringz);
        scope(exit) igPopID();
        auto transform = layerTransform(layerPath, targetGridUuid);
        bool changed;
        changed = ngCheckbox(__("Invert Depth"), &transform.invert) || changed;
        changed = igDragFloat(__("Z Scale"),
            &transform.zScale, 0.01f, -100.0f, 100.0f, "%.3f") || changed;
        changed = igDragFloat(__("Z Offset"),
            &transform.zOffset, 0.01f, -100.0f, 100.0f, "%.3f") || changed;
        if (incButtonColored(__("Reset Z"), ImVec2(120, 0))) {
            transform.zOffset = 0.0f;
            transform.zScale = 1.0f;
            transform.invert = false;
            changed = true;
        }
        storeLayerTransform(layerPath, targetGridUuid, transform, changed);
    }

    void drawMappingLayerRow(ref PsdDepthLayerMapping mapping, Deformable grid = null, PsdDepthGridLayerMask* layerMask = null) {
        bool problem = !mapping.matched || mapping.ambiguous || mapping.ignored;
        if (onlyProblemLayers && !problem) return;
        auto previewLayer = findLayerPreview(mapping.layerPath, mapping.targetGridUuid);
        igTableNextRow();
        igTableNextColumn();
        drawComposedLayerEnabledCheckbox(mapping.layerPath, mapping.targetGridUuid);
        igTableNextColumn();
        drawLayerPreviewHoverText(mapping.layerPath, mapping.layerPath, mapping.targetGridUuid, false);
        igTableNextColumn();
        drawLayerPreviewHoverText(mapping.layerName, mapping.layerPath, mapping.targetGridUuid, true);
        igTableNextColumn();
        drawMatchedNodeText(mapping);
        igTableNextColumn();
        if (drawManualMappingCombo(mapping)) previewDirty = true;
        igTableNextColumn();
        incText(mapping.status);
        igTableNextColumn();
        incText(layerMask !is null ? layerMask.sampledVertices.to!string : "-");
        igTableNextColumn();
        incText(layerMask !is null ? layerMask.selectedVertices.to!string : "-");
        igTableNextColumn();
        incText(previewLayer !is null ? previewLayer.depthStats.maskedPixels.to!string : "-");
        igTableNextColumn();
        incText(previewLayer !is null ? previewLayer.depthStats.zeroPixels.to!string : "-");
        igTableNextColumn();
        incText(previewLayer !is null && previewLayer.depthStats.hasDepth ?
            "%.3f".format(previewLayer.depthStats.rangeDepth01) : "-");
    }

    void drawSelectedGridLayers(float height) {
        ensureSelectedGridIndex();
        if (selectedGridIndex < 0) return;

        if (isOthersSelected()) {
            incText(_("Target: Others"));
        } else {
            auto gridResult = preview.grids[selectedGridIndex];
            incText(_("Target: %s").format(gridResult.grid !is null ? gridResult.grid.name : "-"));
        }

        if (igBeginTable("###PsdDepthGridMasks", 11, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg | ImGuiTableFlags.ScrollY, ImVec2(0, height))) {
            igTableSetupColumn(__("Use"), ImGuiTableColumnFlags.WidthFixed, 46);
            igTableSetupColumn(__("Path"));
            igTableSetupColumn(__("Depth Layer"));
            igTableSetupColumn(__("Matched Node"));
            igTableSetupColumn(__("Remap"));
            igTableSetupColumn(__("Status"));
            igTableSetupColumn(__("Sampled"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Selected"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Mask px"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Zero px"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Range"), ImGuiTableColumnFlags.WidthFixed, 64);
            igTableHeadersRow();
            if (isOthersSelected()) {
                foreach (ref mapping; preview.mappings) {
                    if (mapping.matched && mapping.targetGridUuid != 0) continue;
                    drawMappingLayerRow(mapping);
                }
            } else {
                auto gridResult = preview.grids[selectedGridIndex];
                foreach (ref layerMask; gridResult.layerMasks) {
                    auto mapping = findMapping(layerMask.layerPath, gridResult.grid.uuid);
                    if (mapping is null) continue;
                    drawMappingLayerRow(*mapping, gridResult.grid, &layerMask);
                }
            }
            igEndTable();
        }
    }

    void drawComposedSourceLayers(float height) {
        if (preview.composedLayers.length == 0) return;
        incText(_("Composed Source Layers"));
        if (igBeginTable("###PsdDepthComposedSourceLayers", 8,
            ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg | ImGuiTableFlags.ScrollY,
            ImVec2(0, height))) {
            igTableSetupColumn(__("Use"), ImGuiTableColumnFlags.WidthFixed, 46);
            igTableSetupColumn(__("Preview"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Source Layer"));
            igTableSetupColumn(__("Target"));
            igTableSetupColumn(__("Mask px"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Zero px"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Range"), ImGuiTableColumnFlags.WidthFixed, 64);
            igTableSetupColumn(__("Status"), ImGuiTableColumnFlags.WidthFixed, 92);
            igTableHeadersRow();
            foreach (i, ref layer; preview.composedLayers) {
                auto layerPath = layer.layerPath.length ? layer.layerPath : layer.id;
                igTableNextRow();
                igTableNextColumn();
                drawComposedLayerEnabledCheckbox(layerPath, layer.targetGridUuid);
                igTableNextColumn();
                auto texture = layerPreviewTexture(layerPath, layer.targetGridUuid, true);
                incTextureSlotUntitled(("###composedSourceLayerPreview" ~ i.to!string), texture,
                    ImVec2(60, 60), 18, ImGuiWindowFlags.NoInputs, false);
                if (igIsItemHovered() && texture !is null) {
                    drawLayerPreviewTooltip(layerPath, layer.targetGridUuid, true);
                }
                igTableNextColumn();
                drawLayerPreviewHoverText(layer.colorLayerName.length ? layer.colorLayerName : layer.layerName,
                    layerPath, layer.targetGridUuid, true);
                igTableNextColumn();
                incText(layer.targetGridName.length ? layer.targetGridName : "-");
                igTableNextColumn();
                incText(layer.depthStats.maskedPixels.to!string);
                igTableNextColumn();
                incText(layer.depthStats.zeroPixels.to!string);
                igTableNextColumn();
                incText(layer.depthStats.hasDepth ? "%.3f".format(layer.depthStats.rangeDepth01) : "-");
                igTableNextColumn();
                incText(layer.enabled ? (layer.targetGridUuid != 0 ? _("Bound") : _("Unbound")) : _("Disabled"));
            }
            igEndTable();
        }
    }

    void drawGridPreview(float height) {
        if (preview.grids.length == 0 && !hasOtherMappings()) return;
        auto leftWidth = max(260.0f, incAvailableSpace().x * 0.38f);
        if (igBeginTable("###PsdDepthGridReviewLayout", 2, ImGuiTableFlags.Resizable | ImGuiTableFlags.SizingStretchProp, ImVec2(0, height))) {
            igTableSetupColumn(__("Targets"), ImGuiTableColumnFlags.WidthFixed, leftWidth);
            igTableSetupColumn(__("Depth Layers"));
            igTableNextRow();
            igTableNextColumn();
            drawGridList(height);
            igTableNextColumn();
            drawSelectedGridLayers(height);
            igEndTable();
        }
    }

    void drawSourceMappingTab(float height) {
        auto lines = diagnostics();
        if (lines.length) {
            drawDiagnostics();
            if (errorMessage.length) return;
            igSeparator();
        }
        if (preview.grids.length == 0 && !hasOtherMappings() && preview.composedLayers.length == 0) {
            incText(_("No mapped targets were found. Remap a source layer to a GridDeformer or PathDeformer."));
            return;
        }
        if (preview.composedLayers.length > 0) {
            auto sourceHeight = min(220.0f, max(120.0f, height * 0.34f));
            drawComposedSourceLayers(sourceHeight);
            igSeparator();
            drawGridPreview(max(120.0f, height - sourceHeight - 16.0f));
        } else {
            drawGridPreview(height);
        }
    }

    PsdDepthComposedLayer* selected3DAdjustLayerPreview() {
        if (preview.composedLayers.length == 0) return null;
        auto lastIndex = cast(ptrdiff_t)preview.composedLayers.length - 1;
        selected3DAdjustLayerIndex = clamp(selected3DAdjustLayerIndex, 0, lastIndex);
        return &preview.composedLayers[cast(size_t)selected3DAdjustLayerIndex];
    }

    void draw3DAdjustLayerControlsPanel(float height) {
        incText("%s: %d".format(_("Layers"), cast(int)preview.composedLayers.length));
        incText("%s: %s".format(_("Composition"), preview.compositionModeName));
        igSeparator();

        if (preview.composedLayers.length == 0) return;
        auto listHeight = clamp(height * 0.34f, 140.0f, max(140.0f, height - 260.0f));
        if (igBeginChild("###PsdDepth3DLayerList", ImVec2(0, listHeight), true)) {
            foreach (i, ref layerPreview; preview.composedLayers) {
                auto selected = selected3DAdjustLayerIndex == cast(ptrdiff_t)i;
                auto label = "%s\n%d x %d  (%d, %d)###psdDepth3DLayer%d".format(
                    layerPreview.layerName.length ? layerPreview.layerName : layerPreview.layerPath,
                    layerPreview.width,
                    layerPreview.height,
                    layerPreview.left,
                    layerPreview.top,
                    cast(int)i
                );
                if (igSelectable(label.toStringz, selected, ImGuiSelectableFlags.SpanAllColumns, ImVec2(0, 42))) {
                    selected3DAdjustLayerIndex = cast(ptrdiff_t)i;
                }
            }
        }
        igEndChild();

        auto selectedLayer = selected3DAdjustLayerPreview();
        if (selectedLayer is null) return;
        auto layerPreview = *selectedLayer;

        igSeparator();
        if (igBeginChild("###PsdDepth3DLayerDetail", ImVec2(0, 0), false,
            ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse)) {
            drawLayerPreviewHoverText(
                layerPreview.layerName,
                layerPreview.layerPath,
                layerPreview.targetGridUuid,
                true);
            incText("%s: %dx%d  (%d, %d)".format(
                _("Layer"),
                layerPreview.width,
                layerPreview.height,
                layerPreview.left,
                layerPreview.top
            ));

            drawComposedLayerShowCheckbox(layerPreview.layerPath, layerPreview.targetGridUuid);
            drawComposedLayerDepthEnabledCheckbox(layerPreview.layerPath, layerPreview.targetGridUuid);
            if (layerPreview.depthEnabled) {
                drawLayerZControls(layerPreview.layerPath, layerPreview.targetGridUuid);
            } else {
                incText(_("Attached to depth below"));
            }
        }
        igEndChild();
    }

    void resetPsdDepth3DAdjustCameraToBounds(float sourceWidth, float sourceHeight, ImVec2 canvasSize) {
        threeDAdjustCamera.yaw = 0.42f;
        threeDAdjustCamera.pitch = 0.35f;
        threeDAdjustCamera.zoom = min(
            (canvasSize.x - 80.0f) / max(1.0f, sourceWidth),
            (canvasSize.y - 80.0f) / max(1.0f, sourceHeight)
        );
        threeDAdjustCamera.zoom = clamp(threeDAdjustCamera.zoom, 0.1f, 8.0f);
        threeDAdjustCamera.pan = vec2(0);
        threeDAdjustPreviewDirty = true;
    }

    bool threeDAdjustCameraChanged() {
        return threeDAdjustLastYaw != threeDAdjustCamera.yaw ||
            threeDAdjustLastPitch != threeDAdjustCamera.pitch ||
            threeDAdjustLastZoom != threeDAdjustCamera.zoom ||
            threeDAdjustLastPan.x != threeDAdjustCamera.pan.x ||
            threeDAdjustLastPan.y != threeDAdjustCamera.pan.y;
    }

    void rememberThreeDAdjustCamera() {
        threeDAdjustLastYaw = threeDAdjustCamera.yaw;
        threeDAdjustLastPitch = threeDAdjustCamera.pitch;
        threeDAdjustLastZoom = threeDAdjustCamera.zoom;
        threeDAdjustLastPan = threeDAdjustCamera.pan;
    }

    DepthSampleChannel threeDAdjustSampleChannel(PsdDepthChannel channel) {
        final switch (channel) {
        case PsdDepthChannel.AverageRGB:
            return DepthSampleChannel.AverageRGB;
        case PsdDepthChannel.R:
            return DepthSampleChannel.R;
        case PsdDepthChannel.G:
            return DepthSampleChannel.G;
        case PsdDepthChannel.B:
            return DepthSampleChannel.B;
        case PsdDepthChannel.Luminance:
            return DepthSampleChannel.Luminance;
        }
    }

    DepthSampleChannel threeDAdjustSampleChannel() {
        return threeDAdjustSampleChannel(settings.channel);
    }

    bool threeDAdjustDepthAt(ref PsdDepthComposedLayer layerPreview, int x, int y, out float depth) {
        depth = 0.0f;
        ptrdiff_t layerIndex = -1;
        foreach (i, ref candidate; preview.composedLayers) {
            if (&candidate is &layerPreview) {
                layerIndex = cast(ptrdiff_t)i;
                break;
            }
        }
        if (layerIndex < 0) return false;
        DepthSamplePoint sampleAt(int sampleX, int sampleY) {
            if (sampleX < 0 || sampleY < 0 || sampleX >= layerPreview.width || sampleY >= layerPreview.height) {
                return ngDepthSampleMissingPoint();
            }
            ptrdiff_t sourceIndex;
            ubyte[4] sourcePixel;
            if (!ngPsdDepthResolvedPixelAt(preview, cast(size_t)layerIndex,
                layerPreview.left + sampleX, layerPreview.top + sampleY, sourceIndex, sourcePixel)) {
                return ngDepthSampleMissingPoint();
            }
            auto source = &preview.composedLayers[cast(size_t)sourceIndex];
            auto alpha = cast(float)sourcePixel[3] / 255.0f;
            if (alpha <= source.alphaThreshold) return ngDepthSampleMissingPoint();
            auto value = ngDepthSamplePixelDepth(
                sourcePixel[],
                0,
                DepthSampleChannel.AverageRGB,
                source.invert,
                source.backDepth,
                source.frontDepth,
                source.sourceDepthScale
            );
            return DepthSamplePoint(true, value * source.depthScale + source.depthOffset, alpha);
        }
        auto sampled = ngDepthSampleConvolve!sampleAt(
            threeDAdjustSampleConvolution(), layerPreview.customRadius, x, y);
        if (!sampled.valid) return false;
        depth = sampled.value;
        return true;
    }

    void prepareThreeDAdjustSamples() {
        if (threeDAdjustSamplesPrepared) return;
        threeDAdjustSamples = null;

        foreach (ref layerPreview; preview.composedLayers) {
            PsdDepth3DAdjustSample[] samples;
            foreach (y; 0 .. layerPreview.height) {
                foreach (x; 0 .. layerPreview.width) {
                    auto rgbaIndex = (cast(size_t)y * cast(size_t)layerPreview.width + cast(size_t)x) * 4;
                    if (rgbaIndex + 3 >= layerPreview.maskRgba.length ||
                        rgbaIndex + 3 >= layerPreview.colorRgba.length ||
                        layerPreview.maskRgba[rgbaIndex] == 0 ||
                        layerPreview.maskRgba[rgbaIndex + 3] == 0 ||
                        layerPreview.colorRgba[rgbaIndex + 3] < 3) continue;
                    float depth;
                    if (!threeDAdjustDepthAt(layerPreview, x, y, depth)) continue;
                    PsdDepth3DAdjustSample sample;
                    sample.x = x;
                    sample.y = y;
                    sample.depthByte = layerPreview.depthRgba[rgbaIndex];
                    sample.depth = depth;
                    sample.r = layerPreview.colorRgba[rgbaIndex + 0];
                    sample.g = layerPreview.colorRgba[rgbaIndex + 1];
                    sample.b = layerPreview.colorRgba[rgbaIndex + 2];
                    sample.a = layerPreview.colorRgba[rgbaIndex + 3];
                    samples ~= sample;
                }
            }
            threeDAdjustSamples[layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid)] = samples;
        }

        if (preview.compositionMode == PsdDepthCompositionMode.NToOne &&
            preview.depthSource.kind == PsdDepthCompositeSourceKind.FlatImage &&
            preview.compositionWidth > 0 && preview.compositionHeight > 0) {
            float[] upperDepthLimit;
            upperDepthLimit.length = cast(size_t)preview.compositionWidth * cast(size_t)preview.compositionHeight;
            upperDepthLimit[] = float.max;

            // Same single document-space upper-depth buffer as depth-draw.
            foreach_reverse (ref layerPreview; preview.composedLayers) {
                if (!layerPreview.enabled || !layerPreview.visible) continue;
                auto key = layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid);
                auto samples = key in threeDAdjustSamples;
                if (samples is null) continue;
                auto depthStep = max(abs((layerPreview.frontDepth - layerPreview.backDepth) *
                    layerPreview.sourceDepthScale * layerPreview.depthScale) / 255.0f, 0.000001f);
                foreach (ref sample; *samples) {
                    auto documentX = layerPreview.left + sample.x;
                    auto documentY = layerPreview.top + sample.y;
                    if (documentX < 0 || documentY < 0 ||
                        documentX >= preview.compositionWidth || documentY >= preview.compositionHeight) continue;
                    auto documentIndex = cast(size_t)documentY * cast(size_t)preview.compositionWidth +
                        cast(size_t)documentX;
                    if (upperDepthLimit[documentIndex] < float.max) {
                        sample.depth = min(sample.depth, upperDepthLimit[documentIndex] - depthStep);
                    }
                    upperDepthLimit[documentIndex] = min(upperDepthLimit[documentIndex], sample.depth);
                }
            }
        }
        threeDAdjustSamplesPrepared = true;
    }

    bool preparedThreeDAdjustDepthAt(ref PsdDepthComposedLayer layerPreview, int x, int y, out float depth) {
        depth = 0.0f;
        prepareThreeDAdjustSamples();
        auto key = layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid);
        auto samples = key in threeDAdjustSamples;
        if (samples is null) return false;
        auto wanted = y * layerPreview.width + x;
        ptrdiff_t low;
        auto high = cast(ptrdiff_t)(*samples).length - 1;
        while (low <= high) {
            auto middle = low + (high - low) / 2;
            auto sample = (*samples)[cast(size_t)middle];
            auto found = sample.y * layerPreview.width + sample.x;
            if (found < wanted) low = middle + 1;
            else if (found > wanted) high = middle - 1;
            else {
                depth = sample.depth;
                return true;
            }
        }
        return false;
    }

    float threeDAdjustDepthDisplayScale() {
        Deformable[] targets;
        foreach (ref gridResult; preview.grids) {
            if (gridResult.grid !is null && !gridResult.skipped) targets ~= gridResult.grid;
        }
        auto puppet = incActivePuppet();
        auto targetScale = ngDepthDisplayScaleForTargetsInNodeSpace(
            puppet is null ? null : puppet.root,
            targets);
        if (targetScale > 0.0f) return targetScale;
        return ngDepthDisplayScaleForDocument(preview.compositionWidth, preview.compositionHeight);
    }

    PsdDepth3DAdjustSample[] threeDAdjustLayerSamples(ref PsdDepthComposedLayer layerPreview) {
        prepareThreeDAdjustSamples();
        auto key = layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid);
        auto existing = key in threeDAdjustSamples;
        return existing is null ? null : *existing;
    }

    PsdDepth3DAdjustMesh* threeDAdjustLayerMesh(ref PsdDepthComposedLayer layerPreview) {
        auto key = layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid);
        auto existing = key in threeDAdjustMeshes;
        if (existing !is null) return existing;

        PsdDepth3DAdjustMesh mesh;
        if (layerPreview.width <= 1 || layerPreview.height <= 1) {
            threeDAdjustMeshes[key] = mesh;
            return key in threeDAdjustMeshes;
        }

        auto step = max(1, PsdDepth3DAdjustMeshStep);
        auto cols = ((layerPreview.width - 1) + step - 1) / step + 1;
        auto rows = ((layerPreview.height - 1) + step - 1) / step + 1;
        int[] lookup;
        lookup.length = cast(size_t)cols * cast(size_t)rows;
        lookup[] = -1;

        int sampleX(int x) {
            return min(layerPreview.width - 1, x * step);
        }
        int sampleY(int y) {
            return min(layerPreview.height - 1, y * step);
        }
        int vertexAt(int gx, int gy) {
            return lookup[cast(size_t)gy * cast(size_t)cols + cast(size_t)gx];
        }

        foreach (gy; 0 .. rows) {
            foreach (gx; 0 .. cols) {
                auto x = sampleX(gx);
                auto y = sampleY(gy);
                auto index = (cast(size_t)y * cast(size_t)layerPreview.width + cast(size_t)x) * 4;
                if (index + 3 >= layerPreview.maskRgba.length ||
                    index + 3 >= layerPreview.colorRgba.length) {
                    continue;
                }
                if (layerPreview.colorRgba[index + 3] < 3) continue;
                float depth;
                if (!preparedThreeDAdjustDepthAt(layerPreview, x, y, depth)) continue;

                auto vertexIndex = cast(int)(mesh.vertexData.length / 5);
                lookup[cast(size_t)gy * cast(size_t)cols + cast(size_t)gx] = vertexIndex;
                mesh.vertexData ~= cast(float)layerPreview.left + cast(float)x;
                mesh.vertexData ~= cast(float)layerPreview.top + cast(float)y;
                mesh.vertexData ~= depth;
                mesh.vertexData ~= (cast(float)x + 0.5f) / cast(float)layerPreview.width;
                mesh.vertexData ~= (cast(float)y + 0.5f) / cast(float)layerPreview.height;
            }
        }

        if (settings.smoothWavySurface) {
            float[] gridDepths;
            ubyte[] vertexValid;
            int[] vertexGroup;
            gridDepths.length = lookup.length;
            vertexValid.length = lookup.length;
            vertexGroup.length = lookup.length;
            vertexGroup[] = -1;
            foreach (gridIndex, vertexIndex; lookup) {
                if (vertexIndex < 0) continue;
                vertexValid[gridIndex] = 1;
                vertexGroup[gridIndex] = 0;
                gridDepths[gridIndex] = mesh.vertexData[cast(size_t)vertexIndex * 5 + 2];
            }
            auto depthRange = abs((layerPreview.frontDepth - layerPreview.backDepth) *
                layerPreview.sourceDepthScale * layerPreview.depthScale);
            auto smoothed = ngDepthDrawSmoothGridDepthValues(
                gridDepths, vertexValid, vertexGroup, cols, rows, 18.0f, depthRange);
            foreach (gridIndex, vertexIndex; lookup) {
                if (vertexIndex < 0) continue;
                mesh.vertexData[cast(size_t)vertexIndex * 5 + 2] = smoothed[gridIndex];
            }
        }

        foreach (gy; 0 .. rows - 1) {
            foreach (gx; 0 .. cols - 1) {
                auto p0 = vertexAt(gx, gy);
                auto p1 = vertexAt(gx + 1, gy);
                auto p2 = vertexAt(gx, gy + 1);
                auto p3 = vertexAt(gx + 1, gy + 1);
                if (p0 >= 0 && p1 >= 0 && p3 >= 0) {
                    mesh.indices ~= [cast(uint)p0, cast(uint)p1, cast(uint)p3];
                }
                if (p0 >= 0 && p3 >= 0 && p2 >= 0) {
                    mesh.indices ~= [cast(uint)p0, cast(uint)p3, cast(uint)p2];
                }
            }
        }

        threeDAdjustMeshes[key] = mesh;
        return key in threeDAdjustMeshes;
    }

    void draw3DAdjustRelationshipCanvas(float height) {
        auto space = incAvailableSpace();
        auto canvasHeight = max(260.0f, height - 8.0f);
        auto canvasSize = ImVec2(max(320.0f, space.x), canvasHeight);
        ImVec2 origin;
        igGetCursorScreenPos(&origin);
        auto drawList = igGetWindowDrawList();
        auto bg = igGetColorU32(ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
        auto border = igGetColorU32(ImVec4(0.45f, 0.48f, 0.50f, 1.0f));
        auto textureTint = igGetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        auto canvasMax = ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y);
        ImDrawList_AddRectFilled(drawList, origin, canvasMax, bg, 4.0f);
        ImDrawList_AddRect(drawList, origin, canvasMax, border, 4.0f, ImDrawFlags.None, 1.0f);
        igInvisibleButton("###psdDepth3DAdjustViewport", canvasSize);
        auto io = igGetIO();
        if (igIsItemHovered()) {
            auto previousYaw = threeDAdjustCamera.yaw;
            auto previousPitch = threeDAdjustCamera.pitch;
            auto previousZoom = threeDAdjustCamera.zoom;
            auto previousPan = threeDAdjustCamera.pan;
            auto usesWheel = io.MouseWheel != 0;
            updateDepthCamera3D(
                threeDAdjustCamera,
                io,
                io.MouseDown[1] && !io.KeyShift,
                (io.MouseDown[1] && io.KeyShift) || io.MouseDown[2],
                usesWheel
            );
            if (previousYaw != threeDAdjustCamera.yaw ||
                previousPitch != threeDAdjustCamera.pitch ||
                previousZoom != threeDAdjustCamera.zoom ||
                previousPan.x != threeDAdjustCamera.pan.x ||
                previousPan.y != threeDAdjustCamera.pan.y) {
                threeDAdjustPreviewDirty = true;
            }
            if (usesWheel) igSetItemUsingMouseWheel();
        }

        int left;
        int top;
        int right;
        int bottom;
        bool hasBounds;
        void includeBounds(int x0, int y0, int x1, int y1) {
            if (!hasBounds) {
                left = x0;
                top = y0;
                right = x1;
                bottom = y1;
                hasBounds = true;
            } else {
                left = min(left, x0);
                top = min(top, y0);
                right = max(right, x1);
                bottom = max(bottom, y1);
            }
        }
        bool hasRenderPixels(ref PsdDepthComposedLayer layerPreview) {
            return threeDAdjustLayerSamples(layerPreview).length > 0;
        }
        foreach (ref layerPreview; preview.composedLayers) {
            if (!hasRenderPixels(layerPreview)) continue;
            includeBounds(
                layerPreview.left,
                layerPreview.top,
                layerPreview.left + layerPreview.width,
                layerPreview.top + layerPreview.height
            );
        }
        if (!hasBounds) includeBounds(0, 0, max(1, preview.compositionWidth), max(1, preview.compositionHeight));
        if (right <= left) right = left + 1;
        if (bottom <= top) bottom = top + 1;
        auto centerX = (cast(float)left + cast(float)right) * 0.5f;
        auto centerY = (cast(float)top + cast(float)bottom) * 0.5f;
        auto sourceW = cast(float)(right - left);
        auto sourceH = cast(float)(bottom - top);
        enum ulong PsdDepth3DAdjustSceneCameraKey = ulong.max;
        if (threeDAdjustCameraTargetUuid != PsdDepth3DAdjustSceneCameraKey ||
            threeDAdjustCameraLeft != left ||
            threeDAdjustCameraTop != top ||
            threeDAdjustCameraRight != right ||
            threeDAdjustCameraBottom != bottom) {
            threeDAdjustCameraTargetUuid = PsdDepth3DAdjustSceneCameraKey;
            threeDAdjustCameraLeft = left;
            threeDAdjustCameraTop = top;
            threeDAdjustCameraRight = right;
            threeDAdjustCameraBottom = bottom;
            resetPsdDepth3DAdjustCameraToBounds(sourceW, sourceH, canvasSize);
            threeDAdjustPreviewDirty = true;
        }
        auto depthDisplayScale = threeDAdjustDepthDisplayScale();

        auto framebufferWidth = max(1, cast(int)canvasSize.x);
        auto framebufferHeight = max(1, cast(int)canvasSize.y);
        if (threeDAdjustGpuRenderer is null) threeDAdjustGpuRenderer = new PsdDepth3DAdjustGpuRenderer();
        auto gpuTexture = threeDAdjustGpuRenderer.render(
            framebufferWidth,
            framebufferHeight,
            {
                foreach (ref layerPreview; preview.composedLayers) {
                    if (!hasRenderPixels(layerPreview)) continue;
                    auto mesh = threeDAdjustLayerMesh(layerPreview);
                    if (mesh is null || mesh.indices.length == 0) continue;
                    auto layerTexture = layerPreviewTexture(
                        layerPreview.layerPath,
                        layerPreview.targetGridUuid,
                        false);
                    threeDAdjustGpuRenderer.drawLayer(
                        layerTexture,
                        *mesh,
                        centerX,
                        centerY,
                        threeDAdjustCamera.yaw,
                        threeDAdjustCamera.pitch,
                        threeDAdjustCamera.zoom,
                        threeDAdjustCamera.pan,
                        depthDisplayScale
                    );
                }
            }
        );
        if (gpuTexture !is null) {
            ImDrawList_AddImage(
                drawList,
                cast(void*)gpuTexture.getTextureId(),
                origin,
                canvasMax,
                ImVec2(0, 1),
                ImVec2(1, 0),
                textureTint
            );
            ImDrawList_AddRect(drawList, origin, canvasMax, border, 4.0f, ImDrawFlags.None, 1.0f);
            return;
        }
        if (threeDAdjustPreviewTexture !is null &&
            threeDAdjustPreviewWidth == framebufferWidth &&
            threeDAdjustPreviewHeight == framebufferHeight &&
            !threeDAdjustPreviewDirty &&
            !threeDAdjustCameraChanged()) {
            ImDrawList_AddImage(
                drawList,
                cast(void*)threeDAdjustPreviewTexture.getTextureId(),
                origin,
                canvasMax,
                ImVec2(0, 0),
                ImVec2(1, 1),
                textureTint
            );
            ImDrawList_AddRect(drawList, origin, canvasMax, border, 4.0f, ImDrawFlags.None, 1.0f);
            return;
        }
        threeDAdjustPreviewDirty = false;
        rememberThreeDAdjustCamera();
        ubyte[] framebuffer;
        float[] zBuffer;
        framebuffer.length = cast(size_t)framebufferWidth * cast(size_t)framebufferHeight * 4;
        zBuffer.length = cast(size_t)framebufferWidth * cast(size_t)framebufferHeight;
        foreach (i; 0 .. cast(size_t)framebufferWidth * cast(size_t)framebufferHeight) {
            auto pixelIndex = i * 4;
            framebuffer[pixelIndex + 0] = 20;
            framebuffer[pixelIndex + 1] = 23;
            framebuffer[pixelIndex + 2] = 26;
            framebuffer[pixelIndex + 3] = 255;
            zBuffer[i] = float.max;
        }
        size_t renderedPixels;

        import std.math : cos, sin;

        float cameraYawCos = cos(threeDAdjustCamera.yaw);
        float cameraYawSin = sin(threeDAdjustCamera.yaw);
        float cameraPitchCos = cos(threeDAdjustCamera.pitch);
        float cameraPitchSin = sin(threeDAdjustCamera.pitch);
        float cameraZoom = threeDAdjustCamera.zoom;
        float cameraPanX = threeDAdjustCamera.pan.x;
        float cameraPanY = threeDAdjustCamera.pan.y;

        float cameraDepthForPoint(vec2 point, float depth) {
            float x = point.x;
            float y = point.y;
            float z = depth;

            float rz = -x * cameraYawSin + z * cameraYawCos;
            return y * cameraPitchSin + rz * cameraPitchCos;
        }

        ImVec2 projectDocumentPoint(vec2 documentPoint, float depth, float sceneDepthDisplayScale) {
            float x = documentPoint.x - centerX;
            float y = documentPoint.y - centerY;
            float z = -depth * sceneDepthDisplayScale;

            float rx = x * cameraYawCos + z * cameraYawSin;
            float rz = -x * cameraYawSin + z * cameraYawCos;
            float ry = y * cameraPitchCos - rz * cameraPitchSin;
            return ImVec2(
                origin.x + canvasSize.x * 0.5f + rx * cameraZoom + cameraPanX,
                origin.y + canvasSize.y * 0.5f + ry * cameraZoom + cameraPanY
            );
        }

        float edgeFunction(float ax, float ay, float bx, float by, float cx, float cy) {
            return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
        }

        ubyte sampleCoverage(ref PsdDepthComposedLayer layerPreview, float u, float v) {
            auto textureWidth = layerPreview.width;
            auto textureHeight = layerPreview.height;
            if (textureWidth <= 0 || textureHeight <= 0) return 0;
            auto x = clamp(cast(int)(u * cast(float)textureWidth), 0, textureWidth - 1);
            auto y = clamp(cast(int)(v * cast(float)textureHeight), 0, textureHeight - 1);
            auto index = (cast(size_t)y * cast(size_t)textureWidth + cast(size_t)x) * 4;
            if (index + 3 >= layerPreview.colorRgba.length) return 0;
            if (index + 3 >= layerPreview.maskRgba.length) return 0;
            if (layerPreview.maskRgba[index] == 0 || layerPreview.maskRgba[index + 3] == 0) return 0;
            return layerPreview.colorRgba[index + 3];
        }

        bool sampleColor(ref PsdDepthComposedLayer layerPreview, float u, float v, out ubyte r, out ubyte g, out ubyte b, out ubyte a) {
            r = g = b = a = 0;
            auto textureWidth = layerPreview.width;
            auto textureHeight = layerPreview.height;
            if (textureWidth <= 0 || textureHeight <= 0) return false;
            auto x = clamp(cast(int)(u * cast(float)textureWidth), 0, textureWidth - 1);
            auto y = clamp(cast(int)(v * cast(float)textureHeight), 0, textureHeight - 1);
            auto index = (cast(size_t)y * cast(size_t)textureWidth + cast(size_t)x) * 4;
            if (index + 3 >= layerPreview.colorRgba.length) return false;
            if (index + 3 >= layerPreview.maskRgba.length) return false;
            if (layerPreview.maskRgba[index] == 0 || layerPreview.maskRgba[index + 3] == 0) return false;
            r = layerPreview.colorRgba[index + 0];
            g = layerPreview.colorRgba[index + 1];
            b = layerPreview.colorRgba[index + 2];
            a = layerPreview.colorRgba[index + 3];
            return true;
        }

        void rasterizeImageTriangle(
            ref PsdDepthComposedLayer layerPreview,
            ImVec2 p0,
            ImVec2 p1,
            ImVec2 p2,
            ImVec2 uv0,
            ImVec2 uv1,
            ImVec2 uv2,
            float z0,
            float z1,
            float z2
        ) {
            import std.math : ceil, floor;

            auto x0 = p0.x - origin.x;
            auto y0 = p0.y - origin.y;
            auto x1 = p1.x - origin.x;
            auto y1 = p1.y - origin.y;
            auto x2 = p2.x - origin.x;
            auto y2 = p2.y - origin.y;
            auto area = edgeFunction(x0, y0, x1, y1, x2, y2);
            if (area == 0.0f) return;

            auto minX = clamp(cast(int)floor(min(min(x0, x1), x2)), 0, framebufferWidth - 1);
            auto maxX = clamp(cast(int)ceil(max(max(x0, x1), x2)), 0, framebufferWidth - 1);
            auto minY = clamp(cast(int)floor(min(min(y0, y1), y2)), 0, framebufferHeight - 1);
            auto maxY = clamp(cast(int)ceil(max(max(y0, y1), y2)), 0, framebufferHeight - 1);

            for (int y = minY; y <= maxY; y++) {
                for (int x = minX; x <= maxX; x++) {
                    auto px = cast(float)x + 0.5f;
                    auto py = cast(float)y + 0.5f;
                    auto w0 = edgeFunction(x1, y1, x2, y2, px, py);
                    auto w1 = edgeFunction(x2, y2, x0, y0, px, py);
                    auto w2 = edgeFunction(x0, y0, x1, y1, px, py);
                    if (area > 0.0f) {
                        if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
                    } else {
                        if (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f) continue;
                    }
                    w0 /= area;
                    w1 /= area;
                    w2 /= area;
                    auto z = z0 * w0 + z1 * w1 + z2 * w2;
                    auto zIndex = cast(size_t)y * cast(size_t)framebufferWidth + cast(size_t)x;
                    if (z >= zBuffer[zIndex]) continue;

                    auto u = uv0.x * w0 + uv1.x * w1 + uv2.x * w2;
                    auto v = uv0.y * w0 + uv1.y * w1 + uv2.y * w2;
                    if (sampleCoverage(layerPreview, u, v) < 128) continue;

                    ubyte sr;
                    ubyte sg;
                    ubyte sb;
                    ubyte sa;
                    if (!sampleColor(layerPreview, u, v, sr, sg, sb, sa) || sa < 3) continue;

                    auto pixelIndex = zIndex * 4;
                    auto alpha = cast(float)sa / 255.0f;
                    framebuffer[pixelIndex + 0] = cast(ubyte)clamp(cast(int)(cast(float)sr * alpha +
                        cast(float)framebuffer[pixelIndex + 0] * (1.0f - alpha) + 0.5f), 0, 255);
                    framebuffer[pixelIndex + 1] = cast(ubyte)clamp(cast(int)(cast(float)sg * alpha +
                        cast(float)framebuffer[pixelIndex + 1] * (1.0f - alpha) + 0.5f), 0, 255);
                    framebuffer[pixelIndex + 2] = cast(ubyte)clamp(cast(int)(cast(float)sb * alpha +
                        cast(float)framebuffer[pixelIndex + 2] * (1.0f - alpha) + 0.5f), 0, 255);
                    framebuffer[pixelIndex + 3] = 255;
                    zBuffer[zIndex] = z;
                }
            }
        }

        foreach (ref layerPreview; preview.composedLayers) {
            if (!hasRenderPixels(layerPreview)) continue;
            if (layerPreview.width <= 1 || layerPreview.height <= 1) continue;
            auto samples = threeDAdjustLayerSamples(layerPreview);
            if (samples.length == 0) continue;
            auto step = 1;
            void rasterizeDepthSample(PsdDepth3DAdjustSample sample, float depth) {
                auto documentPoint = vec2(
                    cast(float)layerPreview.left + cast(float)sample.x,
                    cast(float)layerPreview.top + cast(float)sample.y
                );
                auto projected = projectDocumentPoint(documentPoint, depth, depthDisplayScale);
                auto sx = cast(int)round(projected.x - origin.x);
                auto sy = cast(int)round(projected.y - origin.y);
                if (sx < -1 || sy < -1 || sx > framebufferWidth || sy > framebufferHeight) return;

                auto cameraPoint = vec2(documentPoint.x - centerX, documentPoint.y - centerY);
                auto z = cameraDepthForPoint(cameraPoint, -depth * depthDisplayScale);
                auto radius = max(1, step / 2);
                for (int dy = -radius; dy <= radius; dy++) {
                    auto y = sy + dy;
                    if (y < 0 || y >= framebufferHeight) continue;
                    for (int dx = -radius; dx <= radius; dx++) {
                        auto x = sx + dx;
                        if (x < 0 || x >= framebufferWidth) continue;
                        auto zIndex = cast(size_t)y * cast(size_t)framebufferWidth + cast(size_t)x;
                        if (z >= zBuffer[zIndex]) continue;

                        auto alpha = cast(float)sample.a / 255.0f;
                        auto pixelIndex = zIndex * 4;
                        framebuffer[pixelIndex + 0] = cast(ubyte)clamp(cast(int)(cast(float)sample.r * alpha +
                            cast(float)framebuffer[pixelIndex + 0] * (1.0f - alpha) + 0.5f), 0, 255);
                        framebuffer[pixelIndex + 1] = cast(ubyte)clamp(cast(int)(cast(float)sample.g * alpha +
                            cast(float)framebuffer[pixelIndex + 1] * (1.0f - alpha) + 0.5f), 0, 255);
                        framebuffer[pixelIndex + 2] = cast(ubyte)clamp(cast(int)(cast(float)sample.b * alpha +
                            cast(float)framebuffer[pixelIndex + 2] * (1.0f - alpha) + 0.5f), 0, 255);
                        framebuffer[pixelIndex + 3] = 255;
                        zBuffer[zIndex] = z;
                        renderedPixels++;
                    }
                }
            }

            foreach (sample; samples) {
                rasterizeDepthSample(sample, sample.depth);
            }
        }

        inTexPremultiply(framebuffer);
        if (threeDAdjustPreviewTexture is null ||
            threeDAdjustPreviewWidth != framebufferWidth ||
            threeDAdjustPreviewHeight != framebufferHeight) {
            if (threeDAdjustPreviewTexture !is null) threeDAdjustPreviewTexture.dispose();
            threeDAdjustPreviewTexture = new Texture(framebuffer, framebufferWidth, framebufferHeight, 4, 4, false, false);
            threeDAdjustPreviewWidth = framebufferWidth;
            threeDAdjustPreviewHeight = framebufferHeight;
        } else {
            threeDAdjustPreviewTexture.setData(framebuffer, 4);
        }
        ImDrawList_AddImage(
            drawList,
            cast(void*)threeDAdjustPreviewTexture.getTextureId(),
            origin,
            canvasMax,
            ImVec2(0, 0),
            ImVec2(1, 1),
            textureTint
        );
        ImDrawList_AddRect(drawList, origin, canvasMax, border, 4.0f, ImDrawFlags.None, 1.0f);
    }

    void draw3DAdjustTargetPreview(float height) {
        auto available = incAvailableSpace();
        auto controlWidth = min(360.0f, max(300.0f, available.x * 0.34f));
        if (igBeginTable("###PsdDepth3DRelationshipLayout", 2,
            ImGuiTableFlags.Resizable | ImGuiTableFlags.SizingStretchProp, ImVec2(0, height))) {
            igTableSetupColumn(__("3D View"), ImGuiTableColumnFlags.WidthStretch);
            igTableSetupColumn(__("Adjust"), ImGuiTableColumnFlags.WidthFixed, controlWidth);
            igTableNextRow();
            igTableNextColumn();
            draw3DAdjustRelationshipCanvas(height);
            igTableNextColumn();
            draw3DAdjustLayerControlsPanel(height);
            igEndTable();
        }
    }

    PsdDepth3DAdjustGeometryStats threeDAdjustGeometryStats(ref PsdDepthGridResult gridResult) {
        PsdDepth3DAdjustGeometryStats stats;
        if (gridResult.grid is null) return stats;
        auto targetView = new DepthTargetView(gridResult.grid);
        auto targetVertices = targetView.getVertices();
        foreach (layerMask; gridResult.layerMasks) {
            if (findLayerPreview(layerMask.layerPath, gridResult.grid.uuid) is null) continue;
            stats.layerPlanes++;
            stats.depthRangeLines += 2;
        }
        if (targetVertices.length > 1) stats.targetWireLines = targetVertices.length - 1;
        foreach (i; 0 .. targetVertices.length) {
            if (i < gridResult.missingVertexMask.length && gridResult.missingVertexMask[i]) {
                stats.missingPoints++;
            } else {
                stats.sampledPoints++;
            }
        }
        return stats;
    }

    void draw3DAdjustTab(float height) {
        auto lines = diagnostics();
        if (lines.length && (errorMessage.length || preview.grids.length == 0)) {
            drawDiagnostics();
            return;
        }
        if (preview.composedLayers.length == 0) {
            incText(_("No composed source layers were loaded."));
            return;
        }
        if (lines.length) {
            drawDiagnostics();
            igSeparator();
        }
        if (igBeginChild("###PsdDepth3DAdjustContent", ImVec2(0, height), false,
            ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse)) {
            draw3DAdjustTargetPreview(incAvailableSpace().y);
        }
        igEndChild();
    }

    void apply() {
        if (previewDirty) rebuildPreview();
        if (errorMessage.length) {
            incDialog(__("Error"), errorMessage);
            return;
        }
        auto result = ngApplyPsdDepthImportResult(composedPreview);
        if (!result.succeeded) {
            lastApplyErrorMessage = result.message;
            incDialog(__("Error"), result.message);
            return;
        }
        lastApplyErrorMessage = null;
        close();
    }

protected:
    override
    void onBeginUpdate() {
        flags |= ImGuiWindowFlags.NoSavedSettings |
            ImGuiWindowFlags.NoScrollbar |
            ImGuiWindowFlags.NoScrollWithMouse;

        ImVec2 wpos = ImVec2(
            igGetMainViewport().Pos.x + (igGetMainViewport().Size.x / 2),
            igGetMainViewport().Pos.y + (igGetMainViewport().Size.y / 2),
        );
        ImVec2 uiSize = ImVec2(980, 760);
        igSetNextWindowPos(wpos, ImGuiCond.Appearing, ImVec2(0.5, 0.5));
        igSetNextWindowSize(uiSize, ImGuiCond.Appearing);
        igSetNextWindowSizeConstraints(ImVec2(760, 520), ImVec2(float.max, float.max));
        super.onBeginUpdate();
    }

    override
    void onUpdate() {
        if (previewDirty) rebuildPreview();

        auto space = incAvailableSpace();
        auto settingsWidth = min(340.0f, max(280.0f, space.x * 0.28f));
        if (igBeginTable("###PsdDepthDialogLayout", 2,
            ImGuiTableFlags.Resizable | ImGuiTableFlags.SizingStretchProp, ImVec2(0, space.y))) {
            igTableSetupColumn(__("Settings"), ImGuiTableColumnFlags.WidthFixed, settingsWidth);
            igTableSetupColumn(__("Preview"), ImGuiTableColumnFlags.WidthStretch);
            igTableNextRow();
            igTableNextColumn();

            auto actionsHeight = 62.0f;
            auto settingsHeight = max(120.0f, space.y - actionsHeight);
            if (igBeginChild("###PsdDepthSettingsPane", ImVec2(0, settingsHeight), true)) {
                drawOptions();
            }
            igEndChild();
            auto actionWidth = incAvailableSpace().x;
            if (incButtonColored(__("Apply"), ImVec2(actionWidth, 26))) {
                apply();
            }
            if (incButtonColored(__("Cancel"), ImVec2(actionWidth, 26))) {
                close();
            }

            igTableNextColumn();
            if (igBeginChild("###PsdDepthWorkspace", ImVec2(0, space.y), false,
                ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse)) {
                if (igBeginTabBar("###PsdDepthMapImportTabs")) {
                    auto threeDTabFlags = initial3DAdjustTabSelected ?
                        ImGuiTabItemFlags.None : ImGuiTabItemFlags.SetSelected;
                    if (igBeginTabItem(__("3D Adjust"), null, threeDTabFlags)) {
                        initial3DAdjustTabSelected = true;
                        draw3DAdjustTab(max(120.0f, incAvailableSpace().y));
                        igEndTabItem();
                    }
                    if (igBeginTabItem(__("Source / Mapping"))) {
                        drawSourceMappingTab(max(120.0f, incAvailableSpace().y));
                        igEndTabItem();
                    }
                    igEndTabBar();
                }
            }
            igEndChild();
            igEndTable();
        }
    }

    override
    void onClose() {
        disposePreviewTextures();
    }

public:
    this(string path) {
        this.path = path;
        super(_("PSD Depth Map Import"));
    }

    version (RegressionSmoke) {
        void rebuildPreviewForRegressionSmoke() {
            rebuildPreview();
        }

        string loadErrorForRegressionSmoke() const {
            return errorMessage;
        }

        size_t previewGridCountForRegressionSmoke() const {
            return preview.grids.length;
        }

        bool remapLayerNameToGridForRegressionSmoke(string layerName, ulong gridUuid) {
            foreach (mapping; preview.mappings) {
                if (mapping.layerName.toLower != layerName.toLower) continue;
                settings.layerTargetGridUuidOverrides[mapping.layerPath] = gridUuid.to!string;
                rebuildPreview();
                return true;
            }
            return false;
        }

        bool remapAnyLayerToSampledGridForRegressionSmoke(string gridName, ulong gridUuid) {
            string[] layerPaths;
            foreach (mapping; preview.mappings) {
                if (mapping.layerPath.length == 0) continue;
                layerPaths ~= mapping.layerPath;
            }
            foreach (layerPath; layerPaths) {
                settings.layerTargetGridUuidOverrides[layerPath] = gridUuid.to!string;
                rebuildPreview();
                if (hasSampledPreviewGridForRegressionSmoke(gridName)) return true;
            }
            return false;
        }

        bool hasSampledPreviewGridForRegressionSmoke(string gridName) {
            foreach (gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                return !gridResult.skipped &&
                    gridResult.depths.length > 0 &&
                    gridResult.sampledVertices > 0 &&
                    gridResult.previewWidth > 0 &&
                    gridResult.previewHeight > 0 &&
                    gridResult.rawCompositePreviewRgba.length > 0;
            }
            return false;
        }

        bool has3DAdjustGeometryForRegressionSmoke(string gridName) {
            foreach (ref gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                auto stats = threeDAdjustGeometryStats(gridResult);
                return stats.layerPlanes > 0 &&
                    stats.depthRangeLines > 0 &&
                    stats.targetWireLines > 0 &&
                    stats.sampledPoints + stats.missingPoints == gridResult.grid.vertices.length &&
                    stats.sampledPoints > 0;
            }
            return false;
        }

        string[] diagnosticsForRegressionSmoke() {
            return diagnostics();
        }

        string previewSummaryForRegressionSmoke() {
            string[] lines;
            foreach (layer; preview.composedLayers) {
                lines ~= "layer=%s bounds=%s,%s %sx%s enabled=%s depth=%s zero=%s".format(
                    layer.layerPath,
                    layer.left,
                    layer.top,
                    layer.width,
                    layer.height,
                    layer.enabled,
                    layer.depthStats.maskedPixels,
                    layer.depthStats.zeroPixels);
            }
            foreach (mapping; preview.mappings) {
                lines ~= "mapping layer=%s target=%s status=%s matched=%s manual=%s".format(
                    mapping.layerPath,
                    mapping.targetGridName,
                    mapping.status,
                    mapping.matched,
                    mapping.manual);
            }
            foreach (gridResult; preview.grids) {
                lines ~= "grid=%s depths=%s sampled=%s missing=%s preview=%sx%s raw=%s skipped=%s coverage=%s".format(
                    gridResult.grid !is null ? gridResult.grid.name : "<null>",
                    gridResult.depths.length,
                    gridResult.sampledVertices,
                    gridResult.missingVertices,
                    gridResult.previewWidth,
                    gridResult.previewHeight,
                    gridResult.rawCompositePreviewRgba.length,
                    gridResult.skipped,
                    gridResult.coverageSources);
            }
            return lines.join(" | ");
        }

        bool applyForRegressionSmoke(out string message) {
            if (previewDirty) rebuildPreview();
            if (errorMessage.length) {
                message = errorMessage;
                return false;
            }
            auto result = ngApplyPsdDepthImportResult(composedPreview);
            message = result.message;
            return result.succeeded;
        }

        void setGpuCompositionForRegressionSmoke(bool enabled) {
            settings.useGpuComposition = enabled;
            previewDirty = true;
            rebuildPreview();
        }

        bool setFirstMappedLayerZOffsetForRegressionSmoke(string gridName, float zOffset) {
            if (previewDirty) rebuildPreview();
            foreach (ref gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                foreach (layerMask; gridResult.layerMasks) {
                    if (layerMask.layerPath.length == 0) continue;
                    auto transform = layerTransform(layerMask.layerPath, gridResult.grid.uuid);
                    transform.zOffset = zOffset;
                    storeLayerTransform(layerMask.layerPath, gridResult.grid.uuid, transform, true);
                    return true;
                }
            }
            return false;
        }

        PsdDepthImportResult previewForRegressionSmoke() {
            if (previewDirty) rebuildPreview();
            return preview;
        }

        bool hasAppliedDepthsForRegressionSmoke(string gridName) {
            foreach (gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                auto mapped = cast(DepthMappedNode)gridResult.grid;
                if (mapped is null) return false;
                auto depths = mapped.copyDepths();
                return depths !is null && depths.length == gridResult.depths.length && depths == gridResult.depths;
            }
            return false;
        }
    }
}
