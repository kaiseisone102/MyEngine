# MyEngine グラフィックス最新技術ロードマップ (2026 再構成版 rev.12)

最終更新: 2026-05-29 (rev.12: **PART4 §6 4d「Pure GPU-driven cleanup = 完了」追加反映 (commit f8d1e1f)**。 user 報告「HUD `Cull : 0 / 67` がどこを向いても 0」を契機に判明: 4-前-4 (15b89ad) で compactCmd が device-local 化以降 readback 経路が断たれて HUD は永久 0 だった (props は GPU 経由で正常描画)。 option B (純 GPU-driven 化) で HUD 行 + CPU Frustum オラクル + 全 wire-up 撤去。 8 files +2 -51。 §5 項 12 残作業 list から該当項目を撤去、 付記に Pure GPU-driven cleanup entry 追加、 PART3c-2 / PART2 完了 entry に inline note (※検証は史実・f8d1e1f で撤去)。 / rev.11: **PART4 §6 4c 完了 (8 commits: two-pass HZB occlusion + Tier 1 α Nanite/Granite 2024 baseline + 1-tap minmax fast path) + §6 4d 大半完了 (10 commits: audit-driven α/γ × 3/M × 3/N × 4 = 最新技術取り込み)** = **PART4 essentially complete**。 §5 推奨の次の一手を「§8 畳み込み or 2C/2A/2F/1I-D」に更新、 付記に 4c 内訳 + 4d 内訳 + P620 [Caps] 18 中 17 = 1 を追記。 / rev.10: **PART4 §6 4b 完了** (HiZPass = SPD-style single-dispatch min+max RG32F pyramid)。 §5 推奨の次の一手を「PART4 4c (two-pass occlusion 本体)」に更新、 付記に 4b 成果 (capability `subgroupOps` + `shaderStorageImageArrayDynamicIndexing` 実測有効化 / hand-rolled SPD アルゴリズム / hzb_debug_widget viewer) を追記。 / rev.9: PART4 §6 4-前-0〜4-前-5 + 4a-1 + 4a-2 完了反映 = **PART4 §6 Hi-Z 受け皿全部立った**。 §5 推奨の次の一手を「PART4 4b HZB 本体」に更新、 付記に 4-前 / 4a-1 / 4a-2 と Vulkan13 W の成果を追記、 4a-2 の §3-1a clean rebuild 怠った教訓 (mode select → 本編で TDR) を反映 / rev.8: 2B PART3c-2 (prop の indirect 差し替え・CPU draw 撤去) 完了を反映 = **Phase 2B 完了** (prop bucket の GPU-driven 骨格が立ち上がり、CullingPass が実描画に接続)。§4 の 2B 見出しを完了に、§5 の推奨の一手を「2B 完了・次は 2C/Hi-Z/3B/2F」に更新、付記に PART3c-2 成果と `drawIndirectFirstInstance` 必須の確定事実・block 散在=連続区間 indirect・HUD 検証足場を追記 / rev.7: 2B PART3c のスコープを prop のみに明確化 (terrain は対象外)・3c-1 完了 (static_cull_build.h) と 3c-2 次を反映、**Phase 2F (terrain bucket = GPU-driven 地形 + splat + 距離 LOD + チャンクストリーミング) を新設** = 完成形の「terrain は別 bucket」を独立 Phase として受け皿化。PART3c で terrain を prop bucket に一度統合し撤回した経緯を反映 / rev.6: 2B PART3b (per-draw SSBO + shader 改修) 完了を反映、§4 の 2B 見出しを PART3b 完了・PART3c 進行中に、§5 の推奨の一手を「次は 2B PART3c (indirect 差し替え)」に更新、付記に PART3b 成果と cursor リセットの教訓を追記 / rev.5: 2B PART3a 完了反映 / rev.4: 1K 主要部・2B PART0-2 完了反映) / 対象 GPU: NVIDIA Quadro P620 (Pascal 世代, GP107 / CUDA 512 / VRAM 2GB GDDR5 / 帯域 64GB/s / TDP 40W) / API: Vulkan 1.4 + SDL3

このロードマップは「GPU の性能に関係なく最新の技術を取り入れたい」という方針に沿って、すでに積み上げた土台の上に何を積むかを整理したもの。各 Phase は独立して commit でき、見栄え・性能・最新度のどれに効くかを明記している。

> rev.3 の方針転換: 「Pascal で動くか否か」で Phase を時間軸に振り分けるのをやめ、**すべての Phase を実装対象**として扱う。最新技術 (mesh shader, HW レイトレ, フル GI 等) も、能力チェック + フォールバック付きで実装する前提に変えた。各 Phase には「620 での動作」を 3 段階 (快適 / 重い・品質ノブで対応 / フォールバックのみ・HW 更新待ち) で注記する。設計の核となる「能力チェック + フォールバック」は §3 に独立した節として書き起こした。
>
> rev.2 からの継続: リソース管理リファクタ (RAII 二層化) 完了を反映済み。water 復元・semaphore 再利用警告は解決済み。

---

## 1. 現状の到達点 (完成済みの土台)

多くの入門エンジンは古いやり方 (頂点バッファ + 固定 descriptor) で止まるが、MyEngine はすでに次の現代的基盤を持っている。

描画の「データの流し方」の現代性:

* Vulkan 1.4 + VMA (Vulkan Memory Allocator) によるメモリ管理
* Buffer Device Address (BDA): ポインタで GPU バッファを直接参照。skin matrices もインスタンスデータも descriptor なしで読む。※ BDA は Vulkan 1.2 でコア化されあらゆる世代の GPU で広く使える機能で、Pascal でも問題なく動く (MyEngine が現に使えていることが証明)
* Bindless texture (descriptor indexing, 1024 枚): テクスチャをインデックスで参照、マテリアルごとの descriptor 切り替え不要
* C++/GLSL 共有の types.h: 構造体定義を 1 箇所に統一
* HDR レンダリング + 3 種トーンマッパー (ACES / AgX / Khronos PBR Neutral) 切り替え + 永続化
* GPU instancing (SSBO + BDA, gl_InstanceIndex): 多数の同一メッシュを 1 ドローで
* CPU フラスタムカリング (Gribb-Hartmann) + デバッグ HUD 統計
* 統一 InstanceData (model + color + params): 草・将来の雲・木・花がすべて同じバッファ表現に乗る設計
* 手続き草・植生システム: 地形追従散布 + 風揺れ + 色バリエーション + 設定トグル
* 影 (shadow map, static + skinned)、水面 + 反射、パーティクル、デバッグライン描画

実装基盤の堅牢性 (リソース管理リファクタで到達):

* GPU リソースを 3 種のラッパーに一元化。`VkUnique<Handle>` (純 Vulkan ハンドルの寿命を持つ move-only RAII)、`VmaBuffer` (VMA リソース = VkBuffer + VmaAllocation)、`VmaImage` (VMA リソース = VkImage + VmaAllocation)。生メモリの手動 map/unmap・手動 vkDestroy*・生ハンドルの寿命管理がエンジン内から根絶された
* **メモリの VMA 管理: image 側は完了** (2026-05-25)。`VmaImage` を新設し RenderTarget / Texture / swapchain depth を移行、未使用化した `ResourceFactory::createImage`/`createImageVMA` を削除した。**エンジン内に image 用の生 vkAllocateMemory は無い。** 残るは buffer 側で生メモリ経路 (`createBuffer`) を使う mesh / model_loader / terrain_mesh / texture staging の VmaBuffer 化 (別タスク)。「メモリは全部 VMA」は image については達成済み
* 移行済みクラス: Texture / Mesh / TerrainMesh / WaterMesh / InstanceBufferPool / SkinBufferPool / MaterialRegistry / RenderTarget / ReflectionTarget / Model(SubMesh) / Swapchain / HudPipeline / DebugLinePass / ParticlePass / FrameUniforms / WaterPass / WaterPipeline / BindlessTextureRegistry / FrameSync、そして全描画パス (Shadow / Post / Bloom / Reflection / Main)
* persistent mapping を host-visible バッファに統一 (vkMapMemory の都度呼びは高コストなため、VMA の persistent mapped で確保しっぱなしにする現代的な定石)
* swapchain image ごとの present-wait semaphore 管理 (= 旧来の per-frame 使い回しによる semaphore reuse 警告を構造的に解消)

評価: データの流し方 (BDA + bindless) も土台のリソース管理 (ハンドルは全て三層 RAII / メモリは buffer・image とも VMA 化済み、残りは一部 buffer の生メモリ経路のみ) もモダン。足りないのは描画結果の「質」と「規模」。以下はその順で最新技術を載せていく。

---

## 2. 世界の最新動向 (2025-2026, web 確認済み)

主流は大きく 3 方向に進んでいる。

第一に GPU-driven rendering。カリングとドローコマンド生成を CPU から GPU の compute shader へ移す流れ。MultiDrawIndirect と compute culling を組み合わせ、数万〜数十万オブジェクトを CPU をほぼ介さず描く。vkguide の作例では百万単位のカリングを 0.5ms 以下で回せると説明され、Vulkanised 2026 でも GPU-driven + occlusion culling (Hi-Z) が主要トピック。さらに進むと meshlet (64 三角形程度の塊) 単位の mesh shader / task shader によるカリングへ。

第二にリアルタイム GI (大域照明)。probe ベースの DDGI や Unreal Engine 5 の Lumen が代表格。2025-2026 は neural radiance caching や 3D Gaussian ベース GI、object-centric な neural transfer model 等の研究も活発。重要な区別として、GI は「RT 必須」ではない:
* 純スクリーンスペース系 (SSGI/SSAO) は深度・法線だけで完結し世代を問わず動く。情報が画面内に限られる弱点はあるが、他の GI 手法と比べれば相対的に軽量 (ただし 620 では毎フレームの多点サンプルが帯域を食うので、実機では「重い・品質ノブ」扱い。§4 の 1J/3-GI 注記参照)
* SDF ベース GI: シーンを Signed Distance Field に焼いてソフトウェアレイトレする方式 (Flax Engine の realtime GI、SDFDDGI 論文など) は HW レイトレ非対応ハードを明確に狙っており、Pascal 相当でも動く実績がある (ただし重い)
* RTXGI 流の運用: RT 対応マシンで probe を焼き、非対応環境では静的データとして実行時ロードする分業も可能

第三に画質まわり。TAA、clustered/tiled forward lighting、bloom・被写界深度・モーションブラー等のポストエフェクト群、PCSS/contact-hardening 系のソフトシャドウ。世代を問わず効果が出やすく見栄えの伸びが大きい。

### GPU 世代の境界 (web 確認済み, このロードマップの前提)

最新技術を「実装対象」にするうえで、何が 620 のハードに無いかを正確に把握しておく。

* **Turing 世代から追加された機能** (= Pascal の 620 には無い): ハードウェアレイトレーシング、mesh shader / task shader、variable rate shading、texture-space shading。これらは Vulkan 拡張が 620 で公開されないため、能力チェックで弾かれフォールバックに落ちる
* **Pascal の 620 でも動く機能**: compute shader、SSBO、Buffer Device Address、indirect draw (MultiDrawIndirect)、descriptor indexing (bindless)。MyEngine が既に BDA / bindless / instancing を使えていることが動作の証明。つまり GPU-driven rendering の入口 (compute カリング + indirect draw) は 620 で動く
* **性能・VRAM の壁** (機能の有無とは別の制約): 620 は VRAM 2GB・帯域 64GB/s・TDP 40W (GTX 1050〜1050Ti 相当)。スクリーンスペース系 (Bloom/SSAO/TAA) は中間 render target を増やすので 2GB と帯域に当たりうる。フル解像度より半解像度処理で品質を保つ等の「品質ノブ」が要る場面がある

含意: 「ハードが無くて動かない」(RT/mesh shader) と「ハードはあるが重い」(SDF GI / 多数ライト / フル解像度ポスト) は別問題。前者はフォールバック (二択)、後者は品質ノブ (連続調整) で扱う。これが §3 の設計方針につながる。

---

## 3. 設計思想: 能力チェック + フォールバック (このロードマップの背骨)

「GPU の性能に関係なく最新技術を取り入れる」を安全かつ意味のある形で実現するための、全 Phase 共通の設計方針。最新技術を「載せる (実装する)」ことと「実行する (その GPU で動かす)」ことを分けるのが核心。

### 大原則: 最新経路は実装する。実行は GPU 能力で分岐する

最新技術 (mesh shader, HW レイトレ, フル DDGI 等) もコードとしては実装する。ただし起動時に GPU の能力を `vkGetPhysicalDeviceFeatures2` / 拡張列挙で問い合わせ、「対応していれば最新経路、非対応なら従来経路にフォールバックする」分岐を必ず入れる。これは UE5 等の現代エンジンが普通にやっていること (Lumen の HW レイトレ版 / ソフトウェアレイトレ版の切り替えなど)。

この方針が目的に合う理由:
* **620 でも安全**: 非対応経路は起動時チェックで自動的に無効化され、フォールバックで描画は出続ける
* **最新技術の考え方を今学べる**: 実装過程で最新 API・シェーダ・データ構造を書くこと自体が学習になる。動かす GPU が無くてもコードを書く価値がある
* **GPU 更新で自動的に開く**: 将来 RTX に更新した瞬間、書いておいた最新経路を能力チェックが検出して有効化する。作り直し不要

### 「620 で非対応の機能を実装したら何が起きるか」(安全性の確認)

物理的に GPU が壊れることはない。起こるのは次のいずれか:
* (正しく能力チェックすれば) 起動時に拡張有効化が `VK_ERROR_EXTENSION_NOT_PRESENT` 等で弾かれ、その機能を無効化して進む / 起動失敗する。実行に入らない
* (チェックを怠った場合) Validation layer が「機能が有効化されていない」と警告 / ドライバが描画を拒否し真っ黒 / 最悪アプリかドライバがクラッシュして落ちる
いずれもアプリが終了するだけで、ハードウェアは無傷。GPU 側の温度・電力保護 (サーマルスロットリング、620 はそもそも 40W) も別途あり、ソフトから過負荷でハードを壊すことは原理的にできない。唯一の実害は「重すぎて実用 FPS が出ない」ことだが、これは故障ではなくただ遅いだけ。

### 2 種類のフォールバック

最新技術は性質で 2 つに分かれ、フォールバックの形が違う。各 Phase の注記はこの区別に対応する。

1. **ハード機能そのものが無いもの** (mesh shader, HW レイトレ): 拡張が公開されないので起動時チェックで弾かれ、従来経路に落ちる。コードは「RTX なら最新経路 / なければ従来経路」の二択分岐。620 では従来経路が常に選ばれる = 注記「フォールバックのみ」
2. **ハード機能はあるが重いもの** (SDF GI, voxel cone tracing, 多数ライト, フル解像度ポスト): 620 でも動くが実用 FPS が出ないことがある。フォールバックは「品質ノブを下げる」形 (probe 解像度↓、半解像度処理、更新頻度の間引き)。二択でなく連続調整 = 注記「重い (品質ノブで対応)」

### 注記の凡例 (各 Phase 末尾に付く)

* **620: 快適** — ハードもあり負荷も軽い。フル品質で実用 FPS が出る
* **620: 重い (品質ノブで対応)** — ハードはあるが VRAM/帯域/演算で重い。半解像度・低解像度 probe・間引き等のノブで実用域に乗せる。最新 GPU でノブを上げると本来の品質に
* **620: フォールバックのみ (HW 更新待ち)** — ハード機能が無い。実装はするが 620 では従来経路が動く。RTX 更新で最新経路が自動的に開く

---

## 4. ロードマップ本体

すべて実装対象。並びは「見栄えがすぐ伸びる順 + 学びの素直さ」で、620 での動作注記を各 Phase に付す。

### 影・ライティングの質

#### Phase 1G — 影の品質向上 (PCF → PCSS ソフトシャドウ) 【完了 2026-05-25, commit 3436ae5】

今の影はハードエッジ。PCF (Percentage Closer Filtering) でシャドウマップ周囲数点を平均し輪郭を柔らかく。さらに PCSS (ブロッカー検索でペナンブラ幅を可変にし、接触部シャープ・遠いほどボケる) へ。PCF は一様カーネルで高速だが contact hardening は出ない、PCSS は可変カーネルで接触部が締まる、という関係。
**620: 快適。** PCF はサンプル数が軽負荷。PCSS もブロッカー検索のサンプル数がノブになり、620 でも実用域。
**実装済み:** `shared/shadow_sampling.glsl` に Vogel ディスク PCF + PCSS を集約し、4 lighting frag (triangle / instanced / skinned / bindless) から `sampleShadowFactor()` を呼ぶ。品質ノブ shadowParams.y (0:hard / 1:Soft / 2:High)。shader のみで C++/types.h/サンプラ不変。

#### Phase 1K — マテリアルの PBR 化 【主要部 完了 2026-05-25】

簡易ライティング (ハーフランバート等) から metallic-roughness の物理ベースシェーディング (Cook-Torrance BRDF) へ。bindless 基盤があるので albedo / normal / metallic-roughness / AO マップを差し込める。GI を将来入れる前提条件でもある。
**620: 快適。** ピクセルあたりの BRDF 計算が増えるだけで、追加バッファも少ない。VRAM 余裕があればテクスチャ枚数が増える点だけ注意。
**完了:** 1K-A = BRDF を `shared/pbr.glsl` に集約 (commit 964c733、`pbrDirectLighting` は1ライト×1表面の純粋関数で多光源/減衰/影に拡張容易)。1K-5 = 法線マップを surface gradient framework (Mikkelsen 2020) で実装 (593ef17 等、detail/地形/decal 合成に正しく対応する受け皿)。1K-4 = metallic-roughness マップ (ddc5435、roughness=G/metallic=B linear)。各機能は設定トグル付き。**残: 1K-6 AO (どのモデルも AO テクスチャ未保持なので別途用意 or skip) / emissive。実質一区切り。**

#### Phase 2A — clustered / tiled forward lighting

画面を 3D クラスタ (タイル × 深度スライス) に分割し、各クラスタに影響するライトだけ集める。数百のポイントライトを forward で扱える。BDA + SSBO 基盤がそのまま活きる。夜の街・松明・魔法エフェクト等の多光源に必須。
**620: 重い (品質ノブで対応)。** クラスタ分割の compute とライトリストは 620 で動く。実効ライト数・クラスタ解像度がノブ。数百ライトを同時に焚くと帯域に当たるので、影響範囲の小さいライトを間引く。

#### Phase 2E — カスケードシャドウマップ (CSM)

視錐台を距離で分割し、近いカスケードほど高解像度のシャドウマップを割り当てる。広いシーンで影の解像度を保つ。オープンワールド志向なら 1G の次の影の課題。ShadowPass が RAII 化済みなので、ターゲットとパイプラインをカスケード本数ぶん増やす形で乗る。
**620: 重い (品質ノブで対応)。** カスケード数 (3〜4) とシャドウマップ解像度が VRAM を食う。620 では解像度・カスケード数を控えめにすれば動く。

### ポストエフェクト (スクリーンスペース)

#### Phase 1I — Bloom (ブルーム) 【完了 2026-05-25, commit d03f3ff / compute mip-chain】

明るい部分を抽出 → ミップチェーンで down/up sampling → 加算合成。HDR 基盤があるので相性抜群。
**実装方針 (確定): compute シェーダで実装する。** fragment + render pass + framebuffer + ブレンド state を積み上げる旧来の作り方ではなく、storage image に直接読み書きする compute 版を採る。これが最新かつ最小構造で、最も散らからない (START_HERE §0 の判断原則: 最新 = 直接的 = 保守的、は同じ基準)。
**620: ノブ (mip 段数) で調整。**
**実装済み:** エンジン初の compute pass。BloomPass が mip 列 (storage+sampled VmaImage を既定6段) を内包。Jimenez/CoD 方式 — bright (HDR→mip0, soft-knee) → 13-tap downsample (mip0→1 のみ Karis average で firefly 抑制) → 3x3 tent upsample (加算)。全て vkCmdDispatch、render pass も framebuffer も無い。mip 列は GENERAL 運用、HDR 入力は SHADER_READ_ONLY、最終 mip0 は execute 末で SHADER_READ_ONLY に遷移して PostPass が合成 (post 無改修)。VulkanRenderer は bloom target を持たない。確立した compute 作法 (8x8 workgroup / storage image descriptor / dispatch 間 COMPUTE→COMPUTE バリア) は Codebase_Guide §3 に記録 = 2B が踏襲する。残: PART D (強度/段数を settings 連携・目視チューニング) が任意。

#### Phase 1J — SSAO / GTAO

物が接する隙間・くぼみに陰を落とし立体感と接地感を出す。深度バッファから計算するのでライティングモデルを変えず導入できる。GTAO (Ground Truth AO) まで行くと品質が高い。深度・法線を読む新パス + 専用 render target が要る。
**620: 重い (品質ノブで対応)。** サンプル数・カーネル半径・処理解像度がノブ。毎フレーム深度を多点サンプルするので帯域に効く。620 では半解像度 + 少サンプル + ブラーで実用域に。

#### Phase 2D — TAA (時間的アンチエイリアス)

複数フレームをジッタリングして蓄積しエッジのギザギザと shimmering を除去。モーションベクトルを出すパスが要る。SSAO/SSGI/SSR のノイズを時間方向に均す土台にもなる。
**620: 重い (品質ノブで対応)。** 履歴バッファ (フル解像度の color 履歴) が VRAM を食い、毎フレーム再投影で帯域も使う。620 では VRAM 2GB の予算管理が肝。動く範囲だが、他のフル解像度ターゲットとの同居に注意。

### GPU-driven / スケール

#### Phase 2B — compute シェーダによる GPU カリング + indirect draw 【完了 (PART0-2 + PART3a + PART3b + PART3c-1 + PART3c-2)】

CPU フラスタムカリングを GPU の compute shader に移す。可視判定 → ドローコマンドを GPU バッファに書き出し → vkCmdDrawIndexedIndirect で描画。CPU 負荷が激減し、草・木・オブジェクトを桁違いに増やせる。GPU-driven rendering の入口。統一 InstanceData + BDA がここで真価を発揮 (compute にカメラ情報とバッファアドレスを push constant で渡せば descriptor 切り替えなしで次フレームのカリングを先行実行できる)。発展形として Hi-Z occlusion culling も 620 で射程内。
**620: 快適 (CPU 軽減) 〜 規模次第で重い。** compute・indirect draw・BDA はすべて Pascal 標準機能で動く。効果は「620 の compute が劇的に速い」より「CPU が楽になり描画オブジェクトを増やせる」方向。増やしすぎると今度は GPU 演算・VRAM が頭打ちになるので、描画数がノブ。
**進捗 = 完了。** PART0 (設計) / PART1 (CullObject 受け皿, df9d843) / PART2 (compute cull pass, 5cbc7e6) = CullingPass (全 BDA・descriptor 無し) + cull.comp が GPU でフラスタムカリングして IndirectCommand の instanceCount を生成。**PART3a (メッシュ統合, ac7bbd1)**: 全 static prop を無制限 multi-block GeometryBuffer に統合 = 前提 (a)。**PART3b (per-draw SSBO + shader, c5adced ほか)**: model/materialId/alpha を DrawData SSBO + gl_InstanceIndex(firstInstance) 化 = 前提 (b)(c)。**PART3c-1 (632433a)**: `static_cull_build.h` が prop を SubMesh 粒度で走査し drawId 連番で DrawData/CullObject/DrawTemplate/PreparedDraw を同時生成 = 前提 (d)。terrain は一度 prop bucket に統合し撤回 (別 bucket = Phase 2F)。**PART3c-2 (1cf23b9) = 完了**: main の prepared CPU draw ループを `vkCmdDrawIndexedIndirect` に差し替え、CPU draw を撤去。**CullingPass が prop の実描画に初接続 = prop bucket の GPU-driven 骨格完成。** GPU が instanceCount==0 を自動スキップ = カリングが実描画に効く。能力チェックで `multiDrawIndirect` + **`drawIndirectFirstInstance`** を実測有効化 (P620 は両対応・`[Caps] 1/1`)。**block 散在 (≈17-18 連続区間) のため「同一 block の連続区間ごとに 1 draw 呼び出し」** (対応=区間ぶん単発 MDI / 非対応=区間内 drawCount=1 ループ。全 draw を1 MDI は不可)。シェーダ無改修。検証は HUD `Cull : 可視/総数` (視点回転で分子が動く)・prop 全部正常描画・validation/VUID/leak ゼロ。**発展形 Hi-Z occlusion (PART4) はこの骨格の上に drop-in** (cmdBuf 駆動構造・COMPUTE→DRAW_INDIRECT バリア維持済み)。terrain の GPU-driven 化は Phase 2F。(詳細は Codebase_Guide §3.5 / START_HERE §2 / Work_Protocol §5e)

#### Phase 2C — LOD (Level of Detail) システム

距離に応じてメッシュ詳細度を切り替える。compute culling と組み合わせれば遠くは低ポリ・近くは高ポリと自動選択。広いマップの性能に直結。2B の compute から LOD 選択も書けるので 2B の自然な発展。
**620: 快適 (むしろ 620 で効く)。** LOD はまさに非力な GPU を救う技術。低ポリ化で頂点・VRAM 負荷が下がるので、620 では恩恵が大きい。

#### Phase 2F — terrain bucket (GPU-driven 地形 + splat マテリアル + 距離 LOD + チャンクストリーミング)

**大規模オープンワールドの地形を、prop とは別の専用 bucket として GPU-driven 化する Phase。** 完成形アーキテクチャ (START_HERE §2) で確定した「prop と terrain は別 GeometryBuffer・別 indirect バッチ」を実体化する。2B (prop の GPU-driven 骨格) と同じ仕組み (GeometryBuffer + DeletionQueue + compute cull + indirect draw) を terrain 用に並立させる。prop bucket とは混ぜない (地形チャンクは寿命・サイズ・アロケーション粒度が prop と全く違い、同一 megabuffer だと断片化する)。構成要素:

* **専用 GeometryBuffer (terrain bucket)**: 地形チャンクのジオメトリを prop とは別の megabuffer に。multi-block + VmaVirtualBlock (2B PART3a と同型) を再利用。`terrain_mesh.h/.cpp` には既に geom 対応コードが残置済み (PART3c で一度統合し撤回した際のもの) で、ここで terrain 専用 GeometryBuffer に繋ぎ直して再利用する。
* **専用 cull**: 地形は視錐台カリング + **距離 LOD** (近=高密度メッシュ / 遠=低密度) を compute で選択。prop の cull.comp とは別パスか、bucket 識別を持たせた共通パス。
* **splat マテリアル経路 (土・岩・砂のブレンド)**: 地形シェーダ内で複数マテリアルをスプラットマップ / 傾斜ベースでブレンド (Step3 地形描画設計: 平坦=草・急斜面=岩の自動切替、スプラットマップで手動指定、2-4種混合)。現状は grass_field 1種だが、**受け皿をこの Phase で最新の形 (splat) で用意し、マテリアルを後から足す** (§0 の「受け皿を先に最新で用意」原則)。
* **チャンクストリーミング**: マップを chunk 分割し、範囲外をアンロード・接近時にロード。**前提: 遅延破棄 (DeletionQueue、実装済み) と buffer 系 VMA 化** (依存マップ §0 土台 side「ストリーミング」層。in-flight リソースの破棄タイミングを遅延破棄が解く)。chunked LOD / geometry clipmap でマップ 1km×1km 以上に対応。
* **遠景処理**: 数 km 先の地形表現 (将来)。

**620: 快適〜重い (ノブ)。** 距離 LOD + チャンクで描画三角形を抑えれば 620 でも動く。splat は texture fetch が増えるので段数をノブに。
**前提: 2B (prop GPU-driven 骨格・PART3 完了) / 遅延破棄 / buffer 系 VMA 化。** これらが揃ってから着手するのが素直 (依存マップ参照)。地形描画の段階分解 (Step1 フラット → Step2 ハイトマップ起伏 → Step3 splat → Step4 LOD+大規模 → Step5 植生) は別途の地形設計メモに準拠。

### GI / 反射 / ジオメトリ最新形

#### Phase 3-GI — リアルタイム GI (段階導入: SSGI → SDF → フル DDGI)

間接光。実装は軽い順に段階導入する設計にすると、620 でも「GI とは何か」を体験でき、GPU 更新で上位段に自動で乗れる。
* 段 1: SSGI (スクリーンスペース) — 深度・法線から間接光を近似。SSAO の延長で 620 でも動く
* 段 2: SDF ベース GI — シーンを Global SDF に焼いてソフトウェアレイトレ。Flax/SDFDDGI 系。620 でも動く実績ありだが重い
* 段 3: フル DDGI (probe + HW レイトレ) — 高品質・低ノイズだが RT 必須
能力チェックで「RT あり → 段 3 / なし → 段 2 or 段 1」と切り替える。
**620: 段 1 は重い (品質ノブ) / 段 2 は重い (品質ノブ、probe 解像度・更新間引き) / 段 3 はフォールバックのみ (HW 更新待ち)。**

#### Phase 3-Refl — 反射の現代化 (planar → SSR → RT 反射)

今の水面反射は planar reflection (品質は据え置き中)。段階導入:
* 段 1: SSR (スクリーンスペース反射) — 深度・color から画面内の反射を近似。620 で動く
* 段 2: HW レイトレ反射 — 正確な反射。RT 必須
能力チェックで RT あれば段 2、なければ段 1 or 既存 planar にフォールバック。
**620: 段 1 (SSR) は重い (品質ノブ) / 段 2 はフォールバックのみ (HW 更新待ち)。**

#### Phase 3B — mesh shader / task shader

meshlet 単位の GPU カリング + 描画。ジオメトリパイプラインの最新形。2B (compute culling) を発展させる形で実装し、能力チェックで「mesh shader 対応 → meshlet 経路 / 非対応 → 2B の compute + 通常 vertex shader 経路」に分岐。
**620: フォールバックのみ (HW 更新待ち)。** mesh shader 拡張が 620 で公開されないため、常に 2B 経路が動く。RTX 更新で meshlet 経路が自動的に開く。

#### Phase 3C — ハードウェアレイトレーシング (反射・影・GI の正確版)

VK_KHR_ray_tracing_pipeline。3-GI 段 3・3-Refl 段 2 の実体。RT 対応時の最上位経路として実装。
**620: フォールバックのみ (HW 更新待ち)。**

---

## 5. 推奨する次の一手

**1G / VmaImage 化 / 1I (compute bloom) / 1K (PBR 主要部) / 2B (compute cull + indirect draw、PART0-2 + PART3a/3b/3c = 全完了) / 2B PART4 (Hi-Z) essentially complete (§6 4-前 + 4a + 4b + 4c + 4d 大半 + Pure GPU-driven cleanup f8d1e1f・**28 commits・P620 [Caps] 18 中 17 = 1 で実走**) は完了済み。Phase 2B 完了 = prop bucket の GPU-driven 骨格 + two-pass HZB occlusion (Nanite/Granite 2024 baseline) + 純 GPU-driven (HUD・CPU オラクル無し) が立ち上がった。現在の起点はここから枝分かれ。** 以下は残りを、最新技術を学びつつ 620 で確実に動き、見栄えが伸びる順に並べたもの:

完了済み (参考・着手順の足跡):
1. ~~Phase 1G (PCF→PCSS ソフトシャドウ)~~ 完了 (commit 3436ae5)
2. ~~VmaImage 化 (土台)~~ 完了 (commit b9ac20b..1349a04)。image メモリを VMA 一本化。確定ルール通り 1I の前に完了
3. ~~Phase 1I (Bloom)~~ 完了 (commit d03f3ff)。compute mip-chain で実装。エンジン初の compute pass
4. ~~Phase 1K (PBR マテリアル) 主要部~~ 完了。1K-A BRDF 集約 (964c733) / 1K-5 法線マップ surface gradient (593ef17 等) / 1K-4 metallic-roughness (ddc5435)。残: 1K-6 AO (どのモデルも AO テクスチャ未保持で優先度低・skip 可) / emissive。実質一区切り
5. ~~Phase 2B PART0 (設計) / PART1 (CullObject 受け皿, df9d843) / PART2 (compute cull pass, 5cbc7e6)~~ 完了。CullingPass (全 BDA・descriptor 無し) + cull.comp が GPU でフラスタムカリングし IndirectCommand の instanceCount を生成。GPU 可視数 = CPU オラクル一致・validation ゼロで検証済み。
6. ~~Phase 2B PART3a (メッシュ統合, ac7bbd1) / PART3b (per-draw SSBO + shader, c5adced) / PART3c-1 (ビルダ 632433a) / PART3c-2 (indirect 差し替え・CPU draw 撤去, 1cf23b9)~~ **完了 = Phase 2B 完了**。prop の prepared CPU draw ループを `vkCmdDrawIndexedIndirect` に差し替え CPU draw を撤去、CullingPass が prop の実描画に初接続。能力チェックで `multiDrawIndirect`+`drawIndirectFirstInstance` を実測有効化 (P620 両対応)。block 散在のため連続区間ごと indirect (区間 MDI / 非対応はループ)。**シェーダ無改修**。HUD `Cull : 可視/総数` で検証 (視点回転で分子が動く)。PART4 (Hi-Z) を見越し cmdBuf 駆動構造維持。

現在の起点 (**2B PART4 essentially complete** = 4c + 4d 大半 + Pure GPU-driven cleanup 完了・19 commits ad97879..f8d1e1f) から:
7. **PART4 §8 畳み込み** ← ★次推奨。 PART4 の確定事項 (visHistory / HZB descriptor set / persistent pipeline cache / sync2 generic layouts / two-pass HZB / Tier 1 α Nanite/Granite 2024 baseline / Vulkan14Features chain) を本書 §4 / 付記・依存マップ Hi-Z ノード・Codebase_Guide §3.5・Work_Protocol §5f に書き戻し、 PART4 設計書を「履歴」として閉じる。 これをやらないと次 Phase で再発明する。
8. **Phase 2C (LOD)** — P620 を救う / 大規模オープンワールド前提に必要。 2B + Hi-Z 骨格の上に乗る発展。
9. **Phase 2A (clustered forward+ 多光源)** — Foundations §5 + bindless 連携。 1K PBR の上に乗る。 開発中の dummy light で動かしつつ多光源データ駆動の受け皿を確立。
10. **Phase 2F terrain bucket** — terrain を専用 GeometryBuffer + 専用 cull + splat + 距離 LOD + チャンクストリーミングで GPU-driven 化 (前提: 遅延破棄 + buffer 系 VMA 化 + ストリーミング層)。
11. **Phase 1I PART D** (bloom 強度・段数を settings 連携・目視チューニング) — 本体は完了済み、これは仕上げ。
12. **4d 残作業 (別 commit / 別 Phase で着手可)**: DGC 経路 **実装** (`VkIndirectCommandsLayoutEXT` / `VkIndirectExecutionSetEXT` ラッパ・Pascal 非対応で実 device 必要) / Shader Object 経路 (`ShaderProgram` 抽象) / Descriptor Buffer 経路 (Pascal 強制無効化ロジック) / Timeline semaphore (`FrameSync` 内部経路分岐) / Async compute での HZB / cull 並列実行 (Foundations §2 と一緒) / `[BlockDbg]` / `[Cull2B]` 一時ログ掃除 / transparent MRT mismatch fix (chest 開封時 outNormal/outMotion validation noise・4a-2 由来) / 4b Obs C (R32G32_SFLOAT storage image format properties query) / 4b Obs D (subgroup ID → linearIdx canonical mapping)。 ※ ~~`lastCullGpuVisible_` HUD GPU readback or 純 GPU-driven 化 readback 撤去~~ は ✅ commit f8d1e1f で完了済 (option B = 純 GPU-driven 化)。
13. **Phase 3B mesh shader** — 2B + Hi-Z + LOD の上に乗る発展。 P620 非対応で fallback 経路あり。

任意 / 並行可:
- **Phase 1I PART D** (bloom 強度・段数を settings 連携・目視チューニング) — 本体は完了済み、これは仕上げ
- **buffer 系 VMA 化** (mesh/model_loader/terrain_mesh/texture staging の createBuffer → VmaBuffer) — image 側は VMA 一本化済み、buffer 側が残土台。**※ 2B PART3 のメッシュ統合と作業領域が重なる (mesh/model_loader を触る) ので、PART3a の設計時に一緒に片付けるか判断するとよい**
- **Phase 2A (clustered 多光源)** — GPU-driven とは別系統。1K PBR の上に乗る

特に Phase 2B PART3 は、CPU draw ループを GPU 主導に置き換える GPU-driven rendering の核心であり、メッシュのメモリレイアウトをオープンワールド規模 (数千〜数万オブジェクト) に耐える形へ作り変える、エンジンにとって不可逆で本質的な一歩。

最新技術 (3-GI / 3-Refl / 3B / 3C) も「能力チェック + フォールバック」(§3) で**今から実装してよい**。620 ではフォールバック経路 or 品質ノブ下限で動き、GPU 更新で最新経路が自動的に開く。学びは今のうちに積める。

リソース管理が三層 RAII (VkUnique / VmaBuffer / VmaImage) に揃った今、どの Phase も「ハンドルは VkUnique、buffer メモリは VmaBuffer、image メモリは VmaImage で持つ」という同じ作法で書けるので、配管に気を取られず描画ロジックに集中できる。image メモリの VMA 化も完了したので、render target を増やす Phase をいつでも素直に書ける。

---

## 6. 横断的に効く改善 (Phase とは別)

> 解決済み (rev.2 で除去): water_pass の復元、queue semaphore reuse 警告の解消、buffer の vkAllocateMemory → VMA 移行 (buffer のみ)。
> 解決済み (2026-05-25 追加): **image 系の VMA 化 (VmaImage 新設、RenderTarget/Texture/swapchain depth 移行) と、deprecated な生メモリ image 経路 (createImage/createImageVMA) の削除**。下記「image 系の VMA 化」「リファクタの総仕上げ」の image 部分は完了。残るは buffer 側の生メモリ経路 (createBuffer) のみ。

残っている改善候補:

* **VRAM 予算の可視化** (620 では特に重要): `VK_EXT_memory_budget` で VRAM 使用量を HUD に出す。2GB の残量が見えると、Bloom/SSAO/TAA/CSM の追加ターゲットを足す前に予算が判断できる。「品質ノブをどこまで上げられるか」の指標になる。軽い作業で俯瞰の第一歩
* リファクタの総仕上げ: ResourceFactory の `[[deprecated]]` な生メモリ経路の削除。**image 側 (createImage / createImageVMA) は削除済み (2026-05-25)**。残るは buffer 側 (`createBuffer`) で、mesh / model_loader / terrain_mesh / texture staging を VmaBuffer 化すれば削除できる (`createBufferVMA` の要否もそのとき精査)
* ~~image 系の VMA 化~~ **完了 (2026-05-25)**: `VmaImage` ラッパー (VkImage + VmaAllocation) を新設し、RenderTarget / Texture / swapchain depth を移行。「メモリは全部 VMA」が image については達成。§4 のポスト系で増える render target も VMA 管理で VRAM が見通せる。1I (compute bloom) の mip 列もこの VmaImage で確保した
* 遅延破棄 (deferred deletion) キュー: 破棄を MAX_FRAMES_IN_FLIGHT フレーム後に実行するキュー。WorldTerrain/WorldWater の手動 clear ループ (engine_app の stopgap) を撤去でき、swapchain 再作成やチャンクストリーミング時の "in-flight リソースをいつ消すか" が綺麗に解ける。オープンワールドのストリーミングの前提
* 草配置の改善: 碁盤目の規則性を崩す (オフセット幅拡大 + 確率スキップ)、本物 PNG テクスチャ差し替え
* RenderDoc / Nsight でのフレームキャプチャ習慣化: ボトルネック特定と学習が加速。620 でどの Phase が帯域/VRAM のどこで詰まるかを実測できる

---

## 付記: 直近の成果

### PART4 §6 4d Pure GPU-driven cleanup = 完了 (2026-05-29, commit f8d1e1f)

* **発端**: user 報告「HUD `Cull : 0 / 67` がどこを向いても 0」。 props は画面で正常描画されているのに HUD だけ stale。
* **根本原因**: 4-前-4 (commit 15b89ad, 3-pass scan compaction) で compactCmd が host-mapped → **device-local** に変わって以降、 旧 host-mapped 経路の `lastVisible_[]` を更新する code path が消滅 (culling_pass.cpp 内のコメントが「4d HUD cleanup でやる」と明示していた)。 `lastVisible_[i]` は init で 0 に初期化されるだけで以降 0 のまま = HUD は永久 0。 props は GPU compactCmd 経由で正常描画されていたため動作上は健全だった。
* **option B 採用 = 純 GPU-driven の本来形**: §1.5-C 「最新が第一」+ START_HERE/Roadmap 既載「CPU が可視数を知らない = GPU-driven の本来形」方針に従い、 HUD `Cull` 行 + CPU Frustum オラクル + 全 wire-up 撤去。
* **撤去 site (8 files, +2 -51)**:
  - `culling_pass.h`: `lastGpuVisible()` / `lastCpuVisible()` getter 削除、 `lastVisible_[MAX_FRAMES_IN_FLIGHT]` array + `lastCpuVisible_` field 削除。
  - `culling_pass.cpp`: execute() 内の CPU Frustum オラクル block (6 行) + destroyBuffersToDeletionQueue() の `lastVisible_[i] = 0` 初期化撤去。 Frustum::extract は cull.comp の push constant planes 用に同 file 内で引き続き使用 (削除しない)。
  - `pass_chain.h/.cpp`: `lastCullGpuVisible()` / `lastCullTotal()` accessor + field + 代入 site (2 行) 削除。
  - `vulkan_renderer.h`: `cullGpuVisible()` / `cullTotal()` relay getter 削除。
  - `gameplay_layer.cpp`: RenderDebugData への代入 2 行削除。
  - `render_debug_system.h/.cpp`: `cullGpuVisible` / `cullTotal` field + HUD `"Cull : %d / %d"` 行 + 周辺コメント block 撤去。
* **clean rebuild + mspdbsrv kill (§3-1a / §3-3)**: ヘッダ struct size 変更 (`lastVisible_` array + 多数の field 削除) で `PassChain` / `CullingPass` / `RenderDebugData` の sizeof が変化したため、 §3-1a 適用。 mspdbsrv が stale lock していたため §3-3 で kill して通過。
* **検証**: Vulkan init + asset load + ReflectionPass rebuild + ModelLoader knight/skeleton load まで validation エラー / VUID / leak 0 で通過。 user 目視「画面 OK / Cull 行 HUD から消えている」確認済み。
* **将来の精密照合**: 同一フレーム基準の精密照合が要る場合は別 commit で `countBuf` を small staging buffer に `vkCmdCopyBuffer` する形 (option A) で復活可能 (frame fence で safe・1 フレ遅延あり)。

### PART4 §6 4c 完了 = two-pass HZB occlusion + Tier 1 α (Nanite/Granite 2024 baseline) + 1-tap minmax fast path (2026-05-29, 8 commits ad97879 / 477985d / f242327 / 7e446a9 / e41cfd7 / 91a6885 / 2f7daf9 / ccf5c03)

* **4c-A (ad97879)**: half-extent (`CullObject.extentDrawId.xyz`) 充填 + `HiZPass::previousPyramidView()` accessor (受け皿)。
* **4c-B (477985d)**: capability getters (`samplerFilterMinmax`, async compute family) + `HizParams` BDA buffer + cull.comp helpers (`aabbScreenBounds` / `mipFromScreenSize`)・gate-off で dead-code 配置。 P620 [Caps]: `samplerFilterMinmax=1 asyncComputeFamily=2 (dedicated=1)`。
* **4c-C (f242327)**: full machinery (HiZPass `minReductionSampler` + `ensureAllSlotsInGeneral` + per-drawId 永続 1bit `visHistory` buffer + descriptor set 0 with HZB samplers + cull.comp pass1/2 paths + main_pass `Pass enum` + loadOp 分岐) gate-off で merge。
* **4c-D (7e446a9)**: **two-pass HZB occlusion 活性化**。 pass_chain 順序 = `Cull(pass1) → MainPass(FirstOpaque) → HiZPass → depth barrier(ATTACHMENT_OPTIMAL → READ_ONLY_OPTIMAL) → Cull(pass2) → MainPass(SecondAndNonOpaque)`。 main_pass の SecondAndNonOpaque は `loadOp=LOAD` で pass1 の HDR + depth を保存。 visHistory writeback で次フレ pass1 が前フレ可視オブジェクトのみ早期 cull。
* **fix #1 (e41cfd7)**: HZB descriptor set に `UPDATE_AFTER_BIND` flag 追加 (bindings + layout + pool 3 箇所)。 4c-D 直後の起動で multi-dispatch 中の `vkUpdateDescriptorSets` が validation flood (`VkDescriptorSet was destroyed or updated without UPDATE_AFTER_BIND`) → bindless texture array と同じ pattern に統一して解消。
* **fix #2 (91a6885) — ユーザー目視発見**: 4c-D 直後の起動で「青背景が黒化・宝箱/墓/壁が消失」(スクショ確認)。 原因は main_pass `SecondAndNonOpaque` 経路で常に走っていた `toAttach` barrier (`oldLayout=UNDEFINED → COLOR_ATTACHMENT`) が pass1 の HDR clear + cleared depth を破棄していた。 `if (info.pass != Pass::SecondAndNonOpaque)` で skip して解消。 user 目視「画面正常」確認。
* **Tier 1 α 活性化 (2f7daf9)**: 4c 時点の pass1 は「prev_vis のみ」(Maister 2018 simpler 形式) だったのを、 pass1 でも `hzbPrev` を sample して `prev_vis && frustum && cone && !hzbOccluded(hzbPrev)` に格上げ = **Nanite/Granite 2024 baseline** 適合。 hzbPrev 受け皿は 4c-A から立っていたので shader 1 行追加で済む。
* **1-tap fast path (ccf5c03)**: cull.comp `hzbSampleMinR` に spec const `kHzbMinReductionFastPath`。 `samplerFilterMinmax=1` の device では reductionMode=MIN sampler 経由で 1 `textureLod` = `min(2x2)` を取り出す (4-tap fallback あり)。 P620 (=1) で fast path 自動選択。
* **検証**: user 目視「画面正常」(全 prop / 反射 / shadow / grass / Player 正常)。 validation エラー 0 (chest 開封時の transparent MRT warning は 4a-2 由来で別軸)。

### PART4 §6 4d 大半完了 = audit-driven 最新技術取り込み 10 commits (2026-05-29)

設計時の「2-3 commit」想定を、 user 主導の audit (「最新技術を取りこぼしている箇所がないか」) で複数 round に細分化して進めた。

* **α (082d792)**: 4b Obs B 解決。 `HiZPass::initialTransitionToGeneral` の `VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT` → `VK_PIPELINE_STAGE_2_NONE` (sync2 best practice for UNDEFINED→X)。
* **γ-1 (4b9c32c) PostPass**: VkRenderPass + VkFramebuffer 撤去・swapchain `UNDEFINED → COLOR_ATTACHMENT → PRESENT_SRC_KHR` 明示 barrier。
* **γ-2 (da74526) ShadowPass**: depth-only dynamic rendering 化・`ShadowPipelineParams` に `VkPipelineCache` 追加で M1 も同時配線。
* **γ-3 (33e1511) ReflectionPass**: dynamic rendering 化 + **bonus**: startup の outNormal/outMotion validation warning 4 件が消滅 (reflection が triangle.frag を 1-attachment renderPass で作っていた legacy mismatch が解消)。
* **結果**: engine 全体 (src/renderer grep) で **VkRenderPass / VkFramebuffer の実 API 使用ゼロ** = Vulkan 1.3 dynamic-rendering native。
* **M3 (47c3571)**: `VK_KHR_dynamic_rendering_local_read` (Vulkan 1.4 core) capability 受け皿 = Phase 3 SS 効果 / tile-based fast path。
* **M1 (a62b7f0) — Vulkan13 §3 Y closed**: **persistent VkPipelineCache**。 `<AppData>/MyEngine/MyEngine/pipeline.cache` に load/save。 全 14 vkCreate*Pipelines callsite が `ctx_->pipelineCache()` 経由 (graphics + compute + shadow 経路の `ShadowPipelineParams` 拡張含む)。 user clean exit で 490KB 書き出し実証。 起動 hitch 防止の本命受け皿。
* **M2 (fcef5ab)**: `depth_layouts.h` ヘルパ撤去 → sync2 generic `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL` / `READ_ONLY_OPTIMAL` 一括置換 (19 callsites + ヘッダ削除・コード -45 行)。 sync2 推奨の generic layout を全面適用。
* **N1 (7298968)**: `pipelineCreationCacheControl` (Vulkan 1.3 core) enable・open-world streaming で `VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT` 経由の hitch 防止受け皿。
* **N4 (c01c2e5)**: **`VkPhysicalDeviceVulkan14Features` chain 追加** (engine が API 1.4 で動作中なのに 1.4 features を一切 enable してない構造欠陥を修正)・`maintenance5` / `maintenance6` enable・M3 の KHR struct を canonical 1.4 form に統一。
* **N2+N3 (1481049)**: `VK_EXT_graphics_pipeline_library` + `VK_KHR_pipeline_binary` capability 受け皿 (extension name walk による query のみ・activation は Phase 2A/3 で)。
* **P620 最終 [Caps] log (audit 完了時実測)**:
  ```
  multiDrawIndirect=1 drawIndirectFirstInstance=1 synchronization2=1
  drawIndirectCount=1 deviceGeneratedCommands=0 dynamicRendering=1
  separateDepthStencilLayouts=1 subgroupOps=1 subgroupSize=32
  samplerFilterMinmax=1 asyncComputeFamily=2 (dedicated=1)
  dynamicRenderingLocalRead=1 pipelineCreationCacheControl=1
  maintenance5=1 maintenance6=1 graphicsPipelineLibrary=1 pipelineBinary=1
  ```
  **18 capability 中 17 が =1** (Pascal P620 で対応・DGC のみ 0 で fallback 経路あり)。

### PART4 §6 4b 完了 = HiZPass = SPD-style single-dispatch min+max RG32F pyramid (2026-05-28, commit ffe9673)
* **新規 `renderer/hiz_pass.{h,cpp}` + `shaders/hiz_spd.comp`**。 main_pass の swapchain depth (4a-2 で SAMPLED 化済み・post-barrier で `DEPTH_READ_ONLY_OPTIMAL`) を入力に、 per-frame (MAX_FRAMES_IN_FLIGHT=2) の `VK_FORMAT_R32G32_SFLOAT` mip chain (.r=min / .g=max・Design §3.3-N) を **1 vkCmdDispatch** で生成。 AMD FidelityFX SPD の「タイル + atomic counter」パターンを GLSL でハンドロール (FFX SDK 外部依存ゼロ・MIT-style 同等の単一 dispatch 設計)。
* **per workgroup = 256 threads / 64×64 source tile**。 LDS 16×16 に Phase A..F (mip0=32×32 per group → mip5=1×1 per group) を貯めながら順次 imageStore。 全 group が `coherent` atomic counter を `atomicAdd`、 last group のみ `memoryBarrierImage()` 後に mip6..N (1280×720 で N≈10) を継続。
* **新規 viewer `renderer/hzb_debug_widget.{h,cpp}`**: 右下ドック、 frame / mip スライダ、 RG32F の .r=赤 .g=緑 で min/max を同時可視化。 4a-2 の `gbuffer_debug_widget` と並ぶ独立クラス、 GENERAL レイアウトのまま ImGui::Image で sample。 RenderDoc に頼らず 4b の正しさを確認できる。
* **capability 追加 (Design §1.5-C 実測してから書く)**: `subgroupOps` (basic + **shuffle** in COMPUTE・wave shader が `subgroupShuffleXor` のみ使うので ARITH/QUAD は要求しない) + `subgroupSize` (`VkPhysicalDeviceSubgroupProperties` 経由) + `shaderStorageImageArrayDynamicIndexing` (per-mip storage image array を loop-uniform 動的 index で引く必須 feature)。 P620 実測: `subgroupOps=1 subgroupSize=32`。 **二派生 spv**: `hiz_spd.comp` (LDS-only fallback) + `hiz_spd_wave.comp` (Phase C を `subgroupShuffleXor` 経由・subgroupOps && subgroupSize >= 32 で選択、 P620 は wave 経路選択)。 Phase D-F は両派生とも LDS。 atomic counter は **device-local** `VmaBuffer::createDeviceLocal(STORAGE | TRANSFER_DST)` (vkCmdFillBuffer リセット・BDA/host-visible 不要)。
* **pass_chain 配線**: `mainPass_.execute()` 直後・ `overlayPass_.execute()` の前。 depth が `depth_layouts::readOnly(*ctx_)` に遷移済みのタイミング = mid-pass barrier 不要のクリーン経路。 descriptor write は `depth_layouts::readOnly()` 経由 (固定 `DEPTH_READ_ONLY_OPTIMAL` だと VUID 違反)。 `onSwapchainResized` で pyramid を depth 解像度に追従させて rebuild (pipeline / set layout は extent-independent で keep)。
* **同期 (Vulkan Memory Model 整合)**: last group ループ前に `memoryBarrierImage()` (device-scope・cross-group acquire)、 last group ループ内反復は `groupMemoryBarrier()` + `barrier()` (workgroup-scope・device-scope より軽量)。 `barrier()` 単独だと coherent storage-image 可視性が無いので image-memory fence 必須。
* **ソース review で見つかって修正したバグ**: (1) depth descriptor 固定 layout → `depth_layouts::readOnly()` 経由、 (2) last group ループに image-memory fence 追加、 (3) subgroup capability を BASIC+SHUFFLE に修正 (ARITH/QUAD 削除)、 (4) atomic counter を device-local 化、 (5) `kMaxFramesInFlight` を `FrameSync::MAX_FRAMES_IN_FLIGHT` 参照に。
* **検証**: `Vulkan ready` + 全 capability 検出・ HiZ 関連 VUID/leak/device-lost **ゼロ**。 残る validation warning 4 件は 4a-2 由来の outNormal/outMotion 1-attachment subpass の既存もので 4b と無関係。 描画見た目 (HZB viewer の min/max が mip ごとに保守的に縮約) は **ユーザー目視 OK 確認済み**。 **types.h 不変・§3-1a clean rebuild 不要・incremental build で通過**。
* **未対応 (別 commit 候補・side/MyEngine_HiZ_PART4_Design.md §6 「4b 完了後の残作業」)**: Obs B (TOP_OF_PIPE → NONE で sync2 best practice 化、 4d で巻き取り) / Obs C (RG32F format properties query + fallback、 P620 以外対応時) / Obs D (subgroup mapping を REQUIRE_FULL_SUBGROUPS 等で固定、 mobile 対応時)。 いずれも P620 では実害ゼロ・correctness 上の問題なし。

### PART4 §6 4a-2 完了 = depth-normal-motion MRT 受け皿 + OverlayPass 分離 (2026-05-28, commit ed0d80e)
* **main_pass の opaque を MRT 3 attachment 化** (HDR + GBuffer normal R10G10B10A2 octahedral + motion vector RG16F) + swapchain depth SAMPLED_BIT 化。 Phase 3 SS 効果 (SSAO/SSGI/SSR/DoF/TAA) と PART4 4b HZB が必要な受け皿が全部立った。
* **HUD/ImGui を OverlayPass という新 dynamic-rendering pass に分離**。 HudPipeline / ImGui は `depthAttachmentFormat = VK_FORMAT_UNDEFINED` で rebuild、 OverlayPass の BeginRendering は color-only (HDR LOAD) = mid-pass barrier / feedback-loop 心配なしのクリーン経路。
* **新規ファイル**: `overlay_pass.{h,cpp}`、 `gbuffer_debug_widget.{h,cpp}` (右上ドック ImGui::Image viewer)、 `depth_layouts.h` (separate vs combined depth/stencil layout 選択の一元化)、 `shared/gbuffer.glsl` (octahedral encode + motion vector helper)。
* **capability 追加**: `dynamicRendering` (Vulkan 1.3 core) + `separateDepthStencilLayouts` (Vulkan 1.2 core optional, fallback あり)。 `[Caps] ... dynamicRendering=1 separateDepthStencilLayouts=1` (P620 両対応)。
* **FrameUBO に `mat4 prevViewProj`** (Phase 3 TAA まで shader 改修ゼロで届く受け皿)。
* **教訓 (Work_Protocol §3-1a)**: 共有ヘッダ struct size 変更 (FrameUBO に prevViewProj 追加で 352B → 416B) で clean rebuild を怠ると mode select → 本編 startGame 遷移時のみ画面凍結 (TDR、 validation 無音) を起こす。 当初は depth layout / OverlayPass / GBuffer feedback loop を疑って多数の修正を入れたが効かず、 clean rebuild が決定打となって解消した。 根本原因の仮説 (FrameUBO レイアウト不一致で gl_Position が NaN/巨大値) は §3-1a 事例②参照。 §3-1a の徹底必須。

### PART4 §6 4a-1 完了 = main_pass を Vulkan 1.3 dynamic rendering 化 (2026-05-28, commit af3dd72)
* main_pass の VkRenderPass + VkFramebuffer を撤去、 `vkCmdBeginRendering` + `VkPipelineRenderingCreateInfo` ベースに。 子 pass (debug_line / hud / particle / water / imgui) も同経路に伝播。 4a-2 の MRT 拡張前提。
* `vulkan_context::dynamicRendering()` capability 追加。 attachment format 変更を VkRenderPass 再構築なしで吸収できる「最新 Vulkan 標準形」 (vkguide v1.3 / Khronos Vulkan-Samples)。

### PART4 §6 4-前-5 完了 = GPU-driven shadow (2026-05-28, commit 986ba44)
* ShadowPass の static draw を CullingPass の Shadow CullSet (新 enum `CullSet{Camera, Shadow}` で per-set output buffer 化) + `vkCmdDrawIndexedIndirectCount` 経由に。 main bucket と同形の compaction を shadow にも適用。 skinned shadow は legacy CPU loop 維持。 vkguide サンプル (main+shadow 両方 GPU-driven) と揃った。

### PART4 §6 4-前-4 完了 = 3-pass scan compaction + IndirectCount + DGC 受け皿 (2026-05-28, commit 15b89ad)
* `scan_local.comp` (subgroup ops + LDS で per-workgroup prefix) + `scan_globals.comp` (workgroup 間 prefix) + `scan_scatter.comp` (compact draw write) の3 pass scan。 `cull.comp` は predicate-only に縮小。 `indirect_exec::Mode{DGC, IndirectCount, Legacy}` で capability 別経路 picker。

### PART4 §6 4-前-3 完了 = persistent device-local CullObject + bit-packed visBuf + grow (2026-05-28, commit ec9c586)
* CullObject buffer を per-frame ring から persistent device-local + staging upload + sync2 barrier 経路に。 visBuf は 32 object / uint32 (`atomicOr/atomicAnd`)。 満杯時は `DeletionQueue` で旧バッファ遅延破棄。

### PART4 §6 4-前-2 完了 = meshlet-ready CullObject + cone test (2026-05-28, commit b8e39b2)
* CullObject に cone backface test 用フィールド + cluster ID (Nanite 風 DAG の受け皿) を追加。 builder で cone 充填。 `cull.comp` に cone test 受け皿 (object 粒度では cheap な追加 cull、 meshlet 段階で同じ関数が活きる)。

### PART4 §6 4-前-1 完了 = block sort + BlockRange 導入 (2026-05-28, commit ff9f7a9)
* `static_cull_build` で同一 GeometryBuffer block の prop を連続区間に並べ替え、 main_pass の区間検出 (前後比較ループ) を撤去して `BlockRange` 配列に置き換え。 シェーダ無改修・挙動不変。

### PART4 §6 4-前-0 完了 = Reverse-Z + 無限遠 perspective (2026-05-28, commit 702c773)
* 全 pass に Reverse-Z (clear=0.0, compareOp=GREATER) と無限遠 perspective を全面適用。 `include/MyEngine/renderer/projection.h` に `makeReversedZInfinitePerspective(fovY, aspect, zNear)` ヘルパ (Y-flip 込み)。 shadow / reflection / depth-related shader 全部に波及。 4b HZB の精度確保前提。

### Vulkan13 W 完了 = sync2 barrier helper (2026-05-28, commit e1494bf)
* `include/MyEngine/renderer/barrier.h` (header-only) を新設。 sync2 `VkDependencyInfo` を一括記録する `recordBatch` + 単発 `recordImage/recordBuffer`、 capability 未対応の sync1 fallback も同居。 ImageBarrier / BufferBarrier / MemoryBarrier 構造体 + QFOT パラメータ (streaming / async compute 用)。 5 サイト移行 (bloom mip / culling / shadow / mainPass のうち適用箇所)。


### 2026-05-27 — Phase 2B PART3c-2 (prop の indirect 差し替え・CPU draw 撤去) = Phase 2B 完了

prop bucket の GPU-driven 骨格が立ち上がった。PART2 で作った CullingPass (GPU がフラスタムカリングして instanceCount を生成) が、初めて実描画に接続された。ビルド通過・validation/VUID/leak ゼロ・prop 全部正常描画 (目視)・1 コミット (1cf23b9)。

* **indirect 差し替え**: main の prepared CPU draw ループ (`vkCmdDrawIndexed`) を `vkCmdDrawIndexedIndirect` に差し替え、CPU draw を撤去。CullingPass の `commandBuffer(frameIndex)` (instanceCount 0/1 入り) を描画ソースに配線 (pass_chain → MainPass::ExecuteInfo)。GPU が instanceCount==0 を自動スキップ = カリングが実描画に効く。シェーダ無改修 (PART3b の gl_InstanceIndex 経路のまま)。
* **能力チェック (確定事実)**: 起動時に `multiDrawIndirect` と **`drawIndirectFirstInstance`** を query し対応時のみ有効化。**P620 は両対応を実測** (`[Caps] 1/1`)。**`drawIndirectFirstInstance` が必須** — firstInstance に DrawData slot を載せて引くので、non-zero firstInstance を indirect で使うこの feature が要る (`multiDrawIndirect` だけでは不足。web/Vulkan 仕様で確認)。非対応なら CPU draw fallback (direct draw の firstInstance は無制限)。
* **block 散在 = 連続区間 indirect**: prop draw の blockIndex 分布は散在 (実測 blockSwitches≈17-18 / draws=77)。**「同一 GeometryBuffer block の連続区間ごとに 1 draw 呼び出し」** にする (対応=区間長ぶん単発 MDI、非対応=区間内 drawCount=1 ループ)。**「全 draw を1 MDI」は不可** (block 切替で vertex/index buffer の bind が変わる)。呼び出し回数を block ごとに減らすのは builder の blockIndex ソート (任意の後段最適化・今は不要)。
* **【史実】検証足場 (HUD)**: PART3c-2 当時はデバッグ HUD に `Cull : <可視> / <総数>` を追加。視点回転で分子が動く = GPU カリングが実描画に効いている。**当初 GPU=CPU オラクル照合 (緑/赤) を出したが、`lastGpuVisible` (前フレーム dispatch) と CPU オラクル (今フレーム) の基準が 1 フレームずれ、高速移動時に偽の不一致 (赤) が出たため撤去** (表示比較側のズレで、実バグではない)。CPU オラクル + readback (PART2 検証用) は残置・無害として残していた。 → ✅ **2026-05-29 commit f8d1e1f で全撤去** (Pure GPU-driven cleanup・上記付記参照)。

### 2026-05-27 — Phase 2B PART3c-1 (prop の SubMesh 粒度ビルダ) + terrain bucket 分離 (Phase 2F 新設)

prop の GPU-driven 化の土台 (3c-1) を完成。あわせて terrain を別 bucket として分離し直し、ロードマップに Phase 2F を新設。すべてビルド通過・validation/VUID/leak ゼロ・地形/prop 目視正常・都度 commit。

* **terrain を GeometryBuffer に統合 → 撤回 (ff920ae → 632433a)**: 3c-0 で terrain を prop の GeometryBuffer に載せたが、完成形「terrain は別 bucket」に反するため legacy CPU draw に戻した。`terrain_mesh.h/.cpp` の geom 対応コードは残置 (Phase 2F で再利用)。**教訓: スコープ判断は資料を確認してから (START_HERE §0-8 / Work_Protocol §1-4)。資料を読まず terrain 込みのスコープをユーザーに二択で出し、さらに「terrain は geom 化済み」と思い込みワールド地形 (legacy) を prepared 経路に乗せて描画破壊した。**
* **static_cull_build.h (88a041f)**: prop (cube + Model SubMesh) を SubMesh 粒度で走査し drawId 連番で DrawData slot (pushOne) + CullObject + DrawTemplate + PreparedDraw を同時生成する header-only ビルダ。drawId = DrawData slot = firstInstance = indirect command index で三者一致。pass_chain が reflection 後・cull 前に build → CullingPass に SubMesh 粒度で流す。main の prop opaque は prepared draws の CPU ループで描画 (block 切替対応、まだ indirect でない)。**GPU カリング = CPU オラクル一致 (gpu=cpu=56/78) で検証。**
* **Phase 2F 新設**: terrain bucket (専用 GeometryBuffer + 専用 cull + 距離 LOD + splat マテリアル + チャンクストリーミング) をロードマップの独立 Phase として受け皿化。地形マテリアル (土/岩/砂) はここの splat で扱う。

### 2026-05-27 — Phase 2B PART3b (per-draw SSBO + static shader 改修)

GPU-driven の前提 (b)(c) を達成。static の per-draw データを push constant から SSBO + gl_InstanceIndex 経路へ移し、indirect-ready にした。すべてビルド通過・validation/VUID/leak ゼロ・目視 (反射含む) 正常・都度 commit。

* **bindAndDraw 集約** (d6dbcde): Mesh/SubMesh に `bindAndDraw(cmd, instanceCount, firstInstance)` を追加。bind と firstIndex/vertexOffset が同一オブジェクトから出るので desync 不能 (PART3a の bind 取りこぼし device-lost の構造的対策)。firstInstance 引数が PART3c の受け皿。
* **static_draw.h 共通化** (a1b93cf): main_pass / reflection_pass に重複していた drawMeshList / drawStaticModelList / drawTerrainList を `renderer/static_draw.h` (header-only inline) に集約。materialId の出どころは `useRealMaterial` フラグで分岐 (main=実 ID / reflection=0)。挙動不変。
* **DrawData + DrawDataPool** (68a5c31): types.h に `DrawData` (mat4 model + uint materialId + float alpha + pad, 80B, std430)。`renderer/draw_data_pool.h` は header-only の per-frame 線形 SSBO+BDA プール (InstanceBufferPool と同型、`pushOne`→slot、`bufferAddress`)。容量は CullingPass::MAX_DRAWS に揃える (§5b の動的成長負債は cull cmds と共通で別途)。
* **per-draw SSBO 切替** (c5adced): 新 push constant `StaticDrawPushConstants` (uvec2 drawBuffer, 8B/VERTEX)。`triangle.vert` を `DrawBuffer(push.drawBuffer).data[gl_InstanceIndex]` 参照に改修し materialId を flat varying で frag へ。`triangle.frag` は push constant 撤去し flat in で受ける。両 static layout の push constant を縮小。static_draw.h の各ヘルパは `pool.pushOne(...)`→slot を `bindAndDraw(cmd,1,slot)` / terrain は `vkCmdDrawIndexed(...,slot)` の firstInstance に。terrain も同経路、shadow/skinned は別シェーダ・別 layout で不変。
* **教訓 (Work_Protocol に制度化)**: per-draw SSBO を複数パス (reflection→main) で共有するとき、cursor リセット (beginFrame) を**全消費者の前で1回だけ**置く。後段に置くと後のパスが前の slot を上書きし GPU 実行時に取り違える (目視では偶然映って気づけない = bloom Off レイアウトバグと同類)。beginFrame は ShadowPass 後・reflection ブロック前へ。手順面では複数行 if/else を対話プロンプトに貼らない (§2-7) を再確認。


### 2026-05-25 — Phase 1K (PBR 主要部) + Phase 2B PART2 (GPU-driven カリングの compute pass)

PBR の主要部を最新手法で固め、GPU-driven レンダリングの心臓部 (compute がカリングして indirect command を生成) を実装。すべてビルド通過・validation/VUID/leak ゼロ・検証・都度 commit。

* **Phase 1K-A** (964c733): Cook-Torrance BRDF を `shared/pbr.glsl` に集約。`pbrDirectLighting` は1ライト×1表面の純粋関数 (影/減衰/多光源を呼び出し側に逃がし、2A 多光源等に BRDF 無改修で拡張できる設計)。
* **Phase 1K-5** (593ef17 等): 法線マップを surface gradient bump mapping framework (Mikkelsen 2020、Unreal 採用) で実装。detail/地形/decal を勾配空間で線形加算してから一度だけ resolve する受け皿。頂点 tangent 不要 (dFdx/dFdy で cotangent frame)。
* **Phase 1K-4** (ddc5435 / トグル 23e1bac): metallic-roughness マップ。glTF の roughness=G/metallic=B を linear 読み。
* **bloom Off レイアウトバグ修正** (657e701): bloom 無効時に mip0 が GENERAL のまま残る既存バグ。教訓 = validation ログは `-First` で切らず全行見る。
* **Phase 2B PART1** (df9d843): CullObject 受け皿 (types.h CullObject 32B、aabb.h transformAABB/worldBoundingSphere、SceneData cullObjects_、scene_renderer で充填)。
* **Phase 2B PART2** (5cbc7e6): **エンジン2つ目の compute pass。GPU フラスタムカリング。** 新規 `CullingPass` (全 BDA・descriptor 無し) + `cull.comp` (local_size_x=64、buffer_reference で CullObject 読み・6平面球判定・instanceCount 0/1 書き込み) + `vma_buffer` の `createMappedStorageBDA` に `extraUsage` overload (INDIRECT 付き BDA buffer 用) + pass_chain 配線 (MainPass 直前 + COMPUTE→DRAW_INDIRECT バリア)。**【史実】GPU 可視数 = CPU Frustum オラクル一致で検証 (13/26)** (※ 検証用 CPU オラクル + 全 wire-up は commit f8d1e1f で純 GPU-driven 化撤去・上記付記「Pure GPU-driven cleanup」参照)。 MainPass はまだ CPU draw ループ。
* **方針上の学び**: PART3 で MainPass を indirect draw 化しようとして、static の構造 (SubMesh 別 buffer / draw 単位 = item×SubMesh / per-SubMesh push constant) が単純な差し替えを許さないと判明。**A 方針 (= メッシュ単一 buffer 統合 + per-draw SSBO + vertex shader 改修) がオープンワールド GPU-driven の本筋**と確認。compute 結果の CPU 読み戻しは per-frame fence 後 (次の同 frameIndex) に読む、という GPU↔CPU 同期の作法も確定。
* **2B PART3a 完了 (2026-05-26, commit ac7bbd1)**: 全 static prop ジオメトリ (cube/grass/Model SubMesh) を共有 megabuffer に統合 = indirect 化の前提 (a) 達成。**無制限 multi-block + VmaVirtualBlock (要素単位)** で、満杯時に新ブロック自動追加・容量で詰まらない。**学びの累積**: (1) 開発中前提で設計せよ=現状基準で固定容量を決めるな (1M 固定で armor が溢れた→作り直し)。(2) VmaVirtualBlock は要素単位で使う=76バイト非2べき乗 stride を byte で渡すと device lost (二分法 + 割り切れ DIAG で根本原因特定。validation 無言・GPU-AV は P620 で自滅)。(3) 同期コピー staging は即破棄、DeletionQueue は drawFrame 中の動的 free 専用。準備として DeletionQueue・device-local VmaBuffer + copyBufferRegion も新設。詳細は Work_Protocol §5b/§5c。

### 2026-05-25 — VmaImage 全完了 + Phase 1I compute bloom (エンジン初の compute pass)

image メモリの VMA 一本化と、その上での 1I を一気に進めた。すべてビルド通過・validation/VUID/leak ゼロ・目視確認・都度 commit。

* **VmaImage 新設** (commit b9ac20b): VkImage + VmaAllocation の move-only RAII。VmaBuffer と対称。
* **VmaImage 全移行** (d5e7eaf RenderTarget → ad8fa08 Texture → 70f30b6 swapchain depth)。ShadowPass output / ReflectionTarget も RenderTarget 経由で自動カバー。
* **deprecated image 経路削除** (1349a04): 未使用化した `ResourceFactory::createImage` / `createImageVMA` を削除。エンジン内に image 用の生 vkAllocateMemory は無い。
* **Phase 1I compute mip-chain bloom** (d03f3ff、後始末 194e73c): 旧 fragment 版を compute 版に全面置換。BloomPass が mip 列 (storage+sampled VmaImage 既定6段) を内包。Jimenez/CoD 方式 (bright → 13-tap downsample[mip0→1 Karis] → 3x3 tent upsample 加算)、全て vkCmdDispatch、render pass も framebuffer も無い。**エンジン初の compute pass**。確立した compute 作法は Codebase_Guide §3 に記録 (2B が踏襲)。未使用 fragment bloom シェーダ4本も削除。
* 方針上の学び (START_HERE §0 に判断原則として記録): **「最新技術で作る」と「保守性・散らからない」は同じ一つの基準**。最新の直接的なやり方を最初に選べばコードは最小になる。1I で fragment 版を組んでから compute 版に作り直した反省から確定。

### 2026-05-24/25 — リソース管理の全面 RAII 化 (段階 1 完了)

GPU リソースを `VkUnique` (ハンドル寿命) + `VmaBuffer` (VMA メモリ) の二層に一元化するリファクタを完遂。生メモリの手動 map/unmap・手動 vkDestroy*・生ハンドル管理を根絶。(その後 VmaImage を加えて三層化、上記参照)

* VMA 化: ParticlePass / DebugLinePass / FrameUniforms
* water 復元: s1_1 のくぼみの水面が出ていなかった件を修正 (push constant のモデル行列ゼロ初期化で全頂点が原点に潰れ、色 alpha 0 で透明化)。続けて water 一式 (mesh / pass / pipeline) を二層化
* VkUnique 化: BindlessTextureRegistry / FrameSync (command pool・semaphore・fence。present-wait semaphore は per-image 管理で reuse 警告も構造的に解消)
* 全描画パスの VkUnique 化: Shadow / Post / Bloom / Reflection / Main。全シーン (static/skinned/grass/bindless、water、debug line、particle、HUD、ImGui) が従来どおり、リーク・validation エラーゼロを確認

備考: 反射 (planar reflection) の視覚品質は意図的に据え置き。現代化は Phase 3-Refl で能力チェック + フォールバック付きで扱う。

### 2026-05-21 — Phase 1F (草・植生) 完成

* テクスチャ + メッシュ + 地形追従 + カリング + 統一 InstanceData + 色バリエーション + 風揺れ (設定トグル + 永続化)
* 既存バグ 3 件を根本修正: CMakeLists の post シェーダ COMMAND 欠落 (クリーンビルド破綻)、設定セーブ未配線 (persistDirty 未消費)、長年の FPS 劣化バグ (HUD draw list の clear 漏れ — drawFrame 区間計測 + 二分探索で特定)
* F8 デバッグ表示に静的オブジェクト (木・墓・宝箱) の当たり判定 AABB 可視化を追加
