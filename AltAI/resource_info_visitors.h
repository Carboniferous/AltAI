#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;
    class ResourceInfo;

    boost::shared_ptr<ResourceInfo> makeResourceInfo(BonusTypes bonusType, PlayerTypes playerType);
    void streamResourceInfo(std::ostream& os, const boost::shared_ptr<ResourceInfo>& pResourceInfo);

    // current units requiring this resource as required and optional (and, or) - TODO exclude obsolete ones
    std::pair<int, int> getResourceMilitaryUnitCount(const boost::shared_ptr<ResourceInfo>& pResourceInfo);

    struct ResourceQuery
    {
        enum ResourceQueryFlags
        {
            // theoretically realisable value, actual value, unrealised value
            AssumeAllCitiesHaveResource, OnlyCitiesWithResource, OnlyCitiesWithoutResource
        };
    };

    struct ResourceHappyInfo
    {
        ResourceHappyInfo() : rawHappy(0), actualHappy(0), potentialHappy(0), unusedHappy(0)
        {
        }
        int rawHappy;
        std::vector<std::pair<BuildingTypes, int> > buildingHappy;
        int actualHappy, potentialHappy, unusedHappy;

        void debug(std::ostream& os) const;
    };

    ResourceHappyInfo getResourceHappyInfo(const boost::shared_ptr<ResourceInfo>& pResourceInfo, ResourceQuery::ResourceQueryFlags mode);

    struct ResourceHealthInfo
    {
        ResourceHealthInfo() : rawHealth(0), actualHealth(0), potentialHealth(0), unusedHealth(0)
        {
        }
        int rawHealth;
        std::vector<std::pair<BuildingTypes, int> > buildingHealth;
        int actualHealth, potentialHealth, unusedHealth;

        void debug(std::ostream& os) const;
    };

    ResourceHealthInfo getResourceHealthInfo(const boost::shared_ptr<ResourceInfo>& pResourceInfo);

    struct ResourceBuildingInfo
    {
        std::vector<std::pair<BuildingTypes, int> > buildingModifiers, nationalBuildingModifiers, globalBuildingModifiers;
    };

    ResourceBuildingInfo getResourceBuildingInfo(const boost::shared_ptr<ResourceInfo>& pResourceInfo);

    void updateRequestData(CityData& data, const boost::shared_ptr<ResourceInfo>& pResourceInfo, bool isAdding);

    std::vector<BuildTypes> getBuildTypes(const boost::shared_ptr<ResourceInfo>& pResourceInfo);

    TechTypes getTechForResourceReveal(const boost::shared_ptr<ResourceInfo>& pResourceInfo);
}
