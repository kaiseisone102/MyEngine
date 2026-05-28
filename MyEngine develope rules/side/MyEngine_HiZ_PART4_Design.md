# MyEngine Phase 2B PART4 — Hi-Z Occlusion Culling 設計書 (rev.8)

最終更新: 2026-05-28 (rev.8: §6 **4b = 完了** (HiZPass = SPD-style single-dispatch min+max RG32F pyramid)。 §3.3-M SPD / §3.3-N min+max ペア HZB 取り込み済み = (M) + (N) 完了 (受け皿でなく実装)。 §3.4-V Async compute queue は受け皿のみのまま、 §3.3-Q / §3.3-R / §3.4-U / §3.4-T (他 pass) は 4d 残。 次は §6 4c (two-pass occlusion 本体 + AABB 遮蔽 + cull.comp 拡張)。 / rev.7: §6 4-前-0 / 4-前-1 / 4-前-2 / 4-前-3 / 4-前-4 / 4-前-5 / 4a-1 / 4a-2 = **完了**。 4a を「4a-1 (dynamic rendering migration)」+「4a-2 (depth-normal-motion MRT + OverlayPass + GBuffer viewer)」に分割実装、 §3.4-T「Dynamic rendering 受け皿」を 4d から 4a-1 に繰り上げて clean integration を達成 (古い手法で組んでから最新に作り直す順序の禁止 §0)。 §3.4-S Motion vector RT、 §3.2-H Depth-normal prepass、 §3.4-T Dynamic rendering 取り込み済み。 §3.1-A scan compaction、 §3.1-B 動的容量、 §3.2-G meshlet-ready、 §3.2-I scan-based compaction、 §3.2-J Shadow GPU-driven、 §3.2-K Persistent CullObject、 §3.3-O Reverse-Z、 §3.3-P DGC 受け皿、 全部立った。 次は §6 4b (HZB SPD 本体)。 / rev.6: **rev.5 再点検で見つかった追加取りこぼし 4 件を取り込み**。(S) **Motion vector RT を 4a の MRT に追加** — TAA/TSR/FSR/DLSS は全て motion vector 入力必須・Phase 3 post AA で後付けすると shader 全改修になるため今 MRT 拡張で受け皿確保。(T) **Dynamic rendering (VK_KHR_dynamic_rendering / Vulkan 1.3 core) 受け皿** — VkRenderPass + VkFramebuffer は古い形式・現代 Vulkan の標準は dynamic rendering。能力ゲートで対応時 dynamic rendering 経路・非対応時 VkRenderPass fallback。(U) **Timeline semaphore (VK_KHR_timeline_semaphore / Vulkan 1.2 core) 受け皿** — binary semaphore からの進化・async compute / 複数 queue / フレーム fence 管理を大幅単純化。Foundations §2 の前提基盤。(V) **Async compute queue 受け皿** — Foundations §2 と接続・HZB/cull を main pass と並列化可能な queue family 取得だけを今追加。実並列化は後段。**MyEngine が「現代 Vulkan の標準形」に乗っていない箇所** (VkRenderPass / binary semaphore / single graphics+present queue) を能力ゲート付きで段階移行可能な形にする / rev.5: SPD / min+max HZB / Reverse-Z / DGC / shader_object / descriptor_buffer(Pascal 安全弁) / rev.4: two-pass / meshlet-ready / depth-normal prepass / Blelloch / shadow GPU-driven / persistent / rev.3: 基盤再設計 / rev.2: スケール受け皿繰り上げ / rev.1: 初期設計) / 対象: Phase 2B 完了 (prop bucket GPU-driven 骨格) の発展として prop bucket に Hi-Z occlusion を足す。大規模オープンワールド (複数光源・複数地面・数千〜数万オブジェクト) を本気で前提にする

> **この文書の位置づけ**: PART4 (Hi-Z) 専用の作業設計書。正本5枚 (START_HERE / Roadmap / 依存マップ / Codebase_Guide / Work_Protocol) と対で読む。PART4 が完了したら、ここの確定事項を正本5枚へ畳み込む (START_HERE §2 / Roadmap §4 の 2B 発展節+付記 / 依存マップ Hi-Z ノード / Codebase_Guide §3.5 / Work_Protocol §5f) — セッション終わり運用 (§6)。それまではこの文書が PART4 の作業正本。
> **方針の根拠 (§0-2 / §1.5)**: §1.5-B (GPU-driven 完成形・cmdBuf 駆動構造維持で Hi-Z を drop-in)、§1.5-C (能力チェック+フォールバック・実測してから分岐)、§0 (最新を第一基準・受け皿を先に最新で用意)。

---

## 0. ゴールとスコープ境界

**ゴール**: Phase 2B で立ち上がった prop bucket の GPU-driven 骨格 (compute frustum cull → indirect draw) に、**Hi-Z occlusion culling** を 2 パス目として足す。遮蔽されたオブジェクト (壁や地形の陰) の `instanceCount` をさらに 0 にして描画から落とす。

**規模感の認識 (ユーザー要件)**: MyEngine は大規模オープンワールド前提。複数光源 (Phase 2A clustered)、複数地面/広域地形 (Phase 2F terrain bucket)、数千〜数万オブジェクトを想定する。Hi-Z はこの規模に耐える形 (解像度ベースでスケール・全 bucket 共有・他経路へ横展開可能) で設計する。

**今回 PART4 でやる範囲**: prop bucket の Hi-Z occlusion のみ。
**やらない範囲 (別 Phase)**: terrain の GPU-driven 化 (Phase 2F)・skinned/instanced への横展開・LOD (Phase 2C)・clustered 多光源 (Phase 2A)・shadow 用 per-light HZB。
**ただし「大規模前提」として**: Hi-Z の構造は terrain bucket や他経路にも同じ仕組みで横展開できる形 (受け皿を最新で・§0) にする。具体策は §4。

### 0.1 スケール第一の設計指針 (rev.2 で格上げ・「これで動くからいいだろう」を駆逐する基準)

**Hi-Z は「大量オブジェクトを描けるようにする」機能である。だから入力・出力・遮蔽精度のどれにも「今の小規模で足りる固定値・近似」を残さない。** 実装は段階に刻んでよい (§6) が、以下の**アーキテクチャ受け皿は PART4 で建てる** (中身のチューニングは後でよいが、構造を後付けにしない = §0「受け皿を先に最新で用意」)。「77 draw で動くから」を理由に下記を後段・任意に逃がすのは禁止。

- **(A) draw-count + 可視コマンド圧縮の受け皿**: `instanceCount=0/1` を GPU がスキップするだけの方式は、空コマンドを毎フレーム全走査し、ブロック区間ごとの CPU indirect 呼び出しも減らない。大規模では **GPU が書く draw-count バッファ + 可視コマンドの圧縮 (prefix-sum/stream compaction)** に乗せ、`vkCmdDrawIndexedIndirectCount` で「bucket あたり最小回数の GPU-driven 呼び出し」に収束させる (web: nvpro MDI-count は「空 drawindirect の走査を避ける・実データで推奨」、Blelloch scan で可視リストを連結)。能力ゲート付き (drawIndirectCount / Vulkan 1.2 コアの vkCmdDrawIndexedIndirectCount)。
- **(B) 入力キャップを固定しない**: `MAX_DRAWS=4096` 固定は「数千〜数万」と矛盾。GeometryBuffer (§5c) と同じ動的成長 (multi-block) に。Hi-Z 着手の前提作業 (§6 PART4-前)。
- **(C) 遮蔽は AABB 精度を最初から**: bounding sphere だけだと、大きく平たい/長い遮蔽器 (壁・崖・地形チャンク・木立) の裏が緩くしかカリングされない (一番効いてほしい所で効かない)。`CullObject.extentDrawId.xyz` (half-extent) は**既に枠が確保済み**なので、それを充填し AABB 画面投影で遮蔽判定する (sphere は粗い早期棄却に併用)。
- **(D) two-pass の受け皿を今確保**: pop-free な two-pass occlusion を真の drop-in にするため、**per-object「前フレーム可視」フラグの受け皿**を今のデータ構造に確保する (実装は後段でも、構造を後付けにしない)。
- **(E) CPU indirect 呼び出しを収束させる**: block 区間ごとの indirect 呼び出しは block 数とともに膨張する。block sort + (A) の count バッファで「bucket あたり数回」に収束させることを §4 の追跡課題とする (「任意」にしない)。

### 0.2 最尖端取り込み指針 (rev.4・「P620 だから無理」を駆逐)

§0 (受け皿を先に最新で用意・「対象がまだ無いから最新不要」は禁止) と §1.5-C (能力チェック+フォールバック・最新経路は必ず実装) を厳密適用。**P620 性能や mesh shader 非対応を理由に最新を諦めない**。能力で動かない部分はフォールバック経路を作る (二重実装でなく退避先)。

- **(F) Two-pass HZB occlusion を baseline に**: 1 フレーム遅延 single-pass は撤廃。現代の go-to (Maister "the go-to technique these days is two-phase occlusion culling with HiZ"・UE5 Nanite が採用) に揃える。受け皿 (D visBuf) を**読む形**で実装。
- **(G) CullObject を meshlet-ready 拡張**: normal cone (cone-vs-view test 用) + cluster bounds + parent/child cluster ID を型に追加。mesh shader (Phase 3B) が P620 非対応でも**データ型は今 meshlet 化可能な形に決める** (§0「受け皿を先に最新で」)。cull.comp に cone test を入れる (object 粒度でも cheap な追加 cull・meshlet 化したら同じ関数が活きる)。
- **(H) Depth-normal prepass (軽量 GBuffer) 共有**: 4a を「深度のみ保存」でなく「depth+normal を出す軽量 GBuffer prepass」として設計。SSAO/SSGI/SSR/Hi-Z/DoF が全部この prepass を共有。各 SS フェーズが個別に深度/法線パスを後付けする手戻りを防ぐ (Foundations §5 で指摘済み)。
- **(I) Stream compaction を Blelloch scan で実装**: atomic 起点は回り道なので取らない。2025 best practice (onedraw 等) が scan を本筋にしている。最初から scan で実装。
- **(J) Shadow_pass を GPU-driven 化 (PART4 範囲内)**: 4-前 で建てる基盤 (block sort + compactCmdBuf + drawIndirectCount + scan compaction) を shadow_pass にも同形で適用。vkguide サンプル (main+shadow 両方 GPU-driven で 125k @ 290 FPS) と揃える。per-light shadow HZB は後段だが、shadow に GPU-driven cull は今入れる。
- **(K) 永続 GPU オブジェクトバッファの受け皿**: 毎フレーム CPU rebuild でなく「変化時だけ更新」できる形の受け皿確保 (Foundations §4)。実装は段階的でよいが、CullObject 配列が per-frame 上書き前提の固定設計にならないよう、persistent buffer + dirty range update の経路を構造として用意する。
- **(L) 維持する選択 = forward+ 路線**: Visibility Buffer / Nanite 風 deferred materials は取らない。これは §1.5 (透明・MSAA との両立) と Roadmap 2A (clustered forward+) の明示的選択であり、妥協ではない。最尖端形式の一つ。Nanite クラスタ DAG / virtualized geometry は遠い将来 (Roadmap 明示)。

### 0.3 最尖端取り込み指針 第二弾 (rev.5・rev.4 で残った取りこぼし駆逐)

- **(M) HZB 生成を SPD (Single Pass Downsampler) で 1-dispatch 化**: rev.4 §6 4b の "BloomPass 雛形・per-mip dispatch" は世代遅れ。**AMD FidelityFX SPD** = LDS で中間ストレージ + global atomic counter で thread group 間同期 + (オプション) subgroup ops。**1 thread group が 64×64 を 1×1 までダウンサンプル、最後の tile が last 6 MIP を処理 → 4096×4096 を 1 dispatch で完結**。Bevy/Granite/AMD 公式採用。RDNA 最適化と書かれるが GLSL 版が標準提供で NVIDIA/Intel でも動作。MyEngine では VK_KHR_shader_subgroup_extended_types + atomic counter で実装。
- **(N) HZB を min+max ペア (RG32F) で同時生成**: SPD の user-defined 2x2 reduction で同時計算。max は通常の Hi-Z occlusion、min は将来の反射/particle sort/shadow culling/transparent depth bounds で使う。RG32F = 8B/texel で R32F の倍だが、HZB はそもそも画面解像度依存で小さい (1920×1080 全 MIP 合計 ~10MB → 20MB 程度)。受け皿として今取る (§0「受け皿を先に最新で」)。
- **(O) Reverse-Z depth 採用**: `vulkan_renderer.cpp:101` の `cameraParams = vec4(0.1f, 200.f, ...)` (forward-Z = depth=1 が遠方) を撤廃。**Reverse-Z (depth=0 が遠方)** に切替。理由: float depth は near 側に値域が密、far 側で粗 — forward-Z だと遠方で精度崩壊 (z-fighting)。Reverse-Z は **float の指数表現と reciprocal depth の組み合わせで near/far 全域でほぼ均一精度**を得る業界標準 (UE / Frostbite / vkguide 推奨)。HZB も同恩恵。Foundations §8.8 の "far=200 が大規模と矛盾" 負債と直結 — **far を拡張する前に reverse-Z にしないと精度が崩れる**。MyEngine 改修箇所: cameraParams 値の意味反転 + 全 depth compare op を `VK_COMPARE_OP_GREATER` 系へ + clear depth 値 0.0 + 投影行列 (or near/far swap)。すべての pass に波及するため PART4 内で一括対応。
- **(P) VK_EXT_device_generated_commands (DGC) の受け皿**: indirect の次世代 (XDC 2024 発表)。<rev.4 までの IndirectCount は> pipeline 切替不可だが **DGC は pipeline / push constants / shader objects 切替を GPU が組み立てる**。VkIndirectCommandsLayoutEXT + VkIndirectExecutionSetEXT で複数 pipeline をスロット化、GPU が選択。能力ゲート (`deviceGeneratedCommands` feature + 関連限界値) で対応時 DGC 経路 / 非対応時は 4-前-4 の IndirectCount fallback。Pascal は VK_NV_device_generated_commands で旧式対応していたが EXT 版対応は実機 query 必須。**受け皿として CullingPass / MainPass に DGC 経路の分岐口を 4d 段階で挿す** (実装は対応時のみ・非対応なら IndirectCount のまま動く)。
- **(Q) VK_EXT_shader_object の受け皿**: pipeline オブジェクト不要、shader をコマンドバッファ記録中に mix-and-match バインド。**Pipeline state explosion 解消** (Roadmap 2A clustered forward+ の多 pipeline 化が来る前に対応すると効く)。能力ゲート (`shaderObject` feature) で対応時 shader_object 経路 / 非対応時 VkPipeline 経路。**受け皿として PipelineLayout / VkPipeline を抽象化する型 (ShaderProgram 等) を 4d で導入**。実装は段階的でよい (今ある VkPipeline 経路と並走可能・能力で切替)。
- **(R) VK_EXT_descriptor_buffer の受け皿 (Pascal driver bug 警告付き)**: descriptor を buffer に直接書く方式。bindless の発展形・Foundations §3 の 1024 固定負債解消と接続。**ただし Pascal 世代で driver 535 系以降 Xid 69 クラッシュ報告あり** (vkd3d-proton NVRM bug 4624081・cyberpunk 2077 等で再現)。MyEngine は **能力 query 結果 + vendor/device ID 判定** で「Pascal なら明示的に無効化」する安全弁を設ける。Pascal 以外で対応 + 動作確認できた driver でのみ有効化。受け皿は今取る (将来 P620 から退役したら自動で有効化される形)。実装は段階的でよい。

### 0.4 最尖端取り込み指針 第三弾 (rev.6・rev.5 再点検で見つかった追加取りこぼし駆逐)

- **(S) Motion vector RT を 4a の MRT に追加** — TAA / TSR / FSR / DLSS のすべてが motion vector を入力に要求 (前フレーム → 今フレームの reprojection)。Phase 3 post AA で後付けすると shader 全改修の手戻り。**今 4a で MRT を 3 枚 (HDR color / normal / motion) にする**。motion vector は前フレーム MVP と今フレーム MVP からの NDC 差分 (RG16F)。Phase 3 で TAA を載せるとき shader 改修なしで乗る。受け皿として今取る (§0「受け皿を先に最新で」)。
- **(T) Dynamic rendering (VK_KHR_dynamic_rendering / Vulkan 1.3 core) 受け皿** — VkRenderPass + VkFramebuffer は古い形式。**現代 Vulkan の標準は dynamic rendering** (Vulkan 1.3 で core 化、attachment をコマンドバッファ記録中に直接指定)。MyEngine は今 `VkRenderPass renderPass_` + `framebuffers_` 持ち = 古い形式 (main_pass.h で確認済み)。能力ゲート (`dynamicRendering` feature) で対応時 dynamic rendering 経路 / 非対応時 VkRenderPass fallback。受け皿として **`RenderTarget` 抽象型を導入** (内部に VkRenderingInfoKHR or VkRenderPass+VkFramebuffer)、4d で能力分岐。
- **(U) Timeline semaphore (VK_KHR_timeline_semaphore / Vulkan 1.2 core) 受け皿** — binary semaphore からの進化。**現代 Vulkan の同期は timeline semaphore が標準** (単調増加値・wait-after / multi-wait / multi-signal 可能・async compute / 複数 queue / フレーム fence の管理を大幅に単純化)。MyEngine は `FrameSync` が binary `VkSemaphore` (frame_sync.h:63 `imageAvailableSemaphores_` で確認済み) = 古い形式の可能性。能力ゲート (`timelineSemaphore` feature) で対応時 timeline 経路 / 非対応時 binary fallback。**Foundations §2 (async compute / 専用 transfer queue) の前提基盤**として PART4 で受け皿確保。
- **(V) Async compute queue 受け皿** — Foundations §2 で既出だが PART4 では graphics queue 単独で動かしている。**現代 Vulkan は async compute queue で HZB / cull を main pass と並列実行**して GPU 使用率を上げる (DGC の preprocess も async compute 前提)。`vulkan_context` で **async compute queue family の取得**を今追加 (非対応なら graphics queue にフォールバック)。実並列化は後段でよい (queue 取得 + queue 指定可能な API 形だけ今整える)。Foundations §2 と一緒に進める形。

---

## 1. 実ソースから判明した事実 (設計を縛る前提・推測でなく確認済み)

PART4 の構造はこれらの事実で決まる。すべて 2026-05-27 に実ソース dump で確認。

### ① 深度バッファは現状「破棄」されている — 4a で保存・サンプル可能化が必要
- `main_pass.cpp` のレンダーパス: `depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE` (L123)。**メインパス後に深度内容は捨てられる。**
- Hi-Z は深度から HZB (深度ピラミッド) を作るので、**深度を STORE し、かつ SAMPLED 可能にする**必要がある。
- 深度アタッチメント: color と並ぶ 2 番目のアタッチメント。`depth.format = swapchain_->depthFormat()` (L120)、`depthRef{1, ...DEPTH_STENCIL_ATTACHMENT_OPTIMAL}` (L130)、`finalLayout = DEPTH_STENCIL_ATTACHMENT_OPTIMAL` (L127)、framebuffer は `{hdrColorView_, swapchain_->depthView()}` (L338)、clear は `clearValues[1].depthStencil = {1.0f, 0}` (L374)。

### ② CullObject の half-extent は「枠だけ」で未充填 — sphere 投影から始められる
- `types.h` の `CullObject` = `vec4 centerRadius` (xyz=world AABB center, w=bounding sphere radius) + `vec4 extentDrawId` (xyz=world AABB half-extent, w=drawId)。half-extent は「将来の精密 AABB-vs-plane 用」とコメントで確保済み。
- だが `static_cull_build.h` を grep すると、充填しているのは `centerRadius` (球) と `extentDrawId.w` (drawId) **のみ** (L78=drawId, L105/L122-123=centerRadius)。**half-extent (extentDrawId.xyz) は書かれていない。**
- 含意: **Hi-Z の遮蔽テストは sphere の画面投影から早期棄却を始められる**が、それ「だけ」で終わらせない。大きく平たい/長い遮蔽器の裏を効かせるには AABB 精度が要る (§0.1-C)。half-extent は**既に枠が確保済みで未充填なだけ**なので、4c で充填し AABB 画面投影を本体に入れる (rev.2 で「4d 任意」から格上げ)。

### ③ CullingPass は今「descriptor ゼロ・全 BDA」 — Hi-Z で HZB サンプラ 1 個が増える
- `cull.comp` は CullObject と DrawCmd を BDA (`buffer_reference`) で読み書きし、`culling_pass.cpp` の pipeline layout は `li.setLayoutCount = 0` (descriptor set ゼロ)。push constant のみ。
- HZB は**サンプルする深度テクスチャ**なので BDA では渡せず **`sampler2D` の descriptor が要る**。
- 含意: Hi-Z 導入で CullingPass は「全 BDA」→「BDA + HZB サンプラ 1 個」になる。破綻ではない (BloomPass が既に descriptor compute をやっている。Codebase_Guide §3 の compute 作法に従う)。設計上の明示的な変化点として記録。

### ④ 深度フォーマットは findDepthFormat で実機決定
- `vulkan_context.cpp findDepthFormat()`: 候補 `{D32_SFLOAT, D32_SFLOAT_S8_UINT, D24_UNORM_S8_UINT}` の最初に通るもの。`vulkan_renderer.cpp` が `swapchain_.init(..., ctx_.findDepthFormat())`。
- shadow_pass は `VK_FORMAT_D32_SFLOAT` 固定 (pass_chain で `si.depthFormat = VK_FORMAT_D32_SFLOAT`)。メイン深度はおそらく純 `D32_SFLOAT` だが実機 query 値を 4a で確認する。
- HZB に格納するのは深度の max リダクション値なので、ピラミッドは `R32_SFLOAT` で持つ (深度フォーマットそのままでなく、サンプルした深度値を float で書く)。

### ⑤ 深度は per-frame でなく 1 枚を使い回し — だから「ピラミッド側」を二重化する (最重要)
- `swapchain.h`: `VmaImage depthImage_` / `VkImageView depthView_` は**単数**。`MAX_FRAMES_IN_FLIGHT` 枚ではない。recreate でのみ作り直す (`createDepthResources`)。
- 含意: フレーム N のメインパスがフレーム N-1 の深度を**上書き**する。1 フレーム遅延 HZB (N のカリングで N-1 の深度を読む) を素直にやるには、深度を増やすのでなく **HZB ピラミッド (HiZPyramid) を per-frame で 2 枚持つ**のが正解。深度→ピラミッド生成した時点で N-1 の情報はピラミッドに移るので、深度 1 枚が上書きされても N のカリングはピラミッドを読めばよい。**深度リソースは増やさない。**

### ⑥ pass_chain の実行順が確定 — Hi-Z の 2 パスの挿入位置が決まる
`pass_chain.cpp` の execute (確認済み行番号):
- L286: ShadowPass.execute
- L292: `drawDataPool_.beginFrame` (per-draw SSBO cursor リセット・全消費者の前で 1 回)
- L294-333: ReflectionPass (water があれば)
- L338-340: `static_cull::build(...)` (prop の DrawData/CullObject/DrawTemplate/PreparedDraw 生成)
- L341-354: `[BlockDbg]` 一時ログ (blockSwitches 計測・⑦)
- L356-365: **CullingPass.execute** (`ce.viewProj = proj * view`, `ce.cullObjects`/`ce.drawTemplates`)
- L367以降: **MainPass.execute** (深度書き込み)

Hi-Z の挿入位置:
- **HZB 生成パス (HiZPass.execute)** = MainPass の**直後** (深度が書かれた後)。
- **遮蔽判定** = CullingPass.execute (L364) の中 (cull.comp 拡張)。読むのは**前フレームに生成した HiZPyramid**。CullingPass に「前フレームの pyramid view/sampler」を渡す配線を足す。
- 初回フレームは前フレーム pyramid が未生成 → 遮蔽スキップ (全可視) で安全に開始 (保守的フォールバック)。

### ⑦ 一時ログが残置 — 4d の掃除対象
- `[BlockDbg]` (pass_chain L341-354)、`[Cull2B]` (scene_renderer.cpp 末尾・pass_chain の cull execute 後) が PART2/3 由来で残置。資料が「PART3 完了後に削除」と言っている。PART4 の純 GPU-driven 化仕上げで掃除する。

### 補足 (確定): cmdBuf 駆動構造は Hi-Z drop-in 準備済み
- `cull.comp` の最終行は `cmds.cmds[drawId].instanceCount = sphereVisible(...) ? 1u : 0u;`。**Hi-Z はこの判定に「かつ遮蔽されていない」を AND するだけ**で乗る。
- `culling_pass.cpp` 末尾に COMPUTE→DRAW_INDIRECT バリア (L 末尾、`VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT` → `VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT`) が既にある。
- `MAX_DRAWS = 4096` 固定 (culling_pass.h)。§5b の動的成長負債で、大規模化で効く (§4)。
- 能力 getter は確立済み: `vulkan_context.h` に `multiDrawIndirect()` (L62) / `drawIndirectFirstInstance()` (L63) と private bool メンバ (L86-87)。Hi-Z の能力分岐もこの作法に倣う。

### ⑧ 現状は GPU-driven の「入口形」 — 大規模ではスケール壁。PART4 で受け皿を建てて越える
PART2/3 から引き継いだ現状の描画は、GPU-driven occlusion の**入口形**であり、そのままでは大規模オープンワールドでスケールしない。これは欠陥ではなく「段階の最初」だが、**Hi-Z を載せる今こそ受け皿を最新形にする**タイミング (§0.1)。具体的なスケール壁:
- **空コマンド全走査**: `instanceCount=0` の draw も indirect コマンド配列には残り、GPU が毎フレーム MAX_DRAWS 本を走査する。可視率が低い (= occlusion がよく効く) ほど無駄が増える皮肉。→ (A) 圧縮で解消。
- **CPU indirect 呼び出しが block 区間数に比例**: main は連続 block 区間ごとに `vkCmdDrawIndexedIndirect` を 1 回 (実測 77 draw / 17-18 区間)。数万 draw・数百 block では区間数=呼び出し数が膨張し CPU 提出が再びボトルネックに (GPU-driven の意義が薄れる)。→ (A) count バッファ + (E) block sort で収束。
- **入力 4096 キャップ**: 数千〜数万を描く機能なのに入力上限が 4096。→ (B) 動的成長。
- **sphere 遮蔽の緩さ**: 大遮蔽器の裏がほぼ素通り。→ (C) AABB。
これらを「77 draw で動くから後で」にしないのが rev.2 の主眼。実装順は §6 で刻むが、受け皿 (count バッファ・動的容量・half-extent・可視履歴) は PART4 内で建てる。

### ⑨ 現行構造は GPU-driven の最初期形 — rev.3 で「block sort + 圧縮 + drawIndirectCount」に再設計 (実コード dump 確認済み)
PART4-PRE-A〜E の実コード dump で、現行 prop bucket の indirect 描画構造が判明。これは「動くが最初期形」であり、最新 GPU-driven ベストプラクティスとは乖離。**rev.3 の主タスクはこの基盤の破棄と再設計**。

**現行 (甘い) 構造 — 実コード行で確定**:
- `static_cull_build.h emit()`: draw を**遭遇順**に走査し drawId 連番採番 (cube → static models → terrain)。blockIndex 順でない。
- `cull.comp L62`: `cmds.cmds[drawId].instanceCount = sphereVisible ? 1u : 0u` ── 空 (instanceCount=0) コマンドは cmdBuf 内に残置。
- `main_pass.cpp:434-450`: PreparedDraw を走査して**同じ blockIndex の連続区間 (runStart..runEnd) を検出**し、その区間ぶんを `vkCmdDrawIndexedIndirect(off=runStart*stride, runLen)` で 1 回呼ぶ。実測 77 draw / 17-18 区間 = 1 フレームに 17-18 回 indirect 呼び出し。
- `culling_pass.h MAX_DRAWS=4096`: 固定上限。

**最新 best practice (web 確認・2025 現役)**:
- vkguide: 空 draw 残置は「最適でない」。「mesh/material 変更ごとに 1 draw call になるので、**前処理で同じ mesh/material のセクションに圧縮**する」。
- vkguide: 圧縮の方法は「**インスタンシング**」or「**空 draw を除いた DrawIndirectCount**」。
- 2025 onedraw 記事: 「**exclusive scan compute シェーダで可視コマンドの圧縮リストを作る** → 別の compute シェーダが述語とインデックスで出力バッファに書く」。
- vkguide tutorial 規模: **125,000 オブジェクトを RTX2080 で 290 FPS** (同手法)。

**rev.3 の再設計 (確定)**:
- **builder で block sort**: `static_cull_build.h` で draw を **blockIndex 順に並べてから drawId を採番**。これだけで cmdBuf 上に block 区間が**自然に連続配置**される。設計書 §3.1-A (E) が既に書いていた方向 (rev.2 で「block sort を任意から (A) の前提作業に格上げ」と明記済み)。
- **multi-block 機構は不要**: block sort で block 区間が連続するので、cmdBuf / cullBuf / compactCmdBuf / drawData は**単一バッファのまま**でよい。`vkCmdDrawIndexedIndirectCount(compactCmdBuf, ..., countBuf, ...)` を block ごとに 1 回呼ぶ (block 数 ≦ 数個〜数十個に収束)。
- **容量は「初期値であって上限でない」grow**: §6 or 句採用。MAX_DRAWS は撤廃でなく**初期容量**に意味替え。満杯時は VmaBuffer 作り直し + 旧バッファを DeletionQueue で遅延破棄 (§5c GeometryBuffer の遅延破棄作法を流用、multi-block 機構は流用しない = use case が違うため)。
- **空 draw 残置の撤廃**: cull.comp は `instanceCount=0/1` を書くのでなく、可視時に `compactCmd[atomicAdd(count,1)] = template` で前詰め (rev.2 §3.1-A 通り)。これで「空 draw 全走査」が消える。

**実コードへの影響範囲 (確定済み)**:
- `static_cull_build.h emit/build`: 並べ替え前処理を追加。drawId 採番は連番のまま (並び順だけ変わる)。
- `cull.comp`: ほぼ無改修 (addressing は単一 BDA のまま)。Hi-Z 追加で sphere/AABB 投影と HZB サンプラ追加 (4c で別途)。
- `main_pass.cpp:434-450`: **区間検出ロジック撤去** (古い甘い構造の駆逐)。builder が返す block-range 配列を素直に for ループで bind → `vkCmdDrawIndexedIndirectCount`。
- `CullingPass` / `DrawDataPool`: `ensureCapacity(uint32_t)` 追加。MAX_DRAWS の意味替え。`compactCmdBuf_` / `countBuf_` / `visBuf_` 新設 (rev.2 §3.1-A/D 通り)。
- `BuildResult`: block-range 配列 (`std::vector<BlockRange>{ blockIndex, firstDraw, drawCount }`) を追加で返す (main_pass の区間検出を builder 側に移す)。

**「動的容量グロースは GeometryBuffer の手本に揃える」の正しい解釈** (実コードで確定): GeometryBuffer の multi-block 機構は **永続所有 + 個別 free が必要** な use case で活きる装備。CullingPass / DrawDataPool は **per-frame 全クリア・cursor リセット**で個別 free がない use case。だから GeometryBuffer から**継承するのは原則** (固定容量を残さない・遅延破棄を経由する) であって、**形式 (multi-block + VmaVirtualBlock) ではない**。これは §8.1 (Foundations_Audit) の「手本に揃える」を**形式の模倣でなく原則の継承**として解釈する立場。§6 or 句が両形式を承認している中で、**block sort により block 区間が連続する → 単一バッファで足りる**という実コード事実から後者を選んだ。

---

## 2. 設計の核 — Two-pass HZB occlusion (rev.4 で baseline 化)

### なぜ two-pass か (現代の go-to)
1 フレーム遅延 single-pass は「動く最初期形」であって、現代の baseline は two-pass。Maister "the go-to technique these days is two-phase occlusion culling with HiZ"。Medium 記事 "Two-Pass Occlusion Culling is currently getting more popular due to its appearance in Unreal Engine 5's Nanite"。Nanite 自体が "VisBuffer/BasePass two-pass rendering architecture" (UE5 公式)。1 フレーム遅延の pop (オブジェクト粒度で「丸ごとパッと出る」) を構造的に排除する。

### フレーム N のデータフロー (two-pass)
1. **パス1 cull**: cull.comp が **visBuf (前フレーム可視フラグ)** を読み、可視だったオブジェクトのみフラスタム判定 → `compactCmd1[]` に圧縮 + `countBuf1`。
2. **パス1 draw**: main_pass が `vkCmdDrawIndexedIndirectCount(compactCmd1, ..., countBuf1, ...)` で前フレーム可視分のみ描画 → 深度プライム。
3. **HZB 生成**: HiZPass.execute が深度を max ダウンサンプルして frame N の HiZPyramid を生成。
4. **パス2 cull**: cull.comp が**全オブジェクト**をフラスタム判定 AND HZB 遮蔽判定 (今フレームの pyramid 使用) → `compactCmd2[]` + `countBuf2`。同時に **visBuf に今フレームの可視結果を書き戻す** (次フレームのパス1 用)。
5. **パス2 draw**: main_pass が `vkCmdDrawIndexedIndirectCount(compactCmd2, ..., countBuf2, ...)` で**パス1で描かれていない可視オブジェクト**を描画 (パス1 で描いた分は visBuf 経由でスキップ)。

pop が出ない: 新しく見え始めたオブジェクトは「前フレーム不可視」でパス1 をスキップされるが、HZB が今フレーム深度から作られているのでパス2 で正しく可視判定され描画される。初回フレームは visBuf 全 false → パス1 はゼロ描画 → パス2 がフラスタム cull のみで全オブジェクト処理 (HZB は初回未生成なのでフラスタムのみ・保守的に全可視扱い) → 次フレームから正常運用。

### HZB 生成 = BloomPass の mip-chain がほぼそのまま雛形
- web 確認: HZB は深度バッファを 2x2 の min/max でダウンサンプルする MIP チェーン (max リダクション・LESS 深度テストなので各領域の最大深度を保守的に格納)。
- 資料の compute 作法 (Codebase_Guide §3: 8x8 workgroup / storage image / dispatch 間 COMPUTE→COMPUTE バリア / GENERAL 運用) と BloomPass の downsample をそのまま流用。違いは「加重平均 → max リダクション」と入力深度の 2 点のみ。
- 1I で確立した compute 作法を踏襲する資料方針 (Codebase_Guide §3) にも合致。

### 遮蔽判定 (cull.comp 拡張・パス2 で使用)
- web 標準: オブジェクト境界を画面空間に投影 → フットプリントに合う mip レベルを選択 → オブジェクト最近接深度を pyramid の該当領域 max 深度と比較。**保守的に** (迷ったら描く)。
- §0.1-C の通り **half-extent 充填 → AABB 8 頂点を画面投影 → 画面 AABB → 対応 mip max 深度と比較**。sphere は粗い早期棄却に併用。
- 加えて §0.2-G で追加した **normal cone-vs-view test** で backface 棄却 (object 粒度でも cheap・meshlet 化したら同じ関数が活きる)。
- パス2 cull 最終行: `instanceCount = frustumVisible && !occluded && !backface ? 1u : 0u;` だが、rev.4 では compactCmd への前詰めなので `if (visible) compactCmd[scan_index] = template;`。

---

## 3. 所有構造とクラス設計

### 新クラス HiZPass (renderer/hiz_pass.*)
- **HiZPyramid を per-frame で 2 枚所有** (`MAX_FRAMES_IN_FLIGHT`)。⑤ の理由。
- 各 pyramid = 深度をダウンサンプルした mip 連鎖 (storage+sampled VmaImage、BloomPass の mip 列と同型)。フォーマット `R32_SFLOAT` (max リダクション値)。mip0 = 深度の 1:1 コピー (or 半解像度起点はノブ)、以降 max ダウンサンプル。
- `init(ctx, shaderDir, extent)` / `shutdown()` / `execute(cmd, frameIndex, depthView)` / `onSwapchainResized()`。
- getter: `pyramidView(frameIndex)` / `pyramidSampler()` (CullingPass が前フレーム分を読む)。
- compute 作法は Codebase_Guide §3 を踏襲 (8x8 workgroup / GENERAL 運用 / COMPUTE→COMPUTE バリア / 各 mip を storage image として書き次段が sampled で読む)。

### CullingPass の変更 (③)
- pipeline layout に **descriptor set 1 個追加** (`sampler2D` の HZB binding)。今までの「全 BDA・set ゼロ」から「BDA + サンプラ 1」へ。
- `ExecuteInfo` に `VkImageView hizPyramidView` / `VkSampler hizSampler` / `bool hizEnabled` (前フレーム pyramid 有効か) を追加。
- push constant に Hi-Z 用パラメータ追加 (画面寸法・mip 数・proj から画面投影に要る値)。現状 PushConstants は 116B (planes[6]+cullAddr+cmdAddr+objectCount) なので余裕を確認して追加。
- cull.comp に HZB サンプラ binding と遮蔽判定関数を追加。

### pass_chain の配線 (⑥)
- HiZPass を所有 (`hizPass_`)。MainPass.execute (L367 ブロック) の**直後**に `hizPass_.execute(cmd, frameIndex, swapchain depthView)`。
- CullingPass.execute (L356-365) に**前フレーム** pyramid を渡す: `ce.hizPyramidView = hizPass_.pyramidView(prevFrameIndex)` 等。prevFrameIndex の扱い (MAX_FRAMES_IN_FLIGHT のリング) と初回フォールバックをここで持つ。

### 3.1 スケール受け皿の設計 (rev.2・§0.1 の (A)〜(E) を構造として建てる)

**(A) draw-count バッファ + 可視コマンド圧縮**: cull.comp は既に per-draw 可視性を知っている。これを「instanceCount に書くだけ」で終わらせず、**可視 draw を前詰めした圧縮コマンド列 + GPU が書く draw 数 (count)** を生成する経路を建てる。
  - 構成: CullingPass が `cmdBuf_` (テンプレ) に加えて **`compactCmdBuf_` (圧縮後の VkDrawIndexedIndirectCommand 列)** と **`countBuf_` (uint32 draw 数、`INDIRECT_BUFFER` usage)** を per-frame で持つ。
  - cull.comp は可視なら `atomicAdd(count, 1)` で slot を取り `compactCmd[slot] = template` を書く (prefix-sum 版は後段最適化だが、まず atomic で受け皿を建てる。web: 大規模では Blelloch prefix-sum scan に上げる)。
  - main は `vkCmdDrawIndexedIndirectCount(compactCmdBuf_, 0, countBuf_, 0, maxDraws, stride)` で **bucket あたり最小回数**描く。
  - **能力ゲート (§5)**: `vkCmdDrawIndexedIndirectCount` は Vulkan 1.2 コア (または `VK_KHR_draw_indirect_count` / feature `drawIndirectCount`)。**非対応なら現状の instanceCount=0/1 + per-block-run indirect にフォールバック** (今あるパスがそのままフォールバックになる = 二重実装でなく退避先)。P620 の対応は実測 (§7)。
  - **block 切替との両立 (E)**: 圧縮列でも block 境界で vertex/index buffer の bind が変わる問題は残る。対策は builder で **blockIndex 順に drawId を採番 (block sort)** し、block ごとに 1 回の `...IndirectCount` を出す形に収束させる。block sort を「任意」から **(A) の前提作業**に格上げ。

**(B) 動的容量 (MAX_DRAWS 固定の撤廃)**: `cullBuf_` / `cmdBuf_` / `compactCmdBuf_` / DrawDataPool を GeometryBuffer (§5c) と同じ「満杯で伸びる」方式へ。Hi-Z 着手の前提作業として PART4-前 (§6) に置く。最低でも「初期値であって上限でない (超えたら伸びる経路を最初から実装)」(§5b)。

**(C) AABB 遮蔽 (half-extent 充填)**: `static_cull_build.h` で `CullObject.extentDrawId.xyz` に world AABB half-extent を充填 (cube=単位 AABB、Model=localAABB を model 行列で変換した extent)。cull.comp の遮蔽判定は **AABB 8 頂点を画面投影 → 画面 AABB の [min,max] と最近接深度 → 対応 mip の max 深度と比較**。sphere は粗い早期棄却に併用。これを 4c 本体に置く (4d 任意にしない)。

**(D) 可視性履歴の受け皿 (two-pass drop-in 用)**: per-object「前フレーム可視」フラグ列 (`visBuf_`、uint8/bit) を CullingPass に確保し、cull.comp が毎フレーム可視結果を書く。**PART4 baseline (1 フレーム遅延単一パス) では書くだけで読まない**が、これがあると two-pass occlusion (1 パス目 = 前フレーム可視分のみ描いて深度プライム → HZB → 2 パス目 = 残りを再判定) が**構造改修なしで drop-in** できる。受け皿だけ今建て、two-pass 実装は後段 PART。

**まとめ (実装段階 vs 受け皿)**: 実装は §6 で刻む。しかし (A) の count/圧縮バッファ・(B) 動的容量・(C) half-extent 充填・(D) 可視履歴バッファの**4つの受け皿は PART4 内で建てる**。これらを「77 draw で動くから後段」にしないことが、オープンワールド前提の Hi-Z と「これで動くだけの Hi-Z」を分ける。

### 3.2 最尖端構造 (rev.4・§0.2 の (F)〜(K) を構造として建てる)

**(F) Two-pass HZB occlusion を baseline 化**: §2 に書いた two-pass のフレームフローを CullingPass / MainPass の構造に落とす。
  - CullingPass が **per-pass で別個に dispatch** する: `executePass1(cmd, frameIndex, visBuf)` (前フレーム可視のみフラスタム + 圧縮) / `executePass2(cmd, frameIndex, hizPyramid, visBuf_write)` (全オブジェクト + HZB + 圧縮 + visBuf 更新)。
  - 圧縮バッファは **per-pass で別個**: `compactCmd1Buf_` / `countBuf1_` / `compactCmd2Buf_` / `countBuf2_` を per-frame で持つ。
  - MainPass.execute が 2 回 indirect draw call (パス1 → HiZ生成 → パス2)。pass_chain がオーケストレート。
  - 初回フレーム visBuf 全 false → パス1 ゼロ描画 → パス2 がフラスタムのみで全描画 (HZB 未生成は保守的に全可視扱い) → 次フレームから正常。

**(G) Meshlet-ready CullObject 型拡張**: `types.h` の `CullObject` を拡張。
  - 現状 (32B): `vec4 centerRadius` + `vec4 extentDrawId`。
  - rev.4 (拡張): 上に加え `vec4 coneApexCutoff` (xyz=cone apex, w=cos(cone半角)) + `vec4 coneAxisLodBias` (xyz=cone axis 正規化, w=LOD bias 等で予約) + `uint parentClusterId` + `uint childClusterIdMask`。後者2つは Nanite 風 cluster DAG の受け皿 (mesh shader 段階で使う・object 粒度では未使用)。
  - 拡張後サイズは std430 16B align で 64B 程度に収まる予定 (詳細は実装時確定)。push_constant でなく BDA でアクセスなので size 制約は緩い。
  - cull.comp に `bool coneVisible(vec3 apex, vec3 axis, float cosHalf, vec3 viewPos)` を追加 (cone-vs-view test = cheap な backface cluster cull)。object 粒度では cube/grass/全方向対称オブジェクトは cone を「無効化値」で詰めてスキップ。意味のあるメッシュ (壁・葉・板状物) では効く。
  - **builder (`static_cull_build.h`) で cone を計算して充填**: 静的メッシュは事前計算可能。

**(H) Depth-normal prepass (軽量 GBuffer) 共有**: 4a を「depth+normal を出すレンダーパス」として設計。
  - main_pass の opaque 描画で **MRT (multiple render targets)** = HDR color + normal (R10G10B10A2 or RG16F) を同時に書く。深度は通常通り。
  - or 別案: main_pass の前に専用 **prepass** (depth+normal のみ書く) を入れ、main_pass は depth `loadOp=LOAD` + `depthTestEnable=EQUAL` で重複描画排除 (early-Z 強化)。 トレードオフは実装時に決定 (P620 の memory bandwidth と shader cost で実測)。
  - normal RT は SSAO/SSGI/SSR/DoF の共有入力。HZB は別途深度から生成。
  - これで Phase 3 の各 SS 効果が個別に depth/normal prepass を後付けしない (Foundations §5 漏れ解消)。

**(I) Blelloch scan-based compaction**: atomic 起点を取らず最初から scan で実装。
  - cull.comp は予測 (述語 = 可視 か) のみ書き、別 compute パス (`scan_compact.comp`) が **Blelloch exclusive scan** で圧縮インデックスを算出 → 最終的に `compactCmd[scan_index[i]] = template[i]` (i が可視のとき) を書く。
  - thread group 内 scan は subgroup ops + shared mem で実装。block 跨ぎ scan は cascade (2 段) で対応。
  - block sort 済みなので block 内 scan で済む (block 跨ぎは block bounds で区切れる)。
  - 受け皿: `scan_compact.comp` 新設 + scan 中間バッファ (`scanIntermediate_`) を per-frame で確保。

**(J) Shadow_pass の GPU-driven 化**: 4-前 で建てる基盤を shadow にも同形で適用。
  - `ShadowCullingPass` を新設 (or CullingPass に shadow 用 execute を追加)。cull.comp は frustum を **shadow camera (light view-proj)** に差し替えるだけで同じシェーダで動く (引数化)。
  - 圧縮バッファ・count バッファ・block sort・drawIndirectCount すべて main bucket と同じ構造を shadow にも適用。
  - shadow_pass の draw を CPU loop から `vkCmdDrawIndexedIndirectCount` に切替。
  - per-light shadow HZB は本 PART4 のスコープ外 (Phase 後段)、shadow に Hi-Z occlusion 自体は次 PART で乗せられる受け皿を 4-前 段階で作る。

**(K) 永続 GPU オブジェクトバッファの受け皿**: CullObject 配列を per-frame 上書き前提から `persistent + dirty range update` に移行可能な形に。
  - CullingPass の `cullBuf_` を per-frame でなく **persistent (frame 共有)** にする (cursor は不要・更新は dirty range のみ)。CullObject の中身が変わるのはアセットロード/アンロード時とオブジェクト trans 変化時のみ。`grow` は (B) で対応済み。
  - builder は per-frame で「変化のあったオブジェクト」のみ更新する形に段階移行。初回 (rev.4 着手時) は **「全 dirty で毎フレーム全更新」のまま** = 受け皿の構造だけ persistent にし、動作は現状互換。後の Phase で dirty tracking を追加。
  - これだけで Foundations §4 「毎フレーム全 rebuild」の構造的解消経路ができる。

### 3.3 最尖端構造 第二弾 (rev.5・§0.3 の (M)〜(R) を構造として建てる)

**(M) HZB 生成 = SPD (Single Pass Downsampler) 実装**: HiZPass を BloomPass 雛形でなく SPD で実装。
  - 新 compute shader `hiz_spd.comp` を AMD FidelityFX SPD GLSL 版をベースに作成 (engine 慣習に合わせ調整)。
  - 1 thread group = 64×64 タイル担当。LDS 中間ストレージ。global atomic counter で thread group 間同期。最後の tile が last 6 MIP を処理。
  - 入力: 深度 image (R32 or D32_SFLOAT を R32 として sample)。出力: HiZPyramid (RG32F・min+max ペア・mip chain)。
  - User-defined 2x2 reduction = `vec2(min(d), max(d))` で min/max 同時計算 ((N))。
  - subgroup ops 対応版と LDS-only 版の 2 経路を能力 query で切替 (§5 capability)。
  - 受け皿: HiZPass の execute が 1 dispatch・per-frame pyramid (RG32F)・atomic counter buffer (per-frame の reset 必要)。

**(N) min+max ペア HZB**: 上記 SPD で同時生成。アクセス時は `.r` = min depth、`.g` = max depth。
  - cull.comp の遮蔽判定は `.g` (max) と比較 (現状の Hi-Z occlusion)。
  - `.r` (min) は将来用 (反射深度範囲・particle sort 等)。今は書くだけで読まないが受け皿として確保。
  - フォーマット: `VK_FORMAT_R32G32_SFLOAT`。両 channel storage image / sampled image として使う。

**(O) Reverse-Z depth 全面採用**: 描画パスのほぼ全体に波及する一括変更。
  - `vulkan_renderer.cpp:101`: `cameraParams = vec4(near=200.0f, far=0.1f, ...)` と near/far を**逆**にする (or 投影行列の作り方を reverse-Z 用に変える・実装は実コード見て決定)。
  - 全 render pass の `depth.loadOp = CLEAR` 時の `clearValues[1].depthStencil = {0.0f, 0}` (1.0 → 0.0)。main_pass.cpp:374 ほか。
  - 全 pipeline の `VkPipelineDepthStencilStateCreateInfo::depthCompareOp = VK_COMPARE_OP_GREATER` (or `GREATER_OR_EQUAL`)。main_pass.cpp:281 ほか。
  - shadow_pass / reflection_pass / depth-related shader 全部に波及。一括 commit (4-前-0 として PART4 着手の最初に行う)。
  - HZB は max を保持する形 (max depth = 「最も浅い = 0 に近い」値) のままで OK (テスト方向は変わるが意味は同じ)。

**(P) DGC (device-generated commands) の受け皿**: indirect 経路の上位互換として CullingPass / MainPass に分岐口を用意。
  - `vulkan_context` に `deviceGeneratedCommands()` getter 追加 (4-前-4 の `drawIndirectCount()` と同じ作法)。
  - MainPass.execute の indirect 描画箇所を 3 経路に: ① DGC 対応 → `vkCmdExecuteGeneratedCommandsEXT` / ② IndirectCount 対応 → `vkCmdDrawIndexedIndirectCount` / ③ 非対応 → BlockRange + 既存 indirect。
  - DGC 経路は 4d 以降の実装でよい (受け皿だけ 4-前 で確保)。`VkIndirectCommandsLayoutEXT` / `VkIndirectExecutionSetEXT` のラッパクラスを設計するが、空実装で能力 fallback できる形。

**(Q) Shader Object の受け皿**: VkPipeline 抽象化。
  - `vulkan_context` に `shaderObject()` getter 追加。
  - 新型 `ShaderProgram` (or `RenderShader` 等・命名は実装時) を導入: 内部に `VkShaderEXT[]` (shader_object 対応時) or `VkPipeline` (非対応時) を持ち、`bind(cmd)` で統一インタフェース。
  - 既存 `VkUnique<VkPipeline>` メンバを段階的に `ShaderProgram` に置換していく (4d 以降)。受け皿は今取る (新 pass 作成時から `ShaderProgram` を使う形で書く)。

**(R) Descriptor Buffer の受け皿 (Pascal 無効化付き)**: bindless / descriptor 管理の次世代。
  - `vulkan_context` に `descriptorBuffer()` getter 追加。**ただし内部で vendor=NVIDIA + Pascal generation (compute capability or device name 判定) なら強制 false を返す** = 駆動 bug 回避。
  - Foundations §3 の bindless 動的化と同時実装。BindlessTextureRegistry を descriptor buffer 経路に切替できる構造に。
  - 受け皿は今取る・実装は Foundations §3 着手時 (or P620 退役後)。

### 3.4 最尖端構造 第三弾 (rev.6・§0.4 の (S)〜(V) を構造として建てる)

**(S) Motion vector RT 出力**: 4a の MRT 化を 2 枚 → 3 枚に拡張。
  - MRT: ① HDR color (既存) ② normal (R10G10B10A2 or RG16F) ③ **motion vector (RG16F: ΔX/ΔY in NDC)**。
  - vertex/fragment shader 改修: 前フレーム MVP を frame UBO に追加・vertex で前フレーム位置を計算・fragment で NDC 差分を出力。
  - 前フレーム MVP は frame_uniforms.h の LightingUBO に追加 (or 専用フィールド)。
  - 受け皿: shader が motion vector を吐くだけ・読む側 (TAA) は Phase 3 で実装。今は MRT に書き出すだけで OK。

**(T) Dynamic rendering 受け皿**: `RenderTarget` 抽象型を導入。
  - 新型 `RenderTarget` (or `RenderingContext` 等・命名は実装時): 内部に dynamic rendering 用 `VkRenderingInfoKHR` 構築データ or 既存 `VkRenderPass + VkFramebuffer` を持つ。
  - `begin(cmd)` / `end(cmd)` インタフェース統一: 対応時 `vkCmdBeginRenderingKHR` / 非対応時 `vkCmdBeginRenderPass`。
  - `vulkan_context` に `dynamicRendering()` getter 追加 (Vulkan 1.3 core / `VK_KHR_dynamic_rendering` feature)。
  - 既存 `main_pass.cpp` / `shadow_pass.cpp` / `reflection_pass.cpp` / `bloom_pass.cpp` etc. が VkRenderPass を持つ箇所を段階的に `RenderTarget` 経由に置換 (4d 以降)。

**(U) Timeline semaphore 受け皿**: `FrameSync` を timeline 対応に。
  - `vulkan_context` に `timelineSemaphore()` getter 追加 (Vulkan 1.2 core)。
  - `FrameSync` 内部に timeline semaphore 経路 + binary semaphore 経路を能力分岐で持つ。外部インタフェース (acquireNextImage / present) は変えない。
  - 対応時: VkSemaphoreSubmitInfo に timeline value を載せ、wait/signal を timeline で表現。
  - Foundations §2 (async compute) と組み合わせて活きる。PART4 では受け皿だけ。

**(V) Async compute queue 受け皿**: `vulkan_context` で async compute queue family を取得。
  - キュー選択: graphics queue family と異なる compute capable family (NVIDIA は graphics と兼用が多い・AMD は dedicated compute family あり・Intel も最近対応)。
  - `vulkan_context::asyncComputeQueue()` getter 追加 (非対応なら graphics queue を返す = fallback)。
  - CullingPass / HiZPass の execute が queue 指定可能な API に (`execute(cmd, queue, ...)`)。今は pass_chain が graphics queue 上でしか呼ばないが、API 形は async 移行可能。
  - 実並列化は Foundations §2 着手時に行う (PART4 では受け皿だけ)。

---

## 4. 大規模前提の織り込み (ユーザー要件への具体回答)

- **解像度ベースでスケール**: HZB のコストは画面解像度依存で、**オブジェクト数・地形面積に依存しない**。数千〜数万 prop や広域地形でも HZB 自体は重くならない (大規模オープンワールドに本質的に向く)。620 では「重い(ノブ)」= mip 段数・判定解像度・起点解像度 (フル/半) がノブ (依存マップ §4)。
- **複数地面 (Phase 2F terrain bucket) への受け皿**: HiZPyramid は画面の深度ピラミッド 1 枚 (per-frame 2 枚) で、**全 bucket が共有して読む**もの。だから HZB を CullingPass の中に埋めず **独立した HiZPass の所有リソース**として作り、prop の cull も将来の terrain の cull も同じ pyramid をサンプルできる形にする (§0 受け皿を先に最新で)。しかも terrain は今 legacy CPU draw でも**深度バッファには描かれている**ので、**大きな地形は今でも prop の遮蔽器 (occluder) として効く** (HZB に乗る) = 大規模での Hi-Z の主効果。
- **複数光源 (Phase 2A clustered) との関係**: Hi-Z は**カメラ視点の可視性**判定で、ライティングと直交 (独立)。多光源化しても Hi-Z 無改修。shadow 用 per-light HZB は将来の別 Phase。
- **MAX_DRAWS=4096 固定の負債 (§5b)**: 大規模化で prop draw が 4096 を超えると溢れる。**rev.2 で「並行可・いつか」から PART4 前提作業に格上げ** (§0.1-B / §3.1-B)。Hi-Z は大量描画を可能にする機能なので、入力キャップを残すのは自己矛盾。GeometryBuffer (§5c) と同じ動的成長へ。
- **(E) CPU indirect 呼び出しの収束 (追跡課題)**: 現状は連続 block 区間ごとに `vkCmdDrawIndexedIndirect` (77 draw / 17-18 区間)。数万 draw・数百 block では区間数=CPU 呼び出し数が膨張し、GPU-driven の意義 (CPU 提出削減) が薄れる。**block sort (blockIndex 順に drawId 採番) + draw-count バッファ (§3.1-A) で「bucket あたり数回」に収束させる**。block sort は「任意の後段最適化」ではなく、大規模での core 課題として追跡 (§3.1-A の前提)。

---

## 5. 能力ゲート (§5b の宿題がここで発火)

資料 §5b は「最初に能力分岐が要る Phase = 2B の Hi-Z 着手直前に、capability 構造体の置き場と分岐の層を 1 箇所で決めてから書く」と指定。PART4 (4d) で決める。

- **Hi-Z 本体は Pascal (P620) で動く** (compute + 深度サンプルは標準機能)。能力で弾かれるものではない。
- **能力分岐の対象 (2 軸)**: ① **draw-count 圧縮** = `vkCmdDrawIndexedIndirectCount` (drawIndirectCount, Vulkan 1.2 コア / `VK_KHR_draw_indirect_count`) 対応なら GPU 供給 count で bucket あたり最小回数描画 (§3.1-A) / 非対応なら instanceCount=0/1 + per-block-run indirect の既存パスにフォールバック。② **HZB max ダウンサンプル** = `VK_EXT_sampler_filter_minmax` (minmax リダクションサンプラ) で 1 サンプル化 / 非対応なら compute で明示 max (4 テクセル読んで max)。**いずれも P620 で対応かは要実測** (着手時に web + 実機 query)。
- **§5b 指定の作業**: capability 構造体の住処 (どの struct に能力ビットを持つか) と分岐の層 (起動時・pass_chain・各 pass のどこで分岐するか) を 4d で 1 箇所に決め、Codebase_Guide に明文化する。`vulkan_context.h` の既存 `multiDrawIndirect_` / `drawIndirectFirstInstance_` パターンが参考。
- **実測してから書く (§5b)**: 「対応しているはず」で書き始めない。`vkGetPhysicalDeviceFeatures2` / `vkEnumerateDeviceExtensionProperties` で実測。

---

## 6. 分割案 (細かく commit・§1-2。描画が壊れたら戻れるように)

> **rev.5 の方針**: rev.4 の最尖端構造に加え、SPD・min+max HZB・Reverse-Z・DGC/shader_object/descriptor_buffer の能力ゲートを取り込み。Reverse-Z は全 pass に波及するため最初に一括対応 (4-前-0)。Hi-Z 本体は SPD で 1-dispatch 化。隣接最尖端 3 件 (DGC / shader_object / descriptor_buffer) は受け皿 + 能力ゲート + Pascal 安全弁を 4d で確立。

### 4-前-0 — Reverse-Z depth 全面切替 (Hi-Z 着手前提・全 pass 波及) — ✅ **完了 2026-05-28 commit 702c773**
- `vulkan_renderer.cpp:101`: cameraParams の near/far を reverse-Z 用に。
- 全 render pass の clear depth 値を `1.0f → 0.0f` (main_pass.cpp:374 / shadow_pass / reflection_pass)。
- 全 pipeline の `depthCompareOp` を `LESS → GREATER` (main_pass.cpp:281 ほか)。
- 投影行列の作り方を reverse-Z 用に (glm の perspective を near/far swap、or 専用ヘルパを書く)。
- **検証**: 描画不変 (depth 比較方向が反転しているだけで結果は同じ)・遠方の z-fighting が減少 (目視で確認)・validation/leak ゼロ。
- **commit 単位**: 1 commit (depth 関連を一括変更しないと中間状態が描画崩壊)。

### 4-前-1 — builder の block sort + main_pass の区間検出撤去 + BlockRange 配列導入 (シェーダ無改修) — ✅ **完了 2026-05-28 commit ff9f7a9**
- `static_cull_build.h`: emit を **blockIndex でソート**してから drawId を連番採番する形に変更。`BuildResult` に `std::vector<BlockRange>{ blockIndex, firstDraw, drawCount }` を追加。
- `main_pass.cpp:434-450`: runStart..runEnd の検出ロジック**撤去**。`BlockRange` を for ループで素直に `bindBlock → vkCmdDrawIndexedIndirect(off=range.firstDraw*stride, range.drawCount, stride)`。
- **検証**: 描画不変・validation/leak ゼロ。indirect 呼び出し回数が block 数に一致。
- **commit 単位**: 1 commit。

### 4-前-2 — CullObject の meshlet-ready 拡張 (§3.2-G) + builder で cone 充填 + cull.comp に cone test — ✅ **完了 2026-05-28 commit b8e39b2**
- `types.h CullObject` を拡張: `coneApexCutoff` / `coneAxisLodBias` / `parentClusterId` / `childClusterIdMask` を追加。32B → ~64B。
- `static_cull_build.h`: 静的メッシュは事前計算で cone を充填 (cube/grass は無効化値 = `cosHalf = -1` で常時 visible)。
- `cull.comp`: `bool coneVisible(...)` 追加。判定は `frustumVisible && coneVisible`。
- **検証**: 描画不変・将来 cone を本物にすれば backface 棄却が効くことを確認。
- **commit 単位**: 1 commit。

### 4-前-3 — Persistent CullObject buffer (§3.2-K) + CullingPass / DrawDataPool に grow 経路 + visBuf 確保 — ✅ **完了 2026-05-28 commit ec9c586**
- CullingPass の `cullBuf_` を persistent (frame 共有) に変更。MAX_DRAWS は**初期容量**に意味替え。
- `ensureCapacity(uint32_t need)` 追加: 旧バッファは DeletionQueue で遅延破棄 (§5c 作法)。
- `visBuf_` (per-object 可視フラグ、persistent) を確保。初期値 0。
- DrawDataPool も同様に grow 経路追加。
- **検証**: 現状 prop 数で挙動不変・validation/leak ゼロ。4096 超で grow 発火確認。
- **commit 単位**: 1 commit。

### 4-前-4 — Scan compaction (§3.2-I) + `compactCmd1/2Buf_` + `countBuf1/2_` + main を IndirectCount 化 + DGC 受け皿 — ✅ **完了 2026-05-28 commit 15b89ad**
- `compactCmd1Buf_` / `compactCmd2Buf_` / `countBuf1_` / `countBuf2_` を追加。grow 対象。
- 新 compute shader `scan_compact.comp` 追加: Blelloch exclusive scan で予測 → 圧縮インデックス → 書き込み。subgroup ops + shared memory。
- `cull.comp` は predicate のみ書く形に変更。
- `main_pass.cpp` indirect 呼び出しを **3 経路に能力ゲート**: ① DGC 対応 (§3.3-P・受け皿は今・実装は 4d) / ② IndirectCount 対応 → `vkCmdDrawIndexedIndirectCount` / ③ 非対応 → 4-前-1 の BlockRange + 既存 indirect。
- `vulkan_context` に `drawIndirectCount()` / `deviceGeneratedCommands()` getter 追加。
- **検証**: 対応 GPU では空 draw 走査が消える。fallback 描画が同等動作。validation/leak ゼロ。
- **commit 単位**: 1 commit。

### 4-前-5 — Shadow_pass の GPU-driven 化 (§3.2-J) — ✅ **完了 2026-05-28 commit 986ba44**
- `CullingPass::executeShadow(cmd, frameIndex, lightViewProj)` を追加。同じ cull.comp を frustum 引数差し替えで使う。
- shadow 用 `compactCmd_` / `countBuf_` / `BlockRange` を持つ。
- `shadow_pass.cpp` の CPU draw loop を IndirectCount に切替 (能力対応時)。
- **検証**: shadow 描画不変・main + shadow 両方で indirect 呼び出し収束。validation/leak ゼロ。
- **commit 単位**: 1 commit。

### 4a — Depth-normal-motion prepass (軽量 GBuffer・§3.2-H + §3.4-S) + 深度をサンプル可能に
- main_pass の opaque を **MRT 3 枚に拡張**: ① HDR color (既存) ② normal RT (R10G10B10A2 or RG16F) ③ **motion vector RT (RG16F: ΔX/ΔY in NDC)**。深度は `storeOp=STORE` + `SAMPLED` usage 追加 + メインパス後に SHADER_READ_ONLY へ遷移。
- normal 出力をシェーダに追加 (world-space normal を encode して書く)。
- **motion vector 出力**: vertex shader で前フレーム MVP と今フレーム MVP の両方から NDC 位置を計算、fragment shader で差分を吐く。frame_uniforms に **前フレーム MVP** を追加 (Phase 3 で TAA を載せたとき shader 改修不要)。
- **検証**: HDR color / 深度の描画不変。normal RT と motion vector RT を可視化して正しく出ているか確認。カメラ移動で motion vector が画面全体に出る・静止物は 0 になることを確認。
- **commit 単位**: 1 commit (MRT 3 枚 + 深度 SAMPLED + レイアウト遷移 + 前フレーム MVP 追加をまとめて)。

#### ✅ 4a-1 完了 (2026-05-28 commit af3dd72) — main_pass を Vulkan 1.3 dynamic rendering 化
- 4a を「VkRenderPass で MRT 3 枚」ではなく **「最新標準形の Dynamic Rendering (VK_KHR_dynamic_rendering / Vulkan 1.3 core) で MRT 3 枚」** で組むため、 前段として main_pass + child pipeline を VkRenderPass から `vkCmdBeginRendering` に移行 (§3.4-T 「Dynamic Rendering 受け皿」を 4d から繰り上げ)。
- main_pass の VkRenderPass + VkFramebuffer を撤去、 `VkRenderingAttachmentInfo` + `VkRenderingInfo` ベース。 pipeline は `VkPipelineRenderingCreateInfo` の pNext 経由で attachment format を宣言、 renderPass = VK_NULL_HANDLE。
- child pipeline (debug_line / hud / particle / water / imgui) も同経路に伝播。 ImGui_ImplVulkan_Init に `UseDynamicRendering = true` + `PipelineInfoMain.PipelineRenderingCreateInfo`。
- `vulkan_context::dynamicRendering()` capability 追加 (1.3 core、 P620 実測 1)。

#### ✅ 4a-2 完了 (2026-05-28 commit ed0d80e) — depth-normal-motion MRT + OverlayPass 分離 + GBuffer viewer
- opaque は **3 attachment MRT** (HDR + GBuffer normal **R10G10B10A2_UNORM (octahedral 20bit + 余裕 4bit)** + motion vector **RG16F (NDC ΔXY)**) + 深度の scope、 非 opaque (water/transparent/debug_line/particle) は 1 attachment HDR + 深度の scope、 という 2 scope 分割で Vulkan VUID-06195 整合。
- **HUD/ImGui は OverlayPass という新 dynamic-rendering pass に分離**: main_pass の MRT scope に巻き込まれない形にすることで mid-pass barrier (GBuffer 関連の attachment → read-only 中間遷移) を完全排除、 feedback-loop hazard も無いクリーン経路に。 HudPipeline / ImGui pipeline は `depthAttachmentFormat = VK_FORMAT_UNDEFINED` で rebuild、 OverlayPass の BeginRendering は color-only (HDR LOAD)。
- **新規ファイル**: `renderer/overlay_pass.{h,cpp}` (proper class)、 `renderer/gbuffer_debug_widget.{h,cpp}` (右上ドック ImGui::Image viewer, normal/motion/depth 表示・HDR は feedback-loop hazard で非表示)、 `renderer/depth_layouts.h` (separate vs combined depth/stencil layout 選択の一元化、 main_pass と widget が両方ここを呼ぶ)、 `shaders/shared/gbuffer.glsl` (octahedral encode + motion vector helper)。
- FrameUBO に `mat4 prevViewProj` 追加 (352B → 416B)。 host (`VulkanRenderer::buildCompleteFrameUBO`) は前フレーム末尾の `proj * view` をスナップ。
- **capability 追加**: `separateDepthStencilLayouts` (Vulkan 1.2 core optional, fallback あり)、 P620 実測 1。
- **教訓 (Work_Protocol §3-1a)**: 共有ヘッダの struct size 変更で clean rebuild を怠った結果、 mode select → 本編 startGame 遷移時のみ画面凍結 (TDR、 validation 無音 + run.log にも痕跡なし) を起こした。 当初は depth layout / OverlayPass の barrier / GBuffer feedback loop を疑って widget 抽出 / capability fallback / sync2 NONE 化 等の修正を順次入れたが全て効果無し、 最終的に clean rebuild が決定打となって解消。 根本原因の仮説 (FrameUBO レイアウト不一致で vertex shader の gl_Position が NaN / 巨大値になる) は Work_Protocol §3-1a 事例②に記録。 共有ヘッダ struct size 変更は **必ず clean rebuild**。

**4a-1 + 4a-2 完了 = 4a の受け皿 (深度サンプル可能 + GBuffer normal + motion vector + dynamic rendering + OverlayPass) 全部立った。** 次は 4b。

### 4b — HiZPass 新設 = SPD (Single Pass Downsampler・§3.3-M) で min+max ペア (§3.3-N) 生成 ✅ **完了 2026-05-28 commit ffe9673**
- **新規 `renderer/hiz_pass.{h,cpp}`** + **`shaders/hiz_spd.comp` (LDS-only) + `shaders/hiz_spd_wave.comp` (wave-ops)** + viewer **`renderer/hzb_debug_widget.{h,cpp}`**。 1 vkCmdDispatch で全 mip (1280×720 設定で 10 mip) 生成。
- アルゴリズム = AMD FidelityFX SPD の「64×64 source tile / 256 threads / LDS + atomic counter for last-group continuation」パターンを GLSL でハンドロール (FFX SDK 外部依存ゼロ・MIT-style 同等の単一 dispatch 設計)。
- HiZPyramid フォーマット = `VK_FORMAT_R32G32_SFLOAT` (.r=min / .g=max・(N))。 per-frame 2 枚 (`MAX_FRAMES_IN_FLIGHT`)、 dedicated `VmaImage`、 `STORAGE | SAMPLED`、 per-mip storage view + 1 full-chain sampled view。 atomic counter は `VmaBuffer::createMappedStorageBDA + TRANSFER_DST` で取得、 execute 冒頭に `vkCmdFillBuffer(0)` でリセット。
- capability gate (§5b 実測してから書く): **`subgroupOps` = BASIC + SHUFFLE in COMPUTE** (wave shader が使う `subgroupShuffleXor` のみが SHUFFLE 要求、 ARITHMETIC / QUAD は未使用なので query しない・P620 実測 1) + `subgroupSize >= 32` (16x16 thread grid で `subgroupShuffleXor(_, 16)` が同一 subgroup 内で完結する条件、 P620 実測 32) + `shaderStorageImageArrayDynamicIndexing` (per-mip storage image array を loop-uniform 動的 index で引く必須 feature、 P620 実測 1)。 **wave-ops 派生 spv は本実装** (`hiz_spd_wave.comp`)= Phase C (mip1→mip2) の LDS round-trip を `subgroupShuffleXor` 経由に置換 (xor 1 = x neighbor / xor 16 = y neighbor / xor 17 = diagonal、 4 mip1 値を register に取得して reduce4)、 Phase D-F は LDS のまま (8x8 以下の reduction は subgroup 境界を越えるため)。 LDS-only 派生 (`hiz_spd.comp`) は subgroup ops 非対応 / subgroupSize < 32 device 用 fallback。 atomic counter は **device-local** `VmaBuffer::createDeviceLocal` (BDA / host-visible 不要)。
- depth descriptor write は `depth_layouts::readOnly(*ctx_)` 経由 (固定 `DEPTH_READ_ONLY_OPTIMAL` だと `separateDepthStencilLayouts` 非対応 device で VUID-vkCmdDispatch-imageLayout に遭うため、 main_pass post-barrier と必ず同期)。
- **メモリ同期 (Vulkan Memory Model 整合)**: (a) **cross-group acquire** = last group のループ前に `memoryBarrierImage()` (device-scope、 他 group の mip5 write を取得); (b) **within-group iteration acquire** = ループ内の write→read 可視性は `groupMemoryBarrier()` (workgroup-scope、 同一 workgroup 内なので十分・device-scope より軽量)。 `barrier()` 単独では coherent storage-image の cross-invocation 可視性は保証されないため両層必須。 修正は両 spv 共通 (通常 GPU の L1 coherence で偶然通るが仕様上 undefined であったのを fix)。
- pass_chain 配線: `mainPass_.execute()` 直後・ `overlayPass_.execute()` の前 (depth が `depth_layouts::readOnly()` に遷移済みのタイミング = mid-pass barrier 不要)。 `onSwapchainResized` で pyramid を depth 解像度に追従 rebuild。
- **cull は無改修** (4c で拡張)。 検証: ビルド成功 + 両 spv 生成 (20440B / 20068B)・ exe 起動はユーザーの希望でスキップ (PC クラッシュリスク懸念)、 ソース review で重大バグ 2 件 (depth layout / image memory visibility) を発見・修正後の commit。 viewer は右下ドック (frame / mip スライダ・RG32F で min を赤・max を緑同時表示)。
- types.h 不変 = §3-1a clean rebuild 不要・ incremental build で通過。

### 4c — Two-pass occlusion 本体 (§3.2-F) + AABB 遮蔽 (§0.1-C) + cull.comp 拡張
**ここが Hi-Z PART4 の最尖端核**。
- CullingPass に `executePass1` / `executePass2` を分離。pass_chain がオーケストレート: パス1 cull → パス1 draw → HiZ生成 (SPD) → パス2 cull → パス2 draw。
- visBuf を cull.comp が読み書き (パス1 読む・パス2 書く)。
- `static_cull_build.h` で half-extent 充填。
- `cull.comp`: HZB サンプラ (descriptor set 1) 配線。AABB 8 頂点を画面投影 → mip 選択 → HZB の `.g` (max) と比較 (Reverse-Z なので比較方向は inverted)。sphere 早期棄却 + cone test + frustum + occluded の AND。
- **検証**: 二段描画で pop が消える。Cull HUD の分子がパス1+パス2 で正しい総和。over-cull なし。
- **commit 単位**: 1 commit (two-pass 切替は一括)。

### 4d — 能力ゲート確定 + フォールバック整備 + 隣接最尖端の受け皿 + 仕上げ
- **能力ゲート (§5)**: minmax サンプラの実機対応を確定 (`VK_EXT_sampler_filter_minmax`)。**非対応フォールバック = compute 明示 max** (SPD 内で対応)。
- **能力 query を `vulkan_context::capabilities()` struct に集約** (1 箇所に決め Codebase_Guide 明文化・§5b 指定):
  - `drawIndirectCount` (Vulkan 1.2 core / `VK_KHR_draw_indirect_count`)
  - `deviceGeneratedCommands` (DGC・`VK_EXT_device_generated_commands`)
  - `shaderObject` (`VK_EXT_shader_object`)
  - `descriptorBuffer` (`VK_EXT_descriptor_buffer`・**Pascal 強制無効化ロジック組込**)
  - `dynamicRendering` (Vulkan 1.3 core / `VK_KHR_dynamic_rendering`・§3.4-T)
  - `timelineSemaphore` (Vulkan 1.2 core / `VK_KHR_timeline_semaphore`・§3.4-U)
  - `samplerFilterMinmax` (`VK_EXT_sampler_filter_minmax`)
- **DGC 受け皿 (§3.3-P)**: `VkIndirectCommandsLayoutEXT` / `VkIndirectExecutionSetEXT` のラッパクラス雛形を導入 (空実装で能力 fallback できる形・実 DGC 経路は対応時のみ)。
- **Shader Object 受け皿 (§3.3-Q)**: `ShaderProgram` 抽象型を導入 (内部に `VkShaderEXT[]` or `VkPipeline`)。既存 VkPipeline メンバを段階移行できる形。
- **Descriptor Buffer 受け皿 (§3.3-R)**: getter に **Pascal 強制無効化**ロジック組み込み (vendor=NVIDIA + Pascal generation 判定で false 返す)。
- **Dynamic rendering 受け皿 (§3.4-T)**: `RenderTarget` 抽象型を導入 (内部に `VkRenderingInfoKHR` or `VkRenderPass + VkFramebuffer`)。既存 pass の VkRenderPass 持ちを段階移行できる形。
- **Timeline semaphore 受け皿 (§3.4-U)**: `FrameSync` 内部に timeline / binary の能力分岐経路を追加。外部インタフェース不変。
- **Async compute queue 受け皿 (§3.4-V)**: `vulkan_context` で async compute queue family を取得・`asyncComputeQueue()` getter 追加 (非対応なら graphics queue を返す fallback)。CullingPass / HiZPass の execute を queue 指定可能な API 形に。
- ⑦ 一時ログ (`[BlockDbg]` / `[Cull2B]`) 掃除。「純 GPU-driven 化仕上げ」(CullingPass の CPU オラクル + readback 撤去) を同時に。
- **commit 単位**: 能力 query 集約 + 受け皿雛形 (DGC / shader_object / descriptor_buffer / dynamic rendering / timeline semaphore / async compute) + 仕上げで 2-3 commit に分けて進める (一括は変更が大きすぎる)。

### 4b 完了後の残作業 (意図的に未対応・別 commit で着手可)

ソース再点検 (2026-05-28、 commit ffe9673 直前) で見つかった改善余地。 correctness bugs は全て修正済み (Bug 1 / 2 / 3 + Obs A / E + kMaxFramesInFlight 一元化)。 以下は **動作は正しいが「より正しい」形にできる**項目。 ユーザー判断で 4b スコープでは見送り。

- **Obs B: 初期 layout 遷移の stage flag が legacy 形式** ([hiz_pass.cpp:418-419](src/renderer/hiz_pass.cpp#L418-L419))
  ```cpp
  ib.srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;  // sync2 では deprecated
  ib.srcAccess = 0;
  ```
  - **正しい sync2 形式**: `VK_PIPELINE_STAGE_2_NONE` + `VK_ACCESS_2_NONE` (UNDEFINED→GENERAL は「何の同期も要らない」のだから明示的に NONE が筋)。
  - **影響**: P620 の validation layer は警告しない (sync2 が TOP_OF_PIPE_BIT を後方互換で受ける)。 一部の新しめドライバ + strict mode で warning が出る可能性。
  - **修正コスト**: 2 行変更だけ。 4d で他 pass の barrier 全部を sync2 best practice 化するときに巻き取る方が筋。
  - **着手時期**: 4d (能力ゲート集約 + barrier 統一掃除のとき)。

- **Obs C: VK_FORMAT_R32G32_SFLOAT storage image 対応の format properties query 無し** ([hiz_pass.cpp:81](src/renderer/hiz_pass.cpp#L81) `createPyramids`)
  - 現状: pyramid を `VK_FORMAT_R32G32_SFLOAT` で無条件に作る。 P620 (Pascal) は対応・desktop GPU 全数対応だが、 Vulkan spec 上は **optional feature** (`vkGetPhysicalDeviceFormatProperties(...).optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT`)。
  - **§1.5-C 「能力チェック + フォールバック」厳密適用なら query + fallback (例: R32_SFLOAT で min のみ、 max は別 pass) が筋**。 ただし fallback 経路を実装すると複雑化。
  - **影響**: P620 / 現代 desktop GPU では実害ゼロ。 古い integrated GPU で起動失敗の理論的リスク。
  - **修正コスト**: query は 5 行。 fallback shader 実装は ~100 行 + 動作検証。
  - **着手時期**: P620 以外の hardware で動作確認するときに合わせて (今は不要)。

- **Obs D: subgroup ID → linearIdx の canonical mapping 前提** ([hiz_spd_wave.comp](shaders/hiz_spd_wave.comp))
  - wave shader は `gl_LocalInvocationIndex` から `(tx, ty) = (idx & 15, idx >> 4)` を計算し、 `subgroupShuffleXor(_, 16)` で y-neighbor を取る。 これは **「subgroup invocation = linearIdx % subgroupSize」「subgroup ID = linearIdx / subgroupSize」**という線形マッピングを仮定。
  - **Vulkan spec 上は implementation-defined**。 NVIDIA Pascal+ / AMD GCN+ / Intel Skylake+ の desktop ドライバは canonical linear で実装している (実証済み)。 mobile GPU や exotic 実装で異なる可能性は残る。
  - **完全 portable にするには** `VK_EXT_subgroup_size_control` + `VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT` で mapping を固定する、 または shader を `gl_SubgroupID` / `gl_SubgroupInvocationID` ベースに書き換える ([参考: Khronos subgroup tutorial](https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/extensions/VK_KHR_shader_subgroup.adoc))。
  - **影響**: P620 では実害ゼロ (linear mapping)。 完全 portable 要求は今のところ無し。
  - **修正コスト**: REQUIRE_FULL_SUBGROUPS_BIT を pipeline create に追加するなら ~10 行。 shader を subgroup ID ベースに書き換えるなら ~50 行。
  - **着手時期**: mobile / 異種 GPU 対応を本格化するとき (Roadmap 上未予定)。

### 後段 (別 PART・PART4 のスコープ外だが受け皿は確保済み)
- **Dirty tracking で persistent CullObject の差分更新** (受け皿 K 利用)。
- **Shadow 用 per-light HZB** (受け皿 J 利用)。
- **terrain bucket (Phase 2F) での HZB / GPU-driven 共有**。
- **Mesh shader 段階での meshlet cull** (受け皿 G 利用)。
- **DGC 経路の実装** (受け皿 P 利用・能力対応 GPU で測定)。
- **Shader Object 経路への移行** (受け皿 Q 利用・既存 VkPipeline を段階置換)。
- **Descriptor Buffer 経路** (受け皿 R 利用・P620 退役後 or 非 Pascal で有効化)。
- **Dynamic rendering への完全移行** (受け皿 T 利用・全 pass の VkRenderPass を `RenderTarget` 抽象経由に段階置換)。
- **Timeline semaphore への完全移行** (受け皿 U 利用・FrameSync を timeline 主軸に・Foundations §2 と一緒)。
- **Async compute での HZB / cull 並列化** (受け皿 V 利用・Foundations §2 着手時)。
- **TAA / TSR / FSR / DLSS 等の temporal upscaling** (Phase 3 post AA・受け皿 S の motion vector RT を利用)。
- Roadmap 既出: clustered LOD / Nanite 風 / virtualized geometry。

---

## 7. 着手前に確認する未確定項目 (推測で埋めない・§1-1)

- **【4a 着手時に必ず確認】深度 image の現状 usage フラグ**: `swapchain.cpp` の `createDepthResources()` の**定義本体**がまだ取れていない (dump が呼び出し箇所 L86/L128 にヒットした)。深度 image が今 `DEPTH_STENCIL_ATTACHMENT` だけか、既に `SAMPLED` を持つか未確認。4a で `createDepthResources` 定義を dump し、`SAMPLED` usage の要追加可否を確定してから置換する。
  - 確認コマンド例: `swapchain.cpp` 全体から `VkImageCreateInfo` / `usage` / `VK_IMAGE_USAGE` を含む行を出す、または `createDepthResources` の定義行 (`void Swapchain::createDepthResources`) を探して本体を出す。
- **メイン深度の実フォーマット** (④): `D32_SFLOAT` か stencil 付きか。stencil 付きだと aspect 指定が変わる。4a の深度可視化/HZB 入力で aspect を正しく扱うため実機 query 値を確認。
- **CullingPass PushConstants の空き**: Hi-Z パラメータ追加で push constant 上限 (128B 保証) に収まるか。現状 116B。収まらなければ UBO か追加 BDA に逃がす。
- **minmax サンプラの P620 対応** (§5): 実機 query で確認してから 4d の経路を確定。
- **`vkCmdDrawIndexedIndirectCount` / `drawIndirectCount` の P620 対応** (§3.1-A / §5): Vulkan 1.2 コア相当だが、feature `drawIndirectCount` or `VK_KHR_draw_indirect_count` の有無を実機 query。非対応なら instanceCount=0/1 + per-block-run indirect の既存パスにフォールバック (退避先として温存)。
- **動的容量化の影響範囲** (§3.1-B): `cullBuf_`/`cmdBuf_`/`compactCmdBuf_`/DrawDataPool を multi-block 化する際、BDA アドレスが block ごとに変わる点と cull.comp の addressing を確認 (GeometryBuffer の multi-block 教訓 §5c を流用)。
- **atomic 圧縮の競合コスト** (§3.1-A): 4c は atomicAdd で受け皿を建てるが、可視数が多いと atomic 競合が出うる。prefix-sum (Blelloch) への引き上げ判断は実測してから (後段)。

---

## 8. 正本へ畳む先 (PART4 完了時・§6 運用)

PART4 完了時、この設計書の確定事項を以下へ反映してこの作業正本を畳む:
- **START_HERE §2**: 現在地を PART4 完了に、次の一手を更新。
- **Roadmap §4** (2B 発展節 = Hi-Z occlusion) を進行中/完了に + **付記**に PART4 成果エントリ新設 + §5 推奨の一手更新。
- **依存マップ** §0 層図・§4 Hi-Z ノードを完了に + §6 着手順。
- **Codebase_Guide §2** に hiz_pass.* 追加 + §3.5 に Hi-Z データフロー + §3 に確定事実 (1 フレーム遅延 HZB・per-frame pyramid・descriptor 変化・minmax 能力ゲート)。
- **Work_Protocol §5f** (新設) に PART4 Hi-Z 確定事項。
- 各ファイル冒頭の rev/最終更新日も更新。
