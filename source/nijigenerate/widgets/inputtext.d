/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.widgets.inputtext;
import nijigenerate.widgets;
import nijigenerate.core;
import nijilive;
import bindbc.sdl;
//import std.stdio;
import std.string;
import core.memory : GC;

private {

    struct TextCallbackUserData {
        char[]* buf;
    }

    char[] inputTextBuffer(string value) {
        auto buf = value.dup;
        buf ~= '\0';
        return buf;
    }

    void storeInputText(ref string dst, const(char)[] src) {
        size_t len = 0;
        while (len < src.length && src[len] != '\0') ++len;
        dst = src[0 .. len].idup;
    }
}

/**
    D compatible text input
*/
bool incInputText(string wId, ref string buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags.None) {
    auto available = incAvailableSpace();
    return incInputText(wId, available.x, buffer, flags);
}

/**
    D compatible text input
*/
bool incInputText(string wId, float width, ref string buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags.None) {

    auto textBuf = inputTextBuffer(buffer);

    // Push ID
    auto id = igGetID(wId.ptr, wId.ptr+wId.length);
    igPushID(id);
    scope(exit) igPopID();

    // Set desired width
    igPushItemWidth(width);
    scope(exit) igPopItemWidth();

    // Create callback data
    TextCallbackUserData cb;
    cb.buf = &textBuf;

    // Call ImGui's input handling
    bool changed = igInputText(
        "###INPUT",
        textBuf.ptr,
        textBuf.length,
        flags | ImGuiInputTextFlags.CallbackResize,
        cast(ImGuiInputTextCallback)(ImGuiInputTextCallbackData* data) {
            TextCallbackUserData* udata = cast(TextCallbackUserData*)data.UserData;

            // Allow resizing strings on GC heap
            if (data.EventFlag == ImGuiInputTextFlags.CallbackResize) {

                // Make sure the buffer doesn't become negatively sized.
                if (data.BufTextLen < 0) data.BufTextLen = 0;

                // Resize and pass buffer ptr in
                (*udata.buf).length = data.BufTextLen+1;

                // Keep the null terminator inside the mutable working buffer.
                data.Buf = (*udata.buf).ptr;
                data.Buf[data.BufTextLen] = '\0';
            }
            return 0;
        },
        &cb
    );
    storeInputText(buffer, textBuf);
    if (changed) {
        return true;
    }

    ImVec2 min, max;
    igGetItemRectMin(&min);
    igGetItemRectMax(&max);

    auto rect = SDL_Rect(
        cast(int)min.x+32, 
        cast(int)min.y, 
        cast(int)max.x, 
        32
    );

    SDL_SetTextInputRect(&rect);
    return false;
}
/**
    D compatible text input
*/
bool incInputText(string wId, string label, ref string buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags.None) {
    auto available = incAvailableSpace();
    return incInputText(wId, label, available.x, buffer, flags);
}

/**
    D compatible text input
*/
bool incInputText(string wId, string label, float width, ref string buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags.None) {

    auto textBuf = inputTextBuffer(buffer);

    // Push ID
    auto id = igGetID(wId.ptr, wId.ptr+wId.length);
    igPushID(id);
    scope(exit) igPopID();

    // Set desired width
    igPushItemWidth(width);
    scope(exit) igPopItemWidth();

    // Render label
    scope(success) {
        igSameLine(0, igGetStyle().ItemSpacing.x);
        igTextEx(label.ptr, label.ptr+label.length);
    }

    // Create callback data
    TextCallbackUserData cb;
    cb.buf = &textBuf;

    // Call ImGui's input handling
    bool changed = igInputText(
        "###INPUT",
        textBuf.ptr,
        textBuf.length,
        flags | ImGuiInputTextFlags.CallbackResize,
        cast(ImGuiInputTextCallback)(ImGuiInputTextCallbackData* data) {
            TextCallbackUserData* udata = cast(TextCallbackUserData*)data.UserData;

            // Allow resizing strings on GC heap
            if (data.EventFlag == ImGuiInputTextFlags.CallbackResize) {

                // Make sure the buffer doesn't become negatively sized.
                if (data.BufTextLen < 0) data.BufTextLen = 0;
            
                // Resize and pass buffer ptr in
                (*udata.buf).length = data.BufTextLen+1;

                // Keep the null terminator inside the mutable working buffer.
                data.Buf = (*udata.buf).ptr;
                data.Buf[data.BufTextLen] = '\0';
            }
            return 0;
        },
        &cb
    );
    storeInputText(buffer, textBuf);
    if (changed) {
        return true;
    }

    ImVec2 min, max;
    igGetItemRectMin(&min);
    igGetItemRectMax(&max);

    auto rect = SDL_Rect(
        cast(int)min.x+32, 
        cast(int)min.y, 
        cast(int)max.x, 
        32
    );

    SDL_SetTextInputRect(&rect);
    return false;
}

/**
    D compatible text input
*/
bool incInputTextMultiline(string wId, ref string buffer, ImVec2 size, ImGuiInputTextFlags flags = ImGuiInputTextFlags.None) {

    auto textBuf = inputTextBuffer(buffer);
/*
    // Push ID
    auto id = igGetID(wId.ptr, wId.ptr+wId.length);
    igPushID(id);
    scope(exit) igPopID();

    // Set desired width
    igPushItemWidth(width);
    scope(success) igPopItemWidth();

    // Render label
    scope(success) {
        igSameLine(0, igGetStyle().ItemSpacing.x);
        igTextEx(label.ptr, label.ptr+label.length);
    }
*/
    // Create callback data
    TextCallbackUserData cb;
    cb.buf = &textBuf;

    // Call ImGui's input handling
    bool changed = igInputTextMultiline(
        "###INPUT",
        textBuf.ptr,
        textBuf.length,
        size,
        flags | ImGuiInputTextFlags.CallbackResize,
        cast(ImGuiInputTextCallback)(ImGuiInputTextCallbackData* data) {
            TextCallbackUserData* udata = cast(TextCallbackUserData*)data.UserData;

            // Allow resizing strings on GC heap
            if (data.EventFlag == ImGuiInputTextFlags.CallbackResize) {

                // Make sure the buffer doesn't become negatively sized.
                if (data.BufTextLen < 0) data.BufTextLen = 0;
            
                // Resize and pass buffer ptr in
                (*udata.buf).length = data.BufTextLen+1;

                // Keep the null terminator inside the mutable working buffer.
                data.Buf = (*udata.buf).ptr;
                data.Buf[data.BufTextLen] = '\0';
            }
            return 0;
        },
        &cb
    );
    storeInputText(buffer, textBuf);
    if (changed) {
        return true;
    }

    ImVec2 min, max;
    igGetItemRectMin(&min);
    igGetItemRectMax(&max);

    auto rect = SDL_Rect(
        cast(int)min.x+32, 
        cast(int)min.y, 
        cast(int)max.x, 
        32
    );

    SDL_SetTextInputRect(&rect);
    return false;
}
