module utils.io;
import utils;
import std.bitmanip;
import std.string;

public import std.file;
public import std.stdio;

/**
    Reads file value in big endian fashion
*/
T readValue(T)(ref File file) {
    size_t currPos = file.tell();
    T value = bigEndianToNative!T(file.rawRead(new ubyte[T.sizeof])[0 .. T.sizeof]);
    return value;
}

/**
    Reads file value in big endian fashion
*/
T peekValue(T)(ref File file) {
    T val = file.readValue!T;
    file.skip(-cast(ptrdiff_t)T.sizeof);
    return val;
}

/**
    Reads a string
*/
string readStr(ref File file, uint length) {
    return cast(string) file.rawRead(new ubyte[length]);
}

/**
    Peeks a string
*/
string peekStr(ref File file, uint length) {
    string val = file.readStr(length);
    file.seek(-(cast(int)length), SEEK_CUR);
    return val;
}

/**
    Reads a pascal string
*/
string readPascalStr(ref File file, uint readTo = 0) {
    uint length = file.readValue!ubyte;
    uint extra = readTo > 0 ? readTo-length : 0;
    if (length == 0) {
        file.skip(1);
        return "";
    }

    string str = file.readStr(length);
    if (extra > 0) file.skip(extra);
    return str[0..length];
}

/**
    Reads values
*/
ubyte[] read(ref File file, size_t length) {
    return file.rawRead(new ubyte[length]);
}

/**
    Peeks values
*/
ubyte[] peek(ref File file, ptrdiff_t length) {
    ubyte[] result = file.read(length);
    file.seek(-cast(ptrdiff_t)length, SEEK_CUR);
    return result;
}

/**
    Skips bytes
*/
void skip(ref File file, ptrdiff_t length) {
    file.seek(cast(ptrdiff_t)length, SEEK_CUR);
}