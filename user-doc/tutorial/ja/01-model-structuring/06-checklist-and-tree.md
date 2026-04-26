# 06. チェックポイントと参考ツリー

## 6. 最後にチェックしたいポイント

簡単に動かしてみる前に、次のあたりを確認しておくと安心です。

- Root 直下に `Body`（上半身）と `Leg R/L`（下半身）などの大枠が整理されているか
- 腕や（必要なら）翼 / 尻尾 / 背後リボン等は、`Body` や土台 Part 基準でぶら下がっているか
- 顔は首の子、首は胴の子になっているか
  **いずれも親が Part になっているか** を確認する
- 手足や後ろ髪が「付け根 → 先端」の順で並んでいるか
- 影パーツが「影を落とす本体」の子になっているか
- GridDeformer が必要な場所にだけ入っていて、子に正しく Part がぶら下がっているか

ここまでできていれば、基本的なモーションや細かい調整を入れても壊れにくいモデル構成になっています。

## 7. 参考ツリー例（抽象化）

実際にはキャラクターごとに差はありますが、ここまでの内容を反映した一例を示します。

- `Root`
  - `Body::G` (GridDeformer) → `Body` (Part)
    - `Neck` (Part) → `Face::Root` (Node)
      - `Face::G` (GridDeformer) → `Face` (Part)
        - `Eyes` / `Mouth` / `Ear`
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

ここまでの内容を守ってツリーを組めば、

- どこをどう動かすと何が動くのかが分かりやすい
- AutoMesh の適用範囲とアルゴリズムを間違えにくい
- 後からパーツを追加しても、構造を壊さずに拡張しやすい

といった “扱いやすいモデル構成” を作ることができます。
