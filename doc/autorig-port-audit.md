# Deterministic rig port audit

**Correction:** earlier internal checks did not establish Python equivalence.
The complete original Python pipeline has now been executed on the same source
as D. Registered anatomy, hierarchy, depth, all 21 bones and 42 native curves
are compared against its actual output. See `autorig-python-comparison.md` for
the differential evidence and the distinction between execution, numerical
findings and rendered image comparison.

The final actual-model audit additionally covers all 171 saved native binding
series, all 97 Part UVs and triangle lists, and 330 saved local-control/cheek
operations. The 42 template curves produce 43 saved scalar Bone bindings because
the original TRS command explicitly writes a zero counterpart for a single-axis
translation. This implicit series is retained in D as well. Shared shoulder
frames, facial anchor pins, grid clamping, outside-mesh transfer and rounding
now follow the original Python functions; no character-specific branch was added.

This document tracks the original script pipeline against the imported-model D
implementation. A successful build alone does not establish pipeline parity.
The fifteen-stage imported-model workflow has completed real-model execution,
saved-model numerical readback and optional preview generation. This record
distinguishes those checks from differential equivalence to other implementations.

The user selected the imported model as the input. PSD extraction and import are
therefore the upstream source boundary; equivalent layer names, hierarchy,
clipping links, texture alpha and UV mapping must be captured from that model.
The port uses the existing editor commands and worker Fiber. Python and njc
subprocesses are not part of production execution.

| Original responsibility | Current status |
| --- | --- |
| Source identity, fresh intermediate ownership and resume checks | Implemented; checkpoint digests and restart/retry regression checks pass |
| Imported source names, hierarchy, UVs and alpha observations | Implemented; source observation and UV-to-root saved readback pass |
| Semantic name normalization, parent scope and clipping receivers | Implemented; classification fixtures and real-model automatic classification pass |
| Unknown ornaments assigned to nearest alpha support with recorded candidates | Implemented and checked on the failing model's 97 active materials without overrides |
| Explicit background and geometric opaque-perimeter backdrop exclusion | Implemented with classification fixtures |
| Source group preparation and face/eye/mouth composite coverage | Implemented, preserving source identities and shared render boundaries |
| Largest connected sclera support for eye landmarks | Implemented with eight-connected source alpha components |
| Humanoid, face, mouth and local mechanism selection | Implemented; only humanoids receive the anatomical scaffold |
| Measured anatomical evidence and registered scaffold | Implemented; all 21 saved bones, pose Z origins, constraints and 42 curves compared with the original |
| Native Part AutoMesh settings and proximal shoulder preparation | Implemented including exact distance contours and shoulder-specific sampling |
| Native mesh save and exact readback | Implemented with source TRS and UV-to-root verification |
| Domain assembly, grouped carriers and hierarchy application | Implemented with registered semantic charts and material-origin hierarchy |
| Embedded surface templates and geometry/depth evaluation | Exact common anatomical template compiled in; pure D TPS registration, bilateral fields, shoulder depth joins and hair ordering implemented |
| Native skeleton, bone sources, constraints and standard parameters | Implemented; saved native skeleton, driver and constraint readbacks pass |
| Source UV-to-parent affine registration before deformation authoring | Implemented, with immutable source UV-to-root readback |
| Native shoulder welding and exact correspondence readback | Implemented with matching-contour eligibility, native vertex pairs and saved readback |
| Local shape controls and facial endpoint constraints | Eye contact profiles, smile endpoints, combined lid bands, gaze, shared brows, mouth, local bend and source eyelash topology implemented; saved local operation readbacks pass |
| Stored and effective depth input validation | Implemented with native depth unit and per-source settings validation |
| Depth angle baking and parameter ownership | Existing automatic Grid keys removed before explicit native baking; all parameters reset for each pose, original iteration order and complete target hierarchy preserved; native refresh drained |
| Face orientation, cheek/contour corrections and fixed feet constraints | Native orientation, near-cheek residuals and Node LockToRoot foot constraints implemented; registered workflow excludes the extra Fit Z and unrequested fixed-foot Hermite stage |
| Saved-rig structure, neutral/intermediate/extreme deformation validation | Scaffold, native drivers, baked keys, local controls, cheek keys, composites, welding and source UV readbacks pass; 10,464 intermediate samples include combined face/body yaw and roll |
| Optional rendering, neutral comparison and head-support review | Implemented behind the render option; numerical results do not imply visual acceptance |
| Shared nijigenerate anatomical-template registration and reconstruction checks | Implemented using the exact independently authored template; endpoint, curve and surface comparisons recorded |
| Live2D Cubism Core and information originating from it | Permanently prohibited by AGENTS.md; this boundary does not prohibit independently authored nijigenerate anatomical templates |
| Attempt-all completion report with stage errors and provenance | Implemented; explicit failed finishing stages retain rollback checkpoints and remain failed |

Tests must distinguish a numerical result, a completed stage, an attempted stage,
and a visually reviewed result. Optional image generation must remain optional.
No completion claim may be inferred from a subset of passing stages.

## Correction of the execution restriction

The earlier explanation that the Python workflow could not run because it
performs reference registration/reconstruction was wrong. Those names refer
to the bundled nijigenerate anatomical template and generated model checks.
They do not justify omitting these stages or declaring them prohibited.
The original `scripts/run_psd_rig.py --render-images` entry point is being
executed on the preserved Ao-latest PSD for full output comparison.

## Real-model validation

Run `8a0770ab-a23a-4abd-8660-385ffde30efb` completed all fifteen stages.
Its second saved-rig verification attempt reported `passed: true`, with no
numerical findings. It generated all fifteen yaw/pitch previews and the saved
neutral preview. Neutral RGBA mean absolute error was 0.1359531 against the
captured source image (the acceptance limit is 1).

Representative opposing head poses were inspected. Colored artifacts behind
the lower body already appear in the imported source preview, so this run does
not establish a clean original artwork/rendering baseline or general artistic
acceptance for every model. Visual acceptance remains separate in the report.

A subsequent fresh run, `cbcfafd5-e82c-4ff5-9504-8ebfdda0aeb4`, completed every
stage on its first attempt without material-role overrides. Its saved validation
again passed all 1,680 keys and 10,464 intermediate samples, with nineteen bones,
fifteen head previews and neutral mean absolute error 0.1360955.
