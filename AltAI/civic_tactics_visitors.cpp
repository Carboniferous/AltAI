#include "AltAI.h"

#include "./civic_tactics_visitors.h"
#include "./civic_info.h"
#include "./civic_tactics_items.h"
#include "./building_tactics_deps.h"
#include "./player.h"

namespace AltAI
{
    class MakeCivicTacticsVisitor : public boost::static_visitor<>
    {
    public:
        MakeCivicTacticsVisitor(const Player& player, CivicTypes civicType)
            : player_(player), civicType_(civicType), isEconomic_(false), isMilitary_(false)
        {
        }

        template <typename T>
                result_type operator() (const T&)
        {
        }

        void operator() (const CivicInfo::BaseNode& node)
        {
            if (node.health != 0)
            {
                isEconomic_ = true;
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }

            if (isEconomic_)
            {
                tacticsItems_.push_back(ICivicTacticPtr(new EconomicCivicTactic()));
            }

            pTactic_ = CivicTacticsPtr(new CivicTactics(player_.getPlayerID(), civicType_));

            for (size_t i = 0, count = tacticsItems_.size(); i < count; ++i)
            {
                pTactic_->addTactic(tacticsItems_[i]);
            }

            if (node.prereqTech != NO_TECH)
            {
                pTactic_->setTechDependency(ResearchTechDependencyPtr(new ResearchTechDependency(node.prereqTech)));
            }
        }

        void operator() (const CivicInfo::BuildingNode& node)
        {
            if (node.buildingType != NO_BUILDING && (node.happy != 0 || node.health != 0))
            {
                isEconomic_ = true;
            }
            // todo: node.specialBuildingTypeNotReqd
        }

        void operator() (const CivicInfo::ImprovementNode& node)
        {
            isEconomic_ = true;
        }

        void operator() (const CivicInfo::UnitNode& node)
        {
            if (node.freeExperience > 0)
            {
                isMilitary_ = true;
            }
            if (node.stateReligionFreeExperience > 0)
            {
                isMilitary_ = true;
                depItems_.push_back(IDependentTacticPtr(new StateReligionDependency()));
            }
        }

        void operator() (const CivicInfo::YieldNode& node)
        {
            if (!isEmpty(node.yieldModifier))
            {
                isEconomic_ = true;
            }
            if (!isEmpty(node.capitalYieldModifier))
            {
                isEconomic_ = true;
            }
        }

        void operator() (const CivicInfo::CommerceNode& node)
        {
            if (!isEmpty(node.commerceModifier) || !isEmpty(node.extraSpecialistCommerce) || node.freeSpecialists > 0)
            {
                isEconomic_ = true;
            }
            // todo: node.validSpecialists
        }

        void operator() (const CivicInfo::MaintenanceNode& node)
        {
            // todo
        }

        void operator() (const CivicInfo::HurryNode& node)
        {
            tacticsItems_.push_back(ICivicTacticPtr(new HurryCivicTactic(node.hurryType)));
        }

        void operator() (const CivicInfo::TradeNode& node)
        {
            if (node.extraTradeRoutes > 0 || node.noForeignTrade)
            {
                isEconomic_ = true;
            }
            // todo: node.noForeignCorporations
        }

        void operator() (const CivicInfo::HappyNode& node)
        {
            if (node.largestCityHappy > 0)
            {
                isEconomic_ = true;
            }
            if (node.happyPerUnit > 0)
            {
                isEconomic_ = true;
                tacticsItems_.push_back(ICivicTacticPtr(new HappyPoliceCivicTactic(node.happyPerUnit)));
            }
        }

        void operator() (const CivicInfo::MiscEffectNode& node)
        {
            // todo
        }

        const CivicTacticsPtr& getTactic() const
        {
            return pTactic_;
        }

    private:
        const Player& player_;
        CivicTypes civicType_;
        CivicTacticsPtr pTactic_;

        bool isEconomic_, isMilitary_;
        std::vector<ICivicTacticPtr> tacticsItems_;
        std::vector<IDependentTacticPtr> depItems_;
    };

    CivicTacticsPtr makeCivicTactics(const Player& player, const boost::shared_ptr<CivicInfo>& pCivicInfo)
    {
        MakeCivicTacticsVisitor visitor(player, pCivicInfo->getCivicType());
        boost::apply_visitor(visitor, pCivicInfo->getInfo());
        return visitor.getTactic();
    }
}