module nijigenerate.api.acp.types;

/// Position in a text document (zero-based).
struct Position {
    size_t line;
    size_t character;
}

/// Range within a document.
struct Range {
    Position start;
    Position end;
}

/// Single text edit.
struct TextEdit {
    Range range;
    string newText;
}

/// Workspace-level edit (single document for now).
struct WorkspaceEdit {
    string uri;
    TextEdit[] edits;
}

/// Document snapshot.
struct Document {
    string uri;
    string languageId;
    string text;
    size_t version_; // version is a keyword in D
}

/// Progress / status levels for notifications.
enum StatusLevel {
    info,
    warning,
    error,
    progress
}

/// Status notification payload.
struct StatusNotification {
    string title;
    string message;
    StatusLevel level;
}
