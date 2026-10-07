module nijigenerate.autorig.deterministic.contracts;

import core.thread.fiber : Fiber;
import nijigenerate.autorig.framework : AutoRigTaskContext;
import std.exception : enforce;
import std.json : JSONValue, JSONType;
import std.math : isFinite;
import std.digest.sha : sha256Of;
import std.digest : toHexString;
import std.algorithm : sort;
import std.array : appender, array;
import std.stdio : File;
import std.digest.sha : SHA256;
import std.string : toLower;

alias Point2 = double[2];
alias Point3 = double[3];

ulong ngRigUnsigned(JSONValue value) {
    if (value.type == JSONType.uinteger) return value.uinteger;
    enforce(value.type == JSONType.integer && value.integer>=0,"Expected an unsigned rig integer");
    return cast(ulong)value.integer;
}

double ngRigNumber(JSONValue value) {
    enforce(value.type == JSONType.float_ || value.type == JSONType.integer || value.type == JSONType.uinteger,
        "Expected a numeric rig value");
    double result = value.type == JSONType.float_ ? value.floating : value.type == JSONType.integer ?
        cast(double)value.integer : cast(double)value.uinteger;
    enforce(isFinite(result), "Nonfinite rig number");
    return result;
}

double[] ngRigNumbers(JSONValue value) {
    double[] result;
    foreach (entry; value.array) result ~= ngRigNumber(entry);
    return result;
}

Point2 ngRigPoint(JSONValue value) {
    auto numbers = ngRigNumbers(value);
    enforce(numbers.length == 2, "Expected a two-dimensional rig point");
    return [numbers[0], numbers[1]];
}

Point2[] ngRigPoints(JSONValue value) {
    Point2[] result;
    foreach (entry; value.array) result ~= ngRigPoint(entry);
    return result;
}

JSONValue ngRigPointsJson(Point2[] points) {
    JSONValue[] result;
    foreach (point; points) result ~= JSONValue(point[]);
    return JSONValue(result);
}

JSONValue ngRigGet(JSONValue object, string key, JSONValue fallback = JSONValue.init) {
    enforce(object.type == JSONType.object, "Expected a rig object");
    auto value = key in object.object;
    return value is null ? fallback : *value;
}

double ngRigScalar(JSONValue object, string key, double fallback) {
    return ngRigNumber(ngRigGet(object, key, JSONValue(fallback)));
}

string ngRigString(JSONValue object, string key, string fallback = "") {
    return ngRigGet(object, key, JSONValue(fallback)).str;
}

/** Canonical D artifact identity. This schema does not reuse Python JSON hashes. */
string ngRigCanonical(JSONValue value) {
    if (value.type == JSONType.object) {
        auto keys = value.object.keys.sort.array;
        auto output = appender!string();
        output.put('{');
        foreach (i, key; keys) {
            if (i) output.put(',');
            output.put(JSONValue(key).toString());
            output.put(':');
            output.put(ngRigCanonical(value[key]));
        }
        output.put('}');
        return output.data;
    }
    if (value.type == JSONType.array) {
        auto output = appender!string();
        output.put('[');
        foreach (i, entry; value.array) {
            if (i) output.put(',');
            output.put(ngRigCanonical(entry));
        }
        output.put(']');
        return output.data;
    }
    if (value.type == JSONType.float_) enforce(isFinite(value.floating), "Nonfinite artifact value");
    return value.toString();
}

string ngRigDigest(JSONValue value) {
    return sha256Of(ngRigCanonical(value)).toHexString.idup;
}

void ngRigCheckpoint(AutoRigTaskContext context) {
    enforce(context is null || !context.isCanceled(), "Automatic rig canceled");
    if (Fiber.getThis() !is null) Fiber.yield();
    enforce(context is null || !context.isCanceled(), "Automatic rig canceled");
}

string ngRigFileDigest(string path) {
    auto file = File(path,"rb");
    SHA256 digest;
    foreach (chunk; file.byChunk(64*1024)) digest.put(chunk);
    return digest.finish().toHexString.idup.toLower;
}
