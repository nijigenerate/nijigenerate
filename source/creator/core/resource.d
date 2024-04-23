module creator.core.resource;

import inochi2d;
import std.string;
import std.conv;
import std.uni;
import std.utf;
import std.algorithm;
import std.array;
import creator.core.selector.tokenizer;
import creator.core.selector.parser;
import creator;

enum ResourceType {
    Node,
    Parameter,
    Binding,
    Vec2,
    Vec3,
    Transform,
    IntArray,
    FloatArray,
}

class ResourceInfo(T) { static ResourceType type(); }
class ResourceInfo(T: Node) { static ResourceType type() { return ResourceType.Node; }}
class ResourceInfo(T: Parameter) { static ResourceType type() { return ResourceType.Parameter; }}
class ResourceInfo(T: ParameterBinding) { static ResourceType type() { return ResourceType.Binding; }}
class ResourceInfo(T: vec2[]) { static ResourceType type() { return ResourceType.Vec2; }}
class ResourceInfo(T: vec3[]) { static ResourceType type() { return ResourceType.Vec3; }}
class ResourceInfo(T: Transform) { static ResourceType type() { return ResourceType.Transform; }}
class ResourceInfo(T: int[]) { static ResourceType type() { return ResourceType.IntArray; }}
class ResourceInfo(T: float[]) { static ResourceType type() { return ResourceType.FloatArray; }}

class Resource {
protected:
    ResourceType type_;
public:
    abstract string name();
    abstract uint uuid();
    abstract string typeId();
    ResourceType type() { return type_; }
}

class Proxy(T: Node) : Resource {
    T target;
public:
    override
    string name() { return target.name; }
    override
    string typeId() { return target.typeId; }
    override
    uint uuid() { return target.uuid; }

    this(T obj) {
        target = obj;
        type_ = ResourceInfo!T.type;
    }

    ref T obj() {
        return target;
    }
}

class Proxy(T: Parameter) : Resource {
    T target;
public:
    override
    string name() { return target.name; }
    override
    uint uuid() { return target.uuid; }
    override
    string typeId() { return "Parameter"; }

    this(T obj) {
        target = obj;
        type_ = ResourceInfo!T.type;
    }

    ref T obj() {
        return target;
    }
}

class Proxy(T: ParameterBinding) : Resource {
    T target;
public:
    override
    string name() { return target.getName; }
    override
    uint uuid() { return target.getNodeUUID; }
    override
    string typeId() { return "Binding"; }

    this(T obj) {
        target = obj;
        type_ = ResourceInfo!T.type;
    }

    ref T obj() {
        return target;
    }
}