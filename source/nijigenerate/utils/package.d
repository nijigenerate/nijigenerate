module nijigenerate.utils;


/**
    Gets an icon from a nijilive Type ID
*/
string incTypeIdToIcon(string typeId) {
    switch(typeId) {
        case "Part": return "";
        case "Composite": return "";
        case "Mask": return "\ue14e";
        case "SimplePhysics": return "";
        case "Camera": return "";
        case "MeshGroup": return "\ue886";
        case "DynamicComposite": return "";
        case "PathDeformer": return "";
        case "GridDeformer": return "\ue3ec";
        case "Parameter": return "";
        case "Binding": return "";
        default: return "\ue97a"; 
    }
}
