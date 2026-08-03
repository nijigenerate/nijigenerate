# PSD Depth BoneSource Rotation 設計

## 目的

`BoneSource.rotation` は、DepthBone原点から表示メッシュへ伸びる接線の角度から、Yaw回転に使うdepth半径をBoneSourceごとに設定する。

この値は表示メッシュの静止位置を直接変更しない。接線を基準にしたYaw、すなわちコード上の `transform.r.y` の回転差分だけを変更する。

必須条件は次の通りである。

```text
transform.r.y = 0
    => BoneSource.rotation の値にかかわらず表示変形は同一

transform.r.y != 0 かつ depth != 0
    => BoneSource.rotation によりYaw回転半径Dが変わり、回り方も変わる

同じSource内の任意の頂点i,j
    => BoneSource.rotationを変えても、変形後の相対ベクトルと凹凸形状は通常Yawと同一
```

この条件は `transform.r.x/r.z` が非0の場合にも維持する。

nijigenerateのDepthBoneでは `depthEditRotation(pitch, yaw, roll)` が `transform.r.x`, `transform.r.y`, `transform.r.z` を受ける。したがってYawは `transform.r.y` である。Face::G、FrontHair::G、BackHair::Gが参照するHead Boneも、Face::Yaw-Pitchから `transform.r.y` と `transform.r.x` を受け、`transform.r.z` bindingは持たない。

## 座標系と単位

対象ローカル空間は次の通りである。

```text
X: 表示プレーン上の横方向
Y: 表示プレーン上の縦方向
Z: rotation = 0 の深度方向
```

`BoneSource.rotation = R` は対象ローカル `+Y` 軸回りの角度である。Bone原点に最も近い面サンプルまでの垂直Z距離を `d0` とし、Source全体のX方向回転中心補正 `deltaX` と接線距離 `D` を次で求める。

```text
deltaX = d0 * tan(R)
D = d0 / cos(R)
atan2(deltaX, d0) = R
sqrt(deltaX^2 + d0^2) = |D|
```

- データ、command、保存値はradian。
- Inspectorはdegree表示・編集。
- Nodeの `transform.r.*` と同じ単位境界を使う。
- 保存値は `[-pi, pi)` へ正規化する。
- 既定値と旧データの補完値は0。

## 表示静止点とSource共通の仮想回転点

深度へscaleとoffsetを適用する。

```text
d = rawDepth * depthScale + depthOffset
```

表示メッシュの各静止点 `p_i` はBoneSource.rotationで動かさない。

```text
p_i = (x_i, y_i, d_i)
```

Bone原点のXYに最も近い面サンプルから `d0` を一度だけ求める。`transform.r.y` のYaw回転差分を計算する仮想回転点 `q_i` は、全頂点へ同じ `deltaX` を加えて作る。

```text
q_i = p_i + (deltaX, 0, 0)
```

`q_i` は表示位置ではない。図で示された角度付き接線の長さ `D` を、Source全体の共通回転中心として使うための代理点である。

各頂点の `d_i` から個別に `D_i=d_i/cos(R)` を作ってはならない。頂点ごとに異なる回転中心となり、面と凸部の相対depthがYawに応じて伸縮する。BoneSource.rが変えるのはSource共通の回転中心だけであり、depth mapが表す凹凸そのものではない。

この区別により、BoneSource.rを変更しただけではメッシュは動かず、`transform.r.y` が動いたときだけ接線depthによる差が現れる。

## 変形式

同じDepthBone poseについて、次の2つのskin matrixを作る。

```text
M0 = transform.r.yだけを0にしたskin matrix
M1 = transform.r.x/r.y/r.zをすべて含む通常のskin matrix
```

Sourceごとの変形点を次で計算する。

```text
base            = M0 * p_i
rotationWithYaw = M1 * q_i
rotationNoYaw   = M0 * q_i

pDeformed_i = base + rotationWithYaw - rotationNoYaw
            = M0*p_i + (M1*q_i - M0*q_i)
```

括弧内だけが `transform.r.y` による半径DのYaw回転差分である。`transform.r.x/r.z` による通常変形は `M0*p` に残るため、BoneSource.rが他軸へ混入しない。

### `transform.r.y = 0`

この場合は `M1 = M0` なので、Rに関係なく次になる。

```text
pDeformed_i = M0*p_i + M0*q_i - M0*q_i
            = M0*p_i
```

実装では丸め誤差も避けるため、`transform.r.y == 0` のときは括弧内を計算せず、通常matrixを `p` へ直接適用する。

### `BoneSource.rotation = 0`

この場合は `deltaX=0`, `q_i=p_i` なので、従来変形と一致する。

```text
pDeformed_i = M0*p_i + M1*p_i - M0*p_i
            = M1*p_i
```

実装では `sin(R) == 0 && cos(R) == 1` の場合も通常matrixを直接適用し、既存結果をそのまま維持する。

### 凹凸形状の保存

全頂点へ同じ `deltaX` を加えるため、任意の2頂点について次が成立する。

```text
q_i - q_j = p_i - p_j
pDeformed_i - pDeformed_j = M1 * (p_i - p_j)
```

したがってBoneSource.rはSource全体の軌道だけを変え、面に対する凹凸、頂点間距離、メッシュ形状を変えない。

## Influenceと複数Source

距離、falloff、terminal、lock判定には表示静止点 `p` を使う。BoneSource.rでweight選択を変えると、`transform.r.y = 0` でも他軸のpose結果が変わる可能性があるためである。

採用された各Sourceについてだけ共通 `deltaX` とSourceの `M0` を使い、上記の回転差分を計算する。

```text
rest     = sum(p * weight)
deformed = sum(pDeformed * weight)
offset   = rootToTarget(deformed).xy - rootToTarget(rest).xy
```

## Fit Z to Depth

`Fit Z to Depth` は、図の原点・プレーン間の距離と接線からDepthBoneのrest Zを配置する処理である。接線の角度Rを保ったサンプル位置を使う。

```text
d = (rawDepth * depthScale + depthOffset) * worldScale
sampleX = vertex.x + d * tan(R)
sampleY = vertex.y
sampleZ = d
```

`sampleX/sampleY` でBoneに最も近いsampleを選び、そのsampleのworld ZへFitする。接線長は `D=d/cos(R)` だが、Z成分は垂直距離 `d` のままである。

## データモデルとUI

`ExDepthBoneSourceSettings` に次を保存する。

```d
float rotation = 0.0f;
```

Command JSON:

```json
{
  "weight": 1.0,
  "depthOffset": 0.0,
  "depthScale": 1.0,
  "rotation": 0.5235988
}
```

GridDeformer / PathDeformer InspectorのSource一覧とpopupへRotationを追加する。Node Rotation Inspectorと同じ `degrees()` / `radians()` helper、`°` 表記、drag精度を使う。

## GPU packet

Source recordは28 floatsとする。

```text
 0 boneIndex
 1 isTerminal
 2 lockToRoot
 3 linearFalloff
 4 weight
 5 depthScale
 6 depthOffset（worldScale適用済み）
 7 multiplier
 8 sinRotation
 9 cosRotation
10 rotationPivotXShift = deltaX
11 poseYaw = transform.r.y
12..27 M0: no-yaw skin matrix（row-major 16 floats）
```

通常matrix `M1` はBone recordから取得する。Source recordの `M0` と組み合わせ、shader内でYaw半径Dの回転差分を計算する。

関連strideはすべて28に合わせる。

- `DepthBoneGpuSourceStride`
- `NgDepthBoneGpuAsyncSourceStride`
- GLSL `SOURCE_STRIDE`

stale判定用 `sourceInputs` はSourceごとの先頭10設定値を保持する。Bone poseは既存のpose入力fingerprintで判定する。

## GPU shader概要

Influence選択時:

```glsl
vec3 sourceRestLocal = vec3(x, y, sourceDepth);
```

最終変形時:

```glsl
vec3 rotationRest = sourceRest + vec3(rotationPivotXShift, 0, 0);

deformedPoint = M0 * rest
    + M1 * rotationRest
    - M0 * rotationRest;
```

`poseYaw == 0` または `R == 0` は通常skinningへ直接分岐する。

## 互換性と検証条件

- 旧ファイルはrotation 0として読む。
- rotation 0の結果は従来と一致する。
- `ry = 0, rx/rz = 0` でrotation変更によるoffsetは0。
- `ry = 0, rx/rz != 0` でもrotation変更前後が完全一致する。
- `ry != 0, depth != 0` ではrotation変更によりYawの回転距離が変わる。
- Source基準線について `X=d0*tan(R)`, `Z=d0`, angle=R, length=`d0/cos(R)` が成立する。
- 同じXYでdepthだけが異なる頂点対について、R変更前後で相対変形が一致する。
- `+ry/-ry`の双方で、面と凸部の3D距離が同一に保たれる。
- 頂点別の `D_i=d_i/cos(R)` を作らない。
- Fit Zは `x+d*tan(R)`, `z=d` を使う。
- save/load、undo/redo、dirty hash、全keypoint更新、非同期stale判定を通す。
- embedded GLSLをvalidatorで検証する。

## 対象外

- BoneSource orientationの3軸化。
- BoneSource.rによる表示静止メッシュ自体の移動。
- PSD depth mapの回転・再サンプリング。
- BoneSource.rのParameter binding化。
