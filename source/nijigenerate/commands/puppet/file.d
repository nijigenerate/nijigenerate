module nijigenerate.commands.puppet.file;

import nijigenerate.commands.base;
import nijigenerate.commands.puppet.base;

import nijilive;
import nijigenerate.io.save;
import nijigenerate.project;
import nijigenerate.windows;
import nijigenerate.widgets;
import i18n;
import std.array;
import std.path: extension, setExtension;

class NewFileCommand : ExCommand!() {
    this() { super(_("New"), _("Create new project.")); }

    override
    void run(Context ctx) {
        incNewProjectAsk();
    }
}

class ShowOpenFileDialogCommand : ExCommand!() {
    this() { super(_("Open"), _("Show \"Open\" dialog.")); }

    override
    void run(Context ctx) {
        incFileOpen();
    }
}

class OpenFileCommand : ExCommand!(TW!(string, "file", "specifies file path.")) {
    this(string file) { super(_("Open from file path"), _("Open puppet from specified file."), file); }

    override
    void run(Context ctx) {
        if (file) incOpenProject(file);
    }

    // Do not expose direct-execution variant to shortcut editor (use dialog command)
    override bool shortcutRunnable() { return false; }
}

class ShowSaveFileDialogCommand : ExCommand!() {
    this() { super(_("Save"), _("Show \"Save\" dialog.")); }

    override
    void run(Context ctx) {
        incFileSave();
    }
}

class SaveFileCommand : ExCommand!(TW!(string, "file", "specifies file path.")) {
    this(string file) { super(_("Save to file path"), _("Save puppet to specified file."), file); }

    override
    void run(Context ctx) {
        if (file) incSaveProject(file);
    }

    // Do not expose direct-execution variant to shortcut editor (use dialog command)
    override bool shortcutRunnable() { return false; }
}

class ShowSaveFileAsDialogCommand : ExCommand!() {
    this() { super(_("Save As..."), _("Show \"Save as\" dialog.")); }

    override
    void run(Context ctx) {
        incFileSaveAs();
    }
}

class ShowImportPSDDialogCommand : ExCommand!() {
    this() { super(_("Import Photoshop Document"), _("Show \"Import Photoshop Document\" dialog.")); }

    override
    void run(Context ctx) {
        incPopWelcomeWindow();
        incImportShowPSDDialog();
    }
}

class ShowImportKRADialogCommand : ExCommand!() {
    this() { super(_("Import Krita Document"), _("Show \"Import Krita Document\" dialog.")); }

    override
    void run(Context ctx) {
        incPopWelcomeWindow();
        incImportShowKRADialog();
    }
}

class ShowImportINPDialogCommand : ExCommand!() {
    this() { super(_("Import nijilive puppet"), _("Show \"Import nijilive puppet file\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.inp"], "nijilive Puppet (*.inp)" }
        ];

        string file = incShowOpenDialog(filters, _("Import..."));
        if (file) {
            incImportINP(file);
        }
    }
}

class ShowImportImageFolderDialogCommand : ExCommand!() {
    this() { super(_("Import Image Folder"), _("Show \"Import Image Folder\" dialog.")); }

    override
    void run(Context ctx) {
        string folder = incShowOpenFolderDialog(_("Select a Folder..."));
        if (folder) {
            incImportFolder(folder);
        }
    }
}

class ShowMergePSDDialogCommand : ExCommand!() {
    this() { super(_("Merge Photoshop Document"), _("Show \"Merge Photoshop Document\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.psd"], "Photoshop Document (*.psd)" }
        ];

        string file = incShowOpenDialog(filters, _("Import..."));
        if (file) {
            incPopWelcomeWindow();
            incPushWindow(new PSDMergeWindow(file));
        }
    }
}

class ShowMergeKRADialogCommand : ExCommand!() {
    this() { super(_("Merge Krita Document"), _("Show \"Merge Krita Document\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.kra"], "Krita Document (*.kra)" }
        ];

        string file = incShowOpenDialog(filters, _("Import..."));
        if (file) {
            incPopWelcomeWindow();
            incPushWindow(new KRAMergeWindow(file));
        }
    }
}

class ShowMergeImageFileDialogCommand : ExCommand!() {
    this() { super(_("Merge Image Files"), _("Show \"Merge image files\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.png"], "Portable Network Graphics (*.png)" },
            { ["*.jpeg", "*.jpg"], "JPEG Image (*.jpeg)" },
            { ["*.tga"], "TARGA Graphics (*.tga)" }
        ];

        string path = incShowImportDialog(filters, _("Import..."), true);
        if (path) {
            try {
                incCreatePartsFromFiles(path.split("|"));
            } catch (Exception ex) {
                incDialog(__("Error"), ex.msg);
            }
        }
    }
}

class ShowMergeINPDialogCommand : ExCommand!() {
    this() { super(_("Merge nijigenerate project"), _("Show \"Merge nijilive puppet file\" dialog.")); }

    override
    void run(Context ctx) {
        incPopWelcomeWindow();
        // const TFD_Filter[] filters = [
        //     { ["*.inp"], "nijilive Puppet (*.inp)" }
        // ];

        // c_str filename = tinyfd_openFileDialog(__("Import..."), "", filters, false);
        // if (filename !is null) {
        //     string file = cast(string)filename.fromStringz;
        // }
    }
}

class ShowExportToINPDialogCommand : ExCommand!() {
    this() { super(_("Export nijilive puppet"), _("Show \"Export to nijilive puppet\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.inp"], "nijilive Puppet (*.inp)" }
        ];

        string file = incShowSaveDialog(filters, "", _("Export..."));
        if (file) incExportINP(file);
    }
}

class ShowExportToPNGDialogCommand : ExCommand!() {
    this() { super(_("Export PNG (*.png)"), _("Show \"Export to png image\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.png"], "Portable Network Graphics (*.png)" }
        ];

        string file = incShowSaveDialog(filters, "", _("Export..."));
        if (file) incPushWindow(new ImageExportWindow(file.setExtension("png")));
    }
}

class ShowExportToJpegDialogCommand : ExCommand!() {
    this() { super(_("Export JPEG (*.jpeg)"), _("Show \"Export to jpeg image\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.jpeg", "*.jpg"], "JPEG Image (*.jpeg)" }
        ];

        string file = incShowSaveDialog(filters, "", _("Export..."));
        if (file) incPushWindow(new ImageExportWindow(file.setExtension("jpeg")));
    }
}

class ShowExportToTGADialogCommand : ExCommand!() {
    this() { super(_("Export TARGA (*.tga)"), _("Show \"Export to TGA image\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.tga"], "TARGA Graphics (*.tga)" }
        ];

        string file = incShowSaveDialog(filters, "", _("Export..."));
        if (file) incPushWindow(new ImageExportWindow(file.setExtension("tga")));
    }
}

class ShowExportToVideoDialogCommand : ExCommand!() {
    this() { super(_("Export Video"), _("Show \"Export to video\" dialog.")); }

    override
    void run(Context ctx) {
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
        }
    }
}

class CloseProjectCommand : ExCommand!() {
    this() { super(_("Close Project"), _("Close active puppet project.")); }
    
    override 
    void run(Context ctx) {
        incCloseProjectAsk();
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
    ShowImportKRADialog,
    ShowImportINPDialog,
    ShowImportImageFolderDialog,
    ShowMergePSDDialog,
    ShowMergeKRADialog,
    ShowMergeImageFileDialog,
    ShowMergeINPDialog,
    ShowExportToINPDialog,
    ShowExportToPNGDialog,
    ShowExportToJpegDialog,
    ShowExportToTGADialog,
    ShowExportToVideoDialog,
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
}
