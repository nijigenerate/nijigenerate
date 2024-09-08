/*
    Copyright © 2020-2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Lin, Yong Xiang <r888800009@gmail.com>
*/

module nijigenerate.io.config;
import nijigenerate.core;
import nijigenerate.core.input;
import nijigenerate.io;

import std.string;
import std.algorithm;
import std.algorithm.iteration: filter;
import std.array;
import std.json;
import std.file;

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

// incAppendMouseMode for UI logic, user can choose mouse bindings or keyboard bindings
bool incAppendMouseMode = false;

// incSelectedBindingEntry for UI logic, user can click the entry to select which action to bind
ActionConfigEntry incSelectedBindingEntry = null;

bool incKeyBindingEntrySelected(ActionConfigEntry entry) {
    return incSelectedBindingEntry == entry;
}

void incSetSelectedBindingEntry(ActionConfigEntry entry) {
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

        // we mantain two lists, `uncommittedBindingEntrys` allows user to revert changes
        AbstractBindingEntry[] bindingEntrys;
        AbstractBindingEntry[] uncommittedBindingEntrys;

        // extactMatch means that the key binding is an exact match
        // like mutually exclusive actions, but it is only for itself, would not affect other actions
        bool extactMatch = false;
    }

    /** 
        Params:
            entryKey - the key of the entry 
            name - display name
            description - description of the entry
            extactMatch - make sure the key binding is an exact match, it like mutually exclusive actions (not exactly)
                because it only affects itself
    */
    this(string entryKey, string name, string description, bool extactMatch = false) {
        this.entryKey = entryKey;
        this.entryName = name;
        this.actionDescription = description;
        this.bindingEntrys = [];
        this.extactMatch = extactMatch;
    }

    string getName() {
        return entryName;
    }

    string getKey() {
        return entryKey;
    }

    AbstractBindingEntry[] getBindedEntries() {
        return bindingEntrys;
    }

    AbstractBindingEntry[] getUncommittedBindedEntries() {
        return uncommittedBindingEntrys;
    }

    void removeAllBinding() {
        bindingEntrys = [];
        uncommittedBindingEntrys = [];
    }

    void append(AbstractBindingEntry entry) {
        // we append to uncommitted list first, it allows user to revert changes
        uncommittedBindingEntrys ~= entry;
    }

    /**
        isActivated() and isInactive() allow different conditions
        for example, drag is a special case in viewport movement (refer to git history `/viewport/package.d`)
            drag isActivated() is check mouse `down and drag` started but drag isInactive() is check `!mouse down`
    */
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

    bool hasUncommittedChanges() {
        return uncommittedBindingEntrys.length > 0;
    }

    /**
        if we want bindingEntry work, we need to commit changes
    */
    void commitChanges() {
        bindingEntrys ~= uncommittedBindingEntrys;
        uncommittedBindingEntrys = [];

        // commit delete
        bindingEntrys = bindingEntrys.filter!(a => !a.toDelete).array;
    }

    void revertChanges() {
        uncommittedBindingEntrys = [];

        // revert delete
        foreach (entry; bindingEntrys)
            entry.toDelete = false;
    }

    JSONValue toJSON() {
        JSONValue data = JSONValue();
        JSONValue[] bindings;

        // we don't serialize ActionEntry content, because is should be hard coded
        // serialize all binding entries
        foreach (entry; bindingEntrys)
            bindings ~= entry.toJSON();
        data["bindings"] = JSONValue(bindings);
        return data;
    }
    /**
        fromJSON() is used to load data from disk
            checkOnly - if true, only check the key, do not load data
    */
    JSONValue fromJSON(string key, JSONValue data, bool checkOnly = false) {
        if (key != entryKey)
            throw new Exception("Key mismatch");

        if (!checkOnly)
            removeAllBinding();

        foreach (entry; data["bindings"].array) {
            auto binding = new AbstractBindingEntry();
            if (entry["className"].get!string == "KeyBindingEntry")
                binding = new KeyBindingEntry([ImGuiKey.None]);
            else if (entry["className"].get!string == "MouseBindingEntry")
                binding = new MouseBindingEntry(ImGuiMouseButton.Left);
            else
                throw new Exception("Unknown binding entry class");
            binding.fromJSON(entry);

            // we don't load data if checkOnly is true
            if (!checkOnly)
                bindingEntrys ~= binding;
        }

        return data;
    }
}

/**
    ActionConfigEntry just for decoupling the BindingBuilder and ActionEntry,
    and it allows user to configure the binding with default configuration
*/
class ActionConfigEntry : ActionEntry {
    private {
        BindingBuilder builder;
    }

    this(string entryKey, string name, string description, bool extactMatch = false) {
        builder = new BindingBuilder();
        super(entryKey, name, description, extactMatch);
    }

    BindingBuilder getBuilder() {
        return builder;
    }

    ActionConfigEntry setMouseMode(BindingMode mode) {
        this.getBuilder().setMouseMode(mode);
        return this;
    }

    ActionConfigEntry setKeyMode(BindingMode mode) {
        this.getBuilder().setKeyMode(mode);
        return this;
    }

    void append(ImGuiMouseButton mouse) {
        auto binding = this.getBuilder().buildMouse(mouse);
        super.append(binding);
    }

    void append(ImGuiKey[] keys) {
        auto binding = this.getBuilder().buildKeys(keys);
        super.append(binding);
    }

    void buildDefault() {
        auto builder = this.getBuilder();
        // developer might only include the important presets 
        // without setting a default binding for all
        if (builder.hasDefaultBinding())
            super.append(builder.buildDefault());
    }
}

unittest {
    auto entry = new ActionConfigEntry("test", "Test", "Test");
    assert(entry.getName() == "Test");
    assert(entry.getKey() == "test");

    // test incKeyBindingEntrySelected
    assert(!incKeyBindingEntrySelected(entry));
    incSetSelectedBindingEntry(entry);
    assert(incKeyBindingEntrySelected(entry));
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

    JSONValue toJSON() {
        // override this method
        throw new Exception("Not implemented");
    }

    void fromJSON(JSONValue data) {
        // override this method
        throw new Exception("Not implemented");
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

ImGuiKey incSwitchCommandKey(ImGuiKey key) {
    if (!incSettingsGet!bool("SwapCommandControl"))
        return key;

    switch (key) {
        case ImGuiKey.LeftCtrl:
            return ImGuiKey.LeftSuper;
        case ImGuiKey.RightCtrl:
            return ImGuiKey.RightSuper;
        case ImGuiKey.ModCtrl:
            return ImGuiKey.ModSuper;
        case ImGuiKey.LeftSuper:
            return ImGuiKey.LeftCtrl;
        case ImGuiKey.RightSuper:
            return ImGuiKey.RightCtrl;
        case ImGuiKey.ModSuper:
            return ImGuiKey.ModCtrl;
        default:
            return key;
    }
}

ImGuiKey[] incSwitchCommandKey(ImGuiKey[] keys) {
    import std.algorithm.iteration: map;
    return keys.map!(a => incSwitchCommandKey(a)).array;
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

    // for modifier keys, we don't count them in keyCountPressed, keyCountPressedRepeat, keyCountDown
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
        // clear key count
        keyCountPressed = 0;
        keyCountPressedRepeat = 0;
        keyCountDown = 0;

        keyModifierCount = 0;
        keyModifierCountLR = 0;

        foreach (keyRaw; keyStatePressed.keys) {
            auto key = incSwitchCommandKey(keyRaw);
            // clear key state
            keyStatePressed[key] = false;
            keyStatePressedRepeat[key] = false;
            keyStateDown[key] = false;

            // check key state
            keyStatePressed[key] = igIsKeyPressed(key, false);
            keyStatePressedRepeat[key] = igIsKeyPressed(key, true);
            keyStateDown[key] = igIsKeyDown(key);

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

    KeyBindingEntry setMode(BindingMode mode) {
        if (mode == BindingMode.Clicked)
            throw new Exception("Key binding does not support Clicked mode, consider using Pressed or PressedRepeat mode");

        this.mode = mode;
        return this;
    }

    BindingMode getMode() {
        return mode;
    }

    ImGuiKey[] getKeys() {
        return keys;
    }

    override
    bool isActive(bool extactMatch) {
        // extactMatch obtains the result through key count to handle mutually exclude actions (ctrl+s, ctrl+shift+s)
        bool result = true;
        int downCount = 0;
        int pressedCount = 0;
        int pressedRepeatCount = 0;

        int modifierCount = 0;
        int modifierCountLR = 0;
        foreach (keyRaw; keys) {
            if (!result)
                return false;

            // 1. the key is a modifier key
            auto key = incSwitchCommandKey(keyRaw);
            if (incIsModifierKeyLR(key)) {
                result &= KeyScanner.keyStateDown[key];
                modifierCountLR++;
                continue;
            }
            if (incIsModifierKey(key)) {
                result &= KeyScanner.keyStateDown[key];
                modifierCount++;

                // This line is necessary, some KeyBindings no left/right modifiers, but KeyScanner does.
                // The logic for handling left/right modifier more complex. (simplify implementation)
                // This problem could also be resolved by disallowing users config left/right modifier keys.
                // also see incDrawRightLeftModifierSwitch()
                modifierCountLR++;
                continue;
            }

            // 2. the key is a normal key
            switch (mode) {
                case BindingMode.Down:
                    result &= KeyScanner.keyStateDown[key];
                    break;
                case BindingMode.Pressed:
                    result &= KeyScanner.keyStatePressed[key];
                    break;
                case BindingMode.PressedRepeat:
                    result &= KeyScanner.keyStatePressedRepeat[key];
                    break;
                default:
                    throw new Exception("Unknown key binding mode");
            }

            // we only check the key count if it is an exact match
            if (!extactMatch)
                continue;

            // count key count, it means what case should be triggered
            // when pressed, down always true, and pressed repeat always true
            switch (mode) {
                case BindingMode.Pressed:
                    pressedCount++;
                    goto case;
                case BindingMode.PressedRepeat:
                    pressedRepeatCount++;
                    goto case;
                case BindingMode.Down:
                    downCount++;
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

    override
    JSONValue toJSON() {
        JSONValue data = JSONValue();
        data["keys"] = JSONValue(keys);
        data["mode"] = JSONValue(mode);
        data["className"] = "KeyBindingEntry";
        return data;
    }

    override
    void fromJSON(JSONValue data) {
        keys = data["keys"].array.map!(a => a.get!ImGuiKey).array;
        mode = cast(BindingMode) data["mode"].get!string;

        KeyScanner.addKeys(keys);
    }
}


// Mock mouse button
IMouse incMouse = null;

interface IMouse {
    bool isClicked(ImGuiMouseButton button);
    bool isDragRequested(ImGuiMouseButton button);
    bool isDown(ImGuiMouseButton button);
}

class IncImguiMouse : IMouse {
    private {
        ImGuiMouseButton button;
    }

    bool isClicked(ImGuiMouseButton button) {
        return igIsMouseClicked(button);
    }

    bool isDragRequested(ImGuiMouseButton button) {
        return incInputIsDragRequested(button);
    }

    bool isDown(ImGuiMouseButton button) {
        return igIsMouseDown(button);
    }
}

enum ClickState {
    Clicked,
    DragRequested,
    Down,
    Up,
}

class UnitTestMouse : IMouse {
    public {
        ClickState[ImGuiMouseButton] button;
    }

    void clean() {
        button[ImGuiMouseButton.Left] = ClickState.Up;
        button[ImGuiMouseButton.Middle] = ClickState.Up;
        button[ImGuiMouseButton.Right] = ClickState.Up;
    }

    bool isClicked(ImGuiMouseButton button) {
        return this.button[button] == ClickState.Clicked;
    }

    void click(ImGuiMouseButton button) {
        this.button[button] = ClickState.Clicked;
    }

    bool isDragRequested(ImGuiMouseButton button) {
        return this.button[button] == ClickState.DragRequested;
    }

    bool isDown(ImGuiMouseButton button) {
        return this.button[button] == ClickState.Down;
    }

    void down(ImGuiMouseButton button) {
        this.button[button] = ClickState.Down;
    }
}

class MouseBindingEntry : AbstractBindingEntry {
    private {
        ImGuiMouseButton button;
        BindingMode mode;
    }

    MouseBindingEntry setMode(BindingMode mode) {
        if (mode == BindingMode.Pressed || mode == BindingMode.PressedRepeat)
            throw new Exception("Mouse binding does not support Pressed or PressedRepeat mode, consider using Clicked mode");
        this.mode = mode;
        return this;
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
                return incMouse.isClicked(button);
            case BindingMode.Down:
                return incMouse.isDown(button);
            case BindingMode.Dragged:
                return incMouse.isDown(button) && incMouse.isDragRequested(button);
            default:
                throw new Exception("Unknown mouse binding mode");
        }
        return false;
    }

    override
    bool isInactive(bool extactMatch) {
        if (mode == BindingMode.Dragged)
            // keeping original condition from incViewportMovement()
            return !incMouse.isDown(button);

        return !isActive(extactMatch);
    }

    override
    JSONValue toJSON() {
        JSONValue data = JSONValue();
        data["button"] = JSONValue(button);
        data["mode"] = JSONValue(mode);
        data["className"] = "MouseBindingEntry";
        return data;
    }

    override
    void fromJSON(JSONValue data) {
        button = data["button"].get!ImGuiMouseButton;
        mode = cast(BindingMode) data["mode"].get!string;
    }
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

unittest {
    assert(incMouseToText(ImGuiMouseButton.Left) == "Left");
    assert(incMouseToText(ImGuiMouseButton.Middle) == "Middle");
    assert(incMouseToText(ImGuiMouseButton.Right) == "Right");
}

string incGetKeyString(ImGuiKey key) {
    if (key in incKeyDisplayMap)
        return incKeyDisplayMap[key];
    
    return incToDString(igGetKeyName(key));
}

/*
    incKeysToStr() for logic, incKeysToStrUI() for UI logic
*/
string incKeysToStr(ImGuiKey[] keys) {
    string result = "";
    foreach (key; keys)
        result ~= incGetKeyString(key) ~ " ";
    return result.strip().replace(" ", "+");
}

string incKeysToStrUI(ImGuiKey[] keys) {
    return incKeysToStr(incSwitchCommandKey(keys));
}

/*
It can only be tested after imgui is loaded, otherwise segmentation fault
unittest {
    assert(incKeysToStr([ImGuiKey.LeftCtrl, ImGuiKey.Z]) == "LCtrl+Z");
    assert(incKeysToStr([ImGuiKey.LeftCtrl, ImGuiKey.LeftShift, ImGuiKey.S]) == "LCtrl+LShift+S");
}
*/

static class BindingRecorder {
    private {
        static bool[ImGuiKey] recordedKeys;
    }    

    // appendRightLeftModifier for UI logic, user can choose append left/right modifier keys or not
    static bool appendRightLeftModifier = false;

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
        if (!appendRightLeftModifier && incIsModifierKeyLR(key))
            return;
        else if (appendRightLeftModifier && incIsModifierKey(key))
            return;

        recordedKeys[key] = true;
    }

    static ImGuiKey[] getRecordedKeys() {
        return recordedKeys.keys;
    }

    static bool hasKeys(ImGuiKey[] keys) {
        foreach (key; keys)
            if (key !in recordedKeys || !recordedKeys[key])
                return false;
        return true;
    }
}

unittest {
    // test appendRightLeftModifier
    BindingRecorder.clearRecordedKeys();
    BindingRecorder.appendRightLeftModifier = false;
    BindingRecorder.recordKey(ImGuiKey.LeftCtrl);
    BindingRecorder.recordKey(ImGuiKey.ModCtrl);
    BindingRecorder.recordKey(ImGuiKey.S);
    assert(BindingRecorder.getRecordedKeys().length == 2);
    assert(BindingRecorder.hasKeys([ImGuiKey.ModCtrl, ImGuiKey.S]));
    assert(!BindingRecorder.hasKeys([ImGuiKey.LeftCtrl]));

    // test appendRightLeftModifier
    BindingRecorder.clearRecordedKeys();
    BindingRecorder.appendRightLeftModifier = true;
    BindingRecorder.recordKey(ImGuiKey.LeftCtrl);
    BindingRecorder.recordKey(ImGuiKey.ModCtrl);
    BindingRecorder.recordKey(ImGuiKey.S);
    assert(BindingRecorder.getRecordedKeys().length == 2);
    assert(BindingRecorder.hasKeys([ImGuiKey.LeftCtrl, ImGuiKey.S]));

    // test removeRecordedKey
    BindingRecorder.clearRecordedKeys();
    BindingRecorder.recordKey(ImGuiKey.S);
    BindingRecorder.removeRecordedKey(ImGuiKey.S);
    assert(BindingRecorder.getRecordedKeys().length == 0);
}

// hashmap for fast access key/mouse bindings
ActionConfigEntry[string] incInputBindings;

// List of default actions
ActionConfigEntry[][string] incDefaultActions;

void incInitBindingHashMap() {
    incInputBindings = new ActionConfigEntry[string];

    // build hashmap for fast access
    foreach (category; incDefaultActions.keys) {
        foreach (entry; incDefaultActions[category]) {
            // check if the entry is already in the hashmap, prevent unexpected behavior
            if (entry.entryKey in incInputBindings)
                throw new Exception("Duplicate key binding entry: " ~ entry.getKey());

            incInputBindings[entry.getKey()] = entry;
        }
    }
}

void incInitInputBinding() {
    // setup default actions
    incDefaultActions =  [
        "Gereral": [
            new ActionConfigEntry("undo", _("Undo"), _("Undo the last action"), true)
                .setKeyMode(BindingMode.PressedRepeat),
            new ActionConfigEntry("redo", _("Redo"), _("Redo the last action"), true)
                .setKeyMode(BindingMode.PressedRepeat),
            new ActionConfigEntry("select_all", _("Select All"), _("Select all text or objects"), true),

            // Currently, I cannot find existing actions for these
            //new ActionConfigEntry("copy", _("Copy"), _("Copy the selected text or object"), true),
            //new ActionConfigEntry("paste", _("Paste"), _("Paste the copied text or object"), true),
            //new ActionConfigEntry("cut", _("Cut"), _("Cut the selected text or object"), true),
        ],
        "ViewPort": [
            new ActionConfigEntry("mirror_view", _("Mirror View"), _("Mirror the Viewport")),
            new ActionConfigEntry("move_viewport", _("Move Viewport"), _("Move the Viewport"), false)
                .setKeyMode(BindingMode.Down)
                .setMouseMode(BindingMode.Down),
        ],
        "File Handling": [
            new ActionConfigEntry("new_file", _("New File"), _("Create a new file"), true),
            new ActionConfigEntry("open_file", _("Open File"), _("Open a file"), true),
            new ActionConfigEntry("save_file", _("Save File"), _("Save the current file"), true),
            new ActionConfigEntry("save_file_as", _("Save File As"), _("Save the current file as a new file"), true),
        ],
    ];

    incInitBindingHashMap();

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

    // init I/O Mock
    incMouse = new IncImguiMouse();
}


void incInputRecording() {
    import std.traits : EnumMembers;
    foreach (key ; EnumMembers!ImGuiKey) {
        // pass IM_ASSERT(key >= ImGuiKey_LegacyNativeKey_BEGIN && key < ImGuiKey_NamedKey_END)
        if (key < ImGuiKeyI.LegacyNativeKey_BEGIN || key >= ImGuiKeyI.NamedKey_END)
            continue;

        if (igIsKeyDown(key))
            BindingRecorder.recordKey(key);
    }                   
}

/** 
    BindingBuilder allow we to build the default key bindings
*/
class BindingBuilder {
    private {
        // Mouse preconfig
        ImGuiMouseButton mouseButton;
        BindingMode mouseMode;

        // Key preconfig
        ImGuiKey[] keys;
        BindingMode keyMode;

        bool isDefaultMouse = false;
        bool hasDefault = false;
    }

    this() {
        // set default mode
        this.keyMode = BindingMode.Pressed;
        this.mouseMode = BindingMode.Clicked;
    }

    bool hasDefaultBinding() {
        return hasDefault;
    }

    BindingBuilder setDefault(ImGuiMouseButton button, BindingMode mode) {
        if (hasDefault)
            throw new Exception("Default has been configured");

        this.mouseButton = button;
        this.mouseMode = mode;
        isDefaultMouse = true;
        hasDefault = true;
        return this;
    }

    BindingBuilder setKeyMode(BindingMode mode) {
        this.keyMode = mode;
        return this;
    }

    BindingBuilder setMouseMode(BindingMode mode) {
        this.mouseMode = mode;
        return this;
    }

    AbstractBindingEntry buildMouse(ImGuiMouseButton button) {
        return new MouseBindingEntry(button).setMode(mouseMode);
    }

    AbstractBindingEntry buildKeys(ImGuiKey[] keys) {
        return new KeyBindingEntry(keys).setMode(keyMode);
    }

    AbstractBindingEntry buildDefault() {
        if (!hasDefault)
            throw new Exception("Default is not configured");

        if (isDefaultMouse)
            return this.buildMouse(this.mouseButton);
        return this.buildKeys(this.keys);
    }

    BindingBuilder setDefault(ImGuiKey[] keys, BindingMode mode) {
        this.keys = keys;
        this.keyMode = mode;
        isDefaultMouse = false;
        hasDefault = true;
        return this;
    }
}

unittest {
    auto builder = new BindingBuilder();
    // set default mode
    builder.setMouseMode(BindingMode.Down);
    builder.setKeyMode(BindingMode.Down);
    auto entry1 = builder.buildMouse(ImGuiMouseButton.Left);
    testAssertMouse(entry1, ImGuiMouseButton.Left, BindingMode.Down);
    auto entry2 = builder.buildKeys([ImGuiKey.LeftCtrl]);
    testAssertKey(entry2, [ImGuiKey.LeftCtrl], BindingMode.Down);
}

void incRemoveAllBinding() {
    // clean all binding
    foreach (entry; incInputBindings.values)
        entry.removeAllBinding();
}

/**
    incConfigureDefaultBindings(), it should call after all default bindings are added
*/
void incConfigureDefaultBindings() {
    incRemoveAllBinding();

    // build all default bindings
    foreach (entry; incInputBindings.values) {
        entry.buildDefault();
        entry.commitChanges();
    }
}

int incBindingEntriesCount() {
    int count = 0;
    foreach (entry; incInputBindings.values)
        count += entry.getBindedEntries().length;
    return count;
}


void testAssertMouse(AbstractBindingEntry entry, ImGuiMouseButton button, BindingMode mode) {
    if (auto mouse = cast(MouseBindingEntry) entry)
        assert(mouse.getButton() == button && mouse.getMode() == mode);
    else
        assert(false);
}

void testAssertKey(AbstractBindingEntry entry, ImGuiKey[] keys, BindingMode mode) {
    if (auto key = cast(KeyBindingEntry) entry)
        assert(key.getKeys() == keys && key.getMode() == mode);
    else
        assert(false);
}

unittest {
    const string unittest_file = "unittest_keybindings.json";

    void testCheckInitBinding() {
        // check undo key binding
        assert(incInputBindings["undo"].bindingEntrys.length == 1);
        assert(incInputBindings["undo"].uncommittedBindingEntrys.length == 0);
        testAssertKey(incInputBindings["undo"].bindingEntrys[0], [ImGuiKey.ModCtrl, ImGuiKey.Z], BindingMode.PressedRepeat);

        // check mouse1 mouse binding
        assert(incInputBindings["mouse1"].bindingEntrys.length == 2);
        testAssertMouse(incInputBindings["mouse1"].bindingEntrys[0], ImGuiMouseButton.Left, BindingMode.Down);
        testAssertMouse(incInputBindings["mouse1"].bindingEntrys[1], ImGuiMouseButton.Right, BindingMode.Clicked);
    }

    /*
        you should invoke this method for all tests, make sure unittest independent
    */
    void testInitBindings() {
        // setup default actions
        incDefaultActions =  [
            "Gereral": [
                new ActionConfigEntry("undo", _("Undo"), _("Undo the last action"), true)
                    .setKeyMode(BindingMode.PressedRepeat),
                new ActionConfigEntry("select_all", _("Select All"), _("Select all text or objects"), true),
                new ActionConfigEntry("mouse1", _("Mouse 1"), _("Mouse 1"), true),
                new ActionConfigEntry("redo", _("Redo"), _("Redo the last action"), true)
                    .setKeyMode(BindingMode.PressedRepeat),
                new ActionConfigEntry("ToolModifier", _("Tool Modifier"), _("Tool Modifier"), true)
                    .setKeyMode(BindingMode.Down),
            ],
        ];

        incInitBindingHashMap();

        // check init binding
        assert(incInputBindings.length == 5);

        // test incAddShortcut and incAddMouse it should add into builders
        incAddShortcut("undo", "Ctrl+Z", BindingMode.PressedRepeat);
        incAddShortcut("redo", "Ctrl+Shift+Z", BindingMode.PressedRepeat);
        incAddShortcut("select_all", "Ctrl+A", BindingMode.Pressed);
        incAddShortcut("ToolModifier", "LCtrl", BindingMode.Down);
        incAddMouse("mouse1", ImGuiMouseButton.Left, BindingMode.Down);

        incConfigureDefaultBindings();
        assert(incBindingEntriesCount() == 5);

        // Add mouse Down left
        incInputBindings["mouse1"].setMouseMode(BindingMode.Clicked).append(ImGuiMouseButton.Right);
        incCommitBindingsChanges();
        assert(incBindingEntriesCount() == 6);

    }

    void testSaveLoadBinding() {
        import std.file : exists;

        testInitBindings();

        // save bindings
        incSaveBindings(unittest_file);
        assert(exists(unittest_file));

        // clean all binding
        incRemoveAllBinding();
        foreach (entry; incInputBindings.values)
            assert(entry.bindingEntrys.length == 0);

        // load bindings
        incLoadBindings(unittest_file);
        foreach (entry; incInputBindings.values)
            assert(entry.bindingEntrys.length > 0, entry.getKey());
    }

    void testCommit() {
        // test delete binding
        testInitBindings();
        incInputBindings["undo"].bindingEntrys[0].tagDelete();
        incCommitBindingsChanges();
        assert(incInputBindings["undo"].bindingEntrys.length == 0);

        // test revert changes
        testInitBindings();
        incInputBindings["undo"].bindingEntrys[0].tagDelete();
        incRevertBindingsChanges();
        incCommitBindingsChanges();
        assert(incInputBindings["undo"].bindingEntrys.length == 1);
    }

    void testIOMouse() {
        incMouse = new UnitTestMouse();

        auto incMouse = cast(UnitTestMouse) incMouse;

        // test mouse binding
        testInitBindings();
        testCheckInitBinding();

        // simulate mouse input, we simulate mouse down right and left
        incMouse.down(ImGuiMouseButton.Left);
        incMouse.down(ImGuiMouseButton.Right);
        assert(incIsActionActivated("mouse1"));
        incMouse.clean();
        assert(incIsActionInactive("mouse1"));

        incMouse.click(ImGuiMouseButton.Middle);
        assert(incIsActionInactive("mouse1"));
    }

    /** 
        the test have three parts
            1. user using default key bindings and save to disk
            2. app update, and user add new key bindings, and save to disk
            3. app update again, some features removed, config should not crash
    */
    void testConifgBackwardsCompatible() {
        // setup default actions
        incDefaultActions =  [
            "Gereral": [
                new ActionConfigEntry("select_all", _("Select All"), _("Select all text or objects"), true),
            ],
        ];

        incInitBindingHashMap();
        incAddShortcut("select_all", "Ctrl+A", BindingMode.Pressed);
        incLoadBindingConfig(unittest_file);
        incSaveBindings(unittest_file);

        // setup new default actions
        incDefaultActions =  [
            "Gereral": [
                new ActionConfigEntry("undo", _("Undo"), _("Undo the last action"), true)
                    .setKeyMode(BindingMode.PressedRepeat),
                new ActionConfigEntry("select_all", _("Select All"), _("Select all text or objects"), true),
                new ActionConfigEntry("removed_features", _("Removed Features"), _("Removed Features"), true),
            ],
        ];

        incInitBindingHashMap();
        incAddShortcut("undo", "Ctrl+Z", BindingMode.PressedRepeat);
        incAddShortcut("select_all", "Ctrl+A", BindingMode.Pressed);
        incAddShortcut("removed_features", "Ctrl+R", BindingMode.Pressed);
        incLoadBindingConfig(unittest_file);
        assert(incBindingEntriesCount() == 3);

        // user try to add new binding and save
        incInputBindings["select_all"].append(incStringToKeys("Ctrl+Shift+A"));
        incCommitBindingsChanges();
        assert(incBindingEntriesCount() == 4);
        incSaveBindings(unittest_file);

        // load again and remove the removed_features
        incDefaultActions =  [
            "Gereral": [
                new ActionConfigEntry("undo", _("Undo"), _("Undo the last action"), true)
                    .setKeyMode(BindingMode.PressedRepeat),
                new ActionConfigEntry("select_all", _("Select All"), _("Select all text or objects"), true),
            ],
        ];

        incInitBindingHashMap();
        incAddShortcut("undo", "Ctrl+Z", BindingMode.PressedRepeat);
        incAddShortcut("select_all", "Ctrl+A", BindingMode.Pressed);
        incLoadBindingConfig(unittest_file);
    
        // now we have 3 bindings, because removed_features is removed
        assert(incBindingEntriesCount() == 3);
        assert(incInputBindings["select_all"].getBindedEntries().length == 2);
    }

    void testNoDefaultKeys() {
        // we should allow some actions without default keys, for example, some actions are not used frequently
        incDefaultActions =  [
            "Gereral": [
                new ActionConfigEntry("super_action", "Super Action", "Super Action", true),
            ],
        ];

        incInitBindingHashMap();
        incLoadBindingConfig(unittest_file);
        assert(incBindingEntriesCount() == 0);
    }

    // run test
    testSaveLoadBinding();
    testCommit();
    testIOMouse();
    testConifgBackwardsCompatible();
    testNoDefaultKeys();

    // just test some main functions logic
    incInitInputBinding();
}

/*
    for unit test, we split the function to two parts,
    incLoadBindingConfig() can be used to load the default key bindings
    incLoadBindingConfig(path) for specific path
*/
void incLoadBindingConfig() {
    incLoadBindingConfig(incGetDefaultBindingPath());
}

void incLoadBindingConfig(string path) {
    incConfigureDefaultBindings();
    if (exists(path))
        incLoadBindings(path);
}

void incAddShortcut(string actionKey, string key, BindingMode mode = BindingMode.Pressed) {
    // we assume actionKey is already in the hashmap, do not check it
    incInputBindings[actionKey].getBuilder().setDefault(incStringToKeys(key), mode);
}

void incAddMouse(string actionKey, ImGuiMouseButton button, BindingMode mode = BindingMode.Down) {
    // we assume actionKey is already in the hashmap, do not check it
    incInputBindings[actionKey].getBuilder().setDefault(button, mode);
}

bool incIsActionActivated(string actionKey) {
    // we assume actionKey is already in the hashmap, do not check it
    return incInputBindings[actionKey].isActivated();
}

bool incIsActionInactive(string actionKey) {
    // we assume actionKey is already in the hashmap, do not check it
    return incInputBindings[actionKey].isInactive();
}

/**
    incCommitBindingsChanges() commit changes to Memory
*/
void incCommitBindingsChanges() {
    if (incInputBindings.length == 0)
        throw new Exception("init input bindings first");

    foreach (entry; incInputBindings.values)
        entry.commitChanges();
}

/**
    incRevertBindingsChanges() revert changes to Memory, it could not revert committed changes
*/
void incRevertBindingsChanges() {
    if (incInputBindings.length == 0)
        throw new Exception("init input bindings first");

    foreach (entry; incInputBindings.values)
        entry.revertChanges();
}

const string INC_KEY_BINDING_VERSION = "0.0.1";

/**
    incSaveBindings() save committed changes to disk
*/
void incSaveBindings(string path) {
    JSONValue data = JSONValue();

    // set version
    data["keybindings_version"] = INC_KEY_BINDING_VERSION;

    // serialize bindings
    foreach (entry; incInputBindings.values) {
        data[entry.getKey()] = entry.toJSON();
    }

    // save to disk, atomic write
    string tmp_path = path ~ ".tmp";
    write(tmp_path, data.toString());
    rename(tmp_path, path);
}

/**
    incLoadBindings() load bindings from disk, and it should invoke by higher level function
    it do not configure the default key bindings, use incLoadBindingConfig() instead
*/
void incLoadBindings(string path) {
    // load bindings from disk
    // check version
    JSONValue data = JSONValue(parseJSON(readText(path)));
    if (data["keybindings_version"].get!string != INC_KEY_BINDING_VERSION)
        throw new Exception("Keybindings version mismatch");
    data.object.remove("keybindings_version");

    // pass one just check data
    bool checkOnly = true;
    incLoadToActionEntry(data, checkOnly);

    // pass two load data
    checkOnly = false;
    incLoadToActionEntry(data, checkOnly);
}

void incLoadToActionEntry(JSONValue data, bool checkOnly) {
    foreach (key, entry; data.object)
        if (key in incInputBindings)
            incInputBindings[key].fromJSON(key, entry, checkOnly);
}

void incLoadBindingsShowDialog() {
    // filter seems not working for .json, so we do not use it
    const TFD_Filter[] filters = [];

    string file = incShowOpenDialog(filters, _("Open..."));
    if (file)
        incLoadBindings(file);
}

void incSaveBindingsShowDialog() {
    const TFD_Filter[] filters = [
        { ["*.json"], "JSON (*.json)" }
    ];

    string file = incShowSaveDialog(filters, "keybindings.json", _("Save..."));
    if (file)
        incSaveBindings(file);
}

string incGetDefaultBindingPath() {
    import std.path : buildPath;
    import nijigenerate.core.path;
    return buildPath(incGetAppConfigPath(), "keybindings.json");
}