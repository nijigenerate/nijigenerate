# 対話ルール
- 最終回答は、最初の指示と同じ言語で返してください。

# コミットルール
- gitする前には必ずプロジェクトのビルドをしてエラーがないことを確認してください
- このプロジェクトをビルドする際には、`dub build -c osx-full` というコマンドを使用してください
- ビルドをする際には警告が多量に出るため、エラーを確実に捕まえる工夫をしてください
- ビルドが通っても、循環参照などのランタイムエラーが出ることがあります。可能な限り循環参照をなくしてください。
- commit logは英語にしてください。

# コーディングルール
- コメントは英語にしてください
- ソースコードは既存の参照先の流儀に従ってください

# 主な連携プロジェクト
- nijilive: ベースのライブラリ。nijigenerateプロジェクトのルートパスから見て `../nijilive`にgitのファイルがあります。
- i2d-imgui: Dear ImGuiのラッパーライブラリです
- i18n: 翻訳機能を提供します。_コマンド、__コマンドがあり、string, c文字列のそれぞれに対応します。

# プロジェクトの大まかな配置
nijigenerateプロジェクトは、２Dのアニメーションのモーフィングを行うための編集ソフトの実装です。nijiliveプロジェクトで定義されているパペット表示、変形昨日をベースにし、そのための編集を行います。
- source/nijigenerate配下にソースコードがあります。
- source/nijigenerate/commands : エディタが提供するコマンドの実装
- source/nijigenerate/viewport : エディタのメインビューポートの実装
- source/nijigenerate/viewport/common : エディタのエディットモードによらない共通実装
- source/nijigenerate/viewport/common/operations : エディタの対象のNodeのクラスに応じて操作を変更する共通インターフェースクラスの実装があります。
- source/nijigenerate/viewport/common/tools : エディタでメッシュ編集やパラメータ編集でメッシュなどを変形するためのツールの実装があります。
- source/nijigenerate/viewport/anim : アニメーション編集モード用の実装があります。
- source/nijigenerate/viewport/model : エディタのメインのエディットモードの実装があります。パラメータ編集と、モデルの構造の編集の②つのサブモードがあります。
- source/nijigenerate/viewport/vertex : エディタの各Parts、MeshGroup、PathDeformer、DynamicCompositeなどのメッシュ編集モードの実装があります。
- source/nijigenerate/ext : nijiliveのコンポーネントを拡張したクラス定義があります。
- source/nijigenerate/panels : エディタの各種ドッキングウィンドウのクラス定義があります。
- source/nijigenerate/windows : エディタのポップアップウィンドウのクラス定義があります。
- source/nijigenerate/widgets : エディタ用のウィジェットのクラス定義があります。
