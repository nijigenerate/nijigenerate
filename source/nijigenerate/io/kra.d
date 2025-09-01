/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.io.kra;
import nijigenerate;
import nijigenerate.ext;
import nijigenerate.core.tasks;
import nijigenerate.widgets.dialog;
import nijilive.math;
import nijilive;
import kra;
import i18n;
import std.format;
import nijigenerate.io;
import mir.serde;
import nijigenerate.io.inimport;

import kra;
struct Traits(T: kra.KRA) {
    alias Layer = kra.Layer;
    static auto layers(T document) { return document.layers; }
    static bool isVisible(Layer layer) { return layer.isVisible; }
    static bool isGroupStart(Layer layer) { return !layer.type == kra.LayerType.Any; }
    static bool isGroupEnd(Layer layer) { return layer.type == LayerType.SectionDivider; }
    alias parseDocument = kra.parseDocument;
    alias BlendingMode = kra.BlendingMode;
}

bool incImportShowKRADialog() {
    TFD_Filter[] filters = [{ ["*.kra"], "Krita Document (*.kra)" }];
    string file = incShowImportDialog(filters, _("Import..."));
    return incAskImport!KRA(file);
}

alias incAskImportKRA = incAskImport!KRA;