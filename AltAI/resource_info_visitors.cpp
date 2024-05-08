#include "AltAI.h"

#include "./resource_info_visitors.h"
#include "./resource_info.h"
#include "./resource_info_streams.h"
#include "./city_data.h"
#include "./health_helper.h"
#include "./happy_helper.h"
#include "./building_helper.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./iters.h"

namespace AltAI
{
    namespace
    {
        // update CityData with changes resulting from bonus being added/removed
        class CityResourcesUpdater : public boost::static_visitor<>
        {
        public:
            CityResourcesUpdater(CityData& data, bool isAdding) : data_(data), multiplier_(isAdding ? 1 : -1)
            {
            }

            template <typename T>
                result_type operator() (const T&) const
            {
            }

            void operator() (const ResourceInfo::BaseNode& node) const
            {
                if (node.baseHealth > 0)
                {
                    data_.getHealthHelper()->changeBonusGoodHealthiness(multiplier_ * node.baseHealth);
                }
                else if (node.baseHealth < 0)
                {
                    data_.getHealthHelper()->changeBonusBadHealthiness(multiplier_ * node.baseHealth);
                }

                if (node.baseHappy > 0)
                {
                    data_.getHappyHelper()->changeBonusGoodHappiness(multiplier_ * node.baseHappy);
                }
                else if (node.baseHappy < 0)
                {
                    data_.getHappyHelper()->changeBonusBadHappiness(multiplier_ * node.baseHappy);
                }

                for (size_t i = 0, count = node.buildingNodes.size(); i < count; ++i)
                {
                    if (data_.getBuildingsHelper()->getNumBuildings(node.buildingNodes[i].buildingType) > 0)
                    {
                        (*this)(node.buildingNodes[i]);
                    }
                }
            }

            void operator() (const ResourceInfo::BuildingNode& node) const
            {
                if (node.bonusHealth > 0)
                {
                    data_.getHealthHelper()->changeBonusGoodHealthiness(multiplier_ * node.bonusHealth);
                }
                else if (node.bonusHealth < 0)
                {
                    data_.getHealthHelper()->changeBonusBadHealthiness(multiplier_ * node.bonusHealth);
                }

                if (node.bonusHappy > 0)
                {
                    data_.getHappyHelper()->changeBonusGoodHappiness(multiplier_ * node.bonusHappy);
                }
                else if (node.bonusHappy < 0)
                {
                    data_.getHappyHelper()->changeBonusBadHappiness(multiplier_ * node.bonusHappy);
                }
            }

        private:
            CityData& data_;
            int multiplier_;
        };
    }

    void ResourceHappyInfo::debug(std::ostream& os) const
    {
        os << "(ResourceHappyInfo): rawHappy = " << rawHappy << ", actualHappy = " << actualHappy 
           << ", potentialHappy = " << potentialHappy << ", unusedHappy = " << unusedHappy << " ";
        for (size_t i = 0, count = buildingHappy.size(); i < count; ++i)
        {
            os << gGlobals.getBuildingInfo(buildingHappy[i].first).getType() << " = " << buildingHappy[i].second << ", ";
        }
    }

    void ResourceHealthInfo::debug(std::ostream& os) const
    {
        os << "(ResourceHealthInfo): rawHealth = " << rawHealth << ", actualHealth = " << actualHealth 
           << ", potentialHealth = " << potentialHealth << ", unusedHealth = " << unusedHealth << " ";
        for (size_t i = 0, count = buildingHealth.size(); i < count; ++i)
        {
            os << gGlobals.getBuildingInfo(buildingHealth[i].first).getType() << " = " << buildingHealth[i].second << ", ";
        }
    }

    boost::shared_ptr<ResourceInfo> makeResourceInfo(BonusTypes bonusType, PlayerTypes playerType)
    {
        return boost::shared_ptr<ResourceInfo>(new ResourceInfo(bonusType, playerType));
    }

    void streamResourceInfo(std::ostream& os, const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        os << pResourceInfo->getInfo();
    }

    class MilitaryUnitResourceVisitor : public boost::static_visitor<>
    {
    public:
        template <typename T>
            result_type operator() (const T&) const
        {
        }

        result_type operator() (const ResourceInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.unitNodes.size(); i < count; ++i)
            {
                (*this)(node.unitNodes[i]);
            }
        }

        result_type operator() (const ResourceInfo::UnitNode& node)
        {
            (node.isAndRequirement ? andUnitTypes : orUnitTypes).push_back(node.unitType);
        }

        const std::vector<UnitTypes>& getAndUnitTypes() const
        {
            return andUnitTypes;
        }

        const std::vector<UnitTypes>& getOrUnitTypes() const
        {
            return orUnitTypes;
        }

    private:
        std::vector<UnitTypes> andUnitTypes, orUnitTypes;
    };

    std::pair<int, int> getResourceMilitaryUnitCount(const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        MilitaryUnitResourceVisitor visitor;
        boost::apply_visitor(visitor, pResourceInfo->getInfo());
        // todo - eliminate entries which are for obsoleted units
        return std::make_pair(visitor.getAndUnitTypes().size(), visitor.getOrUnitTypes().size());
    }

    class HappyInfoResourceVisitor : public boost::static_visitor<>
    {
    public:
        explicit HappyInfoResourceVisitor(PlayerTypes playerType, ResourceQuery::ResourceQueryFlags mode)
            : playerType_(playerType), mode_(mode)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
        }

        result_type operator() (const ResourceInfo::BaseNode& node)
        {
            info.rawHappy = node.baseHappy;

            for (size_t i = 0, count = node.buildingNodes.size(); i < count; ++i)
            {
                (*this)(node.buildingNodes[i]);
            }

            CityIter iter(CvPlayerAI::getPlayer(playerType_));
            CvCity* pLoopCity;
            // how many citizens are made happy now if we had the resource, how many would be happy if we had the requisite buildings,
            // and how many more could be made happy with the buildings we have.
            // E.g. suppose we have 1 unhappy citizen in a city, with a forge - and we analyse gold - this would give:
            // actualHappy = 1, potentialHappy = 0, unusedHappy = 1, but if the city had no forge, it should give:
            // actualHappy = 1, potentialHappy = 1, unusedHappy = 0
            int actualHappy = 0, potentialHappy = 0, unusedHappy = 0;
            while (pLoopCity = iter())
            {
                bool cityHasBonus = pLoopCity->hasBonus(node.bonusType);

                if (mode_ == ResourceQuery::OnlyCitiesWithResource && !cityHasBonus ||
                    mode_ == ResourceQuery::OnlyCitiesWithoutResource && cityHasBonus)
                {
                    continue;
                }

                int unhappyCitizens = std::max<int>(0, pLoopCity->unhappyLevel() - pLoopCity->happyLevel());

                const int baseHappy = std::min<int>(node.baseHappy, unhappyCitizens);
                actualHappy += baseHappy;

                // have we got any left?
                unhappyCitizens = std::max<int>(0, unhappyCitizens - baseHappy);

                if (unhappyCitizens == 0)
                {
                    unusedHappy += node.baseHappy - baseHappy;
                }

                for (size_t i = 0, count = info.buildingHappy.size(); i < count; ++i)
                {
                    const int buildingCount = pLoopCity->getNumBuilding(info.buildingHappy[i].first);
                    if (buildingCount > 0)
                    {
                        const int thisBuildingHappyCount = std::min<int>(buildingCount * info.buildingHappy[i].second, unhappyCitizens);
                        actualHappy += thisBuildingHappyCount;

                        unhappyCitizens = std::max<int>(0, unhappyCitizens - thisBuildingHappyCount);

                        if (unhappyCitizens == 0)
                        {
                            unusedHappy += buildingCount * info.buildingHappy[i].second - thisBuildingHappyCount;
                        }
                    }
                    else  // just assumes one building allowed (should use getCITY_MAX_NUM_BUILDINGS(), but this would be wrong if building is limited in some other way, e.g. wonders)
                    {
                        potentialHappy += info.buildingHappy[i].second;
                    }
                }
            }
            info.actualHappy = actualHappy;
            info.potentialHappy = potentialHappy;
            info.unusedHappy = unusedHappy;
        }

        result_type operator() (const ResourceInfo::BuildingNode& node)
        {
            info.buildingHappy.push_back(std::make_pair(node.buildingType, node.bonusHappy));
        }

        const ResourceHappyInfo& getInfo() const
        {
            return info;
        }

    private:
        PlayerTypes playerType_;
        ResourceHappyInfo info;
        ResourceQuery::ResourceQueryFlags mode_;
    };

    ResourceHappyInfo getResourceHappyInfo(const boost::shared_ptr<ResourceInfo>& pResourceInfo, ResourceQuery::ResourceQueryFlags mode)
    {
        HappyInfoResourceVisitor visitor(pResourceInfo->getPlayerType(), mode);
        boost::apply_visitor(visitor, pResourceInfo->getInfo());
        return visitor.getInfo();
    }

    class HealthInfoResourceVisitor : public boost::static_visitor<>
    {
    public:
        explicit HealthInfoResourceVisitor(PlayerTypes playerType) : playerType_(playerType)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
        }

        result_type operator() (const ResourceInfo::BaseNode& node)
        {
            info.rawHealth = node.baseHealth;

            for (size_t i = 0, count = node.buildingNodes.size(); i < count; ++i)
            {
                (*this)(node.buildingNodes[i]);
            }

            CityIter iter(CvPlayerAI::getPlayer(playerType_));
            CvCity* pLoopCity;
            int actualHealth = 0, potentialHealth = 0, unusedHealth = 0;
            while (pLoopCity = iter())
            {
                // todo - handle resources that create unhealthiness (e.g. coal/oil)
                int unhealthyCitizens = pLoopCity->healthRate();

                const int baseHealth = std::min<int>(node.baseHealth, unhealthyCitizens);
                // have we got any left?
                unhealthyCitizens = std::max<int>(0, unhealthyCitizens - baseHealth);

                if (unhealthyCitizens == 0)
                {
                    unusedHealth += node.baseHealth - baseHealth;
                }
                actualHealth += baseHealth;

                for (size_t i = 0, count = info.buildingHealth.size(); i < count; ++i)
                {
                    const int buildingCount = pLoopCity->getNumBuilding(info.buildingHealth[i].first);
                    if (buildingCount > 0)
                    {
                        const int thisBuildingHealthCount = std::min<int>(buildingCount * info.buildingHealth[i].second, unhealthyCitizens);
                        unhealthyCitizens = std::max<int>(0, unhealthyCitizens - thisBuildingHealthCount);

                        if (unhealthyCitizens == 0)
                        {
                            unusedHealth += buildingCount * info.buildingHealth[i].second - thisBuildingHealthCount;
                        }

                        actualHealth += thisBuildingHealthCount;
                    }
                    else  // just assumes one building allowed
                    {
                        potentialHealth += info.buildingHealth[i].second;
                    }
                }
            }
            info.actualHealth = actualHealth;
            info.potentialHealth = potentialHealth;
            info.unusedHealth = unusedHealth;
        }

        result_type operator() (const ResourceInfo::BuildingNode& node)
        {
            info.buildingHealth.push_back(std::make_pair(node.buildingType, node.bonusHealth));
        }

        const ResourceHealthInfo& getInfo() const
        {
            return info;
        }

    private:
        PlayerTypes playerType_;
        ResourceHealthInfo info;
    };

    ResourceHealthInfo getResourceHealthInfo(const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        HealthInfoResourceVisitor visitor(pResourceInfo->getPlayerType());
        boost::apply_visitor(visitor, pResourceInfo->getInfo());
        return visitor.getInfo();
    }

    void updateRequestData(CityData& data, const boost::shared_ptr<ResourceInfo>& pResourceInfo, bool isAdding)
    {
        boost::apply_visitor(CityResourcesUpdater(data, isAdding), pResourceInfo->getInfo());
        data.checkHappyCap();
        data.recalcOutputs();
    }

    std::vector<BuildTypes> getBuildTypes(const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        return pResourceInfo->getBuildTypes();
    }

    class ResourceRevealTechVisitor : public boost::static_visitor<>
    {
    public:
        template <typename T>
            result_type operator() (const T&) const
        {
        }

        result_type operator() (const ResourceInfo::BaseNode& node)
        {
            revealTech_ = node.revealTech;
        }

        TechTypes getRevealTech() const
        {
            return revealTech_;
        }

    private:
        TechTypes revealTech_;
    };    

    TechTypes getTechForResourceReveal(const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        ResourceRevealTechVisitor visitor;
        boost::apply_visitor(visitor, pResourceInfo->getInfo());
        return visitor.getRevealTech();
    }

    class BuildingInfoResourceVisitor : public boost::static_visitor<>
    {
    public:
        explicit BuildingInfoResourceVisitor(PlayerTypes playerType) : player_(CvPlayerAI::getPlayer(playerType))
        {
        }

        template <typename T>
            void operator() (const T&) const
        {
        }

        void operator() (const ResourceInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.buildingNodes.size(); i < count; ++i)
            {
                (*this)(node.buildingNodes[i]);
            }
        }

        void operator() (const ResourceInfo::BuildingNode& node)
        {
            if (node.productionModifier > 0)
            {
                const BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(node.buildingType).getBuildingClassType();
                if (player_.isBuildingClassMaxedOut(buildingClassType, gGlobals.getBuildingClassInfo(buildingClassType).getExtraPlayerInstances()))
                {
                    return;
                }

                const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = !isWorldWonder && isNationalWonderClass(buildingClassType);
                if (isWorldWonderClass(buildingClassType))
                {
                    info.globalBuildingModifiers.push_back(std::make_pair(node.buildingType, node.productionModifier));
                }
                else if (isNationalWonderClass(buildingClassType))
                {
                    info.nationalBuildingModifiers.push_back(std::make_pair(node.buildingType, node.productionModifier));
                }
                else
                {
                    info.buildingModifiers.push_back(std::make_pair(node.buildingType, node.productionModifier));
                }
            }
        }

        const ResourceBuildingInfo& getInfo() const
        {
            return info;
        }

    private:
        const CvPlayerAI& player_;
        ResourceBuildingInfo info;
    };

    ResourceBuildingInfo getResourceBuildingInfo(const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        BuildingInfoResourceVisitor visitor(pResourceInfo->getPlayerType());
        boost::apply_visitor(visitor, pResourceInfo->getInfo());
        return visitor.getInfo();
    }
}