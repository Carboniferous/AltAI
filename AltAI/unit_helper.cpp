#include "AltAI.h"

#include "./unit_helper.h"

namespace AltAI
{
    UnitHelper::UnitHelper(const CvCity* pCity) : pCity_(pCity)
    {
        buildingDefence_ = pCity->getBuildingDefense();

        unitFreeExperience_ = pCity->getFreeExperience();
        for (int i = 0; i < NUM_DOMAIN_TYPES; ++i)
        {
            domainFreeExperience_.push_back(pCity->getDomainFreeExperience((DomainTypes)i));
        }

        for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
        {
            combatTypeFreeExperience_.push_back(pCity->getUnitCombatFreeExperience((UnitCombatTypes)i));
        }
    }

    UnitHelperPtr UnitHelper::clone() const
    {
        UnitHelperPtr copy = UnitHelperPtr(new UnitHelper(*this));
        return copy;
    }

    void UnitHelper::changeBuildingDefence(int change)
    {
        buildingDefence_ += change;
        buildingDefence_ = std::max<int>(0, buildingDefence_);
    }

    void UnitHelper::changeUnitFreeExperience(int change)
    {
        unitFreeExperience_ += change;
    }

    void UnitHelper::changeDomainFreeExperience(DomainTypes domainType, int change)
    {
        domainFreeExperience_[domainType] += change;
    }

    void UnitHelper::changeUnitCombatTypeFreeExperience(UnitCombatTypes unitCombatType, int change)
    {
        combatTypeFreeExperience_[unitCombatType] += change;
    }

    int UnitHelper::getBuildingDefence() const
    {
        // handle whether the unit ignores building defence in caller
        return buildingDefence_;
    }

    int UnitHelper::getUnitFreeExperience(UnitTypes unitType) const
    {
        int experience = 0;
        if (unitType != NO_UNIT)
        {
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);
            if (unitInfo.getCombat() > 0 && unitInfo.getUnitCombatType() != NO_UNITCOMBAT)
            {
                experience = unitFreeExperience_;
                experience += domainFreeExperience_[unitInfo.getDomainType()];
                experience += combatTypeFreeExperience_[unitInfo.getUnitCombatType()];
            }
        }
        return experience;
    }
}