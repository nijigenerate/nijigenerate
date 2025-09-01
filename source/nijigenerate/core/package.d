/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.core;
import nijigenerate.core.input;

public import nijigenerate.core.window;
public import nijigenerate.core.settings;
public import nijigenerate.core.actionstack;
public import nijigenerate.core.tasks;
public import nijigenerate.core.path;
public import nijigenerate.core.font;
public import nijigenerate.core.dpi; 
public import nijigenerate.core.selector;
public import nijigenerate.core.logo;



private {
    ImFont* mainFont;
}


/**
    Main font
*/
ImFont* incMainFont() {
    return mainFont;
}
