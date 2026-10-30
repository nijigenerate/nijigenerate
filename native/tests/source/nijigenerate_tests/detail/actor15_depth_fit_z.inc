// Complete existing regression.d11562-11635 only.
void testDepthDrawFitZContracts() {
    using Text = nijigenerate::compat::RetainedString;
    using Floats = nijigenerate::compat::Slice<float>;
    using namespace nijigenerate::viewport::depth::draw;
    using Ranges = nijigenerate::compat::Slice<DepthDrawRange>;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto rawRange = ngDepthDrawRangeFromValues(Floats{-0.5f, 0.0f, 0.5f});
    check(rawRange.valid && near(rawRange.minDepth, -0.5f) && near(rawRange.maxDepth, 0.5f),
        "DepthDraw Fit Z should measure finite raw sample ranges");
    auto fit = ngDepthDrawFitZToGap(rawRange, -0.2f, 0.8f, 0.1f);
    const bool fitSucceeded = fit.succeeded;
    auto fitMessage = concatenateSourceStrings({text("DepthDraw Fit Z should fit a valid raw range into a valid gap: "), fit.error});
    require(fitSucceeded, fitMessage);
    check(near(fit.targetMin, -0.1f) && near(fit.targetMax, 0.7f),
        "DepthDraw Fit Z should apply margin to both gap endpoints");
    check(near(fit.zScale, 0.8f) && near(fit.zOffset, 0.3f),
        "DepthDraw Fit Z should compute z scale and z offset from raw endpoints");
    DepthDrawLayer layer;
    layer.id = text("fit-layer");
    layer.depthPixels = {0, 0, 0, 255};
    auto pixelsBefore = layer.depthPixels.duplicate();
    auto applyFit = ngDepthDrawApplyFitZToGap(layer, rawRange, -0.2f, 0.8f, 0.1f);
    check(applyFit.succeeded && near(layer.zScale, 0.8f) && near(layer.zOffset, 0.3f),
        "DepthDraw Fit Z apply should update only layer z transform values");
    const auto pixelsUnchanged = [&] {
        if (layer.depthPixels.size() != pixelsBefore.size()) return false;
        for (std::size_t i = 0; i < pixelsBefore.size(); ++i) if (layer.depthPixels[i] != pixelsBefore[i]) return false;
        return true;
    };
    check(pixelsUnchanged(), "DepthDraw Fit Z must not modify source pixels");
    auto flatFit = ngDepthDrawFitZToGap(ngDepthDrawRangeFromValues(Floats{0.25f, 0.25f}), 0.0f, 1.0f, 0.0f);
    check(flatFit.succeeded && near(flatFit.zScale, 1.0f) && near(flatFit.zOffset, 0.25f),
        "DepthDraw Fit Z should center a flat raw range in the target gap");
    auto emptyRaw = ngDepthDrawFitZToGap(ngDepthDrawRangeFromValues(Floats{}), 0.0f, 1.0f, 0.0f);
    check(!emptyRaw.succeeded && !emptyRaw.error.empty(), "DepthDraw Fit Z should reject empty raw ranges");
    auto emptyGap = ngDepthDrawFitZToGap(rawRange, 0.0f, 0.1f, 0.1f);
    check(!emptyGap.succeeded && !emptyGap.error.empty(), "DepthDraw Fit Z should reject gaps consumed by margin");
    auto zeroWidthGap = ngDepthDrawFitZToGap(rawRange, 0.0f, 0.1f, 0.05f);
    check(!zeroWidthGap.succeeded && !zeroWidthGap.error.empty(), "DepthDraw Fit Z should reject zero-width gaps after margin");
    auto backRange = ngDepthDrawRangeFromValues(Floats{-0.9f, -0.6f});
    auto currentRange = ngDepthDrawRangeFromValues(Floats{-0.2f, 0.2f});
    auto frontRange = ngDepthDrawRangeFromValues(Floats{0.6f, 0.9f});
    auto adjacentGap = ngDepthDrawGapFromAdjacentRanges(Ranges{backRange, currentRange, frontRange}, 1);
    check(adjacentGap.valid && near(adjacentGap.back, -0.6f) && near(adjacentGap.front, 0.6f),
        "DepthDraw Fit Z adjacent gap should use previous max and next min");
    auto adjacentFit = ngDepthDrawFitZToGap(currentRange, adjacentGap, 0.1f);
    check(adjacentFit.succeeded && near(adjacentFit.targetMin, -0.5f) && near(adjacentFit.targetMax, 0.5f),
        "DepthDraw Fit Z adjacent gap should feed manual gap fitting with margin");
    auto missingAdjacentGap = ngDepthDrawGapFromAdjacentRanges(Ranges{backRange, currentRange}, 1);
    check(!missingAdjacentGap.valid && !missingAdjacentGap.error.empty(),
        "DepthDraw Fit Z adjacent gap should reject missing previous or next layer");
    auto overlapAdjacentGap = ngDepthDrawGapFromAdjacentRanges(Ranges{
        ngDepthDrawRangeFromValues(Floats{0.2f, 0.8f}), currentRange,
        ngDepthDrawRangeFromValues(Floats{0.1f, 0.4f})}, 1);
    check(!overlapAdjacentGap.valid && !overlapAdjacentGap.error.empty(),
        "DepthDraw Fit Z adjacent gap should reject overlapping neighboring ranges");
    auto targetGap = ngDepthDrawGapFromTargetRange(ngDepthDrawRangeFromValues(Floats{-0.25f, 0.75f}));
    check(targetGap.valid && near(targetGap.back, -0.25f) && near(targetGap.front, 0.75f),
        "DepthDraw Fit Z target gap should use target base depth range endpoints");
    auto selectedGap = ngDepthDrawGapFromSelectedLayerRanges(backRange, frontRange);
    check(selectedGap.valid && near(selectedGap.back, -0.6f) && near(selectedGap.front, 0.6f),
        "DepthDraw Fit Z selected layer gap should use selected back max and front min");
    auto diagnostics = ngDepthDrawFitZDiagnostics(currentRange, adjacentGap, adjacentFit);
    check(diagnostics.rawRangeValid && diagnostics.gapValid && diagnostics.succeeded &&
        near(diagnostics.rawMin, -0.2f) && near(diagnostics.rawMax, 0.2f) &&
        near(diagnostics.gapBack, -0.6f) && near(diagnostics.gapFront, 0.6f) &&
        near(diagnostics.zScale, adjacentFit.zScale) && near(diagnostics.zOffset, adjacentFit.zOffset),
        "DepthDraw Fit Z diagnostics should expose raw range, gap, and transform values");
}
