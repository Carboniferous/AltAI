#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class BuildingsHelper;
    typedef boost::shared_ptr<BuildingsHelper> BuildingsHelperPtr;

    class BuildingsHelper
    {
    public:
        explicit BuildingsHelper(const CvCity* pCity);
        BuildingsHelperPtr clone() const;

        int getNumBuildings(BuildingTypes buildingType) const;
        int getNumRealBuildings(BuildingTypes buildingType) const;
        int getNumActiveBuildings(BuildingTypes buildingType) const;
        int getNumFreeBuildings(BuildingTypes buildingType) const;

        int getBuildingOriginalTime(BuildingTypes buildingType) const;

        PlayerTypes getBuildingOriginalOwner(BuildingTypes buildingType) const;
        void setBuildingOriginalOwner(BuildingTypes buildingType);

        void changeNumRealBuildings(BuildingTypes buildingType, bool adding = true);
        void changeNumFreeBuildings(BuildingTypes buildingType, bool adding = true);

        PlotYield getBuildingYieldChange(BuildingClassTypes buildingClassType) const;
        void setBuildingYieldChange(BuildingClassTypes buildingClassType, PlotYield plotYield);
        void changeBuildingYieldChange(BuildingClassTypes buildingClassType, PlotYield plotYield);

        Commerce getBuildingCommerceChange(BuildingClassTypes buildingClassType) const;
        void setBuildingCommerceChange(BuildingClassTypes buildingClassType, Commerce commerce);
        void changeBuildingCommerceChange(BuildingClassTypes buildingClassType, Commerce commerce);

        Commerce getBuildingCommerce(const CityData& data, BuildingTypes buildingType) const;

        int getProductionModifier(const CityData& data, BuildingTypes buildingType) const;

        void updatePower(bool isDirty, bool isAdding);
        void updateAreaCleanPower(bool isAdding);

        bool isPower() const;
        bool isDirtyPower() const;
        bool isAreaCleanPower() const;
        int getPowerCount() const;
        int getDirtyPowerCount() const;

    private:
        const CvCity* pCity_;
        PlayerTypes owner_;

        std::vector<int> buildings_, realBuildings_, activeBuildings_;
        std::vector<int> freeBuildings_;

        std::vector<int> originalBuildTimes_;

        std::vector<PlayerTypes> buildingOriginalOwners_;

        std::map<BuildingClassTypes, PlotYield> buildingYieldsMap_;
        std::map<BuildingClassTypes, Commerce> buildingCommerceMap_;

        int powerCount_, dirtyPowerCount_;
        bool isPower_, isAreaCleanPower_;
    };
}