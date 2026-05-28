#include "core/event_bus.h"

void EventBus::push(const GameEvent& ev) {
    queue_.push_back(ev);  // push_back: append to end
}

void EventBus::push(GameEvent&& ev) {
    queue_.push_back(std::move(ev));  // push_back: append to end
}

std::vector<GameEvent> EventBus::drain() {
    std::vector<GameEvent> out;
    // Pass all accumulated events out and leave the bus empty
    out.swap(queue_);  // swap: exchange contents with another vector
    return out;
}

void EventBus::clear() {
    queue_.clear();  // clear: remove all elements
}

bool EventBus::empty() const {
    return queue_.empty();  // empty: is the queue empty
}
