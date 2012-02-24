#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class BuildingsHelper
    {
    public:
        BuildingsHelper(const CvCity* pCity, CityData& data);

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

        Commerce getBuildingCommerce(BuildingTypes buildingType) const;

        int getProductionModifier(BuildingTypes buildingType) const;

        void updatePower(bool isDirty, bool isAdding);
        void updateAreaCleanPower(bool isAdding);

        bool isPower() const;
        bool isDirtyPower() const;
        bool isAreaCleanPower() const;
        int getPowerCount() const;
        int getDirtyPowerCount() const;

    private:
        CityData& data_;
        const CvCity* pCity_;
        PlayerTypes owner_;

        std::vector<int> buildings_;
        std::vector<int> freeBuildings_;

        std::vector<PlayerTypes> buildingOriginalOwners_;

        std::map<BuildingClassTypes, PlotYield> buildingYieldsMap_;
        std::map<BuildingClassTypes, Commerce> buildingCommerceMap_;

        int powerCount_, dirtyPowerCount_;
        bool isPower_, isAreaCleanPower_;
    };

    typedef boost::shared_ptr<BuildingsHelper> BuildingsHelperPtr;
}