#include "packer.hpp"
#include <common/object_key_policy.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <string>
#include <algorithm>
#include <bit>
#include <limits>
#include <nijigenerate/compat/native_arithmetic_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <stdexcept>
#include <source_location>

namespace nijigenerate::atlas::packer {
namespace {
void prepareTexturePackerClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(TexturePacker),
            nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("nijigenerate.atlas.packer.TexturePacker"));
        return true;
    }();
    (void)registered;
}
}
// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0;
// retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
nicxlive::core::d_runtime::RetainedString TexturePacker::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t TexturePacker::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool TexturePacker::opEquals(std::shared_ptr<TexturePacker> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int TexturePacker::opCmp(std::shared_ptr<TexturePacker>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
void TexturePacker::prepareSourceClassInfo() { prepareTexturePackerClassInfo(); }
bool ngTexturePackerObjectEquals(std::shared_ptr<TexturePacker> left, std::shared_ptr<TexturePacker> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right),
        &prepareTexturePackerClassInfo);
}
namespace detail {
namespace {
std::int32_t add(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t sub(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
std::int32_t mul(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
std::int32_t abs(std::int32_t value) { return value >= 0 ? value : sub(0, value); }
compat::Slice<Vec4i> removeAt(compat::Slice<Vec4i> values, std::size_t index) {
    for (std::size_t i = index; i + 1 < values.size(); ++i) values[i] = values[i + 1];
    return values.slice(0, values.size() - 1);
}
}
bool contains(Vec4i a, Vec4i b) {
    return a.x >= b.x && a.y >= b.y && add(a.x, a.z) <= add(b.x, b.z) && add(a.y, a.w) <= add(b.y, b.w);
}
Vec4i Bin::scoreRect(Vec2i requested, std::int32_t& score1, std::int32_t& score2) {
    score1 = std::numeric_limits<std::int32_t>::max();
    score2 = std::numeric_limits<std::int32_t>::max();
    auto newNode = findNewNodeFit(requested, score1, score2);
    if (newNode.w == 0) {
        score1 = std::numeric_limits<std::int32_t>::max();
        score2 = std::numeric_limits<std::int32_t>::max();
    }
    return newNode;
}
Vec4i Bin::scoreRect(Vec2i requested) {
    const auto score1 = std::numeric_limits<std::int32_t>::max();
    const auto score2 = std::numeric_limits<std::int32_t>::max();
    return findNewNodeFit(requested, score1, score2);
}
void Bin::place(Vec4i& newNode) {
    auto rectanglesToProcess = freeRectangles.size();
    for (std::int32_t i = 0; static_cast<std::size_t>(i) < rectanglesToProcess; i = add(i, 1)) {
        if (splitFree(freeRectangles[i], newNode)) {
            // The D caller discards remove's shorter returned slice. Preserve that behavior.
            removeAt(freeRectangles, i);
            i = sub(i, 1);
            --rectanglesToProcess;
        }
    }
    prune();
    usedRectangles.append(newNode);
}
Vec4i Bin::findNewNodeFit(Vec2i requested, std::int32_t, std::int32_t) {
    Vec4i bestNode;
    auto bestShortFit = std::numeric_limits<std::int32_t>::max();
    auto bestLongFit = std::numeric_limits<std::int32_t>::max();
    for (const auto freeRect : freeRectangles) {
        if (freeRect.z >= requested.x && freeRect.w >= requested.y) {
            const auto leftoverH = abs(sub(freeRect.z, requested.x));
            const auto leftoverV = abs(sub(freeRect.w, requested.y));
            const auto shortSideFit = std::min(leftoverH, leftoverV);
            const auto longSideFit = std::max(leftoverH, leftoverV);
            if (shortSideFit < bestShortFit || (shortSideFit == bestShortFit && longSideFit < bestLongFit)) {
                bestNode = {freeRect.x, freeRect.y, requested.x, requested.y};
                bestShortFit = shortSideFit;
                bestLongFit = longSideFit;
            }
        }
    }
    return bestNode;
}
bool Bin::splitFree(Vec4i freeNode, Vec4i& usedNode) {
    if (usedNode.x >= add(freeNode.x, freeNode.z) || add(usedNode.x, usedNode.z) <= freeNode.x ||
        usedNode.y >= add(freeNode.y, freeNode.w) || add(usedNode.y, usedNode.w) <= freeNode.y) return false;
    if (usedNode.x < add(freeNode.x, freeNode.z) && add(usedNode.x, usedNode.z) > freeNode.x) {
        if (usedNode.y > freeNode.y && usedNode.y < add(freeNode.y, freeNode.w)) {
            auto newNode = freeNode;
            newNode.w = sub(usedNode.y, newNode.y);
            freeRectangles.append(newNode);
        }
        if (add(usedNode.y, usedNode.w) < add(freeNode.y, freeNode.w)) {
            auto newNode = freeNode;
            newNode.y = add(usedNode.y, usedNode.w);
            newNode.w = sub(add(freeNode.y, freeNode.w), add(usedNode.y, usedNode.w));
            freeRectangles.append(newNode);
        }
    }
    if (usedNode.y < add(freeNode.y, freeNode.w) && add(usedNode.y, usedNode.w) > freeNode.y) {
        if (usedNode.x > freeNode.x && usedNode.x < add(freeNode.x, freeNode.z)) {
            auto newNode = freeNode;
            newNode.z = sub(usedNode.x, newNode.x);
            freeRectangles.append(newNode);
        }
        if (add(usedNode.x, usedNode.z) < add(freeNode.x, freeNode.z)) {
            auto newNode = freeNode;
            newNode.x = add(usedNode.x, usedNode.z);
            newNode.z = sub(add(freeNode.x, freeNode.z), add(usedNode.x, usedNode.z));
            freeRectangles.append(newNode);
        }
    }
    return true;
}
void Bin::prune() {
    for (std::int32_t i = 0; static_cast<std::size_t>(i) < freeRectangles.size(); i = add(i, 1)) {
        for (std::int32_t j = add(i, 1); static_cast<std::size_t>(j) < freeRectangles.size(); j = add(j, 1)) {
            if (contains(freeRectangles[i], freeRectangles[j])) {
                freeRectangles = removeAt(freeRectangles, i);
                i = sub(i, 1);
                break;
            }
            if (contains(freeRectangles[j], freeRectangles[i])) {
                freeRectangles = removeAt(freeRectangles, j);
                j = sub(j, 1);
            }
        }
    }
}
Bin::Bin(Vec2i requested) : size(requested), freeRectangles{{0, 0, requested.x, requested.y}} {}
void Bin::resize(Vec2i) {}
Vec4i Bin::insert(Vec2i requested) {
    std::int32_t score1 = 0, score2 = 0;
    auto newNode = scoreRect(requested, score1, score2);
    place(newNode);
    return newNode;
}
void Bin::remove(Vec4i area) {
    for (std::size_t i = 0; i < usedRectangles.size(); ++i) {
        if (usedRectangles[i] == area) {
            usedRectangles = removeAt(usedRectangles, i);
            break;
        }
    }
    freeRectangles.append(area);
}
void Bin::clear() {
    freeRectangles = {{0, 0, size.x, size.y}};
    usedRectangles = {};
}
float Bin::occupancy() {
    std::uint64_t surfaceArea = 0;
    for (const auto rect : usedRectangles) surfaceArea += static_cast<std::uint64_t>(mul(rect.z, rect.w));
    const auto denominator = static_cast<std::uint64_t>(mul(size.x, size.y));
    if (denominator == 0) {
        // D performs integer division and traps here; do not turn this into float NaN/Inf.
        compat::ngFailIntegerDivideByZero();
    }
    return static_cast<float>(surfaceArea / denominator);
}
Vec2i Bin::getSize() { return size; }
}
TexturePacker::TexturePacker(Vec2i textureSize) : bin(textureSize) { prepareSourceClassInfo(); }
Vec4i TexturePacker::packTexture(Vec2i size) { return bin.insert(size); }
void TexturePacker::remove(Vec4i area) { bin.remove(area); }
void TexturePacker::clear() { bin.clear(); }
}
