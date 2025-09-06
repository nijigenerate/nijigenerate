module nijigenerate.viewport.vertex.automesh.common;

import nijilive.core;
import nijigenerate.core.cv.image;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.vertex.automesh.alpha_provider;
import inmath;

struct AlphaInput {
    Image img;        // RGBA image where A contains the alpha mask (may be null when constructed from provider only)
    int w;
    int h;
    bool fromProvider;
    vec2 providerWorldCenter; // valid when fromProvider
}

AlphaInput alphaInputFromProvider(IAlphaProvider provider) {
    AlphaInput ai;
    ai.w = provider.width();
    ai.h = provider.height();
    ai.fromProvider = true;
    auto b = provider.boundsWorld();
    ai.providerWorldCenter = vec2(b.x + b.w * 0.5f, b.y + b.h * 0.5f);
    return ai;
}

AlphaInput alphaInputFromProviderWithImage(IAlphaProvider provider) {
    AlphaInput ai = alphaInputFromProvider(provider);
    if (ai.w > 0 && ai.h > 0) {
        ai.img = new Image(ai.w, ai.h, ImageFormat.IF_RGB_ALPHA);
        ai.img.data[] = 0;
        const(ubyte)* ap = provider.alphaPtr();
        foreach (i; 0 .. ai.w * ai.h) ai.img.data[i * 4 + 3] = ap[i];
    }
    return ai;
}

AlphaInput getAlphaInput(Drawable target) {
    AlphaInput ai;
    if (auto part = cast(Part)target) {
        auto tex = part.textures[0];
        if (tex is null) return ai;
        ubyte[] data = tex.getTextureData();
        ai.img = new Image(tex.width, tex.height, ImageFormat.IF_RGB_ALPHA);
        ai.w = tex.width;
        ai.h = tex.height;
        ai.img.data[] = 0;
        // Copy full RGBA so downstream that expects RGB can work, but A is the only channel used
        if (data.length == ai.img.data.length) ai.img.data[] = data[];
        else {
            // Fallback: copy alpha only if layout differs
            foreach (i; 0 .. ai.w*ai.h) ai.img.data[i*4+3] = data[i*4+3];
        }
        ai.fromProvider = false;
    } else {
        auto provider = new MeshGroupAlphaProvider(target);
        scope(exit) provider.dispose();
        ai.w = provider.width();
        ai.h = provider.height();
        if (ai.w <= 0 || ai.h <= 0) return ai;
        ai.img = new Image(ai.w, ai.h, ImageFormat.IF_RGB_ALPHA);
        ai.img.data[] = 0;
        const(ubyte)* ap = provider.alphaPtr();
        foreach (i; 0 .. ai.w*ai.h) ai.img.data[i*4 + 3] = ap[i];
        auto b = provider.boundsWorld();
        ai.providerWorldCenter = vec2(b.x + b.w * 0.5f, b.y + b.h * 0.5f);
        ai.fromProvider = true;
    }
    return ai;
}

vec2 alphaImageCenter(AlphaInput ai) {
    return vec2(ai.w / 2, ai.h / 2);
}

void mapImageCenteredMeshToTargetLocal(ref IncMesh mesh, Drawable target, AlphaInput ai) {
    // mesh vertices are expected to be relative to image center (imgCenter at (0,0))
    if (ai.fromProvider) {
        mat4 inv = target.transform.matrix.inverse;
        foreach (v; mesh.vertices) {
            vec2 worldPos = v.position + ai.providerWorldCenter;
            v.position = (inv * vec4(worldPos, 0, 1)).xy;
        }
    } else {
        if (auto dcomposite = cast(DynamicComposite)target) {
            foreach (vertex; mesh.vertices) vertex.position += dcomposite.textureOffset;
        }
    }
}
