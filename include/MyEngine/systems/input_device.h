#pragma once
// =============================================================================
// input_device.h — 入力デバイス抽象インターフェース
// =============================================================================
// poll に float dt を渡すことで、 デバイスが時間ベースの処理を行えるように
// する (例: キーリピート、 デッドゾーン、 アナログスティックの時間方向平滑化)。
// =============================================================================

class EventBus;
struct ActionState;

class IInputDevice {
   public:
    virtual ~IInputDevice() = default;

    // 1 フレーム分の入力を取り込む。
    // - dt    : このフレームの経過時間 (秒)。 リピート判定等に使う。
    // - events: 単発イベントを push
    // - state : 連続状態をマージ
    virtual void poll(float dt, EventBus& events, ActionState& state) = 0;
};
