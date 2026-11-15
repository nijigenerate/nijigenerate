# StructuralPatternProcessor

`structural-p01` ～ `structural-p35` を AutoRigSessionManager に登録する。
対応する設計は `autorig-parameter-audit/autorig-generalized-pattern-design.md`。
モデル名や Part 名を判定条件には使用しない。

## 実装範囲

現段階は **取込済みINXに対する条件検出、自動計画、役割付き対象指定、変形キー／表示設定の生成、構造検証**。
35 個の登録が、設計書にある全自動認識・全制約ソルバの完成を意味するわけではない。
PSD バイナリの読込、画像による人体・衣装の意味認識、隠れ面の描画、メッシュや階層の新規作成はこの Processor では行わない。
PSD は既存のインポート／メッシュ生成を通して INX にした後で入力する。

P22～P29 は指定された局所座標・ウェイト・アンカーから **運動学的なキー** を生成する。
SimplePhysics の新設、接触・紐長の時系列制約、周期入力の供給、骨格推定は未実装。
P30/P31 は既存 Node/物理 Node に入力依存のバインドを追加する。
見た目、追跡入力、物理運動が完成したことを構造検証の成功から推論しない。

## タスクと接続

|タスク|入力|出力|
|---|---|---|
|`detect`|`model`: INX Blob、`selection`: 任意の JSON|`targets`: JSON、元の `model`|
|`deform`|`model`、`targets`、`recipe`: JSON|変形後の `model`、`receipt`: JSON|
|`verify`|`before`、`after`、`targets`、`receipt`|検証済み `model`、`report`: JSON、`targets`|

`rig-pattern` ワークフローが上記を接続する。`recipe.controls` は1個以上の操作を持てる。
後の操作が失敗した場合も入力モデルは変更されない。編集中モデルへ直接書き戻さず、INX Blob を出力する。
画像・テクスチャのバイナリ部分は保持し、verify で一致を確認する。
タスク例外は既存フレームワークの失敗状態へ変換する。

## 条件検出と手動置換

省略時の `selection` は自動検出。`detection.cpp` が35パターンの構造・サンプル挙動・幾何を調べる。
既存の注釈や `condition` の指定は不要。根拠が確認できた `matches` と、構造上の候補である
`candidates` を分ける。候補は自動適用されない。意味や関係を確認し、その `roles` を manual に渡して選択する。
Node の `autorig_structure.patterns: ["p17", ...]` と `autorig_structure.roles` に構造認識結果を渡すこともできる。
この注釈を生成する PSD 意味認識器は未実装。

### 自動計画（2026-11-13追加）

`recipe`は省略可能。`planning.cpp`が入力モデルから`plan`を生成する。
検出結果には実行対象の`matches/parts/nodes`、計算した`recipe`、採用した`policy`、不足条件の`reason`が含まれる。
候補を一律に確定へ格上げせず、以下の生成経路を使う。元モデルの動作・物理定数の復元ではない。

|入力条件|生成するもの|
|---|---|
|P13/P14: Multiply/Screen等、明示マスクの受け面|入力両端で15%の不透明度減衰、層の幅の1%の位置移動。受け面の変形入力があればリンクし、無ければ受け面ごとに共通の照明入力Parameterを新設して接続。基本AutoRig未完了でも生成可能|
|P18: マスク参照先がサンプル上で縦に閉じる|開口中心の移動から対象の既存中心移動を差し引いた残差。対象自体は潰さず、元の開閉入力から駆動|
|P19: 確認済み開口の多段マスク内部、または小さくコンパクトな内部面|短辺の±1.5%の独立XY微小変形。物理ノード・時間信号の生成は含まない|
|P20: サンプル上で確認した開口面|中立を変えない±5%の独立幅制御|
|P08〜P10: 既存の明示マスク参照|同じ参照を保持。Mask/DodgeMaskを混同して追加しない|
|手動指定または構造注釈で確認したP03/P06/P07/P17/P22〜P27/P29/P32/P34/P35のPart|局所主軸・支点・代表長、支持部からの距離による保護ウェイトを求め、サンプルを生成|
|手動指定したP01/P04の元面と対象|元面の三角形内に対応が取れる頂点に、重心座標補間で変位を移送。座標基底を変換し元の入力へ接続|
|手動指定したP16の接合境界|境界頂点を近接対応し双方向Weldingを生成。重なっているだけの候補には適用しない|
|手動指定したP28の複数支持先と対象|支持先のサンプル変位、対象上のアンカー、距離補間からキーを生成。紐長保存ソルバではない|

独立操作の振幅は新規の設計値であり、作者の元キーを再現した値ではない。
手動の幾何プロファイルは曲げ・回転±0.08rad、伸縮±5%、局所移動±代表長2%、
P29は移動と微小回転の運動学的合成。見た目の受入れ・時間応答は別途検証する。
新設した照明入力は照明層を操作するもので、受け面本体の角度変形を生成したという意味ではない。
P05はローカル回転、P11はClipToLower、P21は不透明度による独立表示操作を生成する。
P12/P15/P30/P31/P33は操作の意図を静止素材から決められないため、`selection.intent`で必要な値を与える。

バッチでも手動置換できる。旧来の`{"pNN": recipe}`に加え、次の形を受け付ける。

```json
{"p16":{"selection":{"mode":"manual","matches":[{"roles":{"target":[1001],"seam":[2001]}}]}}}
```

`selection.intent`の項目:

- P01: `source_node`、必要なら`source_parameter`。P04は`surface`役割から元面を決定。
- P02: `source_parameter`, `destination_parameter`, `input_axis`, `output_axis`、任意の`gain/bias`。
- P12: `gain_x`。P15: `blend_mode`。
- P13/P14: 必要なら`surface_node`, `driver_parameter`。手動で指定された素材は合成モードだけでは除外しない。
- P30/P31: `property`, `gain_x`、必要なら`neutral`。
- P33: `property`, `value`。

空の手動選択は自動検出へ戻さない。生成した入力リンク以外の既存バインドを保持し、
検証時に自動計画を再計算して一致を確認する。生成キーとレシピはreceiptに記録する。
`structural_key`はバッチ出力での重複生成を防ぐ管理情報。表示名は条件に使わない。
エディタ側で未知フィールドを落として再保存した場合の管理情報保持は未対応。

PSD抽出の静的情報保持と40モデルでの結果は
`autorig-parameter-audit/psd-autorig-results/README.md`へ集約する。
全35種のPSD画像意味認識が完成したという意味ではない。衣装・支持・接合の多義性や不足素材は残る。

明示した条件による候補抽出例:

```json
{"mode":"auto","condition":{"type":"Part","parent_type":"Node","min_aspect":3}}
```

`condition` の各項目は AND。利用可能項目は `type`, `parent_type`, `binding`（プロパティ名）、
`min_children`, `min_masks`, `min_aspect`（メッシュの長辺/短辺）。指定時は既定の候補条件を置き換える。
縦横比から「リボン」「脚」と意味分類したことにはならない。対象がその構造であるという判断は呼出側の責任。
`support` は親、`mask`/`surface` はマスク元、`seam` は既存 Welding の相手を候補として解決する。
役割を解決できない候補は `unresolved` に出し、変形を停止する。

手動置換例（P08）:

```json
{"mode":"manual","matches":[{"roles":{"target":[1001,1002],"mask":[2001]}}]}
```

自動候補との併合ではなく置換。空配列は「対象なし」であり自動検出へ戻らない。
UUID の存在・重複・役割・Part 型・自己マスク／自己接合を自動と同じ経路で検証する。
検出結果は `autorig.pattern-targets/1`:

```json
{
  "schema":"autorig.pattern-targets/1","pattern":"p08",
  "source":"元JSONのfingerprint","mode":"manual",
  "parts":[1001,1002,2001],"nodes":[],
  "matches":[{"roles":{"target":[1001,1002],"mask":[2001]},"provenance":"manual"}],
  "unresolved":[],"status":"matched"
}
```

`parts`/`nodes` は全役割の参照一覧、実際の変形先は各 match の `target`。
Welding は `seam` 側にも相互リンクを書き込む例外。
`source` が現在のモデルと一致しない検出結果は使わず、検出し直す。
Fingerprint は取り違え検出用で、署名や認証用途ではない。

## 35パターンの検出条件

共通観測は、親子グラフ、ローカル／累積2D変換、メッシュ境界・連結成分・穴、包含、近接境界、
マスク・接合、パラメータリンク、物理出力の推移的な参照先、設定済みキーの変化。
未設定キーは根拠に数えず、基準キーが未設定の場合は設定済みキー間で変化を測る。
既存パラメータやレイヤーの名称は読まない。

|ID|検出する根拠|結果の扱い|
|---|---|---|
|P01|変化する頂点場・局所変換バインド|既存の空間制御としてmatch。意味軸名は推定しない|
|P02|X/Yリンクから推移的に到達するNode|match、リンク元／先パラメータIDを添付|
|P03|設定済み2軸サンプルの混合差分が非ゼロ|非分離の補正場としてmatch|
|P04|マスク受け面、同じ階層文脈での包含|surface候補。接着意図・対応は要確認|
|P05|同じNodeに頂点変形と局所変換が併存|親をsupportとしてmatch|
|P06|縦横比2.5超のメッシュ、回転成分|体積軸・曲げ・ねじりの候補|
|P07|複数受け面、または固定／可動領域と境界接触|被覆候補、前後側面の割当は要確認|
|P08|明示マスク参照、または他面に包含された層|参照はmatch、包含だけなら候補|
|P09|DodgeMask、または受け面からはみ出す描画領域の外接矩形|DodgeMaskはmatch、はみ出しはアルファ確認が必要|
|P10|複数・共有・連鎖するマスク|mask役割を解決してmatch|
|P11|PartのClipToLower/SliceFromLower|match|
|P12|設定済みzSortキーが変化|match。定数バインドを除外|
|P13|設定済みOpacityキーが変化|match。定数バインドを除外|
|P14|受け面、照明用合成モード、変形／移動／Opacity制御の組合せ|影・光沢候補。画像意味は要確認|
|P15|非Normalの合成モード、色・素材プロパティの変化|観測された素材設定としてmatch|
|P16|既存Welding、または複数の近接メッシュ境界頂点|既存接合はmatch、新規境界は候補。矩形の重なりだけでは接合候補にしない|
|P17|異方的な変形による領域縮小、または包含された横長領域|縮小挙動はmatch、未リグ形状は開口候補。目・口という分類ではない|
|P18|縮小する受け面の内部素材の運動、または内部包含|support候補。開閉入力との対応は要確認|
|P19|入れ子の素材に微小変形または物理出力|局所揺れ候補。瞳／光沢などの意味は要確認|
|P20|開口・包含領域の異方的な変形|開口幅制御候補|
|P21|0/1の表示状態が切り替わるOpacityキー|状態素材候補。表情か衣装かは要確認|
|P22|局所回転、回転変形場、または細長い階層列|回転制御はmatch、未リグの列は関節候補|
|P23|共通の親に複数の細長い子|分岐候補。関節根元は要確認|
|P24|放射方向の伸縮場、またはXYの伸縮制御|放射場はmatch、XY制御だけなら膨張候補|
|P25|親支持と固定／可動領域、膨張または物理出力|柔軟体積候補|
|P26|物理駆動、または細い支持部に接続した塊状素材|吊り下げ候補。剛性と支点は要確認|
|P27|メッシュの穴、または結節状の親と複数の細長い尾|ループ・結び目候補。交差の接続関係は要確認|
|P28|細長い面が複数の接合先／近接境界を持つ|複数アンカー候補。固定点かどうかは要確認|
|P29|直接・祖先・リンク経由の複数物理出力|駆動の合成関係としてmatch|
|P30|物理Node自身または祖先の変換制御|support付きでmatch|
|P31|既存物理Nodeの倍率・減衰・周波数・長さ・重力の入力依存|match。単に物理Nodeがあるだけでは該当しない|
|P32|局所に限った変形、または小さい非一様変形|局所補正場としてmatch|
|P33|MeshGroup/Composite/DynamicComposite|伝播境界としてmatch、現在の設定を添付|
|P34|固定／可動頂点の混在、または非連結メッシュ領域|部分適用候補。人体・衣装という意味分割は要確認|
|P35|局所XY移動バインド、または一様な頂点移動|独立移動制御としてmatch|

各結果には `evidence`, `metrics`, `roles`, `parts`, `nodes`, `requires_review`, `missing` を出力。
`confidence` の0.95／0.55は観測根拠の強さを区別する固定値で、統計的に校正した正解確率ではない。
候補しかない場合のstatusは `needs_review`。候補を明示選択せずdeformすると停止する。

PSD取込後の、キーがまだないメッシュでも幾何候補の経路を使える。
ただし画像の色・生アルファ・全身文脈を読む視覚モデルは追加していないため、
目と宝石、口と衣装の穴、リボンと別の帯を自動的に意味分類するものではない。
外接矩形の包含やメッシュの穴は画像の包含・穴そのものではない。XY回転を含む3D姿勢では
2D外接矩形の比較を避ける。画像認識や隠れ面復元が済んだと見なして自動適用しない。

## 変形レシピ

P13 の例:

```json
{"controls":[{
  "label":"Surface visibility","x":[-1,0,1],"y":[-1,0,1],
  "neutral":0.7,"gain_x":0.2,"gain_y":0
}]}
```

各操作は既存パラメータを上書きせず、新しいパラメータを作る（P02 のリンク追加は例外）。
`uuid` は任意、省略時は衝突しない ID を割り当てる。`label` は既存名・同一レシピ内の他操作と重複不可。
`x`,`y` は物理入力値の昇順サンプル（各2～65個、0を含む）。正規化した axis_points と min/max を生成する。
現状は両軸を持つキーを生成する。未使用軸も必要。

解析的な頂点変形の共通指定:

```json
{"controls":[{
  "label":"Local correction","x":[-1,0,1],"y":[-1,0,1],
  "origin":[0,0],"direction":[1,0],"extent":100,"amount":0.1,
  "weights":[0,0.5,1]
}]}
```

座標・距離は対象 Part のローカル座標、回転量はラジアン。
`weights` は頂点順の0～1、P07/P25/P34では必須。それ以外では省略時1。
複数 Part が異なる座標系や頂点数を持つ場合は、対象を分けて Processor を実行するか、
`controls[].bindings` に対象ごとのサンプルを渡す。共通原点を無条件に共有しない。
`bindings` は INX の `node,param_name,values,isSet,interpolate_mode` 形式。
各バインドの参照先が `target` に含まれること、型・値・サンプル寸法を検査する。

## パターンごとの実装と残る範囲

|ID|役割（target以外）|実行する処理／追加指定|未実装・事前条件|
|---|---|---|---|
|P01|なし|既存 source_parameter/source_node/property のキーを移植。deform は correspondence と basis で移送|意味軸の自動同定、異なる軸への再サンプリング|
|P02|なし|source_parameter → destination_parameter の input_axis/output_axis に gain/bias でリンク追加|リンク先の既存バインド対象を全て target に指定。入力軸の意味推定なし|
|P03|なし|局所変形に amount × X × Y の残差場|参照画像からの残差フィッティング|
|P04|surface|P01 の頂点場移送、受け面の選択と deform を必須化|自動表面登録、UVからの対応推定、実行時の拘束ではなくサンプル移植|
|P05|support|property に局所変換の入力依存バインドを追加|既存の親階層を保持。親子構造の生成なし|
|P06|なし|長軸位置で角度を変える曲げキー|ねじりの3D再投影、隠れ面生成|
|P07|support|保護ウェイト付き局所変形|前後側面の分離・支持領域の推定|
|P08|mask|Mask 参照追加、既存参照を重複させない|マスク形状の新規描画、可動形状は別の変形操作で指定|
|P09|mask|DodgeMask 参照追加|残余貫通の画像検出、過剰面の描画|
|P10|mask|複数target×maskの参照を生成|最適マスクネットワークの推定、循環解消|
|P11|なし|blend_mode を ClipToLower/SliceFromLower に設定|合成スコープ・順序の自動再構築|
|P12|なし|入力依存 zSort バインド|前後関係から切替境界を推定する処理|
|P13|なし|neutral + gain_x×X + gain_y×Y の opacity|影意味分類、角度曲線の自動推定|
|P14|surface|property に影などの局所変換バインド|法線・照明からの影／濃度生成|
|P15|なし|blend_mode を設定、または controls の property に tint.r 等の色・素材バインドを生成|入力による素材差替え。静的モードと色制御は別のレシピで実行|
|P16|seam|links の source/target/indices/weight から双方向 Welding を生成|境界の自動対応。indices=-1は非接合、対応は一対一|
|P17|なし|端点を保持し中央を閉じる局所頂点場|眼・口の認識、内部素材／接合構造の生成|
|P18|support|property に開閉連動の局所位置等のバインド|開口境界との接触制約推定|
|P19|なし|局所XY微小移動のキー|揺れ入力を発生させる物理ノード生成|
|P20|なし|局所XY方向の独立伸縮|表情サブタイプの認識・専用素材生成|
|P21|なし|opacity の表示状態キー|専用素材の描画、状態選択入力生成|
|P22|なし|指定支点の回転キー|関節列・骨格の生成と推定|
|P23|support|指定支点の回転キー|分岐構造推定、枝ごとの拘束・角度配分|
|P24|なし|局所体積の伸縮キー|周期信号の供給|
|P25|support|保護ウェイト付き伸縮キー|体積保存ソルバ、二次運動の時間積分|
|P26|support|指定支点の剛体回転キー|重力・衝突・振り子入力の生成|
|P27|support|指定支点の回転キー|結び目・輪・垂れ部分のネットワーク生成、伸長制約|
|P28|support|anchors[].position/motion_x/motion_y を距離二乗の逆数で補間|紐長保存・接触の制約ソルバ。運動学的補間である|
|P29|なし|modes[].offsets/gain_x/gain_y の頂点場を加算|複数物理ドライバの生成・接続|
|P30|support|既存物理等の Node に property の変換キー|支持階層の自動再構築|
|P31|なし|既存 SimplePhysics の property に outputScale.x/y・angleDamping 等のバインド|自動係数推定。property省略時はoutputScale.x|
|P32|なし|局所XY平行移動の頂点場、または authored bindings|補正意図の推定|
|P33|なし|MeshGroup.dynamic_deformation / Composite.propagate_meshgroup を変更|デフォーマの新設・階層移動|
|P34|なし|必須の領域ウェイトに従う局所変形|一枚絵からの人体／衣装領域抽出|
|P35|なし|property に独立移動等のバインド|複数の階層で親子を同時指定すると二重適用し得る。群の切り分けが必要|

P01/P04 の correspondence は対象頂点ごとの `{ "indices":[元頂点ID...], "weights":[係数...] }`。
係数和は1。`basis:[[a,b],[c,d]]` は元の変位ベクトルから対象ローカル変位への変換。
元パラメータと同じ物理サンプル軸を指定する。座標や物理軸が不明なまま暗黙に合わせない。

P16 の例:

```json
{"links":[{"source":1001,"target":2001,"indices":[2,-1,0],"weight":0.5}]}
```

相手側に逆対応も生成する。既存接合を上書きせず拒否する。相手のウェイトは `1-weight`。

## 検証と再実行

`verify` は元／結果／対象指定の一致、Node の存在と階層保持、メッシュ／UV保持、
対象外 Node の不変、元パラメータ保持、生成バインドの寸法・型・有限値・重複、opacity範囲、
パラメータリンク循環を検査する。INX Blob のテクスチャ部分も照合する。
P02 は元の設定と既存バインドを保持し、リンクだけを追加する。
検出は名前非依存だが fingerprint は名前を含む元 JSON 全体に依存するため、改名後も再検出が必要。

`report.passed=true` はこの構造検証の成功。
常に `visual_review_required=true`、`time_response_verified=false` を返す。
nijikan画像比較、補間途中の反転、マスク循環、物理安定性、PSD認識精度は未検証。
生成済み制御を同名のまま再生成すると失敗する。別名の追加で二重駆動しないよう対象と入力設計を確認する。

## ビルドとテスト

バッチ実行用の `nijigenerate_autorig_patterns_batch INPUT.inx OUTPUT_DIRECTORY [RECIPES.json|--detect-only]`
は、実際のAutoRigSessionから35 Processorを呼び出し、対象と段階別の結果を保存する。
既定では全35個に`verify`までの実行を要求し、その依存タスクである`detect`と`deform`を実行する。
基本AutoRigの成否、検出対象の有無、明示レシピの有無によるバッチ側のスキップは行わない。
レシピ省略時は空オブジェクトを渡し、各Processorが自動計画を生成して適用可能性を検査する。
設定不足や未確定候補による停止も、実際に呼び出した`deform`の失敗として記録する。
`deform`失敗時の`verify`は入力が成立しないため`Blocked`となる。検証コード実行済みとは数えない。
各タスクの実行回数・状態・エラーを`pNN-execution.json`と`report.json`に保存する。
既存と同じ設定なら `verified_unchanged`、変更を伴って検証に成功した場合のみ `applied` と記録する。
失敗した処理の変更は採用せず、残りのProcessorを継続する。最終モデルは`patterns-result.inx`に保存する。
`--detect-only`は明示的に検出だけを要求する場合のオプションであり、基本AutoRig失敗時に自動指定しない。
レシピファイルは `{"p01": {...}, "p02": {...}}` の形式。
自動計画の範囲は上記の通り。PSD全体の汎用意味認識器を実装したものではない。

```sh
cmake --build --preset macos-release --target nijigenerate_autorig_patterns_batch
python3 native/tests/source/autorig_patterns_batch_test.py build/macos-native/native/nijigenerate_autorig_patterns_batch
```

リポジトリルートから:

```sh
cmake --build --preset macos-release
cmake --build --preset macos-release --target nijigenerate_autorig_patterns_tests
build/macos-native/native/nijigenerate_autorig_patterns_tests
cmake --build --preset macos-release --target nijigenerate_autorig_pattern_workflow_tests
build/macos-native/native/nijigenerate_autorig_pattern_workflow_tests
cmake --build --preset macos-release --target nijigenerate_autorig_pattern_detection_tests
build/macos-native/native/nijigenerate_autorig_pattern_detection_tests
```

35 パターンそれぞれの代表レシピ、手動指定、改名非依存、条件検索、対象無し、UUID不正、
古い検出結果、複数操作の失敗時の入力保持、Welding相互対応、頂点保護、opacity範囲、
重複マスク、INXバイナリ保持、キャンセルを検査する。
これは設計書全体の受入れ試験ではない。
別のワークフローテストでは、実際の WorkflowProcessor を35個構築し、必須入力と接続の整合性、
依存タスクが重複せず3個に展開されることを検査する。
検出テストでは注釈なしの構造・キーフィクスチャで35パターンの陽性を確認し、単純な静止形状、
定数キー、未設定キー、接合していない境界、改名、候補の適用防止、手動空選択、キャンセルを確認する。

追加の引数に INX のパスを渡すと、そのモデルを変更せず35パターンの既定検出を実行する。
PSD意味認識や全モデルへの変形適用試験とは区別する。

2026-11-13 の確認結果: macos-release のアプリ全体ビルド、35 パターンの代表レシピと上記不変条件テストが成功。
`autorig-parameter-audit/selection.json` の active_ids に指定された最新版40モデルすべてで、
読込と35パターンの既定検出が完走した（Aka 0.9.3、Midori 0.9.3、AngelGirl link2、gimp-tan 2.7を含む）。
元モデルファイルは変更していない。検出件数は候補数であり、認識の正解率や全パターンの発見を保証しない。

検出拡張後の40モデルの集計は [detector-validation.json](/Users/seagetch/src/nijigenerate/autorig-parameter-audit/detector-validation.json)。
モデルごと・パターンごとのmatches/candidates件数と、検出器ソースのSHA-256を記録した。
既存のP31制御が40モデル中で検出されなかったことと、P31検出器が未実装であることは異なる。
P31の正例は物理プロパティを変えるフィクスチャで検証している。
