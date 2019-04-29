#pragma once 

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class BonusHelper;
    typedef boost::shared_ptr<BonusHelper> BonusHelperPtr;

    class BonusHelper
    {
    public:
        BonusHelper(const CvCity* pCity);
        BonusHelperPtr clone() const;

        int getNumBonuses(BonusTypes bonusType) const;
        void changeNumBonuses(BonusTypes bonusType, int change);
        void allowOrDenyBonus(BonusTypes bonusType, bool allow);

    private:
        std::vector<int> bonuses_;
        std::vector<int> blockedBonuses_;
    };

    
}