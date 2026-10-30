// Existing regression.d5612-5618 and 5878-5887. Include once inside detail.
nijigenerate::io::depthmap_psd::PsdDepthImportResult composePsdDepthImportForRegression(
    nijigenerate::io::depthmap_psd::PsdDepthImportResult imported) {
    using nijigenerate::compat::RetainedString;
    nijigenerate::commands::depth::map::PsdDepthComposedView composed;
    RetainedString composeError;
    auto composedOk = nijigenerate::commands::depth::map::ngComposePsdDepthImportResult(
        imported, composed, composeError);
    require(composedOk, concatenateSourceStrings({
        RetainedString::fromStaticLiteral("PSD depth import regression compose should succeed: "), composeError}));
    return imported;
}

auto applyPsdDepthImportForRegression(nijigenerate::io::depthmap_psd::PsdDepthImportResult imported) {
    using nijigenerate::compat::RetainedString;
    using namespace nijigenerate::commands::depth::map;
    if (imported.composedLayers.size() == 0) {
        return ngApplyPsdDepthImportResult(ngPsdDepthComposedViewForRegression(imported));
    }
    PsdDepthComposedView composed;
    RetainedString composeError;
    auto composedOk = ngComposePsdDepthImportResult(imported, composed, composeError);
    require(composedOk, concatenateSourceStrings({
        RetainedString::fromStaticLiteral("PSD depth import regression compose should succeed: "), composeError}));
    return ngApplyPsdDepthImportResult(composed);
}
