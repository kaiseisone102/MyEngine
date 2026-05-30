# START HERE — MyEngine セッション用ブートストラップ

最終更新: 2026-05-30 (**§0 最優先ルールに §0-9 (設計提示前の二つの自己審査: ① 最新技術であること ② 既存に引っ張られて妥協していないこと を根拠つきで明記してから提案) + §0-10 (推測でコードを書くな・絶対) を追記** (詳細は Work_Protocol §1-2a / §1-2b rev.19)。 / **Phase 2G-1 + 2G-2a 完了反映 (HEAD 1b57f8e・user runtime 検証済)**: skinned を §1.5-B 「すべて GPU-driven」へ寄せる前半。 2G-1 = batched compute skinning pre-pass (skinning.comp + SkinnedVertexPool deinterleaved + SkinInstancePool + SkinningPass・受け皿 gpu_skinning.h を per-instance に是正)・2G-2a = passthrough vert で skinned stream を読み旧 vertex-shader skinning 全撤去 + SkinnedDrawData SSBO で indirect-ready = **skin once 達成 (3〜4x→1x)**。 §2 直近の完了 list 先頭 + §1.5-B 射程明示 (skinned = 2G-1/2a 完了・残 2G-2b) を更新。 同期: Roadmap rev.15 / Deps rev.14 / Foundations rev.9 / INDEX rev.10。 残 2G-2b (CullingPass CullSet + indirect = CPU draw ループ撤去) / 2G-3。 / 2026-05-29 (**最新化マラソン 28 commits 反映**: A1-A6 (buffer VMA + 生 vkAllocateMemory ゼロ) + E + E clean (camera-relative 全 10 site wire-up) + F1-F5 (固定容量一族 5 クラス動的化) + G (bindless free-list) + N (memory_priority 実利用) + O (debug_utils GPU markers) + W (sync_validation + swapchain hazard fix) + C (transfer queue) + I (memory_budget) + B (FrameSync timeline semaphore migration) + D+L+K+T+Z+J+Q (8 receptacle → 5 実 enable) + U (JobSystem) + M (AsyncCompute receptacle) + V/R/S/H/X/Y/P (7 design-memo headers)。 §2 引き継ぎブロックに詳細・新規ファイル一覧・正直な妥協度評価 (🟢17 / 🟡9 / 🔴14)、 直近の完了 list 冒頭に新エントリ。 P620 [Caps] 30 capability・DGC のみ 0。 / **PART4 §6 4d「Pure GPU-driven cleanup = 完了」追加反映 (commit f8d1e1f)**。 user 報告「HUD `Cull : 0 / 67` がどこを向いても 0」を契機に判明: 4-前-4 (15b89ad) で compactCmd が device-local 化されて以降、 旧 host-mapped readback 経路の `lastVisible_[]` を更新する code path が断たれて HUD は永久 0 だった (props は GPU 経由で正常描画されていたため動作上は健全・HUD だけ stale)。 option B 採用 = 純 GPU-driven の本来形 (compactCmd / countBuf は device-local のまま CPU は可視数を知らない) で HUD `Cull` 行 + CPU Frustum オラクル + 全 wire-up 撤去。 8 files +2 -51。 §2 引き継ぎブロック (historical PART3c-2 内 line 178/185) に inline note、 次にやること list から残作業項目を撤去、 直近の完了に commit f8d1e1f 追加。 / **PART4 §6 4c 完了反映 = two-pass HZB occlusion + Tier 1 α (Nanite/Granite 2024 baseline) + 1-tap minmax fast path** + **§6 4d 大半完了 (γ × 3 + M × 3 + N × 4 = 10 commits の audit-driven 最新技術取り込み)**。§2 引き継ぎブロックを 4c/4d 完了で全面書き換え、 直近の完了に 18 commit 追加 (ad97879..1481049)、 次の一手を「PART4 完了畳み込み or 2C LOD / 2A 多光源 / 2F terrain bucket」に更新。 18 commit 内訳: 4c-A/B/C/D + α + β + γ-1/2/3 + M3/M1/M2 + N1/N4/N2+N3 + Tier 1 α 活性化 + 4c 中 bug fix 2 件。 P620 [Caps] 18 中 17 = 1 で実走確認。 思想ルール (§0/§1.5) は不変。 / 2026-05-28: **PART4 §6 4b 完了反映 = HiZPass 新設 (SPD-style single-dispatch min+max RG32F pyramid)**。§2 引き継ぎブロック先頭を 4b 完了に書き換え、 次の一手を 4c (two-pass occlusion 本体) に更新、 直近の完了一覧に 4b エントリ追加 (commit ffe9673)。 思想ルール (§0/§1.5) は不変。 / 2026-05-28: 運用モードを Claude Code 直接ツール利用前提に切替。§3 で正本ファイルの所在を明示、§4 の「ユーザーが dump/コマンドを貼る方式」前提を削除し Claude が直接 Read/Edit/Bash する形に書換、§5 のチャット添付ループを「Edit で資料ファイル直接更新」に書換。怖い変更 (types.h・shader 全置換・全 pass 波及) は変更前後を提示してから commit するハイブリッド原則を §4 に明示。**§4 にソース内コメント英語ルールを追記** (既存日本語は順次英語化・DEBT-11)。詳細は Work_Protocol rev.15 / Foundations_Audit §8.9 を参照。)

> このファイルは新セッションの冒頭で毎回 Claude が読む「正本への入口」。
> Claude はまずこのファイルを最後まで Read し、下の【Claude への指示】に従うこと。
> セッション開始時に読む正本: この START_HERE と一緒に、ロードマップ本体・依存マップ・作業プロトコル・コードベースガイド (計5枚、下記 §3) を Read する。

---

## 0. 最優先ルール (Claude はこれを最初に守る)

0. **【最優先の判断原則 / 2026-05-25 追記・改訂】最優先は「最新技術で作る」こと。「直接的・散らからない (保守性)」はそれに従属する副次基準にすぎない。**
   - **最新技術が第一基準。** 「直接的・散らからない」は、最新技術を実現し維持しやすくするための手段であって、最新を退ける理由には絶対にならない。両者が衝突したら必ず最新を取る。「直接的」は、複数のやり方が同じくらい最新なときに、その中から選ぶための副次基準にすぎない。**最新より直接性を優先するのは厳禁** (実際にそれをやってユーザーを激怒させた。二度とやらない)。
   - **「その機能の対象がまだ無いから最新版は不要」という論法は禁止。** エンジンは今まさに作っている最中なのだから、ある機能が使う対象 (例: 法線合成のための detail map・地形ブレンド・decal) が「まだ無い」のは当たり前。それを「無いから最新を入れない理由」にするのは、未来に作るものを今ある前提で評価する倒錯。**最新技術でエンジンを作るとは、受け皿を先に最新の形で用意し、中身を後から埋めていくことそのもの。** 今使わない経路は**スタブを置くかコメントで枠を残す**のが正しい。「今は縮退するから旧来版で」は却下。
     - 具体例 (1K-5 法線): surface gradient bump mapping framework (Mikkelsen 2020, Unreal 採用) が最新で、複数法線合成 (detail/地形/decal) に正しく対応する。合成対象が今無くても、surface gradient で書く。合成経路はスタブ/コメントで残す。「合成が無いから (2) 微分再構成でいい」は禁止された論法。
   - **古い手法で組んでから「最新をよこせ」と言われて作り直す順序は厳禁** (Phase 1I bloom で fragment 版を組んでから compute 版に切り替え時間を浪費した。これを繰り返さない)。各実装は最初から最新で決める。
   - **「最新か?」と「保守性は?」を毎回別々に問い直してユーザーを足止めしない。** 最新を選ぶ → その実現手段として最も直接的な書き方を採る、の一方向。保守性を独立の関門にしない。
   - 補足 (なお最新は多くの場合直接的でもある): bloom の compute 版は render pass も framebuffer もブレンド state も不要で storage image に直接読み書きするだけ = 最新かつ最小。最新を選ぶと結果的に散らからないことが多い。ただしこれは「最新だから直接的」であって「直接的だから採る」ではない。順序を逆にしない。

1. **正本の優先順位**: このファイル群 (START_HERE / ロードマップ本体 / 依存マップ / 作業プロトコル / コードベースガイドの5枚) が唯一の正。**過去の記憶やメモリ機能の内容と矛盾したら、必ずこのファイル群を優先する。** メモリに古い方針 (rev.1/rev.2 など) が残っていても、ディスク上の最新ファイル本文が正しい。
2. **着手前に方針を確認・復唱する**: いきなり作業を始めない。まずこのファイル (特に §1.5 設計アーキテクチャ方針) とロードマップの現方針を読み、**「①今のゴール ②該当する設計方針 (§1.5 の A〜E のどれか) ③現在地 ④次の一手 ⑤スコープ境界 (今やる範囲と、やらない範囲=別 Phase)」を自分の言葉で短く復唱してから動く。** ユーザーがそれを見て方針のズレを正せるようにする。**設計・スコープの提案をするときは、その根拠となる §1.5 の項目 (or 正本の §番号) を提案文に明記する。根拠の節番号を書けないなら方針を確認していない証拠なので、提案を出す前に正本を引き直す (§0-8)。復唱・根拠明記をせずに設計提案を始めるのは、それ自体が「方針未確認」の赤信号。**
3. **勝手に方針を変えない**: ゴール (§1) は不変。実装方針 (能力チェック + フォールバック等) も勝手に変えない。変更が必要と思ったら、進める前にユーザーに確認する。
4. **「承知しました」と言って違う方向に進む事故を避ける**: 過去に、合意したつもりで全く違う方針で進んでしまった事故がある。少しでも解釈に迷ったら、推測で進めず確認する。
5. **指示された範囲を勝手に広げない**: 「このファイルを直して」に対して全ファイル一括更新を始める等の過剰解釈をしない (過去に発生)。スコープが曖昧なら広げる前に確認する。
6. **資料は変更が確定するたび、その該当ファイルだけを都度修正して出力する**: 作業して状態が変わったら、ロードマップの「現在地」と、必要なら依存マップ等を更新した版を、変わったファイルだけ出力する (バッチしない)。ユーザーがそれを保存して次回また添付する運用。
7. **着手前に、5ファイルの本文を全部 Read する**: 正本5枚 (START_HERE / ロードマップ本体 / 依存マップ / 作業プロトコル / コードベースガイド) の本文を Read で読み込んでから着手する。**「メモリにあるはず」「前セッションで読んだはず」で動かない** (記憶は引き継がれない・§5)。本文欠落のまま作業しない。INDEX.md (横断索引) も併せて Read すると効率がよい。
8. **【最重要・2026-05-27 追記】スコープ・アーキテクチャの判断は、必ず資料を確認してから行う。資料に答えが書いてある判断を、資料を読まずに自分で決めて (まして二択にしてユーザーに) 出すのは厳禁。** これを毎回守る:
   - **判断の前に資料を引く**: 「terrain は prop と統合する? 別 bucket?」「PART3c のスコープは?」のような設計判断に直面したら、自分の記憶やその場の都合 (目先のデータ・作業量) で決めず、**まず該当資料 (この §2 現在地・ロードマップ・依存マップ) を Read/Grep し、そこに書いてある方針を引用してから**動く。
   - **資料に書いてあるのに確認せず誘導した事故 (2026-05-27)**: PART3c のスコープは資料で「**prop (cube/Model) のみ。terrain は別 bucket・ストリーミング Phase**」と確定済みだったのに、Claude が資料を読まず「全 static 統合 (terrain 込み)」を二択で出してユーザーに選ばせ、terrain を prop の GeometryBuffer に統合して描画破壊 → 戻す手戻りを発生させた。ユーザーは「資料に書いてある」「前提がずれすぎ」と何度も指摘した。**資料に答えがある判断をユーザーに毎回言わせるのは、ユーザーの時間と信頼を奪う最悪の振る舞い。**
   - **ユーザーが「資料に書いてない?」「ロードマップ見て」と言ったら、それは『お前は資料を読んでいない』というサイン。** 即座にディスクから資料を Read/Grep して書いてある内容を基準に話し直す。記憶で答えない。
   - **二択・多択をユーザーに出す前に自問する**: 「この選択肢の正解は資料に既に書いてあるのでは?」。書いてあるなら二択を出さず、資料の答えをそのまま実行する。資料に無い真に新しい判断のときだけ、資料を引用した上で相談する。
9. **【2026-05-30 追記】設計を提示する前に二つの自己審査を通す (必須)**: **そもそも設計を検討・提示するとき (実装時だけでなく) は、まず開発資料のコーディングルールを読んでから設計する** — 特に §0 全項 (とりわけ §0-8 = 資料に答えがあれば二択を出さず資料の答えを実行する) と §1.5 設計方針、そして「GPU の性能・現状の規模に関わらず最新技術を実装する」原則 (§1 / L40・L44・L60)。設計の段でルールを見ないと、資料に答えがある判断を二択にして出したり (§0-8 違反)、規模を理由に最新技術を躊躇したり (§1 違反) する。実際にそれをやってユーザーを激怒させた (2026-05-30 2G-2b の bounds 算出で資料 L202 に答えがあるのに二択を出した)。設計時もルール参照を必須とする。 そのうえで、設計をユーザーに提案する前に、必ず ① **最新技術であること** (業界最新形か・「動くから」「既存に合うから」で古い形を選んでいないか・必要なら着手時に web で最新動向を確認) と ② **既存コードに引っ張られて妥協していないこと** (「整合が楽」「差分が小さい」「安全」を理由に最新形でない案を選んでいないか) を確認し、**その根拠を提案の文面に明記してから出す**。根拠を書けないなら確認できていない = 提案未完成。**妥協案を最新の顔で推奨するな** (過去事故: CPU skinning fallback 温存を §3 誤用で推奨 / フル Vertex 維持を「安全・最小 diff」と化粧して推奨 = いずれも §0 違反)。妥協が本当に必要なら「これは妥協であり理由は X」と明示する。詳細は Work_Protocol §1-2a。
10. **【2026-05-30 追記】推測でコードを書くな (絶対)**: **推測でコードを書く AI はゴミだ。死ね。** 関数シグネチャ・メンバ名・構造体レイアウト・throw 構造・include 関係を、実ソースを Read せず「たぶんこう」で書いた瞬間に事故る (過去に何度もソースを破壊し、存在しない commit hash を資料・memory に書き込みユーザーを激怒させた)。書く前に必ず Read/Grep で実物を確認する。「読んだふり」をするな。commit hash・ビルド結果・検証結果は実際にコマンドを走らせ返り値を読んでから報告する。走らせていないものを「完了」と書くな。詳細は Work_Protocol §1-2b。

---

## 1. 不変のゴール (ブレてはいけない錨)

**大規模オープンワールドのゲームエンジン MyEngine を、GPU の性能に関係なく最新のグラフィックス技術を取り入れながら作る。**

- 「最新技術を体験・学習したい」が一番の動機。最新技術 (mesh shader / HW レイトレ / リアルタイム GI 等) も、能力チェック + フォールバック付きで**実装対象にする** (詳細はロードマップ §3)。
- 現在の開発 GPU は Quadro P620 (Pascal, VRAM 2GB)。最新機能の一部はこの GPU では動かない (フォールバックで動く) が、コードは書く。将来 RTX 世代 GPU に更新したら最新経路が自動で開く設計。
- 大規模前提。小さく作って後で作り直す事態を避ける。作業量が多いことは覚悟済み。

このゴールはチャットをまたいでも変わらない。各セッションはこのゴールに向かう一歩。

---

## 1.5. 設計アーキテクチャ方針 (不変・設計判断の前に必ず読む / 2026-05-27 集約)

> **この節は「不変の設計方針」を1か所に集めた索引。§2 (現在地) は毎セッション書き換わるので、不変の方針はこちらに置く。設計・スコープの提案をする前に、必ずこの節を読み、関連項目を復唱・引用すること (§0-2 / §0-8)。** 各項目の詳細は末尾の「正本」を参照。

**A. 最優先の判断原則** (詳細: §0)
- 最新技術が第一基準。保守性 (直接的・散らからない) はそれに従属する副次基準。衝突したら必ず最新を取る。
- 「対象がまだ無いから最新は不要」は禁止。**受け皿を先に最新の形で用意し、中身を後から埋める**。今使わない経路はスタブ/コメントで枠を残す。
- 古い手法で組んでから最新に作り直す順序は厳禁。各実装は最初から最新で決める。

**B. GPU-driven 完成形アーキテクチャ** (詳細: Roadmap §2・§4 / Phase_Dependencies §4)
- **すべての描画 (static prop / terrain / 将来は skinned も) を最終的に compute カリング + indirect draw に乗せる。** CPU draw ループが残るものは無くす (スケールしないため)。これが「大規模エンジン」の核。
  - **射程の明示 (2026-05-30 監査で確定・進捗追記)**: prop = ✅ 完了 (Phase 2B)。 **skinned = Phase 2G** (✅ 2G-1 + 2G-2a 完了 HEAD 1b57f8e = compute skin once + passthrough vert で頂点 skinning 3〜4x→1x + 旧 vertex-shader skinning 撤去・runtime 検証済 / 🔄 2G-2b PART0-2 完了 (CullSet に Skinned + per-CullBucket 入力 + per-bone conservative bounds・HEAD e79de3c)・残 PART3-4 = skinned_cull + indirect + two-pass HZB occlusion で CPU draw ループ撤去 = §1.5-B 完全準拠)。 **grass = Phase 2H** (現状 CPU per-blade frustum cull = 違反・未着手)。 terrain = Phase 2F (別 bucket)。 残る CPU ループ (water / bindless test / reflection static / transparent) は後段。 「将来は skinned も」の「将来」= Phase 2G として番号確定。
- **bucket を分ける**: **static prop 用 GeometryBuffer と terrain 用 GeometryBuffer は別の megabuffer・別 indirect バッチ**にする。地形チャンクは寿命・サイズ・アロケーション粒度が prop と全く違い、同一 megabuffer に混ぜると断片化する。**prop と terrain を同じ GeometryBuffer に入れない** (この境界を破って描画破壊した事故あり = §0-8)。
- bucket は同じ仕組み (GeometryBuffer + DeletionQueue + compute cull + indirect) を 2 つ並立させる。prop = Phase 2B、terrain = Phase 2F。
- **「compute が instanceCount を書く → indirect が読む」cmdBuf 駆動構造を維持** (PART4 Hi-Z occlusion がこの間に drop-in する前提。COMPUTE→DRAW_INDIRECT バリアもこの形)。
- drawIndirectCount による compaction は完成形の後段の最適化 (構造を変えず層を足せる)。今は instanceCount=0/1 を GPU がスキップする形で十分。

**C. 能力チェック + フォールバック (実装の背骨)** (詳細: Roadmap §3)
- 最新経路は必ず実装する。実行時に GPU 能力で分岐する (対応=最新経路 / 非対応=フォールバック)。開発 GPU P620 で動かない機能 (mesh shader / HW レイトレ) もコードは書き、RTX 更新で自動で開く。
- 固定容量を現状基準で決めない。動的成長 (multi-block 等) にする (§Work_Protocol 5b)。
- **GPU-driven indirect の必須能力 (2026-05-27 確定・実測)**: per-draw データを firstInstance 経由 `gl_InstanceIndex` で引く設計では、`multiDrawIndirect` に加えて **`drawIndirectFirstInstance` が必須** (non-zero firstInstance を indirect で使うため)。起動時に query して対応時のみ有効化し、非対応なら CPU draw にフォールバック。P620 は両方対応を実測 (`[Caps] 1/1`)。

**D. データの持ち方 (モダンな土台、既存)** (詳細: Codebase_Guide §1・§3)
- Buffer Device Address (BDA) + bindless + VMA + per-draw SSBO + gl_InstanceIndex(firstInstance)。descriptor 切り替えを減らし indirect/GPU-driven に繋げる。
- 新しい共有構造体は types.h のレイアウト規約 (vec3→vec4 パディング等) に従う。
- 二層 RAII (VkUnique / VmaBuffer / VmaImage) を新パスで必ず使う。

**E. terrain (大規模地形) の設計** (詳細: Roadmap Phase 2F / Phase_Dependencies §1・§4)
- terrain は B の通り**専用 bucket**。Phase 2F = 専用 GeometryBuffer + 専用 cull (視錐台 + 距離 LOD) + **splat マテリアル経路 (土/岩/砂のブレンド)** + チャンクストリーミング。
- 地形マテリアルが複数 (土・岩) になる受け皿は terrain bucket の splat 側。現状1種類でも splat の枠を先に用意する (A の原則)。
- 前提: 2B (prop GPU-driven 骨格) + 遅延破棄 + buffer 系 VMA 化 + ストリーミング層。

**この節の使い方 (設計・提案の前のセルフチェック)**:
1. 設計判断・スコープ提案をする前に、A〜E のどれに該当するかを確認する。
2. **提案文に「どの方針 (§1.5 の A〜E のどれ・どの正本) に基づくか」を明記する。** 根拠の節番号を書けないなら、それは方針を確認していない証拠 → 出す前に正本を引く。
3. 着手時の復唱 (§0-2) に、ゴール・**該当する設計方針 (§1.5)**・現在地・次の一手・**スコープ境界**を含める。これでユーザーがズレを即座に検知できる。

**正本 (詳細はこちら)**: 最優先原則=§0 / GPU-driven 完成形=Roadmap §2・§4 と Phase_Dependencies §4 / 能力チェック=Roadmap §3 / データの持ち方=Codebase_Guide §1・§3・§3.5 / terrain=Roadmap Phase 2F・Phase_Dependencies §1 / 確定設計=Work_Protocol §5b-§5e。

---

## 2. 現在地 (ここだけ毎セッション更新する)

> ### ▶ 前回セッションからの引き継ぎ (新規チャットはまずここを読む / 2026-05-30 更新)
>
> **【Phase 2G-2b 進行中: skinned を GPU-driven indirect 化。 PART0-2 完了 (HEAD e79de3c)・PART3 設計済・PART4 残】**
>
> - **PART0** (9deae63): `CullSet` enum に Skinned+Count 追加 (前セッション未コミットの build 破壊状態 = `kNumCullSets=CullSet::Count` だが enum に Count 無し、を復旧)。
> - **PART1** (5968a08): `Model::animationAABB_` = load 時に全アニメクリップ走査して **per-bone conservative bounds** (Unterguggenberger 2021・LBS 凸結合で per-bone 影響箱を全ポーズ union・dense sample + inter-sample margin)。 剣振り等で AABB はみ出し誤 cull を防ぐ。 Animator 再利用で runtime と同一 skin 数学。
> - **PART2** (e79de3c): CullingPass 入力を **per-CullBucket{Prop,Skinned}** に。 Camera+Shadow=Prop 共有・Skinned=専用 bucket。 出力は per-CullSet のまま。 Skinned bucket は未 dispatch = 描画不変。
> - **PART3 設計済 (未実装・skinned_cull_build.h は未作成)**: skinned_cull_build.h (static_cull_build 対称・animationAABB・firstInstance=2G-2a slot・block-sort) を skinning walk 内 inline emit (§5d re-walk 回避) + `execute(set=Skinned, frustum)`。 draw は CPU drawSkinnedPrepared のまま = 描画 no-op。 HUD cull 数は pure GPU-driven (readback 撤去済) のため入れない。
> - **PART4 残 (怖い変更・目視必須)**: MainPass skinned→indirect (CPU draw 撤去) + **skinned two-pass HZB occlusion 相乗り** (roadmap #5・**HZB は 2G-2b の中**・2G-2c は存在しない)。
> - **最新技術 web 再確認 (2025)**: two-pass HZB occlusion は今も GPU-driven baseline・LBS 標準 (DQS でない)・graphics-state bucket で static/skinned 分離が標準。 work graphs / DGC / mesh shader は HW ゲートの最前線で受け皿/別 Phase。 → 2G-2b は production 最新 baseline。
> - 詳細は Roadmap Phase 2G。 ※ この session で Claude Code の PreToolUse ゲート (read-before-edit 強制・per-turn marker・`.claude/hooks`) を追加したが、 これは tooling であり engine 設計ではない。
>
> **【最新化マラソン: 28 commits / Foundations §1-4 + Vulkan13 §1-6 + Open-world receptacles を一気通貫で実装】**
>
> User の指示「最新技術の導入と古い技術の投棄」「妥協なし」「オープンワールド前提」に従い、 4 回の audit ラウンドで合計 36 Package を立ち上げ、 そのうち真に runtime に効くもの = 17 件、 半 wire-up = 2 件、 受け皿のみ = 17 件 (Phase 待ち)。 個別実装は session 内で逐次 commit。 重要な achievements を時系列で:
>
> **A1-A6 (6 commits 995b779 / 46cb937 / 185ac09 / 80ccb76 / a030372 / dab4faf)**: buffer 系 VMA 化マラソン。 Foundations §8.2 / §8.5 / §1-3 解消:
> - A1: Mesh dead-code 撤去 (loadFromObj 0 callers + uploadBuffer 経路全削除 + private hybrid 撤去)
> - A2: TerrainMesh VmaBuffer 化 (sharedFlatTerrain + stage_registry 経路維持)
> - A3: SubMesh + ModelLoader dead-code 撲滅 (uploadBuffer 経路 / private hybrid)
> - A4: Texture staging VmaBuffer 化 (生 vkMapMemory 撲滅完了)
> - A5: **ResourceFactory legacy API 全削除 = エンジン内 生 vkAllocateMemory / vkBindBufferMemory / vkMapMemory / vkUnmapMemory / vkFreeMemory ゼロ達成**。 「メモリは全部 VMA」が buffer 側でも完成 (image 側は 2026-05-25 完了済)。 -121 lines net。
> - A6: title_layer s_dbg + pass_chain [BlockDbg] 残骸撤去 (PBR_NORMAL_TEST は既に解消済を確認)
>
> **E (2 commits 4dc8923 + f9c7a3a) + E clean (641abcb)**: Foundations §1 ★★★ camera-relative / floating-origin:
> - 4dc8923: `include/MyEngine/world/engine_origin.h` 新設 (`EngineOrigin::current()` = (0,0,0) 固定) + types.h 座標規約コメント
> - f9c7a3a: prop opaque (DrawData) + camera_system + title_layer で origin 減算 wire-up (~30% カバー)
> - 641abcb (E clean): **toEngineRelative helper を engine_origin.h に追加し全 10 site で wire-up 完了** (main_pass / shadow_pass / reflection_pass / pass_chain grass / static_draw.h 3 関数 / water_pass / particle_pass / debug_line_pass)。 origin == 0 で完全 numeric no-op・floating-origin を ON にした瞬間に prop + skinned + grass + reflection + terrain + water + particle + debug line すべてが lockstep でシフト。 唯一の明示的例外 = TerrainMesh/WaterMesh VB に world 焼きこみなので per-frame は補正済だが km 級 shift では VB 自体の精度低下が起きる (Phase 2F terrain bucket で chunk 境界 rebake)。
>
> **F1-F5 (5 commits c3f46ea / 0f07dc0 / 659bece / 132d0d5 / a46f208)**: Foundations §8.1 固定容量一族の動的化:
> - F1: MaterialRegistry MAX_MATERIALS=256 → INITIAL_CAPACITY + doubling on add overflow + DeletionQueue 経由旧 buffer 安全破棄
> - F2: InstanceBufferPool 8192 → 動的・peakRequested 追跡 + beginFrame 前に grow
> - F3: SkinBufferPool 128 → 動的・per-entity Slot 安定性維持 (boneOffset 不変なので growToDouble で新 buffer 確保 + free-list 拡張)
> - F4: ParticlePass 2048 → 動的・execute 内で alive count 検査 → 必要時 growToFit
> - F5: DebugLinePass 10000 → 動的・line+tri VB pair grow
>
> **G (17d5f8f)**: BindlessTextureRegistry free-list + slot reuse (Foundations §3)。 MAX_TEXTURES=1024 cap は維持 (descriptor pool 成長は別 Phase = G+)、 release/registerTexture で release/reuse 経路。 streaming で texture 入替が working set ≤ 1024 なら無限可。
>
> **N (4f7d47f)**: VK_EXT_memory_priority **実利用** (受け皿のみだったのを完全活性化)。 VMA allocator に `EXT_MEMORY_PRIORITY_BIT` 付与・extension を device に push・feature struct を pNext chain。 各 VmaBuffer/VmaImage factory に priority 設定: createMappedStorageBDA=0.5 / createMappedHostVisible=0.3 / createDeviceLocal=0.8 / VmaImage 全=0.75。 P620 で `[Caps] memoryPriority=1`。
>
> **O (e048503)**: VK_EXT_debug_utils GPU profiling markers。 `include/MyEngine/renderer/debug_utils.h` 新設 (lazy proc address load + DBG_LABEL マクロ + ScopedLabel RAII)。 主要 8 pass (Reflection/Cull1/Cull(Shadow)/Shadow/Main(First)/HiZ/Cull2/Main(Second)/Overlay/Bloom/Post) に label。 buffer/image/pipeline の object 名は follow-up。
>
> **W (df6f5ae + 750135f)**: VK_LAYER_KHRONOS_synchronization_validation 有効化 + 即座に検出された swapchain hazard を修正。
> - df6f5ae: VkValidationFeaturesEXT 経由でレイヤー機能 ON (debug builds)
> - 750135f: post_pass.cpp:279 の UNDEFINED → COLOR_ATTACHMENT barrier の `srcStage = NONE` を `COLOR_ATTACHMENT_OUTPUT_BIT` に変更 (queue submit が imageAvailableSemaphore を COLOR_ATTACHMENT_OUTPUT で待つので、 barrier も同 stage で揃えて execution dependency chain を closed-loop に)。 vkAcquireNextImageKHR との WRITE_AFTER_READ hazard 撲滅。
>
> **C (e7b852e)**: Foundations §2 a-2。 vulkan_context が transfer-only family を query (TRANSFER bit + GRAPHICS/COMPUTE clear)・fallback は graphics family。 P620 で **family 1 = dedicated transfer 検出**。 transferQueue()/hasDedicatedTransfer() getter。 streaming 着手前 (Phase 2F) の隠れ前提を closed。
>
> **I (8484ea7)**: VK_EXT_memory_budget enable + VMA allocator bit + memoryBudget() getter。 vmaGetHeapBudgets が driver-live 値を返す。 P620 で `memoryBudget=1`。
>
> **B (3670ef1 + eeba2ed)**: timeline semaphore 受け皿 → 実 migration。
> - 3670ef1: vk12Features.timelineSemaphore = TRUE + cap getter (受け皿)
> - eeba2ed: **FrameSync の per-frame VkFence array → 単一 timeline semaphore に migration**。 frameTimeline_ (initialValue = MAX_FRAMES_IN_FLIGHT) + nextSignalValue_。 acquireNextImage で vkWaitSemaphores・submitAndPresent で VkTimelineSemaphoreSubmitInfo chain で signal。 binary semaphore (imageAvailable / renderFinished) は swapchain 用に残置。 副次効果 = sync_validation が flag していた **20 件の CullingPass cross-frame WRITE_AFTER_READ/WRITE hazards が全部解消** (per-frame buffer 再利用が timeline 経由で explicit になった)。
>
> **D+L+K+T+Z+J+Q (2da80b9 受け皿 → f880ddb 活性化)**: 8 receptacles → 5 実 enable。
> - 2da80b9: VK_EXT_extended_dynamic_state3 / VK_EXT_shader_object / VK_KHR_present_id / VK_KHR_present_wait / VK_EXT_swapchain_maintenance1 / VK_EXT_image_view_min_lod / VK_EXT_host_image_copy / VK_KHR_calibrated_timestamps を query (cap getter のみ・extension は未 enable・feature struct 未 chain)
> - f880ddb: そのうち **L/K/K/Z/J/Q = 5/7 を実 enable**。 deviceExtsVec に push + 各 feature struct を pNext chain (appendToChain lambda 経由)。 J は 1.4 promotion で vk14Features.hostImageCopy = TRUE。 これで vkCreateShadersEXT / VkPresentIdKHR / vkWaitForPresentKHR / VkImageViewMinLodCreateInfoEXT / vkCopyMemoryToImage / vkGetCalibratedTimestampsKHR が callable。 T (swapchain_maintenance1) は instance ext VK_EXT_surface_maintenance1 依存で保留、 D (EDS3) は 30+ feature 個別 query 要で保留 (commit メッセージで明示)。
>
> **U (fdbddda)**: Foundations §2 ★★★。 `include/MyEngine/core/job_system.h` header-only。 JobSystem::init(workerCount) で std::thread N 本起動 + condition_variable + packaged_task queue。 init(0) で **inert-friendly** (submit が calling thread で inline 実行) なので既存単一スレッド経路は無改修。 Phase 2F streaming 着手時に worker pool を起動するだけで async asset load / decode / chunk eviction が動く。
>
> **M (8b4deff)**: AsyncCompute timeline semaphore receptacle。 `include/MyEngine/renderer/async_compute.h` header-only。 AsyncComputeContext が単一 timeline semaphore を所有・nextValue() で monotonic 値払い出し。 P620 (asyncComputeFamily=2 dedicated=1) で HZB + cull pass2 を graphics queue と並列実行する将来 wiring の土台。 cross-queue 実 submission は Phase 仕事。
>
> **V/R/S/H/X/Y/P (8604de5)**: 7 rendering-technique receptacle (header-only design memos)。 全て init()/shutdown() 空関数 + 詳細な per-Phase activation 設計コメント:
> - V: auto_exposure.h (luminance histogram compute + EMA-smoothed exposure・modern HDR essential)
> - R: taa_history.h (HDR ping-pong target・全 temporal 技法共通)
> - S: gpu_skinning.h (persistent skinned VB + compute pre-pass・1000+ chars で 3x cost saving)
> - H: persistent_object_buffer.h (CullObject + DrawData を GPU 永続化・毎フレ CPU rebuild 脱却・Foundations §4)
> - X: sky_atmosphere.h (Hillaire 2020 4 LUTs・UE5 / Frostbite 標準)
> - Y: decal_pass.h (screen-space projected decals・dynamic_rendering_local_read 活用)
> - P: bc7_texture.h (block-compressed loader + offline tool dispatch・2 GB VRAM 守備)
>
> ※ V-P 7 件は **完全に header-only stub**で実 runtime コードはゼロ。 commit メッセージにそう書いてあり、 audit でも確認済。 Phase 着手時に init/shutdown 本体を埋める前提。
>
> **正直な妥協度評価** (audit 結果):
> - 🟢 **完全採用** (legacy 0 / modern active): sync2 / dynamic rendering / VMA / BDA / bindless / indirect / persistent pipeline cache / Timeline semaphore / camera-relative / debug markers / sync validation / memory priority+budget = **17 項目**
> - 🟡 **受け皿のみ** (Vulkan feature 取得済だが API 関数 0 呼出): shader_object / present_id/wait / image_view_min_lod / host_image_copy / calibrated_timestamps / dynamic_rendering_local_read / pipelineCreationCacheControl flag / mailbox present mode / GPU compute skinning = **9 項目** (Phase 着手時に活用)
> - 🔴 **未着手** (痕跡ゼロ): EDS3 / swapchain_maintenance1 / DGC 実装 / mesh shader / RT / VRS / V-P 7 stub の本実装 = **14 項目**
>
> **P620 [Caps] log (実走測定)**: 30 capability ほぼ全て =1 (DGC のみ 0 = Pascal hardware 制約)。 `multiDrawIndirect drawIndirectFirstInstance synchronization2 drawIndirectCount dynamicRendering separateDepthStencilLayouts subgroupOps subgroupSize samplerFilterMinmax asyncComputeFamily (dedicated) transferFamily (dedicated) dynamicRenderingLocalRead pipelineCreationCacheControl maintenance5 maintenance6 graphicsPipelineLibrary pipelineBinary memoryPriority memoryBudget timelineSemaphore extDynState3 shaderObject presentId presentWait swapchainMaint1 imageViewMinLod hostImageCopy calibratedTimestamps`。
>
> **次の推奨の一手** (どこから着手しても他 Package を阻まない):
> - **★★★** mailbox present mode + K activation = frame pacing 完成 (現状 FIFO のみ)
> - **★★★** pipelineCreationCacheControl 活用 = streaming hitch 検出機構を ON
> - **★★** L (shader_object) で VkPipeline 撤廃の本実装 = modern triad 完成
> - ✅ S (compute skinning) 本実装 = Phase 2G-1/2a 完了 (HEAD 1b57f8e・skin once 達成)。 2G-2b PART0-2 完了 (HEAD e79de3c)・残 PART3-4 ★次推奨
> - **★★** B 副次仕上げ: AsyncComputeContext を実 cross-queue submit に wire-up (M activation)
> - **★** Z + G+ (descriptor pool grow) = texture mip streaming 完成
> - **★** Q calibrated_timestamps + GPU profiling = frame budget 計測の本実装
> - **★** V-P stub の本実装は per-Phase (Phase 2D = R/V、 Phase 2F = H/U/M、 Phase 3 = X/Y/J、 etc.)
> - 任意で 1I PART D / 2A 多光源 / 2C LOD / 2F terrain bucket は依然候補
>
> 着手手順: §0-2 復唱 (ゴール ← §1 / 設計方針 ← §1.5 / 現在地 ← §2 / スコープ境界) + §0-8 (資料を引いてから判断・記憶で動かない) を毎回守る。 Vulkan/グラフィックス話題は **着手時にまず最新動向 web 確認**。 怖い変更 (types.h struct size 変更等) は §3-1a clean rebuild。
>
> **新規ファイル一覧 (このセッションで追加)**:
> - `include/MyEngine/world/engine_origin.h` (E receptacle + toEngineRelative helper)
> - `include/MyEngine/renderer/debug_utils.h` + `src/renderer/debug_utils.cpp` (O VK_EXT_debug_utils helpers)
> - `include/MyEngine/renderer/async_compute.h` (M receptacle)
> - `include/MyEngine/renderer/auto_exposure.h` (V stub)
> - `include/MyEngine/renderer/taa_history.h` (R stub)
> - `include/MyEngine/renderer/gpu_skinning.h` (S stub)
> - `include/MyEngine/renderer/persistent_object_buffer.h` (H stub)
> - `include/MyEngine/renderer/sky_atmosphere.h` (X stub)
> - `include/MyEngine/renderer/decal_pass.h` (Y stub)
> - `include/MyEngine/renderer/bc7_texture.h` (P stub)
> - `include/MyEngine/core/job_system.h` (U receptacle)
>
> ---
>
> **【参考: 前 chunk の引き継ぎ (履歴)】 — PART4 §6 4c 完了 = two-pass HZB occlusion 本体 + Tier 1 α (Nanite/Granite 2024 baseline) + 1-tap minmax fast path / 2026-05-29, 8 commits = ad97879 / 477985d / f242327 / 7e446a9 / e41cfd7 / 91a6885 / 2f7daf9 / ccf5c03】**
> - **4c-A (ad97879)**: half-extent (`CullObject.extentDrawId.xyz`) 充填 + `HiZPass::previousPyramidView()` accessor (受け皿)。
> - **4c-B (477985d)**: capability getters (`samplerFilterMinmax`, async compute family) + `HizParams` BDA buffer + cull.comp helpers (`aabbScreenBounds` / `mipFromScreenSize`)・gate-off で dead-code 配置。
> - **4c-C (f242327)**: full machinery — HiZPass `minReductionSampler` (samplerFilterMinmax 有効時に reductionMode=MIN) + `ensureAllSlotsInGeneral` + per-drawId 永続 1bit `visHistory` buffer + descriptor set 0 (HZB samplers) + cull.comp pass1/2 paths + main_pass `Pass enum` + loadOp 分岐。 gate-off で merge。
> - **4c-D (7e446a9)**: **two-pass HZB occlusion 活性化**。 pass_chain 順序 = `Cull(pass1) → MainPass(FirstOpaque) → HiZPass → depth barrier(ATTACHMENT_OPTIMAL → READ_ONLY_OPTIMAL) → Cull(pass2) → MainPass(SecondAndNonOpaque)`。 main_pass の SecondAndNonOpaque は `loadOp=LOAD` で pass1 の HDR + depth を保存。 visHistory writeback で次フレ pass1 が前フレ可視オブジェクトのみ早期 cull。
> - **fix #1 (e41cfd7)**: HZB descriptor set に `UPDATE_AFTER_BIND` flag 追加。 4c-D 直後の起動で multi-dispatch 中の `vkUpdateDescriptorSets` が validation flood (`VkDescriptorSet was destroyed or updated without UPDATE_AFTER_BIND`) → bindings + layout + pool 3 箇所に flag 追加で解消。 bindless texture array と同じ pattern に統一。
> - **fix #2 (91a6885) — ユーザー目視発見**: 4c-D 直後の起動で「青背景が黒化・宝箱/墓/壁が消失」(スクショ確認)。 原因は main_pass `SecondAndNonOpaque` 経路で常に走っていた `toAttach` barrier (`oldLayout=UNDEFINED → COLOR_ATTACHMENT`) が pass1 の HDR clear + cleared depth を破棄していた。 `if (info.pass != Pass::SecondAndNonOpaque)` で skip して解消。 ユーザー目視「画面正常」確認。
> - **Tier 1 α 活性化 (2f7daf9)**: 4c 時点の pass1 は「prev_vis のみ」(Maister 2018 simpler 形式) だったのを、 pass1 でも `hzbPrev` を sample して `prev_vis && frustum && cone && !hzbOccluded(hzbPrev)` に格上げ = **Nanite/Granite 2024 baseline** 適合。 hzbPrev 受け皿は 4c-A から立っていたので shader 1 行追加で済む。
> - **1-tap fast path (ccf5c03)**: cull.comp `hzbSampleMinR` に spec const `kHzbMinReductionFastPath`。 `samplerFilterMinmax=1` の device では reductionMode=MIN sampler 経由で 1 `textureLod` = `min(2x2)` を取り出す (4-tap fallback あり)。 P620 (=1) で fast path 自動選択。
> - **検証**: ユーザー目視「画面正常」(全 prop / 反射 / shadow / grass / Player 正常)。 validation エラー 0 (chest 開封時の transparent MRT warning は 4a-2 由来で別軸)。 `[Caps] samplerFilterMinmax=1 asyncComputeFamily=2 (dedicated=1)`。
>
> **【PART4 4d 大半完了 = audit-driven 最新技術取り込み 10 commits / 2026-05-29】**
> 設計時の「2-3 commit」想定を、 user 主導の audit (「最新技術を取りこぼしている箇所がないか」) で複数 round 細分化して進めた。
>
> **α + β (audit 第 1 弾)**:
> - **α (082d792)**: 4b Obs B 解決。 `HiZPass::initialTransitionToGeneral` の `VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT` → `VK_PIPELINE_STAGE_2_NONE` (sync2 best practice for UNDEFINED→X)。
> - **β**: 上記 1-tap fast path (ccf5c03)。
>
> **γ × 3 (audit 第 1 弾・dynamic rendering 完全移行)**:
> - **γ-1 (4b9c32c) PostPass**: VkRenderPass + VkFramebuffer 撤去・swapchain `UNDEFINED → COLOR_ATTACHMENT → PRESENT_SRC_KHR` 明示 barrier。
> - **γ-2 (da74526) ShadowPass**: depth-only dynamic rendering 化・`ShadowPipelineParams` に `VkPipelineCache` 追加で M1 も同時配線。
> - **γ-3 (33e1511) ReflectionPass**: dynamic rendering 化 + **bonus**: startup の outNormal/outMotion validation warning 4 件が消滅 (reflection が triangle.frag を 1-attachment renderPass で作っていた legacy mismatch が解消)。
> - **結果**: engine 全体 (src/renderer grep) で **VkRenderPass / VkFramebuffer の実 API 使用ゼロ** = Vulkan 1.3 dynamic-rendering native。
>
> **M × 3 (audit 第 2 弾・最新 1.3/1.4 core 取り込み)**:
> - **M3 (47c3571)**: `VK_KHR_dynamic_rendering_local_read` (Vulkan 1.4 core) capability 受け皿 = Phase 3 SS 効果 / tile-based fast path。
> - **M1 (a62b7f0)**: **persistent VkPipelineCache** (Vulkan13 §3 Y closed)。 `<AppData>/MyEngine/MyEngine/pipeline.cache` に load/save、 全 14 vkCreate*Pipelines callsite が `ctx_->pipelineCache()` 経由。 user clean exit で 490KB 書き出し実証。
> - **M2 (fcef5ab)**: `depth_layouts.h` ヘルパ撤去 → sync2 generic `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL` / `READ_ONLY_OPTIMAL` 一括置換 (19 callsites + ヘッダ削除・コード -45 行)。
>
> **N × 4 (audit 第 3 弾・pipeline 周辺最新化)**:
> - **N1 (7298968)**: `pipelineCreationCacheControl` (Vulkan 1.3 core) enable・open-world streaming で `VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT` 経由の hitch 防止受け皿。
> - **N4 (c01c2e5)**: **`VkPhysicalDeviceVulkan14Features` chain 追加** (engine が API 1.4 で動作中なのに 1.4 features を一切 enable してない構造欠陥を修正)・`maintenance5` / `maintenance6` enable・M3 の KHR struct を canonical 1.4 form に統一。
> - **N2+N3 (1481049)**: `VK_EXT_graphics_pipeline_library` + `VK_KHR_pipeline_binary` capability 受け皿 (extension name walk による query のみ・activation は Phase 2A/3 で)。
>
> **P620 最終 [Caps] log (audit 完了時実測)**:
> ```
> multiDrawIndirect=1 drawIndirectFirstInstance=1 synchronization2=1
> drawIndirectCount=1 deviceGeneratedCommands=0 dynamicRendering=1
> separateDepthStencilLayouts=1 subgroupOps=1 subgroupSize=32
> samplerFilterMinmax=1 asyncComputeFamily=2 (dedicated=1)
> dynamicRenderingLocalRead=1 pipelineCreationCacheControl=1
> maintenance5=1 maintenance6=1 graphicsPipelineLibrary=1 pipelineBinary=1
> ```
> **18 capability 中 17 が =1** (Pascal P620 で対応・DGC のみ 0 で fallback 経路あり)。
>
> **4d で残った仕事 (PART4 完了を阻まない・別 commit で着手可)**: DGC 経路 **実装** (Pascal 非対応で実 device 必要) / Shader Object 経路 (`ShaderProgram` 抽象) / Descriptor Buffer 経路 (Pascal 強制無効化ロジック) / Timeline semaphore (`FrameSync` 内部経路分岐) / Async compute での HZB / cull 並列実行 (Foundations §2 と一緒) / `[BlockDbg]` / `[Cull2B]` 一時ログ掃除 / transparent MRT mismatch fix (chest 開封時 outNormal/outMotion noise・4a-2 由来) / 4b Obs C (R32G32_SFLOAT storage image format properties query) / 4b Obs D (subgroup ID → linearIdx canonical mapping)。 ※ ~~`lastCullGpuVisible_` HUD GPU readback or 純 GPU-driven 化 readback 撤去~~ は ✅ commit f8d1e1f で完了済 (option B = 純 GPU-driven 化)。
>
> **【次の推奨の一手】PART4 はここで完了とみなして本書 (START_HERE / Roadmap §4 の 2B 発展節 + 付記 / 依存マップ Hi-Z ノード / Codebase_Guide §3.5 / Work_Protocol §5f) への畳み込み (§8) → その後 2C LOD (P620 を救う) / 2A 多光源 (clustered forward+) / Phase 2F terrain bucket / 1I PART D (bloom ノブ settings 連携) のいずれか着手**。 「畳み込みなしで次 Phase へ」も可だが、 PART4 の知見 (sync2 generic layouts / persistent pipeline cache / visHistory / two-pass HZB) は Codebase_Guide / Work_Protocol に書き残さないと次の Phase で再発明する。
>
> ---
>
> **【参考: 前 chunk の引き継ぎ (履歴)】 — PART4 4b 完了 = HiZPass 新設 (SPD-style single-dispatch min+max RG32F pyramid) / 2026-05-28, commit ffe9673】**
> - **新規 `renderer/hiz_pass.{h,cpp}` + `shaders/hiz_spd.comp`**。 main_pass の swapchain depth (4a-2 で `SAMPLED_BIT` + `DEPTH_READ_ONLY_OPTIMAL` post-barrier 済み) を入力に、 per-frame (MAX_FRAMES_IN_FLIGHT=2) の RG32F mip chain (.r=min / .g=max・Design §3.3-N) を **1 dispatch** で生成。 アルゴリズムは AMD FidelityFX SPD のタイル + atomic counter パターンを GLSL でハンドロール (FFX SDK 外部依存なし・MIT-style 同等の単一 dispatch を実現)。
> - **per workgroup = 256 threads / 64×64 source tile**。 group 内で mip 0..5 (32×32→16×16→8×8→4×4→2×2→1×1 per group) を LDS 経由で生成。 全 group が `atomicAdd` で counter を競い、 最後に書き込んだ group が mip 6..N (1280×720 設定で N≈10) まで継続。 `coherent` qualifier + `memoryBarrierImage()` で inter-group 可視性確保。
> - **新規 viewer**: `renderer/hzb_debug_widget.{h,cpp}` (右下ドック、 frame / mip スライダ、 RG32F の .r=赤 .g=緑 で min/max を同時表示)。 4a-2 の `gbuffer_debug_widget` と並ぶ独立クラス、 ImGui::Image で pyramid mip 個別 view を表示。 GENERAL レイアウトのまま sample (Vulkan 仕様で許可)。
> - **capability 追加 (Design §1.5-C 「実測してから書く」)**: `subgroupOps` (basic + **shuffle** in COMPUTE・wave shader が `subgroupShuffleXor` のみ使うので ARITH/QUAD は要求しない)・ `subgroupSize`・ `shaderStorageImageArrayDynamicIndexing` (HiZPass の per-mip storage image array を loop-uniform 動的 index で引くため必須)。 P620 実測: `subgroupOps=1 subgroupSize=32`。 **二派生 spv**: `hiz_spd.comp` (LDS-only、 全 GPU 対応 fallback) + `hiz_spd_wave.comp` (Phase C を `subgroupShuffleXor(_, 1) / (_, 16) / (_, 17)` 経由・subgroupOps && subgroupSize >= 32 で選択、 P620 では wave 経路選択)。 Phase D-F は両派生とも LDS (8x8 以下の reduction は subgroup 境界を越えるため)。
> - **pass_chain 配線**: `mainPass_.execute()` 直後・ `overlayPass_.execute()` の前 (depth が `depth_layouts::readOnly(*ctx_)` に遷移済みのタイミングで sample、 mid-pass barrier 不要のクリーン経路)。 ExecuteInfo は `cmd + frameIndex` のみ。 `onSwapchainResized` で pyramid を depth 解像度に追従させて rebuild (pipeline / set layout は extent-independent で keep)。 atomic counter は **device-local** `VmaBuffer::createDeviceLocal(STORAGE | TRANSFER_DST)` (vkCmdFillBuffer でリセット・BDA/host-visible 不要)。
> - **同期 (Vulkan Memory Model 整合)**: last group 継続ループは `memoryBarrierImage()` (cross-group acquire 前) + `groupMemoryBarrier() + barrier()` ペア (within-group iteration、 device-scope より軽量)。 `barrier()` 単独では coherent storage-image の cross-invocation 可視性が無いため image-memory fence 必須。
> - **検証**: `Vulkan ready` + 全 capability 検出 (`subgroupOps=1 subgroupSize=32 dynamicRendering=1 separateDepthStencilLayouts=1`)・ HiZ 関連 VUID/leak/device-lost **ゼロ**・ pre-existing validation warning は 4a-2 由来の outNormal/outMotion 1-attachment subpass 警告のみ。 描画見た目 (HZB viewer の表示・min/max が mip ごとに保守的に縮約) は **ユーザー目視 OK 確認済み**。
> - **ソース review で見つかって修正したバグ** (commit に含む): (1) depth descriptor の固定 layout を `depth_layouts::readOnly(*ctx_)` 経由に (`separateDepthStencilLayouts` 非対応 device での VUID 防止)、 (2) last group ループの image-memory fence 追加、 (3) subgroup capability query を BASIC+SHUFFLE に修正、 (4) atomic counter を device-local 化、 (5) `kMaxFramesInFlight` を `FrameSync::MAX_FRAMES_IN_FLIGHT` 参照に。 詳細は side/MyEngine_HiZ_PART4_Design.md §6 4b 節。 残る Obs B/C/D (Vulkan Memory Model 厳密化 / RG32F format query / subgroup mapping 厳密化) は別 commit で 4d 以降にやる予定。
> - **types.h は触っていない (FrameUBO 含め struct size 不変) ので §3-1a clean rebuild は不要だった**。 incremental build で通過。 ※ 4a-2 では FrameUBO に prevViewProj を足して clean rebuild を怠った事故があったが、 4b では struct 変更なしの追加で済んでいる。
>
> **【PART4 4a-2 完了 = depth-normal-motion MRT 受け皿 + OverlayPass 分離 / 2026-05-28, commit ed0d80e】**
> - **main_pass の opaque を MRT 3 attachment (HDR + GBuffer normal R10G10B10A2 octahedral + motion vector RG16F) に拡張**、 swapchain depth は SAMPLED_BIT 付き = Phase 3 SS 効果 (SSAO/SSGI/SSR/DoF/TAA) と PART4 4b HZB が必要な受け皿が全部揃った。Phase 3 着手時の shader 大改修ゼロが目標。
> - **HUD/ImGui は OverlayPass という新しい dynamic-rendering pass に分離** (main_pass の MRT scope に巻き込まれない形)。HudPipeline / ImGui pipeline は `depthAttachmentFormat = VK_FORMAT_UNDEFINED` で rebuild、 OverlayPass の BeginRendering は color-only (HDR LOAD) = mid-pass barrier も feedback-loop hazard も無いクリーン経路。
> - **新規ファイル**: `overlay_pass.{h,cpp}`、`gbuffer_debug_widget.{h,cpp}` (normal/motion/depth を ImGui::Image で右上ドック表示)、`depth_layouts.h` (separate vs combined depth/stencil layout 選択の一元化)、`shared/gbuffer.glsl` (octahedral encode + motion vector helper)。
> - **capability 追加**: `dynamicRendering` (Vulkan 1.3 core) と `separateDepthStencilLayouts` (Vulkan 1.2 core, optional) を query して有効化。後者未対応では `DEPTH_STENCIL_*_OPTIMAL` combined にフォールバック (`depth_layouts::attachment/readOnly` 内で吸収)。`[Caps] ... dynamicRendering=1 separateDepthStencilLayouts=1` (P620 実測)。
> - **FrameUBO に `mat4 prevViewProj`**: opaque vertex shader が `prevClip = prevViewProj * model * pos` を計算し、 fragment が NDC delta を motion vector RT に書く。host 側で前フレーム VP をスナップショット。
> - **教訓 (Work_Protocol §3-1a 違反した)**: 共有ヘッダ struct size 変更 (FrameUBO に prevViewProj 追加 = 352B → 416B) で **clean rebuild 必須だったのに incremental build を続けた**結果、 mode select → 本編 startGame 遷移時のみ画面凍結 (TDR、 validation 無音) を起こした。 当初は depth layout / OverlayPass / GBuffer feedback loop を疑って多数の修正を入れたが効かず、 最終的に clean rebuild が決定打となって解消。 根本原因の仮説 (FrameUBO レイアウト不一致で gl_Position が NaN/巨大値になる) は Work_Protocol §3-1a 事例②参照。 §3-1a の徹底必須。
>
> **【PART4 4a-1 完了 = main_pass を Vulkan 1.3 dynamic rendering 化 / 2026-05-28, commit af3dd72】**
> - 4a-2 の MRT 拡張前提として VkRenderPass + VkFramebuffer を撤去、 `vkCmdBeginRendering` + `VkPipelineRenderingCreateInfo` ベースに移行。子 pass (debug_line / hud / particle / water / imgui) も同経路に伝播。 attachment format 変更を VkRenderPass 再構築なしで吸収できる「最新 Vulkan 標準形」 (vkguide v1.3 / Khronos Vulkan-Samples)。
>
> **【PART4 4-前-5 完了 = GPU-driven shadow / 2026-05-28, commit 986ba44】**
> - ShadowPass の static draw を CullingPass の Shadow CullSet (新 enum `CullSet{Camera, Shadow}` で per-set output buffer 化) + `vkCmdDrawIndexedIndirectCount` 経由に。 main bucket と同形の compaction を shadow にも適用。 skinned shadow は legacy CPU loop 維持。 vkguide サンプル (main+shadow 両方 GPU-driven) と揃った。
>
> **【ここまでの全体像】**
> - PART4 §6 の **4-前-0 〜 4-前-5 + 4a-1 + 4a-2 = 完了**。 これで Hi-Z occlusion 本体 (4b HZB SPD → 4c two-pass cull) に必要な受け皿 (Reverse-Z + 動的容量 + meshlet-ready CullObject + scan compaction + DGC/IndirectCount フォールバック + GPU-driven shadow + depth/normal/motion MRT prepass + dynamic rendering + OverlayPass + separate depth layouts) が全部立っている。
>
> **【PART3c のスコープ確定 — 4a-2 で前提が満たされた今でも重要】PART3c で GPU-driven 化するのは prop (cube + Model) だけ。terrain は対象外。** terrain は START_HERE の GPU-driven 完成形アーキテクチャ通り「**別 bucket** (専用 GeometryBuffer + 専用 cull + splat マテリアル経路 + 距離 LOD)」として、**ストリーミング Phase で**別途やる (依存マップ §0 の土台 side「ストリーミング」層)。地形マテリアル (土・岩・砂) は terrain bucket の splat / マテリアルブレンドで扱う (現状1種類でも受け皿は terrain bucket 側)。**prop と terrain を同じ GeometryBuffer に混ぜない** (寿命・サイズ・アロケーション粒度が違い断片化する)。
>
> **【2B PART3c-2 完了 = prop の GPU-driven 骨格が立ち上がった / 2026-05-27, commit 1cf23b9】**
> - **prop opaque の CPU draw ループを `vkCmdDrawIndexedIndirect` に差し替え、CPU draw を撤去した。** PART2 で作った CullingPass (GPU がフラスタムカリングして instanceCount=0/1 を生成済み) が、これで**初めて実描画に接続**された = prop bucket の GPU-driven 骨格が完成。main は CullingPass の `commandBuffer(frameIndex)` を indirect 描画ソースにし、GPU が instanceCount==0 の draw を自動スキップ = カリングが実描画に効く。
> - **能力チェック (VulkanContext で実測)**: `multiDrawIndirect` と **`drawIndirectFirstInstance`** を起動時に query して対応時のみ有効化。**P620 は両方 1 (対応) を実測確認**。`[Caps] multiDrawIndirect=1 drawIndirectFirstInstance=1`。
>   - **重要な確定事実**: 我々は firstInstance に DrawData slot を載せて `gl_InstanceIndex` で引くので、**`drawIndirectFirstInstance` が必須** (`multiDrawIndirect` だけでは足りない。non-zero firstInstance を持つ indirect コマンドにはこの feature が要る。web/Vulkan 仕様で確認)。非対応なら indirect で firstInstance を 0 に強制され slot 機構が壊れるため、その場合は CPU draw ループにフォールバック (direct draw の firstInstance は無制限)。
> - **block 分布は散在 (実測 blockSwitches≈17-18 / draws=77)**。よって描画は **「同一 GeometryBuffer block の連続区間ごとに 1 回の draw 呼び出し」**: `multiDrawIndirect` 対応なら区間長ぶんの単発 MDI、非対応なら区間内を drawCount=1 の indirect ループ。**「全 draw を1回の MDI」は不可** (block 切替で vertex/index buffer の bind が変わるため、区間で割るのが必須)。呼び出し回数を block ごと 4 本に減らすのは builder で blockIndex 順にソートする**任意の後段最適化** (今は不要。compaction と同じ「構造を変えず層を足す」)。
> - **検証 (HUD)**: PART3c-2 当時はデバッグ HUD に `Cull : <可視> / <総数>` を追加。視点を回すと分子が動く = GPU カリングが実描画に効いている。描画は prop 全部正常・validation/VUID/leak ゼロ・目視 OK。**当初 GPU=CPU オラクル照合を HUD に出したが、`lastGpuVisible` (前フレーム dispatch) と CPU オラクル (今フレーム) の基準が 1 フレームずれて高速移動時に偽の不一致 (赤) が出たため撤去** (looks fine ≠ correct とは別問題で、表示比較側のズレ)。CullingPass の `lastCpuVisible()` (PART2 由来) は残置・無害として残していた。 → **【現状 commit f8d1e1f で全撤去】**: 4-前-4 (15b89ad) で compactCmd device-local 化以降 readback 経路が断たれて HUD は永久 0 だったため、 純 GPU-driven 化 (option B) で HUD `Cull` 行 + CPU Frustum オラクル + 全 wire-up を撤去 (Codebase_Guide §3 「検証足場 (HUD)」項参照)。
> - **現在の描画経路**: prop opaque = **indirect (GPU-driven)**。terrain = legacy CPU draw (`drawTerrainList`、別 bucket=Phase 2F 待ち)。grass/skinned/transparent/reflection = 従来経路 (PART3b の per-draw SSBO 経路)。
>
> **次にやること = PART4 4b 完了。次は 4c (two-pass occlusion 本体) 着手**:
> - **PART4 4c** (two-pass occlusion 本体 + AABB 遮蔽 + cull.comp 拡張) — 4b の HZB を読んで遮蔽判定する核。 `CullingPass` に `executePass1` (前フレーム可視のみ frustum + 圧縮) / `executePass2` (全 object + HZB occlusion + 圧縮 + visBuf 更新) を分離・ pass_chain が「パス1 cull → パス1 draw → 4b HZB 生成 → パス2 cull → パス2 draw」をオーケストレート。 `static_cull_build.h` で `CullObject.extentDrawId.xyz` (world AABB half-extent) を充填、 cull.comp が AABB 画面投影 → mip 選択 → HZB の occluder 値と比較。 設計詳細: side/MyEngine_HiZ_PART4_Design.md §6 「4c」・ §3.2-F / §3.1-C。
> - **PART4 4d** (能力ゲート集約 + DGC/Shader Object/Descriptor Buffer/Timeline semaphore/Async compute 受け皿 + RenderTarget 抽象 + 一時ログ掃除)。
> - **任意 / 並行可**: 2C LOD (P620 を救う) / Phase 2F terrain bucket / 1I PART D (bloom ノブ settings 連携) / buffer 系 VMA 化 / 2A 多光源。
> - ~~純 GPU-driven 化の仕上げ (任意): CullingPass の CPU オラクル + readback (PART2 検証用) を撤去し、CPU が可視数を知らない形にする。~~ ✅ **完了 2026-05-29 commit f8d1e1f** (HUD `Cull` 行 + 全 wire-up 撤去・8 files +2 -51)。
> - **4b の中身高速化 (済)**: `hiz_spd_wave.comp` 派生で Phase C を `subgroupShuffleXor` 経由に。 P620 (subgroupSize=32) では wave 経路自動選択。
> - **4b 未対応の改善余地 (Obs B/C/D・別 commit 候補)**: いずれも P620 では実害ゼロ。 詳細は side/MyEngine_HiZ_PART4_Design.md §6 「4b 完了後の残作業」。 (a) **Obs B**: 初期 layout 遷移を `VK_PIPELINE_STAGE_2_NONE` に (sync2 best practice・4d で他 pass barrier 統一時に巻き取り推奨)。 (b) **Obs C**: `VK_FORMAT_R32G32_SFLOAT` storage image の format properties query + fallback (Vulkan optional feature)。 (c) **Obs D**: wave shader の subgroup ID → linearIdx canonical mapping 前提を `REQUIRE_FULL_SUBGROUPS` で固定 (mobile 対応時に)。
>
> **着手手順 (毎回守る)**: (1) §0 を読む (特に §0-8 = スコープ判断は資料確認してから・記憶で決めない)。(2) Vulkan/グラフィックスの話なので**着手時にまず最新動向を web 確認**。(3) 関わる実ソースを**推測せず dump して確認**してから設計を出す。**特に「○○は前の PART で対応済み」という思い込みを確認なしで使わない** (3c で「terrain は geom 化済み」と思い込み、ワールド地形が legacy のままなのを確認せず prepared 経路に乗せて描画破壊した)。(4) 細かく commit。
>
> **PART3a で踏んだ地雷 (同じ轍を踏まないため。詳細は Work_Protocol §5b/§5c)**:
> - **現状基準で固定容量を決めるな**。最初 GeometryBuffer を 1M 頂点固定にして全アセットが載らず armor 等が消えた。開発中はアセットが増え続けるので、容量は動的に伸びる構造 (multi-block) にする。今後 instance / draw command / SSBO の上限を置くときも同じ。
> - **VmaVirtualBlock は要素単位 (頂点数/index数) で使う**。Vertex stride 76B は非2べき乗で、byte 単位で渡すと返るオフセットが 76 の倍数にならず /76 で切り捨て → 範囲外描画 → device lost。size も alignment も要素単位 (alignment=1) にする。
> - **同期コピーの staging は即破棄、DeletionQueue は drawFrame 中の動的 free 専用**。alloc は起動時 (drawFrame 外) に呼ばれるので staging を DeletionQueue に積むと collectFrame が走らず溜まって VRAM 枯渇 → device lost。copyBufferRegion は内部で vkQueueWaitIdle 同期するので staging は即 reset。
> - **デバッグ作法**: validation layer が無言で vkQueueSubmit failed になる device-lost は、二分法 (容疑箇所を強制的に単純化して再現するか見る) + 自前の割り切れ/範囲 DIAG ログで切り分けた。GPU-AV は P620 では内部エラーで自滅して使えない。
> - **ログは run.log にリダイレクトしてから Grep する。先頭 N 行で打ち切らない** (頻発ログに埋もれて見落とす。実際に古い exe のログを見て誤った判断をした)。exe 実行は必ずプロジェクトルート (`C:\MyEngine`) を CWD にする。VsDevShell は `cd` を挟むと環境が一部リセットされ `stdarg.h not found` でビルド失敗することがある → その時は VsDevShell を再読込してからビルド。ビルド後は exe の LastWriteTime が今に更新されたか必ず確認 (古い exe を実行して誤判断しない)。

>
> **PART3b で踏んだ罠 (2026-05-27)**:
> - **per-draw SSBO を複数パスで共有するときは、cursor リセット (beginFrame) を全消費者より前に1回だけ置く**。reflection→main の順で同じ DrawDataPool を消費するのに、beginFrame を grass の所 (=reflection の後) に置いていたため、main 充填が reflection の slot を上書きし、GPU 実行時に reflection が main のデータを読む状態になっていた。**目視では偶然正しく映って気づけなかった** (bloom Off レイアウトバグと同類: looks fine ≠ correct)。beginFrame を ShadowPass 後・reflection ブロック前 (反射 On/Off 共通で必ず通る位置) へ移動して解消。slot は reflection [0..Nrefl) → main [Nrefl..) の連続割り当て。
>
> 詳細は下記「直近の完了」の PART3a 完了ブロック、Codebase_Guide §2 (geometry_buffer/deletion_queue) + §3.5、Work_Protocol §5b/§5c、依存マップ §6。

- **直近の完了**:
  - **Phase 2G-1 + 2G-2a 完了 (2026-05-30, HEAD 1b57f8e・user runtime 検証済)**: skinned を §1.5-B 「すべて GPU-driven」へ寄せる前半。
    - **2G-1** (560e182 / f7085aa / 8cbf82a): batched compute skinning pre-pass。 `skinning.comp` (1 dispatch・flat global-vertex-id binary search) が SkinInstance[] (SkinInstancePool host-mapped) を読み、 model-local の skinned 頂点を deinterleaved stream (pos fp32 ping-pong + normal oct16・SkinnedVertexPool device-local) に書く。 SkinningPass (descriptor-less compute・sync2 barrier) を reflection 前に dispatch。 受け皿 `gpu_skinning.h` (S) の per-mesh 固定誤設計を per-instance `SkinnedVertexPool` に是正。 F12 burst の grow-mid-loop device-lost を pre-grow-once で修正。
    - **2G-2a** (1b57f8e): skinned main/shadow/reflection を passthrough 化。 `triangle_skinned.vert` / `shadow_skinned.vert` が skinned stream を BDA + gl_VertexIndex で pull (bone math 撤去)・per-draw を SkinnedDrawData SSBO (gl_InstanceIndex・prop と同形 indirect-ready) + SkinnedDrawDataPool + `skinned_draw.h` PreparedSkinnedDraw (pass_chain で1回構築し 3 pass 共有) に・drawSkinnedPrepared 化・**旧 vertex-shader skinning と旧 push 構造体 全撤去 (残存参照ゼロ)**。 model-local 保持で 3 view 共有 = **skin once 達成 (3〜4x→1x)**。 §3-1a clean rebuild OK。
    - **残**: 2G-2b PART3-4 (PART0-2 完了 HEAD e79de3c・残 = skinned_cull + indirect + two-pass HZB occlusion で CPU draw ループ撤去) / 2G-3 (motion vector double-buffer + LDS bone cache + async overlap)。 詳細は Roadmap §4 Phase 2G / INDEX §1。
  - **最新化マラソン 28 commits 完了 (2026-05-29, 995b779 .. 641abcb)**: Foundations §1-4 + Vulkan13 §1-6 + Open-world receptacles を一気通貫で実装。 詳細は引き継ぎブロック冒頭。 主要マイルストーン:
    - **A1-A6**: buffer 系 VMA 化マラソン + **エンジン内 生 vkAllocateMemory ゼロ達成**
    - **E + E clean**: Foundations §1 ★★★ camera-relative 全 10 site wire-up (toEngineRelative helper 経由)
    - **F1-F5**: Foundations §8.1 固定容量一族 (Material/Instance/Skin/Particle/DebugLine) 全部 dynamic 化
    - **G**: BindlessTextureRegistry free-list + slot reuse
    - **N**: VK_EXT_memory_priority 実利用 (allocator bit + 全 factory に priority 設定)
    - **O**: VK_EXT_debug_utils GPU markers (8 pass label・debug_utils.{h,cpp} 新設)
    - **W**: VK_LAYER_KHRONOS_synchronization_validation 有効化 + 即発見 swapchain hazard 修正
    - **C**: transfer queue family + queue 取得 (P620 family 1 dedicated)
    - **I**: VK_EXT_memory_budget enable + allocator bit
    - **B**: **FrameSync の per-frame VkFence array → 単一 timeline semaphore に migration**。 副次効果 = 20 件の CullingPass cross-frame hazards 撲滅
    - **D+L+K+T+Z+J+Q**: 8 receptacle 取得 → 5 (L/K/K/Z/J/Q) を実 enable (extension push + feature struct chain)
    - **U**: JobSystem header-only worker thread pool 受け皿 (init(0) で inert-friendly)
    - **M**: AsyncCompute timeline semaphore receptacle (header-only)
    - **V/R/S/H/X/Y/P**: 7 rendering-technique design-memo headers (init/shutdown 空・Phase 着手時に実装)
    - 詳細な妥協度評価 + P620 [Caps] 30 capability 一覧 + 新規ファイル一覧は引き継ぎブロック参照
  - **PART4 4d Pure GPU-driven cleanup 完了 (2026-05-29, commit f8d1e1f)**: HUD `Cull : 0 / 67` がどこを向いても 0 だった件の根本対応。 4-前-4 (15b89ad) で compactCmd が device-local 化されて以降、 旧 host-mapped readback 経路の `lastVisible_[]` を更新する code path が断たれて HUD は永久 0 だった (props は GPU compactCmd 経由で正常描画されていたため動作上は健全)。 option B 採用 = 純 GPU-driven の本来形 = HUD `Cull` 行 + CPU Frustum オラクル + 全 wire-up (`lastVisible_[]` / `lastCpuVisible_` / `lastGpuVisible` / `lastCpuVisible` / `lastCullGpuVisible_` / `lastCullTotal_` / `cullGpuVisible` / `cullTotal` / 各 getter ×6 + 代入 site ×4 + HUD field ×2) を 8 files +2 -51 で一括撤去。 struct size 変更 (§3-1a) で clean rebuild + mspdbsrv kill (§3-3) を実行。 Vulkan init + asset load + ReflectionPass rebuild + ModelLoader まで validation エラー / VUID / leak 0 で通過、 user 目視「画面 OK / Cull 行 HUD から消えている」確認済み。 同一フレーム精密照合が要る場合は別 commit で countBuf を small staging に CopyBuffer する形で復活可能。
  - **PART4 4c 完了 (2026-05-29, 8 commits ad97879 / 477985d / f242327 / 7e446a9 / e41cfd7 / 91a6885 / 2f7daf9 / ccf5c03)**: two-pass HZB occlusion 本体 + Tier 1 α (Nanite/Granite 2024 baseline) + 1-tap minmax fast path。 `CullingPass` に `executePass1` / `executePass2` 経路 + per-drawId 永続 `visHistory` 1bit buffer + HZB descriptor set (UPDATE_AFTER_BIND)、 `cull.comp` に `hzbSampleMinR` (spec const で 1-tap/4-tap 分岐)、 `MainPass` に `Pass enum` (Single / FirstOpaque / SecondAndNonOpaque) + loadOp 分岐 + `toAttach` skip 修正、 `pass_chain` が `Cull1→Main(First)→HiZ→depth barrier→Cull2→Main(Second)` をオーケストレート。 user 目視「画面正常」確認 (青背景の chest/gravestone/wall も正常描画)。 詳細は引き継ぎブロック参照。
  - **PART4 4d 大半完了 (2026-05-29, 10 commits = 082d792 / 4b9c32c / da74526 / 33e1511 / 47c3571 / a62b7f0 / fcef5ab / 7298968 / c01c2e5 / 1481049)**: audit-driven 最新技術取り込み。 α (4b Obs B sync2 fix) + γ × 3 (Post/Shadow/Reflection 全 dynamic rendering 化 = engine 全体で VkRenderPass/VkFramebuffer 実 API 使用ゼロ) + M3 (dynamic_rendering_local_read 受け皿) + M1 (**persistent VkPipelineCache** = Vulkan13 §3 Y closed、 全 14 vkCreate*Pipelines callsite が ctx_->pipelineCache() 経由、 user clean exit で 490KB 書き出し実証) + M2 (sync2 generic layouts へ全面置換・depth_layouts.h ヘルパ撤去で -45 行) + N1 (pipelineCreationCacheControl enable) + N4 (**Vulkan14Features chain 追加**・maintenance5/6 enable・engine が API 1.4 で動作中なのに 1.4 features を一切 enable してない構造欠陥を修正) + N2/N3 (graphics_pipeline_library / pipeline_binary 受け皿)。 P620 [Caps] 18 capability 中 17 が =1 で実走。 詳細は引き継ぎブロック参照。
  - **PART4 4b 完了 (2026-05-28, commit ffe9673)**: HiZPass 新設 (SPD-style single-dispatch min+max RG32F pyramid)。 `renderer/hiz_pass.{h,cpp}` + `shaders/hiz_spd.comp` (LDS-only) + `shaders/hiz_spd_wave.comp` (wave-ops 派生・Phase C で `subgroupShuffleXor` 経由) + viewer `renderer/hzb_debug_widget.{h,cpp}`。 per-frame 2 枚 pyramid + atomic counter で 1 dispatch 生成。 能力 `subgroupOps` (basic+arith+shuffle) + `subgroupSize >= 32` で wave 経路選択、 そうでなければ LDS-only fallback。 `shaderStorageImageArrayDynamicIndexing` を実測有効化 (P620 全対応・`subgroupSize=32`)。 **初版で発見した 2 件の重大バグ修正済み**: (a) depth descriptor の固定 `DEPTH_READ_ONLY_OPTIMAL` を `depth_layouts::readOnly(*ctx_)` 経由に (`separateDepthStencilLayouts` 非対応 device での VUID 違反防止)、 (b) `lastGroupContinue` の `barrier()` のみだと coherent storage-image の write→read 可視性が Vulkan Memory Model 上 undefined のため `memoryBarrierImage()` 追加 (両 spv 共通)。 詳細は引き継ぎブロック参照。
  - **PART4 4a-2 完了 (2026-05-28, commit ed0d80e)**: depth-normal-motion MRT prepass + OverlayPass dynamic rendering 化。 詳細は引き継ぎブロック参照。Phase 3 SS 効果 (SSAO/SSGI/SSR/DoF/TAA) と 4b HZB が必要な「深度サンプル可能 + GBuffer normal + motion vector」の受け皿が完成。
  - **PART4 4a-1 完了 (2026-05-28, commit af3dd72)**: main_pass + child pass (debug_line/hud/particle/water/imgui) を VkRenderPass から `vkCmdBeginRendering` + `VkPipelineRenderingCreateInfo` ベースに移行。 4a-2 の MRT 拡張前提。
  - **PART4 4-前-5 完了 (2026-05-28, commit 986ba44)**: GPU-driven shadow (CullingPass に CullSet enum + per-set output buffer 化、 shadow_pass の static draw を `vkCmdDrawIndexedIndirectCount` 経由に)。 skinned shadow は legacy CPU loop 維持。
  - **PART4 4-前-4 完了 (2026-05-28, commit 15b89ad)**: 3-pass scan compaction (`scan_local` + `scan_globals` + `scan_scatter` の subgroup ops + LDS 経路) + IndirectCount 経路 + DGC 受け皿 (`indirect_exec::Mode{DGC, IndirectCount, Legacy}`)。
  - **PART4 4-前-3 完了 (2026-05-28, commit ec9c586)**: persistent device-local CullObject + bit-packed visBuf (32 object / uint32) + grow 経路 (`DeletionQueue` で旧バッファ遅延破棄)。
  - **PART4 4-前-2 完了 (2026-05-28, commit b8e39b2)**: meshlet-ready CullObject (cone backface test 用 + cluster ID) + builder で cone 充填 + `cull.comp` に cone test 受け皿。
  - **PART4 4-前-1 完了 (2026-05-28, commit ff9f7a9)**: builder の block sort + main_pass の区間検出撤去 + BlockRange 配列導入 (シェーダ無改修)。
  - **PART4 4-前-0 完了 (2026-05-28, commit 702c773)**: Reverse-Z + 無限遠 perspective に全面切替 (全 pass 波及)。 `include/MyEngine/renderer/projection.h` に `makeReversedZInfinitePerspective` ヘルパ。
  - **Vulkan13 W 完了 (2026-05-28, commit e1494bf)**: `VK_KHR_synchronization2` (Vulkan 1.3 core) 用 barrier helper (`include/MyEngine/renderer/barrier.h`) を導入し 5 サイトを移行。 batched `VkDependencyInfo` 経路と sync1 fallback 同居。
  - **Phase 1G (PCF/PCSS ソフトシャドウ) 完了** (2026-05-25, commit 3436ae5)。新規共有シェーダ `include/MyEngine/shaders/shared/shadow_sampling.glsl` に Vogel ディスク PCF + PCSS (コンタクトハードニング) を集約し、4 つの lighting frag (triangle / triangle_instanced / triangle_skinned / triangle_bindless) から `sampleShadowFactor()` を呼ぶ形に。品質ノブ shadowParams.y = 0:hard(ガビガビ・低スペック用) / 1:Soft(Vogel PCF 16-tap) / 2:High(PCSS)。**shader のみ**で C++/types.h/サンプラ/descriptor は不変。
  - **VmaImage 化 完了 (image メモリ完全 VMA 一本化)** (2026-05-25)。`renderer/vma_image.h/.cpp` (VmaBuffer と対称の move-only RAII, VkImage + VmaAllocation) を新設 (commit b9ac20b)。RenderTarget (d5e7eaf) → Texture (ad8fa08) → swapchain depth (70f30b6) を順に移行 (ShadowPass output / ReflectionTarget も RenderTarget 経由で自動カバー)。最後に未使用化した `ResourceFactory::createImage` / `createImageVMA` を削除 (1349a04)。**エンジン内に image 用の生 vkAllocateMemory は無い。** 各段でビルド通過・validation/VUID/leak ゼロ・描画正常 (スクショ確認済み)。
  - **Phase 1I: compute mip-chain bloom 完成 (Jimenez/CoD, 2026-05-25, commit d03f3ff)。エンジン初の compute pass。** 旧 fragment 版 (2枚 ping-pong + 単一しきい値 + 9-tap Gaussian) を、BloomPass が mip 列 (storage+sampled VmaImage を既定6段) を内包する compute 実装に全面置換。bright→downsample(13-tap, mip0→1 Karis)→upsample(3x3 tent 加算) を全て vkCmdDispatch。render pass も framebuffer も無い。VulkanRenderer は bloom target を持たなくなり (bloomTargetA_/B_ 削除)、最終 bloom = mip0 を PostPass が合成 (post 無改修)。compute の作法 (8x8 workgroup / GENERAL 運用 / mip0 往復遷移 / COMPUTE→COMPUTE バリア) を確立し Codebase_Guide §3 に記録 (後続 compute Phase が踏襲)。ビルド通過・validation/VUID/leak ゼロ・bloom 目視確認済み。
  - 段階1 (リソース管理の全面 RAII 化) は、これで image 側も完全に閉じた。
  - **Phase 1K-A: PBR BRDF を `shared/pbr.glsl` に集約 (2026-05-25, commit 964c733)。** 4 lighting frag (triangle/instanced/skinned/bindless) に重複していた Cook-Torrance (GGX/Smith/Schlick) を共有 include に集約し、各 frag は `pbrDirectLighting()` + `pbrAmbient()` 呼び出しに。**`pbrDirectLighting` は1ライト×1表面の純粋関数で影・減衰・多光源・ambient を含まない** (オープンワールドの多光源 2A・ポイントライト減衰・ライト別影に BRDF を書き換えず拡張できる設計、ユーザーと議論して確定)。bindless のコピペズレも正版に統一。純粋リファクタで見た目ピクセル同一・validation/leak ゼロ確認済み。なお PBR 本体 (Cook-Torrance + GpuMaterial SSBO) は過去の 1K-2 で実装済みだった (今回それを集約した)。
  - **Phase 1K-4: metallic-roughness マップ完成 (2026-05-25, commit ddc5435 / トグル 23e1bac)。** model_loader に `resolveMRTextureIndex` (aiTextureType_METALNESS→無ければ DIFFUSE_ROUGHNESS、embedded `*N`) を追加し、1K-5 の linear テクスチャ土台に乗せて読む (normal index 集合を `linearTexIndices` にリネームし MR も追加＝1回 resize で確定)。bindless 登録、`gm.mrIdx` 設定。triangle.frag は `m.mrIdx >= 0` のとき MR テクスチャをサンプルし定数 factor を上書き (**glTF: roughness=G チャンネル / metallic=B チャンネル、linear**)。検証: shield_iron で金属枠が低 roughness・高 metallic、木の板はマットと部位ごとに質感分離。**設定トグルも追加** ("Metal/Rough Map" On/Off、FrameUBO.shadowParams.w で frag に伝達、settings.json 永続化)。validation/leak ゼロ。
  - **bloom Off 時のレイアウトバグ修正 (2026-05-25, commit 657e701)。** Phase 1I 以来の既存バグ: PostPass は bloom mip0 を常に SHADER_READ_ONLY でサンプルするが、bloom 無効時 pass_chain が execute をスキップし mip0 が GENERAL のまま残り、毎フレーム validation エラー (VUID-vkCmdDraw-None-09600)。`BloomPass::clearToReadable` (mip0 を黒クリア→SHADER_READ_ONLY、dispatch 無し) を bloom 無効分岐で呼ぶよう修正。bloom mip に TRANSFER_DST usage 追加。**この発見の教訓: validation ログは `-First` で切らず全行確認する** (Claude が先頭しか見ず長期間見落としていた)。
  - **Phase 2B PART2: GPU compute フラスタムカリング pass 完成 (2026-05-25, commit 5cbc7e6)。エンジン2つ目の compute pass。** 新規 `CullingPass` (renderer/culling_pass.*) は **descriptor を一切持たない全 BDA 構成** (InstanceBufferPool/SkinBufferPool/MaterialRegistry と同じ作法)。per-frame で 2 本の BDA buffer を所有: CullObject SSBO (`createMappedStorageBDA`) と IndirectCommand バッファ (`VkDrawIndexedIndirectCommand[]`、`createMappedStorageBDA` に `INDIRECT_BUFFER` usage を追加)。新規 `cull.comp` (local_size_x=64) が CullObject を buffer_reference で読み、フラスタム6平面 vs 球を判定して `cmds[drawId].instanceCount` に 0/1 を書く。フラスタム平面は CPU 側 `Frustum::extract` (renderer/frustum.h) して push constant で渡す (vec4 planes[6] + uvec2 cullAddr + uvec2 cmdAddr + uint objectCount = 116B)。pass_chain の MainPass 直前で execute、末尾に COMPUTE→DRAW_INDIRECT buffer バリア。**検証: GPU 可視数が CPU Frustum オラクルと完全一致 (テスト視点で 13/26、両 frame in-flight)・validation/VUID/leak ゼロ。** MainPass はまだ CPU draw ループのまま (PART3 で indirect 化)。
    - **PART2 で得た知見 (経験の累積)**: ① compute 結果の CPU 読み戻しは「dispatch を記録した同フレーム内」では見えない (GPU 未実行)。**per-frame fence (frame_sync) のおかげで、同じ frameIndex が次に来た execute 冒頭 = fence 待ち後に前回 dispatch 結果を読むのが正しい**。検証ログをこの方式に直して一致を確認した。② IndirectCommand バッファは `STORAGE | SHADER_DEVICE_ADDRESS | INDIRECT_BUFFER` が要る。`createMappedStorageBDA` に `extraUsage` 引数 (既定 0、既存呼び出し無改修) を足して対応。③ DrawCmd の buffer_reference は `buffer_reference_align = 4` (20B stride。CullObject の 16 と区別)。
- **次の推奨の一手 = PART4 §6 4c + 4d 大半完了 (18 commits) + Pure GPU-driven cleanup (f8d1e1f, +1)。 PART4 はここで完了とみなして本書 (§8 = START_HERE / Roadmap §4 2B 発展節 + 付記 / 依存マップ Hi-Z ノード / Codebase_Guide §3.5 / Work_Protocol §5f) への畳み込みフェーズ → その後は 2C LOD / 2A 多光源 / Phase 2F terrain bucket / 1I PART D のいずれか着手。** 残作業 (DGC 実装 / shader_object / descriptor_buffer / timeline semaphore / async compute 並列化 / debug log 掃除 / transparent MRT mismatch / Obs C/D) は PART4 完了を阻まないので別 commit で着手可。 詳細は上の「▶ 前回セッションからの引き継ぎ」参照 (4c / 4d 内訳・P620 [Caps] 実測・残作業)。 設計判断は記憶でなく資料を引いてから (§0-8)。 着手時にまず最新動向を web 確認。
  - **【2B PART3 の障壁 (実ソース確認で判明 2026-05-25 / 進捗は下記「PART3a 完了」を正とする)】**: static 描画は当初 indirect 化の前提を満たしていなかった。(1) **SubMesh ごとに別々の vertex/index buffer** (model.h SubMesh) で SubMesh 単位に bind = 単一の大 buffer に未統合 → **PART3a で共有 GeometryBuffer に統合済み (解消)**。(2) **描画単位が「draw item × SubMesh」** で PART1 drawId と 1:1 でない → PART3c で対応。(3) **per-SubMesh で push constant (model + materialId) を更新** (indirect は per-draw push constant 更新不可) → PART3b で per-draw SSBO に。→ A 方針 = 「(a) メッシュ統合 〔PART3a 完了、実際は無制限 multi-block〕、(b) model/materialId を per-draw SSBO 化し gl_InstanceIndex (firstInstance 経由) で引く 〔PART3b〕、(c) static vertex shader 改修 〔PART3b〕、(d) drawId を SubMesh 粒度に再定義 + indirect 差し替え 〔PART3c〕」。これがオープンワールド GPU-driven の本来の姿。
  - **【PART3 設計時の Hi-Z 見越し (2026-05-25 追記)】PART3 は「PART4 で Hi-Z occlusion が来る」前提で設計すること。** Hi-Z は frustum cull の次に挿す 2 パス目の compute (深度ピラミッド/HZB を読み、遮蔽されたオブジェクトの instanceCount をさらに 0 にする) なので、PART3 では **「compute が instanceCount を書く → indirect draw が読む」という cmdBuf 駆動の構造を必ず維持**する (この間に Hi-Z パスが drop-in する)。per-draw SSBO や統合メッシュのレイアウトも、後で 2 パス目 compute が同じ CullObject/cmdBuf を後処理できる形に保つ。**`CullObject` の AABB half-extent (extentDrawId.xyz) は Hi-Z の画面空間 AABB 投影用に PART1 で既に確保済み**なので、これを消さない。順序の根拠と定番構成 (two-pass occlusion + HZB) は Codebase_Guide §3.5 の web 確認動向、Hi-Z の前提 (深度ピラミッド生成=新規) は Phase_Dependencies の Hi-Z ノード参照。
  - **【GPU-driven 完成形アーキテクチャ (2026-05-25 確定、web 再確認済み)】大規模オープンワールドのゴール形。ここに向かって PART3 以降を積む:**
    - **すべての描画 (static prop / terrain / 将来は skinned も) を最終的に compute カリング + indirect draw に乗せる。** CPU draw ループが残るものは無くす (スケールしないため)。これが「大規模エンジン」の核。
  - **射程の明示 (2026-05-30 監査で確定・進捗追記)**: prop = ✅ 完了 (Phase 2B)。 **skinned = Phase 2G** (✅ 2G-1 + 2G-2a 完了 HEAD 1b57f8e = compute skin once + passthrough vert で頂点 skinning 3〜4x→1x + 旧 vertex-shader skinning 撤去・runtime 検証済 / 🔄 2G-2b PART0-2 完了 (CullSet に Skinned + per-CullBucket 入力 + per-bone conservative bounds・HEAD e79de3c)・残 PART3-4 = skinned_cull + indirect + two-pass HZB occlusion で CPU draw ループ撤去 = §1.5-B 完全準拠)。 **grass = Phase 2H** (現状 CPU per-blade frustum cull = 違反・未着手)。 terrain = Phase 2F (別 bucket)。 残る CPU ループ (water / bindless test / reflection static / transparent) は後段。 「将来は skinned も」の「将来」= Phase 2G として番号確定。
    - **ただし bucket を分ける**: static prop 用 GeometryBuffer と terrain 用 GeometryBuffer (チャンクストリーミング対象) は**別の megabuffer・別 indirect バッチ**にする。理由: 地形チャンクは寿命・サイズ・アロケーション粒度が prop と全く違い、同一 megabuffer に混ぜると断片化する (web 確認: Aokana 等オープンワールド事例は地形/広域をストリーミング + 専用構造で別管理し、可視分だけ VRAM にロード)。**GeometryBuffer クラス自体は汎用サブアロケータなので、prop でも terrain でも同じものを再利用できる** (terrain bucket は後でストリーミング Phase に同じ仕組みで足す = 作り直しでない)。
    - **Vulkan の `firstInstance` (base instance) を使う**: per-draw データ (model/materialId) は SSBO に置き、indirect command の firstInstance に drawId を載せて `gl_InstanceIndex` で引く。DX 系記事にある「per-instance-rate vertex buffer を別途作る回避策」(AnKi 等) は **DX の base instance 非サポート回避策なので Vulkan では不要**。Vulkan を選んでいる利点。
    - **Hi-Z occlusion は frame N→N+1 の 1 フレーム遅延 HZB** (オブジェクト単位なら遅延は問題なし。web 確認: AnKi も同方式)。PART4 で prop bucket に足し、後で terrain bucket にも。
    - **visibility buffer / meshlet / voxel(SVDAG) は「次の世代」**で、いずれも上記 GPU-driven 土台の上に乗るか別ジャンル。今やる megabuffer+cull+indirect を飛ばす理由にはならない (web 再確認済み)。着手するなら土台完成後。
    - 現在地: prop bucket の PART3a (GeometryBuffer 統合, ac7bbd1) + PART3b (per-draw SSBO + shader, c5adced) + PART3c (ビルダ 632433a + indirect 差し替え 1cf23b9) = **完了 = prop bucket の GPU-driven 骨格が完成 (CPU draw 撤去・CullingPass が実描画に接続)**。次は Hi-Z occlusion (PART4) / 2C LOD / terrain bucket (Phase 2F) など完成形の後段。
  - **【PART3a 完了 (2026-05-26, commit ac7bbd1)】prop ジオメトリの megabuffer 統合 = 完了。**
    - 積み上げ: DeletionQueue 新設 (a8a0ad4、frameIndex リング遅延破棄) → device-local VmaBuffer + copyBufferRegion (70f65dc) → GeometryBuffer (b502bdc) → AssetRegistry 配線 (8cdbb94) → cube/grass 統合 (a8a1568) → SubMesh 統合 → **無制限 multi-block + VmaVirtualBlock 化 (ac7bbd1)**。
    - **到達点**: cube / grass / Model SubMesh の全 static prop ジオメトリが共有 megabuffer に統合され、firstIndex/vertexOffset で引ける = **indirect 化の前提 (a) が完成**。GeometryBuffer は満杯で新ブロックを自動追加する**無制限方式** (block #0〜#3 自動追加・capacity exhausted ゼロ・armor 含む全モデル pixel-correct・validation/VUID/leak ゼロ確認)。設計詳細と教訓は Work_Protocol §5b/§5c。
    - **この PART3a で得た重大な教訓 (Work_Protocol に制度化済み)**: (1) 開発中前提で設計する=現状基準で固定容量を決めるな (1M 固定で armor が溢れた失敗)。(2) VmaVirtualBlock は要素単位で使う=76バイト非2べき乗 stride をバイトで渡すと device lost。(3) 同期コピーの staging は即破棄、DeletionQueue は drawFrame 中の動的 free 専用。
  - **【PART3b 完了 (2026-05-27)】per-draw SSBO + static vertex/frag shader 改修 = 完了。**
    - 積み上げ: bindAndDraw 集約 (d6dbcde、Mesh/SubMesh に `bindAndDraw(cmd,inst,firstInstance)` を足し bind+draw の desync を構造的に防止) → static_draw.h 共通化 (a1b93cf、main/reflection の重複描画ヘルパ 3 つを `renderer/static_draw.h` の inline に集約、挙動不変) → DrawData + DrawDataPool (68a5c31、types.h に DrawData 80B、renderer/draw_data_pool.h は header-only の per-frame 線形 SSBO+BDA = InstanceBufferPool と同型) → per-draw SSBO 切替 (c5adced)。
    - **到達点**: model/materialId/alpha を per-draw DrawData SSBO に置き、static の vertex (`triangle.vert`) が `DrawBuffer(push.drawBuffer).data[gl_InstanceIndex]` で引き、materialId を flat varying で frag へ。frag (`triangle.frag`) は push constant を撤去し flat in で受ける。両 static layout の push constant は `StaticDrawPushConstants` (8B/VERTEX、SSBO アドレスのみ) に縮小。**per-SubMesh の push constant 更新が消滅 = indirect 化の前提 (b)(c) 達成。static は indirect-ready** (PART3c は CPU ループ → `vkCmdDrawIndexedIndirect` の差し替えのみ・シェーダ無改修)。terrain も同経路 (firstInstance=slot)、shadow/skinned は別シェーダ・別 layout で不変。検証: ビルド/validation/VUID/leak ゼロ・静的物/反射ともピクセル正常 (視点移動でも反射安定)。
    - **この PART3b の重大な教訓 (Work_Protocol に制度化)**: per-draw SSBO を複数パス (reflection→main) で共有するとき、cursor リセット (beginFrame) を**全消費者の前で1回だけ**置く。後段 (grass の所) に置くと後のパスが前のパスの slot を上書きし、GPU 実行時に取り違える (目視では偶然映って気づけない)。beginFrame は ShadowPass 後・reflection ブロック前に。
  - **【PART3c 完了 (2026-05-27)】prop の SubMesh 粒度ビルダ (3c-1, 632433a) + indirect 差し替え・CPU draw 撤去 (3c-2, 1cf23b9) = 完了。**
    - 3c-1: `renderer/static_cull_build.h` (header-only, namespace `static_cull`) が main の opaque prop を SubMesh 粒度で走査し drawId 連番で DrawData slot + CullObject + DrawTemplate + PreparedDraw を同時生成 (三者一致)。pass_chain が reflection 後・cull 前に build → CullingPass に流す。**GPU カリング = CPU オラクル一致を確認 (gpu=cpu=56/78)。** terrain は一度 prop bucket に統合 (3c-0 ff920ae) し撤回、legacy CPU draw に戻した (別 bucket=Phase 2F)。
    - 3c-2: main の prepared CPU draw ループを `vkCmdDrawIndexedIndirect` に差し替え、CPU draw を撤去。**CullingPass が prop の実描画に初接続 = prop bucket の GPU-driven 骨格完成。** シェーダ無改修。能力チェックで `multiDrawIndirect`+`drawIndirectFirstInstance` を実測有効化 (P620 は両対応)。**block 散在 (≈17-18 連続区間) のため「同一 block の連続区間ごとに 1 draw 呼び出し」** (対応=区間ぶん単発 MDI / 非対応=区間内 drawCount=1 ループ)。「全 draw を1 MDI」は不可。**【史実】検証は HUD `Cull : 可視/総数`** (視点回転で分子が動く)・validation/VUID/leak ゼロ・prop 全部正常描画 (目視) (※ HUD `Cull` 行 + 全 readback wire-up は commit f8d1e1f で純 GPU-driven 化撤去)。詳細は Codebase_Guide §3.5 / Work_Protocol §5e。
    - **この PART3c-2 の確定事実 (資料に未記載だった)**: ① **`drawIndirectFirstInstance` が必須能力**。firstInstance に DrawData slot を載せる設計なので、non-zero firstInstance を indirect で使うこの feature が要る (`multiDrawIndirect` だけでは不足。非対応なら CPU draw fallback)。② GPU=CPU オラクル照合の常時 HUD 表示は不採用 — `lastGpuVisible` (前フレーム) と CPU オラクル (今フレーム) の基準が 1 フレームずれ高速移動で偽の不一致が出るため (※ 後に commit f8d1e1f で readback 経路ごと純 GPU-driven 化撤去・同一フレーム精密照合が要る場合は countBuf を small staging に CopyBuffer する形 = option A で復活可能)。
  - 任意 / 並行可: 1I PART D (bloom 強度・段数を settings 連携・目視チューニング)、buffer 系 VMA 化 (mesh/model_loader/terrain_mesh/texture staging の createBuffer → VmaBuffer)。2B 完了後の候補: 1K-6 AO (モデルが AO 未保持で優先度低・skip可) / 2A 多光源。
  - 完了済みで一覧から外したもの: 1G ソフトシャドウ / VmaImage 全移行・deprecated image 削除 / 1I compute bloom + 未使用 fragment bloom 削除 / 1K-A BRDF 集約 / 1K-5 法線マップ (surface gradient) / 1K-4 metallic-roughness / bloom Off レイアウトバグ修正 / 2B PART0 (設計) / 2B PART1 (CullObject 受け皿) / 2B PART2 (compute cull pass) / 2B PART3a (メッシュ統合, ac7bbd1) / 2B PART3b (per-draw SSBO + shader, c5adced ほか) / 2B PART3c (3c-1 ビルダ 632433a + 3c-2 indirect 差し替え・CPU draw 撤去 1cf23b9) = **2B 完了** / Vulkan13 W (sync2 barrier helper, e1494bf) / 2B PART4 §6 「4-前-0〜4-前-5 + 4a-1 + 4a-2」(702c773 / ff9f7a9 / b8e39b2 / ec9c586 / 15b89ad / 986ba44 / af3dd72 / ed0d80e) + **4b** (HiZPass = SPD-style single-dispatch min+max RG32F pyramid, commit ffe9673) + **4c** (two-pass HZB occlusion + Tier 1 α + 1-tap fast path, 8 commits ad97879..ccf5c03) + **4d 大半** (audit-driven α + γ × 3 + M × 3 + N × 4 = 10 commits 082d792..1481049) + **Pure GPU-driven cleanup** (commit f8d1e1f = HUD `Cull` 行 + CPU Frustum オラクル + 全 wire-up 撤去) = **PART4 essentially complete (P620 [Caps] 18 中 17 = 1 で実走)。 次は §8 畳み込み + 後段 Phase**。
- **土台 side の残り**:
  - **buffer 系 VMA 化** (別タスク): `ResourceFactory::createBuffer` (生メモリ版) を使う mesh / model_loader / terrain_mesh / texture(staging) を VmaBuffer 化すれば、createBuffer も削除でき buffer 側も VMA 一本化する。`createBufferVMA` は現状未使用に見える (要精査、このとき判断)。
  - 遅延破棄キュー (ストリーミングの前提)。
- **保留中の判断**:
  - 反射 (planar reflection) の品質は意図的に据え置き中。現代化は Phase 3-Refl で扱う。
  - **directional シャドウマップの swimming (プレイヤー移動で木の影が泳ぐ) は意図的に未対処** (2026-05-25 ユーザー判断)。原因はライト VP がプレイヤー追従 (camera_system.cpp の `target = playerPos`)。対策は texel snapping だが今はやらない。

> セッション終わりの更新チェックリスト (状態は複数ファイルに散在するので、変わった分を点検する。**バッチせず、変更が確定したファイルだけを都度出す**):
> 1. この START_HERE §2 (直近の完了・次の一手) を最新化。完了した Phase はここから外し、次の一手を更新。
> 2. ロードマップを更新: §付記 (直近の成果) に今回の commit を追記。**着手順が変わったら §5 (推奨する次の一手) の番号順も必ず直す** (ここを直し忘れると §2/依存マップ §6 と食い違う — 過去に実際に起きた)。
> 3. 依存マップ §1 の段階状態 (完了日など) を更新。
> 4. 変更した各ファイル冒頭の「最終更新」日付を直す。
> 5. types.h やラッパー仕様で実ソース確認して確定した事項があれば、コードベースガイドの「要確認」を消して確定記述に。

---

## 3. 正本ファイルの所在と読む順序

セッション開始時に以下を Read で読み込む。場所は `C:\MyEngine\MyEngine develope rules\<最新日付フォルダ>\` (現在は `2025_0528\`)。`side\` フォルダの追加文書も着手中の作業に応じて Read する。

1. **MyEngine_START_HERE.md** (このファイル) — 入口・ゴール (§1)・**設計アーキテクチャ方針 (§1.5、設計判断の前に必ず読む)**・現在地 (§2)・運用ルール (§0)
2. **MyEngine_Graphics_Roadmap_2026.md** (rev.3 以降) — 全 Phase の詳細、能力チェック+フォールバック設計 (§3)、620 動作注記、推奨の一手
3. **MyEngine_Phase_Dependencies.md** — Phase 依存関係、着手順序、二重作業を防ぐ要注意ノード
4. **MyEngine_Work_Protocol.md** (rev.2 以降) — 作業手順の正本。ビルド/起動/commit、VkUnique/VmaBuffer/VmaImage 化の鉄則、やらないことリスト、pdb ロック、置換の罠
5. **MyEngine_Codebase_Guide.md** (rev.2 以降) — コードベースの地図。三層ラッパー仕様 (VkUnique/VmaBuffer/VmaImage)、ファイル構成、確立済みの設計事実、用語集

side/ の補助文書 (着手作業に応じて追加 Read):
- **MyEngine_INDEX.md** — 横断索引 (毎セッション最初に読むと早い)
- **MyEngine_HiZ_PART4_Design.md** — PART4 Hi-Z 作業設計書 (Hi-Z 着手中)
- **MyEngine_Foundations_Audit.md** — オープンワールド土台監査 (DEBT-1〜10)
- **MyEngine_Vulkan13_Modernization.md** — Vulkan 1.3 現代化計画 (W〜AA)

読む順序: このファイル (§1 ゴール → **§1.5 設計方針** → §2 現在地) → ロードマップ本体 (何を) → 依存マップ (どの順で) → 作業プロトコル (どう作業するか) → コードベースガイド (どのファイル・どの仕様か)。
実装・設計に着手する直前には特に §1.5 (設計方針) と 4 (作業プロトコル) と 5 (コードベースガイド) を読む。

---

## 4. 開発環境・作業上の約束

- グラフィックス/Vulkan/ゲームエンジンの話題では、**必ず最新の技術動向を web 検索で確認してから**出力する。
- **推測でソースを修正・追加しない。必ず実ソースを Read/Grep で確認してから実装する。**
- ソースのインデントは 4 スペース。
- **ソース内コメントは英語で書く** (2026-05-28 確立・詳細は Work_Protocol §2)。既存の日本語コメントは触る機会に順次英語化 (DEBT-11)。資料ファイル本文は引き続き日本語。
- 環境は Windows / PowerShell + Bash (両方ツール利用可)。Claude は Read/Edit/Glob/Grep/Write/Bash/PowerShell でファイル・コマンドを直接扱える。
- ビルド中はコンソールでマウス選択しない (pdb ロックでビルドが固まる既知の罠。固まったら taskkill /F /IM cl.exe /T と mspdbsrv.exe を kill して再ビルド・Work_Protocol §3-3)。
- 置換・新規 .cpp の最終判定は obj/exe のタイムスタンプを正とする (Work_Protocol §3-1b)。
- **怖い変更 (types.h 構造体・shader 全置換・pipeline layout・全 pass 波及) は変更前後を提示してユーザー承認後に Edit する** (ハイブリッド原則・Work_Protocol §1-2)。

---

## 5. ブートストラップ運用そのものについて

- Claude の記憶はセッションをまたがない。新セッションは毎回ディスクの正本5枚 (+ INDEX + 着手中の side) を Read することで方針を引き継ぐ。
- メモリ機能 (`C:\Users\<user>\.claude\projects\c--MyEngine\memory\`) は補助で、ユーザー嗜好・運用知見の保存用。**正本はディスク上のこのファイル群**。設計内容をメモリに重複保存しない (ディスクが正・メモリは古くなる)。
- 変更が確定するたびに Claude が該当ファイルを Edit/Write で直接更新する。バッチせず変更ごとに都度反映 (§0-6)。
