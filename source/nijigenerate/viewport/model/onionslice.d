
module nijigenerate.viewport.model.onionslice;

import nijilive.core.texture;
import nijilive.utils.snapshot;
import nijilive;
import nijigenerate;
import nijilive.core.render.commands : PartDrawPacket;
import std.algorithm : min, max;

struct OnionSliceGeometry {
    rect area;
    mat4 worldMatrix;

    this(rect area, mat4 cameraMatrix, mat4 renderMatrix, mat4 modelMatrix) {
        this.area = area;
        // Snapshot pixels and vertices already include the capture zoom.
        // Preserve their actual draw transform instead of treating pixels as model units.
        worldMatrix = cameraMatrix.inverse * renderMatrix * modelMatrix;
    }

    this(Vec2Array vertices, mat4 cameraMatrix, mat4 renderMatrix, mat4 modelMatrix) {
        // Vec2Array elements are mutable views. Copy their values before accumulating bounds.
        vec2 boundsMin = vertices[0];
        vec2 boundsMax = boundsMin;
        foreach (vertex; vertices) {
            boundsMin.x = min(boundsMin.x, vertex.x);
            boundsMin.y = min(boundsMin.y, vertex.y);
            boundsMax.x = max(boundsMax.x, vertex.x);
            boundsMax.y = max(boundsMax.y, vertex.y);
        }
        this(rect(boundsMin.x, boundsMin.y, boundsMax.x - boundsMin.x, boundsMax.y - boundsMin.y),
            cameraMatrix, renderMatrix, modelMatrix);
    }

    mat4 matrix(mat4 cameraMatrix) const {
        return cameraMatrix * worldMatrix;
    }
}

private class OnionSliceCamera : Camera {
    OnionSliceGeometry geometry;

    override mat4 matrix() {
        return geometry.matrix(inGetCamera().matrix);
    }
}

private struct OnionSliceFrame {
    Texture texture;
    OnionSliceGeometry geometry;
}

// Read the protected composite draw packet without changing the shared snapshot.
private class OnionSliceGeometryReader : Composite {
    static OnionSliceGeometry capture(Composite composite) {
        PartDrawPacket packet;
        composite.fillDrawPacket(packet);
        return OnionSliceGeometry(composite.vertices,
            inGetCamera().matrix, packet.renderMatrix, packet.modelMatrix);
    }
}

class OnionSlice {
private:
    int numHistory;
    OnionSliceFrame[] history;
    OnionSliceCamera overlayCamera;
    Snapshot  snapshot;
    int lastIndex;
    float opacity_;
    vec3 color_;
    vec2u lastKeypoint;
    bool hasLastKeypoint;
    Puppet activePuppet;

    this() {
        numHistory = 2;
        history.length = numHistory;
        overlayCamera = new OnionSliceCamera();
        lastIndex = 0;
        snapshot = null;
        opacity_ = 0.2f;
        color_ = vec3(1, 1, 1);
        lastKeypoint = vec2u(0, 0);
    }

    static OnionSlice instance;

public:
    static OnionSlice singleton() {
        if (instance && instance.activePuppet != incActivePuppet()) {
            instance = null;
        }
        if (instance is null) {
            instance = new OnionSlice();
            instance.activePuppet = incActivePuppet();
        }
        return instance;
    }

    void start() {
        if (snapshot is null) {
            snapshot = Snapshot.get(incActivePuppet());
            hasLastKeypoint = false;
        }
    }

    void stop() {
        if (snapshot !is null) {
            snapshot.release();
            snapshot = null;
            foreach (i; 0..history.length) {
                if (history[i].texture !is null)
                    history[i].texture.dispose();
                history[i] = OnionSliceFrame.init;
            }
        }
    }

    void toggle() {
        if (enabled()) {
            stop();
        } else {
            start();
        }
    }

    void capture(vec2u keypoint) {
        if (snapshot !is null) {
            if (hasLastKeypoint && keypoint == lastKeypoint) return;
            lastKeypoint = keypoint;
            hasLastKeypoint = true;
            snapshot.onCaptured((Texture tex) {
                if (snapshot is null) return;
                auto composite = cast(Composite)activePuppet.root.parent;
                if (composite is null || composite.vertices.length == 0) return;
                auto geometry = OnionSliceGeometryReader.capture(composite);
                if (history[lastIndex].texture !is null) {
                    history[lastIndex].texture.dispose();
                }
                history[lastIndex] = OnionSliceFrame(tex.dup(), geometry);
                lastIndex = (lastIndex + 1)%numHistory;
            });
            snapshot.capture();
        }
    }

    void draw() {
        if (enabled && incArmedParameter()) {
            for (int i = 0; i < numHistory - 1; i ++) {
                auto index = (lastIndex + i)%numHistory;
                if (history[index].texture !is null) {
                    overlayCamera.geometry = history[index].geometry;
                    inDrawTextureAtRect(history[index].texture, history[index].geometry.area, rect(0, 0, 1, 1),
                        opacity_ * (i+2) * (i+2) * (i+2) / numHistory / numHistory / numHistory,
                        color_, vec3(0, 0, 0), null, overlayCamera);
                }
            }
        }
    }

    bool enabled() { return snapshot !is null; }
}
