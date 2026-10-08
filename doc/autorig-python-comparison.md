# AutoRig Python comparison

The previous completion statement was incorrect: internal D readback checks
established consistency of the generated D rig, not equivalence to Python.
The current D implementation is assessed against a fresh complete original
Python execution, including actual parameter images. Internal consistency alone
must not be described as Python equivalence.

## General implementation requirement

The Python algorithm is the implementation specification. Matching one model's
images does not permit character-specific branches, filenames, UUID lookups,
fitted constants or manually authored pose corrections. Rig policies come from
the original common template and source semantic/alpha evidence. Production
code must not read the comparison artifacts under `out/`.

The identity-independence regression changes the entire imported UUID range,
root identity, source digest and material path prefix while retaining the same
semantic artwork. The compiler must produce identical Bone geometry, all native
curves, template registration, grid axes and depth values, with only source-node
references remapped. This check passes alongside the existing synthetic fixtures
for separate and bilateral materials and imported source-group partitions.
It detects identity dependence; it does not replace differential checks against
Python on additional source artwork.

## Same-source full execution on 2026-10-07

### Additional original-function audit

The 180-image comparison below did not cover every compiler decision. A further
audit found two genuine algorithm differences and corrected the shared D code:

- Shoulder eligibility now uses observed evidence landmarks before skeleton
  fitting and the original nominal-bounds pixel frame with width/height minus
  one. Bilateral arm materials are excluded from single-side shoulder tests.
- Cheek corrections use the hierarchy's face-origin Part, anatomical feature
  anchors, and the original eye/mouth supporting Part origins. Grid sampling
  clamps to the native axes. Outside the skin mesh, transfer uses the nearest
  vertex rather than the nearest triangle edge. Skin-local residuals are rounded
  before transfer to related facial Parts, with the original triangle thresholds.
- Initial anatomical groups follow the original left-before-right order.
- Explicit translation curves retain their zero counterpart, matching the
  original TRS command's two-axis write. A template containing only pelvis Y
  translation therefore still saves the zero pelvis X keys. The common template
  retains its 42 authored curves; the saved model contains 43 scalar Bone
  bindings, including this implicit zero series.

`tools/regression/autorig_compare_owned_logic.py` compares the original complete
Python artifacts with the D compiler replay on actual imported-model state. It
does not generate artwork fixtures or read model files. The added
`--audit-owned-state` test mode runs the production controls, cheek and shoulder
compilers without entering synthetic tests. On the completed fresh run
`f446fe16-b406-4ffd-87a6-38499cdfb1b4`, all 97 active materials match, all 255
local-control operations match within 0.00004 model units, and all 75 cheek
operations match within 0.00001. Protected skin vertices and shoulder matching
ranges agree exactly. Hierarchy origins differ by at most 2.274e-13.

Run `9546da5d-1bb2-4fd4-b67d-f0999d3ab747` completed all 15 stages and 180 pose
captures. All 21 saved Bone coordinates and flags match exactly; all 97 Part UVs
and triangle lists match exactly; nominal world vertices differ by at most
5.466e-5. Its 171-series readback identified the omitted zero translation series
described above. These reports are retained as `final-native-output-comparison.json`
and `final-logic-python-d/` and are not the final zero-series verification.

The executable containing that last correction completed all 15 stages from
the identical original import as `29adeca2-f782-4416-866b-2fed70f9d032`, each on
attempt 1. The saved `d-final-parity/model.inx` was reopened through NJC after
reading the original Python model. All 171 native binding series are present,
their axes and key-presence arrays agree, and maximum value error is 0.000198365
model units. Bone coordinates and flags, all 97 Part UV arrays, and all native
Part triangle lists agree exactly. Nominal world mesh vertex error is at most
0.000054654. All 255 saved local-control operations agree within 0.00004; all
75 saved cheek operations agree within 0.0000135. No control or correction
binding is missing. The production-source audit finds no Ao names, comparison
paths or character UUID constants. Reports use the `final-parity` prefix.

The final 180-pose render comparison is
`final-parity-python-d/comparison.json`, with paired review sheets in
`final-parity-review/`. Minimum silhouette IoU is 0.99999432469552, and neutral
silhouette IoU is exactly 1. Maximum premultiplied color MAE on visible support
is 0.029529459771903868 on a 0-255 scale; neutral color MAE is
0.022801835226485222. Neutral, face yaw/pitch, body roll and mouth-expression
sheets were visually inspected. `final-parity-summary.json` records source and
saved-model hashes and the exact report paths. These checks complete the
same-source Python/D comparison; they do not assert bit-identical pixels or
quality acceptance of the numerical deformation findings retained by Python.

The original skill entry point `run_psd_rig.py --render-images` was executed
without skipping its bundled template registration. The Ao PSD input SHA256 is
`78F487728A6F5C1FC8078BF3DFF1A173FAA78E2A2547370624C097C81FEB336F`.
The initial Python import is the input of the D comparison runs. Both outputs
are saved separately under `out/rig-comparison-20261007`.

The original full output has 21 bones and 42 native rotation/translation curves.
The previous D output had 19 bones and six simplified rotation curves. Its head
yaw/pitch limits were 22/15 degrees instead of 30/20 degrees. Static registration
now reads the exact single anatomical template, installs semantic TPS charts,
support bones, per-bone Z origins, depth fields, shoulder joins, hair overlap
ordering and all 42 curves. It uses the existing dense D solver without a runtime
Python dependency. Imported source UVs remain the sampling authority.

The registered program from run `ebb07da3-8a0d-4aee-a5c5-26f2abf5d5c6` matches
all bone names and constraints. Maximum endpoint error is 3.4106e-13 model units;
maximum native driver error is 1.4211e-14; native depth scale matches exactly.
Comparison against Python's final resampled depth has maximum error 5.9092e-5
model units. This numerical comparison does not establish final image parity.

Before the template correction, both full outputs were captured at the same
1920x1080 camera with all 167 Python pose inputs. Neutral silhouette IoU is
0.999989 and premultiplied color MAE is 0.02404 on occupied pixels. Body::Roll=1
silhouette IoU is only 0.785144. The old neutral-image claim used an unverified
different input and must not be used as same-source evidence.

The saved models are `python/model.inx` and `d-from-psd/model.inx`. The image
manifest is `d-from-psd/pairs.json`; comparisons and metrics are in
`actual-python-d/`. These D images explicitly describe the output **before** the
registered-template correction. The corrected output requires its own captures.

Grid generation now follows the original two-pass AutoMesh procedure, including
coalescing lines at native float32 precision and resampling depth at the generated
coordinates. Saved-grid checks compare each posed cell with its own rest area;
an average-cell threshold falsely rejected legitimate landmark-aligned narrow
cells. Regression tests verify that a narrow neutral cell passes while an actual
fold still fails.

## Saved native parity and final parameter captures

The user visually confirmed the Python-only reference sheets on 2026-10-07.
Those exact 180 original renders and their parameter inputs are the comparison
baseline. `reviewed-reference-comparison/` contains paired Python/D sheets for
face and body yaw/pitch, roll, both eyes, brows and mouth. Every pair is checked
against the reviewed reference's source path, SHA256 and parameter values.

The saved 21 Bone positions, pose Z origins, Node LockToRoot flags and
parent-inheritance settings match the original exactly. All 42 native curve
axes and key-presence arrays match; maximum float32 curve error is 6.7463e-7.
An extra surface Fit Z and a fixed-foot Hermite correction were removed because
the original registered workflow runs neither operation.

The original explicit bake removes preexisting automatically refreshed Grid
bindings, resets every parameter for each pose, iterates X before Y and submits
all native targets together. D now follows the same procedure. Previously the
native saved Grid keys differed by up to 8.507794 model units although Bone
inputs agreed. After the correction, all 84 semantic-surface angle bindings
agree within 0.0001835 model units. The comparison reads the live saved bindings
through NJC, not a separately calculated projection.

The earlier bake-correction baseline is `d-rebaked-final/model.inx`. Its `pairs.json` contains
all 167 original validation poses and 13 original head-support poses, using
identical parameter values, camera and 1920x1080 rendering. Its full
comparison is `rebaked-final-python-d/comparison.json`. Earlier `d-final` and
`final-python-d` artifacts are preserved attempts before the bake correction.
Across all 180 poses, minimum silhouette IoU is 0.99999432469552;
maximum premultiplied color MAE on visible support is 0.02513711791954543
on a 0-255 scale. Neutral silhouette IoU is exactly 1 and color MAE is
0.020646051587465. No alignment or resizing is used in these metrics.
Fixed common crops of neutral, body roll, face yaw and combined body/face poses
were also visually inspected. These results certify this same-source execution,
not untested character inputs or arbitrary parameter combinations.

Both workflows preserve numerical deformation findings instead of treating
successful stage execution as a numerical or visual quality acceptance. The
original reports four findings; the D report also checks intermediate and
combined poses and records 15 affected samples. Structural and ownership checks
pass. These findings remain in the reports and have not been suppressed.

## Historical pre-registration numerical differences

The following audit describes the earlier simplified implementation. The
registered-template implementation supersedes these missing scaffold, hierarchy,
depth, and driver policies. It is retained to explain the failed earlier result,
not as a description of the current production code.

`tools/regression/autorig_compare_python.py` executes explicitly selected,
independently authored Python anatomy, section, lexical classification and
material-grouping functions. It does not import the Python package or execute
its complete workflow. It compares owned artifacts of run
`cbcfafd5-e82c-4ff5-9504-8ebfdda0aeb4`.

The grouping and depth comparisons deliberately reuse D's observations and
assigned roles. They isolate differences in subsequent processing; they are not
a claim that a fresh PSD Python run has these exact intermediate values.
Native compile policy is reproduced from its independent pre-registration
domain selection, offset and bone-selection rules. It is evaluated on the
existing D sampling points and scale, without executing registration code.

| Check | Observed result |
| --- | --- |
| Initial lexical role/feature candidates on active parts | No differences among 97 parts; spatial fallback and final Python assignment are not certified |
| Anatomical scaffold on identical evidence | Maximum joint difference 1.7053e-13 model units |
| Python semantic grouping versus D domain splitting | 21 groups versus 28 domains; four hand/foot chart assignments also change |
| Python depth policy evaluated at D sample points | 42 parts differ; includes missing offsets and different surface-kind selection |
| Python bone-selection policy after canonical chart grouping | 13 parts differ |
| Chest station on the same observed cloud | 49.7093 model units difference |
| Waist station on the same observed cloud | 84.2937 model units difference |

The machine-readable detail is `out/autorig-python-comparison.json`, including
part identities, paths, depth differences and expected/actual bone lists.

## Historical pre-registration implementation mismatches

1. Python `psd_evidence.derive` measures chest/waist garment sections, projects
   them onto the torso axis, and uses .35/.75 only when measured stations violate
   its conditions. D `ngRigDeriveEvidence` always uses .35/.75. This changes the
   input to a scaffold solver that otherwise agrees numerically.
2. Python `hierarchy.material_groups` canonicalizes neck and terminal hand/foot
   charts and groups by semantic chart. D groups by owner/chart/source parent,
   preserving source-parent partitions as separate deformation domains.
3. Python `hierarchy.compile_hierarchy` constructs Body::Root, Head::Root and
   limb roots, selects body/face origin materials, and assigns surface parents
   through those material origins. D `buildRig` inserts grids under existing
   source parents and moves render units into them. That hierarchy is a different
   implementation; the named root/material-inheritance construction is absent.
4. Python's independent native domain rules offset back hair by body height
   times .003, other head coverings by .004, apron by .003, sleeves/shoulders by
   limb radius times .2, and tail by negative body height times .025. D initializes
   every domain offset to zero.
5. Bone assignment differs for pelvis/chest/thigh/ankle attachments and for
   merged hand/foot charts. For example, waist ornaments and plush parts have
   Pelvis-only sources under Python's selection, while D assigns the complete
   torso chain. Python's canonical leg skin chart uses Thigh/Shin/Foot; D makes
   separate Foot-only domains for the shoes.
6. D selects depth surface kinds from its role instead of the canonical chart
   label used by Python. On the compared inputs this also changes back-skirt
   domains, producing substantially larger differences than rounding errors.
7. Python evidence measures overlapping hair/face alpha and records depth order
   constraints. D evidence does not publish those constraints.

These mismatches are upstream of baking. More baked keys or successful saved
readbacks cannot repair them. The existing 1,680-key and 10,464-sample checks
validate D's own generated arrays and orientation; they do not validate the
Python hierarchy, measurements or target deformation.

## Corrected execution scope

The explanation that reference registration/reconstruction made the original
Python workflow prohibited was incorrect and is withdrawn. The actual AGENTS.md
boundary prohibits Live2D Cubism Core and information originating from it.
The independently authored nijigenerate anatomical template and its registration
and generated-model checks are not prohibited merely because they use the term
reference. These stages must not be omitted on that basis.

The independent-function comparison intentionally executes only selected
functions and cannot certify complete end-to-end equivalence. The original
`scripts/run_psd_rig.py --render-images` entry point is now being run separately
on the preserved Ao-latest PSD, including registration and finishing checks.

## Reproduce

From the development PowerShell:

```powershell
python tools/regression/autorig_compare_python.py `
  --scripts C:/Users/siget/src/nijigenerate-auto-rigging-skills/nijigenerate-deterministic-rig/scripts `
  --run C:/Users/siget/AppData/Roaming/.nijigenerate/autorig/runs/cbcfafd5-e82c-4ff5-9504-8ebfdda0aeb4 `
  --out out/autorig-python-comparison.json
```

Correction must start with observations/stations, canonical domains and the
material-origin hierarchy, then depth/driver/bone selection. Subsequent control,
bake and correction stages need comparison on those corrected inputs. A success
badge must not be treated as a Python equivalence check.

## Corrections in progress

The D compiler now projects measured chest/waist sections with the same fallback
conditions, canonicalizes hand/foot/neck charts, groups across source parents,
uses nominal domain bounds, quantizes axes/depth like the native Python compiler,
and applies its offset/surface-kind/bone-selection rules, including bilateral
limb support. The pure hierarchy compiler produces material-origin groups,
semantic surface parents, origin parts, clipping receivers and render units.

Recompiling the owned observation through the current D test executable reports
21 domains, zero depth/bone-policy differences, chest difference zero and waist
difference 5.68e-14. The old observation lacks source draw properties and stable
group order, so its two tied render-unit ordering differences are not certified
away. New observation/layout snapshots preserve source order and draw properties.

Native application now creates material origins, preserves neutral transforms
and absolute sort while moving render units, and puts facial mechanisms under
the face origin. Saved verification checks these parent/origin invariants.
Generic tests cover different source parents sharing one domain and a bilateral
arm with six source bones and zero prior depth. The standalone checks pass.

These are intermediate corrections. Ao-latest output-model equivalence and
complete control/bake/correction equivalence have not yet been established.

The fresh owned run `c70e9e36-f64a-4bd1-85cb-fc61d486bf05`, layout attempt 2,
also has zero hierarchy topology/render-unit differences when evaluated by
Python's independent hierarchy compiler. The report is
`out/autorig-python-fresh-comparison.json`. This proves the compared compile
decisions on those inputs, not end-to-end deformation parity.

Native application additionally connects retained imported group grids to the
same source bones as their semantic surface. Their depths must be initialized
before source attachment: missing depth samples otherwise enter the native
bone projection as NaN and corrupt parameter values. The current implementation
samples the parent surface at each imported group vertex and explicitly includes
those grids in angle baking. Saved readback checks source lists and influence
rules. Runtime and Python output-image comparisons remain required.

Runtime verification found and corrected a further API mismatch: native
`Node.zSort` already includes parent sort, whereas the inspector writes
`relZSort`. Summing `zSort` along ancestors counted parent values repeatedly,
and incorrectly hid sleeves, collar and thigh decorations behind skin in the
neutral image. Moves now preserve `zSortNoOffset` and subtract only the new
parent's `zSortNoOffset`; control snapshots/readbacks use `relZSort`.

Retained inner grids also exposed post-bake correction invalidation. Writing
the fixed-foot field with ordinary edit notifications regenerated its parent
bone output and replaced that field. Computed post-bake writes now use the
existing action's notification suppression path, consistent with native GPU
writeback. The saved fixed-foot field error fell from roughly 89 model units
to 0.00003. Final neutral and Python pose-image parity remain unverified.

The corrected sort run's neutral RGBA mean error is 0.1398 rather than 3.337.
Its saved numerical checks pass. The subsequent visual claim that head/blink
poses lose their lower body was incorrect: pixel comparisons show unchanged
bottom halves, including in the earlier captures. Diagnostic copies with
bindings removed or zeroed are not production corrections. Additional warmup,
capture-convergence and cache-invalidation changes based on that mistaken
observation were removed. `SaveScreenshot`
previously uploaded already straight RGBA into a temporary GPU texture and
called `Texture.save`, which divides by alpha again. It now directly encodes
the CPU RGBA buffer as PNG, avoiding double alpha division and changes to
renderer texture bindings. The successful numerical report
alone still does not establish Python image equivalence.

Run `86f035b3-0b8d-45a5-adbc-b38df94d80f3` starts again from the same owned
unrigged imported model with the corrected PNG encoder. Its independent Python
comparison (`out/autorig-python-recaptured-comparison.json`) again reports
97 parts, 21 semantic groups/domains, zero hierarchy/unit/depth/bone/lexical
differences and chest/waist station errors of 0/5.68e-14. These are explicitly
the permitted intermediate comparisons on D observations; Python output-model
pose images still require a supplied independent completed model.

The corrected encoder's fresh neutral RGBA mean error is 0.06960 on the same
1298-by-1285 camera frame. All 105 local-control pose images have exactly the
same lower-half premultiplied color pixels as their neutral image. These checks
validate preservation outside the face, not equality to Python's controls.

The final direct-encoding executable passes this run's verify attempt 3:
19 bones, 1,260 baked keys, 8,154 intermediate samples, no numerical findings
and 146 pose PNGs. `out/autorig-neutral-source-d-comparison.png` shows imported
source/D/difference on the same full frame; its comparison is not Python/D.
`out/autorig-d-local-control-review.png` is a D-only inspection sheet of all
105 control poses at one fixed image region. The saved D model is
`out/autorig-validated-d.inx`. Neither the workflow success nor these D image
checks complete the pending independent Python output-model comparison.

## Final image comparison

Render both completed models in the same application, viewport size, camera,
background and parameter values. Include neutral, head/body yaw-pitch and roll,
each eye blink/gaze/expression, eyebrows and mouth expressions. Preserve the
capture settings in the pose manifest. Do not fit each output separately or
align/resize the images to conceal rig differences.

`tools/regression/autorig_compare_images.py --pairs <manifest.json> --out <directory>`
produces side-by-side Python/D/color-difference/alpha-difference images and a
machine-readable report. The manifest is an array of `{pose, python, d}` records
with absolute paths. Color error uses premultiplied RGB and alpha support;
silhouette IoU and changed-pixel fraction are also reported. Numerical metrics
do not automatically accept a pose: inspect the rendered results and differences.
