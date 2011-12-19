#include "./bonus_helper.h"

namespace AltAI
{
    BonusHelper::BonusHelper(const CvCity* pCity)
    {
        const int bonusCount = gGlobals.getNumBonusInfos();
        bonuses_.resize(bonusCount, 0);
        blockedBonuses_.resize(bonusCount, 0);

        for (int i = 0; i < bonusCount; ++i)
        {
            blockedBonuses_[i] = pCity->getNoBonusCount((BonusTypes)i);
            if (blockedBonuses_[i] > 0)
            {
                // use area to get bonus count, as getNumBonuses() will return 0 regardless of actual number
                bonuses_[i] = pCity->area()->getNumBonuses((BonusTypes)i);
            }
            else
            {
                bonuses_[i] = pCity->getNumBonuses((BonusTypes)i);
            }
            
        }
    }

    int BonusHelper::getNumBonuses(BonusTypes bonusType) const
    {
        return blockedBonuses_[bonusType] == 0 ? bonuses_[bonusType] : 0;
    }

    void BonusHelper::changeNumBonuses(BonusTypes bonusType, int change)
    {
        bonuses_[bonusType] = std::max<int>(0, bonuses_[bonusType] + change);
    }

    void BonusHelper::allowOrDenyBonus(BonusTypes bonusType, bool allow)
    {
        blockedBonuses_[bonusType] = std::max<int>(0, blockedBonuses_[bonusType] + (allow ? -1 : 1));
    }
}