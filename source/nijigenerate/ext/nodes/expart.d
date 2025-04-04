/*
    nijilive Part extended with layer information
    previously Inochi2D Part extended with layer information

    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.ext.nodes.expart;
import nijilive.core.nodes.part;
import nijilive.core.nodes;
import nijilive.core;
import nijilive.fmt.serialize;
//import std.stdio : writeln;
import nijilive.math;

@TypeId("Part")
class ExPart : Part {
protected:
    override
    void serializeSelf(ref InochiSerializer serializer) {
        super.serializeSelf(serializer);
        serializer.putKey("psdLayerPath");
        serializer.putValue(layerPath);
    }

    override
    SerdeException deserializeFromFghj(Fghj data) {
        auto err = super.deserializeFromFghj(data);
        if (err) return err;

        if (!data["psdLayerPath"].isEmpty) data["psdLayerPath"].deserializeValue(layerPath);
        return null;
    }

public:
    /**
        Layer path to match against, should be in the following format:  
        /<layer group>/../<layer>  
        For single layers just the layer name suffices.
    
        Note that matches will fail if the structure of the PSD changes.
    */
    string layerPath;

    this(Node parent = null) { super(parent); }
    this(MeshData data, Texture[] textures, Node parent = null) { super(data, textures, parent); }
}

/**
   Creates a basic ExPart
*/
ExPart incCreateExPart(Texture tex, Node parent = null, string name = "New Part") {
	MeshData data = MeshData([
		vec2(-(tex.width/2), -(tex.height/2)),
		vec2(-(tex.width/2), tex.height/2),
		vec2(tex.width/2, -(tex.height/2)),
		vec2(tex.width/2, tex.height/2),
	], 
	[
		vec2(0, 0),
		vec2(0, 1),
		vec2(1, 0),
		vec2(1, 1),
	],
	[
		0, 1, 2,
		2, 1, 3
	]);
	ExPart p = new ExPart(data, [tex], parent);
	p.name = name;
    return p;
}

void incRegisterExPart() {
    inRegisterNodeType!ExPart();
}
