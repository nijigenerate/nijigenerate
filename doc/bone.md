# Depth Bone Rig 実装計画

この文書は、DepthEditで作成した深度情報とボーンベース変形を組み合わせ、複数ノードにまたがるGridDeformer/PathDeformerのdeformationを生成するための実装計画を整理する。

## 基本方針

Boneは階層構造を持つノードとして実装する。

BindingはBone階層には持たせない。Bone親子関係とは別の、対象ノードとBoneの対応表として管理する。

全vertices分のweightを個別に永続管理しない。保存するのは「どの対象がどのBoneを参照するか」と、weight計算に必要な少数のルール/パラメータだけである。各pointのweightはdeformation生成時に計算する。

Mask Sourceと同じにするのはUI上の指定方法であり、内部データ構造を対象ノード側に置くという意味ではない。

```text
姿勢・編集・階層 = ExDepthBone のノード階層
対象・Bone対応付け = DepthRigBinding の対応表
weight = DepthRigBinding のルールから必要時に計算
```

理由:

- 各Boneは3D空間で角度変化し、Deform modeでパラメータキー編集される必要がある。
- 既存のNode transform、Inspector、selection、undo/redo、parameter bindingの仕組みに乗せる方が自然。
- 1つのGridDeformer/PathDeformerは `pelvis`, `spine`, `chest` など複数Boneから重みを受けるため、BindingはBone親子階層では表現できない。
- Bone階層は姿勢計算のための構造であり、Bindingは `対象 -> bone refs + weight rule` の対応表である。
- どの対象がどのBoneに従うかは、Mask Sourceと同様のUIで対象ノードから指定できる必要がある。
- UI上は対象ノードから指定しても、保存形式はDepthRig用の対応表として独立させる。
- vertices数に比例するweight配列を保存すると、mesh変更に弱く、データ量も無駄に増える。weightはrest pointとBone segmentから決定できるため、永続化しない。

## ノード構成

追加するノード:

```text
ExDepthRigRoot : Node
ExDepthBone    : Node
```

典型的なツリー:

```text
BodyDepthRig : ExDepthRigRoot
  Pelvis : ExDepthBone
    Spine : ExDepthBone
      Chest : ExDepthBone
        Neck : ExDepthBone
          Head : ExDepthBone
        Clavicle.L : ExDepthBone
          UpperArm.L : ExDepthBone
            Forearm.L : ExDepthBone
              Hand.L : ExDepthBone
        Clavicle.R : ExDepthBone
          UpperArm.R : ExDepthBone
            Forearm.R : ExDepthBone
              Hand.R : ExDepthBone
    Thigh.L : ExDepthBone
      Shin.L : ExDepthBone
        Foot.L : ExDepthBone
    Thigh.R : ExDepthBone
      Shin.R : ExDepthBone
        Foot.R : ExDepthBone
```

`ExDepthRigRoot` と `ExDepthBone` は編集支援用ノードであり、それ自体は描画出力を持たない。最終的な変形結果は対象ノードの `DeformationParameterBinding` に焼き込む。

## ExDepthRigRoot

役割:

- 子孫の `ExDepthBone` を収集する。
- Bone階層のグループ、表示、一括操作、テンプレート作成の入口になる。
- Deform modeでBone姿勢を解決するためのルートになる。
- 変形対象ごとのBone対応付けをフラットな対応表として保持する。

保存すべき情報:

```text
rootId
displayOptions
templateOptions
bindings[]
  - targetUuid
  - targetKind
  - sourceBoneUuids[]
  - influenceRule
  - options
```

Bone階層はNodeのchildrenとして保存する。BindingはBone階層ではなくRootの対応表として保存する。

### 標準骨格テンプレート

`ExDepthRigRoot` には、標準的な骨格ボーン群を一括追加する機能を持たせる。

これは編集開始時の初期配置を作るためのUI機能であり、生成後は通常の `ExDepthBone` 階層として扱う。テンプレート専用の別データ構造は持たない。

標準テンプレートで作成するBone:

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

生成時に行うこと:

1. 選択中の `ExDepthRigRoot` の子として上記Bone階層を作成する。
2. 各Boneに `boneId`、`restHead`、`restTail`、`restRoll` を設定する。
3. 左右Boneには `.L` / `.R` の命名と左右対称の初期座標を与える。
4. 既存Boneがある場合は、上書きではなく追加する。必要ならユーザーが削除する。

初期座標は対象モデルから自動推定しない。まずはRoot原点を基準にした標準比率で配置し、ユーザーがLayout modeで調整する。

## ExDepthBone

役割:

- ボーン1本を表す。
- Nodeツリー上の親子関係でBone階層を表す。
- rest姿勢とpose姿勢を持つ。
- Deform modeで3D回転・移動の操作対象になる。

保存すべき情報:

```text
boneId
restHead
restTail
restRoll
constraintType
hingeAxis
lockRotation
lockTranslation
rotationLimits
maxStepRadians
```

Nodeのtransformはpose編集に利用する。ただし、rest head/tail/lengthは明示的に持つ。Node transformだけに押し込めない。

必要なruntime状態:

```text
restHead
restTail
restLength
restQuaternion
localRestQuaternion
localRestOffset
poseEuler
poseTranslation
worldHead
worldTail
worldQuaternion
bindMatrix
inverseBindMatrix
skinMatrix
```

## Deformable Target

変形対象は既存の `Deformable` を基本にする。ただし対象はGridDeformerとPathDeformerに限定する。

`GridDeformer`, `PathDeformer` はいずれも `Deformable` として扱える。したがって、変形結果の書き込み先を抽象化するために独自Adapterを作る必要は基本的にない。

DepthRigが必要とする共通情報は既存の `Deformable` から得られる。

```text
pointCount = deformable.vertices.length
rest XY    = deformable.vertices[i]
output     = DeformationParameterBinding.vertexOffsets[i]
```

DepthRig側で追加で必要なのは、`Deformable` の各頂点に対応するZ値を得る仕組みだけである。

```text
rest 3D = vec3(deformable.vertices[i].x, deformable.vertices[i].y, depthAt(deformable, i))
```

### Depth Source

Z値の取得は対象タイプごとに異なるため、ここだけを小さく分離する。

```text
depthAt(Deformable target, size_t index) -> float
```

候補:

- `ExGridDeformer`: `depths[index]`
- `PathDeformer`: 0、または近傍Grid/DepthRig設定から推定

初期実装では `ExGridDeformer.depths` のみ対応すればよい。PathDeformerは `depthAt = 0` のfallbackで開始できる。

### DepthRig Binding

Boneとの対応付けは、内部的には `DepthRigBinding` として扱う。

`DepthRigBinding` は対象ノードと、その対象が参照する `ExDepthBone` のリスト、およびweight計算ルールを持つ。

保存すべき情報:

```text
targetUuid
targetKind
sourceBoneUuids[]
influenceRule
  - maxInfluences
  - radiusScale
  - minimumRadius
  - falloff
  - multipliersByBoneUuid
options
```

UI上の操作場所と保存先は分ける。UIでは対象ノードのInspectorから指定するが、保存はDepthRigの対応表として行う。

### Binding出力

出力は既存のDeform flowに乗せる。

```text
DeformationParameterBinding deform =
    parameter.getOrAddBinding(deformable, "deform")

deform.update(currentKeyPoint, generatedOffsets)
```

つまり、DepthRigは独自の変形binding形式を作らず、最終出力は通常の `DeformationParameterBinding.vertexOffsets` とする。

## Rig Runtime

`draw-depth` の `rigState.js` の考え方を移植する。

必要処理:

1. `ExDepthRigRoot` から子孫 `ExDepthBone` を収集する。
2. Node親子関係からruntime bone hierarchyを構築する。
3. rest head/tailからrest quaternionとinverse bind matrixを計算する。
4. Deform modeの現在poseをBoneノードから読み取る。
5. 親から順にworld head/tail/world quaternionを解決する。
6. `skinMatrix = currentWorld * inverseBind` を計算する。

移植元:

```text
~/src/github/draw-depth/src/puppet/rigState.js
createRigState
solveRigPose
```

## Weight計算

対象pointごとに、Bone線分との距離からweightを計算する。

基本式:

```text
distanceSq = distance from restPoint3D to bone rest segment
sigma = max(bone.restLength * radiusScale, minimumRadius)
score = exp(-distanceSq / (sigma * sigma)) * multiplier
```

計算手順:

1. 対象ノードのrest pointsを取得する。
2. 対応する `DepthRigBinding.sourceBoneUuids` からBone一覧を取得する。
3. 各pointについて全Boneのscoreを計算する。
4. `influenceRule.maxInfluences` に従って上位Boneを選ぶ。
5. weightを正規化する。
6. その場でスキニングに使う。

各pointのweightは保存しない。meshやdepthsが変わっても、同じBone Sourceとruleから再計算する。

移植元:

```text
~/src/github/draw-depth/src/puppet/bindWeights.js
distanceToSegmentSquared
createMeshBindingWithPolicy
```

`draw-depth` の `layerBinding.js` にあるレイヤー名分類は、初期実装では不要。nijigenerateではユーザーが対象ノードとBoneを明示し、必要なら後から自動分類を追加する。

## Deformation生成

Deform modeでBone poseを編集し、その結果を対象ノードへ焼き込む。

処理:

1. Armed parameterとcurrent keypointを取得する。
2. 対象GridDeformer/PathDeformerに対応する `DepthRigBinding` を取得する。
3. 参照BoneからRootをたどり、Rig runtimeを構築する。
4. 現在のBoneノードposeから `skinMatrix` を計算する。
5. 対象の各pointについてweightを計算し、その場でスキニングする。
6. rest XYとの差分をoffsetにする。
7. 対象の `DeformationParameterBinding` にoffsetを書き込む。

スキニング:

```text
deformed3D = Σ (bone.skinMatrix * restPoint3D) * weight
offset2D = deformed3D.xy - restPoint3D.xy
```

### 3Dから2Dへの変換

スキニングは3D空間で行うが、最終的な出力は既存の2D deformationである。

3D変形後の点を2Dへ落とすとき、カメラ投影や透視投影は行わない。追加の投影モデルも持たない。

単純にZ成分を捨て、XY成分だけを使う。

```text
rest3D     = vec3(x, y, z)
deformed3D = skinning(rest3D)

result2D = vec2(deformed3D.x, deformed3D.y)
offset2D = result2D - vec2(rest3D.x, rest3D.y)
```

Zはボーン変形計算に参加するための奥行きであり、出力時には保存しない。既存の `DeformationParameterBinding.vertexOffsets` には `offset2D` のみを書き込む。

移植元:

```text
~/src/github/draw-depth/src/puppet/deform.js
applySkinningToGeometry
```

## UI

既存フローに合わせて、LayoutとDeformに分ける。

### Layout mode

`ExDepthRigRoot` Inspector:

```text
Bones
  Add Bone
  Delete Bone
  Edit Rest Head/Tail
  Set Parent
  Mirror
  Add Standard Skeleton
```

`Add Standard Skeleton` は、選択中の `ExDepthRigRoot` に標準骨格テンプレートを一括追加する。

`GridDeformer` / `PathDeformer` Inspector:

```text
Depth Bone Sources
  Add Bone Source
  Remove Bone Source
  Source List
  Influence Rule
  Show Influence Preview
```

これはMask Sourceと同じ指定方法にする。ユーザーは対象ノードを選択し、その対象が参照するBoneをリストに追加する。

このUIは `DepthRigBinding.sourceBoneUuids` を編集する入口である。対象ノード自身の保存形式をこのUIから決めない。

`ExDepthBone` Inspector:

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

### Deform mode

Parameter armed時:

- `ExDepthBone` を選択すると、通常Nodeと同じようにrotation/translationを編集できる。
- GridDeformer/PathDeformerは対応する `DepthRigBinding` のBone Sourceとruleからweightを計算し、deformationを生成する。
- Preview表示とApply/update current keyを行う。

UI上は独立した `Depth Bone Deform` モードを作らない。既存の `ModelEditSubMode.Deform` に乗せる。

## Viewport表示

Layout mode:

- Bone rest lineを表示する。
- Bone head/tail handleを表示する。
- Influence previewを表示する。

Deform mode:

- Bone current pose lineを表示する。
- 選択Boneを強調表示する。
- Preview時に生成後の対象deformationを表示する。

## Action/Undo

必要なAction:

```text
DepthBoneSourceListChangeAction
DepthBoneBindingRuleChangeAction
DepthBoneRestChangeAction
DepthBoneConstraintChangeAction
DepthRigApplyDeformationAction
```

Deform modeでは、既存の `DeformationParameterBinding` 更新のActionにできるだけ乗せる。

## Command/MCP

他の機能と同様に、Depth Bone Rig操作は `ExCommand` として登録し、MCPから呼べるようにする。

配置案:

```text
source/nijigenerate/commands/depth/bone.d
enum DepthBoneCommand
```

MCP化するコマンドは、UI操作と同じ粒度にする。内部実装の都合をMCP APIへ漏らさない。

### Rig/Bone作成

```text
CreateDepthRigRoot
  parent: Node
  name: string
```

選択した親Nodeの下に `ExDepthRigRoot` を作成する。

```text
AddDepthBone
  parent: Node
  boneId: string
  restHead: float[]  // [x, y, z]
  restTail: float[]  // [x, y, z]
  restRoll: float
```

`parent` は `ExDepthRigRoot` または `ExDepthBone` とする。指定した親の子として `ExDepthBone` を追加する。

```text
AddStandardDepthSkeleton
  root: Node
  scale: float
```

選択した `ExDepthRigRoot` に標準骨格ボーン群を一括追加する。これはUIの `Add Standard Skeleton` と同じ操作である。

### Bone編集

```text
SetDepthBoneRest
  bone: Node
  restHead: float[]  // [x, y, z]
  restTail: float[]  // [x, y, z]
  restRoll: float
```

Boneのrest姿勢を更新する。

```text
SetDepthBoneConstraint
  bone: Node
  constraint: string // JSON object
```

Boneの制約を更新する。JSONには `lockRotation`, `lockTranslation`, `hingeAxis`, `rotationLimits` などを含める。

```text
ListDepthBones
  root: Node
```

Root配下の `ExDepthBone` 一覧をJSONで返す。

### Bone Source指定

```text
AddDepthBoneSource
  root: Node
  target: Node
  bone: Node
```

`target` のDepth Bone Sourcesに `bone` を追加する。UI上は対象ノードInspectorでBone Sourceを追加する操作に対応する。

```text
RemoveDepthBoneSource
  root: Node
  target: Node
  bone: Node
```

`target` のDepth Bone Sourcesから `bone` を削除する。

```text
ListDepthBoneSources
  root: Node
  target: Node
```

`target` に対応する `DepthRigBinding.sourceBoneUuids` をJSONで返す。

### Influence Rule

```text
SetDepthBoneInfluenceRule
  root: Node
  target: Node
  rule: string // JSON object
```

`maxInfluences`, `radiusScale`, `minimumRadius`, `falloff`, `multipliersByBoneUuid` を更新する。

```text
GetDepthBoneInfluenceRule
  root: Node
  target: Node
```

現在のruleをJSONで返す。

```text
PreviewDepthBoneInfluence
  root: Node
  target: Node
  bone: Node
```

対象Boneの影響範囲をViewport overlay用に表示する。MCPの返り値は表示状態または要約JSONに留め、全vertices分のweight配列は返さない。

### Deform適用

```text
ApplyDepthBoneDeform
  root: Node
  targets: Node[]
```

現在armed parameter/current keyに対し、指定targetのdeformation offsetsを生成して書き込む。weightは `DepthRigBinding.sourceBoneUuids` とruleからその場で計算する。

```text
PreviewDepthBoneDeform
  root: Node
  targets: Node[]
```

書き込みを行わず、現在のBone poseから生成されるdeformationをViewport previewに反映する。

### Command設計上の注意

- コマンド引数ではNode参照をUUIDで渡せるようにする。
- `target` はGridDeformer/PathDeformerに限定する。
- 全vertices分のweight配列を受け取る/返す/保存するコマンドは作らない。
- UIのMask Source風指定と同じ操作は `AddDepthBoneSource` / `RemoveDepthBoneSource` に対応させる。
- 最終的な変形書き込みは既存の `DeformationParameterBinding` 更新Actionに寄せる。

## Serialization

追加ノード:

```text
ExDepthRigRoot
ExDepthBone
```

`ExDepthRigRoot`:

```json
{
  "type": "DepthRigRoot",
  "displayOptions": {},
  "bindings": [
    {
      "target": 123,
      "targetKind": "grid",
      "sourceBoneUuids": [456, 789],
      "influenceRule": {
        "maxInfluences": 4,
        "radiusScale": 1.0,
        "minimumRadius": 1.0,
        "falloff": "gaussian",
        "multipliersByBoneUuid": {}
      }
    }
  ]
}
```

`ExDepthBone`:

```json
{
  "type": "DepthBone",
  "boneId": "chest",
  "restHead": [0, 100, 0],
  "restTail": [0, 160, 0],
  "restRoll": 0,
  "constraintType": null,
  "hingeAxis": null,
  "lockRotation": false,
  "lockTranslation": false
}
```

Bone階層はNodeのchildrenで保存する。BindingはRootの対応表に対象uuid、Bone uuid参照、weight計算ruleだけを保存する。

## Export/最終形態

`ExDepthRigRoot` と `ExDepthBone` は編集支援用ノードであり、最終的な描画には不要。

方針:

- nijigenerateプロジェクト内では保存する。
- deformationを焼いた後も再編集用に残せる。
- 必要ならexport時に除外する。
- 不要ならユーザー操作でBake後に削除できる。

## 実装順

1. `ExDepthRigRoot` / `ExDepthBone` の型追加と登録
2. serialize/deserialize
3. 標準骨格テンプレートの一括追加
4. `DepthRigBinding` 対応表
5. GridDeformer InspectorのBone Source指定UI
6. Rig runtime構築
7. `DepthRigBinding.sourceBoneUuids` とruleからweightをオンデマンド計算
8. Deform modeでGridDeformerへのdeformation生成
9. Viewport overlay
10. PathDeformer InspectorのBone Source指定UI
11. 制約/IK/Auto Fit

## 最小実装ゴール

まずは以下が動けばよい。

```text
1. ExDepthRigRootを作成
2. Add Standard Skeletonで標準骨格ボーン群を追加
3. Body::GのInspectorでPelvis/Spine/ChestをDepth Bone Sourcesに追加
4. Body::Gのdepthsから3D rest pointsを作る
5. Deform modeでChest Boneを回転
6. Body::Gの各pointについてweightをその場で計算する
7. Body::Gの現在parameter keyにdeformation offsetsが入る
```

この段階ではPathDeformer、自動人体推定、IK、mirrorは不要。

## 注意点

- BindingはBone階層に持たせない。
- 全vertices分のweight配列を保存しない。
- Boneは内部配列だけにしない。3D角度編集と既存Deform flowに乗せるためNodeにする。
- Mask Sourceと同じにするのはUI上の指定方法である。
- UI上は対象ノードからBone Sourceを指定するが、Bindingの保存形式はDepthRigの対応表として扱う。
- DepthEditはdepths/depth-ops編集の責務に留める。
- Bone deformationはModelEditSubMode.Deformの中で動作させる。
