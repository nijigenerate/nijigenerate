// Selected existing regression.d DepthDraw helpers; exact spans accompany each body.
// Include once outside all namespaces.
#include "root_depthdraw_fixture.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/io/depthimage.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <span>
#include <utility>

namespace nijigenerate_tests::regression::detail {
nijigenerate::core::settings::SettingsJsonValue fixtureRequired(
    nijigenerate::core::settings::SettingsJsonObject object, nijigenerate::compat::RetainedString key,
    nijigenerate::compat::RetainedString owner);
nijigenerate::compat::RetainedString fixtureString(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name);
nijigenerate::compat::Slice<std::uint8_t> fixtureByteArray(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name);
double fixtureNumber(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name);
nijigenerate::compat::Slice<int> fixtureIntArray(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name);
bool fixtureBool(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name);
nijigenerate::core::settings::SettingsJsonValue loadDepthDrawFixture(nijigenerate::compat::RetainedString name) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    const std::array path{
        RetainedString::fromStaticLiteral("source"),
        RetainedString::fromStaticLiteral("nijigenerate_tests"),
        RetainedString::fromStaticLiteral("fixtures"),
        RetainedString::fromStaticLiteral("depthdraw"), name};
    auto input = nijigenerate::compat::phobos::ngReadUtf8Text(nijigenerate::compat::phobos::ngBuildPath(path));
    return nijigenerate::autorig::json::detail::parseJSON(RetainedStringSlice(input));
}

// Existing regression.d10173-10203.
std::size_t runDepthDrawAlphaMaskFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using namespace nijigenerate::io::depthimage;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto equalBytes = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto rgbaValue = fixtureRequired(object, text("rgba"), id);
    auto rgba = fixtureByteArray(rgbaValue, concatenateSourceStrings({id, text(".rgba")}));
    auto depthValue = fixtureRequired(object, text("depthPixels"), id);
    auto depthPixels = fixtureByteArray(depthValue, concatenateSourceStrings({id, text(".depthPixels")}));
    auto expectedValue = fixtureRequired(object, text("expected"), id);
    require(expectedValue.is_object(), concatenateSourceStrings({id, text(".expected must be an object")}));
    auto expected = expectedValue.sourceObjectRef();

    auto alphaMask = ngDepthDrawAlphaMaskFromRgbaSource(rgba, 1);
    auto surfaceMask = ngDepthDrawAlphaMaskFromRgbaSource(rgba, 255);
    auto depthMask = ngDepthDrawMaskFromDepthPixelsSource(depthPixels);
    auto splitDepthPixels = ngDepthDrawApplyBinaryMaskToDepthPixelsSource(depthPixels, alphaMask);
    auto splitRenderDepthMask = ngDepthDrawMaskFromDepthPixelsSource(splitDepthPixels);

    auto expectedAlphaValue = fixtureRequired(expected, text("alphaMaskMin1"), id);
    auto expectedAlpha = fixtureByteArray(expectedAlphaValue,
        concatenateSourceStrings({id, text(".expected.alphaMaskMin1")}));
    require(equalBytes(alphaMask, expectedAlpha),
        concatenateSourceStrings({id, text(" alphaMask min=1 must match depth-draw extractAlphaMask")}));
    auto expectedSurfaceValue = fixtureRequired(expected, text("surfaceMaskMin255"), id);
    auto expectedSurface = fixtureByteArray(expectedSurfaceValue,
        concatenateSourceStrings({id, text(".expected.surfaceMaskMin255")}));
    require(equalBytes(surfaceMask, expectedSurface),
        concatenateSourceStrings({id, text(" surfaceMask min=255 must match depth-draw extractAlphaMask")}));
    auto expectedDepthValue = fixtureRequired(expected, text("depthMaskPositive"), id);
    auto expectedDepth = fixtureByteArray(expectedDepthValue,
        concatenateSourceStrings({id, text(".expected.depthMaskPositive")}));
    require(equalBytes(depthMask, expectedDepth),
        concatenateSourceStrings({id, text(" depth mask must match depth-draw createMaskFromDepthPixels")}));
    auto expectedSplitDepthValue = fixtureRequired(expected, text("splitDepthPixels"), id);
    auto expectedSplitDepth = fixtureByteArray(expectedSplitDepthValue,
        concatenateSourceStrings({id, text(".expected.splitDepthPixels")}));
    require(equalBytes(splitDepthPixels, expectedSplitDepth),
        concatenateSourceStrings({id, text(" split depth pixels must match depth-draw splitFlatDepthByColorLayers")}));
    auto expectedSplitMaskValue = fixtureRequired(expected, text("splitDepthMaskPixels"), id);
    auto expectedSplitMask = fixtureByteArray(expectedSplitMaskValue,
        concatenateSourceStrings({id, text(".expected.splitDepthMaskPixels")}));
    require(equalBytes(alphaMask, expectedSplitMask),
        concatenateSourceStrings({id, text(" split depthMaskPixels must use color alpha mask")}));
    auto expectedSplitRenderValue = fixtureRequired(expected, text("splitRenderDepthMask"), id);
    auto expectedSplitRender = fixtureByteArray(expectedSplitRenderValue,
        concatenateSourceStrings({id, text(".expected.splitRenderDepthMask")}));
    require(equalBytes(splitRenderDepthMask, expectedSplitRender),
        concatenateSourceStrings({id, text(" split renderDepthMask must be positive split depth pixels")}));
    auto expectedSplitSurfaceValue = fixtureRequired(expected, text("splitSurfaceMaskPixels"), id);
    auto expectedSplitSurface = fixtureByteArray(expectedSplitSurfaceValue,
        concatenateSourceStrings({id, text(".expected.splitSurfaceMaskPixels")}));
    require(equalBytes(surfaceMask, expectedSplitSurface),
        concatenateSourceStrings({id, text(" split surfaceMaskPixels must preserve strict surface alpha mask")}));
    return 1;
}

// Existing regression.d10519-10567.
std::size_t runDepthDrawGlobalDepthScaleFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ConstSlice;
    using namespace nijigenerate::io::depthimage;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto equalBytes = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto casesValue = fixtureRequired(object, text("cases"), id);
    require(casesValue.is_array(), concatenateSourceStrings({id, text(".cases must be an array")}));
    for (auto caseValue : casesValue.sourceArrayValue()) {
        require(caseValue.is_object(), concatenateSourceStrings({id, text(".cases[] must be an object")}));
        auto caseObject = caseValue.sourceObjectRef();
        auto depthValue = fixtureRequired(caseObject, text("depth"), concatenateSourceStrings({id, text(".case")}));
        auto depth = static_cast<std::uint8_t>(fixtureNumber(depthValue,
            concatenateSourceStrings({id, text(".case.depth")})));
        auto scaleValue = fixtureRequired(caseObject, text("scale"), concatenateSourceStrings({id, text(".case")}));
        const bool useNan = scaleValue.is_string() &&
            fixtureString(scaleValue, concatenateSourceStrings({id, text(".case.scale")})).view() == "NaN";
        double scale = useNan ? std::numeric_limits<double>::quiet_NaN() :
            fixtureNumber(scaleValue, concatenateSourceStrings({id, text(".case.scale")}));
        auto centerValue = fixtureRequired(caseObject, text("center"), concatenateSourceStrings({id, text(".case")}));
        auto center = fixtureNumber(centerValue, concatenateSourceStrings({id, text(".case.center")}));
        auto caseExpectedValue = fixtureRequired(caseObject, text("expected"),
            concatenateSourceStrings({id, text(".case")}));
        auto expected = static_cast<std::uint8_t>(fixtureNumber(caseExpectedValue,
            concatenateSourceStrings({id, text(".case.expected")})));
        require(ngDepthDrawScaleDepthValueAroundCenter(depth, scale, center) == expected,
            concatenateSourceStrings({id, text(" scaleDepthValueAroundCenter must match depth-draw")}));
    }
    auto pixelsValue = fixtureRequired(object, text("pixels"), id);
    auto pixels = fixtureByteArray(pixelsValue, concatenateSourceStrings({id, text(".pixels")}));
    auto maskValue = fixtureRequired(object, text("mask"), id);
    auto mask = fixtureByteArray(maskValue, concatenateSourceStrings({id, text(".mask")}));
    auto scaleValue = fixtureRequired(object, text("scale"), id);
    auto scale = fixtureNumber(scaleValue, concatenateSourceStrings({id, text(".scale")}));
    auto expectedValue = fixtureRequired(object, text("expected"), id);
    require(expectedValue.is_object(), concatenateSourceStrings({id, text(".expected must be an object")}));
    auto expected = expectedValue.sourceObjectRef();

    auto result = ngDepthDrawApplyGlobalDepthScale(ConstSlice<std::uint8_t>(pixels), scale,
        ConstSlice<std::uint8_t>(mask));
    auto expectedMaskedValue = fixtureRequired(expected, text("applyGlobalDepthScale"),
        concatenateSourceStrings({id, text(".expected")}));
    require(expectedMaskedValue.is_object(),
        concatenateSourceStrings({id, text(".expected.applyGlobalDepthScale must be an object")}));
    auto expectedMasked = expectedMaskedValue.sourceObjectRef();
    auto maskedPixelsValue = fixtureRequired(expectedMasked, text("pixels"),
        concatenateSourceStrings({id, text(".expected.masked")}));
    auto expectedPixels = fixtureByteArray(maskedPixelsValue,
        concatenateSourceStrings({id, text(".expected.masked.pixels")}));
    require(equalBytes(result.pixels, expectedPixels),
        concatenateSourceStrings({id, text(" applyGlobalDepthScale pixels must match depth-draw")}));
    auto centroidValue = fixtureRequired(expectedMasked, text("centroid"),
        concatenateSourceStrings({id, text(".expected.masked")}));
    auto expectedCentroid = fixtureNumber(centroidValue, concatenateSourceStrings({id, text(".expected.masked.centroid")}));
    require(std::abs(result.centroid - expectedCentroid) <= 0.000001,
        concatenateSourceStrings({id, text(" applyGlobalDepthScale centroid must match depth-draw")}));
    auto countValue = fixtureRequired(expectedMasked, text("count"),
        concatenateSourceStrings({id, text(".expected.masked")}));
    auto expectedCount = static_cast<std::size_t>(fixtureNumber(countValue,
        concatenateSourceStrings({id, text(".expected.masked.count")})));
    require(result.count == expectedCount,
        concatenateSourceStrings({id, text(" applyGlobalDepthScale count must match depth-draw")}));

    auto noMaskResult = ngDepthDrawApplyGlobalDepthScale(ConstSlice<std::uint8_t>(pixels), scale);
    auto expectedNoMaskValue = fixtureRequired(expected, text("applyGlobalDepthScaleNoMask"),
        concatenateSourceStrings({id, text(".expected")}));
    require(expectedNoMaskValue.is_object(),
        concatenateSourceStrings({id, text(".expected.applyGlobalDepthScaleNoMask must be an object")}));
    auto expectedNoMask = expectedNoMaskValue.sourceObjectRef();
    auto noMaskPixelsValue = fixtureRequired(expectedNoMask, text("pixels"),
        concatenateSourceStrings({id, text(".expected.noMask")}));
    auto expectedNoMaskPixels = fixtureByteArray(noMaskPixelsValue,
        concatenateSourceStrings({id, text(".expected.noMask.pixels")}));
    require(equalBytes(noMaskResult.pixels, expectedNoMaskPixels),
        concatenateSourceStrings({id, text(" applyGlobalDepthScale no-mask pixels must match depth-draw")}));
    auto noMaskCentroidValue = fixtureRequired(expectedNoMask, text("centroid"),
        concatenateSourceStrings({id, text(".expected.noMask")}));
    auto expectedNoMaskCentroid = fixtureNumber(noMaskCentroidValue,
        concatenateSourceStrings({id, text(".expected.noMask.centroid")}));
    require(std::abs(noMaskResult.centroid - expectedNoMaskCentroid) <= 0.000001,
        concatenateSourceStrings({id, text(" applyGlobalDepthScale no-mask centroid must match depth-draw")}));
    auto noMaskCountValue = fixtureRequired(expectedNoMask, text("count"),
        concatenateSourceStrings({id, text(".expected.noMask")}));
    auto expectedNoMaskCount = static_cast<std::size_t>(fixtureNumber(noMaskCountValue,
        concatenateSourceStrings({id, text(".expected.noMask.count")})));
    require(noMaskResult.count == expectedNoMaskCount,
        concatenateSourceStrings({id, text(" applyGlobalDepthScale no-mask count must match depth-draw")}));
    return 1;
}

// Existing regression.d10433-10517.
std::size_t runDepthDrawPsdMaskExportFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngFloatToInt32;
    using nijigenerate::core::settings::SettingsJsonObject;
    using namespace nijigenerate::io::depthimage;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto equalBytes = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto expectedValue = fixtureRequired(object, text("expected"), id);
    require(expectedValue.is_object(), concatenateSourceStrings({id, text(".expected must be an object")}));
    auto expected = expectedValue.sourceObjectRef();
    const auto numberField = [&](const SettingsJsonObject& fields, RetainedString key,
        RetainedString ownerSuffix, RetainedString nameSuffix) {
        auto field = fixtureRequired(fields, key, concatenateSourceStrings({id, ownerSuffix}));
        return fixtureNumber(field, concatenateSourceStrings({id, nameSuffix}));
    };
    const auto bytesField = [&](const SettingsJsonObject& fields, RetainedString key,
        RetainedString ownerSuffix, RetainedString nameSuffix) {
        auto field = fixtureRequired(fields, key, concatenateSourceStrings({id, ownerSuffix}));
        return fixtureByteArray(field, concatenateSourceStrings({id, nameSuffix}));
    };

    auto maskCaseValue = fixtureRequired(object, text("maskCase"), id);
    require(maskCaseValue.is_object(), concatenateSourceStrings({id, text(".maskCase must be an object")}));
    auto maskCase = maskCaseValue.sourceObjectRef();
    auto alpha = bytesField(maskCase, text("alpha"), text(".maskCase"), text(".maskCase.alpha"));
    auto width = ngFloatToInt32(numberField(maskCase, text("width"), text(".maskCase"), text(".maskCase.width")));
    auto height = ngFloatToInt32(numberField(maskCase, text("height"), text(".maskCase"), text(".maskCase.height")));
    auto layerLeft = ngFloatToInt32(numberField(maskCase, text("layerLeft"), text(".maskCase"), text(".maskCase.layerLeft")));
    auto layerTop = ngFloatToInt32(numberField(maskCase, text("layerTop"), text(".maskCase"), text(".maskCase.layerTop")));
    auto mask = bytesField(maskCase, text("mask"), text(".maskCase"), text(".maskCase.mask"));
    auto maskWidth = ngFloatToInt32(numberField(maskCase, text("maskWidth"), text(".maskCase"), text(".maskCase.maskWidth")));
    auto maskHeight = ngFloatToInt32(numberField(maskCase, text("maskHeight"), text(".maskCase"), text(".maskCase.maskHeight")));
    auto maskLeft = ngFloatToInt32(numberField(maskCase, text("maskLeft"), text(".maskCase"), text(".maskCase.maskLeft")));
    auto maskTop = ngFloatToInt32(numberField(maskCase, text("maskTop"), text(".maskCase"), text(".maskCase.maskTop")));
    auto relativeValue = fixtureRequired(maskCase, text("positionRelativeToLayer"),
        concatenateSourceStrings({id, text(".maskCase")}));
    auto positionRelativeToLayer = fixtureBool(relativeValue,
        concatenateSourceStrings({id, text(".maskCase.positionRelativeToLayer")}));
    auto defaultColor = static_cast<std::uint8_t>(numberField(maskCase, text("defaultColor"),
        text(".maskCase"), text(".maskCase.defaultColor")));
    auto maskApplied = ngDepthDrawApplyPsdMaskToAlphaSource(alpha, width, height, layerLeft, layerTop,
        mask, maskWidth, maskHeight, maskLeft, maskTop, positionRelativeToLayer, defaultColor);
    auto expectedAlpha = bytesField(expected, text("maskAppliedAlpha"), text(".expected"), text(".expected.maskAppliedAlpha"));
    require(equalBytes(maskApplied, expectedAlpha),
        concatenateSourceStrings({id, text(" PSD mask alpha multiplication must match depth-draw")}));

    auto overrideCaseValue = fixtureRequired(object, text("depthOverrideCase"), id);
    require(overrideCaseValue.is_object(), concatenateSourceStrings({id, text(".depthOverrideCase must be an object")}));
    auto overrideCase = overrideCaseValue.sourceObjectRef();
    auto colorAlpha = bytesField(overrideCase, text("colorAlpha"),
        text(".depthOverrideCase"), text(".depthOverrideCase.colorAlpha"));
    auto depthPixels = bytesField(overrideCase, text("depthPixels"),
        text(".depthOverrideCase"), text(".depthOverrideCase.depthPixels"));
    auto overrideMasks = ngDepthDrawDepthOverrideMasksSource(colorAlpha, depthPixels);
    auto expectedOverrideValue = fixtureRequired(expected, text("depthOverride"),
        concatenateSourceStrings({id, text(".expected")}));
    require(expectedOverrideValue.is_object(),
        concatenateSourceStrings({id, text(".expected.depthOverride must be an object")}));
    auto expectedOverride = expectedOverrideValue.sourceObjectRef();
    auto expectedColor = bytesField(expectedOverride, text("colorMask"),
        text(".expected.depthOverride"), text(".expected.depthOverride.colorMask"));
    require(equalBytes(overrideMasks.colorMask, expectedColor),
        concatenateSourceStrings({id, text(" color mask must match depth-draw")}));
    auto expectedStrict = bytesField(expectedOverride, text("strictSurfaceMask"),
        text(".expected.depthOverride"), text(".expected.depthOverride.strictSurfaceMask"));
    require(equalBytes(overrideMasks.strictSurfaceMask, expectedStrict),
        concatenateSourceStrings({id, text(" strict surface mask must match depth-draw")}));
    auto expectedSurface = bytesField(expectedOverride, text("surfaceMask"),
        text(".expected.depthOverride"), text(".expected.depthOverride.surfaceMask"));
    require(equalBytes(overrideMasks.surfaceMask, expectedSurface),
        concatenateSourceStrings({id, text(" surface mask must match depth-draw")}));
    auto expectedRender = bytesField(expectedOverride, text("renderDepthMask"),
        text(".expected.depthOverride"), text(".expected.depthOverride.renderDepthMask"));
    require(equalBytes(overrideMasks.renderDepthMask, expectedRender),
        concatenateSourceStrings({id, text(" render depth mask must match depth-draw")}));

    auto exportCaseValue = fixtureRequired(object, text("exportCase"), id);
    require(exportCaseValue.is_object(), concatenateSourceStrings({id, text(".exportCase must be an object")}));
    auto exportCase = exportCaseValue.sourceObjectRef();
    auto sourceWidth = ngFloatToInt32(numberField(exportCase, text("sourceWidth"),
        text(".exportCase"), text(".exportCase.sourceWidth")));
    auto sourceHeight = ngFloatToInt32(numberField(exportCase, text("sourceHeight"),
        text(".exportCase"), text(".exportCase.sourceHeight")));
    auto sourceLeft = ngFloatToInt32(numberField(exportCase, text("sourceLeft"),
        text(".exportCase"), text(".exportCase.sourceLeft")));
    auto sourceTop = ngFloatToInt32(numberField(exportCase, text("sourceTop"),
        text(".exportCase"), text(".exportCase.sourceTop")));
    auto sourceAlpha = bytesField(exportCase, text("sourceAlpha"),
        text(".exportCase"), text(".exportCase.sourceAlpha"));
    auto preparedLeft = ngFloatToInt32(numberField(exportCase, text("preparedLeft"),
        text(".exportCase"), text(".exportCase.preparedLeft")));
    auto preparedTop = ngFloatToInt32(numberField(exportCase, text("preparedTop"),
        text(".exportCase"), text(".exportCase.preparedTop")));
    auto preparedWidth = ngFloatToInt32(numberField(exportCase, text("preparedWidth"),
        text(".exportCase"), text(".exportCase.preparedWidth")));
    auto preparedHeight = ngFloatToInt32(numberField(exportCase, text("preparedHeight"),
        text(".exportCase"), text(".exportCase.preparedHeight")));
    auto exportDepthPixels = bytesField(exportCase, text("exportDepthPixels"),
        text(".exportCase"), text(".exportCase.exportDepthPixels"));
    auto sourceDepthPixels = bytesField(exportCase, text("sourceDepthPixels"),
        text(".exportCase"), text(".exportCase.sourceDepthPixels"));
    auto coverageMask = bytesField(exportCase, text("coverageMask"),
        text(".exportCase"), text(".exportCase.coverageMask"));
    auto exported = ngDepthDrawPsdExportDepthPixelsSource(sourceWidth, sourceHeight, sourceLeft, sourceTop,
        sourceAlpha, preparedLeft, preparedTop, preparedWidth, preparedHeight,
        exportDepthPixels, sourceDepthPixels, coverageMask);
    auto expectedExport = bytesField(expected, text("exportDepthPixels"),
        text(".expected"), text(".expected.exportDepthPixels"));
    require(equalBytes(exported, expectedExport),
        concatenateSourceStrings({id, text(" PSD export depth pixels must match depth-draw")}));
    return 1;
}

// Existing regression.d10306-10362.
std::size_t runDepthDrawDepthSplitFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ConstSlice;
    using nijigenerate::compat::ngFloatToInt32;
    using namespace nijigenerate::io::depthimage;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto equalElements = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto widthValue = fixtureRequired(object, text("imageWidth"), id);
    auto imageWidth = ngFloatToInt32(fixtureNumber(widthValue,
        concatenateSourceStrings({id, text(".imageWidth")})));
    auto heightValue = fixtureRequired(object, text("imageHeight"), id);
    auto imageHeight = ngFloatToInt32(fixtureNumber(heightValue,
        concatenateSourceStrings({id, text(".imageHeight")})));
    auto layerValues = fixtureRequired(object, text("layers"), id);
    require(layerValues.is_array(), concatenateSourceStrings({id, text(".layers must be an array")}));
    Slice<DepthDrawSplitLayer> layers;
    auto entries = layerValues.sourceArrayValue();
    const auto count = entries.size();
    for (std::size_t i = 0; i < count; ++i) {
        auto layerValue = entries[i];
        const bool isObject = layerValue.is_object();
        const std::array<format::TypedFormatArgument, 2> typeArguments{
            format::ngFormatArgument(id), format::ngFormatArgument(i)};
        auto typeRendered = format::ngFormatTyped("%s.layers[%s] must be an object", typeArguments);
        auto typeMessage = nijigenerate::platform::detail::ngPathAllocation([&] {
            return RetainedString(std::move(typeRendered));
        });
        require(isObject, typeMessage);
        auto layerObject = layerValue.sourceObjectRef();
        DepthDrawSplitLayer layer;
        const std::array<format::TypedFormatArgument, 2> ownerArguments{
            format::ngFormatArgument(id), format::ngFormatArgument(i)};
        auto ownerRendered = format::ngFormatTyped("%s.layers[%s]", ownerArguments);
        auto owner = nijigenerate::platform::detail::ngPathAllocation([&] {
            return RetainedString(std::move(ownerRendered));
        });
        auto leftValue = fixtureRequired(layerObject, text("left"), owner);
        layer.left = ngFloatToInt32(fixtureNumber(leftValue, concatenateSourceStrings({owner, text(".left")})));
        auto topValue = fixtureRequired(layerObject, text("top"), owner);
        layer.top = ngFloatToInt32(fixtureNumber(topValue, concatenateSourceStrings({owner, text(".top")})));
        auto layerWidthValue = fixtureRequired(layerObject, text("width"), owner);
        layer.width = ngFloatToInt32(fixtureNumber(layerWidthValue, concatenateSourceStrings({owner, text(".width")})));
        auto layerHeightValue = fixtureRequired(layerObject, text("height"), owner);
        layer.height = ngFloatToInt32(fixtureNumber(layerHeightValue, concatenateSourceStrings({owner, text(".height")})));
        auto maskValue = fixtureRequired(layerObject, text("maskPixels"), owner);
        layer.maskPixels = fixtureByteArray(maskValue, concatenateSourceStrings({owner, text(".maskPixels")}));
        layers.append(layer);
    }
    auto stableValue = fixtureRequired(object, text("stableDepthPixels"), id);
    auto stableDepthPixels = fixtureByteArray(stableValue, concatenateSourceStrings({id, text(".stableDepthPixels")}));
    auto bandValue = fixtureRequired(object, text("contourBandMask"), id);
    auto contourBandMask = fixtureByteArray(bandValue, concatenateSourceStrings({id, text(".contourBandMask")}));
    auto expectedValue = fixtureRequired(object, text("expected"), id);
    require(expectedValue.is_object(), concatenateSourceStrings({id, text(".expected must be an object")}));
    auto expected = expectedValue.sourceObjectRef();

    auto visibleLayerMap = ngDepthDrawBuildVisibleLayerMapSource(imageWidth, imageHeight, layers);
    auto visibleValue = fixtureRequired(expected, text("visibleLayerMap"),
        concatenateSourceStrings({id, text(".expected")}));
    auto expectedVisible = fixtureIntArray(visibleValue, concatenateSourceStrings({id, text(".expected.visibleLayerMap")}));
    require(equalElements(visibleLayerMap, expectedVisible),
        concatenateSourceStrings({id, text(" visible layer map must match depth-draw")}));
    auto radiusZero = ngDepthDrawSeedLayerDepthPixelsSource(layers[0], 0, imageWidth, imageHeight,
        stableDepthPixels, visibleLayerMap, layers[0].maskPixels, layers, ConstSlice<std::uint8_t>{}, 0);
    auto zeroValue = fixtureRequired(expected, text("seedLayer0Radius0"),
        concatenateSourceStrings({id, text(".expected")}));
    auto expectedZero = fixtureByteArray(zeroValue, concatenateSourceStrings({id, text(".expected.seedLayer0Radius0")}));
    require(equalElements(radiusZero, expectedZero),
        concatenateSourceStrings({id, text(" radius 0 seed layer depth must match depth-draw")}));
    auto radiusOne = ngDepthDrawSeedLayerDepthPixelsSource(layers[0], 0, imageWidth, imageHeight,
        stableDepthPixels, visibleLayerMap, layers[0].maskPixels, layers, ConstSlice<std::uint8_t>{}, 1);
    auto oneValue = fixtureRequired(expected, text("seedLayer0Radius1"),
        concatenateSourceStrings({id, text(".expected")}));
    auto expectedOne = fixtureByteArray(oneValue, concatenateSourceStrings({id, text(".expected.seedLayer0Radius1")}));
    require(equalElements(radiusOne, expectedOne),
        concatenateSourceStrings({id, text(" radius 1 seed layer depth must match depth-draw")}));
    auto contour = ngDepthDrawSeedLayerDepthPixelsSource(layers[0], 0, imageWidth, imageHeight,
        stableDepthPixels, visibleLayerMap, layers[0].maskPixels, layers, contourBandMask, 0);
    auto contourValue = fixtureRequired(expected, text("seedLayer0Contour"),
        concatenateSourceStrings({id, text(".expected")}));
    auto expectedContour = fixtureByteArray(contourValue,
        concatenateSourceStrings({id, text(".expected.seedLayer0Contour")}));
    require(equalElements(contour, expectedContour),
        concatenateSourceStrings({id, text(" contour-skipped seed layer depth must match depth-draw")}));

    auto upperChecks = fixtureRequired(expected, text("upperChecks"), concatenateSourceStrings({id, text(".expected")}));
    require(upperChecks.is_array(), concatenateSourceStrings({id, text(".expected.upperChecks must be an array")}));
    auto upperZeroValue = upperChecks.sourceArrayValue()[0];
    auto upperZero = fixtureBool(upperZeroValue, concatenateSourceStrings({id, text(".expected.upperChecks[0]")}));
    require(upperZero == ngDepthDrawHasUpperLayerMaskNearbySource(layers, 0, 0, 0, 1),
        concatenateSourceStrings({id, text(" upper mask nearby check 0 must match depth-draw")}));
    auto upperOneValue = upperChecks.sourceArrayValue()[1];
    auto upperOne = fixtureBool(upperOneValue, concatenateSourceStrings({id, text(".expected.upperChecks[1]")}));
    require(upperOne == ngDepthDrawHasUpperLayerMaskNearbySource(layers, 0, 2, 1, 1),
        concatenateSourceStrings({id, text(" upper mask nearby check 1 must match depth-draw")}));
    auto upperTwoValue = upperChecks.sourceArrayValue()[2];
    auto upperTwo = fixtureBool(upperTwoValue, concatenateSourceStrings({id, text(".expected.upperChecks[2]")}));
    require(upperTwo == ngDepthDrawHasUpperLayerMaskNearbySource(layers, 0, 0, 1, 0),
        concatenateSourceStrings({id, text(" upper mask nearby check 2 must match depth-draw")}));
    return 1;
}

// Existing regression.d10383-10431.
std::size_t runDepthDrawLayerBoundsSamplingFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ConstSlice;
    using nijigenerate::compat::ngFloatToInt32;
    using nijigenerate::core::settings::SettingsJsonObject;
    using namespace nijigenerate::io::depthimage;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto equalBytes = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto depthLayerValue = fixtureRequired(object, text("depthLayer"), id);
    auto colorLayerValue = fixtureRequired(object, text("colorLayer"), id);
    auto expectedValue = fixtureRequired(object, text("expected"), id);
    require(depthLayerValue.is_object(), concatenateSourceStrings({id, text(".depthLayer must be an object")}));
    require(colorLayerValue.is_object(), concatenateSourceStrings({id, text(".colorLayer must be an object")}));
    require(expectedValue.is_object(), concatenateSourceStrings({id, text(".expected must be an object")}));
    auto depthLayer = depthLayerValue.sourceObjectRef();
    auto colorLayer = colorLayerValue.sourceObjectRef();
    auto expected = expectedValue.sourceObjectRef();

    auto pixelsValue = fixtureRequired(depthLayer, text("pixels"),
        concatenateSourceStrings({id, text(".depthLayer")}));
    auto depthPixels = fixtureByteArray(pixelsValue, concatenateSourceStrings({id, text(".depthLayer.pixels")}));
    auto alphaValue = fixtureRequired(depthLayer, text("alphaMask"),
        concatenateSourceStrings({id, text(".depthLayer")}));
    auto alphaMask = fixtureByteArray(alphaValue, concatenateSourceStrings({id, text(".depthLayer.alphaMask")}));
    // Lower each authored dimension expression, including its repeated lookups.
    const auto dimension = [&](const SettingsJsonObject& layer, RetainedString key,
        RetainedString ownerSuffix, RetainedString nameSuffix) {
        auto field = fixtureRequired(layer, key, concatenateSourceStrings({id, ownerSuffix}));
        return ngFloatToInt32(fixtureNumber(field, concatenateSourceStrings({id, nameSuffix})));
    };
    auto depthWidth = dimension(depthLayer, text("width"), text(".depthLayer"), text(".depthLayer.width"));
    auto depthHeight = dimension(depthLayer, text("height"), text(".depthLayer"), text(".depthLayer.height"));
    auto depthLeft = dimension(depthLayer, text("left"), text(".depthLayer"), text(".depthLayer.left"));
    auto depthTop = dimension(depthLayer, text("top"), text(".depthLayer"), text(".depthLayer.top"));
    auto colorWidth = dimension(colorLayer, text("width"), text(".colorLayer"), text(".colorLayer.width"));
    auto colorHeight = dimension(colorLayer, text("height"), text(".colorLayer"), text(".colorLayer.height"));
    auto colorLeft = dimension(colorLayer, text("left"), text(".colorLayer"), text(".colorLayer.left"));
    auto colorTop = dimension(colorLayer, text("top"), text(".colorLayer"), text(".colorLayer.top"));
    auto sampled = ngDepthDrawSampleDepthLayerToColorLayerSource(depthPixels,
        depthWidth, depthHeight, depthLeft, depthTop, alphaMask, colorWidth, colorHeight, colorLeft, colorTop);
    auto sampledValue = fixtureRequired(expected, text("sampled"), concatenateSourceStrings({id, text(".expected")}));
    auto expectedSampled = fixtureByteArray(sampledValue, concatenateSourceStrings({id, text(".expected.sampled")}));
    require(equalBytes(sampled, expectedSampled),
        concatenateSourceStrings({id, text(" sampleDepthLayerToColorLayer must match depth-draw with alpha mask")}));

    auto noMaskDepthWidth = dimension(depthLayer, text("width"), text(".depthLayer"), text(".depthLayer.width"));
    auto noMaskDepthHeight = dimension(depthLayer, text("height"), text(".depthLayer"), text(".depthLayer.height"));
    auto noMaskDepthLeft = dimension(depthLayer, text("left"), text(".depthLayer"), text(".depthLayer.left"));
    auto noMaskDepthTop = dimension(depthLayer, text("top"), text(".depthLayer"), text(".depthLayer.top"));
    auto noMaskColorWidth = dimension(colorLayer, text("width"), text(".colorLayer"), text(".colorLayer.width"));
    auto noMaskColorHeight = dimension(colorLayer, text("height"), text(".colorLayer"), text(".colorLayer.height"));
    auto noMaskColorLeft = dimension(colorLayer, text("left"), text(".colorLayer"), text(".colorLayer.left"));
    auto noMaskColorTop = dimension(colorLayer, text("top"), text(".colorLayer"), text(".colorLayer.top"));
    auto sampledWithoutAlphaMask = ngDepthDrawSampleDepthLayerToColorLayerSource(depthPixels,
        noMaskDepthWidth, noMaskDepthHeight, noMaskDepthLeft, noMaskDepthTop, ConstSlice<std::uint8_t>{},
        noMaskColorWidth, noMaskColorHeight, noMaskColorLeft, noMaskColorTop);
    auto noMaskValue = fixtureRequired(expected, text("sampledWithoutAlphaMask"),
        concatenateSourceStrings({id, text(".expected")}));
    auto expectedNoMask = fixtureByteArray(noMaskValue,
        concatenateSourceStrings({id, text(".expected.sampledWithoutAlphaMask")}));
    require(equalBytes(sampledWithoutAlphaMask, expectedNoMask),
        concatenateSourceStrings({id, text(" sampleDepthLayerToColorLayer must match depth-draw without alpha mask")}));
    return 1;
}

// Existing regression.d10240-10270.
std::size_t runDepthDrawDepthCleanupFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngFloatToInt32;
    using namespace nijigenerate::io::depthimage;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto equalBytes = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto widthValue = fixtureRequired(object, text("width"), id);
    auto width = ngFloatToInt32(fixtureNumber(widthValue, concatenateSourceStrings({id, text(".width")})));
    auto heightValue = fixtureRequired(object, text("height"), id);
    auto height = ngFloatToInt32(fixtureNumber(heightValue, concatenateSourceStrings({id, text(".height")})));
    auto maskValue = fixtureRequired(object, text("maskPixels"), id);
    auto maskPixels = fixtureByteArray(maskValue, concatenateSourceStrings({id, text(".maskPixels")}));
    auto depthValue = fixtureRequired(object, text("depthPixels"), id);
    auto depthPixels = fixtureByteArray(depthValue, concatenateSourceStrings({id, text(".depthPixels")}));
    auto expectedValue = fixtureRequired(object, text("expected"), id);
    require(expectedValue.is_object(), concatenateSourceStrings({id, text(".expected must be an object")}));
    auto expected = expectedValue.sourceObjectRef();

    auto contour = ngDepthDrawBuildLayerContourBandMaskSource(maskPixels, width, height, 2);
    auto contourValue = fixtureRequired(expected, text("contourBandThickness2"),
        concatenateSourceStrings({id, text(".expected")}));
    auto expectedContour = fixtureByteArray(contourValue,
        concatenateSourceStrings({id, text(".expected.contourBandThickness2")}));
    require(equalBytes(contour, expectedContour),
        concatenateSourceStrings({id, text(" contour band mask must match depth-draw")}));
    auto eroded = ngDepthDrawErodePositiveDepthMaskSource(depthPixels, width, height, 1);
    auto erodedValue = fixtureRequired(expected, text("erodedPositiveThickness1"),
        concatenateSourceStrings({id, text(".expected")}));
    auto expectedEroded = fixtureByteArray(erodedValue,
        concatenateSourceStrings({id, text(".expected.erodedPositiveThickness1")}));
    require(equalBytes(eroded, expectedEroded),
        concatenateSourceStrings({id, text(" positive-depth erosion must match depth-draw")}));
    auto inpaint = ngDepthDrawInpaintMaskedLayerDepthSource(depthPixels, maskPixels, width, height);
    auto inpaintValue = fixtureRequired(expected, text("inpaintPixels"),
        concatenateSourceStrings({id, text(".expected")}));
    auto expectedInpaintPixels = fixtureByteArray(inpaintValue,
        concatenateSourceStrings({id, text(".expected.inpaintPixels")}));
    const bool sameInpaint = equalBytes(inpaint.pixels, expectedInpaintPixels);
    const std::array<format::TypedFormatArgument, 3> inpaintArguments{
        format::ngFormatArgument(id), format::ngFormatArgument(inpaint.pixels),
        format::ngFormatArgument(expectedInpaintPixels)};
    auto inpaintRendered = format::ngFormatTyped(
        "%s inpainted depth pixels must match depth-draw actual=%s expected=%s", inpaintArguments);
    auto inpaintMessage = nijigenerate::platform::detail::ngPathAllocation([&] {
        return RetainedString(std::move(inpaintRendered));
    });
    require(sameInpaint, inpaintMessage);
    auto filledValue = fixtureRequired(expected, text("inpaintFilledMask"),
        concatenateSourceStrings({id, text(".expected")}));
    auto expectedFilledMask = fixtureByteArray(filledValue,
        concatenateSourceStrings({id, text(".expected.inpaintFilledMask")}));
    require(equalBytes(inpaint.filledMask, expectedFilledMask),
        concatenateSourceStrings({id, text(" inpaint filled mask must match depth-draw")}));
    auto smoothed = ngDepthDrawSmoothMaskedPositiveDepthSource(depthPixels, maskPixels, width, height);
    auto smoothValue = fixtureRequired(expected, text("smooth"), concatenateSourceStrings({id, text(".expected")}));
    auto expectedSmooth = fixtureByteArray(smoothValue, concatenateSourceStrings({id, text(".expected.smooth")}));
    require(equalBytes(smoothed, expectedSmooth),
        concatenateSourceStrings({id, text(" smoothed depth pixels must match depth-draw")}));
    return 1;
}

// Existing regression.d10272-10304.
std::size_t runDepthDrawDepthPruneFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngFloatToInt32;
    using namespace nijigenerate::io::depthimage;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto equalBytes = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto imageWidthValue = fixtureRequired(object, text("imageWidth"), id);
    auto imageWidth = ngFloatToInt32(fixtureNumber(imageWidthValue,
        concatenateSourceStrings({id, text(".imageWidth")})));
    auto imageHeightValue = fixtureRequired(object, text("imageHeight"), id);
    auto imageHeight = ngFloatToInt32(fixtureNumber(imageHeightValue,
        concatenateSourceStrings({id, text(".imageHeight")})));
    auto layerValue = fixtureRequired(object, text("layer"), id);
    require(layerValue.is_object(), concatenateSourceStrings({id, text(".layer must be an object")}));
    auto layerObject = layerValue.sourceObjectRef();
    DepthDrawPruneLayer layer;
    auto leftValue = fixtureRequired(layerObject, text("left"), concatenateSourceStrings({id, text(".layer")}));
    layer.left = ngFloatToInt32(fixtureNumber(leftValue, concatenateSourceStrings({id, text(".layer.left")})));
    auto topValue = fixtureRequired(layerObject, text("top"), concatenateSourceStrings({id, text(".layer")}));
    layer.top = ngFloatToInt32(fixtureNumber(topValue, concatenateSourceStrings({id, text(".layer.top")})));
    auto widthValue = fixtureRequired(layerObject, text("width"), concatenateSourceStrings({id, text(".layer")}));
    layer.width = ngFloatToInt32(fixtureNumber(widthValue, concatenateSourceStrings({id, text(".layer.width")})));
    auto heightValue = fixtureRequired(layerObject, text("height"), concatenateSourceStrings({id, text(".layer")}));
    layer.height = ngFloatToInt32(fixtureNumber(heightValue, concatenateSourceStrings({id, text(".layer.height")})));
    auto layerIndexValue = fixtureRequired(object, text("layerIndex"), id);
    auto layerIndex = ngFloatToInt32(fixtureNumber(layerIndexValue,
        concatenateSourceStrings({id, text(".layerIndex")})));
    auto thresholdValue = fixtureRequired(object, text("threshold"), id);
    auto threshold = fixtureNumber(thresholdValue, concatenateSourceStrings({id, text(".threshold")}));
    auto depthValue = fixtureRequired(object, text("depthPixels"), id);
    auto depthPixels = fixtureByteArray(depthValue, concatenateSourceStrings({id, text(".depthPixels")}));
    auto maskValue = fixtureRequired(object, text("maskPixels"), id);
    auto maskPixels = fixtureByteArray(maskValue, concatenateSourceStrings({id, text(".maskPixels")}));
    auto stableValue = fixtureRequired(object, text("stableDepthPixels"), id);
    auto stableDepthPixels = fixtureByteArray(stableValue,
        concatenateSourceStrings({id, text(".stableDepthPixels")}));
    auto visibleValue = fixtureRequired(object, text("visibleLayerMap"), id);
    auto visibleLayerMap = fixtureIntArray(visibleValue, concatenateSourceStrings({id, text(".visibleLayerMap")}));
    auto expectedValue = fixtureRequired(object, text("expected"), id);
    require(expectedValue.is_object(), concatenateSourceStrings({id, text(".expected must be an object")}));
    auto expected = expectedValue.sourceObjectRef();

    auto pruned = ngDepthDrawPruneForeignDepthSeeds(depthPixels, layer, layerIndex, imageWidth, imageHeight,
        stableDepthPixels, visibleLayerMap, maskPixels, threshold);
    auto pixelsValue = fixtureRequired(expected, text("pixels"), concatenateSourceStrings({id, text(".expected")}));
    auto pixels = fixtureByteArray(pixelsValue, concatenateSourceStrings({id, text(".expected.pixels")}));
    require(equalBytes(pruned.pixels, pixels),
        concatenateSourceStrings({id, text(" pruned depth pixels must match depth-draw")}));
    auto stateValue = fixtureRequired(expected, text("debugState"), concatenateSourceStrings({id, text(".expected")}));
    auto state = fixtureByteArray(stateValue, concatenateSourceStrings({id, text(".expected.debugState")}));
    require(equalBytes(pruned.debugState, state),
        concatenateSourceStrings({id, text(" prune debug state must match depth-draw")}));
    auto scoreValue = fixtureRequired(expected, text("debugScore"), concatenateSourceStrings({id, text(".expected")}));
    auto score = fixtureByteArray(scoreValue, concatenateSourceStrings({id, text(".expected.debugScore")}));
    require(equalBytes(pruned.debugScore, score),
        concatenateSourceStrings({id, text(" prune debug score must match depth-draw")}));
    return 1;
}

// Existing regression.d10205-10238.
std::size_t runDepthDrawAlphaDepthGapFillFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngFloatToInt32;
    using namespace nijigenerate::io::depthimage;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto equalBytes = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto widthValue = fixtureRequired(object, text("width"), id);
    auto width = ngFloatToInt32(fixtureNumber(widthValue, concatenateSourceStrings({id, text(".width")})));
    auto heightValue = fixtureRequired(object, text("height"), id);
    auto height = ngFloatToInt32(fixtureNumber(heightValue, concatenateSourceStrings({id, text(".height")})));
    auto layerValue = fixtureRequired(object, text("layerIndex"), id);
    auto layerIndex = ngFloatToInt32(fixtureNumber(layerValue, concatenateSourceStrings({id, text(".layerIndex")})));
    auto depthValue = fixtureRequired(object, text("depthPixels"), id);
    auto depthPixels = fixtureByteArray(depthValue, concatenateSourceStrings({id, text(".depthPixels")}));
    auto alphaValue = fixtureRequired(object, text("alphaMask"), id);
    auto alphaMask = fixtureByteArray(alphaValue, concatenateSourceStrings({id, text(".alphaMask")}));
    auto expectedValue = fixtureRequired(object, text("expected"), id);
    require(expectedValue.is_object(), concatenateSourceStrings({id, text(".expected must be an object")}));
    auto expected = expectedValue.sourceObjectRef();

    auto detected = ngDepthDrawDetectAlphaDepthGapsSource(depthPixels, alphaMask, width, height, layerIndex);
    auto maskValue = fixtureRequired(expected, text("detectedMask"), concatenateSourceStrings({id, text(".expected")}));
    auto mask = fixtureByteArray(maskValue, concatenateSourceStrings({id, text(".expected.detectedMask")}));
    require(equalBytes(detected.mask, mask),
        concatenateSourceStrings({id, text(" detected alpha-depth gap mask must match depth-draw")}));
    auto zeroValue = fixtureRequired(expected, text("zero"), concatenateSourceStrings({id, text(".expected")}));
    auto zero = static_cast<std::size_t>(fixtureNumber(zeroValue, concatenateSourceStrings({id, text(".expected.zero")})));
    require(detected.zero == zero, concatenateSourceStrings({id, text(" zero gap count must match depth-draw")}));
    auto depressionValue = fixtureRequired(expected, text("depression"), concatenateSourceStrings({id, text(".expected")}));
    auto depression = static_cast<std::size_t>(fixtureNumber(depressionValue,
        concatenateSourceStrings({id, text(".expected.depression")})));
    require(detected.depression == depression,
        concatenateSourceStrings({id, text(" depression gap count must match depth-draw")}));
    auto cliffValue = fixtureRequired(expected, text("cliff"), concatenateSourceStrings({id, text(".expected")}));
    auto cliff = static_cast<std::size_t>(fixtureNumber(cliffValue, concatenateSourceStrings({id, text(".expected.cliff")})));
    require(detected.cliff == cliff, concatenateSourceStrings({id, text(" cliff gap count must match depth-draw")}));
    auto focusedValue = fixtureRequired(expected, text("focusedAdded"), concatenateSourceStrings({id, text(".expected")}));
    auto focusedAdded = static_cast<std::size_t>(fixtureNumber(focusedValue,
        concatenateSourceStrings({id, text(".expected.focusedAdded")})));
    require(detected.focusedAdded == focusedAdded,
        concatenateSourceStrings({id, text(" focused gap count must match depth-draw")}));
    auto totalValue = fixtureRequired(expected, text("total"), concatenateSourceStrings({id, text(".expected")}));
    auto total = static_cast<std::size_t>(fixtureNumber(totalValue, concatenateSourceStrings({id, text(".expected.total")})));
    require(detected.total == total, concatenateSourceStrings({id, text(" total gap count must match depth-draw")}));

    auto filled = ngDepthDrawMedianFillDepth(
        std::span<const std::uint8_t>(depthPixels.data(), depthPixels.size()),
        std::span<const std::uint8_t>(alphaMask.data(), alphaMask.size()),
        std::span<const std::uint8_t>(detected.mask.data(), detected.mask.size()), width, height);
    auto filledDepthValue = fixtureRequired(expected, text("filledDepth"), concatenateSourceStrings({id, text(".expected")}));
    auto filledDepth = fixtureByteArray(filledDepthValue, concatenateSourceStrings({id, text(".expected.filledDepth")}));
    require(equalBytes(filled.depth, filledDepth),
        concatenateSourceStrings({id, text(" median-filled depth must match depth-draw")}));
    auto filledValue = fixtureRequired(expected, text("filled"), concatenateSourceStrings({id, text(".expected")}));
    auto filledCount = static_cast<std::size_t>(fixtureNumber(filledValue,
        concatenateSourceStrings({id, text(".expected.filled")})));
    require(filled.filled == filledCount, concatenateSourceStrings({id, text(" filled count must match depth-draw")}));
    auto remainingValue = fixtureRequired(expected, text("remaining"), concatenateSourceStrings({id, text(".expected")}));
    auto remaining = static_cast<std::size_t>(fixtureNumber(remainingValue,
        concatenateSourceStrings({id, text(".expected.remaining")})));
    require(filled.remaining == remaining, concatenateSourceStrings({id, text(" remaining count must match depth-draw")}));
    return 1;
}

// Existing regression.d10569-10598.
std::size_t runDepthDrawGridConvolutionFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngFloatToInt32;
    using namespace nijigenerate::io::depthimage;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto widthValue = fixtureRequired(object, text("width"), id);
    auto width = ngFloatToInt32(fixtureNumber(widthValue, concatenateSourceStrings({id, text(".width")})));
    auto heightValue = fixtureRequired(object, text("height"), id);
    auto height = ngFloatToInt32(fixtureNumber(heightValue, concatenateSourceStrings({id, text(".height")})));
    auto sourceValue = fixtureRequired(object, text("sourcePixels"), id);
    auto sourcePixels = fixtureByteArray(sourceValue, concatenateSourceStrings({id, text(".sourcePixels")}));
    auto maskValue = fixtureRequired(object, text("maskPixels"), id);
    auto maskPixels = fixtureByteArray(maskValue, concatenateSourceStrings({id, text(".maskPixels")}));
    auto casesValue = fixtureRequired(object, text("cases"), id);
    require(casesValue.is_array(), concatenateSourceStrings({id, text(".cases must be an array")}));
    auto cases = casesValue.sourceArrayValue();
    const auto caseCount = cases.size();
    for (std::size_t caseIndex = 0; caseIndex < caseCount; ++caseIndex) {
        auto caseValue = cases[caseIndex];
        const bool isObject = caseValue.is_object();
        const std::array<format::TypedFormatArgument, 2> typeArguments{
            format::ngFormatArgument(id), format::ngFormatArgument(caseIndex)};
        auto typeRendered = format::ngFormatTyped("%s.cases[%s] must be an object", typeArguments);
        auto typeMessage = nijigenerate::platform::detail::ngPathAllocation([&] {
            return RetainedString(std::move(typeRendered));
        });
        require(isObject, typeMessage);
        auto caseObject = caseValue.sourceObjectRef();
        auto nameValue = fixtureRequired(caseObject, text("name"), concatenateSourceStrings({id, text(".case")}));
        auto caseName = fixtureString(nameValue, concatenateSourceStrings({id, text(".case.name")}));
        auto specValue = fixtureRequired(caseObject, text("specMode"), caseName);
        auto specMode = fixtureString(specValue, concatenateSourceStrings({caseName, text(".specMode")}));
        auto gridXValue = fixtureRequired(caseObject, text("gridX"), caseName);
        auto gridX = ngFloatToInt32(fixtureNumber(gridXValue, concatenateSourceStrings({caseName, text(".gridX")})));
        auto gridYValue = fixtureRequired(caseObject, text("gridY"), caseName);
        auto gridY = ngFloatToInt32(fixtureNumber(gridYValue, concatenateSourceStrings({caseName, text(".gridY")})));
        auto kernelValue = fixtureRequired(caseObject, text("kernelSize"), caseName);
        auto kernelSize = ngFloatToInt32(fixtureNumber(kernelValue,
            concatenateSourceStrings({caseName, text(".kernelSize")})));
        auto interpolationValue = fixtureRequired(caseObject, text("interpMode"), caseName);
        auto interpMode = fixtureString(interpolationValue, concatenateSourceStrings({caseName, text(".interpMode")}));
        auto actual = ngDepthDrawCreateMaskedGridDepthPixelsSource(
            width, height, sourcePixels, maskPixels, specMode, gridX, gridY, kernelSize, interpMode);
        auto pixelsValue = fixtureRequired(caseObject, text("pixels"), caseName);
        auto expected = fixtureByteArray(pixelsValue, concatenateSourceStrings({caseName, text(".pixels")}));
        const bool equal = actual.size() == expected.size() &&
            (actual.empty() || std::equal(actual.begin(), actual.end(), expected.begin()));
        const std::array<format::TypedFormatArgument, 2> resultArguments{
            format::ngFormatArgument(id), format::ngFormatArgument(caseName)};
        auto resultRendered = format::ngFormatTyped("%s.%s masked grid convolution must match depth-draw", resultArguments);
        auto resultMessage = nijigenerate::platform::detail::ngPathAllocation([&] {
            return RetainedString(std::move(resultRendered));
        });
        require(equal, resultMessage);
    }
    return 1;
}
}
