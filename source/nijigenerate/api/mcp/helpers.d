module nijigenerate.api.mcp.helpers;

import std.json;
import std.conv : to;
import std.stdio : writefln;
import std.meta : AliasSeq;
import std.traits : isInstanceOf, TemplateArgsOf, EnumMembers;
import inmath : vec2, vec2u, vec3;
import nijigenerate.commands.base : CommandResult, ExCommandResult, ExCommandResultImpl, TW, BaseExArgsOf, CreateResult, DeleteResult, LoadResult;
import nijigenerate.commands : Context, AllCommandMaps;
import nijigenerate.core.shortcut.base : ngBuildExecutionContext;
import nijigenerate.core.selector.resource : Resource, to;
import nijigenerate.project : incActivePuppet;
import nijilive; // Node, Parameter, Puppet
import nijilive.core.resource : LiveResource = Resource;
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

private JSONValue _encodeCreateResult(R)(CreateResult!R rr) {
    JSONValue[string] m;
    m["succeeded"] = JSONValue(rr.succeeded);
    if (rr.message.length) m["message"] = JSONValue(rr.message);
    JSONValue arrCreated = JSONValue.emptyArray;
    foreach (c; rr.created) arrCreated.array ~= _encodeResource(c);
    m["created"] = arrCreated;
    return JSONValue(m);
}
private JSONValue _encodeDeleteResult(R)(DeleteResult!R rr) {
    JSONValue[string] m;
    m["succeeded"] = JSONValue(rr.succeeded);
    if (rr.message.length) m["message"] = JSONValue(rr.message);
    JSONValue arrDeleted = JSONValue.emptyArray;
    foreach (d; rr.deleted) arrDeleted.array ~= _encodeResource(d);
    m["deleted"] = arrDeleted;
    return JSONValue(m);
}
private JSONValue _encodeLoadResult(R)(LoadResult!R rr) {
    JSONValue[string] m;
    m["succeeded"] = JSONValue(rr.succeeded);
    if (rr.message.length) m["message"] = JSONValue(rr.message);
    JSONValue arrLoaded = JSONValue.emptyArray;
    foreach (l; rr.loaded) arrLoaded.array ~= _encodeResource(l);
    m["loaded"] = arrLoaded;
    return JSONValue(m);
}

// Generic encoder for result payloads
private JSONValue _encodeValue(T)(auto ref T v) {
    static if (is(T == bool)) return JSONValue(v);
    else static if (is(T : long) || is(T : ulong) || is(T : int) || is(T : uint) || is(T : short) || is(T : ushort)) return JSONValue(cast(long)v);
    else static if (is(T : double) || is(T : float)) return JSONValue(cast(double)v);
    else static if (is(T == string)) return JSONValue(v);
    else static if (is(T : Resource)) return _encodeResource(v);
    else static if (is(T == JSONValue)) return v;
    else static if (is(T : const(JSONValue))) return v;
    else static if (is(T == Parameter[])) {
        JSONValue arr = JSONValue.emptyArray;
        foreach (p; v) arr.array ~= _encodeResource(p);
        return arr;
    } else static if (is(T == Node[])) {
        JSONValue arr = JSONValue.emptyArray;
        foreach (n; v) arr.array ~= _encodeResource(n);
        return arr;
    } else {
        import std.conv : to;
        return JSONValue(to!string(v));
    }
}

// Encode CommandResult using static result type RT when available
JSONValue commandResultToJson(RT)(RT res) if (is(RT : CommandResult)) {
    JSONValue[string] m;
    m["status"] = JSONValue(res.succeeded ? "ok" : "error");
    m["succeeded"] = JSONValue(res.succeeded);
    if (res.message.length) m["message"] = JSONValue(res.message);

    static if (is(RT : CreateResult!R, R)) {
        m["result"] = _encodeCreateResult(cast(CreateResult!R)res);
        m["resultType"] = JSONValue("CreateResult!(" ~ R.stringof ~ ")");
    } else static if (is(RT : DeleteResult!R, R)) {
        m["result"] = _encodeDeleteResult(cast(DeleteResult!R)res);
        m["resultType"] = JSONValue("DeleteResult!(" ~ R.stringof ~ ")");
    } else static if (is(RT : LoadResult!R, R)) {
        m["result"] = _encodeLoadResult(cast(LoadResult!R)res);
        m["resultType"] = JSONValue("LoadResult!(" ~ R.stringof ~ ")");
    } else static if (isInstanceOf!(ExCommandResultImpl, RT)) {
        alias T = TemplateArgsOf!RT[0];
        auto er = cast(ExCommandResultImpl!T) res;
        m["resultType"] = JSONValue(T.stringof);
        m["result"] = _encodeValue(er.result);
    } else {
        m["resultType"] = JSONValue("CommandResult");
    }
    return JSONValue(m);
}

JSONValue commandResultToJsonRuntime(CommandResult res) {
    JSONValue[string] m;
    m["status"] = JSONValue(res.succeeded ? "ok" : "error");
    m["succeeded"] = JSONValue(res.succeeded);
    if (res.message.length) m["message"] = JSONValue(res.message);

    if (auto er = cast(ExCommandResultImpl!JSONValue)res) {
        m["resultType"] = JSONValue("JSONValue");
        m["result"] = er.result;
        return JSONValue(m);
    }

    alias ResourceTypes = AliasSeq!(Node, Parameter, ParameterBinding, Puppet, ExParameterGroup);
    static foreach (R; ResourceTypes) {{
        if (auto cr = cast(CreateResult!R)res) {
            m["result"] = _encodeCreateResult(cr);
            m["resultType"] = JSONValue("CreateResult!(" ~ R.stringof ~ ")");
            return JSONValue(m);
        }
        if (auto dr = cast(DeleteResult!R)res) {
            m["result"] = _encodeDeleteResult(dr);
            m["resultType"] = JSONValue("DeleteResult!(" ~ R.stringof ~ ")");
            return JSONValue(m);
        }
        if (auto lr = cast(LoadResult!R)res) {
            m["result"] = _encodeLoadResult(lr);
            m["resultType"] = JSONValue("LoadResult!(" ~ R.stringof ~ ")");
            return JSONValue(m);
        }
    }}

    m["resultType"] = JSONValue("CommandResult");
    return JSONValue(m);
}

// Build a Context from default app state plus optional overrides
private bool _jsonNumber(JSONValue value, out float result) {
    if (value.type == JSONType.float_) {
        result = cast(float)value.floating;
        return true;
    }
    if (value.type == JSONType.integer) {
        result = cast(float)cast(double)value.integer;
        return true;
    }
    return false;
}

private bool _readParamValue(JSONValue value, out vec2 parameterValue, out string message) {
    if (value.type != JSONType.array || value.array.length == 0) {
        message = "context.parameterValue must be [x] or [x,y]";
        return false;
    }

    float x;
    if (!_jsonNumber(value.array[0], x)) {
        message = "context.parameterValue[0] must be a number";
        return false;
    }

    float y = 0;
    if (value.array.length >= 2 && !_jsonNumber(value.array[1], y)) {
        message = "context.parameterValue[1] must be a number";
        return false;
    }

    parameterValue = vec2(x, y);
    return true;
}

private bool _readBindingDescriptors(JSONValue value, Puppet puppet, Parameter param, out ParameterBinding[] bindings, out string message) {
    if (value.type != JSONType.array) {
        message = "context.bindings must be an array of {target, name}";
        return false;
    }
    if (param is null) {
        message = "context.bindings requires context.parameters[0]";
        return false;
    }

    foreach (entry; value.array) {
        if (entry.type != JSONType.object) {
            message = "context.bindings entries must be objects";
            return false;
        }

        auto obj = entry.object;
        string targetKey = ("target" in obj) ? "target" : (("targetUuid" in obj) ? "targetUuid" : "");
        string nameKey = ("name" in obj) ? "name" : (("bindingName" in obj) ? "bindingName" : "");
        if (targetKey.length == 0 || nameKey.length == 0) {
            message = "context.bindings entries require target and name";
            return false;
        }
        if (obj[targetKey].type != JSONType.integer || obj[targetKey].integer < 0) {
            message = "context.bindings target must be a non-negative integer UUID";
            return false;
        }
        if (obj[nameKey].type != JSONType.string || obj[nameKey].str.length == 0) {
            message = "context.bindings name must be a non-empty string";
            return false;
        }

        auto targetUuid = cast(uint)obj[targetKey].integer;
        LiveResource target = puppet.find!(Node)(targetUuid);
        if (target is null) target = puppet.find!(Parameter)(targetUuid);
        if (target is null) {
            message = "context.bindings target UUID was not found: " ~ targetUuid.to!string;
            return false;
        }

        auto binding = param.getBinding(target, obj[nameKey].str);
        if (binding is null) {
            message = "Binding was not found on parameter '" ~ param.name ~ "': target=" ~
                targetUuid.to!string ~ ", name=" ~ obj[nameKey].str;
            return false;
        }
        bindings ~= binding;
    }

    if (bindings.length == 0) {
        message = "context.bindings must contain at least one binding descriptor";
        return false;
    }
    return true;
}

Context buildContextFromPayload(JSONValue payloadCopy) {
    auto ctx = ngBuildExecutionContext();
    if ("context" !in payloadCopy || payloadCopy["context"].type != JSONType.object) return ctx;
    auto cobj = payloadCopy["context"];
    bool hasExplicitParameterContext = ("parameters" in cobj) || ("armedParameters" in cobj);
    auto puppet = incActivePuppet();
    if (puppet !is null) {
        if ("parameters" in cobj && cobj["parameters"].type == JSONType.array) {
            ctx.hasParameters = false;
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
            ctx.hasArmedParameters = false;
            Parameter[] aparams;
            foreach (u; cobj["armedParameters"].array) {
                if (u.type == JSONType.integer) {
                    auto p = puppet.find!(Parameter)(cast(uint)u.integer);
                    if (p !is null) aparams ~= p;
                }
            }
            if (aparams.length) ctx.armedParameters = aparams;
        } else if ("parameters" in cobj) {
            // An explicit parameter context must not inherit the app's armed parameter.
            ctx.hasArmedParameters = false;
        }
        if ("nodes" in cobj && cobj["nodes"].type == JSONType.array) {
            ctx.hasNodes = false;
            Node[] nodes;
            foreach (u; cobj["nodes"].array) {
                if (u.type == JSONType.integer) {
                    auto n = puppet.find!(Node)(cast(uint)u.integer);
                    if (n !is null) nodes ~= n;
                }
            }
            if (nodes.length) ctx.nodes = nodes;
        }
        if ("bindings" in cobj || "activeBindings" in cobj) {
            auto value = ("bindings" in cobj) ? cobj["bindings"] : cobj["activeBindings"];
            Parameter param = (ctx.hasParameters && ctx.parameters.length > 0) ? ctx.parameters[0] : null;
            ParameterBinding[] bindings;
            string message;
            if (!_readBindingDescriptors(value, puppet, param, bindings, message))
                throw new Exception(message);
            ctx.activeBindings = bindings;
        }
    }
    if ("keyPoint" in cobj)
        throw new Exception("context.keyPoint is not supported by MCP. Use context.parameterValue with parameter-axis values.");
    if ("parameterValue" in cobj && "paramValue" in cobj)
        throw new Exception("Use only one of context.parameterValue or deprecated context.paramValue.");
    if (hasExplicitParameterContext && !("parameterValue" in cobj) && !("paramValue" in cobj)) {
        // The key point from the active editor belongs to the previous armed parameter.
        ctx.hasKeyPoint = false;
        ctx.hasExplicitKeyPoint = false;
    }
    if ("parameterValue" in cobj || "paramValue" in cobj) {
        auto value = ("parameterValue" in cobj) ? cobj["parameterValue"] : cobj["paramValue"];
        vec2 parameterValue;
        string message;
        if (!_readParamValue(value, parameterValue, message))
            throw new Exception(message);

        ctx.parameterValue = parameterValue;
        ctx.hasKeyPoint = false;
        ctx.hasExplicitKeyPoint = false;
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
                    if (val.type == JSONType.string) {
                        static foreach (mem; EnumMembers!TParam) {{
                            enum string memName = __traits(identifier, mem);
                            static if (__traits(compiles, cast(string)mem)) {
                                enum string memValue = cast(string)mem;
                                if (val.str == memName || val.str == memValue) mixin("inst."~fname~" = mem;");
                            } else {
                                if (val.str == memName) mixin("inst."~fname~" = mem;");
                            }
                        }}
                    } else if (val.type == JSONType.integer) {
                        // Backward compatibility for older clients that used numeric enum ordinals.
                        static if (__traits(compiles, cast(TParam) cast(int) 0)) {
                            mixin("inst."~fname~" = cast(TParam) cast(int) val.integer;");
                        }
                    }
                } else static if (is(TParam == int) || is(TParam == uint) || is(TParam == long) || is(TParam == ulong)) {
                    if (val.type == JSONType.integer) mixin("inst."~fname~" = cast("~TParam.stringof~")val.integer;");
                } else static if (is(TParam == float) || is(TParam == double)) {
                    if (val.type == JSONType.float_) mixin("inst."~fname~" = cast("~TParam.stringof~")val.floating;");
                    else if (val.type == JSONType.integer) mixin("inst."~fname~" = cast("~TParam.stringof~")cast(double)val.integer;");
                } else static if (is(TParam == string)) {
                    if (val.type == JSONType.string) mixin("inst."~fname~" = val.str;");
                } else static if (is(TParam == string[])) {
                    if (val.type == JSONType.array) {
                        string[] outv;
                        foreach (e; val.array) {
                            if (e.type == JSONType.string)
                                outv ~= e.str;
                        }
                        mixin("inst."~fname~" = outv;");
                    }
                } else static if (is(TParam == JSONValue)) {
                    mixin("inst."~fname~" = val;");
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
            } else static if (is(TParam == JSONValue)) {
                mixin("inst."~fname~" = JSONValue.init;");
            }
        }}
    }
}
