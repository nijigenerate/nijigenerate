# PSD Depth BoneSource Rotation 実装タスク

`doc/psd-depth-rotation.md` の実装・検証記録。

## 完了条件

- BoneSource.rを保存・編集できる。
- データはradian、UIはNode Rotationと同じdegree表示。
- 表示静止点を動かさず、Source共通の `X=d0*tan(R)`, `Z=d0` をYaw回転中心へ使う。
- `transform.r.y = 0` ではBoneSource.rによる差がない。
- `transform.r.y != 0 && depth != 0` ではBoneSource.rによって回り方が変わる。
- BoneSource.rはSource全体の回転中心だけを変え、面に対する凹凸形状を変えない。
- Fit Zは図の投影式を使う。

## データ・UI・Fit Z

- [x] `ExDepthBoneSourceSettings.rotation` を追加。
- [x] save/load、JSON command、List、Set、undo/redoへrotationを追加。
- [x] 非有限値を0へ戻し、`[-pi, pi)` へ正規化。
- [x] GridDeformer / PathDeformer Inspectorへdegree editorを追加。
- [x] Node Rotationと同じ単位変換・表示規約を使用。
- [x] Fit Zを `x+d*tan(R)`, `z=d` へ変更。
- [x] Fit Zのnearest sample、scale、offset、rotation回帰を追加。

## 変形モデル

- [x] 表示静止点 `p=(x,y,d)` をBoneSource.rから分離。
- [x] Bone原点に最も近い面サンプルから共通距離 `d0` を取得。
- [x] Source共通の `deltaX=d0*tan(R)` と `D=d0/cos(R)` を求める。
- [x] 全頂点の仮想回転点を `q_i=p_i+(deltaX,0,0)` とする。
- [x] 基準線の角度がR、長さがDになることを数値検証。
- [x] 通常matrix `M1` と `transform.r.y=0` のmatrix `M0` を構築。
- [x] `pDeformed_i=M0*p_i+(M1*q_i-M0*q_i)` をshaderへ実装。
- [x] Influence、falloff、terminal、lock判定には固定表示点を使用。
- [x] `ry=0` と `R=0` は通常skinningへ直接分岐。

## GPU packet

- [x] Source recordを28 floatsへ変更。
- [x] index 8/9へsin/cos。
- [x] index 10へSource共通rotationPivotXShift。
- [x] index 11へposeYaw。
- [x] index 12..27へno-yaw skin matrix `M0`。
- [x] CPU、async validation、GLSLのstrideを28で統一。
- [x] Source rotationをdirty hashとstale判定へ追加。

## 回帰条件

- [x] `ry=0, rx=rz=0`: Rを変えてもoffset 0。
- [x] `ry=0, rx/rz!=0`: R変更前後のfloatが完全一致。
- [x] `ry!=0, depth!=0`: R変更によりoffsetが変化。
- [x] Bone原点と面サンプルの距離が0ならR変更による差が消える。
- [x] Scale適用後のdepthで、R変更が1pxを超える実質的なYaw距離差を生成。
- [x] adjusted depthを2倍にすると、BoneSource.r由来の変位差も2倍になる。
- [x] Source共通の回転中心補正が `+ry/-ry` の双方で有限に働くことを検証。
- [x] 同一XYでdepthだけ異なる頂点対の相対変形がR変更前後で一致。
- [x] `+ry/-ry`の双方で面と凸部の3D距離を保存。
- [x] packet layoutとshader source contractを検証。
- [x] rapid edit、running job置換、全keypoint更新を検証。

## 実行済み検証

- [x] `dub build -c regression-tests`
- [x] `./out/nijigenerate-regression-tests --only depthbone.gpu-packet`
- [x] embedded GLSL: `glslangValidator --stdin -S vert`
- [x] `./out/nijigenerate-regression-tests --only depthbone.sources`
- [x] `./out/nijigenerate-regression-tests --only depthbone.fit-z`
- [x] `./out/nijigenerate-regression-tests --only depthbone.gpu-all-keypoints`
- [x] `./out/nijigenerate-regression-tests --only depthbone.serialization`
- [x] `./out/nijigenerate-regression-tests --only depthbone.preview-commands`
- [x] `dub build -c osx-full`
- [x] `git diff --check`

## 以前の誤実装

1回目は仮想接線点を表示静止点として直接skinningし、`rz=0`でも他軸とBoneSource.rが結合した。

2回目は回転軸を共役回転で傾けたが、図が要求する「depth距離を分解した接線点を基準にした回転差分」ではなかった。

3回目はこの差分を `transform.r.z` へ適用したが、実装上のYawは `transform.r.y` である。Face::G、FrontHair::G、BackHair::GのHead Boneには `transform.r.z` bindingがないため、実モデルでBoneSource.rが効かなかった。

4回目は `pR=(x+d*tan(R),y,d)` としてRをYaw前の方向にも入れたため、Yaw符号で反転する前後項が生じた。これにより一方のYawで顔が髪へ潜り、反対側でdepthが潰れる非対称が発生した。

5回目は各頂点のdepthから `D_i=d_i/cos(R)` を作ったため、面と凸部へ異なる回転中心を与え、Yawに応じて凹凸そのものを伸縮させた。

6回目はSource共通化したものの、補正をZ方向の `D-d0` としたため、接線長だけが変わり、BoneSource.rで指定した線の角度が回転結果へ入らなかった。

現在はBone原点に最も近い面からSource共通の `deltaX=d0*tan(R)` を一つだけ求め、全頂点へ同じX方向回転中心補正を適用する。基準線は角度Rと長さDを持ち、メッシュ内の相対depthと凹凸形状は通常Yawと同じ剛体変換で保存される。
