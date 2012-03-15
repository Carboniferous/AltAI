#include "./unit_tactics_visitors.h"
#include "./unit_info.h"
#include "./unit_info_visitors.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./unit_analysis.h"
#include "./helper_fns.h"
#include "./city.h"
#include "./civ_helper.h"
#include "./civ_log.h"

namespace AltAI
{
    class MakeEconomicUnitConditionsVisitor : public boost::static_visitor<>
    {
    public:
        MakeEconomicUnitConditionsVisitor(const Player& player, UnitTypes unitType) : player_(player), constructItem_(unitType)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }

            if (node.domainType != DOMAIN_LAND)
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Explore;
            }
        }

        void operator() (const UnitInfo::BuildNode& node)
        {
            for (size_t i = 0, count = node.buildTypes.size(); i < count; ++i)
            {
                constructItem_.possibleBuildTypes.insert(std::make_pair(node.buildTypes[i], 0));
            }
        }

        void operator() (const UnitInfo::ReligionNode& node)
        {
            if (player_.getCvPlayer()->getHasReligionCount(node.prereqReligion) > 0)
            {
                if (!node.religionSpreads.empty())
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Culture;
                    constructItem_.economicFlags |= EconomicFlags::Output_Happy;

                    for (size_t i = 0, count = node.religionSpreads.size(); i < count; ++i)
                    {
                        constructItem_.religionTypes.push_back(node.religionSpreads[i].first);
                    }
                }
            }
        }

        void operator() (const UnitInfo::MiscAbilityNode& node)
        {
            if (node.canFoundCity)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Settler;
            }
            if (node.betterHutResults)
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Explore;
            }
        }

        ConstructItem getConstructItem() const
        {
            return constructItem_.isEmpty() ? ConstructItem(NO_UNIT): constructItem_;
        }

    private:
        const Player& player_;
        ConstructItem constructItem_;
    };

    class MakeMilitaryExpansionUnitConditionsVisitor : public boost::static_visitor<>
    {
    public:
        MakeMilitaryExpansionUnitConditionsVisitor(const Player& player, UnitTypes unitType) : player_(player), constructItem_(unitType)
        {
            pUnitAnalysis_ = player_.getAnalysis()->getUnitAnalysis();
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }

            if (node.domainType != DOMAIN_LAND)
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Explore;
            }
        }

        void operator() (const UnitInfo::CityCombatNode& node)
        {
            if (node.extraDefence > 0)
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Defence;
            }
            if (node.extraAttack > 0)
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_City_Attack;
            }
        }

        void operator() (const UnitInfo::CollateralNode& node)
        {
            constructItem_.militaryFlags |= MilitaryFlags::Output_Collateral;
            constructItem_.militaryFlagValuesMap.insert(std::make_pair(MilitaryFlags::Output_Collateral,
                std::make_pair(pUnitAnalysis_->getCityAttackUnitValue(constructItem_.unitType), -1)));
            // TODO: add another entry for field collateral?
        }

        void operator() (const UnitInfo::CombatNode& node)
        {
            int attackValue = pUnitAnalysis_->getAttackUnitValue(constructItem_.unitType);
            int defenceValue = pUnitAnalysis_->getDefenceUnitValue(constructItem_.unitType);

            if (attackValue > 0)
            {
                constructItem_.militaryFlagValuesMap.insert(std::make_pair(MilitaryFlags::Output_Attack, std::make_pair(attackValue, -1)));
            }

            if (defenceValue > 0)
            {
                constructItem_.militaryFlagValuesMap.insert(std::make_pair(MilitaryFlags::Output_Defence, std::make_pair(defenceValue, -1)));
            }

            int cityAttackValue = pUnitAnalysis_->getCityAttackUnitValue(constructItem_.unitType);
            if (cityAttackValue > 0)
            {
                constructItem_.militaryFlagValuesMap.insert(std::make_pair(MilitaryFlags::Output_City_Attack, std::make_pair(cityAttackValue, -1)));
            }

            int cityDefenceValue = pUnitAnalysis_->getCityDefenceUnitValue(constructItem_.unitType);
            if (cityDefenceValue > 0)
            {
                constructItem_.militaryFlagValuesMap.insert(std::make_pair(MilitaryFlags::Output_City_Defence, std::make_pair(cityDefenceValue, -1)));
            }
            
            for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
            {
                int counterValue = pUnitAnalysis_->getUnitCounterValue(constructItem_.unitType, (UnitCombatTypes)i);
                if (counterValue > 0)
                {
                    constructItem_.militaryFlagValuesMap.insert(std::make_pair(MilitaryFlags::Output_UnitCombat_Counter, std::make_pair(counterValue, (UnitCombatTypes)i)));
                }
            }

            if (!constructItem_.militaryFlagValuesMap.empty())
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Combat_Unit;
            }

            if (node.moves > 1)
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Extra_Mobility;
            }
        }

        void operator() (const UnitInfo::CargoNode& node)
        {
            if (node.cargoDomain == DOMAIN_LAND)
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Unit_Transport;
            }
        }

        ConstructItem getConstructItem() const
        {
            return constructItem_.isEmpty() ? ConstructItem(NO_UNIT): constructItem_;
        }

    private:
        const Player& player_;
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis_;
        ConstructItem constructItem_;
    };

    ConstructItem getEconomicUnitTactics(const Player& player, UnitTypes unitType, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        MakeEconomicUnitConditionsVisitor visitor(player, unitType);
        boost::apply_visitor(visitor, pUnitInfo->getInfo());

        return visitor.getConstructItem();
    }

    ConstructItem getMilitaryExpansionUnitTactics(const Player& player, UnitTypes unitType, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        MakeMilitaryExpansionUnitConditionsVisitor visitor(player, unitType);
        boost::apply_visitor(visitor, pUnitInfo->getInfo());

        return visitor.getConstructItem();
    }
}