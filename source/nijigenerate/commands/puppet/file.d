module nijigenerate.commands.puppet.file;

import nijigenerate.commands.base;
import nijigenerate.commands.puppet.base;

import nijilive;
import nijigenerate.project;
import nijigenerate.windows;
import nijigenerate.widgets;
import i18n;
import std.array;
import std.path: extension, setExtension;

class NewFileCommand : ExCommand!() {
    this() { super("New", "Create new project."); }

    override
    void run(Context ctx) {
        fileNew();
    }
}

class ShowOpenFileDialogCommand : ExCommand!() {
    this() { super("Open", "Show \"Open\" dialog."); }

    override
    void run(Context ctx) {
        fileOpen();
    }
}

class OpenFileCommand : ExCommand!(TW!(string, "file", "specifies file path.")) {
    this(string file) { super("Open", "Open puppet from specified file.", file); }

    override
    void run(Context ctx) {
        if (file) incOpenProject(file);
    }
}

class ShowSaveFileDialogCommand : ExCommand!() {
    this() { super("Save", "Show \"Save\" dialog."); }

    override
    void run(Context ctx) {
        fileSave();
    }
}

class SaveFileCommand : ExCommand!(TW!(string, "file", "specifies file path.")) {
    this(string file) { super("Save", "Save puppet to specified file.", file); }

    override
    void run(Context ctx) {
        if (file) incSaveProject(file);
    }
}

class ShowSaveFileAsDialogCommand : ExCommand!() {
    this() { super("Save As...", "Show \"Save as\" dialog."); }

    override
    void run(Context ctx) {
        fileSaveAs();
    }
}

class ShowImportPSDDialogCommand : ExCommand!() {
    this() { super("Photoshop Document", "Show \"Import Photoshop Document\" dialog."); }

    override
    void run(Context ctx) {
        incPopWelcomeWindow();
        incImportShowPSDDialog();
    }
}

class ShowImportKRADialogCommand : ExCommand!() {
    this() { super("Krita Document", "Show \"Import Krita Document\" dialog."); }

    override
    void run(Context ctx) {
        incPopWelcomeWindow();
        incImportShowKRADialog();
    }
}

class ShowImportINPDialogCommand : ExCommand!() {
    this() { super("nijilive Puppet", "Show \"Import nijilive puppet file\" dialog."); }

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
    this() { super("Image Folder", "Show \"Import Image Folder\" dialog."); }

    override
    void run(Context ctx) {
        string folder = incShowOpenFolderDialog(_("Select a Folder..."));
        if (folder) {
            incImportFolder(folder);
        }
    }
}

class ShowMergePSDDialogCommand : ExCommand!() {
    this() { super("Photoshop Document", "Show \"Merge Photoshop Document\" dialog."); }

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
    this() { super("Krita Document", "Show \"Merge Krita Document\" dialog."); }

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
    this() { super("Image Files", "Show \"Merge image files\" dialog."); }

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
    this() { super("nijigenerate Project", "Show \"Merge nijilive puppet file\" dialog."); }

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
    this() { super("nijilive Puppet", "Show \"Export to nijilive puppet\" dialog."); }

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
    this() { super("PNG (*.png)", "Show \"Export to png image\" dialog."); }

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
    this() { super("JPEG (*.jpeg)", "Show \"Export to jpeg image\" dialog."); }

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
    this() { super("TARGA (*.tga)", "Show \"Export to TGA image\" dialog."); }

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
    this() { super("Video", "Show \"Export to video\" dialog."); }

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
    this() { super("Close Project", "Close active puppet project."); }
    
    override 
    void run(Context ctx) {
        // TODO: Check if changes were done to project and warn before
        // creating new project
        incNewProject();
        incPushWindow(new WelcomeWindow());        
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
private {

    static this() {
        import std.traits : EnumMembers;

        static foreach (name; EnumMembers!FileCommand) {
            static if (__traits(compiles, { mixin(registerCommand!(name)); }))
                mixin(registerCommand!(name));
        }

//        mixin(registerCommand!(NodeCommand.MoveNode, null, 0));
    }
}
