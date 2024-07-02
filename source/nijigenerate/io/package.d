/*
    Copyright © 2020-2023, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.io;
public import nijigenerate.io.psd;
public import nijigenerate.io.kra;
public import nijigenerate.io.inpexport;
public import nijigenerate.io.videoexport;
public import nijigenerate.io.imageexport;

import tinyfiledialogs;
public import tinyfiledialogs : TFD_Filter;
import std.string;
import std.uri;
import i18n;

import bindbc.sdl;
import nijigenerate.core;

private {
}

string incShowImportDialog(const(TFD_Filter)[] filters, string title, bool multiple = false) {
        c_str filename = tinyfd_openFileDialog(title.toStringz, "", filters, multiple);
        if (filename !is null) {
            string file = cast(string) filename.fromStringz;
            return file;
        }
        return null;
}

string incShowOpenFolderDialog(string title = "Open...") {
        c_str filename = tinyfd_selectFolderDialog(title.toStringz, null);
        if (filename !is null)
            return cast(string) filename.fromStringz;
        return null;
}

string incShowOpenDialog(const(TFD_Filter)[] filters, string title = "Open...") {
        c_str filename = tinyfd_openFileDialog(title.toStringz, "", filters, false);
        if (filename !is null) {
            string file = cast(string) filename.fromStringz;
            return file;
        }
        return null;
}

string incShowSaveDialog(const(TFD_Filter)[] filters, string fname, string title = "Save...") {
        c_str filename = tinyfd_saveFileDialog(title.toStringz, fname.toStringz, filters);
        if (filename !is null) {
            string file = cast(string) filename.fromStringz;
            return file;
        }
        return null;
}

//
// Reusable basic loaders
//

void incCreatePartsFromFiles(string[] files) {
    import std.path: baseName, extension;
    import nijilive: ShallowTexture, inTexPremultiply, Puppet, inCreateSimplePart;
    import nijigenerate.actions: incAddChildWithHistory;
    import nijigenerate.widgets: incDialog;
    import nijigenerate: incActivePuppet, incSelectedNode;

    foreach (file; files) {
        string fname = file.baseName;

        switch (fname.extension.toLower) {
            case ".png", ".tga", ".jpeg", ".jpg":
                try {
                    auto tex = new ShallowTexture(file);
                    inTexPremultiply(tex.data, tex.channels);

                    incAddChildWithHistory(
                        inCreateSimplePart(*tex, null, fname),
                        incSelectedNode(),
                        fname
                    );
                } catch (Exception ex) {

                    if (ex.msg[0 .. 11] == "unsupported") {
                        incDialog(__("Error"), _("%s is not supported").format(fname));
                    } else incDialog(__("Error"), ex.msg);
                }

                // We've added new stuff, rescan nodes
                incActivePuppet().rescanNodes();
                break;
            default: throw new Exception("Invalid file type "~fname.extension.toLower);
        }
    }
}
