#include "contours.hpp"
#include "common.hpp"
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/scope_exit.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/core/cv/contours.hpp"
#include <math/veca.hpp>
#include <nodes/grid_deformer.hpp>
#include <cmath>
#include <stdexcept>
#include <new>
#include <source_location>
#include <utility>

namespace nijigenerate::viewport::vertex::automesh::contours {
namespace {
using Vec2 = nicxlive::core::math::Vec2;
using Vec2Array = nicxlive::core::math::Vec2Array;
using Mesh = AutoMeshProcessor::Mesh;
namespace cv = ::nijigenerate::core::cv::contours;

#if defined(_WIN32) && defined(_MSC_VER)
using SourceReal = double;
#else
using SourceReal = long double;
#endif

Vec2 subtract(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
// Actual inmath lengthSquared: float component powers accumulated into real.
SourceReal lengthSquared(Vec2 value) {
    SourceReal result = 0;
    const float xSquare = value.x * value.x;
    result += static_cast<SourceReal>(xSquare);
    const float ySquare = value.y * value.y;
    result += static_cast<SourceReal>(ySquare);
    return result;
}
float sourceSign(float value) { return value > 0 ? 1.0f : value == 0 ? 0.0f : -1.0f; }

// This adapter retains the actual shared image and channel, never a mask copy.
struct AlphaChannel {
    std::shared_ptr<core::cv::image::Image> owner;
    mutable compat::ContiguousView<std::uint8_t, 3> image;
    std::array<std::size_t, 2> dimensions;
    explicit AlphaChannel(std::shared_ptr<core::cv::image::Image> input)
        : owner(std::move(input)), image(owner->sliced()), dimensions{image.shape()[0], image.shape()[1]} {
#ifndef NDEBUG
        if (image.shape()[2] <= 3)
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("Slice.opIndex: index must be less than length"),
                compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 2292);
#endif
    }
    const auto& shape() const { return dimensions; }
    std::uint8_t operator()(std::ptrdiff_t y, std::ptrdiff_t x) const {
        return image.storage().data()[offset(y, x)];
    }
    std::uint8_t& operator()(std::ptrdiff_t y, std::ptrdiff_t x) {
        return image.storage().data()[offset(y, x)];
    }
private:
    std::size_t offset(std::ptrdiff_t row, std::ptrdiff_t column) const {
        const auto y = static_cast<std::size_t>(row);
        const auto x = static_cast<std::size_t>(column);
#ifndef NDEBUG
        if (x >= dimensions[1])
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("2D slice of ubyte: bounds check failed at 2 dimension"),
                compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 816);
        if (y >= dimensions[0])
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("2D slice of ubyte: bounds check failed at 1 dimension"),
                compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 820);
#endif
        return (y * image.shape()[1] + x) * image.shape()[2] + 3;
    }
};

template<class ContourType> Vec2Array contoursToVec2s(ContourType& contour) {
    Vec2Array result;
    for (const auto point : contour) result.push_back({static_cast<float>(point.x), static_cast<float>(point.y)});
    return result;
}
Vec2 calcMoment(Vec2Array contour) {
    const auto points = contour.toArray();
    if (points.empty()) {
        compat::RetainedString message;
        try {
            message = compat::RetainedString(
                "Cannot reduce an empty input range w/o an explicit seed value.");
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        throw compat::ThrowableException(std::move(message),
            compat::RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 4378);
    }
    auto moment = points.front();
    for (std::size_t i = 1; i < points.size(); ++i) {
        moment = {moment.x + points[i].x, moment.y + points[i].y};
    }
    const float count = static_cast<float>(contour.size());
    return {moment.x / count, moment.y / count};
}
compat::Slice<Vec2> scaling(Vec2Array contour, Vec2 moment, float scale, std::int32_t) {
    const auto points = contour.toArray();
    compat::Slice<Vec2> result;
    result.resize(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto point = points[i];
        const auto difference = subtract(point, moment);
        const Vec2 scaled{difference.x * scale, difference.y * scale};
        result[i] = Vec2{scaled.x + moment.x, scaled.y + moment.y};
    }
    return result;
}
Vec2Array resampling(Vec2Array contour, double rate, bool mirrorHoriz, float axisHoriz,
    bool, float) {
    Vec2Array sampled;
    std::uint64_t base = 0;
    if (mirrorHoriz) {
        float minDistance = -1;
        for (std::size_t i = 0; i < contour.size(); ++i) {
            const Vec2 vertex = contour.at(i);
            if (minDistance < 0 || vertex.x - axisHoriz < minDistance) {
                base = i;
                minDistance = vertex.x - axisHoriz;
            }
        }
    }
    sampled.push_back(contour.at(base));
    float side = 0;
    for (std::size_t index = 1; index < contour.size(); ++index) {
        const Vec2 previous = sampled.at(sampled.size() - 1);
        const Vec2 current = contour.at((index + base) % contour.size());
        if (lengthSquared(subtract(current, previous)) > rate * rate) {
            if (mirrorHoriz) {
                if (side == 0) side = sourceSign(current.x - axisHoriz);
                else if (sourceSign(current.x - axisHoriz) != side) continue;
            }
            sampled.push_back(current);
        }
    }
    return sampled;
}

void generateContours(ContourAutoMeshProcessor& processor, common::AlphaInput input, Mesh mesh,
    bool mirrorHoriz, float axisHoriz, bool mirrorVert, float axisVert) {
    AlphaChannel binary(input.img);
    for (std::size_t y = 0; y < binary.shape()[0]; ++y) {
        for (std::size_t x = 0; x < binary.shape()[1]; ++x) {
            const auto threshold = static_cast<std::uint8_t>(compat::ngFloatToInt32(processor.maskThreshold));
            binary(y, x) = binary(y, x) < threshold ? 0 : 255;
        }
    }
    compat::ngRequireObject(mesh)->clear();
    const Vec2 center{static_cast<float>(input.w / 2), static_cast<float>(input.h / 2)};
    cv::Contours foundContours;
    compat::Slice<cv::ContourHierarchy> hierarchy;
    cv::findContours(binary, foundContours, hierarchy, cv::RetrievalMode::EXTERNAL, cv::ApproximationMethod::SIMPLE);
    for (const auto contour : foundContours) {
        const auto points = contoursToVec2s(contour);
        if (points.empty()) continue;
        const auto scales = processor.SCALES;
        const auto moment = calcMoment(points);
        if (processor.MAX_DISTANCE < 0) processor.MAX_DISTANCE = processor.SAMPLING_STEP * 2;
        for (const double scale : scales) {
            double samplingRate = processor.SAMPLING_STEP;
            const double first = processor.MAX_DISTANCE / scale;
            const double second = scale > 0 ? samplingRate / (scale * scale) : 1;
            samplingRate = second < first ? second : first;
            const auto sampled = resampling(points, samplingRate, mirrorHoriz, center.x + axisHoriz,
                mirrorVert, center.y + axisVert);
            const auto scaled = scaling(sampled, moment, static_cast<float>(scale), 0);
            for (const Vec2 point : scaled) {
                if (!mesh->vertices.empty()) {
                    const auto vertices = mesh->vertices;
                    const auto distanceTo = [&](std::size_t index) {
                        const auto position = subtract(point, center);
                        const auto vertexPosition = compat::ngRequireObject(vertices[index])->position;
                        return std::sqrt(lengthSquared(subtract(position, vertexPosition)));
                    };
                    SourceReal minDistance = distanceTo(0);
                    for (std::size_t i = 1; i < vertices.size(); ++i) {
                        const SourceReal distance = distanceTo(i);
                        minDistance = minDistance < distance ? minDistance : distance;
                    }
                    if (minDistance > processor.MIN_DISTANCE) {
                        mesh->vertices.append(core::math::mesh::ngCreateMeshVertexSource([&] {
                            return subtract(point, center);
                        }));
                    }
                } else {
                    mesh->vertices.append(core::math::mesh::ngCreateMeshVertexSource([&] {
                        return subtract(point, center);
                    }));
                }
            }
        }
    }
}
}

ContourAutoMeshProcessor::Mesh ContourAutoMeshProcessor::autoMesh(Target target, Mesh mesh,
    bool mirrorHoriz, float axisHoriz, bool mirrorVert, float axisVert) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    if (std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(target)) return mesh;
    if (MAX_DISTANCE < 0) MAX_DISTANCE = SAMPLING_STEP * 2;
    auto input = common::getAlphaInput(target);
    if (input.w == 0 || input.h == 0 || !input.img) return mesh;
    generateContours(*this, input, mesh, mirrorHoriz, axisHoriz, mirrorVert, axisVert);
    auto result = std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(target) ? mesh->autoTriangulate() : mesh;
    common::mapImageCenteredMeshToTargetLocal(result, target, input);
    return result;
}

ContourAutoMeshProcessor::Mesh ContourAutoMeshProcessor::autoMesh(NodeSlice targets, Mesh mesh,
    bool mirrorHoriz, float axisHoriz, bool mirrorVert, float axisVert) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    auto provider = GenericProjectionAlphaProvider::createSource(targets);
    Mesh result;
    compat::ngRunWithScopeExit([&] {
        auto input = common::alphaInputFromProviderWithImage(provider);
        if (input.w == 0 || input.h == 0 || !input.img) result = mesh;
        else {
            generateContours(*this, input, mesh, mirrorHoriz, axisHoriz, mirrorVert, axisVert);
            result = mesh->autoTriangulate();
        }
    }, [&] { provider->dispose(); });
    return result;
}

} // namespace nijigenerate::viewport::vertex::automesh::contours
