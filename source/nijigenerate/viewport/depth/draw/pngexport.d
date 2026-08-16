module nijigenerate.viewport.depth.draw.pngexport;

import nijigenerate.io.depthimage : ngDepthDrawDecodeDepthPixelsFromRgba;
import nijigenerate.viewport.depth.draw.layer;
import nijigenerate.viewport.depth.draw.manifest;
import nijigenerate.viewport.depth.draw.session;
import imagefmt;
import std.exception : enforce;
import std.file : mkdirRecurse, write;
import std.path : buildPath, dirName, relativePath;
import std.string : format;

struct DepthDrawPngExportResult {
    bool succeeded;
    string manifestPath;
    string[] layerPaths;
    size_t exportedLayers;
    size_t skippedLayers;
}

private string safeLayerFileName(DepthDrawLayer layer, size_t index) {
    auto source = layer.id.length ? layer.id : (layer.displayName.length ? layer.displayName : "layer-%s".format(index));
    string result;
    foreach (c; source) {
        auto safe = (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' ||
            c == '_';
        result ~= safe ? c : '_';
    }
    return result.length ? result : "layer-%s".format(index);
}

ubyte[] ngDepthDrawLayerToExportRgba(DepthDrawLayer layer) {
    enforce(layer.width > 0 && layer.height > 0, "DepthDraw layer dimensions must be positive");
    enforce(layer.depthPixels.length >= cast(size_t)layer.width * cast(size_t)layer.height * 4,
        "DepthDraw layer must have RGBA depth pixels");

    auto depthPixels = ngDepthDrawDecodeDepthPixelsFromRgba(layer.depthPixels, layer.channel);
    ubyte[] result;
    result.length = cast(size_t)layer.width * cast(size_t)layer.height * 4;
    foreach (i; 0 .. cast(size_t)layer.width * cast(size_t)layer.height) {
        auto depth = depthPixels[i];
        auto offset = i * 4;
        result[offset + 0] = depth;
        result[offset + 1] = depth;
        result[offset + 2] = depth;
        result[offset + 3] = layer.depthPixels[offset + 3];
    }
    return result;
}

void ngSaveDepthDrawLayerPng(DepthDrawLayer layer, string path) {
    auto rgba = ngDepthDrawLayerToExportRgba(layer);
    int error;
    auto encoded = write_image_mem(IF_PNG, layer.width, layer.height, rgba, 4, error);
    enforce(error == 0, "DepthDraw PNG export failed: " ~ IF_ERROR[error]);
    scope(exit) _free(encoded.ptr);
    write(path, encoded);
}

DepthDrawPngExportResult ngExportDepthDrawPngSession(
    DepthDrawSession session,
    string outputDir,
    string manifestPath
) {
    enforce(session !is null, "DepthDraw session is required");
    enforce(outputDir.length > 0, "DepthDraw PNG export directory is required");
    enforce(manifestPath.length > 0, "DepthDraw PNG export manifest path is required");
    mkdirRecurse(outputDir);

    auto exported = new DepthDrawSession();
    exported.selectedLayerId = session.selectedLayerId;
    exported.selectedGridUuid = session.selectedGridUuid;
    exported.display = session.display;
    exported.bindings = session.bindings.dup;
    exported.documentWidth = session.documentWidth;
    exported.documentHeight = session.documentHeight;

    DepthDrawPngExportResult result;
    result.manifestPath = manifestPath;
    foreach (i, layer; session.layers) {
        auto exportedLayer = layer;
        if (!layer.hasDepthPixels()) {
            result.skippedLayers++;
            exported.layers ~= exportedLayer;
            continue;
        }

        auto fileName = "%03s-%s.png".format(i, safeLayerFileName(layer, i));
        auto path = buildPath(outputDir, fileName);
        ngSaveDepthDrawLayerPng(layer, path);
        exportedLayer.sourcePath = relativePath(path, manifestPath.dirName);
        exportedLayer.rgba = null;
        exportedLayer.depthPixels = null;
        exportedLayer.alphaMask = null;
        exportedLayer.normalCoverage = null;
        // Cleanup is baked into the exported PNG and must not be replayed.
        exportedLayer.cleanupOperations = null;
        exported.layers ~= exportedLayer;
        result.layerPaths ~= path;
        result.exportedLayers++;
    }

    ngSaveDepthDrawManifest(exported, manifestPath);
    result.succeeded = true;
    return result;
}
