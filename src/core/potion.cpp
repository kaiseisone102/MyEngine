// =============================================================================
// core/potion.cpp
// =============================================================================
#include "core/potion.h"

namespace potion {

const char* modelName(PotionType type) {
    switch (type) {
        case PotionType::Small: return "potion_s";
    }
    return "potion_s";
}

const char* typeName(PotionType type) {
    switch (type) {
        case PotionType::Small: return "Small Potion";
    }
    return "Potion";
}

int healAmount(PotionType type) {
    switch (type) {
        case PotionType::Small: return 1;
    }
    return 1;
}

}  // namespace potion
