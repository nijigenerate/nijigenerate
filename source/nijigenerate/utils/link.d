/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.utils.link;
import std.process;

/**
    Opens a link with the user's preferred webbrowser
*/
void incOpenLink(string link) {
    browse(link);
}
