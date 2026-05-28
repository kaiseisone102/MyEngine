# MyEngine オープンワールド土台監査 — 先回りすべき受け皿 / 先送り負債 (rev.5)

最終更新: 2026-05-28 (rev.5: §5 描画アーキ depth-normal prepass の共有 = ✅ **解消済** (PART4 4a-2 commit ed0d80e)。 MRT 3 attachment (HDR + Normal R10G10B10A2 octahedral + Motion vector RG16F) + 深度 SAMPLED + 前フレーム MVP + OverlayPass 分離 = Phase 3 SSAO/SSGI/SSR/DoF/TAA の受け皿が全部立った。 §7 優先度サマリも該当項目を「解消済」に書換。 / rev.4: §8.9 DEBT-11「既存日本語コメントの英語化」を追加。Work_Protocol rev.15 / START_HERE で「ソース内コメントは英語」ルールが明文化されたのを受け、既存の日本語コメント (src/ 配下に >45 件 / 10 ファイル超) を**順次英語化** (一括 commit でなく触る機会に翻訳) とする方針を記録。リスク低・優先度低 / rev.3: §8.8「DEBT-6〜10 の追加探索 (同期/エラー処理/ハードコード/肥大ファイル/shader 二重定義/recreate)」を追記。**大きな新規負債は出ず、基礎品質はむしろ健全と確認** (drawFrame 内に WaitIdle なし・assert/abort ゼロ・例外統一・types.h 共有で shader 二重定義は最小・recreate 連鎖に漏れの気配なし)。小〜中の新規負債のみ拾った: far=200 直書き (大規模と矛盾しうる)・catch(...) の中身確認 (shadow_pass は正しいクリーンアップ+再throw / model_loader 3箇所は未確認)・gameplay_layer.cpp 946 行の肥大 (非グラフィックス)・HiZPass を recreate 連鎖に入れる宿題。**負債探索は逓減と判断 = 主要負債は DEBT-1〜5 + 土台§1〜§5 で出尽くし** / rev.2: §8「実態確認済みの既存負債 (実ソース棚卸し)」を新設。DEBT-1〜5 の dump で §1〜§5 の推測のうち実体のあるものを行番号付きで事実化し、資料未記載の新規負債も追加。固定容量は一族で散在だが GeometryBuffer/VmaImage/water_mesh に「解消後の手本」が既に存在することを確認。負債を ①意図的据え置き ②今すぐ安く直せる ③将来 Phase で計画解消 に分類 / rev.1: 「オープンワールドエンジン全体として、後から入れると全系統作り直しになる土台負債」を web 調査 (座標精度/floating origin・スレッド/transfer キュー・bindless 常駐性・空間分割/永続オブジェクト・描画アーキ forward+/GBuffer) で洗い出し新設。各項目に「なぜ作り直しか」「受け皿」「確認用 dump」「危険度」を付す) / 対象: MyEngine を大規模オープンワールド (複数光源・複数地面・数千〜数万オブジェクト・広域) として成立させるための土台判断。正本5枚 (START_HERE / Roadmap / 依存マップ / Codebase_Guide / Work_Protocol) + PART4 設計書と対で読む

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

## 1. 【最危険】座標精度 / floating origin / camera-relative 描画

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
- (a) `vulkan_context` で **transfer queue family を今取得**しておく (現状は graphics + present のみの疑い)。
- (b) GeometryBuffer.alloc / テクスチャアップロードを **メインスレッド外から呼べる形** (コマンドキュー越し) で設計する。
- (c) 最小のジョブシステム / スレッドプール (async asset loading + parallel command recording の土台)。

### 今やること
最低でも transfer queue family の取得と、「アップロードはキュー越しに非同期化できる」という経路の確保。ジョブシステム本体は streaming 着手時でよいが、**同期アップロード・単一スレッドを暗黙の前提にしない**。

### 確認用 dump
- `vulkan_context.cpp` のキュー選択: transfer family を取得しているか (graphics/present だけか)。
- `model_loader` / `texture` のアップロード経路と呼び出しタイミング (起動時のみか)。
- drawFrame ループにスレッド分岐があるか (おそらく無い)。

### 危険度: ★★★ (高)。Phase 2F の隠れ前提。streaming を「層」と呼ぶ前にスレッド土台が要る。

---

## 3. 【中〜高】テクスチャ常駐性 — bindless 1024 固定・退去なし

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

## 4. 【中〜高】空間分割 / 永続 GPU オブジェクトバッファ

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
1. ★★★ 座標精度 / camera-relative 描画規約 (オープンワールド第一の地雷)
2. ★★★ スレッド / transfer キュー (Phase 2F streaming の隠れ前提)
3. ★★ bindless 動的化 + free-list (MAX_DRAWS と同じ負債クラス)
4. ★★ 永続 GPU オブジェクト + 空間分割 (毎フレーム CPU rebuild の脱却)
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
- `instance_buffer_pool.h:37` `MAX_INSTANCES = 8192` (草インスタンス)
- `material_registry.h:33` `MAX_MATERIALS = 256`
- `skin_buffer_pool.h` `MAX_ENTITIES` / `particle_pass.h:33` `kMaxParticlesPerFrame=2048` / `debug_line_pass.h:42` `kMaxVerticesPerFrame=10000`

**重要 — 解消後の手本がエンジン内に既にある**: `geometry_buffer.h:66-67` の `DEFAULT_BLOCK_VERTICES/INDICES` は「初期値であって上限でない」動的成長 (§5c の正しい形)。つまりこの一族の解消は**新設計の発明でなく「GeometryBuffer に揃える」作業**＝低リスク。

### 8.2 生メモリ経路の残存 (DEBT-2) — 範囲が4ファイルに確定
`ResourceFactory::createBuffer` (生 `vkAllocateMemory`+`vkMapMemory`+`vkFreeMemory`) の利用者を確定:
- `mesh.cpp:245/257`、`model_loader.cpp:174/186`、`terrain_mesh.cpp:354/366`、`texture.cpp:104` (staging)
- image は VMA 済み (`vma_image.cpp`)。buffer 側だけ残土台 (依存マップ §1・Codebase_Guide §1 の記述と一致)。
- **新規発見**: `resource_factory.h:38` の `[[deprecated]]` メッセージが **"Will be removed after Phase 1B migration"** ── Phase 1B は完了済みなのに削除されず、**期限切れの deprecated** が残置。
- **`createBufferVMA` (resource_factory.cpp:221) は呼び出し元が grep に出ず、未使用デッドコードの濃厚な疑い** (資料の「要精査」を裏付け)。buffer 系 VMA 化のとき VmaBuffer ファクトリで足りるか判断し、不要なら削除。

### 8.3 一時ログ・stopgap (DEBT-3) — 想定より健全
- `pass_chain.cpp:342-351` `[BlockDbg]` (`s_blockDbg<3` で3回だけ・PART4 で掃除予定・PART4 §1⑦)
- `culling_pass.h:21` の `[Cull2B]` は**コメントのみ。実際の出力行は grep に出ず** = 既に消えている可能性大 (PART4 4d で最終確認)。
- `engine_app.cpp:154` WorldTerrain/WorldWater **stopgap** (資料通り・遅延破棄まで据え置き = ①)
- `main_pass.cpp:461` terrain "for now" legacy (Phase 2F 待ち = ①)
- **新規発見**: `triangle.frag:57` `PBR_NORMAL_TEST: temporary procedural bump` ── シェーダに検証用の一時 bump が残置。`#ifdef` 等でガードされているか要確認、剥き出しなら撤去 (②)。
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
- **② 今すぐ安く直せる (小さく独立・リスク低)**: `title_layer.cpp:92` `[DEBUG]` 残骸ログ削除 / `triangle.frag:57` `PBR_NORMAL_TEST` のガード確認・撤去 / `createBufferVMA` 未使用確定→削除 + `[[deprecated]]` 期限切れメッセージ更新。
- **③ 将来 Phase で計画的に解消 (土台 side・本監査の §1〜§5 と統合)**:
  - 固定容量の一族 (8.1) を GeometryBuffer 式の動的成長へ (§3 bindless と統合・**手本がエンジン内にある**)。
  - buffer 系 VMA 化 4ファイル (8.2) → `createBuffer`+`vkAllocateMemory` 撲滅 (VmaImage の前例通り)。
  - private buffer ハイブリッド撤去 (8.5・geom 完全移行後)。
  - 関数内 static のスレッド非安全分 (8.4) → §2 ジョブシステム着手時に整理。
  - AssetRegistry getter の const 不一致 (8.5・const_cast 解消)。

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
