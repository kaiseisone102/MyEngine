# MyEngine Phase 依存関係マップ (テキスト版 rev.11)

最終更新: 2026-05-29 (rev.11: **PART4 §6 4d「Pure GPU-driven cleanup = 完了」追加反映 (commit f8d1e1f)**。 user 報告「HUD `Cull : 0 / 67` 永久 0」を契機: 4-前-4 (15b89ad) で compactCmd device-local 化以降 readback 経路が断たれていた (props は GPU 経由で正常描画・HUD だけ stale)。 option B (純 GPU-driven 化) で HUD 行 + CPU Frustum オラクル + 全 wire-up 撤去 (8 files +2 -51)。 §4 Hi-Z node 残作業から該当項目撤去、 §6 着手順表に commit f8d1e1f 行追加、 §199 PART2 完了 entry に inline note。 / rev.10: **PART4 §6 4c + 4d 大半完了 (18 commits) = PART4 essentially complete**。 §4 の Hi-Z occlusion ノードを「完了」に・次は §8 畳み込み or 後段 Phase へ、 §6 着手順に 4c の 8 commits と 4d の 10 commits を追記、 ★次を「2C / 2A / 2F / 1I-D」に。 / rev.9: **PART4 §6 4b 完了** (HiZPass = SPD-style single-dispatch min+max RG32F pyramid)。 §4 の Hi-Z occlusion ノードに 4b 完了 + 次 = 4c (two-pass occlusion 本体)、 §6 着手順に 4b 行を追記し ★次を 4c に。 / rev.8: PART4 §6 4-前-0〜4-前-5 + 4a-1 + 4a-2 完了反映 = **PART4 Hi-Z 受け皿全部立った**。 §4 の Hi-Z occlusion ノードに 8 段の進捗マーク + 次 = 4b HZB SPD、 §6 着手順に PART4 4-前/4a の 8 段を追記し ★次を 4b に。 / rev.7: 2B PART3c-2 (prop の indirect 差し替え・CPU draw 撤去, 1cf23b9) 完了 = **Phase 2B 完了** を反映。§0 層図・進捗マークを 2B 完了に、§4 の 2B ノードを完了に、§6 着手順の ★次 を「2B 完了・次は 2C/Hi-Z/2F」に、§7 を PART3c-2 完了に更新。`drawIndirectFirstInstance` 必須・block 散在=連続区間 indirect の確定事実を追記 / rev.6: 2B PART3c のスコープを prop のみに明確化 (terrain は対象外)・PART3c-1 完了 (static_cull_build.h, GPU=CPU カリング一致) と PART3c-2 次を反映、**Phase 2F (terrain bucket) を新設**=完成形「terrain は別 bucket」を依存ノード化 (前提: 2B + 遅延破棄 + ストリーミング層)、§0 層図に 2F 追加、PART3c で terrain を prop bucket に統合し撤回した事故記録を追加 / rev.5: 2B PART3b (per-draw SSBO + shader 改修) 完了を反映、着手順を「次は 2B PART3c (indirect 差し替え)」に更新 / rev.4: 2B PART3a 完了、§7 のメッシュ統合ノードを完了に / rev.3: 1K 主要部 / 2B PART0-2 完了) / 対象: グラフィックスロードマップ rev.8 の全 Phase + 土台 side (リソース管理リファクタ)

このドキュメントは「どの Phase がどの Phase の前提か」を整理し、着手順を見誤らないための地図。ロードマップ本体 (MyEngine_Graphics_Roadmap_2026.md) と対で読む。土台 side (VmaImage / 遅延破棄 / ストリーミング) も込みで、土台と描画機能が一枚でどう絡むかを示す。

凡例:
- `A ← 前提: B, C` = A を始める前に B, C があると素直 (B,C は A の足場)
- 「前提」には 2 種類ある: **必須** (これが無いと成り立たない) と **推奨** (無くてもできるが、先にやると後が楽 / 二重作業を避けられる)
- 各項目に rev.3 の 620 動作注記を併記 (快適 / 重い(ノブ) / フォールバックのみ)

---

## 0. 全体の地形 (5 つの層)

依存は大きく左から右へ流れる。左ほど土台、右ほど高度・最新。

```
[土台 side]      [影・ライティング]     [ポスト]          [GPU-driven]              [GI/反射/最新ジオメトリ]
段階1(完了)  ──→  1G PCF/PCSS(完了)──→  1I Bloom(完了)    2B PART0-2(完了)          3-GI (SSGI→SDF→DDGI)
VmaImage(完了)──→ 1K PBR(主要部完了)──→ 1J SSAO/GTAO      2B 完了(PART3a/3b/3c)→2C/Hi-Z  3-Refl (SSR→RT反射)
遅延破棄      ──→  2A clustered         2D TAA            2C LOD / Hi-Z occlusion   3B mesh shader
ストリーミング ───────────────────────────────────────→ 2F terrain bucket          3C HW レイトレ
```

進捗マーク: 段階1 / VmaImage 化 / 1G / 1I / 1K(主要部) / 2B PART0-2 / 2B PART3a / 2B PART3b / 2B PART3c-1 / 2B PART3c-2 (1cf23b9) = Phase 2B 完了 / PART4 §6 4-前-0〜4-前-5 + 4a-1 + 4a-2 (受け皿) / **PART4 §6 4b (HiZPass = SPD-style single-dispatch min+max RG32F pyramid, commit ffe9673) = HZB 生成到達**。★次 = **PART4 4c (two-pass occlusion 本体)** = 4b の HZB を AABB 画面投影 + mip 選択で occluder と比較し遮蔽カリング。 並行可候補: 2C LOD / 3B mesh shader (RTX 後) / Phase 2F terrain bucket / 任意で 4b 中身高速化 (wave-ops 派生)。詳細は Codebase_Guide §3.5 / §3.6 / START_HERE §2 / side/MyEngine_HiZ_PART4_Design.md §6 「4c」。

層をまたぐ主な依存:
- 土台 side は全描画 Phase の足場 (特にポスト系の render target 増設)
- 影・ライティングの質 (1K PBR) は GI の前提
- ポスト (1J SSAO) は GI 段1 (SSGI) の足場
- GPU-driven (2B) は LOD・mesh shader の前提
- GI/反射は段階導入で、下の段が上の段の足場 + フォールバック先

---

## 1. 土台 side (リソース管理リファクタ) — すべての足場

### 段階1: 二層 RAII 化 【完了済み】
- 前提: なし (完了)
- これが足場になるもの: **新しい render target / pipeline / buffer を足す全 Phase**。VkUnique/VmaBuffer の型に乗せれば配管コードを書かずに済む
- 状態: 2026-05-25 完了。Shadow/Post/Bloom/Reflection/Main + 全バッファ系クラスが二層化済み

### VmaImage 化 (§6) 【完了済み 2026-05-25】
- ← 前提: 段階1 (必須。VmaBuffer と同じ設計の image 版を足す)
- これが足場になるもの: **ポスト系 Phase 全部 (1I/1J/2D/2E)**。これらは中間 render target (image) を増やすので、image が VMA 管理だと VRAM 使用量が見通せる
- 状態: **完了** (commit b9ac20b → d5e7eaf → ad8fa08 → 70f30b6 → 1349a04)。`renderer/vma_image.h/.cpp` 新設 → RenderTarget / Texture / swapchain depth を移行 (ShadowPass output / ReflectionTarget も RenderTarget 経由で自動カバー) → 未使用化した `ResourceFactory::createImage`/`createImageVMA` を削除。**エンジン内に image 用の生 vkAllocateMemory は無い。** 1I (compute bloom) の mip 列もこの VmaImage で確保した。
- 残り: buffer 側の VMA 化 (mesh/model_loader/terrain_mesh/texture staging の `createBuffer`) は別タスクで未着手。
- 位置づけ: **確定ルール通り 1I の前に完了済み** (§6/§7)。これによりポスト系の VRAM 予算管理が楽になった。

### 遅延破棄 (deferred deletion) キュー (§6)
- ← 前提: 段階1 (必須)
- これが足場になるもの: **チャンクストリーミング (必須前提)**、swapchain 再作成の安全性、動的なリソース入れ替えがある全機能
- 620: 該当なし (土台作業)
- 位置づけ: WorldTerrain/WorldWater の手動 clear 撤去もここで解決

### チャンクストリーミング
- ← 前提: 遅延破棄 (必須 — in-flight リソースの破棄タイミング管理)、VmaImage/VmaBuffer (必須 — ロード/アンロードで頻繁に確保解放するので VMA 必須)
- これが足場になるもの: 広いオープンワールド (描画範囲外をアンロードして VRAM を空ける)。620 の 2GB では特に重要
- 620: 設計次第 (ストリーミング自体は CPU/IO 主体。むしろ 620 の VRAM を救う技術)
- 位置づけ: オープンワールドのスケールを上げる本命。LOD (2C) と組むと効果大

---

## 2. 影・ライティングの質

### Phase 1G — PCF / PCSS ソフトシャドウ 【完了済み 2026-05-25】
- ← 前提: なし (既存 ShadowPass の上に乗る。ShadowPass は段階1 で RAII 化済み)
- これが足場になるもの: 2E CSM (影の質の次の課題。1G のフィルタ `shared/shadow_sampling.glsl` を各カスケードに適用する形)
- 620: 快適
- 状態: **完了** (commit 3436ae5)。`shared/shadow_sampling.glsl` に Vogel PCF + PCSS を集約、4 lighting frag から呼ぶ。品質ノブ shadowParams.y (0:hard / 1:Soft / 2:High)。

### Phase 1K — PBR マテリアル 【主要部 完了 2026-05-25】
- ← 前提: なし (既存 bindless テクスチャ基盤の上に乗る)
- これが足場になるもの: **3-GI (必須に近い)** — GI は物理ベースのライティングがあって初めて意味を持つ。間接光を足すなら BRDF が物理ベースである必要がある
- 620: 快適
- 状態: **主要部完了** — 1K-A BRDF を `shared/pbr.glsl` に集約 (964c733)、1K-5 法線マップ surface gradient (593ef17 等)、1K-4 metallic-roughness (ddc5435)。残: 1K-6 AO (モデル未保持で skip 可) / emissive。types.h アライメント規約は実ソースで明文化済みと確認済み (Codebase_Guide §2 の旧「TODO」は解消)。
- 位置づけ: 見た目の説得力 + GI への布石は確保済み。GI を狙う縦ライン (§6) の起点。

### Phase 2A — clustered / tiled forward lighting
- ← 前提: 1K PBR (推奨 — 多数ライトを足すなら物理ベースのライティングと組む方が自然)、BDA+SSBO (既存・必須)
- これが足場になるもの: 多光源シーン (夜・松明・魔法)
- 620: 重い (ノブ: 実効ライト数・クラスタ解像度)
- 位置づけ: ライティングを「質」から「量」へ広げる。GI とは別系統

### Phase 2E — カスケードシャドウマップ (CSM)
- ← 前提: 1G (推奨 — PCF/PCSS のフィルタを各カスケードに適用する形)、VmaImage (推奨 — カスケード本数ぶん shadow map が増えるので VRAM 管理)
- これが足場になるもの: 広いシーンの影品質 (オープンワールド)
- 620: 重い (ノブ: カスケード数・解像度)
- 位置づけ: オープンワールド志向なら 1G の次の影の課題

---

## 3. ポスト (スクリーンスペース)

### Phase 1I — Bloom 【完了済み 2026-05-25 / compute mip-chain】
- ← 前提: HDR 基盤 (既存・必須)、VmaImage (mip 列の確保に使用・完了済み)
- これが足場になるもの: ほぼ独立 (他 Phase の前提にはならない)。ただし**エンジン初の compute pass として compute の作法を確立**し、2B (compute カリング) がそれを踏襲する (Codebase_Guide §3)
- 620: ノブ (mip 段数) で調整
- 状態: **完了** (commit d03f3ff、後始末 194e73c)。旧 fragment 版 (2枚 ping-pong + 単一しきい値 + 9-tap Gaussian) を、BloomPass が mip 列 (storage+sampled VmaImage を既定6段) を内包する **compute 実装** に全面置換 (Jimenez/CoD 方式: bright→13-tap downsample[mip0→1 Karis]→3x3 tent upsample 加算、全て vkCmdDispatch、render pass も framebuffer も無い)。VulkanRenderer は bloom target を持たない。最終 bloom = mip0 を PostPass が合成 (post 無改修)。未使用 fragment bloom シェーダ4本も削除済み。
- 位置づけ: 完了。残タスクは PART D (品質ノブを settings 連携・目視チューニング) が任意で残る。

### Phase 1J — SSAO / GTAO
- ← 前提: 深度・法線バッファ (要確認: 法線を G-buffer 的に出しているか)、VmaImage (推奨。依存グラフ上は推奨だが、運用順は §6/§7 で確定: 1J より前)
- これが足場になるもの: **3-GI 段1 SSGI (足場)** — SSGI は SSAO の深度・法線サンプルの延長
- 620: 重い (ノブ: サンプル数・半径・半解像度処理)
- 位置づけ: 立体感・接地感。SSGI への布石でもある

### Phase 2D — TAA
- ← 前提: モーションベクトル出力パス (新規必須)、VmaImage (推奨 — 履歴バッファ)、HDR+ポスト基盤 (既存)
- これが足場になるもの: **SSGI/SSR/SSAO のノイズを時間方向に均す土台** (3-GI 段1/2, 3-Refl 段1 と相性)
- 620: 重い (ノブ: 履歴解像度。VRAM 予算が肝)
- 位置づけ: AA そのものに加え、スクリーンスペース系を本格化するなら欲しくなる縁の下

---

## 4. GPU-driven / スケール

### Phase 2B — compute カリング + indirect draw 【完了 (PART0-2 + PART3a + PART3b + PART3c-1 + PART3c-2)】
- ← 前提: 統一 InstanceData (既存・必須)、BDA (既存・必須)、compute shader (Pascal 標準・利用可)
- これが足場になるもの: **2C LOD (必須前提)**、**3B mesh shader (発展元・必須前提)**、**2F terrain bucket (同じ仕組みを再利用)**、Hi-Z occlusion culling (発展)
- 620: 快適 (CPU 軽減) 〜 規模次第で重い
- **スコープ注記: 2B が GPU-driven 化するのは prop (cube + Model) のみ。terrain は対象外 = Phase 2F (別 bucket)。** prop と terrain を同じ GeometryBuffer に混ぜない (断片化)。
- 状態: **完了 (PART0-2 / PART3a ac7bbd1 / PART3b c5adced / PART3c-1 632433a / PART3c-2 1cf23b9)。** CullingPass + cull.comp が GPU カリングで instanceCount を生成し、**PART3c-2 で main の prepared CPU draw ループを `vkCmdDrawIndexedIndirect` に差し替え・CPU draw 撤去 = CullingPass が prop の実描画に初接続** (prop bucket の GPU-driven 骨格完成)。能力チェックで `multiDrawIndirect` + **`drawIndirectFirstInstance`** を実測有効化 (P620 両対応)。block 散在 (≈17-18 連続区間) のため「同一 block の連続区間ごとに 1 draw 呼び出し」(区間 MDI / 非対応はループ。全 1 MDI は不可)。シェーダ無改修。検証は HUD `Cull : 可視/総数` (視点回転で分子が動く)。**発展形 Hi-Z occlusion (PART4) はこの骨格に drop-in** (cmdBuf 駆動・COMPUTE→DRAW_INDIRECT バリア維持済み)。詳細は Codebase_Guide §3.5 / Work_Protocol §5e。
- 位置づけ: **「最新技術」の本丸**。GPU-driven の入口。ここから 2C/3B/2F が枝分かれする中核ノード。**1I で確立した compute 作法 (Codebase_Guide §3) を PART2 で踏襲済み**。PART3 のメッシュ統合は土台 side (buffer 系 VMA 化) と作業領域が重なる点に注意 (§7)。
- **事故記録 (2026-05-27)**: PART3c で資料を読まずスコープを「全 static 統合 (terrain 込み)」と誤判断し terrain を prop bucket に統合 (3c-0) → 描画破壊 → 撤回。スコープ判断は資料確認してから (Work_Protocol §1-4 / START_HERE §0-8)。

### Phase 2F — terrain bucket (GPU-driven 地形 + splat + 距離 LOD + チャンクストリーミング) 【新規・未着手】
- ← 前提: **2B (prop GPU-driven 骨格・PART3 完了。必須=同じ GeometryBuffer/cull/indirect の仕組みを再利用)**、**遅延破棄 (必須=チャンクのロード/アンロードで in-flight リソースを安全に破棄)**、**buffer 系 VMA 化 (推奨)**、ストリーミング層 (土台 side)
- これが足場になるもの: 大規模オープンワールドの地形そのもの (完成形の中核の一つ)
- 620: 快適〜重い (ノブ)。距離 LOD + チャンクで描画三角形を抑える。splat の texture fetch 段数をノブに
- 内容: prop とは別の専用 GeometryBuffer に地形チャンクを置き、視錐台 + 距離 LOD で compute カリング、indirect draw。地形シェーダ内で土/岩/砂を splat (スプラットマップ + 傾斜) でブレンド (受け皿を先に最新の形で用意、マテリアルは後で足す)。chunked LOD / clipmap で 1km×1km 以上。`terrain_mesh.h/.cpp` の geom 対応コードは PART3c で残置済み = 再利用。
- 位置づけ: 完成形「prop と terrain は別 bucket・両方 GPU-driven」(START_HERE §2) の terrain 側の実体。地形描画の段階分解 (Step1 フラット → Step2 起伏 → Step3 splat → Step4 LOD+大規模 → Step5 植生) に準拠。

### Phase 2C — LOD システム
- ← 前提: **2B (必須 — compute から詳細度を選択)**
- これが足場になるもの: 広いマップの性能、チャンクストリーミングと組むと効果大
- 620: 快適 (むしろ 620 を救う)
- 位置づけ: 2B の自然な発展。非力 GPU ほど恩恵大

### Hi-Z occlusion culling (2B 発展・PART4) ✅ **essentially complete (4-前 + 4a + 4b + 4c + 4d 大半 + Pure GPU-driven cleanup = 28 commits)**
- ← 前提: 2B (必須・**完了**)、深度ピラミッド生成 (4b で新設)
- 620: 軽い (P620 [Caps] 18 中 17 = 1 で実走)
- 位置づけ: 2B の発展形。遮蔽されたオブジェクトも除外し、さらにドローを減らす。 **Nanite/Granite 2024 baseline (Tier 1 α) 適合** = pass1 が前フレ HZB 経由で前フレ可視オブジェクトを早期 cull → MainPass(FirstOpaque) で描画 → 4b HZB 再生成 → pass2 で全 object を新 HZB で再評価 → MainPass(SecondAndNonOpaque) で残り描画。 visHistory で次フレ pass1 用 prev_vis 永続化。
- **進捗 (2026-05-29)**: PART4 §6 全段完了:
  - **4-前-0..5 + 4a-1/2 + 4b = 完了** (上記履歴・8 + 1 commits)
  - **4c (two-pass HZB occlusion + Tier 1 α + 1-tap fast path) = 完了** (8 commits ad97879 / 477985d / f242327 / 7e446a9 / e41cfd7 / 91a6885 / 2f7daf9 / ccf5c03)。 4c-A (half-extent + hzbPrev 受け皿) → 4c-B (capability + helpers) → 4c-C (full machinery gate-off) → 4c-D (activation) + fix #1 (UPDATE_AFTER_BIND) + fix #2 (toAttach skip for SecondAndNonOpaque) + Tier 1 α 活性化 + 1-tap fast path。 user 目視「画面正常」確認。
  - **4d 大半 (audit-driven 最新化) = 完了** (10 commits 082d792..1481049)。 α (4b Obs B sync2 fix) + γ-1/2/3 (Post/Shadow/Reflection 全 dynamic rendering 化 = engine 全体で VkRenderPass / VkFramebuffer 実 API 使用ゼロ) + M3 (dynamic_rendering_local_read 受け皿) + M1 (persistent VkPipelineCache = Vulkan13 §3 Y closed) + M2 (sync2 generic layouts へ全面置換) + N1 (pipelineCreationCacheControl) + N4 (Vulkan14Features chain + maintenance5/6 enable・engine が API 1.4 で動作中なのに 1.4 features を一切 enable してない構造欠陥を修正) + N2/N3 (graphics_pipeline_library / pipeline_binary 受け皿)。
- **Pure GPU-driven cleanup (commit f8d1e1f, 2026-05-29) = 完了**: HUD `Cull` 行 + CPU Frustum オラクル + 全 wire-up 撤去 (`lastVisible_[]` / `lastCpuVisible_` / `lastCullGpuVisible_` / `lastCullTotal_` / `cullGpuVisible` / `cullTotal` / 各 getter + 代入 site)。 4-前-4 (15b89ad) で compactCmd device-local 化以降 readback 経路が断たれていたのを option B (純 GPU-driven の本来形) で清算。 同一フレーム精密照合が要る場合は別 commit で countBuf を small staging に CopyBuffer する形 (option A) で復活可能。
- **4d で残った仕事 (PART4 完了を阻まない・別 commit / 別 Phase で着手可)**: DGC 経路 **実装** (`VkIndirectCommandsLayoutEXT` ラッパ・Pascal 非対応で実 device 必要) / Shader Object 経路 / Descriptor Buffer (Pascal 強制無効化ロジック) / Timeline semaphore / Async compute での HZB / cull 並列実行 (Foundations §2 と一緒) / debug log 掃除 / transparent MRT mismatch fix / 4b Obs C/D。
- **次推奨 = PART4 §8 畳み込み** (本書 + Roadmap + Codebase_Guide + Work_Protocol への完了内容書き戻し・PART4 設計書を「履歴」として閉じる) → その後 **2C LOD / 2A 多光源 / Phase 2F terrain bucket / 1I PART D / 3B mesh shader** のいずれか。

---

## 5. GI / 反射 / 最新ジオメトリ (段階導入)

### Phase 3-GI — リアルタイム GI
段階導入。下の段が上の段の足場 + フォールバック先。
- 段1 SSGI ← 前提: 1J SSAO (足場・推奨)、1K PBR (推奨)。620: 重い (ノブ)
- 段2 SDF GI ← 前提: 段1 (推奨)、SDF 生成パス (新規必須)。620: 重い (ノブ: probe 解像度・更新間引き)
- 段3 フル DDGI ← 前提: 3C HW レイトレ (必須)。620: フォールバックのみ
- これが足場になるもの: シーン全体の間接光のリアリティ
- 位置づけ: 能力チェックで段を切替。620 は段1/2、RTX 更新で段3

### Phase 3-Refl — 反射の現代化
段階導入。
- 段1 SSR ← 前提: 深度・color (既存)、(TAA があるとノイズが減って相性良い・推奨)。620: 重い (ノブ)
- 段2 RT 反射 ← 前提: 3C HW レイトレ (必須)。620: フォールバックのみ
- フォールバック先: 既存の planar reflection (水面)
- 位置づけ: 能力チェックで RT あれば段2、なければ段1 or planar

### Phase 3B — mesh shader / task shader
- ← 前提: **2B (必須 — compute culling の発展形として実装)**、mesh shader HW (Turing+・620 非対応)
- 620: フォールバックのみ (常に 2B 経路。RTX 更新で meshlet 経路が開く)
- 位置づけ: ジオメトリパイプラインの最新形。2B があって初めて発展として書ける

### Phase 3C — HW レイトレーシング
- ← 前提: HW レイトレ (Turing+・620 非対応)、(反射・GI・影のどれを RT 化するかで個別)
- これが足場になるもの: 3-GI 段3、3-Refl 段2 の実体
- 620: フォールバックのみ
- 位置づけ: RT 対応時の最上位経路。GI/反射の段階導入の最終段を担う

---

## 6. 推奨着手順序 (依存を満たす最短ルート)

ロードマップの推奨の一手を依存マップ上で検証し、§7 の「二重作業を防ぐ順序」と整合させた確定ルート。**1G / VmaImage 化 / 1I / 1K(主要部) / 2B PART0-2 / 2B PART3a (メッシュ統合) / 2B PART3b (per-draw SSBO + shader) までは完了済み。現在の起点は 2B PART3c (A 方針)**:

```
[完了] 1G       PCF/PCSS       commit 3436ae5                影が柔らかく
   ↓
[完了] VmaImage 化             commit b9ac20b..1349a04        image を VMA 管理に (1I の前に完了 = 確定ルール通り)
   ↓
[完了] 1I       Bloom(compute) commit d03f3ff                光が滲む。エンジン初の compute pass
   ↓
[完了] 1K       PBR(主要部)     964c733/593ef17/ddc5435       質感 + GI布石 (BRDF集約/法線/MR)
   ↓
[完了] 2B PART1 CullObject受け皿 commit df9d843               カリング対象の CPU 受け皿
   ↓
[完了] 2B PART2 compute cull    commit 5cbc7e6                エンジン2つ目の compute pass。GPU=CPU 一致検証済 (※検証用 CPU オラクル + 全 wire-up は commit f8d1e1f で純 GPU-driven 化撤去)
   ↓
完了   2B PART3a メッシュ統合   [無制限 multi-block GeometryBuffer]  ac7bbd1
完了   2B PART3b per-draw SSBO  [DrawData SSBO + gl_InstanceIndex + shader改修]  c5adced
完了   2B PART3c-1 ビルダ        [static_cull_build.h・SubMesh 粒度 drawId 連番]  632433a
完了   2B PART3c-2 indirect化   [prepared CPU ループ → vkCmdDrawIndexedIndirect・CPU draw撤去]  1cf23b9
       → Phase 2B 完了 = prop bucket の GPU-driven 骨格が立ち上がった (スコープは prop のみ・terrain は 2F)
   ↓
完了   Vulkan13 W                [sync2 barrier helper・renderer/barrier.h]  e1494bf
完了   PART4 4-前-0              [Reverse-Z + 無限遠 perspective 全面]  702c773
完了   PART4 4-前-1              [block sort + BlockRange]  ff9f7a9
完了   PART4 4-前-2              [meshlet-ready CullObject + cone test 受け皿]  b8e39b2
完了   PART4 4-前-3              [persistent device-local CullObject + bit-packed visBuf + grow]  ec9c586
完了   PART4 4-前-4              [3-pass scan compaction + IndirectCount + DGC 受け皿]  15b89ad
完了   PART4 4-前-5              [GPU-driven shadow / per-CullSet output buffer]  986ba44
完了   PART4 4a-1                [main_pass を Vulkan 1.3 dynamic rendering 化]  af3dd72
完了   PART4 4a-2                [depth-normal-motion MRT + OverlayPass 分離]  ed0d80e
完了   PART4 4b HZB SPD          [hiz_spd.comp 新設・SPD-style single-dispatch min+max RG32F]  (commit ffe9673)
完了   PART4 4c-A                [half-extent (CullObject.extentDrawId.xyz) 充填 + HiZPass::previousPyramidView() 受け皿]  ad97879
完了   PART4 4c-B                [capability getters (samplerFilterMinmax, async compute family) + HizParams BDA + cull.comp helpers]  477985d
完了   PART4 4c-C                [full machinery (HiZPass minReductionSampler + visHistory + HZB descriptor set + cull.comp pass1/2 + main_pass Pass enum) gate-off]  f242327
完了   PART4 4c-D                [two-pass HZB occlusion 活性化]  7e446a9
完了   PART4 4c-D fix #1         [HZB descriptor set UPDATE_AFTER_BIND 追加]  e41cfd7
完了   PART4 4c-D fix #2         [main_pass SecondAndNonOpaque で toAttach barrier skip = user 目視で発見した青背景黒化 + prop 消失 bug]  91a6885
完了   PART4 Tier 1 α            [pass1 が hzbPrev も sample = Nanite/Granite 2024 baseline 適合]  2f7daf9
完了   PART4 1-tap fast path     [samplerFilterMinmax 経由で 1 textureLod 化・spec const 分岐]  ccf5c03
完了   PART4 4d α                [4b Obs B 解決 = HiZPass initialTransitionToGeneral を sync2 STAGE_NONE]  082d792
完了   PART4 4d γ-1 PostPass      [Vulkan 1.3 dynamic rendering 化]  4b9c32c
完了   PART4 4d γ-2 ShadowPass    [Vulkan 1.3 dynamic rendering 化 + ShadowPipelineParams に VkPipelineCache 追加 = M1 同時配線]  da74526
完了   PART4 4d γ-3 ReflectionPass [Vulkan 1.3 dynamic rendering 化 + outNormal/outMotion validation warning 4 件消滅]  33e1511
完了   PART4 4d M3                [VK_KHR_dynamic_rendering_local_read (1.4 core) capability 受け皿]  47c3571
完了   PART4 4d M1 (= Vulkan13 §3 Y) [persistent VkPipelineCache・SDL_GetPrefPath 経由・14 callsite 全経由・490KB 書き出し実証]  a62b7f0
完了   PART4 4d M2                [depth_layouts.h ヘルパ撤去 → sync2 generic layouts 一括置換 19 callsites + ヘッダ削除・-45 行]  fcef5ab
完了   PART4 4d N1                [pipelineCreationCacheControl (1.3 core) enable + FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT 受け皿]  7298968
完了   PART4 4d N4                [VkPhysicalDeviceVulkan14Features chain 追加 + maintenance5/6 enable]  c01c2e5
完了   PART4 4d N2+N3             [VK_EXT_graphics_pipeline_library + VK_KHR_pipeline_binary capability 受け皿]  1481049
完了   PART4 Pure GPU-driven cleanup [HUD `Cull` 行 + CPU Frustum オラクル + 全 wire-up 撤去 = compactCmd device-local 化以降の readback 経路断絶を option B で清算・8 files +2 -51]  f8d1e1f
       → PART4 essentially complete (P620 [Caps] 18 中 17 = 1 で実走)
   ↓        ここから枝分かれ:
   ├─→ PART4 §8 畳み込み       [本書 + Roadmap + Codebase_Guide + Work_Protocol への完了内容書き戻し]  ←★ 次の本命
   ├─→ 2C LOD                  (2B 必須・P620 を救う)
   ├─→ 2A 多光源 (clustered)    (1K PBR の上に乗る・別系統)
   ├─→ 2F terrain bucket       [専用 GeometryBuffer + 専用 cull + splat + 距離 LOD + チャンクストリーミング]
   ├─→ 1I PART D               (bloom 品質ノブ settings 連携・仕上げ)
   ├─→ 4d 残作業 (別 commit)    [DGC 実装 / shader_object / descriptor_buffer / timeline semaphore / async compute 並列化 / debug log 掃除 / readback / transparent MRT mismatch / 4b Obs C/D]
   └─→ 3B mesh shader          (2B 必須・RTX更新後)
```

**2B PART3 の中身 (A 方針 = オープンワールド GPU-driven の本筋) = 全完了。スコープは prop (cube + Model) のみ・terrain は別 bucket = Phase 2F**: static prop は (1) ~~SubMesh ごとに別 vertex/index buffer~~ → **PART3a で無制限 multi-block GeometryBuffer に統合 (ac7bbd1)** (2) ~~描画単位 = draw item×SubMesh~~ → **PART3c-1 で SubMesh 粒度の drawId 連番ビルダに (632433a)** (3) ~~per-SubMesh push constant 更新~~ → **PART3b で DrawData SSBO + gl_InstanceIndex 化 (c5adced)** (4) ~~drawId を SubMesh 粒度に再定義 → indirect 差し替え~~ → **PART3c-2 で `vkCmdDrawIndexedIndirect` 差し替え + CPU draw 撤去 (1cf23b9)**。多段 (~~PART3a~~ → ~~3b~~ → ~~3c-1~~ → ~~3c-2~~) で刻んで **2B 完了**。`drawIndirectFirstInstance` 必須 (firstInstance に DrawData slot を載せるため・P620 両対応実測)、block 散在のため連続区間ごと indirect。詳細は Codebase_Guide §3.5 / Work_Protocol §5e。

任意 / 並行可:
- **1I PART D** (bloom 品質ノブを settings 連携・目視チューニング) — 1I 本体は完了済み、これは仕上げ。いつでも可。
- **buffer 系 VMA 化** (mesh/model_loader/terrain_mesh/texture staging の createBuffer → VmaBuffer) — image 側は VMA 一本化済み、buffer 側が残土台。**2B PART3a のメッシュ統合と mesh/model_loader を共に触るので、一緒に片付けるか PART3a 設計時に判断 (§7)**。
- 1K-6 AO (モデル未保持で skip 可) / 2A 多光源 (GPU-driven とは別系統)。

他の土台 side:
- 遅延破棄 → チャンクストリーミングの前提。オープンワールド本格化の前に

GI を狙う場合の縦ライン:
```
1K PBR → 1J SSAO → 3-GI 段1 SSGI → (TAA でノイズ低減) → 3-GI 段2 SDF → [RTX更新] → 段3 DDGI
```

反射を狙う場合の縦ライン:
```
(既存 planar) → 3-Refl 段1 SSR → (TAA 推奨) → [RTX更新] → 3-Refl 段2 RT反射
```

---

## 7. 「先にやると二重作業を防げる」もの (要注意ノード)

着手順で特に効く前後関係:
- **VmaImage 化はポスト系の前に**: 後からやると、先に作った render target を VMA に移し替える二重作業になる
- **2B は 2C/3B の前に必須**: LOD も mesh shader も 2B の compute 基盤の上に乗る。2B 無しで個別に書くと後で作り直し
- **1K PBR は GI の前に**: GI を先に入れてから PBR にすると、ライティングの土台が変わって GI 側を調整し直すことになる
- **遅延破棄はストリーミングの前に必須**: ストリーミングは毎フレーム的にリソースを入れ替えるので、破棄タイミング管理 (遅延破棄) が無いと in-flight リソースを壊す
- **TAA はスクリーンスペース GI/反射/AO のノイズ対策として、それらと前後して**: 厳密な前提ではないが、TAA があるとノイズが許容範囲に収まり品質ノブを上げやすい
- **【2026-05-27 更新】2B PART3a (ac7bbd1) + PART3b (c5adced) + PART3c-1 (static_cull_build.h, 632433a) + PART3c-2 (indirect 差し替え・CPU draw 撤去, 1cf23b9) = 全完了 = Phase 2B 完了**: indirect 描画の前提 (single buffer 統合 + per-draw SSBO + gl_InstanceIndex + SubMesh 粒度ビルダ) (a)(b)(c)(d) を達成し、PART3c-2 で実際に `vkCmdDrawIndexedIndirect` に差し替え CPU draw を撤去 = CullingPass が prop の実描画に接続。**`drawIndirectFirstInstance` が必須能力** (firstInstance に DrawData slot を載せる・P620 両対応実測)、block 散在のため連続区間ごと indirect。**スコープは prop のみ。terrain は別 bucket = Phase 2F (未着手)。** 一度 terrain を prop bucket に統合し撤回した経緯あり (Work_Protocol §1-4 / §5e)。
- **【2026-05-26 更新】buffer 系 VMA 化と PART3a の重なりについて**: PART3a で mesh/model_loader/SubMesh の geometry は GeometryBuffer (VmaBuffer ベース) に移ったので、それらの頂点/index buffer は実質 VMA 化済み。残る createBuffer→VmaBuffer 対象は terrain_mesh / texture staging など。PART3b/3c は shader と draw command が主で mesh buffer は触らないので、buffer 系 VMA 化は独立タスクとして好きな時に。
