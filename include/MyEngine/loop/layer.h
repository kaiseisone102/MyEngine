#pragma once
// =============================================================================
// layer.h — 画面レイヤー基底 + LayerStack へのコマンドインターフェース
// =============================================================================
// 設計:
//   ILayer = 1 つの画面 (タイトル / ゲーム本編 / 設定 / etc) の責務を持つ。
//   LayerStack はスタック構造で複数 Layer を重ね合わせ、 上から下に処理する。
//
//   LayerCommands は Layer から LayerStack を操作するためのインターフェース。
//   Layer は LayerStack の具体型を知らず、 LayerCommands 経由でしか操作できない。
//   これにより:
//     - Layer の単体テストで MockLayerCommands を渡せる
//     - 循環依存を防げる (Layer が LayerStack を直接持たない)
//     - 反復中の安全な遅延操作が実現できる
//
// 寿命:
//   Layer は std::unique_ptr で所有される。 LayerStack に渡すと所有が移る。
// =============================================================================

#include <memory>

class EventBus;
class ILayer;

// ─────────────────────────────────────────────────────────────────────────
// LayerStack を操作するための最小インターフェース。
// LayerStack 本体が実装する。 Layer は ctor で LayerCommands& を受け取り、
// このインターフェース経由でのみスタックを操作する。
//
// テスト時は MockLayerCommands を実装して push/pop/replace/quit の呼び出し
// 回数を検証できる。
// ─────────────────────────────────────────────────────────────────────────
class LayerCommands {
   public:
    virtual ~LayerCommands() = default;

    // 新しい Layer をスタック最上位に積む。
    // 反復中に呼ばれた場合は遅延適用される (flushPending で適用)。
    virtual void requestPush(std::unique_ptr<ILayer> layer) = 0;

    // 最上位の Layer を取り除く。
    virtual void requestPop() = 0;

    // 全 Layer をクリアして newLayer 1 つだけにする。
    virtual void requestReplace(std::unique_ptr<ILayer> newLayer) = 0;

    // メインループ終了を要求する。
    virtual void requestQuit() = 0;
};

// ─────────────────────────────────────────────────────────────────────────
// 画面レイヤー基底。
// 各画面 (TitleLayer / GameplayLayer / SettingsLayer 等) はこれを継承する。
//
// ライフサイクル:
//   onEnter()   - スタックに push された直後
//   onExit()    - スタックから pop される直前
//   onPause()   - 自分の上に別 Layer が push されてアクティブでなくなった時
//   onResume()  - 上の Layer が pop されてアクティブに戻った時
//
// フレーム処理 (毎フレーム下記順):
//   handleEvents() - EventBus を読んで自分宛のイベントを処理
//   update()       - シミュレーション・ロジック更新
//   render()       - 描画
//
// 描画スタック制御:
//   blocksUpdate() - 自分が最上位の時、 下の Layer の update() を抑制するか
//   blocksRender() - 自分が最上位の時、 下の Layer の render() を抑制するか
//
//   (例) SettingsLayer がオーバーレイ -> blocksUpdate=true, blocksRender=false
//        ゲームは止まるが背景に見える
// ─────────────────────────────────────────────────────────────────────────
class ILayer {
   public:
    virtual ~ILayer() = default;

    virtual void onEnter()  {}
    virtual void onExit()   {}
    virtual void onPause()  {}
    virtual void onResume() {}

    // EventBus を読んで自分宛のイベントを処理。 LayerCommands で
    // Layer の push/pop を要求できる。
    virtual void handleEvents(const EventBus& events, LayerCommands& cmds) {
        (void)events;
        (void)cmds;
    }

    // 1 フレームのロジック更新。
    // dt: 経過秒数
    // isTop: 自分がスタック最上位かどうか (= アクティブかどうか)
    virtual void update(float dt, bool isTop) {
        (void)dt;
        (void)isTop;
    }

    // 1 フレームの描画。
    virtual void render() {}

    // 自分が最上位の時、 下の Layer の update を止めるか。
    // (例) ポーズメニュー中はゲーム停止 -> true
    //      ロード画面中はバックグラウンドで進む -> false
    virtual bool blocksUpdate() const { return true; }

    // 自分が最上位の時、 下の Layer の render を止めるか。
    // (例) 全画面メニュー -> true
    //      半透明オーバーレイ -> false
    virtual bool blocksRender() const { return false; }
};
