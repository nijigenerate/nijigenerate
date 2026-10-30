# imagefmt 2.1.2 image providers

This directory ports the complete writer call graph required by
`source/nijigenerate/io/imageexport.d` from the independently installed BSD-2-Clause
imagefmt 2.1.2 sources. It is not a complete port of the external imagefmt library.
The separate reader target now supplies real BMP, TGA, PNG and JPEG decoding,
including the shared 8/16-bit conversions. PNG and JPEG complete original-source
closures were statically inspected after the export provider was introduced.
This does not establish Windows runtime acceptance or complete coverage of every
public API in the external package.

`imagefmt_reader.cmake` registers all four decoder translation units and links
the actual writer/allocation/conversion provider. PNG read/write share the same
thread-local sticky error state and real zlib provider. JPEG uses its baseline
entropy decoder, IDCT and reconstruction code; it does not promise unsupported
progressive formats. The current source audit receipts are
`build/review-sweep-current/heartbeat-1752/imagefmt-png-whole-static-audit.json`
and `build/review-sweep-current/heartbeat-1752/imagefmt-jpeg-reader-whole-source-current.json`.

| Original file | Implemented export scope |
| --- | --- |
| `package.d` | Format/error constants, allocation ownership, in-memory Writer, growth, endian writes, buffer writes, reachable 8-bit conversions, `write_image_mem` and dispatch |
| `bmp.d` | Complete `write_bmp`: dimensions/channels, V4 header/masks, conversion, padding, orientation and errors |
| `tga.d` | Complete `write_tga`, `write_tga_idat`, `rle_compress`: all 1–4 channel conversions, RLE/raw packets, footer, orientation and errors |
| `png.d` | Complete `write_png`, encoder/chunk state, Paeth filtering, zlib compression/finish, CRC, IDAT/IEND and errors |
| `jpeg.d` | No writer exists in 2.1.2; JPEG and unknown formats retain `ERROR.unsupp` from the original dispatcher |

`writer.hpp` exposes only the actual memory-writer provider needed by the C++
export implementation and the file writer required by the texture inspector.
The file writer preserves the independent package.d filename detection, narrow
C fopen path, 4096-byte buffering, partial-output/flush error behavior, and ignored
fclose result. The Texture.save call intentionally ignores the returned error.
Internal 8-bit conversion loops combine the equivalent
per-channel original functions; CRC uses the same polynomial directly instead
of the original lookup table. Both preserve the algorithm's results.

The allocator family and vertical orientation retain D module thread-local
storage. Each Buffer snapshots its allocation family so RAII cannot release an
allocation through a later replacement allocator. PNG retains the original
thread-local, first-error-wins state, including the fact that `sete(0)` does not
clear a previous error. The original successful-encode shrink allocation failure
also retains its null-buffer/unchanged-error result.

The following explicit safety adaptations avoid relying on uninitialized bytes
or C++ undefined behavior: BMP padding is zeroed; Writer capacity growth is
bounded before signed overflow; PNG rejects an unrepresentable zlib line size;
TGA work-buffer allocation failure returns out-of-memory before touching the
buffer; PNG exits on failed Writer allocation instead of repeatedly trying to
flush the same full zlib buffer. Windows write failure closes its handle instead
of leaking it through the original short-circuit condition. All six adaptations
are accepted by the root implementation decision; Windows behavioral validation
is pending. This permits counting the complete M151 source implementation and
production connection, not a tested or acceptance-Done result.

`imageexport.cpp` preserves encoding before the file/status catch boundary,
formatting before translation, file-error reporting and allocator-paired release.
It rethrows `std::bad_alloc` (D allocation Errors are outside `catch(Exception)`).
It uses the actual file stream, translation and notification implementations.

Extension case conversion now reuses the real generated Phobos lowercase data
and the existing replacement decoder (`core/i18n_case.hpp`). The original
`std.uni.toLower` uses `byDchar`, whose `byUTF` default replaces malformed UTF;
it does not always throw a UTF exception. The original re-encoded-prefix offset
and possible D bounds Error for malformed input before a cased character are
preserved, outside the file-write catch boundary. The strict `compat/utf8.hpp`
path adapter is unsuitable here because it rejects malformed UTF and NUL where
the original Windows path conversion replaces UTF errors and passes NUL through.

Windows paths therefore use that same existing replacement decoder followed by
UTF-16 encoding. File operations use the actual CreateFileW/WriteFile/CloseHandle
sequence, including exclusive sharing, CREATE_ALWAYS and 1 GiB chunks. Open,
write, close and success-status exceptions are mapped to the original fopen
failure status; exceptions while constructing that failure status escape.
Translation receives the already-formatted message, and its real current
implementation uses byte-preserving string lookup/fallback.

The PNG implementation requires a real `ZLIB::ZLIB` target. The existing
`nicxlive/utils/stb_image_write.h` placeholder is intentionally not used: its PNG
callback emits TGA bytes and it has no BMP/JPEG writer. No new external package
was downloaded or installed.

No build or test was run while the project source-first gate remained active.
Registration and source inspection do not constitute runtime acceptance.
