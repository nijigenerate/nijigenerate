/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.utils.repair;
import nijilive;

private {
}

/**
    Attempts to repair a corrupt puppet
*/
void incAttemptRepairPuppet(Puppet p) {

    // Attempt to repair node IDs
    incRegenerateNodeIDs(p.root);

    // Finish off by rescanning nodes.
    p.rescanNodes();
}

/**
    Attempts to repair puppet IDs
*/
void incRegenerateNodeIDs(Node n) {
    foreach(child; n.children) {
        incRegenerateNodeIDs(child);
    }

    // Force a new UUID
    n.forceSetUUID(inCreateUUID());
}

void incPremultTextures(Puppet p) {
    foreach(ref Texture texture; p.textureSlots) {
        ubyte[] data = texture.getTextureData();
        inTexPremultiply(data);
        texture.setData(data);
    }
}
