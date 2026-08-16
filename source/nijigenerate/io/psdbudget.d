module nijigenerate.io.psdbudget;

import psd : ChannelType, Layer;

// A decoded mask may coexist with its duplicate and each of the six buffers
// produced by the three horizontal/vertical feather passes until GC runs.
enum ulong PsdMaskExtractionBytesPerPixel = 8;

private bool reserveBytes(ulong requiredBytes, ulong maxBytes, ref ulong reservedBytes) {
    if (reservedBytes > maxBytes || requiredBytes > maxBytes - reservedBytes) return false;
    reservedBytes += requiredBytes;
    return true;
}

private bool reserveRectangle(
    long width,
    long height,
    ulong bytesPerPixel,
    ulong maxBytes,
    ref ulong reservedBytes,
    bool reserveFeatherLine = false,
) {
    if (width <= 0 || height <= 0 || bytesPerPixel == 0) return false;
    auto unsignedWidth = cast(ulong)width;
    auto unsignedHeight = cast(ulong)height;
    if (unsignedWidth > ulong.max / unsignedHeight) return false;
    auto pixelCount = unsignedWidth * unsignedHeight;
    if (pixelCount > ulong.max / bytesPerPixel) return false;
    if (!reserveBytes(pixelCount * bytesPerPixel, maxBytes, reservedBytes)) return false;
    if (!reserveFeatherLine) return true;

    // Feathering performs six passes. Account for every prefix-sum allocation
    // as well as every full-mask allocation above, without relying on GC timing.
    auto lineLength = unsignedWidth > unsignedHeight ? unsignedWidth : unsignedHeight;
    if (lineLength == ulong.max || lineLength + 1 > ulong.max / ulong.sizeof) return false;
    auto lineBytes = (lineLength + 1) * ulong.sizeof;
    if (lineBytes > ulong.max / 6) return false;
    return reserveBytes(lineBytes * 6, maxBytes, reservedBytes);
}

bool ngReservePsdLayerExtraction(
    ref Layer layer,
    ulong layerBytesPerPixel,
    ulong maxBytes,
    ref ulong retainedBytes,
) {
    auto proposedBytes = retainedBytes;
    if (!reserveRectangle(layer.width, layer.height, layerBytesPerPixel, maxBytes, proposedBytes)) return false;

    foreach (ref channel; layer.channels) {
        if (channel.type != ChannelType.LAYER_OR_VECTOR_MASK && channel.type != ChannelType.LAYER_MASK) continue;

        long maskWidth = layer.width;
        long maskHeight = layer.height;
        if (channel.type == ChannelType.LAYER_MASK && layer.layerMask.length > 0) {
            maskWidth = cast(long)layer.layerMask[0].right - layer.layerMask[0].left;
            maskHeight = cast(long)layer.layerMask[0].bottom - layer.layerMask[0].top;
        } else if (channel.type == ChannelType.LAYER_OR_VECTOR_MASK) {
            if (layer.vectorMask.length > 0) {
                maskWidth = cast(long)layer.vectorMask[0].right - layer.vectorMask[0].left;
                maskHeight = cast(long)layer.vectorMask[0].bottom - layer.vectorMask[0].top;
            } else if (layer.layerMask.length > 0) {
                maskWidth = cast(long)layer.layerMask[0].right - layer.layerMask[0].left;
                maskHeight = cast(long)layer.layerMask[0].bottom - layer.layerMask[0].top;
            }
        }
        if (maskWidth < 0 || maskHeight < 0) return false;
        if (maskWidth == 0 || maskHeight == 0) {
            maskWidth = layer.width;
            maskHeight = layer.height;
        }
        if (!reserveRectangle(maskWidth, maskHeight,
                PsdMaskExtractionBytesPerPixel, maxBytes, proposedBytes, true)) return false;
    }

    retainedBytes = proposedBytes;
    return true;
}
