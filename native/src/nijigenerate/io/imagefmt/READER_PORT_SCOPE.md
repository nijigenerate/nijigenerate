# Imagefmt reader provider

The independent imagefmt 2.1.2 `package.d` supplies the Reader, allocation,
conversion and format-dispatch behavior. The existing allocator-owning Buffer
retains the allocator family across moves and destruction. Configuration remains
thread local (`maximumImageSize`, `verticalOrientationRead`). The production
consumer uses memory input, with real Windows file input handled separately by
the existing `ngReadProjectFile` provider.

The decoder detection order is PNG, BMP, JPEG, TGA. Codec errors are not inferred
from another codec's text. The consumer uses the actual imagefmt Error and
errorText to preserve the unsupported-image branch. Requested texture channels
remain conversion metadata: decoding itself requests original channels and
8-bit output, exactly as the independent ShallowTexture constructor does.

The BGR-to-gray conversion retains the original green component used twice in
the weighted luminance expression. Buffer expansion and reduction preserve the
original 8-to-16 multiplication by 257 and 16-to-8 high-byte extraction.

C++ safety adaptations: invalid negative stream read counts, oversized buffers
and arithmetic overflow fail without invalid access. Exhausted stream block
reads terminate with fail rather than looping forever. Memory-buffer refill
retains the original reset-to-start and fail behavior. Allocation failure while
creating a stream buffer reports out-of-memory rather than an empty success.
These exceptional-input adaptations are explicit and are not a claim of exact
Windows behavioral equivalence. No build or tests have been run.

The shared provider is not a whole external package migration. File-name reader
overloads, generic public conversion-function symbols and unused public helpers
are not claimed complete. BMP/TGA/PNG/JPEG reader bodies are separate original
file scopes. M178 is incomplete until all codec implementations and production
registration arrive and the full direct closure is statically checked.
