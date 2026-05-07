# 12. チェックポイントと参考ツリー

## 最後にチェックしたいポイント

簡単に動かしてみる前に、次のあたりを確認しておきます。

- Root 直下に `Body`（上半身）と `Leg R/L`（下半身）などの大枠が整理されているか
- 腕や、必要なら翼 / 尻尾 / 背後リボンなどが、`Body` や土台 Part 基準でぶら下がっているか
- 顔は首の子、首は胴の子になっているか
- 顔と首の親が、必要な箇所で Part になっているか
- 右目、左目、口は、それぞれ `DynamicComposite` でまとまっているか
- `DynamicComposite` に変換した目や口で、表示が欠けていないか
- 手足や後ろ髪が、付け根から先端の順で並んでいるか
- 左右に対応する Part や Deformer が、左右反転ペアとして登録されているか
- 影パーツが、影を落とす本体の子になっているか
- GridDeformer が必要な場所にだけ入っていて、子に正しく Part がぶら下がっているか
- `Part` には `Optimum`、`GridDeformer` には Grid 用 AutoMesh がそれぞれ適用されているか

顔、胴体、髪などをまとめて動かしたい場合は、Part のメッシュだけでなく、外側の GridDeformer も確認しておきます。  
あとから大きく変形させる予定のある場所ほど、ここでグリッドを用意しておくと調整が楽になります。

ここまでできていれば、基本的なモーションや細かい調整を入れても壊れにくいモデル構成になっています。

## 参考ツリー例

実際にはキャラクターごとに差がありますが、ここまでの内容を反映した一例を示します。

- `Root`
  - `Body::G` (GridDeformer) → `Body` (Part)
    - `Neck` (Part) → `Face::Root` (Node)
      - `Face::G` (GridDeformer) → `Face` (Part)
        - `Eye R` / `Eye L` / `Mouth` (DynamicComposite)
        - `Ear`
        - `FrontHair::G` (GridDeformer) → `FrontHair` (Part)
      - `BackHair::G` (GridDeformer) → `BackHair` (Part)
      - `SideHair::G` (GridDeformer) → `SideHair` (Part)
    - `Arm R::Root` (Node) → `Arm R` (Part) → `Hand R` (Part + 影)
    - `Arm L::Root` (Node) → `Arm L` (Part) → `Hand L` (Part + 影)
    - `Skirt` (Part) → `Ribbon` / `Frill` (Part)
      - `Tail::Root` / `Wing::Root` / `BackRibbon::Root` (Node) → `Base` (Part) → ... → `Tip` (Part)
    - `Chest::G` (GridDeformer) → `Chest` (Part)
  - `Leg R::Root` / `Leg L::Root` (Node) → `UpperLeg` (Part) → `LowerLeg` (Part) → `Foot` (Part + 影)
  - アクセサリ各種（必要に応じて Grid / 物理を追加）

## ここまでの到達点

ここまでの内容を守ってツリーを組めば、どこをどう動かすと何が動くのかが分かりやすくなります。  
AutoMesh の適用範囲とアルゴリズムも間違えにくくなり、後からパーツを追加しても構造を壊さずに拡張しやすくなります。

## 次に読む

- 基本パラメータの定義と設定に進む  
  → [パラメータ定義チュートリアル](../02-parameter-definition.md)
