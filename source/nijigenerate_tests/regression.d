module nijigenerate_tests.regression;

import nijigenerate.actions;
import nijigenerate.api.acp.protocol : ACPError, ACP_METHOD_INITIALIZE, ACP_METHOD_PING, ACP_PROTOCOL_VERSION, ErrorCode, JSONRPC_VERSION;
import nijigenerate.api.acp.types : Document, Position, Range, StatusLevel, StatusNotification, TextEdit, WorkspaceEdit;
import nijigenerate.api.mcp.helpers : buildContextFromPayload, commandResultToJsonRuntime;
import nijigenerate.api.mcp.auth : ApprovalRequest;
import nijigenerate.api.mcp.http_transport : createHttpTransport;
import nijigenerate.api.mcp.resource_listing : buildCurrentResourceList, rewriteResourcesListResponse;
import nijigenerate.api.mcp.server : ngMcpApplySettings, ngMcpAuthEnabled, ngMcpStop;
import nijigenerate.api.mcp.task : ngMcpEnqueueAction, ngMcpInitTask, ngMcpProcessQueue, ngRunInMainThread;
import nijigenerate.commands;
import nijigenerate.commands.binding.binding;
import nijigenerate.commands.base;
import nijigenerate.commands.inspector.apply_node;
import nijigenerate.commands.model.set_deform_binding;
import nijigenerate.commands.node.base : clipboardNodes, conversionMap;
import nijigenerate.commands.node.dynamic;
import nijigenerate.commands.node.node;
import nijigenerate.commands.node.simplephysics;
import nijigenerate.commands.parameter.group;
import nijigenerate.commands.parameter.param;
import nijigenerate.commands.parameter.paramedit;
import nijigenerate.commands.parameter.prop;
import nijigenerate.commands.puppet.file;
import nijigenerate.commands.puppet.edit;
import nijigenerate.commands.puppet.tool : AttemptRepairPuppetCommand, GenerateFakeLayerNameCommand, ImportSessionDataCommand, PremultTextureCommand, RebleedTextureCommand, RegenerateMipmapsCommand, RegenerateNodeIDsCommand;
import nijigenerate.commands.viewport.control;
import nijigenerate.commands.viewport.palette;
import nijigenerate.commands.vertex.define_mesh;
import nijigenerate.atlas.packer : TexturePacker;
import nijigenerate.core.colorbleed : incColorBleedPixels;
import nijigenerate.core.actionstack;
import nijigenerate.core.input : _K;
import nijigenerate.core.selector.query : Selector;
import nijigenerate.core.selector.resource : Resource, ResourceType;
import nijigenerate.core.selector.treestore : TreeStore_;
import nijigenerate.core.settings;
import nijigenerate.core.shortcut.base;
import nijigenerate.core.shortcut.defaults;
import nijigenerate.ext;
import nijigenerate.ext.nodes.exdepthbone;
import nijigenerate.ext.nodes.exdepthops;
import nijigenerate.ext.nodes.exgriddeformer;
import nijigenerate.ext.param;
import nijigenerate.io.autosave;
import nijigenerate.io.inpexport;
import nijigenerate.project;
import nijigenerate.viewport.common.mesh : IncMesh;
import meshNodeOps = nijigenerate.viewport.common.mesheditor.operations.node;
import meshDeformableOps = nijigenerate.viewport.vertex.mesheditor.deformable;
import meshDrawableOps = nijigenerate.viewport.vertex.mesheditor.drawable;
import nijigenerate.viewport.depth.camera : DepthBrushSettings, DepthCamera3D, projectDepthPoint, unprojectDepthPoint;
import nijigenerate.viewport.depth.mesheditor : DepthMeshEditor;
import nijigenerate.viewport.depth.tools.operation : DepthAttachedPointOperation, DepthOperationColor, DepthOperationNegativeColor, DepthOperationNegativeSelectedColor, DepthOperationPositiveColor, DepthOperationPositiveSelectedColor, DepthOperationSelectedColor, DepthPlaneOperation, DepthRingOperation, depthOperationColor, depthToolRound, distanceToSegment;
import nijigenerate.viewport.vertex : ngActiveAutoMeshProcessor, ngAutoMeshProcessors;
import nijigenerate.viewport.vertex.automesh : AutoMeshProcessor;
import nijigenerate.viewport.vertex.automesh.meta : IAutoMeshReflect;
import nijigenerate.windows.paramsplit : ngSplitParameterBindings;
import nijilive;
import nijilive.core.nodes.deformer.grid;
import nijilive.core.nodes.drivers;
import nijilive.core.nodes.mask : Mask;
import nijilive.core.nodes.node : inRegisterNodeType;
import nijilive.core.render.scheduler : RenderContext;
import kra : KRA, parseKRADocument = parseDocument;
import psd : PSD, parsePSDDocument = parseDocument;
import std.base64 : Base64;
import std.exception : enforce;
import std.algorithm.searching : canFind, countUntil, endsWith, startsWith;
import std.algorithm.sorting : sort;
import std.array : join;
import std.conv : to;
import std.file : SpanMode, dirEntries, exists, isFile, mkdirRecurse, read, readText, remove, rmdirRecurse, tempDir, write;
import std.format : format;
import std.path : buildPath, relativePath, setExtension;
import std.json : JSONType, JSONValue;
import std.stdio : stderr, writeln;
import std.string : split, splitLines, stripLeft;

private struct Scenario {
    string id;
    string category;
    string title;
    string status;
    string note;
}

private enum automated = "automated";
private enum computerUse = "computer-use";
private enum pending = "pending";

private double regressionNow() {
    return 0.0;
}

private immutable Scenario[] scenarios = [
    Scenario("coverage.source-command-inventory", "Coverage Audit", "Source modules, major subsystems, and command entrypoints are represented in the regression catalog", automated, "This is an accounting guard, not behavioral coverage; it fails when a source subsystem or command family has no catalog owner."),
    Scenario("coverage.command-base", "Coverage Audit", "Base command contracts, command metadata, categories, shortcut labels, and command result behavior", automated, "Covers base CommandResult payloads, Context masks, ExCommand labels/args, and command metadata reflection."),
    Scenario("coverage.full-feature-scenario-inventory", "Coverage Audit", "Every command, window, panel, inspector, mesh tool, and depth tool class has a regression scenario owner", automated, "Builds a source-derived feature inventory so newly added user-facing feature entrypoints cannot remain uncataloged."),
    Scenario("coverage.source-module-scenario-inventory", "Coverage Audit", "Every source module is assigned to a regression scenario family", automated, "Builds a module-to-scenario map for the whole source tree; this is the baseline scenario catalog before individual behavioral tests are filled in."),

    Scenario("project.new-open-save", "Project/File", "New, open, save, save-as, close, dirty state, and recent-file behavior", automated, "Covers headless-safe save/open command paths, file creation, dirty-state clearing, and model round-trip."),
    Scenario("project.file-dialogs", "Project/File", "Open, save, save-as, import, merge, and export dialog command entrypoints", computerUse, "Needs UI dialog smoke with cancellable and accepted paths."),
    Scenario("project.close-dirty-prompts", "Project/File", "Close project, unsaved-change prompts, cancel, save, and discard paths", computerUse, "Needs UI prompt smoke."),
    Scenario("project.recent-files", "Project/File", "Recent-file insertion, duplicate removal, missing-file cleanup, and menu display", automated, "Covers settings-backed recent project insertion, duplicate promotion, and list length pruning."),
    Scenario("project.autosave-recovery", "Project/File", "Autosave, recovery, lockfile, recovery rejection, and stale-record cleanup paths", automated, "Covers autosave file creation, lockfile state, recovery record creation, and stale-record pruning; restart dialog rejection uses computer-use."),
    Scenario("project.import-psd", "Project/File", "PSD import, layer grouping, node-type mapping, clipping, opacity, blend, and texture placement", automated, "Covers generated PSD fixture reader and import command path; rich layered PSD golden coverage remains asset-dependent."),
    Scenario("project.import-kra", "Project/File", "KRA import, layer grouping, node-type mapping, clipping, opacity, blend, and texture placement", automated, "Covers generated KRA fixture reader and import command path; rich layered KRA golden coverage remains asset-dependent."),
    Scenario("project.import-inp", "Project/File", "INP import and compatibility with exported files", automated, "Covers command-level import and merge of generated INP fixtures with nodes, parameters, and bindings."),
    Scenario("project.import-images", "Project/File", "Image and image-folder import into Parts", automated, "Covers generated PNG fixtures through image-folder import and merge-image-files command paths."),
    Scenario("project.merge-psd", "Project/File", "Merge PSD into existing model while preserving existing nodes and bindings", automated, "Covers generated PSD fixture merge command preserving existing model state."),
    Scenario("project.merge-kra", "Project/File", "Merge KRA into existing model while preserving existing nodes and bindings", automated, "Covers generated KRA fixture merge command preserving existing model state."),
    Scenario("project.merge-inp", "Project/File", "Merge INP into an existing model", automated, "Covers INP merge command with generated fixture."),
    Scenario("project.merge-images", "Project/File", "Merge individual image files into an existing model", automated, "Covered with generated PNG fixtures."),
    Scenario("project.export-inp", "Project/File", "INP export including exclusion of editor-only nodes", automated, "Covers export pruning of DepthRigRoot/DepthBone plus their bindings; full file serialization still needs a stable fixture."),
    Scenario("project.export-png", "Project/File", "PNG export with transparency, bounds, scale, and selected output path", computerUse, "Needs render verification."),
    Scenario("project.export-jpeg", "Project/File", "JPEG export with background handling and selected output path", computerUse, "Needs render verification."),
    Scenario("project.export-tga", "Project/File", "TGA export with alpha and selected output path", computerUse, "Needs render verification."),
    Scenario("project.export-screenshot", "Project/File", "Viewport screenshot save and capture-live screenshot command paths", computerUse, "Needs viewport render verification."),
    Scenario("project.export-video", "Project/File", "Video export dialog, encoder settings, frame range, and output file", computerUse, "Needs render/video fixture."),
    Scenario("project.session-import", "Project/File", "Import session data into the current project", automated, "Covers generated INP session extData import and missing-session rejection."),
    Scenario("project.texture-maintenance", "Project/File", "Premultiply texture, rebleed texture, regenerate mipmaps, and atlas maintenance commands", automated, "Covers texture maintenance commands on a generated texture-backed part and drains rebleed task queue."),
    Scenario("project.repair-maintenance", "Project/File", "Attempt repair, regenerate node IDs, fake layer names, and corrupted-project recovery commands", automated, "Covers repair command entrypoints, fake layer-path generation, and UUID regeneration on a generated puppet."),
    Scenario("io.serialization-inx", "Project/File", "Native INX save/load serialization of nodes, parameters, bindings, mesh, and metadata", automated, "Covers generated native INX round-trip for node transforms, mesh data, parameters, and value bindings."),
    Scenario("io.serialization-textures", "Project/File", "Native project texture file paths, texture slots, atlas references, and missing-texture recovery", automated, "Covers generated texture-backed INX save/load and restored Part texture slot dimensions."),
    Scenario("io.serialization-inp", "Project/File", "INP import/export serialization including external compatibility and editor-only pruning", automated, "Covered in part by project.import-inp and project.export-inp."),
    Scenario("io.save-native", "Project/File", "Native save path, backup overwrite, dirty-state reset, and format compatibility", automated, "Covers native save extension handling, overwrite, swap cleanup, project path state, dirty reset, and reload."),
    Scenario("io.inimport-model", "Project/File", "INP importer model merge, UUID remap, texture remap, and binding remap", automated, "Covers generated INP import/merge with nodes, textures, parameters, and binding remap."),
    Scenario("io.inpexport-model", "Project/File", "INP exporter node filtering, texture packaging, binding export, and external compatibility", automated, "Covers generated INP export plus reimport and editor-only node pruning."),
    Scenario("io.psd-reader", "Project/File", "PSD reader layer tree, masks, blend modes, opacity, clipping, and image extraction", automated, "Covers generated PSD fixture parse and import handoff; layered mask golden coverage remains asset-dependent."),
    Scenario("io.kra-reader", "Project/File", "KRA reader layer tree, masks, blend modes, opacity, clipping, and image extraction", automated, "Covers generated KRA fixture parse and import handoff; layered mask golden coverage remains asset-dependent."),
    Scenario("io.image-export", "Project/File", "Image export renderer path for PNG, JPEG, TGA, alpha, scale, and bounds", computerUse, "Needs render snapshot fixture."),
    Scenario("io.video-export", "Project/File", "Video export frame stepping, encoder invocation, cancellation, and output validation", computerUse, "Needs video export fixture."),
    Scenario("io.image-codecs", "Project/File", "Image loading/saving through PNG, JPEG, TGA, PSD, and KRA code paths", automated, "Covers generated PNG/JPEG/TGA texture codec round-trips and keeps PSD/KRA reader coverage in import scenarios."),
    Scenario("io.video-codecs", "Project/File", "Video export frame generation, encoder invocation, cancellation, and error paths", computerUse, "Needs video export fixture."),
    Scenario("atlas.pack", "Project/File", "Atlas packer rectangle placement, texture upload metadata, invalidation, and repack stability", automated, "Covers headless max-rect texture packer placement, non-overlap, removal, and clear behavior; GL atlas rendering remains render.atlas-packer."),
    Scenario("atlas.color-bleed", "Project/File", "Color bleed and rebleed operations preserve alpha edges and atlas consistency", automated, "Covers generated transparent-edge texture color bleeding and alpha preservation."),

    Scenario("node.create-delete-undo", "Node Hierarchy", "Create, delete, undo, redo, copy, cut, paste, duplicate", automated, "Create/delete/toggle through commands are covered; copy/cut/paste/duplicate still need deeper fixture coverage."),
    Scenario("node.add-types", "Node Hierarchy", "Add Node, Part, Composite, DynamicComposite, MeshGroup, GridDeformer, PathDeformer, Camera, SimplePhysics, DepthRigRoot, and DepthBone nodes", automated, "Covers dynamic AddNode command creation for every registered node-menu type with undo/redo."),
    Scenario("node.insert-types", "Node Hierarchy", "Insert supported node types before, after, and inside selected nodes", automated, "Covers dynamic InsertNode command creation for every registered node-menu type with undo/redo."),
    Scenario("node.cut-copy-paste-duplicate", "Node Hierarchy", "Cut, copy, paste, duplicate, UUID regeneration, texture references, and binding references", automated, "Covers node clipboard copy/paste, duplicate UUID regeneration, undo/redo, and Cut remaining disabled until implemented."),
    Scenario("node.reparent-order", "Node Hierarchy", "Move, reorder, reparent, and preserve transforms", automated, "Move/reparent through command is covered; transform preservation needs deeper fixture coverage."),
    Scenario("node.rename-undo-merge", "Node Hierarchy", "Node name edit merges per text-edit session and remains undoable", automated, "Covered by node-name-undo-merge."),
    Scenario("node.transform-inspector", "Node Hierarchy", "Translation, rotation, scale, sort, lock-to-root, pin-to-parent, snapping", automated, "Covers inspector apply commands for translation, rotation, scale, z-sort, and lock-to-root with undo/redo."),
    Scenario("node.visibility-lock", "Node Hierarchy", "Visibility, lock, selection, multi-selection, and tree filtering", computerUse, "Needs UI interaction smoke."),
    Scenario("node.convert-reload", "Node Hierarchy", "Reload texture and convert node operations", automated, "ConvertToCommand is covered with undo/redo; reload side effects still need deeper fixture coverage."),
    Scenario("node.centralize", "Node Hierarchy", "Centralize selected node pivots and undo/redo the resulting transform changes", automated, "Covers CentralizeNodeCommand with undo/redo for parent and child local transforms."),
    Scenario("node.type-conversion", "Node Hierarchy", "Convert node type between compatible registered node classes", automated, "Covers every conversionMap pair through ConvertToCommand with undo/redo and type preservation."),
    Scenario("node.resource-selector", "Node Hierarchy", "Selector parser, query, tree store, and resource selector integration", automated, "Covers selector query evaluation for nodes, parameters, bindings, direct children, and tree-store hierarchy."),
    Scenario("core.selector-parser", "Node Hierarchy", "Selector tokenizer, parser, query evaluation, and tree-store resource lookup", automated, "Covers selector grammar through query fixtures and Resource TreeStore construction."),
    Scenario("core.node-registry", "Node Hierarchy", "Node class registration, menu descriptors, icons, and dynamic construction", automated, "Covers menu-visible node type registration, dynamic command creation, stable command ids, and construction."),

    Scenario("inspectors.node-types", "Inspectors", "Puppet, Node, Part, Drawable, Composite, DynamicComposite, Camera inspectors", automated, "Covers Node, Part, Drawable, Composite, and Camera inspector command properties with undo/redo; Puppet and DynamicComposite render/UI smoke still need deeper coverage."),
    Scenario("inspectors.puppet", "Inspectors", "Puppet metadata, canvas, texture atlas, and project-level inspector controls", automated, "Covers puppet metadata, physics globals, preserve-pixels render setting, texture slot population, and native save/load round-trip."),
    Scenario("inspectors.node", "Inspectors", "Node transform, visibility, lock, lockToRoot, pinToParent, and sort controls", automated, "Covered in part by node.transform-inspector."),
    Scenario("inspectors.part", "Inspectors", "Part texture, tint, screen tint, emission, opacity, blend, clipping, masks, and welding controls", automated, "Covered in part by part.texture, part.mask-*, and part.welding."),
    Scenario("inspectors.drawable", "Inspectors", "Drawable offset, bounds, drawing behavior, and shared drawable controls", automated, "Covered in part by inspectors.node-types."),
    Scenario("inspectors.composite", "Inspectors", "Composite and DynamicComposite blending, masks, thresholds, tint, and draw settings", automated, "Covered in part by inspectors.node-types; dynamic runtime needs render fixture."),
    Scenario("inspectors.camera", "Inspectors", "Camera viewport and projection inspector controls", automated, "Covered in part by inspectors.node-types."),
    Scenario("inspectors.simplephysics", "Inspectors", "SimplePhysics mapping, curve type, parameter assignment, runtime settings, and reset controls", automated, "Covered in part by simplephysics.*."),
    Scenario("inspectors.mesh-deformers", "Inspectors", "MeshGroup, GridDeformer, PathDeformer inspector controls", automated, "Covers MeshGroup, GridDeformer, and PathDeformer inspector command properties with undo/redo."),
    Scenario("inspectors.depth-bone", "Inspectors", "DepthRigRoot and DepthBone inspector controls", automated, "Covers DepthBone inspector command paths for rest pose and constraints with undo/redo."),
    Scenario("inspectors.format-strings", "Inspectors", "Formatted labels substitute values and do not show raw %.0f%% tokens", automated, "Covers source-level ImGui format string mistakes that render raw percent patterns such as %%0.2f."),
    Scenario("inspectors.commit-boundaries", "Inspectors", "Inspector text inputs, drags, toggles, color edits, and drag-drop commit undo actions once per edit", computerUse, "Needs computer-use UI commit-boundary fixture."),

    Scenario("part.texture", "Part Properties", "Texture load, reload, UV, opacity, tint multiply/screen, emission, blend mode", automated, "Covers Part inspector command state changes for tint, screen tint, emission, opacity, and blend mode with undo/redo; texture reload and UV still need fixture coverage."),
    Scenario("part.texture-reload", "Part Properties", "Reload texture from source path, texture slot update, and missing-source handling", automated, "Covers file-backed texture replacement on Part texture slots, post-load texture slot repopulation, and missing file rejection with generated PNG fixtures."),
    Scenario("part.uv-mesh-coherence", "Part Properties", "Texture UVs, mesh vertices, indices, and deformation array lengths stay coherent", automated, "Covers DefineMesh/DefineVertices keeping mesh, UV, and deformation array lengths coherent through undo/redo."),
    Scenario("part.clipping-mask", "Part Properties", "Clipping and mask threshold behavior", automated, "Covers Clip/Slice blend modes and mask threshold state changes with undo/redo; pixel render output still needs snapshot coverage."),
    Scenario("part.mask-add-remove", "Part Properties", "Mask Source add/remove undo/redo", automated, "Covered by mask-source-add-undo-redo."),
    Scenario("part.mask-reorder", "Part Properties", "Mask Source reorder preserves exact bindings through undo/redo", automated, "Covered by mask-source-reorder-undo-redo."),
    Scenario("part.mask-mode", "Part Properties", "Mask Source mode changes are undoable", automated, "Covered by mask-source-mode-undo-redo."),
    Scenario("part.welding", "Part Properties", "Welding add/remove/edit undo/redo", automated, "Covered by welding-undo-redo."),
    Scenario("part.welding-runtime", "Part Properties", "Welding deformation follows source part and preserves inverse/counter weights", automated, "Covers welded Drawable post-process deformation blending, counter-link index generation, weights, and filter removal."),

    Scenario("parameter.lifecycle", "Parameters", "Create, delete, duplicate, rename, move, group, split, range, defaults", automated, "Create/duplicate/delete/rename/group/move/color/delete through commands are covered; split/range/defaults still need deeper fixture coverage."),
    Scenario("parameter.create-presets", "Parameters", "Create 1D, 2D, mouth, and template parameters with correct ranges and default keypoints", automated, "Covers command-created 1D, 2D, and mouth parameter presets with ranges and axis points."),
    Scenario("parameter.groups", "Parameters", "Create, delete, move, recolor, and reorder parameter groups", automated, "Covered in part by parameter.lifecycle."),
    Scenario("parameter.split-window", "Parameters", "Parameter split dialog, axis split, and binding migration", automated, "Covers the shared split implementation used by the dialog, including axis-copy, binding migration, and undo/redo."),
    Scenario("parameter.copy-paste", "Parameters", "Copy, paste, duplicate, duplicate with flip, and paste with flip parameters", automated, "Covers parameter clipboard paste, paste-with-flip command path, duplicate, duplicate-with-flip, binding copy, and undo/redo."),
    Scenario("parameter.link", "Parameters", "Link one parameter axis to another and preserve axis mapping", automated, "Covers LinkToCommand creating ParameterParameterBinding with min/max axis mapping and undo/redo."),
    Scenario("parameter.arm-select", "Parameters", "Arm/disarm/select parameter and controller value changes", automated, "Covers arm/disarm plus command-driven keypoint and armed keypoint selection."),
    Scenario("parameter.starting-keyframe", "Parameters", "Set starting keyframe for parameter preview and editing", automated, "Covers SetStartingKeyFrameCommand defaults update and undo/redo."),
    Scenario("parameter.keyframe-basic", "Parameters", "Set, unset, reset, invert, mirror, flip, paste keyframes", automated, "Set/unset/reset/invert/mirror are covered on ValueParameterBinding; flip/paste still need deformation/golden fixtures."),
    Scenario("parameter.keyframe-mirror-fill", "Parameters", "Horizontal, vertical, diagonal, and 1D mirrored auto-fill commands", automated, "Covers mirror-fill commands for value bindings with source extrapolation, sign flip, undo, and redo."),
    Scenario("parameter.keyframe-copy-paste", "Parameters", "Copy, paste, remove binding, and interpolation mode commands", automated, "Covers binding copy/paste, remove binding, interpolation mode, and undo/redo."),
    Scenario("parameter.keyframe-2d", "Parameters", "2D keypoint X/Y frame add/remove pair behavior", automated, "Covers grouped X/Y add and remove undo/redo as one action."),
    Scenario("parameter.binding-interp", "Parameters", "Binding interpolation and reInterpolate stability", automated, "Covers 1D and 2D ValueParameterBinding interpolation and reInterpolate recovery after unset."),
    Scenario("parameter.binding-deform", "Parameters", "DeformationParameterBinding value length, interpolation, mirror, flip, and symmetrize behavior", automated, "Covers SetDeformBinding command creation, value length, undo/redo, and binding restoration."),
    Scenario("parameter.binding-trs", "Parameters", "TRS binding creation, value setting, interpolation, and target cleanup", automated, "Covers SetTRSBinding command creation, translation/scale/rotation values, and undo/redo."),
    Scenario("parameter.binding-model", "Parameters", "SetDeformBinding and SetTRSBinding model commands wire targets and undo/redo correctly", automated, "Covers model binding commands against generated Part fixtures."),
    Scenario("parameter.axes-props", "Parameters", "Parameter min/max, axis breakpoint, and binding remap undo/redo", automated, "Covers ApplyParameterPropsAxesCommand with bound values."),
    Scenario("parameter.template-depth-bone", "Parameters", "DepthBone standard parameter template creation and key values", automated, "Covers standard DepthBone face/body parameter template creation, binding values, and undo/redo."),
    Scenario("parameter.controller-widget", "Parameters", "Parameter controller widget drag, snap, armed keypoint update, and view synchronization", computerUse, "Needs parameter panel input smoke."),
    Scenario("parameter.binding-cleanup", "Parameters", "Deleting parameters, nodes, and targets removes stale bindings without dangling references", automated, "Covers command-level binding removal and parameter deletion undo/redo without dangling active bindings."),

    Scenario("animation.lifecycle", "Animation", "Create, rename, delete animations and tracks", automated, "Covers animation create/update/rename/delete action paths with undo/redo and lane preservation."),
    Scenario("animation.properties", "Animation", "Animation name, length, lead-in/out, additive, weight, fps/timestep, and metadata editing", automated, "Covers animation property create/update/rename undo/redo through animation action paths."),
    Scenario("animation.keyframes", "Animation", "Add, edit, remove, copy, paste animation keyframes", automated, "Covers 1D and 2D add/edit/remove keyframe action paths with undo/redo; copy/paste still needs timeline UI/input coverage."),
    Scenario("animation.timeline-ui", "Animation", "Timeline track selection, lane expansion, scrub, drag, and keyframe selection", computerUse, "Needs timeline input smoke."),
    Scenario("animation.playback", "Animation", "Playback, scrubbing, loop, fps, and preview state", computerUse, "Needs UI timing smoke."),
    Scenario("animation.keyframe-copy-paste", "Animation", "Animation keyframe clipboard copy, paste, replace, and undo grouping", computerUse, "Needs computer-use timeline fixture."),
    Scenario("animation.track-binding-cleanup", "Animation", "Animation tracks survive node/parameter rename and clean up deleted targets", automated, "Covers parameter rename stability and deleted target behavior across native save/load."),

    Scenario("api.external-control", "API/Agent", "External API, MCP task queue, command execution, and agent panel integration", automated, "Covers command result JSON, context payload overrides, resource listing, and queued main-thread dispatch; agent panel UI remains computer-use."),
    Scenario("api.acp-protocol", "API/Agent", "ACP protocol transport, echo agent, message parsing, cancellation, and error reporting", automated, "Covers ACP protocol constants, typed payload structs, and JSON-RPC error serialization; process transport remains in ACP stdio/client scenarios."),
    Scenario("api.mcp-server", "API/Agent", "MCP auth, HTTP transport, resource listing, command execution, and queued task dispatch", automated, "Covers disabled settings application, auth toggle behavior, resource listing, and queued command dispatch; live HTTP request smoke remains separate."),
    Scenario("api.mcp-auth", "API/Agent", "MCP auth token generation, validation, rejection, and settings persistence", automated, "Covers approval request data contract and auth UI source contract without blocking headless tests."),
    Scenario("api.mcp-http-transport", "API/Agent", "MCP HTTP transport request parsing, response streaming, errors, and shutdown", automated, "Covers transport construction, auth toggles, handler dispatch, close path, and registered route/source contract."),
    Scenario("api.mcp-resources", "API/Agent", "MCP resource listing, selectors, node resources, parameter resources, and serialization", automated, "Covers model-derived MCP resource entries, binding resource URIs, resources/list response rewriting, and context override parsing."),
    Scenario("api.mcp-task-queue", "API/Agent", "MCP queued task dispatch, cancellation, result propagation, and main-thread safety", automated, "Covers queued action processing, cross-thread ngRunInMainThread result propagation, and exception propagation."),
    Scenario("api.acp-client", "API/Agent", "ACP client start, request, response, cancellation, stderr, and shutdown behavior", automated, "Covers ACPClient source contract for process launch, reader threads, cancellation, permission responses, stderr capture, polling, and shutdown paths."),
    Scenario("api.acp-stdio", "API/Agent", "ACP stdio transport framing, partial reads, malformed input, and process cleanup", automated, "Covers ACP stdio adapter contract and its delegated transport factory."),
    Scenario("api.acp-echo-agent", "API/Agent", "ACP echo agent round-trip and error behavior", automated, "Covers echo-agent build guard, Content-Length framing, initialize handling, and protocol-version response contract."),
    Scenario("api.agent-panel", "API/Agent", "Agent panel opt-in state, disabled state, connection errors, and tool output display", computerUse, "Needs UI smoke."),

    Scenario("mesh.vertex-scope", "Mesh/Vertex Editor", "VertexEdit enter/exit scope and undo/redo scope guard", automated, "Covers common ActionStackScope guard semantics for VertexEdit nesting."),
    Scenario("mesh.select-tool", "Mesh/Vertex Editor", "Select tool single select, marquee select, lasso select, additive select, and deselect", computerUse, "Needs input simulation."),
    Scenario("mesh.point-tool", "Mesh/Vertex Editor", "Point tool create, select, drag, delete, snap, and constrained movement", computerUse, "Needs input simulation."),
    Scenario("mesh.connect-tool", "Mesh/Vertex Editor", "Connect tool edge creation, deletion, duplicate prevention, and topology validity", computerUse, "Needs input simulation."),
    Scenario("mesh.line-tool", "Mesh/Vertex Editor", "Line tool stroke creation, continuation, cancellation, and topology editing", computerUse, "Needs input simulation."),
    Scenario("mesh.path-tool", "Mesh/Vertex Editor", "Path tool curve/path creation, point movement, and topology editing", computerUse, "Needs input simulation."),
    Scenario("mesh.edge-cutter", "Mesh/Vertex Editor", "Edge cutter split, intersection, undo/redo, and invalid-cut rejection", computerUse, "Needs input simulation."),
    Scenario("mesh.lasso-tool", "Mesh/Vertex Editor", "Lasso selection and lasso-based topology operations", computerUse, "Needs input simulation."),
    Scenario("mesh.brush-tools", "Mesh/Vertex Editor", "Circle, rectangle, double-threshold, and brush-state behavior", computerUse, "Needs input simulation."),
    Scenario("mesh.grid-tool", "Mesh/Vertex Editor", "Grid creation, resize, drag, point move, and bake", computerUse, "Needs input simulation."),
    Scenario("mesh.bezier-deform-tool", "Mesh/Vertex Editor", "BezierDeform tool creation, editing, apply, cancel, and undo", computerUse, "Needs input simulation."),
    Scenario("mesh.define-grid-command", "Mesh/Vertex Editor", "Define Grid command updates GridDeformer vertices and undo/redo", automated, "Covers headless GridDeformer topology changes through command path."),
    Scenario("mesh.define-mesh-command", "Mesh/Vertex Editor", "Define Mesh and Define Vertices commands update mesh topology and undo/redo", automated, "Covers DefineMeshCommand and DefineVerticesCommand on drawable and deformable targets with undo/redo."),
    Scenario("mesh.operations-node", "Mesh/Vertex Editor", "Node, Drawable, and Deformable mesh editor operations update the correct targets", automated, "Covers operation-level fixtures for Node hit testing plus Drawable and Deformable target updates through applyToTarget."),
    Scenario("mesh.multi-object", "Mesh/Vertex Editor", "Edit multiple objects at once", automated, "Covers multi-target mesh editor apply grouping and one-step undo/redo."),
    Scenario("mesh.mirror-symmetry", "Mesh/Vertex Editor", "Mirror, symmetry, snap, apply, cancel", automated, "Covers mirror counterpart lookup, mirrored delta application, reset-before-apply cancellation, and undo/redo."),
    Scenario("core.math-mesh", "Mesh/Vertex Editor", "Mesh math, triangulation, vertex operations, path math, and spline helpers", automated, "Covers mesh vertex connection invariants, triangulation invariants, and path extraction fixtures."),
    Scenario("core.math-triangle", "Mesh/Vertex Editor", "Triangle predicates, triangulation invariants, winding, and degenerate geometry handling", automated, "Covers deterministic triangulation bounds, triangle index invariants, and fillPoly rasterization."),
    Scenario("core.math-path", "Mesh/Vertex Editor", "Path sampling, closest-point, tangent, subdivision, and spline helper behavior", automated, "Covers skeleton path extraction for empty, connected, and multi-component fixtures."),
    Scenario("core.math-skeletonize", "Mesh/Vertex Editor", "Skeletonization graph construction, pruning, branch selection, and deterministic output", automated, "Covers generated bitmap skeletonization and extracted path invariants."),
    Scenario("mesh.common-operations", "Mesh/Vertex Editor", "Common mesh helpers update vertices, indices, selection, and deformation arrays consistently", automated, "Covers MeshVertex connection, duplicate prevention, disconnect, and disconnectAll symmetry."),
    Scenario("mesh.spline", "Mesh/Vertex Editor", "Viewport spline editing helpers for handles, tangents, and sampling", automated, "Covers CatmullSpline interpolation, closest-point lookup, point insertion/removal, and remapped mesh target updates."),
    Scenario("mesh.operation-node", "Mesh/Vertex Editor", "Mesh editor node operation hit testing, transform handles, and scoped actions", automated, "Covers Node operation target setup, hit testing, rectangle filtering, and selection lookup."),
    Scenario("mesh.operation-drawable", "Mesh/Vertex Editor", "Mesh editor drawable operation edits mesh vertices and drawable deformation arrays", automated, "Covers Drawable operation mesh vertex mutation, applyToTarget, undo, and redo."),
    Scenario("mesh.operation-deformable", "Mesh/Vertex Editor", "Mesh editor deformable operation edits deformer vertices and offsets", automated, "Covers Deformable operation vertex mutation, applyToTarget, undo, and redo."),
    Scenario("mesh.tool-select", "Mesh/Vertex Editor", "Vertex select tool hit testing, rectangle selection, additive selection, and deselection", computerUse, "Needs UI input simulation."),
    Scenario("mesh.tool-point", "Mesh/Vertex Editor", "Vertex point tool drag, move, undo, and initial mode activation", computerUse, "Needs UI input simulation."),
    Scenario("mesh.tool-connect", "Mesh/Vertex Editor", "Vertex connect tool creates, removes, and validates mesh edges/triangles", computerUse, "Needs UI input simulation."),
    Scenario("mesh.tool-lasso", "Mesh/Vertex Editor", "Lasso selection tool selects, deselects, and handles closed/freeform strokes", computerUse, "Needs UI input simulation."),
    Scenario("mesh.tool-edge-cutter", "Mesh/Vertex Editor", "Edge cutter tool splits edges/faces and keeps mesh arrays coherent", computerUse, "Needs UI input simulation."),
    Scenario("mesh.tool-brush", "Mesh/Vertex Editor", "Brush tool with circle, rectangle, and double-threshold brush state edits vertices predictably", computerUse, "Needs UI input simulation."),
    Scenario("core.cv-image-contours", "Mesh/Vertex Editor", "CV image, contour extraction, distance transform, and skeletonize helpers", automated, "Covers generated image buffers, distance transform, contour extraction, hierarchy validation, polygon tests, and contour simplification."),

    Scenario("deform.path-tool", "Deformation Tools", "PathDeformer based deformation tool creation, edit, apply, undo", automated, "Covers BezierDeformTool point insertion, point movement, apply to PathDeformer, and undo/redo."),
    Scenario("deform.grid-tool", "Deformation Tools", "GridDeformer based deformation tool creation, edit, apply, undo", automated, "Covers GridTool virtual mesh creation, apply to GridDeformer, and undo/redo."),
    Scenario("deform.pathdeformer-runtime", "Deformation Tools", "PathDeformer runtime deformation, handles, smoothing, and binding interaction", automated, "Covers PathDeformer child deformation sampling, length invariants, and unchanged baseline behavior."),
    Scenario("deform.griddeformer-runtime", "Deformation Tools", "GridDeformer runtime deformation, grid interpolation, binding interaction, and mesh length invariants", automated, "Covers GridDeformer child deformation interpolation, length invariants, and unchanged baseline behavior."),
    Scenario("deform.meshgroup-compat", "Deformation Tools", "Legacy MeshGroup compatibility and migration behavior", automated, "Covers GridDeformer-to-MeshGroup migration copy, grid mesh reconstruction, deformation preservation, and conversion map ownership."),
    Scenario("deform.bezier-tool", "Deformation Tools", "Bezier deformation tool handles, interpolation, apply, and undo/redo", computerUse, "Needs UI input simulation."),
    Scenario("deform.onetime-scope", "Deformation Tools", "OneTimeDeform Vertex/Deform subtool scope and escape prevention", automated, "Covers common ActionStackScope guard semantics used by tool subscopes; input transition smoke remains manual."),
    Scenario("deform.onetime-vertex-subtool", "Deformation Tools", "OneTimeDeform Vertex subtool with PathDeformer and GridDeformer filter creation/editing", computerUse, "Needs input simulation."),
    Scenario("deform.onetime-deform-subtool", "Deformation Tools", "OneTimeDeform Deform subtool point/grid movement and visual preview", computerUse, "Needs input simulation."),
    Scenario("deform.onetime-apply", "Deformation Tools", "OneTimeDeform Apply preserves final visual result as much as possible", computerUse, "Needs computer-use golden deformation comparison."),
    Scenario("deform.onetime-save", "Deformation Tools", "Saving while OneTimeDeform subtool is active applies virtual deformation to copied model", computerUse, "Needs computer-use save fixture."),
    Scenario("deform.undo-redo-group", "Deformation Tools", "Nested undo/redo for VertexEdit, DepthEdit, and OneTimeDeform", automated, "Covers nested scope close behavior and level restoration."),

    Scenario("depth.edit-scope", "Depth Edit", "DepthEdit enter/exit scope and undo/redo scope guard", automated, "Covers common ActionStackScope guard semantics for DepthEdit."),
    Scenario("depth.select-tool", "Depth Edit", "Depth selection tool hit testing, select, deselect, and box selection", computerUse, "Needs UI input simulation."),
    Scenario("depth.point-tool", "Depth Edit", "Depth point tool edits depth values and supports undo/redo", computerUse, "Needs UI input simulation."),
    Scenario("depth.attached-point-tool", "Depth Edit", "Attached depth point tool follows mesh vertices and updates bound depth values", computerUse, "Needs UI input simulation."),
    Scenario("depth.ring-tool", "Depth Edit", "Depth ring tool displays front-side rings and edits radial depth", computerUse, "Needs UI input simulation."),
    Scenario("depth.plane-tool", "Depth Edit", "Depth plane tool fits and applies planar depth gradients", computerUse, "Needs UI input simulation."),
    Scenario("depth.landmark-tool", "Depth Edit", "Depth landmark tool creates, selects, moves, and deletes landmarks", computerUse, "Needs UI input simulation."),
    Scenario("depth.renderer", "Depth Edit", "Depth renderer draws signs, colors, rings, lines, and hidden/back-side cues correctly", computerUse, "Needs computer-use render snapshot fixture."),
    Scenario("depth.sign-colors", "Depth Edit", "Positive and negative depth display colors and front-side markers", automated, "Covers positive, negative, zero, selected, and rounding color contracts; rendered marker geometry remains in depth.renderer."),
    Scenario("depth.persistence", "Depth Edit", "Depth map edit apply, cancel, interpolation, and save/load", automated, "Covers depth arrays and depth operations through copy/replace/rebuffer helpers plus native INX save/load round-trip."),
    Scenario("depth.exdepthmapped", "Depth Edit", "DepthMapped node serialization, depth array resize, and depth operation helpers", automated, "Covers ExGridDeformer depth array copying, resize-on-rebuffer, depth operation copy, and native INX round-trip."),
    Scenario("depth.camera", "Depth Edit", "Depth camera projection, viewport transform, hit testing, and depth edit view state", automated, "Covers depth camera projection/unprojection math and pan/zoom/yaw/pitch/depth effects; UI viewport smoke remains in viewport.depth-mode."),
    Scenario("depth.operation-helpers", "Depth Edit", "Depth operation helpers apply, cancel, copy, resize, and interpolate depth arrays", automated, "Covers DepthMapped/DepthOperation copy, replace, resize, clone, and basic geometry helper contracts."),
    Scenario("depth.commands", "Depth Edit", "Depth map and individual depth operation commands", automated, "Covers Set/List/Clear Depths and Add/Update/Move/Remove/Clear/Apply depth-ops commands with undo/redo."),

    Scenario("depthbone.template-bones", "Depth Bone", "Standard DepthBone skeleton template creation", automated, "Covers standard skeleton creation, hierarchy, Head parent-to-target default, and Foot lock-to-root defaults."),
    Scenario("depthbone.template-parameters", "Depth Bone", "Standard DepthBone parameter template creation from param values", automated, "Covers standard DepthBone parameter and binding template command on a generated skeleton."),
    Scenario("depthbone.root-node", "Depth Bone", "DepthRigRoot creation, icon/type registration, serialization, and export exclusion", automated, "Covers node registration, DepthRigRoot round-trip, and INP export pruning."),
    Scenario("depthbone.bone-node", "Depth Bone", "DepthBone creation, parent/child hierarchy, rest transforms, constraints, and serialization", automated, "Covers DepthBone node round-trip of rest pose and constraints."),
    Scenario("depthbone.binding-create", "Depth Bone", "Binding creation, update, removal, and target validation", automated, "Covers command-level DepthBone source binding creation, settings update, removal, and undo/redo."),
    Scenario("depthbone.sources", "Depth Bone", "Bone Source add/remove/reorder/offset/scale/weight undo and refresh", automated, "Covers source list actions and command-level add/remove/settings undo/redo; generated refresh still needs golden fixture coverage."),
    Scenario("depthbone.influence-rule", "Depth Bone", "Influence rule get/set, terminal bone selection, max influence, and radius behavior", automated, "Covers command-level influence rule set/get with undo/redo and serialization."),
    Scenario("depthbone.preview-commands", "Depth Bone", "List, preview influence, preview deform, and apply deform commands", automated, "Covers reduced command fixture for listing bones/sources, influence preview deformation, posed deform preview, apply-to-binding, undo, and redo."),
    Scenario("depthbone.refresh-queue", "Depth Bone", "All-keypoint refresh queue slices across frames and prioritizes current keypoints", computerUse, "Needs computer-use scheduler/frame fixture."),
    Scenario("depthbone.cleanup", "Depth Bone", "Deleting bones or target structures cleans stale source/binding references", automated, "Covers DeleteNodeCommand cleanup of DepthBone source references with undo/redo."),
    Scenario("depthbone.skinning", "Depth Bone", "Skinning influence, terminal bone rule, lockToRoot, and parent-to-target options", automated, "Covers a golden two-bone fixture where terminal lockToRoot prevents parent translation from moving vertices beyond the locked terminal bone."),
    Scenario("depthbone.serialization", "Depth Bone", "DepthRigRoot, DepthBone, binding, source, influence, and template serialization round-trip", automated, "Covers native INX round-trip of root, bones, constraints, bindings, source settings, and influence rule."),
    Scenario("depthbone.export-pruning", "Depth Bone", "INP export removes editor-only DepthRigRoot and DepthBone nodes and related bindings", automated, "Covered by project.export-inp."),

    Scenario("automesh.grid-processor", "AutoMesh", "Grid AutoMesh processor for Part and GridDeformer targets", automated, "Covers deterministic alpha fixtures for Part targets and cached non-Part input."),
    Scenario("automesh.skeleton-processor", "AutoMesh", "Skeleton AutoMesh processor for Part and PathDeformer targets", automated, "Covers deterministic alpha fixture path extraction and control point output."),
    Scenario("automesh.optimum-processor", "AutoMesh", "Optimum AutoMesh processor for Part targets", automated, "Covers deterministic alpha fixture contour/skeleton triangulation output."),
    Scenario("automesh.contour-processor", "AutoMesh", "Contour AutoMesh processor and contour extraction pipeline", automated, "Covers deterministic alpha fixture contour sampling and mesh output."),
    Scenario("automesh.alpha-provider", "AutoMesh", "Alpha provider, contour thresholds, and teacher image auto-fit options", automated, "Covers PartAlphaProvider and cached AlphaInput conversion used by AutoMesh processors."),
    Scenario("automesh.non-part-targets", "AutoMesh", "AutoMesh for GridDeformer, PathDeformer, MeshGroup, and non-Part nodes", automated, "Covers GridDeformer AutoMesh through cached alpha input without render backend dependency."),
    Scenario("automesh.schema-values", "AutoMesh", "AutoMesh schema, get values, set values, presets, active processor, and derived config", automated, "Covers reflected AutoMesh schemas, values, preset application, active processor switching, and config undo/redo."),
    Scenario("automesh.batch-undo", "AutoMesh", "Batch AutoMesh settings apply as one undo/redo operation", automated, "Covers reflectable AutoMesh config changes, derived advanced values, and single-entry undo/redo."),
    Scenario("automesh.async-shortcut", "AutoMesh", "Async AutoMesh calls and shortcut-triggered execution", computerUse, "Needs UI/async smoke."),
    Scenario("automesh.processor-common", "AutoMesh", "AutoMesh common helpers, processor registration, reflected options, and error reporting", automated, "Covers processor registry, schema/value reflection, preset application, active processor switching, and config undo/redo."),

    Scenario("simplephysics.parameter", "SimplePhysics", "Parameter set, replace, clear, and drag-and-drop set undo/redo", automated, "Covered for command path by simplephysics-parameter-undo-redo."),
    Scenario("simplephysics.settings", "SimplePhysics", "Gravity, damping, frequency, output scale, length, and curve type undo/redo", automated, "Covered by simplephysics-settings-undo-redo."),
    Scenario("simplephysics.mapping", "SimplePhysics", "Model type, map mode, local-only, curve type, and parameter unmap behavior", automated, "Covers model type, map mode, local-only, parameter assignment, and parameter clear through SimplePhysics commands."),
    Scenario("simplephysics.runtime", "SimplePhysics", "Physics preview, reset, and runtime parameter output", computerUse, "Needs simulation fixture."),
    Scenario("simplephysics.serialization", "SimplePhysics", "SimplePhysics node serialization of mappings, curve type, parameter, and runtime settings", automated, "Covers native INX round-trip of parameter reference, model type, map mode, local-only, and numeric runtime settings."),

    Scenario("viewport.navigation", "Viewport/UI", "Zoom, pan, focus, reset, fit model, reset position, reset zoom, mirror, and background controls", computerUse, "Needs UI smoke."),
    Scenario("viewport.model-mode", "Viewport/UI", "Model viewport layout/deform mode switching and selected editor delegation", computerUse, "Needs UI smoke."),
    Scenario("viewport.animation-mode", "Viewport/UI", "Animation viewport mode, playback preview, and animation editor delegation", computerUse, "Needs UI smoke."),
    Scenario("viewport.depth-mode", "Viewport/UI", "Depth viewport mode, camera, editor delegation, and depth renderer integration", computerUse, "Needs UI smoke."),
    Scenario("viewport.driver-postprocess", "Viewport/UI", "Driver, onion/slice, physics, postprocess, and rendering toggles", computerUse, "Needs render smoke."),
    Scenario("viewport.flip-pairs", "Viewport/UI", "List, add, auto-add, remove flip pairs, and flip-pair window command paths", automated, "Covers list/add/auto-add/remove command paths; window opening remains computer-use."),
    Scenario("viewport.palette-command-list", "Viewport/UI", "Palette command list and command palette discovery", automated, "Covers command collection, category mapping, token derivation, and filtering."),
    Scenario("viewport.panels", "Viewport/UI", "Panel show/hide, docking layout, tree panel, parameter panel, inspector panel", computerUse, "Needs UI smoke."),
    Scenario("viewport.main-menu-toolbar-status", "Viewport/UI", "Main menu, toolbar, status bar, and viewport bottom controls", computerUse, "Needs UI smoke."),
    Scenario("viewport.action-history", "Viewport/UI", "Action history panel undo/redo index and clear behavior", automated, "Covers ActionStack index traversal, saved-state tracking, and clear behavior behind the history panel."),

    Scenario("render.backend-gl-sdl", "Rendering", "OpenGL and SDL backend initialization, resize, frame lifecycle, and shutdown", computerUse, "Needs computer-use app/backend smoke."),
    Scenario("render.atlas-packer", "Rendering", "Atlas packing, texture upload, texture slot population, and atlas invalidation", automated, "Covers packer allocation/reuse plus Puppet texture slot population, dedupe, replacement, and invalidation; GL upload smoke remains in render.backend-gl-sdl."),
    Scenario("render.blend-modes", "Rendering", "Normal, clip, slice, masks, advanced blend modes, opacity, tint, screen tint, and emission", computerUse, "Needs computer-use render snapshot fixtures."),
    Scenario("render.postprocess", "Rendering", "Postprocess toggle, difference aggregation, onion/slice, and live screenshot rendering", computerUse, "Needs computer-use render snapshot fixtures."),
    Scenario("render.camera", "Rendering", "Camera projection, viewport crop, and camera-driven export", automated, "Covers camera node projection plus PNG/TGA export command paths and output dimensions; JPEG and pixel golden coverage remain in project.export-jpeg/render.blend-modes/postprocess."),
    Scenario("render.onion-slice", "Rendering", "Onion skin, slice visualization, and model viewport postprocess flags", computerUse, "Needs computer-use render snapshot fixture."),
    Scenario("render.texture-lifecycle", "Rendering", "Texture upload, reload, disposal, atlas slot reuse, and missing texture fallbacks", automated, "Covers headless texture replacement, slot rebuild, missing-source rejection, and explicit disposal; backend upload smoke remains in render.backend-gl-sdl."),

    Scenario("settings.shortcuts", "Settings/Shortcuts", "Shortcut create, update, clear, conflict, save/load", automated, "Covers command registry shortcut conflict handling and settings-backed save/load."),
    Scenario("settings.default-shortcuts", "Settings/Shortcuts", "Default shortcut registration, command metadata, and reset-to-default behavior", automated, "Covers default shortcut registration against command instances."),
    Scenario("settings.ui", "Settings/Shortcuts", "Language, UI scale, theme, file handling, autosave, viewport settings", automated, "Covers typed settings store for UI/file/viewport settings; visual application still needs UI smoke."),
    Scenario("settings.window", "Settings/Shortcuts", "Settings window tabs, editing, validation, persistence, and cancel/apply behavior", computerUse, "Needs UI smoke."),
    Scenario("settings.ai-mcp", "Settings/Shortcuts", "AI agent and MCP settings are opt-in and persist correctly", automated, "Covers AI/MCP opt-in settings persistence without starting services."),
    Scenario("settings.paths", "Settings/Shortcuts", "Config path resolution, user data path, recent file path, and portable path behavior", automated, "Covers config, imgui, font, locale, and settings path resolution."),

    Scenario("panels.node-tree", "Panels", "Node tree display, search/filter, selection, context actions, drag reorder, and scroll state", computerUse, "Needs UI smoke."),
    Scenario("panels.parameter-list", "Panels", "Parameter list display, search/filter, group display, armed parameter controls, and keypoint grid", computerUse, "Needs UI smoke."),
    Scenario("panels.inspector", "Panels", "Inspector routing, multi-selection inspector selection, and property commit behavior", computerUse, "Needs UI smoke."),
    Scenario("panels.timeline", "Panels", "Timeline display, keyframe rows, playback controls, and animation editing panel", computerUse, "Needs UI smoke."),
    Scenario("panels.scene-resource", "Panels", "Scene panel, resource panel, logger panel, and tool-settings panel behavior", computerUse, "Needs UI smoke."),
    Scenario("panels.shell", "Panels", "Shell command input, output, error handling, and history", computerUse, "Needs UI smoke."),
    Scenario("panels.logger", "Panels", "Logger panel filtering, message display, warning/error visibility, and clear behavior", computerUse, "Needs UI smoke."),
    Scenario("panels.armed-parameter", "Panels", "Armed parameter panel shows armed state, selected keypoint, and current values", computerUse, "Needs UI smoke."),
    Scenario("panels.action-history", "Panels", "Action history panel displays groups, undo/redo cursor, saved state, and clear behavior", automated, "Covered by viewport.action-history."),

    Scenario("tools.command-browser", "Tools/Windows", "Command browser search, execution, shortcut display", computerUse, "Needs UI smoke."),
    Scenario("tools.shell", "Tools/Windows", "Shell panel execution and output behavior", computerUse, "Needs UI smoke."),
    Scenario("tools.texture-viewer", "Tools/Windows", "Texture viewer and texture operations", computerUse, "Needs UI smoke."),
    Scenario("tools.export-dialogs", "Tools/Windows", "Image, video, INP, and merge/export dialog workflows", computerUse, "Needs UI smoke."),
    Scenario("tools.ai-agent", "Tools/Windows", "AI agent panel permission, disabled state, and tool output states", computerUse, "Needs UI smoke."),
    Scenario("windows.welcome-about", "Tools/Windows", "Welcome window, about window, nag screen, and default layout behavior", computerUse, "Needs UI smoke."),
    Scenario("windows.automesh-batch", "Tools/Windows", "AutoMesh batching window selection, per-target config, apply, and undo grouping", computerUse, "Needs UI smoke."),
    Scenario("windows.export-import", "Tools/Windows", "PSD/KRA merge, image export, video export, INP export, and texture viewer windows", computerUse, "Needs UI smoke."),
    Scenario("windows.parameter-editors", "Tools/Windows", "Parameter editor, split parameter, rename, flip config, and edit animation windows", computerUse, "Needs UI smoke."),
    Scenario("windows.settings", "Tools/Windows", "Settings window tabs, validation, apply/cancel, and persistent values", computerUse, "Needs UI smoke."),
    Scenario("windows.autosave", "Tools/Windows", "Autosave recovery window accepts, rejects, deletes, and opens records", computerUse, "Needs UI smoke."),
    Scenario("windows.rename", "Tools/Windows", "Rename window commit, cancel, merge undo, and validation behavior", computerUse, "Needs UI smoke."),
    Scenario("windows.flip-config", "Tools/Windows", "Flip configuration window add, auto-add, remove, apply, and persistence", computerUse, "Needs UI smoke."),
    Scenario("windows.parameter-split", "Tools/Windows", "Parameter split window migrates axes, bindings, and undo grouping", computerUse, "Needs UI smoke."),

    Scenario("widgets.buttons-tooltips", "Widgets", "Button, toolbar, tooltip, label, category, lock, and markdown widgets", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.inputs", "Widgets", "InputText, drag, drag-drop, toggle, controller, timeline, progress, and texture widgets", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.dialogs-modals", "Widgets", "Dialog, modal, notification, output, shadow, viewport, and dummy widgets", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.button", "Widgets", "Button variants, icon buttons, disabled state, active state, and click result", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.toolbar", "Widgets", "Toolbar layout, icon spacing, selection, disabled state, and tooltip integration", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.tooltip", "Widgets", "Tooltip delay, markdown text, wrapped labels, and disabled widget hints", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.label-category", "Widgets", "Label, category header, collapsible state, and nested layout behavior", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.inputtext", "Widgets", "InputText editing, focus, commit, cancel, and IME-safe behavior", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.drag", "Widgets", "Drag numeric controls, slider stepping, commit boundaries, and formatting", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.dragdrop", "Widgets", "Drag-drop source, target, payload typing, cancellation, and hover feedback", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.toggle-lock", "Widgets", "Toggle and lock widgets show boolean state and produce one commit per change", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.controller", "Widgets", "2D controller widget hit testing, snapping, current value display, and keypoint selection", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.timeline", "Widgets", "Timeline widget track rows, key markers, scrub position, zoom, and selection", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.progress-output-notification", "Widgets", "Progress, output, notification, and status widgets render and clear state correctly", computerUse, "Needs computer-use widget-level render fixture."),
    Scenario("widgets.texture-viewport-shadow", "Widgets", "Texture preview, viewport widget, shadow, dummy, and modal helper drawing", computerUse, "Needs computer-use widget-level render fixture."),

    Scenario("i18n.wrapped-strings", "I18N", "UI strings are wrapped by _() or __() where appropriate", automated, "Covers common visible UI text call sites with a source scan for direct English string literals."),
    Scenario("i18n.pot", "I18N", "POT update includes new UI text", automated, "Covers template.pot presence and simple single-line _()/__() source msgid coverage."),
    Scenario("platform.windows-write", "Platform/Crash", "Windows release build does not crash on console write paths", automated, "Covers release-sensitive console write calls with a source scan; file/socket writes and debug/version/unittest writes are allowed."),
    Scenario("platform.paths-dpi-fonts", "Platform/Crash", "Platform paths, DPI scaling, font loading, logo resources, and config paths", automated, "Covers config/font/locale path creation and DPI scale persistence in a headless platform fixture."),
    Scenario("platform.crashdump", "Platform/Crash", "Crash dump setup, exception logging, and release crash paths", automated, "Covers crash dump text generation, path generation, and file writing without triggering a native crash."),
    Scenario("platform.startup-shutdown", "Platform/Crash", "Startup, shutdown, background service cleanup, and no module-constructor cycles", automated, "Covers a source-level module-constructor inventory guard and test-binary startup without constructor cycles."),
    Scenario("platform.version", "Platform/Crash", "Version metadata is available to UI, export, diagnostics, and release builds", automated, "Covers generated version metadata availability."),
    Scenario("platform.input-window", "Platform/Crash", "Window lifecycle, input event mapping, timing callback, DPI changes, and focus state", computerUse, "Needs computer-use backend/window fixture."),
    Scenario("platform.tasks", "Platform/Crash", "Background task queue runs, cancels, reports errors, and shuts down cleanly", automated, "Covers task queue add, yield, update, completion, status, and progress reset."),
    Scenario("platform.debug-logging", "Platform/Crash", "Debug logging is gated by build mode and does not crash release builds", automated, "Covers source-level console/debug output guard shared with Windows console write safety."),

    Scenario("undo.grouped-actions", "Undo/Redo", "Action groups undo/redo as one unit", automated, "Covered by action-group-undo-redo."),
    Scenario("undo.command-actions", "Undo/Redo", "All mutating commands push undoable actions or explicitly declare non-mutating behavior", automated, "Covers source-level command audit for Action pushes, grouped action pushes, history helpers, and explicitly non-undoable command classes."),
    Scenario("undo.ui-commit-boundaries", "Undo/Redo", "Text edits, drags, sliders, toggles, and dialogs merge or split undo entries at correct commit boundaries", computerUse, "Needs computer-use UI interaction smoke."),
    Scenario("undo.direct-mutation-audit", "Undo/Redo", "Model, parameter, deformer, mesh, and inspector mutations go through actions", automated, "Covers a source-level guard for direct model/parameter/deformation writes in command, panel, window, and viewport layers; targeted fixtures remain in related scenarios."),
    Scenario("undo.scope-guards", "Undo/Redo", "Mode, editor, subtool, and nested scopes close automatically at defined guard boundaries", automated, "Covered by mesh/depth/onetime scope guard fixtures."),
    Scenario("undo.action-merge", "Undo/Redo", "Text edit and drag actions merge within one edit session and split across sessions", automated, "Covers core action-stack merge semantics for same-target edits, non-merge splits, undo truncation, and redo of merged final values; UI commit boundaries remain in undo.ui-commit-boundaries."),
];

@ShortcutHidden @McpHidden @GuiDialog @EffectConfigEdit
private class RegressionMetaCommand : ExCommand!() {
    this() {
        super("Meta Label", "Meta Description");
        ngRegisterCommandMeta(this);
    }
}

private class RegressionArgCommand : ExCommand!(TW!(string, "name", "Name"), int) {
    this() {
        super("Arg Label", "Arg Description", "Alice", 7);
    }
}

private void runCase(string name, void function() test) {
    stderr.writeln("running ", name);
    test();
}

private void resetCase() {
    incActionClearHistory();
    ngCreateHeadlessRegressionProject();
    incActionClearHistory();
}

private void require(bool condition, string message) {
    enforce(condition, message);
}

private void configureRegressionConfigDir() {
    import nijigenerate.core.path : ENV_CONFIG_PATH;
    import std.process : environment;

    auto configDir = buildPath("/private/tmp", "nijigenerate-regression-config");
    if (!exists(configDir))
        mkdirRecurse(configDir);
    environment[ENV_CONFIG_PATH] = configDir;
}

private uint[] maskOrder(Part part) {
    uint[] result;
    foreach (mask; part.masks)
        result ~= mask.maskSrcUUID;
    return result;
}

private void assertMaskOrder(Part part, scope const(uint)[] expected, string label) {
    auto actual = maskOrder(part);
    require(actual == expected, label);
}

private Part newPart(string name) {
    auto part = new Part(incActivePuppet().root);
    part.name = name;
    return part;
}

private Part newMeshPart(string name) {
    MeshData data;
    data.vertices = Vec2Array([
        vec2(0, 0),
        vec2(10, 0),
        vec2(0, 10),
    ]);
    data.uvs = Vec2Array([
        vec2(0, 0),
        vec2(1, 0),
        vec2(0, 1),
    ]);
    data.indices = [0, 1, 2];
    auto texture = new Texture(cast(ubyte[])[255, 255, 255, 255], 1, 1, 4, 4, false, false);
    auto part = new Part(data, [texture], incActivePuppet().root);
    part.name = name;
    return part;
}

private string[] directPartNames(Puppet puppet) {
    string[] result;
    foreach (child; puppet.root.children) {
        if (auto part = cast(Part)child)
            result ~= part.name;
    }
    return result;
}

private Part findDirectPart(Puppet puppet, string name) {
    foreach (child; puppet.root.children) {
        if (auto part = cast(Part)child) {
            if (part.name == name)
                return part;
        }
    }
    return null;
}

private Parameter findParameter(Puppet puppet, string name) {
    foreach (param; puppet.parameters) {
        if (param.name == name)
            return param;
    }
    return null;
}

private void writeRegressionPng(string path, ubyte r, ubyte g, ubyte b, int width = 2, int height = 2) {
    ubyte[] pixels;
    pixels.length = width * height * 4;
    foreach (i; 0 .. width * height) {
        auto offset = i * 4;
        pixels[offset + 0] = r;
        pixels[offset + 1] = g;
        pixels[offset + 2] = b;
        pixels[offset + 3] = 255;
    }
    auto texture = ShallowTexture(pixels, width, height, 4);
    texture.save(path);
}

private void writeRegressionFixtureBase64(string path, string encoded) {
    write(path, Base64.decode(encoded));
}

private void writeRegressionPsdFixture(string path) {
    enum minimalPsd = "OEJQUwABAAAAAAAAAAMAAAABAAAAAQAIAAMAAAAAAAAAAAAAAAQAAAAAAAAAAAA=";
    writeRegressionFixtureBase64(path, minimalPsd);
}

private void writeRegressionKraFixture(string path) {
    enum minimalKra = "UEsDBBQAAAAAALmEulyh8AOnEwAAABMAAAAIAAAAbWltZXR5cGVhcHBsaWNhdGlvbi94LWtyaXRhUEsDBBQAAAAAALmEulyi9NGxYAAAAGAAAAALAAAAbWFpbmRvYy54bWw8RE9DPjxJTUFHRSBuYW1lPSJyZWdyZXNzaW9uIiB3aWR0aD0iMSIgaGVpZ2h0PSIxIiBjb2xvcnNwYWNlbmFtZT0iUkdCQSI+PGxheWVycy8+PC9JTUFHRT48L0RPQz5QSwECFAMUAAAAAAC5hLpcofADpxMAAAATAAAACAAAAAAAAAAAAAAAgAEAAAAAbWltZXR5cGVQSwECFAMUAAAAAAC5hLpcovTRsWAAAABgAAAACwAAAAAAAAAAAAAAgAE5AAAAbWFpbmRvYy54bWxQSwUGAAAAAAIAAgBvAAAAwgAAAAAA";
    writeRegressionFixtureBase64(path, minimalKra);
}

private void testPSDAndKRAReaderImportMergeFixtures() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-psd-kra");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto psdPath = buildPath(fixtureDir, "minimal.psd");
    auto kraPath = buildPath(fixtureDir, "minimal.kra");
    writeRegressionPsdFixture(psdPath);
    writeRegressionKraFixture(kraPath);

    PSD psdDoc = parsePSDDocument(psdPath);
    require(psdDoc.width == 1 && psdDoc.height == 1, "PSD reader should parse generated fixture dimensions");
    require(psdDoc.layers.length == 0, "PSD reader should accept an empty-layer fixture");

    KRA kraDoc = parseKRADocument(kraPath);
    require(kraDoc.width == 1 && kraDoc.height == 1, "KRA reader should parse generated fixture dimensions");
    require(kraDoc.layers.length == 0, "KRA reader should accept an empty-layer fixture");

    auto ctx = new Context();
    require((new ImportPSDCommand(psdPath)).run(ctx).succeeded, "ImportPSDCommand should accept generated PSD fixture");
    require(incActivePuppet() !is null, "PSD import should leave an active puppet");

    require((new ImportKRACommand(kraPath)).run(ctx).succeeded, "ImportKRACommand should accept generated KRA fixture");
    require(incActivePuppet() !is null, "KRA import should leave an active puppet");

    resetCase();
    auto basePart = newMeshPart("psd-kra-merge-base");
    require((new MergePSDCommand(psdPath, false, false)).run(ctx).succeeded, "MergePSDCommand should accept generated PSD fixture");
    require(findDirectPart(incActivePuppet(), "psd-kra-merge-base") is basePart, "PSD merge should preserve existing Part");
    require((new MergeKRACommand(kraPath, false, false)).run(ctx).succeeded, "MergeKRACommand should accept generated KRA fixture");
    require(findDirectPart(incActivePuppet(), "psd-kra-merge-base") is basePart, "KRA merge should preserve existing Part");
}

private void testImageCodecRoundTrips() {
    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-image-codecs");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto sourcePng = buildPath(fixtureDir, "source.png");
    writeRegressionPng(sourcePng, 24, 128, 240, 5, 3);

    auto png = ShallowTexture(sourcePng, 4);
    require(png.width == 5 && png.height == 3 && png.convChannels == 4, "PNG codec should load generated RGBA dimensions");

    auto texture = new Texture(png);
    foreach (extension; ["png", "tga"]) {
        auto path = buildPath(fixtureDir, "roundtrip." ~ extension);
        texture.save(path);
        require(exists(path) && isFile(path), extension ~ " codec should write a texture file");
        auto loaded = ShallowTexture(path, 4);
        require(loaded.width == 5 && loaded.height == 3, extension ~ " codec should preserve texture dimensions");
        require(loaded.convChannels == 4, extension ~ " codec should convert loaded texture to RGBA");
    }
    texture.dispose();

    auto psdReader = buildPath(regressionSourceRoot("io"), "psd.d");
    auto kraReader = buildPath(regressionSourceRoot("io"), "kra.d");
    require(exists(psdReader) && readText(psdReader).canFind("module nijigenerate.io.psd"), "PSD reader module should remain present for fixture-backed import scenarios");
    require(exists(kraReader) && readText(kraReader).canFind("module nijigenerate.io.kra"), "KRA reader module should remain present for fixture-backed import scenarios");
}

private bool near(float a, float b) {
    auto d = a - b;
    if (d < 0)
        d = -d;
    return d < 0.0001f;
}

private bool nearVec3(vec3 a, vec3 b) {
    return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

private bool nearVec4(vec4 a, vec4 b) {
    return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z) && near(a.w, b.w);
}

private bool nearVec2(vec2 a, vec2 b) {
    return near(a.x, b.x) && near(a.y, b.y);
}

private bool nearVec2Array(Vec2Array a, Vec2Array b) {
    if (a.length != b.length)
        return false;
    foreach (i; 0 .. a.length) {
        if (!nearVec2(a[i], b[i]))
            return false;
    }
    return true;
}

private bool containsVec2(Vec2Array values, vec2 expected) {
    foreach (i; 0 .. values.length) {
        if (nearVec2(values[i], expected))
            return true;
    }
    return false;
}

private bool jsonEquivalent(string a, string b) {
    import std.json : parseJSON;
    return parseJSON(a) == parseJSON(b);
}

private Node findDirectNode(Puppet puppet, string name) {
    foreach (child; puppet.root.children) {
        if (child.name == name)
            return child;
    }
    return null;
}

private Node findNodeRecursive(Node root, string name) {
    if (root.name == name)
        return root;
    foreach (child; root.children) {
        auto found = findNodeRecursive(child, name);
        if (found !is null)
            return found;
    }
    return null;
}

private void appendNodeSummary(Node root, ref string[] lines, string indent = "") {
    lines ~= indent ~ root.name ~ ":" ~ root.typeId();
    foreach (child; root.children)
        appendNodeSummary(child, lines, indent ~ "  ");
}

private string nodeTreeSummary(Node root) {
    string[] lines;
    appendNodeSummary(root, lines);
    return lines.join("\n");
}

private void testMaskSourceAddUndoRedo() {
    resetCase();

    auto target = newPart("target");
    auto mask = newPart("mask");

    incActionPush(new PartAddMaskAction(mask, target, MaskingMode.Mask));
    assertMaskOrder(target, [mask.uuid], "mask add should apply immediately");

    incActionUndo();
    assertMaskOrder(target, [], "mask add undo should remove the mask");

    incActionRedo();
    assertMaskOrder(target, [mask.uuid], "mask add redo should restore the mask");
}

private void testMaskSourceReorderUndoRedo() {
    resetCase();

    auto target = newPart("target");
    auto maskA = newPart("maskA");
    auto maskB = newPart("maskB");
    auto maskC = newPart("maskC");

    target.masks = [
        MaskBinding(maskA.uuid, MaskingMode.Mask, maskA),
        MaskBinding(maskB.uuid, MaskingMode.Mask, maskB),
        MaskBinding(maskC.uuid, MaskingMode.DodgeMask, maskC),
    ];

    auto oldMasks = target.masks.dup;
    auto moving = oldMasks[1];
    auto newMasks = oldMasks[0..1] ~ oldMasks[2..$];
    newMasks = newMasks[0..0] ~ moving ~ newMasks[0..$];

    incActionPush(new PartMaskListChangeAction("Reorder Mask Source", target, oldMasks, newMasks));
    assertMaskOrder(target, [maskB.uuid, maskA.uuid, maskC.uuid], "mask reorder should apply immediately");

    incActionUndo();
    assertMaskOrder(target, [maskA.uuid, maskB.uuid, maskC.uuid], "mask reorder undo should restore exact old order");

    incActionRedo();
    assertMaskOrder(target, [maskB.uuid, maskA.uuid, maskC.uuid], "mask reorder redo should restore exact new order");
}

private void testMaskSourceModeUndoRedo() {
    resetCase();

    auto target = newPart("target");
    auto mask = newPart("mask");

    target.masks = [
        MaskBinding(mask.uuid, MaskingMode.Mask, mask),
    ];

    incActionPush(new PartChangeMaskModeAction(target, mask, MaskingMode.DodgeMask));
    require(target.masks[0].mode == MaskingMode.DodgeMask, "mask mode change should apply immediately");

    incActionUndo();
    require(target.masks[0].mode == MaskingMode.Mask, "mask mode undo should restore old mode");

    incActionRedo();
    require(target.masks[0].mode == MaskingMode.DodgeMask, "mask mode redo should restore new mode");
}

private void testWeldingUndoRedo() {
    resetCase();

    auto drawable = newMeshPart("drawable");
    auto target = newMeshPart("target");
    ptrdiff_t[] indices = [0, 1, 2];

    incActionPush(new DrawableAddWeldingAction(drawable, target, indices, 0.25f));
    require(drawable.welded.length == 1, "welding add should create link");
    require(target.welded.length == 1, "welding add should create counter link");
    require(near(drawable.welded[0].weight, 0.25f), "welding add should set weight");
    require(near(target.welded[0].weight, 0.75f), "welding add should set counter weight");

    ptrdiff_t[] changedIndices = [2, 1, 0];
    incActionPush(new DrawableChangeWeldingAction(drawable, target, changedIndices, 0.6f));
    require(drawable.welded[0].indices == changedIndices, "welding change should update indices");
    require(near(drawable.welded[0].weight, 0.6f), "welding change should update weight");
    require(near(target.welded[0].weight, 0.4f), "welding change should update counter weight");

    incActionUndo();
    require(drawable.welded[0].indices == indices, "welding change undo should restore indices");
    require(near(drawable.welded[0].weight, 0.25f), "welding change undo should restore weight");

    incActionRedo();
    require(drawable.welded[0].indices == changedIndices, "welding change redo should restore indices");
    require(near(drawable.welded[0].weight, 0.6f), "welding change redo should restore weight");

    incActionPush(new DrawableRemoveWeldingAction(drawable, target));
    require(drawable.welded.length == 0, "welding remove should remove link");
    require(target.welded.length == 0, "welding remove should remove counter link");

    incActionUndo();
    require(drawable.welded.length == 1, "welding remove undo should restore link");
    require(target.welded.length == 1, "welding remove undo should restore counter link");

    incActionRedo();
    require(drawable.welded.length == 0, "welding remove redo should remove link again");
    require(target.welded.length == 0, "welding remove redo should remove counter link again");
}

private void testWeldingRuntimeDeformation() {
    resetCase();

    auto drawable = newMeshPart("weld-runtime-drawable");
    auto target = newMeshPart("weld-runtime-target");

    drawable.deformation[0] = vec2(8, 0);
    drawable.deformation[1] = vec2(8, 0);
    drawable.deformation[2] = vec2(8, 0);
    target.deformation[0] = vec2(0, 0);
    target.deformation[1] = vec2(0, 0);
    target.deformation[2] = vec2(0, 0);

    ptrdiff_t[] indices = [0, 1, 2];
    incActionPush(new DrawableAddWeldingAction(drawable, target, indices, 0.25f));
    require(drawable.welded.length == 1 && target.welded.length == 1, "welding runtime setup should create reciprocal links");
    require(drawable.welded[0].indices == indices, "source welding should preserve explicit index mapping");
    require(target.welded[0].indices == indices, "counter welding should map target vertices back to source vertices");
    require(near(drawable.welded[0].weight, 0.25f), "source welding weight should be preserved");
    require(near(target.welded[0].weight, 0.75f), "counter welding weight should be inverse of source weight");
    RenderContext ctx;
    drawable.runPostTask2(ctx);
    target.runPostTask2(ctx);
    require(nearVec2(drawable.deformation[0], vec2(2, 0)) &&
            nearVec2(drawable.deformation[1], vec2(2, 0)) &&
            nearVec2(drawable.deformation[2], vec2(2, 0)),
        "welding runtime should pull source deformation toward the weighted shared position");
    require(nearVec2(target.deformation[0], vec2(2, 0)) &&
            nearVec2(target.deformation[1], vec2(2, 0)) &&
            nearVec2(target.deformation[2], vec2(2, 0)),
        "welding runtime should push target deformation toward the weighted shared position");

    incActionUndo();
    require(drawable.welded.length == 0 && target.welded.length == 0, "undo welding setup should remove reciprocal links");

    drawable.deformation[0] = vec2(8, 0);
    drawable.deformation[1] = vec2(8, 0);
    drawable.deformation[2] = vec2(8, 0);
    target.deformation[0] = vec2(0, 0);
    target.deformation[1] = vec2(0, 0);
    target.deformation[2] = vec2(0, 0);
    drawable.runPostTask2(ctx);
    target.runPostTask2(ctx);
    require(nearVec2(drawable.deformation[0], vec2(8, 0)) &&
            nearVec2(target.deformation[0], vec2(0, 0)),
        "undo welding setup should remove reciprocal post-process behavior");
}

private void testNodeNameUndoMergesPerNode() {
    resetCase();

    auto nodeA = new Node(incActivePuppet().root);
    auto nodeB = new Node(incActivePuppet().root);
    nodeA.name = "A";
    nodeB.name = "B";

    string oldA = nodeA.name;
    nodeA.name = "A1";
    incActionPush(new NodeValueChangeAction!(Node, string)("name", nodeA, oldA, nodeA.name, &nodeA.name_));

    nodeA.name = "A12";
    incActionPush(new NodeValueChangeAction!(Node, string)("name", nodeA, "A1", nodeA.name, &nodeA.name_));

    require(incActionHistory().length == 1, "successive name edits on the same node should merge");

    string oldB = nodeB.name;
    nodeB.name = "B1";
    incActionPush(new NodeValueChangeAction!(Node, string)("name", nodeB, oldB, nodeB.name, &nodeB.name_));

    require(incActionHistory().length == 2, "name edits on different nodes must not merge");

    incActionUndo();
    require(nodeB.name == "B", "undo should restore the second node name");
    require(nodeA.name == "A12", "undo of node B must not affect node A");

    incActionUndo();
    require(nodeA.name == "A", "merged node A name undo should restore the original value");

    incActionRedo();
    require(nodeA.name == "A12", "merged node A name redo should restore the final value");
}

private bool isChildOf(Node parent, Node child) {
    foreach (candidate; parent.children) {
        if (candidate is child)
            return true;
    }
    return false;
}

private immutable string[] regressionNodeMenuTypes = [
    "Node",
    "Mask",
    "Composite",
    "SimplePhysics",
    "MeshGroup",
    "DynamicComposite",
    "PathDeformer",
    "GridDeformer",
    "DepthRigRoot",
    "DepthBone",
    "Camera",
];

private void ensureRegressionNodeTypesRegistered() {
    // The full app initializes node factories through renderer/runtime setup.
    // The headless regression harness does not create a renderer, so register
    // the menu-visible factories explicitly.
    inRegisterNodeType!Node();
    inRegisterNodeType!Mask();
    inRegisterNodeType!Composite();
    inRegisterNodeType!SimplePhysics();
    inRegisterNodeType!MeshGroup();
    inRegisterNodeType!DynamicComposite();
    inRegisterNodeType!PathDeformer();
    inRegisterNodeType!GridDeformer();
    incInitExtNodes();
}

private void testNodeDynamicAddTypesUndoRedo() {
    ensureRegressionNodeTypesRegistered();

    foreach (className; regressionNodeMenuTypes) {
        resetCase();

        auto ctx = new Context();
        ctx.puppet = incActivePuppet();

        auto result = ensureAddNodeCommand(className).run(ctx);
        require(result.succeeded, "dynamic AddNode should succeed for " ~ className);
        require(result.created.length == 1, "dynamic AddNode should create exactly one node for " ~ className);

        auto created = result.created[0];
        require(created !is null, "dynamic AddNode should return a created node for " ~ className);
        require(created.typeId() == className, "dynamic AddNode should create requested type " ~ className ~ ", got " ~ created.typeId());
        require(created.parent is incActivePuppet().root, "dynamic AddNode should parent " ~ className ~ " under root when no selection exists");
        require(isChildOf(incActivePuppet().root, created), "dynamic AddNode should insert " ~ className ~ " into root children");

        incActionUndo();
        require(!isChildOf(incActivePuppet().root, created), "undo dynamic AddNode should remove " ~ className);
        incActionRedo();
        require(isChildOf(incActivePuppet().root, created), "redo dynamic AddNode should restore " ~ className);
    }
}

private void testNodeDynamicInsertTypesUndoRedo() {
    ensureRegressionNodeTypesRegistered();

    foreach (className; regressionNodeMenuTypes) {
        resetCase();

        auto parent = incActivePuppet().root;
        auto child = new Node(parent);
        child.name = "insert-child-" ~ className;

        auto ctx = new Context();
        ctx.puppet = incActivePuppet();
        ctx.nodes = [child];

        auto result = ensureInsertNodeCommand(className).run(ctx);
        require(result.succeeded, "dynamic InsertNode should succeed for " ~ className);
        require(result.created.length == 1, "dynamic InsertNode should create exactly one node for " ~ className);

        auto inserted = result.created[0];
        require(inserted !is null, "dynamic InsertNode should return a created node for " ~ className);
        require(inserted.typeId() == className, "dynamic InsertNode should create requested type " ~ className ~ ", got " ~ inserted.typeId());
        require(inserted.parent is parent, "dynamic InsertNode should place " ~ className ~ " under the original parent");
        require(child.parent is inserted, "dynamic InsertNode should move the selected child under inserted " ~ className);
        require(isChildOf(inserted, child), "dynamic InsertNode should keep child in inserted node children for " ~ className);

        incActionUndo();
        require(child.parent is parent, "undo dynamic InsertNode should restore selected child parent for " ~ className);
        require(isChildOf(parent, child), "undo dynamic InsertNode should restore selected child in original parent for " ~ className);
        require(!isChildOf(parent, inserted), "undo dynamic InsertNode should remove inserted " ~ className);

        incActionRedo();
        require(inserted.parent is parent, "redo dynamic InsertNode should restore inserted " ~ className);
        require(child.parent is inserted, "redo dynamic InsertNode should move selected child under inserted " ~ className);
        require(isChildOf(inserted, child), "redo dynamic InsertNode should restore child list for " ~ className);
    }
}

private void testNodeRegistryDynamicCommandCoverage() {
    ensureRegressionNodeTypesRegistered();

    foreach (className; regressionNodeMenuTypes) {
        resetCase();

        auto addCommand = ensureAddNodeCommand(className);
        auto insertCommand = ensureInsertNodeCommand(className);

        require(addCommand !is null, "node registry should expose AddNode command for " ~ className);
        require(insertCommand !is null, "node registry should expose InsertNode command for " ~ className);
        require(addCommand.label().length > 0, "AddNode command should have a label for " ~ className);
        require(addCommand.description().length > 0, "AddNode command should have a description for " ~ className);
        require(insertCommand.label().length > 0, "InsertNode command should have a label for " ~ className);
        require(insertCommand.description().length > 0, "InsertNode command should have a description for " ~ className);
        require(addCommand.shortcutRunnable(), "dynamic AddNode command should be shortcut-visible for " ~ className);
        require(insertCommand.shortcutRunnable(), "dynamic InsertNode command should be shortcut-visible for " ~ className);
        require(addCommand.mcpExposed(), "dynamic AddNode command should be MCP-visible for " ~ className);
        require(insertCommand.mcpExposed(), "dynamic InsertNode command should be MCP-visible for " ~ className);

        auto addId = ngCommandIdFromKey(AddNodeKey(className));
        auto insertId = ngCommandIdFromKey(InsertNodeKey(className));
        require(addId == "Node.Add." ~ className, "AddNode key should produce a stable command id for " ~ className ~ ": " ~ addId);
        require(insertId == "Node.Insert." ~ className, "InsertNode key should produce a stable command id for " ~ className ~ ": " ~ insertId);

        auto ctx = new Context();
        ctx.puppet = incActivePuppet();
        auto added = addCommand.run(ctx);
        require(added.succeeded, "node registry AddNode construction should succeed for " ~ className);
        require(added.created.length == 1 && added.created[0] !is null, "node registry AddNode should return a constructed node for " ~ className);
        require(added.created[0].typeId() == className, "node registry AddNode should construct requested type " ~ className);
    }
}

private Resource[] runSelector(string query) {
    auto selector = new Selector();
    selector.build(query);
    return selector.run();
}

private bool resourceNamed(Resource[] resources, string name, ResourceType type) {
    foreach (resource; resources) {
        if (resource.type == type && resource.name == name)
            return true;
    }
    return false;
}

private void testSelectorQueryAndTreeStore() {
    resetCase();
    ensureRegressionNodeTypesRegistered();

    auto body = new Node(incActivePuppet().root);
    body.name = "Body";
    auto face = new Node(body);
    face.name = "Face";
    auto grid = new GridDeformer(body);
    grid.name = "Grid";
    auto part = newMeshPart("PartA");
    body.addChild(part);

    auto param = new ExParameter("ParamA", false);
    incActivePuppet().parameters ~= param;
    auto binding = newValueBinding(param, face, "transform.t.x");
    binding.setValue(vec2u(1, 0), 0.5f);

    auto allNodes = runSelector("*");
    require(resourceNamed(allNodes, "Body", ResourceType.Node), "selector * should include Body node");
    require(resourceNamed(allNodes, "Face", ResourceType.Node), "selector * should include Face node");
    require(resourceNamed(allNodes, "Grid", ResourceType.Node), "selector * should include GridDeformer node");
    require(resourceNamed(allNodes, "PartA", ResourceType.Node), "selector * should include Part node");
    require(resourceNamed(allNodes, "ParamA", ResourceType.Parameter), "selector * should include parameters");

    auto directGrid = runSelector("Node.Body > GridDeformer.Grid");
    require(directGrid.length == 1, "selector should find direct GridDeformer child");
    require(directGrid[0].name == "Grid" && directGrid[0].typeId == "GridDeformer", "selector direct child should return GridDeformer");

    auto attrFace = runSelector(`*[name="Face"]`);
    require(attrFace.length == 1 && attrFace[0].name == "Face", "selector attribute name query should find Face");

    auto boundParam = runSelector("Node.Face Parameter");
    require(boundParam.length == 1 && boundParam[0].name == "ParamA", "selector should find parameter bound to Face");

    auto boundBinding = runSelector("Node.Face Binding");
    require(boundBinding.length == 1 && boundBinding[0].type == ResourceType.Binding, "selector should find binding bound to Face");

    auto parameterBinding = runSelector("Parameter.ParamA Binding");
    require(parameterBinding.length == 1 && parameterBinding[0].type == ResourceType.Binding, "selector should walk from parameter to binding");

    auto treeResources = runSelector("Node.Body, GridDeformer.Grid");
    auto store = new TreeStore_!false();
    store.setResources(treeResources);
    require(store.roots.length == 1, "tree store should preserve selected ancestor as root");
    require(store.roots[0].name == "Body", "tree store root should be selected Body ancestor");
    require((store.roots[0] in store.children) !is null, "tree store should register root children");
    require(store.children[store.roots[0]].length == 1, "tree store should attach selected grid under Body");
    require(store.children[store.roots[0]][0].name == "Grid", "tree store child should be Grid");
}

private void testNodeCommandCreateDeleteToggleUndoRedo() {
    resetCase();

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();

    auto addResult = (new AddNodeCommand("Node")).run(ctx);
    require(addResult.succeeded, "AddNodeCommand should succeed");
    auto node = addResult.created[0];
    require(node.parent is incActivePuppet().root, "AddNodeCommand should parent node under puppet root");
    require(isChildOf(incActivePuppet().root, node), "AddNodeCommand should insert node into root children");

    incActionUndo();
    require(!isChildOf(incActivePuppet().root, node), "undo AddNodeCommand should remove created node");
    incActionRedo();
    require(isChildOf(incActivePuppet().root, node), "redo AddNodeCommand should restore created node");

    ctx.nodes = [node];
    require((new ToggleVisibilityCommand()).run(ctx).succeeded, "ToggleVisibilityCommand should succeed");
    require(!node.getEnabled(), "ToggleVisibilityCommand should disable enabled node");
    incActionUndo();
    require(node.getEnabled(), "undo ToggleVisibilityCommand should restore enabled state");
    incActionRedo();
    require(!node.getEnabled(), "redo ToggleVisibilityCommand should restore disabled state");

    require((new DeleteNodeCommand()).run(ctx).succeeded, "DeleteNodeCommand should succeed");
    require(!isChildOf(incActivePuppet().root, node), "DeleteNodeCommand should remove node from parent");
    incActionUndo();
    require(isChildOf(incActivePuppet().root, node), "undo DeleteNodeCommand should restore node under parent");
    incActionRedo();
    require(!isChildOf(incActivePuppet().root, node), "redo DeleteNodeCommand should remove node again");
}

private void testNodeClipboardCopyPasteUndoRedo() {
    resetCase();

    auto parent = new Node(incActivePuppet().root);
    parent.name = "ClipboardParent";
    auto child = new Node(parent);
    child.name = "ClipboardChild";
    auto grandchild = new Node(child);
    grandchild.name = "ClipboardGrandchild";

    auto ctxChild = new Context();
    ctxChild.nodes = [child];
    auto ctxParent = new Context();
    ctxParent.nodes = [parent];

    clipboardNodes.length = 0;
    auto cut = new CutNodeCommand();
    require(!cut.runnable(ctxChild), "CutNodeCommand should remain disabled until cut semantics are implemented");

    auto copy = new CopyNodeCommand();
    require(copy.runnable(ctxChild), "CopyNodeCommand should be runnable for selected nodes");
    require(copy.run(ctxChild).succeeded, "CopyNodeCommand should copy selected node");
    require(clipboardNodes.length == 1, "CopyNodeCommand should populate node clipboard");
    require(clipboardNodes[0] !is child, "copied node should be a duplicate object");
    require(clipboardNodes[0].uuid != child.uuid, "copied node should regenerate UUID");
    require(clipboardNodes[0].children.length == 1, "copied node should preserve children");
    require(clipboardNodes[0].children[0].uuid != grandchild.uuid, "copied child should regenerate UUID");

    auto originalChildren = parent.children.length;
    auto paste = new PasteNodeCommand();
    require(paste.runnable(ctxParent), "PasteNodeCommand should be runnable when clipboard has nodes");
    require(paste.run(ctxParent).succeeded, "PasteNodeCommand should paste clipboard nodes");
    require(clipboardNodes.length == 0, "PasteNodeCommand should consume clipboard nodes");
    require(parent.children.length == originalChildren + 1, "paste should add one child under target parent");
    Node pasted;
    foreach (candidate; parent.children) {
        if (candidate !is child && candidate.uuid != child.uuid) {
            pasted = candidate;
            break;
        }
    }
    require(pasted !is null, "paste should add a duplicated child");
    require(pasted !is child && pasted.uuid != child.uuid, "pasted node should be a distinct duplicate");
    require(pasted.children.length == 1 && pasted.children[0].uuid != grandchild.uuid,
        "pasted node should preserve descendant structure");

    incActionUndo();
    require(parent.children.length == originalChildren, "undo paste should remove pasted duplicate");
    incActionRedo();
    require(parent.children.length == originalChildren + 1, "redo paste should restore pasted duplicate");

    clipboardNodes.length = 0;
}

private void testProjectImportImagesCommandPaths() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-image-import");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto imageA = buildPath(fixtureDir, "layer-a.png");
    auto imageB = buildPath(fixtureDir, "layer-b.png");
    writeRegressionPng(imageA, 255, 0, 0, 2, 3);
    writeRegressionPng(imageB, 0, 255, 0, 4, 2);

    auto ctx = new Context();
    require((new ImportImageFolderCommand(fixtureDir)).run(ctx).succeeded, "ImportImageFolderCommand should succeed");

    auto imported = incActivePuppet();
    require(imported !is null, "image-folder import should leave an active puppet");
    require(findDirectPart(imported, "layer-a") !is null, "image-folder import should create layer-a part");
    require(findDirectPart(imported, "layer-b") !is null, "image-folder import should create layer-b part");

    auto partA = findDirectPart(imported, "layer-a");
    auto partB = findDirectPart(imported, "layer-b");
    require(partA.textures.length > 0 && partA.textures[0] !is null, "imported layer-a should have a texture");
    require(partB.textures.length > 0 && partB.textures[0] !is null, "imported layer-b should have a texture");
    require(partA.textures[0].width == 2 && partA.textures[0].height == 3, "layer-a texture dimensions should match source PNG");
    require(partB.textures[0].width == 4 && partB.textures[0].height == 2, "layer-b texture dimensions should match source PNG");

    resetCase();
    require((new MergeImageFilesCommand(imageA ~ "|" ~ imageB)).run(ctx).succeeded, "MergeImageFilesCommand should succeed");

    auto merged = incActivePuppet();
    require(findDirectPart(merged, "layer-a.png") !is null, "merge image files should add layer-a part");
    require(findDirectPart(merged, "layer-b.png") !is null, "merge image files should add layer-b part");
    require(directPartNames(merged).length == 2, "merge image files should create exactly the two fixture parts");
}

private void drainRegressionTasks() {
    import nijigenerate.core.tasks : incTaskLength, incTaskUpdate;

    size_t guard;
    while (incTaskLength() > 0 && guard++ < 1000)
        incTaskUpdate();
    require(incTaskLength() == 0, "task queue should drain within guard");
}

private void testProjectTextureMaintenanceCommands() {
    resetCase();

    auto pixels = cast(ubyte[])[200, 100, 50, 128];
    auto texture = new Texture(pixels.dup, 1, 1, 4, 4, false, false);
    texture.lock();
    texture.setData(pixels.dup);

    MeshData data;
    data.vertices = Vec2Array([vec2(0, 0), vec2(1, 0), vec2(0, 1)]);
    data.uvs = Vec2Array([vec2(0, 0), vec2(1, 0), vec2(0, 1)]);
    data.indices = [0, 1, 2];
    auto part = new Part(data, [texture], incActivePuppet().root);
    part.name = "texture-maintenance-part";
    incActivePuppet().populateTextureSlots();
    require(incActivePuppet().textureSlots.length == 1, "generated part should populate one texture slot");

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();

    auto premult = (new PremultTextureCommand()).run(ctx);
    require(premult.succeeded, "PremultTextureCommand should succeed");
    auto premultiplied = texture.getTextureData();
    require(premultiplied[0] == cast(ubyte)(200 * 128 / 255), "premultiply should update red channel");
    require(premultiplied[1] == cast(ubyte)(100 * 128 / 255), "premultiply should update green channel");
    require(premultiplied[2] == cast(ubyte)(50 * 128 / 255), "premultiply should update blue channel");
    require(premultiplied[3] == 128, "premultiply should preserve alpha channel");

    auto mipmaps = (new RegenerateMipmapsCommand()).run(ctx);
    require(mipmaps.succeeded, "RegenerateMipmapsCommand should succeed");

    auto rebleed = (new RebleedTextureCommand()).run(ctx);
    require(rebleed.succeeded, "RebleedTextureCommand should enqueue work and succeed");
    drainRegressionTasks();

    texture.unlock();
}

private bool rectsOverlap(vec4i a, vec4i b) {
    return a.x < b.x + b.z &&
        a.x + a.z > b.x &&
        a.y < b.y + b.w &&
        a.y + a.w > b.y;
}

private void testAtlasPackerRectLifecycle() {
    auto packer = new TexturePacker(vec2i(32, 32));
    auto a = packer.packTexture(vec2i(10, 10));
    auto b = packer.packTexture(vec2i(8, 12));
    auto c = packer.packTexture(vec2i(6, 6));

    require(a.z == 10 && a.w == 10, "atlas packer should preserve first rectangle size");
    require(b.z == 8 && b.w == 12, "atlas packer should preserve second rectangle size");
    require(c.z == 6 && c.w == 6, "atlas packer should preserve third rectangle size");
    require(!rectsOverlap(a, b), "atlas packer should not overlap first and second rectangles");
    require(!rectsOverlap(a, c), "atlas packer should not overlap first and third rectangles");
    require(!rectsOverlap(b, c), "atlas packer should not overlap second and third rectangles");

    packer.remove(b);
    auto reused = packer.packTexture(vec2i(8, 12));
    require(reused.z == 8 && reused.w == 12, "atlas packer should reuse a removed rectangle-sized area");

    packer.clear();
    auto afterClear = packer.packTexture(vec2i(32, 32));
    require(afterClear == vec4i(0, 0, 32, 32), "atlas packer clear should restore the full free area");
}

private void testRenderAtlasPackerTextureSlots() {
    resetCase();

    auto packer = new TexturePacker(vec2i(64, 64));
    auto a = packer.packTexture(vec2i(16, 12));
    auto b = packer.packTexture(vec2i(24, 18));
    auto c = packer.packTexture(vec2i(8, 8));
    require(a.z == 16 && a.w == 12, "render atlas packer should preserve first allocation size");
    require(b.z == 24 && b.w == 18, "render atlas packer should preserve second allocation size");
    require(c.z == 8 && c.w == 8, "render atlas packer should preserve third allocation size");
    require(!rectsOverlap(a, b) && !rectsOverlap(a, c) && !rectsOverlap(b, c), "render atlas packer should not overlap live allocations");
    packer.remove(b);
    auto reused = packer.packTexture(vec2i(24, 18));
    require(reused.z == 24 && reused.w == 18, "render atlas packer should accept a same-sized allocation after removal");
    packer.clear();
    require(packer.packTexture(vec2i(64, 64)) == vec4i(0, 0, 64, 64), "render atlas packer clear should invalidate all previous allocations");

    auto sharedTexture = new Texture(cast(ubyte[])[255, 0, 0, 255], 1, 1, 4, 4, false, false);
    auto replacementTexture = new Texture(cast(ubyte[])[0, 255, 0, 255], 1, 1, 4, 4, false, false);
    auto extraTexture = new Texture(cast(ubyte[])[0, 0, 255, 255], 1, 1, 4, 4, false, false);
    scope(exit) {
        sharedTexture.dispose();
        replacementTexture.dispose();
        extraTexture.dispose();
    }

    auto partA = newMeshPart("atlas-slot-a");
    auto partB = newMeshPart("atlas-slot-b");
    auto originalA = partA.textures[TextureUsage.Albedo];
    auto originalB = partB.textures[TextureUsage.Albedo];
    scope(exit) {
        partA.textures[] = null;
        partB.textures[] = null;
        if (originalA) originalA.dispose();
        if (originalB) originalB.dispose();
    }

    originalA.dispose();
    originalB.dispose();
    originalA = null;
    originalB = null;
    partA.textures[TextureUsage.Albedo] = sharedTexture;
    partB.textures[TextureUsage.Albedo] = sharedTexture;
    incActivePuppet().populateTextureSlots();
    require(incActivePuppet().textureSlots.length == 1, "texture slot population should deduplicate shared texture instances");
    require(incActivePuppet().getTextureSlotIndexFor(sharedTexture) == 0, "shared texture should be discoverable in slot zero");

    partB.textures[TextureUsage.Albedo] = replacementTexture;
    incActivePuppet().populateTextureSlots();
    require(incActivePuppet().textureSlots.length == 2, "texture slot population should include replacement texture");
    require(incActivePuppet().getTextureSlotIndexFor(sharedTexture) >= 0, "original shared texture should remain while referenced");
    require(incActivePuppet().getTextureSlotIndexFor(replacementTexture) >= 0, "replacement texture should receive a slot");

    partA.textures[TextureUsage.Emissive] = extraTexture;
    incActivePuppet().populateTextureSlots();
    require(incActivePuppet().textureSlots.length == 3, "texture slot population should include auxiliary usage textures");

    partA.textures[TextureUsage.Albedo] = null;
    incActivePuppet().populateTextureSlots();
    require(incActivePuppet().getTextureSlotIndexFor(sharedTexture) < 0, "texture slot rebuild should invalidate unreferenced textures");
    require(incActivePuppet().textureSlots.length == 2, "texture slot rebuild should keep only currently referenced textures");
}

private void testColorBleedPreservesAlphaAndExtendsColor() {
    ubyte[] pixels;
    pixels.length = 3 * 3 * 4;
    auto center = (1 + 1 * 3) * 4;
    pixels[center + 0] = 200;
    pixels[center + 1] = 50;
    pixels[center + 2] = 25;
    pixels[center + 3] = 255;

    auto shallow = ShallowTexture(pixels, 3, 3, 4);
    incColorBleedPixels(&shallow, 4);

    auto top = (1 + 0 * 3) * 4;
    require(shallow.data[center + 0] == 200 && shallow.data[center + 3] == 255, "color bleed should preserve opaque source pixel");
    require(shallow.data[top + 0] == 200, "color bleed should copy red channel into transparent neighbor");
    require(shallow.data[top + 1] == 50, "color bleed should copy green channel into transparent neighbor");
    require(shallow.data[top + 2] == 25, "color bleed should copy blue channel into transparent neighbor");
    require(shallow.data[top + 3] == 0, "color bleed should preserve transparent neighbor alpha");
}

private void testProjectRepairMaintenanceCommands() {
    resetCase();

    auto textureA = new Texture(cast(ubyte[])[255, 0, 0, 255], 1, 1, 4, 4, false, false);
    auto textureB = new Texture(cast(ubyte[])[0, 255, 0, 255], 1, 1, 4, 4, false, false);
    auto partA = incCreateExPart(textureA, incActivePuppet().root, "repair-part-a");
    auto partB = incCreateExPart(textureB, incActivePuppet().root, "repair-part-b");
    partA.layerPath = "";
    partB.layerPath = "";
    auto oldRootId = incActivePuppet().root.uuid;
    auto oldPartAId = partA.uuid;
    auto oldPartBId = partB.uuid;

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();

    auto fakeNames = (new GenerateFakeLayerNameCommand()).run(ctx);
    require(fakeNames.succeeded, "GenerateFakeLayerNameCommand should succeed");
    require(partA.layerPath == "/" ~ partA.name, "fake layer names should update first part path");
    require(partB.layerPath == "/" ~ partB.name, "fake layer names should update second part path");

    auto repair = (new AttemptRepairPuppetCommand()).run(ctx);
    require(repair.succeeded, "AttemptRepairPuppetCommand should succeed on generated puppet");

    auto regenIds = (new RegenerateNodeIDsCommand()).run(ctx);
    require(regenIds.succeeded, "RegenerateNodeIDsCommand should succeed");
    require(incActivePuppet().root.uuid != oldRootId, "RegenerateNodeIDsCommand should change root UUID");
    require(partA.uuid != oldPartAId, "RegenerateNodeIDsCommand should change first part UUID");
    require(partB.uuid != oldPartBId, "RegenerateNodeIDsCommand should change second part UUID");
    require(partA.uuid != partB.uuid, "RegenerateNodeIDsCommand should keep regenerated UUIDs unique");
}

private void testProjectNewSaveOpenCommandPaths() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-save-open");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto saveBase = buildPath(fixtureDir, "roundtrip");
    auto savePath = saveBase ~ ".inx";
    if (exists(savePath) && isFile(savePath))
        remove(savePath);

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();

    auto addResult = (new AddNodeCommand("Node")).run(ctx);
    require(addResult.succeeded, "AddNodeCommand should succeed before save");
    auto node = addResult.created[0];
    ctx.nodes = [node];
    require((new SetNodeNameCommand(["Roundtrip Node"])).run(ctx).succeeded, "SetNodeNameCommand should succeed before save");
    require(incActionIsModified(), "node edits should mark project modified before save");

    require((new SaveFileCommand(saveBase)).run(ctx).succeeded, "SaveFileCommand should succeed");
    require(exists(savePath) && isFile(savePath), "SaveFileCommand should write an .inx file");
    require(!incActionIsModified(), "manual save should mark current action index as saved");

    incNewProject();
    require(findDirectNode(incActivePuppet(), "Roundtrip Node") is null, "new project should clear saved node before open");

    auto openResult = (new OpenFileCommand(savePath)).run(ctx);
    require(openResult.succeeded, "OpenFileCommand should succeed");
    require(findDirectNode(incActivePuppet(), "Roundtrip Node") !is null, "OpenFileCommand should restore saved node");
    require(!incActionIsModified(), "open project should start with a clean action history");
    require(incActiveProject().path == savePath, "opened project path should be the saved .inx path");
}

private void testNativeSavePathOverwriteAndReload() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-native-save");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto saveBase = buildPath(fixtureDir, "native-save");
    auto savePath = saveBase ~ ".inx";
    auto swapPath = savePath ~ ".swp";

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    auto first = (new AddNodeCommand("Node")).run(ctx).created[0];
    ctx.nodes = [first];
    require((new SetNodeNameCommand(["first-saved-node"])).run(ctx).succeeded, "first saved node rename should succeed");
    require((new SaveFileCommand(saveBase)).run(ctx).succeeded, "initial native save should succeed");
    require(incProjectPath() == saveBase, "native save should keep project path without implicit extension");
    require(exists(savePath) && isFile(savePath), "native save should write .inx file");
    require(!exists(swapPath), "native save should rename away .swp file");
    require(!incActionIsModified(), "native save should mark action index clean");

    ctx.nodes = null;
    ctx.hasNodes = false;
    auto second = (new AddNodeCommand("Node")).run(ctx).created[0];
    ctx.nodes = [second];
    require((new SetNodeNameCommand(["second-saved-node"])).run(ctx).succeeded, "second saved node rename should succeed");
    require(incActionIsModified(), "post-save edit should mark project dirty before overwrite");
    require((new SaveFileCommand(saveBase)).run(ctx).succeeded, "overwrite native save should succeed");
    require(!exists(swapPath), "overwrite save should leave no .swp file");
    require(!incActionIsModified(), "overwrite save should mark action index clean");

    incNewProject();
    require(findDirectNode(incActivePuppet(), "first-saved-node") is null, "new project should clear first saved node");
    require((new OpenFileCommand(savePath)).run(ctx).succeeded, "reload overwritten native save should succeed");
    require(findDirectNode(incActivePuppet(), "first-saved-node") !is null, "reload should preserve first saved node");
    require(findDirectNode(incActivePuppet(), "second-saved-node") !is null, "reload should include overwritten second node");
}

private void testProjectINXSerializationRoundTrip() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-inx-roundtrip");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto saveBase = buildPath(fixtureDir, "native-roundtrip");
    auto savePath = saveBase ~ ".inx";

    auto sourceImage = buildPath(fixtureDir, "inx-roundtrip-part.png");
    writeRegressionPng(sourceImage, 64, 128, 192, 4, 4);

    auto ctx = new Context();
    require((new ImportImageFolderCommand(fixtureDir)).run(ctx).succeeded, "ImportImageFolderCommand should create texture-backed INX fixture");
    auto sourcePart = findDirectPart(incActivePuppet(), "inx-roundtrip-part");
    require(sourcePart !is null, "image import should create source Part for INX fixture");

    sourcePart.localTransform.translation.vector[0] = 12.5f;
    sourcePart.localTransform.translation.vector[1] = -4.0f;
    sourcePart.tint = vec3(0.25f, 0.5f, 0.75f);
    sourcePart.opacity = 0.65f;

    auto sourceParam = new ExParameter("INXParam", false);
    sourceParam.min = vec2(0, 0);
    sourceParam.max = vec2(1, 0);
    incActivePuppet().parameters ~= sourceParam;
    auto sourceBinding = newValueBinding(sourceParam, sourcePart, "transform.t.x");
    sourceBinding.setValue(vec2u(1, 0), 18.0f);
    incActivePuppet().root.build();
    incActivePuppet().populateTextureSlots();

    require((new SaveFileCommand(saveBase)).run(ctx).succeeded, "SaveFileCommand should save generated INX fixture");
    require(exists(savePath) && isFile(savePath), "generated INX fixture should exist");

    incNewProject();
    require(findDirectPart(incActivePuppet(), "inx-roundtrip-part") is null, "new project should clear generated INX fixture");

    require((new OpenFileCommand(savePath)).run(ctx).succeeded, "OpenFileCommand should load generated INX fixture");
    auto importedPart = findDirectPart(incActivePuppet(), "inx-roundtrip-part");
    require(importedPart !is null, "INX round-trip should restore Part node");
    require(importedPart.textures[0] !is null, "INX round-trip should restore Part texture slot");
    require(importedPart.textures[0].width == 4 && importedPart.textures[0].height == 4, "INX round-trip should restore texture dimensions");
    require(importedPart.vertices.length > 0, "INX round-trip should restore mesh vertices");
    require(importedPart.getMesh().indices.length > 0, "INX round-trip should restore mesh indices");
    require(near(importedPart.localTransform.translation.vector[0], 12.5f), "INX round-trip should restore transform X");
    require(near(importedPart.localTransform.translation.vector[1], -4.0f), "INX round-trip should restore transform Y");
    require(nearVec3(importedPart.tint, vec3(0.25f, 0.5f, 0.75f)), "INX round-trip should restore tint");
    require(near(importedPart.opacity, 0.65f), "INX round-trip should restore opacity");

    auto importedParam = findParameter(incActivePuppet(), "INXParam");
    require(importedParam !is null, "INX round-trip should restore parameter");
    require(importedParam.bindings.length == 1, "INX round-trip should restore value binding");
    require(importedParam.bindings[0].getTarget.target is importedPart, "INX round-trip should reconnect binding target");
    auto importedBinding = cast(ValueParameterBinding)importedParam.bindings[0];
    require(importedBinding !is null, "INX round-trip should preserve ValueParameterBinding type");
    require(near(importedBinding.getValue(vec2u(1, 0)), 18.0f), "INX round-trip should restore binding key value");
}

private void testProjectAutosaveRecoveryRecords() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-autosave");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    incSettingsSet("prev_autosaves", cast(string[])[]);
    incSettingsSet("prev_autosave_mainpaths", cast(string[])[]);

    auto saveBase = buildPath(fixtureDir, "autosave-roundtrip");
    auto savePath = saveBase ~ ".inx";
    auto autosaveDir = getAutosaveDir(saveBase);
    if (exists(autosaveDir))
        rmdirRecurse(autosaveDir);
    scope(exit) {
        if (exists(autosaveDir))
            rmdirRecurse(autosaveDir);
    }

    auto node = new Node(incActivePuppet().root);
    node.name = "Autosave Node";

    auto ctx = new Context();
    require((new SaveFileCommand(saveBase)).run(ctx).succeeded, "SaveFileCommand should succeed before autosave");
    require(exists(savePath) && isFile(savePath), "manual save should create the main .inx file");
    require(incProjectPath() == saveBase, "manual save should keep the active project base path");

    incSetAutosaveFileLimit(2);
    incAutosaveProject(saveBase);

    require(exists(autosaveDir), "autosave should create the autosave directory");
    auto backups = currentBackups(autosaveDir);
    require(backups.length == 1, "autosave should create one backup file");
    auto backupPath = backups[0].name;
    require(exists(backupPath) && isFile(backupPath), "autosave backup path should exist");
    require(incCheckLockfile(saveBase), "autosave should create a lockfile for recovery detection");

    auto records = incGetPrevAutosaves();
    require(records.length == 1, "autosave should register one recovery record");
    require(records[0].autosavePath == backupPath, "recovery record should point at the backup file");
    require(records[0].mainsavePath == saveBase, "recovery record should point at the main project base path");

    incReleaseLockfile();
    require(!incCheckLockfile(saveBase), "normal release should remove the autosave recovery lockfile");

    remove(backupPath);
    incPruneAutosaveList();
    require(incGetPrevAutosaves().length == 0, "prune should remove stale autosave recovery records");
}

private void testProjectRecentFilesSettings() {
    auto oldProjects = incGetPrevProjects();
    scope(exit) incSettingsSet("prev_projects", oldProjects);

    incSettingsSet("prev_projects", cast(string[])[]);

    foreach (i; 0 .. 12)
        incAddPrevProject("/tmp/nijigenerate-recent-" ~ i.to!string ~ ".inx");

    auto projects = incGetPrevProjects();
    require(projects.length == 10, "recent project list should be pruned to 10 entries");
    require(projects[0].endsWith("nijigenerate-recent-11.inx"), "newest recent project should be first");
    require(projects[$ - 1].endsWith("nijigenerate-recent-2.inx"), "oldest retained recent project should be the tenth newest");

    incAddPrevProject("/tmp/nijigenerate-recent-5.inx");
    projects = incGetPrevProjects();
    require(projects.length == 10, "adding a duplicate recent project should not grow the list");
    require(projects[0].endsWith("nijigenerate-recent-5.inx"), "duplicate recent project should be promoted to the front");
    size_t duplicateCount;
    foreach (project; projects) {
        if (project.endsWith("nijigenerate-recent-5.inx"))
            duplicateCount++;
    }
    require(duplicateCount == 1, "duplicate recent project should appear exactly once");
}

private void testProjectINPImportMergeRoundTripCommandPaths() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-inp-roundtrip");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto exportPath = buildPath(fixtureDir, "roundtrip.inp");
    auto sourcePart = newMeshPart("inp-roundtrip-part");
    sourcePart.localTransform.translation.vector[0] = 12.0f;
    sourcePart.tint = vec3(0.25f, 0.5f, 0.75f);
    auto sourceParam = new ExParameter("INPParam", false);
    sourceParam.min = vec2(0, 0);
    sourceParam.max = vec2(1, 0);
    incActivePuppet().parameters ~= sourceParam;
    auto sourceBinding = newValueBinding(sourceParam, sourcePart, "transform.t.x");
    sourceBinding.setValue(vec2u(1, 0), 18.0f);

    incActivePuppet().root.build();
    incActivePuppet().populateTextureSlots();
    inWriteINPPuppet(incActivePuppet(), exportPath);
    require(exists(exportPath.setExtension("inp")), "generated INP fixture should be written");

    auto ctx = new Context();
    auto importResult = (new ImportINPCommand(exportPath)).run(ctx);
    require(importResult.succeeded, "ImportINPCommand should import generated INP fixture");
    auto importedPart = findDirectPart(incActivePuppet(), "inp-roundtrip-part");
    require(importedPart !is null, "INP import should restore exported Part");
    auto importedParam = findParameter(incActivePuppet(), "INPParam");
    require(importedParam !is null, "INP import should restore exported parameter");
    require(importedParam.bindings.length == 1, "INP import should restore exported binding");
    require(importedParam.bindings[0].getTarget.target is importedPart, "INP import should reconnect binding target");

    resetCase();
    auto basePart = newMeshPart("merge-base-part");
    auto baseParam = new ExParameter("BaseParam", false);
    incActivePuppet().parameters ~= baseParam;

    auto mergeResult = (new MergeINPCommand(exportPath, true, true)).run(ctx);
    require(mergeResult.succeeded, "MergeINPCommand should merge generated INP fixture");
    require(findDirectPart(incActivePuppet(), "merge-base-part") is basePart, "INP merge should preserve existing Part");
    auto mergedPart = findDirectPart(incActivePuppet(), "inp-roundtrip-part");
    require(mergedPart !is null, "INP merge should add imported Part");
    require(findParameter(incActivePuppet(), "BaseParam") is baseParam, "INP merge should preserve existing parameter");
    auto mergedParam = findParameter(incActivePuppet(), "INPParam");
    require(mergedParam !is null, "INP merge should add imported parameter");
    require(mergedParam.bindings.length == 1, "INP merge should add imported binding");
    require(mergedParam.bindings[0].getTarget.target is mergedPart, "INP merge should reconnect imported binding target");
}

private void testProjectSessionImportCommandPath() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-session-import");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    enum sessionKey = "com.inochi2d.inochi-session.bindings";
    auto sessionPath = buildPath(fixtureDir, "session-source.inp");
    incActivePuppet().extData[sessionKey] = cast(ubyte[])`{"tracking":"ok","version":1}`;
    inWriteINPPuppet(incActivePuppet(), sessionPath);
    require(exists(sessionPath.setExtension("inp")), "session source INP should be written");

    resetCase();
    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    require((new ImportSessionDataCommand(sessionPath)).run(ctx).succeeded, "ImportSessionDataCommand should import session extData");
    require((sessionKey in incActivePuppet().extData) !is null, "session import should set extData key");
    require(cast(string)incActivePuppet().extData[sessionKey] == `{"tracking":"ok","version":1}`, "session import should copy extData payload");

    auto missingPath = buildPath(fixtureDir, "session-missing.inp");
    resetCase();
    inWriteINPPuppet(incActivePuppet(), missingPath);
    resetCase();
    ctx = new Context();
    ctx.puppet = incActivePuppet();
    require(!(new ImportSessionDataCommand(missingPath)).run(ctx).succeeded, "ImportSessionDataCommand should reject INP without session extData");
    require((sessionKey in incActivePuppet().extData) is null, "failed session import should not add extData");
}

private void testNodeCommandMoveUndoRedo() {
    resetCase();

    auto parentA = new Node(incActivePuppet().root);
    auto parentB = new Node(incActivePuppet().root);
    auto child = new Node(parentA);
    parentA.name = "parentA";
    parentB.name = "parentB";
    child.name = "child";

    auto ctx = new Context();
    ctx.nodes = [child];

    require((new MoveNodeCommand(parentB, 0)).run(ctx).succeeded, "MoveNodeCommand should succeed");
    require(child.parent is parentB, "MoveNodeCommand should reparent node");
    require(isChildOf(parentB, child), "MoveNodeCommand should insert node into new parent");

    incActionUndo();
    require(child.parent is parentA, "undo MoveNodeCommand should restore old parent");
    require(isChildOf(parentA, child), "undo MoveNodeCommand should restore child list");

    incActionRedo();
    require(child.parent is parentB, "redo MoveNodeCommand should restore new parent");
    require(isChildOf(parentB, child), "redo MoveNodeCommand should restore new child list");
}

private void testNodeCentralizeCommandUndoRedo() {
    resetCase();

    auto parent = new Node(incActivePuppet().root);
    parent.name = "centralize-parent";
    auto childA = new Node(parent);
    childA.name = "centralize-child-a";
    auto childB = new Node(parent);
    childB.name = "centralize-child-b";

    parent.localTransform.translation = vec3(0, 0, 0);
    childA.localTransform.translation = vec3(10, 0, 0);
    childB.localTransform.translation = vec3(30, 0, 0);
    parent.transformChanged();
    childA.transformChanged();
    childB.transformChanged();

    auto parentBefore = parent.localTransform;
    auto childABefore = childA.localTransform;
    auto childBBefore = childB.localTransform;

    auto ctx = new Context();
    ctx.nodes = [parent];
    require((new CentralizeNodeCommand()).run(ctx).succeeded, "CentralizeNodeCommand should succeed");
    require(!nearVec2(parent.localTransform.translation.xy, parentBefore.translation.xy), "centralize should move parent pivot");
    auto parentAfter = parent.localTransform;
    auto childAAfter = childA.localTransform;
    auto childBAfter = childB.localTransform;

    incActionUndo();
    require(nearVec3(parent.localTransform.translation, parentBefore.translation), "centralize undo should restore parent transform");
    require(nearVec3(childA.localTransform.translation, childABefore.translation), "centralize undo should restore child A transform");
    require(nearVec3(childB.localTransform.translation, childBBefore.translation), "centralize undo should restore child B transform");

    incActionRedo();
    require(nearVec3(parent.localTransform.translation, parentAfter.translation), "centralize redo should restore parent pivot");
    require(nearVec3(childA.localTransform.translation, childAAfter.translation), "centralize redo should restore child A transform");
    require(nearVec3(childB.localTransform.translation, childBAfter.translation), "centralize redo should restore child B transform");
}

private void testNodeInspectorTransformUndoRedo() {
    resetCase();

    auto node = new Node(incActivePuppet().root);
    node.name = "inspector-target";
    auto inspector = new NINode([node], ModelEditSubMode.Layout);
    auto ctx = new Context();
    ctx.nodes = [node];
    ctx.inspectors = [inspector];

    require((new ApplyInspectorPropCommand!(NINode, "translationX")(12.5f)).run(ctx).succeeded, "TranslationX inspector command should succeed");
    require(near(node.localTransform.translation.vector[0], 12.5f), "translation X inspector command should apply");
    incActionUndo();
    require(near(node.localTransform.translation.vector[0], 0.0f), "undo translation X inspector command should restore old value");
    incActionRedo();
    require(near(node.localTransform.translation.vector[0], 12.5f), "redo translation X inspector command should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NINode, "rotationZ")(0.5f)).run(ctx).succeeded, "RotationZ inspector command should succeed");
    require(near(node.localTransform.rotation.vector[2], 0.5f), "rotation Z inspector command should apply");
    incActionUndo();
    require(near(node.localTransform.rotation.vector[2], 0.0f), "undo rotation Z inspector command should restore old value");
    incActionRedo();
    require(near(node.localTransform.rotation.vector[2], 0.5f), "redo rotation Z inspector command should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NINode, "scaleX")(1.75f)).run(ctx).succeeded, "ScaleX inspector command should succeed");
    require(near(node.localTransform.scale.vector[0], 1.75f), "scale X inspector command should apply");
    incActionUndo();
    require(near(node.localTransform.scale.vector[0], 1.0f), "undo scale X inspector command should restore old value");
    incActionRedo();
    require(near(node.localTransform.scale.vector[0], 1.75f), "redo scale X inspector command should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NINode, "zSort")(3.0f)).run(ctx).succeeded, "ZSort inspector command should succeed");
    require(near(node.zSort, 3.0f), "z-sort inspector command should apply");
    incActionUndo();
    require(near(node.zSort, 0.0f), "undo z-sort inspector command should restore old value");
    incActionRedo();
    require(near(node.zSort, 3.0f), "redo z-sort inspector command should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NINode, "lockToRoot")(true)).run(ctx).succeeded, "LockToRoot inspector command should succeed");
    require(node.lockToRoot, "lock-to-root inspector command should apply");
    incActionUndo();
    require(!node.lockToRoot, "undo lock-to-root inspector command should restore old value");
    incActionRedo();
    require(node.lockToRoot, "redo lock-to-root inspector command should restore new value");
}

private void testPartInspectorPropertiesUndoRedo() {
    resetCase();

    auto part = newMeshPart("part-inspector-target");
    auto inspector = new NIPart([part], ModelEditSubMode.Layout);
    auto ctx = new Context();
    ctx.nodes = [part];
    ctx.inspectors = [inspector];

    require((new ApplyInspectorPropCommand!(NIPart, "tint")(vec3(0.25f, 0.5f, 0.75f))).run(ctx).succeeded, "Part tint inspector command should succeed");
    require(nearVec3(part.tint, vec3(0.25f, 0.5f, 0.75f)), "Part tint inspector command should apply");
    incActionUndo();
    require(nearVec3(part.tint, vec3(1, 1, 1)), "undo Part tint should restore default");
    incActionRedo();
    require(nearVec3(part.tint, vec3(0.25f, 0.5f, 0.75f)), "redo Part tint should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPart, "screenTint")(vec3(0.1f, 0.2f, 0.3f))).run(ctx).succeeded, "Part screen tint inspector command should succeed");
    require(nearVec3(part.screenTint, vec3(0.1f, 0.2f, 0.3f)), "Part screen tint inspector command should apply");
    incActionUndo();
    require(nearVec3(part.screenTint, vec3(0, 0, 0)), "undo Part screen tint should restore default");
    incActionRedo();
    require(nearVec3(part.screenTint, vec3(0.1f, 0.2f, 0.3f)), "redo Part screen tint should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPart, "emissionStrength")(2.5f)).run(ctx).succeeded, "Part emission strength inspector command should succeed");
    require(near(part.emissionStrength, 2.5f), "Part emission strength inspector command should apply");
    incActionUndo();
    require(near(part.emissionStrength, 1.0f), "undo Part emission strength should restore default");
    incActionRedo();
    require(near(part.emissionStrength, 2.5f), "redo Part emission strength should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPart, "opacity")(0.42f)).run(ctx).succeeded, "Part opacity inspector command should succeed");
    require(near(part.opacity, 0.42f), "Part opacity inspector command should apply");
    incActionUndo();
    require(near(part.opacity, 1.0f), "undo Part opacity should restore default");
    incActionRedo();
    require(near(part.opacity, 0.42f), "redo Part opacity should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPart, "blendingMode")(BlendMode.Multiply)).run(ctx).succeeded, "Part blending mode inspector command should succeed");
    require(part.blendingMode == BlendMode.Multiply, "Part blending mode inspector command should apply");
    incActionUndo();
    require(part.blendingMode == BlendMode.Normal, "undo Part blending mode should restore default");
    incActionRedo();
    require(part.blendingMode == BlendMode.Multiply, "redo Part blending mode should restore new value");
}

private void testPartTextureReloadFixture() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-texture-reload");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto albedoPath = buildPath(fixtureDir, "albedo.png");
    auto replacementPath = buildPath(fixtureDir, "replacement.png");
    auto emissivePath = buildPath(fixtureDir, "emissive.png");
    auto mismatchedPath = buildPath(fixtureDir, "mismatched.png");
    writeRegressionPng(albedoPath, 255, 0, 0, 2, 2);
    writeRegressionPng(replacementPath, 0, 255, 0, 4, 3);
    writeRegressionPng(emissivePath, 0, 0, 255, 4, 3);
    writeRegressionPng(mismatchedPath, 0, 0, 0, 1, 1);

    auto part = newMeshPart("texture-reload-part");
    incActivePuppet().populateTextureSlots();
    require(incActivePuppet().textureSlots.length == 1, "initial Part fixture should populate one texture slot");
    auto oldTexture = part.textures[TextureUsage.Albedo];
    scope(exit) {
        foreach (texture; part.textures) {
            if (texture)
                texture.dispose();
        }
        if (oldTexture)
            oldTexture.dispose();
    }

    auto replacement = ShallowTexture(replacementPath, 4);
    part.textures[TextureUsage.Albedo] = new Texture(replacement);
    incActivePuppet().rescanNodes();
    incActivePuppet().populateTextureSlots();
    incActivePuppet().updateTextureState();
    require(part.textures[TextureUsage.Albedo] !is oldTexture, "texture reload should replace the Part albedo texture instance");
    require(part.textures[TextureUsage.Albedo].width == 4 && part.textures[TextureUsage.Albedo].height == 3,
        "texture reload should use dimensions from the replacement image");
    require(incActivePuppet().textureSlots.length == 1 && incActivePuppet().textureSlots[0] is part.textures[TextureUsage.Albedo],
        "texture reload should repopulate puppet texture slots with the replacement texture");

    auto emissive = ShallowTexture(emissivePath, 3);
    require(emissive.width == part.textures[TextureUsage.Albedo].width && emissive.height == part.textures[TextureUsage.Albedo].height,
        "matching auxiliary texture fixture should be accepted by size validation");
    part.textures[TextureUsage.Emissive] = new Texture(emissive);
    incActivePuppet().populateTextureSlots();
    require(incActivePuppet().textureSlots.length == 2, "adding a matching emissive texture should add another texture slot");

    auto mismatched = ShallowTexture(mismatchedPath, 3);
    require(mismatched.width != part.textures[TextureUsage.Albedo].width || mismatched.height != part.textures[TextureUsage.Albedo].height,
        "mismatched auxiliary texture fixture should be rejected by size validation");

    bool missingRejected;
    try {
        auto missing = ShallowTexture(buildPath(fixtureDir, "missing.png"), 4);
        auto unused = new Texture(missing);
    } catch (Exception) {
        missingRejected = true;
    }
    require(missingRejected, "loading a missing texture source should throw instead of replacing the texture slot");
}

private void testPuppetInspectorStateRoundTrip() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-puppet-inspector");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto part = newMeshPart("puppet-inspector-texture-part");
    incActivePuppet().meta.name = "Inspector Puppet";
    incActivePuppet().meta.artist = "Artist A, Artist B";
    incActivePuppet().meta.rigger = "Rigger A";
    incActivePuppet().meta.contact = "contact@example.invalid";
    incActivePuppet().meta.licenseURL = "https://example.invalid/license";
    incActivePuppet().meta.copyright = "Copyright Fixture";
    incActivePuppet().meta.reference = "https://example.invalid/source";
    incActivePuppet().meta.preservePixels = true;
    incActivePuppet().physics.pixelsPerMeter = 123.0f;
    incActivePuppet().physics.gravity = 8.5f;
    incActivePuppet().populateTextureSlots();
    incActivePuppet().updateTextureState();
    require(incActivePuppet().getRootParts().length == 1, "puppet inspector fixture should expose root part count");
    require(incActivePuppet().textureSlots.length == 1 && incActivePuppet().textureSlots[0] is part.textures[0],
        "puppet inspector fixture should populate texture atlas slots");

    auto saveBase = buildPath(fixtureDir, "puppet-inspector");
    auto savePath = saveBase ~ ".inx";
    auto ctx = new Context();
    require((new SaveFileCommand(saveBase)).run(ctx).succeeded, "puppet inspector fixture should save");

    incNewProject();
    require((new OpenFileCommand(savePath)).run(ctx).succeeded, "puppet inspector fixture should load");
    require(incActivePuppet().meta.name == "Inspector Puppet", "puppet metadata name should round-trip");
    require(incActivePuppet().meta.artist == "Artist A, Artist B", "puppet metadata artists should round-trip");
    require(incActivePuppet().meta.rigger == "Rigger A", "puppet metadata riggers should round-trip");
    require(incActivePuppet().meta.contact == "contact@example.invalid", "puppet metadata contact should round-trip");
    require(incActivePuppet().meta.licenseURL == "https://example.invalid/license", "puppet metadata license URL should round-trip");
    require(incActivePuppet().meta.copyright == "Copyright Fixture", "puppet metadata copyright should round-trip");
    require(incActivePuppet().meta.reference == "https://example.invalid/source", "puppet metadata origin/reference should round-trip");
    require(incActivePuppet().meta.preservePixels, "puppet preserve-pixels setting should round-trip");
    require(near(incActivePuppet().physics.pixelsPerMeter, 123.0f), "puppet physics pixels-per-meter should round-trip");
    require(near(incActivePuppet().physics.gravity, 8.5f), "puppet physics gravity should round-trip");
    require(incActivePuppet().textureSlots.length == 1, "loaded puppet should restore texture slots");
}

private void testNodeTypeInspectorCommandsUndoRedo() {
    resetCase();

    auto drawable = cast(Drawable)newMeshPart("drawable-inspector-target");
    auto drawableInspector = new NIDraw([drawable], ModelEditSubMode.Layout);
    auto drawableCtx = new Context();
    drawableCtx.nodes = [cast(Node)drawable];
    drawableCtx.inspectors = [drawableInspector];

    require((new ApplyInspectorPropCommand!(NIDraw, "offsetX")(4.5f)).run(drawableCtx).succeeded, "Drawable offsetX inspector command should succeed");
    require(near(drawable.getMesh().origin.vector[0], 4.5f), "Drawable offsetX inspector command should apply");
    incActionUndo();
    require(near(drawable.getMesh().origin.vector[0], 0.0f), "undo Drawable offsetX should restore old value");
    incActionRedo();
    require(near(drawable.getMesh().origin.vector[0], 4.5f), "redo Drawable offsetX should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIDraw, "offsetY")(-3.25f)).run(drawableCtx).succeeded, "Drawable offsetY inspector command should succeed");
    require(near(drawable.getMesh().origin.vector[1], -3.25f), "Drawable offsetY inspector command should apply");
    incActionUndo();
    require(near(drawable.getMesh().origin.vector[1], 0.0f), "undo Drawable offsetY should restore old value");
    incActionRedo();
    require(near(drawable.getMesh().origin.vector[1], -3.25f), "redo Drawable offsetY should restore new value");

    auto composite = new Composite(incActivePuppet().root);
    composite.name = "composite-inspector-target";
    auto compositeInspector = new NICmp([composite], ModelEditSubMode.Layout);
    auto compositeCtx = new Context();
    compositeCtx.nodes = [cast(Node)composite];
    compositeCtx.inspectors = [compositeInspector];

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NICmp, "tint")(vec3(0.2f, 0.4f, 0.6f))).run(compositeCtx).succeeded, "Composite tint inspector command should succeed");
    require(nearVec3(composite.tint, vec3(0.2f, 0.4f, 0.6f)), "Composite tint inspector command should apply");
    incActionUndo();
    require(nearVec3(composite.tint, vec3(1, 1, 1)), "undo Composite tint should restore default");
    incActionRedo();
    require(nearVec3(composite.tint, vec3(0.2f, 0.4f, 0.6f)), "redo Composite tint should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NICmp, "screenTint")(vec3(0.3f, 0.2f, 0.1f))).run(compositeCtx).succeeded, "Composite screenTint inspector command should succeed");
    require(nearVec3(composite.screenTint, vec3(0.3f, 0.2f, 0.1f)), "Composite screenTint inspector command should apply");
    incActionUndo();
    require(nearVec3(composite.screenTint, vec3(0, 0, 0)), "undo Composite screenTint should restore default");
    incActionRedo();
    require(nearVec3(composite.screenTint, vec3(0.3f, 0.2f, 0.1f)), "redo Composite screenTint should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NICmp, "opacity")(0.35f)).run(compositeCtx).succeeded, "Composite opacity inspector command should succeed");
    require(near(composite.opacity, 0.35f), "Composite opacity inspector command should apply");
    incActionUndo();
    require(near(composite.opacity, 1.0f), "undo Composite opacity should restore default");
    incActionRedo();
    require(near(composite.opacity, 0.35f), "redo Composite opacity should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NICmp, "threshold")(0.7f)).run(compositeCtx).succeeded, "Composite threshold inspector command should succeed");
    require(near(composite.threshold, 0.7f), "Composite threshold inspector command should apply");
    incActionUndo();
    require(near(composite.threshold, 0.5f), "undo Composite threshold should restore default");
    incActionRedo();
    require(near(composite.threshold, 0.7f), "redo Composite threshold should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NICmp, "blendingMode")(BlendMode.Screen)).run(compositeCtx).succeeded, "Composite blendingMode inspector command should succeed");
    require(composite.blendingMode == BlendMode.Screen, "Composite blendingMode inspector command should apply");
    incActionUndo();
    require(composite.blendingMode == BlendMode.Normal, "undo Composite blendingMode should restore default");
    incActionRedo();
    require(composite.blendingMode == BlendMode.Screen, "redo Composite blendingMode should restore new value");

    auto camera = new ExCamera(incActivePuppet().root);
    camera.name = "camera-inspector-target";
    auto cameraInspector = new NICam([camera], ModelEditSubMode.Layout);
    auto cameraCtx = new Context();
    cameraCtx.nodes = [camera];
    cameraCtx.inspectors = [cameraInspector];

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NICam, "viewportOrigin")(vec2(1280, 720))).run(cameraCtx).succeeded, "Camera viewport inspector command should succeed");
    require(nearVec2(camera.getViewport(), vec2(1280, 720)), "Camera viewport inspector command should apply");
    incActionUndo();
    require(nearVec2(camera.getViewport(), vec2(1920, 1080)), "undo Camera viewport should restore default");
    incActionRedo();
    require(nearVec2(camera.getViewport(), vec2(1280, 720)), "redo Camera viewport should restore new value");
}

private void testPartClippingMaskPropertiesUndoRedo() {
    resetCase();

    auto part = newMeshPart("part-clipping-target");
    auto inspector = new NIPart([part], ModelEditSubMode.Layout);
    auto ctx = new Context();
    ctx.nodes = [part];
    ctx.inspectors = [inspector];

    require((new ApplyInspectorPropCommand!(NIPart, "maskAlphaThreshold")(0.81f)).run(ctx).succeeded, "Part mask threshold inspector command should succeed");
    require(near(part.maskAlphaThreshold, 0.81f), "Part mask threshold inspector command should apply");
    incActionUndo();
    require(near(part.maskAlphaThreshold, 0.5f), "undo Part mask threshold should restore default");
    incActionRedo();
    require(near(part.maskAlphaThreshold, 0.81f), "redo Part mask threshold should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPart, "blendingMode")(BlendMode.ClipToLower)).run(ctx).succeeded, "ClipToLower blend mode command should succeed");
    require(part.blendingMode == BlendMode.ClipToLower, "ClipToLower blend mode command should apply");
    incActionUndo();
    require(part.blendingMode == BlendMode.Normal, "undo ClipToLower blend mode should restore default");
    incActionRedo();
    require(part.blendingMode == BlendMode.ClipToLower, "redo ClipToLower blend mode should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPart, "blendingMode")(BlendMode.SliceFromLower)).run(ctx).succeeded, "SliceFromLower blend mode command should succeed");
    require(part.blendingMode == BlendMode.SliceFromLower, "SliceFromLower blend mode command should apply");
    incActionUndo();
    require(part.blendingMode == BlendMode.ClipToLower, "undo SliceFromLower blend mode should restore previous value");
    incActionRedo();
    require(part.blendingMode == BlendMode.SliceFromLower, "redo SliceFromLower blend mode should restore new value");
}

private void testMeshDeformerInspectorCommandsUndoRedo() {
    resetCase();

    auto meshGroup = new MeshGroup(incActivePuppet().root);
    meshGroup.name = "meshgroup-inspector-target";
    auto meshInspector = new NIMesh([meshGroup], ModelEditSubMode.Layout);
    auto meshCtx = new Context();
    meshCtx.nodes = [meshGroup];
    meshCtx.inspectors = [meshInspector];

    require((new ApplyInspectorPropCommand!(NIMesh, "dynamic")(true)).run(meshCtx).succeeded, "MeshGroup dynamic inspector command should succeed");
    require(meshGroup.dynamic, "MeshGroup dynamic inspector command should apply");
    incActionUndo();
    require(!meshGroup.dynamic, "undo MeshGroup dynamic should restore default");
    incActionRedo();
    require(meshGroup.dynamic, "redo MeshGroup dynamic should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIMesh, "translateChildren")(false)).run(meshCtx).succeeded, "MeshGroup translateChildren inspector command should succeed");
    require(!meshGroup.getTranslateChildren(), "MeshGroup translateChildren inspector command should apply");
    incActionUndo();
    require(meshGroup.getTranslateChildren(), "undo MeshGroup translateChildren should restore default");
    incActionRedo();
    require(!meshGroup.getTranslateChildren(), "redo MeshGroup translateChildren should restore new value");

    auto grid = new GridDeformer(incActivePuppet().root);
    grid.name = "grid-inspector-target";
    auto gridInspector = new NIGrid([grid], ModelEditSubMode.Layout);
    auto gridCtx = new Context();
    gridCtx.nodes = [grid];
    gridCtx.inspectors = [gridInspector];

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIGrid, "dynamic")(true)).run(gridCtx).succeeded, "GridDeformer dynamic inspector command should succeed");
    require(grid.dynamic, "GridDeformer dynamic inspector command should apply");
    incActionUndo();
    require(!grid.dynamic, "undo GridDeformer dynamic should restore default");
    incActionRedo();
    require(grid.dynamic, "redo GridDeformer dynamic should restore new value");

    auto path = new PathDeformer(incActivePuppet().root);
    path.name = "path-inspector-target";
    path.driver = new ConnectedPendulumDriver(path);
    auto pathInspector = new NIPath([path], ModelEditSubMode.Layout);
    auto pathCtx = new Context();
    pathCtx.nodes = [path];
    pathCtx.inspectors = [pathInspector];

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPath, "dynamic")(true)).run(pathCtx).succeeded, "PathDeformer dynamic inspector command should succeed");
    require(path.dynamic, "PathDeformer dynamic inspector command should apply");
    incActionUndo();
    require(!path.dynamic, "undo PathDeformer dynamic should restore default");
    incActionRedo();
    require(path.dynamic, "redo PathDeformer dynamic should restore new value");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPath, "physicsEnabled")(false)).run(pathCtx).succeeded, "PathDeformer physicsEnabled inspector command should succeed");
    require(!path.physicsEnabled, "PathDeformer physicsEnabled inspector command should apply");
    incActionUndo();
    require(path.physicsEnabled, "undo PathDeformer physicsEnabled should restore old driver");
    incActionRedo();
    require(!path.physicsEnabled, "redo PathDeformer physicsEnabled should disable driver again");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPath, "physicsEnabled")(true)).run(pathCtx).succeeded, "PathDeformer physicsEnabled re-enable command should succeed");
    require(path.physicsEnabled, "PathDeformer physicsEnabled re-enable should create driver");

    incActionClearHistory();
    require((new ApplyInspectorPropCommand!(NIPath, "curveType")(CurveType.Bezier)).run(pathCtx).succeeded, "PathDeformer curveType inspector command should succeed");
    require(path.curveType == CurveType.Bezier, "PathDeformer curveType inspector command should apply");
    incActionUndo();
    require(path.curveType == CurveType.Spline, "undo PathDeformer curveType should restore default");
    incActionRedo();
    require(path.curveType == CurveType.Bezier, "redo PathDeformer curveType should restore new value");

    incActionClearHistory();
    auto oldGravity = (cast(ConnectedPendulumDriver)path.driver).gravity;
    require((new ApplyInspectorPropCommand!(NIPath, "gravity")(2.25f)).run(pathCtx).succeeded, "PathDeformer gravity inspector command should succeed");
    require(near((cast(ConnectedPendulumDriver)path.driver).gravity, 2.25f), "PathDeformer gravity inspector command should apply");
    incActionUndo();
    require(near((cast(ConnectedPendulumDriver)path.driver).gravity, oldGravity), "undo PathDeformer gravity should restore old value");
    incActionRedo();
    require(near((cast(ConnectedPendulumDriver)path.driver).gravity, 2.25f), "redo PathDeformer gravity should restore new value");

    incActionClearHistory();
    auto oldRestoreConstant = (cast(ConnectedPendulumDriver)path.driver).restoreConstant;
    require((new ApplyInspectorPropCommand!(NIPath, "restoreConstant")(3.5f)).run(pathCtx).succeeded, "PathDeformer restoreConstant inspector command should succeed");
    require(near((cast(ConnectedPendulumDriver)path.driver).restoreConstant, 3.5f), "PathDeformer restoreConstant inspector command should apply");
    incActionUndo();
    require(near((cast(ConnectedPendulumDriver)path.driver).restoreConstant, oldRestoreConstant), "undo PathDeformer restoreConstant should restore old value");
    incActionRedo();
    require(near((cast(ConnectedPendulumDriver)path.driver).restoreConstant, 3.5f), "redo PathDeformer restoreConstant should restore new value");

    incActionClearHistory();
    auto oldDamping = (cast(ConnectedPendulumDriver)path.driver).damping;
    require((new ApplyInspectorPropCommand!(NIPath, "damping")(0.4f)).run(pathCtx).succeeded, "PathDeformer damping inspector command should succeed");
    require(near((cast(ConnectedPendulumDriver)path.driver).damping, 0.4f), "PathDeformer damping inspector command should apply");
    incActionUndo();
    require(near((cast(ConnectedPendulumDriver)path.driver).damping, oldDamping), "undo PathDeformer damping should restore old value");
    incActionRedo();
    require(near((cast(ConnectedPendulumDriver)path.driver).damping, 0.4f), "redo PathDeformer damping should restore new value");
}

private void testNodeConvertCommandUndoRedo() {
    resetCase();

    auto node = new Node(incActivePuppet().root);
    node.name = "convert-source";
    auto ctx = new Context();
    ctx.nodes = [node];

    auto result = (new ConvertToCommand("GridDeformer")).run(ctx);
    require(result.succeeded, "ConvertToCommand should succeed");
    require(result.created.length == 1, "ConvertToCommand should return converted node");
    auto converted = result.created[0];
    require(cast(GridDeformer)converted !is null, "ConvertToCommand should create target node type");
    require(isChildOf(incActivePuppet().root, converted), "ConvertToCommand should insert converted node under original parent");
    require(!isChildOf(incActivePuppet().root, node), "ConvertToCommand should remove source node from parent");

    incActionUndo();
    require(isChildOf(incActivePuppet().root, node), "undo ConvertToCommand should restore source node");
    require(!isChildOf(incActivePuppet().root, converted), "undo ConvertToCommand should remove converted node");

    incActionRedo();
    require(isChildOf(incActivePuppet().root, converted), "redo ConvertToCommand should restore converted node");
    require(!isChildOf(incActivePuppet().root, node), "redo ConvertToCommand should remove source node again");
}

private void testNodeTypeConversionMapUndoRedo() {
    foreach (fromType, toTypes; conversionMap()) {
        foreach (toType; toTypes) {
            resetCase();
            auto node = inInstantiateNode(fromType, incActivePuppet().root);
            require(node !is null, "conversion source type should instantiate: " ~ fromType);
            node.name = fromType ~ "-to-" ~ toType;

            auto ctx = new Context();
            ctx.nodes = [node];
            auto command = new ConvertToCommand(toType);
            require(command.run(ctx).succeeded, "ConvertToCommand should convert %s to %s".format(fromType, toType));
            require(incActionHistory().length == 1, "node conversion should push one undoable action for %s -> %s".format(fromType, toType));

            auto converted = incActivePuppet().root.children[$ - 1];
            require(converted.typeId() == toType, "converted node should have destination type %s -> %s".format(fromType, toType));
            require(!isChildOf(incActivePuppet().root, node), "converted source should be detached for %s -> %s".format(fromType, toType));

            incActionUndo();
            require(isChildOf(incActivePuppet().root, node), "undo should restore source node for %s -> %s".format(fromType, toType));
            require(!isChildOf(incActivePuppet().root, converted), "undo should detach converted node for %s -> %s".format(fromType, toType));

            incActionRedo();
            require(isChildOf(incActivePuppet().root, converted), "redo should restore converted node for %s -> %s".format(fromType, toType));
            require(!isChildOf(incActivePuppet().root, node), "redo should detach source again for %s -> %s".format(fromType, toType));
        }
    }
}

private bool treeContainsType(T)(Node root) {
    if (cast(T)root !is null)
        return true;
    foreach (child; root.children) {
        if (treeContainsType!T(child))
            return true;
    }
    return false;
}

private void testProjectINPExportPrunesDepthRigNodes() {
    resetCase();

    auto normal = newMeshPart("normal");
    normal.name = "normal";
    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "depth-root";
    auto bone = new ExDepthBone(root);
    bone.name = "depth-bone";
    bone.boneId = "Bone";

    auto param = new ExParameter("ExportParam", false);
    incActivePuppet().parameters ~= param;
    auto normalBinding = newValueBinding(param, normal, "transform.t.x");
    auto depthBinding = newValueBinding(param, bone, "transform.t.x");
    require(param.bindings.length == 2, "export fixture should contain normal and depth-bone bindings");

    ngINPExportPruneEditorOnlyNodes(incActivePuppet());

    require(!treeContainsType!ExDepthRigRoot(incActivePuppet().root), "INP export pruning should remove DepthRigRoot");
    require(!treeContainsType!ExDepthBone(incActivePuppet().root), "INP export pruning should remove DepthBone");
    require(incActivePuppet().find!Node(normal.uuid) !is null, "INP export pruning should preserve normal nodes");
    require(param.bindings.length == 1, "INP export pruning should drop bindings targeting excluded depth nodes");
    auto exportedBindingTarget = cast(Node)param.bindings[0].getTarget.target;
    require(exportedBindingTarget !is null && exportedBindingTarget.uuid == normal.uuid, "INP export pruning should preserve bindings targeting exported nodes");
}

private void testDepthMappedNodeSerializationRoundTrip() {
    resetCase();
    ensureRegressionNodeTypesRegistered();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-depthmapped");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto grid = new ExGridDeformer(incActivePuppet().root);
    grid.name = "depth-grid";
    grid.rebuffer(Vec2Array([
        vec2(-1, -1),
        vec2(1, -1),
        vec2(-1, 1),
        vec2(1, 1),
    ]));
    grid.replaceDepths([0.0f, 0.25f, -0.5f, 1.0f]);

    ExDepthOp ring;
    ring.type = ExDepthOpType.Ring;
    ring.p0 = vec2(-1, 0);
    ring.p1 = vec2(1, 0);
    ring.amount = 0.75f;
    ring.width = 0.5f;
    ring.hardness = 0.25f;
    grid.replaceDepthOps([ring]);

    auto copied = new ExGridDeformer(incActivePuppet().root);
    copied.name = "copied-depth-grid";
    copied.copyDepthsFrom(grid);
    copied.copyDepthOpsFrom(grid);
    require(copied.copyDepths() == [0.0f, 0.25f, -0.5f, 1.0f], "DepthMapped copy should duplicate depths");
    require(copied.copyDepthOps().length == 1 && copied.copyDepthOps()[0].type == ExDepthOpType.Ring, "DepthOperation copy should duplicate operations");

    copied.rebuffer(Vec2Array([
        vec2(-1, -1),
        vec2(0, -1),
        vec2(1, -1),
        vec2(-1, 1),
        vec2(0, 1),
        vec2(1, 1),
    ]));
    require(copied.copyDepths().length == copied.vertices.length, "DepthMapped rebuffer should resize depth array to vertices");

    incActivePuppet().root.build();
    auto saveBase = buildPath(fixtureDir, "depthmapped-roundtrip");
    auto savePath = saveBase ~ ".inx";
    auto ctx = new Context();
    require((new SaveFileCommand(saveBase)).run(ctx).succeeded, "SaveFileCommand should save depth-mapped fixture");
    require(exists(savePath) && isFile(savePath), "depth-mapped INX fixture should exist");
    require((cast(ubyte[])read(savePath)).canFind(cast(ubyte[])"depth-grid"), "depth-mapped INX fixture should contain depth-grid node before load");

    ensureRegressionNodeTypesRegistered();
    auto loadedPuppet = inLoadPuppet!ExPuppet(savePath);
    auto loaded = cast(ExGridDeformer)findNodeRecursive(loadedPuppet.root, "depth-grid");
    require(loaded !is null, "depth-mapped INX round-trip should restore ExGridDeformer; tree:\n" ~ nodeTreeSummary(loadedPuppet.root));
    require(loaded.copyDepths() == [0.0f, 0.25f, -0.5f, 1.0f], "depth-mapped INX round-trip should restore depths");
    auto loadedOps = loaded.copyDepthOps();
    require(loadedOps.length == 1, "depth-mapped INX round-trip should restore depth operation count");
    require(loadedOps[0].type == ExDepthOpType.Ring, "depth-mapped INX round-trip should restore operation type");
    require(nearVec2(loadedOps[0].p0, vec2(-1, 0)) && nearVec2(loadedOps[0].p1, vec2(1, 0)), "depth-mapped INX round-trip should restore ring endpoints");
    require(near(loadedOps[0].amount, 0.75f) && near(loadedOps[0].width, 0.5f) && near(loadedOps[0].hardness, 0.25f), "depth-mapped INX round-trip should restore ring settings");
}

private void testDepthSignColorContracts() {
    require(nearVec4(depthOperationColor(0.25f), DepthOperationPositiveColor), "positive depth should use positive operation color");
    require(nearVec4(depthOperationColor(0.25f, true), DepthOperationPositiveSelectedColor), "selected positive depth should use selected positive color");
    require(nearVec4(depthOperationColor(-0.25f), DepthOperationNegativeColor), "negative depth should use negative operation color");
    require(nearVec4(depthOperationColor(-0.25f, true), DepthOperationNegativeSelectedColor), "selected negative depth should use selected negative color");
    require(nearVec4(depthOperationColor(0.0f), DepthOperationColor), "zero depth should use neutral operation color");
    require(nearVec4(depthOperationColor(0.0f, true), DepthOperationSelectedColor), "selected zero depth should use selected neutral color");
    require(near(depthToolRound(0.12345f), 0.123f), "depth values should round to 0.001 precision for UI editing");
    require(near(depthToolRound(-0.12355f), -0.124f), "negative depth values should round symmetrically to 0.001 precision");
}

private void testDepthCameraProjectionContracts() {
    DepthCamera3D camera;
    camera.yaw = 0.45f;
    camera.pitch = -0.32f;
    camera.zoom = 1.75f;
    camera.pan = vec2(23.0f, -17.0f);

    auto points = [
        vec2(-120.0f, -40.0f),
        vec2(0.0f, 0.0f),
        vec2(75.0f, 95.0f),
    ];
    auto depths = [-85.0f, 0.0f, 140.0f];
    foreach (point; points) {
        foreach (depth; depths) {
            auto projected = projectDepthPoint(point, depth, camera);
            auto unprojected = unprojectDepthPoint(projected, depth, camera);
            require(nearVec2(unprojected, point), "depth camera project/unproject should round-trip");
        }
    }

    auto originProjection = projectDepthPoint(vec2(0, 0), 0.0f, camera);
    require(nearVec2(originProjection, camera.pan), "zero-depth origin should project to camera pan");

    auto shallow = projectDepthPoint(vec2(30, -20), 0.0f, camera);
    auto deep = projectDepthPoint(vec2(30, -20), 100.0f, camera);
    require(!nearVec2(shallow, deep), "non-zero yaw/pitch should make depth visible in projected position");

    DepthCamera3D flatCamera;
    flatCamera.zoom = 2.0f;
    flatCamera.pan = vec2(-10.0f, 5.0f);
    auto flatPoint = vec2(12.5f, -7.25f);
    auto flatProjected = projectDepthPoint(flatPoint, 500.0f, flatCamera);
    require(nearVec2(flatProjected, flatPoint * flatCamera.zoom + flatCamera.pan), "flat camera projection should be ordinary pan/zoom regardless of depth");
    require(nearVec2(unprojectDepthPoint(flatProjected, 500.0f, flatCamera), flatPoint), "flat camera unprojection should invert pan/zoom");
}

private void testRenderCameraExportCommands() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-render-camera");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto missing = (new ExportPNGCommand(buildPath(fixtureDir, "missing-camera"), "missing")).run(new Context());
    require(!missing.succeeded, "camera export should reject a missing camera");

    auto camera = new ExCamera(incActivePuppet().root);
    camera.name = "small-camera";
    camera.setViewport(vec2(8, 6));
    camera.localTransform.translation = vec3(4, -3, 0);
    camera.localTransform.scale = vec2(2, 0.5f);
    incActivePuppet().root.build();
    require(camera.getViewport() == vec2(8, 6), "ExCamera should expose configured viewport dimensions");

    auto pngBase = buildPath(fixtureDir, "camera-export");
    auto png = (new ExportPNGCommand(pngBase, "small-camera", true, false)).run(new Context());
    require(png.succeeded, "ExportPNGCommand should succeed with a named camera");
    auto pngImage = ShallowTexture(pngBase ~ ".png", 4);
    require(pngImage.width == 8 && pngImage.height == 6, "ExportPNGCommand should use the camera viewport dimensions");

    auto tgaBase = buildPath(fixtureDir, "camera-export");
    auto tga = (new ExportTGACommand(tgaBase, "small-camera", true, false)).run(new Context());
    require(tga.succeeded, "ExportTGACommand should succeed with a named camera");
    auto tgaImage = ShallowTexture(tgaBase ~ ".tga", 4);
    require(tgaImage.width == 8 && tgaImage.height == 6, "ExportTGACommand should use the camera viewport dimensions");
}

private void testDepthOperationHelperContracts() {
    resetCase();
    ensureRegressionNodeTypesRegistered();

    auto grid = new ExGridDeformer(incActivePuppet().root);
    grid.name = "depth-helper-grid";
    grid.rebuffer(Vec2Array([
        vec2(0, 0),
        vec2(10, 0),
        vec2(0, 10),
    ]));
    grid.replaceDepths([0.25f, -0.5f, 1.0f]);
    auto copiedDepths = grid.copyDepths();
    copiedDepths[0] = 99;
    require(grid.copyDepths()[0] == 0.25f, "copyDepths should return a defensive copy");

    auto copied = new ExGridDeformer(incActivePuppet().root);
    copied.copyDepthsFrom(grid);
    require(copied.copyDepths() == [0.25f, -0.5f, 1.0f], "copyDepthsFrom should copy depths from depth-mapped source");
    copied.replaceDepths(null);
    require(copied.copyDepths() is null, "replaceDepths(null) should clear depth array");

    grid.rebuffer(Vec2Array([
        vec2(0, 0),
        vec2(10, 0),
        vec2(0, 10),
        vec2(10, 10),
    ]));
    require(grid.copyDepths().length == grid.vertices.length, "rebuffer should resize existing depth array");
    require(grid.copyDepths()[$ - 1] == 0.0f, "rebuffer should initialize added depth entries to zero");

    ExDepthOp attached;
    attached.type = ExDepthOpType.AttachedPoint;
    attached.index = 2;
    attached.amount = 0.5f;
    ExDepthOp plane;
    plane.type = ExDepthOpType.Plane;
    plane.center = vec2(1, 2);
    plane.radiusX = 3;
    plane.radiusY = 4;
    plane.angle = 15;
    plane.targetDepth = -0.25f;
    plane.flattenStrength = 0.75f;
    grid.replaceDepthOps([attached, plane]);
    auto copiedOps = grid.copyDepthOps();
    copiedOps[0].amount = 99;
    require(near(grid.copyDepthOps()[0].amount, 0.5f), "copyDepthOps should return a defensive copy");
    copied.copyDepthOpsFrom(grid);
    require(copied.copyDepthOps().length == 2, "copyDepthOpsFrom should copy operation count");
    require(copied.copyDepthOps()[1].type == ExDepthOpType.Plane, "copyDepthOpsFrom should preserve operation type");

    auto attachedOp = new DepthAttachedPointOperation(1, 0.4f);
    auto attachedClone = cast(DepthAttachedPointOperation)attachedOp.clone();
    require(attachedClone !is attachedOp && attachedClone.index == 1 && near(attachedClone.amount, 0.4f),
        "attached depth operation clone should preserve index and amount");

    DepthBrushSettings settings;
    settings.amount = -0.6f;
    settings.radiusY = 12.0f;
    settings.hardness = 0.35f;
    auto ringOp = new DepthRingOperation(vec2(0, 0), vec2(10, 0), settings);
    ringOp.p0Angle = 45;
    ringOp.p1Angle = 135;
    auto ringClone = cast(DepthRingOperation)ringOp.clone();
    require(ringClone !is ringOp && nearVec2(ringClone.p0, ringOp.p0) && nearVec2(ringClone.p1, ringOp.p1),
        "ring depth operation clone should preserve endpoints");
    require(near(ringClone.amount, -0.6f) && near(ringClone.width, 12.0f) && near(ringClone.hardness, 0.35f),
        "ring depth operation clone should preserve brush settings");
    require(near(ringClone.p0Angle, 45) && near(ringClone.p1Angle, 135),
        "ring depth operation clone should preserve endpoint angles");

    settings.amount = 0.75f;
    settings.flattenStrength = 0.4f;
    settings.angle = 25.0f;
    auto planeOp = new DepthPlaneOperation(vec2(5, 6), 7, 8, settings);
    auto planeClone = cast(DepthPlaneOperation)planeOp.clone();
    require(planeClone !is planeOp && nearVec2(planeClone.center, planeOp.center),
        "plane depth operation clone should preserve center");
    require(near(planeClone.radiusX, 7) && near(planeClone.radiusY, 8),
        "plane depth operation clone should preserve radii");

    require(near(distanceToSegment(vec2(5, 5), vec2(0, 0), vec2(10, 0)), 5.0f),
        "distanceToSegment should measure perpendicular distance within segment");
    require(near(distanceToSegment(vec2(15, 0), vec2(0, 0), vec2(10, 0)), 5.0f),
        "distanceToSegment should clamp beyond segment endpoint");
}

private JSONValue depthCommandOp(string type, JSONValue[string] fields) {
    fields["type"] = JSONValue(type);
    return JSONValue(fields);
}

private JSONValue jsonVec2Value(float x, float y) {
    JSONValue value = JSONValue.emptyArray;
    value.array ~= JSONValue(cast(double)x);
    value.array ~= JSONValue(cast(double)y);
    return value;
}

private void testDepthMapCommandsUndoRedo() {
    resetCase();
    ensureRegressionNodeTypesRegistered();
    auto grid = new ExGridDeformer(incActivePuppet().root);
    grid.name = "depth-command-grid";
    grid.rebuffer(Vec2Array([
        vec2(0, 0),
        vec2(10, 0),
        vec2(0, 10),
        vec2(10, 10),
    ]));

    auto ctx = new Context();
    require(cmd!(DepthMapCommand.SetDepths)(ctx, grid, [0.0f, 0.25f, -0.5f, 1.0f]).succeeded,
        "SetDepths command should succeed");
    require(grid.copyDepths() == [0.0f, 0.25f, -0.5f, 1.0f], "SetDepths should apply depths");
    incActionUndo();
    require(grid.copyDepths() is null, "undo SetDepths should restore null depths");
    incActionRedo();
    require(grid.copyDepths() == [0.0f, 0.25f, -0.5f, 1.0f], "redo SetDepths should restore depths");
    auto listedDepths = cast(ExCommandResult!JSONValue)cmd!(DepthMapCommand.ListDepths)(ctx, grid);
    require(listedDepths !is null && listedDepths.succeeded, "ListDepths command should return JSON");
    require(listedDepths.result["count"].integer == 4, "ListDepths should report depth count");

    JSONValue[string] attachedFields;
    attachedFields["index"] = JSONValue(1);
    attachedFields["amount"] = JSONValue(0.4);
    require(cmd!(DepthMapCommand.AddDepthOp)(ctx, grid, depthCommandOp("attached-point", attachedFields), -1).succeeded,
        "AddDepthOp should add an attached-point op");
    require(grid.copyDepthOps().length == 1 && grid.copyDepthOps()[0].type == ExDepthOpType.AttachedPoint,
        "AddDepthOp should store attached-point op");
    JSONValue[string] ringFields;
    ringFields["p0"] = jsonVec2Value(0, 0);
    ringFields["p1"] = jsonVec2Value(10, 0);
    ringFields["amount"] = JSONValue(0.75);
    ringFields["width"] = JSONValue(8.0);
    ringFields["hardness"] = JSONValue(0.5);
    ringFields["p0Angle"] = JSONValue(45.0);
    ringFields["p1Angle"] = JSONValue(135.0);
    require(cmd!(DepthMapCommand.AddDepthOp)(ctx, grid, depthCommandOp("ring", ringFields), -1).succeeded,
        "AddDepthOp should add a ring op");
    require(grid.copyDepthOps().length == 2 && grid.copyDepthOps()[1].type == ExDepthOpType.Ring,
        "AddDepthOp should append ring op");

    ringFields["amount"] = JSONValue(-0.25);
    require(cmd!(DepthMapCommand.UpdateDepthOp)(ctx, grid, 1, depthCommandOp("ring", ringFields)).succeeded,
        "UpdateDepthOp should replace one op");
    require(near(grid.copyDepthOps()[1].amount, -0.25f), "UpdateDepthOp should update ring amount");
    incActionUndo();
    require(near(grid.copyDepthOps()[1].amount, 0.75f), "undo UpdateDepthOp should restore previous op");
    incActionRedo();
    require(near(grid.copyDepthOps()[1].amount, -0.25f), "redo UpdateDepthOp should restore updated op");
    require(cmd!(DepthMapCommand.MoveDepthOp)(ctx, grid, 1, 0).succeeded, "MoveDepthOp should succeed");
    require(grid.copyDepthOps()[0].type == ExDepthOpType.Ring, "MoveDepthOp should reorder ops");
    require(cmd!(DepthMapCommand.RemoveDepthOp)(ctx, grid, 1).succeeded, "RemoveDepthOp should succeed");
    require(grid.copyDepthOps().length == 1, "RemoveDepthOp should remove one op");
    incActionUndo();
    require(grid.copyDepthOps().length == 2, "undo RemoveDepthOp should restore removed op");
    require(cmd!(DepthMapCommand.ApplyDepthOps)(ctx, grid).succeeded, "ApplyDepthOps should succeed");
    auto appliedDepths = grid.copyDepths();
    require(appliedDepths !is null && appliedDepths.length == grid.vertices.length, "ApplyDepthOps should bake depths to vertex count");
    auto listedOps = cast(ExCommandResult!JSONValue)cmd!(DepthMapCommand.ListDepthOps)(ctx, grid);
    require(listedOps !is null && listedOps.succeeded, "ListDepthOps command should return JSON");
    require(listedOps.result["count"].integer == 2, "ListDepthOps should report operation count");
    require(cmd!(DepthMapCommand.ClearDepthOps)(ctx, grid).succeeded, "ClearDepthOps should succeed");
    require(grid.copyDepthOps().length == 0, "ClearDepthOps should remove all ops");
    incActionUndo();
    require(grid.copyDepthOps().length == 2, "undo ClearDepthOps should restore ops");
    require(cmd!(DepthMapCommand.ClearDepths)(ctx, grid).succeeded, "ClearDepths should succeed");
    require(grid.copyDepths() is null, "ClearDepths should clear depth array");

    auto editor = new DepthMeshEditor(false);
    scope(exit) editor.dispose();
    editor.setTargets([cast(Node)grid]);
    auto editorOne = editor.getEditorFor(grid);
    require(editor.commitOperationAdd(editorOne, new DepthAttachedPointOperation(2, 0.3f)),
        "DepthMeshEditor should commit added depth op through command");
    require(editor.copyOperations(editorOne).length == 3, "editor should show command-added operation");
    require(grid.copyDepthOps().length == 2, "editor operation edits should stay local until Apply");
    require(cmd!(EditCommand.Undo)(ctx).succeeded, "Undo command should run for editor-added depth op");
    editor.update(null, Camera.init);
    require(editor.copyOperations(editorOne).length == 2, "editor should resync operation list after undo");
    require(cmd!(EditCommand.Redo)(ctx).succeeded, "Redo command should run for editor-added depth op");
    editor.update(null, Camera.init);
    require(editor.copyOperations(editorOne).length == 3, "editor should resync operation list after redo");
    editor.closeStack();
    editor.applyToTargets();
    require(grid.copyDepthOps().length == 3, "editor Apply should save local operation edits through depth-op command");
}

private void testSimplePhysicsParameterUndoRedo() {
    resetCase();

    auto physics = new SimplePhysics(incActivePuppet().root);
    physics.name = "physics";
    auto paramA = new Parameter("PhysicsA", false);
    auto paramB = new Parameter("PhysicsB", false);
    incActivePuppet().parameters ~= paramA;
    incActivePuppet().parameters ~= paramB;

    auto ctx = new Context();
    ctx.nodes = [cast(Node)physics];

    auto setA = new SetSimplePhysicsParameterCommand(paramA);
    require(setA.run(ctx).succeeded, "setting SimplePhysics parameter A should succeed");
    require(physics.param is paramA, "SimplePhysics parameter A should apply");

    auto setB = new SetSimplePhysicsParameterCommand(paramB);
    require(setB.run(ctx).succeeded, "setting SimplePhysics parameter B should succeed");
    require(physics.param is paramB, "SimplePhysics parameter B should apply");
    require(incActionHistory().length == 2, "SimplePhysics parameter changes must not merge");

    incActionUndo();
    require(physics.param is paramA, "undo should restore the previous SimplePhysics parameter");

    auto clear = new ClearSimplePhysicsParameterCommand();
    require(clear.run(ctx).succeeded, "clearing SimplePhysics parameter should succeed");
    require(physics.param is null, "SimplePhysics parameter clear should apply");

    incActionUndo();
    require(physics.param is paramA, "undo clear should restore the previous SimplePhysics parameter");
}

private void testSimplePhysicsSettingsUndoRedo() {
    resetCase();

    auto physics = new SimplePhysics(incActivePuppet().root);
    physics.name = "physics";

    auto ctx = new Context();
    ctx.nodes = [cast(Node)physics];

    require((new SetSimplePhysicsModelTypeCommand(PhysicsModel.SpringPendulum)).run(ctx).succeeded, "setting physics model type should succeed");
    require(physics.modelType == PhysicsModel.SpringPendulum, "physics model type should apply");
    incActionUndo();
    require(physics.modelType == PhysicsModel.Pendulum, "physics model type undo should restore old value");
    incActionRedo();
    require(physics.modelType == PhysicsModel.SpringPendulum, "physics model type redo should restore new value");

    require((new SetSimplePhysicsMapModeCommand(ParamMapMode.XY)).run(ctx).succeeded, "setting physics map mode should succeed");
    require(physics.mapMode == ParamMapMode.XY, "physics map mode should apply");
    incActionUndo();
    require(physics.mapMode == ParamMapMode.AngleLength, "physics map mode undo should restore old value");

    require((new SetSimplePhysicsLocalOnlyCommand(true)).run(ctx).succeeded, "setting localOnly should succeed");
    require(physics.localOnly, "physics localOnly should apply");
    incActionUndo();
    require(!physics.localOnly, "physics localOnly undo should restore old value");

    require((new SetSimplePhysicsGravityCommand(2.0f)).run(ctx).succeeded, "setting gravity should succeed");
    require(near(physics.gravity, 2.0f), "physics gravity should apply");
    incActionUndo();
    require(near(physics.gravity, 1.0f), "physics gravity undo should restore old value");

    require((new SetSimplePhysicsLengthCommand(250.0f)).run(ctx).succeeded, "setting length should succeed");
    require(near(physics.length, 250.0f), "physics length should apply");
    incActionUndo();
    require(near(physics.length, 100.0f), "physics length undo should restore old value");

    require((new SetSimplePhysicsFrequencyCommand(3.5f)).run(ctx).succeeded, "setting frequency should succeed");
    require(near(physics.frequency, 3.5f), "physics frequency should apply");
    incActionUndo();
    require(near(physics.frequency, 1.0f), "physics frequency undo should restore old value");

    require((new SetSimplePhysicsAngleDampingCommand(0.2f)).run(ctx).succeeded, "setting angle damping should succeed");
    require(near(physics.angleDamping, 0.2f), "physics angle damping should apply");
    incActionUndo();
    require(near(physics.angleDamping, 0.5f), "physics angle damping undo should restore old value");

    require((new SetSimplePhysicsLengthDampingCommand(0.3f)).run(ctx).succeeded, "setting length damping should succeed");
    require(near(physics.lengthDamping, 0.3f), "physics length damping should apply");
    incActionUndo();
    require(near(physics.lengthDamping, 0.5f), "physics length damping undo should restore old value");

    require((new SetSimplePhysicsOutputScaleXCommand(1.7f)).run(ctx).succeeded, "setting output scale X should succeed");
    require(near(physics.outputScale.x, 1.7f), "physics output scale X should apply");
    incActionUndo();
    require(near(physics.outputScale.x, 1.0f), "physics output scale X undo should restore old value");

    require((new SetSimplePhysicsOutputScaleYCommand(1.8f)).run(ctx).succeeded, "setting output scale Y should succeed");
    require(near(physics.outputScale.y, 1.8f), "physics output scale Y should apply");
    incActionUndo();
    require(near(physics.outputScale.y, 1.0f), "physics output scale Y undo should restore old value");
}

private void testSimplePhysicsSerializationRoundTrip() {
    resetCase();

    auto physics = new SimplePhysics(incActivePuppet().root);
    physics.name = "physics-serialized";
    auto param = new Parameter("PhysicsSerializedParam", false);
    incActivePuppet().parameters ~= param;
    physics.param = param;
    physics.modelType = PhysicsModel.SpringPendulum;
    physics.mapMode = ParamMapMode.XY;
    physics.localOnly = true;
    physics.gravity = 2.25f;
    physics.length = 123.0f;
    physics.frequency = 4.5f;
    physics.angleDamping = 0.35f;
    physics.lengthDamping = 0.45f;
    physics.outputScale = vec2(1.25f, 1.75f);
    incActivePuppet().root.build();

    auto fixtureDir = buildPath(tempDir(), "nijigenerate-regression");
    mkdirRecurse(fixtureDir);
    auto saveBase = buildPath(fixtureDir, "simplephysics-roundtrip");
    auto savePath = saveBase ~ ".inx";
    require((new SaveFileCommand(saveBase)).run(new Context()).succeeded, "SaveFileCommand should save SimplePhysics fixture");
    require(exists(savePath) && isFile(savePath), "SimplePhysics INX fixture should exist");

    ensureRegressionNodeTypesRegistered();
    auto loadedPuppet = inLoadPuppet!ExPuppet(savePath);
    auto loaded = cast(SimplePhysics)findNodeRecursive(loadedPuppet.root, "physics-serialized");
    require(loaded !is null, "SimplePhysics node should load from INX; tree:\n" ~ nodeTreeSummary(loadedPuppet.root));
    require(loaded.param !is null && loaded.param.name == "PhysicsSerializedParam", "SimplePhysics parameter reference should round-trip");
    require(loaded.modelType == PhysicsModel.SpringPendulum, "SimplePhysics model type should round-trip");
    require(loaded.mapMode == ParamMapMode.XY, "SimplePhysics map mode should round-trip");
    require(loaded.localOnly, "SimplePhysics localOnly should round-trip");
    require(near(loaded.gravity, 2.25f), "SimplePhysics gravity should round-trip");
    require(near(loaded.length, 123.0f), "SimplePhysics length should round-trip");
    require(near(loaded.frequency, 4.5f), "SimplePhysics frequency should round-trip");
    require(near(loaded.angleDamping, 0.35f), "SimplePhysics angle damping should round-trip");
    require(near(loaded.lengthDamping, 0.45f), "SimplePhysics length damping should round-trip");
    require(near(loaded.outputScale.x, 1.25f) && near(loaded.outputScale.y, 1.75f), "SimplePhysics output scale should round-trip");
}

private bool hasParameter(Parameter param) {
    foreach (candidate; incActivePuppet().parameters) {
        if (candidate is param)
            return true;
    }
    return false;
}

private void testParameterLifecycleUndoRedo() {
    resetCase();

    auto param = new Parameter("Param", true);
    param.min = vec2(-1, -1);
    param.max = vec2(1, 1);
    incActivePuppet().parameters ~= param;
    incActionPush(new ParameterAddAction(param));
    require(hasParameter(param), "parameter add should apply");

    incActionUndo();
    require(!hasParameter(param), "parameter add undo should remove parameter");

    incActionRedo();
    require(hasParameter(param), "parameter add redo should restore parameter");

    auto oldName = param.name;
    param.name = "Renamed";
    incActionPush(new ParameterValueChangeAction!(string)("name", param, oldName, param.name, &param.name_));
    require(param.name == "Renamed", "parameter rename should apply");

    incActionUndo();
    require(param.name == "Param", "parameter rename undo should restore old name");

    incActionRedo();
    require(param.name == "Renamed", "parameter rename redo should restore new name");

    incActionPush(new ParameterRemoveAction(param));
    incActivePuppet().removeParameter(param);
    require(!hasParameter(param), "parameter remove should apply");

    incActionUndo();
    require(hasParameter(param), "parameter remove undo should restore parameter");

    incActionRedo();
    require(!hasParameter(param), "parameter remove redo should remove parameter again");
}

private void testParameterCommandLifecycleUndoRedo() {
    resetCase();

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();

    auto add2d = new Add2DParameterCommand(-1, 1);
    auto addResult = add2d.run(ctx);
    require(addResult.succeeded, "Add2DParameterCommand should succeed");
    require(incActivePuppet().parameters.length == 1, "Add2DParameterCommand should add one parameter");
    auto param = incActivePuppet().parameters[0];
    require(param.isVec2, "Add2DParameterCommand should create a 2D parameter");
    require(param.axisPoints[0].length == 3 && param.axisPoints[1].length == 3, "Add2DParameterCommand should create centered axis points for symmetric range");

    incActionUndo();
    require(incActivePuppet().parameters.length == 0, "undo Add2DParameterCommand should remove the parameter");

    incActionRedo();
    require(incActivePuppet().parameters.length == 1, "redo Add2DParameterCommand should restore the parameter");
    param = incActivePuppet().parameters[0];

    ctx.parameters = [param];
    auto duplicateResult = (new DuplicateParameterCommand()).run(ctx);
    require(duplicateResult.succeeded, "DuplicateParameterCommand should succeed");
    require(incActivePuppet().parameters.length == 2, "DuplicateParameterCommand should add a second parameter");
    auto duplicated = incActivePuppet().parameters[1];
    require(duplicated !is param, "DuplicateParameterCommand should create a new parameter instance");
    require(duplicated.isVec2 == param.isVec2, "DuplicateParameterCommand should preserve dimensionality");

    incActionUndo();
    require(incActivePuppet().parameters.length == 1, "undo DuplicateParameterCommand should remove duplicated parameter");

    incActionRedo();
    require(incActivePuppet().parameters.length == 2, "redo DuplicateParameterCommand should restore duplicated parameter");
    duplicated = incActivePuppet().parameters[1];

    ctx.parameters = [duplicated];
    auto deleteResult = (new DeleteParameterCommand()).run(ctx);
    require(deleteResult.succeeded, "DeleteParameterCommand should succeed");
    require(incActivePuppet().parameters.length == 1, "DeleteParameterCommand should remove selected parameter");

    incActionUndo();
    require(incActivePuppet().parameters.length == 2, "undo DeleteParameterCommand should restore selected parameter");

    incActionRedo();
    require(incActivePuppet().parameters.length == 1, "redo DeleteParameterCommand should remove selected parameter again");
}

private void assertAxisPoints(Parameter param, int axis, scope const(float)[] expected, string label) {
    require(param.axisPoints[axis].length == expected.length, label ~ " axis point count");
    foreach (i, value; expected)
        require(near(param.axisPoints[axis][i], value), "%s axis point %s".format(label, i));
}

private void testParameterCreatePresets() {
    resetCase();

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();

    auto onePositiveResult = (new Add1DParameterCommand(0, 1)).run(ctx);
    require(onePositiveResult.succeeded, "Add1DParameterCommand 0..1 should succeed");
    auto onePositive = onePositiveResult.created[0];
    require(!onePositive.isVec2, "0..1 preset should create 1D parameter");
    require(near(onePositive.min.x, 0) && near(onePositive.max.x, 1), "0..1 preset should set range");
    assertAxisPoints(onePositive, 0, [0.0f, 1.0f], "0..1 1D");

    auto oneSymResult = (new Add1DParameterCommand(-1, 1)).run(ctx);
    require(oneSymResult.succeeded, "Add1DParameterCommand -1..1 should succeed");
    auto oneSym = oneSymResult.created[0];
    require(!oneSym.isVec2, "-1..1 preset should create 1D parameter");
    require(near(oneSym.min.x, -1) && near(oneSym.max.x, 1), "-1..1 preset should set range");
    assertAxisPoints(oneSym, 0, [0.0f, 0.5f, 1.0f], "-1..1 1D");

    auto twoPositiveResult = (new Add2DParameterCommand(0, 1)).run(ctx);
    require(twoPositiveResult.succeeded, "Add2DParameterCommand 0..1 should succeed");
    auto twoPositive = twoPositiveResult.created[0];
    require(twoPositive.isVec2, "0..1 preset should create 2D parameter");
    require(nearVec2(twoPositive.min, vec2(0, 0)) && nearVec2(twoPositive.max, vec2(1, 1)), "0..1 2D preset should set range");
    assertAxisPoints(twoPositive, 0, [0.0f, 1.0f], "0..1 2D X");
    assertAxisPoints(twoPositive, 1, [0.0f, 1.0f], "0..1 2D Y");

    auto twoSymResult = (new Add2DParameterCommand(-1, 1)).run(ctx);
    require(twoSymResult.succeeded, "Add2DParameterCommand -1..1 should succeed");
    auto twoSym = twoSymResult.created[0];
    require(twoSym.isVec2, "-1..1 preset should create 2D parameter");
    require(nearVec2(twoSym.min, vec2(-1, -1)) && nearVec2(twoSym.max, vec2(1, 1)), "-1..1 2D preset should set range");
    assertAxisPoints(twoSym, 0, [0.0f, 0.5f, 1.0f], "-1..1 2D X");
    assertAxisPoints(twoSym, 1, [0.0f, 0.5f, 1.0f], "-1..1 2D Y");

    auto mouthResult = (new AddMouthParameterCommand()).run(ctx);
    require(mouthResult.succeeded, "AddMouthParameterCommand should succeed");
    auto mouth = mouthResult.created[0];
    require(mouth.isVec2, "mouth preset should create 2D parameter");
    require(nearVec2(mouth.min, vec2(-1, 0)) && nearVec2(mouth.max, vec2(1, 1)), "mouth preset should set range");
    assertAxisPoints(mouth, 0, [0.0f, 0.25f, 0.5f, 0.6f, 1.0f], "mouth X");
    assertAxisPoints(mouth, 1, [0.0f, 0.3f, 0.5f, 0.6f, 1.0f], "mouth Y");

    require(incActivePuppet().parameters.length == 5, "all parameter presets should be added");
    incActionUndo();
    require(incActivePuppet().parameters.length == 4, "undo should remove latest preset");
    incActionRedo();
    require(incActivePuppet().parameters.length == 5, "redo should restore latest preset");
}

private void testParameterCopyPasteDuplicateAndLinkCommands() {
    resetCase();

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    require((new Add1DParameterCommand(-1, 1)).run(ctx).succeeded, "source parameter should be created");
    require((new Add1DParameterCommand(-1, 1)).run(ctx).succeeded, "destination parameter should be created");
    auto source = incActivePuppet().parameters[0];
    auto dest = incActivePuppet().parameters[1];

    auto node = new Node(incActivePuppet().root);
    node.name = "parameter-copy-node";
    auto sourceBinding = newValueBinding(source, node, "transform.t.x");
    sourceBinding.setValue(vec2u(0, 0), -2.0f);
    sourceBinding.setValue(vec2u(2, 0), 4.0f);
    sourceBinding.reInterpolate();

    ctx.parameters = [source];
    require((new CopyParameterCommand()).run(ctx).succeeded, "CopyParameterCommand should succeed");
    ctx.parameters = [dest];
    require((new PasteParameterCommand()).run(ctx).succeeded, "PasteParameterCommand should succeed");
    auto pasted = cast(ValueParameterBinding)dest.getBinding(node, "transform.t.x");
    require(pasted !is null, "PasteParameterCommand should create destination binding");
    require(pasted.isSet(vec2u(0, 0)) && near(pasted.getValue(vec2u(0, 0)), -2.0f), "paste should copy low key value");
    require(pasted.isSet(vec2u(2, 0)) && near(pasted.getValue(vec2u(2, 0)), 4.0f), "paste should copy high key value");

    incActionUndo();
    require(dest.getBinding(node, "transform.t.x") is null, "undo PasteParameterCommand should remove pasted binding");
    incActionRedo();
    require(dest.getBinding(node, "transform.t.x") !is null, "redo PasteParameterCommand should restore pasted binding");

    incActionClearHistory();
    ctx.parameters = [source];
    auto duplicateResult = (new DuplicateParameterCommand()).run(ctx);
    require(duplicateResult.succeeded, "DuplicateParameterCommand should succeed");
    auto duplicated = duplicateResult.created[0];
    require(duplicated.bindings.length == source.bindings.length, "DuplicateParameterCommand should copy bindings");
    incActionUndo();
    require(incActivePuppet().parameters.length == 2, "undo DuplicateParameterCommand should remove duplicate");
    incActionRedo();
    require(incActivePuppet().parameters.length == 3, "redo DuplicateParameterCommand should restore duplicate");

    incActionClearHistory();
    ctx.parameters = [source];
    auto duplicateFlipResult = (new DuplicateParameterWithFlipCommand()).run(ctx);
    require(duplicateFlipResult.succeeded, "DuplicateParameterWithFlipCommand should succeed");
    require(duplicateFlipResult.created[0].bindings.length == source.bindings.length, "DuplicateParameterWithFlipCommand should recreate bindings");
    incActionUndo();
    require(incActivePuppet().parameters.length == 3, "undo DuplicateParameterWithFlipCommand should remove flipped duplicate");
    incActionRedo();
    require(incActivePuppet().parameters.length == 4, "redo DuplicateParameterWithFlipCommand should restore flipped duplicate");

    incActionClearHistory();
    require((new CopyParameterCommand()).run(ctx).succeeded, "CopyParameterCommand should refill clipboard");
    ctx.parameters = [dest];
    require((new PasteParameterWithFlipCommand()).run(ctx).succeeded, "PasteParameterWithFlipCommand should succeed");
    require(dest.getBinding(node, "transform.t.x") !is null, "PasteParameterWithFlipCommand should keep/create destination binding");

    incActionClearHistory();
    ctx.parameters = [dest];
    require((new LinkToCommand(source, 0, 0)).run(ctx).succeeded, "LinkToCommand should succeed");
    auto linkBinding = cast(ParameterParameterBinding)dest.getBinding(source, "X");
    require(linkBinding !is null, "LinkToCommand should create ParameterParameterBinding");
    require(linkBinding.isSet(vec2u(0, 0)) && near(linkBinding.values[0][0], source.min.x), "link should map destination min to source min");
    require(linkBinding.isSet(vec2u(2, 0)) && near(linkBinding.values[2][0], source.max.x), "link should map destination max to source max");

    incActionUndo();
    require(dest.getBinding(source, "X") is null, "undo LinkToCommand should remove link binding");
    incActionRedo();
    require(dest.getBinding(source, "X") !is null, "redo LinkToCommand should restore link binding");
}

private bool hasGroup(ExParameterGroup group) {
    foreach (candidate; (cast(ExPuppet)incActivePuppet()).groups) {
        if (candidate is group)
            return true;
    }
    return false;
}

private void testParameterCommandRenameAndGroupUndoRedo() {
    resetCase();

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();

    auto addResult = (new Add1DParameterCommand(-1, 1)).run(ctx);
    require(addResult.succeeded, "Add1DParameterCommand should succeed");
    auto param = cast(ExParameter)incActivePuppet().parameters[0];
    require(param !is null, "parameter command should create ExParameter");

    ctx.parameters = [param];
    require((new SetParameterNameCommand("Renamed Param")).run(ctx).succeeded, "SetParameterNameCommand should succeed");
    require(param.name == "Renamed Param", "parameter rename command should apply");
    incActionUndo();
    require(param.name != "Renamed Param", "undo SetParameterNameCommand should restore old name");
    incActionRedo();
    require(param.name == "Renamed Param", "redo SetParameterNameCommand should restore new name");

    auto groupResult = (new CreateParamGroupCommand()).run(ctx);
    require(groupResult.succeeded, "CreateParamGroupCommand should succeed");
    auto group = groupResult.created[0];
    require(hasGroup(group), "CreateParamGroupCommand should add a parameter group");
    incActionUndo();
    require(!hasGroup(group), "undo CreateParamGroupCommand should remove the group");
    incActionRedo();
    require(hasGroup(group), "redo CreateParamGroupCommand should restore the group");

    ctx.parameters = [param];
    require((new MoveParameterCommand(group, 0)).run(ctx).succeeded, "MoveParameterCommand should succeed");
    require(param.getParent() is group, "MoveParameterCommand should attach parameter to group");
    incActionUndo();
    require(param.getParent() is null, "undo MoveParameterCommand should detach parameter from group");
    incActionRedo();
    require(param.getParent() is group, "redo MoveParameterCommand should attach parameter to group again");

    ctx.parameters = [group];
    require((new ChangeGroupColorCommand([0.25f, 0.5f, 0.75f])).run(ctx).succeeded, "ChangeGroupColorCommand should succeed");
    require(group.color == vec3(0.25f, 0.5f, 0.75f), "ChangeGroupColorCommand should apply");
    incActionUndo();
    require(group.color != vec3(0.25f, 0.5f, 0.75f), "undo ChangeGroupColorCommand should restore old color");
    incActionRedo();
    require(group.color == vec3(0.25f, 0.5f, 0.75f), "redo ChangeGroupColorCommand should restore new color");

    require((new DeleteParamGroupCommand()).run(ctx).succeeded, "DeleteParamGroupCommand should succeed");
    require(!hasGroup(group), "DeleteParamGroupCommand should remove group");
    require(param.getParent() is null, "DeleteParamGroupCommand should detach children");
    incActionUndo();
    require(hasGroup(group), "undo DeleteParamGroupCommand should restore group");
    require(param.getParent() is group, "undo DeleteParamGroupCommand should restore child membership");
    incActionRedo();
    require(!hasGroup(group), "redo DeleteParamGroupCommand should remove group again");
    require(param.getParent() is null, "redo DeleteParamGroupCommand should detach children again");
}

private ValueParameterBinding newValueBinding(Parameter param, Node node, string name, bool setZero = true) {
    auto binding = cast(ValueParameterBinding)param.createBinding(node, name, setZero);
    require(binding !is null, "ValueParameterBinding fixture should be created");
    param.addBinding(binding);
    return binding;
}

private bool containsBinding(ParameterBinding[] bindings, ParameterBinding target) {
    foreach (binding; bindings) {
        if (binding is target)
            return true;
    }
    return false;
}

private Context bindingContext(Parameter param, ParameterBinding binding, vec2u keyPoint) {
    auto ctx = new Context();
    ctx.parameters = [param];
    ctx.bindings = [binding];
    ctx.keyPoint = keyPoint;
    return ctx;
}

private ExParameter new2DParameter(string name) {
    auto param = new ExParameter(name, true);
    param.min = vec2(-1, -1);
    param.max = vec2(1, 1);
    param.insertAxisPoint(0, 0.5f);
    param.insertAxisPoint(1, 0.5f);
    incActivePuppet().parameters ~= param;
    return param;
}

private void testParameterSplitBindingMigrationUndoRedo() {
    resetCase();

    auto param = new2DParameter("Split Source");
    param.insertAxisPoint(0, 0.25f);
    param.insertAxisPoint(1, 0.75f);

    auto keepA = new Node(incActivePuppet().root);
    keepA.name = "split-keep-a";
    auto move = new Node(incActivePuppet().root);
    move.name = "split-move";
    auto keepB = new Node(incActivePuppet().root);
    keepB.name = "split-keep-b";

    auto keepBindingA = newValueBinding(param, keepA, "transform.t.x");
    auto movedBinding = newValueBinding(param, move, "transform.t.y");
    auto keepBindingB = newValueBinding(param, keepB, "transform.r.z");
    keepBindingA.setValue(vec2u(0, 0), -1.0f);
    movedBinding.setValue(vec2u(1, 1), 2.0f);
    keepBindingB.setValue(vec2u(2, 2), 3.0f);

    auto originalBindings = param.bindings.dup;
    auto newParam = ngSplitParameterBindings(0, param, [move.uuid]);
    require(newParam !is null, "parameter split should create a new parameter when a node is selected");
    require(incActivePuppet().parameters.length == 2, "parameter split should insert the new parameter");
    require(incActivePuppet().parameters[1] is newParam, "parameter split should insert immediately after the source parameter");
    require(newParam.name == "Split Source (Split)", "split parameter should use the dialog suffix");
    require(newParam.isVec2 == param.isVec2, "split parameter should preserve dimensionality");
    require(param.axisPoints[0] == newParam.axisPoints[0] && param.axisPoints[1] == newParam.axisPoints[1], "split parameter should copy all axis points");
    require(param.bindings.length == 2, "source parameter should retain unselected-node bindings");
    require(containsBinding(param.bindings, keepBindingA) && containsBinding(param.bindings, keepBindingB), "source parameter should keep both non-moved bindings");
    require(!containsBinding(param.bindings, movedBinding), "source parameter should not retain moved binding");
    require(newParam.bindings.length == 1 && newParam.bindings[0] is movedBinding, "new parameter should receive selected-node bindings");

    incActionUndo();
    require(incActivePuppet().parameters.length == 1 && incActivePuppet().parameters[0] is param, "undo split should remove the created parameter");
    require(param.bindings.length == originalBindings.length, "undo split should restore all source bindings");
    foreach (binding; originalBindings)
        require(containsBinding(param.bindings, binding), "undo split should restore binding identity");
    require(newParam.bindings.length == 0, "undo split should clear bindings from the removed split parameter");

    incActionRedo();
    require(incActivePuppet().parameters.length == 2 && incActivePuppet().parameters[1] is newParam, "redo split should restore the split parameter");
    require(param.bindings.length == 2 && !containsBinding(param.bindings, movedBinding), "redo split should move selected binding again");
    require(newParam.bindings.length == 1 && newParam.bindings[0] is movedBinding, "redo split should restore moved binding on split parameter");

    auto noOp = ngSplitParameterBindings(0, param, []);
    require(noOp is null, "parameter split should not create an empty split parameter");
    require(incActivePuppet().parameters.length == 2, "empty split should not change parameter list");
}

private void testParameterArmSelectAndKeypointCommands() {
    resetCase();

    auto first = new2DParameter("First");
    auto second = new2DParameter("Second");

    incSelectParam(first);
    require(incSelectedParam() is first, "incSelectParam should set the primary selected parameter");
    incAddSelectParam(second);
    require(incSelectedParams().length == 2, "incAddSelectParam should add to parameter selection");
    incRemoveSelectParam(first);
    require(incSelectedParams().length == 1 && incSelectedParam() is second, "incRemoveSelectParam should update parameter selection");

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    ctx.parameters = [second];

    require((new ToggleParameterArmCommand(1)).run(ctx).succeeded, "ToggleParameterArmCommand should arm the selected parameter");
    require(incArmedParameter() is second, "ToggleParameterArmCommand should set armed parameter");
    require(incArmedParameterIdx() == 1, "ToggleParameterArmCommand should preserve armed parameter index");
    require(!incActivePuppet().enableDrivers, "arming a parameter should disable drivers");

    require((new ToggleParameterArmCommand(1)).run(ctx).succeeded, "ToggleParameterArmCommand should disarm the armed parameter");
    require(incArmedParameter() is null, "ToggleParameterArmCommand should disarm when the same parameter is armed");
    require(incActivePuppet().enableDrivers, "disarming should re-enable drivers");

    ctx.parameterValue = vec2(-1, 1);
    require((new SetParameterKeypointCommand()).run(ctx).succeeded, "SetParameterKeypointCommand should set a keypoint value");
    require(second.value == vec2(-1, 1), "SetParameterKeypointCommand should update parameter value");
    require(incArmedParameter() is null, "SetParameterKeypointCommand should not arm the parameter");

    incSetEditMode(EditMode.VertexEdit, false);
    auto armedCtx = new Context();
    armedCtx.puppet = incActivePuppet();
    armedCtx.armedParameters = [second];
    armedCtx.parameterValue = vec2(0, 0);
    require((new SetArmedParameterAndKeypointCommand()).run(armedCtx).succeeded, "SetArmedParameterAndKeypointCommand should set and arm a keypoint");
    require(incEditMode() == EditMode.ModelEdit, "SetArmedParameterAndKeypointCommand should return to model edit mode");
    require(incArmedParameter() is second, "SetArmedParameterAndKeypointCommand should arm the target parameter");
    require(incArmedParameterIdx() == 1, "SetArmedParameterAndKeypointCommand should resolve the parameter index");
    require(second.value == vec2(0, 0), "SetArmedParameterAndKeypointCommand should update parameter value");
}

private void testParameterKeyframeCommandUndoRedo() {
    resetCase();

    auto node = new Node(incActivePuppet().root);
    auto param = new ExParameter("Param", true);
    param.min = vec2(-1, -1);
    param.max = vec2(1, 1);
    param.insertAxisPoint(0, 0.5f);
    param.insertAxisPoint(1, 0.5f);
    incActivePuppet().parameters ~= param;

    auto binding = newValueBinding(param, node, "transform.t.x");
    auto kp = vec2u(2, 1);
    auto ctx = bindingContext(param, binding, kp);

    node.setValue("transform.t.x", 4.0f);
    require((new ResetKeyFrameCommand()).run(ctx).succeeded, "ResetKeyFrameCommand should succeed");
    require(binding.isSet(kp), "reset keyframe should mark keypoint as set");
    require(near(binding.getValue(kp), 0.0f), "reset keyframe should use default value");
    incActionUndo();
    require(!binding.isSet(kp), "undo reset keyframe should unset keypoint");
    incActionRedo();
    require(binding.isSet(kp) && near(binding.getValue(kp), 0.0f), "redo reset keyframe should restore reset value");

    binding.setValue(kp, 3.0f);
    incActionClearHistory();
    require((new InvertKeyFrameCommand()).run(ctx).succeeded, "InvertKeyFrameCommand should succeed");
    require(near(binding.getValue(kp), -3.0f), "invert keyframe should negate value");
    incActionUndo();
    require(near(binding.getValue(kp), 3.0f), "undo invert keyframe should restore old value");
    incActionRedo();
    require(near(binding.getValue(kp), -3.0f), "redo invert keyframe should restore inverted value");

    binding.setValue(kp, 5.0f);
    incActionClearHistory();
    require((new MirrorKeyFrameHorizontallyCommand()).run(ctx).succeeded, "MirrorKeyFrameHorizontallyCommand should succeed");
    require(near(binding.getValue(kp), -5.0f), "horizontal mirror should flip transform.t.x");
    incActionUndo();
    require(near(binding.getValue(kp), 5.0f), "undo horizontal mirror should restore old value");

    binding.setValue(kp, 7.0f);
    incActionClearHistory();
    require((new UnsetKeyFrameCommand()).run(ctx).succeeded, "UnsetKeyFrameCommand should succeed");
    require(!binding.isSet(kp), "unset keyframe should clear keypoint");
    incActionUndo();
    require(binding.isSet(kp) && near(binding.getValue(kp), 7.0f), "undo unset keyframe should restore old value");
    incActionRedo();
    require(!binding.isSet(kp), "redo unset keyframe should clear keypoint again");

    binding.unset(kp);
    node.setValue("transform.t.x", 9.0f);
    incActionClearHistory();
    require((new SetKeyFrameCommand()).run(ctx).succeeded, "SetKeyFrameCommand should succeed");
    require(binding.isSet(kp), "set keyframe should mark keypoint as set");
    incActionUndo();
    require(!binding.isSet(kp), "undo set keyframe should restore unset state");
    incActionRedo();
    require(binding.isSet(kp), "redo set keyframe should mark keypoint as set again");
}

private void testParameterKeyframeMirrorFillCommands() {
    resetCase();

    auto node2D = new Node(incActivePuppet().root);
    auto param2D = new2DParameter("MirrorFill2D");
    auto binding2D = newValueBinding(param2D, node2D, "transform.t.x", false);

    binding2D.setValue(vec2u(0, 1), 11.0f);
    binding2D.setValue(vec2u(1, 0), 13.0f);
    binding2D.setValue(vec2u(0, 0), 17.0f);
    incActionClearHistory();

    auto horizontalCtx = bindingContext(param2D, binding2D, vec2u(2, 1));
    require((new SetFromHorizontalMirrorCommand()).run(horizontalCtx).succeeded, "SetFromHorizontalMirrorCommand should succeed");
    require(near(binding2D.getValue(vec2u(2, 1)), -11.0f), "horizontal mirror-fill should copy mirrored X source and flip transform.t.x sign");
    incActionUndo();
    require(!binding2D.isSet(vec2u(2, 1)), "undo horizontal mirror-fill should restore unset destination");
    incActionRedo();
    require(near(binding2D.getValue(vec2u(2, 1)), -11.0f), "redo horizontal mirror-fill should restore mirrored destination");

    incActionClearHistory();
    auto verticalCtx = bindingContext(param2D, binding2D, vec2u(1, 2));
    require((new SetFromVerticalMirrorCommand()).run(verticalCtx).succeeded, "SetFromVerticalMirrorCommand should succeed");
    require(near(binding2D.getValue(vec2u(1, 2)), 13.0f), "vertical mirror-fill should copy mirrored Y source without flipping transform.t.x sign");
    incActionUndo();
    require(!binding2D.isSet(vec2u(1, 2)), "undo vertical mirror-fill should restore unset destination");
    incActionRedo();
    require(near(binding2D.getValue(vec2u(1, 2)), 13.0f), "redo vertical mirror-fill should restore mirrored destination");

    incActionClearHistory();
    auto diagonalCtx = bindingContext(param2D, binding2D, vec2u(2, 2));
    require((new SetFromDiagonalMirrorCommand()).run(diagonalCtx).succeeded, "SetFromDiagonalMirrorCommand should succeed");
    require(near(binding2D.getValue(vec2u(2, 2)), -17.0f),
        "diagonal mirror-fill should copy diagonal source and flip transform.t.x sign; got " ~ binding2D.getValue(vec2u(2, 2)).to!string);
    incActionUndo();
    require(!binding2D.isSet(vec2u(2, 2)), "undo diagonal mirror-fill should restore unset destination");
    incActionRedo();
    require(near(binding2D.getValue(vec2u(2, 2)), -17.0f), "redo diagonal mirror-fill should restore mirrored destination");

    auto node1D = new Node(incActivePuppet().root);
    auto param1D = new ExParameter("MirrorFill1D", false);
    param1D.min = vec2(-1, 0);
    param1D.max = vec2(1, 0);
    param1D.insertAxisPoint(0, 0.5f);
    incActivePuppet().parameters ~= param1D;
    auto binding1D = newValueBinding(param1D, node1D, "transform.t.x", false);
    binding1D.setValue(vec2u(0, 0), 19.0f);
    incActionClearHistory();

    auto oneDCtx = bindingContext(param1D, binding1D, vec2u(2, 0));
    require((new SetFrom1DMirrorCommand()).run(oneDCtx).succeeded, "SetFrom1DMirrorCommand should succeed");
    require(near(binding1D.getValue(vec2u(2, 0)), -19.0f), "1D mirror-fill should copy mirrored source and flip transform.t.x sign");
    incActionUndo();
    require(!binding1D.isSet(vec2u(2, 0)), "undo 1D mirror-fill should restore unset destination");
    incActionRedo();
    require(near(binding1D.getValue(vec2u(2, 0)), -19.0f), "redo 1D mirror-fill should restore mirrored destination");
}

private void testParameterStartingKeyframeCommandUndoRedo() {
    resetCase();

    auto param = new2DParameter("StartingKey");
    param.defaults = vec2(0, 0);
    param.value = vec2(0.5f, -0.25f);

    auto ctx = new Context();
    ctx.parameters = [param];

    require((new SetStartingKeyFrameCommand()).run(ctx).succeeded, "SetStartingKeyFrameCommand should succeed");
    require(param.defaults == vec2(0.5f, -0.25f), "SetStartingKeyFrameCommand should copy current value to defaults");

    incActionUndo();
    require(param.defaults == vec2(0, 0), "undo SetStartingKeyFrameCommand should restore previous defaults");

    incActionRedo();
    require(param.defaults == vec2(0.5f, -0.25f), "redo SetStartingKeyFrameCommand should restore new defaults");
}

private void testParameterBindingClipboardAndCleanupCommands() {
    resetCase();

    auto sourceNode = new Node(incActivePuppet().root);
    sourceNode.name = "source-binding-node";
    auto destNode = new Node(incActivePuppet().root);
    destNode.name = "dest-binding-node";
    auto param = new2DParameter("BindingClipboard");
    auto key = vec2u(2, 1);

    auto sourceBinding = newValueBinding(param, sourceNode, "transform.t.x", false);
    auto destBinding = newValueBinding(param, destNode, "transform.t.x", false);
    sourceBinding.setValue(key, 12.5f);
    destBinding.setValue(key, -1.0f);

    auto copyCtx = bindingContext(param, sourceBinding, key);
    require((new CopyBindingCommand()).run(copyCtx).succeeded, "CopyBindingCommand should succeed");

    auto pasteCtx = bindingContext(param, destBinding, key);
    pasteCtx.activeBindings = [destBinding];
    require((new PasteBindingCommand()).run(pasteCtx).succeeded, "PasteBindingCommand should succeed");
    require(near(destBinding.getValue(key), 12.5f), "PasteBindingCommand should copy the keypoint value");

    incActionUndo();
    require(near(destBinding.getValue(key), -1.0f), "undo PasteBindingCommand should restore destination value");
    incActionRedo();
    require(near(destBinding.getValue(key), 12.5f), "redo PasteBindingCommand should restore pasted value");

    auto interpCtx = new Context();
    interpCtx.parameters = [param];
    interpCtx.activeBindings = [destBinding];
    require((new SetInterpolationCommand(InterpolateMode.Nearest)).run(interpCtx).succeeded, "SetInterpolationCommand should succeed");
    require(destBinding.interpolateMode == InterpolateMode.Nearest, "SetInterpolationCommand should change interpolation mode");
    incActionUndo();
    require(destBinding.interpolateMode != InterpolateMode.Nearest, "undo SetInterpolationCommand should restore interpolation mode");
    incActionRedo();
    require(destBinding.interpolateMode == InterpolateMode.Nearest, "redo SetInterpolationCommand should restore interpolation mode");

    auto removeCtx = new Context();
    removeCtx.parameters = [param];
    removeCtx.activeBindings = [destBinding];
    require((new RemoveBindingCommand()).run(removeCtx).succeeded, "RemoveBindingCommand should succeed");
    require(param.getBinding(destNode, "transform.t.x") is null, "RemoveBindingCommand should remove destination binding");

    incActionUndo();
    require(param.getBinding(destNode, "transform.t.x") is destBinding, "undo RemoveBindingCommand should restore binding");
    incActionRedo();
    require(param.getBinding(destNode, "transform.t.x") is null, "redo RemoveBindingCommand should remove binding again");
}

private void testParameterBindingCleanupOnDeleteUndoRedo() {
    resetCase();

    auto node = new Node(incActivePuppet().root);
    node.name = "binding-cleanup-node";
    auto param = new2DParameter("BindingCleanup");
    auto binding = newValueBinding(param, node, "transform.t.x", false);
    binding.setValue(vec2u(2, 2), 7.0f);

    auto removeCtx = new Context();
    removeCtx.parameters = [param];
    removeCtx.activeBindings = [binding];
    require((new RemoveBindingCommand()).run(removeCtx).succeeded, "RemoveBindingCommand should remove active binding");
    require(param.bindings.length == 0, "binding cleanup should leave no binding in parameter");

    incActionUndo();
    require(param.bindings.length == 1 && param.bindings[0] is binding, "undo binding cleanup should restore binding");

    auto deleteCtx = new Context();
    deleteCtx.puppet = incActivePuppet();
    deleteCtx.parameters = [param];
    incActionClearHistory();
    require((new DeleteParameterCommand()).run(deleteCtx).succeeded, "DeleteParameterCommand should delete parameter with bindings");
    require(findParameter(incActivePuppet(), "BindingCleanup") is null, "DeleteParameterCommand should remove parameter from puppet");

    incActionUndo();
    auto restored = findParameter(incActivePuppet(), "BindingCleanup");
    require(restored is param, "undo DeleteParameterCommand should restore parameter object");
    require(param.bindings.length == 1 && param.bindings[0] is binding, "undo DeleteParameterCommand should preserve bindings on restored parameter");

    incActionRedo();
    require(findParameter(incActivePuppet(), "BindingCleanup") is null, "redo DeleteParameterCommand should remove parameter again");
}

private void testAnimationLifecycleUndoRedo() {
    resetCase();

    auto param = new ExParameter("AnimLife", false);
    param.min = vec2(0, 0);
    param.max = vec2(1, 0);
    incActivePuppet().parameters ~= param;

    Animation animation;
    animation.length = 24;
    animation.leadIn = 2;
    animation.leadOut = 20;
    animation.timestep = 1.0 / 30.0;
    animation.animationWeight = 0.75f;
    animation.lanes ~= AnimationLane(
        param.uuid,
        new AnimationParameterRef(param, 0),
        [Keyframe(0, 0.1f, 0.5f), Keyframe(12, 0.9f, 0.5f)],
        InterpolateMode.Linear
    );

    require(ngAnimationCreateOrUpdate("walk", animation), "animation create should succeed");
    require(("walk" in incActivePuppet().getAnimations()) !is null, "animation create should add the animation");
    require(incAnimationGet() !is null && incAnimationGet().name == "walk", "animation create should select the new animation");
    require(incActivePuppet().getAnimations()["walk"].lanes.length == 1, "animation create should preserve lanes");
    require(near(cast(float)incActivePuppet().getAnimations()["walk"].timestep, cast(float)(1.0 / 30.0)), "animation create should preserve timestep");
    require(near(incActivePuppet().getAnimations()["walk"].animationWeight, 0.75f), "animation create should preserve animation weight");

    incActionUndo();
    require(("walk" in incActivePuppet().getAnimations()) is null, "undo animation create should remove the animation");
    require(incAnimationGet() is null, "undo animation create should clear current animation");

    incActionRedo();
    require(("walk" in incActivePuppet().getAnimations()) !is null, "redo animation create should restore the animation");
    require(incAnimationGet() !is null && incAnimationGet().name == "walk", "redo animation create should select the animation");

    Animation renamed = incActivePuppet().getAnimations()["walk"];
    renamed.length = 48;
    renamed.leadIn = 4;
    renamed.leadOut = 44;
    renamed.timestep = 1.0 / 60.0;
    renamed.animationWeight = 0.5f;
    renamed.additive = true;
    incActionClearHistory();
    require(ngAnimationCreateOrUpdate("run", renamed, "walk"), "animation rename should succeed");
    require(("walk" in incActivePuppet().getAnimations()) is null && ("run" in incActivePuppet().getAnimations()) !is null, "animation rename should replace the key");
    require(incActivePuppet().getAnimations()["run"].length == 48, "animation rename should apply edited data");
    require(incActivePuppet().getAnimations()["run"].leadIn == 4 && incActivePuppet().getAnimations()["run"].leadOut == 44, "animation rename should apply lead-in/out");
    require(near(cast(float)incActivePuppet().getAnimations()["run"].timestep, cast(float)(1.0 / 60.0)), "animation rename should apply timestep");
    require(near(incActivePuppet().getAnimations()["run"].animationWeight, 0.5f), "animation rename should apply weight");
    require(incActivePuppet().getAnimations()["run"].additive, "animation rename should apply additive flag");
    require(incActivePuppet().getAnimations()["run"].lanes.length == 1, "animation rename should preserve tracks");

    incActionUndo();
    require(("walk" in incActivePuppet().getAnimations()) !is null && ("run" in incActivePuppet().getAnimations()) is null, "undo animation rename should restore old key");
    require(incActivePuppet().getAnimations()["walk"].length == 24, "undo animation rename should restore old data");
    require(incActivePuppet().getAnimations()["walk"].leadIn == 2 && incActivePuppet().getAnimations()["walk"].leadOut == 20, "undo animation rename should restore old lead-in/out");
    require(near(cast(float)incActivePuppet().getAnimations()["walk"].timestep, cast(float)(1.0 / 30.0)), "undo animation rename should restore old timestep");
    require(near(incActivePuppet().getAnimations()["walk"].animationWeight, 0.75f), "undo animation rename should restore old weight");

    incActionRedo();
    require(("run" in incActivePuppet().getAnimations()) !is null && ("walk" in incActivePuppet().getAnimations()) is null, "redo animation rename should restore new key");
    require(incActivePuppet().getAnimations()["run"].length == 48 && incActivePuppet().getAnimations()["run"].leadOut == 44, "redo animation rename should restore edited properties");

    incActionClearHistory();
    require(ngAnimationDelete("run"), "animation delete should succeed");
    require(("run" in incActivePuppet().getAnimations()) is null, "animation delete should remove the animation");
    require(incAnimationGet() is null, "animation delete should clear current animation when deleting the active one");

    incActionUndo();
    require(("run" in incActivePuppet().getAnimations()) !is null, "undo animation delete should restore animation");
    require(incActivePuppet().getAnimations()["run"].lanes.length == 1, "undo animation delete should restore tracks");
    require(incAnimationGet() !is null && incAnimationGet().name == "run", "undo animation delete should restore active animation");

    incActionRedo();
    require(("run" in incActivePuppet().getAnimations()) is null, "redo animation delete should remove animation again");
}

private void testAnimationTrackBindingCleanup() {
    resetCase();

    auto fixtureDir = buildPath("/private/tmp", "nijigenerate-regression-animation-track-cleanup");
    if (exists(fixtureDir))
        rmdirRecurse(fixtureDir);
    mkdirRecurse(fixtureDir);
    scope(exit) {
        if (exists(fixtureDir))
            rmdirRecurse(fixtureDir);
    }

    auto keepParam = new ExParameter("AnimTrackKeep", false);
    keepParam.min = vec2(0, 0);
    keepParam.max = vec2(1, 0);
    auto deleteParam = new ExParameter("AnimTrackDelete", false);
    deleteParam.min = vec2(0, 0);
    deleteParam.max = vec2(1, 0);
    incActivePuppet().parameters ~= keepParam;
    incActivePuppet().parameters ~= deleteParam;

    Animation animation;
    animation.length = 30;
    animation.lanes ~= AnimationLane(
        keepParam.uuid,
        new AnimationParameterRef(keepParam, 0),
        [Keyframe(0, 0.0f, 0.5f), Keyframe(10, 1.0f, 0.5f)],
        InterpolateMode.Linear
    );
    animation.lanes ~= AnimationLane(
        deleteParam.uuid,
        new AnimationParameterRef(deleteParam, 0),
        [Keyframe(0, 0.25f, 0.5f), Keyframe(10, 0.75f, 0.5f)],
        InterpolateMode.Linear
    );
    require(ngAnimationCreateOrUpdate("track-cleanup", animation), "animation track cleanup fixture should create animation");
    require(incAnimationGet().animation.lanes.length == 2, "animation fixture should contain both lanes");

    auto ctx = new Context();
    ctx.parameters = [keepParam];
    require((new SetParameterNameCommand("AnimTrackRenamed")).run(ctx).succeeded, "renaming animation target parameter should succeed");
    require(incAnimationGet().animation.lanes[0].paramRef.targetParam is keepParam, "animation lane should keep parameter reference across rename");
    require(incAnimationGet().animation.lanes[0].paramRef.targetParam.name == "AnimTrackRenamed", "animation lane should expose renamed parameter");
    incActionUndo();
    require(incAnimationGet().animation.lanes[0].paramRef.targetParam.name == "AnimTrackKeep", "undo rename should be reflected through animation lane reference");
    incActionRedo();
    require(incAnimationGet().animation.lanes[0].paramRef.targetParam.name == "AnimTrackRenamed", "redo rename should be reflected through animation lane reference");

    incActionClearHistory();
    ctx = new Context();
    ctx.puppet = incActivePuppet();
    ctx.parameters = [deleteParam];
    require((new DeleteParameterCommand()).run(ctx).succeeded, "deleting animation target parameter should succeed");
    require(findParameter(incActivePuppet(), "AnimTrackDelete") is null, "deleted animation target parameter should leave the puppet parameter list");

    auto saveBase = buildPath(fixtureDir, "track-cleanup");
    auto savePath = saveBase ~ ".inx";
    require((new SaveFileCommand(saveBase)).run(new Context()).succeeded, "animation cleanup fixture should save");
    require((new OpenFileCommand(savePath)).run(new Context()).succeeded, "animation cleanup fixture should load");

    auto loadedAnimation = "track-cleanup" in incActivePuppet().getAnimations();
    require(loadedAnimation !is null, "animation cleanup fixture should reload animation");
    bool sawRenamedLane = false;
    bool sawDeletedTarget = false;
    foreach (lane; loadedAnimation.lanes) {
        if (lane.paramRef !is null && lane.paramRef.targetParam !is null) {
            if (lane.paramRef.targetParam.name == "AnimTrackRenamed")
                sawRenamedLane = true;
            if (lane.paramRef.targetParam.name == "AnimTrackDelete")
                sawDeletedTarget = true;
        }
    }
    require(sawRenamedLane, "native reload should reconnect lane to renamed surviving parameter");
    require(!sawDeletedTarget, "native reload should not reconnect lanes to deleted parameters");
}

private void testAnimationKeyframesUndoRedo() {
    resetCase();

    Animation animation;
    animation.length = 60;
    require(ngAnimationCreateOrUpdate("keys", animation), "animation setup should create an animation");
    incSetEditMode(EditMode.AnimEdit, false);
    incAnimationChange("keys");

    auto param = new ExParameter("Anim1D", false);
    param.min = vec2(0, 0);
    param.max = vec2(1, 0);
    param.value = vec2(0.25f, 0);
    incActivePuppet().parameters ~= param;

    auto playback = incAnimationGet();
    playback.seek(3);
    Parameter addParam = param;
    incActionClearHistory();
    incAnimationKeyframeAdd(addParam, 0, 0.25f);
    require(incAnimationGet().animation.lanes.length == 1, "1D keyframe add should create one lane");
    require(incAnimationGet().animation.lanes[0].frames.length == 1, "1D keyframe add should create one keyframe");
    require(incAnimationGet().animation.lanes[0].frames[0].frame == 3, "1D keyframe add should use current frame");

    incActionUndo();
    require(incAnimationGet().animation.lanes.length == 0, "undo 1D keyframe add should remove the lane");
    incActionRedo();
    require(incAnimationGet().animation.lanes.length == 1 && incAnimationGet().animation.lanes[0].frames.length == 1, "redo 1D keyframe add should restore the lane");

    incActionClearHistory();
    incAnimationKeyframeAdd(addParam, 0, 0.75f);
    require(incAnimationGet().animation.lanes[0].frames.length == 1, "editing same frame should not duplicate keyframes");
    require(near(incAnimationGet().animation.lanes[0].frames[0].value, 0.75f), "editing same frame should update value");

    incActionUndo();
    require(near(incAnimationGet().animation.lanes[0].frames[0].value, 0.25f), "undo keyframe edit should restore old value");
    incActionRedo();
    require(near(incAnimationGet().animation.lanes[0].frames[0].value, 0.75f), "redo keyframe edit should restore new value");

    incActionClearHistory();
    require(incAnimationKeyframeRemove(addParam, 0), "1D keyframe remove should succeed");
    require(incAnimationGet().animation.lanes[0].frames.length == 0, "1D keyframe remove should remove current frame");

    incActionUndo();
    require(incAnimationGet().animation.lanes[0].frames.length == 1, "undo 1D keyframe remove should restore keyframe");
    incActionRedo();
    require(incAnimationGet().animation.lanes[0].frames.length == 0, "redo 1D keyframe remove should remove keyframe again");
}

private void testParameter2DAnimationKeyframeGroupUndoRedo() {
    resetCase();

    Animation animation;
    incActivePuppet().getAnimations()["regression"] = animation;
    incSetEditMode(EditMode.AnimEdit, false);
    incAnimationChange("regression");

    auto param = new ExParameter("Anim2D", true);
    param.min = vec2(-1, -1);
    param.max = vec2(1, 1);
    param.value = vec2(0.25f, -0.5f);
    incActivePuppet().parameters ~= param;

    auto ctx = new Context();
    ctx.parameters = [param];

    require((new AddAnimationKeyFrameCommand()).run(ctx).succeeded, "AddAnimationKeyFrameCommand should succeed for 2D parameter");
    require(incActionHistory().length == 1, "2D animation keyframe add should be one grouped undo entry");
    require(incAnimationGet().animation.lanes.length == 2, "2D animation keyframe add should create X and Y lanes");
    require(incAnimationGet().animation.lanes[0].frames.length == 1, "X lane should get one keyframe");
    require(incAnimationGet().animation.lanes[1].frames.length == 1, "Y lane should get one keyframe");

    incActionUndo();
    require(incAnimationGet().animation.lanes.length == 0, "undo 2D animation keyframe add should remove both lanes at once");

    incActionRedo();
    require(incAnimationGet().animation.lanes.length == 2, "redo 2D animation keyframe add should restore both lanes at once");

    incActionClearHistory();
    Parameter removeParam = param;
    incActionPushGroup();
    incAnimationKeyframeRemove(removeParam, 0);
    incAnimationKeyframeRemove(removeParam, 1);
    incActionPopGroup();
    require(incActionHistory().length == 1, "2D animation keyframe remove should be one grouped undo entry");
    require(incAnimationGet().animation.lanes.length == 2, "2D animation keyframe remove should preserve lanes");
    require(incAnimationGet().animation.lanes[0].frames.length == 0, "X lane keyframe should be removed");
    require(incAnimationGet().animation.lanes[1].frames.length == 0, "Y lane keyframe should be removed");

    incActionUndo();
    require(incAnimationGet().animation.lanes[0].frames.length == 1, "undo 2D animation keyframe remove should restore X keyframe");
    require(incAnimationGet().animation.lanes[1].frames.length == 1, "undo 2D animation keyframe remove should restore Y keyframe");

    incActionRedo();
    require(incAnimationGet().animation.lanes[0].frames.length == 0, "redo 2D animation keyframe remove should remove X keyframe again");
    require(incAnimationGet().animation.lanes[1].frames.length == 0, "redo 2D animation keyframe remove should remove Y keyframe again");
}

private void testParameterBindingInterpolation() {
    resetCase();

    auto node1D = new Node(incActivePuppet().root);
    auto param1D = new ExParameter("Interp1D", false);
    param1D.min = vec2(-1, 0);
    param1D.max = vec2(1, 0);
    param1D.insertAxisPoint(0, 0.5f);
    incActivePuppet().parameters ~= param1D;

    auto binding1D = newValueBinding(param1D, node1D, "transform.t.x", false);
    binding1D.setValue(vec2u(0, 0), 0.0f);
    binding1D.setValue(vec2u(2, 0), 20.0f);
    require(binding1D.isSet(vec2u(0, 0)) && binding1D.isSet(vec2u(2, 0)), "1D interpolation endpoints should remain set");
    require(near(binding1D.getValue(vec2u(1, 0)), 10.0f), "1D interpolation should fill midpoint");
    binding1D.unset(vec2u(1, 0));
    require(near(binding1D.getValue(vec2u(1, 0)), 10.0f), "1D reInterpolate should recover midpoint after unset");

    auto node2D = new Node(incActivePuppet().root);
    auto param2D = new ExParameter("Interp2D", true);
    param2D.min = vec2(-1, -1);
    param2D.max = vec2(1, 1);
    param2D.insertAxisPoint(0, 0.5f);
    param2D.insertAxisPoint(1, 0.5f);
    incActivePuppet().parameters ~= param2D;

    auto binding2D = newValueBinding(param2D, node2D, "transform.t.x", false);
    binding2D.setValue(vec2u(0, 0), 0.0f);
    binding2D.setValue(vec2u(2, 0), 10.0f);
    binding2D.setValue(vec2u(0, 2), 20.0f);
    binding2D.setValue(vec2u(2, 2), 30.0f);
    require(binding2D.isSet(vec2u(0, 0)) && binding2D.isSet(vec2u(2, 0)) && binding2D.isSet(vec2u(0, 2)) && binding2D.isSet(vec2u(2, 2)), "2D interpolation corners should remain set");
    require(near(binding2D.getValue(vec2u(1, 0)), 5.0f), "2D interpolation should fill top edge");
    require(near(binding2D.getValue(vec2u(0, 1)), 10.0f), "2D interpolation should fill left edge");
    require(near(binding2D.getValue(vec2u(2, 1)), 20.0f), "2D interpolation should fill right edge");
    require(near(binding2D.getValue(vec2u(1, 2)), 25.0f), "2D interpolation should fill bottom edge");
    require(near(binding2D.getValue(vec2u(1, 1)), 15.0f), "2D interpolation should fill center from both axes");
    binding2D.unset(vec2u(1, 1));
    require(near(binding2D.getValue(vec2u(1, 1)), 15.0f), "2D reInterpolate should recover center after unset");
}

private void testParameterTRSBindingModelCommandUndoRedo() {
    resetCase();

    auto param = new2DParameter("TRSBinding");
    auto node = new Node(incActivePuppet().root);
    node.name = "trs-target";

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    ctx.parameters = [param];
    ctx.nodes = [node];
    ctx.keyPoint = vec2u(2, 2);
    ctx.hasExplicitKeyPoint = true;

    auto result = (new SetTRSBindingCommand([10.0f, 20.0f], [2.0f, 3.0f], 45.0f, true)).run(ctx);
    require(result.succeeded, "SetTRSBindingCommand should succeed");
    auto tx = cast(ValueParameterBinding)param.getBinding(node, "transform.t.x");
    auto ty = cast(ValueParameterBinding)param.getBinding(node, "transform.t.y");
    auto sx = cast(ValueParameterBinding)param.getBinding(node, "transform.s.x");
    auto sy = cast(ValueParameterBinding)param.getBinding(node, "transform.s.y");
    auto rz = cast(ValueParameterBinding)param.getBinding(node, "transform.r.z");
    require(tx !is null && ty !is null && sx !is null && sy !is null && rz !is null, "SetTRSBindingCommand should create TRS bindings");
    require(near(tx.getValue(vec2u(2, 2)), 10.0f), "SetTRSBindingCommand should set translation X");
    require(near(ty.getValue(vec2u(2, 2)), 20.0f), "SetTRSBindingCommand should set translation Y");
    require(near(sx.getValue(vec2u(2, 2)), 2.0f), "SetTRSBindingCommand should set scale X");
    require(near(sy.getValue(vec2u(2, 2)), 3.0f), "SetTRSBindingCommand should set scale Y");
    require(near(rz.getValue(vec2u(2, 2)), cast(float)(45.0 * 3.14159265358979323846 / 180.0)), "SetTRSBindingCommand should set rotation in radians");

    incActionUndo();
    require(param.getBinding(node, "transform.t.x") is null, "undo SetTRSBindingCommand should remove newly-created TRS bindings");

    incActionRedo();
    tx = cast(ValueParameterBinding)param.getBinding(node, "transform.t.x");
    rz = cast(ValueParameterBinding)param.getBinding(node, "transform.r.z");
    require(tx !is null && near(tx.getValue(vec2u(2, 2)), 10.0f), "redo SetTRSBindingCommand should restore translation binding");
    require(rz !is null && near(rz.getValue(vec2u(2, 2)), cast(float)(45.0 * 3.14159265358979323846 / 180.0)), "redo SetTRSBindingCommand should restore rotation binding");
}

private void testParameterDeformBindingModelCommandUndoRedo() {
    resetCase();

    auto param = new2DParameter("DeformBinding");
    auto part = newMeshPart("deform-binding-target");

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    ctx.parameters = [param];
    ctx.nodes = [part];
    ctx.keyPoint = vec2u(2, 2);
    ctx.hasExplicitKeyPoint = true;

    auto result = (new SetDeformBindingCommand("deform", [
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
    ])).run(ctx);
    require(result.succeeded, "SetDeformBindingCommand should succeed");
    auto binding = cast(DeformationParameterBinding)param.getBinding(part, "deform");
    require(binding !is null, "SetDeformBindingCommand should create deformation binding");
    require(binding.isSet(vec2u(2, 2)), "SetDeformBindingCommand should set requested keypoint");
    require(binding.getValue(vec2u(2, 2)).vertexOffsets.length == part.vertices.length, "deformation binding offsets should match target vertices");
    require(binding.getValue(vec2u(2, 2)).vertexOffsets[0] != vec2(0, 0), "deformation binding should store non-zero offsets");

    incActionUndo();
    require(param.getBinding(part, "deform") is null, "undo SetDeformBindingCommand should remove newly-created deformation binding");

    incActionRedo();
    binding = cast(DeformationParameterBinding)param.getBinding(part, "deform");
    require(binding !is null, "redo SetDeformBindingCommand should restore deformation binding");
    require(binding.getValue(vec2u(2, 2)).vertexOffsets.length == part.vertices.length, "redo SetDeformBindingCommand should restore offset length");
}

private void testParameterAxesPropsCommandUndoRedo() {
    resetCase();

    auto node = new Node(incActivePuppet().root);
    auto param = new ExParameter("Axes", true);
    param.min = vec2(-1, -1);
    param.max = vec2(1, 1);
    param.insertAxisPoint(0, 0.5f);
    param.insertAxisPoint(1, 0.5f);
    incActivePuppet().parameters ~= param;

    auto oldAxisX = param.axisPoints[0].dup;
    auto oldAxisY = param.axisPoints[1].dup;
    auto binding = newValueBinding(param, node, "transform.t.x");
    binding.setValue(vec2u(1, 1), 6.0f);

    auto ctx = new Context();
    ctx.parameters = [param];

    require((new ApplyParameterPropsAxesCommand(
        [-2.0f, -3.0f],
        [2.0f, 3.0f],
        [0.0f, 0.25f, 0.5f, 0.75f, 1.0f],
        [0.0f, 0.5f, 1.0f]
    )).run(ctx).succeeded, "ApplyParameterPropsAxesCommand should succeed");

    require(param.min == vec2(-2, -3), "ApplyParameterPropsAxesCommand should apply min");
    require(param.max == vec2(2, 3), "ApplyParameterPropsAxesCommand should apply max");
    require(param.axisPoints[0].length == 5 && param.axisPoints[1].length == 3, "ApplyParameterPropsAxesCommand should apply axis breakpoints");
    require(binding.isSet(vec2u(2, 1)) && near(binding.getValue(vec2u(2, 1)), 6.0f), "ApplyParameterPropsAxesCommand should remap bound center value");

    incActionUndo();
    require(param.min == vec2(-1, -1), "undo ApplyParameterPropsAxesCommand should restore min");
    require(param.max == vec2(1, 1), "undo ApplyParameterPropsAxesCommand should restore max");
    require(param.axisPoints[0] == oldAxisX && param.axisPoints[1] == oldAxisY, "undo ApplyParameterPropsAxesCommand should restore axis breakpoints");
    require(binding.isSet(vec2u(1, 1)) && near(binding.getValue(vec2u(1, 1)), 6.0f), "undo ApplyParameterPropsAxesCommand should restore original bound value");

    incActionRedo();
    require(param.min == vec2(-2, -3), "redo ApplyParameterPropsAxesCommand should restore min");
    require(param.max == vec2(2, 3), "redo ApplyParameterPropsAxesCommand should restore max");
    require(param.axisPoints[0].length == 5 && param.axisPoints[1].length == 3, "redo ApplyParameterPropsAxesCommand should restore axis breakpoints");
    require(binding.isSet(vec2u(2, 1)) && near(binding.getValue(vec2u(2, 1)), 6.0f), "redo ApplyParameterPropsAxesCommand should restore remapped bound value");
}

private void testDepthBoneActionsUndoRedo() {
    resetCase();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    auto target = newPart("target");
    auto bone = new ExDepthBone(root);
    bone.boneId = "Bone";

    ExDepthRigBinding binding;
    binding.targetUuid = target.uuid;
    binding.targetKind = ExDepthTargetKind.Grid;
    binding.sourceBoneUuids = [cast(ulong)bone.uuid];
    ExDepthBoneSourceSettings setting;
    setting.boneUuid = bone.uuid;
    setting.weight = 0.75f;
    setting.depthOffset = 1.25f;
    setting.depthScale = 0.5f;
    binding.sourceSettings = [setting];

    auto oldBindings = root.bindings.dup;
    root.bindings = [binding];
    incActionPush(new DepthBoneSourceListChangeAction("DepthBone Source List", root, oldBindings, root.bindings));
    require(root.bindings.length == 1, "depth bone source list change should apply");
    require(root.bindings[0].sourceSettings[0].depthOffset == 1.25f, "depth bone source settings should apply");

    incActionUndo();
    require(root.bindings.length == 0, "depth bone source list undo should restore old list");

    incActionRedo();
    require(root.bindings.length == 1, "depth bone source list redo should restore new list");

    auto oldHead = bone.restHead;
    auto oldTail = bone.restTail;
    auto oldRoll = bone.restRoll;
    auto newHead = vec3(1, 2, 3);
    auto newTail = vec3(4, 5, 6);
    auto newRoll = 0.25f;
    bone.restHead = newHead;
    bone.restTail = newTail;
    bone.restRoll = newRoll;
    incActionPush(new DepthBoneRestChangeAction(bone, oldHead, oldTail, oldRoll, newHead, newTail, newRoll));
    require(bone.restHead == newHead && bone.restTail == newTail && near(bone.restRoll, newRoll), "depth bone rest change should apply");

    incActionUndo();
    require(bone.restHead == oldHead && bone.restTail == oldTail && near(bone.restRoll, oldRoll), "depth bone rest undo should restore old values");

    incActionRedo();
    require(bone.restHead == newHead && bone.restTail == newTail && near(bone.restRoll, newRoll), "depth bone rest redo should restore new values");

    auto constraintAction = new DepthBoneConstraintChangeAction(bone);
    bone.constraintType = "hinge";
    bone.hingeAxis = vec3(0, 1, 0);
    bone.lockRotation = true;
    bone.lockTranslation = true;
    bone.allowParentToTargets = false;
    bone.rotationLimits = [-0.5f, 0.5f];
    bone.maxStepRadians = 0.1f;
    constraintAction.updateNewState();
    incActionPush(constraintAction);
    require(bone.constraintType == "hinge" && bone.lockRotation && bone.lockTranslation && !bone.allowParentToTargets, "depth bone constraint change should apply");

    incActionUndo();
    require(bone.constraintType == "" && !bone.lockRotation && !bone.lockTranslation && bone.allowParentToTargets, "depth bone constraint undo should restore old values");

    incActionRedo();
    require(bone.constraintType == "hinge" && bone.lockRotation && bone.lockTranslation && !bone.allowParentToTargets, "depth bone constraint redo should restore new values");
}

private void testDepthBoneInspectorCommandsUndoRedo() {
    resetCase();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "depth-root";
    auto bone = new ExDepthBone(root);
    bone.name = "depth-bone";
    bone.boneId = "Bone";
    incActivePuppet().rescanNodes();

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    ctx.nodes = [bone];

    auto restResult = cmd!(DepthBoneCommand.SetDepthBoneRest)(
        ctx,
        bone,
        [1.0f, 2.0f, 3.0f],
        [4.0f, 5.0f, 6.0f],
        0.25f
    );
    require(restResult.succeeded, "SetDepthBoneRest command should succeed");
    require(bone.restHead == vec3(1, 2, 3), "SetDepthBoneRest should apply rest head");
    require(bone.restTail == vec3(4, 5, 6), "SetDepthBoneRest should apply rest tail");
    require(near(bone.restRoll, 0.25f), "SetDepthBoneRest should apply rest roll");

    incActionUndo();
    require(bone.restHead == vec3(0, 0, 0), "undo SetDepthBoneRest should restore rest head");
    require(bone.restTail == vec3(0, 100, 0), "undo SetDepthBoneRest should restore rest tail");
    require(near(bone.restRoll, 0.0f), "undo SetDepthBoneRest should restore rest roll");

    incActionRedo();
    require(bone.restHead == vec3(1, 2, 3), "redo SetDepthBoneRest should restore rest head");
    require(bone.restTail == vec3(4, 5, 6), "redo SetDepthBoneRest should restore rest tail");
    require(near(bone.restRoll, 0.25f), "redo SetDepthBoneRest should restore rest roll");

    incActionClearHistory();
    auto constraintResult = cmd!(DepthBoneCommand.SetDepthBoneConstraint)(
        ctx,
        bone,
        `{"constraintType":"hinge","hingeAxis":[0,1,0],"lockRotation":true,"lockTranslation":true,"allowParentToTargets":false,"rotationLimits":[-0.5,0.5],"maxStepRadians":0.1}`
    );
    require(constraintResult.succeeded, "SetDepthBoneConstraint command should succeed");
    require(bone.constraintType == "hinge", "SetDepthBoneConstraint should apply constraint type");
    require(bone.hingeAxis == vec3(0, 1, 0), "SetDepthBoneConstraint should apply hinge axis");
    require(bone.lockRotation && bone.lockTranslation && !bone.allowParentToTargets, "SetDepthBoneConstraint should apply boolean flags");
    require(bone.rotationLimits.length == 2 && near(bone.rotationLimits[0], -0.5f) && near(bone.rotationLimits[1], 0.5f), "SetDepthBoneConstraint should apply rotation limits");
    require(near(bone.maxStepRadians, 0.1f), "SetDepthBoneConstraint should apply max step");

    incActionUndo();
    require(bone.constraintType == "", "undo SetDepthBoneConstraint should restore constraint type");
    require(bone.hingeAxis == vec3(0, 0, 1), "undo SetDepthBoneConstraint should restore hinge axis");
    require(!bone.lockRotation && !bone.lockTranslation && bone.allowParentToTargets, "undo SetDepthBoneConstraint should restore boolean flags");
    require(bone.rotationLimits.length == 0, "undo SetDepthBoneConstraint should restore rotation limits");
    require(near(bone.maxStepRadians, 0.0f), "undo SetDepthBoneConstraint should restore max step");

    incActionRedo();
    require(bone.constraintType == "hinge", "redo SetDepthBoneConstraint should restore constraint type");
    require(bone.hingeAxis == vec3(0, 1, 0), "redo SetDepthBoneConstraint should restore hinge axis");
    require(bone.lockRotation && bone.lockTranslation && !bone.allowParentToTargets, "redo SetDepthBoneConstraint should restore boolean flags");
    require(bone.rotationLimits.length == 2 && near(bone.rotationLimits[0], -0.5f) && near(bone.rotationLimits[1], 0.5f), "redo SetDepthBoneConstraint should restore rotation limits");
    require(near(bone.maxStepRadians, 0.1f), "redo SetDepthBoneConstraint should restore max step");
}

private void testDepthBoneSourceCommandsUndoRedo() {
    resetCase();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "depth-root";
    auto target = new GridDeformer(incActivePuppet().root);
    target.name = "target-grid";
    auto bone = new ExDepthBone(root);
    bone.name = "depth-bone";
    bone.boneId = "Bone";
    incActivePuppet().rescanNodes();

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();

    auto addResult = cmd!(DepthBoneCommand.AddDepthBoneSource)(ctx, root, target, bone);
    require(addResult.succeeded, "AddDepthBoneSource command should succeed");
    require(root.bindings.length == 1, "AddDepthBoneSource should create one binding");
    require(root.bindings[0].targetUuid == target.uuid, "AddDepthBoneSource should bind target uuid");
    require(root.bindings[0].sourceBoneUuids == [cast(ulong)bone.uuid], "AddDepthBoneSource should store source bone uuid");

    incActionUndo();
    require(root.bindings.length == 0, "undo AddDepthBoneSource should remove binding");
    incActionRedo();
    require(root.bindings.length == 1 && root.bindings[0].sourceBoneUuids == [cast(ulong)bone.uuid], "redo AddDepthBoneSource should restore binding");

    auto settingsResult = cmd!(DepthBoneCommand.SetDepthBoneSourceSettings)(
        ctx,
        root,
        target,
        bone,
        `{"weight":0.25,"depthOffset":1.5,"depthScale":2.0}`
    );
    require(settingsResult.succeeded, "SetDepthBoneSourceSettings command should succeed");
    auto setting = root.bindings[0].sourceSetting(bone.uuid);
    require(near(setting.weight, 0.25f), "SetDepthBoneSourceSettings should apply weight");
    require(near(setting.depthOffset, 1.5f), "SetDepthBoneSourceSettings should apply depth offset");
    require(near(setting.depthScale, 2.0f), "SetDepthBoneSourceSettings should apply depth scale");

    incActionUndo();
    setting = root.bindings[0].sourceSetting(bone.uuid);
    require(near(setting.weight, 1.0f), "undo SetDepthBoneSourceSettings should restore default weight");
    require(near(setting.depthOffset, 0.0f), "undo SetDepthBoneSourceSettings should restore default depth offset");
    require(near(setting.depthScale, 1.0f), "undo SetDepthBoneSourceSettings should restore default depth scale");

    incActionRedo();
    setting = root.bindings[0].sourceSetting(bone.uuid);
    require(near(setting.weight, 0.25f) && near(setting.depthOffset, 1.5f) && near(setting.depthScale, 2.0f), "redo SetDepthBoneSourceSettings should restore edited settings");

    auto removeResult = cmd!(DepthBoneCommand.RemoveDepthBoneSource)(ctx, root, target, bone);
    require(removeResult.succeeded, "RemoveDepthBoneSource command should succeed");
    require(root.bindings.length == 0, "RemoveDepthBoneSource should remove empty binding");

    incActionUndo();
    require(root.bindings.length == 1 && root.bindings[0].sourceBoneUuids == [cast(ulong)bone.uuid], "undo RemoveDepthBoneSource should restore binding");
    incActionRedo();
    require(root.bindings.length == 0, "redo RemoveDepthBoneSource should remove binding");
}

private void testDepthBonePreviewApplyCommands() {
    resetCase();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "preview-depth-root";
    auto target = new GridDeformer(incActivePuppet().root);
    target.name = "preview-target-grid";
    auto bone = new ExDepthBone(root);
    bone.name = "preview-depth-bone";
    bone.boneId = "PreviewBone";
    bone.restHead = vec3(0, 0, 0);
    bone.restTail = vec3(0, 100, 0);
    incActivePuppet().rescanNodes();

    auto param = new ExParameter("DepthPreviewParam", false);
    param.min = vec2(0, 0);
    param.max = vec2(1, 0);
    param.value = vec2(1, 0);
    incActivePuppet().parameters ~= param;
    auto tx = newValueBinding(param, bone, "transform.t.x");
    tx.setValue(vec2u(1, 0), 5.0f);

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    ctx.armedParameters = [param];

    require(cmd!(DepthBoneCommand.AddDepthBoneSource)(ctx, root, target, bone).succeeded, "preview fixture should add a depth bone source");
    auto listBones = cast(ExCommandResult!JSONValue)cmd!(DepthBoneCommand.ListDepthBones)(ctx, root);
    require(listBones !is null && listBones.succeeded, "ListDepthBones should return a JSON payload");
    require(listBones.result["items"].array.length == 1, "ListDepthBones should list the fixture bone");
    require(listBones.result["items"].array[0]["boneId"].str == "PreviewBone", "ListDepthBones should preserve boneId");

    auto listSources = cast(ExCommandResult!JSONValue)cmd!(DepthBoneCommand.ListDepthBoneSources)(ctx, root, target);
    require(listSources !is null && listSources.succeeded, "ListDepthBoneSources should return a JSON payload");
    require(listSources.result["sourceBoneUuids"].array.length == 1, "ListDepthBoneSources should list the source bone");
    require(listSources.result["sourceBoneUuids"].array[0].toString() == bone.uuid.to!string, "ListDepthBoneSources should return source UUID");

    require(cmd!(DepthBoneCommand.PreviewDepthBoneInfluence)(ctx, root, target, bone).succeeded, "PreviewDepthBoneInfluence should succeed");
    require(target.deformation.length == target.vertices.length, "PreviewDepthBoneInfluence should preserve deformation length");
    foreach (offset; target.deformation)
        require(near(offset.y, 20.0f), "PreviewDepthBoneInfluence should write radius-scaled preview offsets");

    target.deformation[] = vec2(0, 0);
    require(cmd!(DepthBoneCommand.PreviewDepthBoneDeform)(ctx, root, cast(Node[])[target]).succeeded, "PreviewDepthBoneDeform should succeed");
    foreach (offset; target.deformation)
        require(near(offset.x, 5.0f), "PreviewDepthBoneDeform should apply posed bone translation to preview offsets");

    incActionClearHistory();
    auto applyResult = cmd!(DepthBoneCommand.ApplyDepthBoneDeform)(ctx, root, cast(Node[])[target]);
    require(applyResult.succeeded, "ApplyDepthBoneDeform should succeed");
    require(incActionHistory().length == 1, "ApplyDepthBoneDeform should push one grouped undo action");
    auto deformBinding = cast(DeformationParameterBinding)param.getBinding(target, "deform");
    require(deformBinding !is null, "ApplyDepthBoneDeform should create a deform binding");
    auto appliedOffsets = deformBinding.getValue(vec2u(1, 0)).vertexOffsets;
    require(appliedOffsets.length == target.vertices.length, "ApplyDepthBoneDeform should write offsets for every vertex");
    foreach (offset; appliedOffsets)
        require(near(offset.x, 5.0f), "ApplyDepthBoneDeform should store posed bone offsets in the binding");

    incActionUndo();
    require(param.getBinding(target, "deform") is null, "undo ApplyDepthBoneDeform should remove the created deform binding");

    incActionRedo();
    deformBinding = cast(DeformationParameterBinding)param.getBinding(target, "deform");
    require(deformBinding !is null && deformBinding.getValue(vec2u(1, 0)).vertexOffsets.length == target.vertices.length,
        "redo ApplyDepthBoneDeform should restore the created deform binding and offsets");
}

private void testDepthBoneSkinningLockToRootTerminal() {
    resetCase();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "skinning-depth-root";
    auto shin = ngCreateDepthBone(root, "Shin", vec3(0, 0, 0), vec3(0, 100, 0));
    auto foot = ngCreateDepthBone(shin, "Foot", vec3(0, 100, 0), vec3(0, 200, 0));

    auto target = new GridDeformer(incActivePuppet().root);
    target.name = "skinning-target-grid";
    target.rebuffer(Vec2Array([
        vec2(-10, 140),
        vec2(10, 140),
        vec2(-10, 180),
        vec2(10, 180),
    ]));

    ExDepthRigBinding binding;
    binding.targetUuid = target.uuid;
    binding.targetKind = ExDepthTargetKind.Grid;
    binding.sourceBoneUuids = [cast(ulong)shin.uuid, cast(ulong)foot.uuid];
    binding.influenceRule.maxInfluences = 4;
    binding.influenceRule.radiusScale = 1.0f;
    binding.influenceRule.minimumRadius = 1.0f;
    root.bindings = [binding];

    auto param = new ExParameter("DepthSkinningParam", false);
    param.min = vec2(0, 0);
    param.max = vec2(1, 0);
    param.value = vec2(1, 0);
    incActivePuppet().parameters ~= param;
    auto shinTx = newValueBinding(param, shin, "transform.t.x");
    shinTx.setValue(vec2u(1, 0), 30.0f);

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    ctx.armedParameters = [param];

    foot.lockToRoot = false;
    target.deformation[] = vec2(0, 0);
    require(cmd!(DepthBoneCommand.PreviewDepthBoneDeform)(ctx, root, cast(Node[])[target]).succeeded,
        "skin fixture should preview with unlocked terminal bone");
    foreach (offset; target.deformation)
        require(near(offset.x, 30.0f), "unlocked terminal bone should inherit parent translation beyond the terminal segment");

    foot.lockToRoot = true;
    target.deformation[] = vec2(0, 0);
    require(cmd!(DepthBoneCommand.PreviewDepthBoneDeform)(ctx, root, cast(Node[])[target]).succeeded,
        "skin fixture should preview with locked terminal bone");
    foreach (offset; target.deformation)
        require(nearVec2(offset, vec2(0, 0)), "locked terminal bone should keep vertices beyond the terminal bone fixed to root");
}

private ExDepthBone findDepthBoneById(ExDepthRigRoot root, string boneId) {
    foreach (bone; root.depthBones()) {
        if (bone.boneId == boneId)
            return bone;
    }
    return null;
}

private void testDepthBoneStandardSkeletonTemplate() {
    resetCase();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "template-depth-root";
    ngAddStandardDepthSkeleton(root, 1000.0f);

    auto bones = root.depthBones();
    require(bones.length == 19, "standard DepthBone skeleton should create the expected bone count");
    foreach (boneId; [
        "Pelvis", "Spine", "Chest", "Neck", "Head",
        "Clavicle.L", "UpperArm.L", "Forearm.L", "Hand.L",
        "Clavicle.R", "UpperArm.R", "Forearm.R", "Hand.R",
        "Thigh.L", "Shin.L", "Foot.L", "Thigh.R", "Shin.R", "Foot.R",
    ]) {
        auto bone = findDepthBoneById(root, boneId);
        require(bone !is null, "standard DepthBone skeleton should include " ~ boneId);
        require(bone.restHead != bone.restTail, "standard DepthBone should have non-zero rest segment: " ~ boneId);
    }

    auto head = findDepthBoneById(root, "Head");
    require(head !is null && !head.allowParentToTargets, "Head should disable allowParentToTargets in the standard skeleton");
    auto footL = findDepthBoneById(root, "Foot.L");
    auto footR = findDepthBoneById(root, "Foot.R");
    require(footL !is null && footR !is null, "standard DepthBone skeleton should include both feet");
    require(footL.lockToRoot && footR.lockToRoot, "standard feet should be lockToRoot by default");
}

private void testDepthBoneStandardParameterTemplate() {
    resetCase();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "template-depth-param-root";
    ngAddStandardDepthSkeleton(root, 1000.0f);

    auto command = new AddStandardDepthParametersCommand();
    command.root = root;
    require(command.run(new Context()).succeeded, "AddStandardDepthParametersCommand should create templates");

    auto faceYaw = findParameter(incActivePuppet(), "Face::Yaw-Pitch");
    auto faceRoll = findParameter(incActivePuppet(), "Face::Roll");
    auto bodyYaw = findParameter(incActivePuppet(), "Body::Yaw-Pitch");
    auto bodyRoll = findParameter(incActivePuppet(), "Body::Roll");
    require(faceYaw !is null && faceYaw.isVec2, "Face::Yaw-Pitch should be a 2D parameter");
    require(faceRoll !is null && !faceRoll.isVec2, "Face::Roll should be a 1D parameter");
    require(bodyYaw !is null && bodyYaw.isVec2, "Body::Yaw-Pitch should be a 2D parameter");
    require(bodyRoll !is null && !bodyRoll.isVec2, "Body::Roll should be a 1D parameter");
    require(faceYaw.axisPoints[0].length == 5 && faceYaw.axisPoints[1].length == 5, "Face::Yaw-Pitch should use a 5x5 keypoint grid");
    require(bodyYaw.axisPoints[0].length == 5 && bodyYaw.axisPoints[1].length == 5, "Body::Yaw-Pitch should use a 5x5 keypoint grid");
    require(faceRoll.axisPoints[0].length == 5 && faceRoll.axisPoints[1].length == 1, "Face::Roll should use a 5-key 1D grid");
    require(bodyRoll.axisPoints[0].length == 5 && bodyRoll.axisPoints[1].length == 1, "Body::Roll should use a 5-key 1D grid");

    auto head = findDepthBoneById(root, "Head");
    auto headYaw = cast(ValueParameterBinding)faceYaw.getBinding(head, "transform.r.y");
    require(headYaw !is null, "Face::Yaw-Pitch should bind Head transform.r.y");
    require(near(headYaw.getValue(vec2u(0, 2)), -0.5235988f), "Head yaw left key should match template");
    require(near(headYaw.getValue(vec2u(2, 2)), 0.0f), "Head yaw origin key should match template");
    require(near(headYaw.getValue(vec2u(4, 2)), 0.5235988f), "Head yaw right key should match template");

    auto pelvis = findDepthBoneById(root, "Pelvis");
    auto pelvisX = cast(ValueParameterBinding)bodyRoll.getBinding(pelvis, "transform.t.x");
    require(pelvisX !is null, "Body::Roll should bind Pelvis transform.t.x");
    require(near(pelvisX.getValue(vec2u(0, 0)), 30.0f), "Body::Roll left pelvis X should match template");
    require(near(pelvisX.getValue(vec2u(2, 0)), 0.0f), "Body::Roll origin pelvis X should match template");
    require(near(pelvisX.getValue(vec2u(4, 0)), -30.0f), "Body::Roll right pelvis X should match template");

    incActionUndo();
    require(findParameter(incActivePuppet(), "Face::Yaw-Pitch") is null, "undo standard depth parameters should remove created parameters");

    incActionRedo();
    require(findParameter(incActivePuppet(), "Face::Yaw-Pitch") !is null, "redo standard depth parameters should restore created parameters");
}

private void testDepthBoneInfluenceRuleCommandUndoRedo() {
    resetCase();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "influence-root";
    auto target = new GridDeformer(incActivePuppet().root);
    target.name = "influence-target";

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    auto result = cmd!(DepthBoneCommand.SetDepthBoneInfluenceRule)(
        ctx,
        root,
        target,
        `{"maxInfluences":2,"radiusScale":1.5,"minimumRadius":12.0,"falloff":"linear"}`
    );
    require(result.succeeded, "SetDepthBoneInfluenceRule command should succeed");
    require(root.bindings.length == 1, "SetDepthBoneInfluenceRule should create binding for target");
    require(root.bindings[0].influenceRule.maxInfluences == 2, "SetDepthBoneInfluenceRule should apply maxInfluences");
    require(near(root.bindings[0].influenceRule.radiusScale, 1.5f), "SetDepthBoneInfluenceRule should apply radiusScale");
    require(near(root.bindings[0].influenceRule.minimumRadius, 12.0f), "SetDepthBoneInfluenceRule should apply minimumRadius");
    require(root.bindings[0].influenceRule.falloff == "linear", "SetDepthBoneInfluenceRule should apply falloff");

    auto getResult = cast(ExCommandResult!JSONValue)cmd!(DepthBoneCommand.GetDepthBoneInfluenceRule)(ctx, root, target);
    require(getResult !is null, "GetDepthBoneInfluenceRule should return JSON payload");
    require(getResult.succeeded, "GetDepthBoneInfluenceRule command should succeed");
    require(getResult.result["maxInfluences"].toString() == "2", "GetDepthBoneInfluenceRule should return maxInfluences");

    incActionUndo();
    require(root.bindings.length == 0, "undo SetDepthBoneInfluenceRule should restore prior binding list");

    incActionRedo();
    require(root.bindings.length == 1 && root.bindings[0].influenceRule.maxInfluences == 2, "redo SetDepthBoneInfluenceRule should restore rule");
}

private void testDepthBoneSerializationRoundTrip() {
    resetCase();
    ensureRegressionNodeTypesRegistered();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "serialized-depth-root";
    auto target = new GridDeformer(incActivePuppet().root);
    target.name = "serialized-depth-target";
    auto bone = ngCreateDepthBone(root, "SerializedBone", vec3(1, 2, 3), vec3(4, 5, 6), 0.25f);
    bone.name = "serialized-depth-bone";
    bone.constraintType = "hinge";
    bone.hingeAxis = vec3(0, 1, 0);
    bone.lockRotation = true;
    bone.lockTranslation = true;
    bone.allowParentToTargets = false;
    bone.rotationLimits = [-0.75f, 0.5f];
    bone.maxStepRadians = 0.125f;

    root.addBoneSource(target, ExDepthTargetKind.Grid, bone);
    root.bindings[0].influenceRule.maxInfluences = 2;
    root.bindings[0].influenceRule.radiusScale = 1.5f;
    root.bindings[0].influenceRule.minimumRadius = 12.0f;
    root.bindings[0].influenceRule.falloff = "linear";
    ExDepthBoneSourceSettings setting;
    setting.boneUuid = bone.uuid;
    setting.weight = 0.25f;
    setting.depthOffset = 1.5f;
    setting.depthScale = 2.0f;
    root.bindings[0].setSourceSetting(setting);

    incActivePuppet().root.build();
    auto fixtureDir = buildPath(tempDir(), "nijigenerate-regression-depthbone");
    mkdirRecurse(fixtureDir);
    auto saveBase = buildPath(fixtureDir, "depthbone-roundtrip");
    auto savePath = saveBase ~ ".inx";
    require((new SaveFileCommand(saveBase)).run(new Context()).succeeded, "SaveFileCommand should save DepthBone fixture");
    require(exists(savePath) && isFile(savePath), "DepthBone INX fixture should exist");

    ensureRegressionNodeTypesRegistered();
    auto loadedPuppet = inLoadPuppet!ExPuppet(savePath);
    auto loadedRoot = cast(ExDepthRigRoot)findNodeRecursive(loadedPuppet.root, "serialized-depth-root");
    auto loadedTarget = cast(GridDeformer)findNodeRecursive(loadedPuppet.root, "serialized-depth-target");
    auto loadedBone = cast(ExDepthBone)findNodeRecursive(loadedPuppet.root, "serialized-depth-bone");
    require(loadedRoot !is null, "DepthRigRoot should load from INX; tree:\n" ~ nodeTreeSummary(loadedPuppet.root));
    require(loadedTarget !is null, "DepthBone target should load from INX");
    require(loadedBone !is null, "DepthBone node should load from INX");
    require(loadedBone.boneId == "SerializedBone", "DepthBone boneId should round-trip");
    require(loadedBone.restHead == vec3(1, 2, 3) && loadedBone.restTail == vec3(4, 5, 6), "DepthBone rest segment should round-trip");
    require(near(loadedBone.restRoll, 0.25f), "DepthBone restRoll should round-trip");
    require(loadedBone.constraintType == "hinge" && loadedBone.hingeAxis == vec3(0, 1, 0), "DepthBone constraint should round-trip");
    require(loadedBone.lockRotation && loadedBone.lockTranslation && !loadedBone.allowParentToTargets, "DepthBone constraint booleans should round-trip");
    require(loadedBone.rotationLimits.length == 2 && near(loadedBone.rotationLimits[0], -0.75f) && near(loadedBone.rotationLimits[1], 0.5f), "DepthBone rotation limits should round-trip");
    require(near(loadedBone.maxStepRadians, 0.125f), "DepthBone maxStepRadians should round-trip");

    require(loadedRoot.bindings.length == 1, "DepthRigRoot bindings should round-trip");
    auto loadedBinding = loadedRoot.bindings[0];
    require(loadedBinding.targetUuid == loadedTarget.uuid, "DepthRigBinding target uuid should round-trip");
    require(loadedBinding.sourceBoneUuids == [cast(ulong)loadedBone.uuid], "DepthRigBinding source bone uuid should round-trip");
    auto loadedSetting = loadedBinding.sourceSetting(loadedBone.uuid);
    require(near(loadedSetting.weight, 0.25f) && near(loadedSetting.depthOffset, 1.5f) && near(loadedSetting.depthScale, 2.0f), "DepthBone source settings should round-trip");
    require(loadedBinding.influenceRule.maxInfluences == 2, "DepthBone influence maxInfluences should round-trip");
    require(near(loadedBinding.influenceRule.radiusScale, 1.5f), "DepthBone influence radiusScale should round-trip");
    require(near(loadedBinding.influenceRule.minimumRadius, 12.0f), "DepthBone influence minimumRadius should round-trip");
    require(loadedBinding.influenceRule.falloff == "linear", "DepthBone influence falloff should round-trip");
}

private void testDepthBoneDeleteCleanupUndoRedo() {
    resetCase();

    auto root = new ExDepthRigRoot(incActivePuppet().root);
    root.name = "depth-root";
    auto target = new GridDeformer(incActivePuppet().root);
    target.name = "target-grid";
    auto boneA = new ExDepthBone(root);
    boneA.name = "bone-a";
    boneA.boneId = "Bone.A";
    auto boneB = new ExDepthBone(root);
    boneB.name = "bone-b";
    boneB.boneId = "Bone.B";
    incActivePuppet().rescanNodes();

    auto ctx = new Context();
    ctx.puppet = incActivePuppet();
    require(cmd!(DepthBoneCommand.AddDepthBoneSource)(ctx, root, target, boneA).succeeded, "adding first source should succeed");
    require(cmd!(DepthBoneCommand.AddDepthBoneSource)(ctx, root, target, boneB).succeeded, "adding second source should succeed");
    require(root.bindings.length == 1 && root.bindings[0].sourceBoneUuids.length == 2, "delete cleanup fixture should have two sources");

    ctx.nodes = [cast(Node)boneA];
    require((new DeleteNodeCommand()).run(ctx).succeeded, "DeleteNodeCommand should delete depth bone");
    require(!isChildOf(root, boneA), "DeleteNodeCommand should detach deleted depth bone");
    require(root.bindings.length == 1, "DeleteNodeCommand should keep binding when another source remains");
    require(root.bindings[0].sourceBoneUuids == [cast(ulong)boneB.uuid], "DeleteNodeCommand should remove only deleted bone source");

    incActionUndo();
    require(isChildOf(root, boneA), "undo DeleteNodeCommand should restore deleted depth bone");
    require(root.bindings.length == 1 && root.bindings[0].sourceBoneUuids.length == 2, "undo DeleteNodeCommand should restore removed depth bone source");

    incActionRedo();
    require(!isChildOf(root, boneA), "redo DeleteNodeCommand should detach depth bone again");
    require(root.bindings.length == 1 && root.bindings[0].sourceBoneUuids == [cast(ulong)boneB.uuid], "redo DeleteNodeCommand should clean stale source again");
}

private void testActionGroupUndoRedo() {
    resetCase();

    auto nodeA = new Node(incActivePuppet().root);
    auto nodeB = new Node(incActivePuppet().root);
    nodeA.name = "A";
    nodeB.name = "B";

    incActionPushGroup();
    nodeA.name = "A1";
    incActionPush(new NodeValueChangeAction!(Node, string)("name", nodeA, "A", nodeA.name, &nodeA.name_));
    nodeB.name = "B1";
    incActionPush(new NodeValueChangeAction!(Node, string)("name", nodeB, "B", nodeB.name, &nodeB.name_));
    incActionPopGroup();

    require(incActionHistory().length == 1, "grouped edits should create one undo entry");
    incActionUndo();
    require(nodeA.name == "A" && nodeB.name == "B", "group undo should restore all grouped edits");
    incActionRedo();
    require(nodeA.name == "A1" && nodeB.name == "B1", "group redo should restore all grouped edits");
}

private void testActionHistoryIndexAndModifiedState() {
    resetCase();

    auto node = new Node(incActivePuppet().root);
    node.name = "A";

    incActionMarkSaved();
    require(!incActionIsModified(), "empty history marked saved should not be modified");

    node.name = "B";
    incActionPush(new NodeValueChangeAction!(Node, string)("name", node, "A", node.name, &node.name_));
    require(incActionHistory().length == 1, "first action should enter history");
    require(incActionIndex() == 1, "action index should point after first action");
    require(incActionIsModified(), "pushed action should mark project modified");

    node.name = "C";
    incActionPush(new NodeValueChangeAction!(Node, string)("sort", node, "B", node.name, &node.name_));
    require(incActionHistory().length == 2, "second non-merged action should enter history");
    require(incActionIndex() == 2, "action index should point after second action");

    incActionSetIndex(0);
    require(node.name == "A", "setting action index to zero should undo all actions");
    require(incActionIndex() == 0, "action index should be zero after full undo");
    require(!incActionIsModified(), "returning to saved index should clear modified state");

    incActionSetIndex(1);
    require(node.name == "B", "setting action index to one should replay first action only");
    require(incActionIndex() == 1, "action index should be one after partial replay");
    require(incActionIsModified(), "partial replay past saved state should be modified");

    incActionSetIndex(2);
    require(node.name == "C", "setting action index to history end should replay all actions");
    require(incActionIndex() == 2, "action index should be at history end");

    incActionMarkSaved();
    require(!incActionIsModified(), "marking current index saved should clear modified state");
    incActionUndo();
    require(incActionIsModified(), "undoing away from saved index should mark modified");

    incActionClearHistory();
    require(incActionHistory().length == 0, "clear history should remove actions");
    require(incActionIndex() == 0, "clear history should reset action index");
    require(!incActionIsModified(), "clear history should reset saved state");
}

private void testActionMergeSemantics() {
    resetCase();

    auto node = new Node(incActivePuppet().root);
    node.name = "A";
    incActionMarkSaved();

    node.name = "B";
    incActionPush(new NodeValueChangeAction!(Node, string)("name", node, "A", node.name, &node.name_));
    node.name = "C";
    incActionPush(new NodeValueChangeAction!(Node, string)("name", node, "B", node.name, &node.name_));
    require(incActionHistory().length == 1, "same node property edits should merge into one undo entry");
    require(incActionIndex() == 1, "merged edit should keep one active history entry");

    incActionUndo();
    require(node.name == "A", "undo merged edit should restore the original value");
    require(!incActionIsModified(), "undo merged edit back to saved state should clear modified flag");

    incActionRedo();
    require(node.name == "C", "redo merged edit should restore the final merged value");

    node.name = "D";
    incActionPush(new NodeValueChangeAction!(Node, string)("sort", node, "C", node.name, &node.name_));
    require(incActionHistory().length == 2, "different action names should split undo entries");

    incActionUndo();
    require(node.name == "C", "undo split edit should restore previous value");
    node.name = "E";
    incActionPush(new NodeValueChangeAction!(Node, string)("visibility", node, "C", node.name, &node.name_));
    require(incActionHistory().length == 2 && incActionIndex() == 2,
        "pushing after undo should truncate redo history and append the new action");
    incActionRedo();
    require(node.name == "E", "redo after truncation should not restore the discarded action");
}

private void testDefineGridCommandUndoRedo() {
    resetCase();

    auto grid = new GridDeformer(incActivePuppet().root);
    grid.name = "grid";
    auto oldVertices = grid.vertices.dup;
    require(oldVertices.length == 4, "GridDeformer should start with default 2x2 grid");

    auto ctx = new Context();
    ctx.nodes = [cast(Node)grid];

    require((new DefineGridCommand([0f, 10f, 20f], [0f, 5f, 10f])).run(ctx).succeeded, "DefineGridCommand should succeed");
    require(grid.vertices.length == 9, "DefineGridCommand should apply 3x3 grid vertices");
    require(grid.deformation.length == 9, "DefineGridCommand should resize deformation with vertices");

    incActionUndo();
    require(grid.vertices.length == oldVertices.length, "undo DefineGridCommand should restore previous vertex count");
    require(grid.vertices == oldVertices, "undo DefineGridCommand should restore previous vertices");

    incActionRedo();
    require(grid.vertices.length == 9, "redo DefineGridCommand should restore 3x3 grid vertices");
    require(grid.vertices == Vec2Array([vec2(0, 0), vec2(10, 0), vec2(20, 0), vec2(0, 5), vec2(10, 5), vec2(20, 5), vec2(0, 10), vec2(10, 10), vec2(20, 10)]),
        "redo DefineGridCommand should restore 3x3 grid positions");

    auto param = new ExParameter("GridDeformParam", true);
    incActivePuppet().parameters ~= param;
    auto binding = cast(DeformationParameterBinding)param.getOrAddBinding(grid, "deform");
    foreach (x; 0 .. binding.values.length) {
        foreach (y; 0 .. binding.values[x].length) {
            binding.values[x][y].vertexOffsets.length = grid.vertices.length;
            binding.values[x][y].vertexOffsets[] = vec2(1, 1);
            binding.isSet_[x][y] = true;
        }
    }

    require((new DefineGridCommand([-0.5f, 0.5f], [-0.5f, 0.5f])).run(ctx).succeeded, "DefineGridCommand should shrink to 2x2 grid");
    require(grid.vertices.length == 4, "DefineGridCommand should apply 2x2 grid vertices");
    foreach (x; 0 .. binding.values.length)
        foreach (y; 0 .. binding.values[x].length)
            require(binding.values[x][y].vertexOffsets.length == grid.vertices.length, "DefineGridCommand should resize deformation bindings after topology change");
    param.update();

    incActionUndo();
    require(grid.vertices.length == 9, "undo shrinking DefineGridCommand should restore 3x3 grid vertices");
    require(grid.vertices == Vec2Array([vec2(0, 0), vec2(10, 0), vec2(20, 0), vec2(0, 5), vec2(10, 5), vec2(20, 5), vec2(0, 10), vec2(10, 10), vec2(20, 10)]),
        "undo shrinking DefineGridCommand should restore 3x3 grid positions");
    foreach (x; 0 .. binding.values.length)
        foreach (y; 0 .. binding.values[x].length)
            require(binding.values[x][y].vertexOffsets.length == grid.vertices.length, "undo DefineGridCommand should resize deformation bindings after topology restore");
    param.update();

    float[] elevenAxis = [0f, 1f, 2f, 3f, 4f, 5f, 6f, 7f, 8f, 9f, 10f];
    require((new DefineGridCommand(elevenAxis, elevenAxis)).run(ctx).succeeded, "DefineGridCommand should apply 11x11 point grid");
    auto elevenVertices = grid.vertices.dup;
    require(elevenVertices.length == 121, "11x11 point grid should have 121 vertices");

    float[] fiveCellAxis = [0f, 2f, 4f, 6f, 8f, 10f];
    require((new DefineGridCommand(fiveCellAxis, fiveCellAxis)).run(ctx).succeeded, "DefineGridCommand should apply 5x5 cell grid");
    require(grid.vertices.length == 36, "5x5 cell grid should have 6x6 points");

    incActionUndo();
    require(grid.vertices.length == 121, "undo 5x5 cell DefineGridCommand should restore 11x11 point grid");
    require(grid.vertices == elevenVertices, "undo 5x5 cell DefineGridCommand should restore exact 11x11 grid positions");

    incActionRedo();
    require(grid.vertices.length == 36, "redo 5x5 cell DefineGridCommand should restore 6x6 point grid");

    auto scopedVertices = grid.vertices.dup;
    incSetEditMode(EditMode.VertexEdit, false);
    auto vertexScope = ngOpenActionStackScope(ActionStackScopeUnit.VertexEdit);
    require(ngActionStackScopeActive(ActionStackScopeUnit.VertexEdit), "test setup should open VertexEdit action scope");
    require((new DefineGridCommand([0f, 5f, 10f], [0f, 5f, 10f])).run(ctx).succeeded,
        "DefineGridCommand should apply while VertexEdit action scope is open");
    require(!ngActionStackScopeActive(ActionStackScopeUnit.VertexEdit), "DefineGridCommand should close VertexEdit action scope before applying");
    require(incEditMode() == EditMode.ModelEdit, "DefineGridCommand should leave VertexEdit mode after command apply");
    require(ngActionStackLevel() == 0, "DefineGridCommand action should be recorded on root action stack");
    require(grid.vertices.length == 9, "DefineGridCommand in VertexEdit scope should apply 3x3 point grid");

    incActionUndo();
    require(grid.vertices == scopedVertices, "undo DefineGridCommand from VertexEdit scope should restore previous grid");
    vertexScope.close();
}

private void testDefineMeshAndVerticesCommandsUndoRedo() {
    resetCase();

    auto part = newMeshPart("define-mesh-part");
    auto oldPartVertices = part.getMesh().vertices.dup;
    auto oldPartIndices = part.getMesh().indices.dup;

    auto ctx = new Context();
    ctx.nodes = [cast(Node)part];
    require((new DefineMeshCommand(
        [
            0f, 0f,
            20f, 0f,
            20f, 20f,
            0f, 20f,
        ],
        cast(ushort[])[0, 1, 2, 0, 2, 3]
    )).run(ctx).succeeded, "DefineMeshCommand should succeed for Part");
    require(part.getMesh().vertices.length == 4, "DefineMeshCommand should replace Part vertices");
    require(part.getMesh().uvs.length == part.getMesh().vertices.length, "DefineMeshCommand should keep Part UV count coherent");
    require(part.getMesh().indices.length == 6, "DefineMeshCommand should replace Part triangle indices");
    require(part.deformation.length == part.getMesh().vertices.length, "DefineMeshCommand should keep Part deformation count coherent");

    incActionUndo();
    require(
        nearVec2Array(part.getMesh().vertices, oldPartVertices),
        "undo DefineMeshCommand should restore old Part vertices actual=%s expected=%s".format(part.getMesh().vertices.length, oldPartVertices.length)
    );
    require(
        part.getMesh().indices == oldPartIndices,
        "undo DefineMeshCommand should restore old Part indices actual=%s expected=%s".format(part.getMesh().indices.length, oldPartIndices.length)
    );

    incActionRedo();
    require(part.getMesh().vertices.length == 4, "redo DefineMeshCommand should restore new Part vertices");
    require(part.getMesh().uvs.length == part.getMesh().vertices.length, "redo DefineMeshCommand should restore coherent UV count");
    require(part.getMesh().indices.length == 6, "redo DefineMeshCommand should restore new Part indices");
    require(part.deformation.length == part.getMesh().vertices.length, "redo DefineMeshCommand should restore coherent deformation count");

    incActionClearHistory();

    auto path = new PathDeformer(incActivePuppet().root);
    path.name = "define-vertices-path";
    auto oldPathVertices = path.vertices.dup;
    ctx.nodes = [cast(Node)path];
    require((new DefineVerticesCommand([
        -10f, 0f,
        0f, 10f,
        10f, 0f,
    ])).run(ctx).succeeded, "DefineVerticesCommand should succeed for PathDeformer");
    require(path.vertices.length == 3, "DefineVerticesCommand should replace PathDeformer vertices");

    incActionUndo();
    require(path.vertices == oldPathVertices, "undo DefineVerticesCommand should restore old PathDeformer vertices");

    incActionRedo();
    require(path.vertices.length == 3, "redo DefineVerticesCommand should restore new PathDeformer vertices");
}

private AutoMeshProcessor findAutoMeshProcessor(string id) {
    foreach (processor; ngAutoMeshProcessors()) {
        if (processor.procId() == id)
            return processor;
    }
    return null;
}

private Part newAutoMeshAlphaPart(string name) {
    enum int width = 48;
    enum int height = 48;
    ubyte[] pixels;
    pixels.length = width * height * 4;

    foreach (y; 0 .. height) {
        foreach (x; 0 .. width) {
            auto offset = (y * width + x) * 4;
            pixels[offset + 0] = 255;
            pixels[offset + 1] = 255;
            pixels[offset + 2] = 255;
            bool insideRect = x >= 10 && x <= 37 && y >= 8 && y <= 39;
            bool insideBar = x >= 22 && x <= 26 && y >= 4 && y <= 43;
            pixels[offset + 3] = (insideRect || insideBar) ? 255 : 0;
        }
    }

    MeshData data;
    data.vertices = Vec2Array([
        vec2(-24, -24),
        vec2(24, -24),
        vec2(24, 24),
        vec2(-24, 24),
    ]);
    data.uvs = Vec2Array([
        vec2(0, 0),
        vec2(1, 0),
        vec2(1, 1),
        vec2(0, 1),
    ]);
    data.indices = [0, 1, 2, 0, 2, 3];

    auto texture = new Texture(pixels, width, height, 4, 4, false, false);
    auto part = new Part(data, [texture], incActivePuppet().root);
    part.name = name;
    return part;
}

private IncMesh runAutoMeshOnAlphaPart(string processorId) {
    import core.thread.fiber : Fiber;

    auto processor = findAutoMeshProcessor(processorId);
    require(processor !is null, "AutoMesh processor should exist: " ~ processorId);
    auto part = newAutoMeshAlphaPart("automesh-" ~ processorId);
    auto mesh = new IncMesh(part.getMesh());
    IncMesh result;
    auto fiber = new Fiber({
        result = processor.autoMesh(part, mesh);
    });
    while (fiber.state != Fiber.State.TERM)
        fiber.call();
    return result;
}

private void requireAutoMeshOutput(string processorId, IncMesh mesh, size_t minVertices) {
    require(mesh !is null, "AutoMesh should return a mesh: " ~ processorId);
    require(mesh.vertices.length >= minVertices, "AutoMesh should create expected vertices for " ~ processorId);
    foreach (vertex; mesh.vertices) {
        require(vertex !is null, "AutoMesh should not create null vertices for " ~ processorId);
        require(vertex.position.x == vertex.position.x && vertex.position.y == vertex.position.y, "AutoMesh should not create NaN vertices for " ~ processorId);
    }
}

private void testAutoMeshGridProcessorDeterministicOutput() {
    resetCase();

    auto processor = findAutoMeshProcessor("grid");
    require(processor !is null, "grid AutoMesh processor should exist");
    auto reflect = cast(IAutoMeshReflect)processor;
    require(reflect !is null, "grid AutoMesh processor should be reflectable");
    reflect.writeValues("Simple", `{"x_segments":2,"y_segments":2,"margin":0}`);

    auto mesh = runAutoMeshOnAlphaPart("grid");
    requireAutoMeshOutput("grid", mesh, 9);
    require(mesh.axes.length == 2, "grid AutoMesh should produce two grid axes");
    require(mesh.axes[0].length == 3 && mesh.axes[1].length == 3, "grid AutoMesh should honor 2x2 segment settings");
}

private void testAutoMeshContourProcessorDeterministicOutput() {
    resetCase();

    auto processor = findAutoMeshProcessor("contour");
    require(processor !is null, "contour AutoMesh processor should exist");
    auto reflect = cast(IAutoMeshReflect)processor;
    require(reflect !is null, "contour AutoMesh processor should be reflectable");
    reflect.writeValues("Simple", `{"sampling_step":12,"mask_threshold":1}`);
    reflect.writeValues("Advanced", `{"min_distance":4,"max_distance":24,"scales":[1.0,0.5,0.0]}`);

    auto mesh = runAutoMeshOnAlphaPart("contour");
    requireAutoMeshOutput("contour", mesh, 3);
}

private void testAutoMeshSkeletonProcessorDeterministicOutput() {
    resetCase();

    auto processor = findAutoMeshProcessor("skeleton");
    require(processor !is null, "skeleton AutoMesh processor should exist");
    auto reflect = cast(IAutoMeshReflect)processor;
    require(reflect !is null, "skeleton AutoMesh processor should be reflectable");
    reflect.writeValues("Simple", `{"mask_threshold":1,"target_point_count":6}`);

    auto mesh = runAutoMeshOnAlphaPart("skeleton");
    requireAutoMeshOutput("skeleton", mesh, 2);
}

private void testAutoMeshOptimumProcessorDeterministicOutput() {
    resetCase();

    auto processor = findAutoMeshProcessor("optimum");
    require(processor !is null, "optimum AutoMesh processor should exist");
    auto reflect = cast(IAutoMeshReflect)processor;
    require(reflect !is null, "optimum AutoMesh processor should be reflectable");
    reflect.writeValues("Simple", `{"scales":[0.5,0.0],"min_distance":4,"mask_threshold":1,"div_per_part":8}`);
    reflect.writeValues("Advanced", `{"large_threshold":50,"length_threshold":20,"ratio_threshold":0.05}`);

    auto mesh = runAutoMeshOnAlphaPart("optimum");
    requireAutoMeshOutput("optimum", mesh, 3);
}

private void testAutoMeshAlphaProviderAndNonPartCachedInput() {
    import nijigenerate.viewport.vertex.automesh.alpha_provider : PartAlphaProvider;
    import nijigenerate.viewport.vertex.automesh.common : AlphaInput, alphaInputFromProviderWithImage, clearAutoMeshAlphaInputCache, setAutoMeshAlphaInputCache;

    resetCase();

    auto part = newAutoMeshAlphaPart("automesh-alpha-provider");
    auto provider = new PartAlphaProvider(part);
    scope(exit) provider.dispose();
    require(provider.width() == 48 && provider.height() == 48, "PartAlphaProvider should expose texture dimensions");
    require(provider.alphaPtr() !is null, "PartAlphaProvider should expose alpha data");

    auto input = alphaInputFromProviderWithImage(provider);
    require(input.w == 48 && input.h == 48 && input.img !is null, "alphaInputFromProviderWithImage should create image-backed input");
    require(input.img.data.canFind(cast(ubyte)255), "alpha image should preserve opaque pixels");

    auto processor = findAutoMeshProcessor("grid");
    require(processor !is null, "grid AutoMesh processor should exist");
    auto grid = new GridDeformer(incActivePuppet().root);
    grid.name = "automesh-grid-target";

    AlphaInput[uint] cache;
    cache[grid.uuid] = input;
    setAutoMeshAlphaInputCache(&cache);
    scope(exit) clearAutoMeshAlphaInputCache(&cache);

    MeshData data;
    auto mesh = new IncMesh(data);
    auto result = processor.autoMesh(grid, mesh);
    requireAutoMeshOutput("grid-cached-non-part", result, 4);
}

private void testAutoMeshBatchConfigUndoRedo() {
    resetCase();

    auto processor = findAutoMeshProcessor("grid");
    require(processor !is null, "grid AutoMesh processor should exist");
    auto reflect = cast(IAutoMeshReflect)processor;
    require(reflect !is null, "grid AutoMesh processor should be reflectable");

    auto oldSimple = reflect.values("Simple");
    auto oldAdvanced = reflect.values("Advanced");
    auto updates = oldSimple.canFind(`"x_segments":4.0`) || oldSimple.canFind(`"x_segments":4`)
        ? `{"x_segments":5,"y_segments":4,"margin":0.3}`
        : `{"x_segments":4,"y_segments":3,"margin":0.2}`;

    auto ctx = new Context();
    auto setResult = (new AutoMeshSetValuesCommand(
        "grid",
        "Simple",
        updates
    )).run(ctx);
    require(setResult.succeeded, "AutoMeshSetValuesCommand should succeed");
    require(incActionHistory().length == 1, "AutoMesh config update should push one undo entry");

    auto newSimple = reflect.values("Simple");
    auto newAdvanced = reflect.values("Advanced");
    require(newSimple != oldSimple, "AutoMesh simple config should change");
    require(newAdvanced != oldAdvanced, "AutoMesh derived advanced config should change with grid segments");

    incActionUndo();
    require(jsonEquivalent(reflect.values("Simple"), oldSimple), "undo AutoMesh config should restore simple values");
    require(jsonEquivalent(reflect.values("Advanced"), oldAdvanced), "undo AutoMesh config should restore derived advanced values");

    incActionRedo();
    require(jsonEquivalent(reflect.values("Simple"), newSimple), "redo AutoMesh config should restore simple values");
    require(jsonEquivalent(reflect.values("Advanced"), newAdvanced), "redo AutoMesh config should restore derived advanced values");
}

private void testAutoMeshSchemaValuesPresetsAndActiveProcessor() {
    resetCase();

    auto processors = ngAutoMeshProcessors();
    require(processors.length >= 4, "AutoMesh processor registry should include standard processors");

    bool sawPresetProcessor;
    foreach (processor; processors) {
        require(processor.procId().length > 0, "AutoMesh processor should expose procId");
        require(processor.displayName().length > 0, "AutoMesh processor should expose displayName");
        require(processor.icon().length > 0, "AutoMesh processor should expose icon");

        auto reflect = cast(IAutoMeshReflect)processor;
        require(reflect !is null, "standard AutoMesh processor should be reflectable: " ~ processor.procId());

        auto schema = reflect.schema();
        require(schema.canFind(`"type"`), "AutoMesh schema should include type: " ~ processor.procId());
        require(schema.canFind(`"Simple"`) && schema.canFind(`"Advanced"`), "AutoMesh schema should include config levels: " ~ processor.procId());
        require(schema.canFind(`"presets"`), "AutoMesh schema should include presets array: " ~ processor.procId());

        auto simple = reflect.values("Simple");
        auto advanced = reflect.values("Advanced");
        require(simple.length > 0, "AutoMesh simple values should serialize: " ~ processor.procId());
        require(advanced.length > 0, "AutoMesh advanced values should serialize: " ~ processor.procId());

        require((new AutoMeshGetSchemaCommand(processor.procId())).run(new Context()).succeeded, "AutoMeshGetSchemaCommand should succeed: " ~ processor.procId());
        require((new AutoMeshGetValuesCommand(processor.procId(), "Simple")).run(new Context()).succeeded, "AutoMeshGetValuesCommand should succeed: " ~ processor.procId());

        if (schema.canFind(`"Normal parts"`)) {
            sawPresetProcessor = true;
            if (schema.canFind(`"Detailed mesh"`)) {
                require(
                    (new AutoMeshSetPresetCommand(processor.procId(), "Detailed mesh")).run(new Context()).succeeded,
                    "AutoMesh preset test should prepare a non-Normal preset state: " ~ processor.procId()
                );
                incActionClearHistory();
            }
            auto beforeSimple = reflect.values("Simple");
            auto beforeAdvanced = reflect.values("Advanced");
            require((new AutoMeshSetPresetCommand(processor.procId(), "Normal parts")).run(new Context()).succeeded, "AutoMeshSetPresetCommand should apply Normal parts: " ~ processor.procId());
            require(incActionHistory().length > 0, "AutoMeshSetPresetCommand should push undoable config action");
            incActionUndo();
            require(jsonEquivalent(reflect.values("Simple"), beforeSimple), "undo AutoMesh preset should restore simple values");
            require(
                jsonEquivalent(reflect.values("Advanced"), beforeAdvanced),
                "undo AutoMesh preset should restore advanced values for %s\nactual=%s\nexpected=%s".format(processor.procId(), reflect.values("Advanced"), beforeAdvanced)
            );
            incActionRedo();
            incActionClearHistory();
        }
    }
    require(sawPresetProcessor, "at least one AutoMesh processor should expose presets");

    auto original = ngActiveAutoMeshProcessor();
    auto target = processors[$ - 1] is original ? processors[0] : processors[$ - 1];
    require((new AutoMeshSetActiveCommand(target.procId())).run(new Context()).succeeded, "AutoMeshSetActiveCommand should succeed");
    require(ngActiveAutoMeshProcessor() is target, "AutoMeshSetActiveCommand should change active processor");
    ngActiveAutoMeshProcessor(original);
}

private void clearAllShortcuts() {
    foreach (entry; ngListShortcuts())
        ngClearShortcutFor(entry.command);
}

private void testShortcutSettingsConflictAndReload() {
    resetCase();
    clearAllShortcuts();

    auto addCommand = nijigenerate.commands.node.node.commands[NodeCommand.AddNode];
    auto deleteCommand = nijigenerate.commands.node.node.commands[NodeCommand.DeleteNode];
    require(addCommand !is null && deleteCommand !is null, "shortcut test requires initialized node commands");

    ngRegisterShortcut("Ctrl+Alt+N", addCommand);
    require(ngShortcutFor(addCommand) == "Ctrl+Alt+N", "shortcut registration should bind command");

    ngRegisterShortcut("Ctrl+Alt+N", deleteCommand);
    require(ngShortcutFor(addCommand).length == 0, "shortcut conflict should clear old command");
    require(ngShortcutFor(deleteCommand) == "Ctrl+Alt+N", "shortcut conflict should bind new command");

    ngSaveShortcutsToSettings();
    auto saved = incSettingsGet!(string[string])("Shortcuts");
    bool foundSavedShortcut = false;
    foreach (_id, shortcut; saved) {
        if (shortcut == "Ctrl+Alt+N")
            foundSavedShortcut = true;
    }
    require(foundSavedShortcut, "shortcut save should persist shortcut string with a loadable command id");

    clearAllShortcuts();
    require(ngShortcutFor(deleteCommand).length == 0, "shortcut clear should remove binding");

    ngLoadShortcutsFromSettings();
    require(ngShortcutFor(deleteCommand) == "Ctrl+Alt+N", "shortcut load should restore command binding from settings");

    clearAllShortcuts();
}

private void testTypedSettingsStore() {
    resetCase();

    incSettingsSet("Language", "ja_JP");
    incSettingsSet("UiScale", 1.25f);
    incSettingsSet("ShowDebugOverlay", true);
    incSettingsSet("MaxUndoHistory", cast(size_t)42);
    incSettingsSet("RecentFiles", ["a.inx", "b.inx"]);

    require(incSettingsGet!string("Language") == "ja_JP", "string setting should round-trip");
    require(near(incSettingsGet!float("UiScale"), 1.25f), "float setting should round-trip");
    require(incSettingsGet!bool("ShowDebugOverlay"), "bool setting should round-trip");
    require(incSettingsGet!size_t("MaxUndoHistory") == 42, "size_t setting should round-trip");
    require(incSettingsGet!(string[])("RecentFiles") == ["a.inx", "b.inx"], "string[] setting should round-trip");
    require(incSettingsCanGet("ViewportZoomMode"), "default settings should be loaded");
}

private void testAiMcpSettingsOptInPersistence() {
    resetCase();

    string[string] servers;
    servers["local"] = "127.0.0.1:39200";
    incSettingsSet("MCP.Enabled", false);
    incSettingsSet("MCP.Servers", servers);
    incSettingsSet("ACP.Enabled", false);
    incSettingsSet("ACP.Command", "codex");
    incSettingsSet("ACP.Workdir", "/tmp/nijigenerate-acp");

    require(!incSettingsGet!bool("MCP.Enabled"), "MCP should remain opt-in false in settings store");
    require(incSettingsGet!(string[string])("MCP.Servers")["local"] == "127.0.0.1:39200", "MCP server map should round-trip");
    require(!incSettingsGet!bool("ACP.Enabled"), "ACP should remain opt-in false in settings store");
    require(incSettingsGet!string("ACP.Command") == "codex", "ACP command should round-trip");
    require(incSettingsGet!string("ACP.Workdir") == "/tmp/nijigenerate-acp", "ACP workdir should round-trip");
}

private void testAcpProtocolTypesAndErrorJson() {
    require(ACP_PROTOCOL_VERSION == 1, "ACP protocol version should remain stable");
    require(JSONRPC_VERSION == "2.0", "ACP should use JSON-RPC 2.0");
    require(ACP_METHOD_INITIALIZE == "initialize", "ACP initialize method constant should be stable");
    require(ACP_METHOD_PING == "ping", "ACP ping method constant should be stable");

    auto err = new ACPError(cast(int)ErrorCode.invalidParams, "bad params", "missing field");
    auto json = err.toJSON();
    require(json.type == JSONType.object, "ACP error should serialize to an object");
    require(json["jsonrpc"].str == JSONRPC_VERSION, "ACP error should include jsonrpc version");
    require(json["error"]["code"].integer == cast(int)ErrorCode.invalidParams, "ACP error should include numeric code");
    require(json["error"]["message"].str == "bad params", "ACP error should include message");
    require(json["error"]["data"].str == "missing field", "ACP error should include optional details");

    Document doc;
    doc.uri = "file:///tmp/model.inx";
    doc.languageId = "json";
    doc.text = "{}";
    doc.version_ = 7;
    require(doc.version_ == 7 && doc.uri.endsWith("model.inx"), "ACP document payload should store uri and version");

    TextEdit edit;
    edit.range.start = Position(1, 2);
    edit.range.end = Position(3, 4);
    edit.newText = "replacement";
    WorkspaceEdit workspaceEdit;
    workspaceEdit.uri = doc.uri;
    workspaceEdit.edits = [edit];
    require(workspaceEdit.edits.length == 1, "ACP workspace edit should store text edits");
    require(workspaceEdit.edits[0].range.start.line == 1, "ACP range start should be preserved");

    StatusNotification status;
    status.title = "Running";
    status.message = "Testing";
    status.level = StatusLevel.progress;
    require(status.level == StatusLevel.progress, "ACP status notification level should be preserved");
}

private void testAcpClientSourceContract() {
    auto clientSource = readText(buildPath(regressionSourceRoot("api"), "acp", "client.d"));
    require(clientSource.canFind("class ACPClient"), "ACP client should expose ACPClient");
    require(clientSource.canFind("pipeProcess") && clientSource.canFind("Redirect.all") &&
            clientSource.canFind("pipes.stdout") && clientSource.canFind("pipes.stdin") &&
            clientSource.canFind("pipes.stderr"),
        "ACP client should launch child process with stdin/stdout/stderr pipes");
    require(clientSource.canFind("startReader()") && clientSource.canFind("startStderrReader()"),
        "ACP client should start stdout and stderr reader threads");
    require(clientSource.canFind("setCancelCheck") && clientSource.canFind("cancelCheck()"),
        "ACP client should support external cancellation checks while waiting");
    require(clientSource.canFind("session/cancel") && clientSource.canFind("cancelPrompt"),
        "ACP client should send session/cancel notifications");
    require(clientSource.canFind("sendPermissionResponse") &&
            clientSource.canFind(`"outcome":"cancelled"`) &&
            clientSource.canFind(`"optionId":"`),
        "ACP client should encode permission grant and cancellation responses");
    require(clientSource.canFind("initializeAsync") && clientSource.canFind("pollInitialize") &&
            clientSource.canFind("PollResult"),
        "ACP client should expose non-blocking initialize polling");
    require(clientSource.canFind("void close()") && clientSource.canFind("stderrThread.join") &&
            clientSource.canFind("reader.join"),
        "ACP client shutdown should stop and join reader threads");
    require(clientSource.canFind("drainStderr") && clientSource.canFind(" | stderr: "),
        "ACP client failures should include captured stderr context");
}

private void testMcpResourceListingAndContextHelpers() {
    resetCase();

    auto puppet = incActivePuppet();
    auto node = new Node(puppet.root);
    node.name = "MCPNode";
    auto child = new Node(node);
    child.name = "MCPChild";

    auto param = new ExParameter("MCPParam", false);
    puppet.parameters ~= param;
    auto binding = newValueBinding(param, child, "transform.t.x");
    binding.setValue(vec2u(1, 0), 12.5f);

    auto resources = buildCurrentResourceList();
    require(resources.type == JSONType.object, "MCP resources should return an object");
    require(resources["resources"].type == JSONType.array, "MCP resources should contain resources array");

    bool hasRootGuide;
    bool hasNode;
    bool hasParam;
    bool hasBinding;
    foreach (entry; resources["resources"].array) {
        auto uri = entry["uri"].str;
        auto name = entry["name"].str;
        if (uri == "resource://nijigenerate/resources/find?selector=*") hasRootGuide = true;
        if (uri.endsWith("/" ~ node.uuid.to!string) && name == "MCPNode") hasNode = true;
        if (uri.endsWith("/" ~ param.uuid.to!string) && name == "MCPParam") hasParam = true;
        if (uri.startsWith("resource://nijigenerate/bindings/get?") &&
            uri.canFind("parameter=" ~ param.uuid.to!string) &&
            uri.canFind("target=" ~ child.uuid.to!string) &&
            name.canFind("MCPParam -> MCPChild.transform.t.x"))
            hasBinding = true;
    }
    require(hasRootGuide, "MCP resource listing should include selector guide entry");
    require(hasNode, "MCP resource listing should include model nodes");
    require(hasParam, "MCP resource listing should include parameters");
    require(hasBinding, "MCP resource listing should include parameter bindings");

    JSONValue[string] responseObj;
    JSONValue[string] resultObj;
    resultObj["resources"] = JSONValue.emptyArray;
    responseObj["result"] = JSONValue(resultObj);
    auto response = JSONValue(responseObj);
    JSONValue[string] requestObj;
    requestObj["method"] = JSONValue("resources/list");
    auto request = JSONValue(requestObj);
    rewriteResourcesListResponse(response, request);
    require(response["result"]["resources"].array.length >= resources["resources"].array.length,
        "resources/list response should be rewritten with current resource list");

    auto resultJson = commandResultToJsonRuntime(new CommandResult(true, "done"));
    require(resultJson["status"].str == "ok", "MCP command result JSON should report ok status");
    require(resultJson["succeeded"].type == JSONType.true_, "MCP command result JSON should preserve success flag");
    require(resultJson["message"].str == "done", "MCP command result JSON should preserve messages");

    JSONValue[] nodeIds = [JSONValue(cast(long)node.uuid)];
    JSONValue[] paramIds = [JSONValue(cast(long)param.uuid)];
    JSONValue[] paramValue = [JSONValue(0.25), JSONValue(-0.5)];
    JSONValue[string] bindingDesc;
    bindingDesc["target"] = JSONValue(cast(long)child.uuid);
    bindingDesc["name"] = JSONValue("transform.t.x");
    JSONValue[] bindings = [JSONValue(bindingDesc)];
    JSONValue[string] contextObj;
    contextObj["nodes"] = JSONValue(nodeIds);
    contextObj["parameters"] = JSONValue(paramIds);
    contextObj["parameterValue"] = JSONValue(paramValue);
    contextObj["bindings"] = JSONValue(bindings);
    JSONValue[string] payloadObj;
    payloadObj["context"] = JSONValue(contextObj);
    auto ctx = buildContextFromPayload(JSONValue(payloadObj));

    require(ctx.hasNodes && ctx.nodes.length == 1 && ctx.nodes[0] is node,
        "MCP context payload should select explicit nodes");
    require(ctx.hasParameters && ctx.parameters.length == 1 && ctx.parameters[0] is param,
        "MCP context payload should select explicit parameters");
    require(ctx.hasActiveBindings && ctx.activeBindings.length == 1 && ctx.activeBindings[0] is binding,
        "MCP context payload should resolve binding descriptors");
    require(near(ctx.parameterValue.x, 0.25f) && near(ctx.parameterValue.y, -0.5f),
        "MCP context payload should parse parameterValue as parameter-axis values");
    require(!ctx.hasExplicitKeyPoint, "MCP context payload should avoid stale keypoint context when parameterValue is used");
}

private void testMcpTaskQueueMainThreadDispatch() {
    import core.thread : Thread;
    import core.time : msecs;

    ngMcpInitTask();
    int value;
    ngMcpEnqueueAction({ value = 7; });
    ngMcpProcessQueue();
    require(value == 7, "MCP task queue should execute queued actions");

    __gshared bool done;
    __gshared bool thrown;
    __gshared int result;
    done = false;
    thrown = false;
    result = 0;

    auto worker = new Thread({
        try {
            result = ngRunInMainThread!int({
                value = 11;
                return 42;
            });
        } catch (Exception) {
            thrown = true;
        }
        done = true;
    });
    worker.start();
    foreach (_; 0 .. 1000) {
        ngMcpProcessQueue();
        if (done) break;
        Thread.sleep(1.msecs);
    }
    worker.join();
    require(done, "ngRunInMainThread should complete when the main queue is processed");
    require(!thrown, "ngRunInMainThread should propagate successful result without exception");
    require(result == 42 && value == 11, "ngRunInMainThread should run the delegate on the queued main-thread path");

    done = false;
    thrown = false;
    auto failingWorker = new Thread({
        try {
            ngRunInMainThread!void({
                throw new Exception("expected failure");
            });
        } catch (Exception) {
            thrown = true;
        }
        done = true;
    });
    failingWorker.start();
    foreach (_; 0 .. 1000) {
        ngMcpProcessQueue();
        if (done) break;
        Thread.sleep(1.msecs);
    }
    failingWorker.join();
    require(done && thrown, "ngRunInMainThread should propagate queued delegate exceptions");
}

private void testApiTransportAndServerContracts() {
    auto req = ApprovalRequest(
        "req-1",
        "client-a",
        "mcp:use",
        "http://127.0.0.1:8088/mcp",
        "state-a",
        "http://127.0.0.1/callback"
    );
    require(req.reqId == "req-1", "MCP approval request should preserve request id");
    require(req.clientId == "client-a", "MCP approval request should preserve client id");
    require(req.scopeId == "mcp:use", "MCP approval request should preserve scope");
    require(req.redirectUri.endsWith("/callback"), "MCP approval request should preserve redirect uri");

    auto transport = createHttpTransport("127.0.0.1", 0);
    require(transport.authEnabled, "MCP HTTP transport should default to auth enabled");
    transport.authEnabled = false;
    require(!transport.authEnabled, "MCP HTTP transport auth toggle should disable auth");
    transport.authEnabled = true;
    require(transport.authEnabled, "MCP HTTP transport auth toggle should enable auth");

    bool handled;
    transport.setMessageHandler((JSONValue message) {
        handled = message.type == JSONType.object &&
            "method" in message.object &&
            message["method"].str == "ping";
    });
    JSONValue[string] pingObj;
    pingObj["method"] = JSONValue("ping");
    transport.handleMessage(JSONValue(pingObj));
    require(handled, "MCP HTTP transport should dispatch messages to its handler");
    transport.close();

    ngMcpApplySettings(false, "127.0.0.1", 0);
    ngMcpAuthEnabled(true);
    require(!ngMcpAuthEnabled(), "MCP auth getter should stay false when server transport is not running");
    ngMcpStop();

    auto httpSource = readText(buildPath(regressionSourceRoot("api"), "mcp", "http_transport.d"));
    require(httpSource.canFind(`router.post("/mcp"`), "MCP HTTP transport should register POST /mcp");
    require(httpSource.canFind(`router.get ("/events"`), "MCP HTTP transport should register GET /events");
    require(httpSource.canFind(`/.well-known/oauth-protected-resource/mcp`), "MCP HTTP transport should expose protected resource metadata");
    require(httpSource.canFind(`/auth/token`), "MCP HTTP transport should expose token endpoint");
    require(httpSource.canFind("rewriteResourcesListResponse"), "MCP HTTP transport should rewrite resources/list responses");

    auto authSource = readText(buildPath(regressionSourceRoot("api"), "mcp", "auth.d"));
    require(authSource.canFind("approvalTimeout = 120.seconds"), "MCP auth should keep a bounded approval timeout");
    require(authSource.canFind("ngRunInMainThread"), "MCP auth UI should be marshalled to the main thread");
    require(authSource.canFind("Approve") && authSource.canFind("Deny"), "MCP auth UI should expose approve and deny decisions");

    auto stdioSource = readText(buildPath(regressionSourceRoot("api"), "acp", "transport", "stdio.d"));
    require(stdioSource.canFind("public alias StdioTransport"), "ACP stdio adapter should expose StdioTransport alias");
    require(stdioSource.canFind("createStdioTransport"), "ACP stdio adapter should expose transport factory");
    require(stdioSource.canFind("mcp.transport.stdio.createStdioTransport"), "ACP stdio adapter should delegate to mcp stdio transport");

    auto echoSource = readText(buildPath(regressionSourceRoot("api"), "acp", "echo_agent.d"));
    require(echoSource.canFind("version (ACP_ECHO_TOOL)"), "ACP echo agent should stay build-gated");
    require(echoSource.canFind("Content-Length"), "ACP echo agent should support Content-Length framing");
    require(echoSource.canFind(`method == "initialize"`), "ACP echo agent should handle initialize");
    require(echoSource.canFind("ACP_PROTOCOL_VERSION"), "ACP echo agent should report protocol version");
}

private void testDefaultShortcutRegistration() {
    clearAllShortcuts();
    ngRegisterDefaultShortcuts();

    require(ngShortcutFor(nijigenerate.commands.puppet.file.commands[FileCommand.NewFile]) == _K!"Ctrl-N",
        "default shortcut should bind NewFile");
    require(ngShortcutFor(nijigenerate.commands.puppet.file.commands[FileCommand.ShowOpenFileDialog]) == _K!"Ctrl-O",
        "default shortcut should bind open dialog");
    require(ngShortcutFor(nijigenerate.commands.puppet.file.commands[FileCommand.ShowSaveFileDialog]) == _K!"Ctrl-S",
        "default shortcut should bind save dialog");
    require(ngShortcutFor(nijigenerate.commands.puppet.file.commands[FileCommand.ShowSaveFileAsDialog]) == _K!"Ctrl-Shift-S",
        "default shortcut should bind save-as dialog");
    require(ngShortcutFor(nijigenerate.commands.puppet.edit.commands[EditCommand.Undo]) == _K!"Ctrl-Z",
        "default shortcut should bind undo");
    require(ngShortcutFor(nijigenerate.commands.puppet.edit.commands[EditCommand.Redo]) == _K!"Ctrl-Shift-Z",
        "default shortcut should bind redo");
    require(ngShortcutFor(nijigenerate.commands.viewport.palette.commands[PaletteCommand.ListCommand]) == _K!"Ctrl-Shift-P",
        "default shortcut should bind command palette");

    clearAllShortcuts();
}

private void testViewportFlipPairCommands() {
    resetCase();

    auto puppet = incActivePuppet();
    auto left = new Node(puppet.root);
    auto right = new Node(puppet.root);
    auto autoLeft = new Node(puppet.root);
    auto autoRight = new Node(puppet.root);
    left.name = "Hand.L";
    right.name = "Hand.R";
    autoLeft.name = "Foot.L";
    autoRight.name = "Foot.R";

    auto ctx = new Context();
    ctx.puppet = puppet;

    auto add = new AddFlipPairCommand();
    add.left = left;
    add.right = right;
    incActionClearHistory();
    require(add.run(ctx).succeeded, "AddFlipPairCommand should add an explicit pair");
    require(incActionHistory().length == 1, "AddFlipPairCommand should push one undoable action");
    auto listed = (new ListFlipPairsCommand()).run(ctx);
    require(listed.succeeded, "ListFlipPairsCommand should succeed after add");
    auto listJson = listed.result.toString();
    require(listJson.canFind(left.uuid.to!string) && listJson.canFind(right.uuid.to!string),
        "listed flip pairs should include the explicit pair UUIDs");
    incActionUndo();
    require(!(new ListFlipPairsCommand()).run(ctx).result.toString().canFind(left.uuid.to!string),
        "undo AddFlipPairCommand should remove the explicit pair");
    incActionRedo();
    require((new ListFlipPairsCommand()).run(ctx).result.toString().canFind(left.uuid.to!string),
        "redo AddFlipPairCommand should restore the explicit pair");

    auto autoAdd = new AutoAddFlipPairsCommand();
    autoAdd.leftPattern = ".L";
    autoAdd.rightPattern = ".R";
    incActionClearHistory();
    require(autoAdd.run(ctx).succeeded, "AutoAddFlipPairsCommand should match named pairs");
    require(incActionHistory().length == 1, "AutoAddFlipPairsCommand should push one undoable action");
    auto autoJson = (new ListFlipPairsCommand()).run(ctx).result.toString();
    require(autoJson.canFind(autoLeft.uuid.to!string) && autoJson.canFind(autoRight.uuid.to!string),
        "auto-added flip pairs should include matched node UUIDs");
    incActionUndo();
    require(!(new ListFlipPairsCommand()).run(ctx).result.toString().canFind(autoLeft.uuid.to!string),
        "undo AutoAddFlipPairsCommand should remove auto-added pairs");
    incActionRedo();
    require((new ListFlipPairsCommand()).run(ctx).result.toString().canFind(autoLeft.uuid.to!string),
        "redo AutoAddFlipPairsCommand should restore auto-added pairs");

    auto removePair = new RemoveFlipPairCommand();
    removePair.left = left;
    removePair.right = right;
    incActionClearHistory();
    require(removePair.run(ctx).succeeded, "RemoveFlipPairCommand should remove explicit pair");
    require(incActionHistory().length == 1, "RemoveFlipPairCommand should push one undoable action");
    auto afterRemove = (new ListFlipPairsCommand()).run(ctx).result.toString();
    require(!afterRemove.canFind(left.uuid.to!string) || !afterRemove.canFind(right.uuid.to!string),
        "removed explicit pair should no longer be listed");
    incActionUndo();
    require((new ListFlipPairsCommand()).run(ctx).result.toString().canFind(left.uuid.to!string),
        "undo RemoveFlipPairCommand should restore removed pair");
    incActionRedo();
    require(!(new ListFlipPairsCommand()).run(ctx).result.toString().canFind(left.uuid.to!string),
        "redo RemoveFlipPairCommand should remove pair again");
}

private void testViewportPaletteCommandDiscovery() {
    auto all = collectAllCommands();
    require(all.length > 50, "command palette should collect registered commands");

    bool sawSave;
    bool sawNode;
    foreach (cmd; all) {
        if (cmd is nijigenerate.commands.puppet.file.commands[FileCommand.SaveFile])
            sawSave = true;
        if (cmd is nijigenerate.commands.node.node.commands[NodeCommand.AddNode])
            sawNode = true;
    }
    require(sawSave, "command palette should include SaveFile command");
    require(sawNode, "command palette should include AddNode command");

    auto saveFiltered = filterCommands("save", nijigenerate.commands.viewport.palette.commands[PaletteCommand.ListCommand]);
    bool filteredSave;
    foreach (cmd; saveFiltered) {
        if (cmd is nijigenerate.commands.puppet.file.commands[FileCommand.SaveFile])
            filteredSave = true;
    }
    require(filteredSave, "command palette filter should find SaveFile by text");
    require(getParentCategory(nijigenerate.commands.puppet.file.commands[FileCommand.SaveFile]).length > 0,
        "command palette should derive a parent category");
    require(deriveEnglishToken(nijigenerate.commands.puppet.file.commands[FileCommand.SaveFile]).canFind("save"),
        "command palette should derive an English search token");
}

private void testActionStackScopeGuard() {
    resetCase();

    require(ngActionStackLevel() == 0, "action stack should start at root level");

    auto vertexScope = ngOpenActionStackScope(ActionStackScopeUnit.VertexEdit);
    require(vertexScope.isActive(), "VertexEdit scope should open");
    require(ngActionStackScopeActive(ActionStackScopeUnit.VertexEdit), "VertexEdit scope should be registered");
    require(ngActionStackLevel() == 1, "VertexEdit scope should push one action stack level");

    ngGuardActionStackScopes([ActionStackScopeUnit.VertexEdit]);
    require(vertexScope.isActive(), "guard should keep allowed VertexEdit scope");
    require(ngActionStackLevel() == 1, "guard should not pop allowed scope");

    auto depthScope = ngOpenActionStackScope(ActionStackScopeUnit.DepthEdit);
    require(depthScope.isActive(), "DepthEdit nested scope should open");
    require(ngActionStackLevel() == 2, "nested DepthEdit scope should push another level");

    ngGuardActionStackScopes([ActionStackScopeUnit.VertexEdit]);
    require(vertexScope.isActive(), "guard should preserve allowed outer scope");
    require(!depthScope.isActive(), "guard should close disallowed nested DepthEdit scope");
    require(!ngActionStackScopeActive(ActionStackScopeUnit.DepthEdit), "closed DepthEdit scope should be unregistered");
    require(ngActionStackLevel() == 1, "guard should restore stack level after closing nested scope");

    auto oneTimeScope = ngOpenActionStackScope(ActionStackScopeUnit.OneTimeDeform);
    require(oneTimeScope.isActive(), "OneTimeDeform nested scope should open");
    require(ngActionStackLevel() == 2, "OneTimeDeform nested scope should push another level");

    ngGuardActionStackScopes();
    require(!vertexScope.isActive(), "empty guard should close outer VertexEdit scope");
    require(!oneTimeScope.isActive(), "closing outer scope should also close nested OneTimeDeform scope");
    require(ngActionStackLevel() == 0, "empty guard should restore root action stack level");
}

private void testCommandBaseContracts() {
    require(toCodeString("a\"b") == `"a\"b"`, "toCodeString should quote and escape strings");
    require(toCodeString(null) == "null", "toCodeString should encode null");
    require(toCodeString(CommandGuiDisplay.dialog).length > 0, "toCodeString should encode enum values");

    auto ok = CommandResult(true, "done");
    require(ok.succeeded && ok.message == "done", "CommandResult should store success and message");
    require(ok.waitForCompletion() is ok, "CommandResult.waitForCompletion should return itself for synchronous commands");

    auto typed = ExCommandResult!int(true, 42, "answer");
    require(typed.succeeded && typed.result == 42 && typed.message == "answer", "ExCommandResult should preserve typed payload");

    auto created = CreateResult!Node(true, [cast(Node)new Node(cast(Node)null)], "created");
    require(created.created.length == 1 && created.message == "created", "CreateResult should preserve created objects");
    auto deleted = DeleteResult!Node(true, [created.created[0]], "deleted");
    require(deleted.deleted.length == 1 && deleted.message == "deleted", "DeleteResult should preserve deleted objects");
    auto loaded = LoadResult!Node(true, [created.created[0]], "loaded");
    require(loaded.loaded.length == 1 && loaded.message == "loaded", "LoadResult should preserve loaded objects");

    auto ctx = new Context();
    require(!ctx.hasPuppet && !ctx.hasNodes && !ctx.hasParameters && !ctx.hasKeyPoint, "Context should start without explicit values");
    ctx.puppet = incActivePuppet();
    ctx.nodes = created.created;
    ctx.parameters = [new Parameter("Param", false)];
    ctx.armedParameters = ctx.parameters;
    ctx.bindings = null;
    ctx.activeBindings = null;
    ctx.keyPoint = vec2u(1, 2);
    ctx.parameterValue = vec2(0.25, -0.5);
    require(ctx.hasPuppet && ctx.hasNodes && ctx.hasParameters && ctx.hasArmedParameters, "Context setters should mark object masks");
    require(ctx.hasBindings && ctx.hasActiveBindings && ctx.hasKeyPoint && ctx.hasParameterValue, "Context setters should mark binding/key/value masks");
    require(ctx.keyPoint == vec2u(1, 2) && ctx.parameterValue == vec2(0.25, -0.5), "Context should store keypoint and parameter value");
    ctx.hasNodes = false;
    require(!ctx.hasNodes, "Context mask setters should clear flags");

    auto meta = new RegressionMetaCommand();
    require(meta.label == "Meta Label" && meta.description == "Meta Description", "ExCommand should expose label and description");
    require(!meta.shortcutRunnable, "ShortcutHidden metadata should hide command from shortcuts");
    require(!meta.mcpExposed, "McpHidden metadata should hide command from MCP");
    require(meta.guiDisplay == CommandGuiDisplay.dialog, "GuiDisplay metadata should be reflected");
    require(meta.irreversibleEffect == CommandIrreversibleEffect.configEdit, "IrreversibleEffect metadata should be reflected");

    auto arg = new RegressionArgCommand();
    require(arg.label == "Arg Label" && arg.description == "Arg Description", "ExCommand with args should expose label and description");
    require(arg.name == "Alice" && arg.arg1 == 7, "ExCommand should assign TW and positional argument fields");
    auto metas = RegressionArgCommand.reflectArgMeta();
    require(metas.length == 1 && metas[0].fieldName == "name" && metas[0].fieldDesc == "Name", "ExCommand should reflect TW argument metadata");
}

private void testPlatformVersionMetadata() {
    import nijigenerate.ver : INC_VERSION;
    require(INC_VERSION.length > 0, "version string should not be empty");
    require(INC_VERSION.startsWith("v"), "version string should be release-tag shaped");
    require(!INC_VERSION.canFind("UNKNOWN"), "version string should not be an unresolved placeholder");
}

private void testSettingsPathResolution() {
    import nijigenerate.core.dpi : incGetUIScale, incGetUIScaleFont, incGetUIScaleText, incInitDPIScaling, incSetUIScale;
    import nijigenerate.core.path : APP_FOLDER_NAME, incGetAppConfigPath, incGetAppFontsPath, incGetAppImguiConfigFile, incGetAppLocalePath, incGetAppLocalePathExtra;
    import nijigenerate.core.settings : incSettingsCanGet, incSettingsGet, incSettingsLoad, incSettingsPath, incSettingsSet;
    import std.path : baseName;

    auto configPath = incGetAppConfigPath();
    require(configPath.length > 0, "config path should resolve");
    require(configPath.canFind(APP_FOLDER_NAME), "config path should include application folder name");
    require(exists(configPath), "config path should be created");

    auto imguiPath = incGetAppImguiConfigFile();
    require(imguiPath.endsWith("imgui.ini"), "imgui config path should point to imgui.ini");
    require(imguiPath.canFind(configPath), "imgui config path should live under config path");

    auto fontsPath = incGetAppFontsPath();
    auto localePath = incGetAppLocalePath();
    require(exists(fontsPath) && baseName(fontsPath) == "fonts", "fonts path should be created under config path");
    require(exists(localePath) && baseName(localePath) == "i18n", "locale path should be created under config path");
    auto extraLocale = incGetAppLocalePathExtra();
    require(extraLocale is null || extraLocale.length > 0, "extra locale path should be null or non-empty");

    incSettingsLoad();
    require(incSettingsPath().endsWith("settings.json"), "settings path should point to settings.json");
    require(incSettingsCanGet("ViewportZoomMode"), "default settings should include viewport zoom mode");
    require(incSettingsGet!string("ViewportZoomMode").length > 0, "default viewport zoom mode should be readable");
    incSetUIScale(1.5f);
    incInitDPIScaling();
    version (OSX) {
        require(near(incGetUIScale(), 1.0f), "OSX UI scale should remain OS-native");
    } else {
        require(near(incGetUIScale(), 1.5f), "UI scale should persist through settings");
    }
    require(incGetUIScaleFont() > 0, "UI font scale should be positive");
    require(incGetUIScaleText().length > 0, "UI scale text should render");
    incSettingsSet("RegressionPathProbe", "ok");
    require(incSettingsGet!string("RegressionPathProbe") == "ok", "settings set/get should round-trip string values");
}

private void testMeshCommonVertexConnections() {
    import nijigenerate.core.math.mesh : MeshVertex, connect, disconnect, disconnectAll, isConnectedTo;

    MeshVertex a = MeshVertex(vec2(0, 0));
    MeshVertex b = MeshVertex(vec2(1, 0));
    MeshVertex c = MeshVertex(vec2(2, 0));

    require(!isConnectedTo(&a, &b), "vertices should start disconnected");
    connect(&a, &b);
    require(isConnectedTo(&a, &b) && isConnectedTo(&b, &a), "connect should be symmetric");
    connect(&a, &b);
    require(a.connections.length == 1 && b.connections.length == 1, "connect should not duplicate existing edges");
    connect(&a, &c);
    require(a.connections.length == 2 && isConnectedTo(&c, &a), "connect should support multiple neighbors");
    disconnect(&a, &b);
    require(!isConnectedTo(&a, &b) && !isConnectedTo(&b, &a), "disconnect should be symmetric");
    require(isConnectedTo(&a, &c), "disconnect should leave other edges intact");
    disconnectAll(&a);
    require(a.connections.length == 0 && !isConnectedTo(&a, &c), "disconnectAll should remove every edge symmetrically");
}

private void testMeshEditorOperationTargets() {
    import nijigenerate.core.math.mesh : MeshVertex;

    resetCase();

    auto node = new Node(incActivePuppet().root);
    node.name = "operation-node";
    node.setValue("transform.t.x", 10.0f);
    node.setValue("transform.t.y", 20.0f);

    auto nodeOp = new meshNodeOps.IncMeshEditorOneFor!(Node, EditMode.ModelEdit)(false);
    nodeOp.setTarget(node);
    require(nodeOp.getTarget() is node, "Node operation should keep its target");
    require(nodeOp.getVertexFromPoint(vec2(10, 20)) == 0, "Node operation should hit-test the node translation");
    require(nodeOp.getVertexFromPoint(vec2(200, 200)) == ulong.max, "Node operation should reject distant hit tests");
    require(nodeOp.getInRect(vec2(0, 0), vec2(20, 30), 0) == [0UL], "Node operation should select translation inside a rectangle");
    require(nodeOp.getInRect(vec2(20, 30), vec2(0, 0), 0) == [0UL], "Node operation should normalize inverted rectangle bounds");
    auto nodeVertices = nodeOp.getVerticesByIndex([0UL, 1UL]);
    require(nodeVertices.length == 2 && nodeVertices[0] !is null && nodeVertices[1] is null, "Node operation should expose only the synthetic translation vertex");
    require(nearVec2(nodeVertices[0].position, vec2(10, 20)), "Node operation synthetic vertex should match translation");
    require(nodeOp.filterVertices((MeshVertex* v) => nearVec2(v.position, vec2(10, 20))) == [0UL], "Node operation should filter its translation vertex");

    auto grid = new GridDeformer(incActivePuppet().root);
    grid.name = "operation-grid";
    auto gridOriginal = grid.vertices.dup;
    require(gridOriginal.length > 0, "GridDeformer fixture should create vertices");

    auto deformOp = new meshDeformableOps.IncMeshEditorOneFor!(GridDeformer, EditMode.VertexEdit)();
    deformOp.setTarget(grid);
    require(deformOp.getTarget() is grid, "Deformable operation should keep its target");
    auto deformVertex = deformOp.getVerticesByIndex([0UL], true)[0];
    auto deformColumnX = deformVertex.position.x;
    auto deformMoved = deformVertex.position + vec2(3, 0);
    foreach (candidate; deformOp.filterVertices((MeshVertex* v) => near(v.position.x, deformColumnX)))
        deformOp.moveMeshVertex(deformOp.getVerticesByIndex([candidate], true)[0], deformOp.getVerticesByIndex([candidate], true)[0].position + vec2(3, 0));
    deformOp.applyToTarget();
    require(containsVec2(grid.vertices, deformMoved), "Deformable operation should apply moved vertices to GridDeformer");
    require(grid.vertices.length == gridOriginal.length, "Deformable operation should preserve vertex count for a move");
    incActionUndo();
    require(nearVec2Array(grid.vertices, gridOriginal), "Deformable operation undo should restore GridDeformer vertices");
    incActionRedo();
    require(containsVec2(grid.vertices, deformMoved), "Deformable operation redo should restore moved GridDeformer vertex");

    auto part = newMeshPart("operation-drawable");
    auto drawableOriginal = part.getMesh().vertices.dup;
    require(drawableOriginal.length > 0, "Drawable fixture should create mesh vertices");

    auto drawableOp = new meshDrawableOps.IncMeshEditorOneFor!(Part, EditMode.VertexEdit)();
    drawableOp.setTarget(part);
    require(drawableOp.getTarget() is part, "Drawable operation should keep its target");
    auto drawableVertex = drawableOp.getVerticesByIndex([0UL], true)[0];
    auto drawableMoved = drawableVertex.position + vec2(-2, 6);
    drawableOp.moveMeshVertex(drawableVertex, drawableMoved);
    drawableOp.applyToTarget();
    require(nearVec2(part.getMesh().vertices[0], drawableMoved), "Drawable operation should apply moved vertices to Part mesh");
    require(part.getMesh().vertices.length == drawableOriginal.length, "Drawable operation should preserve vertex count for a move");
    incActionUndo();
    require(nearVec2Array(part.getMesh().vertices, drawableOriginal), "Drawable operation undo should restore mesh vertices");
    incActionRedo();
    require(nearVec2(part.getMesh().vertices[0], drawableMoved), "Drawable operation redo should restore moved mesh vertex");
}

private void testMeshEditorMultiObjectApplyUndoRedo() {
    import nijigenerate.core.math.mesh : MeshVertex;
    import nijigenerate.viewport.vertex.mesheditor.editor : VertexMeshEditor;

    resetCase();

    auto gridA = new GridDeformer(incActivePuppet().root);
    gridA.name = "multi-grid-a";
    auto gridB = new GridDeformer(incActivePuppet().root);
    gridB.name = "multi-grid-b";

    auto originalA = gridA.vertices.dup;
    auto originalB = gridB.vertices.dup;
    require(originalA.length > 0 && originalB.length > 0, "multi-object fixture should have editable grids");

    auto editor = new VertexMeshEditor();
    scope(exit) destroy(editor);
    editor.setTargets([cast(Node)gridA, cast(Node)gridB]);

    auto opA = editor.getEditorFor(gridA);
    auto opB = editor.getEditorFor(gridB);
    require(opA !is null && opB !is null, "multi-object editor should create an operation for every target");

    auto columnA = opA.getVerticesByIndex([0UL], true)[0].position.x;
    auto columnB = opB.getVerticesByIndex([0UL], true)[0].position.x;
    auto movedA = opA.getVerticesByIndex([0UL], true)[0].position + vec2(7, 0);
    auto movedB = opB.getVerticesByIndex([0UL], true)[0].position + vec2(-5, 0);
    foreach (idx; opA.filterVertices((MeshVertex* v) => near(v.position.x, columnA))) {
        auto v = opA.getVerticesByIndex([idx], true)[0];
        opA.moveMeshVertex(v, v.position + vec2(7, 0));
    }
    foreach (idx; opB.filterVertices((MeshVertex* v) => near(v.position.x, columnB))) {
        auto v = opB.getVerticesByIndex([idx], true)[0];
        opB.moveMeshVertex(v, v.position + vec2(-5, 0));
    }
    editor.applyToTarget();

    require(containsVec2(gridA.vertices, movedA), "multi-object apply should update the first GridDeformer");
    require(containsVec2(gridB.vertices, movedB), "multi-object apply should update the second GridDeformer");

    incActionUndo();
    require(nearVec2Array(gridA.vertices, originalA), "one undo should restore the first multi-object target");
    require(nearVec2Array(gridB.vertices, originalB), "one undo should restore the second multi-object target");

    incActionRedo();
    require(containsVec2(gridA.vertices, movedA), "one redo should restore the first multi-object target");
    require(containsVec2(gridB.vertices, movedB), "one redo should restore the second multi-object target");
}

private void testMeshEditorMirrorSymmetryContracts() {
    import nijigenerate.core.math.mesh : MeshVertex;

    resetCase();

    bool containsX(Vec2Array values, float x) {
        foreach (value; values) {
            if (near(value.x, x))
                return true;
        }
        return false;
    }

    auto grid = new GridDeformer(incActivePuppet().root);
    grid.name = "mirror-grid";
    auto ctx = new Context();
    ctx.nodes = [cast(Node)grid];
    require((new DefineGridCommand([-100f, 0f, 100f], [-5f, 5f])).run(ctx).succeeded, "mirror fixture grid should be defined");
    auto original = grid.vertices.dup;

    auto op = new meshDeformableOps.IncMeshEditorOneFor!(Deformable, EditMode.VertexEdit)();
    op.setTarget(grid);
    op.mirrorHoriz = true;
    op.mirrorOrigin = vec2(0, 0);

    auto left = op.getVertexFromPoint(vec2(-100, -5));
    auto right = op.getVertexFromPoint(vec2(100, -5));
    require(left != ulong.max && right != ulong.max && left != right, "mirror fixture should find paired vertices");
    require(op.mirrorVertex(1, left) == right, "horizontal mirror should find the opposite vertex");
    require(op.mirrorDelta(1, vec2(3, -2)) == vec2(-3, -2), "horizontal mirror should invert only the X delta");

    op.select(left);
    require(op.mirrorSelected.canFind(right), "selecting one side should track the mirrored counterpart");

    auto delta = vec2(2, 0);
    auto leftMoved = op.getVerticesByIndex([left], true)[0].position + delta;
    auto rightMoved = op.getVerticesByIndex([right], true)[0].position + op.mirrorDelta(1, delta);
    foreach (idx; op.filterVertices((MeshVertex* v) => near(v.position.x, -100))) {
        auto v = op.getVerticesByIndex([idx], true)[0];
        op.moveMeshVertex(v, v.position + delta);
    }
    foreach (idx; op.filterVertices((MeshVertex* v) => near(v.position.x, 100))) {
        auto v = op.getVerticesByIndex([idx], true)[0];
        op.moveMeshVertex(v, v.position + op.mirrorDelta(1, delta));
    }

    op.resetMesh();
    require(nearVec2Array(grid.vertices, original), "reset before apply should leave the target unchanged");
    require(nearVec2(op.getVerticesByIndex([left], true)[0].position, original[left]), "reset before apply should cancel the local mirrored edit");

    foreach (idx; op.filterVertices((MeshVertex* v) => near(v.position.x, -100))) {
        auto v = op.getVerticesByIndex([idx], true)[0];
        op.moveMeshVertex(v, v.position + delta);
    }
    foreach (idx; op.filterVertices((MeshVertex* v) => near(v.position.x, 100))) {
        auto v = op.getVerticesByIndex([idx], true)[0];
        op.moveMeshVertex(v, v.position + op.mirrorDelta(1, delta));
    }
    op.applyToTarget();

    require(containsVec2(grid.vertices, leftMoved), "mirrored apply should update the edited vertex");
    require(containsX(grid.vertices, rightMoved.x), "mirrored apply should update the counterpart column");

    incActionUndo();
    require(nearVec2Array(grid.vertices, original), "undo should restore mirrored GridDeformer vertices");
    incActionRedo();
    require(containsVec2(grid.vertices, leftMoved), "redo should restore mirrored edited vertex");
    require(containsX(grid.vertices, rightMoved.x), "redo should restore mirrored counterpart column");
}

private void testGridDeformerToolVirtualMeshApplyUndoRedo() {
    import nijigenerate.viewport.common.mesheditor.tools.grid : GridTool;

    resetCase();

    auto grid = new GridDeformer(incActivePuppet().root);
    grid.name = "grid-tool-target";
    auto original = grid.vertices.dup;
    require(original.length == 4, "GridDeformer tool fixture should start from the default 2x2 grid");

    auto op = new meshDeformableOps.IncMeshEditorOneFor!(Deformable, EditMode.VertexEdit)();
    op.setTarget(grid);

    auto tool = new GridTool();
    tool.numCut = 3;
    tool.resetVirtualMeshAsEmpty(op);
    require(!op.vertexMapDirty, "empty virtual grid reset should not mark target dirty before editing");

    require(tool.onDragStart(vec2(0, 0), op), "GridTool should start creating an empty virtual grid by drag");
    require(tool.onDragEnd(vec2(30, 20), op), "GridTool should finish virtual grid creation on drag end");
    require(op.vertexMapDirty, "virtual grid creation should mark the operation dirty");

    require(tool.applyVirtualMeshToTarget(op), "GridTool should apply the virtual mesh to the GridDeformer target");
    require(grid.vertices.length == 9, "GridTool apply should create a 3x3 GridDeformer");
    require(grid.deformation.length == 9, "GridTool apply should keep deformation length aligned with vertices");

    incActionUndo();
    require(nearVec2Array(grid.vertices, original), "undo GridTool apply should restore the original GridDeformer vertices");
    incActionRedo();
    require(grid.vertices.length == 9, "redo GridTool apply should restore the created grid");
}

private void testPathDeformerToolApplyUndoRedo() {
    import nijigenerate.viewport.common.mesheditor.tools.bezierdeform : BezierDeformTool;

    resetCase();

    auto path = new PathDeformer(incActivePuppet().root);
    path.name = "path-tool-target";
    path.rebuffer(Vec2Array([
        vec2(-100, 0),
        vec2(100, 0),
    ]));
    auto original = path.vertices.dup;

    auto op = new meshDeformableOps.IncMeshEditorOneFor!(Deformable, EditMode.VertexEdit)();
    op.setTarget(path);
    auto tool = new BezierDeformTool();

    bool changed;
    op.mousePos = vec2(0, 0);
    tool.updateVertexEdit(null, op, BezierDeformTool.BezierDeformActionID.AddPoint, changed);
    require(op.vertices.length == 3, "BezierDeformTool should add a control point to the operation");
    op.applyToTarget();
    require(path.vertices.length == 3 && containsVec2(path.vertices, vec2(0, 0)),
        "PathDeformer apply should persist the inserted control point");

    incActionUndo();
    require(nearVec2Array(path.vertices, original), "undo PathDeformer apply should restore original control points");
    incActionRedo();
    require(path.vertices.length == 3 && containsVec2(path.vertices, vec2(0, 0)),
        "redo PathDeformer apply should restore inserted control point");

    op.setTarget(path);
    auto middle = op.getVertexFromPoint(vec2(0, 0));
    require(middle != ulong.max, "PathDeformer tool fixture should find the inserted point");
    op.selectOne(middle);
    op.lastMousePos = vec2(0, 0);
    op.mousePos = vec2(0, 20);
    require(tool.onDragStart(op.lastMousePos, op), "BezierDeformTool should start dragging a selected point");
    require(tool.onDragUpdate(op.mousePos, op), "BezierDeformTool should move a selected point during drag");
    require(tool.onDragEnd(op.mousePos, op), "BezierDeformTool should finish dragging a selected point");
    bool operationContainsMovedPoint = false;
    foreach (v; op.vertices) {
        if (nearVec2(v.position, vec2(0, 20))) {
            operationContainsMovedPoint = true;
            break;
        }
    }
    require(operationContainsMovedPoint, "BezierDeformTool drag should move the selected operation point");
    op.applyToTarget();
    require(containsVec2(path.vertices, vec2(0, 20)), "PathDeformer apply should persist the moved control point");

    incActionUndo();
    require(containsVec2(path.vertices, vec2(0, 0)), "undo moved PathDeformer apply should restore the previous point location");
    incActionRedo();
    require(containsVec2(path.vertices, vec2(0, 20)), "redo moved PathDeformer apply should restore the moved point location");
}

private void testMeshGroupGridDeformerCompatibility() {
    resetCase();

    require("MeshGroup" in conversionMap && conversionMap["MeshGroup"].canFind("GridDeformer"),
        "node conversion map should keep MeshGroup to GridDeformer migration available");
    require("GridDeformer" in conversionMap && conversionMap["GridDeformer"].canFind("MeshGroup"),
        "node conversion map should keep GridDeformer to MeshGroup compatibility available");

    auto grid = new GridDeformer(incActivePuppet().root);
    grid.name = "compat-grid";
    grid.dynamic = true;
    grid.rebuffer(Vec2Array([
        vec2(-10, -5),
        vec2(10, -5),
        vec2(-10, 5),
        vec2(10, 5),
    ]));
    grid.deformation = Vec2Array([
        vec2(1, 0),
        vec2(0, 2),
        vec2(-1, 0),
        vec2(0, -2),
    ]);

    auto meshGroup = new MeshGroup(incActivePuppet().root);
    meshGroup.name = "compat-meshgroup";
    meshGroup.copyFrom(grid, false, true);

    require(meshGroup.getMesh().vertices.length == grid.vertices.length,
        "MeshGroup compatibility copy should preserve GridDeformer vertex count");
    require(meshGroup.getMesh().indices.length == 6,
        "MeshGroup compatibility copy should rebuild a quad grid as two triangles");
    require(meshGroup.deformation.length == grid.deformation.length,
        "MeshGroup compatibility copy should preserve deformation length");
    require(nearVec2Array(meshGroup.deformation, grid.deformation),
        "MeshGroup compatibility copy should preserve deformation offsets");
    require(meshGroup.dynamic == grid.dynamic,
        "MeshGroup compatibility copy should preserve dynamic mode");
    require(meshGroup.getTranslateChildren(),
        "MeshGroup compatibility copy from GridDeformer should keep child translation enabled");

    auto copiedMeshGroup = new MeshGroup(incActivePuppet().root);
    copiedMeshGroup.copyFrom(meshGroup, false, true);
    require(copiedMeshGroup.dynamic == meshGroup.dynamic,
        "MeshGroup-to-MeshGroup copy should preserve dynamic mode");
    require(copiedMeshGroup.getTranslateChildren() == meshGroup.getTranslateChildren(),
        "MeshGroup-to-MeshGroup copy should preserve translate-children mode");
}

private void testGridDeformerRuntimeInterpolationContracts() {
    resetCase();

    class RuntimeGridDeformer : GridDeformer {
        this(Node parent) { super(parent); }
        void primeRuntime() {
            RenderContext ctx;
            runPreProcessTask(ctx);
        }
    }

    auto grid = new RuntimeGridDeformer(incActivePuppet().root);
    grid.name = "runtime-grid";
    grid.rebuffer(Vec2Array([
        vec2(0, 0),
        vec2(10, 0),
        vec2(0, 10),
        vec2(10, 10),
    ]));
    grid.deformation = Vec2Array([
        vec2(0, 0),
        vec2(10, 0),
        vec2(0, 0),
        vec2(10, 0),
    ]);
    grid.primeRuntime();

    auto part = newMeshPart("runtime-grid-child");
    part.localTransform.translation = vec3(0, 0, 0);
    auto vertices = Vec2Array([
        vec2(0, 0),
        vec2(5, 5),
        vec2(10, 10),
    ]);
    auto baseDeformation = Vec2Array([
        vec2(0, 0),
        vec2(0, 0),
        vec2(0, 0),
    ]);
    auto matrix = mat4.identity;

    auto unchanged = grid.deformChildren(part, vertices, baseDeformation.dup, &matrix);
    require(unchanged[0].length == vertices.length, "GridDeformer runtime should preserve child deformation length");
    require(unchanged[2], "GridDeformer runtime should report changed output when grid deformation exists");
    require(nearVec2(unchanged[0][0], vec2(0, 0)), "GridDeformer runtime should keep the left edge unchanged");
    require(nearVec2(unchanged[0][1], vec2(5, 0)), "GridDeformer runtime should bilinearly interpolate center X offset");
    require(nearVec2(unchanged[0][2], vec2(10, 0)), "GridDeformer runtime should apply full right-edge X offset");

    grid.deformation[] = vec2(0, 0);
    auto baseline = grid.deformChildren(part, vertices, baseDeformation.dup, &matrix);
    require(baseline[0].length == baseDeformation.length, "GridDeformer baseline should preserve deformation length");
    require(!baseline[2], "GridDeformer baseline should report unchanged output with zero deformation");
    require(nearVec2Array(baseline[0], baseDeformation), "GridDeformer baseline should not add offsets");
}

private void testPathDeformerRuntimeContracts() {
    resetCase();

    class RuntimePathDeformer : PathDeformer {
        this(Node parent) { super(parent); }
        void primeRuntime() {
            RenderContext ctx;
            runPreProcessTask(ctx);
        }
    }

    auto path = new RuntimePathDeformer(incActivePuppet().root);
    path.name = "runtime-path";
    path.rebuffer(Vec2Array([
        vec2(0, 0),
        vec2(10, 0),
    ]));
    path.primeRuntime();
    path.deformedCurve = path.createCurve(Vec2Array([
        vec2(0, 0),
        vec2(10, 10),
    ]));

    auto part = newMeshPart("runtime-path-child");
    auto vertices = Vec2Array([
        vec2(0, 0),
        vec2(5, 0),
        vec2(10, 0),
    ]);
    auto baseDeformation = Vec2Array([
        vec2(0, 0),
        vec2(0, 0),
        vec2(0, 0),
    ]);
    auto matrix = mat4.identity;

    auto deformed = path.deformChildren(part, vertices, baseDeformation.dup, &matrix);
    require(deformed[0].length == vertices.length, "PathDeformer runtime should preserve child deformation length");
    require(deformed[2], "PathDeformer runtime should report changed output when path target differs");
    require(deformed[0][0].distance(vec2(0, 0)) < 0.01f, "PathDeformer runtime should keep the curve root effectively anchored");
    require(deformed[0][1].y > 0, "PathDeformer runtime should lift a midpoint when the target curve rises");
    require(deformed[0][2].y > deformed[0][1].y, "PathDeformer runtime should apply larger offset near the raised end");

    path.deformedCurve = path.createCurve(path.vertices.dup);
    auto baseline = path.deformChildren(part, vertices, baseDeformation.dup, &matrix);
    require(baseline[0].length == vertices.length, "PathDeformer baseline should preserve deformation length");
    require(baseline[2], "PathDeformer baseline should still return a sampled deformation result");
    require(nearVec2Array(baseline[0], baseDeformation), "PathDeformer baseline should not add offsets when target curve equals original");
}

private void testMeshSplineContracts() {
    import nijigenerate.viewport.common.spline : CatmullSpline, SplinePoint;

    auto spline = new CatmullSpline();
    spline.resolution = 10;
    spline.points = [
        SplinePoint(vec2(0, 0), 1, 1),
        SplinePoint(vec2(10, 0), 1, 1),
    ];
    spline.interpolate();
    require(nearVec2(spline.eval(0), vec2(0, 0)), "spline eval should clamp to the first point");
    require(nearVec2(spline.eval(1), vec2(10, 0)), "spline eval should clamp to the last point");
    require(nearVec2(spline.eval(0.5f), vec2(5, 0)), "two-point spline should interpolate linearly");

    vec2 tangent;
    auto off = spline.findClosestPointOffset(vec2(4.25f, 2), tangent);
    require(near(off, 0.425f), "closest-point offset should project onto a linear segment");
    require(nearVec2(tangent, vec2(1, 0)), "closest-point tangent should follow the segment direction");

    auto inserted = spline.addPoint(vec2(5, 0.2f), 1.0f);
    require(inserted == 1 && spline.points.length == 3, "addPoint near a segment should split the spline");
    require(nearVec2(spline.points[1].position, vec2(5, 0)), "split point should be placed on the curve");
    spline.removePoint(1);
    require(spline.points.length == 2, "removePoint should remove the inserted point");
    spline.prependPoint(vec2(-10, 0));
    spline.appendPoint(vec2(20, 0));
    require(spline.points.length == 4, "prependPoint and appendPoint should mutate endpoints");
    require(spline.findPoint(vec2(-10, 0), 1.0f) == 0, "findPoint should find an endpoint within radius");
    require(spline.findPoint(vec2(1000, 1000), 1.0f) == -1, "findPoint should reject distant points");

    MeshData data;
    data.vertices = Vec2Array([
        vec2(10, 0),
        vec2(11, 0),
        vec2(10, 1),
    ]);
    data.uvs = Vec2Array([
        vec2(0, 0),
        vec2(1, 0),
        vec2(0, 1),
    ]);
    data.indices = [0, 1, 2];
    auto mesh = new IncMesh(data);

    auto deformPath = new CatmullSpline();
    deformPath.resolution = 10;
    deformPath.points = [
        SplinePoint(vec2(0, 0), 1, 1),
        SplinePoint(vec2(10, 0), 1, 1),
        SplinePoint(vec2(20, 0), 1, 1),
    ];
    deformPath.interpolate();
    deformPath.createTarget(mesh, mat4.identity);
    require(deformPath.refOffsets.length == mesh.vertices.length, "createTarget should map every mesh vertex to the spline");
    deformPath.splitAt(0.5f);
    require(deformPath.target.points.length == deformPath.points.length, "splitAt should propagate point insertion to the mapped target spline");
}

private void testCoreMathPathExtraction() {
    import nijigenerate.core.math.path : extractPath;
    import core.thread.fiber : Fiber;
    import mir.ndslice;

    vec2u[] runExtract(T)(T skeleton, int width, int height) {
        vec2u[] result;
        auto fiber = new Fiber({
            result = extractPath(skeleton, width, height);
        });
        while (fiber.state != Fiber.State.TERM)
            fiber.call();
        return result;
    }

    ubyte[] emptyData = new ubyte[9];
    int err;
    auto empty = emptyData.sliced.reshape([3, 3], err);
    require(runExtract(empty, 3, 3).length == 0, "extractPath should return empty path for empty skeleton");

    ubyte[] data = new ubyte[25];
    data[2 * 5 + 1] = 1;
    data[2 * 5 + 2] = 1;
    data[2 * 5 + 3] = 1;
    auto line = data.sliced.reshape([5, 5], err);
    auto path = runExtract(line, 5, 5);
    require(path.length == 3, "extractPath should keep the longest connected line");
    require(path[0] == vec2u(1, 2) || path[0] == vec2u(3, 2), "extractPath should start at one endpoint");
    require(path[$ - 1] == vec2u(1, 2) || path[$ - 1] == vec2u(3, 2), "extractPath should end at the other endpoint");

    ubyte[] branches = new ubyte[36];
    foreach (x; 0 .. 5)
        branches[1 * 6 + x] = 1;
    branches[4 * 6 + 4] = 1;
    branches[5 * 6 + 4] = 1;
    auto branchSlice = branches.sliced.reshape([6, 6], err);
    auto longest = runExtract(branchSlice, 6, 6);
    require(longest.length == 5, "extractPath should select the longest component among disconnected components");
}

private void testCoreMathSkeletonizeInvariants() {
    import nijigenerate.core.math.path : extractPath;
    import nijigenerate.core.math.skeletonize : skeletonizeImage;
    import core.thread.fiber : Fiber;
    import mir.ndslice;

    int err;
    ubyte[] data = new ubyte[49];
    foreach (y; 1 .. 6) {
        foreach (x; 1 .. 6)
            data[y * 7 + x] = 1;
    }
    auto image = data.sliced.reshape([7, 7], err);

    size_t before;
    foreach (value; data)
        before += value ? 1 : 0;

    auto fiber = new Fiber({
        skeletonizeImage(image);
    });
    while (fiber.state != Fiber.State.TERM)
        fiber.call();

    size_t after;
    foreach (value; data)
        after += value ? 1 : 0;

    require(after > 0, "skeletonizeImage should preserve a skeleton for a filled square");
    require(after < before, "skeletonizeImage should thin a filled square");
    require(data[3 * 7 + 3] != 0, "skeletonizeImage should keep the center of a symmetric filled square");

    vec2u[] path;
    auto extractFiber = new Fiber({
        path = extractPath(image, 7, 7);
    });
    while (extractFiber.state != Fiber.State.TERM)
        extractFiber.call();
    require(path.length > 0, "skeletonized bitmap should produce an extractable path");
    foreach (point; path)
        require(point.x < 7 && point.y < 7 && image[point.y, point.x] != 0, "extracted skeleton path should stay on foreground pixels");
}

private void testCoreMathTriangleInvariants() {
    import nijigenerate.core.math.mesh : MeshVertex;
    import nijigenerate.core.math.triangle : fillPoly, getBounds, triangulate;

    MeshVertex[] vertices = [
        MeshVertex(vec2(0, 0)),
        MeshVertex(vec2(10, 0)),
        MeshVertex(vec2(10, 10)),
        MeshVertex(vec2(0, 10)),
        MeshVertex(vec2(5, 5)),
    ];

    vec2[] positions;
    foreach (vertex; vertices)
        positions ~= vertex.position;
    auto bounds = getBounds(positions);
    require(bounds == vec4(0, 0, 10, 10), "triangle getBounds should floor/ceil vertex bounds");

    auto triResult = triangulate(vertices, vec4(0, 0, 10, 10));
    auto triangulatedVertices = triResult[0];
    auto tris = triResult[1];
    require(triangulatedVertices.length == vertices.length, "triangulate should preserve original vertices after trimming helper vertices");
    require(tris.length > 0, "triangulate should produce triangles for a square with center point");
    foreach (tri; tris) {
        require(tri.x < triangulatedVertices.length && tri.y < triangulatedVertices.length && tri.z < triangulatedVertices.length, "triangle indices should stay in range");
        require(tri.x != tri.y && tri.y != tri.z && tri.z != tri.x, "triangle should not repeat vertices");
        auto a = triangulatedVertices[tri.x];
        auto b = triangulatedVertices[tri.y];
        auto c = triangulatedVertices[tri.z];
        auto twiceArea = abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
        require(twiceArea > 0.0001f, "triangle should have non-zero area");
    }

    int[] texture;
    texture.length = 121;
    fillPoly(texture, 11, 11, vec4(0, 0, 10, 10), triangulatedVertices.toArray(), tris, 0, 7);
    require(texture.canFind(7), "fillPoly should rasterize at least one pixel for a valid triangle");
}

private void testCoreCvImageContoursDistanceTransform() {
    import nijigenerate.core.cv.image : BitDepth, Image, ImageFormat;
    import nijigenerate.core.cv.distancetransform : distanceTransform, dt1d;
    import nijigenerate.core.cv.contours : ApproximationMethod, ContourHierarchy, RetrievalMode, approximateContour, distanceToSegment, findContours, inBounds, pointInPolygon;
    import core.thread.fiber : Fiber;
    import mir.ndslice;

    auto mono = new Image(3, 2, ImageFormat.IF_MONO);
    require(mono.width == 3 && mono.height == 2 && mono.channels == 1, "mono image should expose dimensions and channel count");
    require(mono.shape == [2UL, 3UL, 1UL], "mono image shape should be height,width,channels");
    require(mono.data.length == 6, "allocated mono image should allocate width*height bytes");
    auto rgba = new Image(2, 2, ImageFormat.IF_RGB_ALPHA, BitDepth.BD_8, new ubyte[16]);
    require(rgba.channels == 4 && rgba.sliced.shape == [2UL, 2UL, 4UL], "rgba image should expose 4-channel slice shape");

    auto dt = dt1d([0.0f, float.max, float.max, 0.0f]);
    require(dt[0].length == 4 && dt[0][0] == 0 && dt[0][3] == 0, "dt1d should preserve zero sites");
    require(dt[0][1] > 0 && dt[0][2] > 0, "dt1d should assign positive distance to non-site entries");

    ubyte[] binaryData = [
        0, 0, 0, 0, 0,
        0, 1, 1, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 0, 0,
    ];
    int err;
    auto binary = binaryData.sliced.reshape([5, 5], err);
    import mir.ndslice.slice : Slice, mir_slice_kind;
    Slice!(float*, 2, mir_slice_kind.contiguous) distance;
    Slice!(int*, 3, mir_slice_kind.contiguous) nearest;
    distanceTransform(binary, distance, nearest);
    require(distance.shape == [5UL, 5UL], "distance transform should produce 2D distance slice");
    require(nearest.shape == [5UL, 5UL, 2UL], "distance transform should produce nearest coordinate slice");
    require(distance[0, 0] == 0 && distance[2, 2] == 0, "background pixels should have zero distance");
    require(distance[1, 1] > 0, "foreground pixels should have positive distance");

    vec2i[][] contours;
    ContourHierarchy[] hierarchy;
    auto contourFiber = new Fiber({
        findContours(binary, contours, hierarchy, RetrievalMode.TREE, ApproximationMethod.SIMPLE);
    });
    while (contourFiber.state != Fiber.State.TERM)
        contourFiber.call();
    require(contours.length > 0, "findContours should find contours in a ring image");
    require(hierarchy.length == contours.length, "findContours should return matching hierarchy entries");
    foreach (h; hierarchy)
        require(h.parent >= -1 && h.parent < cast(int)hierarchy.length, "hierarchy parent should be valid");

    auto polygon = [vec2i(0, 0), vec2i(4, 0), vec2i(4, 4), vec2i(0, 4), vec2i(0, 0)];
    require(pointInPolygon(vec2i(2, 2), polygon), "pointInPolygon should detect inside points");
    require(!pointInPolygon(vec2i(5, 5), polygon), "pointInPolygon should reject outside points");
    require(inBounds(4, 4, 5, 5) && !inBounds(5, 4, 5, 5), "inBounds should respect image dimensions");
    require(distanceToSegment(vec2i(2, 2), vec2i(0, 0), vec2i(4, 0)) > 1.9, "distanceToSegment should measure perpendicular distance");
    auto simplified = approximateContour([vec2i(0, 0), vec2i(1, 0), vec2i(2, 0), vec2i(2, 1)], ApproximationMethod.SIMPLE);
    require(simplified.length == 3, "SIMPLE contour approximation should remove collinear middle points");
}

private void testPlatformTaskQueue() {
    import nijigenerate.core.tasks : incTaskAdd, incTaskGetProgress, incTaskGetStatus, incTaskLength, incTaskProgress, incTaskStatus, incTaskUpdate, incTaskYield;

    while (incTaskLength() > 0)
        incTaskUpdate();

    int step;
    incTaskAdd("regression task", {
        step = 1;
        incTaskStatus("half");
        incTaskProgress(0.5f);
        incTaskYield();
        step = 2;
    });

    require(incTaskLength() == 1, "adding a task should enqueue it");
    incTaskUpdate();
    require(step == 1, "first task update should run until yield");
    require(incTaskGetStatus() == "half", "task status should be set by worker");
    require(incTaskGetProgress() == 0.5f, "task progress should be set by worker");
    require(incTaskLength() == 1, "yielded task should remain queued");
    incTaskUpdate();
    require(step == 2, "second task update should complete worker");
    incTaskUpdate();
    require(incTaskLength() == 0, "completed task should be removed on update");
    require(incTaskGetProgress() == -1, "completed queue should reset progress");
    require(incTaskGetStatus() == "No pending tasks...", "empty task queue should reset status");
}

private void testPlatformCrashDumpGeneration() {
    import nijigenerate.utils.crashdump : genCrashDump, genCrashDumpPath, getCrashDumpDir, writeCrashDump;

    auto ex = new Exception("regression crash");
    auto dump = genCrashDump(ex, "state", 12);
    require(dump.canFind("=== Args State ==="), "crash dump should include state header");
    require(dump.canFind("=== Exception ==="), "crash dump should include exception header");
    require(dump.canFind("regression crash"), "crash dump should include exception message");
    require(dump.canFind(`"state"`) && dump.canFind("12"), "crash dump should serialize state arguments");

    auto dir = getCrashDumpDir();
    require(dir.length > 0, "crash dump directory should resolve");
    auto path = genCrashDumpPath("nijigenerate-regression-crashdump");
    require(path.canFind("nijigenerate-regression-crashdump") && path.endsWith(".txt"), "crash dump path should include filename and txt extension");

    auto written = writeCrashDump("nijigenerate-regression-crashdump", ex, "written");
    require(exists(written), "writeCrashDump should write a file");
    auto text = readText(written);
    require(text.canFind("written") && text.canFind("regression crash"), "written crash dump should contain state and exception");
    remove(written);
}

private string regressionSourceRoot(string subdir) {
    auto direct = buildPath("source", "nijigenerate", subdir);
    if (exists(direct))
        return direct;
    auto parent = buildPath("..", "source", "nijigenerate", subdir);
    if (exists(parent))
        return parent;
    return buildPath("source", "nijigenerate", subdir);
}

private string regressionRepoRoot() {
    if (exists(buildPath("tl", "template.pot")))
        return ".";
    if (exists(buildPath("..", "tl", "template.pot")))
        return "..";
    return ".";
}

private bool hasScenarioPrefix(string prefix) {
    auto expected = prefix ~ ".";
    foreach (scenario; scenarios) {
        if (scenario.id == prefix || scenario.id.startsWith(expected))
            return true;
    }
    return false;
}

private bool isRegressionIdentChar(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_';
}

private string extractRegressionClassName(string line) {
    auto pos = line.countUntil("class ");
    if (pos < 0)
        return "";
    auto rest = line[pos + "class ".length .. $].stripLeft;
    size_t end;
    while (end < rest.length && isRegressionIdentChar(rest[end]))
        end++;
    if (end == 0)
        return "";
    return rest[0 .. end];
}

private string featureScenarioOwner(string kind, string rel) {
    switch (kind) {
        case "command":
            return scenarioPrefixForCommandModule(rel);
        case "window":
            return "windows";
        case "panel":
            return rel.startsWith("panels/inspector/") ? "inspectors" : "panels";
        case "inspector":
            return "inspectors";
        case "tool":
            return scenarioPrefixForSourceModule(rel);
        default:
            return "";
    }
}

private string[][string] collectFullFeatureScenarioInventory(out string[] failures) {
    auto root = regressionSourceRoot("");
    string[][string] inventory;

    void add(string kind, string rel, string name) {
        inventory[kind] ~= rel ~ ":" ~ name;
        auto prefix = featureScenarioOwner(kind, rel);
        if (prefix.length == 0)
            failures ~= "feature has no scenario owner: " ~ kind ~ " " ~ rel ~ ":" ~ name;
        else if (!hasScenarioPrefix(prefix))
            failures ~= "feature owner has no scenario family: " ~ kind ~ " " ~ rel ~ ":" ~ name ~ " -> " ~ prefix;
    }

    foreach (entry; dirEntries(root, SpanMode.depth)) {
        if (!entry.isFile || !entry.name.endsWith(".d"))
            continue;
        auto rel = normalizeRegressionSourcePath(entry.name, root);
        foreach (line; readText(entry.name).splitLines) {
            auto name = extractRegressionClassName(line);
            if (name.length == 0)
                continue;
            if (rel.startsWith("commands/") && name.endsWith("Command") && name != "ExCommand") {
                add("command", rel, name);
            } else if (rel.startsWith("windows/") && name != "Window" && (name.endsWith("Window") || name.endsWith("Modal"))) {
                add("window", rel, name);
            } else if (rel.startsWith("panels/") && name != "Panel" && name.endsWith("Panel")) {
                add("panel", rel, name);
            } else if (rel.startsWith("panels/inspector/") &&
                (name == "NodeInspector" || name == "PuppetInspector" || name == "BaseInspector" ||
                    name == "InspectorHolder" || name == "TypedInspector")) {
                add("inspector", rel, name);
            } else if (rel.startsWith("viewport/") && name != "Tool" && name.endsWith("Tool")) {
                add("tool", rel, name);
            }
        }
    }
    return inventory;
}

private string[][string] collectSourceModuleScenarioInventory(out string[] failures) {
    auto root = regressionSourceRoot("");
    string[][string] inventory;

    foreach (entry; dirEntries(root, SpanMode.depth)) {
        if (!entry.isFile || !entry.name.endsWith(".d"))
            continue;

        auto rel = normalizeRegressionSourcePath(entry.name, root);
        auto prefix = scenarioPrefixForSourceModule(rel);
        if (prefix.length == 0) {
            failures ~= "source module has no scenario owner: " ~ rel;
            continue;
        }
        if (!hasScenarioPrefix(prefix))
            failures ~= "source module owner has no scenario family: " ~ rel ~ " -> " ~ prefix;
        inventory[prefix] ~= rel;
    }

    foreach (prefix, ref modules; inventory)
        modules.sort();

    return inventory;
}

private void testCoverageSourceModuleScenarioInventory() {
    string[] failures;
    auto inventory = collectSourceModuleScenarioInventory(failures);

    immutable requiredPrefixes = [
        "api",
        "atlas",
        "automesh",
        "core",
        "deform",
        "depth",
        "depthbone",
        "inspectors",
        "io",
        "mesh",
        "node",
        "panels",
        "parameter",
        "part",
        "platform",
        "project",
        "render",
        "settings",
        "simplephysics",
        "undo",
        "viewport",
        "windows",
    ];
    foreach (prefix; requiredPrefixes) {
        if ((prefix !in inventory) || inventory[prefix].length == 0)
            failures ~= "scenario family has no source module owner rows: " ~ prefix;
    }

    size_t total;
    foreach (_prefix, modules; inventory)
        total += modules.length;
    require(total >= 265, "module scenario inventory should cover the whole source tree");
    require(failures.length == 0, "source module scenario inventory failures:\n" ~ failures.join("\n"));
}

private void testCoverageFullFeatureScenarioInventory() {
    string[] failures;
    auto inventory = collectFullFeatureScenarioInventory(failures);

    immutable requiredKinds = [
        "command",
        "window",
        "panel",
        "inspector",
        "tool",
    ];
    foreach (kind; requiredKinds) {
        if ((kind !in inventory) || inventory[kind].length == 0)
            failures ~= "feature inventory missing kind: " ~ kind;
    }

    require(("command" in inventory) && inventory["command"].length >= 170,
        "feature scenario inventory should include command classes");
    require(("window" in inventory) && inventory["window"].length >= 16,
        "feature scenario inventory should include window classes");
    require(("panel" in inventory) && inventory["panel"].length >= 14,
        "feature scenario inventory should include panel classes");
    require(("inspector" in inventory) && inventory["inspector"].length >= 15,
        "feature scenario inventory should include inspector classes");
    require(("tool" in inventory) && inventory["tool"].length >= 13,
        "feature scenario inventory should include mesh/depth tool classes");

    size_t total;
    foreach (_kind, items; inventory)
        total += items.length;
    require(total >= 230, "feature scenario inventory should cover all user-facing feature entrypoint classes");
    require(failures.length == 0, "feature scenario inventory failures:\n" ~ failures.join("\n"));
}

private string normalizeRegressionSourcePath(string path, string root) {
    auto rel = relativePath(path, root);
    auto sourcePrefix = "source/nijigenerate/";
    auto sourcePrefixIndex = rel.countUntil(sourcePrefix);
    if (sourcePrefixIndex >= 0)
        rel = rel[sourcePrefixIndex + sourcePrefix.length .. $];
    while (rel.startsWith("../"))
        rel = rel[3 .. $];
    return rel;
}

private string scenarioPrefixForCommandModule(string rel) {
    if (rel.startsWith("commands/automesh/"))
        return "automesh";
    if (rel.startsWith("commands/binding/"))
        return "parameter";
    if (rel.startsWith("commands/depth/"))
        return "depthbone";
    if (rel.startsWith("commands/inspector/"))
        return "inspectors";
    if (rel.startsWith("commands/mesheditor/"))
        return "mesh";
    if (rel.startsWith("commands/model/"))
        return "parameter";
    if (rel.startsWith("commands/node/mask.d") || rel.startsWith("commands/node/welding.d"))
        return "part";
    if (rel.startsWith("commands/node/simplephysics.d"))
        return "simplephysics";
    if (rel.startsWith("commands/node/"))
        return "node";
    if (rel.startsWith("commands/parameter/animedit.d"))
        return "animation";
    if (rel.startsWith("commands/parameter/"))
        return "parameter";
    if (rel.startsWith("commands/puppet/file.d"))
        return "project";
    if (rel.startsWith("commands/puppet/tool.d"))
        return "project";
    if (rel.startsWith("commands/puppet/view.d"))
        return "viewport";
    if (rel.startsWith("commands/puppet/edit.d"))
        return "undo";
    if (rel.startsWith("commands/vertex/"))
        return "mesh";
    if (rel.startsWith("commands/view/"))
        return "panels";
    if (rel.startsWith("commands/viewport/"))
        return "viewport";
    return "";
}

private string scenarioPrefixForSourceModule(string rel) {
    if (rel == "package.d" || rel.endsWith("/package.d"))
        return "coverage";
    if (rel == "ver.d")
        return "platform";
    if (rel == "project.d")
        return "project";
    if (rel == "config.d")
        return "settings";
    if (rel.startsWith("actions/depthbone.d"))
        return "depthbone";
    if (rel.startsWith("actions/depth.d"))
        return "depth";
    if (rel.startsWith("actions/mesh.d") || rel.startsWith("actions/mesheditor.d") || rel.startsWith("actions/vertex.d"))
        return "mesh";
    if (rel.startsWith("actions/binding.d") || rel.startsWith("actions/parameter.d"))
        return "parameter";
    if (rel.startsWith("actions/deformable.d"))
        return "deform";
    if (rel.startsWith("actions/drawable.d") || rel.startsWith("actions/node.d"))
        return "node";
    if (rel.startsWith("actions/"))
        return "undo";
    if (rel.startsWith("api/"))
        return "api";
    if (rel.startsWith("atlas/"))
        return "atlas";
    if (rel.startsWith("backend/"))
        return "render";
    if (rel == "commands/base.d" || (rel.startsWith("commands/") && rel.endsWith("/base.d")))
        return "coverage";
    if (rel.startsWith("commands/"))
        return scenarioPrefixForCommandModule(rel);
    if (rel.startsWith("core/actionstack.d"))
        return "undo";
    if (rel.startsWith("core/cv/") || rel.startsWith("core/math/") || rel.startsWith("core/selector/"))
        return "core";
    if (rel.startsWith("core/shortcut/") || rel.startsWith("core/settings.d"))
        return "settings";
    if (rel.startsWith("core/i18n.d"))
        return "i18n";
    if (rel.startsWith("core/input.d") || rel.startsWith("core/window.d") || rel.startsWith("core/tasks.d") ||
        rel.startsWith("core/dpi.d") || rel.startsWith("core/font.d") || rel.startsWith("core/logo.d") ||
        rel.startsWith("core/path.d") || rel.startsWith("core/dbg.d"))
        return "platform";
    if (rel.startsWith("core/colorbleed.d"))
        return "atlas";
    if (rel.startsWith("core/fmt.d"))
        return "widgets";
    if (rel.startsWith("ext/nodes/exdepthbone.d"))
        return "depthbone";
    if (rel.startsWith("ext/nodes/exdepthmapped.d") || rel.startsWith("ext/nodes/exdepthops.d"))
        return "depth";
    if (rel.startsWith("ext/nodes/exgriddeformer.d"))
        return "deform";
    if (rel.startsWith("ext/nodes/expart.d"))
        return "part";
    if (rel.startsWith("ext/nodes/excamera.d"))
        return "render";
    if (rel.startsWith("ext/param.d"))
        return "parameter";
    if (rel.startsWith("ext/"))
        return "node";
    if (rel.startsWith("io/"))
        return "io";
    if (rel.startsWith("panels/inspector/"))
        return "inspectors";
    if (rel.startsWith("panels/"))
        return "panels";
    if (rel.startsWith("utils/crashdump.d"))
        return "platform";
    if (rel.startsWith("utils/repair.d"))
        return "project";
    if (rel.startsWith("utils/transform.d") || rel.startsWith("utils/link.d"))
        return "core";
    if (rel.startsWith("viewport/vertex/automesh/"))
        return "automesh";
    if (rel.startsWith("viewport/depth/"))
        return "depth";
    if (rel.startsWith("viewport/common/mesheditor/brushes/"))
        return "mesh";
    if (rel.startsWith("viewport/common/mesheditor/tools/onetimedeform.d") ||
        rel.startsWith("viewport/common/mesheditor/tools/pathdeform.d") ||
        rel.startsWith("viewport/common/mesheditor/tools/grid.d") ||
        rel.startsWith("viewport/common/mesheditor/tools/bezierdeform.d"))
        return "deform";
    if (rel.startsWith("viewport/common/mesheditor/") || rel.startsWith("viewport/vertex/mesheditor/") ||
        rel.startsWith("viewport/model/mesheditor/"))
        return "mesh";
    if (rel.startsWith("viewport/common/mesh.d") || rel.startsWith("viewport/common/spline.d"))
        return "mesh";
    if (rel.startsWith("viewport/model/onionslice.d"))
        return "render";
    if (rel.startsWith("viewport/model/") || rel.startsWith("viewport/anim/") || rel.startsWith("viewport/base.d") ||
        rel.startsWith("viewport/vertex/package.d"))
        return "viewport";
    if (rel.startsWith("widgets/"))
        return "widgets";
    if (rel.startsWith("windows/"))
        return "windows";
    return "";
}

private immutable string[] requiredSourceScenarioIds = [
    "api.acp-client",
    "api.acp-echo-agent",
    "api.acp-stdio",
    "api.mcp-auth",
    "api.mcp-http-transport",
    "api.mcp-resources",
    "api.mcp-task-queue",
    "atlas.color-bleed",
    "atlas.pack",
    "automesh.processor-common",
    "core.math-path",
    "core.math-skeletonize",
    "core.math-triangle",
    "core.node-registry",
    "depth.camera",
    "depth.operation-helpers",
    "deform.bezier-tool",
    "depthbone.serialization",
    "io.image-export",
    "io.inimport-model",
    "io.inpexport-model",
    "io.kra-reader",
    "io.psd-reader",
    "io.save-native",
    "io.video-export",
    "mesh.operation-deformable",
    "mesh.operation-drawable",
    "mesh.operation-node",
    "mesh.spline",
    "mesh.tool-brush",
    "mesh.tool-connect",
    "mesh.tool-edge-cutter",
    "mesh.tool-lasso",
    "mesh.tool-point",
    "mesh.tool-select",
    "panels.armed-parameter",
    "panels.logger",
    "platform.input-window",
    "platform.tasks",
    "settings.paths",
    "widgets.button",
    "widgets.controller",
    "widgets.drag",
    "widgets.dragdrop",
    "widgets.inputtext",
    "widgets.label-category",
    "widgets.progress-output-notification",
    "widgets.texture-viewport-shadow",
    "widgets.timeline",
    "widgets.toggle-lock",
    "widgets.toolbar",
    "widgets.tooltip",
    "windows.autosave",
    "windows.flip-config",
    "windows.parameter-split",
    "windows.rename",
    "windows.settings",
];

private void testCoverageSourceCommandInventory() {
    auto root = regressionSourceRoot("");
    string[] sourceFiles;
    string[] failures;

    foreach (entry; dirEntries(root, SpanMode.depth)) {
        if (!entry.isFile || !entry.name.endsWith(".d"))
            continue;
        sourceFiles ~= normalizeRegressionSourcePath(entry.name, root);
    }
    sourceFiles.sort();

    immutable string[] requiredSourceRoots = [
        "actions",
        "api",
        "atlas",
        "backend",
        "commands",
        "core",
        "ext",
        "io",
        "panels",
        "project.d",
        "utils",
        "viewport",
        "widgets",
        "windows",
    ];
    foreach (required; requiredSourceRoots) {
        bool found;
        foreach (rel; sourceFiles) {
            if (rel == required || rel.startsWith(required ~ "/")) {
                found = true;
                break;
            }
        }
        if (!found)
            failures ~= "missing source root in inventory: " ~ required;
    }

    immutable string[] requiredScenarioPrefixes = [
        "coverage",
        "project",
        "io",
        "node",
        "core",
        "inspectors",
        "part",
        "parameter",
        "animation",
        "api",
        "mesh",
        "deform",
        "depth",
        "depthbone",
        "automesh",
        "simplephysics",
        "viewport",
        "render",
        "settings",
        "panels",
        "tools",
        "windows",
        "widgets",
        "i18n",
        "platform",
        "undo",
    ];
    foreach (prefix; requiredScenarioPrefixes) {
        if (!hasScenarioPrefix(prefix))
            failures ~= "missing scenario family: " ~ prefix;
    }

    foreach (scenario; scenarios) {
        if (scenario.status == "manual")
            failures ~= "manual status is not allowed; use computer-use or automated: " ~ scenario.id;
        if (scenario.status == pending)
            failures ~= "pending status is not allowed; use computer-use or automated: " ~ scenario.id;
    }

    foreach (id; requiredSourceScenarioIds) {
        bool found;
        foreach (scenario; scenarios) {
            if (scenario.id == id) {
                found = true;
                break;
            }
        }
        if (!found)
            failures ~= "missing required source-derived scenario: " ~ id;
    }

    foreach (rel; sourceFiles) {
        auto prefix = scenarioPrefixForSourceModule(rel);
        if (prefix.length == 0)
            failures ~= "source module has no scenario owner: " ~ rel;
        else if (!hasScenarioPrefix(prefix))
            failures ~= "source module owner has no scenario family: " ~ rel ~ " -> " ~ prefix;
    }

    foreach (rel; sourceFiles) {
        if (!rel.startsWith("commands/"))
            continue;
        auto text = readText(buildPath(root, rel));
        if (!text.canFind(": ExCommand") && !text.canFind("mixin(\"enum AutoMeshTypedCommand"))
            continue;
        auto prefix = scenarioPrefixForCommandModule(rel);
        if (prefix.length == 0)
            failures ~= "command module has no coverage owner: " ~ rel;
        else if (!hasScenarioPrefix(prefix))
            failures ~= "command module owner has no scenario family: " ~ rel ~ " -> " ~ prefix;
    }

    require(sourceFiles.length >= 265, "source inventory should include all nijigenerate modules");
    require(failures.length == 0, "coverage inventory failures:\n" ~ failures.join("\n"));
}

private bool containsDirectMutationNeedle(string line) {
    foreach (needle; [
        ".name =",
        ".translation =",
        ".rotation =",
        ".scale =",
        ".vertices =",
        ".deformation =",
        ".parameters =",
        ".children =",
        ".values =",
        ".keypoints =",
    ]) {
        auto index = line.countUntil(needle);
        if (index >= 0 && (index + needle.length >= line.length || line[index + needle.length] != '='))
            return true;
    }
    return false;
}

private bool isAllowedDirectMutation(string rel, string line) {
    foreach (allowed; [
        "commands/depth/bone.d|deformable.deformation = offsets",
        "commands/depth/bone.d|job.keypoints = keypoints",
        "commands/depth/bone.d|root.name = name.length",
        "commands/depth/bone.d|deformable.deformation = generateInfluencePreviewOffsets",
        "commands/depth/bone.d|deformable.deformation = generateDepthBoneOffsets",
        "commands/node/base.d|newChild.localTransform.translation =",
        "commands/parameter/base.d|parent.children = parent.children.remove",
        "commands/parameter/base.d|incActivePuppet().parameters = incActivePuppet().parameters.remove",
        "commands/parameter/prop.d|param.name = newName",
        "commands/puppet/file.d|binding.node).name = binding.layer.name",
        "commands/puppet/file.d|binding.node.localTransform.translation =",
        "commands/puppet/file.d|part.localTransform.translation = localPosition",
        "commands/puppet/file.d|settings.scale = scale",
        "commands/viewport/control.d|camera.scale = vec2(zoom)",
        "commands/viewport/control.d|camera.rotation = 0",
        "panels/nodes.d|pctx.parameters = [param]",
        "panels/parameters.d|ctx.parameters =",
        "panels/viewport.d|camera.scale =",
        "viewport/base.d|camera.scale = vec2(incViewportZoom)",
        "viewport/common/mesh.d|data.vertices = positions.dup",
        "viewport/common/mesheditor/operations/impl.d|deform.vertices = value",
        "viewport/common/mesheditor/operations/impl.d|node.transform.translation =",
        "viewport/common/mesheditor/tools/grid.d|deformable.vertices =",
        "viewport/common/mesheditor/tools/onetimedeform.d|backup.binding.values = backup.values",
        "viewport/depth/mesheditor/node.d|offscreenCamera.scale =",
        "viewport/depth/mesheditor/node.d|offscreenCamera.rotation = 0",
        "viewport/vertex/mesheditor/deformable.d|this.vertices = getTarget().getVertices().toMVertices",
        "viewport/vertex/mesheditor/drawable.d|mesh.vertices = indexedVerts",
        "viewport/vertex/package.d|part.deformation = originalDeform",
        "viewport/vertex/package.d|deformEditor.vertices = ngMeshVerticesFromPositions",
        "viewport/vertex/package.d|mesh.vertices = source.vertices",
        "windows/command_browser.d|info.name =",
        "windows/command_browser.d|ctx.parameters = paramsOverride",
        "windows/editanim.d|this.name =",
        "windows/flipconfig.d|this.name = name",
        "windows/inpexport.d|settings.scale =",
        "windows/kramerge.d|binding.node).name = binding.layer.name",
        "windows/kramerge.d|binding.node.localTransform.translation =",
        "windows/kramerge.d|part.localTransform.translation = localPosition",
        "windows/parameditor.d|ctx.parameters = [param]",
        "windows/psdmerge.d|binding.node).name = binding.layer.name",
        "windows/psdmerge.d|binding.node.localTransform.translation =",
        "windows/psdmerge.d|part.localTransform.translation = localPosition",
    ]) {
        auto parts = allowed.split("|");
        if (parts.length == 2 && rel == parts[0] && line.canFind(parts[1]))
            return true;
    }
    return false;
}

private void testUndoDirectMutationAudit() {
    auto root = regressionSourceRoot("");
    string[] failures;
    foreach (prefix; ["commands", "panels", "viewport", "windows"]) {
        foreach (entry; dirEntries(buildPath(root, prefix), SpanMode.depth)) {
            if (!entry.isFile || !entry.name.endsWith(".d"))
                continue;
            auto rel = normalizeRegressionSourcePath(entry.name, root);
            auto lines = readText(entry.name).splitLines();
            foreach (i, line; lines) {
                if (!containsDirectMutationNeedle(line))
                    continue;
                if (line.stripLeft.startsWith("//"))
                    continue;
                if (!isAllowedDirectMutation(rel, line))
                    failures ~= "%s:%s: %s".format(rel, i + 1, line.stripLeft);
            }
        }
    }
    require(
        failures.length == 0,
        "unreviewed direct model/parameter/deformation mutations:\n" ~ failures.join("\n")
    );
}

private bool isRegressionCommandClassLine(string line) {
    auto stripped = line.stripLeft;
    return stripped.startsWith("class ") && stripped.canFind(": ExCommand");
}

private bool commandClassShouldBeUndoable(string className) {
    immutable nonUndoableExact = [
        "AnimEditModeCommand",
        "AutoMeshApplyActiveCommand",
        "AutoMeshGetActiveCommand",
        "AutoMeshGetSchemaCommand",
        "AutoMeshGetValuesCommand",
        "AutoMeshListProcessorsCommand",
        "AttemptRepairPuppetCommand",
        "CaptureLiveScreenshotCommand",
        "CloseProjectCommand",
        "CopyBindingCommand",
        "CopyNodeCommand",
        "CopyParameterCommand",
        "CutNodeCommand",
        "ExportINPCommand",
        "ExportJPEGCommand",
        "ExportPNGCommand",
        "ExportTGACommand",
        "ExportVideoCommand",
        "FitViewportToModelCommand",
        "GenerateFakeLayerNameCommand",
        "GetDepthBoneInfluenceRuleCommand",
        "ImportINPCommand",
        "ImportImageFolderCommand",
        "ImportKRACommand",
        "ImportPSDCommand",
        "ImportSessionDataCommand",
        "ListCommandCommand",
        "ListDepthBoneSourcesCommand",
        "ListDepthBonesCommand",
        "ListFlipPairsCommand",
        "MergeINPCommand",
        "MergeImageFilesCommand",
        "MergeKRACommand",
        "MergePSDCommand",
        "ModelEditModeCommand",
        "NewFileCommand",
        "OpenAutomeshBatchingCommand",
        "OpenFileCommand",
        "OpenFlipPairWindowCommand",
        "PremultTextureCommand",
        "PreviewDepthBoneDeformCommand",
        "PreviewDepthBoneInfluenceCommand",
        "RebleedTextureCommand",
        "RedoCommand",
        "RegenerateMipmapsCommand",
        "RegenerateNodeIDsCommand",
        "ReloadNodeCommand",
        "ResetParametersCommand",
        "ResetPhysicsCommand",
        "ResetViewportPositionCommand",
        "ResetViewportZoomCommand",
        "SaveFileCommand",
        "SaveScreenshotCommand",
        "SelectToolModeCommand",
        "SetArmedParameterAndKeypointCommand",
        "SetDefaultLayoutCommand",
        "ShowCommandBrowserWindowCommand",
        "ShowExportToINPDialogCommand",
        "ShowExportToJpegDialogCommand",
        "ShowExportToPNGDialogCommand",
        "ShowExportToTGADialogCommand",
        "ShowExportToVideoDialogCommand",
        "ShowImportINPDialogCommand",
        "ShowImportImageFolderDialogCommand",
        "ShowImportKRADialogCommand",
        "ShowImportPSDDialogCommand",
        "ShowImportSessionDataDialogCommand",
        "ShowMergeINPDialogCommand",
        "ShowMergeImageFileDialogCommand",
        "ShowMergeKRADialogCommand",
        "ShowMergePSDDialogCommand",
        "ShowOpenFileDialogCommand",
        "ShowSaveFileAsDialogCommand",
        "ShowSaveFileDialogCommand",
        "ShowSaveScreenshotDialogCommand",
        "ShowSettingsWindowCommand",
        "ShowStatusForNerdsCommand",
        "ToggleDifferenceAggregationCommand",
        "ToggleMirrorViewCommand",
        "ToggleOnionSliceCommand",
        "TogglePanelVisibilityCommand",
        "ToggleParameterArmCommand",
        "TogglePhysicsCommand",
        "TogglePostProcessCommand",
        "UndoCommand",
        "VertexModeCommand",
    ];
    if (nonUndoableExact.canFind(className))
        return false;
    if (className.startsWith("Show") || className.startsWith("List") || className.startsWith("Get") ||
        className.startsWith("Open") || className.startsWith("Export") || className.startsWith("Import") ||
        className.startsWith("Merge") || className.startsWith("Save") || className.startsWith("Capture"))
        return false;
    return true;
}

private bool commandModuleHasUndoPath(string text) {
    immutable needles = [
        "incActionPush(",
        "incActionPushGroup(",
        "incActionPopGroup(",
        ".addAction(",
        ".redo();",
        "ngAddNodes(",
        "ngInsertNodes(",
        "ngConvertTo(",
        "incMoveChildrenWithHistory(",
        "incDeleteChildrenWithHistory(",
        "incDeleteChildWithHistory(",
        "pasteFromClipboard(",
        "ngApplyDrawableMeshFromCommand(",
        "ngApplyDeformableVerticesFromCommand(",
        "ensureApplyAutoMeshCommand(",
        "ngAnimationCreateOrUpdate(",
        "ngAnimationDelete(",
        "incAnimationKeyframeAdd(",
        "incAnimationKeyframeRemove(",
    ];
    foreach (needle; needles) {
        if (text.canFind(needle))
            return true;
    }
    return false;
}

private void testUndoCommandActionsAudit() {
    auto root = regressionSourceRoot("commands");
    string[] failures;
    size_t commandClasses;
    size_t undoableClasses;

    foreach (entry; dirEntries(root, SpanMode.depth)) {
        if (!entry.isFile || !entry.name.endsWith(".d"))
            continue;

        auto rel = normalizeRegressionSourcePath(entry.name, regressionSourceRoot(""));
        auto text = readText(entry.name);
        auto moduleHasUndoPath = commandModuleHasUndoPath(text);
        foreach (lineNo, line; text.splitLines) {
            if (!isRegressionCommandClassLine(line))
                continue;
            auto className = extractRegressionClassName(line);
            if (className.length == 0)
                continue;

            commandClasses++;
            if (!commandClassShouldBeUndoable(className))
                continue;

            undoableClasses++;
            if (!moduleHasUndoPath)
                failures ~= "%s:%s: %s has no detected undo/action path".format(rel, lineNo + 1, className);
        }
    }

    require(commandClasses >= 140, "command action audit should see all command classes");
    require(undoableClasses >= 70, "command action audit should classify mutating command classes");
    require(failures.length == 0, "commands missing undo/action coverage:\n" ~ failures.join("\n"));
}

private bool containsConsoleWriteCall(string line) {
    foreach (needle; ["writefln(", "writeln(", "writef("]) {
        auto index = line.canFind(needle);
        if (index)
            return true;
    }
    return false;
}

private int braceDelta(string line) {
    int result;
    foreach (ch; line) {
        if (ch == '{')
            result++;
        else if (ch == '}')
            result--;
    }
    return result;
}

private void testPlatformWindowsConsoleWritesAreGuarded() {
    auto root = regressionSourceRoot("");
    string[] failures;
    immutable string[] allowedConsoleProtocolModules = [
        "api/acp/echo_agent.d",
    ];

    foreach (entry; dirEntries(root, SpanMode.depth)) {
        if (!entry.isFile || !entry.name.endsWith(".d"))
            continue;

        auto rel = relativePath(entry.name, root);
        auto sourcePrefix = "source/nijigenerate/";
        auto sourcePrefixIndex = rel.countUntil(sourcePrefix);
        if (sourcePrefixIndex >= 0)
            rel = rel[sourcePrefixIndex + sourcePrefix.length .. $];
        if (allowedConsoleProtocolModules.canFind(rel))
            continue;

        auto text = readText(entry.name);
        int depth;
        int[] guardedDepths;
        bool inBlockComment;

        foreach (lineNo, line; text.splitLines()) {
            while (guardedDepths.length && depth < guardedDepths[$ - 1])
                guardedDepths.length = guardedDepths.length - 1;

            auto stripped = line.stripLeft();
            auto commentOnly = inBlockComment || stripped.startsWith("//");
            auto guardLine =
                rel == "utils/crashdump.d" ||
                stripped.startsWith("debug") ||
                stripped.startsWith("static if") ||
                stripped.canFind("version(") ||
                stripped.canFind("version (") ||
                stripped.startsWith("unittest");

            auto guarded = guardedDepths.length > 0 || guardLine;
            if (!commentOnly && !guarded && containsConsoleWriteCall(line)) {
                failures ~= "%s:%s: %s".format(rel, lineNo + 1, stripped);
            }

            if (stripped.canFind("/*"))
                inBlockComment = true;
            if (inBlockComment && stripped.canFind("*/"))
                inBlockComment = false;

            auto delta = commentOnly ? 0 : braceDelta(line);
            auto newDepth = depth + delta;
            if (guardLine && delta > 0)
                guardedDepths ~= newDepth;
            depth = newDepth;
            while (guardedDepths.length && depth < guardedDepths[$ - 1])
                guardedDepths.length = guardedDepths.length - 1;
        }
    }

    require(failures.length == 0, "unguarded console write calls:\n" ~ failures.join("\n"));
}

private void testPlatformStartupShutdownModuleConstructors() {
    auto root = regressionSourceRoot("");
    string[] constructors;
    foreach (entry; dirEntries(root, SpanMode.depth)) {
        if (!entry.isFile || !entry.name.endsWith(".d"))
            continue;
        auto rel = normalizeRegressionSourcePath(entry.name, root);
        foreach (lineNo, line; readText(entry.name).splitLines()) {
            auto stripped = line.stripLeft();
            if (stripped.startsWith("//"))
                continue;
            if (
                stripped.canFind("static this()") ||
                stripped.canFind("shared static this()") ||
                stripped.canFind("static ~this()") ||
                stripped.canFind("shared static ~this()")
            ) {
                constructors ~= "%s:%s: %s".format(rel, lineNo + 1, stripped);
            }
        }
    }

    immutable string[] allowedConstructors = [
        "commands/depth/bone.d:57: shared static this() {",
        "panels/agent.d:1744: shared static ~this() {",
        "panels/nodes.d:40: static this() {",
        "panels/package.d:136: static this() {",
        "panels/resource.d:32: static this() {",
        "panels/timeline.d:32: static this() {",
        "viewport/package.d:41: static this() {",
        "widgets/output.d:160: static this() {",
    ];
    foreach (entry; constructors) {
        require(
            allowedConstructors.canFind(entry),
            "new module constructor/destructor must be reviewed for startup/shutdown cycles: " ~ entry
        );
    }
}

private bool containsDirectEnglishLiteralAfter(string line, string marker) {
    auto index = line.countUntil(marker);
    if (index < 0)
        return false;
    auto rest = line[index + marker.length .. $].stripLeft();
    if (!rest.startsWith("\""))
        return false;
    if (rest.length < 2)
        return false;
    if (rest.startsWith("\"nijigenerate\""))
        return false;
    auto first = rest[1];
    if (rest.length > 2 && rest[2] == '"')
        return false;
    return (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z');
}

private bool containsUnwrappedVisibleUiLiteral(string line) {
    foreach (call; [
        "incTooltip(",
        "incText(",
        "incTextDisabled(",
        "incTextWrapped(",
        "incTextLabel(",
        "incTextShadowed(",
        "incTextBordered(",
        "incBeginCategory(",
        "igMenuItem(",
        "igBeginMenu(",
    ]) {
        if (containsDirectEnglishLiteralAfter(line, call))
            return true;
    }

    foreach (call; ["incTextColored(", "igTextColored(", "incDialog("]) {
        auto index = line.countUntil(call);
        if (index < 0)
            continue;
        auto rest = line[index + call.length .. $];
        auto comma = rest.countUntil(",");
        if (comma < 0)
            continue;
        if (containsDirectEnglishLiteralAfter(rest[comma + 1 .. $], ""))
            return true;
    }

    return false;
}

private string decodePoQuoted(string quoted) {
    string result;
    if (quoted.length < 2 || quoted[0] != '"' || quoted[$ - 1] != '"')
        return result;
    for (size_t i = 1; i + 1 < quoted.length; i++) {
        auto c = quoted[i];
        if (c == '\\' && i + 1 < quoted.length - 1) {
            auto next = quoted[++i];
            switch (next) {
                case 'n': result ~= '\n'; break;
                case 't': result ~= '\t'; break;
                case 'r': result ~= '\r'; break;
                case '"': result ~= '"'; break;
                case '\\': result ~= '\\'; break;
                default:
                    result ~= next;
                    break;
            }
        } else {
            result ~= c;
        }
    }
    return result;
}

private string parseDStringLiteral(string line, size_t start) {
    if (start >= line.length || line[start] != '"')
        return null;
    string result;
    for (size_t i = start + 1; i < line.length; i++) {
        auto c = line[i];
        if (c == '\\' && i + 1 < line.length) {
            auto next = line[++i];
            switch (next) {
                case 'n': result ~= '\n'; break;
                case 't': result ~= '\t'; break;
                case 'r': result ~= '\r'; break;
                case '"': result ~= '"'; break;
                case '\\': result ~= '\\'; break;
                default:
                    result ~= next;
                    break;
            }
        } else if (c == '"') {
            return result;
        } else {
            result ~= c;
        }
    }
    return null;
}

private string[] extractSimpleI18nLiterals(string line) {
    string[] result;

    foreach (marker; [`_(`, `__(`]) {
        size_t cursor;
        while (cursor < line.length) {
            auto index = line[cursor .. $].countUntil(marker);
            if (index < 0)
                break;
            auto start = cursor + index + marker.length;
            while (start < line.length && (line[start] == ' ' || line[start] == '\t'))
                start++;
            auto literal = parseDStringLiteral(line, start);
            if (literal !is null && literal.length > 0)
                result ~= literal;
            cursor = start + 1;
        }
    }

    return result;
}

private string[string] readPotMsgids(string path) {
    auto lines = readText(path).splitLines();
    string[string] result;

    for (size_t i; i < lines.length; i++) {
        auto stripped = lines[i].stripLeft();
        if (!stripped.startsWith("msgid "))
            continue;

        auto value = decodePoQuoted(stripped["msgid ".length .. $]);
        size_t j = i + 1;
        while (j < lines.length) {
            auto continuation = lines[j].stripLeft();
            if (!continuation.startsWith("\""))
                break;
            value ~= decodePoQuoted(continuation);
            j++;
        }
        result[value] = value;
        i = j > i ? j - 1 : i;
    }

    return result;
}

private void testI18nPotTemplateCoversSimpleSourceLiterals() {
    auto potPath = buildPath(regressionRepoRoot(), "tl", "template.pot");
    require(exists(potPath) && isFile(potPath), "tl/template.pot should exist");
    auto msgids = readPotMsgids(potPath);

    foreach (msgid; [
        "Depth Bone",
        "Preview Depth Bone Deform",
        "Grid Deformer",
        "Path Deformer",
        "AutoMesh/Set Simple",
        "Command Browser",
        "Mask Sources",
        "Reorder Mask Source",
        "Set SimplePhysics Parameter",
        "Generate Mipmaps...",
    ]) {
        require((msgid in msgids) !is null, "tl/template.pot should include msgid: " ~ msgid);
    }
}

private void testI18nVisibleUiStringsAreWrapped() {
    auto root = regressionSourceRoot("");
    string[] failures;

    foreach (entry; dirEntries(root, SpanMode.depth)) {
        if (!entry.isFile || !entry.name.endsWith(".d"))
            continue;

        auto rel = relativePath(entry.name, root);
        auto sourcePrefix = "source/nijigenerate/";
        auto sourcePrefixIndex = rel.countUntil(sourcePrefix);
        if (sourcePrefixIndex >= 0)
            rel = rel[sourcePrefixIndex + sourcePrefix.length .. $];

        auto text = readText(entry.name);
        bool inBlockComment;
        foreach (lineNo, line; text.splitLines()) {
            auto stripped = line.stripLeft();
            auto commentOnly = inBlockComment || stripped.startsWith("//");
            if (!commentOnly && containsUnwrappedVisibleUiLiteral(stripped))
                failures ~= "%s:%s: %s".format(rel, lineNo + 1, stripped);

            if (stripped.canFind("/*"))
                inBlockComment = true;
            if (inBlockComment && stripped.canFind("*/"))
                inBlockComment = false;
        }
    }

    require(failures.length == 0, "unwrapped visible UI strings:\n" ~ failures.join("\n"));
}

private void testInspectorFormatStringsDoNotEscapeNumericFormats() {
    auto root = regressionSourceRoot("panels/inspector");
    string[] failures;

    foreach (entry; dirEntries(root, SpanMode.depth)) {
        if (!entry.isFile || !entry.name.endsWith(".d"))
            continue;

        auto text = readText(entry.name);
        foreach (lineNo, line; text.splitLines()) {
            if (line.canFind("\"%%0") || line.canFind("\"%%.")) {
                failures ~= "%s:%s: %s".format(relativePath(entry.name, regressionSourceRoot("")), lineNo + 1, line.stripLeft());
            }
        }
    }

    require(failures.length == 0, "escaped inspector numeric format strings:\n" ~ failures.join("\n"));
}

private bool runAutomatedScenario(string id) {
    switch (id) {
        case "coverage.source-command-inventory":
            runCase("coverage-source-command-inventory", &testCoverageSourceCommandInventory);
            return true;
        case "coverage.command-base":
            runCase("command-base-contracts", &testCommandBaseContracts);
            return true;
        case "coverage.full-feature-scenario-inventory":
            runCase("coverage-full-feature-scenario-inventory", &testCoverageFullFeatureScenarioInventory);
            return true;
        case "coverage.source-module-scenario-inventory":
            runCase("coverage-source-module-scenario-inventory", &testCoverageSourceModuleScenarioInventory);
            return true;
        case "project.new-open-save":
            runCase("project-new-save-open-command-paths", &testProjectNewSaveOpenCommandPaths);
            return true;
        case "io.serialization-inx":
        case "io.serialization-textures":
            runCase("project-inx-serialization-roundtrip", &testProjectINXSerializationRoundTrip);
            return true;
        case "io.save-native":
            runCase("native-save-path-overwrite-reload", &testNativeSavePathOverwriteAndReload);
            return true;
        case "io.image-codecs":
            runCase("image-codec-round-trips", &testImageCodecRoundTrips);
            return true;
        case "project.autosave-recovery":
            runCase("project-autosave-recovery-records", &testProjectAutosaveRecoveryRecords);
            return true;
        case "project.session-import":
            runCase("project-session-import-command-path", &testProjectSessionImportCommandPath);
            return true;
        case "project.recent-files":
            runCase("project-recent-files-settings", &testProjectRecentFilesSettings);
            return true;
        case "project.import-inp":
        case "project.merge-inp":
        case "project.merge-psd-kra-inp":
        case "io.serialization-inp":
        case "io.inimport-model":
            runCase("project-inp-import-merge-roundtrip-command-paths", &testProjectINPImportMergeRoundTripCommandPaths);
            runCase("project-inp-export-prunes-depth-rig-nodes", &testProjectINPExportPrunesDepthRigNodes);
            return true;
        case "project.export-inp":
        case "io.inpexport-model":
        case "depthbone.export-pruning":
            runCase("project-inp-export-prunes-depth-rig-nodes", &testProjectINPExportPrunesDepthRigNodes);
            return true;
        case "project.import-images":
        case "project.merge-images":
            runCase("project-import-images-command-paths", &testProjectImportImagesCommandPaths);
            return true;
        case "project.import-psd":
        case "project.import-kra":
        case "project.merge-psd":
        case "project.merge-kra":
        case "io.psd-reader":
        case "io.kra-reader":
            runCase("psd-kra-reader-import-merge-fixtures", &testPSDAndKRAReaderImportMergeFixtures);
            return true;
        case "project.texture-maintenance":
            runCase("project-texture-maintenance-commands", &testProjectTextureMaintenanceCommands);
            return true;
        case "project.repair-maintenance":
            runCase("project-repair-maintenance-commands", &testProjectRepairMaintenanceCommands);
            return true;
        case "atlas.pack":
            runCase("atlas-packer-rect-lifecycle", &testAtlasPackerRectLifecycle);
            return true;
        case "render.atlas-packer":
            runCase("render-atlas-packer-texture-slots", &testRenderAtlasPackerTextureSlots);
            return true;
        case "atlas.color-bleed":
            runCase("color-bleed-preserves-alpha-and-extends-color", &testColorBleedPreservesAlphaAndExtendsColor);
            return true;
        case "node.create-delete-undo":
            runCase("node-command-create-delete-toggle-undo-redo", &testNodeCommandCreateDeleteToggleUndoRedo);
            return true;
        case "node.cut-copy-paste-duplicate":
            runCase("node-clipboard-copy-paste-undo-redo", &testNodeClipboardCopyPasteUndoRedo);
            return true;
        case "node.add-types":
            runCase("node-dynamic-add-types-undo-redo", &testNodeDynamicAddTypesUndoRedo);
            return true;
        case "node.insert-types":
            runCase("node-dynamic-insert-types-undo-redo", &testNodeDynamicInsertTypesUndoRedo);
            return true;
        case "core.node-registry":
            runCase("node-registry-dynamic-command-coverage", &testNodeRegistryDynamicCommandCoverage);
            return true;
        case "node.resource-selector":
        case "core.selector-parser":
            runCase("selector-query-and-tree-store", &testSelectorQueryAndTreeStore);
            return true;
        case "node.reparent-order":
            runCase("node-command-move-undo-redo", &testNodeCommandMoveUndoRedo);
            return true;
        case "node.centralize":
            runCase("node-centralize-command-undo-redo", &testNodeCentralizeCommandUndoRedo);
            return true;
        case "node.transform-inspector":
            runCase("node-inspector-transform-undo-redo", &testNodeInspectorTransformUndoRedo);
            return true;
        case "inspectors.node-types":
        case "inspectors.node":
        case "inspectors.drawable":
        case "inspectors.composite":
        case "inspectors.camera":
            runCase("node-type-inspector-commands-undo-redo", &testNodeTypeInspectorCommandsUndoRedo);
            return true;
        case "inspectors.puppet":
            runCase("puppet-inspector-state-roundtrip", &testPuppetInspectorStateRoundTrip);
            return true;
        case "node.convert-reload":
            runCase("node-convert-command-undo-redo", &testNodeConvertCommandUndoRedo);
            return true;
        case "node.type-conversion":
            runCase("node-type-conversion-map-undo-redo", &testNodeTypeConversionMapUndoRedo);
            return true;
        case "mesh.define-grid-command":
            runCase("define-grid-command-undo-redo", &testDefineGridCommandUndoRedo);
            return true;
        case "mesh.define-mesh-command":
        case "part.uv-mesh-coherence":
            runCase("define-mesh-and-vertices-commands-undo-redo", &testDefineMeshAndVerticesCommandsUndoRedo);
            return true;
        case "mesh.common-operations":
            runCase("mesh-common-vertex-connections", &testMeshCommonVertexConnections);
            return true;
        case "mesh.operations-node":
        case "mesh.operation-node":
        case "mesh.operation-drawable":
        case "mesh.operation-deformable":
            runCase("mesh-editor-operation-targets", &testMeshEditorOperationTargets);
            return true;
        case "mesh.multi-object":
            runCase("mesh-editor-multi-object-apply-undo-redo", &testMeshEditorMultiObjectApplyUndoRedo);
            return true;
        case "mesh.mirror-symmetry":
            runCase("mesh-editor-mirror-symmetry-contracts", &testMeshEditorMirrorSymmetryContracts);
            return true;
        case "deform.grid-tool":
            runCase("grid-deformer-tool-virtual-mesh-apply-undo-redo", &testGridDeformerToolVirtualMeshApplyUndoRedo);
            return true;
        case "deform.path-tool":
            runCase("path-deformer-tool-apply-undo-redo", &testPathDeformerToolApplyUndoRedo);
            return true;
        case "deform.meshgroup-compat":
            runCase("meshgroup-griddeformer-compatibility", &testMeshGroupGridDeformerCompatibility);
            return true;
        case "deform.griddeformer-runtime":
            runCase("griddeformer-runtime-interpolation-contracts", &testGridDeformerRuntimeInterpolationContracts);
            return true;
        case "deform.pathdeformer-runtime":
            runCase("pathdeformer-runtime-contracts", &testPathDeformerRuntimeContracts);
            return true;
        case "mesh.spline":
            runCase("mesh-spline-contracts", &testMeshSplineContracts);
            return true;
        case "core.math-mesh":
            runCase("mesh-common-vertex-connections", &testMeshCommonVertexConnections);
            runCase("core-math-triangle-invariants", &testCoreMathTriangleInvariants);
            runCase("core-math-path-extraction", &testCoreMathPathExtraction);
            return true;
        case "core.math-path":
            runCase("core-math-path-extraction", &testCoreMathPathExtraction);
            return true;
        case "core.math-triangle":
            runCase("core-math-triangle-invariants", &testCoreMathTriangleInvariants);
            return true;
        case "core.math-skeletonize":
            runCase("core-math-skeletonize-invariants", &testCoreMathSkeletonizeInvariants);
            return true;
        case "core.cv-image-contours":
            runCase("core-cv-image-contours-distance-transform", &testCoreCvImageContoursDistanceTransform);
            return true;
        case "automesh.batch-undo":
            runCase("automesh-batch-config-undo-redo", &testAutoMeshBatchConfigUndoRedo);
            return true;
        case "automesh.grid-processor":
            runCase("automesh-grid-processor-deterministic-output", &testAutoMeshGridProcessorDeterministicOutput);
            return true;
        case "automesh.contour-processor":
            runCase("automesh-contour-processor-deterministic-output", &testAutoMeshContourProcessorDeterministicOutput);
            return true;
        case "automesh.skeleton-processor":
            runCase("automesh-skeleton-processor-deterministic-output", &testAutoMeshSkeletonProcessorDeterministicOutput);
            return true;
        case "automesh.optimum-processor":
            runCase("automesh-optimum-processor-deterministic-output", &testAutoMeshOptimumProcessorDeterministicOutput);
            return true;
        case "automesh.alpha-provider":
        case "automesh.non-part-targets":
            runCase("automesh-alpha-provider-non-part-cached-input", &testAutoMeshAlphaProviderAndNonPartCachedInput);
            return true;
        case "automesh.schema-values":
        case "automesh.processor-common":
            runCase("automesh-schema-values-presets-active", &testAutoMeshSchemaValuesPresetsAndActiveProcessor);
            runCase("automesh-batch-config-undo-redo", &testAutoMeshBatchConfigUndoRedo);
            return true;
        case "part.texture":
        case "inspectors.part":
            runCase("part-inspector-properties-undo-redo", &testPartInspectorPropertiesUndoRedo);
            return true;
        case "part.texture-reload":
            runCase("part-texture-reload-fixture", &testPartTextureReloadFixture);
            return true;
        case "render.texture-lifecycle":
            runCase("part-texture-reload-fixture", &testPartTextureReloadFixture);
            runCase("render-atlas-packer-texture-slots", &testRenderAtlasPackerTextureSlots);
            return true;
        case "part.clipping-mask":
            runCase("part-clipping-mask-properties-undo-redo", &testPartClippingMaskPropertiesUndoRedo);
            return true;
        case "part.mask-add-remove":
            runCase("mask-source-add-undo-redo", &testMaskSourceAddUndoRedo);
            return true;
        case "part.mask-reorder":
            runCase("mask-source-reorder-undo-redo", &testMaskSourceReorderUndoRedo);
            return true;
        case "depthbone.sources":
            runCase("depthbone-actions-undo-redo", &testDepthBoneActionsUndoRedo);
            runCase("depthbone-source-commands-undo-redo", &testDepthBoneSourceCommandsUndoRedo);
            return true;
        case "inspectors.depth-bone":
            runCase("depthbone-inspector-commands-undo-redo", &testDepthBoneInspectorCommandsUndoRedo);
            return true;
        case "inspectors.mesh-deformers":
            runCase("mesh-deformer-inspector-commands-undo-redo", &testMeshDeformerInspectorCommandsUndoRedo);
            return true;
        case "depthbone.binding-create":
            runCase("depthbone-source-commands-undo-redo", &testDepthBoneSourceCommandsUndoRedo);
            return true;
        case "depthbone.template-bones":
            runCase("depthbone-standard-skeleton-template", &testDepthBoneStandardSkeletonTemplate);
            return true;
        case "depthbone.template-parameters":
        case "parameter.template-depth-bone":
            runCase("depthbone-standard-parameter-template", &testDepthBoneStandardParameterTemplate);
            return true;
        case "depthbone.root-node":
        case "depthbone.bone-node":
        case "depthbone.serialization":
            runCase("depthbone-serialization-roundtrip", &testDepthBoneSerializationRoundTrip);
            return true;
        case "depthbone.influence-rule":
            runCase("depthbone-influence-rule-command-undo-redo", &testDepthBoneInfluenceRuleCommandUndoRedo);
            return true;
        case "depthbone.preview-commands":
            runCase("depthbone-preview-apply-commands", &testDepthBonePreviewApplyCommands);
            return true;
        case "depthbone.skinning":
            runCase("depthbone-skinning-lock-to-root-terminal", &testDepthBoneSkinningLockToRootTerminal);
            return true;
        case "depthbone.cleanup":
            runCase("depthbone-delete-cleanup-undo-redo", &testDepthBoneDeleteCleanupUndoRedo);
            return true;
        case "part.mask-mode":
            runCase("mask-source-mode-undo-redo", &testMaskSourceModeUndoRedo);
            return true;
        case "part.welding":
            runCase("welding-undo-redo", &testWeldingUndoRedo);
            return true;
        case "part.welding-runtime":
            runCase("welding-runtime-deformation", &testWeldingRuntimeDeformation);
            return true;
        case "parameter.lifecycle":
        case "parameter.groups":
            runCase("parameter-lifecycle-undo-redo", &testParameterLifecycleUndoRedo);
            runCase("parameter-command-lifecycle-undo-redo", &testParameterCommandLifecycleUndoRedo);
            runCase("parameter-command-rename-group-undo-redo", &testParameterCommandRenameAndGroupUndoRedo);
            return true;
        case "parameter.create-presets":
            runCase("parameter-create-presets", &testParameterCreatePresets);
            return true;
        case "parameter.split-window":
            runCase("parameter-split-binding-migration-undo-redo", &testParameterSplitBindingMigrationUndoRedo);
            return true;
        case "parameter.copy-paste":
        case "parameter.link":
            runCase("parameter-copy-paste-duplicate-link", &testParameterCopyPasteDuplicateAndLinkCommands);
            return true;
        case "parameter.arm-select":
            runCase("parameter-arm-select-and-keypoint-commands", &testParameterArmSelectAndKeypointCommands);
            return true;
        case "parameter.starting-keyframe":
            runCase("parameter-starting-keyframe-command-undo-redo", &testParameterStartingKeyframeCommandUndoRedo);
            return true;
        case "parameter.keyframe-basic":
            runCase("parameter-keyframe-command-undo-redo", &testParameterKeyframeCommandUndoRedo);
            return true;
        case "parameter.keyframe-mirror-fill":
            runCase("parameter-keyframe-mirror-fill-commands", &testParameterKeyframeMirrorFillCommands);
            return true;
        case "parameter.keyframe-copy-paste":
            runCase("parameter-binding-clipboard-cleanup-commands", &testParameterBindingClipboardAndCleanupCommands);
            return true;
        case "parameter.keyframe-2d":
            runCase("parameter-2d-animation-keyframe-group-undo-redo", &testParameter2DAnimationKeyframeGroupUndoRedo);
            return true;
        case "parameter.binding-interp":
            runCase("parameter-binding-interpolation", &testParameterBindingInterpolation);
            return true;
        case "parameter.binding-trs":
            runCase("parameter-trs-binding-model-command-undo-redo", &testParameterTRSBindingModelCommandUndoRedo);
            return true;
        case "parameter.binding-deform":
            runCase("parameter-deform-binding-model-command-undo-redo", &testParameterDeformBindingModelCommandUndoRedo);
            return true;
        case "parameter.binding-model":
            runCase("parameter-trs-binding-model-command-undo-redo", &testParameterTRSBindingModelCommandUndoRedo);
            runCase("parameter-deform-binding-model-command-undo-redo", &testParameterDeformBindingModelCommandUndoRedo);
            return true;
        case "parameter.axes-props":
            runCase("parameter-axes-props-command-undo-redo", &testParameterAxesPropsCommandUndoRedo);
            return true;
        case "parameter.binding-cleanup":
            runCase("parameter-binding-cleanup-on-delete-undo-redo", &testParameterBindingCleanupOnDeleteUndoRedo);
            return true;
        case "animation.lifecycle":
        case "animation.properties":
            runCase("animation-lifecycle-undo-redo", &testAnimationLifecycleUndoRedo);
            return true;
        case "animation.track-binding-cleanup":
            runCase("animation-track-binding-cleanup", &testAnimationTrackBindingCleanup);
            return true;
        case "animation.keyframes":
            runCase("animation-keyframes-undo-redo", &testAnimationKeyframesUndoRedo);
            runCase("parameter-2d-animation-keyframe-group-undo-redo", &testParameter2DAnimationKeyframeGroupUndoRedo);
            return true;
        case "node.rename-undo-merge":
            runCase("node-name-undo-merge", &testNodeNameUndoMergesPerNode);
            return true;
        case "simplephysics.parameter":
            runCase("simplephysics-parameter-undo-redo", &testSimplePhysicsParameterUndoRedo);
            return true;
        case "simplephysics.settings":
        case "simplephysics.mapping":
        case "inspectors.simplephysics":
            runCase("simplephysics-settings-undo-redo", &testSimplePhysicsSettingsUndoRedo);
            runCase("simplephysics-parameter-undo-redo", &testSimplePhysicsParameterUndoRedo);
            return true;
        case "simplephysics.serialization":
            runCase("simplephysics-serialization-roundtrip", &testSimplePhysicsSerializationRoundTrip);
            return true;
        case "undo.grouped-actions":
            runCase("action-group-undo-redo", &testActionGroupUndoRedo);
            return true;
        case "undo.action-merge":
            runCase("action-merge-semantics", &testActionMergeSemantics);
            return true;
        case "viewport.action-history":
        case "panels.action-history":
            runCase("action-history-index-modified-state", &testActionHistoryIndexAndModifiedState);
            return true;
        case "viewport.flip-pairs":
            runCase("viewport-flip-pair-commands", &testViewportFlipPairCommands);
            return true;
        case "viewport.palette-command-list":
            runCase("viewport-palette-command-discovery", &testViewportPaletteCommandDiscovery);
            return true;
        case "depth.persistence":
            runCase("depthmapped-node-serialization-roundtrip", &testDepthMappedNodeSerializationRoundTrip);
            runCase("depth-operation-helper-contracts", &testDepthOperationHelperContracts);
            return true;
        case "depth.exdepthmapped":
            runCase("depthmapped-node-serialization-roundtrip", &testDepthMappedNodeSerializationRoundTrip);
            return true;
        case "depth.sign-colors":
            runCase("depth-sign-color-contracts", &testDepthSignColorContracts);
            return true;
        case "depth.camera":
            runCase("depth-camera-projection-contracts", &testDepthCameraProjectionContracts);
            return true;
        case "render.camera":
            runCase("render-camera-export-commands", &testRenderCameraExportCommands);
            return true;
        case "depth.operation-helpers":
            runCase("depth-operation-helper-contracts", &testDepthOperationHelperContracts);
            return true;
        case "depth.commands":
            runCase("depth-map-commands-undo-redo", &testDepthMapCommandsUndoRedo);
            return true;
        case "mesh.vertex-scope":
        case "depth.edit-scope":
        case "deform.onetime-scope":
        case "deform.undo-redo-group":
        case "undo.scope-guards":
            runCase("action-stack-scope-guard", &testActionStackScopeGuard);
            return true;
        case "undo.direct-mutation-audit":
            runCase("undo-direct-mutation-audit", &testUndoDirectMutationAudit);
            return true;
        case "undo.command-actions":
            runCase("undo-command-actions-audit", &testUndoCommandActionsAudit);
            return true;
        case "settings.shortcuts":
            runCase("shortcut-settings-conflict-reload", &testShortcutSettingsConflictAndReload);
            return true;
        case "settings.default-shortcuts":
            runCase("default-shortcut-registration", &testDefaultShortcutRegistration);
            return true;
        case "settings.ui":
            runCase("typed-settings-store", &testTypedSettingsStore);
            return true;
        case "settings.ai-mcp":
            runCase("ai-mcp-settings-opt-in-persistence", &testAiMcpSettingsOptInPersistence);
            return true;
        case "api.acp-protocol":
            runCase("acp-protocol-types-error-json", &testAcpProtocolTypesAndErrorJson);
            return true;
        case "api.acp-client":
            runCase("acp-client-source-contract", &testAcpClientSourceContract);
            return true;
        case "api.external-control":
        case "api.mcp-resources":
            runCase("mcp-resource-listing-context-helpers", &testMcpResourceListingAndContextHelpers);
            runCase("mcp-task-queue-main-thread-dispatch", &testMcpTaskQueueMainThreadDispatch);
            return true;
        case "api.mcp-task-queue":
            runCase("mcp-task-queue-main-thread-dispatch", &testMcpTaskQueueMainThreadDispatch);
            return true;
        case "api.mcp-server":
            runCase("api-transport-server-contracts", &testApiTransportAndServerContracts);
            runCase("mcp-resource-listing-context-helpers", &testMcpResourceListingAndContextHelpers);
            runCase("mcp-task-queue-main-thread-dispatch", &testMcpTaskQueueMainThreadDispatch);
            return true;
        case "api.mcp-auth":
        case "api.mcp-http-transport":
        case "api.acp-stdio":
        case "api.acp-echo-agent":
            runCase("api-transport-server-contracts", &testApiTransportAndServerContracts);
            return true;
        case "settings.paths":
        case "platform.paths-dpi-fonts":
            runCase("settings-path-resolution", &testSettingsPathResolution);
            return true;
        case "platform.version":
            runCase("platform-version-metadata", &testPlatformVersionMetadata);
            return true;
        case "platform.tasks":
            runCase("platform-task-queue", &testPlatformTaskQueue);
            return true;
        case "platform.crashdump":
            runCase("platform-crashdump-generation", &testPlatformCrashDumpGeneration);
            return true;
        case "platform.windows-write":
        case "platform.debug-logging":
            runCase("platform-windows-console-writes-guarded", &testPlatformWindowsConsoleWritesAreGuarded);
            return true;
        case "platform.startup-shutdown":
            runCase("platform-startup-shutdown-module-constructors", &testPlatformStartupShutdownModuleConstructors);
            return true;
        case "i18n.wrapped-strings":
            runCase("i18n-visible-ui-strings-wrapped", &testI18nVisibleUiStringsAreWrapped);
            return true;
        case "i18n.pot":
            runCase("i18n-pot-template-covers-simple-source-literals", &testI18nPotTemplateCoversSimpleSourceLiterals);
            return true;
        case "inspectors.format-strings":
            runCase("inspector-format-strings", &testInspectorFormatStringsDoNotEscapeNumericFormats);
            return true;
        default:
            return false;
    }
}

private void printScenario(Scenario scenario) {
    writeln(scenario.status, "\t", scenario.id, "\t", scenario.category, "\t", scenario.title);
    if (scenario.note.length)
        writeln("\t", scenario.note);
}

private JSONValue scenarioToJson(Scenario scenario) {
    JSONValue[string] object;
    object["id"] = scenario.id;
    object["category"] = scenario.category;
    object["title"] = scenario.title;
    object["status"] = scenario.status;
    object["note"] = scenario.note;
    return JSONValue(object);
}

private void printList() {
    foreach (scenario; scenarios)
        printScenario(scenario);
}

private void printComputerUseManifest() {
    foreach (scenario; scenarios) {
        if (scenario.status == computerUse)
            writeln(scenarioToJson(scenario).toString());
    }
}

private void printFullFeatureInventory() {
    string[] failures;
    auto inventory = collectFullFeatureScenarioInventory(failures);
    string[] kinds;
    foreach (kind, _items; inventory)
        kinds ~= kind;
    kinds.sort();

    size_t total;
    foreach (kind; kinds) {
        auto items = inventory[kind].dup;
        items.sort();
        writeln("feature-kind\t", kind, "\t", items.length);
        total += items.length;
        foreach (item; items) {
            auto sep = item.countUntil(":");
            auto rel = sep >= 0 ? item[0 .. sep] : item;
            writeln("feature-scenario\t", kind, "\t", featureScenarioOwner(kind, rel), "\t", item);
        }
    }
    writeln("feature-total\t", total);
    if (failures.length) {
        stderr.writeln("feature inventory failures:");
        foreach (failure; failures)
            stderr.writeln(failure);
    }
}

private void printSourceModuleScenarioInventory() {
    string[] failures;
    auto inventory = collectSourceModuleScenarioInventory(failures);
    string[] prefixes;
    foreach (prefix, _modules; inventory)
        prefixes ~= prefix;
    prefixes.sort();

    size_t total;
    foreach (prefix; prefixes) {
        auto modules = inventory[prefix].dup;
        modules.sort();
        writeln("module-scenario-kind\t", prefix, "\t", modules.length);
        total += modules.length;
        foreach (mod; modules)
            writeln("module-scenario\t", prefix, "\t", mod);
    }
    writeln("module-total\t", total);
    if (failures.length) {
        stderr.writeln("source module scenario inventory failures:");
        foreach (failure; failures)
            stderr.writeln(failure);
    }
}

private int printCoverage(bool requireAll) {
    size_t automatedCount;
    size_t computerUseCount;
    size_t pendingCount;

    foreach (scenario; scenarios) {
        if (scenario.status == automated)
            automatedCount++;
        else if (scenario.status == computerUse)
            computerUseCount++;
        else
            pendingCount++;
    }

    writeln("coverage:");
    writeln("  total: ", scenarios.length);
    writeln("  automated: ", automatedCount);
    writeln("  computer-use: ", computerUseCount);
    writeln("  pending: ", pendingCount);

    if (requireAll && pendingCount) {
        stderr.writeln(
            "regression catalog is incomplete: ",
            pendingCount,
            " pending scenarios are not assigned to an automated or computer-use runner"
        );
        return 1;
    }
    return 0;
}

private int runOnly(string id) {
    foreach (scenario; scenarios) {
        if (scenario.id != id)
            continue;
        if (scenario.status != automated) {
            stderr.writeln("scenario is not automated: ", id, " (", scenario.status, ")");
            printScenario(scenario);
            return 2;
        }
        runAutomatedScenario(id);
        writeln("regression-tests: OK");
        return 0;
    }

    stderr.writeln("unknown scenario: ", id);
    return 2;
}

private int runAutomatedScenarios() {
    foreach (scenario; scenarios) {
        if (scenario.status == automated)
            require(runAutomatedScenario(scenario.id), "automated scenario has no runner: " ~ scenario.id);
    }

    writeln("regression-tests: OK");
    printCoverage(false);
    return 0;
}

private int runComputerUseScenarios(string driverCommand) {
    import std.process : executeShell;

    if (driverCommand.length == 0) {
        stderr.writeln("computer-use driver command is empty");
        return 2;
    }

    size_t executed;
    foreach (scenario; scenarios) {
        if (scenario.status != computerUse)
            continue;
        executed++;
        writeln("running computer-use\t", scenario.id);
        auto result = executeShell(driverCommand ~ " " ~ scenario.id);
        if (result.status != 0) {
            stderr.writeln("computer-use scenario failed: ", scenario.id);
            stderr.writeln(result.output);
            return result.status ? result.status : 1;
        }
    }

    writeln("computer-use-regression-tests: OK");
    writeln("computer-use executed: ", executed);
    return 0;
}

private int runComputerUseScenario(string id, string driverCommand) {
    import std.process : executeShell;

    if (driverCommand.length == 0) {
        stderr.writeln("computer-use driver command is empty");
        return 2;
    }

    foreach (scenario; scenarios) {
        if (scenario.id != id)
            continue;
        if (scenario.status != computerUse) {
            stderr.writeln("scenario is not computer-use: ", id, " (", scenario.status, ")");
            printScenario(scenario);
            return 2;
        }

        writeln("running computer-use\t", scenario.id);
        auto result = executeShell(driverCommand ~ " " ~ scenario.id);
        if (result.status != 0) {
            stderr.writeln("computer-use scenario failed: ", scenario.id);
            stderr.writeln(result.output);
            return result.status ? result.status : 1;
        }
        writeln("computer-use-regression-tests: OK");
        writeln("computer-use executed: 1");
        return 0;
    }

    stderr.writeln("unknown scenario: ", id);
    return 2;
}

private int usage() {
    writeln("usage:");
    writeln("  nijigenerate-regression-tests");
    writeln("  nijigenerate-regression-tests --list");
    writeln("  nijigenerate-regression-tests --feature-inventory");
    writeln("  nijigenerate-regression-tests --module-inventory");
    writeln("  nijigenerate-regression-tests --computer-use-manifest");
    writeln("  nijigenerate-regression-tests --coverage");
    writeln("  nijigenerate-regression-tests --require-all");
    writeln("  nijigenerate-regression-tests --only <scenario-id>");
    writeln("  nijigenerate-regression-tests --computer-use-only <scenario-id> <driver-command> [args...]");
    writeln("  nijigenerate-regression-tests --computer-use-driver <driver-command> [args...]");
    return 2;
}

int main(string[] args) {
    configureRegressionConfigDir();
    inSetTimingFunc(&regressionNow);
    incSettingsLoad();
    incActionInit();
    incInitExt();
    inRegisterNodeType!Node();
    inRegisterNodeType!Part();
    inRegisterNodeType!Composite();
    inRegisterNodeType!DynamicComposite();
    inRegisterNodeType!MeshGroup();
    inRegisterNodeType!GridDeformer();
    ngInitAllCommands();

    if (args.length == 1)
        return runAutomatedScenarios();

    if (args.length == 2 && args[1] == "--list") {
        printList();
        return 0;
    }
    if (args.length == 2 && args[1] == "--feature-inventory") {
        printFullFeatureInventory();
        return 0;
    }
    if (args.length == 2 && args[1] == "--module-inventory") {
        printSourceModuleScenarioInventory();
        return 0;
    }
    if (args.length == 2 && args[1] == "--computer-use-manifest") {
        printComputerUseManifest();
        return 0;
    }
    if (args.length == 2 && args[1] == "--coverage")
        return printCoverage(false);
    if (args.length == 2 && args[1] == "--require-all")
        return printCoverage(true);
    if (args.length == 3 && args[1] == "--only")
        return runOnly(args[2]);
    if (args.length >= 4 && args[1] == "--computer-use-only")
        return runComputerUseScenario(args[2], args[3 .. $].join(" "));
    if (args.length >= 3 && args[1] == "--computer-use-driver")
        return runComputerUseScenarios(args[2 .. $].join(" "));

    return usage();
}
