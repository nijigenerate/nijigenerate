/*
    Copyright © 2020-2023, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.tracking;
debug (InExperimental) {
import nijigenerate.core;
import nijigenerate.viewport.test;
import nijigenerate.panels;
import nijigenerate.windows;
import nijigenerate.widgets;
import nijigenerate;
import bindbc.imgui;
import nijilive;
import std.conv;
import i18n;
import std.string;

/**
    The textures frame
*/
class TrackingPanel : Panel {
private:
    string[string] optionValues;

    bool trackingModeCheckbox(string receiverName, string tooltip, TrackingMode mode) {
        bool track = incTestGetTrackingMode() == mode;

        // Appended to the name of a face tracking receiver
        // in the Tracking settings panel
        const(char)* recvName = _("%s Receiver").format(receiverName).toStringz;

        if (igCheckbox(recvName, &track)) {
            incTestSetTrackingMode(track ? mode : TrackingMode.None);
            incTestRestartTracker();
        }
        incTooltip(tooltip);
        return track;
    }

    bool canParseAddr(string addr) {
        import std.socket : parseAddress;
        try {
            parseAddress(addr);
            return true;
        } catch (Exception ex) {
            return false;
        }
    }

protected:
    override
    debug(InExperimental)
    void onUpdate() {

        if (incEditMode == EditMode.ModelTest) {
            ImVec2 avail = incAvailableSpace();

            if (trackingModeCheckbox("VMC", _("A reciever which uses your phone and associated app to track your body"), TrackingMode.VMC)) {
                auto adaptorOptions = incTestGetAdaptorOptions();

                string bindingIP = incSettingsGet("vmc_bind_ip", "0.0.0.0");
                if (incInputText(_("Bind Address"), avail.x/2, bindingIP, ImGuiInputTextFlags.None)) {
                    incSettingsSet("vmc_bind_ip", bindingIP);

                    if (this.canParseAddr(bindingIP)) {
                        incSettingsSave();
                        adaptorOptions["address"] = bindingIP;
                        incTestRestartTracker();
                    }
                }
                incTooltip(_("The IP address that the VMC binding server should listen on, default 0.0.0.0"));

                int bindingPort = incSettingsGet("vmc_bind_port", 39540);
                if (igInputInt(__("Port"), &bindingPort)) {
                    incSettingsSet("vmc_bind_port", bindingPort);

                    if (bindingPort > 1 && bindingPort < ushort.max) {
                        incSettingsSave();
                        optionValues["port"] = bindingPort.text;
                        incTestRestartTracker();
                    }
                }
                incTooltip(_("The port that the VMC binding server should listen on, default 39540"));
            }

            if (trackingModeCheckbox("VTube Studio", _("A reciever which uses the VTubeStudio iOS app"), TrackingMode.VTS)) {
                
                string bindingIP = incSettingsGet!string("vts_phone_ip");
                if (incInputText("iPhoneIP", _("iPhone IP"), avail.x/2, bindingIP, ImGuiInputTextFlags.None)) {
                    incSettingsSet("vts_phone_ip", bindingIP);

                    if (this.canParseAddr(bindingIP)) {
                        incSettingsSave();
                        optionValues["phoneIP"] = bindingIP;
                        incTestRestartTracker();
                    }
                }
                incTooltip(_("The IP Address of your iPhone,\nYou can find it in the VSeeFace Config panel in VTube Studio"));
            }

            if (trackingModeCheckbox("OpenSeeFace", _("A receiver which uses OpenSeeFace application"), TrackingMode.OSF)) {
                string bindingIP = incSettingsGet("osf_bind_ip", "0.0.0.0");
                if (incInputText("osfBindAddress", _("OSF Bind Address"), avail.x/2, bindingIP, ImGuiInputTextFlags.None)) {
                    incSettingsSet("osf_bind_ip", bindingIP);

                    if (this.canParseAddr(bindingIP)) {
                        incSettingsSave();
                        optionValues["osf_bind_ip"] = bindingIP;
                        incTestRestartTracker();
                    }
                }

                int bindingPort = incSettingsGet("osf_bind_port", 11573);
                if (igInputInt(__("OSF Listen Port"), &bindingPort)) {
                    incSettingsSet("osf_bind_port", bindingPort);

                    if (bindingPort > 1 && bindingPort < ushort.max) {
                        incSettingsSave();
                        optionValues["osf_bind_port"] = bindingPort.text;
                        incTestRestartTracker();
                    }
                }
            }


            if (igCollapsingHeader(__("Tracking Bindings"), ImGuiTreeNodeFlags.DefaultOpen)) {
                if (igBeginListBox("###BINDINGS")) {
                    foreach(i, binding; incTestGetTrackingBindings()) {
                        igPushID(cast(int)i);

                            const(char)* nm = _("%s bound to %s").format(binding.key, binding.param.name).toStringz;
                            if (igSelectable(nm)) {
                                incTestRemoveTrackingBinding(binding);
                            }

                        igPopID();
                    }
                    igEndListBox();
                }

                if (incButtonColored("", ImVec2(0, 32))) {
                    incPushWindowList(new TrackingBindingWindow(incViewportTestGetCurrBindable()));
                }
            }
        } else {
            incLabelOver(_("Not in Test Mode..."), ImVec2(0, 0), true);
        }
    }

public:
    this() {
        super("Tracking", _("Tracking"), false);
    }
}

/**
    Generate tracking panel frame
*/
debug(InExperimental) mixin incPanel!TrackingPanel;
}


