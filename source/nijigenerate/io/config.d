/*
    Copyright © 2020-2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Lin, Yong Xiang <r888800009@gmail.com>
*/

module nijigenerate.io.config;
import nijigenerate.core;
import nijigenerate.core.input;
import nijigenerate.io;
import nijigenerate.widgets.label;

import std.string;
import std.algorithm.iteration: filter;
import std.array;

import i18n;
import bindbc.imgui;

version (OSX) {
    const string commandChar = "\ueae7";
} else version (linux) {
    const string commandChar = "Super";
} else {
    const string commandChar = "Win";
}

// Convert ImGuiKey to display string
// for icon refer: https://fonts.google.com/icons?icon.query=alt
string[ImGuiKey] incKeyDisplayMap = [
    ImGuiKey.LeftArrow: "\ue5c4",
    ImGuiKey.RightArrow: "\ue5c8",
    ImGuiKey.UpArrow: "\ue5d8",
    ImGuiKey.DownArrow: "\ue5d5",
    ImGuiKey.LeftCtrl: "LCtrl",
    ImGuiKey.RightCtrl: "RCtrl",
    ImGuiKey.ModCtrl: "Ctrl",
    ImGuiKey.LeftAlt: "LAlt",
    ImGuiKey.RightAlt: "RAlt",
    ImGuiKey.ModAlt: "Alt",
    ImGuiKey.LeftShift: "LShift",
    ImGuiKey.RightShift: "RShift",
    ImGuiKey.ModShift: "Shift",
    ImGuiKey.Tab: "Tab",
    ImGuiKey.Space: "\ue256",
    ImGuiKey.LeftSuper: "L" ~ commandChar,
    ImGuiKey.RightSuper: "R" ~ commandChar,
    ImGuiKey.ModSuper: commandChar,
    ImGuiKey.Backspace: "\ue14a",
    ImGuiKey.Delete: "del",
    ImGuiKey.Insert: "\ue88a",
    ImGuiKey.CapsLock: "Caps",
    ImGuiKey.Comma: ",",
    ImGuiKey.LeftBracket: "[",
    ImGuiKey.RightBracket: "]",
];

// incAppendRightLeftModifier for UI logic, user can choose append left/right modifier keys or not
bool incAppendRightLeftModifier = false;

// incAppendMouseMode for UI logic, user can choose mouse bindings or keyboard bindings
bool incAppendMouseMode = false;

// incSelectedBindingEntry for UI logic, user can click the entry to select which action to bind
ActionEntry incSelectedBindingEntry = null;

bool incKeyBindingEntrySelected(ActionEntry entry) {
    return incSelectedBindingEntry == entry;
}

void incSetSelectedBindingEntry(ActionEntry entry) {
    BindingRecorder.clearRecordedKeys();
    incSelectedBindingEntry = entry;
}

/** 
    ActionEntry is a class that represents for a single action / command / shortcut
    it can have multiple key bindings or mouse bindings
*/
class ActionEntry {
    private {
        string entryKey;
        string entryName;
        string actionDescription;
        AbstractBindingEntry[] bindingEntrys;

        // extactMatch means that the key binding is an exact match
        // like mutually exclusive actions, but it is only for itself, would not affect other actions
        bool extactMatch = false;

        // mode for key binding
        BindingMode keyMode;
    }

    /** 
        Params:
            entryKey - the key of the entry 
            name - display name
            description - description of the entry
            extactMatch - make sure the key binding is an exact match, it like mutually exclusive actions (not exactly)
                because it only affects itself
    */
    this(string entryKey, string name, string description, bool extactMatch = false,
            BindingMode keyMode = BindingMode.Pressed) {
        this.entryKey = entryKey;
        this.entryName = name;
        this.actionDescription = description;
        this.bindingEntrys = [];
        this.extactMatch = extactMatch;
        this.keyMode = keyMode;
    }

    string getName() {
        return entryName;
    }

    string getKey() {
        return entryKey;
    }

    void cleanRemoveList() {
        bindingEntrys = bindingEntrys.filter!(a => !a.toDelete).array;
    }

    void append(AbstractBindingEntry entry, bool keepEntryKeyMode = false) {
        if (!keepEntryKeyMode) {
            if (auto key = cast(KeyBindingEntry) entry)
                key.setMode(keyMode);
        }

        bindingEntrys ~= entry;
    }

    bool isActivated() {
        foreach (entry; bindingEntrys)
            if (entry.isActive(this.extactMatch))
                return true;
        return false;
    }

    bool isInactive() {
        bool allInactive = true;
        foreach (entry; bindingEntrys)
            if (!entry.isInactive(this.extactMatch))
                allInactive = false;
        return allInactive;
    }
}

/** 
    AbstractBindingEntry is a class that represents for a single key binding or mouse binding
*/
class AbstractBindingEntry {
    bool isPreconfig = true;
    bool toDelete = false;
    
    this() {

    }

    /*
        check key binding is active, override this method
    */
    bool isActive(bool extactMatch) {
        // override this method
        throw new Exception("Not implemented");
    }

    bool isInactive(bool extactMatch) {
        // override this method if needed
        return !isActive(extactMatch);
    }

    void tagDelete() {
        toDelete = true;
    }
}

enum BindingMode {
    Down = "Down",
    // for mouse only
    Clicked = "Clicked",
    Dragged = "Dragged",
    // for keyboard only
    Pressed = "Pressed",
    PressedRepeat = "Pressed Repeat",
}

/**
    KeyScanner is a class that helps to scan all keys,
    we can implement mutually exclusive actions by using this class
    it mantains the key state of all keys, and we can check the key state by using this class
    also see incInputPoll() but it invoke in the viewports
*/
static class KeyScanner {
    static bool[ImGuiKey] keyStatePressed;
    static bool[ImGuiKey] keyStatePressedRepeat;
    static bool[ImGuiKey] keyStateDown;
    static int keyCountPressed;
    static int keyCountPressedRepeat;
    static int keyCountDown;
    static int keyModifierCount;
    static int keyModifierCountLR;

    static void addKeys(ImGuiKey[] keys) {
        foreach (key; keys) {
            if (key in keyStatePressed)
                continue;
            keyStatePressed[key] = false;
            keyStatePressedRepeat[key] = false;
            keyStateDown[key] = false;
        }
    }

    static void scanAllKeys() {
        // The code is a bit messy. Write tests before refactoring.
        // TODO: Write unit tests before refactoring or modifying the code

        // clear key count
        keyCountPressed = 0;
        keyCountPressedRepeat = 0;
        keyCountDown = 0;
        keyModifierCount = 0;
        keyModifierCountLR = 0;

        foreach (key; keyStatePressed.keys) {
            // clear key state
            keyStatePressed[key] = false;
            keyStatePressedRepeat[key] = false;
            keyStateDown[key] = false;

            // check key state
            keyStatePressed[key] = igIsKeyPressed(key, false);
            keyStatePressedRepeat[key] = igIsKeyPressed(key, true);
            keyStateDown[key] = igIsKeyDown(key);

            // prevent duplicate count
            if (incIsModifierKey(key)) {
                keyModifierCount += keyStateDown[key] ? 1 : 0;
            } else if (incIsModifierKeyLR(key)) {
                keyModifierCountLR += keyStateDown[key] ? 1 : 0;
            } else {
                keyCountPressed += keyStatePressed[key] ? 1 : 0;
                keyCountPressedRepeat += keyStatePressedRepeat[key] ? 1 : 0;
                keyCountDown += keyStateDown[key] ? 1 : 0;
            }
        }
    }
}

void incScanInput() {
    KeyScanner.scanAllKeys();
}

unittest {
    // init KeyList = [Ctrl, Shift, S, LShift]

    // init KeyList = [Ctrl, Shift, LShift, RShift, S]

        // KeyBindingEntry.isActive(extactMatch=true) unit tests
        // for `Ctrl+Shift+S` Keybind There may be errors in these cases
        // LCtrl+Shift+S = True
        // LCtrl+RCtrl+Shift+S = True
        // Ctrl+Shift+S = True
        // Ctrl+RCtrl+Shift+S = True
        // Ctrl+S = False

        // for `LCtrl+S` Keybind
        // LCtrl+S = True
        // LCtrl+RCtrl+S = ?
        // RCtrl+S = False
        // Ctrl+S = False
        // LCtrl+Ctrl+S = True?
        // RCtrl+Ctrl+S = False

        // KeyBindingEntry.isActive(extactMatch=false) unit tests
        // keybinding: Ctrl+S
        // LCtrl+S = True
        // RCtrl+S = True
        // LCtrl+RCtrl+Ctrl+S = True
        // Ctrl+S = True
        // it non mutually exclusive actions, so it should be true
        // Ctrl+Shift+S = True

    // TODO: Implement unit tests
}

class KeyBindingEntry : AbstractBindingEntry {
    private {
        // macosKeyBinding means that the key is a macos key binding
        bool macosKeyBinding = false;

        ImGuiKey[] keys;
        BindingMode mode;
    }

    this(ImGuiKey[] keys) {
        if (keys.length == 0)
            throw new Exception("Key binding must have at least one key");

        this.keys.length = keys.length;
        this.keys[] = keys[];
        this.mode = mode;

        KeyScanner.addKeys(keys);
    }

    void setMode(BindingMode mode) {
        if (mode == BindingMode.Clicked)
            throw new Exception("Key binding does not support Clicked mode, consider using Pressed or PressedRepeat mode");

        this.mode = mode;
    }

    BindingMode getMode() {
        return mode;
    }

    ImGuiKey[] getKeys() {
        return keys;
    }

    override
    bool isActive(bool extactMatch) {
        // The code is a bit messy. Write tests before refactoring.
        // TODO: Write unit tests before refactoring or modifying the code

        bool result = true;
        int downCount = 0;
        int pressedCount = 0;
        int pressedRepeatCount = 0;
        int modifierCount = 0;
        int modifierCountLR = 0;
        foreach (key; keys) {
            switch (mode) {
                case BindingMode.Down:
                    result &= KeyScanner.keyStateDown[key];
                    downCount++;
                    break;
                case BindingMode.Pressed:
                case BindingMode.PressedRepeat:
                    if (incIsModifierKeyLR(key)) {
                        result &= KeyScanner.keyStateDown[key];
                        modifierCountLR++;
                    } else if (incIsModifierKey(key)) {
                        result &= KeyScanner.keyStateDown[key];
                        modifierCount++;
                        modifierCountLR++;
                    } else if (mode == BindingMode.Pressed) {
                        result &= KeyScanner.keyStatePressed[key];
                        // when pressed, down always true, and pressed repeat always true
                        pressedCount++;
                        pressedRepeatCount++;
                        downCount++;
                    } else {
                        result &= KeyScanner.keyStatePressedRepeat[key];
                        // when pressed repeat, down always true
                        pressedRepeatCount++;
                        downCount++;
                    }
                    break;
                default:
                    throw new Exception("Unknown key binding mode");
            }
        }

        // when different mode should have different count
        bool checkPressedCount = false;
        if (mode == BindingMode.Pressed)
            checkPressedCount = KeyScanner.keyCountPressed == pressedCount;
        if (mode == BindingMode.PressedRepeat)
            checkPressedCount = KeyScanner.keyCountPressedRepeat == pressedRepeatCount;

        // check if the key binding is an exact match
        if (extactMatch &&
                (KeyScanner.keyCountDown != downCount ||
                !checkPressedCount ||
                KeyScanner.keyModifierCount != modifierCount ||
                KeyScanner.keyModifierCountLR != modifierCountLR
            )) {
            result = false;
        }

        return result;
    }
}

class MouseBindingEntry : AbstractBindingEntry {
    private {
        ImGuiMouseButton button;
        BindingMode mode;
    }

    void setMode(BindingMode mode) {
        if (mode == BindingMode.Pressed || mode == BindingMode.PressedRepeat)
            throw new Exception("Mouse binding does not support Pressed or PressedRepeat mode, consider using Clicked mode");
        this.mode = mode;
    }

    BindingMode getMode() {
        return mode;
    }

    this(ImGuiMouseButton button) {
        this.button = button;
        this.mode = BindingMode.Down;
    }

    ImGuiMouseButton getButton() {
        return button;
    }

    override
    bool isActive(bool extactMatch) {
        switch (mode) {
            case BindingMode.Clicked:
                return igIsMouseClicked(button);
            case BindingMode.Down:
                return igIsMouseDown(button);
            case BindingMode.Dragged:
                return igIsMouseDown(button) && incInputIsDragRequested(button);
            default:
                throw new Exception("Unknown mouse binding mode");
        }
        return false;
    }

    override
    bool isInactive(bool extactMatch) {
        if (mode == BindingMode.Dragged)
            // keeping original condition from incViewportMovement()
            return !igIsMouseDown(button);

        return !isActive(extactMatch);
    }
}

void incDrawRecorderButtons(string label, ImGuiKey key) {
    igSameLine(0, 2);
    incText(label);
    if (igIsItemClicked())
        BindingRecorder.removeRecordedKey(key);
}

string incMouseToText(ImGuiMouseButton button) {
    switch (button) {
        case ImGuiMouseButton.Left:
            return _("Left");
        case ImGuiMouseButton.Middle:
            return _("Middle");
        case ImGuiMouseButton.Right:
            return _("Right");
        default:
            throw new Exception("Unknown mouse button");
    }
}

string incGetKeyString(ImGuiKey key) {
    if (key in incKeyDisplayMap)
        return incKeyDisplayMap[key];
    
    return incToDString(igGetKeyName(key));
}

string incKeysToStr(ImGuiKey[] keys) {
    string result = "";
    foreach (key; keys)
        result ~= incGetKeyString(key) ~ " ";
    return result.strip().replace(" ", "+");
}

static class BindingRecorder {
    private {
        static bool[ImGuiKey] recordedKeys;
    }    

    static void clearRecordedKeys() {
        recordedKeys = new bool[ImGuiKey];
    }
    
    static void removeRecordedKey(ImGuiKey key) {
        recordedKeys.remove(key);
    }
    
    /** 
        this method is used to record the key, called by the input recording method loop
    */
    static void recordKey(ImGuiKey key) {
        if (key == ImGuiKey.None) return;

        // check if we need to append left and right modifier keys
        // or without them
        if (!incAppendRightLeftModifier) {
            switch (key) {
                case ImGuiKey.LeftCtrl:
                case ImGuiKey.RightCtrl:
                case ImGuiKey.LeftAlt:
                case ImGuiKey.RightAlt:
                case ImGuiKey.LeftShift:
                case ImGuiKey.RightShift:
                case ImGuiKey.LeftSuper:
                case ImGuiKey.RightSuper:
                    return;
                default:
                    break;
            }
        } else {
            switch (key) {
                case ImGuiKey.ModCtrl:
                case ImGuiKey.ModAlt:
                case ImGuiKey.ModShift:
                case ImGuiKey.ModSuper:
                    return;
                default:
                    break;
            }                
        }

        recordedKeys[key] = true;
    }

    static ImGuiKey[] getRecordedKeys() {
        return recordedKeys.keys;
    }

    static void drawRecordedKeys() {
        // Draw recorded keys
        foreach (key; recordedKeys.keys) {
            if (key != recordedKeys.keys[$ - 1])
                incDrawRecorderButtons(incGetKeyString(key) ~ "+", key);
            else
                incDrawRecorderButtons(incGetKeyString(key), key);
        }

        // Draw help text
        igSameLine(0, 2);
        if (BindingRecorder.getRecordedKeys().length == 0)
            incText(_("(Press a key to bind)"));
        else
            incText(_("(Click to remove)"));
    }
}

// hashmap for fast access key/mouse bindings
ActionEntry[string] incInputBindings;

// List of default actions
ActionEntry[][string] incDefaultActions;

void incInitInputBinding() {
    // setup default actions
    incDefaultActions =  [   
        "Gereral": [
            new ActionEntry("undo", _("Undo"), _("Undo the last action"), true, BindingMode.PressedRepeat),
            new ActionEntry("redo", _("Redo"), _("Redo the last action"), true, BindingMode.PressedRepeat),
            new ActionEntry("select_all", _("Select All"), _("Select all text or objects"), true),

            // Currently, I cannot find existing actions for these
            //new ActionEntry("copy", _("Copy"), _("Copy the selected text or object"), true),
            //new ActionEntry("paste", _("Paste"), _("Paste the copied text or object"), true),
            //new ActionEntry("cut", _("Cut"), _("Cut the selected text or object"), true),
        ],
        "ViewPort": [
            new ActionEntry("mirror_view", _("Mirror View"), _("Mirror the Viewport")),
            new ActionEntry("move_viewport", _("Move Viewport"), _("Move the Viewport"), false, BindingMode.Down),
        ],
        "File Handling": [
            new ActionEntry("new_file", _("New File"), _("Create a new file"), true),
            new ActionEntry("open_file", _("Open File"), _("Open a file"), true),
            new ActionEntry("save_file", _("Save File"), _("Save the current file"), true),
            new ActionEntry("save_file_as", _("Save File As"), _("Save the current file as a new file"), true),
        ],
    ];

    // build hashmap for fast access
    foreach (category; incDefaultActions.keys) {
        foreach (entry; incDefaultActions[category]) {
            // check if the entry is already in the hashmap, prevent unexpected behavior
            if (entry.entryKey in incInputBindings)
                throw new Exception("Duplicate key binding entry: " ~ entry.getKey());

            incInputBindings[entry.getKey()] = entry;
        }
    }

    // add must modifier keys to the key scanner
    // This allows early detection of KeyScanner modifier keys for bugs
    // Because the left and right modifier keys and modifier key conditions may be complex
    KeyScanner.addKeys([
        ImGuiKey.LeftCtrl,
        ImGuiKey.RightCtrl,
        ImGuiKey.LeftAlt,
        ImGuiKey.RightAlt,
        ImGuiKey.LeftShift,
        ImGuiKey.RightShift,
        ImGuiKey.LeftSuper,
        ImGuiKey.RightSuper,
        ImGuiKey.ModCtrl,
        ImGuiKey.ModAlt,
        ImGuiKey.ModShift,
        ImGuiKey.ModSuper,
    ]);
}

/*
    our ImGui layout will look like this (UI logic):
    - incDrawAllBindings()
        - incDrawBindingEntries() 
            - incDrawBindingActionEntry()
                - incDrawKeyBindingInput()
                - incDrawMouseBindingInput()
                - incDrawBindingEntry()
*/
void incDrawBindingEntry(AbstractBindingEntry entry) {
    incText("\ue92b"); // delete
    if (igIsItemClicked()) {
        entry.tagDelete();
    }

    // draw the icon and binding discrption
    igSameLine(0, 2);
    if (auto mouse = cast(MouseBindingEntry) entry)
        incText("\ue323" ~ incMouseToText(mouse.getButton())
            ~ " (" ~ mouse.getMode() ~ ")"
        ); // mouse icon
    if (auto key = cast(KeyBindingEntry) entry)
        incText("\ue312" ~ incKeysToStr(key.getKeys())
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
            incSelectedBindingEntry.append(new KeyBindingEntry(keys));
            BindingRecorder.clearRecordedKeys();
        }
    }

    igSameLine(0, 2);

    // cancel
    incText("\ue872");
    if (igIsItemClicked())
       BindingRecorder.clearRecordedKeys();
    igSameLine(0, 2);
    BindingRecorder.drawRecordedKeys();
}

void incDrawMouseBindingInput() {
    incText("\ue323"); // mouse icon
    igSameLine(0, 2);
    incText(_("\ue836 Left"));
    if (igIsItemClicked())
        incSelectedBindingEntry.append(new MouseBindingEntry(ImGuiMouseButton.Left));
    
    igSameLine(0, 2);
    incText(_("\ue836 Middle"));
    if (igIsItemClicked())
        incSelectedBindingEntry.append(new MouseBindingEntry(ImGuiMouseButton.Middle));

    igSameLine(0, 2);
    incText(_("\ue836 Right"));
    if (igIsItemClicked())
        incSelectedBindingEntry.append(new MouseBindingEntry(ImGuiMouseButton.Right));
}

void incDrawBindingInput() {
    if (incAppendMouseMode)
        incDrawMouseBindingInput();
    else
        incDrawKeyBindingInput();
}

void incDrawBindingActionEntry(ActionEntry entry) {
    bool isSelected = incKeyBindingEntrySelected(entry);
    string itemLabel = entry.getName() ~ "##Keybind-" ~ entry.getKey();
    if (igSelectable(itemLabel.toStringz, isSelected, ImGuiSelectableFlags.None, ImVec2(0, 0))) {
        incSetSelectedBindingEntry(entry);
    }

    // draw child nodes
    igBeginGroup();
        igIndent(8);

        if (isSelected)
            incDrawBindingInput();

        foreach (bindingEntry; entry.bindingEntrys)
            incDrawBindingEntry(bindingEntry);
        entry.cleanRemoveList();
            
    igEndGroup();
}

void incDrawBindingEntries(ActionEntry[] entries, string category) {
    incText("\ue8b8"); // settings icon
    igSameLine(0, 2);
    category ~= "##Keybind-category-" ~ category;
    if (igSelectable(category.toStringz, false, ImGuiSelectableFlags.None, ImVec2(0, 0))) {
        
    }

    // draw child nodes
    igBeginGroup();
        igIndent(8);
        foreach (entry; entries)
            incDrawBindingActionEntry(entry);
        
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
    throw new Exception("Not implemented yet");
    // TODO: implement command key handling
    incText("\ue834 Switch \ueae7 key"); // selected
}

void incDrawBindingFileButton() {
    throw new Exception("Not implemented yet");
    // TODO: implement file handling
    incText(_("\ue5d5 reset"));
    igSameLine(0, 2);
    incText(_("\ue161 Export"));
    igSameLine(0, 2);
    incText(_("\uf090 Import"));
    igSameLine(0, 2);
}

void incDrawAllBindings() {
    // draw child nodes
    igBeginGroup();
        igIndent(8);
        foreach (category; incDefaultActions.keys)
            incDrawBindingEntries(incDefaultActions[category], category);
    igEndGroup();
}

void incDrawRightLeftModifierSwitch() {
    if (incAppendRightLeftModifier)
        incText(_("\ue834 Append left and right modifier keys")); // selected
    else
        incText(_("\ue835 Append left and right modifier keys")); // unselected
    if (igIsItemClicked())
        incAppendRightLeftModifier = !incAppendRightLeftModifier;
}

void incInputRecording() {
    import nijigenerate.core.input;
    import std.stdio;
    import std.traits : EnumMembers;
    foreach (key ; EnumMembers!ImGuiKey) {
        // pass IM_ASSERT(key >= ImGuiKey_LegacyNativeKey_BEGIN && key < ImGuiKey_NamedKey_END)
        if (key < ImGuiKeyI.LegacyNativeKey_BEGIN || key >= ImGuiKeyI.NamedKey_END)
            continue;

        if (igIsKeyDown(key))
            BindingRecorder.recordKey(key);
    }                   
}

void incAddShortcut(string actionKey, string key, BindingMode mode = BindingMode.Pressed) {
    // we assume actionKey is already in the hashmap, do not check it
    auto entry = incInputBindings[actionKey];
    auto binding = new KeyBindingEntry(incStringToKeys(key));
    binding.setMode(mode);
    entry.append(binding, true);
}

void incAddMouse(string actionKey, ImGuiMouseButton button, BindingMode mode = BindingMode.Down) {
    // we assume actionKey is already in the hashmap, do not check it
    auto entry = incInputBindings[actionKey];
    auto binding = new MouseBindingEntry(button);
    binding.setMode(mode);
    entry.append(binding, true);
}

bool incIsActionActivated(string actionKey) {
    // we assume actionKey is already in the hashmap, do not check it
    return incInputBindings[actionKey].isActivated();
}

bool incIsActionInactive(string actionKey) {
    // we assume actionKey is already in the hashmap, do not check it
    return incInputBindings[actionKey].isInactive();
}