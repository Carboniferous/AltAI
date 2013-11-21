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

    struct ResourceHappyInfo
    {
        ResourceHappyInfo() : rawHappy(0), actualHappy(0), potentialHappy(0), unusedHappy(0)
        {
        }
        int rawHappy;
        std::vector<std::pair<BuildingTypes, int> > buildingHappy;
        int actualHappy, potentialHappy, unusedHappy;
    };

    ResourceHappyInfo getResourceHappyInfo(const boost::shared_ptr<ResourceInfo>& pResourceInfo);

    struct ResourceHealthInfo
    {
        ResourceHealthInfo() : rawHealth(0), actualHealth(0), potentialHealth(0), unusedHealth(0)
        {
        }
        int rawHealth;
        std::vector<std::pair<BuildingTypes, int> > buildingHealth;
        int actualHealth, potentialHealth, unusedHealth;
    };

    ResourceHealthInfo getResourceHealthInfo(const boost::shared_ptr<ResourceInfo>& pResourceInfo);

    void updateCityData(CityData& data, const boost::shared_ptr<ResourceInfo>& pResourceInfo, bool isAdding);

    std::vector<BuildTypes> getBuildTypes(const boost::shared_ptr<ResourceInfo>& pResourceInfo);
}
