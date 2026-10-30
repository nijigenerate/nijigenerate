// Existing regression.d10867-11560 only; include inside regression::detail after original helpers.
void testDepthDrawCalculationGateContracts() {
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return nijigenerate::compat::RetainedString::fromStaticLiteral(value);
    };
    const auto buildPath = []<class... Pieces>(Pieces... pieces) {
        const std::array<nijigenerate::compat::RetainedString, sizeof...(Pieces)> segments{pieces...};
        return nijigenerate::compat::phobos::ngBuildPath(segments);
    };
    requireSourceContains(
        text("dub.sdl"),
        text("dependency \"psd-d\" path=\"vendor/psd-d\""),
        text("DepthDraw PSD writeback boundary assumes the current vendored psd-d dependency"));
    requireSourceContains(
        buildPath(text("vendor"), text("psd-d"), text("source"), text("psd"), text("package.d")),
        text("public import psd.parser : parseDocument;"),
        text("DepthDraw PSD writeback boundary assumes psd-d exposes PSD reading"));
    const auto psdWriterNeedles = nijigenerate::compat::Slice<nijigenerate::compat::RetainedString>{
        text("writePsd"),
        text("writePSD"),
        text("PSDWriter"),
        text("writeDocument"),
        text("saveDocument"),
        text("serializeDocument"),
        text("LayeredPsd"),
        text("PsdWriter"),
    };
    require(!sourceTreeContainsAny(buildPath(text("vendor"), text("psd-d"), text("source")), psdWriterNeedles),
        text("DepthDraw layered PSD writeback must remain blocked until psd-d exposes a verified writer"));
    require(!sourceTreeContainsAny(buildPath(text("source"), text("nijigenerate"), text("io")), psdWriterNeedles),
        text("DepthDraw layered PSD writeback must remain blocked until nijigenerate IO has a verified writer"));
    require(!sourceTreeContainsAny(buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw")), psdWriterNeedles),
        text("DepthDraw layered PSD writeback must remain blocked until DepthDraw has a verified writer"));
    requireSourceContains(
        text("dub.sdl"),
        text("configuration \"osx-full\""),
        text("DepthDraw Mac OpenGL verification boundary requires the macOS render build configuration"));
    requireSourceContains(
        text("dub.sdl"),
        text("configuration \"osx-regression-smoke\""),
        text("DepthDraw Mac OpenGL verification boundary requires the macOS regression smoke configuration"));
    requireSourceContains(
        text("dub.sdl"),
        text("configuration \"win32-regression-smoke\""),
        text("DepthDraw live UI smoke setup must remain compile-checkable on Windows"));
    requireSourceContains(
        text("dub.sdl"),
        text("subConfiguration \"i2d-imgui\" \"dynamic_dynamicCRT\""),
        text("Windows RegressionSmoke build must keep the regression-compatible i2d-imgui dependency mode"));
    requireSourceContains(
        text("dub.sdl"),
        text("versions \"HaveMCP\" \"RegressionSmoke\" \"InDoesRender\""),
        text("macOS regression smoke must keep render support enabled for OpenGL verification"));
    requireSourceContains(
        buildPath(text(".github"), text("workflows"), text("pr-test.yml")),
        text("windows-regression-smoke-build"),
        text("PR workflow must compile-check the Windows RegressionSmoke app"));
    requireSourceContains(
        buildPath(text(".github"), text("workflows"), text("pr-test.yml")),
        text("dub build --compiler=ldc2 --config=win32-regression-smoke"),
        text("PR workflow must build the Windows RegressionSmoke configuration"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("OpenGL transform feedback support"),
        text("DepthDraw GPU backend must keep explicit OpenGL transform feedback requirement diagnostics"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("for (int dy = -radius; dy <= radius; ++dy)"),
        text("DepthDraw GPU sampling must iterate the requested radius instead of a fixed maximum window"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("for (int dy = -64; dy <= 64; ++dy)"),
        text("DepthDraw GPU sampling must not retain the fixed maximum-radius loop"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("project.psd-depth-map-import-ui-smoke"),
        text("PSD depth import live UI smoke must have a regression-smoke setup entry point"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("commands"), text("puppet"), text("file.d")),
        text("incPushWindow(new PSDDepthMapWindow(path));"),
        text("PSD depth import file-menu command must open the corrected PSDDepthMapWindow workflow"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("commands"), text("puppet"), text("file.d")),
        text("Depth Source (*.psd, *.png)"),
        text("PSD depth import file-menu command must accept PSD and PNG sources"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("\"Body:G\""),
        text("PSD depth import live UI smoke should create a plausible depth target grid before previewing a PSD"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("attachSmokeCoveragePart(bodyGrid, \"bottomwear-back\", 300.0f)"),
        text("PSD depth import live UI smoke must attach a coverage Part so sampled preview grids are meaningful"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("createSmokeDepthGridWithAxes"),
        text("PSD depth import live UI smoke must be able to build a wider sampled grid fixture"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("for (float value = -600.0f; value <= 600.0f; value += 40.0f)"),
        text("PSD depth import live UI smoke must sample a broad grid area instead of only the document center"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("has3DAdjustGeometryForRegressionSmoke"),
        text("PSD depth import live UI smoke must verify nonblank 3D Adjust relationship geometry"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("nijigenerate-psd-depth-import-smoke.png"),
        text("PSD depth import live UI smoke must load a generated PNG through the same dialog"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("applyForRegressionSmoke"),
        text("PSD depth import live UI smoke must apply through the existing PSD depth import path"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("setGpuCompositionForRegressionSmoke(true)"),
        text("PSD depth import live UI smoke must exercise GPU-selected apply routing"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("ngSetDepthDrawGpuTestHooks"),
        text("PSD depth import live UI smoke must install a DepthDraw GPU backend hook for GPU-selected apply"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("regressionSmokeDepthDrawGpuSubmitCount"),
        text("PSD depth import live UI smoke must verify GPU submit was reached"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("regressionSmokeDepthDrawGpuPollCount"),
        text("PSD depth import live UI smoke must verify GPU poll/readback was reached"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("combinePsdDepthImportPreviews"),
        text("PSD depth import live UI smoke must cover multiple mapped target apply"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("incActionUndo()"),
        text("PSD depth import live UI smoke must verify multi-target apply undo"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("incActionRedo()"),
        text("PSD depth import live UI smoke must verify multi-target apply redo"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("igBeginTabItem(__(\"Source / Mapping\"))"),
        text("PSD depth import dialog must keep Source / Mapping inside the corrected dialog workflow"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("igBeginTabItem(__(\"3D Adjust\"),"),
        text("PSD depth import dialog must keep 3D Adjust inside the corrected dialog workflow"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("drawSourceMappingTab(max(120.0f, incAvailableSpace().y))"),
        text("PSD depth import Source / Mapping tab must be rendered by PSDDepthMapWindow"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("###PsdDepthComposedSourceLayers"),
        text("PSD depth import Source / Mapping must expose a dedicated composed source-layer panel before target projection rows"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("if (errorMessage.length || preview.grids.length == 0) return;"),
        text("PSD depth import Source / Mapping must not hide unmatched source layers before users can remap them"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("draw3DAdjustTab(max(120.0f, incAvailableSpace().y))"),
        text("PSD depth import 3D Adjust tab must be rendered by PSDDepthMapWindow"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("glUseProgram(cast(GLuint)previousProgram)"),
        text("PSD depth import 3D preview must restore the caller's OpenGL program"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("glBindVertexArray(cast(GLuint)previousVertexArray)"),
        text("PSD depth import 3D preview must restore the caller's OpenGL vertex array"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("glActiveTexture(cast(GLenum)previousActiveTexture)"),
        text("PSD depth import 3D preview must restore the caller's active texture unit"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("draw3DAdjustRelationshipCanvas"),
        text("PSD depth import 3D Adjust tab must prioritize a dedicated relationship canvas"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("draw3DAdjustLayerControlsPanel"),
        text("PSD depth import 3D Adjust controls must inspect composed source layers, not target mappings"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("###PsdDepth3DRelationshipLayout"),
        text("PSD depth import 3D Adjust tab must use a distinct relationship-view layout"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("###PsdDepth3DAdjustControls"),
        text("PSD depth import 3D Adjust must not fall back to a Source/Mapping-style controls table"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("settings.layerTransforms[layerPath] = transform;"),
        text("PSD depth import 3D Adjust controls must not duplicate layer state in import settings"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("layer.depthOffset = transform.zOffset;"),
        text("PSD depth import 3D Adjust controls must update the sole composed-layer state directly"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("foreach (ref layerPreview; preview.composedLayers)"),
        text("PSD depth import 3D Adjust must be driven by composed source layers before target mapping"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("threeDAdjustLayerDepthDisplayScale"),
        text("PSD depth import 3D Adjust must not move layers into different Z units based on target bounds"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("findComposedLayer(layerPath, targetGridUuid)"),
        text("PSD depth import UI state must resolve a composed layer by source path and target grid"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("commands"), text("depth"), text("map.d")),
        text("ngPsdDepthDocumentOffsetToTargetDepth"),
        text("PSD import composition must not renormalize an adjusted 3D Adjust depth before applying it"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("layerPreview.depthRgba"),
        text("PSD depth import 3D Adjust must displace its surface from raw source-layer depth pixels"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("previewDirty = true;"),
        text("PSD depth import tab state changes must rebuild the shared preview/apply result"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("foreach (path; puppet.findNodesType!PathDeformer(puppet.root)) targets ~= path;"),
        text("PSD depth import manual mapping controls must offer PathDeformer targets"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("struct PsdDepthGridResult"),
        text("PSD depth import must keep a shared target result structure"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("Deformable grid;"),
        text("PSD depth import target results must use the shared Deformable target type"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("foreach (path; paths)"),
        text("PSD depth import source builder must discover PathDeformer targets"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("commands"), text("depth"), text("map.d")),
        text("auto depthMapped = cast(DepthMappedNode)gridResult.grid;"),
        text("PSD depth import apply must use the shared DepthMappedNode target contract"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("commands"), text("depth"), text("map.d")),
        text("auto depthAction = ngApplyDepthsChangeAction(gridResult.grid, gridResult.depths, \"Import PSD Depth Map\");"),
        text("PSD depth import apply must preserve the existing undoable depth action path"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("commands"), text("depth"), text("map.d")),
        text("group.addAction(depthAction);"),
        text("PSD depth import apply must add each depth change to the undoable import group"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("PsdDepth3DAdjustGeometryStats"),
        text("PSD depth import 3D Adjust must expose relationship geometry stats for smoke verification"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("stats.layerPlanes"),
        text("PSD depth import 3D Adjust smoke stats must verify layer-surface geometry"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("No source layers were loaded"),
        text("PSD depth import dialog must diagnose empty source sessions"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("No mapped targets were found"),
        text("PSD depth import dialog must diagnose missing target mappings"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("Unsupported target type"),
        text("PSD depth import dialog must diagnose unsupported target mappings explicitly"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("Preview failed:"),
        text("PSD depth import dialog must diagnose preview failures"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("GPU/apply failed:"),
        text("PSD depth import dialog must diagnose GPU/apply failures"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("diagnosticsForRegressionSmoke"),
        text("PSD depth import diagnostics must be reachable from regression smoke"));
    requireSourceContains(
        buildPath(text("vendor"), text("psd-d"), text("source"), text("psd"), text("parser.d")),
        text("sampleMaskAt(layer, ChannelType.LAYER_MASK, layerMask, i)"),
        text("PSD depth import compatibility depends on psd-d applying layer masks into layer alpha"));
    requireSourceContains(
        buildPath(text("vendor"), text("psd-d"), text("source"), text("psd"), text("parser.d")),
        text("sampleMaskAt(layer, ChannelType.LAYER_OR_VECTOR_MASK, layerOrVectorMask, i)"),
        text("PSD depth import compatibility depends on psd-d applying vector/user masks into layer alpha"));
    requireSourceContains(
        buildPath(text("vendor"), text("psd-d"), text("source"), text("psd"), text("parser.d")),
        text("if (layer.vectorMask.length > 0) return maskGeometry(layer.vectorMask[0]);"),
        text("PSD vector-mask channels must use their own bounds instead of the real layer-mask rectangle"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("source.d")),
        text("applyPsdLayerMaskIfAvailable(drawLayer, layer);"),
        text("DepthDraw must not apply PSD masks a second time after psd-d has composed layer alpha"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("return (layer.flags & LayerFlags.Visible) == 0;"),
        text("PSD depth import visibility must use the same PSD flag interpretation as depth-draw"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("layer.isVisible"),
        text("PSD depth import must not read psd-d's unset isVisible field"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("hiddenComposedLayerPaths"),
        text("PSD depth import settings must not retain a second composed-layer visibility state"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("uniquePsdLayerPath(\"%s/%s\".format(groupState.path, layer.name), layerPathOccurrences)"),
        text("PSD depth import source sessions must preserve group paths and disambiguate duplicate layers"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("image.left = layer.left;"),
        text("PSD depth import source sessions must preserve PSD layer bounds"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("image.opacity = effectiveLayerOpacity;"),
        text("PSD depth import source sessions must preserve effective layer and ancestor-group opacity"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("auto groupStates = ngPsdLayerGroupStates(document.layers);"),
        text("PSD depth import must propagate ancestor-group visibility into child layers"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("if (path == layerPath)"),
        text("PSD depth import matching must keep exact part layer path matching"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("else if (baseName(path) == layerName)"),
        text("PSD depth import matching must keep basename matching"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("else if (part.name == layerName)"),
        text("PSD depth import matching must keep part/node name matching"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("if (settings.matchDirectGridName)"),
        text("PSD depth import matching must keep direct deformer-name matching behind the setting"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("\"DepthEditSmoke:G\""),
        text("DepthEdit live UI smoke should create a target grid instead of opening an empty depth viewport"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("new DepthEditViewport"),
        text("DepthEdit live UI smoke must instantiate the DepthEdit viewport backend"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("targetByGrid(grid.uuid)"),
        text("DepthEdit live UI smoke must verify shared target view initialization"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("activeEditor.setDepth(0, 0.75f)"),
        text("DepthEdit live UI smoke must verify direct depth edits route through the shared target view"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("DepthToolMode.AttachedPoint"),
        text("DepthEdit live UI smoke must verify attached-point tool mode routing"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("DepthToolMode.Ring"),
        text("DepthEdit live UI smoke must verify ring tool mode routing"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("DepthToolMode.Plane"),
        text("DepthEdit live UI smoke must verify plane tool mode routing"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("viewport.activeToolMode() != mode"),
        text("DepthEdit live UI smoke must fail when viewport tool mode routing regresses"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("new DepthAttachedPointOperation"),
        text("DepthEdit live UI smoke must add an attached-point operation through the editor command path"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("new DepthRingOperation"),
        text("DepthEdit live UI smoke must add a ring operation through the editor command path"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("new DepthPlaneOperation"),
        text("DepthEdit live UI smoke must add a plane operation through the editor command path"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("targetView.copyWorkingDepths() != operatedDepths"),
        text("DepthEdit live UI smoke must verify operation results are reflected in the shared target view"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("buildGridLines(targetView"),
        text("DepthEdit live UI smoke must verify shared target render geometry"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("DepthEdit smoke failed to build shared target render geometry"),
        text("DepthEdit live UI smoke must fail when shared target render geometry is missing"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("new PSDDepthMapWindow"),
        text("PSD depth import live UI smoke must open the PSD depth map window"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("Midori-20260621-color-psd-depth.psd"),
        text("PSD depth import live UI smoke should use the local depth-draw PSD fixture when it is available"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("localDepthDrawDataPath"),
        text("PSD depth import live UI smoke must keep a portable fallback when the local depth-draw fixture is absent"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("rebuildPreviewForRegressionSmoke"),
        text("PSD depth import live UI smoke must force preview building during smoke setup"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("hasSampledPreviewGridForRegressionSmoke"),
        text("PSD depth import window must expose smoke-only sampled preview grid checks"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("remapLayerNameToGridForRegressionSmoke"),
        text("PSD depth import window must expose smoke-only manual layer remapping checks"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("remapAnyLayerToSampledGridForRegressionSmoke"),
        text("PSD depth import window must expose smoke-only fallback remapping to a sampled layer"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("remapLayerNameToGridForRegressionSmoke(\"bottomwear-back\""),
        text("PSD depth import live UI smoke must exercise manual remap when the empty smoke puppet has no matching body Part"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("remapAnyLayerToSampledGridForRegressionSmoke"),
        text("PSD depth import live UI smoke must still find a sampled fixture layer if the exact body layer name is absent"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("previewGridCountForRegressionSmoke() == 0"),
        text("PSD depth import live UI smoke must fail when preview building produces no grid previews"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("PSD depth import smoke failed to build sampled preview grid"),
        text("PSD depth import live UI smoke must fail when the generated target grid has no sampled preview"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("regression_smoke.d")),
        text("PSD depth import smoke failed to build preview"),
        text("PSD depth import live UI smoke must fail the process when preview construction fails"));
    requireSourceContains(
        buildPath(text("source"), text("app.d")),
        text("ngRegressionSmokeFailed()"),
        text("RegressionSmoke failures must be converted into a non-zero process result"));
    requireSourceContains(
        buildPath(text("source"), text("app.d")),
        text("return 1"),
        text("RegressionSmoke setup failures must not be reported as successful process exits"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("windows"), text("psddepthmap.d")),
        text("loadErrorForRegressionSmoke"),
        text("PSD depth import window must expose smoke-only load errors for regression setup checks"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("import nijigenerate.io.depthsample"),
        text("PSD depth import must delegate low-level sampling formulas to depthsample.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("ngDepthSampleConvolve"),
        text("PSD depth import must use the shared convolution iterator"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawDepthPixelsFromRgbaRed"),
        text("DepthDraw depth conversion helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawAlphaMaskFromRgba"),
        text("DepthDraw alpha mask helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawSampleDepthLayerToColorLayer"),
        text("DepthDraw document/layer bounds sampling helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthImageCoverageReliableAt"),
        text("Depth image coverage reliability helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("ngDepthImageCoverageAlphaAt"),
        text("PSD depth import must use the neutral coverage alpha helper for RGBA coverage"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("ngDepthImageCoverageReliableAt"),
        text("PSD depth import must use the neutral coverage reliability helper for RGBA coverage"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthImageCompositeAlpha"),
        text("Depth image alpha composition helper must remain in the neutral module"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthImageCoverageAlphaAtUv"),
        text("Depth image UV coverage alpha helper must remain in the neutral module"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("ngDepthImageCompositeAlpha"),
        text("PSD depth import mesh coverage composition must use the neutral alpha composition helper"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("ngDepthImageCoverageAlphaAtUv"),
        text("PSD depth import mesh coverage must use the neutral UV coverage alpha helper"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("void compositeCoverage"),
        text("PSD depth import must not keep a local coverage alpha composition formula"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthmap_psd.d")),
        text("coverageAlphaAtPixel"),
        text("PSD depth import must not keep a local coverage alpha pixel formula"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("coordinate.d")),
        text("ngDepthDrawLayerPixelFromDocument"),
        text("DepthDraw coordinate transform order must remain in the shared coordinate helper"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("composer.d")),
        text("ngDepthDrawLayerPixelFromVertex"),
        text("DepthDraw composer must use the shared coordinate transform helper"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("composer.d")),
        text("ngDepthImageSampleRgbaWithOpacityAndCoverage"),
        text("DepthDraw composer must use the neutral depth image sampling facade for opacity-aware coverage sampling"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("composer.d")),
        text("ngDepthImageSampleRgbaWithOpacity"),
        text("DepthDraw composer must use the neutral depth image sampling facade for opacity-aware direct sampling"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("readback.d")),
        text("ngCompareDepthDrawReadback"),
        text("DepthDraw GPU/readback paths must compare against CPU output through a shared helper"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("import nijigenerate.io.depthimage : DepthImageChannel, DepthImageConvolution"),
        text("DepthDraw GPU packets must use the same neutral depth image channel/convolution enums as CPU sampling"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("ngBuildDepthDrawGpuComposeReadback"),
        text("DepthDraw GPU composition must read back into the shared compose readback representation"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("ngSubmitDepthDrawGpuTargetCompose"),
        text("DepthDraw GPU must expose a target compose submit wrapper for UI/preview integration"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("ngPollDepthDrawGpuTargetCompose"),
        text("DepthDraw GPU must expose a target compose poll wrapper that reconstructs compose results"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("submitDepthDrawGpuComposeBackend"),
        text("DepthDraw GPU compose dispatch must be connected to the real OpenGL layer-sample backend"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("pendingComposeJobs"),
        text("DepthDraw GPU compose dispatch must track real per-layer OpenGL sub-jobs until readback"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("ngSubmitDepthDrawGpuLayerSample(samplePacket"),
        text("DepthDraw GPU compose dispatch must submit one real layer-sample job per layer"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("ngBuildDepthDrawGpuComposeReadback(packet"),
        text("DepthDraw GPU compose dispatch must merge real layer readbacks through the shared readback builder"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("session.d")),
        text("bool useGpuPreview"),
        text("DepthDraw session display options must retain the GPU preview toggle"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("manifest.d")),
        text("\"useGpuPreview\""),
        text("DepthDraw manifest persistence must preserve the GPU preview toggle"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("ngSubmitDepthDrawGpuTargetCompose"),
        text("DepthDraw viewport GPU preview must submit through the target compose wrapper"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("ngPollDepthDrawGpuTargetCompose"),
        text("DepthDraw viewport GPU preview must poll through the target compose wrapper"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("composeSelectedPreviewForUpdate"),
        text("DepthDraw viewport update must route selected preview composition through a testable helper"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("drawSession.display.useGpuPreview"),
        text("DepthDraw viewport selected-preview update helper must check GPU preview mode before CPU composition"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("if (drawSession.display.useGpuPreview) return composeDirtyGpuPreviews();"),
        text("DepthDraw viewport dirty-preview update must route GPU preview mode to GPU submit/poll instead of CPU composition"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("target.replaceWorkingDepths(pollResult.result.depths);"),
        text("DepthDraw viewport GPU preview must write back only the reconstructed GPU readback depths"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("drawSession.clearTargetPreviewDirty(gridUuid);"),
        text("DepthDraw viewport GPU preview must keep dirty state until GPU readback has been consumed"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("mergeDepthReadback"),
        text("DepthDraw GPU readback merge must keep merge-policy logic inside the GPU readback boundary"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("ngDepthDrawGpuLayerSampleSupportsConvolution"),
        text("DepthDraw GPU must expose an explicit convolution capability contract"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")),
        text("DepthImageConvolution.MedianCustom"),
        text("DepthDraw GPU must explicitly reject unsupported custom median convolution instead of silently falling back"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate_tests"), text("regression.d")),
        text("DepthImageConvolution.Median3x3"),
        text("DepthDraw GPU regression coverage must include fixed median convolution support"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate_tests"), text("regression.d")),
        text("ngCompareDepthDrawComposeReadback"),
        text("DepthDraw GPU regression coverage must compare GPU-style readback against CPU composition"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("readback.d")),
        text("tolerance.isFinite"),
        text("DepthDraw GPU/readback comparison must reject non-finite or negative tolerance values"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("source.d")),
        text("ngDepthDrawAlphaMaskFromRgba"),
        text("DepthDraw source loading must use the shared alpha mask helper"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("source.d")),
        text("ngDepthDrawApplyPsdMaskToAlpha"),
        text("DepthDraw PSD source loading must use the shared PSD mask helper"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("pngexport.d")),
        text("ngDepthDrawDecodeDepthPixelsFromRgba"),
        text("DepthDraw PNG export must use the shared selected-channel depth decode helper"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("pngexport.d")),
        text("depthByteFromRgba"),
        text("DepthDraw PNG export must not keep a local RGBA-to-depth formula"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("drawSession.updateDisplayOptions(display)"),
        text("DepthDraw viewport display toggles must route changes through the session update path"));
    requireSourceNotContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("composer.d")),
        text("rgba["),
        text("DepthDraw composer must not duplicate raw RGBA sampling formulas"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawDetectAlphaDepthGaps"),
        text("DepthDraw alpha-depth gap detection helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawMedianFillDepth"),
        text("DepthDraw alpha-depth median fill helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("session.d")),
        text("applyLayerAlphaDepthGapFill"),
        text("DepthDraw alpha-depth gap fill must remain wired through the session dirty path"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawBuildLayerContourBandMask"),
        text("DepthDraw contour-band helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawInpaintMaskedLayerDepth"),
        text("DepthDraw masked depth inpaint helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawPruneForeignDepthSeeds"),
        text("DepthDraw foreign depth seed pruning helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawBuildVisibleLayerMap"),
        text("DepthDraw visible layer map helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("io"), text("depthimage.d")),
        text("ngDepthDrawSeedLayerDepthPixels"),
        text("DepthDraw split seed helper must remain in depthimage.d"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("session.d")),
        text("repairLayerContourDepth"),
        text("DepthDraw contour repair must remain wired through the session dirty path"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("override void drawOptions()"),
        text("DepthDraw viewport must keep viewport option toggles wired"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("override void drawConfirmBar()"),
        text("DepthDraw viewport must keep the label overlay draw path wired"));
    requireSourceContains(
        buildPath(text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("viewport.d")),
        text("incTextLabel(label.text)"),
        text("DepthDraw viewport must render gap-handle text labels through the confirm-bar overlay"));
}
