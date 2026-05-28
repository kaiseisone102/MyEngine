# MyEngine 作業プロトコル (rev.15)

最終更新: 2026-05-28 (rev.15: §2 冒頭に**ソース内コメントは英語で書く**ルールを明文化。新規・変更箇所は英語、既存日本語コメントは触る機会に順次英語化 (Foundations_Audit §8.9 DEBT-11)。資料ファイル本文は引き続き日本語。コミットメッセージは過去ログのスタイルに合わせ英語 / rev.14: §2-2 に**命名規約**を明文化。慣習的短名 (i/j/k/x/y/z/t/dt/e/it) は OK・**頭文字圧縮型 (state→s / sin→s / cos→c / source→s / map→m / model→m) は禁止**。理由は検索可能性 (grep-ability) > タイプ数削減。既存違反例 (`const float c = std::cos / s = std::sin` が gameplay_layer/combat_system/gate_system/particle_system に10箇所) をリファクタ候補として列挙 / rev.13: 運用モードを Claude Code (Read/Edit/Glob/Grep/Bash/PowerShell ツール直接利用) 前提に切替。chat 経由の「ユーザーが dump して貼る・PowerShell 置換テンプレで `$ok` フラグ + WriteAllText・対話プロンプト分岐の罠」前提だった §1-2/§1-3/§2-1〜§2-7 を削除し、§1-2 をハイブリッド原則 (怖い置換は変更前後を提示してから commit)、§1-3 を Edit ツール作法に書換。§2 は壊れた UTF-8 修復 (旧§2-8) と cryptic 識別子リネーム (旧§2-9) だけを Edit ツール用に簡略化して残置。§3 のビルド/起動コマンドは Claude が直接実行する形に再構成。§6 を「Edit で資料ファイル直接更新」に書換。思想ルール (§0/§1-1 精神/§1-4/§4 三層 RAII/§5 やらないこと/§5b〜§5e 確定設計) は不変 / rev.12: §5e を PART3c-2 完了 (indirect 差し替え・CPU draw 撤去, 1cf23b9 = Phase 2B 完了) に更新 / rev.11: §1-4 を新設=「設計・スコープの判断は資料を確認してから (推測禁止のソース版)」、§5e を新設=PART3c スコープと 3c-1 確定事項 / rev.10: §5d を新設=PART3b per-draw SSBO プールの cursor リセット教訓 / rev.9: scene/model.h 記述を削除済み確定に修正 / rev.8: §3-0 にビルド環境を Ninja+clangd 構成として明文化 / rev.7: §2-9 に cryptic 識別子リネーム手順を追加 / rev.6: §1-1 にコンストラクタ依存変更時の完全型確認、§2-8 に文字化けコメント修復手順を追加 / rev.5: §5c に PART3a 完了反映 / rev.4: §5b 開発中前提・§5c GeometryBuffer 確定設計を新設) / 対象: MyEngine の実装作業をする際の手順と禁則

このドキュメントは「どう作業するか」の正本。新規チャットの Claude はこれに従う。
ゴール・現在地は START_HERE、何を作るかはロードマップ、どの順かは依存マップ。このファイルは**手順と事故防止**を担う。

---

## 1. 大原則 (このセッションで実地に確立した、守るべき作法)

### 1-1. 実ソースを見てから実装する (推測禁止)
- Claude Code は Read/Glob/Grep ツールでソースを直接読める。**ただし推測で書かない**。書く前に必ず該当ソースを Read して確認する。
- **メンバを触る前に、その識別子をファイル全体で Grep して全使用箇所を洗い出す。** 特に `&member` (アドレス取得) と、ラムダの出力引数で書き換えている箇所は壊れやすい。
- 関数の throw 構造 (if→throw か、ブロック throw か、try/catch か) を見てから書く。見ずに wrap 位置を推測しない。
- **コンストラクタの依存を変える変更では、その層が新たに「完全型」を要求しないか確認する。** 前方宣言で足りていた型でも、`ctx.state.runtime.window` のように `.member` でアクセスし始めると完全な定義の include が必須になる。コンストラクタ定義だけでなく **include 群まで Read** して必要な `#include` があるか事前確認する (2026-05-26、LayerContext 化で title_layer.cpp が GameState 完全型を要求し C2027 を踏んだ)。

### 1-2. 怖い置換は変更前後を提示してから commit (ハイブリッド原則)
- 通常の局所修正は Edit ツールで直接書く。Edit は old_string/new_string の完全一致で個別に成否を返すので、誤マッチは構造的に弾かれる。
- **ただし下記のような「広く効く変更」は、変更前後 (or 概要) をテキストで提示してユーザー承認を得てから Edit に進む**:
  - types.h の構造体追加/拡張 (C++/GLSL 両側に波及)
  - shader 全 frag/vert への一括変更 (4 lighting frag 同時改修等)
  - pipeline layout / descriptor set layout の変更
  - 描画パスの追加/削除/順序変更 (pass_chain 配線)
  - 全 render pass にまたがる変更 (Reverse-Z 等)
  - 設計判断が分かれる選択 (§1-4 = 資料に答えがあれば二択を出さず実行・無い真に新しい判断のみ提示)
- 分割すべき作業は、最初に「これを 4 PART に分けて進めます」と段階を明示してから着手する (ユーザーが途中で軌道修正できる粒度に)。

### 1-3. 置換は Edit ツール (個別に成否が返る)
- 局所修正は `Edit` (old_string/new_string)、全文書き換えは `Write` を使う。
- Edit は old_string がファイル内で一意でないと失敗するので、文脈を含めて一意にする (前後の行を含める)。`replace_all=true` は変数リネーム等の用途。
- 反映チェック (§3-1b の obj/exe タイムスタンプ) は依然必要 (Edit が通っても CMake の追記漏れ等でビルドに乗らない事故は別軸)。

### 1-4. 設計・スコープの判断は、資料を確認してから行う (推測禁止のソース版・2026-05-27 確立)
- §1-1 が「実ソースを推測するな」なら、これは「**資料 (ロードマップ・START_HERE・依存マップ) を推測するな**」。スコープ/アーキテクチャの判断 (何を今やるか・何を別にするか・bucket をどう分けるか) は、記憶やその場の都合で決めず、**該当資料を引いて書いてある方針を確認してから**動く。
- **ユーザーに二択/多択を出す前に自問する: 「この答えは資料に既に書いてあるのでは?」** 書いてあるなら二択を出さず実行する。ユーザーが「資料に書いてない?」「ロードマップ見て」と言ったら『お前は資料を読んでいない』のサイン — 即座に資料を引く。
- **「○○は前の PART で対応済み」という思い込みを確認なしで使わない。** 実際に効いているか (どの経路で・どのインスタンスが) を dump で確認してから前提にする。
- 事故記録 (2026-05-27): PART3c のスコープは資料で「prop のみ・terrain は別 bucket」と確定済みだったのに、Claude が資料を読まず「全 static 統合 (terrain 込み)」を二択でユーザーに出して選ばせ、さらに「terrain は 3c-0 で geom 化済み」と思い込んでワールド地形 (実際は legacy) を prepared 経路に乗せ、地形描画を破壊した。資料確認と経路確認を怠った二重の推測。詳細は START_HERE §0-8。

---

## 2. 置換・編集の作法

ファイル編集は Claude Code の `Edit` (局所置換) / `Write` (全文書き換え) / `Read` (確認) / `Grep` (検索) を使う。文字コードは UTF-8 (BOM なし)、改行は CRLF (Edit は既存改行を維持)、インデントは 4 スペース。

**ソース内コメントは英語で書く (2026-05-28 確立)。** 新規・変更箇所のコメントは英語で書く。既存の日本語コメントは触る機会に順次英語化する (Foundations_Audit §8.6 DEBT-11・スコープを勝手に広げない §0-5)。**例外**: 資料ファイル (本ドキュメント・正本5枚・side/ 配下) は引き続き日本語。コミットメッセージは過去ログのスタイルに合わせ英語。

### 2-1. 壊れた UTF-8 (文字化けコメント) の修復 (2026-05-26 確立)
ソースに壊れたマルチバイト (`EF BF BD` = U+FFFD「�」、および改行が食われてコメントと次行が癒着) が混入することがある。**Edit の old_string にそのまま貼っても完全一致しない。** 手順:
- Read で該当範囲を確認し、FFFD 行が「コメント単独」か「実コードと癒着」かを周辺行で判別。
- 復元したい文字列を新たに用意し、行単位で Edit。**癒着行は new_string に改行を入れて複数行へ展開**し、実コードを巻き込む行は変更前後を確認する。
- 行を増減させると以降のオフセットがずれるので、同一ファイル内は後ろの行から先に処理する。

### 2-2. 命名規約と cryptic な短い識別子のリネーム (2026-05-26 確立 / 2026-05-28 拡張)

**命名規約 (新しく変数を導入するときの基準):**
- **慣習的短名は OK** (リネームしない / 新規でも使ってよい): ループ `i` / `j` / `k`、座標 `x` / `y` / `z`、時間 `t` / `dt` (delta time)、flecs entity `e`、イテレータ `it`。これらは文脈で意味が一意に決まるので、冗長化はかえって可読性を下げる。
- **頭文字圧縮型の短名は禁止**: `state → s` / `std::sin → s` / `cos の結果 → c` / `source → s` / `map → m` / `model → m` のように、**元の単語を頭文字に圧縮した短名は使わない**。`state` は `state`、sin の結果は `sinYaw` / `sinAngle`、cos は `cosYaw`、source は `source`、と元の単語を保つ。タイプ数を惜しまない。
  - 理由: 同じ短名が文脈ごとに別物を指すと、Grep/IDE 検索で取り違えが起きやすく、Claude (将来の自分含む) が読むとき常に文脈推論しないと意味を確定できない。**検索可能性 (grep-ability) は可読性より優先**。
  - 既存コードに残る違反例 (リファクタ候補・触る機会に直す): `gameplay_layer.cpp:800/876` / `combat_system.cpp:35-36` / `gate_system.cpp:20-21` / `particle_system.cpp:165-166` の `const float c = std::cos(...); const float s = std::sin(...);` → `cosYaw` / `sinYaw` 等に。
- **型は重いのに 1 文字仮引数**は意味が消えるのでリネーム: `GameState& s` → `GameState& gameState`、`Mapping m` → `Mapping mapping`、`Material m` → `Material material` 等。型名の lowerCamel に直す。

**リネーム手順 (cryptic→読める名前へ書き換える時):**
`s` / `m` のような 1 文字仮引数は grep しづらく意味が消えるので、型名の lowerCamel に直す。ただし `s` は `settings` / `systems` / `std::` / `std::sin` の `s` 等、無数の識別子の部分文字列なので **`replace_all` の素朴な一括置換は厳禁**。
- まず Grep で「トークンとしての `s`」を全出現洗い出す (語境界 regex `(?<![A-Za-z0-9_])s(?![A-Za-z0-9_])` / `GameState&?\s+s\b` / `auto&?\s+s\s*=`)。**裸の `s` が別物 (例: `const float s = std::sin(...)`) に使われていないか必ず確認** (上の命名規約により、こちらも `sinYaw` 等にリネームする対象)。
- Edit を形ごとに分ける: 宣言/定義 `GameState& s` → `GameState& gameState`、`s.` アクセス → `gameState.`、別名 `auto& s = state_;`、実引数の裸 `s` (`(s,` / `(s)`)。`replace_all` は前後の文脈を含めて一意になる範囲でのみ使う。
- 置換後は Grep で取り残しを再走査し、残るのが `%s` (printf 書式) や `'s` (英語コメント) など無害なものだけかを確認してからビルド。
- `.h` 宣言と `.cpp` 定義は同一 commit でシグネチャを揃える (片方だけ直すとリンクエラー)。

---

## 3. ビルド・実行・コミット

### 3-0. ビルド環境 = CMake + Ninja + clangd (2026-05-26 確立, この構成が前提) ★最初に読む
ビルドは **Ninja ジェネレータ**に一本化した (clangd の F12/補完が必要とする `compile_commands.json` は Ninja でしか生成されない。Visual Studio ジェネレータは非対応)。ディレクトリ構成:
- `build/` — **現用 (Ninja)。** exe は `build\MyEngine.exe` (Debug サブフォルダ無し・single-config)。clangd はここの `compile_commands.json` を読む。
- `build-vs/` — VS ジェネレータ用 (preset `windows-vcpkg`)。通常は使わない。
- CMakePresets: `windows-vcpkg-ninja` (configure, Ninja, binaryDir=build) / build preset `ninja-debug`。
- settings.json: `cmake.buildDirectory` と clangd `--compile-commands-dir` は両方 `build`。`C_Cpp.intelliSenseEngine` は `disabled` (MS IntelliSense は使わない・clangd が担当)。

**CLI ビルドは生 PowerShell では失敗する** (Ninja は MSVC 環境変数 INCLUDE/LIB/cl.exe を自前で読み込まないため)。**毎回まず VsDevShell を読み込む** — `PowerShell` ツールで:
```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64; cmake --build --preset ninja-debug 2>&1 | Select-String -Pattern "error C|error LNK|FAILED|ninja: build stopped"
```
- `--config Debug` は不要 (single-config。構成は preset の `CMAKE_BUILD_TYPE` で決まる)。
- VSCode の Build ボタンでも同じ `build` にビルドされる (settings.json の `cmake.configureEnvironment` で MSVC 環境が渡るため VsDevShell 不要)。
- 再 configure が要るとき (新規 .cpp 追加で CMakeLists を編集した等) は `cmake --preset windows-vcpkg-ninja` (VsDevShell 読み込み後)。

**F12 (定義ジャンプ) が効かない / `'SDL3/SDL.h' file not found` 等が出るとき**: clangd が `compile_commands.json` を読めていない。手順 = (1) `Glob` で `build/compile_commands.json` の実在確認、(2) 無ければ VSCode で `CMake: Select Configure Preset` → Ninja preset → `CMake: Configure` で生成、(3) `clangd: Restart language server`。新規ヘッダは compile_commands.json に載って初めて clangd のインデックスに入る (新規ファイルの F12 が効かないのはこれが原因のことが多い)。

### 3-1. ビルド (error だけ見る。warning の文字化けは無害)
- 上記の VsDevShell 読み込み + `cmake --build --preset ninja-debug` を実行し、stderr/stdout を `error C|error LNK` で grep。
- 何も出ずプロンプトに戻れば成功 (warning しか無かった)。`error C....` が出たらその行を直す。
- 日本語ビルドメッセージは文字化けして見えるが実体は正常。error の有無だけで判定する。
- exe 生成確認: ビルドログに `-> C:\MyEngine\build\MyEngine.exe` の行があれば OK。出ない場合は「再コンパイル対象が無く最新」なだけのこともある (§3-1b で確認)。
- ※ CMake の C++ ソースは `add_executable` に**明示列挙**されている (GLOB ではない)。新規 .cpp はこの列挙に Edit で追記しないとビルドに乗らない。追記後は `cmake --preset windows-vcpkg-ninja` (VsDevShell 読み込み後) で再 configure してからビルドする。

### 3-1b. 反映チェック (error 0 と「そもそも再ビルドされていない」を区別する) ★重要
「error が出なかった」だけでは、変更がバイナリに反映されたか分からない。Edit は通っても CMake への追記漏れ等でビルド対象に乗っていなければ、古い exe が起動して「直った」と誤判定する事故がある。**ソースを変更したビルドでは、exe が実際に焼き直されたかをタイムスタンプで確認する。**
- Bash の `stat`/`ls -la` 相当を `PowerShell` ツールで: `(Get-Item C:\MyEngine\build\MyEngine.exe).LastWriteTime`。
- ビルド直後にこの時刻が「今」になっていれば反映済み。古いままなら再コンパイルされていない (CMakeLists 追記漏れ・キャッシュ等を疑う)。
- **新規 .cpp を足したときは、対応する .obj が生成・更新されたかを必ず確認する** (例: `Glob` で `build/**/vma_image.obj`)。obj が**空 = ビルドに乗っていない**。
- 補強策: 変更したソースに対応する .obj の更新、またはビルドログに当該 .cpp のコンパイル行が出ているかを見る。少なくとも「コードを変えたのに exe の時刻が変わらない」ときは、起動して正常でも信用しない。
- シェーダ (.frag/.vert) 変更時は exe でなく **.spv のタイムスタンプ**を見る (例: `build\shaders\*_frag.spv`)。

### 3-2. 起動確認
- `.\build\MyEngine.exe` を `Bash` または `PowerShell` ツールで起動し、stderr/stdout から `ready|leaked|Validation|VUID` を grep。
- `Vulkan ready` + leaked/Validation/VUID ゼロ が正常。
- **validation ログは先頭 N 行で切らない (2026-05-25 の教訓)。** 起動直後の数行 (`Vulkan ready` 等) で枠が埋まり、後から毎フレーム出る validation エラーを見逃す。実際に bloom Off 時の mip0 レイアウトエラー (VUID-vkCmdDraw-None-09600) を、`-First 8` で先頭しか見ず Phase 1I 以来ずっと「validation ゼロ」と誤報告し続けていた。出力が `Vulkan validation layer enabled.` の1行だけなら本当にゼロ。
- **設定で切り替わる機能 (bloom/影品質/法線マップ等) は、On と Off の両方の状態で起動して validation を確認する。** 片方の状態でしか確認しないと、もう片方のパス (例: bloom Off 時の execute スキップ経路) のエラーを見逃す。
- 描画系の変更は目視確認も必須。Claude は exe を起動して validation/VUID/leak のテキスト確認まではできるが、**画面の見た目チェックはユーザーにスクショ依頼する**。同期系の変更は「描画が回り続けるか・正常終了するか」も見る。
- 注意: ビルドエラー時や再ビルドされなかったとき、前回の古い exe が起動して `Vulkan ready` が出ることがある。先に 3-1 で error 0 を確認し、かつ 3-1b で exe のタイムスタンプが更新されたことを確認してから起動する (古い exe を見て誤判定しない)。

### 3-3. pdb ロックの罠 (既知のトラブル)
- ビルド中にコンソールでテキストをマウス選択すると、クイック編集モードで出力が固まり、cl.exe が中途半端に生き残って .pdb をロックする。次のビルドが全ファイル `error C1041` で失敗する。
- 直し方: プロセスを kill してから再ビルド。
```powershell
taskkill /F /IM cl.exe /T 2>$null
taskkill /F /IM MSBuild.exe /T 2>$null
taskkill /F /IM mspdbsrv.exe /T 2>$null
```
- C1041 が全ファイルで出る = コードの問題ではなく pdb ロック。コードは無関係。

### 3-4. git commit (必ず cd C:\MyEngine してから)
```powershell
cd C:\MyEngine    # ホーム C:\Users\... だと "not a git repository" になる
git add -A
git commit -m "..."
```
- コミットメッセージは英語で、何をどう変えたかを具体的に (このセッションの commit ログが手本)。
- **新規ファイルを含む commit で `git commit -am` は使わない (`-a` は追跡済みファイルしかステージせず、新規ファイルを取りこぼす)。** 上記どおり `git add -A` → `git commit -m` を守る (2026-05-26、shadow_light.h が `-am` で untracked のまま残った実例)。

### 3-5. ビルド構成・設定ファイル・git の教訓 (2026-05-26)
- **ビルド出力先 (binaryDir) を変えたら `.gitignore` も追従させる。** VS preset の binaryDir を build→build-vs に変えた際、build-vs が ignore 漏れし生成物 800 件超が追跡候補に乗った。`.gitignore` は `build-*/` で build 派生ディレクトリを網羅 (build-vs / 退避用 build-*-old 等も一括除外)。git の変更数が急増したら、まず生成物の ignore 漏れを疑う (実体の変更ファイルは `git status --short` で数件のはず)。
- **`.gitignore` 等 ASCII 前提のファイルに日本語コメントを入れない。** PowerShell の ReadAllText/WriteAllText 経路でエンコーディング事故 (文字化け) が起きやすい。設定ファイルのコメントは ASCII で書く (ソースの日本語コメントは UTF-8 BOM なしで別管理)。
- **行内容で置換対象を特定する regex は、似た行を巻き込まないよう厳密に。** `^# build` が見出し `# Build directories` を誤マッチした実例あり。大文字小文字・前後を限定する。

---

## 4. VkUnique / VmaBuffer / VmaImage 化の鉄則 (二層 RAII への移行手順)

> 仕様の詳細 (メソッド・登録型・ファクトリ) は Codebase_Guide を参照。ここは「移行の手順」。

### 4-1. VkUnique 化 (生 Vulkan ハンドル → VkUnique<T>)
1. メンバを grep して全使用箇所を洗う (生成 vkCreateXxx / 使用 / 破棄 vkDestroyXxx / getter / アドレス取得)。
2. h: メンバ宣言を `VkUnique<VkType> member_;` に。getter は `return member_.get();` に。
3. cpp 生成: `vkCreateXxx(..., &member_)` を、生ローカル `VkType local = VK_NULL_HANDLE; vkCreateXxx(..., &local);` で受け、成功後 (throw ブロックの後) に `member_ = VkUnique<VkType>(device, local);` で wrap。
4. cpp 使用: 値で渡す箇所は `member_.get()`。
5. **アドレス取得 `&member_` は不可** (VkUnique* になる)。`VkType h = member_.get();` をローカルに取り `&h` を渡す (vkWaitForFences / pSetLayouts 等)。
6. cpp 破棄: 手動 `vkDestroyXxx` ブロックを `member_.reset();` に置換 (順序は元のまま)。
7. `!= VK_NULL_HANDLE` の判定は `if (member_)` / `if (!member_)` (explicit operator bool) に。
8. `std::vector<VkType>` は `std::vector<VkUnique<VkType>>` にし、`resize(n, VK_NULL_HANDLE)` は不可 → `clear(); reserve(n);` + `emplace_back(device, handle)`。
9. ラムダが出力参照 `(VkType& out)` でメンバに書く設計なら、ラムダを `-> VkType` 値返しに変え、呼び出し側で wrap。

### 4-2. VmaBuffer 化 (生 VkBuffer + VkDeviceMemory + map → VmaBuffer)
1. 生メモリ buffer + memory + void* mapped を VmaBuffer 1個に。
2. create は適切なファクトリ (Codebase_Guide 参照: createMappedStorageBDA / createMappedHostVisible) 1行。
3. アップロードは `.mapped()` へ memcpy。手動 map/unmap は全廃。
4. bind は `.buffer()`、破棄は `.reset()` (or デストラクタ任せ)。

### 4-2b. VmaImage 化 (生 VkImage + VkDeviceMemory → VmaImage) ※ラッパー作成済み・移行進行中
1. メンバ `VkUnique<VkImage> image_` + `VkUnique<VkDeviceMemory> memory_` を `VmaImage image_` 1個に集約。VkImageView / VkSampler は所有者側の VkUnique のまま (VmaImage は View/Sampler を持たない。VmaBuffer と対称)。
2. create は `VmaImage::createAttachment(ctx, w, h, format, usage)` (color/depth アタッチメント用、dedicated allocation)。mip/array が要る場合は低レベル `VmaImage::create(ctx, ci, dedicated)`。
3. image ハンドルが要る箇所 (ImageView 作成・バリアの `barrier.image`) は `image_.image()`。
4. getter (image()/view()/sampler()) の**シグネチャは変えない** (外部呼び出しを壊さないため、内部だけ差し替える)。
5. 破棄は `image_.reset()` (or デストラクタ任せ)。

### 4-3. descriptor set の扱い
- descriptor pool / layout は VkUnique 化する。
- descriptor set 自体は pool 所有なので VkUnique 化しない (pool の reset/破棄で自動解放)。shutdown では set を `VK_NULL_HANDLE` にクリアするだけ。
- swapchain resize 等で set を明示解放する場合は `vkFreeDescriptorSets(pool_.get(), ...)`。

---

## 5. やらないこと / 触らないこと (事故防止)

- **反射 (planar reflection) の品質改善は今やらない。** ユーザー判断で据え置き中。現代化はロードマップ Phase 3-Refl で扱う。
- **directional シャドウマップの swimming (プレイヤー移動で影が泳ぐ) は据え置き。** ユーザー判断 (2026-05-25)。原因はライト VP がプレイヤー追従 (camera_system.cpp の `target = playerPos`)。対策は texel snapping だが今やらない。
- **engine_app の WorldTerrain/WorldWater 手動 clear ループ (stopgap) は土台側段階2 (遅延破棄キュー) まで触らない。** ※「段階2」は土台側 (リソース管理リファクタ) の段階であって、Phase 2A/2B 等とは別軸 (コードベースガイド用語集参照)。遅延破棄キュー導入時にまとめて撤去する領域。
- **ResourceFactory の createBuffer/createImage は `[[deprecated]]`。** 新規コードでは使わない (VMA ファクトリを使う)。削除は土台側段階1総仕上げのタスクとして別途。なお `createImageVMA` (vmaCreateImage 版) は呼び出し元なしのデッドコード — VmaImage 移行完了後にまとめて削除する。
- **勝手にゴール・方針を変えない。** 最新技術は「能力チェック + フォールバックで実装対象」(ロードマップ §3)。GPU が古いことを理由に Phase を勝手に外さない。
- **「承知しました」と言って違う方向に進まない。** 解釈に迷ったら推測で進めず確認する (過去に重大な方針逸脱の事故があった)。
- **指示された範囲を勝手に広げない。** 「これを追記して」に対して全ファイル一括更新を始める等の過剰解釈をしない (2026-05-25 に実際に発生)。スコープが曖昧なら広げる前に確認する。
- **model は renderer/model.h が唯一の正。** 旧 `scene/model.h` は参照ゼロの死にコピーと確定し**削除済み (2026-05-26, commit 3e4a0ae)**。以前の「暫定ルール・確定まで断言・確定作業は別タスク」は解消済み (コードベースガイド §2 参照)。

---

## 5b. 未確定の設計判断 (着手前に決めること・推測で先に書かない)

これらは「最初に必要になる Phase の直前に、実コードを見て決める」設計判断。今は未定。バラバラに実装すると後で統一する二重作業になるので、必要になったら最初に1箇所で決める。

- **能力チェック (feature gate) の置き場が未定。** ロードマップ §3 の「能力チェック + フォールバック」は思想は決まっているが、実装の置き場 (能力ビットをどの struct に持つか / 分岐を起動時・pass_chain・各 pass のどの層で書くか) は未定義。**最初に能力分岐が要る Phase (2B の Hi-Z occlusion、または 3 系の最新経路) に着手する直前に、capability 構造体の置き場と分岐の層を1箇所で決めてから書く。** 各 Phase でバラバラに能力チェックを書き始めない。決めたらコードベースガイドに明文化する。
- **types.h のアライメント規約は実ソースで確認済み・明文化済み (2026-05-25)。** types.h 冒頭 (LAYOUT RULES コメント) に明記: 全 vec3 は vec4 にパディング / mat4 は16-byte 境界 / vec3+float より vec4 / VkDeviceAddress は8-byte 整列でペアにして16に揃える / C++/GLSL 両用は BDA を含むときだけ `#ifdef __cplusplus` 分岐 (BDA 無しは1定義両用)。実例 = InstanceData(96B)/GpuMaterial(64B)/CullObject(32B)。**これ以上「未確定」ではない。新構造体はこの規約に従って足す** (コードベースガイド §2 の旧 TODO は解消済み)。
- **能力チェックを書く前に、実 apiVersion と拡張の有無を実測する。** NVIDIA の Vulkan 1.4 ドライバは Pascal (620) もフルサポート対象なので「Vulkan 1.4」前提自体は妥当。ただし最新経路の分岐を書く前に、起動時に `vkEnumerateInstanceVersion` / `vkGetPhysicalDeviceProperties2` で実 apiVersion を、`vkEnumerateDeviceExtensionProperties` で mesh shader / HW レイトレ等の拡張の有無を実測し、その実測値で分岐する。「1.4 のはず」「拡張はあるはず」で書き始めない (能力チェック + フォールバックの設計 = ロードマップ §3 はこの実測が前提)。
- **VmaImage の dedicated priority について**: `VmaImage::create` は dedicated 時に `ai.priority = 0.75f` を立てるが、アロケータ生成 (vulkan_context.cpp) のフラグは `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT` のみで `VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT` が無いため、現状 priority は**無視されるだけで無害**。priority を効かせたい場合のみ、別途アロケータ生成にこのビットを足すか検討する (今は不要)。
- **【2026-05-25 追加】2B PART3 (本格 GPU-driven 化) は描画経路の根幹 + メッシュメモリレイアウトを変える大改修。設計を立ててから多段で刻む。** 現状 static は SubMesh ごとに別 vertex/index buffer・描画単位が draw item×SubMesh・per-SubMesh push constant 更新 (main_pass `drawStaticModelList` / model.h SubMesh で確認済み)。indirect 化には (a) 単一 vertex/index buffer 統合 (b) per-draw データの SSBO 化 + gl_InstanceIndex 参照 (c) static vertex shader 改修 (d) drawId を SubMesh 粒度に再定義、が必要。**いきなり全面書き換えに突っ込まず、PART3a (メッシュ統合) の設計を実ソース (mesh.h / model_loader / SubMesh) 確認の上で固めてから着手する。** バックアップ commit を細かく取りながら進める (描画が壊れたら戻れるように)。詳細は START_HERE §2 / Codebase_Guide §3.5 の「PART3 の障壁」。
- **【2026-05-26 追加・最重要の設計原則】「開発中前提」で設計する。現状のシーン規模を基準に固定上限・固定容量を決めてはいけない。** これは MyEngine が大規模オープンワールドを目標にしている以上、すべての容量・バッファ・配列・上限に適用される恒久ルール。
  - **失敗例 (2026-05-26, GeometryBuffer PART3a-2a)**: GeometryBuffer の容量を「現状の全アセットが載るだろう」という見積りで 1M 頂点固定にした。結果、起動時の全モデルロードで armor 等 9 件が capacity exhausted で描画されなくなった。「現状基準で固定容量を決めた」のが原因。開発中はアセットが増え続けるので、現状ぴったり or 少し上、では必ずまた溢れて同じ作り直しになる。**ユーザーの "現状基準で作るのやめない？今開発中なんだよ？" は完全に正しい指摘。**
  - **守るべきこと**: (1) 容量・上限は「現状 + バッファ」で固定するのでなく、**動的に伸びる構造**にする (満杯時に自動で増える)。`std::vector` が自動で伸びるのと同じ発想を GPU リソースにも適用。(2) どうしても固定値を置くなら、それは「初期値」であって「上限」ではない設計にし、超えたら伸びる経路を最初から実装する (TODO や後付けにしない — 後付けは "そのうち" 来ない or 来たとき作り直しになる)。(3) 「とりあえず大きめ固定」も避ける — 大きすぎると VRAM を無駄に食い (P620 は 2GB)、小さすぎると溢れる。動的成長なら使う分だけになる。
  - **適用例**: GeometryBuffer は単一固定 megabuffer をやめ、**複数ブロック (満杯で新ブロック追加) + VmaVirtualBlock** に再設計 (下記 §5c)。今後 instance buffer / draw command buffer / 各種 SSBO で上限を置くときも同じ原則。

## 5c. GeometryBuffer 確定設計 (2026-05-26 / 複数ブロック + VmaVirtualBlock)

PART3a の GeometryBuffer は、上記「開発中前提」原則に従い、**無制限に伸びる複数ブロック方式**で確定。web 確認済み (VMA 公式 virtual allocator が標準手法、NVIDIA は単一巨大バッファの VRAM 過剰コミット警告)。

- **複数ブロック**: `GeometryBuffer` は `std::vector<Block>` を持つ。各 Block = `{ VmaBuffer vbuf; VmaBuffer ibuf; VmaVirtualBlock vtxVirt; VmaVirtualBlock idxVirt; }`。満杯になったら**新ブロックを追加確保** (既存ブロックはコピーせず据え置き = ハンドル不変・コピーコストゼロ・VRAM ピーク 2 倍にならない)。これが「倍リサイズ+全コピー」より優れる点。
- **サブアロケーション**: 手書き free-list をやめ、AMD 公式の **VmaVirtualBlock** (`vmaCreateVirtualBlock`/`vmaVirtualAllocate`/`vmaVirtualFree`) に置換。VMA は本環境に存在確認済み (Vulkan 1.4 対応版)。
- **【最重要の実装教訓 (2026-05-26)】VmaVirtualBlock は「要素単位 (頂点数 / index 数)」で使う。バイト単位で使ってはいけない。** Vertex stride は 76 バイト = **非 2 べき乗**。virtual block をバイトサイズで作り alignment=sizeof(Vertex)=76 を渡しても、返るバイトオフセットは 76 の倍数にならず (例: 19604 % 76 = 72)、それを /76 して頂点 index に戻すと切り捨てで 1 頂点弱ずれ、描画が範囲外を読んで **device lost (vkQueueSubmit failed)** になる。**正しくは: virtual block の size を頂点数で作り、alloc の size も頂点数・alignment=1 にする。返るオフセットがそのまま頂点 index になり端数が出ない。** copyBufferRegion の dst は `offsetElems * sizeof(Vertex)` でバイト化。index も同様 (要素=index 数、alignment=1)。**この device-lost は二分法 (1 巨大ブロック強制で再現 → 複数ブロック無実と確定 → 割り切れチェック DIAG で 76 バイト alignment を特定) で根本原因を切り分けた。validation layer は無言、GPU-AV は P620 で内部エラー自滅だったので、二分法 + 自前 DIAG ログが有効だった。**
- **MeshHandle に `blockIndex` を追加**: どのブロックの buffer を bind するかを描画時に知るため。これが唯一の API 拡張。Mesh/SubMesh は handle を持つだけなのでフィールド 1 個増えるだけ。
- **bind はブロック指定に**: `Mesh::bind`/`SubMesh::bind` の中で、自分の handle.blockIndex のブロックの vertex/index buffer を bind する。**描画ループ側の `vkCmdDrawIndexed(..., firstIndex, vertexOffset, ...)` 行は変えなくてよい** (bind が内部で正しいブロックを選ぶので、描画ループの再改修は最小)。
- **free**: `vmaVirtualFree` を DeletionQueue 経由で (実 free まで配線済み・後付けでない)。
- **新ブロックサイズ**: `max(デフォルトブロックサイズ, 要求サイズ)` — 1 メッシュがデフォルトブロックより大きくても確保できる。デフォルトは控えめ (起動時 VRAM 節約)、足りなければブロックが増える。
- **完成形との整合**: prop bucket / terrain bucket (START_HERE の完成形アーキ) は、それぞれが「ブロック集合を持つ GeometryBuffer インスタンス」になる。複数ブロック設計は bucket 構造に直結。
- **将来**: ストリーミングで free が効けば、空ブロックを丸ごと解放する縮小も足せる (VmaVirtualBlock が空かは `vmaIsVirtualBlockEmpty` で判定可)。これも複数ブロック設計の上に自然に乗る。
- **【確定した実装事実 (2026-05-26, commit ac7bbd1 で PART3a 完了)】**
  - **alloc の staging buffer は即破棄する (DeletionQueue に乗せない)**。`ResourceFactory::copyBufferRegion` は内部で one-time command を submit し `vkQueueWaitIdle` するので、コピーは関数復帰時に完了済み → staging は即 `reset()` して安全。**DeletionQueue 経由にしたら起動時に壊れた**: alloc は起動時のモデルロード = drawFrame の外で呼ばれるので、DeletionQueue の collectFrame が一度も走らず staging が 1 バケツに数百個溜まり VRAM 枯渇 → device lost。**DeletionQueue は drawFrame 中に呼ばれる動的 free 専用**、同期コピーの後始末には使わない、という責務分離が正しい。GeometryBuffer の `free()` (実行中のメッシュ破棄) は引き続き DeletionQueue 経由 (こちらは drawFrame 中なので正しい)。
  - **shutdown は `vmaClearVirtualBlock` してから `vmaDestroyVirtualBlock`**。メッシュは常駐し個別 free しないので、teardown 時に virtual allocation が残っている。clear せず destroy すると VMA が assert で abort する。shutdown は vkDeviceWaitIdle 後なので一括 clear して安全。
  - **完了状態**: cube / grass / Model SubMesh の全 prop ジオメトリが共有 megabuffer に統合され、block #0〜#3 が自動追加 (capacity exhausted ゼロ)、armor 含む全モデルが pixel-correct に描画、validation/VUID/leak ゼロを確認。PART3a 完了。

## 5d. per-draw SSBO プール (DrawDataPool) 確定事項 (2026-05-27 / PART3b)

PART3b で static の per-draw データ (model/materialId/alpha) を push constant から `DrawData` SSBO に移した。`renderer/draw_data_pool.h` (header-only, InstanceBufferPool と同型の per-frame 線形 SSBO+BDA)。draws は `pushOne`→slot を取り、`vkCmdDrawIndexed` の **firstInstance** に slot を渡して vertex shader が `DrawData[gl_InstanceIndex]` で引く (indirect-ready)。確定した作法:

- **【最重要の罠】複数パスが同じ per-frame SSBO プールを共有するなら、cursor リセット (`beginFrame`) は「全消費者より前」で 1 回だけ呼ぶ。** PART3b で reflection→main の順に同じ DrawDataPool を消費するのに、`beginFrame` を grass 充填の所 (= reflection execute の後) に置いていたため、main が cursor=0 から積み直して **reflection の slot を上書き**し、GPU 実行時に reflection が main のデータを読む状態になっていた。**目視では reflection に偶然正しく映って気づけなかった** (looks fine ≠ correct = bloom Off レイアウトバグ §3-2 と同類)。対策: `beginFrame` を ShadowPass 後・reflection ブロック前 (反射 On/Off 共通で必ず通る位置) へ移動。slot は reflection [0..Nrefl) → main [Nrefl..) の連続割り当てになり上書きが消える。**検証は「描画リストを動かして (視点移動) reflection が安定して映り続けるか」**で行う (静止画は偶然一致しうる)。
- **InstanceBufferPool (grass) は main でしか使わない**ので、その beginFrame は grass 充填の所のままで正しい。共有しないプールは消費者の直前で良い。混在に注意。
- **slot 連続割り当ての容量**: reflection 分 + main 分 (opaque + transparent) が 1 フレームで `MAX_DRAWS` (= CullingPass::MAX_DRAWS) に収まること。現状 static は少数 (opaque static 26 程度) なので余裕。§5b の「固定容量を動的成長に」負債は cull cmds と DrawDataPool で共通 (PART3c/その後で一括検討)。
- **bindAndDraw (Mesh/SubMesh)**: bind と `vkCmdDrawIndexed(..., firstIndex, vertexOffset, firstInstance)` を 1 呼び出しに束ねる (d6dbcde)。bind と offset の desync を構造的に防ぐ。draw 経路の重複は `renderer/static_draw.h` (header-only) に集約済み — PART3c で indirect 化するときもここ 1 か所を直す。

---

## 5e. PART3c 確定事項とスコープ (2026-05-27 / GPU-driven の prop 化 = 完了)

**スコープ: PART3c で GPU-driven 化するのは prop (cube + Model) のみ。terrain は対象外 (別 bucket・ストリーミング Phase)。** これは START_HERE の完成形アーキテクチャ通り。prop と terrain を同じ GeometryBuffer に混ぜない (寿命・サイズ・粒度が違い断片化する)。地形マテリアル (土/岩/砂) は terrain bucket 側の splat で扱う。**この境界を毎回確認する (§1-4)。一度 terrain を prop bucket に統合して描画破壊し戻した経緯あり。**

- **`renderer/static_cull_build.h` (header-only, namespace `static_cull`)**: main の opaque **prop** を SubMesh 粒度で走査し、drawId 連番で 3 つを同時生成する単一ビルダ。
  - `pool.pushOne` で **DrawData slot** を取る (= 絶対 slot。reflection が先に積んだ分を自動で考慮、Nrefl 計算不要)。
  - `CullObject` (world sphere, `extentDrawId.w` に drawId を焼く) → cull.comp が `cmds[drawId].instanceCount` を書く。
  - `CullingPass::DrawTemplate` (indexCount/firstIndex/vertexOffset, **firstInstance = slot**)。
  - `PreparedDraw` (blockIndex + draw range) → main の描画用。
  - **三者が drawId で一致** (drawId = cullObjects/drawTemplates の index = command index、firstInstance = DrawData slot)。
  - bounds: cube = 単位 AABB を直書き (`cubeLocalAABB`, foot-based [-0.5,0.5]×[0,1]×[-0.5,0.5])、Model = `localAABB()` を全 SubMesh に流用 (保守的)。
  - **変数は読める名前で書く** (result/cullObject/boundingSphere/preparedDraw/item/subMesh/materials/drawData/drawTemplate)。s/d/r/co/pd/bs 等の暗号的名前は禁止 (Codebase_Guide の cryptic リネーム方針)。
- **配線 (pass_chain)**: `beginFrame(cursor=0)` → reflection execute (pushOne) → **`static_cull::build(...)` を reflection 後・cull 前に呼ぶ** (reflection の slot が先に取られている状態で prop を [Nrefl..) に積む) → CullingPass execute (built.cullObjects/drawTemplates を渡す) → main execute (built.draws を `mi.preparedOpaque`、`mi.geometry` に GeometryBuffer)。
- **main の opaque (3c-2 完了時点)**: prop は **`vkCmdDrawIndexedIndirect`** で CullingPass の `commandBuffer(frameIndex)` (instanceCount 0/1 入り) から描く (CPU draw ループ撤去済み)。GPU が instanceCount==0 を自動スキップ = **GPU カリングが実描画に効いている**。`MainPass::ExecuteInfo::indirectCommandBuffer` に pass_chain が配線。terrain は `drawTerrainList` の CPU draw (別 bucket=Phase 2F 待ち)。grass/skinned/transparent/reflection = 従来経路。
- **検証済み**: GPU カリング結果が CPU オラクルと完全一致 (gpu=cpu=56/78、SubMesh 粒度)。CullingPass の compute→DRAW_INDIRECT バリアは実装済み (culling_pass.cpp)。
- **3c-2 (完了, 1cf23b9) = Phase 2B 完了**: main の prepared CPU ループを `vkCmdDrawIndexedIndirect` (CullingPass の `commandBuffer(frameIndex)`) に差し替え → CPU draw 撤去 → CullingPass が prop の実描画に初接続。**シェーダ無改修**。確定事項:
  - **`drawIndirectFirstInstance` が必須能力 (multiDrawIndirect だけでは不足)**: firstInstance に DrawData slot を載せて `gl_InstanceIndex` で引くので、indirect コマンドの firstInstance が非ゼロ。non-zero firstInstance を indirect で使うにはこの feature が要る (Vulkan 仕様 / web 確認)。VulkanContext が起動時に query して対応時のみ有効化 (`vkGetPhysicalDeviceFeatures` → `features.multiDrawIndirect` / `features.drawIndirectFirstInstance`)、getter で公開。**P620 は両対応を実測** (`[Caps] multiDrawIndirect=1 drawIndirectFirstInstance=1`)。`drawIndirectFirstInstance` 非対応なら **CPU draw ループにフォールバック** (direct draw の firstInstance は無制限)、`multiDrawIndirect` 非対応なら区間内 drawCount=1 の indirect ループ。**能力分岐は実測してから書く (§5b)・「対応しているはず」で書き始めない。**
  - **block 散在 = 連続区間ごと indirect**: prop の blockIndex 分布は散在 (実測 blockSwitches≈17-18 / draws=77)。**「同一 GeometryBuffer block の連続区間ごとに 1 draw 呼び出し」** (対応=区間長ぶん単発 MDI / 非対応=区間内 drawCount=1 ループ)。**「全 draw を 1 回の MDI」は不可** (block 切替で vertex/index buffer の bind が変わる)。呼び出しを block ごとに減らすのは builder の blockIndex ソート (任意の後段最適化・今は不要)。drawIndirectCount による compaction も後段 (構造を変えず層を足せる)。
  - **検証足場の教訓 (HUD)**: デバッグ HUD に `Cull : <可視> / <総数>` を出す (可視=`lastGpuVisible` 前フレーム dispatch、総数=`built.draws.size()`。視点回転で分子が動く = カリングが効いている)。**当初 GPU=CPU オラクル照合を緑/赤で出したが撤去**: `lastGpuVisible` (前フレーム) と CPU オラクル (今フレーム) で基準が 1 フレームずれ、高速移動時に偽の不一致 (赤) が出る (表示比較側のズレで実バグでない・looks fine ≠ correct とは別問題)。閾値で誤魔化すと本物のズレも飲み込むので、照合自体を撤去し可視数表示のみに。**同一フレーム基準の精密照合が要るなら PART4/Hi-Z 着手時に作る** (state を撤去予定の検証足場に増やさない判断)。CullingPass の `lastCpuVisible()` (PART2 由来) は残置・無害で、純 GPU-driven 化 (CPU readback 撤去) のときにまとめて消す。
  - **検証**: prop 全部正常描画 (目視・スクショ)・validation/VUID/leak ゼロ・視点回転で可視数が増減・prop 拾得で総数が減る (ゲームロジック通り)。
- **terrain bucket (将来・ストリーミング Phase)**: 専用 GeometryBuffer + 専用 cull (視錐台 + 距離 LOD) + splat マテリアル経路 + チャンクのロード/アンロード (DeletionQueue が効く)。`terrain_mesh.h/.cpp` の geom 対応コードは残置済みで再利用可 (現在は呼ばれず legacy 動作)。

---

## 6. セッション終わりにやること

- 完了した作業を踏まえ、START_HERE の §2 (現在地) を Edit で直接更新する (完了 Phase を反映、次の一手を更新)。
- ロードマップ・依存マップに変更が要るなら、該当ファイルも Edit で更新。各ファイル冒頭の rev/最終更新日も書き換える。
- **更新するファイルはユーザーの指示した範囲に絞る。** 「このメモを追記して」と言われたら、関係するファイルだけを更新する (全ファイル一括にしない。§5 の過剰解釈禁止)。どこまで更新するか曖昧なら、広げる前に確認する。
- 大きな変更 (構造変更・複数ファイルにまたがる仕様変更) は、Edit を実行する前に変更概要を提示してユーザー承認を得る (§1-2 のハイブリッド原則)。
- **更新を忘れると次セッションが古い現在地から始まる**ので必要なものは必ず Edit する。Claude Code 内では資料ファイルそのものが永続記録 (チャット履歴に頼らない)。
