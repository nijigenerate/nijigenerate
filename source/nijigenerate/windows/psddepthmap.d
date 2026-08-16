module nijigenerate.windows.psddepthmap;

import bindbc.opengl;
import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.commands;
import nijigenerate.commands.depth.map : PsdDepthComposedView, PsdDepthGpuComposeWork,
    ngApplyPsdDepthImportResult, ngCanApplyPsdDepthImportResult, ngCancelPsdDepthImportGpu,
    ngComposePsdDepthImportResult,
    ngComposePsdDepthTarget, ngPollPsdDepthImportGpu, ngPsdDepthConvolutionToDepthImage,
    ngSubmitPsdDepthImportGpu;
version (CommandBrowserDifferential) {
    import nijigenerate.commands.depth.map : ngPsdDepthComposedViewForRegression;
}
import nijigenerate.core.actionstack : ActionStackScope, incActionCanRedo, incActionCanUndo,
    ngOpenActionStackScope;
import nijigenerate.core.shortcut.base : ngSetSelectedNodesProvider;
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
import nijigenerate.viewport.depth.draw.gpu : ngDepthDrawGpuLayerSampleSupportsConvolution;
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

private struct PsdDepthPendingLayerChange {
    string layerPath;
    ulong targetGridUuid;
}

struct PsdDepthDialogLayerState {
    string layerPath;
    ulong targetGridUuid;
    bool visible;
    bool enabled;
    bool depthEnabled;
    bool invert;
    float depthOffset;
    float depthScale;
    bool outlierPruneEnabled;
    bool puppetFitEnabled;
}

struct PsdDepthDialogSettingsState {
    PsdDepthImportSettings settings;
    PsdDepthDialogLayerState[] layers;
    bool onlyProblemLayers;
}

struct PsdDepthDialogLayerPixels {
    string layerPath;
    ulong targetGridUuid;
    ubyte[] depthRgba;
    bool alphaDepthGapFillApplied;
}

struct PsdDepthDialogPartLayerData {
    string layerPath;
    string layerName;
    string colorLayerPath;
    string colorLayerName;
    string sourcePath;
    int left;
    int top;
    int width;
    int height;
    bool visible;
    bool enabled;
    bool depthEnabled;
    ulong targetGridUuid;
    PsdDepthLayerDepthStats depthStats;
    ubyte[] colorRgba;
    ubyte[] depthRgba;
}

struct PsdDepthDialogPartData {
    ulong targetGridUuid;
    string targetGridName;
    string targetType;
    bool skipped;
    int documentWidth;
    int documentHeight;
    size_t coverageSources;
    size_t sampledVertices;
    size_t missingVertices;
    float minDepth;
    float maxDepth;
    float[] vertexX;
    float[] vertexY;
    float[] depths;
    float[] baseDepths;
    string[] winnerLayerPaths;
    bool[] missingVertexMask;
    int previewLeft;
    int previewTop;
    int previewWidth;
    int previewHeight;
    ubyte[] previewRgba;
    PsdDepthDialogPartLayerData[] layers;
}

struct PsdDepthDialogOverallPreview {
    int width;
    int height;
    float yaw;
    float pitch;
    float zoom;
    float panX;
    float panY;
    size_t coveredPixels;
    float minDepth;
    float maxDepth;
    float[] depths;
    ubyte[] rgba;
}

enum size_t PsdDepthDialogPartDataCaptureBudget = 64 * 1024 * 1024;

private bool reservePsdDepthDialogPartDataBytes(
    ref size_t retainedBytes,
    size_t bytes,
    out string error
) {
    if (bytes > PsdDepthDialogPartDataCaptureBudget - retainedBytes) {
        error = "PSD depth dialog part data exceeds the 64 MiB capture budget";
        return false;
    }
    retainedBytes += bytes;
    return true;
}

private bool reservePsdDepthDialogPartDataElements(
    ref size_t retainedBytes,
    size_t count,
    size_t elementBytes,
    out string error
) {
    if (elementBytes == 0 ||
        count > (PsdDepthDialogPartDataCaptureBudget - retainedBytes) / elementBytes) {
        error = "PSD depth dialog part data exceeds the 64 MiB capture budget";
        return false;
    }
    retainedBytes += count * elementBytes;
    return true;
}

version (CommandBrowserDifferential) {
    bool ngPsdDepthDialogCaptureBudgetAcceptsForRegression(size_t[] sizes) {
        size_t retainedBytes;
        string error;
        foreach (bytes; sizes) {
            if (!reservePsdDepthDialogPartDataBytes(retainedBytes, bytes, error)) return false;
        }
        return true;
    }
}

private __gshared PSDDepthMapWindow activePsdDepthMapWindow;

PSDDepthMapWindow ngActivePsdDepthMapWindow() {
    return activePsdDepthMapWindow;
}

private Node[] psdDepthDialogSelectedNodes() {
    if (activePsdDepthMapWindow is null) return null;
    return activePsdDepthMapWindow.selectedDialogContextNodes();
}

struct PsdDepth3DAdjustSample {
    int x;
    int y;
    ubyte depthByte;
    float sourceDepth;
    float depth;
    ubyte r;
    ubyte g;
    ubyte b;
    ubyte a;
}

struct PsdDepth3DAdjustMesh {
    float[] vertexData;
    float[] sourceDepths;
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
    int layerDepthTransformUniform = -1;
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
uniform vec4 layerDepthTransform;
void main() {
    float cy = yawPitch.x;
    float sy = yawPitch.y;
    float cp = yawPitch.z;
    float sp = yawPitch.w;
    float x = documentPosition.x - center.x;
    float y = documentPosition.y - center.y;
    float depth = documentPosition.z;
    if (abs(layerDepthTransform.x) > 0.000001) {
        depth = (depth - layerDepthTransform.y) *
            (layerDepthTransform.z / layerDepthTransform.x) + layerDepthTransform.w;
    } else {
        depth += layerDepthTransform.w - layerDepthTransform.y;
    }
    float z = -depth * depthDisplayScale;
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
        layerDepthTransformUniform = shader.getUniformLocation("layerDepthTransform");
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
        GLint previousDrawFbo;
        GLint previousReadFbo;
        GLint previousRenderbuffer;
        GLint previousProgram;
        GLint previousVertexArray;
        GLint previousArrayBuffer;
        GLint previousElementArrayBuffer;
        GLint previousActiveTexture;
        GLint previousActiveTexture2D;
        GLint previousTexture0;
        GLint[4] previousViewport;
        GLint previousDepthFunc;
        GLint previousBlendEquationRgb;
        GLint previousBlendEquationAlpha;
        GLint previousBlendSrcRgb;
        GLint previousBlendDstRgb;
        GLint previousBlendSrcAlpha;
        GLint previousBlendDstAlpha;
        GLfloat[4] previousClearColor;
        GLfloat previousClearDepth;
        GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean blendEnabled = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFbo);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &previousElementArrayBuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousActiveTexture2D);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
        glActiveTexture(cast(GLenum)previousActiveTexture);
        glGetIntegerv(GL_VIEWPORT, previousViewport.ptr);
        glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &previousBlendEquationRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &previousBlendEquationAlpha);
        glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor.ptr);
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &previousClearDepth);
        scope(exit) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cast(GLuint)previousDrawFbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, cast(GLuint)previousReadFbo);
            glBindRenderbuffer(GL_RENDERBUFFER, cast(GLuint)previousRenderbuffer);
            glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
            glDepthFunc(cast(GLenum)previousDepthFunc);
            glBlendEquationSeparate(
                cast(GLenum)previousBlendEquationRgb, cast(GLenum)previousBlendEquationAlpha);
            glBlendFuncSeparate(
                cast(GLenum)previousBlendSrcRgb, cast(GLenum)previousBlendDstRgb,
                cast(GLenum)previousBlendSrcAlpha, cast(GLenum)previousBlendDstAlpha);
            glClearColor(previousClearColor[0], previousClearColor[1],
                previousClearColor[2], previousClearColor[3]);
            glClearDepth(previousClearDepth);
            if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            glUseProgram(cast(GLuint)previousProgram);
            glBindVertexArray(cast(GLuint)previousVertexArray);
            glBindBuffer(GL_ARRAY_BUFFER, cast(GLuint)previousArrayBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cast(GLuint)previousElementArrayBuffer);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cast(GLuint)previousTexture0);
            glActiveTexture(cast(GLenum)previousActiveTexture);
            glBindTexture(GL_TEXTURE_2D, cast(GLuint)previousActiveTexture2D);
        }

        if (!ensureTarget(targetWidth, targetHeight)) return null;
        ensureShader();
        ensureBuffers();

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

    bool capturePreview(out int resultWidth, out int resultHeight, out ubyte[] rgba) {
        resultWidth = 0;
        resultHeight = 0;
        rgba = null;
        if (fbo == 0 || texture is null || width <= 0 || height <= 0) return false;

        GLint previousReadFbo;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFbo);
        scope(exit) glBindFramebuffer(GL_READ_FRAMEBUFFER, cast(GLuint)previousReadFbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;

        rgba.length = cast(size_t)width * cast(size_t)height * 4;
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.ptr);
        auto rowBytes = cast(size_t)width * 4;
        ubyte[] row;
        row.length = rowBytes;
        foreach (y; 0 .. height / 2) {
            auto opposite = height - y - 1;
            auto top = cast(size_t)y * rowBytes;
            auto bottom = cast(size_t)opposite * rowBytes;
            row[] = rgba[top .. top + rowBytes];
            rgba[top .. top + rowBytes] = rgba[bottom .. bottom + rowBytes];
            rgba[bottom .. bottom + rowBytes] = row[];
        }
        resultWidth = width;
        resultHeight = height;
        return true;
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
        float depthDisplayScale,
        PsdDepthLayerEdit bakedTransform,
        PsdDepthLayerEdit currentTransform
    ) {
        if (layerTexture is null || mesh.vertexData.length == 0 || mesh.indices.length == 0) return;
        import std.math : cos, sin;

        shader.use();
        shader.setUniform(viewportSizeUniform, vec2(cast(float)width, cast(float)height));
        shader.setUniform(centerUniform, vec2(centerX, centerY));
        shader.setUniform(yawPitchUniform, vec4(cos(yaw), sin(yaw), cos(pitch), sin(pitch)));
        shader.setUniform(zoomPanUniform, vec4(zoom, pan.x, pan.y, 0.0f));
        shader.setUniform(depthScaleUniform, depthDisplayScale);
        shader.setUniform(layerDepthTransformUniform, vec4(
            bakedTransform.zScale,
            bakedTransform.zOffset,
            currentTransform.zScale,
            currentTransform.zOffset
        ));
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
    PsdDepthGpuComposeWork pendingPreviewGpuComposition;
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
    ubyte[] threeDAdjustPreviewCaptureRgba;
    int threeDAdjustPreviewCaptureWidth;
    int threeDAdjustPreviewCaptureHeight;
    bool threeDAdjustPreviewDirty = true;
    PsdDepthPendingLayerChange[] pending3DAdjustLayerChanges;
    size_t last3DAdjustApplyRecomposedGridCount;
    PsdDepthLayerEdit[string] threeDAdjustPreparedTransforms;
    size_t[][string] threeDAdjustFrontIntersections;
    bool[string] threeDAdjustDisplayDepthDirty;
    ActionStackScope dialogActionScope;
    CommandScopeRegistration dialogCommandScope;
    bool dialogDisplayed;
    PsdDepthDialogLayerState[] pendingLayerStatesAfterRebuild;
    bool[string] alphaDepthGapFillAppliedByLayer;
    version (CommandBrowserDifferential) bool failNextPreviewCompositionForRegression;
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

    string cleanupLayerKey(string layerPath, ulong targetGridUuid) {
        return "%s\n%s".format(layerPath, targetGridUuid);
    }

    bool applyAlphaDepthGapFillToLayer(ref PsdDepthComposedLayer layer, size_t layerIndex) {
        static immutable DepthDrawAlphaDepthFocusedRule[] focusedRules = [
            DepthDrawAlphaDepthFocusedRule(17, 72, 0, 122, 132, 10, 18),
            DepthDrawAlphaDepthFocusedRule(6, 0, 0, 112, 126, 10, 20),
            DepthDrawAlphaDepthFocusedRule(6, 40, 0, 150, 170, 12, 24),
            DepthDrawAlphaDepthFocusedRule(10, 0, 0, 96, 120, 8, 16),
            DepthDrawAlphaDepthFocusedRule(14, 0, 44, 96, 156, 10, 20),
        ];
        auto pixelCount = cast(size_t)max(0, layer.width * layer.height);
        if (pixelCount == 0 || layer.depthRgba.length != pixelCount * 4 ||
            layer.maskRgba.length != pixelCount * 4) return false;

        auto depth = ngDepthDrawDecodeGrayscaleDepthPixelsFromRgba(layer.depthRgba);
        ubyte[] mask;
        mask.length = pixelCount;
        foreach (i; 0 .. pixelCount) {
            auto offset = i * 4;
            mask[i] = layer.maskRgba[offset] != 0 && layer.maskRgba[offset + 3] != 0 ? 1 : 0;
        }
        auto detected = ngDepthDrawDetectAlphaDepthGaps(
            depth, mask, layer.width, layer.height, cast(int)layerIndex, focusedRules);
        auto filled = ngDepthDrawMedianFillDepth(depth, mask, detected.mask, layer.width, layer.height);
        bool changed;
        foreach (i, value; filled.depth) {
            if (!detected.mask[i] || depth[i] == value) continue;
            auto offset = i * 4;
            changed = true;
            layer.depthRgba[offset] = value;
            layer.depthRgba[offset + 1] = value;
            layer.depthRgba[offset + 2] = value;
            layer.depthRgba[offset + 3] = mask[i] && value > 0 ? 255 : 0;
        }
        return changed;
    }

    void replayAlphaDepthGapFills() {
        foreach (layerIndex, ref layer; preview.composedLayers) {
            auto applied = cleanupLayerKey(layer.layerPath, layer.targetGridUuid) in alphaDepthGapFillAppliedByLayer;
            if (applied is null || !*applied) continue;
            applyAlphaDepthGapFillToLayer(layer, layerIndex);
        }
    }

    void rebuildPreview() {
        cancelPendingPreviewGpuComposition();
        disposePreviewTextures();
        pending3DAdjustLayerChanges = null;
        auto restoredLayerStates = pendingLayerStatesAfterRebuild;
        pendingLayerStatesAfterRebuild = null;
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
            foreach (state; restoredLayerStates) {
                if (auto layer = findComposedLayer(state.layerPath, state.targetGridUuid)) {
                    applyDialogLayerStateFields(*layer, state);
                }
            }
            replayAlphaDepthGapFills();
            startPreviewComposition();
        }
        previewDirty = false;
    }

    bool previewCompositionPending() const {
        return pendingPreviewGpuComposition.isActive();
    }

    void cancelPendingPreviewGpuComposition() {
        if (pendingPreviewGpuComposition.isActive()) {
            ngCancelPsdDepthImportGpu(pendingPreviewGpuComposition);
        }
    }

    bool startPreviewComposition() {
        cancelPendingPreviewGpuComposition();
        composedPreview = PsdDepthComposedView.init;
        string composeError;
        version (CommandBrowserDifferential) {
            if (failNextPreviewCompositionForRegression) {
                failNextPreviewCompositionForRegression = false;
                errorMessage = "Forced PSD depth preview composition failure";
                return false;
            }
        }
        if (!preview.gpuCompositionRequested) {
            if (!ngComposePsdDepthImportResult(preview, composedPreview, composeError)) {
                errorMessage = composeError.length ? composeError : "PSD depth map composition failed";
                return false;
            }
            errorMessage = null;
            return true;
        }

        PsdDepthGpuComposeWork work;
        if (!ngSubmitPsdDepthImportGpu(preview, work, composeError)) {
            errorMessage = composeError.length ? composeError : "PSD depth map GPU composition failed";
            return false;
        }
        bool ready;
        if (!ngPollPsdDepthImportGpu(preview, work, ready, composedPreview, composeError)) {
            errorMessage = composeError.length ? composeError : "PSD depth map GPU composition failed";
            return false;
        }
        if (!ready) pendingPreviewGpuComposition = work;
        errorMessage = null;
        return true;
    }

    void pollPendingPreviewGpuComposition() {
        if (!pendingPreviewGpuComposition.isActive()) return;
        bool ready;
        string composeError;
        if (!ngPollPsdDepthImportGpu(preview, pendingPreviewGpuComposition,
            ready, composedPreview, composeError)) {
            errorMessage = composeError.length ? composeError : "PSD depth map GPU composition failed";
            return;
        }
        if (ready) {
            errorMessage = null;
            lastApplyErrorMessage = null;
        }
    }

    void applyAlphaDepthGapFill() {
        if (previewDirty) rebuildPreview();
        if (errorMessage.length) return;

        foreach (layerIndex, ref layer; preview.composedLayers) {
            if (applyAlphaDepthGapFillToLayer(layer, layerIndex)) {
                alphaDepthGapFillAppliedByLayer[cleanupLayerKey(layer.layerPath, layer.targetGridUuid)] = true;
            }
        }

        disposePreviewTextures();
        startPreviewComposition();
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
        threeDAdjustPreparedTransforms = null;
        threeDAdjustFrontIntersections = null;
        threeDAdjustDisplayDepthDirty = null;
        threeDAdjustSamplesPrepared = false;
        if (threeDAdjustGpuRenderer !is null) threeDAdjustGpuRenderer.dispose();
        threeDAdjustGpuRenderer = null;
        threeDAdjustPreviewTexture = null;
        threeDAdjustPreviewWidth = 0;
        threeDAdjustPreviewHeight = 0;
        threeDAdjustPreviewCaptureRgba = null;
        threeDAdjustPreviewCaptureWidth = 0;
        threeDAdjustPreviewCaptureHeight = 0;
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
        if (previewCompositionPending()) {
            lines ~= _("GPU preview composition is still running.");
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

    Context dialogCommandContext() {
        auto ctx = new Context();
        auto puppet = incActivePuppet();
        if (puppet !is null) ctx.puppet = puppet;
        auto nodes = selectedDialogContextNodes();
        if (nodes.length) ctx.nodes = nodes;
        return ctx;
    }

    Context dialogLayerCommandContext(string layerPath, ulong targetGridUuid) {
        auto ctx = new Context();
        auto puppet = incActivePuppet();
        if (puppet !is null) ctx.puppet = puppet;
        auto nodes = dialogContextNodesForLayer(layerPath, targetGridUuid);
        if (nodes.length) ctx.nodes = nodes;
        return ctx;
    }

    Context dialogNodeCommandContext(Node node) {
        auto ctx = new Context();
        auto puppet = incActivePuppet();
        if (puppet !is null) ctx.puppet = puppet;
        if (node !is null) ctx.nodes = [node];
        return ctx;
    }

    bool drawEnumCombo(string label, ref PsdDepthConvolution value) {
        auto current = ngPsdDepthConvolutionName(value);
        bool changed;
        if (igBeginCombo(label.toStringz, current.toStringz)) {
            foreach (name; ConvolutionNames) {
                auto option = ngPsdDepthConvolutionFromString(name);
                if (settings.useGpuComposition && !ngDepthDrawGpuLayerSampleSupportsConvolution(
                    cast(int)ngPsdDepthConvolutionToDepthImage(option))) continue;
                bool selected = name == current;
                if (igSelectable(name.toStringz, selected)) {
                    value = option;
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
                auto ctx = dialogLayerCommandContext(mapping.layerPath, mapping.targetGridUuid);
                changed = cmd!(PsdDepthDialogCommand.SetPsdDepthDialogLayerMappingAuto)(
                    ctx, mapping.layerPath).succeeded;
            }
            bool ignoreSelected = (mapping.layerPath in settings.ignoredLayerPaths) !is null;
            if (igSelectable(_("Ignore").toStringz, ignoreSelected)) {
                auto ctx = dialogLayerCommandContext(mapping.layerPath, mapping.targetGridUuid);
                changed = cmd!(PsdDepthDialogCommand.SetPsdDepthDialogLayerMappingIgnored)(
                    ctx, mapping.layerPath).succeeded;
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
                    auto ctx = dialogLayerCommandContext(mapping.layerPath, mapping.targetGridUuid);
                    changed = cmd!(PsdDepthDialogCommand.SetPsdDepthDialogLayerMappingTarget)(
                        ctx, mapping.layerPath, cast(Node)grid).succeeded;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    void drawOptions() {
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
                auto ctx = dialogCommandContext();
                cmd!(PsdDepthDialogCommand.SetPsdDepthDialogColorSource)(ctx, colorPath);
            }
        }
        if (settings.colorSourcePath.length) {
            igSameLine();
            if (igButton(__("Clear Color Source"))) {
                auto ctx = dialogCommandContext();
                cmd!(PsdDepthDialogCommand.SetPsdDepthDialogColorSource)(ctx, "");
            }
        }
        auto invert = settings.invert;
        if (ngCheckbox(__("Invert Depth"), &invert)) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogInvert)(ctx, invert);
        }
        incTooltip(_("Default: white is front and black is back."));
        auto backDepth = settings.backDepth;
        if (igDragFloat(__("Back Depth"), &backDepth, 0.01f, -10.0f, 10.0f, "%.3f")) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogBackDepth)(ctx, backDepth);
        }
        auto frontDepth = settings.frontDepth;
        if (igDragFloat(__("Front Depth"), &frontDepth, 0.01f, -10.0f, 10.0f, "%.3f")) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogFrontDepth)(ctx, frontDepth);
        }
        auto depthScale = settings.depthScale;
        if (igDragFloat(__("Depth Scale"), &depthScale, 0.01f, 0.0f, 100.0f, "%.3f")) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogDepthScale)(ctx, depthScale);
        }
        incTooltip(_("Imported depth values are multiplied by this scale before applying."));
        auto channel = settings.channel;
        if (drawEnumCombo(_("Channel"), channel)) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogChannel)(ctx, channel);
        }
        auto convolution = settings.convolution;
        if (drawEnumCombo(_("Sampling"), convolution)) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogSampling)(ctx, convolution);
        }
        if (isCustomConvolution()) {
            auto customRadius = settings.customRadius;
            if (igDragInt(__("Custom Radius"), &customRadius, 0.1f, 1, 64)) {
                auto ctx = dialogCommandContext();
                cmd!(PsdDepthDialogCommand.SetPsdDepthDialogCustomRadius)(ctx, customRadius);
            }
        }
        auto alphaThreshold = settings.alphaThreshold;
        if (igDragFloat(__("Alpha Threshold"), &alphaThreshold, 0.001f, 0.0f, 1.0f, "%.3f")) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogAlphaThreshold)(ctx, alphaThreshold);
        }
        auto missingPolicy = settings.missingPolicy;
        if (drawEnumCombo(_("Missing Vertex Pixel"), missingPolicy)) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogMissingPolicy)(ctx, missingPolicy);
        }
        auto repairContourBand = settings.repairContourBand;
        if (ngCheckbox(__("Repair contour band"), &repairContourBand)) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogContourRepair)(ctx, repairContourBand);
        }
        auto smoothWavySurface = settings.smoothWavySurface;
        if (ngCheckbox(__("Smooth wavy surface"), &smoothWavySurface)) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogSurfaceSmoothing)(ctx, smoothWavySurface);
        }
        if (igButton(__("Fill alpha-depth gaps"))) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.FillPsdDepthDialogAlphaDepthGaps)(ctx);
        }
        auto useGpuComposition = settings.useGpuComposition;
        if (ngCheckbox(__("GPU Composition"), &useGpuComposition)) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogGpuComposition)(ctx, useGpuComposition);
        }
        incTooltip(_("When enabled, apply must use the GPU composition path. CPU fallback is treated as an error."));
        auto matchDirectGridName = settings.matchDirectGridName;
        if (ngCheckbox(__("Direct Grid Name Match"), &matchDirectGridName)) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogDirectGridMatch)(ctx, matchDirectGridName);
        }
        incTooltip(_("Also match PSD layer names directly against GridDeformer names."));
        auto problemFilter = onlyProblemLayers;
        if (ngCheckbox(__("Only show problem layers"), &problemFilter)) {
            auto ctx = dialogCommandContext();
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogProblemFilter)(ctx, problemFilter);
        }
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
            auto ctx = dialogNodeCommandContext(grid);
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogTargetEnabled)(ctx, enabled);
        }
    }

    void drawComposedLayerEnabledCheckbox(string layerPath, ulong targetGridUuid) {
        auto layer = findComposedLayer(layerPath, targetGridUuid);
        if (layer is null) return;
        bool enabled = layer.enabled;
        auto widgetKey = "###useComposedLayer" ~ layerPath ~ targetGridUuid.to!string;
        if (ngCheckbox(widgetKey.toStringz, &enabled)) {
            auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogLayerEnabled)(ctx, layerPath, enabled);
        }
    }

    void drawComposedLayerShowCheckbox(string layerPath, ulong targetGridUuid) {
        auto layer = findComposedLayer(layerPath, targetGridUuid);
        if (layer is null) return;
        bool visible = layer.visible;
        igPushID(("showComposedLayer" ~ layerPath ~ targetGridUuid.to!string).toStringz);
        scope(exit) igPopID();
        if (ngCheckbox(__("Show"), &visible)) {
            auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogLayerVisible)(ctx, layerPath, visible);
        }
    }

    void drawComposedLayerDepthEnabledCheckbox(string layerPath, ulong targetGridUuid) {
        auto layer = findComposedLayer(layerPath, targetGridUuid);
        if (layer is null) return;
        bool enabled = layer.depthEnabled;
        igPushID(("depthEnabledComposedLayer" ~ layerPath ~ targetGridUuid.to!string).toStringz);
        scope(exit) igPopID();
        if (ngCheckbox(__("Enable Depth"), &enabled)) {
            auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogLayerDepthEnabled)(ctx, layerPath, enabled);
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
        threeDAdjustPreparedTransforms = null;
        threeDAdjustFrontIntersections = null;
        threeDAdjustDisplayDepthDirty = null;
        threeDAdjustSamplesPrepared = false;
        threeDAdjustPreviewDirty = true;
    }

    void refreshAfter3DAdjustLayerChange(string layerPath, ulong targetGridUuid) {
        invalidate3DAdjustLayerCaches(layerPath, targetGridUuid);
        if (preview.gpuCompositionRequested) {
            startPreviewComposition();
            pending3DAdjustLayerChanges = null;
            return;
        }
        refreshPreviewDepthsForLayer(layerPath, targetGridUuid);
    }

    void ensureDialogActionScope() {
        if (dialogActionScope is null || !dialogActionScope.isActive()) {
            dialogActionScope = ngOpenActionStackScope();
        }
    }

    void activateDialogCommandContext() {
        activePsdDepthMapWindow = this;
        ngSetSelectedNodesProvider(&psdDepthDialogSelectedNodes);
        if (dialogCommandScope is null || !dialogCommandScope.isActive()) {
            dialogCommandScope = ngPushCommandScope(
                ngCommandScope!PsdDepthDialogCommandScope());
        }
    }

    void deactivateDialogCommandContext() {
        if (activePsdDepthMapWindow is this) {
            activePsdDepthMapWindow = null;
            ngSetSelectedNodesProvider(null);
        }
        if (dialogCommandScope !is null) {
            dialogCommandScope.close();
            dialogCommandScope = null;
        }
    }

    void closeDialogActionScope() {
        if (dialogActionScope !is null) {
            dialogActionScope.close();
            dialogActionScope = null;
        }
    }

    static PsdDepthImportSettings cloneDialogSettings(PsdDepthImportSettings source) {
        PsdDepthImportSettings result = source;
        result.layerTargetGridUuidOverrides = null;
        foreach (key, value; source.layerTargetGridUuidOverrides) {
            result.layerTargetGridUuidOverrides[key] = value;
        }
        result.ignoredLayerPaths = null;
        foreach (key, value; source.ignoredLayerPaths) {
            result.ignoredLayerPaths[key] = value;
        }
        result.disabledGridUuids = null;
        foreach (key, value; source.disabledGridUuids) {
            result.disabledGridUuids[key] = value;
        }
        result.disabledGridLayerKeys = null;
        foreach (key, value; source.disabledGridLayerKeys) {
            result.disabledGridLayerKeys[key] = value;
        }
        return result;
    }

    static PsdDepthDialogLayerState dialogLayerState(ref PsdDepthComposedLayer layer) {
        PsdDepthDialogLayerState result;
        result.layerPath = layer.layerPath;
        result.targetGridUuid = layer.targetGridUuid;
        result.visible = layer.visible;
        result.enabled = layer.enabled;
        result.depthEnabled = layer.depthEnabled;
        result.invert = layer.invert;
        result.depthOffset = layer.depthOffset;
        result.depthScale = layer.depthScale;
        result.outlierPruneEnabled = layer.outlierPruneEnabled;
        result.puppetFitEnabled = layer.puppetFitEnabled;
        return result;
    }

    static bool sameDialogLayerState(PsdDepthDialogLayerState a, PsdDepthDialogLayerState b) {
        return a.layerPath == b.layerPath &&
            a.targetGridUuid == b.targetGridUuid &&
            a.visible == b.visible &&
            a.enabled == b.enabled &&
            a.depthEnabled == b.depthEnabled &&
            a.invert == b.invert &&
            a.depthOffset == b.depthOffset &&
            a.depthScale == b.depthScale &&
            a.outlierPruneEnabled == b.outlierPruneEnabled &&
            a.puppetFitEnabled == b.puppetFitEnabled;
    }

    static void applyDialogLayerStateFields(ref PsdDepthComposedLayer layer, PsdDepthDialogLayerState state) {
        layer.visible = state.visible;
        layer.enabled = state.enabled;
        layer.depthEnabled = state.depthEnabled;
        layer.invert = state.invert;
        layer.depthOffset = state.depthOffset;
        layer.depthScale = state.depthScale;
        layer.outlierPruneEnabled = state.outlierPruneEnabled;
        layer.puppetFitEnabled = state.puppetFitEnabled;
    }

    void stage3DAdjustLayerChange(string layerPath, ulong targetGridUuid) {
        // A layer remains pending until Apply, but every edit still has to
        // refresh its display mesh. Do this before deduplicating the Apply work.
        threeDAdjustDisplayDepthDirty[layerCacheKey(layerPath, targetGridUuid)] = true;
        threeDAdjustPreviewDirty = true;
        foreach (pending; pending3DAdjustLayerChanges) {
            if (pending.layerPath == layerPath && pending.targetGridUuid == targetGridUuid) return;
        }
        pending3DAdjustLayerChanges ~= PsdDepthPendingLayerChange(layerPath, targetGridUuid);
    }

    bool compose3DAdjustChangesForApply() {
        last3DAdjustApplyRecomposedGridCount = 0;
        if (pending3DAdjustLayerChanges.length == 0) return true;

        bool[ulong] targetGridUuids;
        foreach (change; pending3DAdjustLayerChanges) {
            if (change.targetGridUuid != 0) {
                targetGridUuids[change.targetGridUuid] = true;
                continue;
            }
            foreach (ref gridResult; preview.grids) {
                if (gridResult.grid is null || !gridResultUsesLayer(gridResult, change.layerPath)) continue;
                targetGridUuids[gridResult.grid.uuid] = true;
            }
        }

        foreach (i, ref gridResult; preview.grids) {
            if (gridResult.grid is null || gridResult.grid.uuid !in targetGridUuids) continue;
            PsdDepthGridResult composedGrid;
            string composeError;
            if (!ngComposePsdDepthTarget(preview, gridResult, composedGrid, composeError)) {
                errorMessage = composeError.length ? composeError : "PSD depth map composition failed";
                return false;
            }
            preview.grids[i] = composedGrid;
            last3DAdjustApplyRecomposedGridCount++;
        }
        if (last3DAdjustApplyRecomposedGridCount != targetGridUuids.length) {
            errorMessage = "PSD depth map composition target was not found";
            return false;
        }
        errorMessage = null;
        pending3DAdjustLayerChanges = null;
        return true;
    }

    void drawLayerZControls(string layerPath, ulong targetGridUuid) {
        igPushID(("layerZControls" ~ layerPath ~ targetGridUuid.to!string).toStringz);
        scope(exit) igPopID();
        auto transform = layerTransform(layerPath, targetGridUuid);
        if (ngCheckbox(__("Invert Depth"), &transform.invert)) {
            auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogLayerDepthInverted)(
                ctx, layerPath, transform.invert);
        }
        if (igDragFloat(__("Z Scale"),
            &transform.zScale, 0.01f, -100.0f, 100.0f, "%.3f")) {
            auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogLayerDepthScale)(
                ctx, layerPath, transform.zScale);
        }
        if (igDragFloat(__("Z Offset"),
            &transform.zOffset, 0.01f, -100.0f, 100.0f, "%.3f")) {
            auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
            cmd!(PsdDepthDialogCommand.SetPsdDepthDialogLayerDepthOffset)(
                ctx, layerPath, transform.zOffset);
        }
        if (incButtonColored(__("Reset Z"), ImVec2(120, 0))) {
            auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
            cmd!(PsdDepthDialogCommand.ResetPsdDepthDialogLayerDepthTransform)(ctx, layerPath);
        }
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

    bool buildThreeDAdjustOverallPreview(
        int framebufferWidth,
        int framebufferHeight,
        out PsdDepthDialogOverallPreview result
    ) {
        import std.math : cos, sin;

        result = PsdDepthDialogOverallPreview.init;
        framebufferWidth = max(1, framebufferWidth);
        framebufferHeight = max(1, framebufferHeight);

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
        foreach (ref layerPreview; preview.composedLayers) {
            if (threeDAdjustLayerSamples(layerPreview).length == 0) continue;
            includeBounds(
                layerPreview.left,
                layerPreview.top,
                layerPreview.left + layerPreview.width,
                layerPreview.top + layerPreview.height
            );
        }
        if (!hasBounds) {
            includeBounds(0, 0, max(1, preview.compositionWidth), max(1, preview.compositionHeight));
        }
        if (right <= left) right = left + 1;
        if (bottom <= top) bottom = top + 1;

        auto centerX = (cast(float)left + cast(float)right) * 0.5f;
        auto centerY = (cast(float)top + cast(float)bottom) * 0.5f;
        auto sourceWidth = cast(float)(right - left);
        auto sourceHeight = cast(float)(bottom - top);
        auto canvasSize = ImVec2(cast(float)framebufferWidth, cast(float)framebufferHeight);
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
            resetPsdDepth3DAdjustCameraToBounds(sourceWidth, sourceHeight, canvasSize);
        }

        result.width = framebufferWidth;
        result.height = framebufferHeight;
        result.yaw = threeDAdjustCamera.yaw;
        result.pitch = threeDAdjustCamera.pitch;
        result.zoom = threeDAdjustCamera.zoom;
        result.panX = threeDAdjustCamera.pan.x;
        result.panY = threeDAdjustCamera.pan.y;
        result.rgba.length = cast(size_t)framebufferWidth * cast(size_t)framebufferHeight * 4;
        result.depths.length = cast(size_t)framebufferWidth * cast(size_t)framebufferHeight;
        float[] zBuffer;
        zBuffer.length = cast(size_t)framebufferWidth * cast(size_t)framebufferHeight;
        foreach (i; 0 .. zBuffer.length) {
            auto pixelIndex = i * 4;
            result.rgba[pixelIndex + 0] = 20;
            result.rgba[pixelIndex + 1] = 23;
            result.rgba[pixelIndex + 2] = 26;
            result.rgba[pixelIndex + 3] = 255;
            result.depths[i] = float.nan;
            zBuffer[i] = float.max;
        }

        auto yawCos = cos(threeDAdjustCamera.yaw);
        auto yawSin = sin(threeDAdjustCamera.yaw);
        auto pitchCos = cos(threeDAdjustCamera.pitch);
        auto pitchSin = sin(threeDAdjustCamera.pitch);
        auto depthDisplayScale = threeDAdjustDepthDisplayScale();

        foreach (ref layerPreview; preview.composedLayers) {
            auto samples = threeDAdjustLayerSamples(layerPreview);
            foreach (sample; samples) {
                auto depth = threeDAdjustConstrainedDisplayDepth(layerPreview, sample);
                auto documentX = cast(float)layerPreview.left + cast(float)sample.x;
                auto documentY = cast(float)layerPreview.top + cast(float)sample.y;
                auto x = documentX - centerX;
                auto y = documentY - centerY;
                auto z = -depth * depthDisplayScale;
                auto rotatedX = x * yawCos + z * yawSin;
                auto rotatedZ = -x * yawSin + z * yawCos;
                auto rotatedY = y * pitchCos - rotatedZ * pitchSin;
                auto cameraDepth = y * pitchSin + rotatedZ * pitchCos;
                auto screenX = framebufferWidth * 0.5f +
                    rotatedX * threeDAdjustCamera.zoom + threeDAdjustCamera.pan.x;
                auto screenY = framebufferHeight * 0.5f +
                    rotatedY * threeDAdjustCamera.zoom + threeDAdjustCamera.pan.y;
                auto sx = cast(int)round(screenX);
                auto sy = cast(int)round(screenY);
                auto radius = 1;
                foreach (dy; -radius .. radius + 1) {
                    auto pixelY = sy + dy;
                    if (pixelY < 0 || pixelY >= framebufferHeight) continue;
                    foreach (dx; -radius .. radius + 1) {
                        auto pixelX = sx + dx;
                        if (pixelX < 0 || pixelX >= framebufferWidth) continue;
                        auto zIndex = cast(size_t)pixelY * cast(size_t)framebufferWidth +
                            cast(size_t)pixelX;
                        if (cameraDepth >= zBuffer[zIndex]) continue;

                        auto alpha = cast(float)sample.a / 255.0f;
                        auto pixelIndex = zIndex * 4;
                        result.rgba[pixelIndex + 0] = cast(ubyte)clamp(cast(int)(
                            cast(float)sample.r * alpha +
                            cast(float)result.rgba[pixelIndex + 0] * (1.0f - alpha) + 0.5f
                        ), 0, 255);
                        result.rgba[pixelIndex + 1] = cast(ubyte)clamp(cast(int)(
                            cast(float)sample.g * alpha +
                            cast(float)result.rgba[pixelIndex + 1] * (1.0f - alpha) + 0.5f
                        ), 0, 255);
                        result.rgba[pixelIndex + 2] = cast(ubyte)clamp(cast(int)(
                            cast(float)sample.b * alpha +
                            cast(float)result.rgba[pixelIndex + 2] * (1.0f - alpha) + 0.5f
                        ), 0, 255);
                        result.rgba[pixelIndex + 3] = 255;
                        result.depths[zIndex] = depth;
                        zBuffer[zIndex] = cameraDepth;
                    }
                }
            }
        }
        result.minDepth = float.max;
        result.maxDepth = -float.max;
        foreach (depth; result.depths) {
            if (depth != depth) continue;
            result.coveredPixels++;
            result.minDepth = min(result.minDepth, depth);
            result.maxDepth = max(result.maxDepth, depth);
        }
        if (result.coveredPixels == 0) {
            result.minDepth = 0.0f;
            result.maxDepth = 0.0f;
        }
        return true;
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
        threeDAdjustPreparedTransforms = null;

        foreach (ref layerPreview; preview.composedLayers) {
            auto key = layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid);
            threeDAdjustPreparedTransforms[key] = layerTransform(
                layerPreview.layerPath, layerPreview.targetGridUuid);
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
                    sample.sourceDepth = depth;
                    sample.depth = depth;
                    sample.r = layerPreview.colorRgba[rgbaIndex + 0];
                    sample.g = layerPreview.colorRgba[rgbaIndex + 1];
                    sample.b = layerPreview.colorRgba[rgbaIndex + 2];
                    sample.a = layerPreview.colorRgba[rgbaIndex + 3];
                    samples ~= sample;
                }
            }
            threeDAdjustSamples[key] = samples;
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

    PsdDepthLayerEdit threeDAdjustPreparedTransform(ref PsdDepthComposedLayer layerPreview) {
        prepareThreeDAdjustSamples();
        auto key = layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid);
        if (auto transform = key in threeDAdjustPreparedTransforms) return *transform;
        return layerTransform(layerPreview.layerPath, layerPreview.targetGridUuid);
    }

    float threeDAdjustDisplayDepth(ref PsdDepthComposedLayer layerPreview, float bakedDepth) {
        auto baked = threeDAdjustPreparedTransform(layerPreview);
        auto current = layerTransform(layerPreview.layerPath, layerPreview.targetGridUuid);
        if (abs(baked.zScale) > 0.000001f) {
            return (bakedDepth - baked.zOffset) * (current.zScale / baked.zScale) + current.zOffset;
        }
        return bakedDepth + current.zOffset - baked.zOffset;
    }

    PsdDepth3DAdjustSample* threeDAdjustSampleAtDocument(
        ref PsdDepthComposedLayer layerPreview,
        int documentX,
        int documentY
    ) {
        auto x = documentX - layerPreview.left;
        auto y = documentY - layerPreview.top;
        if (x < 0 || y < 0 || x >= layerPreview.width || y >= layerPreview.height) return null;
        auto key = layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid);
        auto samples = key in threeDAdjustSamples;
        if (samples is null) return null;
        auto wanted = y * layerPreview.width + x;
        ptrdiff_t low;
        auto high = cast(ptrdiff_t)(*samples).length - 1;
        while (low <= high) {
            auto middle = low + (high - low) / 2;
            auto found = (*samples)[cast(size_t)middle].y * layerPreview.width +
                (*samples)[cast(size_t)middle].x;
            if (found < wanted) low = middle + 1;
            else if (found > wanted) high = middle - 1;
            else return &(*samples)[cast(size_t)middle];
        }
        return null;
    }

    bool threeDAdjustLayersIntersect(
        ref PsdDepthComposedLayer backLayer,
        ref PsdDepthComposedLayer frontLayer
    ) {
        if (backLayer.left >= frontLayer.left + frontLayer.width ||
            frontLayer.left >= backLayer.left + backLayer.width ||
            backLayer.top >= frontLayer.top + frontLayer.height ||
            frontLayer.top >= backLayer.top + backLayer.height) return false;

        auto backSamples = threeDAdjustLayerSamples(backLayer);
        auto frontSamples = threeDAdjustLayerSamples(frontLayer);
        if (backSamples.length <= frontSamples.length) {
            foreach (ref sample; backSamples) {
                if (threeDAdjustSampleAtDocument(frontLayer,
                    backLayer.left + sample.x, backLayer.top + sample.y) !is null) return true;
            }
        } else {
            foreach (ref sample; frontSamples) {
                if (threeDAdjustSampleAtDocument(backLayer,
                    frontLayer.left + sample.x, frontLayer.top + sample.y) !is null) return true;
            }
        }
        return false;
    }

    size_t[] threeDAdjustFrontIntersectingLayerIndices(ref PsdDepthComposedLayer layerPreview) {
        prepareThreeDAdjustSamples();
        auto key = layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid);
        if (auto cached = key in threeDAdjustFrontIntersections) return *cached;

        size_t[] result;
        ptrdiff_t layerIndex = -1;
        foreach (i, ref candidate; preview.composedLayers) {
            if (&candidate is &layerPreview) {
                layerIndex = cast(ptrdiff_t)i;
                break;
            }
        }
        if (layerIndex >= 0) {
            // composedLayers is stored back-to-front. Only later layers can
            // constrain this one, and only when their visible pixels overlap.
            foreach (i; cast(size_t)layerIndex + 1 .. preview.composedLayers.length) {
                auto frontLayer = &preview.composedLayers[i];
                if (!frontLayer.enabled || !frontLayer.visible) continue;
                if (threeDAdjustLayersIntersect(layerPreview, *frontLayer)) result ~= i;
            }
        }
        threeDAdjustFrontIntersections[key] = result;
        return result;
    }

    float threeDAdjustConstrainedDisplayDepth(
        ref PsdDepthComposedLayer layerPreview,
        ref PsdDepth3DAdjustSample sample
    ) {
        auto depth = threeDAdjustDisplayDepth(layerPreview, sample.sourceDepth);
        auto depthStep = max(abs((layerPreview.frontDepth - layerPreview.backDepth) *
            layerPreview.sourceDepthScale * layerPreview.depthScale) / 255.0f, 0.000001f);
        auto documentX = layerPreview.left + sample.x;
        auto documentY = layerPreview.top + sample.y;
        foreach (frontIndex; threeDAdjustFrontIntersectingLayerIndices(layerPreview)) {
            auto frontLayer = &preview.composedLayers[frontIndex];
            auto frontSample = threeDAdjustSampleAtDocument(*frontLayer, documentX, documentY);
            if (frontSample is null) continue;
            // Comparing against every overlapping front layer directly is
            // sufficient to preserve ordering and avoids recursive duplicate
            // work when several front layers overlap the same pixel.
            depth = min(depth,
                threeDAdjustDisplayDepth(*frontLayer, frontSample.sourceDepth) - depthStep);
        }
        return depth;
    }

    void updateThreeDAdjustMeshDisplayDepths(
        ref PsdDepthComposedLayer layerPreview,
        ref PsdDepth3DAdjustMesh mesh
    ) {
        auto key = layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid);
        auto dirty = key in threeDAdjustDisplayDepthDirty;
        if (dirty is null || !*dirty || mesh.sourceDepths.length * 5 != mesh.vertexData.length) return;

        foreach (vertexIndex, sourceDepth; mesh.sourceDepths) {
            auto offset = vertexIndex * 5;
            PsdDepth3DAdjustSample sample;
            sample.x = cast(int)round(mesh.vertexData[offset] - cast(float)layerPreview.left);
            sample.y = cast(int)round(mesh.vertexData[offset + 1] - cast(float)layerPreview.top);
            sample.sourceDepth = sourceDepth;
            sample.depth = sourceDepth;
            mesh.vertexData[offset + 2] = threeDAdjustConstrainedDisplayDepth(layerPreview, sample);
        }
        threeDAdjustDisplayDepthDirty.remove(key);
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
                auto sourceSample = threeDAdjustSampleAtDocument(
                    layerPreview, layerPreview.left + x, layerPreview.top + y);
                if (sourceSample is null) continue;

                auto vertexIndex = cast(int)(mesh.vertexData.length / 5);
                lookup[cast(size_t)gy * cast(size_t)cols + cast(size_t)gx] = vertexIndex;
                mesh.vertexData ~= cast(float)layerPreview.left + cast(float)x;
                mesh.vertexData ~= cast(float)layerPreview.top + cast(float)y;
                mesh.vertexData ~= depth;
                mesh.sourceDepths ~= sourceSample.sourceDepth;
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
                    updateThreeDAdjustMeshDisplayDepths(layerPreview, *mesh);
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
                        depthDisplayScale,
                        layerTransform(layerPreview.layerPath, layerPreview.targetGridUuid),
                        layerTransform(layerPreview.layerPath, layerPreview.targetGridUuid)
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
                rasterizeDepthSample(sample, threeDAdjustConstrainedDisplayDepth(layerPreview, sample));
            }
        }

        threeDAdjustPreviewCaptureRgba = framebuffer.dup;
        threeDAdjustPreviewCaptureWidth = framebufferWidth;
        threeDAdjustPreviewCaptureHeight = framebufferHeight;
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

    bool apply() {
        if (previewDirty) rebuildPreview();
        if (previewCompositionPending()) {
            lastApplyErrorMessage = _("GPU preview composition is still running.");
            return false;
        }
        // Apply consumes preview.grids directly. Recompose only the grids named by
        // edited layers, using the same target composer as the original path.
        if (!compose3DAdjustChangesForApply()) {
            incDialog(__("Error"), errorMessage);
            return false;
        }
        if (errorMessage.length) {
            incDialog(__("Error"), errorMessage);
            return false;
        }
        string validationError;
        if (!ngCanApplyPsdDepthImportResult(composedPreview, validationError)) {
            lastApplyErrorMessage = validationError;
            incDialog(__("Error"), validationError);
            return false;
        }
        closeDialogActionScope();
        auto result = ngApplyPsdDepthImportResult(composedPreview);
        if (!result.succeeded) {
            if (dialogDisplayed) ensureDialogActionScope();
            lastApplyErrorMessage = result.message;
            incDialog(__("Error"), result.message);
            return false;
        }
        lastApplyErrorMessage = null;
        deactivateDialogCommandContext();
        close();
        return true;
    }

protected:
    override
    void onBeginUpdate() {
        dialogDisplayed = true;
        activateDialogCommandContext();
        ensureDialogActionScope();
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
        if (dialogDisplayed && activePsdDepthMapWindow is this) ensureDialogActionScope();
        if (previewDirty) rebuildPreview();
        else pollPendingPreviewGpuComposition();

        auto space = incAvailableSpace();
        auto settingsWidth = min(340.0f, max(280.0f, space.x * 0.28f));
        if (igBeginTable("###PsdDepthDialogLayout", 2,
            ImGuiTableFlags.Resizable | ImGuiTableFlags.SizingStretchProp, ImVec2(0, space.y))) {
            igTableSetupColumn(__("Settings"), ImGuiTableColumnFlags.WidthFixed, settingsWidth);
            igTableSetupColumn(__("Preview"), ImGuiTableColumnFlags.WidthStretch);
            igTableNextRow();
            igTableNextColumn();

            auto actionsHeight = 94.0f;
            auto settingsHeight = max(120.0f, space.y - actionsHeight);
            if (igBeginChild("###PsdDepthSettingsPane", ImVec2(0, settingsHeight), true)) {
                drawOptions();
            }
            igEndChild();
            auto actionWidth = incAvailableSpace().x;
            auto historyButtonWidth = max(1.0f, (actionWidth - 4.0f) * 0.5f);
            igBeginDisabled(!incActionCanUndo());
            if (incButtonColored(__("Undo"), ImVec2(historyButtonWidth, 26))) {
                auto ctx = dialogCommandContext();
                cmd!(EditCommand.Undo)(ctx);
            }
            igEndDisabled();
            igSameLine();
            igBeginDisabled(!incActionCanRedo());
            if (incButtonColored(__("Redo"), ImVec2(historyButtonWidth, 26))) {
                auto ctx = dialogCommandContext();
                cmd!(EditCommand.Redo)(ctx);
            }
            igEndDisabled();
            igBeginDisabled(previewCompositionPending());
            if (incButtonColored(__("Apply"), ImVec2(actionWidth, 26))) {
                auto ctx = dialogCommandContext();
                cmd!(PsdDepthDialogCommand.ApplyPsdDepthDialog)(ctx);
            }
            igEndDisabled();
            if (incButtonColored(__("Cancel"), ImVec2(actionWidth, 26))) {
                auto ctx = dialogCommandContext();
                cmd!(PsdDepthDialogCommand.CancelPsdDepthDialog)(ctx);
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
        dialogDisplayed = false;
        deactivateDialogCommandContext();
        closeDialogActionScope();
        cancelPendingPreviewGpuComposition();
        disposePreviewTextures();
    }

public:
    bool dialogCommandsAvailable() {
        if (dialogDisplayed && activePsdDepthMapWindow is this) ensureDialogActionScope();
        return dialogDisplayed && dialogActionScope !is null && dialogActionScope.isActive();
    }

    bool applyDialogResult() {
        return apply();
    }

    void cancelDialog() {
        cancelPendingPreviewGpuComposition();
        deactivateDialogCommandContext();
        closeDialogActionScope();
        close();
    }

    Node[] dialogContextNodesForLayer(string layerPath, ulong targetGridUuid) {
        auto puppet = incActivePuppet();
        if (puppet is null || puppet.root is null) return null;

        ulong nodeUuid = targetGridUuid;
        if (auto mapping = findMapping(layerPath, targetGridUuid)) {
            if (mapping.matchedNodeUuid != 0) nodeUuid = mapping.matchedNodeUuid;
        }
        if (nodeUuid == 0) return null;
        auto node = puppet.find!Node(cast(uint)nodeUuid);
        return node !is null ? [node] : null;
    }

    Node[] selectedDialogContextNodes() {
        auto layer = selected3DAdjustLayerPreview();
        if (layer !is null) {
            auto nodes = dialogContextNodesForLayer(layer.layerPath, layer.targetGridUuid);
            if (nodes.length) return nodes;
        }
        if (selectedGridIndex >= 0 && selectedGridIndex < preview.grids.length) {
            auto grid = preview.grids[cast(size_t)selectedGridIndex].grid;
            if (grid !is null) return [cast(Node)grid];
        }
        return null;
    }

    bool dialogContextMatchesLayer(Context ctx, string layerPath, ulong targetGridUuid) {
        if (ctx is null || !ctx.hasNodes() || ctx.nodes.length == 0) return false;
        auto expected = dialogContextNodesForLayer(layerPath, targetGridUuid);
        if (expected.length == 0) return false;
        foreach (selected; ctx.nodes) {
            if (selected is expected[0]) return true;
        }
        return false;
    }

    bool captureDialogContextLayerState(
        Context ctx,
        string layerPath,
        out PsdDepthDialogLayerState state
    ) {
        foreach (ref layer; preview.composedLayers) {
            if (layer.layerPath != layerPath ||
                !dialogContextMatchesLayer(ctx, layer.layerPath, layer.targetGridUuid)) {
                continue;
            }
            state = dialogLayerState(layer);
            return true;
        }
        state = PsdDepthDialogLayerState.init;
        return false;
    }

    bool dialogMappingTargetAvailable(Node target) {
        if (target is null) return false;
        foreach (candidate; currentTargets()) {
            if (candidate is target) return true;
        }
        return false;
    }

    bool dialogContextTargetUuid(Context ctx, out ulong uuid) {
        uuid = 0;
        if (ctx is null || !ctx.hasNodes() || ctx.nodes.length != 1) return false;
        if (!dialogMappingTargetAvailable(ctx.nodes[0])) return false;
        uuid = ctx.nodes[0].uuid;
        return true;
    }

    PsdDepthDialogSettingsState captureDialogSettingsState() {
        PsdDepthDialogSettingsState result;
        result.settings = cloneDialogSettings(settings);
        result.onlyProblemLayers = onlyProblemLayers;
        foreach (ref layer; preview.composedLayers) {
            result.layers ~= dialogLayerState(layer);
        }
        return result;
    }

    bool captureDialogOverallPreview(
        out PsdDepthDialogOverallPreview result,
        out string error
    ) {
        result = PsdDepthDialogOverallPreview.init;
        error = null;
        if (previewDirty) rebuildPreview();
        if (previewCompositionPending()) {
            error = _("GPU preview composition is still running.");
            return false;
        }
        if (errorMessage.length) {
            error = errorMessage;
            return false;
        }

        if (!buildThreeDAdjustOverallPreview(640, 480, result)) {
            error = "Failed to render the PSD depth dialog overall preview";
            return false;
        }
        result.yaw = threeDAdjustCamera.yaw;
        result.pitch = threeDAdjustCamera.pitch;
        result.zoom = threeDAdjustCamera.zoom;
        result.panX = threeDAdjustCamera.pan.x;
        result.panY = threeDAdjustCamera.pan.y;
        return true;
    }

    bool captureDialogContextPartData(
        Context ctx,
        out PsdDepthDialogPartData[] result,
        out string error
    ) {
        result = null;
        error = null;
        if (ctx is null || !ctx.hasNodes() || ctx.nodes.length == 0) {
            error = "Select one or more PSD depth dialog parts through Context.nodes";
            return false;
        }
        if (previewDirty) rebuildPreview();
        if (previewCompositionPending()) {
            error = _("GPU preview composition is still running.");
            return false;
        }
        if (errorMessage.length) {
            error = errorMessage;
            return false;
        }

        size_t retainedBytes;
        foreach (ref gridResult; preview.grids) {
            if (gridResult.grid is null) continue;

            bool selected;
            foreach (node; ctx.nodes) {
                if (node is cast(Node)gridResult.grid) {
                    selected = true;
                    break;
                }
            }
            if (!selected) {
                foreach (ref layer; preview.composedLayers) {
                    if (layer.targetGridUuid != gridResult.grid.uuid) continue;
                    if (dialogContextMatchesLayer(ctx, layer.layerPath, layer.targetGridUuid)) {
                        selected = true;
                        break;
                    }
                }
            }
            if (!selected) continue;

            if (!reservePsdDepthDialogPartDataElements(
                    retainedBytes, gridResult.grid.vertices.length, 2 * float.sizeof, error) ||
                !reservePsdDepthDialogPartDataElements(
                    retainedBytes, gridResult.depths.length, float.sizeof, error) ||
                !reservePsdDepthDialogPartDataElements(
                    retainedBytes, gridResult.baseDepths.length, float.sizeof, error) ||
                !reservePsdDepthDialogPartDataElements(
                    retainedBytes, gridResult.winnerLayerPaths.length, string.sizeof, error) ||
                !reservePsdDepthDialogPartDataElements(
                    retainedBytes, gridResult.missingVertexMask.length, bool.sizeof, error) ||
                !reservePsdDepthDialogPartDataBytes(
                    retainedBytes, gridResult.rawCompositePreviewRgba.length, error)) return false;
            foreach (ref layer; preview.composedLayers) {
                if (layer.targetGridUuid != gridResult.grid.uuid) continue;
                if (!reservePsdDepthDialogPartDataBytes(
                        retainedBytes, layer.colorRgba.length, error) ||
                    !reservePsdDepthDialogPartDataBytes(
                        retainedBytes, layer.depthRgba.length, error)) return false;
            }

            PsdDepthDialogPartData part;
            part.targetGridUuid = gridResult.grid.uuid;
            part.targetGridName = gridResult.grid.name;
            part.targetType = typeid(cast(Object)gridResult.grid).toString();
            part.skipped = gridResult.skipped;
            part.documentWidth = gridResult.documentWidth;
            part.documentHeight = gridResult.documentHeight;
            part.coverageSources = gridResult.coverageSources;
            part.sampledVertices = gridResult.sampledVertices;
            part.missingVertices = gridResult.missingVertices;
            part.minDepth = gridResult.minDepth;
            part.maxDepth = gridResult.maxDepth;
            foreach (vertex; gridResult.grid.vertices) {
                part.vertexX ~= vertex.x;
                part.vertexY ~= vertex.y;
            }
            part.depths = gridResult.depths.dup;
            part.baseDepths = gridResult.baseDepths.dup;
            part.winnerLayerPaths = gridResult.winnerLayerPaths.dup;
            part.missingVertexMask = gridResult.missingVertexMask.dup;
            part.previewLeft = gridResult.previewLeft;
            part.previewTop = gridResult.previewTop;
            part.previewWidth = gridResult.previewWidth;
            part.previewHeight = gridResult.previewHeight;
            part.previewRgba = gridResult.rawCompositePreviewRgba.dup;

            foreach (ref layer; preview.composedLayers) {
                if (layer.targetGridUuid != gridResult.grid.uuid) continue;
                PsdDepthDialogPartLayerData layerData;
                layerData.layerPath = layer.layerPath;
                layerData.layerName = layer.layerName;
                layerData.colorLayerPath = layer.colorLayerPath;
                layerData.colorLayerName = layer.colorLayerName;
                layerData.sourcePath = layer.sourcePath;
                layerData.left = layer.left;
                layerData.top = layer.top;
                layerData.width = layer.width;
                layerData.height = layer.height;
                layerData.visible = layer.visible;
                layerData.enabled = layer.enabled;
                layerData.depthEnabled = layer.depthEnabled;
                layerData.targetGridUuid = layer.targetGridUuid;
                layerData.depthStats = layer.depthStats;
                layerData.colorRgba = layer.colorRgba.dup;
                layerData.depthRgba = layer.depthRgba.dup;
                part.layers ~= layerData;
            }
            result ~= part;
        }

        if (result.length == 0) {
            error = "Context.nodes does not select a part loaded by the PSD depth dialog";
            return false;
        }
        return true;
    }

    bool applyDialogSettingsState(PsdDepthDialogSettingsState state) {
        settings = cloneDialogSettings(state.settings);
        if (settings.useGpuComposition && !ngDepthDrawGpuLayerSampleSupportsConvolution(
            cast(int)ngPsdDepthConvolutionToDepthImage(settings.convolution))) {
            settings.convolution = PsdDepthConvolution.Median3x3;
        }
        onlyProblemLayers = state.onlyProblemLayers;
        pendingLayerStatesAfterRebuild = state.layers.dup;
        previewDirty = true;
        return true;
    }

    bool captureDialogLayerState(
        string layerPath,
        ulong targetGridUuid,
        out PsdDepthDialogLayerState state
    ) {
        if (auto layer = findComposedLayer(layerPath, targetGridUuid)) {
            state = dialogLayerState(*layer);
            return true;
        }
        state = PsdDepthDialogLayerState.init;
        return false;
    }

    bool applyDialogLayerState(PsdDepthDialogLayerState state, bool stageForApply) {
        auto layer = findComposedLayer(state.layerPath, state.targetGridUuid);
        if (layer is null) return false;
        auto previous = dialogLayerState(*layer);
        if (sameDialogLayerState(previous, state)) return false;

        applyDialogLayerStateFields(*layer, state);
        if (stageForApply) {
            stage3DAdjustLayerChange(state.layerPath, state.targetGridUuid);
            if (state.invert != previous.invert) {
                invalidate3DAdjustLayerCaches(state.layerPath, state.targetGridUuid);
            }
            if (preview.gpuCompositionRequested) {
                startPreviewComposition();
                pending3DAdjustLayerChanges = null;
            }
        } else {
            refreshAfter3DAdjustLayerChange(state.layerPath, state.targetGridUuid);
        }
        return true;
    }

    PsdDepthDialogLayerPixels[] captureDialogLayerPixels() {
        PsdDepthDialogLayerPixels[] result;
        foreach (ref layer; preview.composedLayers) {
            PsdDepthDialogLayerPixels pixels;
            pixels.layerPath = layer.layerPath;
            pixels.targetGridUuid = layer.targetGridUuid;
            pixels.depthRgba = layer.depthRgba.dup;
            if (auto applied = cleanupLayerKey(layer.layerPath, layer.targetGridUuid) in
                alphaDepthGapFillAppliedByLayer) {
                pixels.alphaDepthGapFillApplied = *applied;
            }
            result ~= pixels;
        }
        return result;
    }

    bool applyDialogLayerPixels(PsdDepthDialogLayerPixels[] state) {
        bool found;
        foreach (pixels; state) {
            auto layer = findComposedLayer(pixels.layerPath, pixels.targetGridUuid);
            if (layer is null) continue;
            layer.depthRgba = pixels.depthRgba.dup;
            alphaDepthGapFillAppliedByLayer[cleanupLayerKey(pixels.layerPath, pixels.targetGridUuid)] =
                pixels.alphaDepthGapFillApplied;
            found = true;
        }
        if (!found && state.length > 0) return false;

        disposePreviewTextures();
        if (!startPreviewComposition()) return false;
        previewDirty = false;
        return true;
    }

    bool applyDialogAlphaDepthGapFill() {
        if (previewCompositionPending()) return false;
        auto oldPixels = captureDialogLayerPixels();
        auto oldApplied = alphaDepthGapFillAppliedByLayer.dup;
        auto oldComposedPreview = composedPreview;
        applyAlphaDepthGapFill();
        if (errorMessage.length == 0) return true;

        foreach (pixels; oldPixels) {
            auto layer = findComposedLayer(pixels.layerPath, pixels.targetGridUuid);
            if (layer !is null) layer.depthRgba = pixels.depthRgba.dup;
        }
        alphaDepthGapFillAppliedByLayer = oldApplied;
        composedPreview = oldComposedPreview;
        previewDirty = false;
        return false;
    }

    static bool dialogLayerPixelsEqual(
        PsdDepthDialogLayerPixels[] a,
        PsdDepthDialogLayerPixels[] b
    ) {
        if (a.length != b.length) return false;
        foreach (i; 0 .. a.length) {
            if (a[i].layerPath != b[i].layerPath ||
                a[i].targetGridUuid != b[i].targetGridUuid ||
                a[i].depthRgba != b[i].depthRgba ||
                a[i].alphaDepthGapFillApplied != b[i].alphaDepthGapFillApplied) {
                return false;
            }
        }
        return true;
    }

    this(string path) {
        this.path = path;
        super(_("PSD Depth Map Import"));
    }

    version (CommandBrowserDifferential) {
        void beginDialogCommandSessionForRegression() {
            dialogDisplayed = true;
            activateDialogCommandContext();
            ensureDialogActionScope();
        }

        bool dialogCommandScopeActiveForRegression() {
            return dialogCommandScope !is null && dialogCommandScope.isActive();
        }

        bool dialogActionScopeActiveForRegression() {
            return dialogActionScope !is null && dialogActionScope.isActive();
        }

        void setDialogLayerStateForRegression(PsdDepthDialogLayerState state) {
            PsdDepthComposedLayer layer;
            layer.id = state.layerPath;
            layer.layerPath = state.layerPath;
            layer.targetGridUuid = state.targetGridUuid;
            applyDialogLayerStateFields(layer, state);
            preview.composedLayers = [layer];
            previewDirty = false;
        }

        bool setDialogPartDataForRegression(
            Deformable target,
            int width,
            int height,
            ubyte[] colorRgba,
            ubyte[] depthRgba,
            ubyte[] previewRgba,
            float[] depths
        ) {
            if (target is null || preview.composedLayers.length != 1 ||
                width <= 0 || height <= 0) return false;
            auto expectedLength = cast(size_t)width * cast(size_t)height * 4;
            if (colorRgba.length != expectedLength ||
                depthRgba.length != expectedLength ||
                previewRgba.length != expectedLength) return false;

            auto layer = &preview.composedLayers[0];
            layer.targetGridUuid = target.uuid;
            layer.targetGridName = target.name;
            layer.layerName = "Regression Layer";
            layer.colorLayerPath = layer.layerPath;
            layer.colorLayerName = layer.layerName;
            layer.sourcePath = path;
            layer.width = width;
            layer.height = height;
            layer.colorRgba = colorRgba.dup;
            layer.depthRgba = depthRgba.dup;
            layer.maskRgba.length = expectedLength;
            foreach (i; 0 .. cast(size_t)width * cast(size_t)height) {
                auto offset = i * 4;
                layer.maskRgba[offset .. offset + 4] = [cast(ubyte)255, 0, 0, cast(ubyte)255];
            }
            layer.depthStats.hasDepth = depths.length > 0;
            layer.depthStats.maskedPixels = cast(size_t)width * cast(size_t)height;

            PsdDepthGridResult gridResult;
            gridResult.grid = target;
            gridResult.depths = depths.dup;
            gridResult.baseDepths.length = depths.length;
            gridResult.winnerLayerPaths.length = depths.length;
            gridResult.winnerLayerPaths[] = layer.layerPath;
            gridResult.missingVertexMask.length = depths.length;
            gridResult.documentWidth = width;
            gridResult.documentHeight = height;
            gridResult.coverageSources = 1;
            gridResult.sampledVertices = depths.length;
            gridResult.minDepth = depths.length ? depths[0] : 0;
            gridResult.maxDepth = depths.length ? depths[0] : 0;
            foreach (depth; depths) {
                gridResult.minDepth = min(gridResult.minDepth, depth);
                gridResult.maxDepth = max(gridResult.maxDepth, depth);
            }
            gridResult.previewWidth = width;
            gridResult.previewHeight = height;
            gridResult.rawCompositePreviewRgba = previewRgba.dup;
            gridResult.compositePreviewRgba = previewRgba.dup;
            PsdDepthGridLayerMask layerMask;
            layerMask.layerPath = layer.layerPath;
            layerMask.layerName = layer.layerName;
            layerMask.sampledVertices = depths.length;
            layerMask.selectedVertices = depths.length;
            gridResult.layerMasks = [layerMask];
            preview.grids = [gridResult];
            previewDirty = false;
            return true;
        }

        void clearDialogPartDataForRegression() {
            preview.grids = null;
            previewDirty = false;
        }

        bool setDialogLayerPixelsForRegression(
            int width,
            int height,
            ubyte[] depthRgba,
            ubyte[] maskRgba
        ) {
            if (preview.composedLayers.length != 1) return false;
            preview.composedLayers[0].width = width;
            preview.composedLayers[0].height = height;
            preview.composedLayers[0].depthRgba = depthRgba.dup;
            preview.composedLayers[0].maskRgba = maskRgba.dup;
            if (!startPreviewComposition()) return false;
            previewDirty = false;
            return true;
        }

        bool replayDialogCleanupAfterRebuildForRegression(ubyte[] rebuiltDepthRgba) {
            if (preview.composedLayers.length != 1) return false;
            preview.composedLayers[0].depthRgba = rebuiltDepthRgba.dup;
            replayAlphaDepthGapFills();
            return true;
        }

        void failNextPreviewCompositionForRegressionTest() {
            failNextPreviewCompositionForRegression = true;
        }

        void useCurrentPreviewForDialogApplyRegression() {
            composedPreview = ngPsdDepthComposedViewForRegression(preview);
        }

        bool prepareDialogApplyForRegression() {
            preview = PsdDepthImportResult.init;
            pending3DAdjustLayerChanges = null;
            if (!startPreviewComposition()) return false;
            previewDirty = false;
            return true;
        }

        void endDialogCommandSessionForRegression() {
            dialogDisplayed = false;
            deactivateDialogCommandContext();
            closeDialogActionScope();
        }
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
            if (!compose3DAdjustChangesForApply()) {
                message = errorMessage;
                return false;
            }
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

        bool setFirstMappedLayerZTransformForRegressionSmoke(string gridName, float zScale, float zOffset) {
            if (previewDirty) rebuildPreview();
            foreach (ref gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                foreach (layerMask; gridResult.layerMasks) {
                    if (layerMask.layerPath.length == 0) continue;
                    if (auto layer = findComposedLayer(layerMask.layerPath, gridResult.grid.uuid)) {
                        threeDAdjustLayerMesh(*layer);
                    }
                    auto transform = layerTransform(layerMask.layerPath, gridResult.grid.uuid);
                    transform.zScale = zScale;
                    transform.zOffset = zOffset;
                    if (auto layer = findComposedLayer(layerMask.layerPath, gridResult.grid.uuid)) {
                        layer.depthScale = transform.zScale;
                        layer.depthOffset = transform.zOffset;
                    }
                    stage3DAdjustLayerChange(layerMask.layerPath, gridResult.grid.uuid);
                    return true;
                }
            }
            return false;
        }

        bool hasPending3DAdjustLayerChangesForRegressionSmoke() const {
            return pending3DAdjustLayerChanges.length > 0;
        }

        size_t last3DAdjustApplyRecomposedGridCountForRegressionSmoke() const {
            return last3DAdjustApplyRecomposedGridCount;
        }

        bool hasRealtime3DAdjustTransformForRegressionSmoke(string gridName) {
            foreach (ref gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                foreach (layerMask; gridResult.layerMasks) {
                    auto layer = findComposedLayer(layerMask.layerPath, gridResult.grid.uuid);
                    if (layer is null) continue;
                    auto mesh = threeDAdjustLayerMesh(*layer);
                    if (mesh is null || mesh.vertexData.length < 3) continue;
                    auto previousDepth = mesh.vertexData[2];
                    updateThreeDAdjustMeshDisplayDepths(*layer, *mesh);
                    return abs(mesh.vertexData[2] - previousDepth) > 0.000001f;
                }
            }
            return false;
        }

        bool hasFrontOnlyIntersectionConstraintForRegressionSmoke() {
            return frontOnlyIntersectionConstraintDiagnosticsForRegressionSmoke() == "ok";
        }

        string frontOnlyIntersectionConstraintDiagnosticsForRegressionSmoke() {
            ubyte[] solidPixels(ubyte depth) {
                ubyte[] result;
                foreach (_; 0 .. 4) result ~= [depth, depth, depth, cast(ubyte)255];
                return result;
            }
            ubyte[] depthPixels(ubyte[] values) {
                ubyte[] result;
                foreach (value; values) result ~= [value, value, value, cast(ubyte)255];
                return result;
            }

            PsdDepthComposedLayer backLayer;
            backLayer.id = "/constraint-back";
            backLayer.layerPath = backLayer.id;
            backLayer.layerName = "Constraint Back";
            backLayer.width = 2;
            backLayer.height = 2;
            backLayer.colorRgba = solidPixels(255);
            backLayer.depthRgba = depthPixels([cast(ubyte)0, 64, 128, 192]);
            backLayer.maskRgba = solidPixels(255);
            backLayer.coverageMaskRgba = solidPixels(255);

            auto frontLayer = backLayer;
            frontLayer.id = "/constraint-front";
            frontLayer.layerPath = frontLayer.id;
            frontLayer.layerName = "Constraint Front";
            frontLayer.depthRgba = solidPixels(220);

            preview = PsdDepthImportResult.init;
            preview.compositionMode = PsdDepthCompositionMode.NToN;
            preview.depthSource.kind = PsdDepthCompositeSourceKind.PsdLayers;
            preview.compositionWidth = 2;
            preview.compositionHeight = 2;
            preview.composedLayers = [backLayer, frontLayer];
            threeDAdjustSamples = null;
            threeDAdjustMeshes = null;
            threeDAdjustPreparedTransforms = null;
            threeDAdjustFrontIntersections = null;
            threeDAdjustDisplayDepthDirty = null;
            threeDAdjustSamplesPrepared = false;
            prepareThreeDAdjustSamples();

            auto backFrontIndices = threeDAdjustFrontIntersectingLayerIndices(preview.composedLayers[0]);
            auto frontFrontIndices = threeDAdjustFrontIntersectingLayerIndices(preview.composedLayers[1]);
            if (backFrontIndices != [cast(size_t)1] || frontFrontIndices.length != 0) {
                return "front index mismatch: back=%s front=%s".format(
                    backFrontIndices.length, frontFrontIndices.length);
            }

            auto samples = threeDAdjustLayerSamples(preview.composedLayers[0]);
            if (samples.length == 0) return "back layer has no samples";
            auto sample = samples[$ - 1];
            auto initialDepth = sample.depth;
            preview.composedLayers[0].depthScale = 200.0f;
            auto unconstrained = threeDAdjustDisplayDepth(preview.composedLayers[0], sample.sourceDepth);
            auto constrained = threeDAdjustConstrainedDisplayDepth(preview.composedLayers[0], sample);
            if (!(constrained < unconstrained && abs(constrained - initialDepth) > 0.000001f)) {
                return "depth mismatch: initial=%s unconstrained=%s constrained=%s".format(
                    initialDepth, unconstrained, constrained);
            }
            return "ok";
        }

        float[] previewDepthsForRegressionSmoke(string gridName) {
            foreach (gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                return gridResult.depths.dup;
            }
            return null;
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
