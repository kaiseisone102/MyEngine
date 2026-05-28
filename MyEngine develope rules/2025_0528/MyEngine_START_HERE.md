# START HERE — MyEngine セッション用ブートストラップ

最終更新: 2026-05-28 (運用モードを Claude Code 直接ツール利用前提に切替。§3 で正本ファイルの所在を明示、§4 の「ユーザーが dump/コマンドを貼る方式」前提を削除し Claude が直接 Read/Edit/Bash する形に書換、§5 のチャット添付ループを「Edit で資料ファイル直接更新」に書換。怖い変更 (types.h・shader 全置換・全 pass 波及) は変更前後を提示してから commit するハイブリッド原則を §4 に明示。**§4 にソース内コメント英語ルールを追記** (既存日本語は順次英語化・DEBT-11)。思想ルール (§0/§1.5) は不変。詳細は Work_Protocol rev.15 / Foundations_Audit §8.9 を参照。)

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

> ### ▶ 前回セッションからの引き継ぎ (新規チャットはまずここを読む / 2026-05-27 更新)
>
> **【PART3c のスコープ確定 — 最初に読む / 2026-05-27】PART3c で GPU-driven 化するのは prop (cube + Model) だけ。terrain は対象外。** terrain は START_HERE の GPU-driven 完成形アーキテクチャ通り「**別 bucket** (専用 GeometryBuffer + 専用 cull + splat マテリアル経路 + 距離 LOD)」として、**ストリーミング Phase で**別途やる (依存マップ §0 の土台 side「ストリーミング」層)。地形マテリアル (土・岩・砂) は terrain bucket の splat / マテリアルブレンドで扱う (現状1種類でも受け皿は terrain bucket 側)。**prop と terrain を同じ GeometryBuffer に混ぜない** (寿命・サイズ・アロケーション粒度が違い断片化する)。
>
> **【2B PART3c-2 完了 = prop の GPU-driven 骨格が立ち上がった / 2026-05-27, commit 1cf23b9】**
> - **prop opaque の CPU draw ループを `vkCmdDrawIndexedIndirect` に差し替え、CPU draw を撤去した。** PART2 で作った CullingPass (GPU がフラスタムカリングして instanceCount=0/1 を生成済み) が、これで**初めて実描画に接続**された = prop bucket の GPU-driven 骨格が完成。main は CullingPass の `commandBuffer(frameIndex)` を indirect 描画ソースにし、GPU が instanceCount==0 の draw を自動スキップ = カリングが実描画に効く。
> - **能力チェック (VulkanContext で実測)**: `multiDrawIndirect` と **`drawIndirectFirstInstance`** を起動時に query して対応時のみ有効化。**P620 は両方 1 (対応) を実測確認**。`[Caps] multiDrawIndirect=1 drawIndirectFirstInstance=1`。
>   - **重要な確定事実**: 我々は firstInstance に DrawData slot を載せて `gl_InstanceIndex` で引くので、**`drawIndirectFirstInstance` が必須** (`multiDrawIndirect` だけでは足りない。non-zero firstInstance を持つ indirect コマンドにはこの feature が要る。web/Vulkan 仕様で確認)。非対応なら indirect で firstInstance を 0 に強制され slot 機構が壊れるため、その場合は CPU draw ループにフォールバック (direct draw の firstInstance は無制限)。
> - **block 分布は散在 (実測 blockSwitches≈17-18 / draws=77)**。よって描画は **「同一 GeometryBuffer block の連続区間ごとに 1 回の draw 呼び出し」**: `multiDrawIndirect` 対応なら区間長ぶんの単発 MDI、非対応なら区間内を drawCount=1 の indirect ループ。**「全 draw を1回の MDI」は不可** (block 切替で vertex/index buffer の bind が変わるため、区間で割るのが必須)。呼び出し回数を block ごと 4 本に減らすのは builder で blockIndex 順にソートする**任意の後段最適化** (今は不要。compaction と同じ「構造を変えず層を足す」)。
> - **検証 (HUD)**: デバッグ HUD に `Cull : <可視> / <総数>` を追加。視点を回すと分子が動く = GPU カリングが実描画に効いている。描画は prop 全部正常・validation/VUID/leak ゼロ・目視 OK。**当初 GPU=CPU オラクル照合を HUD に出したが、`lastGpuVisible` (前フレーム dispatch) と CPU オラクル (今フレーム) の基準が 1 フレームずれて高速移動時に偽の不一致 (赤) が出たため撤去** (looks fine ≠ correct とは別問題で、表示比較側のズレ)。**同一フレーム基準の精密照合が要るなら PART4/Hi-Z 着手時に作る** (それまでは可視数表示で足りる)。CullingPass の `lastCpuVisible()` (PART2 由来) は残置・無害で、純 GPU-driven 化 (CPU readback 撤去) のときにまとめて消す。
> - **現在の描画経路**: prop opaque = **indirect (GPU-driven)**。terrain = legacy CPU draw (`drawTerrainList`、別 bucket=Phase 2F 待ち)。grass/skinned/transparent/reflection = 従来経路 (PART3b の per-draw SSBO 経路)。
>
> **次にやること = 2B (prop GPU-driven 骨格) が完成したので、ここから枝分かれ。資料の選択肢から選ぶ (記憶で決めない・§0-8)**:
> - **2C LOD** (2B 必須・620 を救う) / **Hi-Z occlusion** (2B の発展・PART4。深度ピラミッド生成が追加前提) / **3B mesh shader** (2B 発展・RTX 更新後に最新経路) — いずれも今できた GPU-driven 骨格の上に乗る。
> - **Phase 2F terrain bucket** (terrain を専用 GeometryBuffer + 専用 cull + splat + 距離 LOD + チャンクストリーミングで GPU-driven 化)。前提=遅延破棄 + buffer 系 VMA 化 + ストリーミング層。
> - **任意 / 並行可**: 1I PART D (bloom ノブ settings 連携) / buffer 系 VMA 化 (mesh/model_loader/terrain_mesh/texture staging) / 2A 多光源。
> - **純 GPU-driven 化の仕上げ (任意)**: CullingPass の CPU オラクル + readback (PART2 検証用) を撤去し、CPU が可視数を知らない形にする。Hi-Z を入れる直前にやるのが素直。
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
  - **Phase 1G (PCF/PCSS ソフトシャドウ) 完了** (2026-05-25, commit 3436ae5)。新規共有シェーダ `include/MyEngine/shaders/shared/shadow_sampling.glsl` に Vogel ディスク PCF + PCSS (コンタクトハードニング) を集約し、4 つの lighting frag (triangle / triangle_instanced / triangle_skinned / triangle_bindless) から `sampleShadowFactor()` を呼ぶ形に。品質ノブ shadowParams.y = 0:hard(ガビガビ・低スペック用) / 1:Soft(Vogel PCF 16-tap) / 2:High(PCSS)。**shader のみ**で C++/types.h/サンプラ/descriptor は不変。
  - **VmaImage 化 完了 (image メモリ完全 VMA 一本化)** (2026-05-25)。`renderer/vma_image.h/.cpp` (VmaBuffer と対称の move-only RAII, VkImage + VmaAllocation) を新設 (commit b9ac20b)。RenderTarget (d5e7eaf) → Texture (ad8fa08) → swapchain depth (70f30b6) を順に移行 (ShadowPass output / ReflectionTarget も RenderTarget 経由で自動カバー)。最後に未使用化した `ResourceFactory::createImage` / `createImageVMA` を削除 (1349a04)。**エンジン内に image 用の生 vkAllocateMemory は無い。** 各段でビルド通過・validation/VUID/leak ゼロ・描画正常 (スクショ確認済み)。
  - **Phase 1I: compute mip-chain bloom 完成 (Jimenez/CoD, 2026-05-25, commit d03f3ff)。エンジン初の compute pass。** 旧 fragment 版 (2枚 ping-pong + 単一しきい値 + 9-tap Gaussian) を、BloomPass が mip 列 (storage+sampled VmaImage を既定6段) を内包する compute 実装に全面置換。bright→downsample(13-tap, mip0→1 Karis)→upsample(3x3 tent 加算) を全て vkCmdDispatch。render pass も framebuffer も無い。VulkanRenderer は bloom target を持たなくなり (bloomTargetA_/B_ 削除)、最終 bloom = mip0 を PostPass が合成 (post 無改修)。compute の作法 (8x8 workgroup / GENERAL 運用 / mip0 往復遷移 / COMPUTE→COMPUTE バリア) を確立し Codebase_Guide §3 に記録 (後続 compute Phase が踏襲)。ビルド通過・validation/VUID/leak ゼロ・bloom 目視確認済み。
  - 段階1 (リソース管理の全面 RAII 化) は、これで image 側も完全に閉じた。
  - **Phase 1K-A: PBR BRDF を `shared/pbr.glsl` に集約 (2026-05-25, commit 964c733)。** 4 lighting frag (triangle/instanced/skinned/bindless) に重複していた Cook-Torrance (GGX/Smith/Schlick) を共有 include に集約し、各 frag は `pbrDirectLighting()` + `pbrAmbient()` 呼び出しに。**`pbrDirectLighting` は1ライト×1表面の純粋関数で影・減衰・多光源・ambient を含まない** (オープンワールドの多光源 2A・ポイントライト減衰・ライト別影に BRDF を書き換えず拡張できる設計、ユーザーと議論して確定)。bindless のコピペズレも正版に統一。純粋リファクタで見た目ピクセル同一・validation/leak ゼロ確認済み。なお PBR 本体 (Cook-Torrance + GpuMaterial SSBO) は過去の 1K-2 で実装済みだった (今回それを集約した)。
  - **Phase 1K-4: metallic-roughness マップ完成 (2026-05-25, commit ddc5435 / トグル 23e1bac)。** model_loader に `resolveMRTextureIndex` (aiTextureType_METALNESS→無ければ DIFFUSE_ROUGHNESS、embedded `*N`) を追加し、1K-5 の linear テクスチャ土台に乗せて読む (normal index 集合を `linearTexIndices` にリネームし MR も追加＝1回 resize で確定)。bindless 登録、`gm.mrIdx` 設定。triangle.frag は `m.mrIdx >= 0` のとき MR テクスチャをサンプルし定数 factor を上書き (**glTF: roughness=G チャンネル / metallic=B チャンネル、linear**)。検証: shield_iron で金属枠が低 roughness・高 metallic、木の板はマットと部位ごとに質感分離。**設定トグルも追加** ("Metal/Rough Map" On/Off、FrameUBO.shadowParams.w で frag に伝達、settings.json 永続化)。validation/leak ゼロ。
  - **bloom Off 時のレイアウトバグ修正 (2026-05-25, commit 657e701)。** Phase 1I 以来の既存バグ: PostPass は bloom mip0 を常に SHADER_READ_ONLY でサンプルするが、bloom 無効時 pass_chain が execute をスキップし mip0 が GENERAL のまま残り、毎フレーム validation エラー (VUID-vkCmdDraw-None-09600)。`BloomPass::clearToReadable` (mip0 を黒クリア→SHADER_READ_ONLY、dispatch 無し) を bloom 無効分岐で呼ぶよう修正。bloom mip に TRANSFER_DST usage 追加。**この発見の教訓: validation ログは `-First` で切らず全行確認する** (Claude が先頭しか見ず長期間見落としていた)。
  - **Phase 2B PART2: GPU compute フラスタムカリング pass 完成 (2026-05-25, commit 5cbc7e6)。エンジン2つ目の compute pass。** 新規 `CullingPass` (renderer/culling_pass.*) は **descriptor を一切持たない全 BDA 構成** (InstanceBufferPool/SkinBufferPool/MaterialRegistry と同じ作法)。per-frame で 2 本の BDA buffer を所有: CullObject SSBO (`createMappedStorageBDA`) と IndirectCommand バッファ (`VkDrawIndexedIndirectCommand[]`、`createMappedStorageBDA` に `INDIRECT_BUFFER` usage を追加)。新規 `cull.comp` (local_size_x=64) が CullObject を buffer_reference で読み、フラスタム6平面 vs 球を判定して `cmds[drawId].instanceCount` に 0/1 を書く。フラスタム平面は CPU 側 `Frustum::extract` (renderer/frustum.h) して push constant で渡す (vec4 planes[6] + uvec2 cullAddr + uvec2 cmdAddr + uint objectCount = 116B)。pass_chain の MainPass 直前で execute、末尾に COMPUTE→DRAW_INDIRECT buffer バリア。**検証: GPU 可視数が CPU Frustum オラクルと完全一致 (テスト視点で 13/26、両 frame in-flight)・validation/VUID/leak ゼロ。** MainPass はまだ CPU draw ループのまま (PART3 で indirect 化)。
    - **PART2 で得た知見 (経験の累積)**: ① compute 結果の CPU 読み戻しは「dispatch を記録した同フレーム内」では見えない (GPU 未実行)。**per-frame fence (frame_sync) のおかげで、同じ frameIndex が次に来た execute 冒頭 = fence 待ち後に前回 dispatch 結果を読むのが正しい**。検証ログをこの方式に直して一致を確認した。② IndirectCommand バッファは `STORAGE | SHADER_DEVICE_ADDRESS | INDIRECT_BUFFER` が要る。`createMappedStorageBDA` に `extraUsage` 引数 (既定 0、既存呼び出し無改修) を足して対応。③ DrawCmd の buffer_reference は `buffer_reference_align = 4` (20B stride。CullObject の 16 と区別)。
- **次の推奨の一手 = 2B 完了 (PART3c-2 まで完了・commit 1cf23b9)。ここから枝分かれ。具体的な選択肢は上の「▶ 前回セッションからの引き継ぎ」の「次にやること」を参照** (2C LOD / Hi-Z occlusion / 3B mesh shader / Phase 2F terrain bucket / 任意の 1I PART D・buffer VMA 化・2A・純 GPU-driven 化仕上げ)。設計判断は記憶でなく資料を引いてから (§0-8)。着手時にまず最新動向を web 確認。
  - **【2B PART3 の障壁 (実ソース確認で判明 2026-05-25 / 進捗は下記「PART3a 完了」を正とする)】**: static 描画は当初 indirect 化の前提を満たしていなかった。(1) **SubMesh ごとに別々の vertex/index buffer** (model.h SubMesh) で SubMesh 単位に bind = 単一の大 buffer に未統合 → **PART3a で共有 GeometryBuffer に統合済み (解消)**。(2) **描画単位が「draw item × SubMesh」** で PART1 drawId と 1:1 でない → PART3c で対応。(3) **per-SubMesh で push constant (model + materialId) を更新** (indirect は per-draw push constant 更新不可) → PART3b で per-draw SSBO に。→ A 方針 = 「(a) メッシュ統合 〔PART3a 完了、実際は無制限 multi-block〕、(b) model/materialId を per-draw SSBO 化し gl_InstanceIndex (firstInstance 経由) で引く 〔PART3b〕、(c) static vertex shader 改修 〔PART3b〕、(d) drawId を SubMesh 粒度に再定義 + indirect 差し替え 〔PART3c〕」。これがオープンワールド GPU-driven の本来の姿。
  - **【PART3 設計時の Hi-Z 見越し (2026-05-25 追記)】PART3 は「PART4 で Hi-Z occlusion が来る」前提で設計すること。** Hi-Z は frustum cull の次に挿す 2 パス目の compute (深度ピラミッド/HZB を読み、遮蔽されたオブジェクトの instanceCount をさらに 0 にする) なので、PART3 では **「compute が instanceCount を書く → indirect draw が読む」という cmdBuf 駆動の構造を必ず維持**する (この間に Hi-Z パスが drop-in する)。per-draw SSBO や統合メッシュのレイアウトも、後で 2 パス目 compute が同じ CullObject/cmdBuf を後処理できる形に保つ。**`CullObject` の AABB half-extent (extentDrawId.xyz) は Hi-Z の画面空間 AABB 投影用に PART1 で既に確保済み**なので、これを消さない。順序の根拠と定番構成 (two-pass occlusion + HZB) は Codebase_Guide §3.5 の web 確認動向、Hi-Z の前提 (深度ピラミッド生成=新規) は Phase_Dependencies の Hi-Z ノード参照。
  - **【GPU-driven 完成形アーキテクチャ (2026-05-25 確定、web 再確認済み)】大規模オープンワールドのゴール形。ここに向かって PART3 以降を積む:**
    - **すべての描画 (static prop / terrain / 将来は skinned も) を最終的に compute カリング + indirect draw に乗せる。** CPU draw ループが残るものは無くす (スケールしないため)。これが「大規模エンジン」の核。
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
    - 3c-2: main の prepared CPU draw ループを `vkCmdDrawIndexedIndirect` に差し替え、CPU draw を撤去。**CullingPass が prop の実描画に初接続 = prop bucket の GPU-driven 骨格完成。** シェーダ無改修。能力チェックで `multiDrawIndirect`+`drawIndirectFirstInstance` を実測有効化 (P620 は両対応)。**block 散在 (≈17-18 連続区間) のため「同一 block の連続区間ごとに 1 draw 呼び出し」** (対応=区間ぶん単発 MDI / 非対応=区間内 drawCount=1 ループ)。「全 draw を1 MDI」は不可。検証は HUD `Cull : 可視/総数` (視点回転で分子が動く)。validation/VUID/leak ゼロ・prop 全部正常描画 (目視)。詳細は Codebase_Guide §3.5 / Work_Protocol §5e。
    - **この PART3c-2 の確定事実 (資料に未記載だった)**: ① **`drawIndirectFirstInstance` が必須能力**。firstInstance に DrawData slot を載せる設計なので、non-zero firstInstance を indirect で使うこの feature が要る (`multiDrawIndirect` だけでは不足。非対応なら CPU draw fallback)。② GPU=CPU オラクル照合の常時 HUD 表示は不採用 — `lastGpuVisible` (前フレーム) と CPU オラクル (今フレーム) の基準が 1 フレームずれ高速移動で偽の不一致が出るため。同一フレーム基準の精密照合は PART4/Hi-Z 時に必要なら作る。
  - 任意 / 並行可: 1I PART D (bloom 強度・段数を settings 連携・目視チューニング)、buffer 系 VMA 化 (mesh/model_loader/terrain_mesh/texture staging の createBuffer → VmaBuffer)。2B 完了後の候補: 1K-6 AO (モデルが AO 未保持で優先度低・skip可) / 2A 多光源。
  - 完了済みで一覧から外したもの: 1G ソフトシャドウ / VmaImage 全移行・deprecated image 削除 / 1I compute bloom + 未使用 fragment bloom 削除 / 1K-A BRDF 集約 / 1K-5 法線マップ (surface gradient) / 1K-4 metallic-roughness / bloom Off レイアウトバグ修正 / 2B PART0 (設計) / 2B PART1 (CullObject 受け皿) / 2B PART2 (compute cull pass) / 2B PART3a (メッシュ統合, ac7bbd1) / 2B PART3b (per-draw SSBO + shader, c5adced ほか) / 2B PART3c (3c-1 ビルダ 632433a + 3c-2 indirect 差し替え・CPU draw 撤去 1cf23b9) = **2B 完了**。
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
