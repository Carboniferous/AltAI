#include "AltAI.h"

#include "./building_tactics_visitors.h"
#include "./building_info_construct_visitors.h"
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
                // always add this, even if currently have enough buildings, as this can change (except for limited wonders)
                dependentTactics_.push_back(IDependentTacticPtr(
                    new CivBuildingDependency(node.buildingCounts[i].first, node.buildingCounts[i].second, thisBuildingType_)));
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
            : player_(player), city_(city), buildingType_(buildingType), isEconomic_(false), isLocal_(false), isRegional_(false), isGlobal_(false)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const BuildingInfo::BaseNode& node)
        {
            MakeBuildingTacticsDependenciesVisitor dependentTacticsVisitor(player_, city_, buildingType_);
            for (size_t i = 0, count = node.buildConditions.size(); i < count; ++i)
            {
                boost::apply_visitor(dependentTacticsVisitor, node.buildConditions[i]);
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }

            if (node.happy > 0)
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(new HappyBuildingTactic()));
                isEconomic_ = true;
            }

            if (node.health > 0)
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(new HealthBuildingTactic()));
                isEconomic_ = true;
            }

            if (isEconomic_)
            {
                isLocal_ = true;
                tacticsItems_.push_back(ICityBuildingTacticPtr(new EconomicBuildingTactic()));
            }

            ICityBuildingTactics::ComparisonFlags compFlag = (isGlobal_ ? ICityBuildingTactics::Global_Comparison :
                (isRegional_ ? ICityBuildingTactics::Area_Comparison :
                    (isLocal_ ? ICityBuildingTactics::City_Comparison : ICityBuildingTactics::No_Comparison)));

            pTactic_ = ICityBuildingTacticsPtr(new CityBuildingTactic(buildingType_, node.cost, city_.getCvCity()->getIDInfo(), compFlag));

            for (size_t i = 0, count = tacticsItems_.size(); i < count; ++i)
            {
                pTactic_->addTactic(tacticsItems_[i]);
            }

            for (size_t i = 0, count = depItems_.size(); i < count; ++i)
            {
                pTactic_->addDependency(depItems_[i]);
            }

            const std::vector<IDependentTacticPtr>& dependentTactics = dependentTacticsVisitor.getDependentTactics();
            for (size_t i = 0, count = dependentTactics.size(); i < count; ++i)
            {
                pTactic_->addDependency(dependentTactics[i]);
            }

            for (size_t i = 0, count = node.techs.size(); i < count; ++i)
            {
                if (player_.getTechResearchDepth(node.techs[i]) > 0)
                {
                    pTactic_->addTechDependency(ResearchTechDependencyPtr(new ResearchTechDependency(node.techs[i])));
                }
            }
        }

        void operator() (const BuildingInfo::YieldNode& node)
        {
            if (node.yield[OUTPUT_FOOD] > 0)  // todo - check plot conds
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(new FoodBuildingTactic()));
            }
            if (!isEmpty(node.modifier) || !isEmpty(node.yield))
            {
                isEconomic_ = true;
            }
            if (node.global)
            {
                isGlobal_ = true;
            }            
        }

        void operator() (const BuildingInfo::CommerceNode& node)
        {
            if (node.modifier[COMMERCE_RESEARCH] > 0 || node.obsoleteSafeCommerce[COMMERCE_RESEARCH] > 0 || node.commerce[COMMERCE_RESEARCH] > 0)
            {
                isEconomic_ = true;
                tacticsItems_.push_back(ICityBuildingTacticPtr(new ScienceBuildingTactic()));
            }
            if (node.modifier[COMMERCE_GOLD] > 0 || node.obsoleteSafeCommerce[COMMERCE_GOLD] > 0 || node.commerce[COMMERCE_GOLD] > 0)
            {
                isEconomic_ = true;
                tacticsItems_.push_back(ICityBuildingTacticPtr(new GoldBuildingTactic()));
            }
            if (node.modifier[COMMERCE_ESPIONAGE] > 0 || node.obsoleteSafeCommerce[COMMERCE_ESPIONAGE] > 0 || node.commerce[COMMERCE_ESPIONAGE] > 0)
            {
                isEconomic_ = true;
                tacticsItems_.push_back(ICityBuildingTacticPtr(new EspionageBuildingTactic()));
            }
            if (node.modifier[COMMERCE_CULTURE] > 0 || node.obsoleteSafeCommerce[COMMERCE_CULTURE] > 0 || node.commerce[COMMERCE_CULTURE] > 0)
            {
                isLocal_ = true;
                tacticsItems_.push_back(ICityBuildingTacticPtr(
                    new CultureBuildingTactic(node.obsoleteSafeCommerce[COMMERCE_CULTURE] + node.commerce[COMMERCE_CULTURE], 0)));
            }
            if (node.global)
            {
                isGlobal_ = true;
            }

            if (!isEmpty(node.stateReligionCommerce))
            {
                ReligionTypes stateReligion = player_.getCvPlayer()->getStateReligion();
                if (stateReligion != NO_RELIGION && city_.getCvCity()->isHasReligion(stateReligion))
                {
                    if (node.stateReligionCommerce[COMMERCE_RESEARCH] > 0)
                    {
                        isEconomic_ = true;
                        tacticsItems_.push_back(ICityBuildingTacticPtr(new ScienceBuildingTactic()));
                    }
                    if (node.stateReligionCommerce[COMMERCE_GOLD] > 0)
                    {
                        isEconomic_ = true;
                        tacticsItems_.push_back(ICityBuildingTacticPtr(new GoldBuildingTactic()));
                    }
                    if (node.stateReligionCommerce[COMMERCE_CULTURE] > 0)
                    {
                        tacticsItems_.push_back(ICityBuildingTacticPtr(new CultureBuildingTactic()));
                    }
                    if (node.stateReligionCommerce[COMMERCE_ESPIONAGE] > 0)
                    {
                        tacticsItems_.push_back(ICityBuildingTacticPtr(new EspionageBuildingTactic()));
                    }
                }
            }
        }

        void operator() (const BuildingInfo::SpecialistSlotNode& node)
        {
            if (!node.specialistTypes.empty())
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(new SpecialistBuildingTactic()));
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
            if (node.extraGlobalTradeRoutes > 0)
            {
                isGlobal_ = true;
            }
        }

        void operator() (const BuildingInfo::PowerNode& node)
        {
            isEconomic_ = true;
            if (node.areaCleanPower)
            {
                isRegional_ = true;
            }
        }

        void operator() (const BuildingInfo::BonusNode& node)
        {
            if (node.prodModifier != 0)
            {
                depItems_.push_back(IDependentTacticPtr(new ResourceProductionBonusDependency(node.bonusType, node.prodModifier)));
            }
            if (node.freeBonusCount > 0)
            {
                isEconomic_ = true;
                isGlobal_ = true;
            }
        }

        void operator() (const BuildingInfo::CityDefenceNode& node)
        {
            if (node.defenceBonus > 0 || node.globalDefenceBonus > 0 || node.bombardRateModifier > 0)
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(new CityDefenceBuildingTactic(node.defenceBonus, node.globalDefenceBonus, node.bombardRateModifier)));
            }
            if (node.globalDefenceBonus)
            {
                isGlobal_ = true;
            }
        }

        void operator() (const BuildingInfo::AreaEffectNode& node)
        {
            isEconomic_ = true;
            if (node.globalHappy != 0 || node.globalHealth != 0)
            {
                isGlobal_ = true;
            }
            else if (node.areaHappy != 0 || node.areaHealth != 0)
            {
                isRegional_ = true;
            }
        }

        void operator() (const BuildingInfo::UnitExpNode& node)
        {
            if (node.freeExperience != 0 || node.globalFreeExperience != 0 || !node.domainFreeExperience.empty() ||
                !node.combatTypeFreeExperience.empty() || node.freePromotion != NO_PROMOTION)
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(
                    new UnitExperienceTactic(node.freeExperience, node.globalFreeExperience, node.domainFreeExperience, node.combatTypeFreeExperience, node.freePromotion)));
            }
            if (node.globalFreeExperience != 0)
            {
                isGlobal_ = true;
            }
        }

        void operator() (const BuildingInfo::UnitNode& node)
        {
            if (node.enabledUnitType != NO_UNIT)  // should always be true
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(
                    new CanTrainUnitBuildingTactic(node.enabledUnitType)));
            }
        }

        void operator() (const BuildingInfo::MiscEffectNode& node)
        {
            if (node.foodKeptPercent > 0)
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(new FoodBuildingTactic()));
                isEconomic_ = true;
            }

            if (node.isGovernmentCenter)
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(new GovCenterTactic(!node.makesCityCapital)));
                if (!node.makesCityCapital)
                {
                    isEconomic_ = true;
                    isGlobal_ = true; // will affect other cities' maintenance
                }
            }

            if (node.cityMaintenanceModifierChange || node.freeBuildingType != NO_BUILDING || node.globalPopChange > 0 || node.noUnhealthinessFromBuildings ||
                node.noUnhealthinessFromPopulation || node.startsGoldenAge)
            {
                isEconomic_ = true;
            }

            if (node.freeBuildingType != NO_BUILDING || node.globalPopChange > 0 || node.startsGoldenAge)
            {
                isGlobal_ = true;
            }

            if (node.nFreeTechs > 0)
            {
                tacticsItems_.push_back(ICityBuildingTacticPtr(new FreeTechBuildingTactic()));
                isEconomic_ = true;
            }

            // todo handle buildings which provide civics
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
                        depItems_.push_back(IDependentTacticPtr(new ReligiousDependency(node.prereqReligion, unitType)));
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
        std::vector<ICityBuildingTacticPtr> tacticsItems_;
        std::vector<IDependentTacticPtr> depItems_;
        bool isEconomic_, isLocal_, isRegional_, isGlobal_;
    };

    class MakeBuildingTacticItemsVisitor : public boost::static_visitor<>
    {
    public:
        MakeBuildingTacticItemsVisitor(const Player& player, BuildingTypes buildingType)
            : player_(player), buildingType_(buildingType), isEconomic_(false)
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

            if (node.happy > 0)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new HappyBuildingTactic()));
                isEconomic_ = true;
            }

            if (node.health > 0)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new HealthBuildingTactic()));
                isEconomic_ = true;
            }

            if (isEconomic_)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new EconomicBuildingTactic()));
            }
        }

        void operator() (const BuildingInfo::YieldNode& node)
        {
            if (node.yield[OUTPUT_FOOD] > 0)  // todo - check plot conds
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new FoodBuildingTactic()));
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
                pTactics_.push_back(ICityBuildingTacticPtr(new ScienceBuildingTactic()));
            }
            if (node.modifier[COMMERCE_GOLD] > 0 || node.obsoleteSafeCommerce[COMMERCE_GOLD] > 0 || node.commerce[COMMERCE_GOLD] > 0)
            {
                isEconomic_ = true;
                pTactics_.push_back(ICityBuildingTacticPtr(new GoldBuildingTactic()));
            }
            if (node.modifier[COMMERCE_ESPIONAGE] > 0 || node.obsoleteSafeCommerce[COMMERCE_ESPIONAGE] > 0 || node.commerce[COMMERCE_ESPIONAGE] > 0)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new EspionageBuildingTactic()));
            }
            if (node.modifier[COMMERCE_CULTURE] > 0 || node.obsoleteSafeCommerce[COMMERCE_CULTURE] > 0 || node.commerce[COMMERCE_CULTURE] > 0)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(
                    new CultureBuildingTactic(node.obsoleteSafeCommerce[COMMERCE_CULTURE] + node.commerce[COMMERCE_CULTURE], 0)));
            }            
        }

        void operator() (const BuildingInfo::SpecialistSlotNode& node)
        {
            if (!node.specialistTypes.empty())
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new SpecialistBuildingTactic()));
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

        void operator() (const BuildingInfo::BonusNode& node)
        {
            if (node.freeBonusCount > 0)
            {
                isEconomic_ = true;
            }
        }

        void operator() (const BuildingInfo::CityDefenceNode& node)
        {
            if (node.defenceBonus > 0 || node.globalDefenceBonus > 0 || node.bombardRateModifier > 0)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new CityDefenceBuildingTactic(node.defenceBonus, node.globalDefenceBonus, node.bombardRateModifier)));
            }
        }

        void operator() (const BuildingInfo::AreaEffectNode& node)
        {
            isEconomic_ = true;
        }

        void operator() (const BuildingInfo::UnitExpNode& node)
        {
            if (node.freeExperience != 0 || node.globalFreeExperience != 0 || !node.domainFreeExperience.empty() ||
                !node.combatTypeFreeExperience.empty() || node.freePromotion != NO_PROMOTION)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(
                    new UnitExperienceTactic(node.freeExperience, node.globalFreeExperience, node.domainFreeExperience, node.combatTypeFreeExperience, node.freePromotion)));
            }
        }

        void operator() (const BuildingInfo::MiscEffectNode& node)
        {
            if (node.foodKeptPercent > 0)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new FoodBuildingTactic()));
                isEconomic_ = true;
            }

            if (node.isGovernmentCenter)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new GovCenterTactic(!node.makesCityCapital)));
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

            if (node.nFreeTechs > 0)
            {
                pTactics_.push_back(ICityBuildingTacticPtr(new FreeTechBuildingTactic()));
                isEconomic_ = true;
            }
        }

        std::list<ICityBuildingTacticPtr> getTactics() const
        {
            return pTactics_;
        }

    private:
        BuildingTypes buildingType_;
        const Player& player_;
        std::list<ICityBuildingTacticPtr> pTactics_;
        bool isEconomic_;
    };

    std::list<ICityBuildingTacticPtr> makeCityBuildingTacticsItems(const Player& player, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        MakeBuildingTacticItemsVisitor visitor(player, pBuildingInfo->getBuildingType());
        boost::apply_visitor(visitor, pBuildingInfo->getInfo());
        return visitor.getTactics();
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
        ILimitedBuildingTacticsPtr pTactic(new LimitedBuildingTactic(pBuildingInfo->getBuildingType()));

        CityIter iter(*player.getCvPlayer());

        while (CvCity* pCity = iter())
        {
            if (couldConstructBuilding(player, player.getCity(pCity), lookAheadDepth, pBuildingInfo, true))
            {
#ifdef ALTAI_DEBUG
                CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for building: "
                    << gGlobals.getBuildingInfo(pBuildingInfo->getBuildingType()).getType();
#endif
                pTactic->addCityTactic(pCity->getIDInfo(), makeCityBuildingTactics(player, player.getCity(pCity), pBuildingInfo));
            }
        }

        return pTactic;
    }

    ILimitedBuildingTacticsPtr makeNationalBuildingTactics(const Player& player, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        const int lookAheadDepth = 2;
        ILimitedBuildingTacticsPtr pTactic(new LimitedBuildingTactic(pBuildingInfo->getBuildingType()));

        CityIter iter(*player.getCvPlayer());

        while (CvCity* pCity = iter())
        {
            if (!pCity->isNationalWondersMaxed())
            {
                if (couldConstructBuilding(player, player.getCity(pCity), lookAheadDepth, pBuildingInfo, true))
                {
#ifdef ALTAI_DEBUG
                    CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__
                        << " Adding tactic for building: " << gGlobals.getBuildingInfo(pBuildingInfo->getBuildingType()).getType() << " for city: " << safeGetCityName(pCity);
#endif
                    pTactic->addCityTactic(pCity->getIDInfo(), makeCityBuildingTactics(player, player.getCity(pCity), pBuildingInfo));
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
            pTactic->addTechDependency(ResearchTechDependencyPtr(new ResearchTechDependency(techType)));

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