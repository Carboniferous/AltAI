#pragma once 

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class BonusHelper
    {
    public:
        BonusHelper(const CvCity* pCity, CityData& data);

        int getNumBonuses(BonusTypes bonusType) const;
        void changeNumBonuses(BonusTypes bonusType, int change);
        void allowOrDenyBonus(BonusTypes bonusType, bool allow);

    private:
        CityData& data_;
        std::vector<int> bonuses_;
        std::vector<int> blockedBonuses_;
    };

    typedef boost::shared_ptr<BonusHelper> BonusHelperPtr;
}