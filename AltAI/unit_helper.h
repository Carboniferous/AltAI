#pragma once

#include "./utils.h"

namespace AltAI
{
    class UnitHelper;
    typedef boost::shared_ptr<UnitHelper> UnitHelperPtr;

    class UnitHelper
    {
    public:
        explicit UnitHelper(const CvCity* pCity);
        UnitHelperPtr clone() const;

        void changeBuildingDefence(int change);

        void changeUnitFreeExperience(int change);
        void changeDomainFreeExperience(DomainTypes domainType, int change);
        void changeUnitCombatTypeFreeExperience(UnitCombatTypes unitCombatType, int change);

        int getBuildingDefence() const;

        int getUnitFreeExperience(UnitTypes unitType) const;

    private:
        const CvCity* pCity_;

        int buildingDefence_;
        int unitFreeExperience_;
        std::vector<int> domainFreeExperience_, combatTypeFreeExperience_;
    };
}
