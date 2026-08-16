module psd.rle;
import psd.layer;
import utils.io;
import std.exception;
import std.format;

/**
    Taken from psd_sdk

    https://github.com/MolecularMatters/psd_sdk/blob/master/src/Psd/PsdDecompressRle.cpp#L18
*/
void decodeRLE(ubyte[] source, ubyte[] destination) {
    size_t sourceOffset;
    size_t destinationOffset;

    while (destinationOffset < destination.length) {
        enforce(sourceOffset < source.length, "Truncated PSD RLE stream: missing PackBits tag");
        const ubyte tag = source[sourceOffset++];

        if (tag == 0x80) {
            // tag == -128 (0x80) is a no-op
        } else if (tag > 0x80) {
            // 0x81 - 0xFF: replicate the next source byte.
            // next 257-tag bytes are replicated from the next source tag
            const size_t count = cast(size_t)(257 - tag);
            enforce(sourceOffset < source.length, "Truncated PSD RLE stream: missing replicated byte");
            enforce(count <= destination.length - destinationOffset,
                "Invalid PSD RLE stream: replicated run exceeds destination");
            destination[destinationOffset .. destinationOffset + count] = source[sourceOffset];
            sourceOffset++;
            destinationOffset += count;
        } else {
            // 0x00 - 0x7F: copy the next tag+1 bytes.
            // copy next tag+1 bytes 1-by-1
            const size_t count = cast(size_t)(tag + 1);
            enforce(count <= source.length - sourceOffset,
                "Truncated PSD RLE stream: literal run exceeds source");
            enforce(count <= destination.length - destinationOffset,
                "Invalid PSD RLE stream: literal run exceeds destination");
            destination[destinationOffset .. destinationOffset + count] =
                source[sourceOffset .. sourceOffset + count];
            sourceOffset += count;
            destinationOffset += count;
        }
    }
}

ubyte[] decodeZip(ubyte[] source, uint width, uint height, bool prediction) {
    import std.zlib : uncompress;

    auto decodedLength = cast(size_t)width * cast(size_t)height;
    auto data = cast(ubyte[])uncompress(source, decodedLength);
    enforce(data.length == decodedLength, "Invalid ZIP-compressed PSD channel length");
    if (!prediction) return data;

    foreach (y; 0 .. height) {
        auto row = cast(size_t)y * cast(size_t)width;
        foreach (x; 1 .. width) {
            auto index = row + x;
            data[index] = cast(ubyte)(cast(uint)data[index] + cast(uint)data[index - 1]);
        }
    }
    return data;
}
