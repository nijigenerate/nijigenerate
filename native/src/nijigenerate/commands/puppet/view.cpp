#include "view_simple_commands.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/core/selector/parse_uint.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/api/mcp/task/task.hpp>
#include <nijigenerate/io/imagefmt/writer.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/phobos_float_sort.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <runtime_state.hpp>
#include <texture.hpp>
#include <compat/object_metadata.hpp>
#include <nodes/drawable.hpp>
#include <nodes/grid_deformer.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::commands::puppet::view {
namespace {
using Json = core::settings::SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using String = core::settings::SettingsString;
using namespace nicxlive::core::math;
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Drawable;
using nicxlive::core::nodes::Deformable;
using nicxlive::core::nodes::GridDeformer;
using nicxlive::core::Puppet;
using nicxlive::core::param::Parameter;
namespace runtime = nicxlive::core;
using namespace core::dbg;
struct ScreenshotOverlayObject { std::uint32_t uuid{}; String kind; };
using Overlays = compat::Slice<ScreenshotOverlayObject>;

bool readUuidString(std::string_view value, std::uint32_t& uuid) {
    try {
        auto tail = value;
        auto parsed = core::selector::ngParseSelectorUInt(tail);
        if (!tail.empty()) return false;
        uuid = parsed; return true;
    } catch (const compat::ThrowableException&) { return false; }
}
bool readOverlayUuid(Json value, std::uint32_t& uuid) {
    uuid = 0;
    if (value.is_int64() && value.as_int64() >= 0) {
        uuid = static_cast<std::uint32_t>(value.as_int64()); return true;
    }
    return value.is_string() && readUuidString(value.as_string().view(), uuid);
}
bool parseScreenshotOverlayObjects(Json value, Overlays& overlays, String& message) {
    overlays = {}; message = {};
    if (value.is_null()) return true;
    if (!value.is_array()) { message = compat::RetainedString::fromStaticLiteral("overlayObjects must be an array"); return false; }
    for (auto entry : value.as_array()) {
        if (!entry.is_object()) { message = compat::RetainedString::fromStaticLiteral("overlayObjects entries must be objects"); return false; }
        auto obj = entry.sourceObjectRef(); ScreenshotOverlayObject overlay;
        if (auto uuid = obj.if_contains("uuid")) {
            if (!readOverlayUuid(*uuid, overlay.uuid)) {
                message = compat::RetainedString::fromStaticLiteral("overlayObjects[].uuid must be a numeric UUID"); return false;
            }
            auto kind = obj.if_contains("overlay");
            if (!kind || !kind->is_string()) kind = obj.if_contains("type");
            if (!kind || !kind->is_string()) kind = obj.if_contains("kind");
            if (!kind || !kind->is_string()) {
                message = compat::RetainedString::fromStaticLiteral("overlayObjects[] requires overlay/type/kind as bounds or mesh"); return false;
            }
            overlay.kind = kind->as_string();
        } else {
            bool found = false;
            for (auto& member : obj) {
                if (!member.value().is_string()) continue;
                if (readUuidString(member.key(), overlay.uuid)) {
                    overlay.kind = member.value().as_string(); found = true; break;
                }
            }
            if (!found) {
                message = compat::RetainedString::fromStaticLiteral("overlayObjects[] must be {uuid, overlay} or {\"<uuid>\": \"bounds|mesh\"}"); return false;
            }
        }
        if (overlay.kind.view() != "bounds" && overlay.kind.view() != "mesh") {
            message = compat::RetainedString::fromStaticLiteral("overlayObjects[] overlay must be bounds or mesh"); return false;
        }
        overlays.append(overlay);
    }
    return true;
}
Vec2 vertexAt(const Vec2Array& points, std::size_t i) {
#ifndef NDEBUG
    if (i >= points.size()) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("veca index out of range"),
        compat::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 139);
#endif
    return points.at(i);
}
void drawScreenshotBoundsOverlay(const std::shared_ptr<Node>& node) {
    auto b = compat::ngRequireObject(node)->getCombinedBounds(true);
    Vec3Array points{{b[0], b[1], 0}, {b[2], b[1], 0}, {b[2], b[3], 0}, {b[0], b[3], 0}};
    const std::uint16_t indices[]{0, 1, 1, 2, 2, 3, 3, 0};
    inDbgSetBuffer(points, indices);
    inDbgLineWidth(3); inDbgDrawLines({1, 0.85f, 0.1f, 1}); inDbgLineWidth(1);
}
bool drawScreenshotDeformableMeshOverlay(const std::shared_ptr<Deformable>& deformable) {
    if (!deformable || deformable->getVertices().empty()) return false;
    auto vertices = deformable->getVertices();
    const bool haveDeform = deformable->deformation.size() == vertices.size();
    auto position = [&](std::size_t i) {
        auto p = vertexAt(vertices, i);
        if (haveDeform) {
            auto offset = vertexAt(deformable->deformation, i);
            p.x += offset.x; p.y += offset.y;
        }
        return Vec3{p.x, p.y, 0};
    };
    if (auto grid = std::dynamic_pointer_cast<GridDeformer>(deformable)) {
        const auto baseVertices = vertices.toArray();
        std::vector<float> xs, ys;
        xs.reserve(baseVertices.size());
        for (const auto& point : baseVertices) xs.push_back(point.x);
        ys.reserve(baseVertices.size());
        for (const auto& point : baseVertices) ys.push_back(point.y);
        compat::phobos_float_sort::sort(xs); compat::phobos_float_sort::sort(ys);
        xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
        ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
        auto cols = xs.size(), rows = ys.size();
        if (cols >= 2 && rows >= 2 && cols * rows == vertices.size()) {
            Vec3Array lines;
            for (std::size_t y = 0; y < rows; ++y) for (std::size_t x = 0; x < cols; ++x) {
                const auto gridPosition = [&](std::size_t i) {
                    auto point = baseVertices[i];
                    if (haveDeform) {
                        const auto offset = vertexAt(grid->deformation, i);
                        point.x += offset.x; point.y += offset.y;
                    }
                    return Vec3{point.x, point.y, 0};
                };
                auto i = y * cols + x; auto start = gridPosition(i);
                if (x + 1 < cols) { lines.push_back(start); lines.push_back(gridPosition(i + 1)); }
                if (y + 1 < rows) { lines.push_back(start); lines.push_back(gridPosition(i + cols)); }
            }
            if (!lines.empty()) {
                inDbgSetBuffer(lines);
                inDbgDrawLines({0.1f, 0.9f, 1, 1}, grid->transform().matrix());
            }
        }
    } else if (vertices.size() >= 2) {
        Vec3Array lines;
        for (std::size_t i = 1; i < vertices.size(); ++i) {
            lines.push_back(position(i - 1)); lines.push_back(position(i));
        }
        if (!lines.empty()) {
            inDbgSetBuffer(lines);
            inDbgDrawLines({0.1f, 0.9f, 1, 1}, deformable->transform().matrix());
        }
    }
    Vec3Array points(vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) points[i].set(position(i));
    inDbgSetBuffer(points);
    inDbgPointsSize(8); inDbgDrawPoints({0, 0, 0, 1}, deformable->transform().matrix());
    inDbgPointsSize(4); inDbgDrawPoints({1, 1, 1, 1}, deformable->transform().matrix());
    return true;
}
bool drawScreenshotOverlays(std::shared_ptr<Puppet> puppet, Json objects, String& message) {
    message = {}; Overlays overlays;
    if (!parseScreenshotOverlayObjects(objects, overlays, message)) return false;
    for (auto overlay : overlays) {
        auto node = compat::ngRequireObject(puppet)->findNodeById(overlay.uuid);
        if (!node) { message = "overlayObjects target node not found: " + std::to_string(overlay.uuid); return false; }
        if (overlay.kind.view() == "bounds") drawScreenshotBoundsOverlay(node);
        else if (overlay.kind.view() == "mesh") {
            if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) {
                drawable->drawMeshLines({0.1f, 0.9f, 1, 1}); drawable->drawMeshPoints();
            } else if (!drawScreenshotDeformableMeshOverlay(std::dynamic_pointer_cast<Deformable>(node))) {
                message = "overlayObjects mesh target is not Drawable or Deformable: " + std::to_string(overlay.uuid);
                return false;
            }
        }
    }
    return true;
}
bool resolveScreenshotParameter(std::shared_ptr<Context> ctx, std::shared_ptr<Parameter>& param, String& message) {
    param = {}; message = {};
    if (compat::ngRequireObject(ctx)->hasArmedParameters() && !ctx->armedParameters().empty()) {
        if (ctx->armedParameters().size() != 1) {
            message = compat::RetainedString::fromStaticLiteral("context.parameterValue requires exactly one armed parameter"); return false;
        }
        param = ctx->armedParameters()[0];
    } else if (ctx->hasParameters() && !ctx->parameters().empty()) {
        if (ctx->parameters().size() != 1) {
            message = compat::RetainedString::fromStaticLiteral("context.parameterValue requires exactly one parameter"); return false;
        }
        param = ctx->parameters()[0];
    } else param = project::incArmedParameter();
    if (!param) { message = compat::RetainedString::fromStaticLiteral("context.parameterValue requires a parameter"); return false; }
    return true;
}
bool resolveScreenshotParameterValue(std::shared_ptr<Parameter> param, Vec2 value, Vec2& resolved, String& message) {
    resolved = {NAN, NAN}; message = {};
    if (!param) { message = compat::RetainedString::fromStaticLiteral("context.parameterValue requires a parameter"); return false; }
    auto x = param->getClosestAxisPointIndex(0, param->mapAxis(0, value.x));
    auto y = param->getClosestAxisPointIndex(1, param->mapAxis(1, value.y));
    if (x >= param->axisPointCount(0) || y >= param->axisPointCount(1)) {
        message = compat::RetainedString::fromStaticLiteral("context.parameterValue resolved keypoint is out of range"); return false;
    }
    resolved = param->getKeypointValue({x, y});
    if (std::abs(resolved.x - value.x) > 1e-5f || std::abs(resolved.y - value.y) > 1e-5f) {
        message = compat::RetainedString::fromStaticLiteral("context.parameterValue does not match an existing key value"); return false;
    }
    return true;
}
bool applyScreenshotParameterContext(std::shared_ptr<Context> ctx, std::shared_ptr<Parameter>& param,
        Vec2& restore, String& message) {
    param = {}; restore = {0, 0}; message = {};
    if (!compat::ngRequireObject(ctx)->hasParameterValue()) return true;
    if (!resolveScreenshotParameter(ctx, param, message)) return false;
    Vec2 capture;
    if (!resolveScreenshotParameterValue(param, ctx->parameterValue(), capture, message)) return false;
    restore = param->value; param->value = capture; return true;
}
bool incCaptureLiveViewport(int& width, int& height, runtime::SourceTextureBytes& data,
        String& message, Json objects = {}) {
    width = 0; height = 0; data = {}; message = {};
    auto puppet = project::incActivePuppet();
    if (!puppet) { message = compat::RetainedString::fromStaticLiteral("No active puppet"); return false; }
    runtime::inGetViewport(width, height);
    if (width <= 0 || height <= 0) { message = compat::RetainedString::fromStaticLiteral("Viewport is empty"); return false; }
    runtime::inSetClearColor(0, 0, 0, 0);
    bool captured = false;
    compat::ngRunWithScopeExit([&] {
        runtime::inBeginScene(); puppet->update(); puppet->draw();
        const bool ok = drawScreenshotOverlays(puppet, objects, message);
        runtime::inEndScene();
        if (!ok) return;
        const auto dataLength = runtime::inViewportDataLength();
        try { data = runtime::SourceTextureBytes(dataLength, 0); }
        catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        runtime::inDumpViewport(data);
        runtime::inTexUnPremuliply(data);
        captured = true;
    }, [] { core::ngResetClearColor(); });
    return captured;
}
Json jsonVec2(Vec2 v) { return Json(Array{Json(double(v.x)), Json(double(v.y))}); }
Json jsonMat4(Mat4 value) {
    Array result;
    for (int row = 0; row < 4; ++row) {
        Array r;
        for (int col = 0; col < 4; ++col) r.append(Json(double(value[row][col])));
        result.append(Json(r));
    }
    return Json(result);
}
Vec2 clipToWorldPoint(Mat4 matrix, Vec2 point) {
    std::array<float, 4> input{point.x, point.y, 0, 1}, result{};
    for (int col = 0; col < 4; ++col) for (int row = 0; row < 4; ++row)
        result[row] = nicxlive::core::math::detail::inmathAccumulate(result[row],
            nicxlive::core::math::detail::inmathProduct(matrix[row][col], input[col]));
    return result[3] != 0 ? Vec2{result[0] / result[3], result[1] / result[3]} : Vec2{result[0], result[1]};
}
Json jsonPointMapping(std::size_t index, Vec2 source, Vec2 overlay, Mat4 matrix) {
    Object result;
    result["index"] = Json(std::bit_cast<std::int64_t>(std::uint64_t(index)));
    result["source"] = jsonVec2(source); result["overlay"] = jsonVec2(overlay);
    result["image"] = jsonVec2(clipToWorldPoint(matrix, overlay)); return Json(result);
}
Mat4 clipToImageMatrix(int width, int height) {
    return Mat4::translation(float(width) * 0.5f, float(height) * 0.5f, 0) *
        Mat4::scale({float(width) * 0.5f, -float(height) * 0.5f, 1});
}
Json buildScreenshotWorldCaptureMeta(int width, int height) {
    auto wc = runtime::inGetCamera().matrix(); auto cw = core::math::source_matrix::inverse(wc);
    auto tl = clipToWorldPoint(cw, {-1, 1}), tr = clipToWorldPoint(cw, {1, 1});
    auto br = clipToWorldPoint(cw, {1, -1}), bl = clipToWorldPoint(cw, {-1, -1});
    auto min = [](float a, float b) { return b < a ? b : a; };
    auto max = [](float a, float b) { return a < b ? b : a; };
    Vec2 lo{min(min(tl.x, tr.x), min(br.x, bl.x)), min(min(tl.y, tr.y), min(br.y, bl.y))};
    Vec2 hi{max(max(tl.x, tr.x), max(br.x, bl.x)), max(max(tl.y, tr.y), max(br.y, bl.y))};
    Object corners, aabb, capture;
    corners["topLeft"] = jsonVec2(tl); corners["topRight"] = jsonVec2(tr);
    corners["bottomRight"] = jsonVec2(br); corners["bottomLeft"] = jsonVec2(bl);
    aabb["min"] = jsonVec2(lo); aabb["max"] = jsonVec2(hi);
    capture["coordinateSystem"] = Json(String(compat::RetainedString::fromStaticLiteral("nijigenerate-world"))); capture["pixelOrigin"] = Json(String(compat::RetainedString::fromStaticLiteral("top-left")));
    capture["width"] = Json(std::int64_t(width)); capture["height"] = Json(std::int64_t(height));
    capture["corners"] = Json(corners); capture["aabb"] = Json(aabb);
    capture["worldToClipMatrix"] = jsonMat4(wc); capture["clipToWorldMatrix"] = jsonMat4(cw); return Json(capture);
}
Json buildScreenshotOverlayMappings(std::shared_ptr<Puppet> puppet, Json objects,
        int width, int height, String& message) {
    message = {}; Overlays overlays;
    if (!parseScreenshotOverlayObjects(objects, overlays, message)) return {};
    auto wi = clipToImageMatrix(width, height) * runtime::inGetCamera().matrix(); Array result;
    for (auto overlay : overlays) {
        auto node = compat::ngRequireObject(puppet)->findNodeById(overlay.uuid);
        if (!node) { message = "overlayObjects target node not found: " + std::to_string(overlay.uuid); return {}; }
        Mat4 target; Json coordinateSystem, overlayPoint; Array points;
        auto drawable = std::dynamic_pointer_cast<Drawable>(node);
        auto deformable = std::dynamic_pointer_cast<Deformable>(node);
        if (overlay.kind.view() == "bounds") {
            target = Mat4::identity(); coordinateSystem = Json(String(compat::RetainedString::fromStaticLiteral("nijigenerate-world")));
            overlayPoint = Json(String(compat::RetainedString::fromStaticLiteral("overlay point is a world-space bounds point: [worldX, worldY, 0, 1]")));
        } else if (drawable) {
            target = drawable->meshOverlayMatrix();
            coordinateSystem = Json(String(compat::RetainedString::fromStaticLiteral("drawable-overlay-point")));
            overlayPoint = Json(String(compat::RetainedString::fromStaticLiteral("overlay point is the exact point sent to debug draw: [vertexX - data.origin.x + deformationX, vertexY - data.origin.y + deformationY, 0, 1]")));
        } else if (deformable) {
            target = deformable->transform().matrix(); coordinateSystem = Json(String(compat::RetainedString::fromStaticLiteral("deformable-deformed-vertex")));
            overlayPoint = Json(String(compat::RetainedString::fromStaticLiteral("overlay point is [vertexX + deformationX, vertexY + deformationY, 0, 1]")));
        } else {
            message = "overlayObjects mesh target is not Drawable or Deformable: " + std::to_string(overlay.uuid); return {};
        }
        auto matrix = wi * target;
        if (overlay.kind.view() == "bounds") {
            auto b = node->getCombinedBounds(true);
            const Vec2 corners[]{{b[0], b[1]}, {b[2], b[1]}, {b[2], b[3]}, {b[0], b[3]}};
            for (std::size_t i = 0; i < 4; ++i) points.append(jsonPointMapping(i, corners[i], corners[i], matrix));
        } else if (drawable) {
            auto vertices = drawable->meshOverlayPoints();
            for (std::size_t i = 0; i < vertices.size(); ++i)
                points.append(jsonPointMapping(i, vertexAt(drawable->getVertices(), i), vertexAt(vertices, i), matrix));
        } else if (deformable) {
            const auto vertices = deformable->getVertices();
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                auto source = vertexAt(vertices, i), point = source;
                if (deformable->deformation.size() == deformable->getVertices().size()) {
                    auto offset = vertexAt(deformable->deformation, i);
                    point.x += offset.x; point.y += offset.y;
                }
                points.append(jsonPointMapping(i, source, point, matrix));
            }
        }
        Object entry;
        entry["uuid"] = Json(std::int64_t(overlay.uuid)); entry["overlay"] = Json(overlay.kind);
        entry["coordinateSystem"] = coordinateSystem; entry["overlayPoint"] = overlayPoint;
        entry["points"] = Json(points); result.append(Json(entry));
    }
    return Json(result);
}
std::string base64(std::span<const std::uint8_t> bytes) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; std::string result;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        auto remaining = bytes.size() - i; auto value = std::uint32_t(bytes[i]) << 16;
        if (remaining > 1) value |= std::uint32_t(bytes[i + 1]) << 8;
        if (remaining > 2) value |= bytes[i + 2];
        result += alphabet[value >> 18]; result += alphabet[(value >> 12) & 63];
        result += remaining > 1 ? alphabet[(value >> 6) & 63] : '=';
        result += remaining > 2 ? alphabet[value & 63] : '=';
    }
    return result;
}
std::string pngFilename(std::string_view name) {
#ifdef _WIN32
    const auto separator = name.find_last_of("/\\:");
#else
    const auto separator = name.find_last_of('/');
#endif
    const auto base = separator == std::string_view::npos ? 0 : separator + 1;
    auto dot = name.find_last_of('.');
    if (dot == std::string_view::npos || dot <= base) dot = name.size();
    return std::string(name.substr(0, dot)) + ".png";
}
} // namespace
SaveScreenshotCommand::SaveScreenshotCommand(compat::RetainedStringSlice filename)
    : SaveScreenshotCommand(SourceConstructorFrame::capture(filename)) {}
SaveScreenshotCommand::SourceConstructorFrame SaveScreenshotCommand::SourceConstructorFrame::capture(
        compat::RetainedStringSlice& filename) {
    return {core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Save Screenshot")),
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Save screenshot.")), filename};
}
SaveScreenshotCommand::SaveScreenshotCommand(SourceConstructorFrame frame)
    : ExCommand<SaveScreenshotArguments>(std::move(frame.label), std::move(frame.description), frame.filename) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(SaveScreenshotCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.view.SaveScreenshotCommand"));
}
std::shared_ptr<CommandResult> SaveScreenshotCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    if (filename.isNull()) return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("Filename not set"));
    auto file = pngFilename(filename.view()); int width, height; runtime::SourceTextureBytes data; String message;
    if (!incCaptureLiveViewport(width, height, data, message)) return CommandResult::opCall(false, message.retainedString());
    auto error = io::imagefmt::writeImageFile(file, width, height, {data.data(), data.size()}, 4);
    if (error != io::imagefmt::Error::none)
        return CommandResult::opCall(false, std::string("PNG encode failed: ") + io::imagefmt::errorText[int(error)]);
    return CommandResult::opCall(true);
}
CaptureLiveScreenshotCommand::CaptureLiveScreenshotCommand()
    : CaptureLiveScreenshotCommand(SourceConstructorFrame::capture()) {}
CaptureLiveScreenshotCommand::SourceConstructorFrame CaptureLiveScreenshotCommand::SourceConstructorFrame::capture() {
    return {core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Capture Live Screenshot")),
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Capture current live viewport and return an MCP image content item with mimeType image/png."))};
}
CaptureLiveScreenshotCommand::CaptureLiveScreenshotCommand(SourceConstructorFrame frame)
    : NativeBase(std::move(frame.label), std::move(frame.description)) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(CaptureLiveScreenshotCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.view.CaptureLiveScreenshotCommand"));
    sourceConstructorBody();
}
void CaptureLiveScreenshotCommand::sourceConstructorBody() {
    overlayObjects = Json(Array{});
}
std::shared_ptr<CaptureLiveScreenshotCommand::Result> CaptureLiveScreenshotCommand::run(
        const std::shared_ptr<Context>& context) { return runTyped(context); }
std::shared_ptr<CaptureLiveScreenshotCommand::Result> CaptureLiveScreenshotCommand::runTyped(std::shared_ptr<Context> ctx) {
    const auto receiver = weak_from_this().lock();
    int width, height; runtime::SourceTextureBytes data; String message;
    std::shared_ptr<Parameter> captureParam; Vec2 restoreValue;
    bool restore = compat::ngRequireObject(ctx)->hasParameterValue();
    auto failure = [](String text) { return ExCommandResult<Json>::opCall(false, Json{}, text.retainedString()); };
    if (restore && api::mcp::task::ngMcpExternalCommandsBlocked())
        return failure(String(compat::RetainedString::fromStaticLiteral("AutoRig is running; parameterized screenshots are blocked")));
    auto capturePuppet = project::incActivePuppet();
    if (!applyScreenshotParameterContext(ctx, captureParam, restoreValue, message)) return failure(message);
    std::shared_ptr<Result> resultValue;
    compat::ngRunWithScopeExit([&] {
        if (!incCaptureLiveViewport(width, height, data, message, overlayObjects)) {
            resultValue = failure(message);
            return;
        }
        auto png = io::imagefmt::writeImageMemory(io::imagefmt::png, width, height,
            {data.data(), data.size()}, 4);
        if (png.error != io::imagefmt::Error::none) {
            resultValue = failure(std::string("PNG encode failed: ") + io::imagefmt::errorText[int(png.error)]);
            return;
        }
        compat::ngRunWithScopeExit([&] {
            resultValue = [&]() -> std::shared_ptr<Result> {
                Object image, meta, result;
                image["type"] = Json(String(compat::RetainedString::fromStaticLiteral("image"))); image["mimeType"] = Json(String(compat::RetainedString::fromStaticLiteral("image/png")));
                image["data"] = Json(base64({png.buffer.data(), png.length}));
                meta["width"] = Json(std::int64_t(width)); meta["height"] = Json(std::int64_t(height));
                meta["worldCapture"] = buildScreenshotWorldCaptureMeta(width, height);
                if (overlayObjects.is_array() && !overlayObjects.as_array().empty()) {
                    auto puppet = project::incActivePuppet();
                    if (!puppet) return failure(String(compat::RetainedString::fromStaticLiteral("No active puppet")));
                    message = {};
                    auto mappings = buildScreenshotOverlayMappings(puppet, overlayObjects, width, height, message);
                    if (!message.empty()) return failure(message);
                    meta["overlayMappings"] = mappings;
                }
                result["mcpDirectToolResult"] = Json(true); result["content"] = Json(Array{Json(image)});
                result["_meta"] = Json(meta);
                return ExCommandResult<Json>::opCall(true, Json(result));
            }();
        }, [&] {
            // The source calls core.stdc.stdlib.free, not the imagefmt allocator hook.
            std::free(png.buffer.detach());
        });
    }, [&] {
        if (restore && captureParam) {
            captureParam->value = restoreValue;
            if (capturePuppet) capturePuppet->update();
        }
    });
    return resultValue;
}
} // namespace nijigenerate::commands::puppet::view
