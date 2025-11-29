module nijilive.core.render.backends.queue;

version (InDoesRender) {

import nijilive.core.render.backends;
import nijilive.core.render.commands;
import nijilive.core.nodes.part : Part;
import nijilive.core.nodes.mask : Mask;
import nijilive.core.nodes.drawable : Drawable;
import nijilive.core.nodes.composite : Composite;
import nijilive.core.texture : Texture;
import nijilive.math : vec2, vec3, vec4, rect, mat4, Vec2Array, Vec3Array;
import nijilive.math.camera : Camera;
import nijilive.core.diff_collect : DifferenceEvaluationRegion, DifferenceEvaluationResult;

/// Serialized command record for the queue backend.
struct QueuedCommand {
    RenderCommandKind kind;
    PartDrawPacket partPacket;
    MaskDrawPacket maskPacket;
    MaskApplyPacket maskApplyPacket;
    CompositeDrawPacket compositePacket;
    DynamicCompositePass dynamicPass;
    bool maskUsesStencil;
}

/// Backend that simply enqueues the incoming packets.
class RenderingBackend(BackendEnum backendType : BackendEnum.Mock) {
private:
    QueuedCommand[] queueData;
    size_t renderImage;
    size_t compositeImage;
    size_t blendImage;

public:
    void initializeRenderer() {}
    void resizeViewportTargets(int, int) {}
    void dumpViewport(ref ubyte[], int, int) {}
    void beginScene() {}
    void endScene() {}
    void postProcessScene() {}
    void addBasicLightingPostProcess() {}

    void initializeDrawableResources() {}
    void bindDrawableVao() {}
    void createDrawableBuffers(out uint ibo) { ibo = 0; }
    void uploadDrawableIndices(uint, ushort[]) {}
    void uploadSharedVertexBuffer(Vec2Array) {}
    void uploadSharedUvBuffer(Vec2Array) {}
    void uploadSharedDeformBuffer(Vec2Array) {}
    void drawDrawableElements(uint, size_t) {}

    bool supportsAdvancedBlend() { return false; }
    bool supportsAdvancedBlendCoherent() { return false; }
    void setAdvancedBlendCoherent(bool) {}
    void setLegacyBlendMode(BlendMode) {}
    void setAdvancedBlendEquation(BlendMode) {}
    void issueBlendBarrier() {}
    void initDebugRenderer() {}
    void setDebugPointSize(float) {}
    void setDebugLineWidth(float) {}
    void uploadDebugBuffer(Vec3Array, ushort[]) {}
    void setDebugExternalBuffer(uint, uint, int) {}
    void drawDebugPoints(vec4, mat4) {}
    void drawDebugLines(vec4, mat4) {}

    void drawPartPacket(ref PartDrawPacket packet) {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.DrawPart;
        cmd.partPacket = packet;
        queueData ~= cmd;
    }

    void drawMaskPacket(ref MaskDrawPacket packet) {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.DrawMask;
        cmd.maskPacket = packet;
        queueData ~= cmd;
    }

    void beginDynamicComposite(DynamicCompositePass pass) {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.BeginDynamicComposite;
        cmd.dynamicPass = pass;
        queueData ~= cmd;
    }

    void endDynamicComposite(DynamicCompositePass pass) {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.EndDynamicComposite;
        cmd.dynamicPass = pass;
        queueData ~= cmd;
    }

    void destroyDynamicComposite(DynamicCompositeSurface) {}

    void beginMask(bool useStencil) {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.BeginMask;
        cmd.maskUsesStencil = useStencil;
        queueData ~= cmd;
    }

    void applyMask(ref MaskApplyPacket packet) {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.ApplyMask;
        cmd.maskApplyPacket = packet;
        queueData ~= cmd;
    }

    void beginMaskContent() {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.BeginMaskContent;
        queueData ~= cmd;
    }

    void endMask() {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.EndMask;
        queueData ~= cmd;
    }

    void beginComposite() {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.BeginComposite;
        queueData ~= cmd;
    }

    void drawCompositeQuad(ref CompositeDrawPacket packet) {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.DrawCompositeQuad;
        cmd.compositePacket = packet;
        queueData ~= cmd;
    }

    void endComposite() {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind.EndComposite;
        queueData ~= cmd;
    }

    void drawTextureAtPart(Texture, Part) {}
    void drawTextureAtPosition(Texture, vec2, float, vec3, vec3) {}
    void drawTextureAtRect(Texture, rect, rect, float, vec3, vec3, Shader = null, Camera = null) {}

    RenderResourceHandle framebufferHandle() { return cast(RenderResourceHandle)renderImage; }
    RenderResourceHandle renderImageHandle() { return cast(RenderResourceHandle)renderImage; }
    RenderResourceHandle compositeFramebufferHandle() { return cast(RenderResourceHandle)compositeImage; }
    RenderResourceHandle compositeImageHandle() { return cast(RenderResourceHandle)compositeImage; }
    RenderResourceHandle mainAlbedoHandle() { return cast(RenderResourceHandle)renderImage; }
    RenderResourceHandle mainEmissiveHandle() { return cast(RenderResourceHandle)renderImage; }
    RenderResourceHandle mainBumpHandle() { return cast(RenderResourceHandle)renderImage; }
    RenderResourceHandle compositeEmissiveHandle() { return cast(RenderResourceHandle)compositeImage; }
    RenderResourceHandle compositeBumpHandle() { return cast(RenderResourceHandle)compositeImage; }
    RenderResourceHandle blendFramebufferHandle() { return cast(RenderResourceHandle)blendImage; }
    RenderResourceHandle blendAlbedoHandle() { return cast(RenderResourceHandle)blendImage; }
    RenderResourceHandle blendEmissiveHandle() { return cast(RenderResourceHandle)blendImage; }
    RenderResourceHandle blendBumpHandle() { return cast(RenderResourceHandle)blendImage; }

    void setDifferenceAggregationEnabled(bool) {}
    bool isDifferenceAggregationEnabled() { return false; }
    void setDifferenceAggregationRegion(DifferenceEvaluationRegion) {}
    DifferenceEvaluationRegion getDifferenceAggregationRegion() { return DifferenceEvaluationRegion.init; }
    bool evaluateDifferenceAggregation(RenderResourceHandle, int, int) { return false; }
    bool fetchDifferenceAggregationResult(out DifferenceEvaluationResult result) {
        result = DifferenceEvaluationResult.init;
        return false;
    }

    RenderShaderHandle createShader(string, string) { return null; }
    void destroyShader(RenderShaderHandle) {}
    void useShader(RenderShaderHandle) {}
    int getShaderUniformLocation(RenderShaderHandle, string) { return -1; }
    void setShaderUniform(RenderShaderHandle, int, bool) {}
    void setShaderUniform(RenderShaderHandle, int, int) {}
    void setShaderUniform(RenderShaderHandle, int, float) {}
    void setShaderUniform(RenderShaderHandle, int, vec2) {}
    void setShaderUniform(RenderShaderHandle, int, vec3) {}
    void setShaderUniform(RenderShaderHandle, int, vec4) {}
    void setShaderUniform(RenderShaderHandle, int, mat4) {}

    RenderTextureHandle createTextureHandle() { return null; }
    void destroyTextureHandle(RenderTextureHandle) {}
    void bindTextureHandle(RenderTextureHandle, uint) {}
    void uploadTextureData(RenderTextureHandle, int, int, int, int, bool, ubyte[]) {}
    void updateTextureRegion(RenderTextureHandle, int, int, int, int, int, ubyte[]) {}
    void generateTextureMipmap(RenderTextureHandle) {}
    void applyTextureFiltering(RenderTextureHandle, Filtering) {}
    void applyTextureWrapping(RenderTextureHandle, Wrapping) {}
    void applyTextureAnisotropy(RenderTextureHandle, float) {}
    float maxTextureAnisotropy() { return 1; }
    void readTextureData(RenderTextureHandle, int, bool, ubyte[]) {}
    size_t textureHandleId(RenderTextureHandle h) { return cast(size_t)h; }
    size_t textureNativeHandle(RenderTextureHandle h) { return cast(size_t)h; }

    void setRenderTargets(size_t renderHandle, size_t compositeHandle, size_t blendHandle = 0) {
        renderImage = renderHandle;
        compositeImage = compositeHandle;
        blendImage = blendHandle;
    }

    /// Returns a view of the queued commands.
    QueuedCommand[] queuedCommands() const {
        return queueData;
    }

    /// Clears the recorded queue.
    void clearQueue() {
        queueData.length = 0;
    }
}

}
