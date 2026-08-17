module psd.rle;
import psd.layer;
import utils.io;
import std.exception;
import std.format;
import std.algorithm : min;

/**
    Taken from psd_sdk

    https://github.com/MolecularMatters/psd_sdk/blob/master/src/Psd/PsdDecompressRle.cpp#L18
*/
void decodeRLE(ubyte[] source, ubyte[] destination) {
    size_t sourceOffset;
    size_t destinationOffset;

    while (sourceOffset < source.length) {
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
    enforce(destinationOffset == destination.length,
        "Truncated PSD RLE stream: scanline is shorter than its destination");
}

ubyte[] decodeZip(ubyte[] source, uint width, uint height, bool prediction) {
    import std.zlib : UnCompress;

    auto decodedLength = cast(size_t)width * cast(size_t)height;
    auto data = new ubyte[decodedLength];
    auto decoder = new UnCompress(64 * 1024);
    size_t sourceOffset;
    size_t decodedOffset;
    enum size_t compressedChunkSize = 1024;
    while (sourceOffset < source.length && !decoder.empty) {
        auto sourceEnd = min(source.length, sourceOffset + compressedChunkSize);
        auto chunk = cast(const(ubyte)[])decoder.uncompress(source[sourceOffset .. sourceEnd]);
        enforce(chunk.length <= decodedLength - decodedOffset,
            "ZIP-compressed PSD channel exceeds its declared dimensions");
        data[decodedOffset .. decodedOffset + chunk.length] = chunk;
        decodedOffset += chunk.length;
        sourceOffset = sourceEnd;
    }
    auto tail = cast(const(ubyte)[])decoder.flush();
    enforce(tail.length <= decodedLength - decodedOffset,
        "ZIP-compressed PSD channel exceeds its declared dimensions");
    data[decodedOffset .. decodedOffset + tail.length] = tail;
    decodedOffset += tail.length;
    enforce(decoder.empty && decodedOffset == decodedLength,
        "Invalid ZIP-compressed PSD channel length");
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
