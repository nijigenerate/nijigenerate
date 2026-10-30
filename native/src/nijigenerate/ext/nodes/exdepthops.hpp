#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <array>
#include <tuple>

#include <math/types.hpp>
#include <math/serialization.hpp>
#include <serde_reader.hpp>
#include <serde_json.hpp>
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/switch_error.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/ext/nodes/exdepthnumeric.hpp"
#include <nijigenerate/core/typed_format_vec2.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nicxlive::core::nodes { class Node; }

namespace nijigenerate::ext::nodes {

// M140.S0001: preserve the D enum's default 32-bit signed representation.
enum class ExDepthOpType : std::int32_t {
    AttachedPoint = 0,
    Ring = 1,
    Plane = 2,
};

// Legacy intake marker; production uses the canonical source SwitchError below.
struct DepthOperationSwitchError {
    ExDepthOpType value;
};

struct ExDepthOp {
    ExDepthOpType type{ExDepthOpType::AttachedPoint};
    std::size_t index{0};
    nicxlive::core::math::Vec2 p0{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    nicxlive::core::math::Vec2 p1{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    nicxlive::core::math::Vec2 center{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    float amount{std::numeric_limits<float>::quiet_NaN()};
    float width{std::numeric_limits<float>::quiet_NaN()};
    float hardness{std::numeric_limits<float>::quiet_NaN()};
    float p0Angle{180.0f};
    float p1Angle{0.0f};
    float radiusX{std::numeric_limits<float>::quiet_NaN()};
    float radiusY{std::numeric_limits<float>::quiet_NaN()};
    float angle{std::numeric_limits<float>::quiet_NaN()};
    float targetDepth{std::numeric_limits<float>::quiet_NaN()};
    float flattenStrength{std::numeric_limits<float>::quiet_NaN()};

    std::string_view typeName() const {
        switch (type) {
        case ExDepthOpType::AttachedPoint: return "attached-point";
        case ExDepthOpType::Ring: return "ring";
        case ExDepthOpType::Plane: return "plane";
        }
        throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            compat::RetainedString::fromStaticLiteral("nijigenerate/ext/nodes/exdepthops.d"), 41);
    }

    template<class Serializer>
    void serialize(Serializer& serializer) const {
        const auto state = serializer.structBegin();
        serializer.putKey("type");
        serializer.putValue(typeName());
        switch (type) {
        case ExDepthOpType::AttachedPoint:
            serializer.putKey("index");
            serializer.serializeValue(index);
            serializer.putKey("amount");
            serializer.serializeValue(amount);
            break;
        case ExDepthOpType::Ring:
            serializer.putKey("p0");
            nicxlive::core::math::serialize(p0, serializer);
            serializer.putKey("p1");
            nicxlive::core::math::serialize(p1, serializer);
            serializer.putKey("amount");
            serializer.serializeValue(amount);
            serializer.putKey("width");
            serializer.serializeValue(width);
            serializer.putKey("hardness");
            serializer.serializeValue(hardness);
            serializer.putKey("p0Angle");
            serializer.serializeValue(p0Angle);
            serializer.putKey("p1Angle");
            serializer.serializeValue(p1Angle);
            break;
        case ExDepthOpType::Plane:
            serializer.putKey("center");
            nicxlive::core::math::serialize(center, serializer);
            serializer.putKey("radiusX");
            serializer.serializeValue(radiusX);
            serializer.putKey("radiusY");
            serializer.serializeValue(radiusY);
            serializer.putKey("angle");
            serializer.serializeValue(angle);
            serializer.putKey("targetDepth");
            serializer.serializeValue(targetDepth);
            serializer.putKey("flattenStrength");
            serializer.serializeValue(flattenStrength);
            break;
        default:
            throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
                compat::RetainedString::fromStaticLiteral("nijigenerate/ext/nodes/exdepthops.d"), 53);
        }
        serializer.structEnd(state);
    }

    nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView data) {
        namespace serde = nicxlive::core::serde;
        compat::RetainedString typeString;
        if (auto error = serde::deserializeValue(data.findFirst("type"), typeString)) return error;
        const auto readFloat = [&data](std::string_view key, float& value) -> serde::ReadError {
            const auto entry = data.findFirst(key);
            if (!entry.isMissing()) return serde::deserializeValue(entry, value);
            return {};
        };
        const auto readVector = [&data](std::string_view key, nicxlive::core::math::Vec2& value) {
            // The original intentionally ignores the vector reader's returned error.
            (void)nicxlive::core::math::deserialize(value, data.findFirst(key));
        };
        if (typeString.view() == "attached-point") {
            type = ExDepthOpType::AttachedPoint;
            const auto entry = data.findFirst("index");
            if (!entry.isMissing()) {
                if (auto error = serde::deserializeValue(entry, index)) return error;
            }
            if (auto error = readFloat("amount", amount)) return error;
            return {};
        }
        if (typeString.view() == "ring") {
            type = ExDepthOpType::Ring;
            readVector("p0", p0);
            readVector("p1", p1);
            if (auto error = readFloat("amount", amount)) return error;
            if (auto error = readFloat("width", width)) return error;
            if (auto error = readFloat("hardness", hardness)) return error;
            if (auto error = readFloat("p0Angle", p0Angle)) return error;
            if (auto error = readFloat("p1Angle", p1Angle)) return error;
            return {};
        }
        if (typeString.view() == "plane") {
            type = ExDepthOpType::Plane;
            readVector("center", center);
            if (auto error = readFloat("radiusX", radiusX)) return error;
            if (auto error = readFloat("radiusY", radiusY)) return error;
            if (auto error = readFloat("angle", angle)) return error;
            if (auto error = readFloat("targetDepth", targetDepth)) return error;
            if (auto error = readFloat("flattenStrength", flattenStrength)) return error;
            return {};
        }
        constexpr std::string_view prefix = "unknown depth operation type: ";
        const auto suffix = typeString.view();
        nicxlive::core::d_runtime::SourceAllocator<serde::ReadError::Object> memory;
        auto* error = memory.allocate(1);
        try {
            const auto message = [&] {
                try {
                    std::string bytes;
                    if (prefix.size() > bytes.max_size() || suffix.size() > bytes.max_size() - prefix.size())
                        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                    bytes.reserve(prefix.size() + suffix.size());
                    bytes.append(prefix);
                    if (!suffix.empty()) bytes.append(suffix);
                    return compat::RetainedString(std::move(bytes));
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }();
            // The genuine inherited Exception constructor is outside allocation conversion.
            std::construct_at(error, message,
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthops.d"), 154);
        } catch (...) {
            memory.deallocate(error, 1);
            throw;
        }
        return serde::ReadError(std::shared_ptr<serde::ReadError::Object>(error,
            [memory](serde::ReadError::Object* value) mutable {
                std::destroy_at(value);
                memory.deallocate(value, 1);
            }, memory));
    }
};

class DepthOperationMappedNode {
public:
    virtual ~DepthOperationMappedNode() = 0;
    virtual compat::Slice<ExDepthOp> copyDepthOps() = 0;
    virtual void replaceDepthOps(compat::Slice<ExDepthOp> values) = 0;
    virtual compat::Slice<float> copyDepthOpBaseDepths() = 0;
    virtual void replaceDepthOpBaseDepths(compat::Slice<float> values) = 0;
};

inline DepthOperationMappedNode::~DepthOperationMappedNode() = default;

// State helper used by editor extensions; interface forwarding is explicit.
struct ExDepthOperated {
    virtual ~ExDepthOperated() = default;
    compat::Slice<ExDepthOp> depthOps;
    compat::Slice<float> depthOpBaseDepths;

    virtual void copyDepthOpsFrom(std::shared_ptr<nicxlive::core::nodes::Node> source);

    virtual compat::Slice<ExDepthOp> copyDepthOps() {
        return depthOps.duplicate();
    }

    virtual void replaceDepthOps(compat::Slice<ExDepthOp> values) {
        depthOps = values.duplicate();
    }

    virtual compat::Slice<float> copyDepthOpBaseDepths() {
        return depthOpBaseDepths.duplicate();
    }

    virtual void replaceDepthOpBaseDepths(compat::Slice<float> values) {
        depthOpBaseDepths = values.duplicate();
        ngNormalizeDepths(std::span<float>(depthOpBaseDepths.data(), depthOpBaseDepths.size()));
    }

    virtual void serializeDepthOps(nicxlive::core::serde::TypedJsonWriter& serializer) {
        serializeDepthOps<nicxlive::core::serde::TypedJsonWriter>(serializer);
    }

    template<class Serializer>
    void serializeDepthOps(Serializer& serializer) {
        if (depthOps.empty()) return;
        serializer.putKey("depth-ops");
        serializer.serializeValue(depthOps);
        if (!depthOpBaseDepths.empty()) {
            serializer.putKey("depth-op-base-depths");
            serializer.serializeValue(depthOpBaseDepths);
        }
    }

    virtual nicxlive::core::serde::ReadError deserializeDepthOps(nicxlive::core::serde::TypedJsonView data) {
        namespace serde = nicxlive::core::serde;
        const auto operations = data.findFirst("depth-ops");
        if (operations.isMissing()) return {};
        if (operations.kind() == serde::JsonKind::Null) {
            depthOps.clear();
            return {};
        }
        depthOps.resize(0);
        // Fghj.byElement is an empty range for every non-array kind.
        if (operations.kind() == serde::JsonKind::Array) {
            for (const auto entry : operations.byElement()) {
                ExDepthOp operation;
                if (auto error = operation.deserializeFromFghj(entry)) return error;
                depthOps.append(operation);
            }
        }
        depthOpBaseDepths.clear();
        const auto bases = data.findFirst("depth-op-base-depths");
        if (!bases.isMissing() && bases.kind() != serde::JsonKind::Null) {
            if (bases.kind() == serde::JsonKind::Array) {
                for (const auto entry : bases.byElement()) {
                    float depth = std::numeric_limits<float>::quiet_NaN();
                    if (auto error = serde::deserializeValue(entry, depth)) return error;
                    depthOpBaseDepths.append(depth);
                }
            }
            ngNormalizeDepths(std::span<float>(depthOpBaseDepths.data(), depthOpBaseDepths.size()));
        }
        return {};
    }

    virtual void resizeDepthOpBaseDepthsToVertices(std::size_t vertexCount) {
        if (depthOpBaseDepths.isNull()) return;
        ngNormalizeDepths(std::span<float>(depthOpBaseDepths.data(), depthOpBaseDepths.size()));
        const auto oldLength = depthOpBaseDepths.size();
        depthOpBaseDepths.resize(vertexCount, std::numeric_limits<float>::quiet_NaN());
        for (auto i = oldLength; i < depthOpBaseDepths.size(); ++i) depthOpBaseDepths[i] = 0.0f;
    }
};

} // namespace nijigenerate::ext::nodes

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::ext::nodes::ExDepthOpType> {
    using Value = ::nijigenerate::ext::nodes::ExDepthOpType;
    inline static constexpr std::string_view name = "ExDepthOpType";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::AttachedPoint, "AttachedPoint"},
        FormatEnumMember<Value>{Value::Ring, "Ring"},
        FormatEnumMember<Value>{Value::Plane, "Plane"}};
};

template<> struct FormatAggregateTraits<::nijigenerate::ext::nodes::ExDepthOp> {
    using Value = ::nijigenerate::ext::nodes::ExDepthOp;
    inline static constexpr std::string_view typeName = "ExDepthOp";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("type", offsetof(Value, type), &Value::type),
            ngFormatAggregateMember("index", offsetof(Value, index), &Value::index),
            ngFormatAggregateMember("p0", offsetof(Value, p0), &Value::p0),
            ngFormatAggregateMember("p1", offsetof(Value, p1), &Value::p1),
            ngFormatAggregateMember("center", offsetof(Value, center), &Value::center),
            ngFormatAggregateMember("amount", offsetof(Value, amount), &Value::amount),
            ngFormatAggregateMember("width", offsetof(Value, width), &Value::width),
            ngFormatAggregateMember("hardness", offsetof(Value, hardness), &Value::hardness),
            ngFormatAggregateMember("p0Angle", offsetof(Value, p0Angle), &Value::p0Angle),
            ngFormatAggregateMember("p1Angle", offsetof(Value, p1Angle), &Value::p1Angle),
            ngFormatAggregateMember("radiusX", offsetof(Value, radiusX), &Value::radiusX),
            ngFormatAggregateMember("radiusY", offsetof(Value, radiusY), &Value::radiusY),
            ngFormatAggregateMember("angle", offsetof(Value, angle), &Value::angle),
            ngFormatAggregateMember("targetDepth", offsetof(Value, targetDepth), &Value::targetDepth),
            ngFormatAggregateMember("flattenStrength", offsetof(Value, flattenStrength), &Value::flattenStrength)};
    }
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::ExDepthOpType> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.ext.nodes.exdepthops.ExDepthOpType)"),
            RetainedString::fromStaticLiteral("(ExDepthOpType)"),
            RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.exdepthops.ExDepthOpType"),
            RetainedString::fromStaticLiteral("ExDepthOpType")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::ExDepthOp> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.ext.nodes.exdepthops.ExDepthOp)"),
            RetainedString::fromStaticLiteral("(ExDepthOp)"),
            RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.exdepthops.ExDepthOp"),
            RetainedString::fromStaticLiteral("ExDepthOp")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::DepthOperationMappedNode> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.ext.nodes.exdepthops.DepthOperationMappedNode)"),
            RetainedString::fromStaticLiteral("(DepthOperationMappedNode)"),
            RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.exdepthops.DepthOperationMappedNode"),
            RetainedString::fromStaticLiteral("DepthOperationMappedNode")};
    }
};
} // namespace nicxlive::core::d_runtime
