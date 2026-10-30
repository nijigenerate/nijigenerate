#pragma once
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/core/settings_string.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/source_readonly_value.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <serialization_fields.hpp>
#include <cstddef>
#include <tuple>

namespace nijigenerate::io::inimport {

// Original generic class identity; fields and methods require their own implementation gates.
template<class Document> class IncImportLayer;

struct IncImportSettings {
    bool keepStructure = true;
    compat::RetainedStringSlice layerGroupNodeType{
        compat::RetainedString::fromStaticLiteral("DynamicComposite")};
};

// Declaration of the original generic importer; its body belongs to M152.
template<class Document> bool incAskImport(core::settings::SettingsString file);
}

namespace nicxlive::core::serde {

template<> struct SourceJsonFields<::nijigenerate::io::inimport::IncImportSettings> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::io::inimport::IncImportSettings& value, Serializer& serializer) {
        auto state = serializer.structBegin();
        auto keepStructure = value.keepStructure;
        serializer.putEscapedKey("keepStructure");
        serializer.serializeValue(keepStructure);
        auto layerGroupNodeType = value.layerGroupNodeType;
        serializer.putEscapedKey("layerGroupNodeType");
        serializer.serializeValue(layerGroupNodeType);
        serializer.structEnd(state);
    }
};

} // namespace nicxlive::core::serde

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::io::inimport::IncImportSettings> {
    using Value = ::nijigenerate::io::inimport::IncImportSettings;
    inline static constexpr std::string_view typeName = "IncImportSettings";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("keepStructure", offsetof(Value, keepStructure), &Value::keepStructure),
            ngFormatAggregateMember("layerGroupNodeType", offsetof(Value, layerGroupNodeType),
                &Value::layerGroupNodeType)};
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::io::inimport::IncImportSettings> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.inimport.IncImportSettings)";
    inline static constexpr char unqualifiedArgument[] = "(IncImportSettings)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.inimport.IncImportSettings";
    inline static constexpr char unqualifiedElement[] = "IncImportSettings";
};

template<> struct SourceTypeSpellings<const ::nijigenerate::io::inimport::IncImportSettings> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(const(nijigenerate.io.inimport.IncImportSettings))";
    inline static constexpr char unqualifiedArgument[] = "(const(IncImportSettings))";
    inline static constexpr char qualifiedElement[] = "const(nijigenerate.io.inimport.IncImportSettings)";
    inline static constexpr char unqualifiedElement[] = "const(IncImportSettings)";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::compat {
template<> struct SourceReadonlyValueBinding<::nijigenerate::io::inimport::IncImportSettings> {
    using Value = ::nijigenerate::io::inimport::IncImportSettings;
    // The two real fields are a bool and a descriptor of immutable string bytes.
    static constexpr const Value& read(const Value& value) noexcept { return value; }
};
} // namespace nijigenerate::compat
