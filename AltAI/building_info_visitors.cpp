#include "AltAI.h"

#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./utils.h"
#include "./buildings_info.h"
#include "./building_info_visitors.h"
#include "./happy_helper.h"
#include "./health_helper.h"
#include "./modifiers_helper.h"
#include "./specialist_helper.h"
#include "./trade_route_helper.h"
#include "./building_helper.h"
#include "./area_helper.h"
#include "./unit_helper.h"
#include "./bonus_helper.h"
#include "./vote_helper.h"
#include "./maintenance_helper.h"
#include "./player_analysis.h"
#include "./helper_fns.h"
#include "./iters.h"
#include "./civ_log.h" 
#include "./visitor_utils.h"

//#include "./hurry_helper.h"
//
//
//
//
//
//
//
//
//#include "./religion_helper.h"
//
//
//
//
//
//
//#include "./gamedata_analysis.h"
//#include "./civ_helper.h"
//

namespace AltAI
{
    class ResourceInfo;
    void updateRequestData(CityData& data, const boost::shared_ptr<ResourceInfo>& pResourceInfo, bool isAdding);
    std::pair<int, int> getResourceMilitaryUnitCount(const boost::shared_ptr<ResourceInfo>& pResourceInfo);

    namespace
    {
        // update CityData with changes resulting from construction of supplied BuildingInfo nodes
        class CityBuildingsUpdater : public boost::static_visitor<>
        {
        public:
            CityBuildingsUpdater(CityData& data, BuildingTypes buildingType) : data_(data), buildingType_(buildingType)
            {
                pPlayer_ = gGlobals.getGame().getAltAI()->getPlayer(data.getOwner()); 
            }

            template <typename T>
                result_type operator() (const T&) const
            {
            }

            void operator() (const BuildingInfo::BaseNode& node) const
            {
                if (node.hurryCostModifier != 0)
                {
                    data_.getHurryHelper()->changeCostModifier(node.hurryCostModifier);
                }
                // add any base happy/health
                if (node.happy > 0)
                {
                    data_.getHappyHelper()->changeBuildingGoodHappiness(node.happy);
                }
                else if (node.happy < 0)
                {
                    data_.getHappyHelper()->changeBuildingBadHappiness(node.happy);
                }
                data_.changeWorkingPopulation();

                if (node.health > 0)
                {
                    data_.getHealthHelper()->changeBuildingGoodHealthiness(node.health);
                }
                else if (node.health < 0)
                {
                    data_.getHealthHelper()->changeBuildingBadHealthiness(node.health);
                }

                for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
                {
                    boost::apply_visitor(*this, node.nodes[i]);
                }
            }
            
            void operator() (const BuildingInfo::YieldNode& node) const
            {
                // TODO - check whether yield modifiers can be conditional on plots too (only store one in CityData)
                if (!isEmpty(node.modifier))
                {
                    if (node.modifier[YIELD_COMMERCE] != 0)
                    {
                        data_.changeCommerceYieldModifier(node.modifier[YIELD_COMMERCE]);
                    }
                    data_.getModifiersHelper()->changeYieldModifier(node.modifier);
                }

                if (!isEmpty(node.powerModifier))
                {
                    if (node.powerModifier[YIELD_COMMERCE] != 0)
                    {
                        data_.changeCommerceYieldModifier(node.modifier[YIELD_COMMERCE]);
                    }
                    data_.getModifiersHelper()->changePowerYieldModifier(node.powerModifier);
                }

                if (node.militaryProductionModifier != 0)
                {
                    data_.getModifiersHelper()->changeMilitaryProductionModifier(node.militaryProductionModifier);
                }

                if (!node.plotCond)  // applies to building itself
                {
                    data_.getCityPlotData().plotYield += node.yield;
                }
                else
                {
                    int xCoord = data_.getCityPlotData().coords.iX, yCoord = data_.getCityPlotData().coords.iY;
                    CvPlot* pPlot = gGlobals.getMap().plot(xCoord, yCoord);

                    if ((pPlot->*(node.plotCond))())
                    {
                        data_.getCityPlotData().plotYield += node.yield;
                    }

                    // update plot yields
                    for (PlotDataListIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                    {
                        if (iter->isActualPlot())
                        {
                            xCoord = iter->coords.iX, yCoord = iter->coords.iY;
                            pPlot = gGlobals.getMap().plot(xCoord, yCoord);
                            if ((pPlot->*(node.plotCond))())
                            {
                                iter->plotYield += node.yield;
                            }
                        }
                    }
                }
            }

            void operator() (const BuildingInfo::SpecialistNode& node) const
            {
                // find 'plots' which represent specialists and update their outputs
                for (size_t i = 0, count = node.specialistTypesAndYields.size(); i < count; ++i)
                {
                    for (PlotDataListIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                    {
                        // TODO - double check everything is in correct units here
                        if (!iter->isActualPlot() && (SpecialistTypes)iter->coords.iY == node.specialistTypesAndYields[i].first)
                        {
                            iter->plotYield += node.specialistTypesAndYields[i].second;
                            iter->commerce += node.extraCommerce;

                            TotalOutput specialistOutput(makeOutput(iter->plotYield, iter->commerce, makeYield(100, 100, data_.getCommerceYieldModifier()), makeCommerce(100, 100, 100, 100), data_.getCommercePercent()));
                            iter->actualOutput = iter->output = specialistOutput;
                        }
                    }
                }

                // change city gpp rate
                if (node.cityGPPRateModifier != 0)
                {
                    data_.getSpecialistHelper()->changeCityGPPModifier(node.cityGPPRateModifier);
                }

                // node.generatedGPP - data added in calcCityOutput_() for this building
            }

            void operator() (const BuildingInfo::CommerceNode& node) const
            {
                // update city commerce and/or commerce modifier
                data_.getCityPlotData().commerce += 100 * node.commerce;
                data_.getCityPlotData().commerce += 100 * node.obsoleteSafeCommerce;

                if (!isEmpty(node.modifier))
                {
                    data_.getModifiersHelper()->changeCommerceModifier(node.modifier);
                }

                if (!isEmpty(node.stateReligionCommerce))
                {
                    data_.getModifiersHelper()->changeStateReligionCommerceModifier(node.stateReligionCommerce);
                }
            }

            void operator() (const BuildingInfo::TradeNode& node) const
            {
                if (node.extraTradeRoutes != 0)
                {
                    data_.getTradeRouteHelper()->changeNumRoutes(node.extraTradeRoutes);
                }

                if (node.extraCoastalTradeRoutes != 0 && data_.getCity()->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
                {
                    data_.getTradeRouteHelper()->changeNumRoutes(node.extraCoastalTradeRoutes);
                }

                if (node.extraGlobalTradeRoutes != 0)
                {
                    data_.getTradeRouteHelper()->changeNumRoutes(node.extraGlobalTradeRoutes);
                }

                if (node.tradeRouteModifier != 0)
                {
                    data_.getTradeRouteHelper()->changeTradeRouteModifier(node.tradeRouteModifier);
                }

                if (node.foreignTradeRouteModifier != 0)
                {
                    data_.getTradeRouteHelper()->changeForeignTradeRouteModifier(node.foreignTradeRouteModifier);
                }
            }

            void operator() (const BuildingInfo::PowerNode& node) const
            {
                data_.getBuildingsHelper()->updatePower(node.isDirty, true);
                if (node.areaCleanPower)  // stored in building helper to save passing areaHelper - todo - redesign CityData so helpers keep back pointer to it
                {
                    data_.getAreaHelper()->changeCleanPowerCount(true);
                    data_.getBuildingsHelper()->updateAreaCleanPower(data_.getAreaHelper()->getCleanPowerCount() > 0);
                }

                data_.getHealthHelper()->updatePowerHealth(data_);  // double use of CityData reinforces above comment!
            }

            void operator() (const BuildingInfo::UnitExpNode& node) const
            {
                if (node.freeExperience != 0)
                {
                    data_.getUnitHelper()->changeUnitFreeExperience(node.freeExperience);
                }
                if (node.globalFreeExperience != 0)
                {
                    data_.getUnitHelper()->changeUnitFreeExperience(node.globalFreeExperience);
                }
                for (int i = 0, count = node.domainFreeExperience.size(); i < count; ++i)
                {
                    if (node.domainFreeExperience[i].second != 0)
                    {
                        data_.getUnitHelper()->changeDomainFreeExperience(node.domainFreeExperience[i].first, node.domainFreeExperience[i].second);
                    }
                }
                for (int i = 0, count = node.combatTypeFreeExperience.size(); i < count; ++i)
                {
                    if (node.combatTypeFreeExperience[i].second != 0)
                    {
                        data_.getUnitHelper()->changeUnitCombatTypeFreeExperience(node.combatTypeFreeExperience[i].first, node.combatTypeFreeExperience[i].second);
                    }
                }
            }

            void operator() (const BuildingInfo::SpecialistSlotNode& node) const
            {
                const CvPlayer& player = CvPlayerAI::getPlayer(data_.getOwner());

                // add any 'plots' for new specialist slots (if we aren't maxed out already for that type)
                for (size_t i = 0, count = node.specialistTypes.size(); i < count; ++i)
                {
                    int specialistCount = data_.getNumPossibleSpecialists(node.specialistTypes[i].first);

                    // todo - should this be total population?
                    if (specialistCount < data_.getWorkingPopulation())  // if this is also changed by this building (e.g. HG)? - OK as long as add pop code updates specs
                    {
                        data_.addSpecialistSlots(node.specialistTypes[i].first, std::min<int>(node.specialistTypes[i].second, data_.getWorkingPopulation() - specialistCount));
                    }
                }

                // add free specialists
                for (size_t i = 0, count = node.freeSpecialistTypes.size(); i < count; ++i)
                {
                    data_.getSpecialistHelper()->changeFreeSpecialistCount(node.freeSpecialistTypes[i].first, node.freeSpecialistTypes[i].second);
                }

                // update city data to include any free specialists added for given improvement types (National Park + Forest Preserves)
                for (size_t i = 0, count = node.improvementFreeSpecialists.size(); i < count; ++i)
                {
                    data_.changeFreeSpecialistCountPerImprovement(node.improvementFreeSpecialists[i].first, node.improvementFreeSpecialists[i].second);
                }
            }

            void operator() (const BuildingInfo::BonusNode& node) const
            {
                // check for increased health/happy from bonus
                if (data_.getBonusHelper()->getNumBonuses(node.bonusType) > 0)
                {
                    if (node.happy > 0)
                    {
                        data_.getHappyHelper()->changeBonusGoodHappiness(node.happy);
                    }
                    else if (node.happy < 0)
                    {
                        data_.getHappyHelper()->changeBonusBadHappiness(node.happy);
                    }
                    data_.changeWorkingPopulation();

                    if (node.health > 0)
                    {
                        data_.getHealthHelper()->changeBonusGoodHealthiness(node.health);
                    }
                    else if (node.health < 0)
                    {
                        data_.getHealthHelper()->changeBonusBadHealthiness(node.health);
                    }

                    if (!isEmpty(node.yieldModifier))
                    {
                        if (node.yieldModifier[YIELD_COMMERCE] != 0)
                        {
                            data_.changeCommerceYieldModifier(node.yieldModifier[YIELD_COMMERCE]);
                        }
                        data_.getModifiersHelper()->changeBonusYieldModifier(node.yieldModifier);
                    }

                    // process new bonus
                    if (node.freeBonusCount > 0)
                    {
                        data_.getBonusHelper()->changeNumBonuses(node.bonusType, node.freeBonusCount);
                        updateRequestData(data_, pPlayer_->getAnalysis()->getResourceInfo(node.bonusType), true); 
                    }

                    // remove access to bonus for this city
                    if (node.isRemoved)
                    {
                        data_.getBonusHelper()->allowOrDenyBonus(node.bonusType, false);                
                        updateRequestData(data_, pPlayer_->getAnalysis()->getResourceInfo(node.bonusType), false);
                    }
                }
            }

            void operator() (const BuildingInfo::CityDefenceNode& node) const
            {
                if (node.defenceBonus != 0)
                {
                    data_.getUnitHelper()->changeBuildingDefence(node.defenceBonus);
                }
                if (node.globalDefenceBonus != 0)
                {
                    data_.getUnitHelper()->changeBuildingDefence(node.globalDefenceBonus);
                }
            }

            void operator() (const BuildingInfo::ReligionNode& node) const
            {
                if (node.religionType != NO_RELIGION)
                {
                    PlotYield yieldChange = data_.getVoteHelper()->getReligiousBuildingYieldChange(node.religionType);
                    Commerce commerceChange = data_.getVoteHelper()->getReligiousBuildingCommerceChange(node.religionType);

                    if (!isEmpty(yieldChange))
                    {
                        data_.getBuildingsHelper()->changeBuildingYieldChange(getBuildingClass(buildingType_), yieldChange);
                    }

                    if (!isEmpty(commerceChange))
                    {
                        data_.getBuildingsHelper()->changeBuildingCommerceChange(getBuildingClass(buildingType_), commerceChange);
                    }
                }
            }

            void operator() (const BuildingInfo::HurryNode& node) const
            {
                if (node.hurryAngerModifier != 0)
                {
                    data_.getHurryHelper()->changeModifier(node.hurryAngerModifier);
                }
                if (node.globalHurryCostModifier != 0)
                {
                    data_.getHurryHelper()->changeCostModifier(node.globalHurryCostModifier);
                }
            }

            void operator() (const BuildingInfo::AreaEffectNode& node) const
            {
                if (node.areaHealth > 0)
                {
                    data_.getHealthHelper()->changeAreaBuildingGoodHealthiness(node.areaHealth);
                }
                else if (node.areaHealth < 0)
                {
                    data_.getHealthHelper()->changeAreaBuildingBadHealthiness(node.areaHealth);
                }
                
                if (node.globalHealth > 0)
                {
                    data_.getHealthHelper()->changePlayerBuildingGoodHealthiness(node.areaHealth);
                }
                else if (node.globalHealth < 0)
                {
                    data_.getHealthHelper()->changePlayerBuildingBadHealthiness(node.areaHealth);
                }
                
                if (node.areaHappy != 0)
                {
                    data_.getHappyHelper()->changeAreaBuildingHappiness(node.areaHappy);
                }
                
                if (node.globalHappy != 0)
                {
                    data_.getHappyHelper()->changePlayerBuildingHappiness(node.globalHappy);
                }
            }

            void operator() (const BuildingInfo::MiscEffectNode& node) const
            {
                if (node.cityMaintenanceModifierChange != 0)
                {
                    data_.getMaintenanceHelper()->changeModifier(node.cityMaintenanceModifierChange);
                }

                if (node.isGovernmentCenter)
                {
                    data_.getMaintenanceHelper()->addGovernmentCentre(data_.getCity()->getIDInfo());
                }

                if (node.foodKeptPercent != 0)
                {
                    data_.changeFoodKeptPercent(node.foodKeptPercent);
                }

                if (node.hurryAngerModifier != 0)
                {
                    data_.getHurryHelper()->changeModifier(node.hurryAngerModifier);
                }

                if (node.globalPopChange != 0)
                {
                    data_.changePopulation(node.globalPopChange);
                }

                if (node.noUnhealthinessFromBuildings)
                {
                    data_.getHealthHelper()->setNoUnhealthinessFromBuildings();
                }

                if (node.noUnhealthinessFromPopulation)
                {
                    data_.getHealthHelper()->setNoUnhealthinessFromPopulation();
                }

                if (node.noUnhappiness)
                {
                    data_.getHappyHelper()->setNoUnhappiness(true);
                }
            }

        private:
            PlayerPtr pPlayer_;
            CityData& data_;
            BuildingTypes buildingType_;
        };

        // update CityData with changes resulting from construction of supplied BuildingInfo nodes from building constructed in another city
        class CityGlobalOutputUpdater : public boost::static_visitor<>
        {
        public:
            explicit CityGlobalOutputUpdater(const CityDataPtr& pCityData, const CvCity* pBuiltCity) : pCityData_(pCityData), pBuiltCity_(pBuiltCity)
            {
                pPlayer_ = gGlobals.getGame().getAltAI()->getPlayer(pCityData->getOwner()); 
            }

            template <typename T>
                result_type operator() (const T&) const
            {
            }

            void operator() (const BuildingInfo::BaseNode& node) const
            {
                for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
                {
                    boost::apply_visitor(*this, node.nodes[i]);
                }
            }

            void operator() (const BuildingInfo::SpecialistNode& node) const
            {
                // find 'plots' which represent specialists and update their outputs
                for (size_t i = 0, count = node.specialistTypesAndYields.size(); i < count; ++i)
                {
                    for (PlotDataListIter iter(pCityData_->getPlotOutputs().begin()), endIter(pCityData_->getPlotOutputs().end()); iter != endIter; ++iter)
                    {
                        // TODO - double check everything is in correct units here
                        if (!iter->isActualPlot() && (SpecialistTypes)iter->coords.iY == node.specialistTypesAndYields[i].first)
                        {
                            iter->plotYield += node.specialistTypesAndYields[i].second;
                            iter->commerce += node.extraCommerce;

                            TotalOutput specialistOutput(makeOutput(iter->plotYield, iter->commerce, 
                                makeYield(100, 100, pCityData_->getCommerceYieldModifier()), makeCommerce(100, 100, 100, 100), pCityData_->getCommercePercent()));
                            iter->actualOutput = iter->output = specialistOutput;
                        }
                    }
                }

                // change global gpp rate
                if (node.playerGPPRateModifier != 0)
                {
                    pCityData_->getSpecialistHelper()->changePlayerGPPModifier(node.playerGPPRateModifier);
                }

            }

            void operator() (const BuildingInfo::YieldNode& node) const
            {
                if (node.global)
                {
                    if (!isEmpty(node.modifier))
                    {
                        if (node.modifier[YIELD_COMMERCE] != 0)
                        {
                            pCityData_->changeCommerceYieldModifier(node.modifier[YIELD_COMMERCE]);
                        }
                        pCityData_->getModifiersHelper()->changeYieldModifier(node.modifier);
                    }

                    if (!isEmpty(node.powerModifier))
                    {
                        if (node.powerModifier[YIELD_COMMERCE] != 0)
                        {
                            pCityData_->changeCommerceYieldModifier(node.modifier[YIELD_COMMERCE]);
                        }
                        pCityData_->getModifiersHelper()->changePowerYieldModifier(node.powerModifier);
                    }

                    if (node.militaryProductionModifier != 0)
                    {
                        pCityData_->getModifiersHelper()->changeMilitaryProductionModifier(node.militaryProductionModifier);
                    }

                    if (node.plotCond)
                    {
                        int xCoord = pCityData_->getCityPlotData().coords.iX, yCoord = pCityData_->getCityPlotData().coords.iY;
                        CvPlot* pPlot = gGlobals.getMap().plot(xCoord, yCoord);

                        if ((pPlot->*(node.plotCond))())
                        {
                            pCityData_->getCityPlotData().plotYield += node.yield;
                        }

                        // update plot yields
                        for (PlotDataListIter iter(pCityData_->getPlotOutputs().begin()), endIter(pCityData_->getPlotOutputs().end()); iter != endIter; ++iter)
                        {
                            if (iter->isActualPlot())
                            {
                                xCoord = iter->coords.iX, yCoord = iter->coords.iY;
                                pPlot = gGlobals.getMap().plot(xCoord, yCoord);
                                if ((pPlot->*(node.plotCond))())
                                {
                                    iter->plotYield += node.yield;
                                }
                            }
                        }
                    }
                }
            }

            void operator() (const BuildingInfo::CommerceNode& node) const
            {
                if (node.global)
                {
                    if (!isEmpty(node.modifier))
                    {
                        pCityData_->getModifiersHelper()->changeCommerceModifier(node.modifier);
                    }

                    if (!isEmpty(node.stateReligionCommerce))
                    {
                        pCityData_->getModifiersHelper()->changeStateReligionCommerceModifier(node.stateReligionCommerce);
                    }
                }
            }

            void operator() (const BuildingInfo::TradeNode& node) const
            {
                if (node.extraCoastalTradeRoutes != 0 && pCityData_->getCity()->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
                {
                    pCityData_->getTradeRouteHelper()->changeNumRoutes(node.extraCoastalTradeRoutes);
                }

                if (node.extraGlobalTradeRoutes != 0)
                {
                    pCityData_->getTradeRouteHelper()->changeNumRoutes(node.extraGlobalTradeRoutes);
                }
            }

            void operator() (const BuildingInfo::BonusNode& node) const
            {
                if (node.freeBonusCount > 0)
                {
                    pCityData_->getBonusHelper()->changeNumBonuses(node.bonusType, node.freeBonusCount);
                    updateRequestData(*pCityData_, pPlayer_->getAnalysis()->getResourceInfo(node.bonusType), true); 
                }
            }

            void operator() (const BuildingInfo::PowerNode& node) const
            {
                if (node.areaCleanPower && pCityData_->getCity()->getArea() == pBuiltCity_->getArea())
                {
                    pCityData_->getAreaHelper()->changeCleanPowerCount(true);
                    pCityData_->getBuildingsHelper()->updateAreaCleanPower(pCityData_->getAreaHelper()->getCleanPowerCount() > 0);
                    pCityData_->getHealthHelper()->updatePowerHealth(*pCityData_);
                }                
            }

            void operator() (const BuildingInfo::AreaEffectNode& node) const
            {
                if (node.areaHealth > 0)
                {
                    pCityData_->getHealthHelper()->changeAreaBuildingGoodHealthiness(node.areaHealth);
                }
                else if (node.areaHealth < 0)
                {
                    pCityData_->getHealthHelper()->changeAreaBuildingBadHealthiness(node.areaHealth);
                }
                
                if (node.globalHealth > 0)
                {
                    pCityData_->getHealthHelper()->changePlayerBuildingGoodHealthiness(node.areaHealth);
                }
                else if (node.globalHealth < 0)
                {
                    pCityData_->getHealthHelper()->changePlayerBuildingBadHealthiness(node.areaHealth);
                }
                
                if (node.areaHappy != 0)
                {
                    pCityData_->getHappyHelper()->changeAreaBuildingHappiness(node.areaHappy);
                }
                
                if (node.globalHappy != 0)
                {
                    pCityData_->getHappyHelper()->changePlayerBuildingHappiness(node.globalHappy);
                }
            }

            void operator() (const BuildingInfo::ReligionNode& node) const
            {
                // todo
            }

            void operator() (const BuildingInfo::HurryNode& node) const
            {
                if (node.globalHurryCostModifier != 0)
                {
                    pCityData_->getHurryHelper()->changeCostModifier(node.globalHurryCostModifier);
                }
            }

            void operator() (const BuildingInfo::MiscEffectNode& node) const
            {
                if (node.globalPopChange != 0)
                {
                    pCityData_->changePopulation(node.globalPopChange);
                }

                if (node.isGovernmentCenter)
                {
                    pCityData_->getMaintenanceHelper()->addGovernmentCentre(pBuiltCity_->getIDInfo());
                }

                if (node.freeBuildingType != NO_BUILDING)
                {
                    if (pCityData_->getBuildingsHelper()->getNumBuildings(node.freeBuildingType) == 0)
                    {
                        pCityData_->getBuildingsHelper()->changeNumFreeBuildings(node.freeBuildingType);
                    }
                }
            }

        private:
            PlayerPtr pPlayer_;
            CityDataPtr pCityData_;
            const CvCity* pBuiltCity_;
        };
    }

    void updateRequestData(CityData& data, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        boost::apply_visitor(CityBuildingsUpdater(data, pBuildingInfo->getBuildingType()), pBuildingInfo->getInfo());
        data.recalcOutputs();
    }

    void updateGlobalRequestData(const CityDataPtr& pCityData, const CvCity* pBuiltCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData->getOwner()))->getStream();
        os << "\nUpdating global data for city: " << narrow(pCityData->getCity()->getName());
        pCityData->debugBasicData(os);
#endif
        boost::apply_visitor(CityGlobalOutputUpdater(pCityData, pBuiltCity), pBuildingInfo->getInfo());
        pCityData->recalcOutputs();
#ifdef ALTAI_DEBUG
        os << "\n\toutput after: ";
        pCityData->debugBasicData(os);
#endif
    }

    boost::shared_ptr<BuildingInfo> makeBuildingInfo(BuildingTypes buildingType, PlayerTypes playerType)
    {
        return boost::shared_ptr<BuildingInfo>(new BuildingInfo(buildingType, playerType));
    }

    std::vector<TechTypes> getRequiredTechs(const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        const BuildingInfo::BaseNode& node = boost::get<BuildingInfo::BaseNode>(pBuildingInfo->getInfo());
        return node.techs;
    }

    struct EnabledUnitVisitor : public boost::static_visitor<>
    {
        template <typename T>
            result_type operator() (const T&) const
        {
        }

        void operator() (const BuildingInfo::UnitNode& node)
        {
            enabledUnitTypes_.push_back(node.enabledUnitType);
        }

        std::vector<UnitTypes> enabledUnitTypes_;
    };

    std::vector<UnitTypes> getEnabledUnits(const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        EnabledUnitVisitor enabledUnitVisitor;
        boost::apply_visitor(enabledUnitVisitor, pBuildingInfo->getInfo());
        return enabledUnitVisitor.enabledUnitTypes_;
    }

    // todo - make this just return the 'bad' bits in a complete BuildingInfo::BuildingInfoNode structure
    // currently returns all nodes which have any bad bit, plus the top level BaseNode
    class BadBuildingNodeFinder : public boost::static_visitor<BuildingInfo::BuildingInfoNode>
    {
    public:
        template <typename T>
            result_type operator() (const T& node) const
        {
            return BuildingInfo::BuildingInfoNode(BuildingInfo::NullNode());
        }

        result_type operator() (const BuildingInfo::YieldNode& node) const
        {
            return YieldAndCommerceFunctor<LessThanZero>()(node.modifier) || 
                YieldAndCommerceFunctor<LessThanZero>()(node.yield) ||
                YieldAndCommerceFunctor<LessThanZero>()(node.powerModifier) || 
                node.militaryProductionModifier < 0
                ? node : BuildingInfo::BuildingInfoNode(BuildingInfo::NullNode());
        }

        result_type operator() (const BuildingInfo::CommerceNode& node) const
        {
            return YieldAndCommerceFunctor<LessThanZero>()(node.commerce) || 
                YieldAndCommerceFunctor<LessThanZero>()(node.modifier) ||
                YieldAndCommerceFunctor<LessThanZero>()(node.obsoleteSafeCommerce) ||
                YieldAndCommerceFunctor<LessThanZero>()(node.stateReligionCommerce)
                ? node : BuildingInfo::BuildingInfoNode(BuildingInfo::NullNode());
        }

        result_type operator() (const BuildingInfo::BonusNode& node) const
        {
            return node.happy < 0 || node.health < 0 || node.prodModifier  < 0 ||
                YieldAndCommerceFunctor<LessThanZero>()(node.yieldModifier)
                ? node : BuildingInfo::BuildingInfoNode(BuildingInfo::NullNode());
        }

        result_type operator() (const BuildingInfo::AreaEffectNode& node) const
        {
            return node.areaHealth < 0 || node.globalHealth < 0 || node.areaHappy < 0 || node.globalHappy < 0
                ? node : BuildingInfo::BuildingInfoNode(BuildingInfo::NullNode());
        }

        result_type operator() (const BuildingInfo::BaseNode& node) const
        {
            BuildingInfo::BaseNode badBaseNode;
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                BuildingInfo::BuildingInfoNode potentialBadNode(boost::apply_visitor(*this, node.nodes[i]));
                if (!boost::apply_visitor(IsVariantOfType<BuildingInfo::NullNode>(), potentialBadNode))
                {
                    badBaseNode.nodes.push_back(potentialBadNode);
                }
            }

            badBaseNode.happy = node.happy < 0 ? node.happy : 0;
            badBaseNode.happy = node.health < 0 ? node.health : 0;
            
            return badBaseNode;
        }
    };

    class CommerceFinder : public boost::static_visitor<Commerce>
    {
    public:
        explicit CommerceFinder(const CvCity* pCity) : pCity_(pCity)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return Commerce();
        }

        result_type operator() (const BuildingInfo::CommerceNode& node)
        {
            PlotYield totalYield;
            CityPlotIter iter(pCity_);

            while (IterPlot pPlot = iter())
            {
                if (pPlot.valid() && pCity_->isWorkingPlot(pPlot))
                {
                    totalYield += pPlot->getYield();
                }
            }
            return node.commerce + node.obsoleteSafeCommerce;
        }

    private:
        const CvCity* pCity_;
    };

    Commerce getCommerceValue(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        return boost::apply_visitor(CommerceFinder(pCity), pBuildingInfo->getInfo());
    }   
}