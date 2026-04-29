# 06. ステップ 3：手足・後頭部の髪を付け根から先端に並べる

手足や髪の束など、細長く連結しているパーツは、付け根から先端へ向かう順番でツリーに並べます。  
体に近い側を親、先端側を子にすると、回転や揺れを設定しやすくなります。

## 理想的な並び

- 腕は、`Body` → `UpperArm` → `LowerArm` → `Hand` の順にする
- 脚は、`Root` → `Leg R::Root` / `Leg L::Root` → `UpperLeg` → `LowerLeg` → `Foot` の順にする
- 翼、尻尾、背後リボンなどは、胴体または土台 Part → `Root` ノード → `Base` → ... → `Tip` の順にする
- 後ろ髪は、`Head::Root` または `Body` → `BackHairRoot` → `HairChunk` → `HairTip` のように並べる
- 口パーツを `DynamicComposite` でまとめる場合も、基準となる口ノードを付け根側に置き、その下に差分をぶら下げる

## よくある失敗

- 手や髪の先端を親にしてしまう
- 翼、尻尾、背後リボンなどを Root 直下に置いてしまう
- 土台 Part に直接すべてをぶら下げ、付け根の Node を作らない

先端を親にすると、根元から自然に動かすことが難しくなります。付け根には Node を挟み、そこを回転や移動の基準にしてください。

## 操作例

1. 右腕の Part を、付け根に近い順に確認する。
2. 付け根の Part を選び、「親として挿入」で `Node` を差し込み、`Arm R::Root` のように名前を付ける。
3. `Arm R::Root` の子に、`UpperArm` → `LowerArm` → `Hand` の順で Part を並べる。
4. 脚は Root 直下に `Leg R::Root` / `Leg L::Root` を作り、その子に `UpperLeg` → `LowerLeg` → `Foot` の順で Part を並べる。
5. 翼、尻尾、背後リボンなども同様に、胴体またはスカートなどを土台として、付け根から先端へ並べる。

## 次に読む

- 補助ノードを親として挿入する  
  → [07. ステップ 4：Node / Composite / DynamicComposite を親として挿入する](./07-step-4-insert-helper-nodes.md)
