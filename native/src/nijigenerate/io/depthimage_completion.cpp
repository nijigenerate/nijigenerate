#include "depthimage.hpp"
#include "depthimage_internal_types.hpp"
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/selector/phobos_sort.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::io::depthimage {
namespace completion {
using namespace detail;
using ubyte = std::uint8_t;
using uint = std::uint32_t;
using ulong = std::uint64_t;
using size_t = std::size_t;
static_assert(sizeof(int) == sizeof(std::uint32_t));
int add32(int a, int b) { return std::bit_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }
int sub32(int a, int b) { return std::bit_cast<int>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b)); }
int mul32(int a, int b) { return std::bit_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
template<class T> int toInt(T value) {
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0)
            return std::numeric_limits<int>::min();
        return static_cast<int>(value);
    } else return std::bit_cast<int>(static_cast<std::uint32_t>(value));
}
template<class T> using Array = compat::Slice<T>;
template<class T> class Read {
    Array<T> retained;
    std::span<const T> values;
public:
    Read() = default;
    Read(std::span<const T> source) : values(source) {}
    Read(const Array<T>& source) : retained(source), values(retained.data(), retained.size()) {}
    std::size_t size() const { return values.size(); }
    const T& operator[](std::size_t index) const {
        if (index >= size()) throw compat::ArrayIndexError(index, size());
        return values[index];
    }
    const T* begin() const { return values.data(); }
    const T* end() const { return size() ? values.data() + size() : values.data(); }
};
template<class Values> static decltype(auto) sourceAt(Values& values, std::size_t index, std::size_t line) {
    const auto length = values.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), line);
    return values[index];
}

template<class Range> auto duplicate(const Range& source) {
    using T = std::remove_cvref_t<decltype(source[0])>;
    if constexpr (std::is_same_v<Range, Array<T>>) {
        return source.duplicate();
    } else {
        if (source.size() == 0) return Array<T>{};
        try {
            return Array<T>(source.begin(), source.end());
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    }
}
template<class T, class Count> void resize(Array<T>& values, Count count) {
    if constexpr (std::is_floating_point_v<T>) values.resize(static_cast<size_t>(count), std::numeric_limits<T>::quiet_NaN());
    else values.resize(static_cast<size_t>(count));
}
template<class T, class Value> void fill(Array<T>& values, Value value) {
    for (auto& entry : values) entry = static_cast<T>(value);
}
template<class T> std::vector<T> toVector(Array<T> values) {
    std::vector<T> result;
    for (auto value : values) result.push_back(value);
    return result;
}
void sortBytes(Array<ubyte> values) {
    if (values.size() > 1) std::sort(values.begin(), values.end());
}
static void sortMedianBytes(Array<ubyte> values);
template<class A, class B> auto min(A a, B b) {
    using T = std::common_type_t<A, B>;
    return static_cast<T>(b < a ? b : a);
}
template<class A, class B> auto max(A a, B b) {
    using T = std::common_type_t<A, B>;
    return static_cast<T>(a < b ? b : a);
}
void enforce(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class T> struct IndexRange {
    struct Iterator {
        T value;
        T operator*() const { return value; }
        Iterator& operator++() { ++value; return *this; }
        bool operator!=(const Iterator& other) const { return value < other.value; }
    };
    T first, last;
    Iterator begin() const { return {first}; }
    Iterator end() const { return {last}; }
};
template<class A, class B> auto dRange(A first, B last) {
    using T = std::common_type_t<A, B>;
    return IndexRange<T>{static_cast<T>(first), static_cast<T>(last)};
}
template<class Range> struct Enumerated {
    Range range;
    struct Iterator {
        Range* range;
        size_t index;
        auto operator*() const { return std::pair{index, (*range)[index]}; }
        Iterator& operator++() { ++index; return *this; }
        bool operator!=(const Iterator& other) const { return index != other.index; }
    };
    Iterator begin() { return {&range, 0}; }
    Iterator end() { return {&range, range.size()}; }
};
template<class Range> auto enumerate(Range range) { return Enumerated<Range>{range}; }
constexpr int depthDrawHistogramBins = 16;
constexpr int depthDrawHistogramShift = 4;
constexpr std::array<std::array<int, 2>, 8> depthDrawEightNeighborOffsets{{
    {-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}};
constexpr std::array<std::array<int, 2>, 4> fourNeighbors{{{-1,0},{1,0},{0,-1},{0,1}}};

Array<float> ngDepthDrawSmoothGridDepthValues(
    Read<float> sourceDepths,
    Read<ubyte> vertexValid,
    Read<int> vertexGroup,
    int cols,
    int rows,
    float depthThreshold = 18.0f,
    float depthScale = 1.0f
);

DepthDrawAlphaDepthGapDetection ngDepthDrawDetectAlphaDepthGaps(
    Read<ubyte> depth,
    Read<ubyte> alphaMask,
    int width,
    int height,
    int layerIndex = 0,
    Read<DepthDrawAlphaDepthFocusedRule> focusedRules = {}
);

DepthDrawAlphaDepthGapFillResult ngDepthDrawMedianFillDepth(
    Read<ubyte> sourceDepth,
    Read<ubyte> alphaMask,
    Read<ubyte> fillMask,
    int width,
    int height
);

Array<ubyte> ngDepthDrawBuildLayerContourBandMask(Read<ubyte> maskPixels, int width, int height, int thickness);

Array<ubyte> ngDepthDrawErodePositiveDepthMask(Read<ubyte> depthPixels, int width, int height, int thickness);

DepthDrawDepthInpaintResult ngDepthDrawInpaintMaskedLayerDepth(
    Read<ubyte> sourceDepthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height
);

Array<ubyte> ngDepthDrawSmoothMaskedPositiveDepth(
    Read<ubyte> sourceDepthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height
);

DepthDrawDepthPruneResult ngDepthDrawPruneForeignDepthSeeds(
    Read<ubyte> sourceDepthPixels,
    DepthDrawPruneLayer layer,
    int layerIndex,
    int imageWidth,
    int imageHeight,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap,
    Read<ubyte> maskPixels,
    double threshold
);

Array<int> ngDepthDrawBuildVisibleLayerMap(int imageWidth, int imageHeight, Read<DepthDrawSplitLayer> layers);

Array<ubyte> ngDepthDrawSeedLayerDepthPixels(
    DepthDrawSplitLayer layer,
    int layerIndex,
    int imageWidth,
    int imageHeight,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap,
    Read<ubyte> maskPixels,
    Read<DepthDrawSplitLayer> layers,
    Read<ubyte> contourBandMask = {},
    int upperMaskRadius = 2
);

bool ngDepthDrawHasUpperLayerMaskNearby(
    Read<DepthDrawSplitLayer> layers,
    int layerIndex,
    int globalX,
    int globalY,
    int radius
);

Array<ubyte> ngDepthDrawCreateMaskedGridDepthPixels(
    int width,
    int height,
    Read<ubyte> sourcePixels,
    Read<ubyte> maskPixels,
    std::string_view specMode,
    int gridX,
    int gridY,
    int kernelSize,
    std::string_view interpMode
);

bool ngDepthDrawHistogramIntegralDimensionsSupported(int width, int height);

DepthDrawHistogramIntegral depthDrawBuildDepthHistogramIntegral(
    Read<ubyte> depth,
    Read<ubyte> alphaMask,
    int width,
    int height
);

DepthDrawHistogramStats depthDrawLocalHistogramStats(
    const DepthDrawHistogramIntegral histogram,
    int width,
    int height,
    int x,
    int y,
    int radius,
    int centerValue = 0
);

uint depthDrawIntegralRangeSum(Read<uint> integral, int stride, int x0, int y0, int x1, int y1);

ubyte depthDrawNeighborMedianValue(
    Read<ubyte> depth,
    Read<ubyte> alphaMask,
    Read<ubyte> pending,
    int width,
    int height,
    size_t index
);

void depthDrawEnqueuePendingNeighbors(
    Array<size_t>& frontier,
    Array<ubyte>& queued,
    Read<ubyte> pending,
    Read<ubyte> alphaMask,
    int width,
    int height,
    size_t index
);

ubyte depthDrawSampleFallbackMedian(
    Read<ubyte> depth,
    Read<ubyte> alphaMask,
    Read<ubyte> pending,
    int width,
    int height,
    int x,
    int y
);

ubyte depthDrawMedian(Array<ubyte> values);

ubyte depthDrawCleanupMedian(Array<ubyte> values);

double depthDrawPercentileFromSorted(Read<ubyte> sortedValues, double percentile);

bool depthDrawIsMaskContourPixel(Read<ubyte> maskPixels, int width, int height, size_t index);

Array<ubyte> depthDrawExpandMaskFrontier(
    Read<ubyte> maskPixels,
    int width,
    int height,
    Read<ubyte> frontier,
    Array<ubyte>& bandMask
);

Array<ubyte> depthDrawErodeBinaryMask(Read<ubyte> maskPixels, int width, int height);

bool depthDrawHasPositiveMaskedNeighbor(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    size_t index
);

ubyte depthDrawSampleMaskedNeighborMedian(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    size_t index
);

ubyte depthDrawSampleMaskedMultiscaleDepth(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    size_t index
);

DepthDrawMaskedDepthEstimate depthDrawEstimateMaskedDepthAtScale(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    size_t index,
    int radius,
    int minSamples,
    double weight
);

DepthDrawMaskedSparseGridDepths depthDrawSampleMaskedSparseGridDepths(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    int x0,
    int y0,
    int radius
);

ubyte depthDrawSampleNearestMaskedDepth(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    int targetX,
    int targetY,
    int searchRadius
);

double depthDrawEstimateGridPlaneDepth(Read<double> grid);

void depthDrawEnqueueMaskedGapNeighbors(
    Array<size_t>& queue,
    Array<ubyte>& queued,
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    size_t index
);

void depthDrawPruneThinForeignSeedComponents(
    Array<ubyte>& depthPixels,
    Array<ubyte>& debugState,
    Array<ubyte>& debugScore,
    DepthDrawPruneLayer layer,
    int layerIndex,
    int imageWidth,
    int imageHeight,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap,
    Read<ubyte> maskPixels,
    ubyte globalMedian,
    double threshold
);

Array<ubyte> depthDrawCollectComponentExternalDepthSupport(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    Read<int> componentIds,
    int componentId,
    Read<ubyte> contourMask,
    int width,
    int height,
    int minX,
    int minY,
    int maxX,
    int maxY,
    int radius
);

Array<ubyte> depthDrawCollectComponentForeignVisibleDepthSupport(
    Read<size_t> indices,
    DepthDrawPruneLayer layer,
    int imageWidth,
    int imageHeight,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap,
    int layerIndex
);

Array<ubyte> depthDrawCollectLocalDepthSupport(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    int centerX,
    int centerY,
    int radius
);

Array<ubyte> depthDrawCollectForeignVisibleDepthSupport(
    int imageWidth,
    int imageHeight,
    int centerX,
    int centerY,
    int radius,
    int layerIndex,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap
);

Array<ubyte> depthDrawCollectPositiveValues(Read<ubyte> values);

compat::ConstSlice<ubyte> depthDrawSplitLayerMask(const DepthDrawSplitLayer layer);

Array<double> depthDrawGaussianKernel(int radius);

int depthDrawSampleGridPosition(int index, int count, int extent);

DepthDrawConvolutionSample depthDrawConvolveDepthAt(
    Read<ubyte> sourcePixels,
    int width,
    int height,
    int centerX,
    int centerY,
    int radius,
    Read<double> kernel
);

double depthDrawSampleLinearGrid(
    Read<double> values,
    Read<ubyte> valid,
    int gridWidth,
    int gridHeight,
    double fx,
    double fy
);

double depthDrawSampleCubicGrid(
    Read<double> values,
    Read<ubyte> valid,
    int gridWidth,
    int gridHeight,
    double fx,
    double fy
);

double depthDrawCatmullRomWeight(double x);

int depthDrawClampInt(int value, int minValue, int maxValue);

Array<float> ngDepthDrawSmoothGridDepthValues(
    Read<float> sourceDepths,
    Read<ubyte> vertexValid,
    Read<int> vertexGroup,
    int cols,
    int rows,
    float depthThreshold,
    float depthScale
) {
    // Port of depth-draw src/scene/geometry.js:smoothGridVertexPositions.
    if (!(cols >= 0 && rows >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Grid dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 731);
    auto count = static_cast<size_t>(cols) * static_cast<size_t>(rows);
    if (!(sourceDepths.size() == count))
        throw compat::ThrowableException(copyEnforceMessage("Depth count must match grid dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 733);
    if (!(vertexValid.size() == count))
        throw compat::ThrowableException(copyEnforceMessage("Valid mask must match grid dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 734);
    if (!(vertexGroup.size() == count))
        throw compat::ThrowableException(copyEnforceMessage("Vertex groups must match grid dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 735);

    Array<float> input;
    if (sourceDepths.size()) {
        try {
            input = Array<float>(sourceDepths.begin(), sourceDepths.end());
        } catch (const compat::OutOfMemoryError&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    }
    auto smoothFlags = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(count);
    const auto duplicateSmoothingArray=[](const auto& values) {
        using Element=std::remove_cvref_t<decltype(values[0])>;
        static_assert(std::is_same_v<Element,ubyte> || std::is_same_v<Element,float>);
        try {
            return values.duplicate();
        } catch (const compat::OutOfMemoryError&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    };
    auto edgeThresholdZ = max(0.0001f, (depthThreshold / 255.0f) * max(depthScale, 0.0001f));

    auto sameGroupValid = [&](int x, int y, int group) {
        if (x < 0 || y < 0 || x >= cols || y >= rows) return false;
        auto index = static_cast<size_t>(y) * static_cast<size_t>(cols) + static_cast<size_t>(x);
        return compat::ngArrayAt(vertexValid, index) != 0 && compat::ngArrayAt(vertexGroup, index) == group;
    };

    for (auto gy : dRange(0, rows)) {
        for (auto gx : dRange(0, cols)) {
            auto index = static_cast<size_t>(gy) * static_cast<size_t>(cols) + static_cast<size_t>(gx);
            if (!sourceAt(vertexValid, index, 751)) continue;
            auto group = sourceAt(vertexGroup, index, 752);
            bool boundary = !sameGroupValid(gx - 1, gy, group) || !sameGroupValid(gx + 1, gy, group) ||
                !sameGroupValid(gx, gy - 1, group) || !sameGroupValid(gx, gy + 1, group);
            float minZ = sourceAt(input, index, 755);
            float maxZ = sourceAt(input, index, 756);
            int samples{};
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (!sameGroupValid(add32(gx, dx), add32(gy, dy), group)) continue;
                    auto sampleIndex = static_cast<size_t>(add32(gy, dy)) * static_cast<size_t>(cols) + static_cast<size_t>(add32(gx, dx));
                    minZ = min(minZ, sourceAt(input, sampleIndex, 762));
                    maxZ = max(maxZ, sourceAt(input, sampleIndex, 763));
                    samples++;
                }
            }
            sourceAt(smoothFlags, index, 767) = boundary || (samples >= 3 && maxZ - minZ >= edgeThresholdZ) ? 1 : 0;
        }
    }

    for (auto _ : dRange(0, 2)) {
        auto expanded = duplicateSmoothingArray(smoothFlags);
        for (auto gy : dRange(0, rows)) {
            for (auto gx : dRange(0, cols)) {
                auto index = static_cast<size_t>(gy) * static_cast<size_t>(cols) + static_cast<size_t>(gx);
                if (!sourceAt(vertexValid, index, 776) || sourceAt(smoothFlags, index, 776)) continue;
                auto group = sourceAt(vertexGroup, index, 777);
                bool adjacent{};
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        auto sx = add32(gx, dx);
                        auto sy = add32(gy, dy);
                        if (!sameGroupValid(sx, sy, group)) continue;
                        auto sampleIndex = static_cast<size_t>(sy) * static_cast<size_t>(cols) + static_cast<size_t>(sx);
                        if (sourceAt(smoothFlags, sampleIndex, 786)) adjacent = true;
                    }
                }
                if (adjacent) sourceAt(expanded, index, 789) = 1;
            }
        }
        smoothFlags = expanded;
    }

    for (auto pass : dRange(0, 6)) {
        auto output = duplicateSmoothingArray(input);
        auto factor = pass % 2 == 0 ? 0.62f : -0.64f;
        for (auto gy : dRange(0, rows)) {
            for (auto gx : dRange(0, cols)) {
                auto index = static_cast<size_t>(gy) * static_cast<size_t>(cols) + static_cast<size_t>(gx);
                if (!sourceAt(vertexValid, index, 801) || !sourceAt(smoothFlags, index, 801)) continue;
                auto group = sourceAt(vertexGroup, index, 802);
                float weightedSum = 0.0f;
                float weightTotal = 0.0f;
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        auto distance = std::sqrt(static_cast<float>(add32(mul32(dx, dx), mul32(dy, dy))));
                        if (distance <= 0.0f || distance > 2.5f) continue;
                        auto sx = add32(gx, dx);
                        auto sy = add32(gy, dy);
                        if (!sameGroupValid(sx, sy, group)) continue;
                        auto sampleIndex = static_cast<size_t>(sy) * static_cast<size_t>(cols) + static_cast<size_t>(sx);
                        auto weight = 1.0f / distance;
                        weightedSum += sourceAt(input, sampleIndex, 815) * weight;
                        weightTotal += weight;
                    }
                }
                if (weightTotal <= 0.0f) continue;
                auto averageZ = weightedSum / weightTotal;
                sourceAt(output, index, 821) = sourceAt(input, index, 821) + factor * (averageZ - sourceAt(input, index, 821));
            }
        }
        input = output;
    }
    return input;
}

DepthDrawAlphaDepthGapDetection ngDepthDrawDetectAlphaDepthGaps(
    Read<ubyte> depth,
    Read<ubyte> alphaMask,
    int width,
    int height,
    int layerIndex,
    Read<DepthDrawAlphaDepthFocusedRule> focusedRules
) {
    // Port of depth-draw src/composite/alphaDepthGapFill.js:detectAlphaDepthGaps.
    if (!(width >= 0 && height >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Depth dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 865);
    if (!(depth.size() == static_cast<size_t>(mul32(width, height))))
        throw compat::ThrowableException(copyEnforceMessage("Depth length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 866);
    if (!(alphaMask.size() == depth.size()))
        throw compat::ThrowableException(copyEnforceMessage("Alpha mask length must match depth"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 867);

    DepthDrawAlphaDepthGapDetection result;
    result.mask = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(depth.size());
    auto histogram = depthDrawBuildDepthHistogramIntegral(depth, alphaMask, width, height);

    for (auto y : dRange(0, height)) {
        for (auto x : dRange(0, width)) {
            auto index = add32(mul32(y, width), x);
            if (!sourceAt(alphaMask, index, 876)) continue;
            auto value = sourceAt(depth, index, 877);
            if (value <= 0) {
                sourceAt(result.mask, index, 879) = 1;
                result.zero += 1;
                continue;
            }
            auto stats16 = depthDrawLocalHistogramStats(histogram, width, height, x, y, 16, value);
            auto stats30 = depthDrawLocalHistogramStats(histogram, width, height, x, y, 30, value);
            auto expected = max(stats16.valid ? stats16.p65 : 0, stats30.valid ? stats30.p65 : 0);
            if (expected > 0 && expected - value >= max(12, toInt(std::llround(static_cast<double>(expected) * 0.075)))) {
                sourceAt(result.mask, index, 887) = 1;
                result.depression += 1;
            }
        }
    }

    for (auto y : dRange(1, max(1, sub32(height, 1)))) {
        for (auto x : dRange(1, max(1, sub32(width, 1)))) {
            auto index = add32(mul32(y, width), x);
            if (!sourceAt(alphaMask, index, 896)) continue;
            auto value = sourceAt(depth, index, 897);
            ubyte high{};
            for (auto offset : depthDrawEightNeighborOffsets) {
                auto sampleIndex = add32(add32(index, mul32(sourceAt(offset, 1, 900), width)), sourceAt(offset, 0, 900));
                if (sourceAt(alphaMask, sampleIndex, 901) && sourceAt(depth, sampleIndex, 901) > high) high = sourceAt(depth, sampleIndex, 901);
            }
            if (toInt(high) - toInt(value) >= max(16, toInt(std::llround(static_cast<double>(high) * 0.10)))) {
                sourceAt(result.mask, index, 904) = 1;
                result.cliff += 1;
            }
        }
    }

    for (auto rule : focusedRules) {
        if (rule.layerIndex != layerIndex) continue;
        auto x0 = max(0, rule.x);
        auto y0 = max(0, rule.y);
        auto x1 = min(width, add32(rule.x, rule.w));
        auto y1 = min(height, add32(rule.y, rule.h));
        for (auto y : dRange(y0, y1)) {
            for (auto x : dRange(x0, x1)) {
                auto index = add32(mul32(y, width), x);
                if (!sourceAt(alphaMask, index, 919)) continue;
                auto stats = depthDrawLocalHistogramStats(histogram, width, height, x, y, rule.radius, sourceAt(depth, index, 920));
                auto expected = stats.valid ? (stats.p65 ? stats.p65 : stats.p50) : 0;
                if (sourceAt(depth, index, 922) <= 0 || (expected > 0 && expected - sourceAt(depth, index, 922) >= rule.lift)) {
                    if (!sourceAt(result.mask, index, 923)) result.focusedAdded += 1;
                    sourceAt(result.mask, index, 924) = 1;
                }
            }
        }
    }

    for (auto value : result.mask) {
        if (value) result.total += 1;
    }
    return result;
}

DepthDrawAlphaDepthGapFillResult ngDepthDrawMedianFillDepth(
    Read<ubyte> sourceDepth,
    Read<ubyte> alphaMask,
    Read<ubyte> fillMask,
    int width,
    int height
) {
    // Port of depth-draw src/composite/alphaDepthGapFill.js:medianFillDepth.
    if (!(width >= 0 && height >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Depth dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 944);
    if (!(sourceDepth.size() == static_cast<size_t>(mul32(width, height))))
        throw compat::ThrowableException(copyEnforceMessage("Depth length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 945);
    if (!(alphaMask.size() == sourceDepth.size()))
        throw compat::ThrowableException(copyEnforceMessage("Alpha mask length must match depth"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 946);
    if (!(fillMask.size() == sourceDepth.size()))
        throw compat::ThrowableException(copyEnforceMessage("Fill mask length must match depth"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 947);

    DepthDrawAlphaDepthGapFillResult result;
    result.depth = duplicate(sourceDepth);
    auto pending = duplicate(fillMask);
    Array<ubyte> queued;
    resize(queued, fillMask.size());
    Array<size_t> pendingIndices;
    Array<size_t> frontier;

    for (auto index : dRange(0, pending.size())) {
        if (!sourceAt(pending, index, 958) || !sourceAt(alphaMask, index, 958)) continue;
        pendingIndices.append(index);
        if (depthDrawNeighborMedianValue(result.depth, alphaMask, pending, width, height, index) > 0) {
            sourceAt(queued, index, 961) = 1;
            frontier.append(index);
        }
    }

    while (frontier.size()) {
        Array<TupleIndexValue> updates;
        for (auto index : frontier) {
            sourceAt(queued, index, 969) = 0;
            if (!sourceAt(pending, index, 970) || !sourceAt(alphaMask, index, 970)) continue;
            auto value = depthDrawNeighborMedianValue(result.depth, alphaMask, pending, width, height, index);
            if (value > 0) updates.append(TupleIndexValue(index, value));
        }
        if (!updates.size()) break;
        Array<size_t> nextFrontier;
        for (auto update : updates) {
            sourceAt(result.depth, update.index, 977) = update.value;
            sourceAt(pending, update.index, 978) = 0;
            result.filled += 1;
            depthDrawEnqueuePendingNeighbors(nextFrontier, queued, pending, alphaMask, width, height, update.index);
        }
        frontier = nextFrontier;
    }

    for (auto index : pendingIndices) {
        if (!sourceAt(pending, index, 986) || !sourceAt(alphaMask, index, 986)) continue;
        auto x = toInt(index % width);
        auto y = toInt(index / width);
        auto value = depthDrawSampleFallbackMedian(result.depth, alphaMask, pending, width, height, x, y);
        if (value > 0) {
            sourceAt(result.depth, index, 991) = value;
            sourceAt(pending, index, 992) = 0;
            result.filled += 1;
        }
    }

    for (auto index : pendingIndices) {
        if (sourceAt(pending, index, 998)) result.remaining += 1;
    }
    return result;
}

Array<ubyte> ngDepthDrawBuildLayerContourBandMask(Read<ubyte> maskPixels, int width, int height, int thickness) {
    // Port of depth-draw src/composite/depthCleanup.js:buildLayerContourBandMask.
    if (!(width >= 0 && height >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Mask dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1005);
    if (!(maskPixels.size() == static_cast<size_t>(mul32(width, height))))
        throw compat::ThrowableException(copyEnforceMessage("Mask length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1006);

    auto contourMask = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(
        maskPixels.size());
    auto bandMask = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(
        maskPixels.size());
    for (auto index : dRange(0, maskPixels.size())) {
        if (depthDrawIsMaskContourPixel(maskPixels, width, height, index)) {
            sourceAt(contourMask, index, 1014) = 1;
            sourceAt(bandMask, index, 1015) = 1;
        }
    }

    auto frontier = contourMask;
    for (auto _pass : dRange(1, max(1, thickness))) {
        frontier = depthDrawExpandMaskFrontier(maskPixels, width, height, frontier, bandMask);
    }
    return bandMask;
}

Array<ubyte> ngDepthDrawErodePositiveDepthMask(Read<ubyte> depthPixels, int width, int height, int thickness) {
    // Port of depth-draw src/composite/depthCleanup.js:erodePositiveDepthMask.
    if (!(width >= 0 && height >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Depth dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1028);
    if (!(depthPixels.size() == static_cast<size_t>(mul32(width, height))))
        throw compat::ThrowableException(copyEnforceMessage("Depth length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1029);

    auto mask = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(
        depthPixels.size());
    for (auto [i, value] : enumerate(depthPixels)) sourceAt(mask, i, 1033) = value > 0 ? 1 : 0;
    for (auto _pass : dRange(0, max(0, thickness))) {
        mask = depthDrawErodeBinaryMask(mask, width, height);
    }
    return mask;
}

DepthDrawDepthInpaintResult ngDepthDrawInpaintMaskedLayerDepth(
    Read<ubyte> sourceDepthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height
) {
    // Port of depth-draw src/composite/depthCleanup.js:inpaintMaskedLayerDepth.
    if (!(width >= 0 && height >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Depth dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1047);
    if (!(sourceDepthPixels.size() == static_cast<size_t>(mul32(width, height))))
        throw compat::ThrowableException(copyEnforceMessage("Depth length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1048);
    if (!(maskPixels.size() == sourceDepthPixels.size()))
        throw compat::ThrowableException(copyEnforceMessage("Mask length must match depth"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1049);

    DepthDrawDepthInpaintResult result;
    if (sourceDepthPixels.size()) {
        try {
            result.pixels = Array<ubyte>(sourceDepthPixels.begin(), sourceDepthPixels.end());
        } catch (const compat::OutOfMemoryError&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    }
    result.filledMask = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(
        sourceDepthPixels.size());
    Array<size_t> queue;
    auto queued = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(
        sourceDepthPixels.size());
    size_t head{};

    for (auto index : dRange(0, sourceDepthPixels.size())) {
        if (!sourceAt(maskPixels, index, 1060) || sourceAt(result.pixels, index, 1060) > 0) continue;
        if (!depthDrawHasPositiveMaskedNeighbor(result.pixels, maskPixels, width, height, index)) continue;
        ::nijigenerate::io::depthimage::detail::appendIndexArray(queue, index);
        sourceAt(queued, index, 1063) = 1;
    }

    while (head < queue.size()) {
        auto index = sourceAt(queue, head++, 1067);
        sourceAt(queued, index, 1068) = 0;
        if (!sourceAt(maskPixels, index, 1069) || sourceAt(result.pixels, index, 1069) > 0) continue;
        auto fillDepth = depthDrawSampleMaskedMultiscaleDepth(result.pixels, maskPixels, width, height, index);
        if (fillDepth <= 0) continue;
        sourceAt(result.pixels, index, 1072) = fillDepth;
        sourceAt(result.filledMask, index, 1073) = 1;
        depthDrawEnqueueMaskedGapNeighbors(queue, queued, result.pixels, maskPixels, width, height, index);
    }
    return result;
}

Array<ubyte> ngDepthDrawSmoothMaskedPositiveDepth(
    Read<ubyte> sourceDepthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height
) {
    // Port of depth-draw src/composite/depthCleanup.js:smoothMaskedPositiveDepth.
    if (!(width >= 0 && height >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Depth dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1086);
    if (sourceDepthPixels.size() != static_cast<size_t>(mul32(width, height)))
        throw compat::ThrowableException(copyEnforceMessage("Depth length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1087);
    if (maskPixels.size() != sourceDepthPixels.size())
        throw compat::ThrowableException(copyEnforceMessage("Mask length must match depth"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1088);

    constexpr std::array<std::array<int, 3>, 9> kernel{{
        {-1, -1, 1}, {0, -1, 2}, {1, -1, 1},
        {-1, 0, 2}, {0, 0, 4}, {1, 0, 2},
        {-1, 1, 1}, {0, 1, 2}, {1, 1, 1},
    }};
    Array<ubyte> input;
    if (sourceDepthPixels.size()) {
        try {
            input = Array<ubyte>(sourceDepthPixels.begin(), sourceDepthPixels.end());
        } catch (const compat::OutOfMemoryError&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    }
    for (auto _pass : dRange(0, 3)) {
        Array<ubyte> output;
        try {
            output = input.duplicate();
        } catch (const compat::OutOfMemoryError&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
        for (auto y : dRange(0, height)) {
            for (auto x : dRange(0, width)) {
                auto index = add32(mul32(y, width), x);
                if (!sourceAt(maskPixels, static_cast<size_t>(index), 1101) || sourceAt(input, static_cast<size_t>(index), 1101) <= 0) continue;
                int weightedSum{};
                int totalWeight{};
                for (auto entry : kernel) {
                    auto sx = add32(x, entry[0]);
                    auto sy = add32(y, entry[1]);
                    if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
                    auto sampleIndex = add32(mul32(sy, width), sx);
                    auto sampleDepth = sourceAt(input, static_cast<size_t>(sampleIndex), 1109);
                    if (!sourceAt(maskPixels, static_cast<size_t>(sampleIndex), 1110) || sampleDepth <= 0) continue;
                    weightedSum += sampleDepth * entry[2];
                    totalWeight += entry[2];
                }
                if (totalWeight > 0) {
                    sourceAt(output, static_cast<size_t>(index), 1115) = static_cast<ubyte>(min(255, max(0, toInt(std::llround(static_cast<double>(weightedSum) / totalWeight)))));
                }
            }
        }
        input = output;
    }
    return input;
}

DepthDrawDepthPruneResult ngDepthDrawPruneForeignDepthSeeds(
    Read<ubyte> sourceDepthPixels,
    DepthDrawPruneLayer layer,
    int layerIndex,
    int imageWidth,
    int imageHeight,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap,
    Read<ubyte> maskPixels,
    double threshold
) {
    // Port of depth-draw src/composite/depthPrune.js:pruneForeignDepthSeeds.
    if (!(layer.width >= 0 && layer.height >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Layer dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1136);
    if (!(imageWidth >= 0 && imageHeight >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Image dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1137);
    if (sourceDepthPixels.size() != static_cast<size_t>(mul32(layer.width, layer.height)))
        throw compat::ThrowableException(copyEnforceMessage("Layer depth length must match layer dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1138);
    if (maskPixels.size() != sourceDepthPixels.size())
        throw compat::ThrowableException(copyEnforceMessage("Mask length must match layer depth"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1140);
    if (stableDepthPixels.size() != static_cast<size_t>(mul32(imageWidth, imageHeight)))
        throw compat::ThrowableException(copyEnforceMessage("Stable depth length must match image dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1141);
    if (visibleLayerMap.size() != stableDepthPixels.size())
        throw compat::ThrowableException(copyEnforceMessage("Visible layer map length must match image dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1143);

    DepthDrawDepthPruneResult result;
    const auto duplicatePruneBytes=[](const auto& values) {
        using Element=std::remove_cvref_t<decltype(values[0])>;
        static_assert(std::is_same_v<Element,ubyte>);
        try {
            return duplicate(values);
        } catch (const compat::OutOfMemoryError&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    };
    result.pixels = duplicatePruneBytes(sourceDepthPixels);
    resize(result.debugState, result.pixels.size());
    resize(result.debugScore, result.pixels.size());
    auto globalSupport = depthDrawCollectPositiveValues(result.pixels);
    auto globalMedian = globalSupport.size() ? depthDrawCleanupMedian(globalSupport) : static_cast<ubyte>(0);

    for (auto i : dRange(0, result.pixels.size())) {
        if (!sourceAt(maskPixels, i, 1153)) continue;
        auto& state = sourceAt(result.debugState, i, 1154);
        state = sourceAt(result.pixels, i, 1154) > 0 ? 2 : 1;
    }

    for (auto y : dRange(0, layer.height)) {
        for (auto x : dRange(0, layer.width)) {
            auto localIndex = add32(mul32(y, layer.width), x);
            auto seedDepth = sourceAt(result.pixels, localIndex, 1160);
            if (!sourceAt(maskPixels, localIndex, 1161) || seedDepth == 0) continue;

            auto globalX = add32(layer.left, x);
            auto globalY = add32(layer.top, y);
            auto sameLayerSupport = depthDrawCollectLocalDepthSupport(
                result.pixels, maskPixels, layer.width, layer.height, x, y, 4);
            if (sameLayerSupport.size() < 4) continue;

            auto sameMedian = depthDrawCleanupMedian(duplicatePruneBytes(sameLayerSupport));
            auto sortedSupport = duplicatePruneBytes(sameLayerSupport);
            sortMedianBytes(sortedSupport);
            auto q1 = depthDrawPercentileFromSorted(sortedSupport, 0.25);
            auto q3 = depthDrawPercentileFromSorted(sortedSupport, 0.75);
            const auto lastSupport = sourceAt(sortedSupport, sortedSupport.size() - 1, 1174);
            const auto firstSupport = sourceAt(sortedSupport, 0, 1174);
            auto localRange = static_cast<double>(lastSupport) - static_cast<double>(firstSupport);
            auto foreignSupport = depthDrawCollectForeignVisibleDepthSupport(
                imageWidth, imageHeight, globalX, globalY, 4, layerIndex, stableDepthPixels, visibleLayerMap);
            if (foreignSupport.size() < 3) continue;

            auto foreignMedian = depthDrawCleanupMedian(duplicatePruneBytes(foreignSupport));
            auto sameDistance = std::abs(static_cast<double>(seedDepth) - static_cast<double>(sameMedian));
            auto foreignDistance = std::abs(static_cast<double>(seedDepth) - static_cast<double>(foreignMedian));
            auto globalDistance = std::abs(static_cast<double>(seedDepth) - static_cast<double>(globalMedian));
            auto localThreshold = max(2.0, min(6.0, threshold * 0.1));
            auto bandDistance = seedDepth < q1 ? q1 - seedDepth : seedDepth > q3 ? seedDepth - q3 : 0.0;
            auto hasSharpLocalGradient = localRange > localThreshold * 3.0 && bandDistance > localThreshold;
            auto score = max(sameDistance, globalDistance) - foreignDistance;
            auto& scoreDestination = sourceAt(result.debugScore, localIndex, 1187);
            const auto previousScore = sourceAt(result.debugScore, localIndex, 1187);
            const auto scoreByte = static_cast<ubyte>(min(255, max(0, toInt(std::llround(score * 24.0)))));
            scoreDestination = max(previousScore, scoreByte);

            if (((sameDistance > localThreshold && globalDistance > localThreshold) || hasSharpLocalGradient) &&
                foreignDistance < min(sameDistance, globalDistance)) {
                sourceAt(result.pixels, localIndex, 1192) = 0;
                sourceAt(result.debugState, localIndex, 1193) = 3;
            }
        }
    }

    depthDrawPruneThinForeignSeedComponents(
        result.pixels, result.debugState, result.debugScore, layer, layerIndex, imageWidth, imageHeight,
        stableDepthPixels, visibleLayerMap, maskPixels, globalMedian, threshold);
    return result;
}

Array<int> ngDepthDrawBuildVisibleLayerMap(int imageWidth, int imageHeight, Read<DepthDrawSplitLayer> layers) {
    // Port of depth-draw src/composite/depthSplit.js:buildVisibleLayerMap.
    if (!(imageWidth >= 0 && imageHeight >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Image dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1206);
    auto visibleLayerMap = ::nijigenerate::io::depthimage::detail::initialPrimitiveArray<int>(
        static_cast<size_t>(mul32(imageWidth, imageHeight)), 0, 4, 864, 877);
    for (auto& value : visibleLayerMap) value = -1;

    for (int layerIndex = sub32(toInt(layers.size()), 1); layerIndex >= 0; layerIndex -= 1) {
        auto layer = sourceAt(layers, static_cast<size_t>(layerIndex), 1212);
        auto maskPixels = depthDrawSplitLayerMask(layer);
        if (maskPixels.size() != static_cast<size_t>(mul32(layer.width, layer.height)))
            throw compat::ThrowableException(copyEnforceMessage("Layer mask length must match layer dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1214);
        for (auto y : dRange(0, layer.height)) {
            for (auto x : dRange(0, layer.width)) {
                auto localIndex = add32(mul32(y, layer.width), x);
                if (!sourceAt(maskPixels, static_cast<size_t>(localIndex), 1219)) continue;
                auto globalX = add32(layer.left, x);
                auto globalY = add32(layer.top, y);
                if (globalX < 0 || globalX >= imageWidth || globalY < 0 || globalY >= imageHeight) continue;
                auto globalIndex = add32(mul32(globalY, imageWidth), globalX);
                if (sourceAt(visibleLayerMap, static_cast<size_t>(globalIndex), 1224) < 0)
                    sourceAt(visibleLayerMap, static_cast<size_t>(globalIndex), 1224) = layerIndex;
            }
        }
    }
    return visibleLayerMap;
}

Array<ubyte> ngDepthDrawSeedLayerDepthPixels(
    DepthDrawSplitLayer layer,
    int layerIndex,
    int imageWidth,
    int imageHeight,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap,
    Read<ubyte> maskPixels,
    Read<DepthDrawSplitLayer> layers,
    Read<ubyte> contourBandMask,
    int upperMaskRadius
) {
    // Port of depth-draw src/composite/depthSplit.js:seedLayerDepthPixels.
    if (!(layer.width >= 0 && layer.height >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Layer dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1244);
    if (!(imageWidth >= 0 && imageHeight >= 0))
        throw compat::ThrowableException(copyEnforceMessage("Image dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1245);
    if (maskPixels.size() != static_cast<size_t>(mul32(layer.width, layer.height)))
        throw compat::ThrowableException(copyEnforceMessage("Mask length must match layer dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1246);
    if (stableDepthPixels.size() != static_cast<size_t>(mul32(imageWidth, imageHeight)))
        throw compat::ThrowableException(copyEnforceMessage("Stable depth length must match image dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1248);
    if (visibleLayerMap.size() != stableDepthPixels.size())
        throw compat::ThrowableException(copyEnforceMessage("Visible layer map length must match image dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1250);
    if (contourBandMask.size()) {
        if (contourBandMask.size() != maskPixels.size())
            throw compat::ThrowableException(copyEnforceMessage("Contour band mask length must match layer dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1252);
    }

    auto depthPixels = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(
        static_cast<size_t>(mul32(layer.width, layer.height)));
    for (auto y : dRange(0, layer.height)) {
        for (auto x : dRange(0, layer.width)) {
            auto localIndex = add32(mul32(y, layer.width), x);
            if (!sourceAt(maskPixels, static_cast<size_t>(localIndex), 1260)) continue;
            if (contourBandMask.size() && sourceAt(contourBandMask, static_cast<size_t>(localIndex), 1261)) continue;

            auto globalX = add32(layer.left, x);
            auto globalY = add32(layer.top, y);
            if (globalX < 0 || globalX >= imageWidth || globalY < 0 || globalY >= imageHeight) continue;
            auto globalIndex = add32(mul32(globalY, imageWidth), globalX);
            if (sourceAt(visibleLayerMap, static_cast<size_t>(globalIndex), 1267) == layerIndex) {
                if (ngDepthDrawHasUpperLayerMaskNearby(layers, layerIndex, globalX, globalY, upperMaskRadius)) continue;
                sourceAt(depthPixels, static_cast<size_t>(localIndex), 1269) =
                    sourceAt(stableDepthPixels, static_cast<size_t>(globalIndex), 1269);
            }
        }
    }
    return depthPixels;
}

bool ngDepthDrawHasUpperLayerMaskNearby(
    Read<DepthDrawSplitLayer> layers,
    int layerIndex,
    int globalX,
    int globalY,
    int radius
) {
    // Port of depth-draw src/composite/depthSplit.js:hasUpperLayerMaskNearby.
    for (auto upperIndex : dRange(add32(layerIndex, 1), toInt(layers.size()))) {
        auto layer = sourceAt(layers, static_cast<size_t>(upperIndex), 1285);
        auto maskPixels = depthDrawSplitLayerMask(layer);
        if (maskPixels.size() != static_cast<size_t>(mul32(layer.width, layer.height)))
            throw compat::ThrowableException(copyEnforceMessage("Layer mask length must match layer dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1287);
        if (globalX < sub32(layer.left, radius) ||
            globalX >= add32(add32(layer.left, layer.width), radius) ||
            globalY < sub32(layer.top, radius) ||
            globalY >= add32(add32(layer.top, layer.height), radius)) {
            continue;
        }

        for (auto dy : dRange(sub32(0, radius), add32(radius, 1))) {
            auto sy = add32(globalY, dy);
            auto localY = sub32(sy, layer.top);
            if (localY < 0 || localY >= layer.height) continue;
            for (auto dx : dRange(sub32(0, radius), add32(radius, 1))) {
                auto sx = add32(globalX, dx);
                auto localX = sub32(sx, layer.left);
                if (localX < 0 || localX >= layer.width) continue;
                if (sourceAt(maskPixels, static_cast<size_t>(add32(mul32(localY, layer.width), localX)), 1304)) return true;
            }
        }
    }
    return false;
}

Array<ubyte> ngDepthDrawCreateMaskedGridDepthPixels(
    int width,
    int height,
    Read<ubyte> sourcePixels,
    Read<ubyte> maskPixels,
    std::string_view specMode,
    int gridX,
    int gridY,
    int kernelSize,
    std::string_view interpMode
) {
    if (sourcePixels.size() != static_cast<size_t>(mul32(width, height))) throw compat::ThrowableException(copyEnforceMessage("Source pixels length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1322);
    if (maskPixels.size() != sourcePixels.size()) throw compat::ThrowableException(copyEnforceMessage("Mask pixels length must match source pixels"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1323);
    if (!(interpMode == "linear" || interpMode == "cubic")) throw compat::ThrowableException(copyEnforceMessage("Only depth-draw linear and cubic grid interpolation are implemented"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1324);

    auto gridWidth = specMode == "size"
        ? max(2, add32(toInt(std::ceil(static_cast<double>(sub32(width, 1)) / static_cast<double>(max(1, gridX)))), 1))
        : max(2, add32(gridX, 1));
    auto gridHeight = specMode == "size"
        ? max(2, add32(toInt(std::ceil(static_cast<double>(sub32(height, 1)) / static_cast<double>(max(1, gridY)))), 1))
        : max(2, add32(gridY, 1));
    auto radius = max(1, kernelSize / 2);
    auto kernel = depthDrawGaussianKernel(radius);

    auto controlValues = ::nijigenerate::io::depthimage::detail::initialPrimitiveArray<double>(
        static_cast<size_t>(mul32(gridWidth, gridHeight)), std::numeric_limits<double>::quiet_NaN(),
        8, 970, 1000);
    auto controlValid = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(
        static_cast<size_t>(mul32(gridWidth, gridHeight)));

    for (auto gy : dRange(0, gridHeight)) {
        auto py = depthDrawSampleGridPosition(gy, gridHeight, height);
        for (auto gx : dRange(0, gridWidth)) {
            auto px = depthDrawSampleGridPosition(gx, gridWidth, width);
            auto sample = depthDrawConvolveDepthAt(sourcePixels, width, height, px, py, radius, kernel);
            auto index = add32(mul32(gy, gridWidth), gx);
            sourceAt(controlValues, static_cast<size_t>(index), 1347) = sample.value;
            sourceAt(controlValid, static_cast<size_t>(index), 1348) = sample.valid ? 1 : 0;
        }
    }

    auto result = ::nijigenerate::io::depthimage::detail::initialByteArray<Array<ubyte>>(sourcePixels.size());
    for (auto y : dRange(0, height)) {
        auto fy = (static_cast<double>(y) / static_cast<double>(max(1, sub32(height, 1)))) * (sub32(gridHeight, 1));
        for (auto x : dRange(0, width)) {
            auto fx = (static_cast<double>(x) / static_cast<double>(max(1, sub32(width, 1)))) * (sub32(gridWidth, 1));
            auto value = interpMode == "cubic"
                ? depthDrawSampleCubicGrid(controlValues, controlValid, gridWidth, gridHeight, fx, fy)
                : depthDrawSampleLinearGrid(controlValues, controlValid, gridWidth, gridHeight, fx, fy);
            auto rounded = static_cast<ubyte>(min(255, max(0, toInt(std::llround(value)))));
            auto index = add32(mul32(y, width), x);
            sourceAt(result, static_cast<size_t>(index), 1363) =
                sourceAt(maskPixels, static_cast<size_t>(index), 1363) ? rounded : 0;
        }
    }
    return result;
}

bool ngDepthDrawHistogramIntegralDimensionsSupported(int width, int height) {
    if (width < 0 || height < 0) return false;
    auto columns = static_cast<ulong>(width) + 1;
    auto rows = static_cast<ulong>(height) + 1;
    if (columns > std::numeric_limits<ulong>::max() / rows) return false;
    auto cells = columns * rows;
    constexpr auto bytesPerCell = depthDrawHistogramBins * sizeof(uint);
    return cells <= DepthDrawMaxHistogramIntegralBytes / bytesPerCell;
}

DepthDrawHistogramIntegral depthDrawBuildDepthHistogramIntegral(
    Read<ubyte> depth,
    Read<ubyte> alphaMask,
    int width,
    int height
) {
    if (!ngDepthDrawHistogramIntegralDimensionsSupported(width, height))
        throw compat::ThrowableException(copyEnforceMessage("Depth layer is too large for alpha-depth gap cleanup"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 1421);
    DepthDrawHistogramIntegral histogram;
    histogram.stride = add32(width, 1);
    auto size = static_cast<size_t>(histogram.stride) * static_cast<size_t>(add32(height, 1));
    try {
        histogram.bins = Array<Array<uint>>(std::vector<Array<uint>>(depthDrawHistogramBins));
    } catch (const compat::OutOfMemoryError&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 877);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 877);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 877);
    }
    for (auto bin : dRange(0, depthDrawHistogramBins)) {
        sourceAt(histogram.bins, bin, 1428) =
            ::nijigenerate::io::depthimage::detail::initialPrimitiveArray<uint>(size, 0, 4, 864, 877);
    }

    auto rowCounts = ::nijigenerate::io::depthimage::detail::initialPrimitiveArray<uint>(
        depthDrawHistogramBins, 0, 4, 864, 877);
    for (auto y : dRange(1, add32(height, 1))) {
        fill(rowCounts, 0);
        auto sourceRow = (y - 1) * width;
        auto outputRow = y * histogram.stride;
        auto previousRow = (y - 1) * histogram.stride;
        for (auto x : dRange(1, add32(width, 1))) {
            auto sourceIndex = sourceRow + x - 1;
            auto value = sourceAt(depth, sourceIndex, 1440);
            if (sourceAt(alphaMask, sourceIndex, 1441) && value > 0) {
                sourceAt(rowCounts, value >> depthDrawHistogramShift, 1442) += 1;
            }
            auto outputIndex = outputRow + x;
            auto previousIndex = previousRow + x;
            for (auto bin : dRange(0, depthDrawHistogramBins)) {
                sourceAt(sourceAt(histogram.bins, bin, 1447), outputIndex, 1447) =
                    sourceAt(sourceAt(histogram.bins, bin, 1447), previousIndex, 1447) +
                    sourceAt(rowCounts, bin, 1447);
            }
        }
    }
    return histogram;
}

DepthDrawHistogramStats depthDrawLocalHistogramStats(
    const DepthDrawHistogramIntegral histogram,
    int width,
    int height,
    int x,
    int y,
    int radius,
    int centerValue
) {
    auto x0 = max(0, sub32(x, radius));
    auto y0 = max(0, sub32(y, radius));
    auto x1 = min(width, add32(add32(x, radius), 1));
    auto y1 = min(height, add32(add32(y, radius), 1));
    uint total{};

    for (auto bin : dRange(0, depthDrawHistogramBins)) {
        auto count = depthDrawIntegralRangeSum(sourceAt(histogram.bins, bin, 1470),
            histogram.stride, x0, y0, x1, y1);
        if (centerValue > 0 && bin == (centerValue >> depthDrawHistogramShift)) {
            count = count > 0 ? count - 1 : 0;
        }
        total += count;
    }

    DepthDrawHistogramStats stats;
    if (total < 10) return stats;
    auto target50 = max(1u, static_cast<uint>(std::ceil(static_cast<double>(total) * 0.50)));
    auto target65 = max(1u, static_cast<uint>(std::ceil(static_cast<double>(total) * 0.65)));
    auto target80 = max(1u, static_cast<uint>(std::ceil(static_cast<double>(total) * 0.80)));
    uint cumulative{};
    stats.valid = true;
    stats.p50 = 255;
    stats.p65 = 255;
    stats.p80 = 255;

    for (auto bin : dRange(0, depthDrawHistogramBins)) {
        auto count = depthDrawIntegralRangeSum(sourceAt(histogram.bins, bin, 1489),
            histogram.stride, x0, y0, x1, y1);
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

uint depthDrawIntegralRangeSum(Read<uint> integral, int stride, int x0, int y0, int x1, int y1) {
    auto topLeft = add32(mul32(y0, stride), x0);
    auto topRight = add32(mul32(y0, stride), x1);
    auto bottomLeft = add32(mul32(y1, stride), x0);
    auto bottomRight = add32(mul32(y1, stride), x1);
    const auto read = [&](int index) {
        const auto position = static_cast<size_t>(index);
        return sourceAt(integral, position, 1510);
    };
    const auto bottomRightValue = read(bottomRight);
    const auto topRightValue = read(topRight);
    const auto bottomLeftValue = read(bottomLeft);
    const auto topLeftValue = read(topLeft);
    return bottomRightValue - topRightValue - bottomLeftValue + topLeftValue;
}

ubyte depthDrawNeighborMedianValue(
    Read<ubyte> depth,
    Read<ubyte> alphaMask,
    Read<ubyte> pending,
    int width,
    int height,
    size_t index
) {
    auto x = toInt(index % width);
    auto y = toInt(index / width);
    Array<ubyte> values;
    for (auto offset : depthDrawEightNeighborOffsets) {
        auto sx = add32(x, sourceAt(offset, 0, 1525));
        auto sy = add32(y, sourceAt(offset, 1, 1526));
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = static_cast<size_t>(sy) * static_cast<size_t>(width) + static_cast<size_t>(sx);
        if (sourceAt(alphaMask, sampleIndex, 1529) && !sourceAt(pending, sampleIndex, 1529) &&
                sourceAt(depth, sampleIndex, 1529) > 0) {
            values.append(sourceAt(depth, sampleIndex, 1530));
        }
    }
    return values.size() ? depthDrawMedian(values) : 0;
}

void depthDrawEnqueuePendingNeighbors(
    Array<size_t>& frontier,
    Array<ubyte>& queued,
    Read<ubyte> pending,
    Read<ubyte> alphaMask,
    int width,
    int height,
    size_t index
) {
    auto x = toInt(index % width);
    auto y = toInt(index / width);
    for (auto offset : depthDrawEightNeighborOffsets) {
        auto sx = add32(x, sourceAt(offset, 0, 1548));
        auto sy = add32(y, sourceAt(offset, 1, 1549));
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = static_cast<size_t>(sy) * static_cast<size_t>(width) + static_cast<size_t>(sx);
        if (sourceAt(pending, sampleIndex, 1552) && sourceAt(alphaMask, sampleIndex, 1552) &&
                !sourceAt(queued, sampleIndex, 1552)) {
            sourceAt(queued, sampleIndex, 1553) = 1;
            frontier.append(sampleIndex);
        }
    }
}

ubyte depthDrawSampleFallbackMedian(
    Read<ubyte> depth,
    Read<ubyte> alphaMask,
    Read<ubyte> pending,
    int width,
    int height,
    int x,
    int y
) {
    for (auto radius : std::array{6, 12, 24, 48, 96}) {
        Array<ubyte> values;
        for (auto dy : dRange(sub32(0, radius), add32(radius, 1))) {
            for (auto dx : dRange(sub32(0, radius), add32(radius, 1))) {
                auto distanceSq = add32(mul32(dx, dx), mul32(dy, dy));
                if (distanceSq <= 0 || distanceSq > radius * radius) continue;
                auto sx = add32(x, dx);
                auto sy = add32(y, dy);
                if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
                auto sampleIndex = static_cast<size_t>(sy) * static_cast<size_t>(width) + static_cast<size_t>(sx);
                if (sourceAt(alphaMask, sampleIndex, 1578) && !sourceAt(pending, sampleIndex, 1578) &&
                        sourceAt(depth, sampleIndex, 1578) > 0) {
                    values.append(sourceAt(depth, sampleIndex, 1579));
                }
            }
        }
        if (values.size()) return depthDrawMedian(values);
    }
    return 0;
}

static void sortMedianBytes(Array<ubyte> values) {
    core::selector::detail::phobosQuickSort(
        std::span<ubyte>{values.data(), values.size()},
        [](ubyte left, ubyte right) { return left < right; }, values.size(), std::size_t{1024});
#ifndef NDEBUG
    for (std::size_t index = 1; index < values.size(); ++index) {
        if (values[index] < values[index - 1]) {
            throw compat::AssertError{
                "Failed to sort range of type ubyte[]", __FILE__, __LINE__};
        }
    }
#endif
}

ubyte depthDrawMedian(Array<ubyte> values) {
    sortMedianBytes(values);
    return sourceAt(values, values.size() / 2, 1590);
}

ubyte depthDrawCleanupMedian(Array<ubyte> values) {
    sortMedianBytes(values);
    return sourceAt(values, (values.size() - 1) >> 1, 1595);
}

double depthDrawPercentileFromSorted(Read<ubyte> sortedValues, double percentile) {
    if (!sortedValues.size()) return 0.0;
    auto index = max(0, min(sub32(toInt(sortedValues.size()), 1),
        toInt(std::llround(static_cast<double>(sortedValues.size() - 1) * percentile))));
    return sourceAt(sortedValues, index, 1602);
}

bool depthDrawIsMaskContourPixel(Read<ubyte> maskPixels, int width, int height, size_t index) {
    if (!sourceAt(maskPixels, index, 1606)) return false;
    auto x = toInt(index % width);
    auto y = toInt(index / width);
    for (auto offset : fourNeighbors) {
        auto sx = add32(x, sourceAt(offset, 0, 1610));
        auto sy = add32(y, sourceAt(offset, 1, 1611));
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) return true;
        if (!sourceAt(maskPixels, add32(mul32(sy, width), sx), 1613)) return true;
    }
    return false;
}

Array<ubyte> depthDrawExpandMaskFrontier(
    Read<ubyte> maskPixels,
    int width,
    int height,
    Read<ubyte> frontier,
    Array<ubyte>& bandMask
) {
    Array<ubyte> next;
    resize(next, maskPixels.size());
    for (auto index : dRange(0, frontier.size())) {
        if (!sourceAt(frontier, index, 1628)) continue;
        auto x = toInt(index % width);
        auto y = toInt(index / width);
        for (auto offset : depthDrawEightNeighborOffsets) {
            auto sx = add32(x, sourceAt(offset, 0, 1632));
            auto sy = add32(y, sourceAt(offset, 1, 1633));
            if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
            auto sampleIndex = static_cast<size_t>(sy) * static_cast<size_t>(width) + static_cast<size_t>(sx);
            if (!sourceAt(maskPixels, sampleIndex, 1636) || sourceAt(bandMask, sampleIndex, 1636)) continue;
            sourceAt(bandMask, sampleIndex, 1637) = 1;
            sourceAt(next, sampleIndex, 1638) = 1;
        }
    }
    return next;
}

Array<ubyte> depthDrawErodeBinaryMask(Read<ubyte> maskPixels, int width, int height) {
    Array<ubyte> eroded;
    resize(eroded, maskPixels.size());
    for (auto index : dRange(0, maskPixels.size())) {
        if (!sourceAt(maskPixels, index, 1648)) continue;
        auto x = toInt(index % width);
        auto y = toInt(index / width);
        bool keep = true;
        for (auto offset : depthDrawEightNeighborOffsets) {
            auto sx = add32(x, sourceAt(offset, 0, 1653));
            auto sy = add32(y, sourceAt(offset, 1, 1654));
            if (sx < 0 || sx >= width || sy < 0 || sy >= height ||
                    !sourceAt(maskPixels, add32(mul32(sy, width), sx), 1655)) {
                keep = false;
                break;
            }
        }
        if (keep) sourceAt(eroded, index, 1660) = 1;
    }
    return eroded;
}

bool depthDrawHasPositiveMaskedNeighbor(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    size_t index
) {
    auto x = toInt(index % width);
    auto y = toInt(index / width);
    for (auto offset : depthDrawEightNeighborOffsets) {
        auto sx = add32(x, sourceAt(offset, 0, 1675));
        auto sy = add32(y, sourceAt(offset, 1, 1676));
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = add32(mul32(sy, width), sx);
        if (sourceAt(maskPixels, sampleIndex, 1679) && sourceAt(depthPixels, sampleIndex, 1679) > 0)
            return true;
    }
    return false;
}

ubyte depthDrawSampleMaskedNeighborMedian(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    size_t index
) {
    auto x = toInt(index % width);
    auto y = toInt(index / width);
    Array<ubyte> values;
    for (auto offset : depthDrawEightNeighborOffsets) {
        auto sx = add32(x, sourceAt(offset, 0, 1695));
        auto sy = add32(y, sourceAt(offset, 1, 1696));
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = add32(mul32(sy, width), sx);
        if (!sourceAt(maskPixels, sampleIndex, 1699) || sourceAt(depthPixels, sampleIndex, 1699) <= 0)
            continue;
        values.append(sourceAt(depthPixels, sampleIndex, 1700));
    }
    return values.size() ? depthDrawCleanupMedian(values) : 0;
}

ubyte depthDrawSampleMaskedMultiscaleDepth(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
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
        return static_cast<ubyte>(min(255, max(0, toInt(std::llround(weightedDepth / totalWeight)))));
    }
    return depthDrawSampleMaskedNeighborMedian(depthPixels, maskPixels, width, height, index);
}

DepthDrawMaskedDepthEstimate depthDrawEstimateMaskedDepthAtScale(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    size_t index,
    int radius,
    int minSamples,
    double weight
) {
    auto x0 = toInt(index % width);
    auto y0 = toInt(index / width);
    auto sampled = depthDrawSampleMaskedSparseGridDepths(depthPixels, maskPixels, width, height, x0, y0, radius);
    DepthDrawMaskedDepthEstimate result;
    result.weight = weight;
    if (sampled.values.size() < static_cast<size_t>(minSamples)) return result;

    auto sortedValues = duplicate(sampled.values);
    sortMedianBytes(sortedValues);
    double centerMean = 0.0;
    for (auto values = sampled.values; auto value : values) centerMean += value;
    centerMean /= sampled.values.size();
    for (auto grid = sampled.grid; auto& value : grid) {
        if (value <= 0.0) value = centerMean;
    }

    auto planeDepth = depthDrawEstimateGridPlaneDepth(sampled.grid);
    auto medianDepth = depthDrawCleanupMedian(duplicate(sampled.values));
    auto lo = depthDrawPercentileFromSorted(sortedValues, 0.2);
    auto hi = depthDrawPercentileFromSorted(sortedValues, 0.8);
    auto robustDepth = max(lo, min(hi, std::round(planeDepth * 0.7 + medianDepth * 0.3)));
    result.valid = true;
    result.depth = robustDepth;
    result.confidence = max(0.0, min(1.0, static_cast<double>(sampled.values.size()) / 9.0));
    return result;
}

DepthDrawMaskedSparseGridDepths depthDrawSampleMaskedSparseGridDepths(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    int x0,
    int y0,
    int radius
) {
    DepthDrawMaskedSparseGridDepths result;
    result.grid = ::nijigenerate::io::depthimage::detail::initialPrimitiveArray<double>(
        9, std::numeric_limits<double>::quiet_NaN(), 8, 970, 1000);
    auto searchRadius = max(1, toInt(std::floor(static_cast<double>(radius) / 3.0)));
    size_t cursor{};
    for (auto gy : dRange(-1, 2)) {
        for (auto gx : dRange(-1, 2)) {
            auto targetX = max(0, min(sub32(width, 1), toInt(std::llround(add32(x0, mul32(gx, radius))))));
            auto targetY = max(0, min(sub32(height, 1), toInt(std::llround(add32(y0, mul32(gy, radius))))));
            auto sampledDepth = depthDrawSampleNearestMaskedDepth(
                depthPixels, maskPixels, width, height, targetX, targetY, searchRadius);
            sourceAt(result.grid, cursor++, 1807) = sampledDepth;
            if (sampledDepth > 0) {
                try {
                    result.values.append(sampledDepth);
                } catch (const compat::OutOfMemoryError&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
                }
            }
        }
    }
    return result;
}

ubyte depthDrawSampleNearestMaskedDepth(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    int targetX,
    int targetY,
    int searchRadius
) {
    ubyte bestDepth{};
    auto bestDistanceSq = std::numeric_limits<int>::max();
    for (auto dy : dRange(sub32(0, searchRadius), add32(searchRadius, 1))) {
        auto y = add32(targetY, dy);
        if (y < 0 || y >= height) continue;
        for (auto dx : dRange(sub32(0, searchRadius), add32(searchRadius, 1))) {
            auto x = add32(targetX, dx);
            if (x < 0 || x >= width) continue;
            auto index = add32(mul32(y, width), x);
            auto depth = sourceAt(depthPixels, index, 1832);
            if (!sourceAt(maskPixels, index, 1833) || depth <= 0) continue;
            auto distanceSq = add32(mul32(dx, dx), mul32(dy, dy));
            if (distanceSq < bestDistanceSq) {
                bestDistanceSq = distanceSq;
                bestDepth = depth;
            }
        }
    }
    return bestDepth;
}

double depthDrawEstimateGridPlaneDepth(Read<double> grid) {
    auto tl = sourceAt(grid, 0, 1845);
    auto tc = sourceAt(grid, 1, 1846);
    auto tr = sourceAt(grid, 2, 1847);
    auto ml = sourceAt(grid, 3, 1848);
    auto mc = sourceAt(grid, 4, 1849);
    auto mr = sourceAt(grid, 5, 1850);
    auto bl = sourceAt(grid, 6, 1851);
    auto bc = sourceAt(grid, 7, 1852);
    auto br = sourceAt(grid, 8, 1853);
    auto gradX = (tr + 2 * mr + br) - (tl + 2 * ml + bl);
    auto gradY = (bl + 2 * bc + br) - (tl + 2 * tc + tr);
    return mc + (gradX + gradY) * 0.125;
}

void depthDrawEnqueueMaskedGapNeighbors(
    Array<size_t>& queue,
    Array<ubyte>& queued,
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    size_t index
) {
    auto x = toInt(index % width);
    auto y = toInt(index / width);
    for (auto offset : depthDrawEightNeighborOffsets) {
        auto sx = add32(x, sourceAt(offset, 0, 1871));
        auto sy = add32(y, sourceAt(offset, 1, 1872));
        if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
        auto sampleIndex = add32(mul32(sy, width), sx);
        if (!sourceAt(maskPixels, sampleIndex, 1875) || sourceAt(depthPixels, sampleIndex, 1875) > 0 ||
                sourceAt(queued, sampleIndex, 1875)) continue;
        queue.append(sampleIndex);
        sourceAt(queued, sampleIndex, 1877) = 1;
    }
}

void depthDrawPruneThinForeignSeedComponents(
    Array<ubyte>& depthPixels,
    Array<ubyte>& debugState,
    Array<ubyte>& debugScore,
    DepthDrawPruneLayer layer,
    int layerIndex,
    int imageWidth,
    int imageHeight,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap,
    Read<ubyte> maskPixels,
    ubyte globalMedian,
    double threshold
) {
    auto totalPixels = static_cast<size_t>(mul32(layer.width, layer.height));
    Array<ubyte> visited;
    Array<int> componentIds;
    resize(visited, totalPixels);
    resize(componentIds, totalPixels);
    for (auto ids = componentIds; auto& id : ids) id = -1;
    auto contourMask = ngDepthDrawBuildLayerContourBandMask(maskPixels, layer.width, layer.height, 1);
    auto wideContourMask = ngDepthDrawBuildLayerContourBandMask(maskPixels, layer.width, layer.height, 4);
    auto linkThreshold = max(8.0, threshold * 0.35);
    Array<size_t> queue;
    int componentId{};

    for (auto start : dRange(0, totalPixels)) {
        if (compat::ngArrayAt(visited, start) || !compat::ngArrayAt(maskPixels, start) ||
            compat::ngArrayAt(depthPixels, start) == 0) continue;

        size_t head{};
        resize(queue, 0);
        queue.append(start);
        compat::ngArrayAt(visited, start) = 1;
        compat::ngArrayAt(componentIds, start) = componentId;
        Array<size_t> indices;
        Array<ubyte> values;
        auto minX = layer.width;
        int maxX{};
        auto minY = layer.height;
        int maxY{};
        size_t contourHits{};
        size_t wideContourHits{};

        while (head < queue.size()) {
            auto index = compat::ngArrayAt(queue, head++);
            indices.append(index);
            values.append(compat::ngArrayAt(depthPixels, index));
            auto x = toInt(index % layer.width);
            auto y = toInt(index / layer.width);
            minX = min(minX, x);
            maxX = max(maxX, x);
            minY = min(minY, y);
            maxY = max(maxY, y);
            if (compat::ngArrayAt(contourMask, index)) contourHits += 1;
            if (compat::ngArrayAt(wideContourMask, index)) wideContourHits += 1;

            for (auto offset : depthDrawEightNeighborOffsets) {
                auto sx = add32(x, offset[0]);
                auto sy = add32(y, offset[1]);
                if (sx < 0 || sx >= layer.width || sy < 0 || sy >= layer.height) continue;
                auto sampleIndex = static_cast<size_t>(sy) * static_cast<size_t>(layer.width) + static_cast<size_t>(sx);
                if (compat::ngArrayAt(visited, sampleIndex) || !compat::ngArrayAt(maskPixels, sampleIndex) ||
                    compat::ngArrayAt(depthPixels, sampleIndex) == 0) continue;
                const auto neighborDepth = static_cast<double>(compat::ngArrayAt(depthPixels, sampleIndex));
                const auto currentDepth = static_cast<double>(compat::ngArrayAt(depthPixels, index));
                if (std::abs(neighborDepth - currentDepth) > linkThreshold) continue;
                compat::ngArrayAt(visited, sampleIndex) = 1;
                compat::ngArrayAt(componentIds, sampleIndex) = componentId;
                queue.append(sampleIndex);
            }
        }

        auto width = add32(sub32(maxX, minX), 1);
        auto height = add32(sub32(maxY, minY), 1);
        auto componentMedian = depthDrawCleanupMedian(duplicate(values));
        auto sameLayerSupport = depthDrawCollectComponentExternalDepthSupport(
            depthPixels, maskPixels, componentIds, componentId, wideContourMask, layer.width, layer.height,
            minX, minY, maxX, maxY, 5);
        auto foreignSupport = depthDrawCollectComponentForeignVisibleDepthSupport(
            indices, layer, imageWidth, imageHeight, stableDepthPixels, visibleLayerMap, layerIndex);
        auto contourRatio = static_cast<double>(contourHits) / static_cast<double>(indices.size());
        auto wideContourRatio = static_cast<double>(wideContourHits) / static_cast<double>(indices.size());
        auto sameMedian = sameLayerSupport.size() ? depthDrawCleanupMedian(duplicate(sameLayerSupport)) : globalMedian;
        auto foreignMedian = foreignSupport.size() ? depthDrawCleanupMedian(duplicate(foreignSupport)) : componentMedian;
        auto sameDistance = std::abs(static_cast<double>(componentMedian) - static_cast<double>(sameMedian));
        auto foreignDistance = std::abs(static_cast<double>(componentMedian) - static_cast<double>(foreignMedian));
        auto isThin = min(width, height) <= 3 || indices.size() <= static_cast<size_t>(mul32(max(width, height), 2));
        auto isSmallish = indices.size() <= static_cast<size_t>(max(48.0, threshold * 8.0));
        auto localThreshold = max(2.0, min(6.0, threshold * 0.1));
        auto componentScore = sameDistance - foreignDistance;

        if (foreignSupport.size() >= 4 &&
            (sameDistance > localThreshold || (sameLayerSupport.size() < 4 && wideContourRatio > 0.7)) &&
            foreignDistance < sameDistance &&
            (contourRatio >= 0.35 || wideContourRatio >= 0.7) &&
            (isThin || isSmallish)) {
            for (auto componentIndices = indices; auto index : componentIndices) {
                compat::ngArrayAt(depthPixels, index) = 0;
                compat::ngArrayAt(debugState, index) = 4;
                const auto existingScore = compat::ngArrayAt(debugScore, index);
                const auto score = static_cast<ubyte>(min(255, max(0, toInt(std::llround(componentScore * 24.0)))));
                compat::ngArrayAt(debugScore, index) = max(existingScore, score);
            }
        }

        componentId = add32(componentId, 1);
    }
}

Array<ubyte> depthDrawCollectComponentExternalDepthSupport(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    Read<int> componentIds,
    int componentId,
    Read<ubyte> contourMask,
    int width,
    int height,
    int minX,
    int minY,
    int maxX,
    int maxY,
    int radius
) {
    Array<ubyte> values;
    auto startX = max(0, sub32(minX, radius));
    auto startY = max(0, sub32(minY, radius));
    auto endX = min(sub32(width, 1), add32(maxX, radius));
    auto endY = min(sub32(height, 1), add32(maxY, radius));

    for (auto y : dRange(startY, add32(endY, 1))) {
        for (auto x : dRange(startX, add32(endX, 1))) {
            auto index = add32(mul32(y, width), x);
            if (!compat::ngArrayAt(maskPixels, index) || compat::ngArrayAt(depthPixels, index) == 0 ||
                compat::ngArrayAt(componentIds, index) == componentId || compat::ngArrayAt(contourMask, index)) {
                continue;
            }
            values.append(compat::ngArrayAt(depthPixels, index));
        }
    }
    return values;
}

Array<ubyte> depthDrawCollectComponentForeignVisibleDepthSupport(
    Read<size_t> indices,
    DepthDrawPruneLayer layer,
    int imageWidth,
    int imageHeight,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap,
    int layerIndex
) {
    Array<ubyte> values;
    for (auto index : indices) {
        auto x = toInt(index % layer.width);
        auto y = toInt(index / layer.width);
        auto globalX = add32(layer.left, x);
        auto globalY = add32(layer.top, y);
        if (globalX < 0 || globalX >= imageWidth || globalY < 0 || globalY >= imageHeight) continue;

        for (auto dy : dRange(-2, 3)) {
            auto sy = add32(globalY, dy);
            if (sy < 0 || sy >= imageHeight) continue;
            for (auto dx : dRange(-2, 3)) {
                auto sx = add32(globalX, dx);
                if (sx < 0 || sx >= imageWidth) continue;
                auto globalIndex = add32(mul32(sy, imageWidth), sx);
                auto visibleLayer = compat::ngArrayAt(visibleLayerMap, globalIndex);
                if (visibleLayer < 0 || visibleLayer == layerIndex) continue;
                values.append(compat::ngArrayAt(stableDepthPixels, globalIndex));
            }
        }
    }
    return values;
}

Array<ubyte> depthDrawCollectLocalDepthSupport(
    Read<ubyte> depthPixels,
    Read<ubyte> maskPixels,
    int width,
    int height,
    int centerX,
    int centerY,
    int radius
) {
    Array<ubyte> values;
    for (auto dy : dRange(sub32(0, radius), add32(radius, 1))) {
        auto sy = add32(centerY, dy);
        if (sy < 0 || sy >= height) continue;
        for (auto dx : dRange(sub32(0, radius), add32(radius, 1))) {
            auto sx = add32(centerX, dx);
            if (sx < 0 || sx >= width || (dx == 0 && dy == 0)) continue;
            auto sampleIndex = add32(mul32(sy, width), sx);
            auto sampleDepth = compat::ngArrayAt(depthPixels, sampleIndex);
            if (!compat::ngArrayAt(maskPixels, sampleIndex) || sampleDepth == 0) continue;
            values.append(sampleDepth);
        }
    }
    return values;
}

Array<ubyte> depthDrawCollectForeignVisibleDepthSupport(
    int imageWidth,
    int imageHeight,
    int centerX,
    int centerY,
    int radius,
    int layerIndex,
    Read<ubyte> stableDepthPixels,
    Read<int> visibleLayerMap
) {
    Array<ubyte> values;
    for (auto dy : dRange(sub32(0, radius), add32(radius, 1))) {
        auto sy = add32(centerY, dy);
        if (sy < 0 || sy >= imageHeight) continue;
        for (auto dx : dRange(sub32(0, radius), add32(radius, 1))) {
            auto sx = add32(centerX, dx);
            if (sx < 0 || sx >= imageWidth || (dx == 0 && dy == 0)) continue;
            auto globalIndex = add32(mul32(sy, imageWidth), sx);
            auto visibleLayer = compat::ngArrayAt(visibleLayerMap, globalIndex);
            if (visibleLayer < 0 || visibleLayer == layerIndex) continue;
            values.append(compat::ngArrayAt(stableDepthPixels, globalIndex));
        }
    }
    return values;
}

Array<ubyte> depthDrawCollectPositiveValues(Read<ubyte> values) {
    Array<ubyte> positive;
    for (auto value : values) {
        if (value > 0) positive.append(value);
    }
    return positive;
}

compat::ConstSlice<ubyte> depthDrawSplitLayerMask(const DepthDrawSplitLayer layer) {
    return compat::ConstSlice<ubyte>(layer.maskPixels.size() ? layer.maskPixels : layer.alphaMask);
}

Array<double> depthDrawGaussianKernel(int radius) {
    auto size = add32(mul32(radius, 2), 1);
    auto kernel = ::nijigenerate::io::depthimage::detail::initialPrimitiveArray<double>(
        static_cast<size_t>(mul32(size, size)), std::numeric_limits<double>::quiet_NaN(), 8, 970, 1000);
    auto sigma = max(1.0, static_cast<double>(radius) * 0.5);
    size_t index = 0;
    for (auto y : dRange(sub32(0, radius), add32(radius, 1))) {
        for (auto x : dRange(sub32(0, radius), add32(radius, 1))) {
            auto distance2 = add32(mul32(x, x), mul32(y, y));
            auto& destination = sourceAt(kernel, index++, 2124);
            destination = std::exp(-static_cast<double>(distance2) / (2.0 * sigma * sigma));
        }
    }
    return kernel;
}

int depthDrawSampleGridPosition(int index, int count, int extent) {
    if (count <= 1 || extent <= 1) return 0;
    return toInt(std::llround((static_cast<double>(index) / static_cast<double>(count - 1)) * static_cast<double>(extent - 1)));
}

DepthDrawConvolutionSample depthDrawConvolveDepthAt(
    Read<ubyte> sourcePixels,
    int width,
    int height,
    int centerX,
    int centerY,
    int radius,
    Read<double> kernel
) {
    double weightedSum = 0.0;
    double weightTotal = 0.0;
    for (auto oy : dRange(sub32(0, radius), add32(radius, 1))) {
        auto y = add32(centerY, oy);
        if (y < 0 || y >= height) continue;
        for (auto ox : dRange(sub32(0, radius), add32(radius, 1))) {
            auto x = add32(centerX, ox);
            if (x < 0 || x >= width) continue;
            const auto pixelIndex = static_cast<size_t>(add32(mul32(y, width), x));
            auto value = sourceAt(sourcePixels, pixelIndex, 2152);
            if (value <= 0) continue;
            auto kernelIndex = add32(mul32(add32(oy, radius), add32(mul32(radius, 2), 1)), add32(ox, radius));
            auto weight = sourceAt(kernel, static_cast<size_t>(kernelIndex), 2155);
            weightedSum += static_cast<double>(value) * weight;
            weightTotal += weight;
        }
    }
    if (weightTotal == 0.0) return DepthDrawConvolutionSample(0.0, false);
    return DepthDrawConvolutionSample(weightedSum / weightTotal, true);
}

double depthDrawSampleLinearGrid(
    Read<double> values,
    Read<ubyte> valid,
    int gridWidth,
    int gridHeight,
    double fx,
    double fy
) {
    auto x0 = toInt(std::floor(fx));
    auto y0 = toInt(std::floor(fy));
    auto x1 = min(sub32(gridWidth, 1), add32(x0, 1));
    auto y1 = min(sub32(gridHeight, 1), add32(y0, 1));
    auto tx = fx - x0;
    auto ty = fy - y0;

    double weightedSum = 0.0;
    double weightTotal = 0.0;
    auto add = [&](int x, int y, double weight) {
        if (weight == 0.0) return;
        auto index = add32(mul32(y, gridWidth), x);
        if (!sourceAt(valid, static_cast<size_t>(index), 2184)) return;
        weightedSum += sourceAt(values, static_cast<size_t>(index), 2185) * weight;
        weightTotal += weight;
    };
    add(x0, y0, (1.0 - tx) * (1.0 - ty));
    add(x1, y0, tx * (1.0 - ty));
    add(x0, y1, (1.0 - tx) * ty);
    add(x1, y1, tx * ty);
    return weightTotal == 0.0 ? 0.0 : weightedSum / weightTotal;
}

double depthDrawSampleCubicGrid(
    Read<double> values,
    Read<ubyte> valid,
    int gridWidth,
    int gridHeight,
    double fx,
    double fy
) {
    auto baseX = toInt(std::floor(fx));
    auto baseY = toInt(std::floor(fy));
    auto tx = fx - baseX;
    auto ty = fy - baseY;

    double weightedSum = 0.0;
    double weightTotal = 0.0;
    for (auto oy : dRange(-1, 3)) {
        auto sy = depthDrawClampInt(add32(baseY, oy), 0, sub32(gridHeight, 1));
        auto wy = depthDrawCatmullRomWeight(static_cast<double>(oy) - ty);
        for (auto ox : dRange(-1, 3)) {
            auto sx = depthDrawClampInt(add32(baseX, ox), 0, sub32(gridWidth, 1));
            auto wx = depthDrawCatmullRomWeight(static_cast<double>(ox) - tx);
            auto weight = wx * wy;
            if (weight == 0.0) continue;
            auto index = add32(mul32(sy, gridWidth), sx);
            if (!sourceAt(valid, static_cast<size_t>(index), 2219)) continue;
            weightedSum += sourceAt(values, static_cast<size_t>(index), 2220) * weight;
            weightTotal += weight;
        }
    }

    return weightTotal == 0.0 ? 0.0 : weightedSum / weightTotal;
}

double depthDrawCatmullRomWeight(double x) {
    auto ax = std::abs(x);
    if (ax <= 1.0) {
        return 1.5 * ax * ax * ax - 2.5 * ax * ax + 1.0;
    }
    if (ax < 2.0) {
        return -0.5 * ax * ax * ax + 2.5 * ax * ax - 4.0 * ax + 2.0;
    }
    return 0.0;
}

int depthDrawClampInt(int value, int minValue, int maxValue) {
    return max(minValue, min(maxValue, value));
}
} // namespace completion

std::vector<float> ngDepthDrawSmoothGridDepthValues(std::span<const float> sourceDepths,
    std::span<const std::uint8_t> vertexValid, std::span<const int> vertexGroup,
    int cols, int rows, float depthThreshold, float depthScale) { return completion::toVector(completion::ngDepthDrawSmoothGridDepthValues(sourceDepths, vertexValid, vertexGroup, cols, rows, depthThreshold, depthScale)); }

compat::Slice<float> ngDepthDrawSmoothGridDepthValuesSource(compat::ConstSlice<float> sourceDepths,
    compat::ConstSlice<std::uint8_t> vertexValid, compat::ConstSlice<int> vertexGroup,
    int cols, int rows, float depthThreshold, float depthScale) {
    return completion::ngDepthDrawSmoothGridDepthValues(sourceDepths.view(), vertexValid.view(),
        vertexGroup.view(), cols, rows, depthThreshold, depthScale);
}

DepthDrawAlphaDepthGapDetection ngDepthDrawDetectAlphaDepthGaps(std::span<const std::uint8_t> depth,
    std::span<const std::uint8_t> alphaMask, int width, int height, int layerIndex,
    std::span<const DepthDrawAlphaDepthFocusedRule> focusedRules) { return completion::ngDepthDrawDetectAlphaDepthGaps(depth, alphaMask, width, height, layerIndex, focusedRules); }

DepthDrawAlphaDepthGapDetection ngDepthDrawDetectAlphaDepthGapsSource(
    compat::ConstSlice<std::uint8_t> depth, compat::ConstSlice<std::uint8_t> alphaMask,
    int width, int height, int layerIndex, compat::ConstSlice<DepthDrawAlphaDepthFocusedRule> focusedRules) {
    return completion::ngDepthDrawDetectAlphaDepthGaps(depth.view(), alphaMask.view(), width, height,
        layerIndex, focusedRules.view());
}

DepthDrawAlphaDepthGapFillResult ngDepthDrawMedianFillDepth(std::span<const std::uint8_t> sourceDepth,
    std::span<const std::uint8_t> alphaMask, std::span<const std::uint8_t> fillMask, int width, int height) { return completion::ngDepthDrawMedianFillDepth(sourceDepth, alphaMask, fillMask, width, height); }

std::vector<std::uint8_t> ngDepthDrawBuildLayerContourBandMask(
    std::span<const std::uint8_t> maskPixels, int width, int height, int thickness) { return completion::toVector(completion::ngDepthDrawBuildLayerContourBandMask(maskPixels, width, height, thickness)); }

compat::Slice<std::uint8_t> ngDepthDrawBuildLayerContourBandMaskSource(
    compat::ConstSlice<std::uint8_t> maskPixels, int width, int height, int thickness) {
    return completion::ngDepthDrawBuildLayerContourBandMask(
        std::span<const std::uint8_t>{maskPixels.data(), maskPixels.size()}, width, height, thickness);
}

std::vector<std::uint8_t> ngDepthDrawErodePositiveDepthMask(
    std::span<const std::uint8_t> depthPixels, int width, int height, int thickness) { return completion::toVector(completion::ngDepthDrawErodePositiveDepthMask(depthPixels, width, height, thickness)); }

compat::Slice<std::uint8_t> ngDepthDrawErodePositiveDepthMaskSource(
    compat::ConstSlice<std::uint8_t> depthPixels, int width, int height, int thickness) {
    return completion::ngDepthDrawErodePositiveDepthMask(depthPixels.view(), width, height, thickness);
}

DepthDrawDepthInpaintResult ngDepthDrawInpaintMaskedLayerDepth(std::span<const std::uint8_t> sourceDepthPixels,
    std::span<const std::uint8_t> maskPixels, int width, int height) { return completion::ngDepthDrawInpaintMaskedLayerDepth(sourceDepthPixels, maskPixels, width, height); }

DepthDrawDepthInpaintResult ngDepthDrawInpaintMaskedLayerDepthSource(
    compat::ConstSlice<std::uint8_t> sourceDepthPixels, compat::ConstSlice<std::uint8_t> maskPixels,
    int width, int height) {
    return completion::ngDepthDrawInpaintMaskedLayerDepth(sourceDepthPixels.view(), maskPixels.view(),
        width, height);
}

std::vector<std::uint8_t> ngDepthDrawSmoothMaskedPositiveDepth(std::span<const std::uint8_t> sourceDepthPixels,
    std::span<const std::uint8_t> maskPixels, int width, int height) { return completion::toVector(completion::ngDepthDrawSmoothMaskedPositiveDepth(sourceDepthPixels, maskPixels, width, height)); }

compat::Slice<std::uint8_t> ngDepthDrawSmoothMaskedPositiveDepthSource(
    compat::ConstSlice<std::uint8_t> sourceDepthPixels, compat::ConstSlice<std::uint8_t> maskPixels,
    int width, int height) {
    return completion::ngDepthDrawSmoothMaskedPositiveDepth(sourceDepthPixels.view(), maskPixels.view(),
        width, height);
}

DepthDrawDepthPruneResult ngDepthDrawPruneForeignDepthSeeds(std::span<const std::uint8_t> sourceDepthPixels,
    DepthDrawPruneLayer layer, int layerIndex, int imageWidth, int imageHeight,
    std::span<const std::uint8_t> stableDepthPixels, std::span<const int> visibleLayerMap,
    std::span<const std::uint8_t> maskPixels, double threshold) { return completion::ngDepthDrawPruneForeignDepthSeeds(sourceDepthPixels, layer, layerIndex, imageWidth, imageHeight, stableDepthPixels, visibleLayerMap, maskPixels, threshold); }

std::vector<int> ngDepthDrawBuildVisibleLayerMap(
    int imageWidth, int imageHeight, std::span<const DepthDrawSplitLayer> layers) { return completion::toVector(completion::ngDepthDrawBuildVisibleLayerMap(imageWidth, imageHeight, layers)); }

compat::Slice<int> ngDepthDrawBuildVisibleLayerMapSource(
    int imageWidth, int imageHeight, compat::ConstSlice<DepthDrawSplitLayer> layers) {
    return completion::ngDepthDrawBuildVisibleLayerMap(imageWidth, imageHeight, layers.view());
}

std::vector<std::uint8_t> ngDepthDrawSeedLayerDepthPixels(DepthDrawSplitLayer layer,
    int layerIndex, int imageWidth, int imageHeight, std::span<const std::uint8_t> stableDepthPixels,
    std::span<const int> visibleLayerMap, std::span<const std::uint8_t> maskPixels,
    std::span<const DepthDrawSplitLayer> layers, std::span<const std::uint8_t> contourBandMask,
    int upperMaskRadius) { return completion::toVector(completion::ngDepthDrawSeedLayerDepthPixels(layer, layerIndex, imageWidth, imageHeight, stableDepthPixels, visibleLayerMap, maskPixels, layers, contourBandMask, upperMaskRadius)); }

compat::Slice<std::uint8_t> ngDepthDrawSeedLayerDepthPixelsSource(DepthDrawSplitLayer layer,
    int layerIndex, int imageWidth, int imageHeight, compat::ConstSlice<std::uint8_t> stableDepthPixels,
    compat::ConstSlice<int> visibleLayerMap, compat::ConstSlice<std::uint8_t> maskPixels,
    compat::ConstSlice<DepthDrawSplitLayer> layers, compat::ConstSlice<std::uint8_t> contourBandMask,
    int upperMaskRadius) {
    return completion::ngDepthDrawSeedLayerDepthPixels(layer, layerIndex, imageWidth, imageHeight,
        stableDepthPixels.view(), visibleLayerMap.view(), maskPixels.view(), layers.view(),
        contourBandMask.view(), upperMaskRadius);
}

bool ngDepthDrawHasUpperLayerMaskNearby(std::span<const DepthDrawSplitLayer> layers,
    int layerIndex, int globalX, int globalY, int radius) { return completion::ngDepthDrawHasUpperLayerMaskNearby(layers, layerIndex, globalX, globalY, radius); }

bool ngDepthDrawHasUpperLayerMaskNearbySource(compat::ConstSlice<DepthDrawSplitLayer> layers,
    int layerIndex, int globalX, int globalY, int radius) {
    return completion::ngDepthDrawHasUpperLayerMaskNearby(layers.view(), layerIndex, globalX, globalY, radius);
}

std::vector<std::uint8_t> ngDepthDrawCreateMaskedGridDepthPixels(int width, int height,
    std::span<const std::uint8_t> sourcePixels, std::span<const std::uint8_t> maskPixels,
    std::string_view specMode, int gridX, int gridY, int kernelSize, std::string_view interpMode) { return completion::toVector(completion::ngDepthDrawCreateMaskedGridDepthPixels(width, height, sourcePixels, maskPixels, specMode, gridX, gridY, kernelSize, interpMode)); }

compat::Slice<std::uint8_t> ngDepthDrawCreateMaskedGridDepthPixelsSource(int width, int height,
    compat::ConstSlice<std::uint8_t> sourcePixels, compat::ConstSlice<std::uint8_t> maskPixels,
    compat::RetainedString specMode, int gridX, int gridY, int kernelSize, compat::RetainedString interpMode) {
    return completion::ngDepthDrawCreateMaskedGridDepthPixels(width, height, sourcePixels.view(),
        maskPixels.view(), specMode.view(), gridX, gridY, kernelSize, interpMode.view());
}

bool ngDepthDrawHistogramIntegralDimensionsSupported(int width, int height) { return completion::ngDepthDrawHistogramIntegralDimensionsSupported(width, height); }
} // namespace nijigenerate::io::depthimage
