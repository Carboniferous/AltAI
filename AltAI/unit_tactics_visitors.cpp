#include "./unit_tactics_visitors.h"
#include "./unit_info.h"
#include "./unit_info_visitors.h"
#include "./city_unit_tactics.h"
#include "./building_tactics_items.h"
#include "./unit_tactics_items.h"
#include "./building_tactics_deps.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./unit_analysis.h"
#include "./iters.h"
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
                std::make_pair(pUnitAnalysis_->getCityAttackUnitValue(constructItem_.unitType, 1), -1)));
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

            int cityAttackValue = pUnitAnalysis_->getCityAttackUnitValue(constructItem_.unitType, 1);
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

    class MakeUnitTacticsDependenciesVisitor : public boost::static_visitor<>
    {
    public:
        MakeUnitTacticsDependenciesVisitor(const Player& player, const City& city)
            : player_(player), city_(city)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            if (!(node.andBonusTypes.empty() && node.orBonusTypes.empty()))
            {
                std::vector<BonusTypes> missingAndBonuses, missingOrBonuses;
                for (size_t i = 0, count = node.andBonusTypes.size(); i < count; ++i)
                {
                    if (!city_.getCvCity()->hasBonus(node.andBonusTypes[i]))
                    {
                        missingAndBonuses.push_back(node.andBonusTypes[i]);
                    }
                }

                for (size_t i = 0, count = node.orBonusTypes.size(); i < count; ++i)
                {
                    if (city_.getCvCity()->hasBonus(node.orBonusTypes[i]))
                    {
                        missingOrBonuses.clear();
                        break;
                    }
                    else
                    {
                        missingOrBonuses.push_back(node.orBonusTypes[i]);
                    }
                }

                if (!missingAndBonuses.empty())
                {
                    dependentTactics_.push_back(IDependentTacticPtr(new CityBonusDependency(missingAndBonuses, node.unitType, false)));
                }

                if (!missingOrBonuses.empty())
                {
                    dependentTactics_.push_back(IDependentTacticPtr(new CityBonusDependency(missingOrBonuses, node.unitType, true)));
                }
            }

            for (size_t i = 0, count = node.techTypes.size(); i < count; ++i)
            {
                if (player_.getTechResearchDepth(node.techTypes[i]) > 0)
                {
                    dependentTactics_.push_back(IDependentTacticPtr(new ResearchTechDependency(node.techTypes[i])));
                }
            }

            bool passedBuildingCheck = node.prereqBuildingType == NO_BUILDING;
            if (!passedBuildingCheck)
            {
                SpecialBuildingTypes specialBuildingType = (SpecialBuildingTypes)gGlobals.getBuildingInfo(node.prereqBuildingType).getSpecialBuildingType();
                if (specialBuildingType != NO_SPECIALBUILDING)
                {
                    passedBuildingCheck = player_.getCivHelper()->getSpecialBuildingNotRequiredCount(specialBuildingType) > 0;
                }
            }

            if (!passedBuildingCheck)
            {
                passedBuildingCheck = city_.getCvCity()->getNumBuilding(node.prereqBuildingType) > 0;
            }

            if (!passedBuildingCheck)
            {
                dependentTactics_.push_back(IDependentTacticPtr(new CityBuildingDependency(node.prereqBuildingType)));
            }
        }

        const std::vector<IDependentTacticPtr>& getDependentTactics() const
        {
            return dependentTactics_;
        }

    private:
        const Player& player_;
        const City& city_;
        std::vector<IDependentTacticPtr> dependentTactics_;
    };

    class MakeUnitTacticsVisitor : public boost::static_visitor<>
    {
    public:
        MakeUnitTacticsVisitor(const Player& player, const City& city, UnitTypes unitType)
            : unitInfo_(gGlobals.getUnitInfo(unitType)), player_(player), city_(city), unitType_(unitType)
        {
            pUnitAnalysis_ = player_.getAnalysis()->getUnitAnalysis();           
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            pTactic_ = ICityUnitTacticsPtr(new CityUnitTactic(unitType_, city_.getCvCity()->getIDInfo()));

            MakeUnitTacticsDependenciesVisitor dependentTacticsVisitor(player_, city_);
            dependentTacticsVisitor(node);

            const std::vector<IDependentTacticPtr>& dependentTactics = dependentTacticsVisitor.getDependentTactics();
            for (size_t i = 0, count = dependentTactics.size(); i < count; ++i)
            {
                pTactic_->addDependency(dependentTactics[i]);
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }            
        }

        void operator() (const UnitInfo::CityCombatNode& node)
        {
        }

        void operator() (const UnitInfo::CollateralNode& node)
        {
        }

        void operator() (const UnitInfo::ReligionNode& node)
        {
        }
        
        void operator() (const UnitInfo::MiscAbilityNode& node)
        {
            if (node.canFoundCity)
            {
                pTactic_->addTactic(ICityUnitTacticPtr(new BuildCityUnitTactic()));
            }
        }

        void operator() (const UnitInfo::BuildNode& node)
        {
            pTactic_->addTactic(ICityUnitTacticPtr(new BuildImprovementsUnitTactic(node.buildTypes)));
        }

        void operator() (const UnitInfo::PromotionsNode& node)
        {
            
        }
        
        void operator() (const UnitInfo::CombatBonusNode& node)
        {
        }

        void operator() (const UnitInfo::AirCombatNode& node)
        {
        }

        void operator() (const UnitInfo::UpgradeNode& node)
        {
        }

        const ICityUnitTacticsPtr& getTactic() const
        {
            return pTactic_;
        }

    private:
        UnitTypes unitType_;
        const CvUnitInfo& unitInfo_;
        const Player& player_;
        const City& city_;
        ICityUnitTacticsPtr pTactic_;
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis_;
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

    ICityUnitTacticsPtr makeCityUnitTactics(const Player& player, const City& city, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        const UnitTypes unitType = pUnitInfo->getUnitType();
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(pUnitInfo->getUnitType());
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis = player.getAnalysis()->getUnitAnalysis();   

        MakeUnitTacticsVisitor visitor(player, city, pUnitInfo->getUnitType());
        boost::apply_visitor(visitor, pUnitInfo->getInfo());
        ICityUnitTacticsPtr pCityUnitTactics = visitor.getTactic();

        int maxPromotionLevel = 0;
        int freeExperience = ((CvCity*)city.getCvCity())->getProductionExperience(unitType);
        int requiredExperience = 2;
        while (freeExperience > requiredExperience)
        {
            // TODO - handle charismatic
            requiredExperience += 2 * ++maxPromotionLevel + 1;
        }
       
        if (!unitInfo.isOnlyDefensive())
        {
            UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCityAttackPromotions(unitType, maxPromotionLevel);

            if (levelsAndPromotions.second.empty())
			{
				levelsAndPromotions = pUnitAnalysis->getCombatPromotions(unitType, maxPromotionLevel);
			}

            UnitData unitData(unitInfo);
            for (Promotions::const_iterator ci(levelsAndPromotions.second.begin()), ciEnd(levelsAndPromotions.second.end()); ci != ciEnd; ++ci)
		    {
			    unitData.applyPromotion(gGlobals.getPromotionInfo(*ci));
		    }

            pCityUnitTactics->addTactic(ICityUnitTacticPtr(new CityAttackUnitTactic(levelsAndPromotions.second)));
        }

        {
            UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCityDefencePromotions(unitType, maxPromotionLevel);

            if (levelsAndPromotions.second.empty())
			{
				levelsAndPromotions = pUnitAnalysis->getCombatPromotions(unitType, maxPromotionLevel);
			}

            UnitData unitData(unitInfo);
            for (Promotions::const_iterator ci(levelsAndPromotions.second.begin()), ciEnd(levelsAndPromotions.second.end()); ci != ciEnd; ++ci)
		    {
			    unitData.applyPromotion(gGlobals.getPromotionInfo(*ci));
		    }

            if (unitData.cityDefencePercent > 0)
            {
                pCityUnitTactics->addTactic(ICityUnitTacticPtr(new CityDefenceUnitTactic(levelsAndPromotions.second)));
            }
        }

        if (!unitInfo.isOnlyDefensive())
        {
            UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCombatPromotions(unitType, maxPromotionLevel);

            UnitData unitData(unitInfo);
            for (Promotions::const_iterator ci(levelsAndPromotions.second.begin()), ciEnd(levelsAndPromotions.second.end()); ci != ciEnd; ++ci)
		    {
			    unitData.applyPromotion(gGlobals.getPromotionInfo(*ci));
		    }

            if (unitData.combat > 0)
            {
                pCityUnitTactics->addTactic(ICityUnitTacticPtr(new FieldAttackUnitTactic(levelsAndPromotions.second)));
            }
        }

        return pCityUnitTactics;
    }

    IUnitTacticsPtr makeUnitTactics(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        const int lookAheadDepth = 2;
        if (couldConstructUnit(player, lookAheadDepth, pUnitInfo, true))
        {
            const UnitTypes unitType = pUnitInfo->getUnitType();
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

            IUnitTacticsPtr pTactic(new UnitTactic(unitType));

            CityIter iter(*player.getCvPlayer());

            while (CvCity* pCity = iter())
            {
#ifdef ALTAI_DEBUG
                CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for unit: " << unitInfo.getType();
#endif
                
                pTactic->addCityTactic(pCity->getIDInfo(), makeCityUnitTactics(player, player.getCity(pCity->getID()), pUnitInfo));               
            }

            return pTactic;
        }

        return IUnitTacticsPtr();
    }
}