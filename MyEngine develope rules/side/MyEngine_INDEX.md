# MyEngine 設計知見 — 横断インデックス (rev.2)

最終更新: 2026-05-28 (rev.2: 運用モード切替で §6「ファイル添付の最小構成 (毎回必須)」を「セッション開始時に Read する最小構成」に書換。Work_Protocol rev.13 / START_HERE / Codebase_Guide rev.11 と整合 / rev.1: 設計知見が 4 系統 8 ファイルに分散したため横断インデックスを新設。全項目 ID (A)〜(AA) の一覧 + 担当ファイル§ + 着手時期 + 状態を 1 表に集約。同じ話題が複数ファイルに散る箇所のクロスリファレンス) / 対象: MyEngine の設計を**最短で見渡す**ためのインデックス。各ファイルへの参照は§単位

> **このファイルの位置づけ**: 各セッション最初に最優先で Read する横断索引。「全部読まないと判断できない」状態を解消し、必要箇所だけ深掘りできるようにする。本文は他ファイルにある (このファイルは目次)。
> **読む順序の推奨**: ① **本 INDEX** → ② 正本5枚で原則確認 → ③ 着手する作業に応じて PART4 / Foundations / Vulkan13 のうち該当§のみ深掘り。

---

## 0. ファイル構成 (4 系統)

| 系統 | ファイル | rev | 行数 | 役割 |
|---|---|---|---|---|
| **正本5枚** | MyEngine_START_HERE.md | — | — | 入口・ゴール・現在地・運用 |
| | MyEngine_Graphics_Roadmap_2026.md | rev.8 | — | 全 Phase 計画 |
| | MyEngine_Phase_Dependencies.md | rev.7 | — | Phase 間依存マップ |
| | MyEngine_Codebase_Guide.md | rev.10 | — | コード構造の地図 |
| | MyEngine_Work_Protocol.md | rev.12 | — | 作業規範・原則 (§0/§1.5/§5b/§5c 等) |
| **作業正本** | MyEngine_HiZ_PART4_Design.md | **rev.6** | **471** | Phase 2B PART4 Hi-Z 設計 (着手中) |
| **土台監査** | MyEngine_Foundations_Audit.md | **rev.3** | **237** | 先回り受け皿 + 実ソース確認済み既存負債 |
| **隣接機能** | MyEngine_Vulkan13_Modernization.md | **rev.1** | — | Vulkan 1.3 modernization (W-AA) |
| **索引** | MyEngine_INDEX.md (本書) | rev.1 | — | 横断インデックス |

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
| (A) | draw-count + 可視コマンド圧縮 | PART4 §3.1-A | 4-前-4 | 🟢実装 |
| (B) | 動的容量 (MAX_DRAWS 撤廃) | PART4 §3.1-B | 4-前-3 | 🟢実装 |
| (C) | AABB 遮蔽 (half-extent 充填) | PART4 §3.1-C | 4c | 🟢実装 |
| (D) | 可視履歴 (visBuf) | PART4 §3.1-D | 4-前-3 | 🟡受け皿 |
| (E) | CPU indirect 呼び出し収束 (block sort) | PART4 §3.1-E | 4-前-1 | 🟢実装 |

### 最尖端取り込み第一弾 (§0.2・rev.4)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (F) | Two-pass HZB occlusion | PART4 §3.2-F | 4c | 🟢実装 |
| (G) | Meshlet-ready CullObject | PART4 §3.2-G | 4-前-2 | 🟡受け皿 (mesh shader 後段) |
| (H) | Depth-normal prepass (軽量 GBuffer) | PART4 §3.2-H | 4a | 🟢実装 |
| (I) | Blelloch scan compaction | PART4 §3.2-I | 4-前-4 | 🟢実装 |
| (J) | Shadow_pass GPU-driven 化 | PART4 §3.2-J | 4-前-5 | 🟢実装 |
| (K) | Persistent CullObject buffer | PART4 §3.2-K | 4-前-3 | 🟡受け皿 (dirty tracking 後段) |
| (L) | forward+ 路線維持 | PART4 §0.2-L | — | ⚫選択 (VisBuffer/Nanite 取らず) |

### 最尖端取り込み第二弾 (§0.3・rev.5)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (M) | SPD (Single Pass Downsampler) HZB 生成 | PART4 §3.3-M | 4b | 🟢実装 |
| (N) | min+max ペア HZB (RG32F) | PART4 §3.3-N | 4b | 🟢実装 |
| (O) | Reverse-Z depth | PART4 §3.3-O | 4-前-0 | 🟢実装 |
| (P) | VK_EXT_device_generated_commands (DGC) | PART4 §3.3-P | 4d | 🟡受け皿 |
| (Q) | VK_EXT_shader_object | PART4 §3.3-Q | 4d | 🟡受け皿 |
| (R) | VK_EXT_descriptor_buffer (Pascal 安全弁) | PART4 §3.3-R | 4d | 🟡受け皿 (Pascal 強制無効) |

### 最尖端取り込み第三弾 (§0.4・rev.6)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (S) | Motion vector RT (4a MRT に追加) | PART4 §3.4-S | 4a | 🟢実装 |
| (T) | Dynamic rendering (VkRenderPass 撤廃) | PART4 §3.4-T | 4d | 🟡受け皿 (RenderTarget 抽象型) |
| (U) | Timeline semaphore | PART4 §3.4-U | 4d | 🟡受け皿 |
| (V) | Async compute queue family 取得 | PART4 §3.4-V | 4d | 🟡受け皿 (実並列化は Foundations §2) |

### 隣接最尖端 (Vulkan13_Modernization・rev.1)

| ID | 名称 | 担当 | 着手時期 | 状態 |
|---|---|---|---|---|
| (W) | VK_KHR_synchronization2 (barrier API 現代化) | Vulkan13 §1 | PART4 4-前-0 の次 | 🟢実装推奨 |
| (X) | VK_EXT_extended_dynamic_state 1/2/3 | Vulkan13 §2 | PART4 4d (Q と同時) | 🟡受け皿 |
| (Y) | Pipeline cache 永続化 | Vulkan13 §3 | いつでも (並列可) | 🟢実装推奨 |
| (Z) | VK_KHR_fragment_shading_rate (VRS) | Vulkan13 §4 | Phase 3 | 🟡受け皿 (Pascal 非対応) |
| (AA) | Infinite far plane (Reverse-Z の本来形) | Vulkan13 §5 | **4-前-0 と同時** | 🟢実装推奨 |

---

## 2. PART4 §6 分割案 — 着手順と項目 ID マッピング

| Step | 内容 | 取り込む項目 ID |
|---|---|---|
| **4-前-0** | Reverse-Z 全面切替 + Infinite far plane | **(O) + (AA)** |
| (次) | barrier API を synchronization2 ヘルパに集約 | **(W)** ← PART4 設計書ではなく Vulkan13 §1 |
| **4-前-1** | builder block sort + main_pass 区間検出撤去 | (E) |
| **4-前-2** | CullObject meshlet-ready 拡張 + cone test | (G) |
| **4-前-3** | Persistent CullObject + grow 経路 + visBuf | (K) + (B) + (D) |
| **4-前-4** | Scan compaction + IndirectCount + DGC 受け皿 | (I) + (A) + (P) 受け皿 |
| **4-前-5** | Shadow_pass GPU-driven 化 | (J) |
| **4a** | Depth-normal-motion prepass + 深度 SAMPLED | (H) + (S) |
| **4b** | HiZPass = SPD で min+max ペア生成 | (M) + (N) |
| **4c** | Two-pass occlusion 本体 + AABB 遮蔽 | (F) + (C) |
| **4d** | 能力ゲート集約 + 受け皿群 + 仕上げ | (P) + (Q) + (R) + (T) + (U) + (V) + (X) |
| 並列 | Pipeline cache 永続化 (いつでも) | (Y) ← Vulkan13 §3 |
| 後段 | TAA / 完全 dirty tracking / shadow HZB / DGC 実装 etc. | 受け皿利用 |
| Phase 3 | VRS の本実装 | (Z) ← Vulkan13 §4 |

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

## 7. 現在の状態 (2026-05-27 時点・着手中)

- **設計フェーズ**: PART4 rev.6 設計完了 + Vulkan13 rev.1 別立て完了。実装未着手。
- **次の着手**: PART4 §6 **4-前-0 (Reverse-Z + Infinite far) を Vulkan13 §5 AA と同時 commit**。
- **その次**: Vulkan13 §1 W (synchronization2 ヘルパ集約) → PART4 4-前-1 (block sort) → 以下 §2 表の順。
- **直前の dump (PART4-PRE-A〜E) で確定済み事実**: §1①〜⑨ (PART4 設計書)。
- **直前の dump (DEBT-1〜10) で確定済み事実**: Foundations §8.1〜8.8。
