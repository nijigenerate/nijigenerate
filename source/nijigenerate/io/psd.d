/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
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
import std.exception : enforce;

import psd;

struct Traits(T: psd.PSD) {
    alias Layer = psd.Layer;
    alias LayerType = psd.LayerType;
    enum layersTopToBottom = true;
    static auto layers(T document) {
        // Some exporters store the entire record stream bottom-to-top. Accept a
        // direction only when every folder header/divider forms a balanced hierarchy.
        auto result = document.layers.dup;
        bool validHierarchy(Layer[] records) {
            size_t depth;
            foreach (layer; records) {
                if (layer.type == LayerType.OpenFolder || layer.type == LayerType.ClosedFolder) ++depth;
                else if (layer.type == LayerType.SectionDivider) {
                    if (!depth) return false;
                    --depth;
                }
            }
            return depth == 0;
        }
        if (!validHierarchy(result)) {
            result.reverse;
            enforce(validHierarchy(result), "PSD folder records do not form a balanced hierarchy");
        }
        return result;
    }
    static bool isVisible(Layer layer) { return (layer.flags & psd.LayerFlags.Visible) == 0; }
    static bool isGroupStart(Layer layer) {
        return layer.type == LayerType.OpenFolder || layer.type == LayerType.ClosedFolder;
    }
    static bool isGroupEnd(Layer layer) { return layer.type == LayerType.SectionDivider; }
    static bool isClippingLayer(Layer layer) { return !layer.clipping; }
    static bool isPassThroughGroup(Layer layer) { return layer.blendModeKey == psd.BlendingMode.PassThrough; }
    alias parseDocument = psd.parseDocument;
    alias BlendingMode = psd.BlendingMode;
}


bool incImportShowPSDDialog() {
    TFD_Filter[] filters = [{ ["*.psd"], "Photoshop Document (*.psd)" }];
    string file = incShowImportDialog(filters, _("Import..."));
    return incAskImport!PSD(file);
}

alias incAskImportPSD = incAskImport!PSD;
