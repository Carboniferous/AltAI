#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;
    class PlayerAnalysis;

    class ModifiersHelper;
    typedef boost::shared_ptr<ModifiersHelper> ModifiersHelperPtr;

    class ModifiersHelper
    {
    public:
        explicit ModifiersHelper(const CvCity* pCity);
        ModifiersHelperPtr clone() const;

        YieldModifier getTotalYieldModifier(const CityData& data) const;
        CommerceModifier getTotalCommerceModifier() const;

        int getUnitProductionModifier(UnitTypes unitType) const;
        int getBuildingProductionModifier(const CityData& data, BuildingTypes buildingType) const;
        int getProjectProductionModifier(const CityData& data, ProjectTypes projectType) const;

        void changeYieldModifier(YieldModifier modifier);
        void changePowerYieldModifier(YieldModifier modifier);
        void changeBonusYieldModifier(YieldModifier modifier);
        void changeAreaYieldModifier(YieldModifier modifier);
        void changePlayerYieldModifier(YieldModifier modifier);
        void changeCapitalYieldModifier(YieldModifier modifier);

        void changeCommerceModifier(CommerceModifier modifier);
        void changePlayerCommerceModifier(CommerceModifier modifier);
        void changeCapitalCommerceModifier(CommerceModifier modifier);

        void changeStateReligionBuildingProductionModifier(int change);
        void changeMilitaryProductionModifier(int change);
        void changeSpaceProductionModifier(int change);
        void changePlayerSpaceProductionModifier(int change);

    private:
        boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis_;

        YieldModifier yieldModifier_;
        YieldModifier powerYieldModifier_, bonusYieldModifier_, areaYieldModifier_, playerYieldModifier_, capitalYieldModifier_;

        int militaryProductionModifier_;
        int worldWonderProductionModifier_, teamWonderProductionModifier_, nationalWonderProductionModifier_;
        int stateReligionBuildingProductionModifier_;
        int spaceProductionModifier_, playerSpaceProductionModifier_;

        CommerceModifier commerceModifier_;
        CommerceModifier playerCommerceModifier_, capitalCommerceModifier_;
    };
}