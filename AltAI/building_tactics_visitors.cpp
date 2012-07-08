#include "./building_tactics_visitors.h"
#include "./building_info_visitors.h"
#include "./building_tactics_items.h"
#include "./city_building_tactics.h"
#include "./building_tactics_deps.h"
#include "./buildings_info.h"
#include "./building_helper.h"
#include "./maintenance_helper.h"
#include "./tech_info_visitors.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./city.h"
#include "./city_simulator.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./civ_log.h"

namespace AltAI
{
    class MakeEconomicBuildingConditionsVisitor : public boost::static_visitor<>
    {
    public:
        MakeEconomicBuildingConditionsVisitor(const Player& player, BuildingTypes buildingType)
            : player_(player), constructItem_(buildingType)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const BuildingInfo::BaseNode& node)
        {
            if (node.happy > 0)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Happy;
            }

            if (node.health > 0)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Health;
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }
        }

        void operator() (const BuildingInfo::YieldNode& node)
        {
            if (!isEmpty(node.modifier) || !isEmpty(node.yield))
            {
                if (node.modifier[YIELD_FOOD] > 0 || node.yield[YIELD_FOOD] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Food;
                }
                if (node.modifier[YIELD_PRODUCTION] > 0 || node.yield[YIELD_PRODUCTION] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Production;
                }
                if (node.modifier[YIELD_COMMERCE] > 0 || node.yield[YIELD_COMMERCE] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Commerce;
                }
            }
        }

        void operator() (const BuildingInfo::SpecialistNode& node)
        {
            if (!isEmpty(node.extraCommerce))
            {
                if (node.extraCommerce[COMMERCE_GOLD] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Gold;
                }
                if (node.extraCommerce[COMMERCE_RESEARCH] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Research;
                }
                if (node.extraCommerce[COMMERCE_CULTURE] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Culture;
                }
                if (node.extraCommerce[COMMERCE_ESPIONAGE] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Espionage;
                }
            }

            if (!node.specialistTypesAndYields.empty())
            {
                for (size_t i = 0, count = node.specialistTypesAndYields.size(); i < count; ++i)
                {
                    if (node.specialistTypesAndYields[i].second[YIELD_FOOD] > 0)
                    {
                        constructItem_.economicFlags |= EconomicFlags::Output_Food;
                    }
                    if (node.specialistTypesAndYields[i].second[YIELD_PRODUCTION] > 0)
                    {
                        constructItem_.economicFlags |= EconomicFlags::Output_Production;
                    }
                    if (node.specialistTypesAndYields[i].second[YIELD_COMMERCE] > 0)
                    {   
                        constructItem_.economicFlags |= EconomicFlags::Output_Commerce;
                    }
                }
            }
        }

        void operator() (const BuildingInfo::CommerceNode& node)
        {
            if (!isEmpty(node.modifier) || !isEmpty(node.commerce) || !isEmpty(node.obsoleteSafeCommerce))
            {
                if (node.modifier[COMMERCE_GOLD] > 0 || node.commerce[COMMERCE_GOLD] > 0 || node.obsoleteSafeCommerce[COMMERCE_GOLD] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Gold;
                }
                if (node.modifier[COMMERCE_RESEARCH] > 0 || node.commerce[COMMERCE_RESEARCH] > 0 || node.obsoleteSafeCommerce[COMMERCE_RESEARCH] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Research;
                }
                if (node.modifier[COMMERCE_CULTURE] > 0 || node.commerce[COMMERCE_CULTURE] > 0 || node.obsoleteSafeCommerce[COMMERCE_CULTURE] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Culture;
                }
                if (node.modifier[COMMERCE_ESPIONAGE] > 0 || node.commerce[COMMERCE_ESPIONAGE] > 0 || node.obsoleteSafeCommerce[COMMERCE_ESPIONAGE] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Espionage;
                }
            }
        }

        void operator() (const BuildingInfo::TradeNode& node)
        {
            bool hasValue = false;
            if (node.extraTradeRoutes != 0 ||
                (node.extraCoastalTradeRoutes != 0 && player_.getCvPlayer()->countNumCoastalCities() > 0) ||
                node.extraGlobalTradeRoutes != 0 ||
                node.tradeRouteModifier != 0 ||
                node.foreignTradeRouteModifier != 0)  // todo - check if can have foreign trade routes (similar to code in TradeRouteHelper)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Commerce;
            }
        }

        void operator() (const BuildingInfo::PowerNode& node)
        {
            constructItem_.economicFlags |= EconomicFlags::Output_Production;
        }

        void operator() (const BuildingInfo::SpecialistSlotNode& node)
        {
            // need enough food to run specialists

            // free specialists
            for (size_t i = 0, count = node.freeSpecialistTypes.size(); i < count; ++i)
            {
                PlotYield yield(getSpecialistYield(*player_.getCvPlayer(), node.freeSpecialistTypes[i].first));
                Commerce commerce(getSpecialistCommerce(*player_.getCvPlayer(), node.freeSpecialistTypes[i].first));
                if (yield[YIELD_FOOD] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Food;
                }
                if (yield[YIELD_PRODUCTION] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Production;
                }
                if (yield[YIELD_COMMERCE] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Commerce;
                }
                if (commerce[COMMERCE_GOLD] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Gold;
                }
                if (commerce[COMMERCE_RESEARCH] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Research;
                }
                if (commerce[COMMERCE_CULTURE] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Culture;
                }
                if (commerce[COMMERCE_ESPIONAGE] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Espionage;
                }
            }
            
            // does this step make more sense to be done as a city specific check?
            /*for (size_t i = 0, count = node.improvementFreeSpecialists.size(); i < count; ++i)
            {
            }*/
        }

        void operator() (const BuildingInfo::BonusNode& node)
        {
            // todo - add flags to indicate condition nature of outputs
            if (node.prodModifier > 0)
            {
                constructItem_.positiveBonuses.push_back(node.bonusType);
            }

            if (!isEmpty(node.yieldModifier) && player_.getCvPlayer()->hasBonus(node.bonusType))
            {
                if (node.yieldModifier[YIELD_FOOD] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Food;
                }
                if (node.yieldModifier[YIELD_PRODUCTION] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Production;
                }
                if (node.yieldModifier[YIELD_COMMERCE] > 0)
                {
                    constructItem_.economicFlags |= EconomicFlags::Output_Commerce;
                }
            }

            if (node.happy > 0)
            {
                constructItem_.positiveBonuses.push_back(node.bonusType);
                constructItem_.economicFlags |= EconomicFlags::Output_Happy;
            }

            if (node.health > 0)
            {
                constructItem_.positiveBonuses.push_back(node.bonusType);
                constructItem_.economicFlags |= EconomicFlags::Output_Health;
            }
        }

        void operator() (const BuildingInfo::FreeBonusNode& node)
        {
            constructItem_.economicFlags |= EconomicFlags::Output_Commerce;  // can always sell extra bonuses
            const CvBonusInfo& bonusInfo = gGlobals.getBonusInfo(node.freeBonuses.first);
            if (bonusInfo.getHappiness() > 0)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Happy;
            }
            if (bonusInfo.getHealth() > 0)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Health;
            }
        }

        void operator() (const BuildingInfo::RemoveBonusNode& node)
        {
        }

        void operator() (const BuildingInfo::MiscEffectNode& node)
        {
            if (node.foodKeptPercent > 0)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Food;
            }

            if (node.cityMaintenanceModifierChange < 0)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Maintenance_Reduction;
            }

            if (node.noUnhealthinessFromBuildings || node.noUnhealthinessFromPopulation)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Food;
                constructItem_.economicFlags |= EconomicFlags::Output_Health;
            }

            if (node.hurryAngerModifier > 0)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Production;
            }

            if (node.startsGoldenAge)
            {
                constructItem_.economicFlags |= EconomicFlags::Output_Production;
                constructItem_.economicFlags |= EconomicFlags::Output_Commerce;
            }
        }

        ConstructItem getConstructItem() const
        {
            return constructItem_.isEmpty() ? ConstructItem(NO_BUILDING): constructItem_;
        }

    private:
        const Player& player_;
        ConstructItem constructItem_;
    };

    class MakeMilitaryBuildingConditionsVisitor : public boost::static_visitor<>
    {
    public:
        MakeMilitaryBuildingConditionsVisitor(const Player& player, BuildingTypes buildingType)
            : player_(player), constructItem_(buildingType)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const BuildingInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }
        }

        void operator() (const BuildingInfo::YieldNode& node)
        {
            if (!isEmpty(node.modifier) || !isEmpty(node.yield))
            {
                if (node.modifier[YIELD_PRODUCTION] > 0 || node.yield[YIELD_PRODUCTION] > 0)
                {
                    constructItem_.militaryFlags |= MilitaryFlags::Output_Production;
                }
            }
        }

        void operator() (const BuildingInfo::SpecialistNode& node)
        {
            if (!node.specialistTypesAndYields.empty())
            {
                for (size_t i = 0, count = node.specialistTypesAndYields.size(); i < count; ++i)
                {
                    if (node.specialistTypesAndYields[i].second[YIELD_PRODUCTION] > 0)
                    {
                        constructItem_.militaryFlags |= MilitaryFlags::Output_Production;
                        break;
                    }
                }
            }
        }

        void operator() (const BuildingInfo::UnitExpNode& node)
        {
            if (node.freeExperience > 0 || !node.domainFreeExperience.empty() || node.freePromotion > 0 || 
                !node.combatTypeFreeExperience.empty() || node.globalFreeExperience > 0)
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Experience;
            }
            if (!node.domainProductionModifier.empty())
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Production;
            }
        }

        void operator() (const BuildingInfo::FreeBonusNode& node)
        {
            // todo - check if bonus can make units available
            // Output_Make_Unit_Available
        }

        void operator() (const BuildingInfo::RemoveBonusNode& node)
        {
            // todo - check if bonus removal can make units unavailable
        }

        void operator() (const BuildingInfo::PowerNode& node)
        {
            constructItem_.militaryFlags |= MilitaryFlags::Output_Production;
        }

        void operator() (const BuildingInfo::CityDefenceNode& node)
        {
            if (node.defenceBonus > 0)
            {
                constructItem_.militaryFlags |= MilitaryFlags::Output_Defence;
            }
        }

        void operator() (const BuildingInfo::MiscEffectNode& node)
        {
        }

        ConstructItem getConstructItem() const
        {
            return constructItem_.isEmpty() ? ConstructItem(NO_BUILDING): constructItem_;
        }

    private:
        const Player& player_;
        ConstructItem constructItem_;
    };

    class MakeBuildingTacticsDependenciesVisitor : public boost::static_visitor<>
    {
    public:
        MakeBuildingTacticsDependenciesVisitor(const Player& player, const City& city, BuildingTypes buildingType)
            : player_(player), city_(city), thisBuildingType_(buildingType)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const BuildingInfo::RequiredBuildings& node)
        {
            for (size_t i = 0, count = node.cityBuildings.size(); i < count; ++i)
            {
                if (city_.getCvCity()->getNumBuilding(node.cityBuildings[i]) == 0)
                {
                    dependentTactics_.push_back(IDependentTacticPtr(new CityBuildingDependency(node.cityBuildings[i])));
                }
            }

            for (size_t i = 0, count = node.buildingCounts.size(); i < count; ++i)
            {
                int buildingCount = 0, thisBuildingCount = 0;
                CityIter iter(*player_.getCvPlayer());
                while (CvCity* pCity = iter())
                {
                    buildingCount += pCity->getNumBuilding(node.buildingCounts[i].first);
                    thisBuildingCount += pCity->getNumBuilding(thisBuildingType_);
                }

                if (buildingCount - thisBuildingCount * node.buildingCounts[i].second < node.buildingCounts[i].second)
                {
                    dependentTactics_.push_back(IDependentTacticPtr(
                        new CivBuildingDependency(node.buildingCounts[i].first, node.buildingCounts[i].second - buildingCount, thisBuildingType_)));
                }
            }
        }

        const std::vector<IDependentTacticPtr>& getDependentTactics() const
        {
            return dependentTactics_;
        }

    private:
        const Player& player_;
        const City& city_;
        BuildingTypes thisBuildingType_;
        std::vector<IDependentTacticPtr> dependentTactics_;
    };

    class MakeBuildingTacticsVisitor : public boost::static_visitor<>
    {
    public:
        MakeBuildingTacticsVisitor(const Player& player, const City& city, BuildingTypes buildingType)
            : player_(player), city_(city), buildingType_(buildingType), isEconomic_(false)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const BuildingInfo::BaseNode& node)
        {
            pTactic_ = ICityBuildingTacticsPtr(new CityBuildingTactic(buildingType_, city_.getCvCity()->getIDInfo()));

            MakeBuildingTacticsDependenciesVisitor dependentTacticsVisitor(player_, city_, buildingType_);
            for (size_t i = 0, count = node.buildConditions.size(); i < count; ++i)
            {
                boost::apply_visitor(dependentTacticsVisitor, node.buildConditions[i]);
            }

            const std::vector<IDependentTacticPtr>& dependentTactics = dependentTacticsVisitor.getDependentTactics();
            for (size_t i = 0, count = dependentTactics.size(); i < count; ++i)
            {
                pTactic_->addDependency(dependentTactics[i]);
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }

            if (node.happy > 0)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new HappyBuildingTactic()));
                isEconomic_ = true;
            }

            if (node.health > 0)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new HealthBuildingTactic()));
                isEconomic_ = true;
            }

            if (isEconomic_)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new EconomicBuildingTactic()));
            }

            for (size_t i = 0, count = node.techs.size(); i < count; ++i)
            {
                if (player_.getTechResearchDepth(node.techs[i]) > 0)
                {
                    pTactic_->addDependency(IDependentTacticPtr(new ResearchTechDependency(node.techs[i])));
                }
            }
        }

        void operator() (const BuildingInfo::YieldNode& node)
        {
            if (node.yield[OUTPUT_FOOD] > 0)  // todo - check plot conds
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new FoodBuildingTactic()));
            }
            if (!isEmpty(node.modifier) || !isEmpty(node.yield))
            {
                isEconomic_ = true;
            }
        }

        void operator() (const BuildingInfo::CommerceNode& node)
        {
            if (node.modifier[COMMERCE_RESEARCH] > 0 || node.obsoleteSafeCommerce[COMMERCE_RESEARCH] > 0 || node.commerce[COMMERCE_RESEARCH] > 0)
            {
                isEconomic_ = true;
                pTactic_->addTactic(ICityBuildingTacticPtr(new ScienceBuildingTactic()));
            }
            if (node.modifier[COMMERCE_GOLD] > 0 || node.obsoleteSafeCommerce[COMMERCE_GOLD] > 0 || node.commerce[COMMERCE_GOLD] > 0)
            {
                isEconomic_ = true;
                pTactic_->addTactic(ICityBuildingTacticPtr(new GoldBuildingTactic()));
            }
            if (node.modifier[COMMERCE_ESPIONAGE] > 0 || node.obsoleteSafeCommerce[COMMERCE_ESPIONAGE] > 0 || node.commerce[COMMERCE_ESPIONAGE] > 0)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new EspionageBuildingTactic()));
            }
            if (node.modifier[COMMERCE_CULTURE] > 0 || node.obsoleteSafeCommerce[COMMERCE_CULTURE] > 0 || node.commerce[COMMERCE_CULTURE] > 0)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new CultureBuildingTactic()));
            }            
        }

        void operator() (const BuildingInfo::SpecialistSlotNode& node)
        {
            if (!node.specialistTypes.empty())
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new SpecialistBuildingTactic()));
            }
            isEconomic_ = true;
        }

        void operator() (const BuildingInfo::SpecialistNode& node)
        {
            isEconomic_ = true;
        }

        void operator() (const BuildingInfo::TradeNode& node)
        {
            isEconomic_ = true;
        }

        void operator() (const BuildingInfo::PowerNode& node)
        {
            isEconomic_ = true;
        }

        void operator() (const BuildingInfo::FreeBonusNode& node)
        {
            isEconomic_ = true;
        }

        void operator() (const BuildingInfo::AreaEffectNode& node)
        {
            isEconomic_ = true;
        }

        void operator() (const BuildingInfo::UnitExpNode& node)
        {
            //int freeExperience, globalFreeExperience;
            //std::vector<std::pair<DomainTypes, int> > domainFreeExperience;
            //std::vector<std::pair<UnitCombatTypes, int> > combatTypeFreeExperience;
            //PromotionTypes freePromotion;
            if (node.freeExperience != 0 || node.globalFreeExperience != 0 || !node.domainFreeExperience.empty() ||
                !node.combatTypeFreeExperience.empty() || node.freePromotion != NO_PROMOTION)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(
                    new UnitExperienceTactic(node.freeExperience, node.globalFreeExperience, node.domainFreeExperience, node.combatTypeFreeExperience, node.freePromotion)));
            }
        }

        void operator() (const BuildingInfo::MiscEffectNode& node)
        {
            if (node.foodKeptPercent > 0)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new FoodBuildingTactic()));
                isEconomic_ = true;
            }

            if (node.isGovernmentCenter)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new GovCenterTactic(!node.makesCityCapital)));
                if (!node.makesCityCapital)
                {
                    isEconomic_ = true;
                }
            }

            if (node.cityMaintenanceModifierChange || node.freeBuildingType != NO_BUILDING || node.globalPopChange > 0 || node.noUnhealthinessFromBuildings ||
                node.noUnhealthinessFromPopulation || node.startsGoldenAge)
            {
                isEconomic_ = true;
            }
        }

        void operator() (const BuildingInfo::ReligionNode& node)
        {
            if (node.prereqReligion != NO_RELIGION && !city_.getCvCity()->isHasReligion(node.prereqReligion))
            {
                for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
                {
                    UnitTypes unitType = getPlayerVersion(player_.getPlayerID(), (UnitClassTypes)i);
                    if (unitType != NO_UNIT && gGlobals.getUnitInfo(unitType).getReligionSpreads(node.prereqReligion) > 0)
                    {
                        pTactic_->addDependency(IDependentTacticPtr(new ReligiousDependency(node.prereqReligion, unitType)));
                        break;
                    }
                }                
            }
        }

        const ICityBuildingTacticsPtr& getTactic() const
        {
            return pTactic_;
        }

    private:
        BuildingTypes buildingType_;
        const Player& player_;
        const City& city_;
        ICityBuildingTacticsPtr pTactic_;
        bool isEconomic_;
    };

    ConstructItem getEconomicBuildingTactics(const Player& player, BuildingTypes buildingType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        MakeEconomicBuildingConditionsVisitor visitor(player, buildingType);
        boost::apply_visitor(visitor, pBuildingInfo->getInfo());

        return visitor.getConstructItem();
    }

    ConstructItem getMilitaryBuildingTactics(const Player& player, BuildingTypes buildingType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        MakeMilitaryBuildingConditionsVisitor visitor(player, buildingType);
        boost::apply_visitor(visitor, pBuildingInfo->getInfo());

        return visitor.getConstructItem();
    }

    ICityBuildingTacticsPtr makeCityBuildingTactics(const Player& player, const City& city, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        MakeBuildingTacticsVisitor visitor(player, city, pBuildingInfo->getBuildingType());
        boost::apply_visitor(visitor, pBuildingInfo->getInfo());
        return visitor.getTactic();
    }

    ILimitedBuildingTacticsPtr makeGlobalBuildingTactics(const Player& player, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        const int lookAheadDepth = 2;
        ILimitedBuildingTacticsPtr pTactic(new GlobalBuildingTactic(pBuildingInfo->getBuildingType()));

        CityIter iter(*player.getCvPlayer());

        while (CvCity* pCity = iter())
        {
            if (couldConstructBuilding(player, player.getCity(pCity->getID()), lookAheadDepth, pBuildingInfo, true))
            {
#ifdef ALTAI_DEBUG
                CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for building: "
                    << gGlobals.getBuildingInfo(pBuildingInfo->getBuildingType()).getType();
#endif
                pTactic->addCityTactic(pCity->getIDInfo(), makeCityBuildingTactics(player, player.getCity(pCity->getID()), pBuildingInfo));
            }
        }

        return pTactic;
    }

    ILimitedBuildingTacticsPtr makeNationalBuildingTactics(const Player& player, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        const int lookAheadDepth = 2;
        ILimitedBuildingTacticsPtr pTactic(new NationalBuildingTactic(pBuildingInfo->getBuildingType()));

        CityIter iter(*player.getCvPlayer());

        while (CvCity* pCity = iter())
        {
            if (!pCity->isNationalWondersMaxed())
            {
                if (couldConstructBuilding(player, player.getCity(pCity->getID()), lookAheadDepth, pBuildingInfo, true))
                {
#ifdef ALTAI_DEBUG
                    CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__
                        << " Adding tactic for building: " << gGlobals.getBuildingInfo(pBuildingInfo->getBuildingType()).getType();
#endif
                    pTactic->addCityTactic(pCity->getIDInfo(), makeCityBuildingTactics(player, player.getCity(pCity->getID()), pBuildingInfo));
                }
            }
        }

        return pTactic;
    }

    IProcessTacticsPtr makeProcessTactics(const Player& player, ProcessTypes processType)
    {
        IProcessTacticsPtr pTactic = IProcessTacticsPtr(new ProcessTactic(processType));
        const CvProcessInfo& processInfo = gGlobals.getProcessInfo(processType);
        TechTypes techType = (TechTypes)processInfo.getTechPrereq();
        if (player.getTechResearchDepth(techType) > 0)
        {
            pTactic->addDependency(IDependentTacticPtr(new ResearchTechDependency(techType)));

            for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
            {
                if (processInfo.getProductionToCommerceModifier(i) > 0)
                {
                    // todo - add tactics
                }
            }
        }

        return pTactic;
    }
}