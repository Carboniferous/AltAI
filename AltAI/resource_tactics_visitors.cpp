#include "AltAI.h"

#include "./resource_tactics_visitors.h"
#include "./resource_info.h"
#include "./resource_tactics.h"
#include "./building_tactics_deps.h"
#include "./player.h"
#include "./city.h"
#include "./happy_helper.h"
#include "./health_helper.h"
#include "./gamedata_analysis.h"

namespace AltAI
{
    class MakeResourceTacticsVisitor : public boost::static_visitor<>
    {
    public:
        explicit MakeResourceTacticsVisitor(const Player& player, BonusTypes bonusType)
            : player_(player), bonusType_(bonusType)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
        }

        result_type operator() (const ResourceInfo::BaseNode& node)
        {
            pTactic_ = ResourceTacticsPtr(new ResourceTactics(player_.getPlayerID(), bonusType_));
            if (node.baseHappy != 0 || node.baseHealth != 0)
            {
                pTactic_->addTactic(IResourceTacticPtr(new EconomicResourceTactic()));
            }

            TechTypes buildTech = GameDataAnalysis::getTechTypeForResourceBuild(bonusType_);
            if (buildTech != NO_TECH)
            {
                pTactic_->setTechDependency(ResearchTechDependencyPtr(new ResearchTechDependency(buildTech)));
            }

            for (size_t buildingIndex = 0, buildingCount = node.buildingNodes.size(); buildingIndex < buildingCount; ++buildingIndex)
            {
                (*this)(node.buildingNodes[buildingIndex]);
            }

            for (size_t unitIndex = 0, unitCount = node.unitNodes.size(); unitIndex < unitCount; ++unitIndex)
            {
                (*this)(node.unitNodes[unitIndex]);
            }

            for (size_t routeIndex = 0, routeCount = node.routeNodes.size(); routeIndex < routeCount; ++routeIndex)
            {
                (*this)(node.routeNodes[routeIndex]);
            }
        }

        result_type operator() (const ResourceInfo::BuildingNode& node)
        {
            if (node.productionModifier > 0)
            {
                pTactic_->addTactic(IResourceTacticPtr(new BuildingResourceTactic(node.buildingType, node.productionModifier)));
            }
        }

        result_type operator() (const ResourceInfo::UnitNode& node)
        {
            pTactic_->addTactic(IResourceTacticPtr(new UnitResourceTactic(node.unitType)));
        }

        result_type operator() (const ResourceInfo::RouteNode& node)
        {
            
        }

        const ResourceTacticsPtr& getTactic() const
        {
            return pTactic_;
        }

    private:
        const Player& player_;
        BonusTypes bonusType_;
        ResourceTacticsPtr pTactic_;
    };

    struct HasBuiltBuilding
    {
        explicit HasBuiltBuilding(BuildingTypes buildingType_) : buildingType(buildingType_) {}

        bool operator() (const std::pair<int, BuildingTypes>& builtBuilding) const
        {
            return builtBuilding.second == buildingType;
        }

        BuildingTypes buildingType;
    };

    class ResourceAffectsCityVisitor : public boost::static_visitor<bool>
    {
    public:
        ResourceAffectsCityVisitor(const CityDataPtr& pCityData, const ProjectionLadder& cityProjection)
          : cityProjection_(cityProjection), pCityData_(pCityData)
        {
            initialPop_ = pCityData_->getPopulation();
            happyCap_ = pCityData_->happyPopulation() - pCityData_->angryPopulation();
            healthCap_ = pCityData_->getHealthHelper()->goodHealth() - pCityData_->getHealthHelper()->badHealth();
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return false;
        }

        result_type operator() (const ResourceInfo::BaseNode& node)
        {
            for (size_t i = 0, count = cityProjection_.entries.size(); i < count; ++i)
            {
                // at or above happy cap 
                if (node.baseHappy != 0 && cityProjection_.entries[i].pop >= initialPop_ + happyCap_ + std::min<int>(0, node.baseHappy))
                {
                    return true;
                }
                // at or above health cap - takes into account some resources give -ve health - effectively lowering the base health cap
                if (node.baseHealth != 0 && cityProjection_.entries[i].pop >= initialPop_ + healthCap_ + std::min<int>(0, node.baseHealth))
                {
                    return true;
                }
            }

            for (size_t buildingIndex = 0, buildingCount = node.buildingNodes.size(); buildingIndex < buildingCount; ++buildingIndex)
            {
                if ((*this)(node.buildingNodes[buildingIndex]))
                {
                    return true;
                }
            }
            return false;
        }

        result_type operator() (const ResourceInfo::BuildingNode& node)
        {
            // assume resource is useful if we have a building built which has its construction rate affected by the resource
            return std::find_if(cityProjection_.buildings.begin(), cityProjection_.buildings.end(), HasBuiltBuilding(node.buildingType)) != cityProjection_.buildings.end();
        }

    private:
        CityDataPtr pCityData_;
        const ProjectionLadder& cityProjection_;
        int initialPop_, happyCap_, healthCap_;
    };

    ResourceTacticsPtr makeResourceTactics(const Player& player, const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        MakeResourceTacticsVisitor visitor(player, pResourceInfo->getBonusType());
        boost::apply_visitor(visitor, pResourceInfo->getInfo());
        return visitor.getTactic();
    }

    bool resourceCanAffectCity(City& city, const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        ResourceAffectsCityVisitor visitor(city.getCityData(), city.getBaseOutputProjection());
        return boost::apply_visitor(visitor, pResourceInfo->getInfo());
    }
}