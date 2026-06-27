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
    import core.stdc.string : memset, memcpy;

    ubyte* dest = destination.ptr;
    ubyte* src = source.ptr;
    uint bytesRead = 0u;
    uint offset = 0u;
    size_t size = destination.length;
    
    while (offset < size)
    {
        const ubyte tag = *src++;
        ++bytesRead;

        if (tag == 0x80)
        {
            // tag == -128 (0x80) is a no-op
        }
        // 0x81 - 0XFF
        else if (tag > 0x80)
        {
            // next 257-tag bytes are replicated from the next source tag
            const uint count = cast(uint)(257 - tag);
            ubyte data = *src++;

            memset(dest + offset, data, count);
            offset += count;

            ++bytesRead;
        }
        // 0x00 - 0x7F
        else
        {
            // copy next tag+1 bytes 1-by-1
            const uint count = cast(uint)(tag + 1);
            
            memcpy(dest + offset, src, count);

            src += count;
            offset += count;

            bytesRead += count;
        }
    }
}