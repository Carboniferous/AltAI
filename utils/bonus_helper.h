#pragma once 

#include "./utils.h"

namespace AltAI
{
    class BonusHelper
    {
    public:
        explicit BonusHelper(const CvCity* pCity);

        int getNumBonuses(BonusTypes bonusType) const;
        void changeNumBonuses(BonusTypes bonusType, int change);
        void allowOrDenyBonus(BonusTypes bonusType, bool allow);

    private:
        std::vector<int> bonuses_;
        std::vector<int> blockedBonuses_;
    };
}