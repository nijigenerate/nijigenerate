module nijigenerate.panels.inspector.puppet;

import nijigenerate.viewport.vertex;
import nijigenerate.viewport.model.deform;
import nijigenerate.core;
import nijigenerate.panels;
import nijigenerate.panels.inspector.common;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate.windows;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate;
import nijilive;
import nijilive.core.nodes.common;
import std.string;
import std.algorithm.searching;
import std.algorithm.mutation;
import std.typecons: tuple;
import std.conv;
import std.utf;
import i18n;
import std.range: enumerate;

class PuppetInspector : BaseInspector!(ModelEditSubMode.Layout, Node) {
    this(Node[] targets, ModelEditSubMode mode) {
        super(targets, mode);
    }

    override
    void inspect(Parameter parameter = null, vec2u cursor = vec2u.init) {
        if (mode == ModelEditSubMode.Layout && targets.length > 0 && targets[0] == incActivePuppet().root)
            run();
    }

    override
    void run() {
        if (targets.length == 0) return;
        auto puppet = incActivePuppet;
        auto rootNode = puppet.root; 

        // Top level
        igPushID(rootNode.uuid);
            string typeString = "";
            auto len = incMeasureString(typeString);
            incText(_("Puppet"));
            igSameLine(0, 0);
            incDummy(ImVec2(-len.x, len.y));
            igSameLine(0, 0);
            incText(typeString);
        igPopID();
        igSeparator();
        
        igSpacing();
        igSpacing();

        // Version info
        {
            len = incMeasureString(_("nijilive Ver."));
            incText(puppet.meta.version_);
            igSameLine(0, 0);
            incDummy(ImVec2(-(len.x), len.y));
            igSameLine(0, 0);
            incText(_("nijilive Ver."));
        }
        
        igSpacing();
        igSpacing();

        if (incBeginCategory(__("General Info"))) {
            igPushID("Part Count");
                incTextColored(CategoryTextColor, _("Part Count"));
                incTextColored(CategoryTextColor, "%s".format(incActivePuppet().getRootParts().length));
            igPopID();
            igSpacing();

            igPushID("Name");
                igTextColored(CategoryTextColor, __("Name"));
                incTooltip(_("Name of the puppet"));
                incInputText("META_NAME", puppet.meta.name);
            igPopID();
            igSpacing();

            igPushID("Artists");
                igTextColored(CategoryTextColor, __("Artist(s)"));
                incTooltip(_("Artists who've drawn the puppet, seperated by comma"));
                incInputText("META_ARTISTS", puppet.meta.artist);
            igPopID();
            igSpacing();

            igPushID("Riggers");
                igTextColored(CategoryTextColor, __("Rigger(s)"));
                incTooltip(_("Riggers who've rigged the puppet, seperated by comma"));
                incInputText("META_RIGGERS", puppet.meta.rigger);
            igPopID();
            igSpacing();

            igPushID("Contact");
                igTextColored(CategoryTextColor, __("Contact"));
                incTooltip(_("Where to contact the main author of the puppet"));
                incInputText("META_CONTACT", puppet.meta.contact);
            igPopID();
        }
        incEndCategory();

        if (incBeginCategory(__("Licensing"))) {
            igPushID("LicenseURL");
                igTextColored(CategoryTextColor, __("License URL"));
                incTooltip(_("Link/URL to license"));
                incInputText("META_LICENSEURL", puppet.meta.licenseURL);
            igPopID();
            igSpacing();

            igPushID("Copyright");
                igTextColored(CategoryTextColor, __("Copyright"));
                incTooltip(_("Copyright holder information of the puppet"));
                incInputText("META_COPYRIGHT", puppet.meta.copyright);
            igPopID();
            igSpacing();

            igPushID("Origin");
                igTextColored(CategoryTextColor, __("Origin"));
                incTooltip(_("Where the model comes from on the internet."));
                incInputText("META_ORIGIN", puppet.meta.reference);
            igPopID();
        }
        incEndCategory();

        if (incBeginCategory(__("Physics Globals"))) {
            igPushID("PixelsPerMeter");
                incText(_("Pixels per meter"));
                incTooltip(_("Number of pixels that correspond to 1 meter in the physics engine."));
                incDragFloat("PixelsPerMeter", &puppet.physics.pixelsPerMeter, 1, 1, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
            igPopID();
            igSpacing();

            igPushID("Gravity");
                incText(_("Gravity"));
                incTooltip(_("Acceleration due to gravity, in m/s². Earth gravity is 9.8."));
                incDragFloat("Gravity", &puppet.physics.gravity, 0.01, 0, float.max, _("%.2f m/s²"), ImGuiSliderFlags.NoRoundToFormat);
            igPopID();
        }
        incEndCategory();

        if (incBeginCategory(__("Rendering Settings"))) {
            igPushID("Filtering");
                if (igCheckbox(__("Use Point Filtering"), &incActivePuppet().meta.preservePixels)) {
                    incActivePuppet().populateTextureSlots();
                    incActivePuppet().updateTextureState();
                }
                incTooltip(_("Makes nijilive model use point filtering, removing blur for low-resolution models."));
            igPopID();
        }
        incEndCategory();
    }
}