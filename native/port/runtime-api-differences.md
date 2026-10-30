# Observed runtime API differences

## Node parent assignment

Current permitted sources inspected:

- `../nijilive/source/nijilive/core/nodes/node.d`: `parent(Node)` calls
  `insertInto(node, OFFSET_END)`, removing the node from its old parent's children
  and appending it to the new parent (or detaching it).
- `../nicxlive/core/nodes/node.cpp`: `setParent()` only assigns the weak parent
  pointer. `insertInto()` implements the structural relationship update.

`native/runtime/node.hpp::ngSetNodeParent` therefore calls `insertInto`, not
`setParent`. The integration probe checks initial attachment, movement between
parents, repeated assignment without duplicate children, and detachment.
This is a tested narrow mapping, not proof of all Node/notify/factory semantics.

The local nicxlive build emits C4828 warnings for existing invalid UTF-8 bytes in
`core/nodes/node.cpp`. This port has not modified that external source tree.

## Texture data readback and locked staging (M104 colorbleed)

`../nijilive/source/nijilive/core/texture.d::getTextureData(false)` returns
`lockedData` while locked. When unlocked it allocates a buffer and asks the
current rendering backend to read the texture. Its `setData(data, -1)` updates
locked staging/modified state when locked, or uploads and generates mipmaps
when unlocked (subject to rendering configuration and handle existence).

`../nicxlive/core/texture.hpp::data()` exposes `data_` only. The distinct
`lockedData_` is private and has no accessor. `texture.cpp::setData(data, -1)`
updates `lockedData_` while locked, but subsequent `data()` still returns the
old committed buffer. Reading `data()` would therefore make repeated bleeding
of a locked texture operate on stale pixels. It also does not perform the D
backend readback path. These APIs cannot be treated as interchangeable.

The `nijigenerate-compatible` nicxlive branch now adds a retained `TextureData`
handle API. `getTextureData` returns staging ownership when locked, otherwise
allocates a buffer and delegates actual readback to `RenderBackend`.
`setData(TextureData, channels)` retains the same staging handle while locked,
and invokes raw upload followed by conditional mipmap generation when unlocked.
`lock` reads through the new API; `unlock` commits modified staging through it.
The older vector setter retains its channel-conversion compatibility behavior.

`QueueRenderBackend` implements these methods from the current D queue source:
readback copies stored bytes and zero-fills any remaining output, raw upload
retains channel bookkeeping and copies/truncates/pads bytes, and queue mipmap
generation is intentionally a no-op. Other backends must implement the new
seams; the base methods throw unsupported rather than fabricate GPU results.
The no-handle read/write paths match the rendering-enabled D branch (zero-filled
readback and inert write). Existing C++ ownership of the originating backend is
retained; complete renderer-context parity remains a separate integration gate.

M104 now implements both the actual nicxlive `ShallowTexture` and `Texture&`
paths. It reads/writes retained handles; it never substitutes `data()` for
readback. The fixed-size mutable buffer sharing needed by texture processing is
implemented. Resizing a shared vector handle does not reproduce independent
D slice lengths or GC append capacity; general slice resize parity remains open.

## Ordered parameter bindings

The compatibility branch replaces the public `Parameter::bindingMap` with
`Parameter::bindings`, an ordered vector matching D's binding iteration order.
This is a C++ source API change: external users of `bindingMap` must migrate.
The current nicxlive source has one consumer outside Parameter itself,
`core/nodes/filter.cpp`; that consumer now iterates `bindings`.

Node lookup compares object identity rather than UUID equality. Deserialization
appends bindings as in D; remove erases the first identical binding. Reconstruction,
finalization and update preserve vector order. Parameter lifecycle methods and
the Puppet destructor support derived editor types. This does not yet establish
complete Parameter-target binding, creation/setZero, serialization or Puppet
lifecycle parity; those are separate remaining dependency tasks.

The existing resource lifetime test's in-memory backend now implements texture
read/upload/mipmap seams. Production backends still reject unsupported readback;
the test adaptation does not introduce a CPU-mirror fallback.

## Typed serialization events

`core/serde_json.hpp` adds `TypedJsonWriter` for ordered, typed JSON events,
including duplicate keys and explicit null versus empty values. It is included
by `serde.hpp`; the existing property-tree reader/writer remains available.
This additive API does not make the existing object serializers or readers
type-faithful. Each must be migrated explicitly. The D comparison covers 8,329
event outputs using the current fghj/mir dependencies on Windows/MSVC.

## Parameter creation and duplication details

The inner type of public `axisPoints` is now `common::SharedArray<float>` instead
of `std::vector<float>`. Copies share elements while descriptor lengths remain
independent, as required by D's outer-array duplication. Exact D GC append
capacity is still not reproduced and is not a completed parity claim.

`BindTarget` retains its Node arm and adds `parameterTarget`. Generic Resource
creation/duplication uses this descriptor; the existing `getNode` API still
returns Node and is not a complete implementation of D's Resource abstraction.
Typed and generic `createBinding`, zero-key initialization and virtual `dup`
are added. The generic Parameter-target overload preserves D's omitted setZero
forwarding. Targeted binding constructors now run clear after derived
construction so the virtual default-value operation uses the actual binding type.

Parameter factory storage is thread-local and initially unset. Standard factory
registration happens in runtime initialization, matching the D initialization
site. Direct C++ format loading previously worked without that initialization;
callers must now initialize the runtime. An unset C++ std::function throws,
whereas the D null delegate failure has a different failure mechanism.

## Puppet metadata and animation data

`Puppet::meta` and `physics` are shared class handles, initialized independently
by each constructor. Consumers use pointer member access. Assignment now shares
identity and null is representable; no implicit default replacement is added.
Typed metadata output matched 444 original D cases and the actual Puppet
ownership probe passed on Windows Debug. The separate typed metadata reader
matched 536 D cases, including null/reused handles, duplicate keys and partial
updates. Whole-Puppet typed read composition remains open.

`Puppet::animations` stores Animation values rather than placeholder shared
pointers. Animation lanes and frames retain D-style shared slice storage.
The Automation placeholder is replaced by its binding, lifecycle and per-thread
factory implementation. Typed reads and complete AnimationPlayer execution are
separate unfinished work; these API changes do not establish full file parity.

The additive `serde_reader.hpp` API requires Boost 1.90 and `Boost::json` linkage.
It retains JSON kinds, duplicate members, numeric tokens and stable owned views.
Its 1,757-case comparison covers the implemented scalar/sequence/vector boundary;
unverified Mir rounding and target-specific extreme-exponent behavior are listed
in the typed-reader contract. Existing property-tree callers are not converted
implicitly and cannot recover lost input types.

## Typed binding and mesh storage

Binding values and `getIsSet()` now use nested `SharedArray` storage. The bool
specialization exposes actual contiguous bool references instead of
`std::vector<bool>` proxies. Callers must preserve slice identity and distinguish
null from shortened empty arrays. The reader/constructor changes follow the D
binding contract; the parameter-only base constructor no longer clears values.
Parameter-target axis access derives from the current target name.

`MeshData::indices` and `gridAxes` use SharedArray as well. `copy()` duplicates
indices and shares grid-axis storage; `clearConnections()` shortens indices to
zero without discarding their storage identity. Render-backend uploads take an
explicit vector snapshot at their existing API boundary, while Unity pointer
access uses a span into the actual storage. External callers comparing vectors
with these fields need range comparisons rather than assuming identical types.
The integrated MeshData comparison passed 279 cases; the full 80-test checkpoint
also passed. Later checkpoints are recorded separately in the integration log.

Typed Node, SimplePhysics and DeformSlot overloads are additive. The legacy
property-tree path does not acquire their type-preserving semantics implicitly.
Full ExPuppet/INP read composition remains a separate dependency gate.

Puppet `parameters` and `automation` now use SharedArray. Ordinary copies share
elements while retaining independent slice lengths; reconstruct explicitly
duplicates each collection before callbacks, matching the original D `.dup`.
The NodeFilter Parameter-array consumers accept these same shared slices by
value, matching D descriptor copying. All overrides/helpers have been updated;
the full build and actual Puppet ownership probe passed.

Typed Parameter construction uses `inParameterCreate(TypedJsonView)` and the
thread-local `TypedParameterFactory`. Its setter is named
`inParameterSetTypedFactory` to keep existing `inParameterSetFactory({})`
calls unambiguous. Runtime initialization installs the D-compatible default
reader callback. This path does not convert JSON through property_tree.
