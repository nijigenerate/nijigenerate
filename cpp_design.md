# nijigenerate C++移植設計

作成日: 2026-10-25。作業ブランチ: `native-cpp`。
本書は移植の設計であり、C++実装の完了報告ではない。実行計画は `tasks.md` を参照する。

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

- D classの参照同一性を保持する。nicxlive Nodeは既存の`shared_ptr`契約に従い、親や観測者には
  必要に応じ`weak_ptr`を使う。Undoが保持する削除済みNodeは復元可能な期間だけ強く保持する。
- 一意所有のGLリソースやUI補助オブジェクトはRAII/`unique_ptr`を基本とする。
  GCを一律の`shared_ptr`置換で模倣せず、循環・イベント購読・callbackの寿命を個別に監査する。
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

### 3.4 Fiberとスレッド

`core/tasks.d`、`core/math/path.d`、`core/math/skeletonize.d`、`core/cv/contours.d`は
呼び出し先の深い位置でyieldする。C++20 coroutineはstacklessなので、そのままの代替にはならない。
1:1移植の第一候補はBoost.Contextによるstackfulな協調タスク。新たなバイナリ依存となるため、
Windows/macOS/Linuxで例外・スタック解放・停止・再開の試作を先に通す。採用確定前の大量移植は禁止する。
不成立なら明示状態機械への変更案を別途設計し、単なる`std::async`置換で進めない。

UI/モデル変更とGL操作はD版と同じスレッド境界に置き、FIFO・yield位置・完了除去のフレームを比較する。
MCP/vibe側のFiberはUIタスクとは別契約。HTTP待機・応答slot・タイムアウト・shutdownを
要求単位の状態とキューへ写し、UIへのdispatch順序を保持する。
派生更新ではgeneration、stale判定、cancel、Undoのaction token、プロジェクト破棄後の応答を検証する。

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

最優先リスクはUDA生成の漏れ、D配列の共有/null、Fiber、nicxliveのエディタ用API不足、
PSD/KRA互換、GPU非同期処理とUndoの寿命。先行試作が失敗した場合は設計を更新し、stubで先へ進めない。

最終ゲートは全モジュールの対応台帳、D回帰試験の移植、コマンド全件比較、保存互換、
三OSの起動・編集・Undo/Redo・再読込・export・MCP/njc、長時間使用時のメモリ/リソース検証。
性能はDの同じfixture・環境で基準を計測してから劣化予算を定める。
新規の高速化を同等性の代用にしない。通常のC++ビルド・配布にD runtimeが含まれないことを確認する。
D版の削除、既定ビルド切替、配布、commit/push/PRは、この設計作業に含めず別途指示を受ける。
