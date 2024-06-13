/*
    Copyright © 2020-2023, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.ext.nodes;

public import nijigenerate.ext.nodes.expart;
public import nijigenerate.ext.nodes.excamera;

void incInitExtNodes() {
    incRegisterExPart();
    incRegisterExCamera();
}