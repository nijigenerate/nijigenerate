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
import std.exception : enforce;

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
    bool clipped;
    bool passThrough;
    bool clippingReceiver;

    bool requiresClippingSurface() {
        return isLayerGroup && (clipped || clippingReceiver);
    }

    @serdeIgnore
    IncImportLayer!T clippingBase;

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
        static if (__traits(hasMember, Traits!T, "isClippingLayer"))
            clipped = Traits!T.isClippingLayer(layer);
        static if (__traits(hasMember, Traits!T, "isPassThroughGroup"))
            passThrough = isGroup && Traits!T.isPassThroughGroup(layer);

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

    // Resolve each clipping chain from its base within the sibling scope.
    void resolveClipping(IncImportLayer!T[] siblings) {
        IncImportLayer!T base;
        foreach (layer; siblings) {
            if (layer.clipped) {
                enforce(base !is null, "PSD clipping layer has no base: " ~ layer.getLayerPath());
                layer.clippingBase = base;
                base.clippingReceiver = true;
            } else base = layer;
            resolveClipping(layer.children);
        }
    }
    resolveClipping(outLayers);
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
                // Do not preserve structure; node type choice irrelevant
                incImport!T(file, IncImportSettings(false));
                return true;
            case AskKeepLayerFolder.Preserve:
                // Preserve structure; choose replacement node type for LayerGroup
                IncImportSettings s;
                s.keepStructure = true;
                // Read from settings with sane default
                import nijigenerate.core.settings : incSettingsGet;
                s.layerGroupNodeType = incSettingsGet!string("LayerGroupReplacement", "Node");
                incImport!T(file, s);
                return true;
            case AskKeepLayerFolder.Cancel:
                return false;
            default:
                throw new Exception("Invalid selection");
        }
    }
}

Node ngCreateImportGroupNode(T)(IncImportLayer!T layer, IncImportSettings settings) {
    if (layer.requiresClippingSurface())
        return inInstantiateNode("DynamicComposite", cast(Node)null);
    if (!settings.keepStructure) return null;
    if (layer.passThrough)
        enforce(layer.imageLayerRef.opacity == 255,
            "Pass-through group opacity needs an inherited opacity adapter: " ~ layer.getLayerPath());
    return inInstantiateNode(layer.passThrough ? "Node" : settings.layerGroupNodeType, cast(Node)null);
}

void ngApplyImportLayerClipping(T)(Node[IncImportLayer!T] importedNodes) {
    foreach (layer, node; importedNodes) if (layer.clippingBase !is null) {
        auto part = cast(Part)node;
        auto base = layer.clippingBase in importedNodes;
        auto drawable = base is null ? null : cast(Drawable)*base;
        enforce(part !is null && drawable !is null,
            "PSD clipping needs a Part and drawable base: " ~ layer.getLayerPath());
        part.masks ~= MaskBinding(drawable.uuid, MaskingMode.Mask, drawable);
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
        Node[IncImportLayer!T] importedNodes;

        void recurseAdd(Node parent, IncImportLayer!T layer) {
            
            Node child;
            if (layer.isLayerGroup) {
                // Retain a rendered group alpha where clipping needs it, even in flattened imports.
                child = ngCreateImportGroupNode(layer, settings);
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

                child = part;
            }

            // If `keepStructure` is disabled, `child` will be null, so we check for that
            if (child) {
                importedNodes[layer] = child;
                child.name = layer.name;
                child.zSort = -(cast(float)layer.index);
                child.reparent(parent, 0);

                // Set layer blending attributes
                child.setEnabled(Traits!T.isVisible(layer.imageLayerRef));
                if (auto part = cast(Part)child) {
                    part.blendingMode = layer.blendMode;
                    part.opacity = (cast(float)layer.imageLayerRef.opacity)/255;
                } else if (auto comp = cast(Composite)child) {
                    comp.blendingMode = layer.blendMode;
                    comp.opacity = (cast(float)layer.imageLayerRef.opacity)/255;
                }
            }

            // Traverse the sublayers tree
            foreach(sublayer; layer.children) {
                if (child)
                    recurseAdd(child, sublayer);
                else 
                    recurseAdd(parent, sublayer);
            }
        }

        foreach(layer; layers) {
            recurseAdd(puppet.root, layer);
        }

        // Restore PSD clipping as native masks before exposing the imported model.
        // The processor can then use only model data, without reopening the PSD.
        ngApplyImportLayerClipping(importedNodes);

        puppet.populateTextureSlots();
        puppet.root.transformChanged();
        foreach (child; puppet.root.children) {
            child.centralize();
        }
        puppet.root.build();
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
