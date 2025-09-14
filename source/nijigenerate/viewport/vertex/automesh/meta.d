module nijigenerate.viewport.vertex.automesh.meta;

// Minimal scaffolding for AutoMesh reflection

enum AutoMeshLevel { Preset, Simple, Advanced }

struct AMParam {
    AutoMeshLevel level;
    string id;
    string label;
    string desc;
    string widget;
    float min = float.nan;
    float max = float.nan;
    float step = float.nan;
    int order = 0;
}
struct AMArray { float min = float.nan; float max = float.nan; float step = float.nan; }
struct AMEnum { string[] values; }
struct AMPreset { string name; }
struct AMPresetProvider {}

interface IAutoMeshReflect {
    string amSchema();
    string amValues(string levelName);
    bool   amApplyPreset(string name);
    bool   amWriteValues(string levelName, string updatesJson);
}

