module nijigenerate.io.pngheader;

import std.exception : enforce;
import std.stdio : File;

private uint pngHeaderUint32(const(ubyte)[] bytes) {
    enforce(bytes.length == 4, "PNG header integer must contain four bytes");
    return (cast(uint)bytes[0] << 24) |
        (cast(uint)bytes[1] << 16) |
        (cast(uint)bytes[2] << 8) |
        cast(uint)bytes[3];
}

void ngInspectPngDimensions(string path, out int width, out int height) {
    auto file = File(path, "rb");
    ubyte[24] header;
    auto readHeader = file.rawRead(header[]);
    enforce(readHeader.length == header.length,
        "PNG source is too short to contain an IHDR header");
    enforce(header[0 .. 8] == cast(const(ubyte)[])[
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A],
        "PNG source has an invalid signature");
    enforce(pngHeaderUint32(header[8 .. 12]) == 13 &&
        header[12 .. 16] == cast(const(ubyte)[])"IHDR",
        "PNG source does not begin with an IHDR chunk");
    auto headerWidth = pngHeaderUint32(header[16 .. 20]);
    auto headerHeight = pngHeaderUint32(header[20 .. 24]);
    enforce(headerWidth > 0 && headerHeight > 0 &&
        headerWidth <= int.max && headerHeight <= int.max,
        "PNG source dimensions are invalid");
    width = cast(int)headerWidth;
    height = cast(int)headerHeight;
}
