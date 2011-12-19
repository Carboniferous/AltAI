#include "./resource_info_visitors.h"
#include "./resource_info.h"
#include "./resource_info_streams.h"
#include "./city_data.h"
#include "./health_helper.h"
#include "./happy_helper.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./iters.h"

namespace AltAI
{
    namespace
    {
        // update CityData with changes resulting from bonus being added/removed
        class CityOutputUpdater : public boost::static_visitor<>
        {
        public:
            CityOutputUpdater(CityData& data, bool isAdding) : data_(data), multiplier_(isAdding ? 1 : -1)
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
                    data_.healthHelper->changeBonusGoodHealthiness(multiplier_ * node.baseHealth);
                }
                else if (node.baseHealth < 0)
                {
                    data_.healthHelper->changeBonusBadHealthiness(multiplier_ * node.baseHealth);
                }

                if (node.baseHappy > 0)
                {
                    data_.happyHelper->changeBonusGoodHappiness(multiplier_ * node.baseHappy);
                }
                else if (node.baseHappy < 0)
                {
                    data_.happyHelper->changeBonusBadHappiness(multiplier_ * node.baseHappy);
                }

                for (size_t i = 0, count = node.buildingNodes.size(); i < count; ++i)
                {
                    (*this)(node.buildingNodes[i]);
                }
            }

            void operator() (const ResourceInfo::BuildingNode& node) const
            {
                if (node.bonusHealth > 0)
                {
                    data_.healthHelper->changeBonusGoodHealthiness(multiplier_ * node.bonusHealth);
                }
                else if (node.bonusHealth < 0)
                {
                    data_.healthHelper->changeBonusBadHealthiness(multiplier_ * node.bonusHealth);
                }

                if (node.bonusHappy > 0)
                {
                    data_.happyHelper->changeBonusGoodHappiness(multiplier_ * node.bonusHappy);
                }
                else if (node.bonusHappy < 0)
                {
                    data_.happyHelper->changeBonusBadHappiness(multiplier_ * node.bonusHappy);
                }
            }

        private:
            CityData& data_;
            int multiplier_;
        };
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
        explicit HappyInfoResourceVisitor(PlayerTypes playerType) : playerType_(playerType)
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
                int unhappyCitizens = std::max<int>(0, pLoopCity->unhappyLevel() - pLoopCity->happyLevel());

                int baseHappy = std::min<int>(node.baseHappy, unhappyCitizens);
                // have we got any left?
                unhappyCitizens = std::max<int>(0, unhappyCitizens - baseHappy);

                if (unhappyCitizens == 0)
                {
                    unusedHappy += node.baseHappy - baseHappy;
                }
                actualHappy += baseHappy;

                for (size_t i = 0, count = info.buildingHappy.size(); i < count; ++i)
                {
                    int buildingCount = pLoopCity->getNumBuilding(info.buildingHappy[i].first);
                    if (buildingCount > 0)
                    {
                        int thisBuildingCount = std::min<int>(buildingCount * info.buildingHappy[i].second, unhappyCitizens);
                        unhappyCitizens = std::max<int>(0, unhappyCitizens - thisBuildingCount);

                        if (unhappyCitizens == 0)
                        {
                            unusedHappy += buildingCount * info.buildingHappy[i].second - thisBuildingCount;
                        }

                        actualHappy += thisBuildingCount;
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
    };

    ResourceHappyInfo getResourceHappyInfo(const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        HappyInfoResourceVisitor visitor(pResourceInfo->getPlayerType());
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
                int unhealthyCitizens = pLoopCity->healthRate();

                int baseHealth = std::min<int>(node.baseHealth, unhealthyCitizens);
                // have we got any left?
                unhealthyCitizens = std::max<int>(0, unhealthyCitizens - baseHealth);

                if (unhealthyCitizens == 0)
                {
                    unusedHealth += node.baseHealth - baseHealth;
                }
                actualHealth += baseHealth;

                for (size_t i = 0, count = info.buildingHealth.size(); i < count; ++i)
                {
                    int buildingCount = pLoopCity->getNumBuilding(info.buildingHealth[i].first);
                    if (buildingCount > 0)
                    {
                        int thisBuildingCount = std::min<int>(buildingCount * info.buildingHealth[i].second, unhealthyCitizens);
                        unhealthyCitizens = std::max<int>(0, unhealthyCitizens - thisBuildingCount);

                        if (unhealthyCitizens == 0)
                        {
                            unusedHealth += buildingCount * info.buildingHealth[i].second - thisBuildingCount;
                        }

                        actualHealth += thisBuildingCount;
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

    void updateCityData(CityData& data, const boost::shared_ptr<ResourceInfo>& pResourceInfo, bool isAdding)
    {
        boost::apply_visitor(CityOutputUpdater(data, isAdding), pResourceInfo->getInfo());
        data.recalcOutputs();
    }
}