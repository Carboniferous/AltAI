#include "AltAI.h"

#include "./resource_tactics_visitors.h"
#include "./resource_info.h"
#include "./resource_tactics.h"
#include "./building_tactics_deps.h"
#include "./player.h"
#include "./city.h"
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
        }

        result_type operator() (const ResourceInfo::BuildingNode& node)
        {
            
        }

        result_type operator() (const ResourceInfo::UnitNode& node)
        {
            pTactic_->addTactic(IResourceTacticPtr(new UnitResourceTactic()));
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

    ResourceTacticsPtr makeResourceTactics(const Player& player, const boost::shared_ptr<ResourceInfo>& pResourceInfo)
    {
        MakeResourceTacticsVisitor visitor(player, pResourceInfo->getBonusType());
        boost::apply_visitor(visitor, pResourceInfo->getInfo());
        return visitor.getTactic();
    }
}