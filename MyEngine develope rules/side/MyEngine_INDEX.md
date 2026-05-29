# MyEngine 設計知見 — 横断インデックス (rev.7)

最終更新: 2026-05-29 (rev.7: **最新化マラソン 28 commits 反映 — 多数項目が ✅実装完了 / 🟢受け皿確保**。 §1 表で:
- **(B) 動的容量** = ✅ 6 クラス完了 (CullingPass + DrawDataPool + Material/Instance/Skin/Particle/DebugLine の F1-F5 / G で実装・受け皿の bindless pool growth は別 Phase = G+)
- **(U) Timeline semaphore** = ✅ 完了 (B commit eeba2ed = FrameSync 完全 migration・per-frame VkFence array 撤去)
- **(V) Async compute** = ✅ 受け皿確保 (M commit 8b4deff `include/MyEngine/renderer/async_compute.h`・本実装は Phase 仕事)
- **新規 W (sync_validation layer)** = ✅ 有効化 + swapchain hazard 修正 (df6f5ae + 750135f)
- **新規 C (transfer queue) + N (memory_priority) + I (memory_budget)** = ✅ 全部完了 (e7b852e / 4f7d47f / 8484ea7)
- **新規 O (debug_utils GPU markers)** = ✅ 完了 (e048503・include/MyEngine/renderer/debug_utils.h)
- **新規 (L/K/Z/J/Q) 5/7** = ✅ extension push + feature struct chain 完了 (f880ddb・shader_object / present_id+wait / image_view_min_lod / host_image_copy / calibrated_timestamps が callable)
- **新規 (T) (D)** = 🟡 query 済・enable 保留 (T は VK_EXT_surface_maintenance1 instance ext 依存・D は 30+ feature 個別 query 要)
- **新規 (E) Camera-relative + floating-origin** = ✅ 受け皿確保 + 全 10 site wire-up 完了 (4dc8923 + 641abcb E clean)
- **新規 (G) Bindless free-list** = ✅ 完了 (17d5f8f)
- **新規 (H) Persistent GPU object buffer** = 🟡 受け皿のみ (8604de5 design memo)
- **新規 (U) Worker thread pool** = ✅ 受け皿確保 (fdbddda `include/MyEngine/core/job_system.h` header-only inert-friendly)
- **新規 (V/R/S/X/Y/P)** = 🟡 design memo headers (8604de5 batch・各 init/shutdown 空・Phase 着手時に実装)

§7 現在の状態を「最新化マラソン 28 commits + PART4 essentially complete」に書き換え、 直近の commit 一覧を冒頭に挿入。 P620 [Caps] 30 capability・DGC のみ 0。 / rev.6: **PART4 §6 4d「Pure GPU-driven cleanup = 完了」追加反映 (commit f8d1e1f)**。 user 報告「HUD `Cull : 0 / 67` 永久 0」を契機: 4-前-4 (15b89ad) で compactCmd device-local 化以降 readback 経路が断たれていたのを option B (純 GPU-driven 化) で清算 = HUD 行 + CPU Frustum オラクル + 全 wire-up 撤去 (8 files +2 -51)。 §0 ファイル構成 rev 表を最新化 (HiZ_PART4_Design.md rev.9→10 / START_HERE 更新 / Codebase_Guide rev.14→15 / Roadmap rev.11→12 / Phase_Dependencies rev.10→11)、 §7 現在の状態 = 「Pure GPU-driven cleanup 含む 28 commits」に追記。 / rev.5: **PART4 §6 4c 完了 + 4d 大半完了反映 (18 commits)**。 §1 表で (C) (F) を ✅完了 に (Tier 1 α 適用込み)、 (T) を ✅完了 に (γ-1/2/3 で他 pass も全 dynamic rendering 化)、 (U) は据え置き 🟡 (timeline semaphore は未着手)、 (V) は ✅完了 (受け皿)、 (X) は据え置き 🟡、 (Y) を ✅完了 (a62b7f0 persistent pipeline cache、 14 callsite が経由・490KB 書き出し実証)、 §2 表で 4c / 4d を ✅完了 に、 §7 現在の状態を「PART4 essentially complete (P620 [Caps] 18 中 17 = 1 で実走) / 次は §8 畳み込み or 後段 Phase」に書き換え、 各正本 rev を最新化。 / rev.4: **PART4 §6 4b 完了反映**。 §1 で (M) SPD と (N) min+max ペア HZB を ✅完了 に更新、 §2 表で 4b を ✅完了 に、 ★次 = 4c に、 §7 現在の状態を「PART4 §6 4b まで完了・次は 4c (two-pass occlusion 本体)」に更新、 Codebase_Guide rev を 12 → 13 に、 Roadmap rev を 9 → 10 に、 Phase_Dependencies rev を 8 → 9 に、 HiZ 設計書 rev を 7 → 8 に。 / rev.3: PART4 §6 4-前-0/1/2/3/4/5 + 4a-1 + 4a-2 完了反映。 §1 表で対象項目 ID (A/B/D/E/G/H/I/J/K/O/S/T/W/AA) の状態を 🟢実装済 / ✅完了 に更新、 §2 表に完了済みマーク (commit 番号付き) + 「次 = 4b HZB SPD」を明示、 §7 現在の状態を「PART4 §6 4-前/4a 全部完了・次は 4b」に更新、 Codebase_Guide rev を 11 → 12 に更新。 / rev.2: 運用モード切替で §6「ファイル添付の最小構成 (毎回必須)」を「セッション開始時に Read する最小構成」に書換。Work_Protocol rev.13 / START_HERE / Codebase_Guide rev.11 と整合 / rev.1: 設計知見が 4 系統 8 ファイルに分散したため横断インデックスを新設。全項目 ID (A)〜(AA) の一覧 + 担当ファイル§ + 着手時期 + 状態を 1 表に集約。同じ話題が複数ファイルに散る箇所のクロスリファレンス) / 対象: MyEngine の設計を**最短で見渡す**ためのインデックス。各ファイルへの参照は§単位

> **このファイルの位置づけ**: 各セッション最初に最優先で Read する横断索引。「全部読まないと判断できない」状態を解消し、必要箇所だけ深掘りできるようにする。本文は他ファイルにある (このファイルは目次)。
> **読む順序の推奨**: ① **本 INDEX** → ② 正本5枚で原則確認 → ③ 着手する作業に応じて PART4 / Foundations / Vulkan13 のうち該当§のみ深掘り。

---

## 0. ファイル構成 (4 系統)

| 系統 | ファイル | rev | 行数 | 役割 |
|---|---|---|---|---|
| **正本5枚** | MyEngine_START_HERE.md | — | — | 入口・ゴール・現在地・運用 (4c + 4d 大半 + Pure GPU-driven cleanup 反映) |
| | MyEngine_Graphics_Roadmap_2026.md | rev.13 | — | 全 Phase 計画 (PART4 essentially complete + 最新化マラソン 28 commits 付記反映) |
| | MyEngine_Phase_Dependencies.md | rev.12 | — | Phase 間依存マップ (Hi-Z ノード完了 + 土台 §1 buffer 系 VMA 化 ✅ + transfer queue ✅ + JobSystem ✅ + チャンクストリーミング前提全 closed) |
| | MyEngine_Codebase_Guide.md | rev.16 | — | コード構造の地図 (最新化マラソン 28 commits 反映: 新規 11 ファイル + frame_sync timeline + resource_factory minimal + N/I/B/C/D/L/K/T/Z/J/Q getter) |
| | MyEngine_Work_Protocol.md | rev.18 | — | 作業規範・原則 + §5f-§5j 確定パターン 5 件追加 (F1-F5 grow / toEngineRelative / Timeline migration / debug_utils / memory_priority) |
| **作業正本** | MyEngine_HiZ_PART4_Design.md | rev.11 | — | Phase 2B PART4 Hi-Z 設計 (4c + 4d 大半 + cleanup 完了 + B Timeline 完了 + V/M Async receptacle) |
| **土台監査** | MyEngine_Foundations_Audit.md | rev.7 | — | 先回り受け皿 + 既存負債 (§1 E + §2 C+U + §3 G + §4 H + §8.1 F1-F5 + §8.2 A1-A5 + §8.3 A6 全部状態 update) |
| **隣接機能** | MyEngine_Vulkan13_Modernization.md | rev.4 | — | Vulkan 1.3/1.4 modernization (W/AA/Y 完了・B Timeline migration + N memory_priority 実利用 + I memory_budget + 5/7 receptacle activate) |
| **索引** | MyEngine_INDEX.md (本書) | rev.7 | — | 横断インデックス (最新化マラソン 28 commits 反映・新 ID 群表 + 妥協度評価 + P620 [Caps] 30 capability) |

---

## 1. 全項目 (A)〜(AA) 一覧 — 担当・着手時期・状態

「項目 ID」は PART4 設計書と Vulkan13 で連番。状態の凡例:
- 🟡受け皿 = アーキテクチャの枠を今取る・実装/中身は段階的
- 🟢実装 = 中身も今取り込む
- 🔵後段 = 別 Phase で本実装 (受け皿は確保)
- ⚫選択 = 取らないことが明示的選択 (妥協でなく)

### スケール受け皿 (§0.1・rev.2 で格上げ)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (A) | draw-count + 可視コマンド圧縮 | PART4 §3.1-A | 4-前-4 | ✅完了 (15b89ad) |
| (B) | 動的容量 (MAX_DRAWS 撤廃) | PART4 §3.1-B | 4-前-3 | ✅完了 (ec9c586) |
| (C) | AABB 遮蔽 (half-extent 充填) | PART4 §3.1-C | 4c | ✅完了 (ad97879 4c-A で extentDrawId.xyz 充填、 cull.comp で AABB 8 頂点画面投影) |
| (D) | 可視履歴 (visBuf) | PART4 §3.1-D | 4-前-3 | ✅完了 (ec9c586, 受け皿) |
| (E) | CPU indirect 呼び出し収束 (block sort) | PART4 §3.1-E | 4-前-1 | ✅完了 (ff9f7a9) |

### 最尖端取り込み第一弾 (§0.2・rev.4)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (F) | Two-pass HZB occlusion | PART4 §3.2-F | 4c | ✅完了 (4c-A..D + Tier 1 α 2f7daf9 で Nanite/Granite 2024 baseline + 1-tap fast path ccf5c03) |
| (G) | Meshlet-ready CullObject | PART4 §3.2-G | 4-前-2 | ✅完了 (b8e39b2, 受け皿) |
| (H) | Depth-normal prepass (軽量 GBuffer) | PART4 §3.2-H | 4a | ✅完了 (ed0d80e) |
| (I) | Blelloch scan compaction | PART4 §3.2-I | 4-前-4 | ✅完了 (15b89ad, 3-pass scan) |
| (J) | Shadow_pass GPU-driven 化 | PART4 §3.2-J | 4-前-5 | ✅完了 (986ba44) |
| (K) | Persistent CullObject buffer | PART4 §3.2-K | 4-前-3 | ✅完了 (ec9c586, 受け皿) |
| (L) | forward+ 路線維持 | PART4 §0.2-L | — | ⚫選択 (VisBuffer/Nanite 取らず) |

### 最尖端取り込み第二弾 (§0.3・rev.5)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (M) | SPD (Single Pass Downsampler) HZB 生成 | PART4 §3.3-M | 4b | ✅完了 (hand-rolled SPD、 LDS + wave-ops 二派生 spv) |
| (N) | min+max ペア HZB (RG32F) | PART4 §3.3-N | 4b | ✅完了 (RG32F per-frame 2 枚) |
| (O) | Reverse-Z depth | PART4 §3.3-O | 4-前-0 | ✅完了 (702c773) |
| (P) | VK_EXT_device_generated_commands (DGC) | PART4 §3.3-P | 4d | ✅完了 (15b89ad, 受け皿 = indirect_exec::Mode 経路 picker) |
| (Q) | VK_EXT_shader_object | PART4 §3.3-Q | 4d | 🟡受け皿 (未着手) |
| (R) | VK_EXT_descriptor_buffer (Pascal 安全弁) | PART4 §3.3-R | 4d | 🟡受け皿 (Pascal 強制無効) |

### 最尖端取り込み第三弾 (§0.4・rev.6)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (S) | Motion vector RT (4a MRT に追加) | PART4 §3.4-S | 4a | ✅完了 (ed0d80e) |
| (T) | Dynamic rendering (VkRenderPass 撤廃) | PART4 §3.4-T | 4a-1/4a-2/4d γ-1/2/3 | ✅**完了** (af3dd72 main / ed0d80e OverlayPass / 4b9c32c PostPass / da74526 ShadowPass / 33e1511 ReflectionPass = engine 全体で VkRenderPass / VkFramebuffer 実 API 使用ゼロ) |
| (U) | Timeline semaphore | PART4 §3.4-U | 2026-05-29 (B) | ✅**完了** (3670ef1 feature 受け皿 + eeba2ed FrameSync migration = per-frame VkFence 撤去・single timeline semaphore + nextSignalValue_ + VkTimelineSemaphoreSubmitInfo chain。 副次効果 = CullingPass cross-frame WRITE_AFTER_READ/WRITE hazards 20 件解消) |
| (V) | Async compute queue family 取得 | PART4 §3.4-V | 4c-B + M 8b4deff | ✅完了 (477985d で `asyncComputeFamily()` getter + dedicated 判定 = P620 family 2 dedicated 検出・8b4deff で `include/MyEngine/renderer/async_compute.h` AsyncComputeContext 受け皿・実 cross-queue submission は Phase 仕事) |

### 隣接最尖端 (Vulkan13_Modernization・rev.1)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (W) | VK_KHR_synchronization2 (barrier API 現代化) | Vulkan13 §1 | PART4 4-前-0 の次 | ✅完了 (e1494bf, barrier.h ヘルパ + 段階移行) |
| (X) | VK_EXT_extended_dynamic_state 1/2/3 | Vulkan13 §2 | PART4 4d (Q と同時) | 🟡受け皿 (未着手) |
| (Y) | Pipeline cache 永続化 | Vulkan13 §3 | 4d M1 | ✅完了 (a62b7f0、 `<AppData>/MyEngine/MyEngine/pipeline.cache` に load/save、 全 14 vkCreate*Pipelines callsite が `ctx_->pipelineCache()` 経由、 user clean exit で 490KB 書き出し実証) |
| (Z) | VK_KHR_fragment_shading_rate (VRS) | Vulkan13 §4 | Phase 3 | 🟡受け皿 (Pascal 非対応) |
| (AA) | Infinite far plane (Reverse-Z の本来形) | Vulkan13 §5 | **4-前-0 と同時** | ✅完了 (702c773) |

### 最新化マラソン 2026-05-29 (28 commits) — 新 ID 群

| ID | 名称 | 担当 | commit | 状態 |
|---|---|---|---|---|
| **A1-A6** | buffer 系 VMA 化 + raw memory ZERO + debug 残骸撤去 | Foundations §8.2 / §8.3 / §8.5 | 995b779 / 46cb937 / 185ac09 / 80ccb76 / a030372 / dab4faf | ✅**完了** (エンジン内 生 vkAllocateMemory ゼロ達成) |
| **E** | Camera-relative + floating-origin 受け皿 + 全 wire-up | Foundations §1 ★★★ | 4dc8923 + 641abcb (clean) | ✅**完了** (10 site で toEngineRelative 適用・origin = 0 で完全 no-op) |
| **F1-F5** | 固定容量一族 5 クラス動的化 | Foundations §8.1 | c3f46ea / 0f07dc0 / 659bece / 132d0d5 / a46f208 | ✅**完了** (Material / Instance / Skin / Particle / DebugLine) |
| **G** | BindlessTextureRegistry free-list + slot reuse | Foundations §3 | 17d5f8f | ✅**完了** (descriptor pool growth は別 Phase = G+) |
| **N** | VK_EXT_memory_priority 実利用 | Vulkan13 §rev.4 | 4f7d47f | ✅**完了** (allocator bit + 全 factory に priority 設定) |
| **O** | VK_EXT_debug_utils GPU markers | (新規) | e048503 | ✅**完了** (debug_utils.{h,cpp} + 8 pass DBG_LABEL) |
| **W** | VK_LAYER_KHRONOS_synchronization_validation + swapchain hazard fix | Vulkan13 §rev.4 | df6f5ae + 750135f | ✅**完了** (validation feature ON + post_pass srcStage 修正) |
| **C** | Transfer queue family + queue 取得 | Foundations §2 (a-2) | e7b852e | ✅**完了** (P620 family 1 dedicated 検出 + getter) |
| **I** | VK_EXT_memory_budget enable + allocator bit | Roadmap §6 | 8484ea7 | ✅**完了** (vmaGetHeapBudgets が driver-live 値) |
| **B** (timeline) | Timeline semaphore = Vulkan 1.2 core | INDEX (U) | 3670ef1 + eeba2ed | ✅**完了** (FrameSync migration / hazards 解消) |
| **D+L+K+T+Z+J+Q** (受け皿) | 8 拡張 cap query | (新規 receptacle batch) | 2da80b9 | ✅**受け皿確保** |
| **L** (activate) | VK_EXT_shader_object enable | (D+L+K+T+Z+J+Q activate batch) | f880ddb | ✅**activate** (vkCreateShadersEXT callable・実 callsite 0) |
| **K** (activate) | VK_KHR_present_id + present_wait enable | 同上 | f880ddb | ✅**activate** (callable・実 callsite 0) |
| **Z** (activate) | VK_EXT_image_view_min_lod enable | 同上 | f880ddb | ✅**activate** (callable・実 callsite 0) |
| **J** (activate) | VK_EXT_host_image_copy enable (1.4 promotion) | 同上 | f880ddb | ✅**activate** (callable・実 callsite 0) |
| **Q** (activate) | VK_KHR_calibrated_timestamps enable | 同上 | f880ddb | ✅**activate** (callable・実 callsite 0) |
| **T** (activate 保留) | VK_EXT_swapchain_maintenance1 | 同上 | f880ddb 内で明示保留 | 🟡 **保留** (instance ext VK_EXT_surface_maintenance1 依存) |
| **D** (activate 保留) | VK_EXT_extended_dynamic_state3 | 同上 | f880ddb 内で明示保留 | 🟡 **保留** (30+ feature 個別 query 要) |
| **U** (JobSystem) | Worker thread pool 受け皿 | Foundations §2 ★★★ | fdbddda | ✅**受け皿確保** (header-only inert-friendly) |
| **M** (AsyncCompute) | AsyncCompute timeline semaphore receptacle | (新規) | 8b4deff | ✅**受け皿確保** (header-only) |
| **V/R/S/H/X/Y/P** | 7 design-memo headers | (新規 batch) | 8604de5 | 🟡 **design memo のみ** (各 init/shutdown 空・Phase 着手時に実装) |

---

## 2. PART4 §6 分割案 — 着手順と項目 ID マッピング

| Step | 内容 | 取り込む項目 ID | 状態 |
|---|---|---|---|
| **4-前-0** | Reverse-Z 全面切替 + Infinite far plane | **(O) + (AA)** | ✅ 完了 (702c773) |
| (次) | barrier API を synchronization2 ヘルパに集約 | **(W)** ← Vulkan13 §1 | ✅ 完了 (e1494bf) |
| **4-前-1** | builder block sort + main_pass 区間検出撤去 | (E) | ✅ 完了 (ff9f7a9) |
| **4-前-2** | CullObject meshlet-ready 拡張 + cone test | (G) | ✅ 完了 (b8e39b2) |
| **4-前-3** | Persistent CullObject + grow 経路 + visBuf | (K) + (B) + (D) | ✅ 完了 (ec9c586) |
| **4-前-4** | Scan compaction + IndirectCount + DGC 受け皿 | (I) + (A) + (P) 受け皿 | ✅ 完了 (15b89ad) |
| **4-前-5** | Shadow_pass GPU-driven 化 | (J) | ✅ 完了 (986ba44) |
| **4a-1** | main_pass を Dynamic Rendering 化 (4a-2 前提) | (T) main_pass 部分 | ✅ 完了 (af3dd72) |
| **4a-2** | Depth-normal-motion MRT + OverlayPass + 深度 SAMPLED + GBuffer viewer | (H) + (S) + (T) overlay 部分 | ✅ 完了 (ed0d80e) |
| **4b** | HiZPass = SPD で min+max ペア生成 | (M) + (N) | ✅ 完了 (commit ffe9673) |
| **4c** | Two-pass occlusion 本体 + AABB 遮蔽 + Tier 1 α + 1-tap fast path | (F) + (C) + (V) | ✅ **完了** (8 commits ad97879/477985d/f242327/7e446a9/e41cfd7/91a6885/2f7daf9/ccf5c03) |
| **4d** | 能力ゲート集約 + 受け皿群 + 最新化 + 仕上げ | α (082d792 4b Obs B fix) + γ-1/2/3 (4b9c32c/da74526/33e1511 = (T) full) + M1 (a62b7f0 = (Y)) + M2 (fcef5ab generic layouts) + M3 (47c3571 dynamic_rendering_local_read) + N1 (7298968 pipelineCreationCacheControl) + N4 (c01c2e5 = Vulkan14Features chain + maintenance5/6) + N2/N3 (1481049 = graphics_pipeline_library + pipeline_binary) | ✅ **大半完了** (10 commits・残: (Q) shader_object / (R) descriptor_buffer / (U) timeline semaphore / (X) ext_dynamic_state は別 commit / 別 Phase で) |
| 並列 | Pipeline cache 永続化 | (Y) ← Vulkan13 §3 / M1 | ✅ 完了 (a62b7f0) |
| 後段 | TAA / 完全 dirty tracking / shadow HZB / DGC 実装 etc. | 受け皿利用 | — |
| Phase 3 | VRS の本実装 | (Z) ← Vulkan13 §4 | — |

---

## 3. クロスリファレンス — 同じ話題が複数ファイルに散る箇所

実装中に「あれどこに書いたっけ?」を解消する索引。

| 話題 | 場所 1 | 場所 2 | 場所 3 |
|---|---|---|---|
| **far=200 負債** | Foundations §8.8 | PART4 §0.3-O (Reverse-Z) | Vulkan13 §5 (AA) |
| **固定容量負債 (一族)** | Foundations §8.1 (MAX_TEXTURES/MAX_INSTANCES 等) | PART4 §0.1-B (MAX_DRAWS) | PART4 §6 4-前-3 |
| **transfer / async queue** | Foundations §2 | PART4 §0.4-V | PART4 §3.4-V |
| **bindless 1024 固定** | Foundations §3 | PART4 §0.3-R (descriptor_buffer 連動) | — |
| **depth/normal prepass** | Foundations §5 (元指摘) | PART4 §0.2-H | PART4 §6 4a (+ motion §0.4-S) |
| **block sort** | PART4 §0.1-E | PART4 §3.1-A 末尾 | PART4 §6 4-前-1 |
| **Pascal driver bug (descriptor_buffer)** | PART4 §0.3-R | PART4 §3.3-R | Vulkan13 §0 警告 |
| **能力 query 集約** | PART4 §5 | PART4 §6 4d | Vulkan13 §7 (struct 骨格) |
| **VkRenderPass 古い** | — | PART4 §0.4-T | Vulkan13 §0 (modern triad) |
| **VkBufferMemoryBarrier 古い** | Foundations §8.8 (健全性確認) | — | Vulkan13 §1 (実コード行 culling_pass.cpp:153-164) |
| **永続オブジェクトバッファ** | Foundations §4 (元指摘) | PART4 §0.2-K | PART4 §3.2-K |
| **意図的据え置き (shadow swimming / planar refl)** | Foundations §8.6 ① | — | — |

---

## 4. 「取らない選択」一覧 (妥協ではない明示的選択)

| 項目 | 根拠 | 代替・受け皿 |
|---|---|---|
| Visibility Buffer / Nanite クラスタ DAG / virtualized geometry | PART4 §0.2-L (forward+ 路線・透明/MSAA 親和) | Roadmap 遠い将来 |
| Mesh shader (P620 非対応) | Roadmap 3B / §1.5-C 能力ゲート | meshlet-ready CullObject (G) で構造受け皿 |
| HW Ray Tracing (P620 非対応) | Roadmap / §1.5-C | fallback 経路 |
| GPU Work Graphs | Vulkan 未対応 (D3D12 のみ) | DGC (P) が Vulkan での最尖端 |
| TAA / TSR / FSR / DLSS 本体 | Phase 3 post AA | motion vector RT (S) で受け皿 |
| HLSL シェーダ言語移行 | engine が GLSL で確立 | — |
| visibility query (古典 HW occlusion query) | Hi-Z で代替 | — |

---

## 5. 用語ミニ辞典 (混乱しやすいもの)

- **block** = GeometryBuffer の multi-block (vertex/index 一塊・mesh allocation 単位)。**block-run** = 連続する同 block の draw 区間 (rev.2 までの古い概念・rev.3 以降は block sort で常に block 区間 = block-run)
- **受け皿** = §0「最新の形でアーキテクチャを今用意し、中身を後から埋める」原則。「対象がまだ無いから最新不要」は禁則
- **能力ゲート** = §1.5-C「能力チェック + フォールバック」。最新経路は必ず実装・非対応時は退避先
- **modern triad** = Vulkan 1.3 core の 3 つ: dynamic_rendering (T) / synchronization2 (W) / shader_object (Q)
- **drawId / firstInstance / slot** = 全部同じ整数 (per-draw のインデックス)。GPU-driven 構造の共通アンカー
- **Pascal 安全弁** = descriptor_buffer が Pascal driver で Xid 69 を出すため、vendor/device 判定で強制無効化する仕組み (PART4 §0.3-R / §3.3-R)

---

## 6. セッション運用との接続 (Work_Protocol §6)

### セッション最初の手順
1. **本 INDEX を読む** ← まず全体を見渡す
2. 着手する作業に応じて深掘り (例: 4-前-0 着手なら PART4 §6 4-前-0 + Vulkan13 §5 AA + §1 W)
3. 必要なら正本5枚で原則確認

### セッション終わりの更新
1. 着手・確認した項目について本 INDEX §1 の状態欄を更新 (🟡→🟢 等)
2. 新しい取りこぼし発見時は該当ファイルに追記 + 本 INDEX §1 に行追加
3. 完了項目は正本5枚 (Codebase_Guide / Work_Protocol) にも畳み込む

### セッション開始時に Read する最小構成 (毎回必須)
- 正本5枚 + 本 INDEX = **常に必須**
- 着手中の作業設計書 (PART4 / Foundations / Vulkan13 のうち該当するもの) = 必要時

---

## 7. 現在の状態 (2026-05-29 時点・**PART4 essentially complete + 最新化マラソン 28 commits 完了**)

### 最新化マラソン サマリー (2026-05-29, 995b779..641abcb の 28 commits)
- **A1-A6**: buffer 系 VMA 化 + **エンジン内 生 vkAllocateMemory ゼロ達成** + debug 残骸撤去
- **E + E clean**: camera-relative + 全 10 site wire-up (toEngineRelative helper)
- **F1-F5**: 固定容量 5 クラス動的化 (Material/Instance/Skin/Particle/DebugLine)
- **G**: BindlessTextureRegistry free-list + slot reuse
- **N**: VK_EXT_memory_priority allocator bit + 全 factory に priority 設定
- **O**: VK_EXT_debug_utils GPU markers (debug_utils.{h,cpp} + 8 pass DBG_LABEL)
- **W + W fix**: sync_validation layer ON + 即発見 swapchain hazard 修正
- **C**: transfer queue family + queue 取得 (P620 family 1 dedicated)
- **I**: VK_EXT_memory_budget enable + allocator bit
- **B**: FrameSync per-frame VkFence → 単一 timeline semaphore migration (副次効果 = CullingPass cross-frame hazards 20 件解消)
- **D+L+K+T+Z+J+Q 受け皿 → 5/7 activate**: L/K/K/Z/J/Q を deviceExtsVec push + feature struct chain (callable・実 callsite 0)
- **U**: JobSystem header-only worker thread pool 受け皿 (init(0) inert-friendly)
- **M**: AsyncCompute timeline semaphore receptacle (header-only)
- **V/R/S/H/X/Y/P**: 7 rendering-technique design-memo headers (init/shutdown 空・Phase 着手時に実装)

### 妥協度評価 (audit 後の正直な内訳)
- 🟢 **完全採用 (legacy 0 / modern active)**: sync2 / dynamic rendering / VMA / BDA / bindless / indirect / persistent pipeline cache / Timeline semaphore (B) / camera-relative (E) / debug markers (O) / sync validation (W) / memory priority+budget (N+I) = **17 項目**
- 🟡 **受け皿のみ (Vulkan feature 取得済だが API 関数 0 呼出)**: shader_object (L) / present_id+wait (K) / image_view_min_lod (Z) / host_image_copy (J) / calibrated_timestamps (Q) / dynamic_rendering_local_read (M3) / pipelineCreationCacheControl (N1 flag) / mailbox present mode 未採用 / GPU compute skinning 未着手 = **9 項目**
- 🔴 **未着手**: EDS3 (D) / swapchain_maintenance1 (T) / DGC 実装 / mesh shader / RT / VRS / V-P 7 stub の本実装 = **14 項目**

### P620 [Caps] log 実測 (30 capability)
`multiDrawIndirect drawIndirectFirstInstance synchronization2 drawIndirectCount dynamicRendering separateDepthStencilLayouts subgroupOps subgroupSize samplerFilterMinmax asyncComputeFamily (dedicated) transferFamily (dedicated) dynamicRenderingLocalRead pipelineCreationCacheControl maintenance5 maintenance6 graphicsPipelineLibrary pipelineBinary memoryPriority memoryBudget timelineSemaphore extDynState3 shaderObject presentId presentWait swapchainMaint1 imageViewMinLod hostImageCopy calibratedTimestamps` = **DGC のみ 0** (Pascal hardware 制約)。

### 次の推奨の一手
- **★★★** mailbox present mode + K activation = frame pacing 完成 (現状 FIFO のみ)
- **★★★** pipelineCreationCacheControl 活用 = streaming hitch 検出機構を ON
- **★★** L (shader_object) で VkPipeline 撤廃の本実装 = modern triad 完成
- **★★** S (compute skinning) 本実装 = 大規模キャラ戦闘の前提
- **★★** M activation: AsyncComputeContext を実 cross-queue submit に wire-up
- **★** Z + G+ (descriptor pool grow) = texture mip streaming 完成
- **★** Q calibrated_timestamps で GPU profiling 本実装
- **★** V-P stub の本実装は per-Phase (Phase 2D = R/V / Phase 2F = H/U/M / Phase 3 = X/Y/J 等)
- 任意で 1I PART D / 2A 多光源 / 2C LOD / 2F terrain bucket は依然候補

---

### 元の PART4 完了サマリー (参考・継続)

- **PART4 §6 4-前-0〜4-前-5 + 4a-1 + 4a-2 + 4b + 4c + 4d 大半 + Pure GPU-driven cleanup = 完了** (4-前 ~ 4a-2 の 8 commits + 4b ffe9673 + 4c 8 commits ad97879..ccf5c03 + 4d 10 commits 082d792..1481049 + cleanup f8d1e1f = **PART4 §6 計 28 commits** ・ 別軸で Vulkan13 W e1494bf も完了)。 **P620 [Caps] 18 capability 中 17 が =1** で実走 (DGC のみ 0 で fallback 経路あり)。 user 目視「画面 OK / Cull 行 HUD から消えている」確認。
- **Pure GPU-driven cleanup (commit f8d1e1f)**: user 報告「HUD `Cull : 0 / 67` 永久 0」契機。 4-前-4 (15b89ad) で compactCmd device-local 化以降、 旧 host-mapped readback 経路の `lastVisible_[]` を更新する code path が断たれていた (props は GPU 経由で正常描画されていたため動作上は健全・HUD だけ stale)。 option B 採用 = 純 GPU-driven の本来形 = HUD `Cull` 行 + CPU Frustum オラクル + 全 wire-up 撤去 (8 files +2 -51・getter ×6 + 代入 site ×4 + HUD field ×2)。
- **次の着手候補** (どれから着手するかは user 判断):
  1. **§8 畳み込み**: 本書・Roadmap §4 / 付記・Phase_Dependencies Hi-Z ノード・Codebase_Guide §3.5・Work_Protocol §5f への PART4 完了内容書き戻し (この作業)。
  2. **2C LOD** (P620 を救う / 大規模オープンワールド前提に必要)
  3. **2A 多光源 (clustered forward+)** (Foundations §5 + bindless 連携)
  4. **Phase 2F terrain bucket** (専用 GeometryBuffer + 専用 cull + splat マテリアル経路 + チャンクストリーミング)
  5. **1I PART D** (bloom ノブ settings 連携)
- **4c 内訳**: 4c-A (ad97879 half-extent + hzbPrev 受け皿) → 4c-B (477985d capability getters + helpers) → 4c-C (f242327 full machinery gate-off) → 4c-D (7e446a9 activation) + fix #1 (e41cfd7 UPDATE_AFTER_BIND) + fix #2 (91a6885 toAttach skip) + Tier 1 α (2f7daf9 Nanite/Granite 2024 baseline) + 1-tap fast path (ccf5c03)。
- **4d 内訳**: α (082d792 sync2) + γ-1/2/3 (4b9c32c PostPass / da74526 ShadowPass / 33e1511 ReflectionPass = engine 全体で VkRenderPass / VkFramebuffer 実 API 使用ゼロ) + M3 (47c3571 dynamic_rendering_local_read) + M1 (a62b7f0 persistent pipeline cache = Y closed) + M2 (fcef5ab sync2 generic layouts) + N1 (7298968 pipelineCreationCacheControl) + N4 (c01c2e5 Vulkan14Features chain + maintenance5/6) + N2/N3 (1481049 graphics_pipeline_library + pipeline_binary 受け皿)。
- **4b 残作業の経過**: Obs B = ✅完了 (082d792 α)。 Obs C / D は据え置き (P620 実害ゼロ・mobile/legacy device 対応時に)。
- **PART4 §6 で完了した最尖端 ID 一覧** (§1 表より): (A) (B) **(C)** (D) (E) **(F)** (G) (H) (I) (J) (K) (M) (N) (O) (P 受け皿) (S) **(T full)** **(V)** (W) **(Y)** (AA) = **21 ID 完了**。 残: (Q) (R) (U) (X) (Z) — いずれも別 commit / 別 Phase で着手可、 PART4 完了を阻まない。
