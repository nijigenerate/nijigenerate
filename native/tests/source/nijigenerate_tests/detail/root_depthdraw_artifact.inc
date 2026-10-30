// Existing regression.d10600-10811; include once outside all namespaces.
#include "root_depthdraw_artifact.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace nijigenerate_tests::regression::detail {
nijigenerate::core::settings::SettingsJsonValue fixtureRequired(
    nijigenerate::core::settings::SettingsJsonObject object, nijigenerate::compat::RetainedString key,
    nijigenerate::compat::RetainedString owner);
nijigenerate::compat::RetainedString fixtureString(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name);
double fixtureNumber(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name);
nijigenerate::compat::Slice<float> fixtureFloatArray(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name);

std::size_t runDepthDrawArtifactFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngFloatToInt32;
    using namespace nijigenerate::compat::phobos;
    using nijigenerate::core::settings::SettingsJsonObject;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto readJson = [](RetainedString filename) {
        auto input = ngReadUtf8Text(filename);
        return nijigenerate::autorig::json::detail::parseJSON(RetainedStringSlice(input));
    };
    const auto buildPath = [](RetainedString first, RetainedString second) {
        const std::array components{first, second};
        return ngBuildPath(components);
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto baseDir = ngDirName(path);
    auto expectedValue = fixtureRequired(object, text("expected"), id);
    require(expectedValue.is_object(), concatenateSourceStrings({id, text(".expected must be an object")}));
    auto expected = expectedValue.sourceObjectRef();
    // Lower repeated nested expressions without evaluating their labels early.
    const auto numberField = [&](const SettingsJsonObject& fields, RetainedString key,
        RetainedString ownerSuffix, std::initializer_list<RetainedString> label) {
        auto field = fixtureRequired(fields, key, concatenateSourceStrings({id, ownerSuffix}));
        return fixtureNumber(field, concatenateSourceStrings(label));
    };
    const auto stringField = [&](const SettingsJsonObject& fields, RetainedString key,
        RetainedString ownerSuffix, std::initializer_list<RetainedString> label) {
        auto field = fixtureRequired(fields, key, concatenateSourceStrings({id, ownerSuffix}));
        return fixtureString(field, concatenateSourceStrings(label));
    };
    auto compositeValue = fixtureRequired(object, text("composite"), id);
    auto compositeName = fixtureString(compositeValue, concatenateSourceStrings({id, text(".composite")}));
    auto compositePath = buildPath(baseDir, compositeName);
    auto composite = readJson(compositePath);
    require(composite.is_object(), concatenateSourceStrings({id, text(".composite must point to an object JSON file")}));
    auto compositeObject = composite.sourceObjectRef();
    auto sourceMode = stringField(compositeObject, text("sourceMode"), text(".composite"), {id, text(".sourceMode")});
    auto expectedSourceMode = stringField(expected, text("sourceMode"), text(".expected"), {id, text(".expected.sourceMode")});
    require(sourceMode.view() == expectedSourceMode.view(),
        concatenateSourceStrings({id, text(" sourceMode should match depth-draw fixture expectation")}));

    auto source = fixtureRequired(compositeObject, text("composedSource"), concatenateSourceStrings({id, text(".composite")}));
    require(source.is_object(), concatenateSourceStrings({id, text(".composedSource must be an object")}));
    auto sourceObject = source.sourceObjectRef();
    auto mode = stringField(sourceObject, text("mode"), text(".composedSource"), {id, text(".mode")});
    auto expectedMode = stringField(expected, text("mode"), text(".expected"), {id, text(".expected.mode")});
    require(mode.view() == expectedMode.view(),
        concatenateSourceStrings({id, text(" composed mode should match depth-draw fixture expectation")}));
    auto width = ngFloatToInt32(numberField(sourceObject, text("width"), text(".composedSource"), {id, text(".width")}));
    auto height = ngFloatToInt32(numberField(sourceObject, text("height"), text(".composedSource"), {id, text(".height")}));
    const bool sameDimensions = width == ngFloatToInt32(numberField(expected, text("width"), text(".expected"),
        {id, text(".expected.width")})) && height == ngFloatToInt32(numberField(expected, text("height"),
        text(".expected"), {id, text(".expected.height")}));
    require(sameDimensions, concatenateSourceStrings({id, text(" composed dimensions should match depth-draw fixture expectation")}));
    auto depthScale = numberField(sourceObject, text("globalDepthScale"), text(".composedSource"), {id, text(".globalDepthScale")});
    auto expectedDepthScale = numberField(expected, text("globalDepthScale"), text(".expected"), {id, text(".expected.globalDepthScale")});
    require(std::abs(depthScale - expectedDepthScale) <= 0.00001,
        concatenateSourceStrings({id, text(" global depth scale should match depth-draw fixture expectation")}));
    auto centroid = numberField(sourceObject, text("globalDepthCentroid"), text(".composedSource"), {id, text(".globalDepthCentroid")});
    auto expectedCentroid = numberField(expected, text("globalDepthCentroid"), text(".expected"), {id, text(".expected.globalDepthCentroid")});
    require(std::abs(centroid - expectedCentroid) <= 0.00001,
        concatenateSourceStrings({id, text(" global depth centroid should match depth-draw fixture expectation")}));

    auto layersValue = fixtureRequired(object, text("layers"), id);
    require(layersValue.is_array(), concatenateSourceStrings({id, text(".layers must be an array")}));
    const auto layerCount = layersValue.sourceArrayValue().size();
    auto expectedLayerCount = static_cast<std::size_t>(numberField(expected, text("layerCount"), text(".expected"),
        {id, text(".expected.layerCount")}));
    require(layerCount == expectedLayerCount,
        concatenateSourceStrings({id, text(" artifact layer count should match depth-draw fixture expectation")}));
    if (auto* compositeLayerCount = expected.if_contains("compositeLayerCount")) {
        auto compositeLayers = fixtureRequired(sourceObject, text("layers"), concatenateSourceStrings({id, text(".composedSource")}));
        require(compositeLayers.is_array(), concatenateSourceStrings({id, text(".composedSource.layers must be an array")}));
        const auto count = compositeLayers.sourceArrayValue().size();
        auto expectedCount = static_cast<std::size_t>(fixtureNumber(*compositeLayerCount,
            concatenateSourceStrings({id, text(".expected.compositeLayerCount")})));
        require(count == expectedCount,
            concatenateSourceStrings({id, text(" composite layer count should match depth-draw fixture expectation")}));
    }
    for (auto layerValue : layersValue.sourceArrayValue()) {
        require(layerValue.is_object(), concatenateSourceStrings({id, text(".layers[] must be an object")}));
        auto layer = layerValue.sourceObjectRef();
        auto layerIndex = static_cast<std::size_t>(numberField(layer, text("index"), text(".layer"), {id, text(".layer.index")}));
        if (auto* expectedTransformValue = layer.if_contains("expectedTransform")) {
            require(expectedTransformValue->is_object(),
                concatenateSourceStrings({id, text(".layer.expectedTransform must be an object")}));
            auto expectedTransform = expectedTransformValue->sourceObjectRef();
            auto compositeLayers = fixtureRequired(sourceObject, text("layers"), concatenateSourceStrings({id, text(".composedSource")}));
            require(compositeLayers.is_array() && layerIndex < compositeLayers.sourceArrayValue().size(),
                concatenateSourceStrings({id, text(" expected transform layer index must exist in composedSource.layers")}));
            auto compositeLayer = compositeLayers.sourceArrayValue()[layerIndex];
            require(compositeLayer.is_object(), concatenateSourceStrings({id, text(".composedSource.layers[] must be objects")}));
            auto compositeLayerObject = compositeLayer.sourceObjectRef();
            auto offset = numberField(compositeLayerObject, text("depthOffset"), text(".composedSource.layer"),
                {id, text(".composedSource.layer.depthOffset")});
            auto expectedOffset = numberField(expectedTransform, text("depthOffset"), text(".layer.expectedTransform"),
                {id, text(".layer.expectedTransform.depthOffset")});
            require(std::abs(offset - expectedOffset) <= 0.000001,
                concatenateSourceStrings({id, text(" layer depthOffset should match depth-draw transform expectation")}));
            auto scale = numberField(compositeLayerObject, text("depthScale"), text(".composedSource.layer"),
                {id, text(".composedSource.layer.depthScale")});
            auto expectedScale = numberField(expectedTransform, text("depthScale"), text(".layer.expectedTransform"),
                {id, text(".layer.expectedTransform.depthScale")});
            require(std::abs(scale - expectedScale) <= 0.000001,
                concatenateSourceStrings({id, text(" layer depthScale should match depth-draw transform expectation")}));
        }
        if (auto* expectedApiTransformValue = layer.if_contains("expectedApiTransform")) {
            require(expectedApiTransformValue->is_object(),
                concatenateSourceStrings({id, text(".layer.expectedApiTransform must be an object")}));
            auto transformName = stringField(layer, text("transformResult"), text(".layer"), {id, text(".layer.transformResult")});
            auto transformResultPath = buildPath(baseDir, transformName);
            auto transformResult = readJson(transformResultPath);
            require(transformResult.is_object(), concatenateSourceStrings({id, text(".layer transform result must be an object")}));
            auto transformResultObject = transformResult.sourceObjectRef();
            auto expectedApiTransform = expectedApiTransformValue->sourceObjectRef();
            require(static_cast<std::size_t>(numberField(transformResultObject, text("index"), text(".layer.transformResult"),
                {id, text(".layer.transformResult.index")})) == layerIndex,
                concatenateSourceStrings({id, text(" layer transform result index should match the fixture layer index")}));
            auto offset = numberField(transformResultObject, text("depthOffset"), text(".layer.transformResult"),
                {id, text(".layer.transformResult.depthOffset")});
            auto expectedOffset = numberField(expectedApiTransform, text("depthOffset"), text(".layer.expectedApiTransform"),
                {id, text(".layer.expectedApiTransform.depthOffset")});
            require(std::abs(offset - expectedOffset) <= 0.000001,
                concatenateSourceStrings({id, text(" layer depthOffset should match depth-draw API transform result")}));
            auto scale = numberField(transformResultObject, text("depthScale"), text(".layer.transformResult"),
                {id, text(".layer.transformResult.depthScale")});
            auto expectedScale = numberField(expectedApiTransform, text("depthScale"), text(".layer.expectedApiTransform"),
                {id, text(".layer.expectedApiTransform.depthScale")});
            require(std::abs(scale - expectedScale) <= 0.000001,
                concatenateSourceStrings({id, text(" layer depthScale should match depth-draw API transform result")}));
        }
        auto metaName = stringField(layer, text("depthMeta"), text(".layer"), {id, text(".layer.depthMeta")});
        auto metaPath = buildPath(baseDir, metaName);
        auto rawName = stringField(layer, text("depthRaw"), text(".layer"), {id, text(".layer.depthRaw")});
        auto rawPath = buildPath(baseDir, rawName);
        auto meta = readJson(metaPath);
        require(meta.is_object(), concatenateSourceStrings({id, text(".layer depth metadata must be an object")}));
        auto metaObject = meta.sourceObjectRef();
        auto layerWidth = ngFloatToInt32(numberField(layer, text("width"), text(".layer"), {id, text(".layer.width")}));
        auto layerHeight = ngFloatToInt32(numberField(layer, text("height"), text(".layer"), {id, text(".layer.height")}));
        require(ngFloatToInt32(numberField(metaObject, text("width"), text(".layer.meta"), {id, text(".layer.meta.width")})) == layerWidth &&
            ngFloatToInt32(numberField(metaObject, text("height"), text(".layer.meta"), {id, text(".layer.meta.height")})) == layerHeight,
            concatenateSourceStrings({id, text(" layer depth metadata dimensions should match the manifest")}));
        auto raw = readJson(rawPath);
        require(raw.is_object(), concatenateSourceStrings({id, text(".layer raw depth metadata must be an object")}));
        auto rawObject = raw.sourceObjectRef();
        require(stringField(rawObject, text("generatedBy"), text(".layer.raw"), {id, text(".layer.raw.generatedBy")}).view() == "depth-draw",
            concatenateSourceStrings({id, text(" raw layer depth artifact must be generated by depth-draw")}));
        require(ngFloatToInt32(numberField(rawObject, text("width"), text(".layer.raw"), {id, text(".layer.raw.width")})) == layerWidth &&
            ngFloatToInt32(numberField(rawObject, text("height"), text(".layer.raw"), {id, text(".layer.raw.height")})) == layerHeight,
            concatenateSourceStrings({id, text(" raw layer depth dimensions should match the manifest")}));
        auto expectedRawStatsValue = fixtureRequired(layer, text("rawStats"), concatenateSourceStrings({id, text(".layer")}));
        require(expectedRawStatsValue.is_object(), concatenateSourceStrings({id, text(".layer.rawStats must be an object")}));
        auto expectedRawStats = expectedRawStatsValue.sourceObjectRef();
        auto rawStatsValue = fixtureRequired(rawObject, text("stats"), concatenateSourceStrings({id, text(".layer.raw")}));
        require(rawStatsValue.is_object(), concatenateSourceStrings({id, text(".layer.raw.stats must be an object")}));
        auto rawStats = rawStatsValue.sourceObjectRef();
        for (auto key : Slice<RetainedString>{text("min"), text("max"), text("nonZero"), text("sum")}) {
            auto actual = static_cast<std::int64_t>(numberField(rawStats, key, text(".layer.raw.stats"),
                {id, text(".layer.raw.stats."), key}));
            auto required = static_cast<std::int64_t>(numberField(expectedRawStats, key, text(".layer.rawStats"),
                {id, text(".layer.rawStats."), key}));
            require(actual == required,
                concatenateSourceStrings({id, text(" raw layer depth "), key, text(" should match depth-draw fixture expectation")}));
        }
        auto average = numberField(rawStats, text("average"), text(".layer.raw.stats"), {id, text(".layer.raw.stats.average")});
        auto expectedAverage = numberField(expectedRawStats, text("average"), text(".layer.rawStats"), {id, text(".layer.rawStats.average")});
        require(std::abs(average - expectedAverage) <= 0.000001,
            concatenateSourceStrings({id, text(" raw layer depth average should match depth-draw fixture expectation")}));
        auto rawHash = stringField(rawObject, text("sha256"), text(".layer.raw"), {id, text(".layer.raw.sha256")});
        auto expectedRawHash = stringField(expectedRawStats, text("sha256"), text(".layer.rawStats"), {id, text(".layer.rawStats.sha256")});
        require(rawHash.view() == expectedRawHash.view(),
            concatenateSourceStrings({id, text(" raw layer depth sha256 should match depth-draw fixture expectation")}));
        if (auto* rawMustDifferFrom = layer.if_contains("rawMustDifferFromSha256")) {
            auto actualHash = stringField(rawObject, text("sha256"), text(".layer.raw"), {id, text(".layer.raw.sha256")});
            auto baselineHash = fixtureString(*rawMustDifferFrom,
                concatenateSourceStrings({id, text(".layer.rawMustDifferFromSha256")}));
            require(actualHash.view() != baselineHash.view(),
                concatenateSourceStrings({id, text(" raw layer depth sha256 should differ from the referenced baseline fixture")}));
        }
        auto rawSamplesValue = fixtureRequired(rawObject, text("samples"), concatenateSourceStrings({id, text(".layer.raw")}));
        auto expectedSamplesValue = fixtureRequired(expectedRawStats, text("samples"), concatenateSourceStrings({id, text(".layer.rawStats")}));
        require(rawSamplesValue.is_array() && expectedSamplesValue.is_array(),
            concatenateSourceStrings({id, text(" raw layer depth samples must be arrays")}));
        const auto rawSampleCount = rawSamplesValue.sourceArrayValue().size();
        require(rawSampleCount == expectedSamplesValue.sourceArrayValue().size(),
            concatenateSourceStrings({id, text(" raw layer depth sample count should match depth-draw fixture expectation")}));
        auto rawEntries = rawSamplesValue.sourceArrayValue();
        for (std::size_t sampleIndex = 0; sampleIndex < rawSampleCount; ++sampleIndex) {
            auto rawSampleValue = rawEntries[sampleIndex];
            require(rawSampleValue.is_object() && expectedSamplesValue.sourceArrayValue()[sampleIndex].is_object(),
                concatenateSourceStrings({id, text(" raw layer depth sample entries must be objects")}));
            auto rawSample = rawSampleValue.sourceObjectRef();
            auto expectedSample = expectedSamplesValue.sourceArrayValue()[sampleIndex].sourceObjectRef();
            for (auto key : Slice<RetainedString>{text("x"), text("y"), text("value")}) {
                auto actual = ngFloatToInt32(numberField(rawSample, key, text(".layer.raw.sample"),
                    {id, text(".layer.raw.sample."), key}));
                auto required = ngFloatToInt32(numberField(expectedSample, key, text(".layer.rawStats.sample"),
                    {id, text(".layer.rawStats.sample."), key}));
                require(actual == required,
                    concatenateSourceStrings({id, text(" raw layer depth sample "), key, text(" should match depth-draw fixture expectation")}));
            }
        }
        if (auto* meshSummaryFile = layer.if_contains("meshSummary")) {
            auto meshName = fixtureString(*meshSummaryFile, concatenateSourceStrings({id, text(".layer.meshSummary")}));
            auto meshPath = buildPath(baseDir, meshName);
            auto mesh = readJson(meshPath);
            require(mesh.is_object(), concatenateSourceStrings({id, text(".layer mesh summary must be an object")}));
            auto meshObject = mesh.sourceObjectRef();
            require(stringField(meshObject, text("generatedBy"), text(".layer.mesh"), {id, text(".layer.mesh.generatedBy")}).view() == "depth-draw",
                concatenateSourceStrings({id, text(" mesh summary must be generated by depth-draw")}));
            auto expectedMeshStatsValue = fixtureRequired(layer, text("meshStats"), concatenateSourceStrings({id, text(".layer")}));
            require(expectedMeshStatsValue.is_object(), concatenateSourceStrings({id, text(".layer.meshStats must be an object")}));
            auto expectedMeshStats = expectedMeshStatsValue.sourceObjectRef();
            auto vertexCount = static_cast<std::int64_t>(numberField(meshObject, text("vertexCount"), text(".layer.mesh"),
                {id, text(".layer.mesh.vertexCount")}));
            auto expectedVertexCount = static_cast<std::int64_t>(numberField(expectedMeshStats, text("vertexCount"),
                text(".layer.meshStats"), {id, text(".layer.meshStats.vertexCount")}));
            require(vertexCount == expectedVertexCount,
                concatenateSourceStrings({id, text(" mesh vertex count should match depth-draw fixture expectation")}));
            auto meshCountsValue = fixtureRequired(meshObject, text("counts"), concatenateSourceStrings({id, text(".layer.mesh")}));
            auto expectedMeshCountsValue = fixtureRequired(expectedMeshStats, text("counts"), concatenateSourceStrings({id, text(".layer.meshStats")}));
            require(meshCountsValue.is_object() && expectedMeshCountsValue.is_object(),
                concatenateSourceStrings({id, text(" mesh counts must be objects")}));
            auto meshCounts = meshCountsValue.sourceObjectRef();
            auto expectedMeshCounts = expectedMeshCountsValue.sourceObjectRef();
            for (auto key : Slice<RetainedString>{text("positions"), text("uvs"), text("normals"), text("indices")}) {
                auto actual = static_cast<std::int64_t>(numberField(meshCounts, key, text(".layer.mesh.counts"),
                    {id, text(".layer.mesh.counts."), key}));
                auto required = static_cast<std::int64_t>(numberField(expectedMeshCounts, key, text(".layer.meshStats.counts"),
                    {id, text(".layer.meshStats.counts."), key}));
                require(actual == required,
                    concatenateSourceStrings({id, text(" mesh "), key, text(" count should match depth-draw fixture expectation")}));
            }
            auto meshHashesValue = fixtureRequired(meshObject, text("sha256"), concatenateSourceStrings({id, text(".layer.mesh")}));
            auto expectedMeshHashesValue = fixtureRequired(expectedMeshStats, text("sha256"), concatenateSourceStrings({id, text(".layer.meshStats")}));
            require(meshHashesValue.is_object() && expectedMeshHashesValue.is_object(),
                concatenateSourceStrings({id, text(" mesh hashes must be objects")}));
            auto meshHashes = meshHashesValue.sourceObjectRef();
            auto expectedMeshHashes = expectedMeshHashesValue.sourceObjectRef();
            for (auto key : Slice<RetainedString>{text("positions"), text("uvs"), text("normals"), text("indices")}) {
                auto actual = stringField(meshHashes, key, text(".layer.mesh.sha256"), {id, text(".layer.mesh.sha256."), key});
                auto required = stringField(expectedMeshHashes, key, text(".layer.meshStats.sha256"),
                    {id, text(".layer.meshStats.sha256."), key});
                require(actual.view() == required.view(),
                    concatenateSourceStrings({id, text(" mesh "), key, text(" sha256 should match depth-draw fixture expectation")}));
            }
            auto meshSamplesValue = fixtureRequired(meshObject, text("samples"), concatenateSourceStrings({id, text(".layer.mesh")}));
            auto expectedMeshSamplesValue = fixtureRequired(expectedMeshStats, text("samples"), concatenateSourceStrings({id, text(".layer.meshStats")}));
            require(meshSamplesValue.is_object() && expectedMeshSamplesValue.is_object(),
                concatenateSourceStrings({id, text(" mesh samples must be objects")}));
            auto meshSamples = meshSamplesValue.sourceObjectRef();
            auto expectedMeshSamples = expectedMeshSamplesValue.sourceObjectRef();
            for (auto key : Slice<RetainedString>{text("positions"), text("uvs"), text("normals"), text("indices")}) {
                auto samplesValue = fixtureRequired(meshSamples, key, concatenateSourceStrings({id, text(".layer.mesh.samples")}));
                auto samples = fixtureFloatArray(samplesValue, concatenateSourceStrings({id, text(".layer.mesh.samples."), key}));
                auto expectedSamplesField = fixtureRequired(expectedMeshSamples, key,
                    concatenateSourceStrings({id, text(".layer.meshStats.samples")}));
                auto expectedSamples = fixtureFloatArray(expectedSamplesField,
                    concatenateSourceStrings({id, text(".layer.meshStats.samples."), key}));
                require(samples.size() == expectedSamples.size(),
                    concatenateSourceStrings({id, text(" mesh "), key, text(" sample count should match")}));
                const auto count = samples.size();
                for (std::size_t sampleIndex = 0; sampleIndex < count; ++sampleIndex) {
                    auto sample = samples[sampleIndex];
                    const bool sameSample = std::abs(sample - expectedSamples[sampleIndex]) <= 0.000001;
                    const std::array<format::TypedFormatArgument, 5> arguments{
                        format::ngFormatArgument(id), format::ngFormatArgument(key), format::ngFormatArgument(sampleIndex),
                        format::ngFormatArgument(sample), format::ngFormatArgument(expectedSamples[sampleIndex])};
                    auto rendered = format::ngFormatTyped("%s mesh %s sample[%s] actual=%s expected=%s", arguments);
                    auto message = nijigenerate::platform::detail::ngPathAllocation([&] {
                        return RetainedString(std::move(rendered));
                    });
                    require(sameSample, message);
                }
            }
        }
    }
    return 1;
}
}
