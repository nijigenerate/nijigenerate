module nijigenerate.io.depthimage;

import nijigenerate.io.depthsample : DepthSampleChannel, DepthSampleConvolution, DepthSamplePoint, DepthSampleResult,
    ngDepthSampleAcceptsAlpha, ngDepthSampleConvolve, ngDepthSampleEffectiveAlpha, ngDepthSampleFrontmost,
    ngDepthSampleMissingPoint, ngDepthSamplePixelDepth, ngDepthSamplePixelDepth01;
import nijigenerate.io.depthmap_psd : PsdDepthChannel, PsdDepthConvolution, PsdDepthImportSettings;
import std.algorithm : max, min, sort;
import std.exception : enforce;
import std.math : abs, ceil, exp, floor, isFinite, lround, round, sqrt;

alias DepthImageChannel = DepthSampleChannel;
alias DepthImageConvolution = DepthSampleConvolution;
alias DepthImageSampleResult = DepthSampleResult;

struct DepthImageSampleSettings {
    bool invert = false;
    float backDepth = -1.0f;
    float frontDepth = 1.0f;
    float depthScale = 1.0f;
    float alphaThreshold = 0.01f;
    int customRadius = 3;
    DepthImageConvolution convolution = DepthImageConvolution.Gaussian3x3;
    DepthImageChannel channel = DepthImageChannel.AverageRGB;
}

PsdDepthImportSettings ngDepthImageToPsdSettings(DepthImageSampleSettings settings) {
    PsdDepthImportSettings result;
    result.invert = settings.invert;
    result.backDepth = settings.backDepth;
    result.frontDepth = settings.frontDepth;
    result.depthScale = settings.depthScale;
    result.alphaThreshold = settings.alphaThreshold;
    result.customRadius = settings.customRadius;
    result.convolution = toPsdConvolution(settings.convolution);
    result.channel = toPsdChannel(settings.channel);
    return result;
}

private PsdDepthChannel toPsdChannel(DepthImageChannel channel) {
    final switch (channel) {
        case DepthImageChannel.AverageRGB:
            return PsdDepthChannel.AverageRGB;
        case DepthImageChannel.R:
            return PsdDepthChannel.R;
        case DepthImageChannel.G:
            return PsdDepthChannel.G;
        case DepthImageChannel.B:
            return PsdDepthChannel.B;
        case DepthImageChannel.Luminance:
            return PsdDepthChannel.Luminance;
    }
}

private PsdDepthConvolution toPsdConvolution(DepthImageConvolution convolution) {
    final switch (convolution) {
        case DepthImageConvolution.Nearest:
            return PsdDepthConvolution.Nearest;
        case DepthImageConvolution.Box3x3:
            return PsdDepthConvolution.Box3x3;
        case DepthImageConvolution.Box5x5:
            return PsdDepthConvolution.Box5x5;
        case DepthImageConvolution.Gaussian3x3:
            return PsdDepthConvolution.Gaussian3x3;
        case DepthImageConvolution.Gaussian5x5:
            return PsdDepthConvolution.Gaussian5x5;
        case DepthImageConvolution.Median3x3:
            return PsdDepthConvolution.Median3x3;
        case DepthImageConvolution.Frontmost3x3:
            return PsdDepthConvolution.Frontmost3x3;
        case DepthImageConvolution.Backmost3x3:
            return PsdDepthConvolution.Backmost3x3;
        case DepthImageConvolution.BoxCustom:
            return PsdDepthConvolution.BoxCustom;
        case DepthImageConvolution.GaussianCustom:
            return PsdDepthConvolution.GaussianCustom;
        case DepthImageConvolution.MedianCustom:
            return PsdDepthConvolution.MedianCustom;
        case DepthImageConvolution.FrontmostCustom:
            return PsdDepthConvolution.FrontmostCustom;
        case DepthImageConvolution.BackmostCustom:
            return PsdDepthConvolution.BackmostCustom;
    }
}

string ngDepthImageChannelName(DepthImageChannel value) {
    final switch (value) {
        case DepthImageChannel.AverageRGB: return "AverageRGB";
        case DepthImageChannel.R: return "R";
        case DepthImageChannel.G: return "G";
        case DepthImageChannel.B: return "B";
        case DepthImageChannel.Luminance: return "Luminance";
    }
}

DepthImageChannel ngDepthImageChannelFromString(string value) {
    switch (value) {
        case "AverageRGB": return DepthImageChannel.AverageRGB;
        case "R": return DepthImageChannel.R;
        case "G": return DepthImageChannel.G;
        case "B": return DepthImageChannel.B;
        case "Luminance": return DepthImageChannel.Luminance;
        default: throw new Exception("Unknown depth image channel: " ~ value);
    }
}

string ngDepthImageConvolutionName(DepthImageConvolution value) {
    final switch (value) {
        case DepthImageConvolution.Nearest: return "Nearest";
        case DepthImageConvolution.Box3x3: return "Box3x3";
        case DepthImageConvolution.Box5x5: return "Box5x5";
        case DepthImageConvolution.Gaussian3x3: return "Gaussian3x3";
        case DepthImageConvolution.Gaussian5x5: return "Gaussian5x5";
        case DepthImageConvolution.Median3x3: return "Median3x3";
        case DepthImageConvolution.Frontmost3x3: return "Frontmost3x3";
        case DepthImageConvolution.Backmost3x3: return "Backmost3x3";
        case DepthImageConvolution.BoxCustom: return "BoxCustom";
        case DepthImageConvolution.GaussianCustom: return "GaussianCustom";
        case DepthImageConvolution.MedianCustom: return "MedianCustom";
        case DepthImageConvolution.FrontmostCustom: return "FrontmostCustom";
        case DepthImageConvolution.BackmostCustom: return "BackmostCustom";
    }
}

DepthImageConvolution ngDepthImageConvolutionFromString(string value) {
    switch (value) {
        case "Nearest": return DepthImageConvolution.Nearest;
        case "Box3x3": return DepthImageConvolution.Box3x3;
        case "Box5x5": return DepthImageConvolution.Box5x5;
        case "Gaussian3x3": return DepthImageConvolution.Gaussian3x3;
        case "Gaussian5x5": return DepthImageConvolution.Gaussian5x5;
        case "Median3x3": return DepthImageConvolution.Median3x3;
        case "Frontmost3x3": return DepthImageConvolution.Frontmost3x3;
        case "Backmost3x3": return DepthImageConvolution.Backmost3x3;
        case "BoxCustom": return DepthImageConvolution.BoxCustom;
        case "GaussianCustom": return DepthImageConvolution.GaussianCustom;
        case "MedianCustom": return DepthImageConvolution.MedianCustom;
        case "FrontmostCustom": return DepthImageConvolution.FrontmostCustom;
        case "BackmostCustom": return DepthImageConvolution.BackmostCustom;
        default: throw new Exception("Unknown depth image convolution: " ~ value);
    }
}

DepthImageSampleResult ngDepthImageSampleRgba(
    const(ubyte)[] rgba,
    int width,
    int height,
    float x,
    float y,
    DepthImageSampleSettings settings
) {
    return ngDepthImageSampleRgbaWithOpacity(rgba, width, height, x, y, 1.0f, settings);
}

DepthImageSampleResult ngDepthImageSampleRgbaWithOpacity(
    const(ubyte)[] rgba,
    int width,
    int height,
    float x,
    float y,
    float opacity,
    DepthImageSampleSettings settings
) {
    return sampleRgbaWithCoverage(
        rgba,
        width,
        height,
        null,
        0,
        0,
        1.0f,
        x,
        y,
        clamp01(opacity),
        settings
    );
}

DepthImageSampleResult ngDepthImageSampleRgbaWithCoverage(
    const(ubyte)[] rgba,
    int width,
    int height,
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    DepthImageSampleSettings settings
) {
    return sampleRgbaWithCoverage(
        rgba,
        width,
        height,
        coverageRgba,
        coverageWidth,
        coverageHeight,
        coverageOpacity,
        x,
        y,
        1.0f,
        settings
    );
}

DepthImageSampleResult ngDepthImageSampleRgbaWithOpacityAndCoverage(
    const(ubyte)[] rgba,
    int width,
    int height,
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float layerOpacity,
    float coverageOpacity,
    float x,
    float y,
    DepthImageSampleSettings settings
) {
    return sampleRgbaWithCoverage(
        rgba,
        width,
        height,
        coverageRgba,
        coverageWidth,
        coverageHeight,
        coverageOpacity,
        x,
        y,
        clamp01(layerOpacity),
        settings
    );
}

DepthImageSampleResult ngDepthImageFrontmost(DepthImageSampleResult[] samples) {
    return ngDepthSampleFrontmost(samples);
}

private DepthImageSampleResult sampleRgbaWithCoverage(
    const(ubyte)[] rgba,
    int width,
    int height,
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    float layerOpacity,
    DepthImageSampleSettings settings
) {
    enforce(width >= 0 && height >= 0, "Image dimensions must be non-negative");
    enforce(coverageWidth >= 0 && coverageHeight >= 0, "Coverage dimensions must be non-negative");
    enforce(rgba.length >= cast(size_t)max(0, width * height) * 4, "RGBA buffer is smaller than dimensions");
    enforce(coverageRgba.length >= cast(size_t)max(0, coverageWidth * coverageHeight) * 4,
        "Coverage RGBA buffer is smaller than dimensions");

    auto centerX = cast(int)round(x);
    auto centerY = cast(int)round(y);
    auto opacity = clamp01(layerOpacity);
    auto coverageLayerOpacity = clamp01(coverageOpacity);

    DepthSamplePoint sampleAt(int sampleX, int sampleY) {
        if (sampleX < 0 || sampleY < 0 || sampleX >= width || sampleY >= height) {
            return ngDepthSampleMissingPoint();
        }
        if (!ngDepthImageCoverageReliableAt(sampleX, sampleY, width, height, coverageRgba, coverageWidth, coverageHeight)) {
            return ngDepthSampleMissingPoint();
        }
        auto index = (cast(size_t)sampleY * cast(size_t)width + cast(size_t)sampleX) * 4;
        auto alpha = ngDepthSampleEffectiveAlpha(rgba, index, opacity) *
            ngDepthImageCoverageAlphaAt(sampleX, sampleY, width, height, coverageRgba, coverageWidth, coverageHeight) *
            coverageLayerOpacity;
        if (!ngDepthSampleAcceptsAlpha(alpha, settings.alphaThreshold)) return ngDepthSampleMissingPoint();
        auto depth = ngDepthSamplePixelDepth(
            rgba,
            index,
            settings.channel,
            settings.invert,
            settings.backDepth,
            settings.frontDepth,
            settings.depthScale
        );
        return DepthSamplePoint(true, depth, alpha);
    }

    auto sample = ngDepthSampleConvolve!sampleAt(settings.convolution, settings.customRadius, centerX, centerY);
    return DepthImageSampleResult(sample.valid, sample.value);
}

float ngDepthImageCoverageAlphaAt(
    int x,
    int y,
    int width,
    int height,
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight
) {
    return ngDepthImageCoverageAlphaAt(x, y, width, height, coverageRgba, coverageWidth, coverageHeight, 4);
}

float ngDepthImageCoverageAlphaAt(
    int x,
    int y,
    int width,
    int height,
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight,
    int coverageChannels
) {
    if (coverageRgba.length == 0 || coverageWidth <= 0 || coverageHeight <= 0) return 1.0f;
    if (x < 0 || y < 0 || x >= width || y >= height) return 0.0f;
    auto coverageX = cast(int)round(((cast(float)x + 0.5f) * cast(float)coverageWidth / cast(float)width) - 0.5f);
    auto coverageY = cast(int)round(((cast(float)y + 0.5f) * cast(float)coverageHeight / cast(float)height) - 0.5f);
    coverageX = max(0, min(coverageWidth - 1, coverageX));
    coverageY = max(0, min(coverageHeight - 1, coverageY));
    auto channels = max(1, coverageChannels);
    auto index = (cast(size_t)coverageY * cast(size_t)coverageWidth + cast(size_t)coverageX) * cast(size_t)channels;
    if (channels < 4) return 1.0f;
    if (index + 3 >= coverageRgba.length) return 1.0f;
    return cast(float)coverageRgba[index + 3] / 255.0f;
}

bool ngDepthImageCoverageReliableAt(
    int x,
    int y,
    int width,
    int height,
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight
) {
    return ngDepthImageCoverageReliableAt(x, y, width, height, coverageRgba, coverageWidth, coverageHeight, 4);
}

bool ngDepthImageCoverageReliableAt(
    int x,
    int y,
    int width,
    int height,
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight,
    int coverageChannels
) {
    return coverageRgba.length == 0 ||
        ngDepthImageCoverageAlphaAt(x, y, width, height, coverageRgba, coverageWidth, coverageHeight, coverageChannels) > 0.5f;
}

float ngDepthImageCoverageAlphaAtUv(
    float u,
    float v,
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight,
    int coverageChannels
) {
    if (coverageRgba.length == 0 || coverageWidth <= 0 || coverageHeight <= 0) return 1.0f;
    auto channels = max(1, coverageChannels);
    if (channels < 4) return 1.0f;

    auto clampedU = max(0.0f, min(1.0f, u));
    auto clampedV = max(0.0f, min(1.0f, v));
    auto x = clampedU * cast(float)max(0, coverageWidth - 1);
    auto y = clampedV * cast(float)max(0, coverageHeight - 1);
    auto x0 = max(0, min(coverageWidth - 1, cast(int)x));
    auto y0 = max(0, min(coverageHeight - 1, cast(int)y));
    auto x1 = max(0, min(coverageWidth - 1, x0 + 1));
    auto y1 = max(0, min(coverageHeight - 1, y0 + 1));
    auto tx = x - cast(float)x0;
    auto ty = y - cast(float)y0;

    auto a00 = coverageAlphaPixel(coverageRgba, coverageWidth, coverageHeight, channels, x0, y0);
    auto a10 = coverageAlphaPixel(coverageRgba, coverageWidth, coverageHeight, channels, x1, y0);
    auto a01 = coverageAlphaPixel(coverageRgba, coverageWidth, coverageHeight, channels, x0, y1);
    auto a11 = coverageAlphaPixel(coverageRgba, coverageWidth, coverageHeight, channels, x1, y1);
    return lerp(lerp(a00, a10, tx), lerp(a01, a11, tx), ty);
}

float ngDepthImageCompositeAlpha(float dst, float alpha) {
    dst = clamp01(dst);
    alpha = clamp01(alpha);
    return 1.0f - ((1.0f - dst) * (1.0f - alpha));
}

private float coverageAlphaPixel(
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight,
    int coverageChannels,
    int x,
    int y
) {
    if (x < 0 || y < 0 || x >= coverageWidth || y >= coverageHeight) return 0.0f;
    auto channels = max(1, coverageChannels);
    auto index = (cast(size_t)y * cast(size_t)coverageWidth + cast(size_t)x) * cast(size_t)channels;
    if (channels < 4 || index + 3 >= coverageRgba.length) return 1.0f;
    return cast(float)coverageRgba[index + 3] / 255.0f;
}

private float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

private float clamp01(float value) {
    return max(0.0f, min(1.0f, value));
}

ubyte[] ngDepthDrawDepthPixelsFromRgbaRed(const(ubyte)[] rgba) {
    enforce(rgba.length % 4 == 0, "RGBA buffer length must be divisible by 4");
    ubyte[] result;
    result.length = rgba.length / 4;
    foreach (i; 0 .. result.length) result[i] = rgba[i * 4];
    return result;
}

ubyte[] ngDepthDrawDecodeGrayscaleDepthPixelsFromRgba(const(ubyte)[] rgba) {
    return ngDepthDrawDecodeDepthPixelsFromRgba(rgba, DepthImageChannel.AverageRGB);
}

ubyte[] ngDepthDrawDecodeDepthPixelsFromRgba(const(ubyte)[] rgba, DepthImageChannel channel) {
    enforce(rgba.length % 4 == 0, "RGBA buffer length must be divisible by 4");
    ubyte[] result;
    result.length = rgba.length / 4;
    foreach (i; 0 .. result.length) {
        auto offset = i * 4;
        if (rgba[offset + 3] == 0) {
            result[i] = 0;
            continue;
        }
        auto depth01 = ngDepthSamplePixelDepth01(rgba, offset, channel, false);
        result[i] = cast(ubyte)min(255, max(0, cast(int)lround(depth01 * 255.0f)));
    }
    return result;
}

ubyte[] ngDepthDrawAlphaMaskFromRgba(const(ubyte)[] rgba, ubyte minAlpha = 1) {
    enforce(rgba.length % 4 == 0, "RGBA buffer length must be divisible by 4");
    ubyte[] mask;
    mask.length = rgba.length / 4;
    foreach (i; 0 .. mask.length) {
        mask[i] = rgba[i * 4 + 3] >= minAlpha ? 1 : 0;
    }
    return mask;
}

ubyte[] ngDepthDrawMaskFromDepthPixels(const(ubyte)[] depthPixels) {
    ubyte[] mask;
    mask.length = depthPixels.length;
    foreach (i, value; depthPixels) {
        mask[i] = value > 0 ? 1 : 0;
    }
    return mask;
}

ubyte[] ngDepthDrawApplyBinaryMaskToDepthPixels(const(ubyte)[] depthPixels, const(ubyte)[] mask) {
    auto length = min(depthPixels.length, mask.length);
    auto result = depthPixels.dup;
    foreach (i; 0 .. length) {
        if (!mask[i]) result[i] = 0;
    }
    foreach (i; length .. result.length) {
        result[i] = 0;
    }
    return result;
}

ubyte[] ngDepthDrawSampleDepthLayerToColorLayer(
    const(ubyte)[] depthPixels,
    int depthWidth,
    int depthHeight,
    int depthLeft,
    int depthTop,
    const(ubyte)[] depthAlphaMask,
    int colorWidth,
    int colorHeight,
    int colorLeft,
    int colorTop
) {
    enforce(depthPixels.length == cast(size_t)(depthWidth * depthHeight), "Depth pixels length must match dimensions");
    enforce(depthAlphaMask.length == 0 || depthAlphaMask.length == depthPixels.length,
        "Depth alpha mask length must match depth pixels");

    ubyte[] result;
    result.length = colorWidth * colorHeight;
    foreach (y; 0 .. colorHeight) {
        auto globalY = colorTop + y;
        auto depthY = globalY - depthTop;
        if (depthY < 0 || depthY >= depthHeight) continue;
        foreach (x; 0 .. colorWidth) {
            auto globalX = colorLeft + x;
            auto depthX = globalX - depthLeft;
            if (depthX < 0 || depthX >= depthWidth) continue;
            auto localIndex = y * colorWidth + x;
            auto depthIndex = depthY * depthWidth + depthX;
            if (depthAlphaMask.length && !depthAlphaMask[depthIndex]) continue;
            result[localIndex] = depthPixels[depthIndex];
        }
    }
    return result;
}

ubyte ngDepthDrawScaleDepthValueAroundCenter(ubyte depth, double scale, double center) {
    if (depth <= 0) return 0;
    if (!scale.isFinite || scale == 1.0 || center <= 0.0) {
        return cast(ubyte)min(255, max(1, cast(int)lround(depth)));
    }
    return cast(ubyte)min(255, max(1, cast(int)lround(center + (cast(double)depth - center) * scale)));
}

struct DepthDrawGlobalDepthScaleResult {
    ubyte[] pixels;
    double centroid;
    size_t count;
}

struct DepthDrawDepthOverrideMasks {
    ubyte[] colorMask;
    ubyte[] strictSurfaceMask;
    ubyte[] surfaceMask;
    ubyte[] renderDepthMask;
}

DepthDrawGlobalDepthScaleResult ngDepthDrawApplyGlobalDepthScale(
    const(ubyte)[] pixels,
    double scale,
    const(ubyte)[] maskPixels = null
) {
    enforce(maskPixels.length == 0 || maskPixels.length == pixels.length, "Depth mask length must match pixels");

    double sum = 0.0;
    size_t count = 0;
    foreach (i, depth; pixels) {
        if (maskPixels.length && !maskPixels[i]) continue;
        if (depth <= 0) continue;
        sum += depth;
        count += 1;
    }

    DepthDrawGlobalDepthScaleResult result;
    result.pixels.length = pixels.length;
    result.centroid = count > 0 ? sum / count : 0.0;
    result.count = count;
    if (count == 0) return result;

    foreach (i, depth; pixels) {
        if (maskPixels.length && !maskPixels[i]) {
            result.pixels[i] = 0;
            continue;
        }
        result.pixels[i] = ngDepthDrawScaleDepthValueAroundCenter(depth, scale, result.centroid);
    }
    return result;
}

ubyte[] ngDepthDrawApplyPsdMaskToAlpha(
    const(ubyte)[] alpha,
    int width,
    int height,
    int layerLeft,
    int layerTop,
    const(ubyte)[] mask,
    int maskWidth,
    int maskHeight,
    int maskLeft,
    int maskTop,
    bool positionRelativeToLayer,
    ubyte defaultMask = 255
) {
    enforce(alpha.length == cast(size_t)(width * height), "Alpha length must match dimensions");
    enforce(mask.length == cast(size_t)(maskWidth * maskHeight), "Mask length must match dimensions");

    auto result = alpha.dup;
    auto localMaskLeft = positionRelativeToLayer ? maskLeft : maskLeft - layerLeft;
    auto localMaskTop = positionRelativeToLayer ? maskTop : maskTop - layerTop;
    foreach (y; 0 .. height) {
        foreach (x; 0 .. width) {
            auto maskX = x - localMaskLeft;
            auto maskY = y - localMaskTop;
            ubyte maskValue = defaultMask;
            if (maskX >= 0 && maskX < maskWidth && maskY >= 0 && maskY < maskHeight) {
                maskValue = mask[maskY * maskWidth + maskX];
            }
            result[y * width + x] = cast(ubyte)min(255, max(0,
                cast(int)lround((cast(double)result[y * width + x] * maskValue) / 255.0)));
        }
    }
    return result;
}

ubyte[] ngDepthDrawScaleAlphaMask(
    const(ubyte)[] sourceMask,
    int sourceWidth,
    int sourceHeight,
    int width,
    int height
) {
    enforce(sourceMask.length == cast(size_t)(sourceWidth * sourceHeight), "Source mask length must match dimensions");
    enforce(sourceWidth >= 0 && sourceHeight >= 0 && width >= 0 && height >= 0, "Mask dimensions must be non-negative");

    ubyte[] result;
    result.length = cast(size_t)(width * height);
    if (sourceWidth == 0 || sourceHeight == 0 || width == 0 || height == 0) return result;

    foreach (y; 0 .. height) {
        auto sourceY = min(sourceHeight - 1, max(0,
            cast(int)floor((cast(double)y + 0.5) * cast(double)sourceHeight / cast(double)height)));
        foreach (x; 0 .. width) {
            auto sourceX = min(sourceWidth - 1, max(0,
                cast(int)floor((cast(double)x + 0.5) * cast(double)sourceWidth / cast(double)width)));
            result[y * width + x] = sourceMask[sourceY * sourceWidth + sourceX] ? 1 : 0;
        }
    }
    return result;
}

ubyte[] ngDepthDrawMaskFromAlphaGreaterThan(const(ubyte)[] alpha, ubyte minAlpha) {
    ubyte[] result;
    result.length = alpha.length;
    foreach (i, value; alpha) result[i] = value > minAlpha ? 1 : 0;
    return result;
}

DepthDrawDepthOverrideMasks ngDepthDrawDepthOverrideMasks(const(ubyte)[] colorAlpha, const(ubyte)[] depthPixels) {
    enforce(colorAlpha.length == depthPixels.length, "Color alpha and depth length must match");

    DepthDrawDepthOverrideMasks result;
    result.colorMask = ngDepthDrawMaskFromAlphaGreaterThan(colorAlpha, 8);
    result.strictSurfaceMask = ngDepthDrawMaskFromAlphaGreaterThan(colorAlpha, 254);

    bool hasStrictSurface = false;
    foreach (value; result.strictSurfaceMask) {
        if (value) {
            hasStrictSurface = true;
            break;
        }
    }
    result.surfaceMask = hasStrictSurface ? result.strictSurfaceMask.dup : result.colorMask.dup;
    result.renderDepthMask.length = depthPixels.length;
    foreach (i, depth; depthPixels) {
        result.renderDepthMask[i] = result.surfaceMask[i] && depth > 0 ? 1 : 0;
    }
    return result;
}

ubyte[] ngDepthDrawPsdExportDepthPixels(
    int sourceWidth,
    int sourceHeight,
    int sourceLeft,
    int sourceTop,
    const(ubyte)[] sourceAlpha,
    int preparedLeft,
    int preparedTop,
    int preparedWidth,
    int preparedHeight,
    const(ubyte)[] exportDepthPixels,
    const(ubyte)[] sourceDepthPixels,
    const(ubyte)[] coverageMask
) {
    enforce(sourceAlpha.length == cast(size_t)(sourceWidth * sourceHeight), "Source alpha length must match dimensions");
    enforce(exportDepthPixels.length == cast(size_t)(preparedWidth * preparedHeight),
        "Export depth length must match dimensions");
    enforce(sourceDepthPixels.length == 0 || sourceDepthPixels.length == exportDepthPixels.length,
        "Source depth length must match export depth length");
    enforce(coverageMask.length == 0 || coverageMask.length == exportDepthPixels.length,
        "Coverage mask length must match export depth length");

    ubyte[] result;
    result.length = sourceWidth * sourceHeight;
    foreach (y; 0 .. sourceHeight) {
        auto globalY = sourceTop + y;
        foreach (x; 0 .. sourceWidth) {
            auto localIndex = y * sourceWidth + x;
            auto globalX = sourceLeft + x;
            auto preparedX = globalX - preparedLeft;
            auto preparedY = globalY - preparedTop;
            if (preparedX < 0 || preparedX >= preparedWidth || preparedY < 0 || preparedY >= preparedHeight) continue;
            auto preparedIndex = preparedY * preparedWidth + preparedX;
            auto sourceDepth = sourceDepthPixels.length ? sourceDepthPixels[preparedIndex] : exportDepthPixels[preparedIndex];
            auto hasDepth = sourceAlpha[localIndex] > 0
                && (coverageMask.length == 0 || coverageMask[preparedIndex])
                && (exportDepthPixels[preparedIndex] > 0 || sourceDepth > 0);
            result[localIndex] = hasDepth ? (exportDepthPixels[preparedIndex] ? exportDepthPixels[preparedIndex] : sourceDepth) : 0;
        }
    }
    return result;
}

struct DepthDrawAlphaDepthFocusedRule {
    int layerIndex;
    int x;
    int y;
    int w;
    int h;
    int lift;
    int radius;
}

struct DepthDrawAlphaDepthGapDetection {
    ubyte[] mask;
    size_t zero;
    size_t depression;
    size_t cliff;
    size_t focusedAdded;
    size_t total;
}

struct DepthDrawAlphaDepthGapFillResult {
    ubyte[] depth;
    size_t filled;
    size_t remaining;
}

float[] ngDepthDrawSmoothGridDepthValues(
    const(float)[] sourceDepths,
    const(ubyte)[] vertexValid,
    const(int)[] vertexGroup,
    int cols,
    int rows,
    float depthThreshold = 18.0f,
    float depthScale = 1.0f
) {
    // Port of depth-draw src/scene/geometry.js:smoothGridVertexPositions.
    enforce(cols >= 0 && rows >= 0, "Grid dimensions must be non-negative");
    auto count = cast(size_t)cols * cast(size_t)rows;
    enforce(sourceDepths.length == count, "Depth count must match grid dimensions");
    enforce(vertexValid.length == count, "Valid mask must match grid dimensions");
    enforce(vertexGroup.length == count, "Vertex groups must match grid dimensions");

    auto input = sourceDepths.dup;
    ubyte[] smoothFlags;
    smoothFlags.length = count;
    auto edgeThresholdZ = max(0.0001f, (depthThreshold / 255.0f) * max(depthScale, 0.0001f));

    bool sameGroupValid(int x, int y, int group) {
        if (x < 0 || y < 0 || x >= cols || y >= rows) return false;
        auto index = cast(size_t)y * cast(size_t)cols + cast(size_t)x;
        return vertexValid[index] != 0 && vertexGroup[index] == group;
    }

    foreach (gy; 0 .. rows) {
        foreach (gx; 0 .. cols) {
            auto index = cast(size_t)gy * cast(size_t)cols + cast(size_t)gx;
            if (!vertexValid[index]) continue;
            auto group = vertexGroup[index];
            bool boundary = !sameGroupValid(gx - 1, gy, group) || !sameGroupValid(gx + 1, gy, group) ||
                !sameGroupValid(gx, gy - 1, group) || !sameGroupValid(gx, gy + 1, group);
            float minZ = input[index];
            float maxZ = input[index];
            int samples;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (!sameGroupValid(gx + dx, gy + dy, group)) continue;
                    auto sampleIndex = cast(size_t)(gy + dy) * cast(size_t)cols + cast(size_t)(gx + dx);
                    minZ = min(minZ, input[sampleIndex]);
                    maxZ = max(maxZ, input[sampleIndex]);
                    samples++;
                }
            }
            smoothFlags[index] = boundary || (samples >= 3 && maxZ - minZ >= edgeThresholdZ) ? 1 : 0;
        }
    }

    foreach (_; 0 .. 2) {
        auto expanded = smoothFlags.dup;
        foreach (gy; 0 .. rows) {
            foreach (gx; 0 .. cols) {
                auto index = cast(size_t)gy * cast(size_t)cols + cast(size_t)gx;
                if (!vertexValid[index] || smoothFlags[index]) continue;
                auto group = vertexGroup[index];
                bool adjacent;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        auto sx = gx + dx;
                        auto sy = gy + dy;
                        if (!sameGroupValid(sx, sy, group)) continue;
                        auto sampleIndex = cast(size_t)sy * cast(size_t)cols + cast(size_t)sx;
                        if (smoothFlags[sampleIndex]) adjacent = true;
                    }
                }
                if (adjacent) expanded[index] = 1;
            }
        }
        smoothFlags = expanded;
    }

    foreach (pass; 0 .. 6) {
        auto output = input.dup;
        auto factor = pass % 2 == 0 ? 0.62f : -0.64f;
        foreach (gy; 0 .. rows) {
            foreach (gx; 0 .. cols) {
                auto index = cast(size_t)gy * cast(size_t)cols + cast(size_t)gx;
                if (!vertexValid[index] || !smoothFlags[index]) continue;
                auto group = vertexGroup[index];
                float weightedSum = 0.0f;
                float weightTotal = 0.0f;
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        auto distance = sqrt(cast(float)(dx * dx + dy * dy));
                        if (distance <= 0.0f || distance > 2.5f) continue;
                        auto sx = gx + dx;
                        auto sy = gy + dy;
                        if (!sameGroupValid(sx, sy, group)) continue;
                        auto sampleIndex = cast(size_t)sy * cast(size_t)cols + cast(size_t)sx;
                        auto weight = 1.0f / distance;
                        weightedSum += input[sampleIndex] * weight;
                        weightTotal += weight;
                    }
                }
                if (weightTotal <= 0.0f) continue;
                auto averageZ = weightedSum / weightTotal;
                output[index] = input[index] + factor * (averageZ - input[index]);
            }
        }
        input = output;
    }
    return input;
}

struct DepthDrawDepthInpaintResult {
    ubyte[] pixels;
    ubyte[] filledMask;
}

struct DepthDrawPruneLayer {
    int left;
    int top;
    int width;
    int height;
}

struct DepthDrawDepthPruneResult {
    ubyte[] pixels;
    ubyte[] debugState;
    ubyte[] debugScore;
}

struct DepthDrawSplitLayer {
    int left;
    int top;
    int width;
    int height;
    ubyte[] maskPixels;
    ubyte[] alphaMask;
}

DepthDrawAlphaDepthGapDetection ngDepthDrawDetectAlphaDepthGaps(
    const(ubyte)[] depth,
    const(ubyte)[] alphaMask,
    int width,
    int height,
    int layerIndex = 0,
    const(DepthDrawAlphaDepthFocusedRule)[] focusedRules = null
) {
    // Port of depth-draw src/composite/alphaDepthGapFill.js:detectAlphaDepthGaps.
    enforce(width >= 0 && height >= 0, "Depth dimensions must be non-negative");
    enforce(depth.length == cast(size_t)(width * height), "Depth length must match dimensions");
    enforce(alphaMask.length == depth.length, "Alpha mask length must match depth");

    DepthDrawAlphaDepthGapDetection result;
    result.mask.length = depth.length;
    auto histogram = depthDrawBuildDepthHistogramIntegral(depth, alphaMask, width, height);

    foreach (y; 0 .. height) {
        foreach (x; 0 .. width) {
            auto index = y * width + x;
            if (!alphaMask[index]) continue;
            auto value = depth[index];
            if (value <= 0) {
                result.mask[index] = 1;
                result.zero += 1;
                continue;
            }
            auto stats16 = depthDrawLocalHistogramStats(histogram, width, height, x, y, 16, value);
            auto stats30 = depthDrawLocalHistogramStats(histogram, width, height, x, y, 30, value);
            auto expected = max(stats16.valid ? stats16.p65 : 0, stats30.valid ? stats30.p65 : 0);
            if (expected > 0 && expected - value >= max(12, cast(int)lround(cast(double)expected * 0.075))) {
                result.mask[index] = 1;
                result.depression += 1;
            }
        }
    }

    foreach (y; 1 .. max(1, height - 1)) {
        foreach (x; 1 .. max(1, width - 1)) {
            auto index = y * width + x;
            if (!alphaMask[index]) continue;
            auto value = depth[index];
            ubyte high;
            foreach (offset; depthDrawEightNeighborOffsets) {
                auto sampleIndex = index + offset[1] * width + offset[0];
                if (alphaMask[sampleIndex] && depth[sampleIndex] > high) high = depth[sampleIndex];
            }
            if (cast(int)high - cast(int)value >= max(16, cast(int)lround(cast(double)high * 0.10))) {
                result.mask[index] = 1;
                result.cliff += 1;
            }
        }
    }

    foreach (rule; focusedRules) {
        if (rule.layerIndex != layerIndex) continue;
        auto x0 = max(0, rule.x);
        auto y0 = max(0, rule.y);
        auto x1 = min(width, rule.x + rule.w);
        auto y1 = min(height, rule.y + rule.h);
        foreach (y; y0 .. y1) {
            foreach (x; x0 .. x1) {
                auto index = y * width + x;
                if (!alphaMask[index]) continue;
                auto stats = depthDrawLocalHistogramStats(histogram, width, height, x, y, rule.radius, depth[index]);
                auto expected = stats.valid ? (stats.p65 ? stats.p65 : stats.p50) : 0;
                if (depth[index] <= 0 || (expected > 0 && expected - depth[index] >= rule.lift)) {
                    if (!result.mask[index]) result.focusedAdded += 1;
                    result.mask[index] = 1;
                }
            }
        }
    }

    foreach (value; result.mask) {
        if (value) result.total += 1;
    }
    return result;
}

DepthDrawAlphaDepthGapFillResult ngDepthDrawMedianFillDepth(
    const(ubyte)[] sourceDepth,
    const(ubyte)[] alphaMask,
    const(ubyte)[] fillMask,
    int width,
    int height
) {
    // Port of depth-draw src/composite/alphaDepthGapFill.js:medianFillDepth.
    enforce(width >= 0 && height >= 0, "Depth dimensions must be non-negative");
    enforce(sourceDepth.length == cast(size_t)(width * height), "Depth length must match dimensions");
    enforce(alphaMask.length == sourceDepth.length, "Alpha mask length must match depth");
    enforce(fillMask.length == sourceDepth.length, "Fill mask length must match depth");

    DepthDrawAlphaDepthGapFillResult result;
    result.depth = sourceDepth.dup;
    auto pending = fillMask.dup;
    ubyte[] queued;
    queued.length = fillMask.length;
    size_t[] pendingIndices;
    size_t[] frontier;

    foreach (index; 0 .. pending.length) {
        if (!pending[index] || !alphaMask[index]) continue;
        pendingIndices ~= index;
        if (depthDrawNeighborMedianValue(result.depth, alphaMask, pending, width, height, index) > 0) {
            queued[index] = 1;
            frontier ~= index;
        }
    }

    while (frontier.length) {
        TupleIndexValue[] updates;
        foreach (index; frontier) {
            queued[index] = 0;
            if (!pending[index] || !alphaMask[index]) continue;
            auto value = depthDrawNeighborMedianValue(result.depth, alphaMask, pending, width, height, index);
            if (value > 0) updates ~= TupleIndexValue(index, value);
        }
        if (!updates.length) break;
        size_t[] nextFrontier;
        foreach (update; updates) {
            result.depth[update.index] = update.value;
            pending[update.index] = 0;
            result.filled += 1;
            depthDrawEnqueuePendingNeighbors(nextFrontier, queued, pending, alphaMask, width, height, update.index);
        }
        frontier = nextFrontier;
    }

    foreach (index; pendingIndices) {
        if (!pending[index] || !alphaMask[index]) continue;
        auto x = cast(int)(index % width);
        auto y = cast(int)(index / width);
        auto value = depthDrawSampleFallbackMedian(result.depth, alphaMask, pending, width, height, x, y);
        if (value > 0) {
            result.depth[index] = value;
            pending[index] = 0;
            result.filled += 1;
        }
    }

    foreach (index; pendingIndices) {
        if (pending[index]) result.remaining += 1;
    }
    return result;
}

ubyte[] ngDepthDrawBuildLayerContourBandMask(const(ubyte)[] maskPixels, int width, int height, int thickness) {
    // Port of depth-draw src/composite/depthCleanup.js:buildLayerContourBandMask.
    enforce(width >= 0 && height >= 0, "Mask dimensions must be non-negative");
    enforce(maskPixels.length == cast(size_t)(width * height), "Mask length must match dimensions");

    ubyte[] contourMask;
    ubyte[] bandMask;
    contourMask.length = maskPixels.length;
    bandMask.length = maskPixels.length;
    foreach (index; 0 .. maskPixels.length) {
        if (depthDrawIsMaskContourPixel(maskPixels, width, height, index)) {
            contourMask[index] = 1;
            bandMask[index] = 1;
        }
    }

    auto frontier = contourMask;
    foreach (_pass; 1 .. max(1, thickness)) {
        frontier = depthDrawExpandMaskFrontier(maskPixels, width, height, frontier, bandMask);
    }
    return bandMask;
}

ubyte[] ngDepthDrawErodePositiveDepthMask(const(ubyte)[] depthPixels, int width, int height, int thickness) {
    // Port of depth-draw src/composite/depthCleanup.js:erodePositiveDepthMask.
    enforce(width >= 0 && height >= 0, "Depth dimensions must be non-negative");
    enforce(depthPixels.length == cast(size_t)(width * height), "Depth length must match dimensions");

    ubyte[] mask;
    mask.length = depthPixels.length;
    foreach (i, value; depthPixels) mask[i] = value > 0 ? 1 : 0;
    foreach (_pass; 0 .. max(0, thickness)) {
        mask = depthDrawErodeBinaryMask(mask, width, height);
    }
    return mask;
}

DepthDrawDepthInpaintResult ngDepthDrawInpaintMaskedLayerDepth(
    const(ubyte)[] sourceDepthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height
) {
    // Port of depth-draw src/composite/depthCleanup.js:inpaintMaskedLayerDepth.
    enforce(width >= 0 && height >= 0, "Depth dimensions must be non-negative");
    enforce(sourceDepthPixels.length == cast(size_t)(width * height), "Depth length must match dimensions");
    enforce(maskPixels.length == sourceDepthPixels.length, "Mask length must match depth");

    DepthDrawDepthInpaintResult result;
    result.pixels = sourceDepthPixels.dup;
    result.filledMask.length = sourceDepthPixels.length;
    size_t[] queue;
    ubyte[] queued;
    queued.length = sourceDepthPixels.length;
    size_t head;

    foreach (index; 0 .. sourceDepthPixels.length) {
        if (!maskPixels[index] || result.pixels[index] > 0) continue;
        if (!depthDrawHasPositiveMaskedNeighbor(result.pixels, maskPixels, width, height, index)) continue;
        queue ~= index;
        queued[index] = 1;
    }

    while (head < queue.length) {
        auto index = queue[head++];
        queued[index] = 0;
        if (!maskPixels[index] || result.pixels[index] > 0) continue;
        auto fillDepth = depthDrawSampleMaskedMultiscaleDepth(result.pixels, maskPixels, width, height, index);
        if (fillDepth <= 0) continue;
        result.pixels[index] = fillDepth;
        result.filledMask[index] = 1;
        depthDrawEnqueueMaskedGapNeighbors(queue, queued, result.pixels, maskPixels, width, height, index);
    }
    return result;
}

ubyte[] ngDepthDrawSmoothMaskedPositiveDepth(
    const(ubyte)[] sourceDepthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height
) {
    // Port of depth-draw src/composite/depthCleanup.js:smoothMaskedPositiveDepth.
    enforce(width >= 0 && height >= 0, "Depth dimensions must be non-negative");
    enforce(sourceDepthPixels.length == cast(size_t)(width * height), "Depth length must match dimensions");
    enforce(maskPixels.length == sourceDepthPixels.length, "Mask length must match depth");

    immutable int[3][] kernel = [
        [-1, -1, 1], [0, -1, 2], [1, -1, 1],
        [-1, 0, 2], [0, 0, 4], [1, 0, 2],
        [-1, 1, 1], [0, 1, 2], [1, 1, 1],
    ];
    auto input = sourceDepthPixels.dup;
    foreach (_pass; 0 .. 3) {
        auto output = input.dup;
        foreach (y; 0 .. height) {
            foreach (x; 0 .. width) {
                auto index = y * width + x;
                if (!maskPixels[index] || input[index] <= 0) continue;
                int weightedSum;
                int totalWeight;
                foreach (entry; kernel) {
                    auto sx = x + entry[0];
                    auto sy = y + entry[1];
                    if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
                    auto sampleIndex = sy * width + sx;
                    auto sampleDepth = input[sampleIndex];
                    if (!maskPixels[sampleIndex] || sampleDepth <= 0) continue;
                    weightedSum += sampleDepth * entry[2];
                    totalWeight += entry[2];
                }
                if (totalWeight > 0) {
                    output[index] = cast(ubyte)min(255, max(0, cast(int)lround(cast(double)weightedSum / totalWeight)));
                }
            }
        }
        input = output;
    }
    return input;
}

DepthDrawDepthPruneResult ngDepthDrawPruneForeignDepthSeeds(
    const(ubyte)[] sourceDepthPixels,
    DepthDrawPruneLayer layer,
    int layerIndex,
    int imageWidth,
    int imageHeight,
    const(ubyte)[] stableDepthPixels,
    const(int)[] visibleLayerMap,
    const(ubyte)[] maskPixels,
    double threshold
) {
    // Port of depth-draw src/composite/depthPrune.js:pruneForeignDepthSeeds.
    enforce(layer.width >= 0 && layer.height >= 0, "Layer dimensions must be non-negative");
    enforce(imageWidth >= 0 && imageHeight >= 0, "Image dimensions must be non-negative");
    enforce(sourceDepthPixels.length == cast(size_t)(layer.width * layer.height),
        "Layer depth length must match layer dimensions");
    enforce(maskPixels.length == sourceDepthPixels.length, "Mask length must match layer depth");
    enforce(stableDepthPixels.length == cast(size_t)(imageWidth * imageHeight),
        "Stable depth length must match image dimensions");
    enforce(visibleLayerMap.length == stableDepthPixels.length, "Visible layer map length must match image dimensions");

    DepthDrawDepthPruneResult result;
    result.pixels = sourceDepthPixels.dup;
    result.debugState.length = result.pixels.length;
    result.debugScore.length = result.pixels.length;
    auto globalSupport = depthDrawCollectPositiveValues(result.pixels);
    auto globalMedian = globalSupport.length ? depthDrawCleanupMedian(globalSupport) : cast(ubyte)0;

    foreach (i; 0 .. result.pixels.length) {
        if (!maskPixels[i]) continue;
        result.debugState[i] = result.pixels[i] > 0 ? 2 : 1;
    }

    foreach (y; 0 .. layer.height) {
        foreach (x; 0 .. layer.width) {
            auto localIndex = y * layer.width + x;
            auto seedDepth = result.pixels[localIndex];
            if (!maskPixels[localIndex] || seedDepth == 0) continue;

            auto globalX = layer.left + x;
            auto globalY = layer.top + y;
            auto sameLayerSupport = depthDrawCollectLocalDepthSupport(
                result.pixels, maskPixels, layer.width, layer.height, x, y, 4);
            if (sameLayerSupport.length < 4) continue;

            auto sameMedian = depthDrawCleanupMedian(sameLayerSupport.dup);
            auto sortedSupport = sameLayerSupport.dup;
            sortedSupport.sort();
            auto q1 = depthDrawPercentileFromSorted(sortedSupport, 0.25);
            auto q3 = depthDrawPercentileFromSorted(sortedSupport, 0.75);
            auto localRange = cast(double)sortedSupport[$ - 1] - cast(double)sortedSupport[0];
            auto foreignSupport = depthDrawCollectForeignVisibleDepthSupport(
                imageWidth, imageHeight, globalX, globalY, 4, layerIndex, stableDepthPixels, visibleLayerMap);
            if (foreignSupport.length < 3) continue;

            auto foreignMedian = depthDrawCleanupMedian(foreignSupport.dup);
            auto sameDistance = abs(cast(double)seedDepth - cast(double)sameMedian);
            auto foreignDistance = abs(cast(double)seedDepth - cast(double)foreignMedian);
            auto globalDistance = abs(cast(double)seedDepth - cast(double)globalMedian);
            auto localThreshold = max(2.0, min(6.0, threshold * 0.1));
            auto bandDistance = seedDepth < q1 ? q1 - seedDepth : seedDepth > q3 ? seedDepth - q3 : 0.0;
            auto hasSharpLocalGradient = localRange > localThreshold * 3.0 && bandDistance > localThreshold;
            auto score = max(sameDistance, globalDistance) - foreignDistance;
            result.debugScore[localIndex] = max(result.debugScore[localIndex],
                cast(ubyte)min(255, max(0, cast(int)lround(score * 24.0))));

            if (((sameDistance > localThreshold && globalDistance > localThreshold) || hasSharpLocalGradient) &&
                foreignDistance < min(sameDistance, globalDistance)) {
                result.pixels[localIndex] = 0;
                result.debugState[localIndex] = 3;
            }
        }
    }

    depthDrawPruneThinForeignSeedComponents(
        result.pixels, result.debugState, result.debugScore, layer, layerIndex, imageWidth, imageHeight,
        stableDepthPixels, visibleLayerMap, maskPixels, globalMedian, threshold);
    return result;
}

int[] ngDepthDrawBuildVisibleLayerMap(int imageWidth, int imageHeight, const(DepthDrawSplitLayer)[] layers) {
    // Port of depth-draw src/composite/depthSplit.js:buildVisibleLayerMap.
    enforce(imageWidth >= 0 && imageHeight >= 0, "Image dimensions must be non-negative");
    int[] visibleLayerMap;
    visibleLayerMap.length = cast(size_t)(imageWidth * imageHeight);
    foreach (ref value; visibleLayerMap) value = -1;

    for (int layerIndex = cast(int)layers.length - 1; layerIndex >= 0; layerIndex -= 1) {
        auto layer = layers[layerIndex];
        auto maskPixels = depthDrawSplitLayerMask(layer);
        enforce(maskPixels.length == cast(size_t)(layer.width * layer.height),
            "Layer mask length must match layer dimensions");
        foreach (y; 0 .. layer.height) {
            foreach (x; 0 .. layer.width) {
                auto localIndex = y * layer.width + x;
                if (!maskPixels[localIndex]) continue;
                auto globalX = layer.left + x;
                auto globalY = layer.top + y;
                if (globalX < 0 || globalX >= imageWidth || globalY < 0 || globalY >= imageHeight) continue;
                auto globalIndex = globalY * imageWidth + globalX;
                if (visibleLayerMap[globalIndex] < 0) visibleLayerMap[globalIndex] = layerIndex;
            }
        }
    }
    return visibleLayerMap;
}

ubyte[] ngDepthDrawSeedLayerDepthPixels(
    DepthDrawSplitLayer layer,
    int layerIndex,
    int imageWidth,
    int imageHeight,
    const(ubyte)[] stableDepthPixels,
    const(int)[] visibleLayerMap,
    const(ubyte)[] maskPixels,
    const(DepthDrawSplitLayer)[] layers,
    const(ubyte)[] contourBandMask = null,
    int upperMaskRadius = 2
) {
    // Port of depth-draw src/composite/depthSplit.js:seedLayerDepthPixels.
    enforce(layer.width >= 0 && layer.height >= 0, "Layer dimensions must be non-negative");
    enforce(imageWidth >= 0 && imageHeight >= 0, "Image dimensions must be non-negative");
    enforce(maskPixels.length == cast(size_t)(layer.width * layer.height),
        "Mask length must match layer dimensions");
    enforce(stableDepthPixels.length == cast(size_t)(imageWidth * imageHeight),
        "Stable depth length must match image dimensions");
    enforce(visibleLayerMap.length == stableDepthPixels.length, "Visible layer map length must match image dimensions");
    if (contourBandMask.length) {
        enforce(contourBandMask.length == maskPixels.length, "Contour band mask length must match layer dimensions");
    }

    ubyte[] depthPixels;
    depthPixels.length = cast(size_t)(layer.width * layer.height);
    foreach (y; 0 .. layer.height) {
        foreach (x; 0 .. layer.width) {
            auto localIndex = y * layer.width + x;
            if (!maskPixels[localIndex]) continue;
            if (contourBandMask.length && contourBandMask[localIndex]) continue;

            auto globalX = layer.left + x;
            auto globalY = layer.top + y;
            if (globalX < 0 || globalX >= imageWidth || globalY < 0 || globalY >= imageHeight) continue;
            auto globalIndex = globalY * imageWidth + globalX;
            if (visibleLayerMap[globalIndex] == layerIndex) {
                if (ngDepthDrawHasUpperLayerMaskNearby(layers, layerIndex, globalX, globalY, upperMaskRadius)) continue;
                depthPixels[localIndex] = stableDepthPixels[globalIndex];
            }
        }
    }
    return depthPixels;
}

bool ngDepthDrawHasUpperLayerMaskNearby(
    const(DepthDrawSplitLayer)[] layers,
    int layerIndex,
    int globalX,
    int globalY,
    int radius
) {
    // Port of depth-draw src/composite/depthSplit.js:hasUpperLayerMaskNearby.
    foreach (upperIndex; layerIndex + 1 .. cast(int)layers.length) {
        auto layer = layers[upperIndex];
        auto maskPixels = depthDrawSplitLayerMask(layer);
        enforce(maskPixels.length == cast(size_t)(layer.width * layer.height),
            "Layer mask length must match layer dimensions");
        if (globalX < layer.left - radius ||
            globalX >= layer.left + layer.width + radius ||
            globalY < layer.top - radius ||
            globalY >= layer.top + layer.height + radius) {
            continue;
        }

        foreach (dy; -radius .. radius + 1) {
            auto sy = globalY + dy;
            auto localY = sy - layer.top;
            if (localY < 0 || localY >= layer.height) continue;
            foreach (dx; -radius .. radius + 1) {
                auto sx = globalX + dx;
                auto localX = sx - layer.left;
                if (localX < 0 || localX >= layer.width) continue;
                if (maskPixels[localY * layer.width + localX]) return true;
            }
        }
    }
    return false;
}

ubyte[] ngDepthDrawCreateMaskedGridDepthPixels(
    int width,
    int height,
    const(ubyte)[] sourcePixels,
    const(ubyte)[] maskPixels,
    string specMode,
    int gridX,
    int gridY,
    int kernelSize,
    string interpMode
) {
    enforce(sourcePixels.length == cast(size_t)(width * height), "Source pixels length must match dimensions");
    enforce(maskPixels.length == sourcePixels.length, "Mask pixels length must match source pixels");
    enforce(interpMode == "linear" || interpMode == "cubic",
        "Only depth-draw linear and cubic grid interpolation are implemented");

    auto gridWidth = specMode == "size"
        ? max(2, cast(int)ceil(cast(double)(width - 1) / cast(double)max(1, gridX)) + 1)
        : max(2, gridX + 1);
    auto gridHeight = specMode == "size"
        ? max(2, cast(int)ceil(cast(double)(height - 1) / cast(double)max(1, gridY)) + 1)
        : max(2, gridY + 1);
    auto radius = max(1, kernelSize / 2);
    auto kernel = depthDrawGaussianKernel(radius);

    double[] controlValues;
    ubyte[] controlValid;
    controlValues.length = gridWidth * gridHeight;
    controlValid.length = gridWidth * gridHeight;

    foreach (gy; 0 .. gridHeight) {
        auto py = depthDrawSampleGridPosition(gy, gridHeight, height);
        foreach (gx; 0 .. gridWidth) {
            auto px = depthDrawSampleGridPosition(gx, gridWidth, width);
            auto sample = depthDrawConvolveDepthAt(sourcePixels, width, height, px, py, radius, kernel);
            auto index = gy * gridWidth + gx;
            controlValues[index] = sample.value;
            controlValid[index] = sample.valid ? 1 : 0;
        }
    }

    ubyte[] result;
    result.length = sourcePixels.length;
    foreach (y; 0 .. height) {
        auto fy = (cast(double)y / cast(double)max(1, height - 1)) * (gridHeight - 1);
        foreach (x; 0 .. width) {
            auto fx = (cast(double)x / cast(double)max(1, width - 1)) * (gridWidth - 1);
            auto value = interpMode == "cubic"
                ? depthDrawSampleCubicGrid(controlValues, controlValid, gridWidth, gridHeight, fx, fy)
                : depthDrawSampleLinearGrid(controlValues, controlValid, gridWidth, gridHeight, fx, fy);
            auto rounded = cast(ubyte)min(255, max(0, cast(int)lround(value)));
            auto index = y * width + x;
            result[index] = maskPixels[index] ? rounded : 0;
        }
    }
    return result;
}

private struct DepthDrawConvolutionSample {
    double value;
    bool valid;
}

private struct TupleIndexValue {
    size_t index;
    ubyte value;
}

private struct DepthDrawHistogramIntegral {
    uint[][] bins;
    int stride;
}

private struct DepthDrawHistogramStats {
    bool valid;
    int p50;
    int p65;
    int p80;
}

private enum depthDrawHistogramBins = 16;
private enum depthDrawHistogramShift = 4;
private immutable int[2][] depthDrawEightNeighborOffsets = [
    [-1, 0],
    [1, 0],
    [0, -1],
    [0, 1],
    [-1, -1],
    [1, -1],
    [-1, 1],
    [1, 1],
];

private DepthDrawHistogramIntegral depthDrawBuildDepthHistogramIntegral(
    const(ubyte)[] depth,
    const(ubyte)[] alphaMask,
    int width,
    int height
) {
    DepthDrawHistogramIntegral histogram;
    histogram.stride = width + 1;
    auto size = cast(size_t)histogram.stride * cast(size_t)(height + 1);
    histogram.bins.length = depthDrawHistogramBins;
    foreach (bin; 0 .. depthDrawHistogramBins) {
        histogram.bins[bin].length = size;
    }

    uint[] rowCounts;
    rowCounts.length = depthDrawHistogramBins;
    foreach (y; 1 .. height + 1) {
        rowCounts[] = 0;
        auto sourceRow = (y - 1) * width;
        auto outputRow = y * histogram.stride;
        auto previousRow = (y - 1) * histogram.stride;
        foreach (x; 1 .. width + 1) {
            auto sourceIndex = sourceRow + x - 1;
            auto value = depth[sourceIndex];
            if (alphaMask[sourceIndex] && value > 0) {
                rowCounts[value >> depthDrawHistogramShift] += 1;
            }
            auto outputIndex = outputRow + x;
            auto previousIndex = previousRow + x;
            foreach (bin; 0 .. depthDrawHistogramBins) {
                histogram.bins[bin][outputIndex] = histogram.bins[bin][previousIndex] + rowCounts[bin];
            }
        }
    }
    return histogram;
}

private DepthDrawHistogramStats depthDrawLocalHistogramStats(
    const(DepthDrawHistogramIntegral) histogram,
    int width,
    int height,
    int x,
    int y,
    int radius,
    int centerValue = 0
) {
    auto x0 = max(0, x - radius);
    auto y0 = max(0, y - radius);
    auto x1 = min(width, x + radius + 1);
    auto y1 = min(height, y + radius + 1);
    uint total;

    foreach (bin; 0 .. depthDrawHistogramBins) {
        auto count = depthDrawIntegralRangeSum(histogram.bins[bin], histogram.stride, x0, y0, x1, y1);
        if (centerValue > 0 && bin == (centerValue >> depthDrawHistogramShift)) {
            count = count > 0 ? count - 1 : 0;
        }
        total += count;
    }

    DepthDrawHistogramStats stats;
    if (total < 10) return stats;
    auto target50 = max(1u, cast(uint)ceil(cast(double)total * 0.50));
    auto target65 = max(1u, cast(uint)ceil(cast(double)total * 0.65));
    auto target80 = max(1u, cast(uint)ceil(cast(double)total * 0.80));
    uint cumulative;
    stats.valid = true;
    stats.p50 = 255;
    stats.p65 = 255;
    stats.p80 = 255;

    foreach (bin; 0 .. depthDrawHistogramBins) {
        auto count = depthDrawIntegralRangeSum(histogram.bins[bin], histogram.stride, x0, y0, x1, y1);
        if (centerValue > 0 && bin == (centerValue >> depthDrawHistogramShift)) {
            count = count > 0 ? count - 1 : 0;
        }
        cumulative += count;
        auto value = min(255, (bin << depthDrawHistogramShift) + (1 << (depthDrawHistogramShift - 1)));
        if (stats.p50 == 255 && cumulative >= target50) stats.p50 = value;
        if (stats.p65 == 255 && cumulative >= target65) stats.p65 = value;
        if (cumulative >= target80) {
            stats.p80 = value;
            break;
        }
    }
    return stats;
}

private uint depthDrawIntegralRangeSum(const(uint)[] integral, int stride, int x0, int y0, int x1, int y1) {
    auto topLeft = y0 * stride + x0;
    auto topRight = y0 * stride + x1;
    auto bottomLeft = y1 * stride + x0;
    auto bottomRight = y1 * stride + x1;
    return integral[bottomRight] - integral[topRight] - integral[bottomLeft] + integral[topLeft];
}

private ubyte depthDrawNeighborMedianValue(
    const(ubyte)[] depth,
    const(ubyte)[] alphaMask,
    const(ubyte)[] pending,
    int width,
    int height,
    size_t index
) {
    auto x = cast(int)(index % width);
    auto y = cast(int)(index / width);
    ubyte[] values;
    foreach (offset; depthDrawEightNeighborOffsets) {
        auto sx = x + offset[0];
        auto sy = y + offset[1];
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = cast(size_t)sy * cast(size_t)width + cast(size_t)sx;
        if (alphaMask[sampleIndex] && !pending[sampleIndex] && depth[sampleIndex] > 0) {
            values ~= depth[sampleIndex];
        }
    }
    return values.length ? depthDrawMedian(values) : 0;
}

private void depthDrawEnqueuePendingNeighbors(
    ref size_t[] frontier,
    ref ubyte[] queued,
    const(ubyte)[] pending,
    const(ubyte)[] alphaMask,
    int width,
    int height,
    size_t index
) {
    auto x = cast(int)(index % width);
    auto y = cast(int)(index / width);
    foreach (offset; depthDrawEightNeighborOffsets) {
        auto sx = x + offset[0];
        auto sy = y + offset[1];
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = cast(size_t)sy * cast(size_t)width + cast(size_t)sx;
        if (pending[sampleIndex] && alphaMask[sampleIndex] && !queued[sampleIndex]) {
            queued[sampleIndex] = 1;
            frontier ~= sampleIndex;
        }
    }
}

private ubyte depthDrawSampleFallbackMedian(
    const(ubyte)[] depth,
    const(ubyte)[] alphaMask,
    const(ubyte)[] pending,
    int width,
    int height,
    int x,
    int y
) {
    foreach (radius; [6, 12, 24, 48, 96]) {
        ubyte[] values;
        foreach (dy; -radius .. radius + 1) {
            foreach (dx; -radius .. radius + 1) {
                auto distanceSq = dx * dx + dy * dy;
                if (distanceSq <= 0 || distanceSq > radius * radius) continue;
                auto sx = x + dx;
                auto sy = y + dy;
                if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
                auto sampleIndex = cast(size_t)sy * cast(size_t)width + cast(size_t)sx;
                if (alphaMask[sampleIndex] && !pending[sampleIndex] && depth[sampleIndex] > 0) {
                    values ~= depth[sampleIndex];
                }
            }
        }
        if (values.length) return depthDrawMedian(values);
    }
    return 0;
}

private ubyte depthDrawMedian(ubyte[] values) {
    values.sort();
    return values[values.length / 2];
}

private ubyte depthDrawCleanupMedian(ubyte[] values) {
    values.sort();
    return values[(values.length - 1) >> 1];
}

private double depthDrawPercentileFromSorted(const(ubyte)[] sortedValues, double percentile) {
    if (!sortedValues.length) return 0.0;
    auto index = max(0, min(cast(int)sortedValues.length - 1,
        cast(int)lround(cast(double)(sortedValues.length - 1) * percentile)));
    return sortedValues[index];
}

private bool depthDrawIsMaskContourPixel(const(ubyte)[] maskPixels, int width, int height, size_t index) {
    if (!maskPixels[index]) return false;
    auto x = cast(int)(index % width);
    auto y = cast(int)(index / width);
    foreach (offset; [[-1, 0], [1, 0], [0, -1], [0, 1]]) {
        auto sx = x + offset[0];
        auto sy = y + offset[1];
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) return true;
        if (!maskPixels[sy * width + sx]) return true;
    }
    return false;
}

private ubyte[] depthDrawExpandMaskFrontier(
    const(ubyte)[] maskPixels,
    int width,
    int height,
    const(ubyte)[] frontier,
    ref ubyte[] bandMask
) {
    ubyte[] next;
    next.length = maskPixels.length;
    foreach (index; 0 .. frontier.length) {
        if (!frontier[index]) continue;
        auto x = cast(int)(index % width);
        auto y = cast(int)(index / width);
        foreach (offset; depthDrawEightNeighborOffsets) {
            auto sx = x + offset[0];
            auto sy = y + offset[1];
            if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
            auto sampleIndex = cast(size_t)sy * cast(size_t)width + cast(size_t)sx;
            if (!maskPixels[sampleIndex] || bandMask[sampleIndex]) continue;
            bandMask[sampleIndex] = 1;
            next[sampleIndex] = 1;
        }
    }
    return next;
}

private ubyte[] depthDrawErodeBinaryMask(const(ubyte)[] maskPixels, int width, int height) {
    ubyte[] eroded;
    eroded.length = maskPixels.length;
    foreach (index; 0 .. maskPixels.length) {
        if (!maskPixels[index]) continue;
        auto x = cast(int)(index % width);
        auto y = cast(int)(index / width);
        bool keep = true;
        foreach (offset; depthDrawEightNeighborOffsets) {
            auto sx = x + offset[0];
            auto sy = y + offset[1];
            if (sx < 0 || sx >= width || sy < 0 || sy >= height || !maskPixels[sy * width + sx]) {
                keep = false;
                break;
            }
        }
        if (keep) eroded[index] = 1;
    }
    return eroded;
}

private bool depthDrawHasPositiveMaskedNeighbor(
    const(ubyte)[] depthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height,
    size_t index
) {
    auto x = cast(int)(index % width);
    auto y = cast(int)(index / width);
    foreach (offset; depthDrawEightNeighborOffsets) {
        auto sx = x + offset[0];
        auto sy = y + offset[1];
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = sy * width + sx;
        if (maskPixels[sampleIndex] && depthPixels[sampleIndex] > 0) return true;
    }
    return false;
}

private ubyte depthDrawSampleMaskedNeighborMedian(
    const(ubyte)[] depthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height,
    size_t index
) {
    auto x = cast(int)(index % width);
    auto y = cast(int)(index / width);
    ubyte[] values;
    foreach (offset; depthDrawEightNeighborOffsets) {
        auto sx = x + offset[0];
        auto sy = y + offset[1];
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = sy * width + sx;
        if (!maskPixels[sampleIndex] || depthPixels[sampleIndex] <= 0) continue;
        values ~= depthPixels[sampleIndex];
    }
    return values.length ? depthDrawCleanupMedian(values) : 0;
}

private ubyte depthDrawSampleMaskedMultiscaleDepth(
    const(ubyte)[] depthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height,
    size_t index
) {
    double weightedDepth = 0.0;
    double totalWeight = 0.0;

    auto estimate = depthDrawEstimateMaskedDepthAtScale(depthPixels, maskPixels, width, height, index, 1, 4, 0.48);
    if (estimate.valid) {
        auto confidence = estimate.weight * estimate.confidence;
        weightedDepth += estimate.depth * confidence;
        totalWeight += confidence;
    }
    estimate = depthDrawEstimateMaskedDepthAtScale(depthPixels, maskPixels, width, height, index, 7, 5, 0.32);
    if (estimate.valid) {
        auto confidence = estimate.weight * estimate.confidence;
        weightedDepth += estimate.depth * confidence;
        totalWeight += confidence;
    }
    estimate = depthDrawEstimateMaskedDepthAtScale(depthPixels, maskPixels, width, height, index, 37, 5, 0.20);
    if (estimate.valid) {
        auto confidence = estimate.weight * estimate.confidence;
        weightedDepth += estimate.depth * confidence;
        totalWeight += confidence;
    }
    if (totalWeight > 0.0) {
        return cast(ubyte)min(255, max(0, cast(int)lround(weightedDepth / totalWeight)));
    }
    return depthDrawSampleMaskedNeighborMedian(depthPixels, maskPixels, width, height, index);
}

private struct DepthDrawMaskedDepthEstimate {
    bool valid;
    double depth;
    double confidence;
    double weight;
}

private struct DepthDrawMaskedSparseGridDepths {
    double[] grid;
    ubyte[] values;
}

private DepthDrawMaskedDepthEstimate depthDrawEstimateMaskedDepthAtScale(
    const(ubyte)[] depthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height,
    size_t index,
    int radius,
    int minSamples,
    double weight
) {
    auto x0 = cast(int)(index % width);
    auto y0 = cast(int)(index / width);
    auto sampled = depthDrawSampleMaskedSparseGridDepths(depthPixels, maskPixels, width, height, x0, y0, radius);
    DepthDrawMaskedDepthEstimate result;
    result.weight = weight;
    if (sampled.values.length < cast(size_t)minSamples) return result;

    auto sortedValues = sampled.values.dup;
    sortedValues.sort();
    double centerMean = 0.0;
    foreach (value; sampled.values) centerMean += value;
    centerMean /= sampled.values.length;
    foreach (ref value; sampled.grid) {
        if (value <= 0.0) value = centerMean;
    }

    auto planeDepth = depthDrawEstimateGridPlaneDepth(sampled.grid);
    auto medianDepth = depthDrawCleanupMedian(sampled.values.dup);
    auto lo = depthDrawPercentileFromSorted(sortedValues, 0.2);
    auto hi = depthDrawPercentileFromSorted(sortedValues, 0.8);
    auto robustDepth = max(lo, min(hi, round(planeDepth * 0.7 + medianDepth * 0.3)));
    result.valid = true;
    result.depth = robustDepth;
    result.confidence = max(0.0, min(1.0, cast(double)sampled.values.length / 9.0));
    return result;
}

private DepthDrawMaskedSparseGridDepths depthDrawSampleMaskedSparseGridDepths(
    const(ubyte)[] depthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height,
    int x0,
    int y0,
    int radius
) {
    DepthDrawMaskedSparseGridDepths result;
    result.grid.length = 9;
    auto searchRadius = max(1, cast(int)floor(cast(double)radius / 3.0));
    size_t cursor;
    foreach (gy; -1 .. 2) {
        foreach (gx; -1 .. 2) {
            auto targetX = max(0, min(width - 1, cast(int)lround(x0 + gx * radius)));
            auto targetY = max(0, min(height - 1, cast(int)lround(y0 + gy * radius)));
            auto sampledDepth = depthDrawSampleNearestMaskedDepth(
                depthPixels, maskPixels, width, height, targetX, targetY, searchRadius);
            result.grid[cursor++] = sampledDepth;
            if (sampledDepth > 0) result.values ~= sampledDepth;
        }
    }
    return result;
}

private ubyte depthDrawSampleNearestMaskedDepth(
    const(ubyte)[] depthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height,
    int targetX,
    int targetY,
    int searchRadius
) {
    ubyte bestDepth;
    auto bestDistanceSq = int.max;
    foreach (dy; -searchRadius .. searchRadius + 1) {
        auto y = targetY + dy;
        if (y < 0 || y >= height) continue;
        foreach (dx; -searchRadius .. searchRadius + 1) {
            auto x = targetX + dx;
            if (x < 0 || x >= width) continue;
            auto index = y * width + x;
            auto depth = depthPixels[index];
            if (!maskPixels[index] || depth <= 0) continue;
            auto distanceSq = dx * dx + dy * dy;
            if (distanceSq < bestDistanceSq) {
                bestDistanceSq = distanceSq;
                bestDepth = depth;
            }
        }
    }
    return bestDepth;
}

private double depthDrawEstimateGridPlaneDepth(const(double)[] grid) {
    auto tl = grid[0];
    auto tc = grid[1];
    auto tr = grid[2];
    auto ml = grid[3];
    auto mc = grid[4];
    auto mr = grid[5];
    auto bl = grid[6];
    auto bc = grid[7];
    auto br = grid[8];
    auto gradX = (tr + 2 * mr + br) - (tl + 2 * ml + bl);
    auto gradY = (bl + 2 * bc + br) - (tl + 2 * tc + tr);
    return mc + (gradX + gradY) * 0.125;
}

private void depthDrawEnqueueMaskedGapNeighbors(
    ref size_t[] queue,
    ref ubyte[] queued,
    const(ubyte)[] depthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height,
    size_t index
) {
    auto x = cast(int)(index % width);
    auto y = cast(int)(index / width);
    foreach (offset; depthDrawEightNeighborOffsets) {
        auto sx = x + offset[0];
        auto sy = y + offset[1];
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = sy * width + sx;
        if (!maskPixels[sampleIndex] || depthPixels[sampleIndex] > 0 || queued[sampleIndex]) continue;
        queue ~= sampleIndex;
        queued[sampleIndex] = 1;
    }
}

private void depthDrawPruneThinForeignSeedComponents(
    ref ubyte[] depthPixels,
    ref ubyte[] debugState,
    ref ubyte[] debugScore,
    DepthDrawPruneLayer layer,
    int layerIndex,
    int imageWidth,
    int imageHeight,
    const(ubyte)[] stableDepthPixels,
    const(int)[] visibleLayerMap,
    const(ubyte)[] maskPixels,
    ubyte globalMedian,
    double threshold
) {
    auto totalPixels = cast(size_t)(layer.width * layer.height);
    ubyte[] visited;
    int[] componentIds;
    visited.length = totalPixels;
    componentIds.length = totalPixels;
    foreach (ref id; componentIds) id = -1;
    auto contourMask = ngDepthDrawBuildLayerContourBandMask(maskPixels, layer.width, layer.height, 1);
    auto wideContourMask = ngDepthDrawBuildLayerContourBandMask(maskPixels, layer.width, layer.height, 4);
    auto linkThreshold = max(8.0, threshold * 0.35);
    size_t[] queue;
    int componentId;

    foreach (start; 0 .. totalPixels) {
        if (visited[start] || !maskPixels[start] || depthPixels[start] == 0) continue;

        size_t head;
        queue.length = 0;
        queue ~= start;
        visited[start] = 1;
        componentIds[start] = componentId;
        size_t[] indices;
        ubyte[] values;
        auto minX = layer.width;
        int maxX;
        auto minY = layer.height;
        int maxY;
        size_t contourHits;
        size_t wideContourHits;

        while (head < queue.length) {
            auto index = queue[head++];
            indices ~= index;
            values ~= depthPixels[index];
            auto x = cast(int)(index % layer.width);
            auto y = cast(int)(index / layer.width);
            minX = min(minX, x);
            maxX = max(maxX, x);
            minY = min(minY, y);
            maxY = max(maxY, y);
            if (contourMask[index]) contourHits += 1;
            if (wideContourMask[index]) wideContourHits += 1;

            foreach (offset; depthDrawEightNeighborOffsets) {
                auto sx = x + offset[0];
                auto sy = y + offset[1];
                if (sx < 0 || sx >= layer.width || sy < 0 || sy >= layer.height) continue;
                auto sampleIndex = cast(size_t)sy * cast(size_t)layer.width + cast(size_t)sx;
                if (visited[sampleIndex] || !maskPixels[sampleIndex] || depthPixels[sampleIndex] == 0) continue;
                if (abs(cast(double)depthPixels[sampleIndex] - cast(double)depthPixels[index]) > linkThreshold) continue;
                visited[sampleIndex] = 1;
                componentIds[sampleIndex] = componentId;
                queue ~= sampleIndex;
            }
        }

        auto width = maxX - minX + 1;
        auto height = maxY - minY + 1;
        auto componentMedian = depthDrawCleanupMedian(values.dup);
        auto sameLayerSupport = depthDrawCollectComponentExternalDepthSupport(
            depthPixels, maskPixels, componentIds, componentId, wideContourMask, layer.width, layer.height,
            minX, minY, maxX, maxY, 5);
        auto foreignSupport = depthDrawCollectComponentForeignVisibleDepthSupport(
            indices, layer, imageWidth, imageHeight, stableDepthPixels, visibleLayerMap, layerIndex);
        auto contourRatio = cast(double)contourHits / cast(double)indices.length;
        auto wideContourRatio = cast(double)wideContourHits / cast(double)indices.length;
        auto sameMedian = sameLayerSupport.length ? depthDrawCleanupMedian(sameLayerSupport.dup) : globalMedian;
        auto foreignMedian = foreignSupport.length ? depthDrawCleanupMedian(foreignSupport.dup) : componentMedian;
        auto sameDistance = abs(cast(double)componentMedian - cast(double)sameMedian);
        auto foreignDistance = abs(cast(double)componentMedian - cast(double)foreignMedian);
        auto isThin = min(width, height) <= 3 || indices.length <= cast(size_t)(max(width, height) * 2);
        auto isSmallish = indices.length <= cast(size_t)max(48.0, threshold * 8.0);
        auto localThreshold = max(2.0, min(6.0, threshold * 0.1));
        auto componentScore = sameDistance - foreignDistance;

        if (foreignSupport.length >= 4 &&
            (sameDistance > localThreshold || (sameLayerSupport.length < 4 && wideContourRatio > 0.7)) &&
            foreignDistance < sameDistance &&
            (contourRatio >= 0.35 || wideContourRatio >= 0.7) &&
            (isThin || isSmallish)) {
            foreach (index; indices) {
                depthPixels[index] = 0;
                debugState[index] = 4;
                debugScore[index] = max(debugScore[index],
                    cast(ubyte)min(255, max(0, cast(int)lround(componentScore * 24.0))));
            }
        }

        componentId += 1;
    }
}

private ubyte[] depthDrawCollectComponentExternalDepthSupport(
    const(ubyte)[] depthPixels,
    const(ubyte)[] maskPixels,
    const(int)[] componentIds,
    int componentId,
    const(ubyte)[] contourMask,
    int width,
    int height,
    int minX,
    int minY,
    int maxX,
    int maxY,
    int radius
) {
    ubyte[] values;
    auto startX = max(0, minX - radius);
    auto startY = max(0, minY - radius);
    auto endX = min(width - 1, maxX + radius);
    auto endY = min(height - 1, maxY + radius);

    foreach (y; startY .. endY + 1) {
        foreach (x; startX .. endX + 1) {
            auto index = y * width + x;
            if (!maskPixels[index] || depthPixels[index] == 0 ||
                componentIds[index] == componentId || contourMask[index]) {
                continue;
            }
            values ~= depthPixels[index];
        }
    }
    return values;
}

private ubyte[] depthDrawCollectComponentForeignVisibleDepthSupport(
    const(size_t)[] indices,
    DepthDrawPruneLayer layer,
    int imageWidth,
    int imageHeight,
    const(ubyte)[] stableDepthPixels,
    const(int)[] visibleLayerMap,
    int layerIndex
) {
    ubyte[] values;
    foreach (index; indices) {
        auto x = cast(int)(index % layer.width);
        auto y = cast(int)(index / layer.width);
        auto globalX = layer.left + x;
        auto globalY = layer.top + y;
        if (globalX < 0 || globalX >= imageWidth || globalY < 0 || globalY >= imageHeight) continue;

        foreach (dy; -2 .. 3) {
            auto sy = globalY + dy;
            if (sy < 0 || sy >= imageHeight) continue;
            foreach (dx; -2 .. 3) {
                auto sx = globalX + dx;
                if (sx < 0 || sx >= imageWidth) continue;
                auto globalIndex = sy * imageWidth + sx;
                auto visibleLayer = visibleLayerMap[globalIndex];
                if (visibleLayer < 0 || visibleLayer == layerIndex) continue;
                values ~= stableDepthPixels[globalIndex];
            }
        }
    }
    return values;
}

private ubyte[] depthDrawCollectLocalDepthSupport(
    const(ubyte)[] depthPixels,
    const(ubyte)[] maskPixels,
    int width,
    int height,
    int centerX,
    int centerY,
    int radius
) {
    ubyte[] values;
    foreach (dy; -radius .. radius + 1) {
        auto sy = centerY + dy;
        if (sy < 0 || sy >= height) continue;
        foreach (dx; -radius .. radius + 1) {
            auto sx = centerX + dx;
            if (sx < 0 || sx >= width || (dx == 0 && dy == 0)) continue;
            auto sampleIndex = sy * width + sx;
            auto sampleDepth = depthPixels[sampleIndex];
            if (!maskPixels[sampleIndex] || sampleDepth == 0) continue;
            values ~= sampleDepth;
        }
    }
    return values;
}

private ubyte[] depthDrawCollectForeignVisibleDepthSupport(
    int imageWidth,
    int imageHeight,
    int centerX,
    int centerY,
    int radius,
    int layerIndex,
    const(ubyte)[] stableDepthPixels,
    const(int)[] visibleLayerMap
) {
    ubyte[] values;
    foreach (dy; -radius .. radius + 1) {
        auto sy = centerY + dy;
        if (sy < 0 || sy >= imageHeight) continue;
        foreach (dx; -radius .. radius + 1) {
            auto sx = centerX + dx;
            if (sx < 0 || sx >= imageWidth || (dx == 0 && dy == 0)) continue;
            auto globalIndex = sy * imageWidth + sx;
            auto visibleLayer = visibleLayerMap[globalIndex];
            if (visibleLayer < 0 || visibleLayer == layerIndex) continue;
            values ~= stableDepthPixels[globalIndex];
        }
    }
    return values;
}

private ubyte[] depthDrawCollectPositiveValues(const(ubyte)[] values) {
    ubyte[] positive;
    foreach (value; values) {
        if (value > 0) positive ~= value;
    }
    return positive;
}

private const(ubyte)[] depthDrawSplitLayerMask(const(DepthDrawSplitLayer) layer) {
    return layer.maskPixels.length ? layer.maskPixels : layer.alphaMask;
}

private double[] depthDrawGaussianKernel(int radius) {
    auto size = radius * 2 + 1;
    double[] kernel;
    kernel.length = size * size;
    auto sigma = max(1.0, cast(double)radius * 0.5);
    size_t index = 0;
    foreach (y; -radius .. radius + 1) {
        foreach (x; -radius .. radius + 1) {
            auto distance2 = x * x + y * y;
            kernel[index++] = exp(-cast(double)distance2 / (2.0 * sigma * sigma));
        }
    }
    return kernel;
}

private int depthDrawSampleGridPosition(int index, int count, int extent) {
    if (count <= 1 || extent <= 1) return 0;
    return cast(int)lround((cast(double)index / cast(double)(count - 1)) * cast(double)(extent - 1));
}

private DepthDrawConvolutionSample depthDrawConvolveDepthAt(
    const(ubyte)[] sourcePixels,
    int width,
    int height,
    int centerX,
    int centerY,
    int radius,
    const(double)[] kernel
) {
    double weightedSum = 0.0;
    double weightTotal = 0.0;
    foreach (oy; -radius .. radius + 1) {
        auto y = centerY + oy;
        if (y < 0 || y >= height) continue;
        foreach (ox; -radius .. radius + 1) {
            auto x = centerX + ox;
            if (x < 0 || x >= width) continue;
            auto value = sourcePixels[y * width + x];
            if (value <= 0) continue;
            auto kernelIndex = (oy + radius) * (radius * 2 + 1) + (ox + radius);
            auto weight = kernel[kernelIndex];
            weightedSum += cast(double)value * weight;
            weightTotal += weight;
        }
    }
    if (weightTotal == 0.0) return DepthDrawConvolutionSample(0.0, false);
    return DepthDrawConvolutionSample(weightedSum / weightTotal, true);
}

private double depthDrawSampleLinearGrid(
    const(double)[] values,
    const(ubyte)[] valid,
    int gridWidth,
    int gridHeight,
    double fx,
    double fy
) {
    auto x0 = cast(int)floor(fx);
    auto y0 = cast(int)floor(fy);
    auto x1 = min(gridWidth - 1, x0 + 1);
    auto y1 = min(gridHeight - 1, y0 + 1);
    auto tx = fx - x0;
    auto ty = fy - y0;

    double weightedSum = 0.0;
    double weightTotal = 0.0;
    void add(int x, int y, double weight) {
        if (weight == 0.0) return;
        auto index = y * gridWidth + x;
        if (!valid[index]) return;
        weightedSum += values[index] * weight;
        weightTotal += weight;
    }
    add(x0, y0, (1.0 - tx) * (1.0 - ty));
    add(x1, y0, tx * (1.0 - ty));
    add(x0, y1, (1.0 - tx) * ty);
    add(x1, y1, tx * ty);
    return weightTotal == 0.0 ? 0.0 : weightedSum / weightTotal;
}

private double depthDrawSampleCubicGrid(
    const(double)[] values,
    const(ubyte)[] valid,
    int gridWidth,
    int gridHeight,
    double fx,
    double fy
) {
    auto baseX = cast(int)floor(fx);
    auto baseY = cast(int)floor(fy);
    auto tx = fx - baseX;
    auto ty = fy - baseY;

    double weightedSum = 0.0;
    double weightTotal = 0.0;
    foreach (oy; -1 .. 3) {
        auto sy = depthDrawClampInt(baseY + oy, 0, gridHeight - 1);
        auto wy = depthDrawCatmullRomWeight(cast(double)oy - ty);
        foreach (ox; -1 .. 3) {
            auto sx = depthDrawClampInt(baseX + ox, 0, gridWidth - 1);
            auto wx = depthDrawCatmullRomWeight(cast(double)ox - tx);
            auto weight = wx * wy;
            if (weight == 0.0) continue;
            auto index = sy * gridWidth + sx;
            if (!valid[index]) continue;
            weightedSum += values[index] * weight;
            weightTotal += weight;
        }
    }

    return weightTotal == 0.0 ? 0.0 : weightedSum / weightTotal;
}

private double depthDrawCatmullRomWeight(double x) {
    auto ax = abs(x);
    if (ax <= 1.0) {
        return 1.5 * ax * ax * ax - 2.5 * ax * ax + 1.0;
    }
    if (ax < 2.0) {
        return -0.5 * ax * ax * ax + 2.5 * ax * ax - 4.0 * ax + 2.0;
    }
    return 0.0;
}

private int depthDrawClampInt(int value, int minValue, int maxValue) {
    return max(minValue, min(maxValue, value));
}
