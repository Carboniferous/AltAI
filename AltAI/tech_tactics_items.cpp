#include "AltAI.h"

#include "./city_unit_tactics.h"
#include "./tech_tactics_items.h"
#include "./tactic_selection_data.h"
#include "./gamedata_analysis.h"
#include "./city_tactics.h"
#include "./city_data.h"
#include "./building_tactics_deps.h"
#include "./building_info_visitors.h"
#include "./tech_info_visitors.h"
#include "./resource_info_visitors.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./iters.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./settler_manager.h"
#include "./civ_helper.h"
#include "./civ_log.h"
#include "./error_log.h"

namespace AltAI
{
    CityImprovementTacticsPtr CityImprovementTactics::factoryRead(FDataStreamBase* pStream)
    {
        CityImprovementTacticsPtr pCityImprovementTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pCityImprovementTactics = CityImprovementTacticsPtr(new CityImprovementTactics());
            break;
        default:
            break;
        }

        pCityImprovementTactics->read(pStream);
        return pCityImprovementTactics;
    }

    CityImprovementTactics::CityImprovementTactics(const std::vector<PlotImprovementData>& plotData) : plotData_(plotData)
    {
    }

    void CityImprovementTactics::addTactic(const IWorkerBuildTacticPtr& pBuildTactic)
    {
        buildTactics_.push_back(pBuildTactic);
    }

    void CityImprovementTactics::addDependency(const ResearchTechDependencyPtr& pDependentTactic)
    {
        dependentTactics_.push_back(pDependentTactic);
    }

    void CityImprovementTactics::update(const Player& player, const CityDataPtr& pCityData)
    {
        CityDataPtr pBaseCityData(pCityData->clone()), pSimulationCityData(new CityData(pCityData->getCity(), plotData_, true));
        //ErrorLog::getLog(*player.getCvPlayer())->getStream() << "\nnew CityData at: " << pSimulationCityData.get();

        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->apply(pSimulationCityData);
        }
        
        ConstructItem constructItem;

        std::vector<IProjectionEventPtr> events;
        base_ = getProjectedOutput(player, pBaseCityData, 30, events, constructItem, __FUNCTION__);

        events.clear();
        projection_ = getProjectedOutput(player, pSimulationCityData, 30, events, constructItem, __FUNCTION__);

        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->remove(pSimulationCityData);
        }
    }

    void CityImprovementTactics::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& tacticSelectionData)
    {
        std::map<UnitTypes, WorkerUnitValue>::iterator iter = tacticSelectionData.workerUnits.find(pCityUnitTactics->getUnitType());

        if (iter != tacticSelectionData.workerUnits.end())
        {
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(pCityUnitTactics->getUnitType());
            TotalOutput improvementsDelta = projection_.getOutput() - base_.getOutput();

            for (size_t i = 0, count = plotData_.size(); i < count; ++i)
            {
                if (plotData_[i].isSelectedAndNotBuilt())
                {
                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(plotData_[i].improvement);
                    if (buildType == NO_BUILD && plotData_[i].removedFeature != NO_FEATURE)
                    {
                        buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(plotData_[i].removedFeature);
                    }

                    if (buildType == NO_BUILD)
                    {
                        continue;
                    }

                    if (unitInfo.getBuilds(buildType))
                    {
                        std::vector<TechTypes> techs;
                        for (size_t j = 0, prereqCount = dependentTactics_.size(); j < prereqCount; ++j)
                        {
                            techs.push_back(dependentTactics_[j]->getResearchTech());
                        }

						/*if (plotData_[i].removedFeature != NO_FEATURE)
						{
							TechTypes techToRemoveFeature = GameDataAnalysis::getTechTypeToRemoveFeature(plotData_[i].removedFeature);
							if (techToRemoveFeature != NO_TECH)
							{
								techs.push_back(techToRemoveFeature);							
							}
						}*/

                        iter->second.addBuild(buildType, boost::make_tuple(plotData_[i].coords, pCityUnitTactics->getCity(), improvementsDelta / count, techs));
                    }
                }
            }
        }
    }

    const std::vector<ResearchTechDependencyPtr>& CityImprovementTactics::getTechDependencies() const
    {
        return dependentTactics_;
    }

    const std::vector<PlotImprovementData>& CityImprovementTactics::getImprovements() const
    {
        return plotData_;
    }

    ProjectionLadder CityImprovementTactics::getProjection() const
    {
        return projection_;
    }

    ProjectionLadder CityImprovementTactics::getBaseProjection() const
    {
        return base_;
    }

    void CityImprovementTactics::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nCity improvements: ";
        for (size_t i = 0, count = plotData_.size(); i < count; ++i)
        {
            CityImprovementManager::logImprovement(os, plotData_[i]);
        }

        for (std::list<IWorkerBuildTacticPtr>::const_iterator ci(buildTactics_.begin()), ciEnd(buildTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->debug(os);
        }
        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->debug(os);
        }
#endif
    }

    void CityImprovementTactics::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        size_t depCount = dependentTactics_.size();
        pStream->Write(depCount);
        for (size_t i = 0; i < depCount; ++i)
        {
            dependentTactics_[i]->write(pStream);
        }

        pStream->Write(buildTactics_.size());
        for (std::list<IWorkerBuildTacticPtr>::const_iterator ci(buildTactics_.begin()), ciEnd(buildTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->write(pStream);
        }

        CityImprovementManager::writeImprovements(pStream, plotData_);

        projection_.write(pStream);
    }

    void CityImprovementTactics::read(FDataStreamBase* pStream)
    {
        size_t depCount;
        pStream->Read(&depCount);
        for (size_t i = 0; i < depCount; ++i)
        {
            ResearchTechDependencyPtr pDependentTactic(new ResearchTechDependency());

            int ID;
            pStream->Read(&ID);  // this should always be 0

            pDependentTactic->read(pStream);
            dependentTactics_.push_back(pDependentTactic);
        }

        size_t buildTacticCount;
        pStream->Read(&buildTacticCount);
        for (size_t i = 0; i < buildTacticCount; ++i)
        {
            buildTactics_.push_back(IWorkerBuildTactic::factoryRead(pStream));
        }

        CityImprovementManager::readImprovements(pStream, plotData_);

        projection_.read(pStream);
    }


    void EconomicImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tEconomic improvement";
#endif
    }

    void EconomicImprovementTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void EconomicImprovementTactic::read(FDataStreamBase* pStream)
    {
    }


    void RemoveFeatureTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tRemove feature";
#endif
    }

    void RemoveFeatureTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void RemoveFeatureTactic::read(FDataStreamBase* pStream)
    {
    }


    void ProvidesResourceTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tProvides resource";
#endif
    }

    void ProvidesResourceTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void ProvidesResourceTactic::read(FDataStreamBase* pStream)
    {
    }


    void HappyImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHappy improvement";
#endif
    }

    void HappyImprovementTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void HappyImprovementTactic::read(FDataStreamBase* pStream)
    {
    }


    void HealthImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHealth improvement";
#endif
    }

    void HealthImprovementTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void HealthImprovementTactic::read(FDataStreamBase* pStream)
    {
    }


    void MilitaryImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tMilitary improvement";
#endif
    }

    void MilitaryImprovementTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void MilitaryImprovementTactic::read(FDataStreamBase* pStream)
    {
    }

    void FreeTechTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tFree tech tactic";
#endif
    }

    void FreeTechTactic::apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData)
    {
        const TechTypes techType = pTechTactics->getTechType();

        if (gGlobals.getGame().countKnownTechNumTeams(techType) == 0)
        {
            selectionData.getFreeTech = true;

            const PlayerTypes playerType = pTechTactics->getPlayer();
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType);
            const CvPlayer& player = CvPlayerAI::getPlayer(playerType);

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(player)->getStream();
            os << "\nChecking free tech data for tech: " << gGlobals.getTechInfo(techType).getType();
#endif
            std::list<TechTypes> prereqTechs = pushTechAndPrereqs(techType, *pPlayer);
            pPlayer->getAnalysis()->recalcTechDepths();

            std::vector<TechTypes> techs = pPlayer->getAnalysis()->getTechsWithDepth(1);

            int maxCost = 0;
            for (size_t i = 0, count = techs.size(); i < count; ++i)
            {
                const int thisCost = calculateTechResearchCost(techs[i], playerType);
                maxCost = std::max<int>(maxCost, thisCost);
#ifdef ALTAI_DEBUG
                os << "\n\tTech: " << gGlobals.getTechInfo(techs[i]).getType() << " has depth = 1 and research cost: " << thisCost;
#endif
            }

            for (std::list<TechTypes>::const_iterator ci(prereqTechs.begin()), ciEnd(prereqTechs.end()); ci != ciEnd; ++ci)
            {
                pPlayer->getCivHelper()->removeTech(*ci);
            }
            pPlayer->getCivHelper()->removeTech(techType);
            pPlayer->getAnalysis()->recalcTechDepths();

            selectionData.freeTechValue = maxCost;
        }
    }

    void FreeTechTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void FreeTechTactic::read(FDataStreamBase* pStream)
    {
    }

    void FoundReligionTechTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tFound religion tech tactic: "
           << (religionType == NO_RELIGION ? " (no religion?) " : gGlobals.getReligionInfo(religionType).getType());
#endif
    }

    void FoundReligionTechTactic::apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData)
    {
        const TechTypes techType = pTechTactics->getTechType();

        if (gGlobals.getGame().countKnownTechNumTeams(techType) == 0 && religionType != NO_RELIGION)
        {
            CultureSourceValue cultureSource;
            cultureSource.cityValue = gGlobals.getReligionInfo(religionType).getHolyCityCommerce(COMMERCE_CULTURE);
            cultureSource.globalValue = gGlobals.getReligionInfo(religionType).getGlobalReligionCommerce(COMMERCE_CULTURE);
            if (cultureSource.globalValue > 0)
            {
                selectionData.cultureSources.push_back(cultureSource);
            }
        }
    }

    void FoundReligionTechTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(religionType);
    }

    void FoundReligionTechTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&religionType);
    }

    void ConnectsResourcesTechTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tConnects resource tech tactic, route type = " << (routeType != NO_ROUTE ? gGlobals.getRouteInfo(routeType).getType() : "none");
#endif
    }

    void ConnectsResourcesTechTactic::apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData)
    {
        const TechTypes techType = pTechTactics->getTechType();
        const PlayerTypes playerType = pTechTactics->getPlayer();
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType);

        std::vector<int> accessibleSubAreas = pPlayer->getAnalysis()->getMapAnalysis()->getAccessibleSubAreas(DOMAIN_LAND);

        for (size_t subAreaIndex = 0, subAreaCount = accessibleSubAreas.size(); subAreaIndex < subAreaCount; ++subAreaIndex)
        {
            // get resources in sub areas - then check if they're connected
            std::vector<CvPlot*> resourcePlots = pPlayer->getAnalysis()->getMapAnalysis()->getResourcePlots(accessibleSubAreas[subAreaIndex],
                std::vector<BonusTypes>(), playerType);

            for (size_t plotIndex = 0, plotCount = resourcePlots.size(); plotIndex < plotCount; ++plotIndex)
            {
                if (resourcePlots[plotIndex]->getRouteType() == NO_ROUTE && !resourcePlots[plotIndex]->isConnectedToCapital(playerType))
                {
                    selectionData.connectableResources[resourcePlots[plotIndex]->getBonusType()].push_back(resourcePlots[plotIndex]->getCoords());
                }
            }
        }
    }

    void ConnectsResourcesTechTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(routeType);
    }

    void ConnectsResourcesTechTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&routeType);
    }

    void ConstructBuildingTechTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tConstruct building tech tactic: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
    }

    void ConstructBuildingTechTactic::apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData)
    {
        CultureSourceValue cultureSource(gGlobals.getBuildingInfo(buildingType));

        if (cultureSource.globalValue > 0)
        {
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pTechTactics->getPlayer());
            XYCoords bestTargetPlot = pPlayer->getSettlerManager()->getBestPlot();
            if (bestTargetPlot != XYCoords())
            {
                int bestValue = 0;
                CultureSourceValue bestExistingCultureSource;
                const std::set<BuildingTypes>& availableBuildings = pPlayer->getAnalysis()->getPlayerTactics()->availableGeneralBuildingsList_;
                for (std::set<BuildingTypes>::const_iterator ci(availableBuildings.begin()), ciEnd(availableBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (couldConstructBuilding(*pPlayer, gGlobals.getMap().plot(bestTargetPlot.iX, bestTargetPlot.iY), 1, pPlayer->getAnalysis()->getBuildingInfo(*ci)))
                    {
                        CultureSourceValue existingCultureSource(gGlobals.getBuildingInfo(*ci));
                        const int thisValue = existingCultureSource.globalValue * 20 * 100 / std::max<int>(1, existingCultureSource.cityCost);

                        if (thisValue > bestValue)
                        {
                            bestExistingCultureSource = existingCultureSource;
                            bestValue = thisValue;
                        }
                    }
                }

                int ourValue = cultureSource.globalValue * 20 * 100 / std::max<int>(1, cultureSource.cityCost);

                if (ourValue > bestValue && couldConstructBuilding(*pPlayer, gGlobals.getMap().plot(bestTargetPlot.iX, bestTargetPlot.iY), 1, pPlayer->getAnalysis()->getBuildingInfo(buildingType)))
                {
                    selectionData.cultureSources.push_back(cultureSource);
                }
            }
        }
    }

    void ConstructBuildingTechTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(buildingType);
    }

    void ConstructBuildingTechTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&buildingType);
    }

    void ProvidesResourceTechTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tProvides resource tech tactic: " << (bonusType != NO_BONUS ? gGlobals.getBonusInfo(bonusType).getType() : "none");
#endif
    }

    void ProvidesResourceTechTactic::apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData)
    {
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pTechTactics->getPlayer());
        int ourResourceCount = pPlayer->getAnalysis()->getMapAnalysis()->getControlledResourceCount(bonusType);
        // todo - add check for being able to build the resource improvement (e.g. can use oil with TECH_COMBUSTION, can build offshore platform with TECH_PLASTICS)
        if (ourResourceCount > 0)
        {
            boost::shared_ptr<ResourceInfo> pResourceInfo = pPlayer->getAnalysis()->getResourceInfo(bonusType);
            CityIter cityIter(*pPlayer->getCvPlayer());
            while (CvCity* pCity = cityIter())
            {
                const City& city = pPlayer->getCity(pCity);
                ProjectionLadder base, comparison;
                
                // todo - wrap resource in projection event
                {
                    CityDataPtr pCityData = city.getCityData()->clone();
                    std::vector<IProjectionEventPtr> events;
                    base = getProjectedOutput(*pPlayer, pCityData, 30, events, ConstructItem(), __FUNCTION__, false);
                }

                {
                    CityDataPtr pCityData = city.getCityData()->clone();
                    updateCityData(*pCityData, pResourceInfo, true);
                    std::vector<IProjectionEventPtr> events;
                    comparison = getProjectedOutput(*pPlayer, pCityData, 30, events, ConstructItem(), __FUNCTION__, false);
                }

                selectionData.resourceOutput += comparison.getOutput() - base.getOutput();
            }
        }
    }

    void ProvidesResourceTechTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(bonusType);
    }

    void ProvidesResourceTechTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&bonusType);
    }
}