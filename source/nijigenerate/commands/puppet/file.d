module nijigenerate.commands.puppet.file;

import nijigenerate.commands.base;
import nijigenerate.commands.puppet.base;

import nijilive;
import nijigenerate.ext;
import nijigenerate.io.save;
import nijigenerate.io.psd;
import nijigenerate.io.kra;
import nijigenerate.io.inimport;
import nijigenerate.io.inpexport;
import nijigenerate.io.imageexport;
import nijigenerate.io.videoexport;
import nijigenerate.project;
import nijigenerate.windows;
import nijigenerate.widgets;
import nijilive.core.animation.player : AnimationPlayer;
import i18n;
import kra : KRA;
import psd : PSD;
import std.array;
import std.algorithm.searching : canFind;
import std.conv : text;
import std.format : format;
import std.path: extension, setExtension;
import std.stdio : File;
import std.string : split;

private __gshared double ngHeadlessVideoClock;

private double ngHeadlessVideoNow() {
    return ngHeadlessVideoClock;
}

private double ngRealtimeNow() {
    return igGetTime();
}

private LoadResult!Puppet ngLoadedActivePuppet(string message) {
    auto puppet = incActivePuppet();
    return new LoadResult!Puppet(true, puppet ? [puppet] : null, message);
}

private CommandResult ngMissingPathResult(string what = "Path") {
    return CommandResult(false, what ~ " not provided");
}

private ExCamera ngResolveExportCamera(string cameraName, out string error) {
    auto puppet = incActivePuppet();
    if (!puppet) {
        error = "No active puppet";
        return null;
    }

    auto cameras = puppet.findNodesType!ExCamera(puppet.root);
    if (cameras.length == 0) {
        error = "No cameras available in the active puppet";
        return null;
    }

    if (!cameraName.length) {
        return cameras[0];
    }

    foreach (camera; cameras) {
        if (camera.name == cameraName) {
            return camera;
        }
    }

    error = format("Camera not found: %s", cameraName);
    return null;
}

private bool ngRenderCamera(ExCamera selectedCamera, bool transparency, bool postprocessing, out ubyte[] data, out int width, out int height) {
    Camera cam = selectedCamera.getCamera();
    vec2 vp = selectedCamera.getViewport();

    Camera oc;
    float or, og, ob, oa;
    int ow, oh;
    inGetViewport(ow, oh);
    oc = inGetCamera();

    width = cast(int)vp.x;
    height = cast(int)vp.y;

    inSetCamera(cam);
    inSetViewport(width, height);
    if (transparency) {
        inGetClearColor(or, og, ob, oa);
        inSetClearColor(0, 0, 0, 0);
    }

    scope(exit) {
        if (transparency) {
            inSetClearColor(or, og, ob, oa);
        }
        inSetViewport(ow, oh);
        inSetCamera(oc);
    }

    inBeginScene();
        incActivePuppet().update();
        incActivePuppet().draw();
    inEndScene();
    if (postprocessing) {
        inPostProcessScene();
    }

    data = new ubyte[inViewportDataLength()];
    inDumpViewport(data);
    return true;
}

private void ngBeginHeadlessVideoExport() {
    ngHeadlessVideoClock = 0;
    inSetTimingFunc(&ngHeadlessVideoNow);
    inUpdate();
    incActivePuppet().resetDrivers();
}

private void ngEndHeadlessVideoExport() {
    inSetTimingFunc(&ngRealtimeNow);
    incActivePuppet().resetDrivers();
}

private string ngUniqueAnimationName(Puppet puppet, string baseName) {
    string candidate = baseName.length ? baseName : "Imported";
    if (candidate !in puppet.getAnimations()) {
        return candidate;
    }

    int i = 2;
    while (true) {
        auto next = format("%s (%s)", candidate, i);
        if (next !in puppet.getAnimations()) {
            return next;
        }
        i++;
    }
}

private void ngMergeImportedPuppet(ExPuppet source, bool mergeParameters = true, bool mergeAnimations = true) {
    auto target = cast(ExPuppet)incActivePuppet();
    if (!target || !source) {
        return;
    }

    foreach (child; source.root.children.dup) {
        child.reparent(target.root, Node.OFFSET_END, true);
    }

    if (mergeParameters) {
        target.parameters ~= source.parameters;
    }

    if (mergeAnimations) {
        foreach (name, anim; source.getAnimations()) {
            target.getAnimations()[ngUniqueAnimationName(target, name)] = anim;
        }
    }

    target.rescanNodes();
    target.populateTextureSlots();
    target.root.transformChanged();
    incInitAnimationPlayer(target);
    incFocusCamera(target.root);
}

private void ngMergePSDDefault(string path, bool renameMapped, bool retranslateMapped) {
    import nijigenerate.viewport.common.mesh;
    import nijilive.core.nodes.composite.projectable : Projectable;
    import psd;
    import std.array : join;
    import std.path : baseName;

    struct Binding {
        psd.Layer layer;
        Texture layerTexture;
        Node node;
        bool replaceTexture;
        string layerPath;
    }

    void updatePart(Node node) {
        auto part = cast(Part)node;
        auto proj = cast(Projectable)node;
        if (part !is null && proj is null) {
            auto mesh = new IncMesh(part.getMesh());
            MeshData data = mesh.export_();
            data.fixWinding();

            foreach (i; 0 .. data.uvs.length) {
                auto tex = part.textures[0];
                data.uvs[i].x /= cast(float)tex.width;
                data.uvs[i].y /= cast(float)tex.height;
                data.uvs[i] += vec2(0.5, 0.5);
            }
            part.rebuffer(data);
        }
        foreach (child; node.children) {
            updatePart(child);
        }
    }

    auto puppet = incActivePuppet();
    auto parts = puppet.findNodesType!ExPart(puppet.root);

    ExPart findPartForSegment(string segment) {
        foreach (ref ExPart part; parts) {
            auto candidate = part.layerPath.length ? part.layerPath : ("/" ~ part.name);
            if (candidate == segment) return part;
        }
        return null;
    }

    ExPart findPartForName(string segment) {
        foreach (ref ExPart part; parts) {
            auto candidate = part.layerPath.length ? part.layerPath : ("/" ~ part.name);
            if (baseName(candidate) == baseName(segment)) return part;
        }
        return null;
    }

    File file = File(path);
    scope(exit) file.close();
    auto document = parseDocument(file);
    scope(exit) destroy(document);

    Binding[] bindings;
    string[] layerPathSegments;
    string calcSegment;
    foreach_reverse(layer; document.layers) {
        if (layer.type != psd.LayerType.Any) {
            if (layer.name != "</Layer set>" && layer.name != "</Layer group>") layerPathSegments ~= layer.name;
            else if (layerPathSegments.length > 0) layerPathSegments.length--;
            calcSegment = layerPathSegments.length > 0 ? "/" ~ layerPathSegments.join("/") : "";
            continue;
        }

        layer.extractLayerImage();
        inTexPremultiply(layer.data);
        auto layerTexture = new Texture(layer.data, layer.width, layer.height);
        layer.data = null;

        string currSegment = "%s/%s".format(calcSegment, layer.name);
        ExPart seg = findPartForSegment(currSegment);
        if (!seg) seg = findPartForName(currSegment);

        if (seg) {
            bindings ~= Binding(layer, layerTexture, seg, true, currSegment);
        } else {
            bindings ~= Binding(layer, layerTexture, puppet.root, false, currSegment);
        }
    }

    import std.algorithm.sorting : sort;
    bindings.sort!((a, b) => (a.replaceTexture ? a.node.depth - 1 : a.node.depth) < (b.replaceTexture ? b.node.depth - 1 : b.node.depth))();

    vec2i docCenter = vec2i(document.width / 2, document.height / 2);
    foreach (binding; bindings) {
        auto layerSize = cast(int[2])binding.layer.size();
        vec2i layerPosition = vec2i(binding.layer.left, binding.layer.top);
        vec3 worldTranslation = vec3(
            (layerPosition.x + (layerSize[0] / 2)) - cast(float)docCenter.x,
            (layerPosition.y + (layerSize[1] / 2)) - cast(float)docCenter.y,
            0
        );

        vec3 localPosition =
            binding.node
                ? Node.getRelativePosition(binding.node.transformNoLock.matrix, mat4.translation(worldTranslation))
                : worldTranslation;

        if (binding.replaceTexture) {
            localPosition =
                binding.node.parent
                    ? Node.getRelativePosition(binding.node.parent.transformNoLock.matrix, mat4.translation(worldTranslation))
                    : worldTranslation;

            binding.node.recalculateTransform = true;
            if (renameMapped) {
                (cast(ExPart)binding.node).name = binding.layer.name;
            }
            if (retranslateMapped) {
                if (binding.node.lockToRoot) binding.node.localTransform.translation = worldTranslation;
                else binding.node.localTransform.translation = localPosition;
            }

            auto part = cast(ExPart)binding.node;
            part.textures[0].dispose();
            part.textures[0] = binding.layerTexture;
            part.layerPath = binding.layerPath;
            if (binding.node.parent) {
                binding.node.parent.notifyChange(binding.node, NotifyReason.StructureChanged);
            }
        } else {
            auto part = incCreateExPart(binding.layerTexture, binding.node, binding.layer.name);
            part.layerPath = binding.layerPath;
            part.localTransform.translation = localPosition;
            if (binding.node.parent) {
                binding.node.parent.notifyChange(binding.node, NotifyReason.StructureChanged);
            }
        }
    }

    puppet.root.transformChanged();
    updatePart(puppet.root);
    puppet.populateTextureSlots();
}

private void ngMergeKRADefault(string path, bool renameMapped, bool retranslateMapped) {
    import nijigenerate.viewport.common.mesh;
    import kra;
    import std.path : baseName;

    struct Binding {
        kra.Layer layer;
        Texture layerTexture;
        Node node;
        bool replaceTexture;
        string layerPath;
    }

    void updatePart(Node node) {
        auto part = cast(Part)node;
        if (part !is null) {
            auto mesh = new IncMesh(part.getMesh());
            MeshData data = mesh.export_();
            data.fixWinding();

            foreach (i; 0 .. data.uvs.length) {
                auto tex = part.textures[0];
                data.uvs[i].x /= cast(float)tex.width;
                data.uvs[i].y /= cast(float)tex.height;
                data.uvs[i] += vec2(0.5, 0.5);
            }
            part.rebuffer(data);
        }
        foreach (child; node.children) {
            updatePart(child);
        }
    }

    auto puppet = incActivePuppet();
    auto parts = puppet.findNodesType!ExPart(puppet.root);

    ExPart findPartForSegment(string segment) {
        foreach (ref ExPart part; parts) {
            auto candidate = part.layerPath.length ? part.layerPath : ("/" ~ part.name);
            if (candidate == segment) return part;
        }
        return null;
    }

    ExPart findPartForName(string segment) {
        foreach (ref ExPart part; parts) {
            auto candidate = part.layerPath.length ? part.layerPath : ("/" ~ part.name);
            if (baseName(candidate) == baseName(segment)) return part;
        }
        return null;
    }

    auto document = parseDocument(path);
    scope(exit) destroy(document);

    Binding[] bindings;
    string[] layerPathSegments;
    string calcSegment;
    foreach (layer; document.layers) {
        if (layer.type != kra.LayerType.Any) {
            if (layer.type != kra.LayerType.SectionDivider) layerPathSegments ~= layer.name;
            else if (layerPathSegments.length > 0) layerPathSegments.length--;
            calcSegment = layerPathSegments.length > 0 ? "/" ~ layerPathSegments.join("/") : "";
            continue;
        }

        layer.extractLayerImage();
        if (layer.data.length == 0) {
            continue;
        }

        inTexPremultiply(layer.data);
        auto layerTexture = new Texture(layer.data, layer.width, layer.height);
        layer.data = null;

        string currSegment = "%s/%s".format(calcSegment, layer.name);
        ExPart seg = findPartForSegment(currSegment);
        if (!seg) seg = findPartForName(currSegment);

        if (seg) {
            bindings ~= Binding(layer, layerTexture, seg, true, currSegment);
        } else {
            bindings ~= Binding(layer, layerTexture, puppet.root, false, currSegment);
        }
    }

    import std.algorithm.sorting : sort;
    bindings.sort!((a, b) => (a.replaceTexture ? a.node.depth - 1 : a.node.depth) < (b.replaceTexture ? b.node.depth - 1 : b.node.depth))();

    vec2i docCenter = vec2i(document.width / 2, document.height / 2);
    foreach (binding; bindings) {
        auto layerSize = cast(int[2])binding.layer.size();
        vec2i layerPosition = vec2i(binding.layer.left, binding.layer.top);
        vec3 worldTranslation = vec3(
            (layerPosition.x + (layerSize[0] / 2)) - cast(float)docCenter.x,
            (layerPosition.y + (layerSize[1] / 2)) - cast(float)docCenter.y,
            0
        );

        vec3 localPosition =
            binding.node
                ? Node.getRelativePosition(binding.node.transformNoLock.matrix, mat4.translation(worldTranslation))
                : worldTranslation;

        if (binding.replaceTexture) {
            localPosition =
                binding.node.parent
                    ? Node.getRelativePosition(binding.node.parent.transformNoLock.matrix, mat4.translation(worldTranslation))
                    : worldTranslation;

            binding.node.recalculateTransform = true;
            if (renameMapped) {
                (cast(ExPart)binding.node).name = binding.layer.name;
            }
            if (retranslateMapped) {
                if (binding.node.lockToRoot) binding.node.localTransform.translation = worldTranslation;
                else binding.node.localTransform.translation = localPosition;
            }

            auto part = cast(ExPart)binding.node;
            part.textures[0].dispose();
            part.textures[0] = binding.layerTexture;
            part.layerPath = binding.layerPath;
        } else {
            auto part = incCreateExPart(binding.layerTexture, binding.node, binding.layer.name);
            part.layerPath = binding.layerPath;
            part.localTransform.translation = localPosition;
        }
    }

    puppet.root.transformChanged();
    updatePart(puppet.root);
    puppet.populateTextureSlots();
}

@McpHidden
@GuiConfirm
@EffectProjectReset
class NewFileCommand : ExCommand!() {
    this() { super(_("New"), _("Create new project.")); }

    override
    CommandResult run(Context ctx) {
        incNewProjectAsk();
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialog
class ShowOpenFileDialogCommand : ExCommand!() {
    this() { super(_("Open"), _("Show \"Open\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        incFileOpen();
        return CommandResult(true);
    }
}

@ShortcutHidden
@EffectImport
class OpenFileCommand : ExCommand!(TW!(string, "file", "specifies file path.")) {
    this(string file) { super(_("Open from file path"), _("Open puppet from specified file."), file); }

    override
    LoadResult!Puppet run(Context ctx) {
        if (file) {
            incOpenProject(file);
            auto puppet = incActivePuppet();
            return new LoadResult!Puppet(true, puppet ? [puppet] : null, "Puppet loaded");
        }
        return new LoadResult!Puppet(false, null, "File path not provided");
    }

}

@McpHidden
@GuiDialog
class ShowSaveFileDialogCommand : ExCommand!() {
    this() { super(_("Save"), _("Show \"Save\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        incFileSave();
        return CommandResult(true);
    }
}

@ShortcutHidden
@EffectFileWrite
class SaveFileCommand : ExCommand!(TW!(string, "file", "specifies file path.")) {
    this(string file) { super(_("Save to file path"), _("Save puppet to specified file."), file); }

    override
    CommandResult run(Context ctx) {
        if (file) { incSaveProject(file); return CommandResult(true); }
        return CommandResult(false, "File path not provided");
    }

}

@McpHidden
@GuiDialog
class ShowSaveFileAsDialogCommand : ExCommand!() {
    this() { super(_("Save As..."), _("Show \"Save as\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        incFileSaveAs();
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialog
class ShowImportPSDDialogCommand : ExCommand!() {
    this() { super(_("Import Photoshop Document"), _("Show \"Import Photoshop Document\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        incPopWelcomeWindow();
        incImportShowPSDDialog();
        return CommandResult(true);
    }
}

@ShortcutHidden
@EffectImport
class ImportPSDCommand : ExCommand!(
    TW!(string, "path", "path to PSD file."),
    TW!(bool, "keepStructure", "preserve layer groups as nodes."),
    TW!(string, "layerGroupNodeType", "replacement node type when preserving group structure.")
) {
    this(string path, bool keepStructure = true, string layerGroupNodeType = "DynamicComposite") {
        super(_("Import Photoshop Document from file path"), _("Import a PSD file without showing a dialog."), path, keepStructure, layerGroupNodeType);
    }

    override
    LoadResult!Puppet run(Context ctx) {
        if (!path.length) {
            return new LoadResult!Puppet(false, null, "Path not provided");
        }
        IncImportSettings settings;
        settings.keepStructure = keepStructure;
        settings.layerGroupNodeType = layerGroupNodeType.length ? layerGroupNodeType : "DynamicComposite";
        incImport!PSD(path, settings);
        return ngLoadedActivePuppet("PSD imported");
    }
}

@McpHidden
@GuiDialog
class ShowImportKRADialogCommand : ExCommand!() {
    this() { super(_("Import Krita Document"), _("Show \"Import Krita Document\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        incPopWelcomeWindow();
        incImportShowKRADialog();
        return CommandResult(true);
    }
}

@ShortcutHidden
@EffectImport
class ImportKRACommand : ExCommand!(
    TW!(string, "path", "path to KRA file."),
    TW!(bool, "keepStructure", "preserve layer groups as nodes."),
    TW!(string, "layerGroupNodeType", "replacement node type when preserving group structure.")
) {
    this(string path, bool keepStructure = true, string layerGroupNodeType = "DynamicComposite") {
        super(_("Import Krita Document from file path"), _("Import a KRA file without showing a dialog."), path, keepStructure, layerGroupNodeType);
    }

    override
    LoadResult!Puppet run(Context ctx) {
        if (!path.length) {
            return new LoadResult!Puppet(false, null, "Path not provided");
        }
        IncImportSettings settings;
        settings.keepStructure = keepStructure;
        settings.layerGroupNodeType = layerGroupNodeType.length ? layerGroupNodeType : "DynamicComposite";
        incImport!KRA(path, settings);
        return ngLoadedActivePuppet("KRA imported");
    }
}

@McpHidden
@GuiDialog
@EffectImport
class ShowImportINPDialogCommand : ExCommand!() {
    this() { super(_("Import nijilive puppet"), _("Show \"Import nijilive puppet file\" dialog.")); }

    override
    LoadResult!Puppet run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.inp"], "nijilive Puppet (*.inp)" }
        ];

        string file = incShowOpenDialog(filters, _("Import..."));
        if (file) {
            incImportINP(file);
            auto puppet = incActivePuppet();
            return new LoadResult!Puppet(true, puppet ? [puppet] : null, "Puppet imported");
        }
        return new LoadResult!Puppet(false, null, "Import canceled");
    }
}

@ShortcutHidden
@EffectImport
class ImportINPCommand : ExCommand!(TW!(string, "path", "path to INP file.")) {
    this(string path) { super(_("Import nijilive puppet from file path"), _("Import an INP file without showing a dialog."), path); }

    override
    LoadResult!Puppet run(Context ctx) {
        if (!path.length) {
            return new LoadResult!Puppet(false, null, "Path not provided");
        }
        incImportINP(path);
        return ngLoadedActivePuppet("INP imported");
    }
}

@McpHidden
@GuiDialog
@EffectImport
class ShowImportImageFolderDialogCommand : ExCommand!() {
    this() { super(_("Import Image Folder"), _("Show \"Import Image Folder\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        string folder = incShowOpenFolderDialog(_("Select a Folder..."));
        if (folder) {
            incImportFolder(folder);
            return CommandResult(true);
        }
        return CommandResult(false, "Import canceled");
    }
}

@ShortcutHidden
@EffectImport
class ImportImageFolderCommand : ExCommand!(TW!(string, "path", "path to folder containing image files.")) {
    this(string path) { super(_("Import Image Folder from file path"), _("Import a folder of image files without showing a dialog."), path); }

    override
    CommandResult run(Context ctx) {
        if (!path.length) return ngMissingPathResult();
        incImportFolder(path);
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialogWindow
class ShowMergePSDDialogCommand : ExCommand!() {
    this() { super(_("Merge Photoshop Document"), _("Show \"Merge Photoshop Document\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.psd"], "Photoshop Document (*.psd)" }
        ];

        string file = incShowOpenDialog(filters, _("Import..."));
        if (file) {
            incPopWelcomeWindow();
            incPushWindow(new PSDMergeWindow(file));
            return CommandResult(true);
        }
        return CommandResult(false, "Merge canceled");
    }
}

@ShortcutHidden
@EffectStructuralEdit
class MergePSDCommand : ExCommand!(
    TW!(string, "path", "path to PSD file."),
    TW!(bool, "renameMapped", "rename mapped parts to imported layer names."),
    TW!(bool, "retranslateMapped", "move mapped parts to imported layer positions.")
) {
    this(string path, bool renameMapped = false, bool retranslateMapped = false) {
        super(_("Merge Photoshop Document from file path"), _("Merge a PSD file into the current puppet using default mapping."), path, renameMapped, retranslateMapped);
    }

    override
    CommandResult run(Context ctx) {
        if (!path.length) return ngMissingPathResult();
        if (!incActivePuppet()) return CommandResult(false, "No active puppet");
        ngMergePSDDefault(path, renameMapped, retranslateMapped);
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialogWindow
class ShowMergeKRADialogCommand : ExCommand!() {
    this() { super(_("Merge Krita Document"), _("Show \"Merge Krita Document\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.kra"], "Krita Document (*.kra)" }
        ];

        string file = incShowOpenDialog(filters, _("Import..."));
        if (file) {
            incPopWelcomeWindow();
            incPushWindow(new KRAMergeWindow(file));
            return CommandResult(true);
        }
        return CommandResult(false, "Merge canceled");
    }
}

@ShortcutHidden
@EffectStructuralEdit
class MergeKRACommand : ExCommand!(
    TW!(string, "path", "path to KRA file."),
    TW!(bool, "renameMapped", "rename mapped parts to imported layer names."),
    TW!(bool, "retranslateMapped", "move mapped parts to imported layer positions.")
) {
    this(string path, bool renameMapped = false, bool retranslateMapped = false) {
        super(_("Merge Krita Document from file path"), _("Merge a KRA file into the current puppet using default mapping."), path, renameMapped, retranslateMapped);
    }

    override
    CommandResult run(Context ctx) {
        if (!path.length) return ngMissingPathResult();
        if (!incActivePuppet()) return CommandResult(false, "No active puppet");
        ngMergeKRADefault(path, renameMapped, retranslateMapped);
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialog
class ShowMergeImageFileDialogCommand : ExCommand!() {
    this() { super(_("Merge Image Files"), _("Show \"Merge image files\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.png"], "Portable Network Graphics (*.png)" },
            { ["*.jpeg", "*.jpg"], "JPEG Image (*.jpeg)" },
            { ["*.tga"], "TARGA Graphics (*.tga)" }
        ];

        string path = incShowImportDialog(filters, _("Import..."), true);
        if (path) {
            try {
                incCreatePartsFromFiles(path.split("|"));
                return CommandResult(true);
            } catch (Exception ex) {
                incDialog(__("Error"), ex.msg);
                return CommandResult(false, ex.msg);
            }
        }
        return CommandResult(false, "Merge canceled");
    }
}

@ShortcutHidden
@EffectStructuralEdit
class MergeImageFilesCommand : ExCommand!(TW!(string, "paths", "paths to image files, separated by |.")) {
    this(string paths) { super(_("Merge Image Files from file paths"), _("Merge image files into the current puppet without showing a dialog."), paths); }

    override
    CommandResult run(Context ctx) {
        if (!paths.length) return ngMissingPathResult("Paths");
        try {
            incCreatePartsFromFiles(paths.split("|"));
            return CommandResult(true);
        } catch (Exception ex) {
            return CommandResult(false, ex.msg);
        }
    }
}

@McpHidden
class ShowMergeINPDialogCommand : ExCommand!() {
    this() { super(_("Merge nijigenerate project"), _("Show \"Merge nijilive puppet file\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        incPopWelcomeWindow();
        // const TFD_Filter[] filters = [
        //     { ["*.inp"], "nijilive Puppet (*.inp)" }
        // ];

        // c_str filename = tinyfd_openFileDialog(__("Import..."), "", filters, false);
        // if (filename !is null) {
        //     string file = cast(string)filename.fromStringz;
        // }
        return CommandResult(true);
    }
}

@ShortcutHidden
@EffectStructuralEdit
class MergeINPCommand : ExCommand!(
    TW!(string, "path", "path to INP file."),
    TW!(bool, "mergeParameters", "merge imported parameters into the active puppet."),
    TW!(bool, "mergeAnimations", "merge imported animations into the active puppet.")
) {
    this(string path, bool mergeParameters = true, bool mergeAnimations = true) {
        super(_("Merge nijigenerate project from file path"), _("Merge an INP project into the current puppet without showing a dialog."), path, mergeParameters, mergeAnimations);
    }

    override
    CommandResult run(Context ctx) {
        if (!path.length) return ngMissingPathResult();
        auto source = cast(ExPuppet)inLoadPuppet!ExPuppet(path);
        if (!source) return CommandResult(false, "Failed to load INP");
        ngMergeImportedPuppet(source, mergeParameters, mergeAnimations);
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialog
class ShowExportToINPDialogCommand : ExCommand!() {
    this() { super(_("Export nijilive puppet"), _("Show \"Export to nijilive puppet\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.inp"], "nijilive Puppet (*.inp)" }
        ];

        string file = incShowSaveDialog(filters, "", _("Export..."));
        if (file) { incExportINP(file); return CommandResult(true); }
        return CommandResult(false, "Export canceled");
    }
}

@ShortcutHidden
@EffectFileWrite
class ExportINPCommand : ExCommand!(
    TW!(string, "file", "output path for INP file."),
    TW!(size_t, "atlasResolution", "atlas resolution."),
    TW!(bool, "nonLinearScaling", "whether oversized parts may scale individually."),
    TW!(float, "scale", "output texture scale."),
    TW!(int, "padding", "atlas padding."),
    TW!(bool, "optimizePruneUnused", "whether disabled branches are pruned.")
) {
    this(string file, size_t atlasResolution = 2048, bool nonLinearScaling = false, float scale = 1, int padding = 16, bool optimizePruneUnused = true) {
        super(_("Export nijilive puppet to file path"), _("Export an INP file without showing a dialog."), file, atlasResolution, nonLinearScaling, scale, padding, optimizePruneUnused);
    }

    override
    CommandResult run(Context ctx) {
        if (!file.length) return ngMissingPathResult("File");
        if (!incActivePuppet()) return CommandResult(false, "No active puppet");

        IncINPExportSettings settings;
        settings.atlasResolution = atlasResolution;
        settings.nonLinearScaling = nonLinearScaling;
        settings.scale = scale;
        settings.padding = padding;
        settings.optimizePruneUnused = optimizePruneUnused;

        incINPExport(incActivePuppet(), settings, file.setExtension("inp"));
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialogWindow
class ShowExportToPNGDialogCommand : ExCommand!() {
    this() { super(_("Export PNG (*.png)"), _("Show \"Export to png image\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.png"], "Portable Network Graphics (*.png)" }
        ];

        string file = incShowSaveDialog(filters, "", _("Export..."));
        if (file) { incPushWindow(new ImageExportWindow(file.setExtension("png"))); return CommandResult(true); }
        return CommandResult(false, "Export canceled");
    }
}

@ShortcutHidden
@EffectFileWrite
class ExportPNGCommand : ExCommand!(
    TW!(string, "file", "output path for PNG file."),
    TW!(string, "cameraName", "camera name. Empty selects the first camera."),
    TW!(bool, "transparency", "whether to render with transparent background."),
    TW!(bool, "postprocessing", "whether to apply post-processing.")
) {
    this(string file, string cameraName = "", bool transparency = false, bool postprocessing = false) {
        super(_("Export PNG to file path"), _("Export a PNG image without showing a dialog."), file, cameraName, transparency, postprocessing);
    }

    override
    CommandResult run(Context ctx) {
        if (!file.length) return ngMissingPathResult("File");
        string error;
        auto camera = ngResolveExportCamera(cameraName, error);
        if (!camera) return CommandResult(false, error);

        ubyte[] data;
        int width, height;
        ngRenderCamera(camera, transparency, postprocessing, data, width, height);
        incExportImage(file.setExtension("png"), data, width, height);
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialogWindow
class ShowExportToJpegDialogCommand : ExCommand!() {
    this() { super(_("Export JPEG (*.jpeg)"), _("Show \"Export to jpeg image\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.jpeg", "*.jpg"], "JPEG Image (*.jpeg)" }
        ];

        string file = incShowSaveDialog(filters, "", _("Export..."));
        if (file) { incPushWindow(new ImageExportWindow(file.setExtension("jpeg"))); return CommandResult(true); }
        return CommandResult(false, "Export canceled");
    }
}

@ShortcutHidden
@EffectFileWrite
class ExportJPEGCommand : ExCommand!(
    TW!(string, "file", "output path for JPEG file."),
    TW!(string, "cameraName", "camera name. Empty selects the first camera."),
    TW!(bool, "transparency", "whether to render with transparent background."),
    TW!(bool, "postprocessing", "whether to apply post-processing.")
) {
    this(string file, string cameraName = "", bool transparency = false, bool postprocessing = false) {
        super(_("Export JPEG to file path"), _("Export a JPEG image without showing a dialog."), file, cameraName, transparency, postprocessing);
    }

    override
    CommandResult run(Context ctx) {
        if (!file.length) return ngMissingPathResult("File");
        string error;
        auto camera = ngResolveExportCamera(cameraName, error);
        if (!camera) return CommandResult(false, error);

        ubyte[] data;
        int width, height;
        ngRenderCamera(camera, transparency, postprocessing, data, width, height);
        incExportImage(file.setExtension("jpeg"), data, width, height);
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialogWindow
class ShowExportToTGADialogCommand : ExCommand!() {
    this() { super(_("Export TARGA (*.tga)"), _("Show \"Export to TGA image\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.tga"], "TARGA Graphics (*.tga)" }
        ];

        string file = incShowSaveDialog(filters, "", _("Export..."));
        if (file) { incPushWindow(new ImageExportWindow(file.setExtension("tga"))); return CommandResult(true); }
        return CommandResult(false, "Export canceled");
    }
}

@ShortcutHidden
@EffectFileWrite
class ExportTGACommand : ExCommand!(
    TW!(string, "file", "output path for TGA file."),
    TW!(string, "cameraName", "camera name. Empty selects the first camera."),
    TW!(bool, "transparency", "whether to render with transparent background."),
    TW!(bool, "postprocessing", "whether to apply post-processing.")
) {
    this(string file, string cameraName = "", bool transparency = false, bool postprocessing = false) {
        super(_("Export TGA to file path"), _("Export a TGA image without showing a dialog."), file, cameraName, transparency, postprocessing);
    }

    override
    CommandResult run(Context ctx) {
        if (!file.length) return ngMissingPathResult("File");
        string error;
        auto camera = ngResolveExportCamera(cameraName, error);
        if (!camera) return CommandResult(false, error);

        ubyte[] data;
        int width, height;
        ngRenderCamera(camera, transparency, postprocessing, data, width, height);
        incExportImage(file.setExtension("tga"), data, width, height);
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialogWindow
class ShowExportToVideoDialogCommand : ExCommand!() {
    this() { super(_("Export Video"), _("Show \"Export to video\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.mp4"], "H.264 Video (*.mp4)" },
            { ["*.avi"], "AVI Video (*.avi)" },
            { ["*.webm"], "WebM Video (*.webm)" },
            { ["*.png"], "PNG Sequence (*.png)" }
        ];

        string file = incShowSaveDialog(filters, "", _("Export..."));
        if (file) {

            // Fallback to .mp4
            if (!extension(file)) file = file.setExtension("mp4");
            incPushWindow(new VideoExportWindow(file));
            return CommandResult(true);
        }
        return CommandResult(false, "Export canceled");
    }
}

@ShortcutHidden
@EffectFileWrite
class ExportVideoCommand : ExCommand!(
    TW!(string, "file", "output path for the video or image sequence."),
    TW!(string, "animationName", "animation name. Empty selects the first animation."),
    TW!(string, "cameraName", "camera name. Empty selects the first camera."),
    TW!(string, "codec", "ffmpeg codec tag. Empty selects auto."),
    TW!(int, "loops", "number of animation loops to export."),
    TW!(int, "framerate", "output framerate. Negative selects animation framerate."),
    TW!(bool, "transparency", "whether to render with transparent background."),
    TW!(bool, "postprocessing", "whether to apply post-processing.")
) {
    this(string file, string animationName = "", string cameraName = "", string codec = "auto", int loops = 1, int framerate = -1, bool transparency = false, bool postprocessing = false) {
        super(_("Export Video to file path"), _("Export a video without showing a dialog."), file, animationName, cameraName, codec, loops, framerate, transparency, postprocessing);
    }

    override
    CommandResult run(Context ctx) {
        import core.thread : Thread;
        import std.datetime : msecs;
        import std.math : ceil;

        if (!file.length) return ngMissingPathResult("File");
        if (!incActivePuppet()) return CommandResult(false, "No active puppet");
        if (!incVideoCanExport()) return CommandResult(false, "FFMPEG was not found, please install FFMPEG to export video");

        string cameraError;
        auto selectedCamera = ngResolveExportCamera(cameraName, cameraError);
        if (!selectedCamera) return CommandResult(false, cameraError);
        if ((cast(int)selectedCamera.getViewport().x) % 2 != 0 || (cast(int)selectedCamera.getViewport().y) % 2 != 0) {
            return CommandResult(false, "Video export requires camera size to be divisible by 2");
        }

        auto animations = incActivePuppet().getAnimations();
        if (animations.length == 0) return CommandResult(false, "No animations are defined for this model");
        auto selectedAnimation = animationName.length ? animationName : animations.keys[0];
        if (selectedAnimation !in animations) return CommandResult(false, format("Animation not found: %s", selectedAnimation));

        string selectedCodec = codec.length ? codec : "auto";
        bool codecFound = selectedCodec == "auto";
        foreach (cdc; incVideoCodecs()) {
            if (cdc.tag == selectedCodec) {
                codecFound = true;
                break;
            }
        }
        if (!codecFound) return CommandResult(false, format("Codec not found: %s", selectedCodec));

        auto player = new AnimationPlayer(incActivePuppet());
        auto playback = player.createOrGet(selectedAnimation);
        loops = clamp(loops, 1, int.max);

        float frametime = playback.animation.timestep;
        float lengthFactor = 1;
        if (framerate >= 1 && framerate != playback.fps) {
            lengthFactor = framerate / cast(float)playback.fps;
            frametime = 1.0f / cast(float)framerate;
        }

        int beginLen = cast(int)ceil(cast(float)playback.loopPointBegin * lengthFactor);
        int endLen = cast(int)ceil((cast(float)playback.animation.length - cast(float)playback.loopPointEnd) * lengthFactor);
        int loopLen = cast(int)ceil((cast(float)playback.loopPointEnd - cast(float)playback.loopPointBegin) * lengthFactor);
        int realLength = beginLen + (loopLen * loops) + endLen;

        VideoExportSettings settings;
        settings.frames = realLength;
        settings.framerate = framerate < 1 ? playback.fps : framerate;
        settings.codec = selectedCodec;
        settings.width = selectedCamera.getViewport().x;
        settings.height = selectedCamera.getViewport().y;
        settings.file = extension(file).length ? file : file.setExtension("mp4");
        settings.transparency = transparency;

        auto vctx = new VideoEncodingContext(settings);
        if (!vctx.checkState()) return CommandResult(false, vctx.errors());

        Camera cam = selectedCamera.getCamera();
        vec2 vp = selectedCamera.getViewport();
        double timestep = frametime / lengthFactor;

        ngBeginHeadlessVideoExport();
        scope(exit) ngEndHeadlessVideoExport();

        playback.play(loops > 0, true);
        player.prerenderAll();

        foreach (_; 0 .. settings.frames) {
            Camera oc;
            float or, og, ob, oa;
            int ow, oh;
            inGetViewport(ow, oh);
            oc = inGetCamera();

            inSetCamera(cam);
            inSetViewport(cast(int)vp.x, cast(int)vp.y);
            if (transparency) {
                inGetClearColor(or, og, ob, oa);
                inSetClearColor(0, 0, 0, 0);
            }

            scope(exit) {
                if (transparency) inSetClearColor(or, og, ob, oa);
                inSetViewport(ow, oh);
                inSetCamera(oc);
            }

            ngHeadlessVideoClock += timestep;
            inUpdate();
            inBeginScene();
                incActivePuppet().update();
                player.update(frametime);
                incActivePuppet().draw();
            inEndScene();
            if (postprocessing) inPostProcessScene();

            if (playback.looped >= loops) {
                playback.stop(false);
            }

            ubyte[] data = new ubyte[inViewportDataLength()];
            inDumpViewport(data);
            vctx.encodeFrame(data);
            if (!vctx.checkState()) {
                return CommandResult(false, vctx.errors());
            }
        }

        vctx.end();
        while (!vctx.hasTerminated()) {
            Thread.sleep(10.msecs);
        }
        return CommandResult(true);
    }
}

@McpHidden
@GuiConfirm
@EffectProjectReset
class CloseProjectCommand : ExCommand!() {
    this() { super(_("Close Project"), _("Close active puppet project.")); }
    
    override 
    CommandResult run(Context ctx) {
        incCloseProjectAsk();
        return CommandResult(true);
    }
}

enum FileCommand {
    NewFile,
    ShowOpenFileDialog,
    OpenFile,
    ShowSaveFileDialog,
    SaveFile,
    ShowSaveFileAsDialog,
    ShowImportPSDDialog,
    ImportPSD,
    ShowImportKRADialog,
    ImportKRA,
    ShowImportINPDialog,
    ImportINP,
    ShowImportImageFolderDialog,
    ImportImageFolder,
    ShowMergePSDDialog,
    MergePSD,
    ShowMergeKRADialog,
    MergeKRA,
    ShowMergeImageFileDialog,
    MergeImageFiles,
    ShowMergeINPDialog,
    MergeINP,
    ShowExportToINPDialog,
    ExportINP,
    ShowExportToPNGDialog,
    ExportPNG,
    ShowExportToJpegDialog,
    ExportJPEG,
    ShowExportToTGADialog,
    ExportTGA,
    ShowExportToVideoDialog,
    ExportVideo,
    CloseProject,
}


Command[FileCommand] commands;

void ngInitCommands(T)() if (is(T == FileCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!FileCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
    // Explicitly register ExCommand variants that require constructor args
    // Provide benign defaults; actual values are supplied at call-time (e.g., MCP, dialogs)
    mixin(registerCommand!(FileCommand.OpenFile, ""));
    mixin(registerCommand!(FileCommand.SaveFile, ""));
    mixin(registerCommand!(FileCommand.ImportPSD, "", true, "DynamicComposite"));
    mixin(registerCommand!(FileCommand.ImportKRA, "", true, "DynamicComposite"));
    mixin(registerCommand!(FileCommand.ImportINP, ""));
    mixin(registerCommand!(FileCommand.ImportImageFolder, ""));
    mixin(registerCommand!(FileCommand.MergePSD, "", false, false));
    mixin(registerCommand!(FileCommand.MergeKRA, "", false, false));
    mixin(registerCommand!(FileCommand.MergeImageFiles, ""));
    mixin(registerCommand!(FileCommand.MergeINP, "", true, true));
    mixin(registerCommand!(FileCommand.ExportINP, "", cast(size_t)2048, false, 1.0f, 16, true));
    mixin(registerCommand!(FileCommand.ExportPNG, "", "", false, false));
    mixin(registerCommand!(FileCommand.ExportJPEG, "", "", false, false));
    mixin(registerCommand!(FileCommand.ExportTGA, "", "", false, false));
    mixin(registerCommand!(FileCommand.ExportVideo, "", "", "", "auto", 1, -1, false, false));
}
