#pragma once
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include "meta_reflect.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::viewport::vertex::automesh {
namespace optimum { class OptimumAutoMeshProcessor; }
namespace contours { class ContourAutoMeshProcessor; }
namespace grid { class GridAutoMeshProcessor; }
namespace skeletonize { class SkeletonExtractor; }
}

namespace nijigenerate::viewport::vertex::automesh::meta {

using autorig::json::detail::parseJSON;

enum class AutoMeshLevel : std::int32_t {
    Preset = 0,
    Simple = 1,
    Advanced = 2
};

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

struct AMParam {
    AutoMeshLevel level = AutoMeshLevel::Preset;
    compat::RetainedString id;
    compat::RetainedString label;
    compat::RetainedString desc;
    compat::RetainedString widget;
    float min = std::numeric_limits<float>::quiet_NaN();
    float max = std::numeric_limits<float>::quiet_NaN();
    float step = std::numeric_limits<float>::quiet_NaN();
    std::int32_t order = 0;
};

struct AMArray {
    float min = std::numeric_limits<float>::quiet_NaN();
    float max = std::numeric_limits<float>::quiet_NaN();
    float step = std::numeric_limits<float>::quiet_NaN();
};

struct AMEnum {
    compat::Slice<compat::RetainedString> values;
};

struct AMPreset {
    compat::RetainedString name;
};

struct AMPresetProvider {};

struct AMProcessor {
    compat::RetainedString id;
    compat::RetainedString displayName;
    std::int32_t order = 0;
};

// Other translated aliases specialize this trait with the real D identifier
// and UDA tuple. Empty attributes use the original identifier fallback;
// untranslated C++ types never get invented/RTTI source identifiers.
struct AMProcessorSourceAttribute {
    std::string_view id;
    std::string_view displayName;
    std::int32_t order;
};
template<class SourceAlias> struct AMSourceAttributes;

template<class SourceAlias> struct AMProcInfo {
private:
    template<class Function> static constexpr bool withFirstProcessorAttribute(Function&& function) {
        bool found = false;
        using Attributes = std::remove_cvref_t<decltype(AMSourceAttributes<SourceAlias>::attributes)>;
        [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
            ([&] {
                if constexpr (std::is_same_v<std::tuple_element_t<Indices, Attributes>,
                        AMProcessorSourceAttribute>) {
                    if (!found) {
                        const auto attribute = std::get<Indices>(AMSourceAttributes<SourceAlias>::attributes);
                        function(attribute);
                        found = true;
                    }
                }
            }(), ...);
        }(std::make_index_sequence<std::tuple_size_v<Attributes>>{});
        return found;
    }
public:
    static constexpr std::string_view id = []() -> std::string_view {
        std::string_view result;
        if (withFirstProcessorAttribute([&](const auto& attribute) {
                result = attribute.id.empty() ? AMSourceAttributes<SourceAlias>::identifier : attribute.id;
            })) return result;
        return AMSourceAttributes<SourceAlias>::identifier;
    }();
    static constexpr std::string_view name = []() -> std::string_view {
        std::string_view result;
        if (withFirstProcessorAttribute([&](const auto& attribute) {
                result = attribute.displayName.empty() ? id : attribute.displayName;
            })) return result;
        return id;
    }();
    static constexpr std::int32_t order = []() -> std::int32_t {
        std::int32_t result = 0;
        withFirstProcessorAttribute([&](const auto& attribute) { result = attribute.order; });
        return result;
    }();
};

template<> struct AMSourceAttributes<optimum::OptimumAutoMeshProcessor> {
    static constexpr std::string_view identifier = "OptimumAutoMeshProcessor";
    static constexpr auto attributes = std::tuple{AMProcessorSourceAttribute{"optimum","Optimum",50}};
};
template<> struct AMSourceAttributes<contours::ContourAutoMeshProcessor> {
    static constexpr std::string_view identifier = "ContourAutoMeshProcessor";
    static constexpr auto attributes = std::tuple{AMProcessorSourceAttribute{"contour","Contour",100}};
};
template<> struct AMSourceAttributes<grid::GridAutoMeshProcessor> {
    static constexpr std::string_view identifier = "GridAutoMeshProcessor";
    static constexpr auto attributes = std::tuple{AMProcessorSourceAttribute{"grid","Grid",200}};
};
template<> struct AMSourceAttributes<skeletonize::SkeletonExtractor> {
    static constexpr std::string_view identifier = "SkeletonExtractor";
    static constexpr auto attributes = std::tuple{AMProcessorSourceAttribute{"skeleton","Skeleton",300}};
};

}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::viewport::vertex::automesh::meta::AutoMeshLevel> {
    using Enum = ::nijigenerate::viewport::vertex::automesh::meta::AutoMeshLevel;
    inline static constexpr std::string_view name = "AutoMeshLevel";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Preset, "Preset"},
        FormatEnumMember<Enum>{Enum::Simple, "Simple"},
        FormatEnumMember<Enum>{Enum::Advanced, "Advanced"}};
};

template<> struct FormatAggregateTraits<::nijigenerate::viewport::vertex::automesh::meta::AMParam> {
    using Value = ::nijigenerate::viewport::vertex::automesh::meta::AMParam;
    inline static constexpr std::string_view typeName = "AMParam";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("level", offsetof(Value, level), &Value::level),
            ngFormatAggregateMember("id", offsetof(Value, id), &Value::id),
            ngFormatAggregateMember("label", offsetof(Value, label), &Value::label),
            ngFormatAggregateMember("desc", offsetof(Value, desc), &Value::desc),
            ngFormatAggregateMember("widget", offsetof(Value, widget), &Value::widget),
            ngFormatAggregateMember("min", offsetof(Value, min), &Value::min),
            ngFormatAggregateMember("max", offsetof(Value, max), &Value::max),
            ngFormatAggregateMember("step", offsetof(Value, step), &Value::step),
            ngFormatAggregateMember("order", offsetof(Value, order), &Value::order)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::viewport::vertex::automesh::meta::AMArray> {
    using Value = ::nijigenerate::viewport::vertex::automesh::meta::AMArray;
    inline static constexpr std::string_view typeName = "AMArray";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("min", offsetof(Value, min), &Value::min),
            ngFormatAggregateMember("max", offsetof(Value, max), &Value::max),
            ngFormatAggregateMember("step", offsetof(Value, step), &Value::step)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::viewport::vertex::automesh::meta::AMEnum> {
    using Value = ::nijigenerate::viewport::vertex::automesh::meta::AMEnum;
    inline static constexpr std::string_view typeName = "AMEnum";
    static constexpr auto fields() {
        return std::tuple{ngFormatAggregateMember("values", offsetof(Value, values), &Value::values)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::viewport::vertex::automesh::meta::AMPreset> {
    using Value = ::nijigenerate::viewport::vertex::automesh::meta::AMPreset;
    inline static constexpr std::string_view typeName = "AMPreset";
    static constexpr auto fields() {
        return std::tuple{ngFormatAggregateMember("name", offsetof(Value, name), &Value::name)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::viewport::vertex::automesh::meta::AMPresetProvider> {
    inline static constexpr std::string_view typeName = "AMPresetProvider";
    static constexpr auto fields() { return std::tuple{}; }
};

template<> struct FormatAggregateTraits<::nijigenerate::viewport::vertex::automesh::meta::AMProcessor> {
    using Value = ::nijigenerate::viewport::vertex::automesh::meta::AMProcessor;
    inline static constexpr std::string_view typeName = "AMProcessor";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("id", offsetof(Value, id), &Value::id),
            ngFormatAggregateMember("displayName", offsetof(Value, displayName), &Value::displayName),
            ngFormatAggregateMember("order", offsetof(Value, order), &Value::order)};
    }
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
#define NG_AUTOMESH_META_VALUE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::meta::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.vertex.automesh.meta." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.vertex.automesh.meta." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_AUTOMESH_META_VALUE_ORIGIN(AutoMeshLevel);
NG_AUTOMESH_META_VALUE_ORIGIN(AMParam);
NG_AUTOMESH_META_VALUE_ORIGIN(AMArray);
NG_AUTOMESH_META_VALUE_ORIGIN(AMEnum);
NG_AUTOMESH_META_VALUE_ORIGIN(AMPreset);
NG_AUTOMESH_META_VALUE_ORIGIN(AMPresetProvider);
NG_AUTOMESH_META_VALUE_ORIGIN(AMProcessor);
#undef NG_AUTOMESH_META_VALUE_ORIGIN
}

#include <nijigenerate/compat/mir_rc_element_definition.hpp>

namespace nijigenerate::compat {
template<> struct MirRCElementDefinition<viewport::vertex::automesh::meta::AMArray> {
    using Value = viewport::vertex::automesh::meta::AMArray;
    // Three Source float fields, with no explicit alignment or hidden context.
    inline static constexpr std::int32_t sourceByteSize = 12;
    inline static constexpr bool elaborateAssign = false;
    inline static constexpr bool elaborateDestructor = false;
    inline static constexpr bool noMutableIndirections = true;
    inline static constexpr bool sharedValueOperationsEquivalent = true;
    inline static constexpr bool readOnlyValueOperationsEquivalent = true;

    static const void* initializerPayload() {
        static const Value initializer{};
        return &initializer;
    }
    static void fillDefault(Value* elements, std::size_t length) {
        for (std::size_t i = 0; i < length; ++i) std::construct_at(elements + i, Value{});
    }
    static void assignElement(Value& destination, Value& source) { destination = source; }
    static void emplaceInt(Value* destination, std::int32_t value) {
        // The Source struct literal initializes min, then defaults max and step.
        std::construct_at(destination, Value{static_cast<float>(value)});
    }
};

// Source meta.d:7-23: these aggregates have no user constructor/opCall. A
// dynamic int cannot initialize an enum, string or string[] field; an empty
// aggregate has no field to receive it. These are Source-negative bodies, not
// native layout failures or invented lifecycle definitions for retained fields.
#define NG_AM_SOURCE_INVALID_INT(Type, Reason) \
template<> struct MirRCIntConstruction<viewport::vertex::automesh::meta::Type> { \
    using Value = viewport::vertex::automesh::meta::Type; \
    using SourceCase = MirRCInvalidSourceIntBody; \
    inline static constexpr std::string_view sourceReason = Reason; \
    static void emplace(Value*, std::int32_t) = delete; \
}
NG_AM_SOURCE_INVALID_INT(AMParam, "dynamic int cannot implicitly initialize AutoMeshLevel");
NG_AM_SOURCE_INVALID_INT(AMEnum, "dynamic int cannot initialize string[]");
NG_AM_SOURCE_INVALID_INT(AMPreset, "dynamic int cannot initialize string");
NG_AM_SOURCE_INVALID_INT(AMPresetProvider, "empty Source aggregate accepts no argument");
NG_AM_SOURCE_INVALID_INT(AMProcessor, "dynamic int cannot initialize first string field");
#undef NG_AM_SOURCE_INVALID_INT
}

#include "meta_class_info.hpp"
#include "meta_reflection.hpp"
