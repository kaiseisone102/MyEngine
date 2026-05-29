# MyEngine Vulkan 1.3 / 1.4 現代化 / 隣接最尖端機能取り込み計画 (rev.3)

最終更新: 2026-05-29 (rev.3: **PART4 §6 4d audit-driven 大量取り込み反映**。 §3 (Y) Pipeline cache 永続化 = ✅ **完了** (commit a62b7f0 = 4d M1・SDL_GetPrefPath 経由・14 callsite 全経由・user clean exit で 490KB 書き出し実証)。 **新 §9 = 4d M3/M2/N1/N4/N2/N3 追加項目** (`dynamic_rendering_local_read` 1.4 core / sync2 generic layouts 全面置換 / `pipelineCreationCacheControl` 1.3 core / Vulkan14Features chain + maintenance5/6 / `graphics_pipeline_library` + `pipeline_binary` 受け皿)、 §0 表の「(T) Dynamic rendering」連動注記を「engine 全体で実 API 使用ゼロ」に格上げ (γ-1/2/3 で Post/Shadow/Reflection も dynamic rendering 化)、 §6 着手順表で 3 = 完了マーク、 §7 Capabilities struct を実体に追従 (`asyncComputeFamily/Queue/hasDedicatedAsyncCompute` / `dynamicRenderingLocalRead` / `pipelineCreationCacheControl` / `maintenance5/6` / `graphicsPipelineLibrary` / `pipelineBinary` / `pipelineCache` 追加)。 / rev.2: §1 (W) synchronization2 = ✅ **完了** (commit e1494bf 2026-05-28、 `include/MyEngine/renderer/barrier.h` ヘルパ + sync2/sync1 fallback 同居 + 段階移行)、 §5 (AA) Infinite far plane = ✅ **完了** (commit 702c773、 `renderer/projection.h::makeReversedZInfinitePerspective` 経由)。 さらに **(T) Dynamic rendering の main_pass + OverlayPass 部分も実装済** (4a-1 commit af3dd72 + 4a-2 commit ed0d80e、 §0 表の連動セクション末尾に注記)。 §6 着手順表で 1/2 = 完了マーク、 3〜5 が残。 / rev.1: PART4 設計 rev.6 の再点検で見つかった隣接最尖端取りこぼし 5 件 (W)〜(AA) を独立文書化。PART4 が Hi-Z 本体に集中するため、これらは別ファイルで管理。すべて Vulkan 現代標準 (1.3 core or 主要拡張) で、能力ゲート + フォールバック付きの受け皿として段階的に取り込み可能。PART4 / Foundations_Audit / 正本5枚と対で読む) / 対象: MyEngine を「現代 Vulkan の標準形」に乗せる作業

> **この文書の位置づけ**: PART4 (Hi-Z) と並ぶ「最尖端化作業」の文書。PART4 が Hi-Z 機能の追加なら、こちらは**既存基盤の現代化** (古い API → Vulkan 1.3 modern API)。両者は独立して進められるが、相互に影響する箇所がある (PART4 で導入する受け皿 P/Q/R/T/U/V とここの W/X が連動)。
> **方針の根拠**: §0 (最新を第一基準・受け皿を先に最新で用意)、§1.5-C (能力チェック+フォールバック・実測してから分岐)、§5b (capability 構造体を 1 箇所に集約)。
> **本文書と PART4 設計 §0.2/§0.3/§0.4 (F)〜(V) の関係**: F〜V は **PART4 のスコープ内**で取り込む受け皿。W〜AA は **PART4 のスコープを膨らませないため別立て**だが、性質は同じ (能力ゲート + 段階移行)。

---

## 0. 分類 — Vulkan 1.3 modern triad と隣接機能

| ID | 機能 | 種別 | PART4 との連動 |
|---|---|---|---|
| (W) | VK_KHR_synchronization2 | Vulkan 1.3 core | PART4 全 barrier 箇所 (cull/HZB/main) で使用 |
| (X) | VK_EXT_extended_dynamic_state 1/2/3 | Vulkan 1.3 部分 core + 拡張 | PART4 §3.3-Q ShaderProgram と並列 |
| (Y) | Pipeline cache 永続化 | 古典 (Vulkan 1.0〜) | PART4 で増える pipeline 数の対策 |
| (Z) | VK_KHR_fragment_shading_rate (VRS) | 拡張 (Turing+) | PART4 §3.4-S motion vector RT が VRS rate map 入力に使える |
| (AA) | Infinite far plane + Reverse-Z | 投影行列の設計選択 | PART4 §0.3-O Reverse-Z の本来形 |

**W/X/Y は Vulkan 1.3 modern Vulkan の標準三本柱**: dynamic_rendering (PART4 T) / shader_object (PART4 Q) と合わせて、現代 Vulkan アプリの「最低限満たすべき形」を構成する (NVIDIA dos-and-don'ts 2025-01 / Khronos sample hello_triangle_1_3 が明示)。MyEngine が VkRenderPass 持ち + binary semaphore + 古い barrier API で動いている現状は **Vulkan 1.0 系の書き方**で、modernization が必要。

---

## 1. (W) VK_KHR_synchronization2 (Vulkan 1.3 core) — Pipeline barrier API 現代化 — ✅ **完了 2026-05-28 commit e1494bf**

### 完了内容
- `include/MyEngine/renderer/barrier.h` (header-only) を新設。 sync2 `VkDependencyInfo` を一括記録する `recordBatch` + 単発 `recordImage/recordBuffer`、 capability 未対応時の sync1 fallback 同居。 `ImageBarrier / BufferBarrier / MemoryBarrier` 構造体 + QFOT (queue family ownership transfer) パラメータ (streaming / async compute 用)。
- `vulkan_context::synchronization2()` capability getter 追加 (1.3 core feature query)。 P620 実測 1。
- 5 サイト移行 (bloom mip / culling / shadow / mainPass のうち適用箇所)。 残りは触る機会に順次移行。
- PART4 §6 4-前-3 以降の新規 barrier (persistent CullObject の staging upload, scan compaction の COMPUTE→DRAW_INDIRECT, main_pass の MRT attachment 遷移) は全て barrier.h ヘルパ経由 = 最初から sync2 経路。

### 元の現状記述 (参考)

### 現状 (実コード確認済み)
- `culling_pass.cpp:153-164`: `VkBufferMemoryBarrier b{}` + `vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 1, &b, 0, nullptr)` = **古い API**。
- 同様の古い barrier が bloom_pass / shadow_pass / main_pass / texture / その他に散在 (実コード dump 必要)。

### 何が新しいか
- 新 API: `VkDependencyInfoKHR` + `vkCmdPipelineBarrier2` (Vulkan 1.3 core)。
- `VkBufferMemoryBarrier2` / `VkImageMemoryBarrier2` は stage と access flag が **memory barrier 構造体内で一緒に指定**される (両者の関係が明確)。
- `VkPipelineStageFlags2` / `VkAccessFlags2` で **より細かい stage 区分** (例: `VERTEX_INPUT` が `INDEX_INPUT` + `VERTEX_INPUT_BIT` に分割、`TRANSFER` が 4 つに分割)。
- NVIDIA 公式 (2025-01): "Use VK_KHR_synchronization2, the new functions allow the application to describe barriers more accurately. Group barriers in one call to vkCmdPipelineBarrier2() ... worst case can be picked instead of sequentially going through all barriers."

### MyEngine への適用
- **能力ゲート**: `vulkan_context::capabilities().synchronization2` を追加 (`VK_KHR_synchronization2` feature query)。Vulkan 1.3 device なら基本対応。
- **受け皿**: barrier ヘルパ関数 `recordBufferBarrier(cmd, ...)` / `recordImageBarrier(cmd, ...)` を 1 箇所に集約し、内部で能力分岐 (synchronization2 対応時は `*Barrier2` + `vkCmdPipelineBarrier2` / 非対応時は古い API)。
- **段階移行**: 既存の barrier 散在箇所をヘルパ経由に置換。1 箇所ずつ commit 可能 (古い API と並走可)。
- **PART4 との連動**: PART4 4-前-4 / 4b / 4c で追加する barrier (cull→indirect, HZB compute mip 間, main pass→HZB) はヘルパ経由で書く = 最初から synchronization2 経路。

### 推奨着手時期
PART4 4-前-0 (Reverse-Z) の次に置く。barrier ヘルパを先に作って PART4 の新規 barrier をすべてヘルパ経由にすれば、自然に最新形で書ける。

---

## 2. (X) VK_EXT_extended_dynamic_state 1/2/3 — Pipeline state の動的化

### 何が新しいか
- 従来: `VkPipeline` 作成時に大量の state を固定 (depth test / cull mode / blend / viewport count 等) → state 組合せ爆発で pipeline 数が増える。
- Extended dynamic state: これらを `vkCmdSet*EXT` で動的設定可能に。pipeline 数を大幅削減。
- Vulkan 1.3 で extended_dynamic_state と 2 が core 化、3 は拡張のまま (cull mode の動的化、blend equation など更に細かい state)。
- 効果 (Khronos 公式 proposal): <span>"Dynamic state helps applications reduce the number of pipelines they need to create and bind ... pipeline 作成を ahead-of-time にしやすくなり hitching 回避 ... applications が hash/cache する state 量も減る"</span>。

### MyEngine への適用
- **能力ゲート**: `extendedDynamicState` / `extendedDynamicState2` / `extendedDynamicState3` 各 feature を `capabilities()` に追加。
- **PART4 §3.3-Q ShaderProgram と連動**: shader_object 経路と extended dynamic state は両方とも pipeline 削減技術で、**組み合わせると pipeline オブジェクトをほぼ撤廃**できる。`ShaderProgram` 抽象型の内部で「shader_object + dynamic state」と「VkPipeline (legacy)」を能力分岐。
- **段階移行**: まず depth test / cull mode / viewport を dynamic 化 (一番効くもの)。後段で blend / rasterization 等。

### 推奨着手時期
PART4 4d (能力ゲート集約 + ShaderProgram 受け皿) で同時に。**ShaderProgram の "shader_object 経路" は extended dynamic state とセットで動く** (shader_object は state を持たないので dynamic state で補完する)。

---

## 3. (Y) Pipeline cache 永続化 — 起動時間短縮 / hitching 解消 — ✅ **完了 2026-05-29 commit a62b7f0 (4d M1)**

### 完了内容
- `VulkanContext` に `pipelineCache_` (`VkUnique<VkPipelineCache>`) + `pipelineCachePath_` (std::string) を追加。 init 時に `createPipelineCache()` 内部で `SDL_GetPrefPath("MyEngine", "MyEngine")` + "pipeline.cache" 経由でファイルから initial data を読み `vkCreatePipelineCache` (initialDataSize は file size、 fresh start なら 0)。 shutdown 時に `savePipelineCache()` 内部で `vkGetPipelineCacheData` 2 呼び出し (size 問い合わせ → 取得) して書き戻し。
- **engine 内 14 全 vkCreate*Pipelines callsite が `ctx_->pipelineCache()` 経由** に統一: main_pass / shadow_pass (経由 `ShadowPipelineParams` 拡張) / reflection_pass / post_pass / overlay_pass / bloom_pass / culling_pass / hiz_pass / particle / debug_line / hud / water / imgui pipeline。 生 VK_NULL_HANDLE 直渡しは禁止 (新規 pipeline を作る際もここを経由)。
- **検証**: user clean exit で 490KB 書き出し実証。 2 回目以降の起動で shader compile hitch がほぼゼロになる前提が立つ。
- **N1 受け皿併設 (7298968)**: `pipelineCreationCacheControl` (Vulkan 1.3 core) も enable。 streaming 中の `VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT` で「runtime 中に新 pipeline compile が必要になった site」を検出する受け皿 (実適用は別 commit で)。
- **UUID チェック**: 現状未実装 (driver 更新で cache 破棄を狙うなら追加可)。 `vkCreatePipelineCache` は invalid data を黙って捨てるので致命傷ではない。

### 元の現状記述 (参考)

### 現状 (要確認)
- MyEngine の `VkPipelineCache` 利用状況は実コード dump で確認 (`vkCreatePipelineCache` の grep)。おそらく **VK_NULL_HANDLE 渡し or in-memory のみ**で disk 永続化未実装。

### 何が新しいか (古典 + Vulkan 1.4 改善)
- 古典: `VkPipelineCache` を作成時に disk から読み込んだバイナリで初期化、shutdown で `vkGetPipelineCacheData` で保存。再起動時の shader compile を省略。
- 効果: 大規模 engine では **起動時の shader compile を完全省略**できる (NVIDIA: "creating pipelines asynchronously, using pipeline cache, and minimizing the number of vkCmdBindPipeline calls")。
- 補完: `VK_EXT_pipeline_creation_cache_control` (Vulkan 1.3 core) で「キャッシュ非対応時の例外動作」を制御可能。

### MyEngine への適用
- **受け皿**: `VulkanContext` に `VkPipelineCache pipelineCache_` を追加し、init で disk から `LoadPipelineCache(path)` (バイナリ + UUID 検証)、shutdown で `SavePipelineCache(path)`。
- すべての `vkCreate*Pipelines` 呼び出しに pipelineCache_ を渡す形に変える (現状は VK_NULL_HANDLE の疑い)。
- **能力ゲート**: pipeline cache は Vulkan 1.0 から存在するので能力分岐不要。
- **UUID チェック**: GPU 変更 / driver 更新で cache データが invalid になるので、device UUID で検証。

### 推奨着手時期
PART4 とは独立。いつでも可。実装が軽い (数十行)。PART4 で pipeline 数が増える (compute pipeline 追加 = SPD / scan / cull の two-pass 化) 前にやると効果大。

---

## 4. (Z) VK_KHR_fragment_shading_rate (VRS) — Fragment 単位の負荷削減

### 何が新しいか
- Fragment shader の実行レートを per-draw / per-primitive / per-tile で動的に変える (1x1 から 4x4 まで)。
- 視覚的に重要でない領域 (周辺視野 / 動きが速い領域 / 影 / 背景) の shading rate を下げて GPU 負荷削減。
- Arm 公式の実測: <span>"8x8 tiles + max 2x2 rate で 12% GPU cycle 削減・読み bandwidth +3% / 書き bandwidth -1%"</span>。
- Turing 以降 (NVIDIA RTX) / RDNA2 以降 (AMD) 対応。**Pascal (P620) は非対応**。

### MyEngine への適用 (Pascal 非対応下での受け皿)
- **能力ゲート**: `capabilities().fragmentShadingRate` を追加 (`VkPhysicalDeviceFragmentShadingRateFeaturesKHR`)。
- **PART4 §3.4-S motion vector RT との連動 (重要)**: VRS の rate map は **screen-space attachment** で指定する形が一般 (tier 2 VRS)。**motion vector が大きい領域 = 動きが速い = 視覚的に重要度低 = rate を下げてよい** → motion vector RT を入力に rate map を生成する compute pass を加えると VRS が自動駆動できる。PART4 で取った motion vector の用途が広がる。
- **段階**: Pascal は非対応なので **受け皿だけ** (Pascal 退役後 or 他 GPU で動かすときに有効化)。能力ゲート + rate map 生成 compute pass の構造を確保する。

### 推奨着手時期
Phase 3 post AA 着手時に motion vector 利用と一緒に。PART4 範囲外だが、受け皿の認識は今 (motion vector RT の出力フォーマット決定に影響しないか確認しておく)。

---

## 5. (AA) Infinite far plane + Reverse-Z — Reverse-Z (PART4 §0.3-O) の本来形 — ✅ **完了 2026-05-28 commit 702c773**

### 完了内容
- `include/MyEngine/renderer/projection.h::makeReversedZInfinitePerspective(fovY, aspect, zNear)` 新設 (Y-flip 込みの一発呼び出し)。
- `vulkan_renderer` で投影行列をこのヘルパ経由に。 cameraParams は `near=0.1f, far=INFINITY, fov, aspect` を保持 (shader 側は cameraParams.y を読まない設計、 必要なら near と ndc_z から linearize)。
- shadow_pass の light projection は orthographic 維持、 reflection_pass の oblique projection は別途処理 = 影響無し。
- 全 render pass の depth clear は 0.0、 compareOp は GREATER に統一。
- HZB の depth 比較は変わらず (reverse-Z、 max depth = 1.0 のテクセルは「遠方の何もない領域」= occlusion test 常時通る)。

### 元の現状記述 (参考)

### 現状 (PART4 §0.3-O)
- Reverse-Z を採用 (depth=0 が遠方・float 精度を near/far 全域でほぼ均一)。
- だが `vulkan_renderer.cpp:101` の `cameraParams = vec4(0.1f, 200.f, ...)` の **far=200 を維持**。

### 何が新しいか (本来形)
- Reverse-Z + float depth の組み合わせは **far→∞ にできる**のが最大利点。
- 投影行列を near 1 点だけで定義する形 (far=∞ の極限を取る): proj[2][2] = 0, proj[3][2] = near。
- これで「遠方クリップが描画範囲制約」がなくなる = **オープンワールドの広域描画**を構造的に解決。
- Foundations §8.8 で「far=200 は大規模と矛盾しうる」と挙げた負債と直結。Reverse-Z だけでは負債を解消しきれず、**infinite far との組み合わせで完成**。

### MyEngine への適用
- **投影行列ヘルパ**: `makeReversedZInfinitePerspective(fovY, aspect, near)` を導入 (glm 拡張 or 自前)。
- `vulkan_renderer.cpp:101` の cameraParams を `vec4(near=0.1f, INFINITY, ...)` (or far フィールドを撤廃して near + fov + aspect のみに)。
- HZB の depth 比較は変わらず (reverse-Z なので max-depth = 最も浅い = 0 に近い値)。infinite far で max depth=1.0 のテクセルは「遠方の何もない領域」=「occlusion test で常に通る」 = 自然に動く。
- **能力ゲート不要**: 投影行列の選択なので Vulkan 機能依存なし。
- **注意**: shadow_pass の light projection が orthographic なら影響なし。reflection_pass の oblique projection は別途確認。

### 推奨着手時期
**PART4 4-前-0 (Reverse-Z 切替) と同時**にやるのが最も自然 (投影行列の改修が同じ箇所)。rev.6 §6 4-前-0 では Reverse-Z だけ書いたが、infinite far も同 commit に含めるべき。**PART4 設計書 rev.7 で 4-前-0 を拡張する**か、本文書の §5 推奨を 4-前-0 着手時に参照する形。

---

## 6. 着手順 / 優先度 (推奨)

| 順 | 項目 | 着手時期 | コスト | 効果 | 状態 |
|---|---|---|---|---|---|
| 1 | (AA) Infinite far + Reverse-Z 完成形 | **PART4 4-前-0 と同時** | 低 (投影行列ヘルパ 1 個 + cameraParams 1 行) | 大 (広域描画の構造的解放) | ✅ 完了 (702c773) |
| 2 | (W) synchronization2 へ移行 | PART4 4-前-0 の次 (PART4 の新 barrier 前) | 中 (barrier ヘルパ + 散在置換) | 大 (barrier 精度 + Vulkan 1.3 入口) | ✅ 完了 (e1494bf) |
| - | (T) Dynamic rendering (engine 全体) | 4a-1 / 4a-2 → 4d γ-1/2/3 | 中 (各 pass の VkRenderPass 撤去 + child pipeline format ベース化 + OverlayPass 分離) | 大 (Vulkan 1.3 modern triad 完成・engine 全体で実 API 使用ゼロ) | ✅ **完了** (4a-1 af3dd72 main + 4a-2 ed0d80e overlay + 4d γ-1 4b9c32c PostPass + 4d γ-2 da74526 ShadowPass + 4d γ-3 33e1511 ReflectionPass) |
| 3 | (Y) Pipeline cache 永続化 | 4d M1 | 低 (数十行 + 14 callsite 配線) | 中 (起動時間短縮 / hitching 解消・user 実測 490KB 書き出し) | ✅ **完了** (a62b7f0) |
| 4 | (X) Extended dynamic state | PART4 4d ShaderProgram と同時 | 中 (能力ゲート + state 動的化) | 中 (pipeline 数削減・PART4 4d 受け皿 Q とセット) | 🟡 未着手 |
| 5 | (Z) VRS 受け皿 | Phase 3 post AA 着手時 | 低 (受け皿だけ・Pascal 非対応) | (motion vector の用途拡大・将来 GPU 移行時に効く) | 🟡 受け皿 (motion vector RT は ed0d80e で取得済 = 連動準備済) |

優先度 1〜3 は **PART4 進行中に並走可能**。優先度 4 は PART4 4d に組み込み。優先度 5 は Phase 3 まで保留。

---

## 7. 能力 query の集約 (PART4 §6 4d の "capabilities() struct" に追加すべき項目)

PART4 4d で集約する `vulkan_context::capabilities()` struct に、本文書由来の項目を追加:

```cpp
// 実体は VulkanContext の getter 群 (struct 集約は未実装・getter 直接でも 1 箇所集約は実現済み):
//
// PART4 / 2B 由来:
ctx_->multiDrawIndirect();                 // ✅ enabled (P620=1)
ctx_->drawIndirectFirstInstance();         // ✅ enabled (P620=1)
ctx_->drawIndirectCount();                 // ✅ enabled (P620=1, 1.2 core)
ctx_->deviceGeneratedCommands();           // 🟡 query のみ (P620=0)
ctx_->dynamicRendering();                  // ✅ enabled (P620=1, 1.3 core)
ctx_->separateDepthStencilLayouts();       // ✅ enabled (P620=1, 1.2 core optional)
ctx_->subgroupOps() / subgroupSize();      // ✅ basic+shuffle / 32 (P620)
ctx_->samplerFilterMinmax();               // ✅ enabled (P620=1, 4c-B)
ctx_->asyncComputeFamily()                 // ✅ family 2 (P620 dedicated)
   / asyncComputeQueue()
   / hasDedicatedAsyncCompute();
ctx_->shaderStorageImageArrayDynamicIndexing(); // ✅ enabled (P620=1, 4b 必須)
// 4d M3/N1/N4/N2/N3 由来:
ctx_->dynamicRenderingLocalRead();         // ✅ enabled (P620=1, 1.4 core, 4d M3)
ctx_->pipelineCreationCacheControl();      // ✅ enabled (P620=1, 1.3 core, 4d N1)
ctx_->maintenance5() / maintenance6();     // ✅ enabled (P620=1, 1.4 core, 4d N4)
ctx_->graphicsPipelineLibrary();           // ✅ enabled (P620=1, 4d N2 受け皿)
ctx_->pipelineBinary();                    // ✅ enabled (P620=1, 4d N3 受け皿)
// 本文書由来 (rev.1/2):
ctx_->synchronization2();                  // ✅ enabled (P620=1, 1.3 core, W)
ctx_->pipelineCache();                     // ✅ persistent (4d M1 = Y closed)
// 残 (受け皿未着手):
//   - extendedDynamicState 1/2/3 (X)
//   - shaderObject (Q)
//   - descriptorBuffer (R, Pascal 強制無効化)
//   - timelineSemaphore (U)
//   - fragmentShadingRate (Z, Pascal 非対応想定)
```

`vulkan_context.cpp` の init で全 query を 1 箇所で実行し、各 pass / pool / pipeline 作成箇所はこの getter を読むだけ (§5b 指定の "capability 構造体の住処を 1 箇所" を getter 経由で実現済み)。 **【4d N4 確定】device 作成時の features pNext chain に `VkPhysicalDeviceVulkan14Features` を追加済み** (engine が API 1.4 で動作中なのに 1.4 features を一切 enable してない構造欠陥を修正)。 1.4 core 機能 (M3 = dynamic_rendering_local_read、 maintenance5/6 など) を query / enable するときはここに足す。

---

## 9. PART4 §6 4d audit-driven 追加項目 (2026-05-29 = M3 / M2 / N1 / N4 / N2 / N3)

PART4 §6 4d で user 主導 audit (「最新技術を取りこぼしている箇所がないか」) により判明・取り込んだ最新項目を本文書にも記録 (rev.1 の (W)〜(AA) の延長線上)。 これらは PART4 設計書 §6 4d と本文書の両方に登録 (横断 ID 採番は別途整理可)。

### M3 — VK_KHR_dynamic_rendering_local_read (Vulkan 1.4 core) — ✅ 完了 commit 47c3571
- 何が新しいか: 同一 dynamic-rendering scope 内で attachment を input attachment 経由で「local read」できる (tile-based GPU の on-chip メモリを使えるパス)。 mobile / Apple Silicon で SS 効果 (deferred lighting / SSAO / SSR) の bandwidth が大幅削減。 desktop でも一部 GPU で有効。
- 完了内容: capability `dynamicRenderingLocalRead()` getter + features chain で enable。 受け皿のみ・activation は Phase 3 SS 効果 / deferred 経路を組むときに `vkCmdSetRenderingAttachmentLocations` 経由で使う。

### M2 — sync2 generic image layouts 全面置換 — ✅ 完了 commit fcef5ab
- 何が新しいか: Vulkan 1.3 で `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL` / `READ_ONLY_OPTIMAL` の generic layout が導入された (旧 color/depth/stencil 別の `*_OPTIMAL` を全部置き換える sync2 推奨形)。 separateDepthStencilLayouts の query/分岐が不要に。
- 完了内容: 旧 `depth_layouts::attachment/readOnly(ctx)` ヘルパを撤去し、 19 callsite (main_pass / shadow_pass / hiz_pass / widget / overlay / reflection / post / culling など) で `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL` / `READ_ONLY_OPTIMAL` を直接利用に置換。 コード -45 行 + ヘッダ 1 ファイル削除。 新規 pass の barrier も generic layouts 直接利用。

### N1 — VK_EXT_pipeline_creation_cache_control (Vulkan 1.3 core) — ✅ 完了 commit 7298968
- 何が新しいか: pipeline 作成時に `VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT` を立てると、 cache hit しなければ `VK_PIPELINE_COMPILE_REQUIRED_EXT` を返す (compile を試みない)。 open-world streaming で「runtime 中に新 pipeline compile が走って hitch」する site を検出・回避できる。
- 完了内容: capability `pipelineCreationCacheControl()` getter + features chain で enable。 受け皿のみ・実適用 (specific create flag 付与 + fallback 経路) は別 commit で。 M1 persistent pipeline cache とセットで効く。

### N4 — VkPhysicalDeviceVulkan14Features chain + maintenance5/6 enable — ✅ 完了 commit c01c2e5
- 何が新しいか: Vulkan 1.4 (Dec 2024 release) で promote された機能群。 maintenance5 = `vkCmdBindIndexBuffer2` (size 指定可) / `vkGetDeviceImageSubresourceLayout` 等、 maintenance6 = `vkCmdBindDescriptorSets2` (push constant 経由) 等。
- 完了内容: device 作成時の features pNext chain に `VkPhysicalDeviceVulkan14Features` を追加 (`Vulkan12Features ← Vulkan13Features ← Vulkan14Features ← PhysicalDeviceFeatures2`)。 engine が API 1.4 で動作中なのに 1.4 features を一切 enable してない構造欠陥を修正。 `maintenance5` / `maintenance6` 両 enable・M3 の KHR struct を canonical 1.4 form に統一。 これで以降の 1.4-only 機能を素直に enable できる。

### N2+N3 — VK_EXT_graphics_pipeline_library + VK_KHR_pipeline_binary 受け皿 — ✅ 完了 commit 1481049
- 何が新しいか:
  - graphics_pipeline_library = pipeline を vertex input / pre-rasterization / fragment / fragment output の 4 partial library に分割し、 別々に compile・実行時に link。 state 組合せで pipeline 数が増える問題を解消 (X = extended dynamic state と同類の解だが、 全 state 経路で適用可能)。
  - pipeline_binary = pipeline cache の data を独立した `VkPipelineBinary` オブジェクトとして扱える (cache UUID / driver 変更時の handling が clean)。 1.3.294 で promote。
- 完了内容: extension name walk による query (`vkEnumerateDeviceExtensionProperties` で walk + features chain で enable) で capability getter のみ。 activation (実 pipeline library 化 + binary export/import) は Phase 2A 多光源 (forward+ で state 組合せ増) や Phase 3 SS 効果着手時に。

---

## 8. 本文書の更新運用 (§6 セッション終わり運用)

- (W)〜(AA) いずれかに着手・確認するたびに本文書を更新し、該当項目の「現状」セクションを「実装済み (commit ...)」に書き換える。
- 新たな取りこぼし (BB 以降) が web 調査で見つかった場合も本文書に追加する (PART4 設計書を膨らませない受け皿)。
- 完了項目は正本5枚 (Codebase_Guide / Work_Protocol) にも畳み込む。
- 正本5枚 / PART4 設計書 / Foundations_Audit / 本文書の **4 系統で MyEngine の設計知見を保持**。
