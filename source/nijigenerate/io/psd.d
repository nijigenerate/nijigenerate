/*
    Copyright © 2020-2023, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.io.psd;
import nijigenerate;
import nijigenerate.ext;
import nijigenerate.core.tasks;
import nijigenerate.widgets.dialog;
import nijilive.math;
import nijilive;
import psd;
import i18n;
import std.format;
import nijigenerate.io;
import mir.serde;
import nijigenerate.io.inimport;
import std.algorithm.mutation;

import psd;

struct Traits(T: psd.PSD) {
    alias Layer = psd.Layer;
    alias LayerType = psd.LayerType;
    static auto layers(T document) { return document.layers.reverse; }
    static bool isVisible(Layer layer) { return (layer.flags & psd.LayerFlags.Visible) == 0; }
    static bool isGroupStart(Layer layer) { return !layer.type == psd.LayerType.Any; }
    static bool isGroupEnd(Layer layer) { return layer.name == "</Layer set>" || layer.name == "</Layer group>"; }
    alias parseDocument = psd.parseDocument;
    alias BlendingMode = psd.BlendingMode;
}


bool incImportShowPSDDialog() {
    TFD_Filter[] filters = [{ ["*.psd"], "Photoshop Document (*.psd)" }];
    string file = incShowImportDialog(filters, _("Import..."));
    return incAskImport!PSD(file);
}

alias incAskImportPSD = incAskImport!PSD;