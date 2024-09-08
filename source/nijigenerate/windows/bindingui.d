/*
    Copyright © 2020-2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Lin, Yong Xiang <r888800009@gmail.com>
*/

module nijigenerate.windows.bindingui;
import i18n;
import bindbc.imgui;
import nijigenerate.io.config;
import nijigenerate.widgets.label;
import std.string;
import nijigenerate.core.settings;

/**
    our ImGui layout will look like this (UI logic):
    - incDrawAllBindings()
        - incDrawBindingEntries() 
            - incDrawBindingActionConfigEntry()
                - incDrawKeyBindingInput()
                - incDrawMouseBindingInput()
                - incDrawBindingEntry()
*/
/**
    UI for key binding entry
        entry - the key binding entry
        commited - means the key binding is commited/saved to memory config, or not
*/
void incDrawBindingEntry(AbstractBindingEntry entry, bool commited) {
    if (entry.toDelete)
        return;

    incText("\ue92b"); // delete
    if (igIsItemClicked()) {
        entry.tagDelete();
    }

    // show hint if not commited
    if (!commited) {
        igSameLine(0, 2);
        incText(_("unsaved"));
    }

    // draw the icon and binding discrption
    igSameLine(0, 2);
    if (auto mouse = cast(MouseBindingEntry) entry)
        incText("\ue323" ~ incMouseToText(mouse.getButton())
            ~ " (" ~ mouse.getMode() ~ ")"
        ); // mouse icon
    if (auto key = cast(KeyBindingEntry) entry)
        incText("\ue312" ~ incKeysToStrUI(key.getKeys())
            ~ " (" ~ key.getMode() ~ ")"
        ); // keyboard icon
}

/** 
    UI for key binding input
*/
void incDrawKeyBindingInput() {
    import std.stdio;
    // add
    incText("\ue5ca");
    if (igIsItemClicked()) {
        ImGuiKey[] keys = BindingRecorder.getRecordedKeys();
        if (keys.length > 0) {
            // swap command key if needed
            keys = incSwitchCommandKey(keys);
            incSelectedBindingEntry.append(keys);
            BindingRecorder.clearRecordedKeys();
        }
    }

    igSameLine(0, 2);

    // cancel
    incText("\ue872");
    if (igIsItemClicked())
       BindingRecorder.clearRecordedKeys();
    igSameLine(0, 2);
    incDrawRecordedKeys(BindingRecorder.getRecordedKeys());
}

void incDrawMouseBindingInput() {
    incText("\ue323"); // mouse icon
    igSameLine(0, 2);
    incText(_("\ue836 Left"));
    if (igIsItemClicked())
        incSelectedBindingEntry.append(ImGuiMouseButton.Left);
    
    igSameLine(0, 2);
    incText(_("\ue836 Middle"));
    if (igIsItemClicked())
        incSelectedBindingEntry.append(ImGuiMouseButton.Middle);

    igSameLine(0, 2);
    incText(_("\ue836 Right"));
    if (igIsItemClicked())
        incSelectedBindingEntry.append(ImGuiMouseButton.Right);
}

void incDrawBindingInput() {
    if (incAppendMouseMode)
        incDrawMouseBindingInput();
    else
        incDrawKeyBindingInput();
}

void incDrawBindingActionConfigEntry(ActionConfigEntry entry) {
    // draw ActionConfigEntry, if clicked, select the entry
    bool isSelected = incKeyBindingEntrySelected(entry);
    string itemLabel = entry.getName() ~ "##Keybind-" ~ entry.getKey();
    if (igSelectable(itemLabel.toStringz, isSelected, ImGuiSelectableFlags.None, ImVec2(0, 0))) {
        incSetSelectedBindingEntry(entry);
    }

    // draw child nodes, it is a group of AbstractBindingEntry
    igBeginGroup();
        igIndent(8);

        if (isSelected)
            incDrawBindingInput();

        foreach (bindingEntry; entry.getBindedEntries())
            incDrawBindingEntry(bindingEntry, true);
        foreach (bindingEntry; entry.getUncommittedBindedEntries())
            incDrawBindingEntry(bindingEntry, false);
            
    igEndGroup();
}

void incDrawBindingEntries(ActionConfigEntry[] entries, string category) {
    // draw category
    incText("\ue8b8"); // settings icon
    igSameLine(0, 2);
    category ~= "##Keybind-category-" ~ category;
    if (igSelectable(category.toStringz, false, ImGuiSelectableFlags.None, ImVec2(0, 0))) {
        
    }

    // draw child nodes, it is a group of ActionConfigEntry
    igBeginGroup();
        igIndent(8);
        foreach (entry; entries)
            incDrawBindingActionConfigEntry(entry);
        
    igEndGroup();
}

void incDrawMouseKeyboardSwitch() {
    if (incAppendMouseMode)
        incText(_("\ue323 Switch Add Mode")); // mouse icon
    else
        incText(_("\ue312 Switch Add Mode")); // keyboard icon
    if (igIsItemClicked())
        incAppendMouseMode = !incAppendMouseMode;
}

void incDrawCommandKeySwitch() {
    bool switchCommandKey = incSettingsGet!bool("SwapCommandControl");
    if (igCheckbox(__("Switch \ueae7 key"), &switchCommandKey)) {
        incSettingsSet("SwapCommandControl", switchCommandKey);
    }
}

void incDrawBindingFileButton() {
    // TODO: implement file handling
    incText(_("\ue5d5 reset"));
    if (igIsItemClicked())
        incConfigureDefaultBindings();

    igSameLine(0, 2);
    incText(_("\ue161 Export"));
    if (igIsItemClicked())
        incSaveBindingsShowDialog();

    igSameLine(0, 2);
    incText(_("\uf090 Import"));
    if (igIsItemClicked())
        incLoadBindingsShowDialog();


}

void incDrawAllBindings() {
    // draw all category groups of ActionConfigEntry
    igBeginGroup();
        igIndent(8);
        foreach (category; incDefaultActions.keys)
            incDrawBindingEntries(incDefaultActions[category], category);
    igEndGroup();
}

void incDrawRightLeftModifierSwitch() {
    igCheckbox(__("Append left and right modifier keys"), &BindingRecorder.appendRightLeftModifier);
}

void incDrawRecordedKeys(ImGuiKey[] keys) {
    foreach (key; keys)
        incDrawRecorderButtons(incGetKeyString(key), key, key == keys[$ - 1]);

    // Draw help text
    igSameLine(0, 2);
    if (BindingRecorder.getRecordedKeys().length == 0)
        incText(_("(Press a key to bind)"));
    else
        incText(_("(Click to remove)"));
}

void incDrawRecorderButtons(string label, ImGuiKey key, bool isLast = false) {
    igSameLine(0, 2);
    incText(label ~ (isLast ? "" : "+"));
    if (igIsItemClicked())
        BindingRecorder.removeRecordedKey(key);
}