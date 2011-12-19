#pragma once

#include "./utils.h"

namespace AltAI
{
    class ReligionHelper;
    class BonusHelper;

    class BuildingHelper
    {
    public:
        explicit BuildingHelper(const CvCity* pCity);

        int getNumBuildings(BuildingTypes buildingType) const;
        int getNumRealBuildings(BuildingTypes buildingType) const;
        int getNumFreeBuildings(BuildingTypes buildingType) const;

        PlayerTypes getBuildingOriginalOwner(BuildingTypes buildingType) const;
        void setBuildingOriginalOwner(BuildingTypes buildingType);

        void changeNumRealBuildings(BuildingTypes buildingType);
        void changeNumFreeBuildings(BuildingTypes buildingType);

        PlotYield getBuildingYieldChange(BuildingClassTypes buildingClassType) const;
        void setBuildingYieldChange(BuildingClassTypes buildingClassType, PlotYield plotYield);
        void changeBuildingYieldChange(BuildingClassTypes buildingClassType, PlotYield plotYield);

        Commerce getBuildingCommerceChange(BuildingClassTypes buildingClassType) const;
        void setBuildingCommerceChange(BuildingClassTypes buildingClassType, Commerce commerce);
        void changeBuildingCommerceChange(BuildingClassTypes buildingClassType, Commerce commerce);

        Commerce getBuildingCommerce(BuildingTypes buildingType, const boost::shared_ptr<ReligionHelper>& religionHelper) const;

        int getProductionModifier(BuildingTypes buildingType, const boost::shared_ptr<BonusHelper>& bonusHelper, const boost::shared_ptr<ReligionHelper>& religionHelper) const;

    private:
        const CvCity* pCity_;
        PlayerTypes owner_;

        std::vector<int> buildings_;
        std::vector<int> freeBuildings_;

        std::vector<PlayerTypes> buildingOriginalOwners_;

        std::map<BuildingClassTypes, PlotYield> buildingYieldsMap_;
        std::map<BuildingClassTypes, Commerce> buildingCommerceMap_;
    };
}