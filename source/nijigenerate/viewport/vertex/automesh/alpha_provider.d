module nijigenerate.viewport.vertex.automesh.alpha_provider;

/**
 * Alpha-only input abstraction for AutoMesh.
 *
 * Providers expose a CPU-readable 8-bit alpha buffer and the world-space
 * rectangle that was projected to generate the buffer.
 */
interface IAlphaProvider {
    int width();
    int height();
    const(ubyte)* alphaPtr();
    RectF boundsWorld();
    void dispose();
}

/// Simple rectangle type (world-space bounds)
struct RectF {
    float x;
    float y;
    float w;
    float h;
}

/// Options for projection-based alpha providers
struct ProjectionOptions {
    int padding = 8;          // extra pixels around bounds
    int targetWidth = 0;      // 0 => auto from bounds
    int targetHeight = 0;     // 0 => auto from bounds
}

import nijilive; // Part, Texture, Node, Texture, DynamicComposite, Puppet, GL helpers
import nijigenerate.project : incActivePuppet;
import nijigenerate.core.window : incResetClearColor;
import std.algorithm : min, max;
import std.exception : enforce;
import bindbc.imgui;
import nijigenerate.project : incSelectedNodes;
import nijigenerate.widgets.texture : incTextureSlot;
import std.conv : to;
import std.math : floor, ceil, isNaN;

/**
 * PartAlphaProvider: wraps a Part's texture alpha channel as AutoMesh input.
 *
 * Note: This mirrors the current behavior in ContourAutoMeshProcessor, but
 * provides it via a unified provider interface.
 */
final class PartAlphaProvider : IAlphaProvider {
    private ubyte[] _alpha;
    private int _w, _h;
    private RectF _bounds;

    this(Part part) {
        if (part is null || part.textures.length == 0 || part.textures[0] is null) {
            _w = _h = 0;
            _bounds = RectF(0, 0, 0, 0);
            return;
        }
        auto texture = part.textures[0];
        _w = texture.width;
        _h = texture.height;
        // Bounds: local texture size around origin; world mapping is applied in downstream
        _bounds = RectF(0, 0, cast(float)_w, cast(float)_h);

        ubyte[] data = texture.getTextureData(); // RGBA, row-major
        if (data.length >= cast(size_t)_w * _h * 4) {
            _alpha.length = cast(size_t)_w * _h;
            size_t p = 0;
            foreach (i; 0 .. _w * _h) {
                // pick A component (assuming RGBA order)
                _alpha[i] = data[p + 3];
                p += 4;
            }
        }
    }

    override int width() { return _w; }
    override int height() { return _h; }
    override const(ubyte)* alphaPtr() { return _alpha.ptr; }
    override RectF boundsWorld() { return _bounds; }
    override void dispose() { _alpha.length = 0; }
}

/**
 * MeshGroupAlphaProvider (stub): projection-based alpha provider for MeshGroup
 * and other composite nodes. This class defines the interface and basic state,
 * and can be extended to perform offscreen rendering and readback.
 */
final class MeshGroupAlphaProvider : IAlphaProvider {
    private ubyte[] _alpha;
    private int _w, _h;
    private RectF _bounds;

    this(Node meshGroup, ProjectionOptions opt = ProjectionOptions.init) {
        if (meshGroup is null) {
            _w = _h = 0; _bounds = RectF(0,0,0,0); return;
        }
        auto puppet = meshGroup.puppet is null ? incActivePuppet() : meshGroup.puppet;
        enforce(puppet !is null, "No active puppet for MeshGroupAlphaProvider");

        // DynamicComposite を使って対象ノードのみをオフスクリーン投影
        auto r = projectAlphaExec([meshGroup], puppet, opt);
        _w = r.w; _h = r.h; _bounds = r.bounds; _alpha = r.alpha;
    }

    override int width() { return _w; }
    override int height() { return _h; }
    override const(ubyte)* alphaPtr() { return _alpha.ptr; }
    override RectF boundsWorld() { return _bounds; }
    override void dispose() { _alpha.length = 0; }
}

/**
 * GenericProjectionAlphaProvider: 任意のノード配列を対象に、暫定的に現在の
 * ビューポートから A をダンプして返す（後続でサブグラフ限定投影に差し替え）。
 */
final class GenericProjectionAlphaProvider : IAlphaProvider {
    private ubyte[] _alpha;
    private int _w, _h;
    private RectF _bounds;

    this(Node[] targets, ProjectionOptions opt = ProjectionOptions.init) {
        auto puppet = incActivePuppet();
        enforce(puppet !is null, "No active puppet for GenericProjectionAlphaProvider");
        auto r = projectAlphaExec(targets, puppet, opt);
        _w = r.w; _h = r.h; _bounds = r.bounds; _alpha = r.alpha;
    }

    override int width() { return _w; }
    override int height() { return _h; }
    override const(ubyte)* alphaPtr() { return _alpha.ptr; }
    override RectF boundsWorld() { return _bounds; }
    override void dispose() { _alpha.length = 0; }
}

private struct ProjectionResult {
    ubyte[] alpha;
    int w;
    int h;
    RectF bounds;
}

private ProjectionResult projectAlphaExec(Node[] targets, Puppet puppet, ProjectionOptions opt) {
    ProjectionResult res;

    if (targets.length == 0 || puppet is null) {
        res.w = res.h = 0;
        res.bounds = RectF(0,0,0,0);
        return res;
    }

    // Collect drawables under targets with coverOthers-aware traversal
    Drawable[] drawables;
    void findSubDrawable(Node n) {
        if (n is null) return;
        // If node covers others, we only consider its children
        if (n.coverOthers()) {
            foreach (child; n.children) findSubDrawable(child);
            return;
        }
        // Composite handling
        if (auto comp = cast(Composite)n) {
            if (comp.propagateMeshGroup) {
                foreach (child; n.children) findSubDrawable(child);
            }
            // Do not traverse further under non-propagating composites
            return;
        }
        // Drawable is a terminal we want; also traverse children
        if (auto d = cast(Drawable)n) {
            drawables ~= d;
            foreach (child; n.children) findSubDrawable(child);
            return;
        }
        // Plain Node: stop here (exclude descendants)
        return;
    }
    foreach (t; targets) findSubDrawable(t);
    if (drawables.length == 0) {
        res.w = res.h = 0; res.bounds = RectF(0,0,0,0); return res;
    }

    // Compute union bounds from transformed vertices
    float minX = float.infinity, minY = float.infinity;
    float maxX = -float.infinity, maxY = -float.infinity;
    foreach (d; drawables) {
        auto m = d.getDynamicMatrix();
        auto verts = d.vertices;
        if (verts.length == 0) continue;
        if (d.deformation.length != verts.length) d.refreshDeform();
        foreach (i, v; verts) {
            vec2 wp = (m * vec4(v + d.deformation[i], 0, 1)).xy;
            if (wp.x < minX) minX = wp.x;
            if (wp.y < minY) minY = wp.y;
            if (wp.x > maxX) maxX = wp.x;
            if (wp.y > maxY) maxY = wp.y;
        }
    }
    if (!(minX < maxX && minY < maxY)) { res.w = res.h = 0; res.bounds = RectF(0,0,0,0); return res; }

    int pad = opt.padding;
    int w = cast(int)ceil(maxX - minX) + pad * 2;
    int h = cast(int)ceil(maxY - minY) + pad * 2;
    if (opt.targetWidth > 0) w = opt.targetWidth;
    if (opt.targetHeight > 0) h = opt.targetHeight;
    if (w <= 0 || h <= 0) { res.w = res.h = 0; res.bounds = RectF(0,0,0,0); return res; }

    // Output buffer
    ubyte[] outA; outA.length = cast(size_t)w * h; outA[] = 0;

    // Helper: composite alpha (over)
    auto compOver = (ubyte dst, ubyte src) {
        // out = src + dst*(1-src)
        uint s = src;
        uint d = dst;
        uint o_ = s + (d * (255 - s)) / 255;
        return cast(ubyte)o_;
    };

    // Rasterize each drawable
    foreach (d; drawables) {
        auto m = d.getDynamicMatrix();
        auto verts = d.vertices;
        if (verts.length == 0) continue;
        if (d.deformation.length != verts.length) d.refreshDeform();

        // Precompute world positions
        vec2[] wpos; wpos.length = verts.length;
        foreach (i, v; verts) {
            wpos[i] = (m * vec4(v + d.deformation[i], 0, 1)).xy;
        }

        // Mesh data
        auto mesh = d.getMesh();
        if (mesh.indices.length == 0 || mesh.vertices.length == 0) continue;

        // Texture alpha if Part; otherwise no contribution (alpha 0)
        ubyte[] texData;
        int tw = 0, th = 0;
        bool sampleTex = false;
        if (auto p = cast(Part)d) {
            auto tex = p.textures[0];
            if (tex !is null) {
                tw = tex.width; th = tex.height;
                texData = tex.getTextureData();
                sampleTex = true;
            }
        }

        // Rasterize triangles
        foreach (tri; 0 .. mesh.indices.length/3) {
            ushort i0 = mesh.indices[tri*3+0];
            ushort i1 = mesh.indices[tri*3+1];
            ushort i2 = mesh.indices[tri*3+2];

            vec2 A = wpos[i0];
            vec2 B = wpos[i1];
            vec2 C = wpos[i2];

            // Convert to pixel space
            auto toPix = (vec2 p) {
                float fx = (p.x - minX) + pad;
                float fy = (p.y - minY) + pad;
                return vec2(fx, fy);
            };
            vec2 Ap = toPix(A), Bp = toPix(B), Cp = toPix(C);

            int minPX = cast(int)floor(min(Ap.x, min(Bp.x, Cp.x)));
            int maxPX = cast(int)ceil (max(Ap.x, max(Bp.x, Cp.x)));
            int minPY = cast(int)floor(min(Ap.y, min(Bp.y, Cp.y)));
            int maxPY = cast(int)ceil (max(Ap.y, max(Bp.y, Cp.y)));
            if (maxPX < 0 || maxPY < 0 || minPX >= w || minPY >= h) continue;
            minPX = max(minPX, 0); minPY = max(minPY, 0);
            maxPX = min(maxPX, w-1); maxPY = min(maxPY, h-1);

            // Barycentric setup
            float denom = (Bp.y - Cp.y)*(Ap.x - Cp.x) + (Cp.x - Bp.x)*(Ap.y - Cp.y);
            if (denom == 0) continue;

            // UVs
            vec2 uv0, uv1, uv2;
            bool hasUV = mesh.uvs.length == mesh.vertices.length && sampleTex;
            if (hasUV) {
                uv0 = mesh.uvs[i0]; uv1 = mesh.uvs[i1]; uv2 = mesh.uvs[i2];
            }

            foreach (py; minPY .. maxPY+1) {
                foreach (px; minPX .. maxPX+1) {
                    vec2 P = vec2(px + 0.5f, py + 0.5f);
                    float w1 = ((Bp.y - Cp.y)*(P.x - Cp.x) + (Cp.x - Bp.x)*(P.y - Cp.y)) / denom;
                    float w2 = ((Cp.y - Ap.y)*(P.x - Cp.x) + (Ap.x - Cp.x)*(P.y - Cp.y)) / denom;
                    float w3 = 1 - w1 - w2;
                    if (w1 < 0 || w2 < 0 || w3 < 0) continue;

                    ubyte a = 0;
                    if (sampleTex && texData.length >= cast(size_t)tw*th*4 && hasUV) {
                        vec2 uv = uv0 * w1 + uv1 * w2 + uv2 * w3;
                        // clamp 0..1
                        float u = uv.x; if (isNaN(u)) u = 0; u = u < 0 ? 0 : (u > 1 ? 1 : u);
                        float v = uv.y; if (isNaN(v)) v = 0; v = v < 0 ? 0 : (v > 1 ? 1 : v);
                        int tx = cast(int)floor(u * (tw - 1));
                        int ty = cast(int)floor(v * (th - 1));
                        size_t idx = cast(size_t)((ty*tw + tx) * 4 + 3);
                        a = texData[idx];
                    }

                    size_t o = cast(size_t)(py*w + px);
                    outA[o] = compOver(outA[o], a);
                }
            }
        }
    }

    res.w = w; res.h = h; res.alpha = outA;
    res.bounds = RectF(minX, minY, maxX - minX, maxY - minY);
    return res;
}

// Unified alpha preview widget state and renderer
struct AlphaPreviewState { bool show; }

// Global preview texture cache and signature
private Texture gAlphaPrevTex;
private int gAlphaPrevW, gAlphaPrevH;
private string gAlphaPrevSig;

package(nijigenerate) void alphaPreviewDisposeTexture() {
    if (gAlphaPrevTex !is null) {
        gAlphaPrevTex.dispose();
        gAlphaPrevTex = null;
    }
    gAlphaPrevW = 0; gAlphaPrevH = 0; gAlphaPrevSig = null;
}

void alphaPreviewWidget(ref AlphaPreviewState state, ImVec2 size = ImVec2(192, 192)) {
    igCheckbox("show_alpha_preview", &state.show);
    if (!state.show) return;

    auto nodes = incSelectedNodes();
    // Build selection signature
    string sig;
    foreach (n; nodes) sig ~= to!string(n.uuid) ~ ":";

    if (sig != gAlphaPrevSig) {
        gAlphaPrevSig = sig;
        IAlphaProvider provider = null;
        scope(exit) if (provider) provider.dispose();
        if (nodes.length == 1) {
            if (auto part = cast(Part)nodes[0]) provider = new PartAlphaProvider(part);
            else provider = new MeshGroupAlphaProvider(nodes[0]);
        } else if (nodes.length > 1) {
            provider = new GenericProjectionAlphaProvider(nodes);
        }

        if (provider !is null && provider.width() > 0 && provider.height() > 0) {
            int w = provider.width();
            int h = provider.height();
            const(ubyte)* ap = provider.alphaPtr();
            if (gAlphaPrevTex !is null && (gAlphaPrevW != w || gAlphaPrevH != h)) {
                gAlphaPrevTex.dispose();
                gAlphaPrevTex = null;
            }
            if (gAlphaPrevTex is null) {
                gAlphaPrevTex = new Texture(w, h);
                gAlphaPrevW = w; gAlphaPrevH = h;
            }
            ubyte[] rgba = new ubyte[w*h*4];
            foreach (i; 0 .. w*h) {
                ubyte a = ap[i];
                rgba[i*4+0] = 0;
                rgba[i*4+1] = 0;
                rgba[i*4+2] = 0;
                rgba[i*4+3] = a;
            }
            gAlphaPrevTex.setData(rgba);
        }
    }

    if (gAlphaPrevTex !is null) {
        incTextureSlot("Alpha", gAlphaPrevTex, size);
    }
}
