# nijigenerate C++移植設計

作成日: 2026-10-25。作業ブランチ: `native-cpp`。
本書は移植の設計であり、C++実装の完了報告ではない。実行計画は `tasks.md` を参照する。

実装・担当・自己レビューの単位はD原本1ファイル全体とする。全宣言・全本文・実provider接続・
公開header/namespace・通常CMake登録をまとめて完了する。WBSの関数・宣言カードは依存と受入記録の
分解であり、関数単位の実装割当・少数修正ごとの報告停止には使わない。カード更新は原本全体の
実装と自己レビュー後に最大10更新ずつ読み戻し・同期し、原本数には加算しない。

## 1. 目的と正とする実装

現在の `source/` のD実装を仕様の正とし、各モジュールをC++へ1:1で移植する。
ランタイムはnijiliveからnicxliveへ切り替え、最終成果物はDコンパイラ・Dランタイムを必要としない。
機能削減、UIの再設計、アルゴリズム置換、数値最適化は移植と分離する。
1:1とは文字列の機械的置換ではなく、モジュール・型・関数・状態遷移・副作用の追跡可能な対応を指す。

- Dの関数名、引数順、既定値、分岐、反復順、座標系、補間式、例外条件を保持する。
- `.inx`/`.inp`、設定、ショートカット、コマンドID、MCP/ACP/njcの外部契約を保持する。
- 現行D版に不具合を発見しても、移植時に黙って修正しない。差異として記録し、修正を別課題にする。
- nicxliveにAPIがあることと、D版と同じ挙動を持つことを区別する。不足は依存側の阻害課題として管理する。
- 参照は独立に作成された現行プロジェクトコードと許可された資料に限定する。
  Live2D Cubism Coreおよびそれに由来する情報は、履歴・キャッシュ・過去の出力も含め一切使用しない。
  削除済みブランチや過去のタスクから参照実装を復元しない。

今回作成したのは設計とタスク一覧のみ。開始時のD参照は現行作業ツリーとし、実装開始前に許可対象ファイルの
SHA-256、依存バージョン、ビルド設定を保存する。未コミットのD編集も参照集合に含め、以後の変更は差分管理する。

## 2. ローカル調査結果と採用範囲

以下は隣接チェックアウトの現在のファイルを読んだ結果であり、リモートの最新版を保証するものではない。

| 確認箇所 | 観察と採用方針 |
|---|---|
| `../nijiexpose/CMakeLists.txt` | C++20、CMake、`native/`、CTest構成を使用。同じ基本構成を採用する |
| `../nijiexpose/cmake/Dependencies.cmake` | nicxliveのローカルソース指定、fast-math/AVX2無効化がある。依存指定方式を参考にする |
| 同上 | SDL3を使用。nijigenerateも初期移植からSDL3を採用する。D側のSDL2による動作を正としてAPI差分を吸収する |
| `../nicxlive/CMakeLists.txt` | C++20、`nicxlive::nicxlive`、Boostを使用。まずこのC++ターゲットへ直接リンクする |
| `../nicxlive/core/nodes/node.hpp` | `Node`は`enable_shared_from_this`を使用。所有権はこの設計と整合させる |
| `source/nijigenerate/commands/base.d` | `TW`、`ExCommand`、UDA、文字列mixinでフィールドとメタデータを生成する |
| `source/nijigenerate/commands/package.d` | 型別コマンドマップ、自動登録、コンパイル時の登録漏れ検査がある |
| `source/nijigenerate/ext/nodes/exdepthmapped.d` | null深度配列と空配列の差、複製、有限値正規化が意味を持つ |
| `source/nijigenerate/core/tasks.d` | FIFOのFiberをフレームごとに再開する。終了を検知して除去するタイミングも仕様 |
| `source/nijigenerate/core/asyncderivedupdate.d` | 派生更新の観測レジストリでありスケジューラではない。この責務を保持する |
| `dub.sdl` | PSD/KRA、MCP、vibe HTTP、i18n、OSQPなどランタイム以外にもD依存がある |

nijiexposeのアプリ構成は参考にするが、その機能モデルでnijigenerateを作り直さない。
nicxliveの全API差分、各外部ライブラリの完全互換性はまだ未検証であり、先行ゲートで確定する。

## 3. 言語機能の対応を先に固定する

### 3.1 テンプレートとコンパイル時生成

基準はC++20。C++ modulesや将来の標準リフレクションに依存しない。

| D | C++の方針 | 必須確認 |
|---|---|---|
| 型テンプレート、制約、`is` | `template`、concepts/`requires`、type traits | 有効な実体化と無効な実体化をcompile-pass/failで比較 |
| `alias`引数 | 型引数、関数/値の非型テンプレート引数、型付きcallable | 型・値・シンボルを区別する |
| `static if` | `if constexpr`、特殊化 | メンバー構造を変える場合は特殊化。関数内分岐だけでは代用しない |
| `AliasSeq`、`staticMap`、`static foreach` | type list、parameter pack、`index_sequence`、fold式 | 展開順と登録順を保持する |
| CTFE、`enum`定数 | `constexpr`/`consteval`、`inline constexpr` | 実行時状態を暗黙に持ち込まない |
| `__traits(compiles)` | `requires`式と検出traits | 失敗を登録の黙った省略に変えない |
| `__traits(getAttributes)`、`EnumMembers`、型名 | 明示的なtraits・enum記述子・安定した名前テーブル | `typeid().name()`を永続化やプロトコルに使わない |
| 文字列mixin | 明示メンバー＋型付き記述子、必要な小規模マクロ | 任意のDコードを正規表現でC++へ変換しない |
| `mixin template ExDepthMapped/ExDepthOperated` | 状態を持つ補助クラス＋明示転送。必要な場合のみCRTP | 仮想関数、状態共有、初期化、シリアライズの順を保持 |
| `@property`、UFCS | getter/setterメソッドと通常関数 | 代入、副作用、評価順を保持 |
| `opCall/opIndex/opSlice/opBinary`、`alias this` | 演算子、view、明示転送メソッド | 暗黙変換の範囲を広げない |

`requires { T(args...); }`が成立しても、選ばれたconstructorの本体まで実体化可能とは限らない。
Dの`__traits(compiles)`が本体を含めて判定する箇所では、宣言・引数変換の検出と本体の成立条件を
区別する。M085の実constructor判定を固定boolや宣言だけの検出で完了にしない。
未実装の本体判定は、その判定を実際に使う処理の未完了条件として記録し、無関係な移植を止める
共通ゲートにはしない。現在禁止されているcompile-pass/failや検証コード生成も実行しない。

M066のconstructorでfieldへ初めて書く箇所は、通常の`opAssign`と区別して初期化として移植する。
実`ExCommand`の4種のconstructorは、それぞれの実本文と成立条件を持つ同じ
`SourceExCommandConstructorProgram`を使う。値引数は実値を持つフレームへ原本順でコピーし、
本文では名前付きlvalueとして使う。実fieldの初期化は`SourceCommandInitialization`、
構築後の引数更新は`SourceCommandAssignment`へ接続する。原本型独自のcopy/postblitや
初期化処理は定義元の実装を必要とし、未対応型を固定boolで成功・失敗にしない。
基底本文の成立だけでは、Storageのfield宣言・`.init`や派生constructor全体の成立を証明しない。

M066のメニューの`alias id`は、定数値と実行時変数の参照を区別する。原本の式名・key型・
解決先classは実定義のmetadataから受け取り、実行時のID保存先はgetterで直接参照する。
表示用の解決と選択時の実行でID・TLS mapを読み直し、callback前の値を固定しない。
メニューから`cmd`へ渡す引数も原本の名前付きlvalueを保持する。

文字列mixinで生成する処理は、宣言位置に原本の引数・文字列と、実際に移植したC++本文を
組として記述する。任意のDソースを解釈するコンパイラーの追加は、この移植APIの必須条件にしない。
比較演算子の有限表や、本文の代わりの固定boolを汎用実装として扱わない。
M127の`ResourceAttrFilter`では、単一のbool述語で原本を表せない場合に、実receiver、入力配列、
実戻り値を受け渡す`process`全体の型付き本文を記述子に持たせる。`this`、`value`、`target`、
`targets`、`result`への変更、foreachで保持する配列descriptor、条件変換、例外と副作用の順を保持する。
name特殊化の最初の未使用`toStringz/fromStringz`は実行し、後続式のoperandが未評価となる場合に
二度目のgetterを先に呼ばない。メソッドや反復の局所変数を捕捉して外へ出るdelegateには、
実際の移植本文が使う共有cellとclosure ownerを接続し、stack参照の借用で代用しない。
Sourceの型名・ClassInfo同一性は原本の`name`、`T`、`alias op`だけから決める。
移植用本文の関数ポインターやC++型を追加のSource型引数にしない。同じSource実体に異なる本文を
黙って登録せず、宣言と実本文の対応を一意にする。本文の欠落・型不一致は実体化の失敗とし、
既知の演算子へのfallbackや、実現可能と見なすboolへ変換しない。この契約の宣言・全本文・
実consumer・公開header・通常CMakeが揃うまでは、該当原本を完了にしない。

M068の`alias id`は、元の宣言識別子と実値を別々に保持する。値が同じ別名を、enum値から名前を
逆引きするtraitsで同一視しない。`identifier ~ "Command"`の解決は、原本の当該スコープで見える
実宣言・importと対応する型付きresolverへ写す。その実型または確認済みの不在結果を宣言位置で
結び、任意Dコンパイラーの追加を前提にしない。ただし任意の実型や不在flagを差し込むだけの
registryではなく、実スコープとの対応が必要である。C++側の対応が未実装であることを、原本での
宣言不在へ変換してはならない。不在なら原本どおりfalseを返すが、解決後の
`id == BindingCommand.SetInterpolation`はconstructorの成立判定の外で評価し、比較自体の型不適合を
falseへ握り潰さない。constructorの宣言だけの検出や固定16件の述語で全本文を完了にせず、
選択した実constructorとその本文の成立条件を対応させ、実16登録callerも同じ処理へ接続する。

Puppetのtyped `find`五関数は、実クラス階層の暗黙変換を`requires`で表し、
Node/Parameterの型付き結果を実RTTI castへ写す。名前引数は所有するcounted stringを値で受け、
Parameterの名前検索は配列の実descriptorを先に保持してから仮想index検索を呼ぶ既存本文を使う。
UUID検索のResource分岐は、より具体的なNode/Parameter分岐と競合させず、Nodeを先に検索し、
失敗した場合だけParameterを検索する。元の二つの名前検索の既定型を保持し、
戻り値の期待型から独自の優先順位を選ばない。Resourceの本文にある暗黙の戻り値変換を
任意派生型へのcast/null成功へ変更しない。将来の`alias this`等の追加変換は、実宣言の移植時に
その変換を明示して扱う。これらの関数宣言の存在を、M085の本体実体化判定の代用にしない。

### 3.2 UDA・コマンド反射の具体方針

C++の独自`[[attribute]]`は標準的に列挙・取得できないため、UDAの代替にしない。
`CommandTraits<C>`と`FieldDescriptor`の型付き定義を唯一のメタデータ源にする。

1. 各コマンドにDのmixinが生成したものと同じ名前・型の実フィールドを宣言する。
   `TW<T, name, desc, hidden, jsonSchema>`は型、メンバーポインタ、名前、説明、公開属性、schemaを持つ記述子へ写す。
   無名引数の`arg0`等も保持する。tupleのみへ置き換えて既存メンバーアクセスを全面改変しない。
2. `ExCommand<T...>`の引数型列とコンストラクタ規則は、traitsのフィールド列から導出する。
   個数・変換可能性を`static_assert`/`requires`で検証し、ラベル省略時の動作もDと合わせる。
3. `ShortcutVisibility`、`McpExposureMeta`、`GuiDisplayMeta`、`IrreversibleEffectMeta`、
   `CommandScopes`、`PresentationOnly`を明示traitsへ写す。属性なしの場合の既定値と継承探索順も移植する。
4. enumとコマンド型を結ぶ型付き登録表をモジュールごとに設け、明示初期化関数から登録する。
   静的オブジェクトのコンストラクタに登録順を依存させない。型別マップと動的キー生成を維持する。
5. GUI、ショートカット、Command Browser、MCP schemaは同じ記述子を読む。
   記述子と実フィールドの型不一致、ID重複、登録漏れをビルドまたはテストで検出する。
6. Dが返すコマンド一覧をfixture化し、ID、引数順、型名、hidden、schema、scope、公開可否を全件比較する。
   C++の型名を外部文字列として使わず、D互換名を明記する。

最初の試作は引数なし、`TW`引数あり、enum引数、動的キー、スコープ制限、MCP非公開の各例を含める。
この試作が通るまで全コマンドの移植を始めない。生成器を導入する場合も生成物は検証可能にし、
通常のC++ビルドにDコンパイラを要求しない。

### 3.3 メモリ・値・初期値

仮想性は関数名や`final`の有無だけで決めず、宣言位置のアクセス属性を追跡する。
Dの`private`・`package`メソッドは非仮想なので、C++側でも追加の仮想slotを作らない。
Puppetのprivate `selfSort`とProjectableのpackage `registerDelegatedTasks`もこの規則に従う。
規則の根拠は[D言語仕様のVirtual Functions](https://dlang.org/spec/function.html#virtual-functions)。

Sourceの可変classに対するserialize呼出しは、C++でも可変な仮想overloadへ到達させる。
既存のconstなnative APIを維持する場合も、共有class handleを`const T&`へ変換した汎用経路だけで
代用しない。共有writerは実class receiverを保持し、実メンバーの可変性を保存して呼ぶ。
動的配列は実descriptorの値渡しとref要素の反復を保ち、class serialize中の配列差替えで
処理中のbackingを失わないようにする。Native vectorの深い複製はこのSource配列契約の代用にしない。

AutomationBindingの`serialize(S)`は可変なC++ member templateとして公開headerに本文を置く。
汎用Sへは所有する名前descriptorと静的key descriptorを渡す。標準writerのprimitive
`putKey`/`putValue`は`view()`と`retentionOwner()`を持つ実文字列descriptorを値で保持して
既存のbyte出力へ渡す。primitiveの空文字出力と`serializeValue`のnull判定は分ける。
vec2のrangeは数学serialize同様、key出力後に値を捕捉して二要素を原本順で出力する。

Automationのfactory表は既存のcanonical文字列AAを使い、delegateの実callableを一つ保持する。
AA検索ごとに`std::function`のmutable contextを複製せず、実登録callableの所有値を取得して呼ぶ。
TypeIdのcompile-time descriptorは静的bytesとして登録し、可変なnative文字列入力はnative境界で所有する。
この登録方針は、任意の派生型のconstructor本体の成立判定やowner-before-constructorの証明とは分ける。

描画登録の`TaskScheduler`引数は、実際のschedulerを所有する`shared_ptr`の値渡しで
nullとcallback中の寿命を表す。NodeとProjectableには同じ実class上の仮想overloadを置き、
原本の登録本文・子の仮想呼出し・実RenderContextを渡すPost0/1/2を移す。
既存の参照引数をno-delete ownerで包んだものは、この経路の実装とはしない。
子のzSort比較は選択されたPhobosのclass-handle用sort順を保ち、null要素の黙った除外や
release時のassert式による追加callbackを入れない。このAPIの実装と、Puppetの実heap所有・
OneTimeDeformerのoverride・呼出し元の切替は別々に到着を確認する。

Dの`is`、通常のclass `==`、classをキーとする連想配列の比較を区別する。
`is`は実complete objectの同一性を使い、仮想`opEquals`へ転送しない。通常の`==`は実classの
仮想`opEquals`と同一型・異型の規則を、既存の`common::ngClassObjectEquals`で処理する。
連想配列は別の`ObjectClassKeyPolicy`を使い、TypeInfo_Classのhash・比較・引数保持順を保存する。
UUID、privateなpointer比較、別のObjectモデルでこれらを代用しない。

`ngRequireObject(Handle value)`は値を返す。shared handleの評価済みreceiverは、そのtemporaryにより
full-expressionの末尾まで実際に保持される。この実装を確認せず、既に保持されている呼出しに
寿命不足や追加の保持層を仮定しない。getterが返す借用pointerを次のstatementで消費する場合は、
必要な区間だけ元の実receiverを保持する。別objectやGC用の記録で置き換えない。

- D classの参照同一性を保持する。nicxlive Nodeは既存の`shared_ptr`契約に従い、親や観測者には
  必要に応じ`weak_ptr`を使う。Undoが保持する削除済みNodeは復元可能な期間だけ強く保持する。
- 一意所有のGLリソースやUI補助オブジェクトはRAII/`unique_ptr`を基本とする。
  GCを一律の`shared_ptr`置換で模倣せず、循環・イベント購読・callbackの寿命を個別に監査する。
- New/import経路の寿命は所有者の解放で管理する。DのGC専用`incFreeMemory`呼出しはC++へ公開しない。
  `IncImportLayer`は文書単位の所有graphが全layerを一意所有し、内部parent/children/clipping参照は
  graphを強く保持しない。外部descriptorだけがgraphを保持し、最後のdescriptor破棄でlayer、
  retained File、pixel backingを解放する。PuppetからNodeは強参照、Nodeから親/Puppetは弱参照、
  AnimationPlayerのarenaへの逆参照も弱参照である。Newでは原本順のwindow終了、async scope解除、
  active project/player置換、selection/armed/history解除を保持し、生存中の外部Undoやcallback保持は
  勝手に切らない。この監査済み範囲ではRAIIによる解放を使っていたが、下記の追加graphには
  その判断を流用しない。
  Dの`GC.minimize`相当のOSへのメモリ返却はC++ allocatorでは保証せず、一般の循環回収も保証しない。
  GL退役queueのdrainやCRT heap操作を、GC呼出しの代替として追加しない。
  他の呼出箇所・新しい所有graphには個別監査を必須とし、この判断を機械的に流用しない。
  Automationのprivate parentも弱参照へ修正する。原本にparent getterはなく、読出しは
  `reconstruct`から本体が空の`AutomationBinding.reconstruct`へ渡す箇所だけである。
  `finalize`は新しいPuppet引数を強く保持してbindingへ渡す。update/派生automationはこの
  private parentを参照しないため、この逆参照からPuppet所有循環を作らない。
- 2026-10-27の現行原本照合では、`MaskBinding.maskSrc`と`WeldingLink.target`は観測者ではなく
  classを保持する参照である。実装も`ClassReference`で実対象を保持し、Puppetからのdetachだけで
  消滅させない。所有循環は明示的なdetach・購読解除・owner終了処理で解消し、
  真に観測専用の逆参照だけをweak_ptrにする。保持が必要な参照を一律に弱めない。
  C++独自GC・tracing collector・mutator登録・safepoint・遅延回収queueは導入しない。
  2026-10-27のユーザー指示により、それらを要求した#39494の方針と導入済み接続は撤回・撤去対象とする。
  Blob・Mesh・SharedArrayも通常のRAII所有権で確保・解放する。原本にある回収呼出しを根拠に
  C++へcollectorを追加したり、他原本の着手・受入条件にしたりしない。
  C++公開APIにも回収要求オプションを残さない。AutoRig memory statusの
  `collectGarbage`は引数field・JSON schema・constructorから除去し、実native情報だけを扱う。
- D structは値型だが内部の動的配列は共有し得る。通常コピー、`.dup`、`.idup`を区別し、
  所有バッファとviewの対応を明記する。`std::vector`の深いコピーを無条件に適用しない。
- nullと空を区別する箇所は`optional<vector<T>>`等を用いる。共有sliceが必要なら所有者を保持する
  専用sliceを用い、`span`の寿命切れを避ける。添字・切り出しの範囲検査を維持する。
- Dの浮動小数点既定初期値はC++のゼロ初期化と異なる。各フィールドの`.init`、明示初期値、
  NaN/Infの扱いを棚卸しする。整数幅、符号、変換、丸め、オーバーフロー、`size_t`も確認する。
- 連想配列の検索は`find`等を使い、C++の`operator[]`による意図しない挿入を避ける。
  順序に依存する処理はD側の実際の保証と使用箇所を調べ、勝手にsortしない。
- 文字列はUTF-8とし、Dのコードポイント反復とバイト反復を分ける。Windowsパス境界は明示変換する。
- `delegate`は寿命が明らかなlambda/`std::function`へ写す。`const`/`immutable`/`inout`の
  推移的な読み取り制約をC++の浅いconstだけで満たしたことにしない。
- `scope(exit/success/failure)`はC++20用scope guard、`assert/enforce`は同じ失敗条件を持つ検査へ写す。
  `@safe/@trusted`は境界監査、`pure/@nogc/nothrow`は契約と検証へ写す。`noexcept`の付与は個別判断する。
- Dのmodule constructor/destructor、TLS、`__gshared`は明示初期化/終了、`thread_local`、共有状態へ分ける。

Commandの共変戻り値は、`shared_ptr<DerivedResult>`をC++のvirtual overrideへ直接返せない。
型付きメソッドを`runTyped`へ改名するだけでは元の静的`run` APIを満たさないため、実Concrete型を
使うheader-onlyの`CovariantCommand`をExCommandとの間に置く。Commandの既存virtual
`run(shared_ptr<Context>)`は変更せず、この実bridgeがConcreteの型付きvirtual
`run(const shared_ptr<Context>&)`へ接続し、同一Result objectを基底参照へ返す。
Concrete側は実際の`shared_ptr<Result>`戻り値を宣言し、既存の単一`runTyped`本文へ渡す際に
Contextを値として保持する。関数ポインタ型をcompile-timeで検査し、宣言tagだけの偽対応や
継承されたbridgeへの自己再帰を認めない。通常の`enable_shared_from_this`と局所強参照で
callback中の実receiverを保持する。別Command object、Result wrapper、GC、SDK変更は導入しない。

`__traits(compiles, registerCommand...)`の登録可否は、呼出しsignatureだけの`requires`では判定しない。
metadata本体と登録側が同じcompile-time domainを参照し、元のscope数制限、実scope型の継承・
構築・型名・実AA代入、および空scope宣言と既定normalの分岐を含める。実constructorと実登録先
への代入も確認した上で、guarded登録は未対応時に生成・metadata登録・AA代入を一切行わない。
明示登録は同じdomainを満たす場合だけ実factoryへ接続し、未対応定義はコンパイル上のエラーとする。
これは対応済みnative providerの規則を共有するAPIであり、任意のC++関数本文のコンパイル可否を
signature検査で推測する仕組みとはしない。既存`Command/ExCommand`のABI・通常登録APIは保持する。

`SimplePhysicsFloatCommand`の文字列mixinは、6引数を受ける実templateと、実`ExCommand`型を
公開する`type`へ対応させる。生成される`value` field、引数metadata、constructor、run、属性を
template本文として実装する。元からある7つの名前付きclassの識別は維持し、そのrunも同じ
template処理へ接続する。ClassNameは静的な元の完全修飾名として保持し、C++の識別子を文字列から
動的生成する仕組みやDの任意syntaxの評価器を追加しない。対象取得は既存の実providerだけを呼び、
そのcallbackの後に現在のvalueを読む。Contextと実target descriptorはAction登録が終わるまで保持する。

Dのconstructor本文から仮想メソッドを呼ぶ場合は、C++の基底constructor内にその呼出しを置かない。
実際の最派生型を通常の強い所有権で生成した後、同じconstructor本文を一度だけ実行する論理初期化へ
対応させる。`SimplePhysicsValueChangeAction`は実型を生成するfactoryから、元のfield代入・配列複製・
Getter走査・仮想`redo`をこの順で呼ぶ。すべての実呼出箇所を同じfactoryへ接続し、別Actionや
仮想呼出しの代役callbackは作らない。派生型のD constructor本文も同じ論理初期化の規約で移植する。
未移植の任意C++派生constructorや任意型の初期値まで自動的に互換となったとは扱わない。

M083のnew式内の引数callbackは、同じ実Actionの確保・所有確立後、constructor本文の前に評価する。
既存factoryへ同期的なtuple引数producerを渡し、producerは保持せず、その場で一度だけ呼ぶ。
config3種とfloat共通runはこの経路を使う。D原本でnew式の前にtargetsを取得するSet/Clear Parameterは、
その原本の順序を維持する。引数callbackやconstructor例外をメモリ確保例外へ置き換えない。

`SimplePhysicsValueChangeAction`の関数aliasは、実関数ポインタと元宣言の識別子を持つ
structuralな型付きcallableを非型template引数として渡す。識別子は各実関数の宣言元で付け、
ポインタ値から名前を逆引きする表や既知の12組限定の処理は作らない。関数呼出しは同じ実本体へ
直接転送する。型引数Tは正規のSource型表記、関数aliasは元識別子、rescanは実bool値から、
同じgeneric ActionのClassInfo名と4型表記を組み立てる。未対応のC++関数ポインタへ名前を推測して
割り当てない。これはD宣言のC++表現であり、Dコードの評価器やコマンド型名検索の代用ではない。

動的ClassInfo名はC++の`typeid(...).name()`から推測しない。移植したclass宣言の元の完全修飾名を、
実型をkeyとする既存の正規ClassInfo供給へ登録する。組込みの既知class以外も同じ供給から取得し、
カタログにないことだけを理由に実型を拒否しない。文字列mixinの実型と既存の名前付き実型は、
同じ元宣言から作るmetadataをそれぞれの正しい型へ公開する。未対応の任意native classへ、基底classの
名前やC++のmangled名を代用しない。

数値型の対応は関数の引数・戻り値だけで決めない。定数、暗黙の型変換、テンプレート実体化、
標準ライブラリ内部の中間値まで追跡し、演算ごとの型・評価順・丸め位置を対応表に記録する。
構成上Dの`real`が80ビット拡張精度を使う箇所では、C++の`long double`という型名だけで
同等と判断せず、対象コンパイラの実表現を確認する。`double`版の`pow`や正規化であっても、
呼び出す実装が内部で`real`へ拡張する場合は、その経路を含めて移植する。

必要な拡張精度演算は共通の数値実装に集約し、CPU固有の命令や浮動小数点環境へのアクセスは
platform境界に限定する。整数による演算・丸めの再現は候補となるが、値の一致だけでは
FP例外フラグ、トラップ、非既定の丸め・精度モード、非正規化数まで一致したとはしない。
`sin`、`cos`、`pow`、`sqrt`は原構成の実際の呼出し先と中間精度を確認し、別の数学関数へ
黙って置換しない。未解決の数値実装に依存する本体は未完了として管理する。
検証はこれらを使うモジュールをまとめたプロジェクト単位で行い、関数ごとのビルドを増やさない。

### 3.4 Fiberとスレッド

#### 3.4.1 実行領域と現行コードの対応

Fiberによる協調実行とOSスレッドによる並列実行を分離する。以下のパスは`source/nijigenerate/`基準。
単一の汎用thread poolへ統合せず、まずDの実行領域、順序、待機条件を保持する。

| 対象 | 現行の実行形態 | C++移植方針 |
|---|---|---|
| `core/tasks.d` | UI更新からFIFO先頭のFiberだけを再開 | UI専用stackful scheduler。1更新に1回の再開またはTERM除去を行う |
| `core/math/path.d`、`core/math/skeletonize.d`、`core/cv/contours.d` | 通常関数の深い呼出し位置でyield | 呼出しスタックを保存する共通Fiber adapter。数値ループとyield位置を保持 |
| `viewport/vertex/package.d` | ローカルFiberをTERMまで反復再開 | 同じ呼出しスレッド上で完了させる。yieldを自動的なUIフレーム待ちに変えない |
| `commands/automesh/dynamic.d` | worker内Fiber、alpha入力cache、UIへ結果をenqueue | CPU入力を先に確保し、worker専有計算とUI適用を分離。結果適用とaction group終了のキュー順を保持 |
| `windows/automeshbatch.d` | worker内Fiber、mutexでstatus/mesh/cancel管理、終了後joinとtexture unlock | バッチ所有のjobと同期snapshot。textureの寿命・unlockはUI側で管理 |
| `autorig/framework.d`、`panels/autorig.d` | worker内Fiber、atomic cancel、同期的なeditor dispatch | session/job所有状態、UI往復、taskごとのaction boundaryを保持 |
| `api/mcp/task.d` | mutex付きUIキュー、通常threadはCondition待機、vibe taskはyield待機 | UI dispatcherと要求単位completionを用意し、OS thread待機とevent loop待機を分離 |
| `api/mcp/http_transport.d` | vibe event loop、Fiberをキーとするresponse slot、SSE | event loop専有のrequest contextと非同期応答。UI schedulerとは別系統 |
| `api/acp/client.d`、`panels/agent.d` | pipe読取・stderr読取・agent workerとmutex/Condition | join可能な専用threadと所有キュー。I/O解除と終了通知を先に設計 |
| `core/asyncderivedupdate.d`、GPU派生更新 | 観測状態とGPU完了の管理 | 観測レジストリをworker schedulerにしない。UI上でgeneration/action tokenを照合 |

UI、ImGui、live modelの変更、Undo、GL contextに属する操作とリソース破棄はeditor threadに限定する。
workerはコピーしたCPU入力または寿命と排他が証明された入力を使い、結果をUIへ渡す。
既存のworkerがNode/processorを捕捉する箇所は、全メソッドをthread-safeと仮定せず、読み書きする
フィールドまで追う。snapshot化で参照同一性や結果が変わる場合は契約差分を記録する。
上表は調査した境界であり、全モジュールの同期監査が完了したという意味ではない。

#### 3.4.2 stackful Fiberの採用と制約

深い通常関数からのyieldを保持するため、Boost.Contextをstackful adapterに使用する。
C++20 coroutineはstacklessであり、呼出し鎖の変換なしにそのまま代替できない。
`std::async`、OS thread、thread poolへの置換も実行順・TLS・GL所有権を変えるため行わない。
2026-10-25の現行`native/src/nijigenerate/core/tasks.cpp`にはBoost.Contextによる実装がある。
既存の[D10契約](native/port/wbs/D10/fiber-contract.md)にはWindowsでの23レコード比較が記録されているが、
worker内Fiber、HTTP、全OS、終了時unwindまで検証済みと解釈しない。

- Fiberの作成・再開・TERM後のstack解放を同じ所有OS threadへ固定し、thread ID検査を設ける。
  他threadへの移送、同一Fiberの同時再開、実行中の再入は禁止する。現行adapterへの追加検証事項とする。
- UI schedulerは終了したフレームには除去しない。次の更新で先頭TERMを除去し、その更新では
  次のFiberを開始しない。実行中のappend、例外伝播、status/progressもD10に従う。
- worker内の`while (!TERM) call()`は協調チェックポイントであり、各yieldでUIへ制御を返す契約ではない。
  cancelを観測する位置と途中結果の扱いを呼出し元ごとに保持する。
- スタック容量は通常呼出しとDで`defaultStackPages * 4`を指定する処理を区別する。
  DとC++で同じバイト数が十分とは限らないため、深い輪郭処理・骨格化・AutoRigで上限を試験する。
- 通常例外は`exception_ptr`で所有callerへ伝え、worker入口で未捕捉のままthreadを終了させない。
  Boostのforced unwindを`catch (...)`で握り潰さない。adapter以外の広いcatchも監査する。
- Fiberはschedulerと実行中の強local ownerが通常のsmart pointerで保持する。Implの自己保持循環や
  GC用stack rootは設けない。終了・取消は明示的な所有者側の処理へ接続し、Boost forced unwindを
  握り潰さない。通常完了・例外・中断を区別し、将来の実行検証はソース実装とは別工程とする。
  中断stackの私的cleanupからUIへの同期dispatchを行わせない。
  cleanupは所有threadのCPU資源解放と明示的なUI終了処理へ分け、Undo終了やtexture unlockを一度だけ行う。
  UI schedulerには既存契約にないcancel APIを追加しない。worker jobのcancelとは区別する。
- mutex保持中にyield、UI同期dispatch、callback呼出し、join、長時間I/Oを行わない設計にする。
  Dのmonitorが再帰取得される箇所は実呼出し鎖を調査し、`std::mutex`へ機械置換しない。

Boost.Contextのスタック破棄と例外制約は[公式Fiber仕様](https://www.boost.org/doc/libs/latest/libs/context/doc/html/context/ff.html)
を参照する。採用バージョン・backend・コンパイラ・stack設定を固定し、Windows/macOS/Linuxで
再開、例外、RAII、shutdownを先行試験する。不成立なら明示状態機械を別途設計する。

#### 3.4.3 dispatch、応答、終了のプロトコル

`api/mcp/task.d`のキューはmutex下で全体を取り出して元のキューから切り離し、lock解除後に実行する。
dispatch中に追加された項目は次のdrainに残す。C++でもこのbatch境界とenqueue確定順を保持する。
複数producer間にDで定義されていない絶対的な実行順を新設しない。

要求は値を所有する`RequestState`として、request ID、project/session generation、結果、例外、
完了状態を持つ。非同期lambdaの`[&]`によるローカル参照やHTTPスタック上の応答先をキューへ保存しない。
通常workerの同期呼出しはmutex＋condition variableのpredicate待機で、結果と例外を同じlockで公開する。
呼出し側に存在するUI直接実行分岐は、その呼出し側で保持する。M029の`ngRunInMainThread`自体は
原Dで常にenqueueして待つため、内部にUI thread判定や直接実行を追加しない。UI threadからの
呼出しを安全化したと主張せず、呼出し側の分岐と実行領域を別途検証する。
HTTP event loop側はcompletionをloopへpostして再開し、`future.get()`やcondition waitでloopを止めない。

現在のM029実装は共有wait stateに結果・例外・完了を保持し、実行中の`TaskFiber::Impl`の
`Kind::vibe`（互換名`mcp`）でのみstackful yieldする。分類はFiber自身に保持し、yield中も残る独立TLS boolでは
代用しない。通常workerには`ManagedThreadScope`を設け、mainは実際の`ngCreateContext`入口で
明示登録する。登録前の判定と別threadからの再登録は検出する。最初の呼出threadをmainと推測しない。
MCP server/HTTP入口からのMCP-kind生成・イベントループ再開は別の未完了統合範囲であり、
このwait処理のソース実装をもってHTTP実行を検証済みとはしない。

AutoMeshのC++ jobはjoin可能なworkerを所有し、`ngFinalize`のGL破棄前に受付停止、cancel、
内部UI queueのdrain、完了callbackでのjoinを行う。結果適用、action group終了、popup終了の
元キュー順を保持し、未完了workerをdetachしない。Windowsでの実終了動作は全ソース実装後に検証する。

MCPの通常設定stopは原本の40回×50ms待機と、期限後にも生存中server/transportを保持する動作を残す。
プロセス終了時は別の`ngMcpShutdown`をUI破棄前に呼び、transport受付停止後にもmain-thread queueを
drainして保有workerをjoinする。Dのdaemon終了時放棄をC++のjoinable thread破棄やdetachへ置き換えない。
HTTP実providerの終了通知とFiber再開が到着するまで、この終了経路全体を動作確認済みとはしない。

Fiber IDに依存していたresponse slotはrequest contextに結び付け、SSE接続はevent loopが所有する。
成功・例外・timeout・切断・shutdownの終端を一度だけ確定し、遅延応答は終端状態を確認して破棄する。
timeoutを「UIで開始済みのコマンドがrollbackされた」と扱わない。通知の204、応答対応、SSE順序と
既存timeout値は別途D fixtureで固定する。HTTPライブラリの選定だけでこの契約を満たしたことにしない。

終了は次の依存順序を明示する。`std::jthread`を基本とするが、そのデストラクタの自動joinだけに任せない。

1. 外部要求と新規job受付を停止し、sessionの終了を記録する。
2. cancel/stopを要求し、Condition待機を通知する。pipe/socketのblocking readも解除できる経路を用意する。
   `stop_token`だけでI/Oや任意のCondition待機が解除されるとは仮定しない。
3. workerが待つ内部UI dispatchは処理または明示的な失敗完了にして、すべての待機元を解放する。
   `panels/autorig.d::stop()`のqueue処理後joinという依存を保持する。UIが先にjoinすると相互待ちになる。
4. job/Fiberを所有threadで終端化し、workerとI/O threadをjoinする。detachを通常終了の代用にしない。
5. 未適用結果を無効化し、UI上でaction group、texture、model、GLを依存順に破棄する。
   GPU実行中資源はGPU完了も確認し、CPU threadのjoinだけで破棄可能と判定しない。

### 3.5 DとC++の共有メモリ・TLSの対応

#### 3.5.1 宣言の変換規則

Dの通常の可変module変数・static変数は原則thread localだが、C++のnamespace変数・通常のstaticは
thread間で同一storageを共有する。この差を落とすと、移植後に初めてdata raceやcache混線が生じる。
一方、Dのclassインスタンスfieldやdelegateが指すheapデータは、参照が渡れば同じ実体へ到達する。
TLSの参照変数を持つことと、その参照先がthread専有であることは同義ではない。

共有JSONのC++搬送でも、moveをDの参照破棄へ読み替えない。
`SettingsJsonObject`と`SettingsJsonValue`は、copyとmoveの双方で実際の共有descriptorを保持し、
元の値もその共有先を参照できるようにする。親のJSON値をその子で置換する代入では、
子の全descriptorを先にsnapshotしてから親の旧保存領域を置き換える。
std::variantやshared_ptrの既定moveで元を空にしたり、途中の旧owner解放後に子のfieldを読み直したりしない。
このsource実装とthread間同期の実装は区別する。GCはC++移植の共有前提にしない。

Fghjの`byElement`・`byKeyValue`は、既存の`TypedJsonView`が保持する実JSON ownerとcursorを使う
入力rangeへ対応する。range・iterator・save・copy・moveは実descriptorを保持し、要素vectorや
全keyの複製、代替JSON graph、ownerだけの箱を生成しない。memberのkeyは元の数えたbytesを保持し、
原本の文字列AA readerと同じく、到達した値の変換成功後に必要なkey複製を行う。
missing・異なるkind・空入力・frontのassert・popFrontの境界も原本に合わせる。
既存のnative `elements()`・`members()`との区別は
[要素range](native/port/contracts/sdk-fghj-element-range-source.md)と
[member range](native/port/contracts/sdk-fghj-member-range-source.md)に記録する。
この接続だけで既存AnimationMapの順序・容量・key storage、raw binary Fghj、ABIの同等性を
完了にしない。

宣言規則は[D Attributes](https://dlang.org/spec/attribute.html)、型修飾は
[D Type Qualifiers](https://dlang.org/spec/const3.html)を参照する。

| Dの状態 | C++への対応 | 注意事項 |
|---|---|---|
| 暗黙TLSの可変module/static変数 | 原則`thread_local`、または明示`ThreadContext`へ移す | 共有化する場合は全callerの所有threadと意味の同等性を証明する |
| `shared T` | 単独flag/counterは`std::atomic<T>`、複合状態はmutex付き所有object | `shared`はatomicやlockを自動挿入する意味ではない。実際のDのatomic/lock操作を写す |
| `__gshared T` | process共通のstorage＋所有thread制約またはmutex | 共有storageであるだけでthread-safeではない。通常globalへ写して終わりにしない |
| `immutable` | 定数は`constexpr`、動的入力は公開後不変の所有snapshot | 到達先を含めた不変性と安全な公開を保証する |
| `const`/`inout` | const viewと明示所有権 | C++の浅いconstや`shared_ptr<const T>`だけでは内部pointer先の可変性を排除できない |
| `static this/~this` | thread開始/終了の明示初期化・解放 | C++の関数local staticの一度だけの初期化とは異なる |
| `shared static this/~this` | アプリ全体の明示init/shutdown | worker開始前に登録を完了し、join後に解放。翻訳単位の初期化順に依存しない |

C++にはDの`shared`に相当する一般的な型修飾がないため、公開APIをeditor専用、worker専有、
immutable snapshot、同期済み共有objectに分け、内部fieldへ無制限にアクセスできないようにする。
`volatile`、`shared_ptr`の参照カウント、関数local staticの初期化保証は、payloadへの競合アクセスを防がない。
同じ`shared_ptr`変数の読み書きと、別々の`shared_ptr`が指す同一objectへの読み書きも区別する。

#### 3.5.2 現行状態への適用

- `core/tasks.d`の`__gshared tasks/status_/progress_`はeditor専有。全fieldをatomicにせず、
  UI thread検査と他threadからのdispatchで保護する。
- `api/mcp/task.d`の`gQueue/gQueueMutex`は真に共有する。worker公開前に初期化し、
  無同期のlazy初期化をC++へ持ち込まない。TLSの`externalCommandsBlocked`はUI gateとして扱い、
  workerが自分のTLSコピーを読むことで許可判定を迂回しないAPIにする。
- `viewport/vertex/automesh/common.d`のalpha入力cacheと`commands/automesh/dynamic.d`の
  set/clearはworker内の呼出し連鎖を監査する。TLSで移す場合はscope guardで以前のcacheを復元し、
  入力所有者がFiberより長生きすることを保証する。job終了後にpointerを残さない。
- `core/asyncderivedupdate.d`の`runRecords/targetRecords/workRecords`とID counterは暗黙TLS。
  初期移植では分離を保持し、UIへ集約するなら全callerがUI専用である証拠を契約に追加する。
  観測結果の更新はUIへdispatchし、workerごとに別のレジストリを誤って作らない。
- `autorig/framework.d`の`shared cancelRequested`とnative blob counterはatomic操作を対応させる。
  sessionのmap/JSON/snapshotはatomic counterでは保護されないため、既存monitorとコピー境界を保持する。
- `windows/automeshbatch.d`のcancel、status、meshと`api/acp/client.d`のinbound queueは、
  全read/writeを同じ同期規則で覆う。Dに無同期アクセスがあれば移植差分として記録し、C++のraceを容認しない。
- UI singleton、command scope stack、GPU pending jobなどの`__gshared`はeditor所有とし、
  起動後読み取り専用の登録表とは区別する。GL handleの最終解放がworkerの参照カウント減少で起きないようにする。

OpenGL移植では状態ごとに次の区別を保持する。`runtime_state`のviewport stack、camera stack、
clear color、およびOpenGL runtimeのscene target、post-processing stackはthread localとする。
一方、原本で`__gshared`のPart/Mask shader、uniform location、drawable bufferと共有IBO管理表は
process共有のままとし、GL contextを所有するrender threadだけが操作する。これらを一律にTLS化したり、
通常のC++globalへ一律に共有化したりしない。Fiberを別threadへ移す場合、保存したGL handleだけでは
contextの所有権は移らないため、そのFiberから直接GLを呼ばずrender threadへ要求を渡す。

`DynamicCompositePass`は原本では参照objectであり、beginで保存したframebuffer/viewportをendが読む。
C++の値objectでは、同じpassを可変参照でbegin/endへ渡し、描画lambdaが保持するpassも可変にする。
`const_cast`やbeginだけの一時コピーでこの共有状態を代用しない。OpenGL backendの選択時には、
Puppetが実際に使うcommand emitterの生成まで接続する。backendの仮想関数を実装しただけで、
描画経路が接続済みとは判定しない。GPU resourceの解放はcontextが生存する所有threadで明示的に行う。

時刻providerのcurrent/last/delta timeと時刻callbackも、原本の通常module変数に対応するTLSとする。
動画出力が固定刻みのcallbackへ切り替えても別threadの時計を書き換えない。process共通のmutexで
共有化すると意味が変わり、callback内から時刻getterへ再入した場合のdeadlockも生む。
初期化時の値resetや未設定callbackの代替時計は原本にないため追加しない。

TLSはOS thread単位でありFiber単位ではない。同一threadのFiber同士はTLSを共有する。
request ID、jobのcancel、alpha cache等がyieldを跨いで異なる値を必要とする場合は`TaskContext`を
引数で渡すか、resume/yieldの境界でcontextを退避・復元する。単に`thread_local`へ置くだけでは不十分。
現行`TaskFiber::Impl::active`のTLSは「現在実行中のFiber」の入口管理であり、job全体の保存領域ではない。
将来thread poolを導入する場合も、worker TLSがjobを跨いで残る問題を別途検証する。

例外の保存領域もOS thread単位で監査する。現在の独立したLDC 1.41の
`core/exception.d:870–907`では、`staticError`が各Error型で同じ256-byteのTLS領域を再利用し、
同じ領域へ実体を再構築する。型ごとのsingletonやthrowごとの新規allocationでは、保持済み参照、
派生型のpayload、別型のErrorへの再利用を同じ意味にできない。同一OS thread上のFiberもこの領域を共有する。
`__switch_errorT`もこの経路を使うため、現在のM140の`SwitchError`クラス・message・file・lineの一致だけでは
M074の不正enum経路を含む移植全体の完了とはしない。正規の共有実装は前提#39499としてroot19が担当し、
人間の明示承認を受け、正規SDKの`Throwable`は共有Stateを保持し、nativeのthrow/catch用コピーも
同じ`msg/file/line/info/next/bypassedException`を参照する構成へ適用済みである。
これは共通fieldの参照同一性とtrace解放の所有者を揃える変更であり、派生payloadの型変更を伴う
TLS再構築はまだ実装していない。#39499を単にWindowsで未検証という扱いへ置き換えない。

同期前提#39498の正規SDK `compat/source_mutex`は、Windowsでは`CRITICAL_SECTION`、
POSIXではrecursive `pthread_mutex`を保持する。`SourceCondition`も同じ`SourceMutex`を保持し、
原本のWindows semaphoreによる待機・通知とPOSIX conditionを実装する。
通常の`nicxlive_common`ターゲットへ公開済みであり、M017とM320の実consumerを接続中である。
M320原本にはCondition待機がないため、利用していない待機機構を追加しない。
原本のmanual lock/unlockは失敗時の書込順も保持する。unlockでErrorが送出され得る経路を
暗黙に`noexcept`のRAII destructorへ移し、`terminate`へ変えてはならない。
実際の呼出し接続、Error保存領域、後続のWindows検証を確認するまで全体完成とは扱わない。

threadとFiberの寿命・待機・終了処理はC++の通常所有権と実際の同期APIで構成する。
GC用のthread登録・協調停止・safepoint・blocking scopeは撤去し、再導入しない。
既存のThrowable共有State、OS thread終了時のSourceError TLS解放、承認済みMutex/Conditionは
例外・同期の実機能として維持し、GC専用接続と混同しない。

#### 3.5.3 公開・可視性・寿命

異なるthreadの競合アクセスにはhappens-beforeを設ける。C++のdata raceは未定義動作なので、
Dで偶然動いた読取順やsleepによる待ちを根拠にしない。
規則は[C++ draftのdata race規定](https://eel.is/c++draft/intro.races)を参照する。

- 基本はmutexで入力/結果/進捗をまとめて移送する。producerのunlockとconsumerのlockが公開境界になる。
  completionの`done`、結果、例外は同じ保護範囲に置き、predicateを確認して待つ。
- Dのatomic操作はまず同等の順序を保ち、指定のない箇所は`seq_cst`を初期方針とする。
  release store/acquire loadで結果を公開する場合、初期化完了後に一度公開し、読者が保持する間は
  payloadを変更・解放しない。flagだけatomicにして同時更新中のvector/JSONを読むことは禁止する。
- `relaxed`への弱化は他データの公開に使わない独立統計値等に限り、根拠を台帳化する。cancel受信は
  worker終了の証拠ではない。結果回収・解放にはcompletionまたはjoinが必要になる。
- 配列slice、`span`、raw pointer、delegate captureは、キュー、停止中Fiber、UIへの遅延適用までを
  含めて寿命を追う。所有バッファのmoveまたは不変共有snapshotを使い、可変aliasを残さない。
- `weak_ptr`のlock成功はprojectが現行である保証ではない。結果適用直前にproject/session ID、
  generation、対象ID、Undo action tokenを再確認し、削除・再読込・Undo後の古い結果を適用しない。

### 3.6 並行処理の移植ゲート

実装時は各対象の契約に「D宣言、実際のread/write caller、所有thread、同期手段、公開点、
yieldの有無、captureの寿命、初期化/終了順、C++対応、差分」を記録する。
`shared`/`__gshared`検索だけでは不十分で、暗黙TLS、通常field、static local、callbackも含める。
既存D10の完了を全並行処理の完了扱いにせず、該当WBS葉の追加契約・結合検証として追跡する。

| 検証 | 必須シナリオ |
|---|---|
| Fiber | FIFO、TERM除去フレーム、深いyield、ネスト、append、例外、RAII一度だけ、所有thread違反、stack容量 |
| TLS/共有状態 | 2 threadの暗黙TLS分離と共有状態の共通性、同一threadの2 Fiberのcontext復元、cache clear後の次job |
| dispatcher | 複数producer、drain中enqueue、UI直接実行、worker待機、例外通知、event loopが他要求を処理可能 |
| cancel/寿命 | yield前後、UI結果待ち、結果適用直前、project close/reload、Undo/Redo、遅延応答、timeoutと完了の競合 |
| shutdown | UI dispatch待ちworker、pipe読取待ち、HTTP/SSE接続中、終了中enqueue、join後callbackなし、GLより先にjob解放 |
| メモリと競合 | 利用可能な環境でTSan、ASan/UBSan、長時間の反復開始/取消でstack・capture・bufferの解放を確認 |

同期テストはbarrier/latchや制御可能なcompletionで順序を再現し、sleep時間に依存させない。
Fiber backendとsanitizerのstack切替対応も確認し、非対応環境の無警告を合格証拠にしない。
三OSのDebug/ReleaseでDとの状態遷移比較と統合試験を行い、未検証OSや既知差分を明示する。
本節の追加は設計のみであり、追加検査・同期実装・試験を実施済みとはしない。

#### 設計追記の受入証拠と後続への引渡し（2026-10-25）

3.4〜3.6は現行Dのscheduler、AutoMesh、AutoRig、MCP dispatcher/HTTP、ACPの実行境界と、
現行C++の`TaskFiber`を突き合わせて追加した。受入対象は実行領域の対応表、TLS/共有状態の変換規則、
同期と終了のプロトコル、検証ゲートの定義であり、並行処理全体の実装完了ではない。
文書の見出し、コードフェンス、D10契約へのローカルリンクを確認した。ソース変更・ビルドは行っていない。

ローカルKanboardの現行DBを参照ID指定で読み取り確認した時点では、`D10.C`はDone（admin）、
`B025`はDone（sol-metadata）、`D21.C`はReview（sol-metadata）、`B033`と`B041`はBacklogだった。
`tasks.md`のB025はIn Progressであり、この確認時点ではボードとの差がある。
他担当のカード状態と共有WBSはこの設計追記では変更しない。

後続担当はD21.Cの受入時に3.4.3の要求寿命・loop非阻塞・shutdown条件との対応を提示し、
B033/B041では3.4.2の同じFiber契約を各OSで検証する。B025/D10.CのDoneはその既存受入範囲に限り、
3.6のTLS分離、worker取消、project破棄、I/O解除の結合試験を代替しない。
暗黙TLSと全callerの監査は各実装葉で行い、未監査の共有化や未検証のOSを完了扱いにしない。

## 4. 構成とnicxlive境界

```text
native/
  src/app.cpp
  src/nijigenerate/<Dと同じ相対パス>.hpp/.cpp
  src/njc/<Dと同じ相対パス>.hpp/.cpp
  compat/                 # D固有の値・slice・scope・Fiber等の狭い互換層
  runtime/                # nicxliveとのAPI差分だけを扱うアダプタ
  tests/                  # Dのテスト対応、差分runner、fixture
  port/                   # 参照hash、シンボル対応、依存API差分、検証結果
cmake/                    # 依存、埋め込み資源、各OSの構成
```

`package.d`は対応する`package.hpp`と必要な初期化用`.cpp`へ写す。Dの論理package namespaceを維持する。
テンプレートの定義は`.hpp`または付随`.inl`へ置く。Dのmodule-privateはC++のnamespaceだけでは
再現できないため、`.cpp`内部やdetail領域へ制限する。ファイルごとの対応は`tasks.md`で追跡する。
現在の`source/`、`dub.sdl`、Dテストは比較のため残す。

nicxliveはC++ APIを使用し、アプリ内部をUnity用C ABI経由にしない。
`native/runtime`は名前・型・API差異の薄いアダプタに限定し、第二のランタイムを実装しない。
以下はD側のimport/使用シンボルから全件抽出して「同等確認済み／変換必要／不足／未検証」に分類する。

- Node派生、Puppet、Param/binding、mesh、transform、texture、animation、physics。
- 派生Nodeのfactory/登録、virtual hook、filter、dirty/cache、描画queueと更新順。
- `.inx`拡張情報と`.inp`出力、serde、未知キー・欠落・null・数値型の扱い。
- Grid/Path/DepthBone、座標変換、GPUパケット、readback、非同期失効と破棄。

`ext/nodes/exdepthbone.d`等のエディタ固有機能はnijigenerate側に保つ。
nicxliveのNode名が似ていても等価と仮定しない。依存の不足をno-opや近似で埋めて完了扱いにしない。
隣接nicxliveの修正が必要になった場合は差分仕様を先に記録し、変更権限と依存固定を別途扱う。

## 5. 外部依存とビルド

### 5.1 プラットフォーム非依存を原則とする

HTTPサーバ・クライアントを含め、アプリケーションコードのプラットフォーム依存はほぼ排除する。
C++標準ライブラリとWindows/macOS/Linuxを共通に扱うライブラリを優先し、OS固有APIの直接利用は
OS固有機能を実現するために不可欠な最小部分だけに限定する。この原則は依存ライブラリの選定条件とする。

- HTTP/JSON-RPC、接続管理、タイムアウト、キャンセル、非同期I/Oは共通の通信基盤で実装する。
  OS別HTTPサーバや独自のWinSock/POSIX socket実装を並立させず、MCPサーバとnjcクライアントで
  共通のtransport方針を使う。認可、要求処理、エラー変換、task管理はOS非依存層に置く。
- スレッド、同期、時刻、ファイル操作は標準ライブラリを優先する。Fiber、プロセス起動・pipe・stdio、
  ファイル監視等は必要に応じクロスプラットフォームライブラリを使い、OS別実装をアプリ各所に持たない。
- window、入力、clipboard、DPI等はSDL3等の共通APIを優先する。
  ネイティブdialog、portal、OSの保存先規約、アプリ統合など、共通APIでは要件を満たせない箇所だけを
  狭いplatform adapterに隔離する。パスの文字コード変換もこの境界で扱う。
- 必要なOS依存コードは `native/platform/` とOS別CMake・配布資源に集約する。
  共通ヘッダにOSヘッダやネイティブhandleを公開せず、業務ロジックにOS判定や条件コンパイルを散在させない。
  platform adapterの各例外には必要性・代替不能な理由・共通契約・対象OS・検証方法を記録する。
- ライブラリ内部のOS backendは許容するが、アプリ側にその差を漏らさない。
  特定OS専用サービスやツールをHTTP等の共通機能の必須依存にしない。

D版のプラットフォーム別実装も、外部動作を維持したうえで共通実装へ集約する。
この集約は1:1移植の例外として対応台帳へ明記し、元の各OS分岐と共通実装の対応を追跡する。
共通化に伴ってエラー条件、タイムアウト、終了・cancel、文字コード等を変えない。
三OSで同じtransport・ロジック試験を実行し、不可避のOS固有機能だけ追加のOS別試験で確認する。

### 5.2 依存とビルド構成

| 現行依存/機能 | 初期移植の方針 |
|---|---|
| nijilive | nicxliveのローカルパスをCMake cache変数で指定。全API互換を先行検証 |
| SDL2 / i2d-imgui / OpenGL | C++側はSDL3と対応Dear ImGuiを固定。D wrapperとbackendのSDL2 APIをSDL3へ対応付け、独自backendの外部動作を保持 |
| inmath / mir ndslice | nicxliveの数学型との変換とstride付きview。座標・精度・aliasingを数値テスト |
| psd-d / kra-d | C++代替の互換性は未確定。ローカルD実装を基準に必要機能を移植するか、同等性を実証した実装をadapter化 |
| std.json / fghj / mir.serde | 型付きJSON adapterを定義。整数/浮動小数、null、欠落、配列、エラーを保存。具体ライブラリはfixtureで選定 |
| MCP / vibe HTTP / ACP | 現行のJSON-RPC、HTTP/stdio、認可、承認、task、resource契約を保持。通信ライブラリの選定は先行試作で確定 |
| i18n-d / tinyfiledialogs / dportals | 翻訳キー・複数形・format・localeとOS別dialog挙動を維持するadapter |
| OSQP | `build-aux/osqp`と既存native solver境界を再利用し、精度・status・warm start等を比較 |
| リソースとversion | `res/`の埋め込みをCMake生成へ。翻訳、フォント、shader、アイコン、各OS配布資源を棚卸し |

SDL3は初期移植の必須依存とし、C++側にSDL2 fallbackや二重backendを設けない。
SDL2→SDL3のAPI変更は1:1移植に必要な境界変換として台帳化する。イベント・キー/マウス・テキスト入力、
window/contextの寿命、DPI/座標、clipboard、複数viewport、終了処理について、D版の観測動作を比較する。
Dear ImGuiのSDL3連携と既存の独自GL backendの責務を明確にし、入力の二重処理を避ける。
具体的なSDL3/ImGuiの組合せは三OSで先行試作して固定し、OS固有APIによる迂回を増やさない。

CMakeはC++20必須、拡張無効、MSVC UTF-8、CTestを基本とする。依存の自動ダウンロードは既定無効にし、
欠落は明確なconfigureエラーにする。依存バージョン・hash・ライセンスを記録する。
fast-math/AVX2は初期無効、CRTと例外・RTTI設定はnicxlive/Boost/ImGui/OSQP間で整合させる。
GUI、headlessテスト、njcを分け、テスト可能なロジックに不要なGL初期化を要求しない。
境界整理のためにDの実装順や依存構造を大規模再設計しない。

実装後に用意するコマンド例（現時点ではCMakeファイル未作成）:

```powershell
cmake -S . -B build/native-cpp -DNIJIGENERATE_NICXLIVE_SOURCE_DIR=../nicxlive -DBUILD_TESTING=ON
cmake --build build/native-cpp --config Debug
ctest --test-dir build/native-cpp -C Debug --output-on-failure
```

D参照のWindowsビルドは `dub build --config=win32-full`、回帰用は
`dub build --config=regression-tests`。Linux/macOSも`dub.sdl`の対応構成を利用する。
依存を取得する処理やmetadata生成によるGit実行を事前確認し、無断のネットワーク・Git操作を組み込まない。

## 6. WBSの分解と移植完了条件

ファイル単位は索引・配置の対応にだけ使用し、実施・完了判定の最小単位にはしない。
`tasks.md`は関数/メソッド/オーバーロード、ローカル関数、constructor/destructor、
型・field・enum・alias・mixin宣言まで分解し、それぞれ「D契約確定」「C++実装」「比較検証」を
独立した葉タスクにする。関数を持たないmoduleにも宣言公開・初期化接続とlink検証を置く。

コマンド生成、AutoMesh反射、Undo、Fiber、HTTP、SDL3など、複数シンボルで同じ不変条件を
共有する箇所は、一括設計する契約単位として対象・不可分な理由・成果物を明示する。
設計の一括化を複数関数の実装をまとめて完了扱いにする理由にはしない。
大きい関数の独立した処理領域や登録callbackは子タスクに分け、親は子以外と結合を担当する。
分解は作業範囲の管理であり、Dの制御フローを保ったC++関数を不必要に分割する指示ではない。

各葉にはWBS ID、D参照行/署名、参照hash、依存、完了証跡を持たせる。
静的棚卸しで未解決の呼出し先・生成実体は契約確定時に解決し、未解決のまま実装完了にしない。
実体化後に新たなシンボルや独立した動作が判明した場合は枝番を追加し、親だけをチェックしない。

1. Dファイルと依存を読む。型・関数・enum・UDA・module初期化・version分岐・unittestを台帳化する。
2. D入力fixtureと出力を固定する。状態変更は操作前後、Undo/Redo、通知、ログ/エラーも採取する。
3. Dの関数単位で同じ制御フローをC++へ写す。言語差をcompat、runtime差をadapterに限定する。
4. 全シンボルにC++対応先を記録する。削除・統合・省略があれば理由と差分テストを必須にする。
5. C++単体、Dとの比較、呼出し元との結合を検証する。循環importは宣言分離で扱い、
   実際の依存グラフの強連結成分単位で結合する。フェーズ順だけを完全な依存順とみなさない。
6. 各シンボルの公開/非公開動作と失敗系が一致して初めて該当する検証葉を完了にする。
   module全体の完了はすべての葉と結合検証の完了から導出する。
   コンパイル成功、UI表示、stubによるリンク成功だけでは完了にしない。

比較では整数・ID・schema・配列順・エラー分類を厳密一致させる。
数値はまず同じ型と演算順で比較し、許容差が必要な箇所はDの実測と演算特性から
絶対/相対誤差をfixtureごとに定義する。全体に大きなepsilonを掛けない。
NaN/Inf、符号付きゼロ、境界値、空/null、重複、無効参照を含める。

レンダリングは同じモデル・ポーズ・時刻・viewport・GPU条件で比較し、画像差分と状態差分を併用する。
GPU/OS差の許容範囲は明記し、スクリーンショットだけを数値処理の検証にしない。
`.inx`保存→再読込、D↔C++相互読込、`.inp`出力、PSD/KRA再importを検証する。
JSONの無意味なobjectキー順は正規化可能だが、配列順、欠落/null、数値型を消す正規化は禁止する。

## 7. リスクと移行終了条件

最優先リスクはUDA生成の漏れ、D配列の共有/null、Fiber、暗黙TLSの共有化、shutdown時の相互待ち、
nicxliveのエディタ用API不足、
PSD/KRA互換、GPU非同期処理とUndoの寿命。先行試作が失敗した場合は設計を更新し、stubで先へ進めない。

最終ゲートは全モジュールの対応台帳、D回帰試験の移植、コマンド全件比較、保存互換、
三OSの起動・編集・Undo/Redo・再読込・export・MCP/njc、3.6の並行処理ゲート、
長時間使用時のメモリ/リソース検証。
性能はDの同じfixture・環境で基準を計測してから劣化予算を定める。
新規の高速化を同等性の代用にしない。通常のC++ビルド・配布にD runtimeが含まれないことを確認する。
D版の削除、既定ビルド切替、配布、commit/push/PRは、この設計作業に含めず別途指示を受ける。

## 8. Runtime stateの連続データと実backend接続

`runtime_state.d`の環境光は、Scene UIが`vec3.vector`の実際の3要素配列を直接編集し、
OpenGLのuniform呼出しがその値をコピーする。C++では、この保存先をSDK内の
`thread_local std::array<float, 3> inSceneAmbientLight`一つにする。
既存App側の名前は同じ変数の`using`宣言とし、UIの`data()`とshader側の
`Vec3{light[0], light[1], light[2]}`を同じ保存先へ接続する。
一時配列からの書戻し、複数変数の同期、構造体をfloat配列へ再解釈する方法は用いない。
これは当該グローバルの配列保存先の対応であり、既存`math::Vec3`や`Vec2Array`の型・配置は変更しない。
SDKのC++型の同一性やABIをDの`vec3`と同一と認定するものではない。

`inDumpViewport`のD配列参照は、既存の保持付き配列を参照で受け取るSDK APIとして公開する。
実backendにも同じ参照を渡し、callback後には現在の配列記述子からポインタを読み直して行反転する。
OpenGL providerは同じ保存先をspanで受け、既存の`glGetTexImage`へ直接渡す。
既存vector APIは互換入口として保持し、Source経路をvectorへのコピーで代用しない。

遅延生成は、リンクされた実OpenGL providerが登録する生成関数へ接続する。
登録時やbackendオブジェクトの生成時にはGPUリソースを作らず、元の操作で初期化する。
既に選択されたbackendは保持し、未登録時に仮の基底backendを生成して黒画像や成功を返さない。
選択された構成の実providerが不足する間は、当該原本全体のSource完了を認定しない。
このSource実装段階では、現行指示に従いconfigure・コンパイル・ビルド・テスト生成・実行を行わない。

## 9. SimplePhysicsの文字列enumと実物理provider

`PhysicsModel`と`ParamMapMode`のD基底型はstringである。C++の整数enumは既知メンバーの
互換タグに限り、Sourceの値保存先には用いない。Source値は型の異なる保持付き文字列値とし、
null・空文字列・未列挙文字列・実際の文字列所有を保持する。コピーは同じ不変バッファを保持し、
別の整数状態と文字列状態を同期するモデルを作らない。既知タグとの変換は明示した対応表だけで行う。
Sourceの比較・capture・SharedValue・command引数・setter・serializeまで同じ実値を渡す。

表示時の列挙子名と文字列保存値を区別する。既知値の`Pendulum`と保存値`pendulum`は別であり、
未知値の表示・deserializeの拒否・resetのassertは、それぞれ原本と実一次providerの経路に合わせる。
文字列enumへ整数の未定義値を渡すことを、任意のD文字列を保持する実装とみなさない。

同じ原本の公開`Pendulum`・`SpringPendulum`は、既存の正規`phys::PhysicsSystem`の実変数登録・
RK4・`setD`へ接続し、独立した近似integratorを追加しない。driverは原本どおりの可変参照とし、
constructorのanchor読出し、仮想getLength、変数登録の順を保持する。`getGravity`・`getScale`・
frequency/dampingのSource getterは原本の可変virtual呼出しへ対応させる。parent/Puppetの欠如を
定数fallbackで隠さない。空のprivate loggerのlazy引数は評価しない。

原本1051行の製品領域と、SDKの実header/body・使用caller・通常公開を一続きで移す。
原本内のunittestは現行指示により生成・実行しない。既存の部分provider完了を原本全体の完了へ
読み替えず、M181はこの実provider到着と接続が揃うまで担当In Progressで保持する。
