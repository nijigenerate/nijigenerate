# Depth Bone Rig 実装タスク

この文書は `doc/bone.md` を実装に落とすためのタスク一覧である。

方針:

- Boneは `ExDepthRigRoot` / `ExDepthBone` のノード階層として実装する。
- BindingはBone階層には持たせず、DepthRig側の対応表として持つ。
- UIはMask Sourceと同じように、対象GridDeformer/PathDeformerのInspectorからBone Sourceを指定できるようにする。
- 全vertices分のweight配列は保存しない。weightはdeformation生成時にオンデマンド計算する。
- 最終出力は既存の `DeformationParameterBinding.vertexOffsets` に書き込む。

## 進捗管理

更新日: 2026-05-20

この表は `doc/bone.md` を実装するための計画と進捗を管理する。状態は各タスクの完了条件を満たした時だけ更新する。ビルドが通っただけでは、save/load、MCP smoke、UI操作、undo/redo、表示確認を含むタスクを `検証済` にはしない。

ステータス:

```text
未着手: まだ作業していない
進行中: 実装中
実装済: 実装は入ったが検証が残っている
検証済: 完了条件を満たしている
保留: 設計判断または前提待ち
```

進捗ボード:

| ID | Phase | タスク | 状態 | 主な成果物 | 確認 |
| --- | --- | --- | --- | --- | --- |
| BONE-01 | 1 | ExDepthRigRootを追加する | 実装済 | `ExDepthRigRoot` | build/save/load |
| BONE-02 | 1 | ExDepthBoneを追加する | 実装済 | `ExDepthBone` | build/save/load |
| BONE-03 | 1 | DepthRigBindingを定義する | 実装済 | `DepthRigBinding`, `InfluenceRule` | weight配列なし |
| BONE-04 | 2 | Depth Bone用Actionを追加する | 実装済 | Action群 | undo/redo |
| BONE-05 | 2 | Depth Bone用ExCommandを追加する | 実装済 | `commands/depth/bone.d` | MCP呼び出し |
| BONE-06 | 3 | Add Standard Skeletonを実装する | 実装済 | 標準骨格生成 | 階層/save/load |
| BONE-07 | 4 | Rig runtimeを実装する | 実装済 | runtime pose/skin matrix | pose計算 |
| BONE-08 | 4 | Depth Sourceを実装する | 実装済 | `depthAt` | Grid/Path fallback |
| BONE-09 | 4 | weightのオンデマンド計算を実装する | 実装済 | influence計算 | weight非保存 |
| BONE-10 | 4 | 3D skinningから2D offsetsを生成する | 実装済 | offset生成 | Z破棄 |
| BONE-11 | 5 | ExDepthRigRoot Inspectorを追加する | 実装済 | Root UI | Action経由 |
| BONE-12 | 5 | ExDepthBone Inspectorを追加する | 実装済 | Bone UI | undo/redo |
| BONE-13 | 5 | GridDeformer/PathDeformer InspectorにDepth Bone Sourcesを追加する | 実装済 | Target UI | Binding同期 |
| BONE-14 | 6 | Layout modeのBone表示を追加する | 実装済 | rest表示 | handle位置 |
| BONE-15 | 6 | Deform modeのBone表示とPreviewを追加する | 進行中 | pose/preview表示 | Apply前非破壊 |
| BONE-16 | 7 | ModelEditSubMode.Deformへ統合する | 実装済 | Deform統合 | 通常Deform非競合 |
| BONE-17 | 8 | PathDeformerをtargetに許可する | 実装済 | Path対応 | fallback動作 |
| BONE-18 | 9 | serialization round-tripを確認する | 未着手 | round-trip確認 | UUID解決 |
| BONE-19 | 9 | MCP smoke testを作る | 未着手 | smoke手順 | 最小ゴール再現 |
| BONE-20 | 9 | build確認 | 検証済 | build結果 | `dub build -c osx-full` |
| BONE-21 | 10 | Bone Sourceごとのdepth offset/scaleを追加する | 実装済 | `sourceSettings` | build/UI操作 |
| BONE-22 | 10 | BoneごとのallowParentToTargetsを追加する | 実装済 | Bone option | build/UI操作 |
| BONE-23 | 11 | Depth Bone自動更新のDirtyキューを追加する | 実装済 | dirty queue API | 重複排除 |
| BONE-24 | 11 | Dirty flushを更新ループへ統合する | 実装済 | flush hook | 変更なし時no-op |
| BONE-25 | 11 | Depth Bone関連変更でdirtyを立てる | 実装済 | command/hook連携 | keypoint/all-keypoints切替 |
| BONE-26 | 11 | 自動更新のUndo/Redo動作を確認する | 未着手 | refresh action | undo/redo |
| BONE-27 | 11 | dirty scopeをKeypoint/AllKeypointsに分離する | 実装済 | scoped dirty queue | 全keypoint更新 |

チェックポイント:

| チェックポイント | 対象タスク | 状態 | 完了条件 |
| --- | --- | --- | --- |
| CP-1: ノード作成 | BONE-01, BONE-02, BONE-03, BONE-06 | 実装済 | Root/Bone作成、標準骨格生成、save/load |
| CP-2: MCP基礎 | BONE-04, BONE-05 | 実装済 | MCPから作成/一覧/Source指定ができる |
| CP-3: 変形計算 | BONE-07, BONE-08, BONE-09, BONE-10 | 実装済 | Applyで2D offsetsを書き込める |
| CP-4: UI/Viewport | BONE-11, BONE-12, BONE-13, BONE-14, BONE-15 | 進行中 | InspectorとViewportから操作できる |
| CP-5: Deform統合 | BONE-16, BONE-17 | 実装済 | Deform modeとPathDeformer対応 |
| CP-6: 検証 | BONE-18, BONE-19, BONE-20 | 進行中 | round-trip、MCP smoke、build完了 |
| CP-7: Source/Bone詳細設定 | BONE-21, BONE-22 | 実装済 | Source深度調整、親影響オプション |
| CP-8: 自動更新 | BONE-23, BONE-24, BONE-25, BONE-26, BONE-27 | 進行中 | Dirty登録、1回flush、scope別更新、Undo確認 |

運用:

- 作業開始時は進捗ボードの該当行を `進行中` にする。
- 実装を入れたら `実装済` にする。
- 完了条件を満たしたら `検証済` にする。
- 判断待ちや設計変更が必要な場合は `保留` にして、該当タスク本文へ理由を書く。
- タスク本文は詳細仕様、進捗ボードは状態管理として使う。

現在の実装メモ:

- `ExDepthRigRoot` / `ExDepthBone` / `DepthRigBinding` のコードは入っている。
- Depth Bone用コマンド、Action、Inspector、Viewport表示のコードは入っている。
- Rig runtimeはrest姿勢、local rest、現在pose、inverse bind、skin matrixを構築する形にした。
- GridDeformer/PathDeformer InspectorにはSource追加/削除、Influence Rule編集、Influence Preview、Deform Preview、Applyを追加した。
- `dub build -c osx-full` は通っている。
- Bone Sourceごとの `weight`, `depthOffset`, `depthScale` は保存され、Inspectorから編集できる。
- `depthOffset` / `depthScale` は `depth * depthScale + depthOffset` の順でSourceごとのZ評価に使う。
- `ExDepthBone.allowParentToTargets` は保存され、Inspectorから編集できる。
- `allowParentToTargets=false` のBoneは、ターゲット変形用runtimeで親姿勢を混ぜない。
- Depth Bone自動更新はDirtyキュー方式で実装済みである。
- `ngMarkDepthBoneDirty` は `DepthRigRoot + Parameter + scope` を基準に重複排除する。
- Dirty scopeは `Keypoint` と `AllKeypoints` に分かれる。
- Deform modeでBone姿勢を編集した場合だけ `Keypoint` とし、現在keypointのみ更新する。
- depths/depth-ops、target vertices、Depth Bone Source、Source設定、Influence Rule、Bone rest/constraint/allowParentToTargets、target/root transformの変更は `AllKeypoints` とし、Parameterの全keypointを更新する。
- dirty登録時にParameterが直接取れない場合は、DepthRig対象に既存 `deform` bindingを持つParameterを解決して `AllKeypoints` 更新する。
- `ngFlushDepthBoneDirty` はアプリ更新ループ終端で呼ばれ、dirtyが空なら即returnする。
- Dirty発火はDepth Bone Source追加/削除/並び替え、Source設定変更、Influence Rule変更、Bone rest/constraint変更、Bone transform編集hook、Depth Edit Apply、target transform変更、Grid/Path vertices定義変更に接続済みである。
- 自動更新のUndo/Redo挙動確認は未完了である。
- ただし、save/load round-trip、MCP smoke、実機UI操作、undo/redo、Deform mode preview確認は未完了である。

未完了として扱う点:

- Standard Skeleton生成、Binding変更、Apply DeformがAction経由でundo/redoできることを確認する。
- GridDeformer/PathDeformerの保存、読み込み、UUID解決をround-tripで確認する。
- MCPコマンドで最小フローを再現するsmoke testを作る。
- Depth Bone関連変更時だけdirtyを立て、変更がない更新では再計算しないことを確認する。
- 同じRoot/Parameter/keypointへの複数dirtyが1回のflushにまとまることを確認する。
- 同じRoot/ParameterにAllKeypoints dirtyがある場合、同じ組のKeypoint dirtyが吸収されることを確認する。
- Source設定やdepth Applyの後、現在keypoint以外のdeform bindingも更新されることを確認する。
- 自動更新で書き込まれるdeform bindingがUndo/Redoで戻ることを確認する。

## Phase 1: ノードと保存形式

### BONE-01: ExDepthRigRootを追加する

対象:

```text
source/nijigenerate/ext/nodes/
source/nijigenerate/ext/
```

実装:

- `ExDepthRigRoot` を追加する。
- `Node` 派生として扱い、描画出力は持たない。
- 子孫 `ExDepthBone` の収集APIを持たせる。
- `DepthRigBinding[] bindings` を持たせる。
- `displayOptions` / `templateOptions` の保存枠を用意する。

完了条件:

- プロジェクト内に `ExDepthRigRoot` を作成できる。
- save/load後も `ExDepthRigRoot` と `bindings` が復元される。
- `dub build -c osx-full` が通る。

### BONE-02: ExDepthBoneを追加する

対象:

```text
source/nijigenerate/ext/nodes/
```

実装:

- `ExDepthBone` を追加する。
- `Node` 派生として扱い、描画出力は持たない。
- `boneId`, `restHead`, `restTail`, `restRoll` を持たせる。
- 制約用に `constraintType`, `hingeAxis`, `lockRotation`, `lockTranslation`, `rotationLimits`, `maxStepRadians` の保存枠を用意する。

完了条件:

- `ExDepthRigRoot` または `ExDepthBone` の子として作成できる。
- save/load後もrest姿勢と制約情報が復元される。

### BONE-03: DepthRigBindingを定義する

対象:

```text
source/nijigenerate/ext/nodes/
```

実装:

- `DepthRigBinding` 構造体またはクラスを定義する。
- 保存する情報は以下に限定する。

```text
targetUuid
targetKind
sourceBoneUuids[]
influenceRule
options
```

- `InfluenceRule` を定義する。

```text
maxInfluences
radiusScale
minimumRadius
falloff
multipliersByBoneUuid
```

- 全vertices分のweight配列は持たせない。

完了条件:

- `ExDepthRigRoot.bindings` に対象とBone Sourceの対応を保存できる。
- JSON/serialize形式に `skinWeights` や `skinBoneUuids` が存在しない。

## Phase 2: コマンドとAction

### BONE-04: Depth Bone用Actionを追加する

対象:

```text
source/nijigenerate/actions/
```

実装:

- `DepthBoneSourceListChangeAction`
- `DepthBoneBindingRuleChangeAction`
- `DepthBoneRestChangeAction`
- `DepthBoneConstraintChangeAction`
- `DepthRigApplyDeformationAction`

方針:

- Bone Source追加/削除は `DepthRigBinding.sourceBoneUuids` の差分として管理する。
- influence rule変更は `DepthRigBinding.influenceRule` の差分として管理する。
- deformation書き込みは既存の `DeformationParameterBinding` 更新Actionに寄せられるなら寄せる。

完了条件:

- 主要操作がundo/redoできる。
- undo/redo後にsave/loadしても状態が壊れない。

### BONE-05: Depth Bone用ExCommandを追加する

対象:

```text
source/nijigenerate/commands/depth/bone.d
source/nijigenerate/commands/package.d
```

実装するコマンド:

```text
CreateDepthRigRoot(parent: Node, name: string)
AddDepthBone(parent: Node, boneId: string, restHead: float[], restTail: float[], restRoll: float)
AddStandardDepthSkeleton(root: Node, scale: float)
SetDepthBoneRest(bone: Node, restHead: float[], restTail: float[], restRoll: float)
SetDepthBoneConstraint(bone: Node, constraint: string)
ListDepthBones(root: Node)
AddDepthBoneSource(root: Node, target: Node, bone: Node)
RemoveDepthBoneSource(root: Node, target: Node, bone: Node)
ListDepthBoneSources(root: Node, target: Node)
SetDepthBoneInfluenceRule(root: Node, target: Node, rule: string)
GetDepthBoneInfluenceRule(root: Node, target: Node)
PreviewDepthBoneInfluence(root: Node, target: Node, bone: Node)
PreviewDepthBoneDeform(root: Node, targets: Node[])
ApplyDepthBoneDeform(root: Node, targets: Node[])
```

制約:

- `target` はGridDeformer/PathDeformerのみ許可する。
- 全vertices分のweight配列を受け取る/返す/保存するコマンドは作らない。
- Node引数は既存コマンドと同じくUUID解決できるようにする。

完了条件:

- MCPから標準骨格作成、Bone Source指定、rule変更、deformation適用まで呼べる。
- `ListDepthBones` / `ListDepthBoneSources` / `GetDepthBoneInfluenceRule` がJSONで状態確認できる。

## Phase 3: 標準骨格テンプレート

### BONE-06: Add Standard Skeletonを実装する

対象:

```text
source/nijigenerate/ext/nodes/
source/nijigenerate/commands/depth/bone.d
```

生成するBone:

```text
Pelvis
  Spine
    Chest
      Neck
        Head
      Clavicle.L
        UpperArm.L
          Forearm.L
            Hand.L
      Clavicle.R
        UpperArm.R
          Forearm.R
            Hand.R
  Thigh.L
    Shin.L
      Foot.L
  Thigh.R
    Shin.R
      Foot.R
```

実装:

- 選択中の `ExDepthRigRoot` の子として標準骨格を作成する。
- 既存Boneは上書きしない。
- `scale` を使ってRoot原点基準の標準比率で `restHead/restTail` を設定する。
- `.L` / `.R` の左右対称座標を設定する。

完了条件:

- コマンドとInspector UIの両方から標準骨格を追加できる。
- save/load後も階層とrest姿勢が維持される。

## Phase 4: Runtimeと計算処理

### BONE-07: Rig runtimeを実装する

対象:

```text
source/nijigenerate/viewport/model/
source/nijigenerate/ext/nodes/
```

実装:

- `ExDepthRigRoot` から子孫 `ExDepthBone` を収集する。
- Node親子関係からruntime bone hierarchyを構築する。
- rest head/tailからrest quaternionとinverse bind matrixを計算する。
- Deform modeの現在poseをBoneノードから読み取る。
- 親から順にworld head/tail/world quaternionを解決する。
- `skinMatrix = currentWorld * inverseBind` を計算する。

参照:

```text
~/src/github/draw-depth/src/puppet/rigState.js
```

完了条件:

- 標準骨格の各Boneについてworld poseとskin matrixが計算できる。
- 既存Node transformと矛盾しない。

### BONE-08: Depth Sourceを実装する

対象:

```text
source/nijigenerate/ext/nodes/exgriddeformer.d
source/nijigenerate/viewport/model/
```

実装:

- `ExGridDeformer.depths[index]` からZを取得する。
- `depths` がnullまたは長さ不一致の場合のfallbackを定義する。
- PathDeformerは初期実装では `depthAt = 0` でよい。

完了条件:

- GridDeformerの各vertexから `rest3D = vec3(x, y, z)` を作れる。
- PathDeformerに対しても落ちずにfallbackできる。

### BONE-09: weightのオンデマンド計算を実装する

対象:

```text
source/nijigenerate/viewport/model/
```

実装:

- 対象pointごとに、`DepthRigBinding.sourceBoneUuids` のBone segmentとの距離からscoreを計算する。
- `influenceRule.maxInfluences` までを選ぶ。
- weightを正規化する。
- 計算結果はその場のdeformation生成にだけ使い、永続化しない。

参照:

```text
~/src/github/draw-depth/src/puppet/bindWeights.js
```

完了条件:

- Body::Gの全vertexについてweightを計算できる。
- save/load結果にweight配列が増えない。

### BONE-10: 3D skinningから2D offsetsを生成する

対象:

```text
source/nijigenerate/viewport/model/
source/nijigenerate/commands/depth/bone.d
```

実装:

- `deformed3D = Σ (bone.skinMatrix * restPoint3D) * weight` を計算する。
- Zは出力時に単純に捨てる。
- `offset2D = deformed3D.xy - rest3D.xy` を生成する。
- 既存の `DeformationParameterBinding.vertexOffsets` に書き込む。

参照:

```text
~/src/github/draw-depth/src/puppet/deform.js
```

完了条件:

- `ApplyDepthBoneDeform` で現在armed parameter/current keyにoffsetを書き込める。
- カメラ投影や透視投影を使っていない。

## Phase 5: Inspector UI

### BONE-11: ExDepthRigRoot Inspectorを追加する

対象:

```text
source/nijigenerate/panels/inspector/
```

UI:

```text
Bones
  Add Bone
  Delete Bone
  Edit Rest Head/Tail
  Set Parent
  Mirror
  Add Standard Skeleton
```

完了条件:

- Root選択時にBone操作ができる。
- `Add Standard Skeleton` がAction経由で実行される。

### BONE-12: ExDepthBone Inspectorを追加する

対象:

```text
source/nijigenerate/panels/inspector/
```

UI:

```text
Rest
  Head
  Tail
  Roll

Constraints
  Lock Rotation
  Lock Translation
  Hinge Axis
  Rotation Limits
```

完了条件:

- rest姿勢と制約が編集できる。
- 変更がundo/redoできる。

### BONE-13: GridDeformer/PathDeformer InspectorにDepth Bone Sourcesを追加する

対象:

```text
source/nijigenerate/panels/inspector/
```

UI:

```text
Depth Bone Sources
  Add Bone Source
  Remove Bone Source
  Source List
  Influence Rule
  Show Influence Preview
```

方針:

- Mask Sourceと同じ操作感にする。
- UIは対象ノードから編集するが、更新するのは `ExDepthRigRoot.bindings` の対応表である。

完了条件:

- Body::Gを選択してPelvis/Spine/ChestをBone Sourceに追加できる。
- Source Listが `DepthRigBinding.sourceBoneUuids` と同期する。

## Phase 6: Viewport表示

### BONE-14: Layout modeのBone表示を追加する

対象:

```text
source/nijigenerate/viewport/model/
```

表示:

- Bone rest line
- Bone head/tail handle
- 選択Boneの強調表示
- Influence preview

完了条件:

- Root/Bone選択時に標準骨格が視認できる。
- head/tail handleが対象Boneのrest姿勢と一致する。

### BONE-15: Deform modeのBone表示とPreviewを追加する

対象:

```text
source/nijigenerate/viewport/model/
```

表示:

- Bone current pose line
- 選択Boneの強調表示
- 生成後の対象deformation preview

完了条件:

- Bone poseを変えるとGridDeformer previewが更新される。
- Apply前に既存deformationを破壊しない。

## Phase 7: Deform mode統合

### BONE-16: ModelEditSubMode.Deformへ統合する

対象:

```text
source/nijigenerate/viewport/model/
source/nijigenerate/project.d
```

実装:

- 独立した `Depth Bone Deform` モードは作らない。
- 既存のDeform mode内で `ExDepthBone` のpose編集を扱う。
- armed parameter/current keyがない場合のUI/エラーを定義する。

完了条件:

- Deform modeでChest Boneを回転し、ApplyでBody::Gにoffsetが入る。
- 通常のDeform操作と競合しない。

## Phase 8: PathDeformer対応

### BONE-17: PathDeformerをtargetに許可する

対象:

```text
source/nijigenerate/viewport/model/
source/nijigenerate/panels/inspector/
source/nijigenerate/commands/depth/bone.d
```

実装:

- Bone Source UIをPathDeformerにも表示する。
- PathDeformerのrest pointsを取得する。
- 初期実装では `depthAt = 0` とする。
- ApplyでPathDeformerの `DeformationParameterBinding.vertexOffsets` に書き込む。

完了条件:

- PathDeformerをBone Source対象として登録できる。
- Applyしても落ちない。

## Phase 9: 検証

### BONE-18: serialization round-tripを確認する

確認:

- `ExDepthRigRoot`
- `ExDepthBone`
- `DepthRigBinding`
- `InfluenceRule`
- 標準骨格階層

完了条件:

- save/load後にUUID参照が解決できる。
- 全vertices分のweight配列が保存されていない。

### BONE-19: MCP smoke testを作る

確認手順:

```text
1. CreateDepthRigRoot
2. AddStandardDepthSkeleton
3. ListDepthBones
4. AddDepthBoneSource
5. SetDepthBoneInfluenceRule
6. PreviewDepthBoneDeform
7. ApplyDepthBoneDeform
```

完了条件:

- MCP経由で最小実装ゴールを再現できる。
- `ListDepthBoneSources` と `GetDepthBoneInfluenceRule` で状態確認できる。

### BONE-20: build確認

実行:

```text
dub build -c osx-full
```

完了条件:

- buildが通る。
- DepthEdit既存機能が壊れていない。

## Phase 10: Source/Bone詳細設定

### BONE-21: Bone Sourceごとのdepth offset/scaleを追加する

実装:

- Bone Sourceごとに `weight`, `depthOffset`, `depthScale` を保存する。
- `depthOffset` / `depthScale` は `depth * depthScale + depthOffset` の順でrest Z評価に使う。
- UIはSliderではなくDragFloatで編集する。

完了条件:

- save/load後にSource設定が残る。
- 設定変更後、該当Parameterの全keypointのdeform bindingが更新される。

### BONE-22: BoneごとのallowParentToTargetsを追加する

実装:

- `ExDepthBone.allowParentToTargets` を保存する。
- Inspectorから切り替えられるようにする。
- `false` のBoneは、親姿勢で自身が動いて見える場合でも、ターゲット変形には自身の明示的poseだけを使う。

完了条件:

- Headで `false` にしたとき、親Boneの回転だけではHead対象が変形しない。
- 設定変更後、該当Parameterの全keypointのdeform bindingが更新される。

## Phase 11: 自動更新

### BONE-23: Depth Bone自動更新のDirtyキューを追加する

実装:

- Depth Bone関連変更はその場で直接再計算せず、dirty requestとしてキューへ積む。
- dirty requestは `DepthRigRoot`, `Parameter`, `keypoint`, `scope`, `reason` を持つ。
- 同じRoot/Parameter/keypointの重複をまとめる。

完了条件:

- 変更がないフレームでは再計算しない。
- 同一フレーム内の同種変更は1回にまとまる。

### BONE-24: Dirty flushを更新ループへ統合する

実装:

- アプリ更新ループ終端で `ngFlushDepthBoneDirty` を呼ぶ。
- dirtyが空なら即returnする。

完了条件:

- UI操作後に1回だけ自動更新される。
- dirtyがない通常フレームでDepth Bone再計算が走らない。

### BONE-25: Depth Bone関連変更でdirtyを立てる

実装:

- Bone姿勢変更は `Keypoint` dirtyにする。
- depths/depth-ops Apply、target vertices、Depth Bone Source、Source設定、Influence Rule、Bone rest/constraint/allowParentToTargets、target/root transform変更は `AllKeypoints` dirtyにする。

完了条件:

- Bone姿勢変更は現在keypointだけを書き換える。
- Source設定やdepth変更は全keypointを書き換える。

### BONE-26: 自動更新のUndo/Redo動作を確認する

確認:

- 自動更新で作られた `DeformationParameterBinding` の変更がundo/redoできる。
- Source設定変更と、それに伴う全keypoint更新のundo/redoが破綻しない。

完了条件:

- undoで変更前のbinding値に戻る。
- redoで自動更新後のbinding値に戻る。

### BONE-27: dirty scopeをKeypoint/AllKeypointsに分離する

実装:

- dirty scopeとして `Keypoint` と `AllKeypoints` を定義する。
- `AllKeypoints` dirtyが同じRoot/Parameterに存在する場合、同じ組の `Keypoint` dirtyを吸収する。
- `AllKeypoints` flushでは `Parameter.axisPoints` の全組み合わせを列挙して、各keypointのoffsetを更新する。
- Viewport表示用の `deformable.deformation` は現在keypointのoffsetを使う。

完了条件:

- Source offset/scale変更直後、Boneを再操作しなくても現在表示が更新される。
- その後、別keypointに移動しても古いSource設定のbindingが残らない。
- dirty登録時に `param=(none)` でも、既存Depth Bone対象Parameterが解決され、`writeBinding=true` で更新される。

## 実装順まとめ

1. `ExDepthRigRoot` / `ExDepthBone`
2. `DepthRigBinding` / `InfluenceRule`
3. serialization
4. Action
5. ExCommand / MCP
6. 標準骨格テンプレート
7. Rig runtime
8. Depth Source
9. オンデマンドweight計算
10. 3D skinningから2D offset生成
11. Inspector UI
12. Viewport表示
13. Deform mode統合
14. PathDeformer対応
15. 検証

## 最初の実装チェックポイント

最初のPRまたはcommitでは、以下までを目標にする。

```text
1. ExDepthRigRoot / ExDepthBoneを作成できる
2. AddStandardDepthSkeletonで標準骨格が作られる
3. save/loadで階層とrest姿勢が戻る
4. ListDepthBonesでBone一覧を取得できる
5. dub build -c osx-full が通る
```

この段階ではdeformation生成、Inspector、Viewport overlayはまだ不要。
