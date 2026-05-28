# MyEngine 設計知見 — 横断インデックス (rev.3)

最終更新: 2026-05-28 (rev.3: PART4 §6 4-前-0/1/2/3/4/5 + 4a-1 + 4a-2 完了反映。 §1 表で対象項目 ID (A/B/D/E/G/H/I/J/K/O/S/T/W/AA) の状態を 🟢実装済 / ✅完了 に更新、 §2 表に完了済みマーク (commit 番号付き) + 「次 = 4b HZB SPD」を明示、 §7 現在の状態を「PART4 §6 4-前/4a 全部完了・次は 4b」に更新、 Codebase_Guide rev を 11 → 12 に更新。 / rev.2: 運用モード切替で §6「ファイル添付の最小構成 (毎回必須)」を「セッション開始時に Read する最小構成」に書換。Work_Protocol rev.13 / START_HERE / Codebase_Guide rev.11 と整合 / rev.1: 設計知見が 4 系統 8 ファイルに分散したため横断インデックスを新設。全項目 ID (A)〜(AA) の一覧 + 担当ファイル§ + 着手時期 + 状態を 1 表に集約。同じ話題が複数ファイルに散る箇所のクロスリファレンス) / 対象: MyEngine の設計を**最短で見渡す**ためのインデックス。各ファイルへの参照は§単位

> **このファイルの位置づけ**: 各セッション最初に最優先で Read する横断索引。「全部読まないと判断できない」状態を解消し、必要箇所だけ深掘りできるようにする。本文は他ファイルにある (このファイルは目次)。
> **読む順序の推奨**: ① **本 INDEX** → ② 正本5枚で原則確認 → ③ 着手する作業に応じて PART4 / Foundations / Vulkan13 のうち該当§のみ深掘り。

---

## 0. ファイル構成 (4 系統)

| 系統 | ファイル | rev | 行数 | 役割 |
|---|---|---|---|---|
| **正本5枚** | MyEngine_START_HERE.md | — | — | 入口・ゴール・現在地・運用 |
| | MyEngine_Graphics_Roadmap_2026.md | rev.8 | — | 全 Phase 計画 |
| | MyEngine_Phase_Dependencies.md | rev.7 | — | Phase 間依存マップ |
| | MyEngine_Codebase_Guide.md | rev.12 | — | コード構造の地図 (4a-1/4a-2 反映) |
| | MyEngine_Work_Protocol.md | rev.17 | — | 作業規範・原則 (§0/§1.5/§5b/§5c 等、 §3-1a 事例②追記) |
| **作業正本** | MyEngine_HiZ_PART4_Design.md | **rev.7** | **— ** | Phase 2B PART4 Hi-Z 設計 (4-前/4a 完了・次は 4b) |
| **土台監査** | MyEngine_Foundations_Audit.md | **rev.4** | **— ** | 先回り受け皿 + 実ソース確認済み既存負債 (SS prepass 受け皿完了反映) |
| **隣接機能** | MyEngine_Vulkan13_Modernization.md | **rev.2** | — | Vulkan 1.3 modernization (W-AA, W/AA 完了・T も適用) |
| **索引** | MyEngine_INDEX.md (本書) | rev.3 | — | 横断インデックス |

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
| (C) | AABB 遮蔽 (half-extent 充填) | PART4 §3.1-C | 4c | 🟢実装 (未着手) |
| (D) | 可視履歴 (visBuf) | PART4 §3.1-D | 4-前-3 | ✅完了 (ec9c586, 受け皿) |
| (E) | CPU indirect 呼び出し収束 (block sort) | PART4 §3.1-E | 4-前-1 | ✅完了 (ff9f7a9) |

### 最尖端取り込み第一弾 (§0.2・rev.4)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (F) | Two-pass HZB occlusion | PART4 §3.2-F | 4c | 🟢実装 (未着手) |
| (G) | Meshlet-ready CullObject | PART4 §3.2-G | 4-前-2 | ✅完了 (b8e39b2, 受け皿) |
| (H) | Depth-normal prepass (軽量 GBuffer) | PART4 §3.2-H | 4a | ✅完了 (ed0d80e) |
| (I) | Blelloch scan compaction | PART4 §3.2-I | 4-前-4 | ✅完了 (15b89ad, 3-pass scan) |
| (J) | Shadow_pass GPU-driven 化 | PART4 §3.2-J | 4-前-5 | ✅完了 (986ba44) |
| (K) | Persistent CullObject buffer | PART4 §3.2-K | 4-前-3 | ✅完了 (ec9c586, 受け皿) |
| (L) | forward+ 路線維持 | PART4 §0.2-L | — | ⚫選択 (VisBuffer/Nanite 取らず) |

### 最尖端取り込み第二弾 (§0.3・rev.5)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (M) | SPD (Single Pass Downsampler) HZB 生成 | PART4 §3.3-M | 4b | 🟢実装 (未着手・★次) |
| (N) | min+max ペア HZB (RG32F) | PART4 §3.3-N | 4b | 🟢実装 (未着手) |
| (O) | Reverse-Z depth | PART4 §3.3-O | 4-前-0 | ✅完了 (702c773) |
| (P) | VK_EXT_device_generated_commands (DGC) | PART4 §3.3-P | 4d | ✅完了 (15b89ad, 受け皿 = indirect_exec::Mode 経路 picker) |
| (Q) | VK_EXT_shader_object | PART4 §3.3-Q | 4d | 🟡受け皿 (未着手) |
| (R) | VK_EXT_descriptor_buffer (Pascal 安全弁) | PART4 §3.3-R | 4d | 🟡受け皿 (Pascal 強制無効) |

### 最尖端取り込み第三弾 (§0.4・rev.6)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (S) | Motion vector RT (4a MRT に追加) | PART4 §3.4-S | 4a | ✅完了 (ed0d80e) |
| (T) | Dynamic rendering (VkRenderPass 撤廃) | PART4 §3.4-T | 4a-1 / 4a-2 → 4d | ✅完了 (af3dd72 main_pass + ed0d80e OverlayPass、 4d で RenderTarget 抽象 + 他 pass 段階移行残) |
| (U) | Timeline semaphore | PART4 §3.4-U | 4d | 🟡受け皿 (未着手) |
| (V) | Async compute queue family 取得 | PART4 §3.4-V | 4d | 🟡受け皿 (未着手・実並列化は Foundations §2) |

### 隣接最尖端 (Vulkan13_Modernization・rev.1)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (W) | VK_KHR_synchronization2 (barrier API 現代化) | Vulkan13 §1 | PART4 4-前-0 の次 | ✅完了 (e1494bf, barrier.h ヘルパ + 段階移行) |
| (X) | VK_EXT_extended_dynamic_state 1/2/3 | Vulkan13 §2 | PART4 4d (Q と同時) | 🟡受け皿 (未着手) |
| (Y) | Pipeline cache 永続化 | Vulkan13 §3 | いつでも (並列可) | 🟢実装推奨 (未着手) |
| (Z) | VK_KHR_fragment_shading_rate (VRS) | Vulkan13 §4 | Phase 3 | 🟡受け皿 (Pascal 非対応) |
| (AA) | Infinite far plane (Reverse-Z の本来形) | Vulkan13 §5 | **4-前-0 と同時** | ✅完了 (702c773) |

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
| **4b** ★次 | HiZPass = SPD で min+max ペア生成 | (M) + (N) | 🟢 着手予定 |
| **4c** | Two-pass occlusion 本体 + AABB 遮蔽 | (F) + (C) | 🟢 未着手 |
| **4d** | 能力ゲート集約 + 受け皿群 + 仕上げ | (Q) + (R) + (U) + (V) + (X)、 (T) は他 pass 段階移行 | 🟡 部分着手 (T main_pass+overlay 済) |
| 並列 | Pipeline cache 永続化 (いつでも) | (Y) ← Vulkan13 §3 | 🟢 未着手 |
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

## 7. 現在の状態 (2026-05-28 時点・PART4 §6 4-前/4a 全部完了・次は 4b)

- **PART4 §6 4-前-0〜4-前-5 + 4a-1 + 4a-2 = 完了** (全 8 段, commit 702c773 / ff9f7a9 / b8e39b2 / ec9c586 / 15b89ad / 986ba44 / af3dd72 / ed0d80e、 Vulkan13 W も完了 e1494bf)。 これで Hi-Z occlusion 本体 (4b/4c) と 4d 受け皿群が乗る土台が全部立った。
- **次の着手 = PART4 §6 4b**: HiZPass 新設 = AMD FidelityFX SPD で min+max ペア HZB を 1 dispatch 生成 (`renderer/hiz_pass.*` + `hiz_spd.comp` 新規)。 入力は 4a-2 で SAMPLED 化した main_pass 出力深度。
- **その次**: 4c (two-pass occlusion 本体 + AABB 遮蔽) → 4d (能力ゲート集約 + DGC/Shader Object/Descriptor Buffer/Timeline semaphore/Async compute 受け皿 + RenderTarget 抽象 + 一時ログ掃除)。
- **PART4 §6 で完了した最尖端 ID 一覧** (§1 表より): (A) (B) (D) (E) (G) (H) (I) (J) (K) (O) (P 受け皿) (S) (T main+overlay) (W) (AA) = **15 ID 完了**。 残: (C) (F) (M) (N) ((T) 他 pass) (Q) (R) (U) (V) (X) (Y) (Z)。
