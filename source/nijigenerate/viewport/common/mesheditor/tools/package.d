module nijigenerate.viewport.common.mesheditor.tools;

public import nijigenerate.viewport.common.mesheditor.tools.enums;
public import nijigenerate.viewport.common.mesheditor.tools.base;
public import nijigenerate.viewport.common.mesheditor.tools.select;
public import nijigenerate.viewport.common.mesheditor.tools.point;
public import nijigenerate.viewport.common.mesheditor.tools.connect;
public import nijigenerate.viewport.common.mesheditor.tools.pathdeform;
public import nijigenerate.viewport.common.mesheditor.tools.bezierdeform;
public import nijigenerate.viewport.common.mesheditor.tools.grid;
public import nijigenerate.viewport.common.mesheditor.tools.brush;
public import nijigenerate.viewport.common.mesheditor.tools.lasso;
public import nijigenerate.viewport.common.mesheditor.tools.onetimedeform;
import nijilive;

private {
    ToolInfo[] infoList;
}

ToolInfo[] incGetToolInfo() {
    if (infoList.length == 0) {
        infoList ~= new ToolInfoImpl!(PointTool);
        infoList ~= new ToolInfoImpl!(ConnectTool);
        infoList ~= new ToolInfoImpl!(PathDeformTool);
        infoList ~= new ToolInfoImpl!(GridTool);
        infoList ~= new ToolInfoImpl!(BrushTool);
        infoList ~= new ToolInfoImpl!(LassoTool);
        infoList ~= new ToolInfoImpl!(BezierDeformTool);
//        infoList ~= new ToolInfoImpl!(OneTimeDeform!MeshGroup); // Disabled tool temporary
//        infoList ~= new ToolInfoImpl!(OneTimeDeform!PathDeformer); // Disabled tool temporary
    }
    return infoList;
}

ToolInfo ngGetToolInfoOf(VertexToolMode mode) {
    foreach (info; infoList) {
        if (info.mode == mode) {
            return info;
        }
    }
    return null;
}