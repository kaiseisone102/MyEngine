#pragma once
// =============================================================================
// core/money.h — お金システム
// =============================================================================
// CMoney      : プレイヤーが所持してる金額 (int)
// MoneyType   : Coin / CoinBag / Diamond (拾い物の種類)
// CMoneyPickup: ステージ上に落ちてるアイテム
// MoneyItemTag: タグ
//
// 加算値:
//   Coin     = +1
//   CoinBag  = +5
//   Diamond  = +10
// =============================================================================

enum class MoneyType {
    Coin,
    CoinBag,
    Diamond,
};

inline int moneyValue(MoneyType t) {
    switch (t) {
        case MoneyType::Coin:    return 1;
        case MoneyType::CoinBag: return 5;
        case MoneyType::Diamond: return 10;
    }
    return 0;
}

inline const char* moneyTypeName(MoneyType t) {
    switch (t) {
        case MoneyType::Coin:    return "Coin";
        case MoneyType::CoinBag: return "CoinBag";
        case MoneyType::Diamond: return "Diamond";
    }
    return "?";
}

inline const char* moneyModelName(MoneyType t) {
    switch (t) {
        case MoneyType::Coin:    return "coin";
        case MoneyType::CoinBag: return "coin_bag";
        case MoneyType::Diamond: return "diamond";
    }
    return nullptr;
}

struct CMoney {
    int amount = 0;
};

struct CMoneyPickup {
    MoneyType type = MoneyType::Coin;
};

struct MoneyItemTag {};
