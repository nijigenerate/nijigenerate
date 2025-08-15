module nijigenerate.core.shortcut.defaults;

// Registers built-in shortcuts for core commands.
// Kept separate from nijigenerate.core.shortcut to avoid module cycles.

import nijigenerate.core.shortcut : ngRegisterShortcut;
import nijigenerate.commands.puppet.file;
import nijigenerate.commands.puppet.edit;
import nijigenerate.commands.node.node;
import nijigenerate.commands.viewport.control;
import nijigenerate.core.input : _K;

void ngRegisterDefaultShortcuts()
{
    // File
    ngRegisterShortcut(_K!"Ctrl-N", nijigenerate.commands.puppet.file.commands[FileCommand.NewFile]);
    ngRegisterShortcut(_K!"Ctrl-O", nijigenerate.commands.puppet.file.commands[FileCommand.ShowOpenFileDialog]);
    ngRegisterShortcut(_K!"Ctrl-S", nijigenerate.commands.puppet.file.commands[FileCommand.ShowSaveFileDialog]);
    ngRegisterShortcut(_K!"Ctrl-Shift-S", nijigenerate.commands.puppet.file.commands[FileCommand.ShowSaveFileAsDialog]);

    // Edit
    ngRegisterShortcut(_K!"Ctrl-Shift-Z", nijigenerate.commands.puppet.edit.commands[EditCommand.Redo], true);
    ngRegisterShortcut(_K!"Ctrl-Z", nijigenerate.commands.puppet.edit.commands[EditCommand.Undo], true);

    // Node
    ngRegisterShortcut(_K!"Ctrl-X", nijigenerate.commands.node.node.commands[NodeCommand.CutNode], true);
    ngRegisterShortcut(_K!"Ctrl-C", nijigenerate.commands.node.node.commands[NodeCommand.CopyNode], true);
    ngRegisterShortcut(_K!"Ctrl-V", nijigenerate.commands.node.node.commands[NodeCommand.PasteNode], true);

    // Viewport control (add when appropriate)
}

