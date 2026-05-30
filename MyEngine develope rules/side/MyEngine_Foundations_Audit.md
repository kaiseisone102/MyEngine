# MyEngine オープンワールド土台監査 — 先回りすべき受け皿 / 先送り負債 (rev.8)

最終更新: 2026-05-30 (rev.8: **§8.10 DEBT-12 新設 = §1.5-B 違反 (CPU draw ループ残存) を全監査で確定**。 user 指示「最新技術より既存整合・最小構成を選んでしまったか確認」を受け描画経路を実ソース全監査。 skinned (全経路 CPU draw ループ + vertex-shader skinning 3〜4 回再実行) / grass (CPU per-blade frustum cull) が §1.5-B 違反と判明 → Phase 2G/2H 新設。 受け皿 gpu_skinning.h (S) は per-mesh 固定 = 複数インスタンス非対応の誤設計、 他 9 受け皿は形健全。 Claude 側の §0 違反 2 件 (妥協案を最新の顔で推奨) も教訓として記録。 / rev.7: **最新化マラソン 28 commits 反映 = 監査項目の大半が受け皿確保 or 解消**: §1 camera-relative = ✅ **受け皿確保 (commit 4dc8923) + 全 10 site wire-up 完了 (commit 641abcb E clean)** (`include/MyEngine/world/engine_origin.h` の `EngineOrigin::current()` 返却 + `toEngineRelative` helper で全描画経路 (prop/skinned/grass/reflection/terrain/water/particle/debug_line) が origin = 0 で完全 no-op・floating-origin 起動時に lockstep shift)。 §2 (a-2) transfer queue = ✅ **解消** (C commit e7b852e = P620 family 1 dedicated 検出 + `transferFamily()`/`transferQueue()`/`hasDedicatedTransfer()` getter)。 §2 worker thread pool = ✅ **受け皿確保** (U commit fdbddda = `include/MyEngine/core/job_system.h` header-only・JobSystem::init(0) inert-friendly で既存単一スレッド経路無改修)。 §3 bindless 動的化 = ✅ **半分解消** (G commit 17d5f8f = free-list + slot reuse・streaming で working set ≤ 1024 なら無限・descriptor pool 成長は別 Phase = G+)。 §4 永続 GPU object buffer = 🟡 **受け皿のみ** (H commit 8604de5 = `persistent_object_buffer.h` header-only design memo・実装は Phase 仕事)。 §8.1 固定容量一族 = ✅ **5/6 解消** (F1-F5 commits c3f46ea/0f07dc0/659bece/132d0d5/a46f208 で Material/Instance/Skin/Particle/DebugLine 全部 dynamic 化・残 BindlessTextureRegistry MAX_TEXTURES は G で free-list 解決済・descriptor pool 成長は別 Phase)。 §8.2/§8.5 buffer 系 VMA 化 + private hybrid 撲滅 = ✅ **解消** (A1-A5 commits 995b779/46cb937/185ac09/80ccb76/a030372 で **エンジン内 生 vkAllocateMemory ゼロ達成**・ResourceFactory legacy memory API 全削除)。 §8.3 debug 残骸 = ✅ **解消** (A6 commit dab4faf で title_layer s_dbg + pass_chain [BlockDbg] 撤去・PBR_NORMAL_TEST は既に解消済を確認)。 §7 優先度サマリも全項目に commit 紐付け。 / rev.6: §2 スレッド/transfer キューの受け皿 (a) を分割: **(a-1) async-compute family は ✅ PART4 4c-B (commit 477985d) で取得済** (`asyncComputeFamily()` / `asyncComputeQueue()` / `hasDedicatedAsyncCompute()` getter・P620 で family 2 dedicated 検出)、 **(a-2) transfer queue family は未取得** で streaming 着手前に追加要。 「今やること」も同様に更新。 / rev.5: §5 描画アーキ depth-normal prepass の共有 = ✅ **解消済** (PART4 4a-2 commit ed0d80e)。 MRT 3 attachment (HDR + Normal R10G10B10A2 octahedral + Motion vector RG16F) + 深度 SAMPLED + 前フレーム MVP + OverlayPass 分離 = Phase 3 SSAO/SSGI/SSR/DoF/TAA の受け皿が全部立った。 §7 優先度サマリも該当項目を「解消済」に書換。 / rev.4: §8.9 DEBT-11「既存日本語コメントの英語化」を追加。Work_Protocol rev.15 / START_HERE で「ソース内コメントは英語」ルールが明文化されたのを受け、既存の日本語コメント (src/ 配下に >45 件 / 10 ファイル超) を**順次英語化** (一括 commit でなく触る機会に翻訳) とする方針を記録。リスク低・優先度低 / rev.3: §8.8「DEBT-6〜10 の追加探索 (同期/エラー処理/ハードコード/肥大ファイル/shader 二重定義/recreate)」を追記。**大きな新規負債は出ず、基礎品質はむしろ健全と確認** (drawFrame 内に WaitIdle なし・assert/abort ゼロ・例外統一・types.h 共有で shader 二重定義は最小・recreate 連鎖に漏れの気配なし)。小〜中の新規負債のみ拾った: far=200 直書き (大規模と矛盾しうる)・catch(...) の中身確認 (shadow_pass は正しいクリーンアップ+再throw / model_loader 3箇所は未確認)・gameplay_layer.cpp 946 行の肥大 (非グラフィックス)・HiZPass を recreate 連鎖に入れる宿題。**負債探索は逓減と判断 = 主要負債は DEBT-1〜5 + 土台§1〜§5 で出尽くし** / rev.2: §8「実態確認済みの既存負債 (実ソース棚卸し)」を新設。DEBT-1〜5 の dump で §1〜§5 の推測のうち実体のあるものを行番号付きで事実化し、資料未記載の新規負債も追加。固定容量は一族で散在だが GeometryBuffer/VmaImage/water_mesh に「解消後の手本」が既に存在することを確認。負債を ①意図的据え置き ②今すぐ安く直せる ③将来 Phase で計画解消 に分類 / rev.1: 「オープンワールドエンジン全体として、後から入れると全系統作り直しになる土台負債」を web 調査 (座標精度/floating origin・スレッド/transfer キュー・bindless 常駐性・空間分割/永続オブジェクト・描画アーキ forward+/GBuffer) で洗い出し新設。各項目に「なぜ作り直しか」「受け皿」「確認用 dump」「危険度」を付す) / 対象: MyEngine を大規模オープンワールド (複数光源・複数地面・数千〜数万オブジェクト・広域) として成立させるための土台判断。正本5枚 (START_HERE / Roadmap / 依存マップ / Codebase_Guide / Work_Protocol) + PART4 設計書と対で読む

> **この文書の位置づけ**: ロードマップの Phase (描画機能の積み上げ) とは別軸の「土台 side」の監査。Hi-Z や PBR のような **後から層を足せる追加機能** と違い、ここに挙げるのは **後から入れると既存の全システムを作り直す** 性質の土台判断。「これで動くからいいだろう」で先送りすると、規模が来たときに最大の手戻りになる箇所を、先回りの優先度順にまとめる。
>
> **最重要の前提 (この文書の確度)**: 項目 1〜4 は **実ソース未確認の推測** である。根拠は「正本5枚が一言も触れていない = おそらく未対応」という消去法。**断定しない。各項目の「確認用 dump」で事実化してから着手判断する** (§1-1 推測禁止のソース版)。項目 5 は PART4 で深度を扱う今まさに効く話で、実ソース (main_pass の深度経路) は PART4 設計書で確認済み。
>
> **方針の根拠**: §0 (最新を第一基準・**受け皿を先に最新の形で用意し中身を後から埋める**)、§0.1 (PART4 設計書のスケール第一指針)、§5b (固定容量を現状基準で決めない)、§1.5-B (GPU-driven 完成形・bucket 分離)。

---

## 0. 分類 — 「受け皿が要る土台」と「後から足せる機能」

オープンワールド化で出てくる項目は 2 種類に分かれる。混同しないことがこの監査の肝。

- **(A) 土台 = 後から入れると全系統作り直し**: 座標精度・スレッド/アップロード経路・リソース常駐性・オブジェクト表現・描画アーキの骨格。これらは「触れたら全部書き直し」なので、**規模が小さい今のうちに受け皿 (規約・データ構造・経路) を最新の形で建てる**。中身のチューニングは後でよい。
- **(B) 機能 = 後から層を足せる**: Hi-Z occlusion・mesh shader・HW レイトレ・フル DDGI・仮想テクスチャリング・two-pass occlusion・Nanite 風クラスタ LOD。これらは土台の上に**追加**で乗るので、先送りしても負債にならない (能力ゲート + フォールバックで実装する Roadmap §3 の対象)。

**この文書が扱うのは (A) だけ。** (B) は Roadmap で管理する。

---

## 1. 【最危険】座標精度 / floating origin / camera-relative 描画 — ✅ **受け皿確保 + 全 wire-up 完了 2026-05-29 (E commit 4dc8923 + E clean commit 641abcb)**

### 完了内容 (E clean)
- `include/MyEngine/world/engine_origin.h` 新設: `myengine::world::EngineOrigin::current()` が `glm::vec3(0,0,0)` 固定 (今日) + `toEngineRelative(const glm::mat4&)` (translation column shift) + `toEngineRelative(const glm::vec3&)` (point shift) helper。
- **全 10 site で wire-up**: camera_system (gameplay view/lightView/viewPos) / title_layer (title view/lightView/viewPos) / static_cull_build::emit (prop opaque DrawData + CullObject) / main_pass (skinned characters + bindless test) / shadow_pass (skinned shadow) / reflection_pass (skinned reflection) / pass_chain (grass InstanceData) / static_draw.h (legacy CPU 経路 = reflection cube/model/terrain + main 非 prop + title terrain 全部) / water_pass (WaterMesh world 焼き補正) / particle_pass (per-particle pos) / debug_line_pass (per-vertex CPU shift)。
- 座標系: view_rel = view_world × translate(origin)、 model_rel = translate(-origin) × model_world。 view_rel × model_rel = view_world × model_world。 origin = 0 で完全 numeric no-op (visual 不変)・floating-origin 起動時に全描画が lockstep でシフト。
- types.h FrameUBO.viewPos / DrawData.model / CullObject.centerRadius/coneApexCutoff に「engine-relative」座標系コメント追加。
- **唯一の明示的例外**: TerrainMesh / WaterMesh の VB に world 座標が焼きこまれている。 per-frame は model 行列補正で正しく描画されるが、 km 級 origin shift では VB 内の値自体の float 精度低下が起きる。 真の floating-origin 移行時は chunk 境界で VB を再焼き (or compute pass で動的生成) が必要 — Phase 2F terrain bucket / Water 現代化と統合。
- 危険度: ✅ 解消 (受け皿 + wire-up 完了)。 残るは float double 化 or rebase 本実装で、 規模が要求してから (規模が小さい今は origin=0 のまま無害)。

### 元の症状記述 (参考)

### 症状
world 座標を float で全系統に焼いていると、原点から離れるほど精度が落ちる。float は原点付近は正確だが、原点から 2 のべき乗ごとに精度が半減する。遠方でオブジェクトが「振動」して見え、z-fighting や物理グリッチが出る。数 km 級で確実に顕在化する。(web 確認: Godot LWC ドキュメント / Floating origin (Wikipedia) / UE5 LWC)

### なぜ作り直しか
MyEngine の GPU-driven 設計の心臓が **すべて world-float 前提** の疑いが濃い:
- `CullObject.centerRadius` = world AABB center (world 座標)
- `DrawData.model` = world 行列、push constant の mat4 model も world
- 物理・カリング・Hi-Z の AABB 投影も world 座標
後から「カメラ相対描画」や double 化を入れると、cull / Hi-Z / DrawData / physics / streaming すべてに波及する全面改修になる。**Hi-Z (PART4) 自体もこの精度問題を継承する** (遠方の AABB 画面投影が暴れる)。

### 受け皿 (現行標準・2 択)
- **(a) CPU 側 double + 描画直前に camera-relative float へ変換** (UE5 Large World Coordinates 方式): ゲームロジック/オブジェクト位置を double で持ち、描画時にカメラ基準の float 行列に落とす。UE5 はこれで惑星規模のジッタをほぼ気にしなくてよくなった。
- **(b) floating origin (原点リベース)**: プレイヤーが一定距離動いたら world 全体を平行移動して原点付近に保つ。常に最高の float 精度を得る。実装は「描画直前に全 model 行列からカメラ平行移動を引く」だけでも近似できる。

### 今やること (受け皿だけ・中身は後で)
**「描画はカメラ相対」という規約を今決める。** model 行列を「カメラ平行移動を引いた形」で push constant / DrawData に載せるだけでも、world が小さい今は無害で、後の作り直しを防ぐ。double 化やリベースの本実装は規模が要求してからでよい。重要なのは **world-float をエンジン全体の暗黙の前提にしないこと**。

### 確認用 dump (事実化してから着手)
- `model` 行列の生成箇所とカメラ位置の扱い: `grep` で `model`・`viewPos`・`view *`・push constant への載せ方。`static_cull_build.h` / `static_draw.h` / `triangle.vert` / FrameUniforms。
- 座標の型: 位置が `glm::vec3` (float) か。double を使っている箇所はあるか。
- 物理側の座標前提。

### 危険度: ★★★ (最高)。オープンワールド第一の典型的作り直し。

---

## 2. 【高】スレッド / ジョブシステム + 専用 transfer キュー

### 症状
正本5枚はスレッドに一切触れていない。`ResourceFactory::copyBufferRegion` が内部で `vkQueueWaitIdle` する同期コピー (§5c で確認済み) = **起動時同期アップロード・単一スレッド**構成のはず。Phase 2F のチャンクストリーミングはこれと両立しない (ロード中にフレームがヒッチする)。

### なぜ作り直しか
資料は streaming の前提を「遅延破棄 + buffer VMA 化 + ストリーミング層」と書くが、**スレッドと transfer キューが抜けている** (隠れた前提)。今のまま AssetRegistry / GeometryBuffer.alloc / DeletionQueue が「メインスレッド・drawFrame 同期」前提で固まると、streaming を足すときにスレッド安全性を全体へ後付けする最悪の retrofit になる。

### 受け皿 (現行標準)
web 確認: 専用の transfer/async-compute キューをローダースレッドに割り当て、メインのフレームループから完全に切り離してアップロードすれば、大きなテクスチャに時間がかかってもヒッチしない。IO スレッドが並列キュー越しにメインループと通信し、Fence で完了確認してからリソースをレンダラに接続する。
- (a-1) ✅ **async-compute family 取得済 (PART4 4c-B commit 477985d)**: `vulkan_context::asyncComputeFamily()` / `asyncComputeQueue()` / `hasDedicatedAsyncCompute()` getter 追加。 P620 で family 2 が dedicated async compute として検出 (`[Caps] asyncComputeFamily=2 (dedicated=1)`)。 HZB / cull pass の並列実行受け皿として確保 (実並列化は別 commit)。
- (a-2) ✅ **transfer queue family 取得済 (C commit e7b852e)**: `vulkan_context::transferFamily()` / `transferQueue()` / `hasDedicatedTransfer()` getter 追加。 P620 で **family 1 = dedicated transfer 検出** (TRANSFER bit + GRAPHICS/COMPUTE clear・[Caps] transferFamily=1 (dedicated=1))。 fallback は graphicsFamily に alias で API portability 保持。 実 streaming 利用は Phase 2F で。
- (b) GeometryBuffer.alloc / テクスチャアップロードを **メインスレッド外から呼べる形** (コマンドキュー越し) で設計する。
- (c) ✅ **最小のジョブシステム受け皿確保 (U commit fdbddda)**: `include/MyEngine/core/job_system.h` header-only。 `myengine::core::JobSystem::init(workerCount)` で std::thread N 本起動 + condition_variable + packaged_task queue・`init(0)` で **inert-friendly** (submit が calling thread で inline 実行) なので既存単一スレッド経路は無改修。 Phase 2F streaming 着手時に worker pool を起動するだけで async asset load / decode / chunk eviction が動く。 lock-free 化 / work-stealing は別 commit。

### 今やること
最低でも **transfer queue family の取得 (async compute family は 4c-B で取得済)** と、「アップロードはキュー越しに非同期化できる」という経路の確保。ジョブシステム本体は streaming 着手時でよいが、**同期アップロード・単一スレッドを暗黙の前提にしない**。

### 確認用 dump
- `vulkan_context.cpp` のキュー選択: transfer family を取得しているか (graphics/present だけか)。
- `model_loader` / `texture` のアップロード経路と呼び出しタイミング (起動時のみか)。
- drawFrame ループにスレッド分岐があるか (おそらく無い)。

### 危険度: ★★★ (高)。Phase 2F の隠れ前提。streaming を「層」と呼ぶ前にスレッド土台が要る。

---

## 3. 【中〜高】テクスチャ常駐性 — bindless 1024 固定・退去なし — ✅ **free-list + slot reuse 解消 2026-05-29 (G commit 17d5f8f)**

### 完了内容 (G)
- `BindlessTextureRegistry` に `freeSlots_` std::vector 追加 + `releaseTexture(index)` 公開 API。
- `registerTexture` は freeSlots_ から pop (LIFO 再利用) → 空ければ nextIndex_++。
- `count()` は **live 数** (`nextIndex_ - freeSlots_.size()`) を返すよう変更、 `capacity()` getter で MAX_TEXTURES 露出。
- `PARTIALLY_BOUND_BIT` (既存) で empty slot 安全 (release 後の slot は次の register まで未使用)。
- 残: descriptor pool growth (MAX_TEXTURES 上限突破) は別 Phase = G+ (pool 再作成 + 全 descriptor 再 write が必要)。
- streaming で working set ≤ 1024 ならば実質無限の出入りに耐える。

### 元の症状記述 (参考)


### 症状
bindless は最大 1024 枚固定で「起動時に登録」パターン (Codebase_Guide §2)。streaming 世界はロード/アンロードでスロットを入れ替えるので、スロットの **free-list (解放時に再利用)** と将来の **mip ストリーミング / 仮想テクスチャリング** が要る。1024 固定は `MAX_DRAWS=4096` と同じ「現状で足りる固定値」負債。

### なぜ作り直しか
「起動時に全部登録・解放しない」前提で bindless レジストリを作り込むと、streaming でテクスチャを入れ替えるときにスロット管理を後付けすることになる。§5b で GeometryBuffer に既に適用した「固定容量を置くな」原則が、テクスチャに未適用なだけ。

### 受け皿
bindless レジストリを **動的成長 + free-list** に (解放スロットを再利用)。これは §5b の原則を素直に適用するだけで、あなたが GeometryBuffer (§5c) で既にやったのと同じ発想。仮想テクスチャリングは基本の常駐性 (free-list) の後でよい (B 分類)。

### 確認用 dump
- `BindlessTextureRegistry`: スロット管理が固定配列か・解放 (free) 経路があるか・登録タイミング。

### 危険度: ★★ (中〜高)。MAX_DRAWS=4096 と同じ負債クラス。streaming で顕在化。

---

## 4. 【中〜高】空間分割 / 永続 GPU オブジェクトバッファ — 🟡 **受け皿のみ確保 2026-05-29 (H commit 8604de5)**

### 受け皿状態 (H)
- `include/MyEngine/renderer/persistent_object_buffer.h` header-only design memo を新設。 PersistentObjectBuffer class が reserve/release/markDirty stub + DirtyRange struct + freeSlots_ + nextIndex_。 init()/shutdown() 空関数。
- 詳細な per-Phase activation plan を header comment に記載: (1) CullObject + DrawData の backing store を per-frame staging から persistent device-local buffer に移す、 (2) static_cull_build callers (asset_registry / stage_registry) を registerObject() / releaseObject() に切り替え、 (3) 毎フレ build() を update-dirty pass に置換、 (4) (optional) chunk grid / quadtree / BVH で「近傍 chunk だけ」を cull 入力に。 Foundations §4 / Life is Feudal 四分木方式の reference 設計を memo。
- **実装は 0** = init/shutdown 空・builder 切替なし・PassChain 接続なし。 Phase 2F streaming or H 本実装着手時に埋める。
- 現状 (~67 prop) は毎フレ CPU rebuild が cheap なので未着手で OK。 数万 prop に達する Phase 2F 前に本実装する。

### 元の症状記述 (参考)

### 症状
現状は scene_renderer が **毎フレーム全 opaque static を CPU で走査** して CullObject 配列を作り直し → アップロードしている (PART3c の `static_cull::build` がそれ)。数万オブジェクトでこれをやると、**毎フレーム O(N) の CPU 再構築とアップロード自体が GPU-driven の意義を打ち消す** (CPU がボトルネックに戻る)。

### なぜ作り直しか
「毎フレーム CPU rebuild」を前提に cull / Hi-Z / DrawData を作り込むと、規模が来たとき **永続 GPU 常駐バッファ + 空間構造** へ移すのに cull 入力経路を全面改修することになる。web 確認: オープンワールド事例 (Life is Feudal) は森全体を四分木でセルに分割し、各セルを木々のバウンディングボリュームとして使う (= 近傍セルだけ触る)。

### 受け皿
- オブジェクトを **変化時だけ更新する GPU 常駐バッファ** にする (毎フレーム全 rebuild をやめる方向)。
- 粗い **セル単位の前段カリング** (グリッド/四分木/BVH) で「近傍だけ」を GPU cull に渡す。
- これは Hi-Z の CullObject 配列と直結するので、**PART4 で「毎フレーム全 rebuild」を固定設計にしない** (将来 GPU 常駐へ移せる形に意識する)。

### 確認用 dump
- `scene_renderer` の buildSceneData / `static_cull::build` の呼び出し頻度 (毎フレーム全走査か)。
- CullObject / DrawData の更新が「変化時だけ」にできる構造か。

### 危険度: ★★ (中〜高)。GPU-driven の規模適性そのもの。数万で CPU rebuild が壁。

---

## 5. 【中・PART4 に今すぐ織り込める】描画アーキ — depth-normal prepass の共有 — ✅ **解消済 2026-05-28 commit ed0d80e (PART4 4a-2)**

### 症状 (元の状態 — 4a-2 で解消)
現状は forward (main_pass が static/skinned/grass/bindless の opaque + transparent を描く)。Phase 2A で clustered forward+ は計画済み (forward の長所=透明/MSAA 親和を保てるので妥当な選択)。だが Phase 3 (SSAO/SSGI/SSR/GI) は **深度+法線の prepass / 軽量 GBuffer を共有前提にする** のが定番。web 確認: forward で反射を正しく解くには深度プリパスと小さな GBuffer (最低でも法線+深度) を用意し前フレーム結果を再投影する。SSAO も深度バッファへのアクセスが要る。

### なぜ (緩い) 作り直しか (元の懸念)
各スクリーンスペース系フェーズ (SSAO/SSGI/SSR/Hi-Z) が **個別に深度/法線パスを後付け** すると、同じ情報を何度も作る/各フェーズで配管が散らかる。1 枚の prepass を共有する設計にしておけば後が楽。forward+ 自体も深度プリパスを要するので、prepass は遅かれ早かれ要る。

### 解消の中身 (PART4 4a-2 で実装 = commit ed0d80e)
- **main_pass の opaque を 3 attachment MRT に拡張**: HDR color (既存) + **GBuffer normal (R10G10B10A2 octahedral)** + **motion vector RT (RG16F NDC ΔXY)**。 後者は §3.4-S Motion Vector RT (Phase 3 TAA/TSR/FSR/DLSS 受け皿)。
- **swapchain depth に SAMPLED_BIT を追加** + main_pass 後に `DEPTH_READ_ONLY_OPTIMAL` (separate / combined フォールバック) に遷移。 → 4b HZB compute, Phase 3 SSAO/SSGI/SSR/DoF が同じ深度を sample できる。
- **FrameUBO に `mat4 prevViewProj`** = Phase 3 TAA を載せたとき shader 改修ゼロ。
- **shared/gbuffer.glsl** に octahedral encode + motion vector helper を新設、 4 opaque fragment shader (triangle / triangle_skinned / triangle_bindless / grass_instanced) から呼ぶ。
- main_pass は VkRenderPass を撤去し Vulkan 1.3 dynamic rendering 化 (§3.4-T 4a-1 で繰り上げ実装)、 HUD/ImGui は **OverlayPass** に分離 (GBuffer 受け皿の attachment scope を完全独立) で feedback-loop hazard 完全排除。
- forward+ 路線は維持 (deferred 化はしていない・OIT は据え置き、 §6 参照)。

### 残り懸念 (Phase 3 着手時に確認)
- normal RT の B/A 12bit は将来の material id / roughness 余裕として確保中 (未充填)。
- motion vector は per-object prevModel まで持っていない (FrameUBO の prevViewProj 経由・shader interface だけ準備済)、 Phase 3 で DrawData に prevModel を足せば shader 改修ゼロで届く受け皿。

---

## 6. 【低〜中・後段でよい】その他の留意点 (作り直しリスクは小さい)

- **透明の OIT / GPU ソート**: 現状 forward 透明 + CPU painter ソート。大量の foliage/particle で order-independent transparency が欲しくなるが、後から足せる (追加機能)。低リスク。
- **決定論 / 固定タイムステップ / physics-render 分離**: 大規模 + ネットワークなら効くが、グラフィックス土台とは別軸。座標精度 (項目 1) と物理 broadphase に絡む程度。
- **VRAM 予算の可視化** (`VK_EXT_memory_budget`): streaming で 2GB をやりくりするなら欲しい計器 (Roadmap §6 既出)。土台というより計測。

---

## 7. 優先度まとめと進め方

**先回りすべき (受け皿が要る・後から入れると全系統作り直し)**:
1. ✅ ~~★★★ 座標精度 / camera-relative 描画規約~~ — **受け皿確保 + 全 10 site wire-up 完了 (E commit 4dc8923 + E clean commit 641abcb)**: `world/engine_origin.h` の `EngineOrigin::current()` + `toEngineRelative` helper を camera_system / title_layer / static_cull_build (prop) / main_pass (skinned) / shadow_pass (skinned shadow) / reflection_pass (skinned reflection) / pass_chain (grass InstanceData) / static_draw.h (legacy CPU 経路 = reflection cube/model/terrain + main 非 prop + title terrain) / water_pass / particle_pass / debug_line_pass で適用。 origin = 0 で完全 numeric no-op (visual 不変)・floating-origin 起動時に全描画が lockstep でシフト。
2. ✅ ~~★★★ スレッド / transfer キュー (Phase 2F streaming の隠れ前提)~~ — **両方解消**: C (commit e7b852e) で transfer queue family + queue 取得 + getter、 U (commit fdbddda) で JobSystem header-only worker pool 受け皿 (inert-friendly)。 §2 詳細参照。
3. ✅ ~~★★ bindless 動的化 + free-list~~ — **G commit 17d5f8f で free-list + slot reuse 解消**。 streaming で working set ≤ MAX_TEXTURES=1024 なら無限。 descriptor pool growth は別 Phase = G+。
4. 🟡 ★★ 永続 GPU オブジェクト + 空間分割 (毎フレーム CPU rebuild の脱却) — **H commit 8604de5 で受け皿のみ (`persistent_object_buffer.h` design memo header-only)・本実装は Phase 仕事**。
5. ✅ ~~depth-normal prepass 共有~~ — **解消済 (PART4 4a-2 commit ed0d80e)**: MRT 3 attachment + 深度 SAMPLED + 前フレーム MVP + OverlayPass 分離。 §5 参照。

**後から足せる (先送りで負債にならない・B 分類)**: Hi-Z / mesh shader / HW RT / フル DDGI / 仮想テクスチャリング / two-pass occlusion / クラスタ LOD。

### 推奨する進め方
1. **まず 1 と 2 を実ソース dump で確認** (推測を事実化する・§1-1)。座標が world-float 前提か / transfer キューを取得しているか。**これらは「確認 → 事実」にするまで着手判断しない。**
2. 確認結果に応じて、この監査を踏まえた **土台 side の項目を Roadmap / 依存マップに反映** (Phase 0 級の土台タスクとして位置づけ)。
3. ~~PART4 設計書 (rev.2) の 4a に「depth-normal prepass 共有 (項目 5)」を追記~~ — **完了 (2026-05-28, PART4 §6 4a-2 commit ed0d80e)**: rev.7 で実装内容に書き換え済 (motion vector + dynamic rendering + OverlayPass まで含む)。
4. 1 (座標精度) は規模が小さい今こそ規約だけ先に決める (camera-relative model 行列)。本実装は規模が要求してから。

### この文書の更新運用 (§6)
土台項目に着手・確認するたびに、この監査の該当項目を「確認済み (実態 = …)」「受け皿実装済み (commit …)」へ更新する。正本5枚と同じく保存 → 次回添付。確定した土台規約 (例: camera-relative 描画) は Work_Protocol / Codebase_Guide にも明文化する。

---

## 8. 実態確認済みの既存負債 (実ソース棚卸し・2026-05-27 / DEBT-1〜5)

> §1〜§5 が「土台の作り直し負債 (大半は資料の沈黙からの推測)」だったのに対し、この §8 は **実ソースを dump して行番号付きで確認した、今すでにコードに存在する負債**。DEBT-1〜5 の grep 結果に基づく (推測でなく事実)。資料が自己申告していた負債はほぼ実在を確認し、資料未記載の新規負債も拾った。

### 8.1 固定容量の一族 (DEBT-1) — §3 の bindless 負債は「一族」だった
`MAX_DRAWS=4096` 単独でなく、**「現状で足りる固定上限」が複数クラスに散在**。すべて §5b 原則 (固定容量を現状基準で決めない) の未適用箇所。大規模化で順に壁になる:
- `bindless_texture_registry.h:43` `MAX_TEXTURES = 1024` (§3 で挙げた通り実在)
- `culling_pass.h:43` `MAX_DRAWS = 4096` (PART4 で動的化予定・PART4 §0.1-B)
- `draw_data_pool.h:37` `MAX_DRAWS = CullingPass::MAX_DRAWS` (同根・PART4 で連動)
- `instance_buffer_pool.h:37` `MAX_INSTANCES = 8192` (草インスタンス) — ✅ **F2 commit 0f07dc0 で動的化** (INITIAL_CAPACITY + peakRequested_ + 次フレ growToFitPeak + DeletionQueue 経由旧 buffer 安全破棄)
- `material_registry.h:33` `MAX_MATERIALS = 256` — ✅ **F1 commit c3f46ea で動的化** (add() overflow で doubling + DeletionQueue)
- `skin_buffer_pool.h` `MAX_ENTITIES` — ✅ **F3 commit 659bece で動的化** (slot 安定性維持 = boneOffset 不変・grow で freeSlots 拡張)
- `particle_pass.h:33` `kMaxParticlesPerFrame=2048` — ✅ **F4 commit 132d0d5 で動的化** (execute 内 alive count → growToFit)
- `debug_line_pass.h:42` `kMaxVerticesPerFrame=10000` — ✅ **F5 commit a46f208 で動的化** (line+tri VB pair grow)
- ✅ **5/6 解消**。 残: `bindless_texture_registry.h:43 MAX_TEXTURES=1024` は G commit 17d5f8f で free-list + slot reuse 解消・descriptor pool growth は別 Phase = G+ (working set ≤ 1024 なら無限の出入りに耐える)。

**重要 — 解消後の手本がエンジン内に既にある**: `geometry_buffer.h:66-67` の `DEFAULT_BLOCK_VERTICES/INDICES` は「初期値であって上限でない」動的成長 (§5c の正しい形)。つまりこの一族の解消は**新設計の発明でなく「GeometryBuffer に揃える」作業**＝低リスク。 → ✅ 上記の通り完了。

### 8.2 生メモリ経路の残存 (DEBT-2) — ✅ **完全解消 2026-05-29 (A1-A5 commits 995b779 / 46cb937 / 185ac09 / 80ccb76 / a030372)**
- **A1 (995b779) Mesh**: 旧 `loadFromObj` (0 callers) + `uploadBuffer` 経路 + private hybrid VkBuffer/Memory 4 メンバ完全削除。 `createCube`/`createCrossQuad` は geom 必須化。 tiny_obj_loader.h include 撤去。
- **A2 (46cb937) TerrainMesh**: legacy private buffer (VkBuffer + VkDeviceMemory ペア) を `VmaBuffer × 2` に置換。 `uploadBuffer()` body は `createMappedHostVisible` (staging) + `createDeviceLocal` (device-local) + `copyBufferRegion`。
- **A3 (185ac09) SubMesh + ModelLoader**: SubMesh から旧 private buffer 4 メンバ削除。 ModelLoader::uploadBuffer 関数撤去。 uploadSubMeshToGpu は GeometryBuffer& only に。
- **A4 (80ccb76) Texture staging**: 旧 `vkMapMemory + memcpy + vkUnmapMemory + vkFreeMemory + vkDestroyBuffer` 配管を `VmaBuffer::createMappedHostVisible` + `memcpy direct to staging.mapped()` に置換。
- **A5 (a030372) ResourceFactory legacy API 全削除**: `createBuffer` / `createBufferVMA` (caller ゼロ dead code 確認) / `findMemoryType` を 3 つとも削除。 `[[deprecated]]` 期限切れメッセージも撤去。 ResourceFactory は **transient command pool + 4 one-time submit helper (copyBuffer / copyBufferRegion / copyBufferToImage / transitionImageLayout) のみ**になった。
- **エンジン内 生 `vkAllocateMemory` / `vkBindBufferMemory` / `vkMapMemory` / `vkUnmapMemory` / `vkFreeMemory` ゼロ達成** (実コード grep: resource_factory.cpp 内のコメント記述 1 件のみ・debug_line_pass.cpp の comment 内記述 1 件のみ)。 「メモリは全部 VMA」が buffer 側でも完成 (image 側は 2026-05-25 完了済)。

### 8.3 一時ログ・stopgap (DEBT-3) — ✅ **A6 commit dab4faf で解消**
- ✅ `pass_chain.cpp` `[BlockDbg]` (`s_blockDbg<3`) 撤去
- ✅ `title_layer.cpp:92` `static int s_dbg` + `[DEBUG]` 残骸ログ撤去
- ℹ️ `triangle.frag:57` `PBR_NORMAL_TEST` は audit 時点では既に解消済を確認 (実コード grep ヒットゼロ・過去 commit で対応済)
- ℹ️ `engine_app.cpp:154` WorldTerrain/WorldWater stopgap (遅延破棄 = ①意図的据え置き・現状維持)
- ℹ️ `main_pass.cpp` terrain "for now" legacy (Phase 2F 待ち = ①意図的据え置き)
- その他の「デバッグ」ヒットは大半が**恒久的なデバッグ機能** (F3 オーバーレイ / デバッグライン / F6-F12 キー / `skeleton.cpp:102` ボーン名出力=「恒久的に残す」と明記) で負債でない。

### 8.4 関数内 static (DEBT-4) — §2 (スレッド化) の地雷 + デバッグ残骸
マルチスレッド化すると競合する**隠れた永続状態**。§2 の transfer キュー/ジョブシステム着手時に整理が要る:
- `pass_chain.cpp:407` `static std::vector<InstancedMeshDrawItem> grassDraw` (描画フレームの static バッファ・スレッド非安全) + `:342` `s_blockDbg`
- `chest_system.cpp` の `s_idx` が**5箇所** (52/73/97/118/140)、`spirit_system.cpp:85`/`spawn_system.cpp:24`/`gameplay_layer.cpp:136`/`world_builder.cpp:21 s_onRemoveRegistered` にも散在。単一スレッド・単一インスタンスでは動くが、**複数インスタンス化/スレッド化で破綻する芽** (③、§2 と統合)。
- **新規発見**: `title_layer.cpp:92` `static int s_dbg` が `[DEBUG] TitleLayer...` を3回出す**残骸ログ** → 削除すべき (②)。

### 8.5 const_cast / private buffer ハイブリッド残置 (DEBT-5)
- **const_cast 5箇所**。`swapchain.cpp:221`・`texture.cpp:118` は `const VulkanContext*` を剥がす VmaImage 移行の既知パターン (資料記載済み・設計上やむなし)。`key_config_layer.cpp:110` は KeyMapping の getter 都合。**新規指摘**: `pass_chain.cpp:264/413` の `const_cast<Mesh*>(&assets->defaultMesh()/grassMesh())` は **AssetRegistry の getter が const を返すのに可変が要る = const 設計の不一致**で、歪みの芽 (③ で getter 設計を見直す候補)。
- **Mesh/Model/TerrainMesh の private buffer ハイブリッド残置を確認** (`mesh.h:95-98`、`model.h:25-28`、`terrain_mesh.h:65-68`)。geom 経路が本線だが各メッシュが**使われない private `VkUnique<VkBuffer>` フィールドを依然保持** (`mesh.h:103`「legacy private buffers stay empty」)。二重持ちの死にフィールド (③、geom 完全移行後に撤去)。`water_mesh.h:42-43` は VmaBuffer 済み (正しい手本)。

### 8.6 分類 (①意図的据え置き / ②今すぐ安く / ③将来 Phase)
- **① 意図的据え置き (負債でなく仕様)**: シャドウ swimming、planar reflection、engine_app stopgap (遅延破棄待ち)、terrain legacy (Phase 2F 待ち)、恒久デバッグ機能群。
- ✅ **② 今すぐ安く直せる**: `title_layer.cpp:92` `[DEBUG]` 残骸ログ削除 = A6 解消・`triangle.frag:57` `PBR_NORMAL_TEST` = 既に解消済 (確認) ・`createBufferVMA` 未使用確定→削除 = A5 解消・`[[deprecated]]` 期限切れメッセージ更新 = A5 (header 自体ごと削除) で解消。
- **③ 将来 Phase で計画的に解消 (土台 side・本監査の §1〜§5 と統合)**:
  - ✅ 固定容量の一族 (8.1) を GeometryBuffer 式の動的成長へ = **F1-F5 + G で 6/6 解消**。
  - ✅ buffer 系 VMA 化 4ファイル (8.2) → `createBuffer`+`vkAllocateMemory` 撲滅 (VmaImage の前例通り) = **A1-A5 で完全解消・エンジン内 生 vkAllocateMemory ゼロ達成**。
  - ✅ private buffer ハイブリッド撤去 (8.5・geom 完全移行後) = A1 (Mesh) + A3 (SubMesh) で完全削除。 残: TerrainMesh は legacy private buffer を VmaBuffer 化 (A2) で残置 (Phase 2F まで terrain は別 bucket 方針)。
  - 関数内 static のスレッド非安全分 (8.4) → §2 ジョブシステム着手時に整理 = **U 受け皿 (JobSystem) は確保済・実 worker 起動 (init(N))・実 static 整理は Phase 2F streaming 着手時**。
  - AssetRegistry getter の const 不一致 (8.5・const_cast 解消) = 未着手 (③ 据え置き)。

### 8.7 総括 (この棚卸しから言えること)
1. **「解消後の手本」が既にエンジン内にある**: GeometryBuffer (動的成長)・VmaImage (生メモリ撲滅)・water_mesh (VmaBuffer) は、固定容量/生メモリ/ハイブリッドを解消した後の姿そのもの。③群は「新設計の発明」でなく「既存の手本に揃える」低リスク作業。
2. **固定容量負債は §1 (座標) §2 (スレッド) より軽い**: 固定値は「超えたら溢れる」が**設計を作り直さず層を足せる** (GeometryBuffer がそうだった)。優先度は依然 **§1 座標精度 > §2 スレッド > 固定容量群 (8.1)**。
3. **新規発見 (資料未記載) のまとめ**: `[[deprecated]]` 期限切れ (8.2)、`title_layer` `[DEBUG]` 残骸 (8.4)、`triangle.frag` 一時 bump (8.3)、`pass_chain` const_cast 不一致 (8.5)、ゲームロジックの static カウンタ散在 (8.4)。いずれも小〜中。

### 8.8 追加探索 DEBT-6〜10 (同期/エラー処理/ハードコード/肥大/shader二重定義/recreate・2026-05-27)

> DEBT-1〜5 が触れていない領域 (同期の重さ・エラー握り潰し・ハードコード・god オブジェクト・shader/C++ 二重定義・recreate 漏れ) を実 dump で確認。**結論: 大きな新規負債は出ず、基礎品質はむしろ健全。** 残る負債は DEBT-1〜5 + 土台 §1〜§5 に集約と判断 (探索は逓減)。以下は「健全と確認できた点」と「拾えた小負債」の両方を事実として残す。

**健全と確認できた点 (負債でない・安心材料として記録):**
- **同期 (DEBT-6)**: `vkQueueWaitIdle`/`vkDeviceWaitIdle` は **drawFrame 内に1つもない**。`frame_sync.cpp:131` は per-frame fence の `vkWaitForFences` (正しいフレーム同期)。他の `vkDeviceWaitIdle` は shutdown/recreate/world 切替 (engine_app:157・swapchain:113・vulkan_renderer:69/188・world_builder:123) = フレームループ外の正しい使い方。`resource_factory.cpp:125 vkQueueWaitIdle` は起動時コピー同期 (§5c 既知・監査§2 の transfer キュー化対象と同一・新規でない)。
- **エラー処理 (DEBT-7)**: `assert`/`abort`/`std::exit`/`std::terminate` は **ゼロ** (例外で投げる設計に統一)。`catch(...)` 5箇所のうち **shadow_pass.cpp:213/244 は握り潰しでなく正しいクリーンアップ** = `vkDestroyShaderModule` でモジュール解放 → `throw;` で再送出 (リーク防止 + 失敗を上に伝える RAII 風)。
- **shader/C++ 二重定義 (DEBT-10)**: GLSL の手書き `struct` は **`cull.comp:28 DrawCmd` のみ** (`VkDrawIndexedIndirectCommand` 対応・既知の意図的二重定義)。他の全 shader は `buffer_reference buffer` か `push_constant` で **types.h を #include して共有** (C++/GLSL 共有 types.h 設計が効いている)。不一致リスクは最小。
- **recreate (DEBT-10)**: `vulkan_renderer.cpp:66 recreateSwapchain` → `pass_chain.onSwapchainResized` → main/bloom/post が連鎖 (bloom mip も再生成と vulkan_renderer:73 が明記)。漏れの気配なし。

**拾えた小〜中の新規負債 (記録対象・「大きくなった後の謎エラー」を防ぐため小さくても残す):**
- **(DEBT-8) far=200 / near=0.1 / FOV45° の直書き** (`vulkan_renderer.cpp:101` `ubo.cameraParams = vec4(0.1f, 200.f, radians(45.f), aspect)`)。1箇所集約なのは良いが、**far=200 は「大規模オープンワールド」と矛盾しうる** (広域・遠景を描くには近すぎる)。監査§1 (座標精度) と関連 = 遠方を扱うときに near/far 再設計が要る。**分類②〜③** (今は設定化、広域描画時に再検討)。
- **(DEBT-7) `model_loader.cpp:257/281/301` の `catch(...)` 3箇所は中身未確認**。shadow_pass が `vkDestroyShaderModule→throw` の正しい形だったので同型 (クリーンアップ catch) の可能性が高いが、**断定しない** (§1-1)。もしログ無しで握り潰しているなら「アセットロード失敗が無言で消える」負債 = 大規模アセットで「なぜか出ない/落ちる」謎エラーの温床。**着手時に中身を確認し、無言握り潰しなら最低限ログを足す。分類②** (確認は安い)。
- **(DEBT-9) `gameplay_layer.cpp` 946 行の肥大** (突出1位・次は main_pass 697)。ただし**ゲームロジック層でグラフィックスでない** (敵/戦闘/ピックアップ/デバッグ描画の寄せ集め)。`components.h:366` は ECS component 集約で自然。main_pass 697・pass_chain 519 は描画司令塔として許容範囲。**分類③** (将来分割候補・グラフィックス土台の阻害ではない)。
- **(DEBT-10/PART4 宿題) HiZPass を recreate 連鎖に入れる**。CullingPass のバッファは解像度非依存で onSwapchainResized 不要だが、**HiZPyramid は解像度依存なので PART4 で `pass_chain.onSwapchainResized` の連鎖に追加必須** (入れ忘れると resize 後に古い解像度のピラミッドを読む)。負債というより**実装時の忘れ防止メモ** → PART4 設計書 §3 (pass_chain 配線) / §6 4b に反映する。

**8.8 の総括**: 同期・エラー・shader 共有・recreate は健全で、エンジンの基礎品質は高い。**負債探索はここで打ち止めが妥当** (これ以上の網羅 dump は逓減)。残課題は既出の DEBT-1〜5 (固定容量・生メモリ・隠れ static 等) と土台 §1〜§5 (座標・スレッド等)、加えて上記の小負債4件。次は新たな負債探索でなく、既出負債の解消 (②今すぐ安く / ③将来 Phase) か Hi-Z PART4 本体に進む段階。

### 8.9 DEBT-11 — 既存日本語コメントの英語化 (2026-05-28 追加)

**背景**: 2026-05-28 に「ソース内コメントは英語で書く」ルールを Work_Protocol §2 / START_HERE §4 に明文化。新規・変更箇所は英語で書くが、**既存の日本語コメントは src/ 配下に広く残存** (実測: 行頭 `// 日本語` パターンが少なくとも 45 ヒット / 10 ファイル超。これは部分計測なので実数はもっと多い)。

**順次英語化方針 (スコープを勝手に広げない §0-5)**:
- 各ファイルを次に編集する機会に、**その PR/commit でついでに翻訳する**。
- 一括翻訳の専用 commit は作らない (大量変更は diff レビュー困難・他作業の git blame を汚す)。
- 触らないファイルの日本語コメントは残置。実害なし。

**例外 (翻訳しない)**:
- 資料ファイル (本ドキュメント・正本5枚・side/ 配下) は日本語のまま。
- 文字化け修復 (Work_Protocol §2-1) の対象になっている壊れたコメントは、修復時に英語で書き直す。

**分類**: ③将来 Phase で順次解消 (一括ではない・触る機会に)。リスク=低 (純粋な翻訳・挙動不変)。優先度=低 (コードの動作・設計に影響しない・grep 可読性も英語化で改善する程度)。

### 8.10 DEBT-12 — §1.5-B 違反: CPU draw ループ残存 (看板=最新 GPU-driven / 実体=旧経路) (2026-05-30 全監査で確定)

**契機**: user 指示「最新技術の導入より既存整合・最小構成を選んでしまったかもしれないので、セッション最初から確認して」。これを受け描画経路を実ソースで全監査した結果、§1.5-B「すべての描画 (static prop / terrain / 将来は skinned も) を最終的に compute カリング + indirect draw に乗せる。CPU draw ループが残るものは無くす」に対する違反 (= 看板倒れ) を行番号付きで確定した。

**実態 (実ソース確認・GPU-driven indirect になっているのは prop / shadow static / particle のみ)**:
- 🔴 **skinned (全経路 CPU draw ループ)**: opaque = `main_pass.cpp drawSkinnedList` (CPU for + push constant + `vkCmdDrawIndexed`) / transparent = 同 / shadow = `shadow_pass.cpp` skinned ループ (static shadow は indirect 化済みなのに skinned だけ legacy) / reflection = `reflection_pass.cpp drawSkinnedList`。加えて頂点 skinning を `triangle_skinned.vert` / `shadow_skinned.vert` の **vertex shader で 3〜4 回再実行** (shadow + main + reflection)。→ **Phase 2G で解消** (compute skinning packed VB + prop と同じ indirect 統合 + 旧 vertex-skinning 撤去)。
- 🔴 **grass (CPU per-blade frustum cull)**: `pass_chain.cpp` で全 blade に CPU `fr.sphereVisible()` → 可視のみ InstanceBufferPool に詰めて GPU instancing。カリングが CPU。→ **Phase 2H で解消** (GPU compute cull + indirect instanced)。
- 🟡 **後段でよい CPU ループ**: water (`water_pass.cpp` 専用パス) / bindless test cube (`main_pass.cpp` 単発・テスト用) / reflection static (`reflection_pass.cpp` 静的・別錐台) / transparent static (OIT Phase 待ち)。これらは §1.5-B 観点で残存だが、規模負債としては小〜中。
- ✅ **準拠済み**: opaque static prop (compactCmd 経由 indirect) / shadow static (`vkCmdDrawIndexedIndirectCount`) / particle (count-driven instancing)。

**受け皿の誤設計 (関連)**: `gpu_skinning.h` (S) は **per-mesh 固定 skinnedVB = 複数インスタンス (敵複数体が別ポーズ) 非対応**。§0「受け皿は最新の形で」に反する誤設計。Phase 2G 着手時に **per-instance pool + indirect 統合射程**へ再設計する。他 9 受け皿 (H/V/R/X/Y/P/M/U/Z) は形健全を確認済み。

**この監査で判明した運用上の教訓 (Claude 側の §0 違反)**: 監査の過程で、Claude が選択肢提示時に「最新技術より既存整合・最小構成・安全」を**推奨**として出した §0 違反が 2 件あった (CPU skinning fallback の温存を §3 能力フォールバックの誤用で正当化・skinned VB のフル Vertex 維持を「安全・差分最小」と化粧)。§3 のフォールバックは **HW 非対応時の二択**であって、全 GPU で動く機能 (compute skinning) の旧経路温存の口実にしてはならない。**「妥協案を最新の顔で推奨しない」** を選択肢提示時の自己チェックに加える。

**分類**: ③将来 Phase で計画解消 (2G = ★次推奨 / 2H)。これは「後から作り直し」ではなく「看板と実体を一致させる最新化」= §0 最優先原則の直接適用。
