#pragma once
// =============================================================================
// core/key.h — 鍵システム (カウント方式)
// =============================================================================
// KeyType 別にカウントを持ち、 ゲートを開ける度に 1 消費する。
// 将来 Master、 Boss 等を追加する場合は KeyType enum と CKeyInventory の
// switch 文に追加する。
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

enum class KeyType {
    Gold,
    Silver,
};

inline const char* keyTypeName(KeyType k) {
    switch (k) {
        case KeyType::Gold:   return "Gold";
        case KeyType::Silver: return "Silver";
    }
    return "Unknown";
}

struct CKeyInventory {
    int goldKeys = 0;
    int silverKeys = 0;

    int count(KeyType k) const {
        switch (k) {
            case KeyType::Gold:   return goldKeys;
            case KeyType::Silver: return silverKeys;
        }
        return 0;
    }

    void give(KeyType k, int n = 1) {
        switch (k) {
            case KeyType::Gold:   goldKeys   += n; break;
            case KeyType::Silver: silverKeys += n; break;
        }
    }

    // 1 つ消費。 持ってなければ false を返してカウンタは変更しない。
    bool consume(KeyType k) {
        switch (k) {
            case KeyType::Gold:
                if (goldKeys > 0)   { goldKeys--;   return true; }
                return false;
            case KeyType::Silver:
                if (silverKeys > 0) { silverKeys--; return true; }
                return false;
        }
        return false;
    }

    bool has(KeyType k) const { return count(k) > 0; }
};

struct CKeyPickup {
    KeyType type = KeyType::Gold;
};

struct KeyItemTag {};
