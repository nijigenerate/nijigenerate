/*
    Copyright © 2020-2023, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.io.inimport;
import nijigenerate;
import nijigenerate.ext;
import nijigenerate.core.tasks;
import nijigenerate.widgets.dialog;
import nijilive.math;
import nijilive;
import i18n;
import std.format;
import nijigenerate.io;
import mir.serde;

struct IncImportSettings {
    bool keepStructure = true;
    string layerGroupNodeType = "DynamicComposite";
}

private {
}


class IncImportLayer(T) {
    string name;

    bool hidden;
    bool isLayerGroup;
    BlendMode blendMode;

    @serdeIgnore
    Traits!T.Layer imageLayerRef;

    @serdeIgnore
    int index;

    @serdeIgnore
    IncImportLayer!T parent;

    IncImportLayer!T[] children;

    this(Traits!T.Layer layer, bool isGroup, IncImportLayer!T parent = null, int index = 0) {
        this.parent = parent;
        this.imageLayerRef = layer;
        this.name = imageLayerRef.name;
        this.isLayerGroup = isGroup;
        this.index = index;

        switch(layer.blendModeKey) {
            case Traits!T.BlendingMode.Normal: blendMode = BlendMode.Normal; break;
            case Traits!T.BlendingMode.Multiply: blendMode = BlendMode.Multiply; break;
            case Traits!T.BlendingMode.Screen: blendMode = BlendMode.Screen; break;
            case Traits!T.BlendingMode.Overlay: blendMode = BlendMode.Overlay; break;
            case Traits!T.BlendingMode.Darken: blendMode = BlendMode.Darken; break;
            case Traits!T.BlendingMode.Lighten: blendMode = BlendMode.Lighten; break;
            case Traits!T.BlendingMode.ColorDodge: blendMode = BlendMode.ColorDodge; break;
            case Traits!T.BlendingMode.LinearDodge: blendMode = BlendMode.LinearDodge; break;
            case Traits!T.BlendingMode.ColorBurn: blendMode = BlendMode.ColorBurn; break;
            case Traits!T.BlendingMode.HardLight: blendMode = BlendMode.HardLight; break;
            case Traits!T.BlendingMode.SoftLight: blendMode = BlendMode.SoftLight; break;
            case Traits!T.BlendingMode.Difference: blendMode = BlendMode.Difference; break;
            case Traits!T.BlendingMode.Exclusion: blendMode = BlendMode.Exclusion; break;
            case Traits!T.BlendingMode.Subtract: blendMode = BlendMode.Subtract; break;
            default: blendMode = BlendMode.Normal; break;
        }
    }

    /**
        Gets the layer path
    */
    string getLayerPath() {
        return parent !is null ? parent.getLayerPath() ~ "/" ~ name : "/" ~ name;
    }

    /**
        Gets the amount of layers
    */
    int count() {
        int c = 1;
        foreach(child; children) {
            c += child.count;
        }
        return c;
    }
}

IncImportLayer!(T)[] incBuildLayerLayout(T)(T document) {
    IncImportLayer!T[] outLayers;

    IncImportLayer!T[] groupStack;
    int index = 0;
    foreach(layer; Traits!T.layers(document)) {
        index--;
        if (Traits!T.isGroupEnd(layer)) {
            if (groupStack.length == 1) {

                outLayers ~= groupStack[$-1];
                groupStack.length--;
                continue;
            } else if (groupStack.length > 1) {
                groupStack[$-2].children ~= groupStack[$-1];
                groupStack.length--;
                continue;
            }

            // uh, this should not happen?
            throw new Exception("Unexpected closing layer group");
        }

        IncImportLayer!T curLayer = new IncImportLayer!T (
            layer, 
            Traits!T.isGroupStart(layer), 
            groupStack.length > 0 ? groupStack[$-1] : null,
            index
        );

        // Add output layers in
        if (curLayer.isLayerGroup) groupStack ~= curLayer;
        else if (groupStack.length > 0) {
            groupStack[$-1].children ~= curLayer;
        } else {
            outLayers ~= curLayer;
        }

    }

    return outLayers;
}

/**
    Imports a image file of type `T` with user prompt.
    also see incAskImportKRA()
*/
bool incAskImport(T)(string file) {
    if (!file) return false;

    auto handler = new LoadHandler!T(file);
    return incKeepStructDialog(handler);
}

class LoadHandler(T) : ImportKeepHandler {
    private string file;

    this(string file) {
        super();
        this.file = file;
    }

    override
    bool load(AskKeepLayerFolder select) {
        switch (select) {
            case AskKeepLayerFolder.NotPreserve:
                incImport!T(file, IncImportSettings(false));
                return true;
            case AskKeepLayerFolder.Preserve:
                incImport!T(file, IncImportSettings(true));
                return true;
            case AskKeepLayerFolder.Cancel:
                return false;
            default:
                throw new Exception("Invalid selection");
        }
    }
}

/**
    Imports a image file of type `T`.
    Note: You should invoke incAskImport!T for UI interaction.
*/
void incImport(T)(string file, IncImportSettings settings = IncImportSettings.init) {
    incNewProject();
    // TODO: Split this up to a seperate file and make it cleaner
    try {

        T doc = Traits!T.parseDocument(file);
        IncImportLayer!T[] layers = incBuildLayerLayout!T(doc);
        vec2i docCenter = vec2i(doc.width/2, doc.height/2);
        Puppet puppet = new ExPuppet();

        void recurseAdd(Node n, IncImportLayer!T layer) {
            
            Node child;
            if (layer.isLayerGroup) {
                if (settings.keepStructure) {
                    child = inInstantiateNode(settings.layerGroupNodeType, cast(Node)null);
                    if (auto part = cast(Part)child) {
                        part.blendingMode = layer.blendMode;
                        part.opacity = (cast(float)layer.imageLayerRef.opacity)/255;
                    } else if (auto comp = cast(Part)child) {
                        comp.opacity = (cast(float)layer.imageLayerRef.opacity)/255;
                    }
                }
            } else {
                
                layer.imageLayerRef.extractLayerImage();
                inTexPremultiply(layer.imageLayerRef.data);
                auto tex = new Texture(layer.imageLayerRef.data, layer.imageLayerRef.width, layer.imageLayerRef.height);
                ExPart part = incCreateExPart(tex, null, layer.name);
                part.layerPath = layer.getLayerPath();

                auto layerSize = cast(int[2])layer.imageLayerRef.size();
                vec2i layerPosition = vec2i(
                    layer.imageLayerRef.left,
                    layer.imageLayerRef.top
                );

                // TODO: more intelligent placement
                part.localTransform.translation = vec3(
                    (layerPosition.x+(layerSize[0]/2))-docCenter.x,
                    (layerPosition.y+(layerSize[1]/2))-docCenter.y,
                    0
                );


                part.setEnabled(Traits!T.isVisible(layer.imageLayerRef));
                part.opacity = (cast(float)layer.imageLayerRef.opacity)/255;
                part.blendingMode = layer.blendMode;

                child = part;
            }

            if (child) {
                child.name = layer.name;
                child.zSort = -(cast(float)layer.index);
                child.reparent(n, 0);
            }

            // Add children
            foreach(sublayer; layer.children) {
                if (settings.keepStructure) {

                    // Normal adding
                    recurseAdd(child, sublayer);
                } else {

                    if (auto composite = cast(Composite)child) {
                    
                        // Composite child iteration
                        recurseAdd(composite, sublayer);
                    } else {

                        // Non-composite child iteration
                        recurseAdd(n, sublayer);
                    }
                }
            }
        }

        foreach(layer; layers) {
            recurseAdd(puppet.root, layer);
        }

        puppet.populateTextureSlots();
        puppet.root.transformChanged();
        puppet.root.centralize();
        incActiveProject().puppet = puppet;
        incFocusCamera(incActivePuppet().root);

        incInitAnimationPlayer(puppet);

        incSetStatus(_("%s was imported...".format(file)));
    } catch (Exception ex) {

        incSetStatus(_("Import failed..."));
        incDialog(__("Error"), _("An error occured during %s import:\n%s").format(typeid(T).name, ex.msg));
    }
    incFreeMemory();
}
