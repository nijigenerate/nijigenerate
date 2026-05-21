/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.ext.nodes;

public import nijigenerate.ext.nodes.expart;
public import nijigenerate.ext.nodes.excamera;
public import nijigenerate.ext.nodes.exdepthmapped;
public import nijigenerate.ext.nodes.exdepthops;
public import nijigenerate.ext.nodes.exdepthbone;
public import nijigenerate.ext.nodes.exgriddeformer;

void incInitExtNodes() {
    incRegisterExPart();
    incRegisterExCamera();
    ngRegisterExDepthBoneNodes();
    incRegisterExGridDeformer();
}
