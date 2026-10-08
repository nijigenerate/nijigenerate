# Embedded deterministic rig resources

The 19 model-independent semantic front-chart templates, their manifest and
schema live in `res/autorig/templates`. The anatomical prior and material-role
rules live in `res/autorig`. D string imports embed these UTF-8 resources in the
executable at compile time. No runtime resource directory, Python runtime,
network request or additional parser library is required.

`nijigenerate.autorig.deterministic.templates` provides independent parsed JSON
values through `ngRigTemplate`, `ngRigHumanoidPrior` and `ngRigMaterialRoles`.
`ngRigValidateEmbeddedTemplates` checks all template identities, versions and
SHA-256 values against the original manifest. Resource changes require a rebuild.
The compiler needs the `res` string-import directory; the application and
standalone solver package both declare it.

The imported-model workflow additionally embeds the original single registered
anatomical template as `res/autorig/reference-humanoid.registered.json`. Its
original raw SHA256 is
`908B217553C870EA81B81902893AAB43B8D487871814127F048338989E865D71`.
`registered.d` checks this resource, solves the semantic TPS calibration with the
existing D dense solver, and evaluates its depth fields, 21-bone support graph
and 42 native rotation/translation curves. Translation curves use the observed
torso length. It preserves the template's Bone Z origins and Node LockToRoot
settings. It does not add a Fit Z pass or a fixed-foot override to the original
registered workflow. The template requires neither PSD reads nor Python at runtime.

The `evaluate-template` AutoRig preset accepts a `request` JSON artifact:

```json
{
  "template": "face_head",
  "bounds": [0, 0, 100, 120],
  "landmarks": {
    "eye_a": [30, 43.2],
    "eye_b": [70, 43.2],
    "nose_tip": [50, 68.4],
    "chin": [50, 120]
  },
  "parameters": {},
  "pose": [12, 8, 0]
}
```

Landmarks use source XY units. Bounds are `[left, top, right, bottom]`; pose is
`[yaw, pitch, roll]` in degrees. An optional `pivot` is a three-dimensional
source-space point. Optional tunable overrides must be declared by the template
and lie within its ranges. Required source landmarks must be supplied. Setting
`sourceFit=false` explicitly enables a bounds-only geometric preview, whose
result is marked as such. `surface_layer` requires explicit `hostDepth` values
in source-width units with one value per guide vertex.

The output contains declared grid axes, UV, fitted neutral XY, normalized depth,
projected and locally corrected XY, both grid validations and the request hash.
The evaluator implements section, sweep, compact relief, host offset, bend, wave
and ribbon-network depth operators, TPS/IDW landmark fitting, rigid rotation and
compact pose corrections. Grid checks reject local folds; they do not prove
global injectivity or resolve contact. Evaluation does not mutate the editor.

The `solve-scaffold` preset accepts `{"evidence": ...}` with measured landmarks,
source-to-model similarity transform and volumes. It emits a connected 19-bone
anatomical scaffold using the embedded prior. `sample-anatomical-depth` accepts
`query`, `domain` and `scaffold` fields. These presets use the existing AutoRig
worker Fiber and publish inspectable JSON artifacts.

Verification checks every manifest hash, independent returned JSON values, all
19 templates at neutral and a rotated pose, finite depth, exact neutral
preservation, linear fitting, grid sampling and orientation. Existing OSQP and
workflow dispatch tests are also included in `tests/autorig_solver.d`.

The complete PSD-to-rig workflow remains under implementation; these resource
and geometry presets are executable components, not a finished full-rig preset.
