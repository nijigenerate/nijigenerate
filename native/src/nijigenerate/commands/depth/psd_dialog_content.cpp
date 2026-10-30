#include "psd_dialog_content.hpp"
#include "psd_dialog_commands.hpp"
#include <nijigenerate/io/imagefmt/writer.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <bit>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <new>
#include <source_location>
#include <stdexcept>
namespace nijigenerate::commands::depth::psd_dialog::detail {
namespace {
compat::RetainedString base64(std::span<const std::uint8_t> bytes) {
    try {
        constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        for (std::size_t i = 0; i < bytes.size();) {
            const auto remaining = bytes.size() - i;
            auto value = std::uint32_t(bytes[i]) << 16;
            if (remaining > 1) value |= std::uint32_t(bytes[i + 1]) << 8;
            if (remaining > 2) value |= bytes[i + 2];
            result += alphabet[value >> 18];
            result += alphabet[(value >> 12) & 63];
            result += remaining > 1 ? alphabet[(value >> 6) & 63] : '=';
            result += remaining > 2 ? alphabet[value & 63] : '=';
            if (remaining <= 3) break;
            i += 3;
        }
        return compat::RetainedString(std::move(result));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
Json finiteFloatToJson(float value) {
    return std::isfinite(value) ? Json(static_cast<double>(value)) : Json(nullptr);
}
Json floatArrayToJson(compat::ConstSlice<float> values) {
    Json result(Array{});
    for (auto value : values) result.as_array().append(finiteFloatToJson(value));
    return result;
}
Json stringArrayToJson(compat::ConstSlice<compat::RetainedString> values) {
    Json result(Array{});
    for (auto value : values) result.as_array().append(Json(core::settings::SettingsString(value)));
    return result;
}
Json boolArrayToJson(const nicxlive::core::common::SharedArray<bool> values) {
    Json result(Array{});
    for (auto value : values) result.as_array().append(Json(value));
    return result;
}
Json vertexPositionsToJson(PsdDepthDialogPartData& part) {
    Json result(Array{});
    const auto count = part.vertexX.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (i >= part.vertexY.size()) break;
        result.as_array().append(Json(Array{finiteFloatToJson(part.vertexX[i]), finiteFloatToJson(part.vertexY[i])}));
    }
    return result;
}
Json colorStatisticsToJson(PsdDepthDialogPartLayerData& layer) {
    std::uint64_t sums[4]{};
    std::size_t pixels = 0;
    const auto count = layer.colorRgba.size() / 4;
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = i * 4;
        if (layer.colorRgba[offset + 3] == 0) continue;
        for (std::size_t channel = 0; channel < 4; ++channel) sums[channel] += layer.colorRgba[offset + channel];
        ++pixels;
    }
    Json average(Array{});
    for (auto sum : sums)
        average.as_array().append(Json(std::bit_cast<std::int64_t>(pixels ? (sum + pixels / 2) / pixels : std::uint64_t(0))));
    Object result;
    result[literalString("nonTransparentPixels")] = Json(std::bit_cast<std::int64_t>(std::uint64_t(pixels)));
    result[literalString("averageRgba8")] = average;
    return Json(result);
}
bool appendPngContent(Array& content, compat::ConstSlice<std::uint8_t> rgba, int width, int height,
        std::int64_t& contentIndex, compat::RetainedString& error) {
    contentIndex = -1;
    error = {};
    if (width <= 0 || height <= 0 || rgba.size() != static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height) * 4) return true;
    auto pngData = io::imagefmt::writeImageMemory(io::imagefmt::png, width, height, rgba.view(), 4);
    if (pngData.error != io::imagefmt::Error::none) {
        try {
            error = std::string("PNG encode failed: ") + io::imagefmt::errorText[static_cast<int>(pngData.error)];
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return false;
    }
    compat::ngRunWithScopeExit([&] {
        Object image;
        image[literalString("type")] = Json(literalString("image"));
        image[literalString("mimeType")] = Json(literalString("image/png"));
        image[literalString("data")] = Json(core::settings::SettingsString(
            base64({pngData.buffer.data(), pngData.length})));
        contentIndex = std::bit_cast<std::int64_t>(std::uint64_t(content.size()));
        content.append(Json(image));
    }, [&] { std::free(pngData.buffer.detach()); });
    return true;
}
bool appendFloat32ResourceContent(Array& content, compat::ConstSlice<float> values,
        compat::RetainedString uri, std::int64_t& contentIndex, compat::RetainedString& error) {
    contentIndex = -1;
    error = {};
    if (values.empty()) return true;
    if (values.size() > (std::numeric_limits<std::size_t>::max)() / sizeof(float)) {
        error = compat::RetainedString::fromStaticLiteral("PSD depth dialog float resource size overflow");
        return false;
    }
    compat::Slice<std::uint8_t> bytes;
    bytes.resize(values.size() * sizeof(float));
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto bits = std::bit_cast<std::uint32_t>(values[i]);
        const auto offset = i * sizeof(float);
        bytes[offset] = static_cast<std::uint8_t>(bits & 0xff);
        bytes[offset + 1] = static_cast<std::uint8_t>((bits >> 8) & 0xff);
        bytes[offset + 2] = static_cast<std::uint8_t>((bits >> 16) & 0xff);
        bytes[offset + 3] = static_cast<std::uint8_t>((bits >> 24) & 0xff);
    }
    Object resource;
    resource[literalString("uri")] = Json(core::settings::SettingsString(uri));
    resource[literalString("mimeType")] = Json(literalString("application/vnd.nijigenerate.depth-map.f32le"));
    resource[literalString("blob")] = Json(core::settings::SettingsString(base64({bytes.data(), bytes.size()})));
    Object item;
    item[literalString("type")] = Json(literalString("resource"));
    item[literalString("resource")] = Json(resource);
    contentIndex = std::bit_cast<std::int64_t>(std::uint64_t(content.size()));
    content.append(Json(item));
    return true;
}
Json imageReference(std::int64_t contentIndex, int width, int height, int left, int top) {
    Object result;
    result[literalString("available")] = Json(contentIndex >= 0);
    result[literalString("contentIndex")] = Json(contentIndex);
    result[literalString("mimeType")] = Json(contentIndex >= 0 ? literalString("image/png") : literalString(""));
    result[literalString("width")] = Json(width);
    result[literalString("height")] = Json(height);
    result[literalString("left")] = Json(left);
    result[literalString("top")] = Json(top);
    return Json(result);
}
}

namespace nijigenerate::commands::depth::psd_dialog {
using namespace detail;
using namespace windows::psddepthmap;
namespace {
std::int64_t signedSize(std::size_t value) { return std::bit_cast<std::int64_t>(std::uint64_t(value)); }
}
GetPsdDepthDialogPartDataCommand::SourceConstructorArguments
GetPsdDepthDialogPartDataCommand::sourceConstructorArguments() {
    return {
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Get PSD Depth Dialog Part Data")),
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Return composed vertex depths, preview images, and color/depth layer images for "
            "the PSD depth dialog parts selected by Context.nodes."))};
}
GetPsdDepthDialogPartDataCommand::GetPsdDepthDialogPartDataCommand()
    : GetPsdDepthDialogPartDataCommand(sourceConstructorArguments()) {}
GetPsdDepthDialogPartDataCommand::GetPsdDepthDialogPartDataCommand(SourceConstructorArguments arguments)
    : NativeBase{arguments.label, arguments.description} {}
bool GetPsdDepthDialogPartDataCommand::runnable(std::shared_ptr<Context>) {
    const auto keepAlive = weak_from_this().lock();
    auto dialog = ngActivePsdDepthMapWindow();
    return dialog != nullptr && dialog->dialogCommandsAvailable();
}
std::shared_ptr<GetPsdDepthDialogPartDataCommand::Result>
GetPsdDepthDialogPartDataCommand::run(const ContextRef& ctx) {
    return runTyped(ctx);
}
std::shared_ptr<GetPsdDepthDialogPartDataCommand::Result>
GetPsdDepthDialogPartDataCommand::runTyped(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    if (!runnable(ctx)) {
        return ExCommandResult<Json>::opCall(
            false,
            Json(nullptr),
            compat::RetainedString::fromStaticLiteral("PSD depth import dialog is not displayed")
        );
    }

    compat::Slice<PsdDepthDialogPartData> parts;
    compat::RetainedString error;
    if (!compat::ngRequireObject(ngActivePsdDepthMapWindow())->captureDialogContextPartData(ctx, parts, error)) {
        return ExCommandResult<Json>::opCall(false, Json(nullptr), error);
    }

    Array content{Json(nullptr)};
    PsdDepthDialogOverallPreview overallPreview;
    if (!compat::ngRequireObject(ngActivePsdDepthMapWindow())->captureDialogOverallPreview(overallPreview, error)) {
        return ExCommandResult<Json>::opCall(false, Json(nullptr), error);
    }
    std::int64_t overallPreviewContentIndex;
    if (!appendPngContent(
        content,
        overallPreview.rgba,
        overallPreview.width,
        overallPreview.height,
        overallPreviewContentIndex,
        error
    )) {
        return ExCommandResult<Json>::opCall(false, Json(nullptr), error);
    }
    std::int64_t overallDepthContentIndex;
    if (!appendFloat32ResourceContent(
        content,
        overallPreview.depths,
        compat::RetainedString::fromStaticLiteral("nijigenerate://psd-depth-dialog/overall-depth.f32le"),
        overallDepthContentIndex,
        error
    )) {
        return ExCommandResult<Json>::opCall(false, Json(nullptr), error);
    }

    Json partEntries = Json(Array{});
    auto capturedParts = parts;
    for (auto& part : capturedParts) {
        Object depth;
        depth[literalString("values")] = floatArrayToJson(part.depths);
        depth[literalString("baseValues")] = floatArrayToJson(part.baseDepths);
        depth[literalString("vertexPositions")] = vertexPositionsToJson(part);
        depth[literalString("winnerLayerPaths")] = stringArrayToJson(part.winnerLayerPaths);
        depth[literalString("missingVertexMask")] = boolArrayToJson(part.missingVertexMask);
        depth[literalString("sampledVertices")] = Json(signedSize(part.sampledVertices));
        depth[literalString("missingVertices")] = Json(signedSize(part.missingVertices));
        depth[literalString("min")] = finiteFloatToJson(part.minDepth);
        depth[literalString("max")] = finiteFloatToJson(part.maxDepth);

        std::int64_t previewContentIndex;
        if (!appendPngContent(
            content,
            part.previewRgba,
            part.previewWidth,
            part.previewHeight,
            previewContentIndex,
            error
        )) {
            return ExCommandResult<Json>::opCall(false, Json(nullptr), error);
        }

        Json layerEntries = Json(Array{});
        auto capturedLayers = part.layers;
        for (auto& layer : capturedLayers) {
            std::int64_t colorContentIndex;
            if (!appendPngContent(
                content,
                layer.colorRgba,
                layer.width,
                layer.height,
                colorContentIndex,
                error
            )) {
                return ExCommandResult<Json>::opCall(false, Json(nullptr), error);
            }
            std::int64_t depthContentIndex;
            if (!appendPngContent(
                content,
                layer.depthRgba,
                layer.width,
                layer.height,
                depthContentIndex,
                error
            )) {
                return ExCommandResult<Json>::opCall(false, Json(nullptr), error);
            }

            Object stats;
            stats[literalString("hasDepth")] = Json(layer.depthStats.hasDepth);
            stats[literalString("maskedPixels")] = Json(signedSize(layer.depthStats.maskedPixels));
            stats[literalString("zeroPixels")] = Json(signedSize(layer.depthStats.zeroPixels));
            stats[literalString("minDepth01")] = finiteFloatToJson(layer.depthStats.minDepth01);
            stats[literalString("maxDepth01")] = finiteFloatToJson(layer.depthStats.maxDepth01);
            stats[literalString("rangeDepth01")] = finiteFloatToJson(layer.depthStats.rangeDepth01);
            stats[literalString("adjacentDelta01")] = finiteFloatToJson(layer.depthStats.adjacentDelta01);

            Object layerEntry;
            layerEntry[literalString("layerPath")] = Json(layer.layerPath);
            layerEntry[literalString("layerName")] = Json(layer.layerName);
            layerEntry[literalString("colorLayerPath")] = Json(layer.colorLayerPath);
            layerEntry[literalString("colorLayerName")] = Json(layer.colorLayerName);
            layerEntry[literalString("sourcePath")] = Json(layer.sourcePath);
            layerEntry[literalString("visible")] = Json(layer.visible);
            layerEntry[literalString("enabled")] = Json(layer.enabled);
            layerEntry[literalString("depthEnabled")] = Json(layer.depthEnabled);
            layerEntry[literalString("depthStats")] = Json(stats);
            layerEntry[literalString("colorStatistics")] = colorStatisticsToJson(layer);
            layerEntry[literalString("colorImage")] = imageReference(
                colorContentIndex, layer.width, layer.height, layer.left, layer.top);
            layerEntry[literalString("depthImage")] = imageReference(
                depthContentIndex, layer.width, layer.height, layer.left, layer.top);
            layerEntries.as_array().append(Json(layerEntry));
        }

        Object partEntry;
        partEntry[literalString("targetGridUuid")] = Json(part.targetGridUuid);
        partEntry[literalString("targetGridName")] = Json(part.targetGridName);
        partEntry[literalString("targetType")] = Json(part.targetType);
        partEntry[literalString("skipped")] = Json(part.skipped);
        partEntry[literalString("documentWidth")] = Json(part.documentWidth);
        partEntry[literalString("documentHeight")] = Json(part.documentHeight);
        partEntry[literalString("coverageSources")] = Json(signedSize(part.coverageSources));
        partEntry[literalString("depth")] = Json(depth);
        partEntry[literalString("previewImage")] = imageReference(
            previewContentIndex,
            part.previewWidth,
            part.previewHeight,
            part.previewLeft,
            part.previewTop
        );
        partEntry[literalString("layers")] = layerEntries;
        partEntries.as_array().append(Json(partEntry));
    }

    Object metadata;
    auto overallPreviewImage = imageReference(
        overallPreviewContentIndex,
        overallPreview.width,
        overallPreview.height
    );
    overallPreviewImage.as_object()[literalString("yaw")] = finiteFloatToJson(overallPreview.yaw);
    overallPreviewImage.as_object()[literalString("pitch")] = finiteFloatToJson(overallPreview.pitch);
    overallPreviewImage.as_object()[literalString("zoom")] = finiteFloatToJson(overallPreview.zoom);
    overallPreviewImage.as_object()[literalString("pan")] = Json(Array{
        finiteFloatToJson(overallPreview.panX),
        finiteFloatToJson(overallPreview.panY),
    });
    metadata[literalString("overallPreviewImage")] = overallPreviewImage;
    Object overallDepth;
    overallDepth[literalString("available")] = Json(overallDepthContentIndex >= 0);
    overallDepth[literalString("contentIndex")] = Json(overallDepthContentIndex);
    overallDepth[literalString("mimeType")] = Json(
        overallDepthContentIndex >= 0
            ? literalString("application/vnd.nijigenerate.depth-map.f32le")
            : literalString("")
    );
    overallDepth[literalString("width")] = Json(overallPreview.width);
    overallDepth[literalString("height")] = Json(overallPreview.height);
    overallDepth[literalString("encoding")] = Json(literalString("float32-le"));
    overallDepth[literalString("layout")] = Json(literalString("row-major-top-left"));
    overallDepth[literalString("valueSpace")] = Json(literalString("composed-depth"));
    overallDepth[literalString("missingValue")] = Json(literalString("NaN"));
    overallDepth[literalString("coveredPixels")] = Json(signedSize(overallPreview.coveredPixels));
    overallDepth[literalString("min")] = finiteFloatToJson(overallPreview.minDepth);
    overallDepth[literalString("max")] = finiteFloatToJson(overallPreview.maxDepth);
    metadata[literalString("overallDepth")] = Json(overallDepth);
    metadata[literalString("parts")] = partEntries;
    Object textContent;
    textContent[literalString("type")] = Json(literalString("text"));
    const auto metadataText = [&] {
        try {
            return compat::ngJsonStringify(Json(metadata));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    textContent[literalString("text")] = Json(core::settings::SettingsString(metadataText));
    content[0] = Json(textContent);

    std::int64_t imageCount = 0;
    std::int64_t resourceCount = 0;
    for (auto item : content) {
        if (!item.is_object() || !item.as_object().if_contains("type")) continue;
        if (item.as_object().at("type").as_string().view() == "image") imageCount++;
        else if (item.as_object().at("type").as_string().view() == "resource") resourceCount++;
    }
    Object meta;
    meta[literalString("partCount")] = Json(signedSize(parts.size()));
    meta[literalString("imageCount")] = Json(imageCount);
    meta[literalString("resourceCount")] = Json(resourceCount);

    Object result;
    result[literalString("mcpDirectToolResult")] = Json(true);
    result[literalString("content")] = Json(content);
    result[literalString("_meta")] = Json(meta);
    return ExCommandResult<Json>::opCall(true, Json(result));
}
}
