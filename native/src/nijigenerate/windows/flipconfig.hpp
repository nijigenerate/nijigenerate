#pragma once

#include <nodes/node.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <serialization_fields.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <memory>
#include <tuple>
#include "flipconfig_model.hpp"

namespace nicxlive::core { class Puppet; }

namespace nijigenerate::windows::flipconfig {

// Model classes retain their own gates; the result records below are real value types.
class FlipPair;
class FlipPairWindow;
struct FlipPairMutationResult {
    bool succeeded = false;
    compat::RetainedStringSlice message{};
    std::size_t index = 0;
};
struct FlipPairBulkMutationResult {
    bool succeeded = false;
    compat::RetainedStringSlice message{};
    std::size_t added = 0;
};

extern thread_local compat::RetainedStringSlice FlipConfigPath;

void ngLoadFlipConfig(std::shared_ptr<nicxlive::core::Puppet> puppet);
void ngDumpFlipConfig(std::shared_ptr<nicxlive::core::Puppet> puppet);
void ngInitFlipConfig();
compat::Slice<std::shared_ptr<FlipPair>> ngGetFlipPairs();
std::shared_ptr<FlipPair> ngGetFlipPairFor(std::shared_ptr<nicxlive::core::nodes::Node> node);
core::settings::SettingsJsonValue ngListFlipPairsJson();
FlipPairMutationResult ngReplaceFlipPairs(compat::Slice<std::shared_ptr<FlipPair>> pairs);
FlipPairMutationResult ngAddFlipPair(std::shared_ptr<nicxlive::core::nodes::Node> left,
                                  std::shared_ptr<nicxlive::core::nodes::Node> right);
FlipPairMutationResult ngRemoveFlipPair(std::shared_ptr<nicxlive::core::nodes::Node> left,
                                     std::shared_ptr<nicxlive::core::nodes::Node> right);
FlipPairBulkMutationResult ngAutoAddFlipPairs(std::shared_ptr<nicxlive::core::Puppet> puppet,
    compat::RetainedStringSlice leftPattern, compat::RetainedStringSlice rightPattern);

} // namespace nijigenerate::windows::flipconfig

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::flipconfig::FlipPairMutationResult> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.flipconfig.FlipPairMutationResult)";
    inline static constexpr char unqualifiedArgument[] = "(FlipPairMutationResult)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.flipconfig.FlipPairMutationResult";
    inline static constexpr char unqualifiedElement[] = "FlipPairMutationResult";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::flipconfig::FlipPairBulkMutationResult> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.flipconfig.FlipPairBulkMutationResult)";
    inline static constexpr char unqualifiedArgument[] = "(FlipPairBulkMutationResult)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.flipconfig.FlipPairBulkMutationResult";
    inline static constexpr char unqualifiedElement[] = "FlipPairBulkMutationResult";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<> struct FormatAggregateTraits<::nijigenerate::windows::flipconfig::FlipPairMutationResult> {
    using Value = ::nijigenerate::windows::flipconfig::FlipPairMutationResult;
    inline static constexpr std::string_view typeName = "FlipPairMutationResult";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("succeeded", offsetof(Value, succeeded), &Value::succeeded),
            ngFormatAggregateMember("message", offsetof(Value, message), &Value::message),
            ngFormatAggregateMember("index", offsetof(Value, index), &Value::index)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::windows::flipconfig::FlipPairBulkMutationResult> {
    using Value = ::nijigenerate::windows::flipconfig::FlipPairBulkMutationResult;
    inline static constexpr std::string_view typeName = "FlipPairBulkMutationResult";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("succeeded", offsetof(Value, succeeded), &Value::succeeded),
            ngFormatAggregateMember("message", offsetof(Value, message), &Value::message),
            ngFormatAggregateMember("added", offsetof(Value, added), &Value::added)};
    }
};

} // namespace nijigenerate::core::translation

namespace nicxlive::core::serde {

template<> struct SourceJsonFields<::nijigenerate::windows::flipconfig::FlipPairMutationResult> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::windows::flipconfig::FlipPairMutationResult& value,
            Serializer& serializer) {
        auto state = serializer.structBegin();
        auto succeeded = value.succeeded;
        serializer.putEscapedKey("succeeded");
        serializer.serializeValue(succeeded);
        auto message = value.message;
        serializer.putEscapedKey("message");
        serializer.serializeValue(message);
        auto index = value.index;
        serializer.putEscapedKey("index");
        serializer.serializeValue(index);
        serializer.structEnd(state);
    }
};

template<> struct SourceJsonFields<::nijigenerate::windows::flipconfig::FlipPairBulkMutationResult> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::windows::flipconfig::FlipPairBulkMutationResult& value,
            Serializer& serializer) {
        auto state = serializer.structBegin();
        auto succeeded = value.succeeded;
        serializer.putEscapedKey("succeeded");
        serializer.serializeValue(succeeded);
        auto message = value.message;
        serializer.putEscapedKey("message");
        serializer.serializeValue(message);
        auto added = value.added;
        serializer.putEscapedKey("added");
        serializer.serializeValue(added);
        serializer.structEnd(state);
    }
};

} // namespace nicxlive::core::serde

// The original module publicly exposes its actual Window subclass as well as
// the model/functions. Keep one canonical class definition and its real base.
#include "flipconfig_window.hpp"
