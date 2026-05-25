# START HERE — MyEngine 新規チャット用ブートストラップ

> このファイルは新規チャットの冒頭で毎回 Claude に渡す「正本への入口」。
> Claude はまずこのファイルを最後まで読み、下の【Claude への指示】に従うこと。
> 添付ファイル: この START_HERE と一緒に、ロードマップ本体・依存マップ・作業プロトコル・コードベースガイド (計5枚、下記 §3) も添付されている前提。

---

## 0. 最優先ルール (Claude はこれを最初に守る)

0. **【最優先の判断原則 / 2026-05-25 追記・改訂】最優先は「最新技術で作る」こと。「直接的・散らからない (保守性)」はそれに従属する副次基準にすぎない。**
   - **最新技術が第一基準。** 「直接的・散らからない」は、最新技術を実現し維持しやすくするための手段であって、最新を退ける理由には絶対にならない。両者が衝突したら必ず最新を取る。「直接的」は、複数のやり方が同じくらい最新なときに、その中から選ぶための副次基準にすぎない。**最新より直接性を優先するのは厳禁** (実際にそれをやってユーザーを激怒させた。二度とやらない)。
   - **「その機能の対象がまだ無いから最新版は不要」という論法は禁止。** エンジンは今まさに作っている最中なのだから、ある機能が使う対象 (例: 法線合成のための detail map・地形ブレンド・decal) が「まだ無い」のは当たり前。それを「無いから最新を入れない理由」にするのは、未来に作るものを今ある前提で評価する倒錯。**最新技術でエンジンを作るとは、受け皿を先に最新の形で用意し、中身を後から埋めていくことそのもの。** 今使わない経路は**スタブを置くかコメントで枠を残す**のが正しい。「今は縮退するから旧来版で」は却下。
     - 具体例 (1K-5 法線): surface gradient bump mapping framework (Mikkelsen 2020, Unreal 採用) が最新で、複数法線合成 (detail/地形/decal) に正しく対応する。合成対象が今無くても、surface gradient で書く。合成経路はスタブ/コメントで残す。「合成が無いから (2) 微分再構成でいい」は禁止された論法。
   - **古い手法で組んでから「最新をよこせ」と言われて作り直す順序は厳禁** (Phase 1I bloom で fragment 版を組んでから compute 版に切り替え時間を浪費した。これを繰り返さない)。各実装は最初から最新で決める。
   - **「最新か?」と「保守性は?」を毎回別々に問い直してユーザーを足止めしない。** 最新を選ぶ → その実現手段として最も直接的な書き方を採る、の一方向。保守性を独立の関門にしない。
   - 補足 (なお最新は多くの場合直接的でもある): bloom の compute 版は render pass も framebuffer もブレンド state も不要で storage image に直接読み書きするだけ = 最新かつ最小。最新を選ぶと結果的に散らからないことが多い。ただしこれは「最新だから直接的」であって「直接的だから採る」ではない。順序を逆にしない。

1. **正本の優先順位**: このファイル群 (START_HERE / ロードマップ本体 / 依存マップ / 作業プロトコル / コードベースガイドの5枚) が唯一の正。**過去チャットの記述と矛盾したら、必ずこのファイル群を優先する。** 過去チャット検索の結果が古い方針 (rev.1/rev.2 など) を指していても、添付された最新ファイルが正しい。
2. **着手前に方針を確認・復唱する**: いきなり作業を始めない。まずこのファイルとロードマップの現方針を読み、「今のゴール・現在地・次の一手」を自分の言葉で短く復唱してから動く。ユーザーがそれを見て方針のズレを正せるようにする。
3. **勝手に方針を変えない**: ゴール (§1) は不変。実装方針 (能力チェック + フォールバック等) も勝手に変えない。変更が必要と思ったら、進める前にユーザーに確認する。
4. **「承知しました」と言って違う方向に進む事故を避ける**: 過去に、合意したつもりで全く違う方針で進んでしまった事故がある。少しでも解釈に迷ったら、推測で進めず確認する。
5. **指示された範囲を勝手に広げない**: 「このファイルを直して」に対して全ファイル一括更新を始める等の過剰解釈をしない (過去に発生)。スコープが曖昧なら広げる前に確認する。
6. **資料は変更が確定するたび、その該当ファイルだけを都度修正して出力する**: 作業して状態が変わったら、ロードマップの「現在地」と、必要なら依存マップ等を更新した版を、変わったファイルだけ出力する (バッチしない)。ユーザーがそれを保存して次回また添付する運用。
7. **着手前に、添付5ファイルの本文を全部読めたか確認する**: パスだけ来て本文がコンテキストに無い場合がある (実際に起きた。特にロードマップは最大ファイルで本文未展開が多い)。5ファイル (START_HERE / ロードマップ本体 / 依存マップ / 作業プロトコル / コードベースガイド) それぞれの本文を読めているかを最初に確認し、欠けていればユーザーに再添付を依頼するか、ディスクから読み込んでから着手する。本文欠落のまま作業しない。

---

## 1. 不変のゴール (ブレてはいけない錨)

**大規模オープンワールドのゲームエンジン MyEngine を、GPU の性能に関係なく最新のグラフィックス技術を取り入れながら作る。**

- 「最新技術を体験・学習したい」が一番の動機。最新技術 (mesh shader / HW レイトレ / リアルタイム GI 等) も、能力チェック + フォールバック付きで**実装対象にする** (詳細はロードマップ §3)。
- 現在の開発 GPU は Quadro P620 (Pascal, VRAM 2GB)。最新機能の一部はこの GPU では動かない (フォールバックで動く) が、コードは書く。将来 RTX 世代 GPU に更新したら最新経路が自動で開く設計。
- 大規模前提。小さく作って後で作り直す事態を避ける。作業量が多いことは覚悟済み。

このゴールはチャットをまたいでも変わらない。各セッションはこのゴールに向かう一歩。

---

## 2. 現在地 (ここだけ毎セッション更新する)

- **直近の完了**:
  - **Phase 1G (PCF/PCSS ソフトシャドウ) 完了** (2026-05-25, commit 3436ae5)。新規共有シェーダ `include/MyEngine/shaders/shared/shadow_sampling.glsl` に Vogel ディスク PCF + PCSS (コンタクトハードニング) を集約し、4 つの lighting frag (triangle / triangle_instanced / triangle_skinned / triangle_bindless) から `sampleShadowFactor()` を呼ぶ形に。品質ノブ shadowParams.y = 0:hard(ガビガビ・低スペック用) / 1:Soft(Vogel PCF 16-tap) / 2:High(PCSS)。**shader のみ**で C++/types.h/サンプラ/descriptor は不変。
  - **VmaImage 化 完了 (image メモリ完全 VMA 一本化)** (2026-05-25)。`renderer/vma_image.h/.cpp` (VmaBuffer と対称の move-only RAII, VkImage + VmaAllocation) を新設 (commit b9ac20b)。RenderTarget (d5e7eaf) → Texture (ad8fa08) → swapchain depth (70f30b6) を順に移行 (ShadowPass output / ReflectionTarget も RenderTarget 経由で自動カバー)。最後に未使用化した `ResourceFactory::createImage` / `createImageVMA` を削除 (1349a04)。**エンジン内に image 用の生 vkAllocateMemory は無い。** 各段でビルド通過・validation/VUID/leak ゼロ・描画正常 (スクショ確認済み)。
  - **Phase 1I: compute mip-chain bloom 完成 (Jimenez/CoD, 2026-05-25, commit d03f3ff)。エンジン初の compute pass。** 旧 fragment 版 (2枚 ping-pong + 単一しきい値 + 9-tap Gaussian) を、BloomPass が mip 列 (storage+sampled VmaImage を既定6段) を内包する compute 実装に全面置換。bright→downsample(13-tap, mip0→1 Karis)→upsample(3x3 tent 加算) を全て vkCmdDispatch。render pass も framebuffer も無い。VulkanRenderer は bloom target を持たなくなり (bloomTargetA_/B_ 削除)、最終 bloom = mip0 を PostPass が合成 (post 無改修)。compute の作法 (8x8 workgroup / GENERAL 運用 / mip0 往復遷移 / COMPUTE→COMPUTE バリア) を確立し Codebase_Guide §3 に記録 (後続 compute Phase が踏襲)。ビルド通過・validation/VUID/leak ゼロ・bloom 目視確認済み。
  - 段階1 (リソース管理の全面 RAII 化) は、これで image 側も完全に閉じた。
  - **Phase 1K-A: PBR BRDF を `shared/pbr.glsl` に集約 (2026-05-25, commit 964c733)。** 4 lighting frag (triangle/instanced/skinned/bindless) に重複していた Cook-Torrance (GGX/Smith/Schlick) を共有 include に集約し、各 frag は `pbrDirectLighting()` + `pbrAmbient()` 呼び出しに。**`pbrDirectLighting` は1ライト×1表面の純粋関数で影・減衰・多光源・ambient を含まない** (オープンワールドの多光源 2A・ポイントライト減衰・ライト別影に BRDF を書き換えず拡張できる設計、ユーザーと議論して確定)。bindless のコピペズレも正版に統一。純粋リファクタで見た目ピクセル同一・validation/leak ゼロ確認済み。なお PBR 本体 (Cook-Torrance + GpuMaterial SSBO) は過去の 1K-2 で実装済みだった (今回それを集約した)。
- **次の推奨の一手 (着手順)**: **1K-5 (ノーマルマップ, 一番見た目に効く) → 1K-4 (metallic-roughness マップ) → 1K-6 (AO) → 2B (compute カリング)**。1K は PBR 本体・BRDF 集約まで完了済み。残りは types.h に枠だけある未使用テクスチャ index (normalIdx/mrIdx/aoIdx) を活かす作業。**1K-5 は surface gradient bump mapping framework (Mikkelsen 2020, Unreal 採用) で実装する** — これが最新で、オープンワールドの複数法線合成 (detail map・地形ブレンド・decal) に正しく対応する。合成対象は今まだ無いが §0 の原則どおり最新の枠で書き、合成経路はスタブ/コメントで残す。「合成が無いから微分再構成版で」は §0 で禁止された論法。pbr.glsl に法線摂動関数を足すだけで各 frag は最小改修。2B は 1I で確立した compute 作法 (Codebase_Guide §3) を踏襲。**graphics/compute 機能は着手時にまず最新動向を web 確認。**
  - 任意: 1I PART D (bloom 強度・段数を settings 連携・目視チューニング)、buffer 系 VMA 化 (mesh/model_loader/terrain_mesh/texture staging の createBuffer → VmaBuffer)。
  - 完了済みで一覧から外したもの: 1G ソフトシャドウ / VmaImage 全移行・deprecated image 削除 / 1I compute bloom + 未使用 fragment bloom 削除 / 1K-A BRDF 集約。
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

新規チャットでは以下を添付済みのはず。無ければユーザーに依頼するか、過去チャットを検索して最新版を探す (ただし §0-1 のとおり、添付された最新版が常に優先)。

1. **MyEngine_START_HERE.md** (このファイル) — 入口・ゴール・現在地・運用ルール
2. **MyEngine_Graphics_Roadmap_2026.md** (rev.3 以降) — 全 Phase の詳細、能力チェック+フォールバック設計 (§3)、620 動作注記、推奨の一手
3. **MyEngine_Phase_Dependencies.md** — Phase 依存関係、着手順序、二重作業を防ぐ要注意ノード
4. **MyEngine_Work_Protocol.md** (rev.2 以降) — 作業手順の正本。PowerShell 置換テンプレート、ビルド/起動/commit、VkUnique/VmaBuffer/VmaImage 化の鉄則、やらないことリスト、pdb ロック・対話プロンプト分岐の罠
5. **MyEngine_Codebase_Guide.md** (rev.2 以降) — コードベースの地図。三層ラッパー仕様 (VkUnique/VmaBuffer/VmaImage)、ファイル構成、確立済みの設計事実、用語集

読む順序: このファイル (ゴール・現在地) → ロードマップ本体 (何を) → 依存マップ (どの順で) → 作業プロトコル (どう作業するか) → コードベースガイド (どのファイル・どの仕様か)。
実装に着手する直前には特に 4 (作業プロトコル) と 5 (コードベースガイド) を読む。

---

## 4. 開発環境・作業上の約束 (ユーザー設定の要点)

- グラフィックス/Vulkan/ゲームエンジンの話題では、**必ず最新の技術動向を web 検索で確認してから**出力する。
- **推測でソースを修正・追加しない。必ず実ソースを確認してから実装する。** (新規チャットでは実ソースが見えないので、ユーザーに dump してもらってから置換する運用)
- ソースのインデントは 4 スペース。
- 環境は Windows / PowerShell。ユーザーがコマンドを実行して結果を貼る方式 (Claude は直接ファイルアクセス不可)。
- ビルド中はコンソールでマウス選択しない (pdb ロックでビルドが固まる既知の罠。固まったら taskkill /F /IM cl.exe /T と mspdbsrv.exe を kill して再ビルド)。
- 対話プロンプトに複数行 if/elseif/else を貼らない (孤立エラー + フラグ誤検出。作業プロトコル §2-7)。置換・新規 .cpp の最終判定は obj/exe のタイムスタンプを正とする (§3-1b)。

---

## 5. このブートストラップ運用そのものについて

- 新規チャットが重くなるのを避けるため、セッションは適宜新しく始める。記憶は引き継がれないので、毎回このファイル群を添付することで方針を引き継ぐ。
- メモリ機能や過去チャット検索は補助。**正本は添付するこのファイル群**。ここがブレ防止の要。
- 変更が確定するたびに Claude がその該当ファイルの更新版を出す → ユーザーが保存 → 次回添付、のループで運用する。
