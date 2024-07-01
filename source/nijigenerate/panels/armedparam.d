/*
    Copyright © 2020-2023, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.armedparam;
import nijigenerate;
import nijigenerate.panels.parameters;
import nijigenerate.panels;
import bindbc.imgui;
import i18n;

/**
    The logger frame
*/
class ArmedParametersPanel : Panel {
private:
    string filter;
    string grabParam = "";
protected:
    override
    void onUpdate() {

        auto parameters = incActivePuppet().parameters;

        if (incArmedParameter() !is null) {
            // Always render the currently armed parameter on top
            incParameterView!true(incArmedParameterIdx(), incArmedParameter(), &grabParam, false, parameters);
        }
        
    }

public:
    this() {
        super("Armed Parameters", _("Armed Parameters"), false);
    }

    override
    bool isActive() { return incArmedParameter !is null; }
}

/**
    Generate logger frame
*/
mixin incPanel!ArmedParametersPanel;
