# Boardes 開発規約

## ビルドとテスト
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -GNinja && cmake --build build
ctest --test-dir build --output-on-failure
# PasS 実データでの取込テスト（手元のみ。未設定なら QSKIP される）
BOARDES_PASS_PARTS_DIR=/home/tomo/Documents/pass/parts ctest --test-dir build --output-on-failure
```

## バージョン管理
- リポジトリ直下の `.version` に「**現在開発中のバージョン**」を書く（セマンティックバージョニング）。
- リリース時は `.version` と同じ内容のタグを打つ（`.version` が `1.2.0` ならタグは `v1.2.0`）。
- **AI は `.version` を書き換えてはならない。** 変更するのは常に人間。
- コードを変更する前に、人間がその変更に見合うよう `.version` を更新しておく。
- AI は作業開始時に `.version` と `git tag -l` を確認し、次のいずれかなら**作業を始める前に警告する**:
  - `.version` が直近のタグと同じ値のまま（＝更新されていない）
  - 変更内容に対して上げ方がセマンティックバージョニング的に不適切
    （破壊的変更なのに patch/minor だけ、機能追加なのに patch だけ、など）
- 警告したうえでユーザーが「そのまま進めて」と言えば、進めてよい。

## データ形式のバージョニング
- 内部ファイル（`.boardes` / `.bpkg` / `.blib` / `library.json` / `.bpart` / `.part.json` / `.bboard`）は
  すべて `schema` に `"boardes.<種別>/<数値>"` を持つ。定義は `src/io/*.h` の `SchemaVersion`。
- 形式を変えたら該当する `SchemaVersion` を上げる。
- 古いバージョンは変換して読むか、明示的に「古すぎて読めません」と拒否する（黙って部分的に読まない）。
- 自分より新しいバージョンは常に「新しいバージョンの Boardes で作られています」と拒否する。
- **未リリースの間（v1.0.0 のタグを打つまで）に限り**、破壊的変更に対するマイグレーションは省略してよい。
  リリース後はこの免除は無くなる。

## コーディング規約
- C++23 / Qt6 Widgets。`.clang-format`（Google / タブ / 幅120）に従う。
- コメント・UI 文言・コミットメッセージは日本語。
- 新規ソース・ヘッダは `CMakeLists.txt` の `BOARDES_CORE_SOURCES` に必ず列挙する。
- Qt Creator で開けることが制約。FetchContent は使わない。
- コミットはユーザーが明示的に指示したときだけ行う。

## ドキュメント
- 実装プランは `/home/tomo/.claude/plans/` にある。設計判断の経緯はそちらが正。
- `design/` は「実物のスクリーンショットを置くだけ」の最小方針。claude.ai/design 側で生まれた
  デザイン案は、ユーザーの明確な指示がない限りコードに取り込まない。
- `design/OVERVIEW.md` はアプリの基本的な説明。**基本的な記述は書き換えず**、UI を変えたときは
  `design/NNN-<内容>.md`（連番）を新規作成して変更点をそこに書き、OVERVIEW.md にはその
  ファイルへの参照を1行足すだけにする（履歴を残すため）。UI を変えたらスクリーンショットも
  `design/screenshot_all.cpp` で取り直す。
