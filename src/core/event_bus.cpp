#include "core/event_bus.h"

void EventBus::push(const GameEvent& ev) {
    queue_.push_back(ev);  // push_back: 末尾に追加
}

void EventBus::push(GameEvent&& ev) {
    queue_.push_back(std::move(ev));  // push_back: 末尾に追加
}

std::vector<GameEvent> EventBus::drain() {
    std::vector<GameEvent> out;
    // 溜めていたイベントをすべて渡して、バス側は空にする
    out.swap(queue_);  // swap: 別の vector と中身を交換
    return out;
}

void EventBus::clear() {
    queue_.clear();  // clear: 要素をすべて削除
}

bool EventBus::empty() const {
    return queue_.empty();  // empty: 空かどうか
}
