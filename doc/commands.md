# Commands Switch Matrix

This table lists the command-level switches that currently exist in the codebase.

Column meanings:

- `runnable()`:
  - `default` = inherited `ExCommand` implementation (`true`)
  - `custom` = overridden by the command
- `shortcutRunnable()`:
  - `true` = visible in the Shortcuts settings UI
  - `false` = hidden from the Shortcuts settings UI
- `TW hidden args`:
  - argument names hidden from the Command Browser via `TW!(..., hidden=true)`
- `Template switch`:
  - template-level switch that feeds into `TW.hidden`
  - currently this only appears in the node command families
- `GUI display`:
  - `-` = no direct GUI display
  - `window`, `dialog`, `modal`, `popup`, `confirm`, `dialog-output`, `dialog+window`, `layout` = command directly triggers that GUI path
- `Irreversible effect`:
  - `-` = no persistent or one-way side effect
  - `file-write`, `import`, `project-reset`, `config-edit`, `apply`, `keyframe-edit`, `binding-edit`, `structural-edit`, `create`, `delete`, `rename`, `texture-regenerate`, `repair`, `layout-reset` = command can cause that class of non-trivial side effect
- `MCP exposure`:
  - `tool` = exposed through MCP tool registration
  - `hidden` = filtered out from MCP tool registration

| Command / family | File | Generated | `runnable()` | `shortcutRunnable()` | `TW hidden args` | Template switch | GUI display | Irreversible effect | MCP exposure | Notes |
|---|---|---:|---|---:|---|---|---|---|---|---|
| `ApplyAutoMeshPT<PT>` | [automesh/dynamic.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/dynamic.d) | yes | custom | true | - | - | - | apply | tool | Per-processor apply command family |
| `AutoMeshGetSchemaCommand` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | no | custom | true | - | - | dialog-output | - | hidden | - |
| `AutoMeshGetValuesCommand` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | no | custom | true | - | - | dialog-output | - | hidden | - |
| `AutoMeshSetPresetCommand` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | no | custom | true | - | - | - | config-edit | tool | Argument-driven |
| `AutoMeshSetValuesCommand` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | no | custom | true | - | - | - | config-edit | tool | Requires `updates` payload |
| `AutoMeshListProcessorsCommand` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | no | custom | true | - | - | dialog-output | - | hidden | - |
| `GetAutoMeshConfigPT<PT>` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | yes | custom | true | - | - | - | - | hidden | Generated per processor |
| `SetAutoMeshConfigPT<PT>` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | yes | custom | true | - | - | - | config-edit | tool | Generated per processor |
| `AutoMeshSetSimple_<PT>Command` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | yes | custom | true | - | - | - | config-edit | tool | Generated simple-level setters |
| `AutoMeshSetAdvanced_<PT>Command` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | yes | custom | true | - | - | - | config-edit | tool | Generated advanced-level setters |
| `AutoMeshSetPreset_<PT>Command` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | yes | custom | true | - | - | - | config-edit | tool | Generated preset setters |
| `AutoMeshGetActiveCommand` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | no | custom | true | - | - | dialog-output | - | hidden | - |
| `AutoMeshSetActiveCommand` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | no | custom | true | - | - | - | config-edit | tool | Argument-driven |
| `AutoMeshApplyActiveCommand` | [automesh/config.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/automesh/config.d) | no | custom | true | - | - | - | apply | tool | Delegates to active processor apply command |
| `UnsetKeyFrameCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `SetKeyFrameCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `ResetKeyFrameCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `InvertKeyFrameCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `MirrorKeyFrameHorizontallyCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `MirrorKeyFrameVerticallyCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `FlipDeformCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `SymmetrizeDeformCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `SetFromHorizontalMirrorCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `SetFromVerticalMirrorCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `SetFromDiagonalMirrorCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `SetFrom1DMirrorCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `CopyBindingCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | - | tool | - |
| `PasteBindingCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | binding-edit | tool | - |
| `RemoveBindingCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | binding-edit | tool | - |
| `SetInterpolationCommand` | [binding/binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/binding/binding.d) | no | default | true | - | - | - | binding-edit | tool | - |
| `ApplyInspectorPropCommand<I, Prop>` | [inspector/apply_node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/inspector/apply_node.d) | yes | default | false | - | - | - | structural-edit | tool | Generated apply family; hidden from shortcut editor |
| `ToggleInspectorPropCommand<I, Prop>` | [inspector/apply_node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/inspector/apply_node.d) | yes | default | true | - | - | - | structural-edit | tool | Generated toggle family |
| `SelectToolModeCommand` | [mesheditor/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/mesheditor/tool.d) | no | custom | true | - | - | - | - | tool | - |
| `SetDeformBindingCommand` | [model/set_deform_binding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/model/set_deform_binding.d) | no | custom | false | - | - | - | binding-edit | tool | Deform-only; `bindingName` must be `deform` |
| `AddNodeCommand` (`AddNodeCommandT!(true)`) | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | true | `_suffix`: false, `className`: visible | `exposeClassName=true` | - | create | tool | Public alias form |
| `InsertNodeCommand` (`InsertNodeCommandT!(true)`) | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | true | `_suffix`: false, `className`: visible | `exposeClassName=true` | - | create | tool | Public alias form |
| `ConvertToCommand` (`ConvertToCommandT!(true)`) | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | true | `className`: visible | `exposeClassName=true` | - | structural-edit | tool | Public alias form |
| `SetNodeNameCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | false | - | - | - | rename | tool | Renames `context.nodes` using matching `newNames[]` |
| `AddNodeCommandT!(false)` dynamic instances | [node/dynamic.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/dynamic.d) | yes | default | true | `className` | `exposeClassName=false` | - | create | tool | Internal node-type-specific family |
| `InsertNodeCommandT!(false)` dynamic instances | [node/dynamic.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/dynamic.d) | yes | default | true | `className` | `exposeClassName=false` | - | create | tool | Internal node-type-specific family |
| `ConvertNodeToCommand!(false)` dynamic instances | [node/dynamic.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/dynamic.d) | yes | custom | true | `toType` | `expose=false` | - | structural-edit | tool | Internal convert-to-type family |
| `MoveNodeCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `DeleteNodeCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | true | - | - | - | delete | tool | - |
| `CutNodeCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | custom | true | - | - | - | delete | tool | - |
| `CopyNodeCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | custom | true | - | - | - | - | tool | - |
| `PasteNodeCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | custom | true | - | - | - | create | tool | - |
| `ReloadNodeCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `VertexModeCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | true | - | - | - | - | tool | - |
| `ToggleVisibilityCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `CentralizeNodeCommand` | [node/node.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/node.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `AddMaskCommand` | [node/mask.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/mask.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `RemoveMaskCommand` | [node/mask.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/mask.d) | no | default | true | - | - | - | delete | tool | - |
| `ChangeMaskModeCommand` | [node/mask.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/mask.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `AddWeldingCommand` | [node/welding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/welding.d) | no | default | true | - | - | - | create | tool | - |
| `RemoveWeldingCommand` | [node/welding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/welding.d) | no | default | true | - | - | - | delete | tool | - |
| `ChangeWeldingWeightCommand` | [node/welding.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/node/welding.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `AddAnimationKeyFrameCommand` | [parameter/animedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/animedit.d) | no | custom | true | - | - | - | keyframe-edit | tool | Requires Animation Edit mode and an active animation. |
| `MoveParameterCommand` | [parameter/group.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/group.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `CreateParamGroupCommand` | [parameter/group.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/group.d) | no | default | true | - | - | - | create | tool | - |
| `ChangeGroupColorCommand` | [parameter/group.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/group.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `DeleteParamGroupCommand` | [parameter/group.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/group.d) | no | default | true | - | - | - | delete | tool | - |
| `Add1DParameterCommand` | [parameter/param.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/param.d) | no | default | true | - | - | - | create | tool | - |
| `Add2DParameterCommand` | [parameter/param.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/param.d) | no | default | true | - | - | - | create | tool | - |
| `AddMouthParameterCommand` | [parameter/param.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/param.d) | no | default | true | - | - | - | create | tool | - |
| `RemoveParameterCommand` | [parameter/param.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/param.d) | no | default | true | - | - | - | delete | tool | - |
| `ConvertTo2DParamCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `FlipXCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `FlipYCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `Flip1DCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `MirrorHorizontallyCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `MirrorVerticallyCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `MirroredAutoFillDir1Command` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `MirroredAutoFillDir2Command` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `MirroredAutoFillDir3Command` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `MirroredAutoFillDir4Command` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `CopyParameterCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | - | tool | - |
| `PasteParameterCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | custom | true | - | - | - | create | tool | - |
| `PasteParameterWithFlipCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | custom | true | - | - | - | create | tool | - |
| `DuplicateParameterCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | create | tool | - |
| `DuplicateParameterWithFlipCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | create | tool | - |
| `DeleteParameterCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | delete | tool | - |
| `LinkToCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `ToggleParameterArmCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `SetParameterKeypointCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | false | - | - | - | - | tool | Moves the current parameter-edit keypoint from `context.parameterValue`; does not change bindings |
| `SetStartingKeyFrameCommand` | [parameter/paramedit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/paramedit.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `SetParameterNameCommand` | [parameter/prop.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/prop.d) | no | default | false | - | - | - | rename | tool | Requires typed name input |
| `ApplyParameterPropsAxesCommand` | [parameter/prop.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/parameter/prop.d) | no | default | false | - | - | - | structural-edit | tool | Requires structured numeric payload |
| `UndoCommand` | [puppet/edit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/edit.d) | no | custom | true | - | - | - | - | tool | - |
| `RedoCommand` | [puppet/edit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/edit.d) | no | custom | true | - | - | - | - | tool | - |
| `ShowSettingsWindowCommand` | [puppet/edit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/edit.d) | no | default | true | - | - | window | - | hidden | - |
| `ShowCommandBrowserWindowCommand` | [puppet/edit.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/edit.d) | no | default | true | - | - | window | - | hidden | - |
| `NewFileCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | confirm | project-reset | hidden | - |
| `ShowOpenFileDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog | - | hidden | - |
| `OpenFileCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | import | tool | Direct path variant |
| `ShowSaveFileDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog | - | hidden | - |
| `SaveFileCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | file-write | tool | Direct path variant |
| `ShowSaveFileAsDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog | - | hidden | - |
| `ShowImportPSDDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog | - | hidden | - |
| `ImportPSDCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | import | tool | Direct path variant |
| `ShowImportKRADialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog | - | hidden | - |
| `ImportKRACommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | import | tool | Direct path variant |
| `ShowImportINPDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog | - | hidden | - |
| `ImportINPCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | import | tool | Direct path variant |
| `ShowImportImageFolderDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog | - | hidden | - |
| `ImportImageFolderCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | import | tool | Direct path variant |
| `ShowMergePSDDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog+window | - | hidden | - |
| `MergePSDCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | structural-edit | tool | Direct path variant |
| `ShowMergeKRADialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog+window | - | hidden | - |
| `MergeKRACommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | structural-edit | tool | Direct path variant |
| `ShowMergeImageFileDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog | - | hidden | - |
| `MergeImageFilesCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | structural-edit | tool | Direct path variant |
| `ShowMergeINPDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | - | - | hidden | Placeholder GUI path |
| `MergeINPCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | structural-edit | tool | Direct path variant |
| `ShowExportToINPDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog | - | hidden | - |
| `ExportINPCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | file-write | tool | Direct path variant |
| `ShowExportToPNGDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog+window | - | hidden | - |
| `ExportPNGCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | file-write | tool | Direct path variant |
| `ShowExportToJpegDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog+window | - | hidden | - |
| `ExportJPEGCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | file-write | tool | Direct path variant |
| `ShowExportToTGADialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog+window | - | hidden | - |
| `ExportTGACommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | file-write | tool | Direct path variant |
| `ShowExportToVideoDialogCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | dialog+window | - | hidden | - |
| `ExportVideoCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | false | - | - | - | file-write | tool | Direct path variant |
| `CloseProjectCommand` | [puppet/file.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/file.d) | no | default | true | - | - | confirm | project-reset | hidden | - |
| `ShowImportSessionDataDialogCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | dialog | - | hidden | - |
| `ImportSessionDataCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | - | import | tool | Direct path variant |
| `PremultTextureCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | - | texture-regenerate | tool | - |
| `RebleedTextureCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | - | texture-regenerate | tool | - |
| `RegenerateMipmapsCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | - | texture-regenerate | tool | - |
| `GenerateFakeLayerNameCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | - | structural-edit | tool | - |
| `AttemptRepairPuppetCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | - | repair | tool | - |
| `RegenerateNodeIDsCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | - | repair | tool | - |
| `ModelEditModeCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | - | - | tool | - |
| `AnimEditModeCommand` | [puppet/tool.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/tool.d) | no | default | true | - | - | - | - | tool | - |
| `SetDefaultLayoutCommand` | [puppet/view.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/view.d) | no | default | true | - | - | layout | layout-reset | hidden | - |
| `ShowSaveScreenshotDialogCommand` | [puppet/view.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/view.d) | no | default | true | - | - | dialog | - | hidden | - |
| `SaveScreenshotCommand` | [puppet/view.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/view.d) | no | default | true | - | - | - | file-write | tool | Direct path variant |
| `CaptureLiveScreenshotCommand` | [puppet/view.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/view.d) | no | default | false | - | - | - | - | tool | Returns MCP image/png content |
| `ShowStatusForNerdsCommand` | [puppet/view.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/view.d) | no | default | true | - | - | - | - | tool | - |
| `ToggleDifferenceAggregationCommand` | [puppet/view.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/puppet/view.d) | no | default | true | - | - | - | - | tool | - |
| `DefineMeshCommand` | [vertex/define_mesh.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/vertex/define_mesh.d) | no | custom | false | - | - | - | structural-edit | tool | Requires programmatic mesh arrays |
| `TogglePanelVisibilityCommand` | [view/panel.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/view/panel.d) | no | custom | true | - | - | - | - | tool | Runtime panel instance required |
| `ToggleMirrorViewCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | - | - | tool | - |
| `ToggleOnionSliceCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | - | - | tool | - |
| `TogglePhysicsCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | - | - | tool | - |
| `TogglePostProcessCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | - | - | tool | - |
| `ResetPhysicsCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | - | - | tool | - |
| `ResetParametersCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | - | keyframe-edit | tool | - |
| `ListFlipPairsCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | false | - | - | - | - | tool | Returns registered flip pairs as JSON |
| `AddFlipPairCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | false | - | - | - | config-edit | tool | Registers a left/right node flip pair |
| `AutoAddFlipPairsCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | false | - | - | - | config-edit | tool | Registers flip pairs by node-name pattern |
| `RemoveFlipPairCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | false | - | - | - | config-edit | tool | Removes a registered left/right node flip pair |
| `OpenFlipPairWindowCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | window | - | hidden | - |
| `OpenAutomeshBatchingCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | modal | - | hidden | - |
| `ResetViewportZoomCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | - | - | tool | - |
| `ResetViewportPositionCommand` | [viewport/control.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/control.d) | no | default | true | - | - | - | - | tool | - |
| `ListCommandCommand` | [viewport/palette.d](/Users/seagetch/src/nijigenerate/nijigenerate/source/nijigenerate/commands/viewport/palette.d) | no | default | true | - | - | popup | - | hidden | - |

Notes:

- MCP exposure is now controlled by command-level UDA metadata and filtered in MCP tool registration.
- File import / merge / export commands now include direct path variants that stay hidden from the Shortcuts settings UI while remaining exposed to MCP.
- `TW.hidden` only affects the Command Browser.
- `shortcutRunnable()` only affects the Shortcuts settings UI.
