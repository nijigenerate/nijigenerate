module nijigenerate.api.mcp.helpers;

import std.json;
import std.stdio : writefln;
import std.meta : AliasSeq;
import std.traits : isInstanceOf, TemplateArgsOf, EnumMembers;
import std.variant : Variant;
import inmath : vec2u, vec3;
import nijigenerate.commands.base : CommandResult, ResourceResult, TW, BaseExArgsOf;
import nijigenerate.commands : Context, AllCommandMaps;
import nijigenerate.core.shortcut.base : ngBuildExecutionContext;
import nijigenerate.core.selector.resource : Resource, to;
import nijigenerate.project : incActivePuppet;
import nijilive; // Node, Parameter, Puppet
import nijilive.core.nodes : Node;
import nijilive.core.param : Parameter;
import nijilive.core.param.binding : ParameterBinding;
import nijigenerate.ext.param : ExParameterGroup;

// Encode resource helpers
private JSONValue _encodeResource(R)(R r) {
    JSONValue[string] m;
    m["type"] = JSONValue(R.stringof);
    static if (__traits(hasMember, R, "uuid")) m["uuid"] = JSONValue(cast(long)r.uuid);
    static if (__traits(hasMember, R, "name")) m["name"] = JSONValue(r.name);
    return JSONValue(m);
}

private JSONValue _encodeResourceResult(R)(ResourceResult!R rr) {
    JSONValue[string] m;
    m["succeeded"] = JSONValue(rr.succeeded);
    m["message"] = JSONValue(rr.message);
    m["change"] = JSONValue(rr.change.stringof);
    JSONValue arrCreated = JSONValue(JSONType.array);
    foreach (c; rr.created) arrCreated.array ~= _encodeResource(c);
    JSONValue arrDeleted = JSONValue(JSONType.array);
    foreach (d; rr.deleted) arrDeleted.array ~= _encodeResource(d);
    JSONValue arrLoaded = JSONValue(JSONType.array);
    foreach (l; rr.loaded) arrLoaded.array ~= _encodeResource(l);
    m["created"] = arrCreated;
    m["deleted"] = arrDeleted;
    m["loaded"] = arrLoaded;
    return JSONValue(m);
}

JSONValue commandResultToJson(CommandResult res) {
    JSONValue[string] m;
    m["status"] = JSONValue(res.succeeded ? "ok" : "error");
    m["succeeded"] = JSONValue(res.succeeded);
    if (res.message.length) m["message"] = JSONValue(res.message);
    auto pt = res.payload.type;
    if (pt && pt !is typeid(void)) {
        bool handled = false;
        static foreach (R; AliasSeq!(Puppet, Parameter, ExParameterGroup, ParameterBinding)) {
            if (res.payload.convertsTo!(ResourceResult!R)) {
                auto rr = res.payload.get!(ResourceResult!R);
                m["payload"] = _encodeResourceResult(rr);
                handled = true;
            }
        }
        if (!handled) {
            m["payloadType"] = JSONValue(pt.toString());
        }
    }
    return JSONValue(m);
}

// Build a Context from default app state plus optional overrides
Context buildContextFromPayload(JSONValue payloadCopy) {
    auto ctx = ngBuildExecutionContext();
    if ("context" !in payloadCopy || payloadCopy["context"].type != JSONType.object) return ctx;
    auto cobj = payloadCopy["context"];
    auto puppet = incActivePuppet();
    if (puppet !is null) {
        if ("parameters" in cobj && cobj["parameters"].type == JSONType.array) {
            Parameter[] params;
            foreach (u; cobj["parameters"].array) {
                if (u.type == JSONType.integer) {
                    auto p = puppet.find!(Parameter)(cast(uint)u.integer);
                    if (p !is null) params ~= p;
                }
            }
            if (params.length) ctx.parameters = params;
        }
        if ("armedParameters" in cobj && cobj["armedParameters"].type == JSONType.array) {
            Parameter[] aparams;
            foreach (u; cobj["armedParameters"].array) {
                if (u.type == JSONType.integer) {
                    auto p = puppet.find!(Parameter)(cast(uint)u.integer);
                    if (p !is null) aparams ~= p;
                }
            }
            if (aparams.length) ctx.armedParameters = aparams;
        }
        if ("nodes" in cobj && cobj["nodes"].type == JSONType.array) {
            Node[] nodes;
            foreach (u; cobj["nodes"].array) {
                if (u.type == JSONType.integer) {
                    auto n = puppet.find!(Node)(cast(uint)u.integer);
                    if (n !is null) nodes ~= n;
                }
            }
            if (nodes.length) ctx.nodes = nodes;
        }
    }
    if ("keyPoint" in cobj && cobj["keyPoint"].type == JSONType.array && cobj["keyPoint"].array.length >= 2) {
        auto a = cobj["keyPoint"].array;
        if ((a[0].type == JSONType.integer || a[0].type == JSONType.float_)
         && (a[1].type == JSONType.integer || a[1].type == JSONType.float_)) {
            ctx.keyPoint = vec2u(cast(uint)(a[0].type == JSONType.integer ? a[0].integer : cast(long)a[0].floating),
                                 cast(uint)(a[1].type == JSONType.integer ? a[1].integer : cast(long)a[1].floating));
        }
    }
    return ctx;
}

// Apply top-level payload to command instance fields (ExCommand args)
void applyPayloadToInstance(C)(C inst, JSONValue payloadCopy) {
    static if (!is(BaseExArgsOf!C == void)) {
        alias Declared = BaseExArgsOf!C;
        static foreach (i, Param; Declared) {{
            static if (isInstanceOf!(TW, Param)) {
                enum fname = TemplateArgsOf!Param[1];
                alias TParam = TemplateArgsOf!Param[0];
            } else {
                enum fname = "arg" ~ i.stringof;
                alias TParam = Param;
            }
            if (fname in payloadCopy) {
                auto val = payloadCopy[fname];
                static if (is(TParam == bool)) {
                    if (val.type == JSONType.true_ || val.type == JSONType.false_) mixin("inst."~fname~" = (val.type==JSONType.true_);");
                } else static if (is(TParam == enum)) {
                    static if (__traits(compiles, cast(string) TParam.init)) {
                        if (val.type == JSONType.string) {
                            static foreach (mem; EnumMembers!TParam) {{
                                static if (__traits(compiles, cast(string)mem)) {
                                    enum string memStr = cast(string)mem;
                                    if (val.str == memStr) { mixin("inst."~fname~" = mem;"); }
                                }
                            }}
                        }
                    } else {
                        if (val.type == JSONType.integer) mixin("inst."~fname~" = cast(TParam) cast(int) val.integer;");
                    }
                } else static if (is(TParam == int) || is(TParam == uint) || is(TParam == long) || is(TParam == ulong)) {
                    if (val.type == JSONType.integer) mixin("inst."~fname~" = cast("~TParam.stringof~")val.integer;");
                } else static if (is(TParam == float) || is(TParam == double)) {
                    if (val.type == JSONType.float_) mixin("inst."~fname~" = cast("~TParam.stringof~")val.floating;");
                    else if (val.type == JSONType.integer) mixin("inst."~fname~" = cast("~TParam.stringof~")cast(double)val.integer;");
                } else static if (is(TParam == string)) {
                    if (val.type == JSONType.string) mixin("inst."~fname~" = val.str;");
                } else static if (is(TParam == vec2u)) {
                    if (val.type == JSONType.array && val.array.length >= 2) {
                        auto a = val.array;
                        uint x = cast(uint)(a[0].type==JSONType.integer ? a[0].integer : cast(long)a[0].floating);
                        uint y = cast(uint)(a[1].type==JSONType.integer ? a[1].integer : cast(long)a[1].floating);
                        mixin("inst."~fname~" = vec2u(x,y);");
                    }
                } else static if (is(TParam == vec3)) {
                    if (val.type == JSONType.array && val.array.length >= 3) {
                        auto a = val.array;
                        float x = cast(float)(a[0].type==JSONType.float_ ? a[0].floating : cast(double)a[0].integer);
                        float y = cast(float)(a[1].type==JSONType.float_ ? a[1].floating : cast(double)a[1].integer);
                        float z = cast(float)(a[2].type==JSONType.float_ ? a[2].floating : cast(double)a[2].integer);
                        mixin("inst."~fname~" = vec3(x,y,z);");
                    }
                } else static if (is(TParam == float[])) {
                    if (val.type == JSONType.array) {
                        float[] outv;
                        foreach (e; val.array) {
                            if (e.type == JSONType.float_)
                                outv ~= cast(float)e.floating;
                            else if (e.type == JSONType.integer)
                                outv ~= cast(float)cast(double)e.integer;
                        }
                        mixin("inst."~fname~" = outv;");
                    }
                } else static if (is(TParam == float[2])) {
                    if (val.type == JSONType.array && val.array.length >= 2) {
                        auto a = val.array;
                        float x = cast(float)(a[0].type==JSONType.float_ ? a[0].floating : cast(double)a[0].integer);
                        float y = cast(float)(a[1].type==JSONType.float_ ? a[1].floating : cast(double)a[1].integer);
                        mixin("inst."~fname~" = [x,y];");
                    }
                } else static if (is(TParam == float[3])) {
                    if (val.type == JSONType.array && val.array.length >= 3) {
                        auto a = val.array;
                        float x = cast(float)(a[0].type==JSONType.float_ ? a[0].floating : cast(double)a[0].integer);
                        float y = cast(float)(a[1].type==JSONType.float_ ? a[1].floating : cast(double)a[1].integer);
                        float z = cast(float)(a[2].type==JSONType.float_ ? a[2].floating : cast(double)a[2].integer);
                        mixin("inst."~fname~" = [x,y,z];");
                    }
                } else static if (is(TParam == ushort[])) {
                    if (val.type == JSONType.array) {
                        ushort[] outv;
                        foreach (e; val.array) {
                            if (e.type == JSONType.float_)
                                outv ~= cast(ushort)e.floating;
                            else if (e.type == JSONType.integer)
                                outv ~= cast(ushort)cast(double)e.integer;
                        }
                        mixin("inst."~fname~" = outv;");
                    }
                } else static if (is(TParam == uint[2])) {
                    if (val.type == JSONType.array && val.array.length >= 2) {
                        auto a = val.array;
                        uint x = cast(uint)(a[0].type==JSONType.integer ? a[0].integer : cast(long)a[0].floating);
                        uint y = cast(uint)(a[1].type==JSONType.integer ? a[1].integer : cast(long)a[1].floating);
                        mixin("inst."~fname~" = [x,y];");
                    }
                } else static if (is(TParam : Node)) {
                    if (val.type == JSONType.integer) {
                        if (auto puppet = incActivePuppet()) {
                            auto nodeVal = puppet.find!(TParam)(cast(uint)val.integer);
                            if (nodeVal !is null) mixin("inst."~fname~" = nodeVal;");
                        }
                    }
                } else static if (is(TParam : Parameter)) {
                    if (val.type == JSONType.integer) {
                        if (auto puppet = incActivePuppet()) {
                            auto pVal = puppet.find!(TParam)(cast(uint)val.integer);
                            if (pVal !is null) mixin("inst."~fname~" = pVal;");
                        }
                    }
                } else static if (is(TParam : Resource)) {
                    if (val.type == JSONType.integer) {
                        if (auto puppet = incActivePuppet()) {
                            auto nVal = puppet.find!(Node)(cast(uint)val.integer);
                            if (nVal !is null) mixin("inst."~fname~" = cast(TParam) nVal;");
                            else {
                                auto pVal = puppet.find!(Parameter)(cast(uint)val.integer);
                                if (pVal !is null) mixin("inst."~fname~" = cast(TParam) pVal;");
                            }
                        }
                    }
                }
            }
        }}
    }
}
