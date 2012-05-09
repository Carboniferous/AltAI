#include "./building_tactics_visitors.h"
#include "./building_info_visitors.h"
#include "./building_tactics_items.h"
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

    class ProjectedEconomicImpactVisitor : public boost::static_visitor<>
    {
    public:
        ProjectedEconomicImpactVisitor(const Player& player, const City& city, const CityDataPtr& pCityData, int selectedEconomicFlags)
            : player_(player), city_(city), pCityData_(pCityData),
              improvementManager_(player.getAnalysis()->getMapAnalysis()->getImprovementManager(city.getCvCity()->getIDInfo())),
              selectedEconomicFlags_(selectedEconomicFlags),
              civLog_(CivLog::getLog(*player.getCvPlayer())->getStream())
        {
        }

        TotalOutput getOutputDelta() const
        {
            return totalOutputDelta_;
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

        void operator() (const BuildingInfo::MiscEffectNode& node)
        {
            //if (node.foodKeptPercent > 0 && (selectedEconomicFlags_ & EconomicFlags::Output_Food))
            //{
            //    const bool canHurryPop = CvPlayerAI::getPlayer(player_.getPlayerID()).canPopRush();

            //    const int requiredFood = 100 * pCityData_->getPopulation() * gGlobals.getFOOD_CONSUMPTION_PER_POPULATION() + pCityData_->getLostFood();

            //    const int foodDelta = city_.getMaxOutputs()[YIELD_FOOD] - requiredFood;

            //    const int size = std::max<int>(pCityData_->getPopulation(), pCityData_->getPopulation() + pCityData_->happyCap);
            //    PlotYield projectedCityYield = improvementManager_.getProjectedYield(size);
            //    TotalOutput projectedCityOutput = makeOutput(projectedCityYield, pCityData_->getYieldModifier(), pCityData_->getCommerceModifier(), pCityData_->getCommercePercent());

            //    //civLog_ << "\nProjectedEconomicImpactVisitor(): misc : " << canHurryPop << ", " <<  requiredFood << ", " << foodDelta << ", " << size << ", " << projectedCityYield << ", " << projectedCityOutput;

            //    if (foodDelta > 0)
            //    {
            //        int multiplier = (foodDelta * node.foodKeptPercent) / 10000;
            //        TotalOutput value = projectedCityOutput * multiplier;

            //        if (canHurryPop)
            //        {
            //            value *= (100 + node.foodKeptPercent);
            //            value /= 100;
            //        }
            //        totalOutputDelta_ += value;
            //        //civLog_ << ", " << multiplier << ", " << value;
            //    }
            //}
        }

    private:
        const Player& player_;
        const City& city_;
        CityDataPtr pCityData_;
        CityImprovementManager& improvementManager_;
        int selectedEconomicFlags_;
        TotalOutput totalOutputDelta_;

        std::ostream& civLog_;
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
        MakeBuildingTacticsDependenciesVisitor(const Player& player, const City& city)
            : player_(player), city_(city)
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
                int buildingCount = 0;
                CityIter iter(*player_.getCvPlayer());
                while (CvCity* pCity = iter())
                {
                    buildingCount += pCity->getNumBuilding(node.buildingCounts[i].first);

                    if (buildingCount >= node.buildingCounts[i].second)
                    {
                        break;
                    }
                }

                if (buildingCount < node.buildingCounts[i].second)
                {
                    dependentTactics_.push_back(IDependentTacticPtr(
                        new CivBuildingDependency(node.buildingCounts[i].first, node.buildingCounts[i].second - buildingCount)));
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
        std::vector<IDependentTacticPtr> dependentTactics_;
    };

    class MakeBuildingTacticsVisitor : public boost::static_visitor<>
    {
    public:
        MakeBuildingTacticsVisitor(const Player& player, const City& city, BuildingTypes buildingType)
            : player_(player), city_(city), buildingType_(buildingType)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const BuildingInfo::BaseNode& node)
        {
            pTactic_ = ICityBuildingTacticsPtr(new CityBuildingTactic(buildingType_));

            MakeBuildingTacticsDependenciesVisitor dependentTacticsVisitor(player_, city_);
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
            }

            if (node.health > 0)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new HealthBuildingTactic()));
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
        }

        void operator() (const BuildingInfo::CommerceNode& node)
        {
            if (node.modifier[COMMERCE_RESEARCH] > 0 || node.obsoleteSafeCommerce[COMMERCE_RESEARCH] > 0 || node.commerce[COMMERCE_RESEARCH] > 0)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new ScienceBuildingTactic()));
            }
            if (node.modifier[COMMERCE_GOLD] > 0 || node.obsoleteSafeCommerce[COMMERCE_GOLD] > 0 || node.commerce[COMMERCE_GOLD] > 0)
            {
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
        }

        void operator() (const BuildingInfo::MiscEffectNode& node)
        {
            if (node.foodKeptPercent > 0)
            {
                pTactic_->addTactic(ICityBuildingTacticPtr(new FoodBuildingTactic()));
            }
        }

        void operator() (const BuildingInfo::ReligionNode& node)
        {
            if (node.prereqReligion != NO_RELIGION && !city_.getCvCity()->isHasReligion(node.prereqReligion))
            {
                pTactic_->addDependency(IDependentTacticPtr(new ReligiousDependency(node.prereqReligion)));
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

    TotalOutput getProjectedEconomicImpact(const Player& player, const City& city, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int selectedEconomicFlags)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        CityDataPtr pCityData = city.getCityData()->clone();
        CitySimulation simulation(city.getCvCity(), pCityData, city.getConstructItem());

        simulation.optimisePlots();
        //simulation.getCityOptimiser()->debug(os, false);
        
        TotalOutput baseOutput = pCityData->getActualOutput();
        baseOutput[OUTPUT_GOLD] -= pCityData->getMaintenanceHelper()->getMaintenance();
#ifdef ALTAI_DEBUG
        os << "\nbaseOutput = " << baseOutput << " city output = " << pCityData->getCityPlotOutput().actualOutput << " maintenance = " << pCityData->getMaintenanceHelper()->getMaintenance();
#endif
        updateRequestData(*pCityData, pBuildingInfo);
        pCityData->getBuildingsHelper()->changeNumRealBuildings(pBuildingInfo->getBuildingType());
        pCityData->recalcOutputs();

        simulation.optimisePlots();
        //simulation.getCityOptimiser()->debug(os, false);
        TotalOutput newOutput = pCityData->getActualOutput();
        newOutput[OUTPUT_GOLD] -= pCityData->getMaintenanceHelper()->getMaintenance();
#ifdef ALTAI_DEBUG        
        os << "\nnew baseOutput = " << newOutput << " city output = " << pCityData->getCityPlotOutput().actualOutput << " maintenance = " << pCityData->getMaintenanceHelper()->getMaintenance();

        os << "\nDelta output = " << newOutput - baseOutput;
#endif
        const int cost = player.getCvPlayer()->getProductionNeeded(pBuildingInfo->getBuildingType());
        // TODO - use correct yield modifiers for trait specific speedups (e.g. libs for creative)
        const int approxBuildTurns = std::max<int>(1, 100 * cost / std::max<int>(1, city.getMaxOutputs()[OUTPUT_PRODUCTION]));

#ifdef ALTAI_DEBUG
        os << "\nCost = " << cost << ", approx build T = " << approxBuildTurns;
#endif
        ProjectedEconomicImpactVisitor visitor(player, city, pCityData, selectedEconomicFlags);
        boost::apply_visitor(visitor, pBuildingInfo->getInfo());

        TotalOutput projectedOutput = visitor.getOutputDelta() + newOutput - baseOutput;

#ifdef ALTAI_DEBUG
        os << "\nprojectedOutput = " << projectedOutput;
#endif
        // TODO - use t-horizon logic here
        projectedOutput = std::max<int>(0, 50 - approxBuildTurns) * projectedOutput;

#ifdef ALTAI_DEBUG
        os << " scaled = " << projectedOutput;
#endif
        return projectedOutput;
    }

    ICityBuildingTacticsPtr makeCityBuildingTactics(const Player& player, const City& city, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        MakeBuildingTacticsVisitor visitor(player, city, pBuildingInfo->getBuildingType());
        boost::apply_visitor(visitor, pBuildingInfo->getInfo());
        return visitor.getTactic();
    }

    ILimitedBuildingTacticsPtr makeGlobalBuildingTactics(const Player& player, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        ILimitedBuildingTacticsPtr pTactic(new GlobalBuildingTactic(pBuildingInfo->getBuildingType()));

        CityIter iter(*player.getCvPlayer());

        while (CvCity* pCity = iter())
        {
            pTactic->addCityTactic(pCity->getIDInfo(), makeCityBuildingTactics(player, player.getCity(pCity->getID()), pBuildingInfo));
        }

        return pTactic;
    }

    ILimitedBuildingTacticsPtr makeNationalBuildingTactics(const Player& player, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        ILimitedBuildingTacticsPtr pTactic(new NationalBuildingTactic(pBuildingInfo->getBuildingType()));

        CityIter iter(*player.getCvPlayer());

        while (CvCity* pCity = iter())
        {
            if (!pCity->isNationalWondersMaxed())
            {
                pTactic->addCityTactic(pCity->getIDInfo(), makeCityBuildingTactics(player, player.getCity(pCity->getID()), pBuildingInfo));
            }
        }

        return pTactic;
    }
}