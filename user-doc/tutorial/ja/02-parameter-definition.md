# パラメータ定義チュートリアル（基本パラメータ / 変形設定）

モデル構成ができたあとに、「どのパラメータを作り、どの Part / Deformer にどのように割り当てるか」をまとめたチュートリアルです。

この章では、顔・胴体・目・眉・口など、最初に作るべき基本パラメータを中心に扱います。髪や服の揺れのような物理系は、ここでは基本方針と入口だけ整理し、詳細な調整は別章で扱う前提にします。

## 読み方

- 最初に全体像をつかみたい  
  → [01. 全体像と推奨パラメータ](./02-parameter-definition/01-overview.md)
- 値域・デフォルト値・キー位置を決めたい  
  → [02. 値域とキー位置](./02-parameter-definition/02-values-and-keys.md)
- 顔の向きを設定したい  
  → [03. 顔の向き](./02-parameter-definition/03-face.md)
- 胴体の向きを設定したい  
  → [04. 胴体の向き](./02-parameter-definition/04-body.md)
- 表情を設定したい  
  → [05. 表情](./02-parameter-definition/05-expression.md)
- 物理系の扱いと最終確認を見たい  
  → [06. 物理系とチェックポイント](./02-parameter-definition/06-physics-and-checklist.md)

## 推奨順

1. [01. 全体像と推奨パラメータ](./02-parameter-definition/01-overview.md)
2. [02. 値域とキー位置](./02-parameter-definition/02-values-and-keys.md)
3. [03. 顔の向き](./02-parameter-definition/03-face.md)
4. [04. 胴体の向き](./02-parameter-definition/04-body.md)
5. [05. 表情](./02-parameter-definition/05-expression.md)
6. [06. 物理系とチェックポイント](./02-parameter-definition/06-physics-and-checklist.md)

## この構成で分けた理由

- パラメータ名の設計と、実際の変形設定を分けて参照しやすくする
- 2 次元パラメータと 1 次元パラメータの役割を混同しないようにする
- 値域・デフォルト値・キー位置を先に固定し、後の変形設定で迷わないようにする
- 基本パラメータの設定は、顔・胴体・表情で編集対象と注意点が異なるため、作業単位ごとに分ける
- 物理系は基本パラメータとは別の判断が必要なため、最後に独立して整理する
