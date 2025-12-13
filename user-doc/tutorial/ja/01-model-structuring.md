# モデル構成チュートリアル（Node / Part の設計と手順）

このチュートリアルでは、nijigenerate でキャラクターモデルを組み立てる際の基本設計思想と、実際の操作手順をまとめます。最初に「どう分けるか」を決め、順序立てて作業を進めることで、後からの調整が楽になります。

## 1. 設計の考え方
- **Root は原点**: 体の大ブロック（頭・胴・腕・脚・尾・アクセ）を Root 直下に配置。
- **形を持つのは Part**: メッシュを持たせるのは Part ノード。AutoMesh を掛ける対象も Part に限定。
- **補助ノードは最小限**  
  - `Node` … ピボット／オフセット用。形なし。
  - `GridDeformer` … 広い面をまとめて変形するときだけ（顔全面、髪の大きな塊など）。対象 Part は子に置く。GridDeformer 自体に AutoMesh は掛けない（Grid 専用プロセッサを除く）。
  - `Composite` … 複数 Part をまとめてマスク・ブレンドしたいときのみ。
  - 物理／Path deformers … 動かしたい Part を子にして付与。

### 典型レイアウト例
- **頭部**: `Head::Root (Node)` → `Head::G (GridDeformer)` → 顔の Part → 目・眉・口などの Part。
- **胴体**: `Body::Root (Node)` → 胴 Part → スカート等の衣装 Part。広域変形が要るものにだけ Grid。
- **腕・脚**: `Arm/Leg::Root (Node)` → 基本 Part → 先端 Part（手/足）。通常 Grid 不要。
- **尾・アクセ**: 独立させたいなら Root 直下、胴と連動させたいなら胴の子。必要なら Grid や物理を追加。

## 2. 実際の操作手順
1. **ルートを準備**  
   Root 直下に `Head::Root` / `Body::Root` / `Arm R::Root` など主要ブロック用の Node を配置。

2. **Part をぶら下げる**  
   各 Root ノードの子として Part を追加。メッシュを持つのはここだけ。

3. **広域変形が必要な箇所に GridDeformer**  
   例: 顔平面や大きな髪塊。`Head::G (GridDeformer)` を Part の親に置き、変形対象 Part を子にする。

4. **Composite / マスクは必要最小限に**  
   目パーツ一式をまとめたい場合などで `Composite` を使用。単一 Part で済むなら追加しない。

5. **物理・Path deform を後付け**  
   揺らしたいリボンや尻尾にだけ物理ノードを追加し、対象 Part を子に配置。

6. **AutoMesh を適用**  
   AutoMesh は **必ず Part を選択** して実行する。GridDeformer を選んでも何もしない（Grid 専用プロセッサを除く）。

7. **構造チェック**  
   - Root 直下の大ブロックが明確か
   - Part 以外に余計なメッシュ保持ノードがないか
   - GridDeformer が必要最低限になっているか

## 3. 参考スナップショット（抽象化）
- `Root`
  - `Tail` (Part)
  - `Body::Root` (GridDeformer) → `Body` (Part)
    - `Arm R::Root` (Node) → `Arm R` (Part) → `Hand R` (Part)
    - `Arm L::Root` (Node) → `Arm L` (Part) → `Hand L` (Part)
    - `Neck` (Part) → `Face::Root` (Node)
      - `Face::G` (GridDeformer) → `Face` (Part) → 目・眉・口など
      - `Back Side Hair::G` (GridDeformer) → `Back Side Hair` (Part) → 髪パーツ
    - `Skirt` (Part) → `Ribbon` / `Frill` (Part)
  - `Leg R` (Part), `Leg L` (Part) + 影パーツ

この流れで組み立てれば、AutoMesh の適用対象が明確になり、不要な Grid / Composite を避けた軽量な構成を維持できます。
