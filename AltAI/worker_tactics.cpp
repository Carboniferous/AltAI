#include "AltAI.h"

#include "./worker_tactics.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./military_tactics.h"
#include "./unit_explore.h"
#include "./map_analysis.h"
#include "./tictacs.h"
#include "./city_unit_tactics.h"
#include "./city_building_tactics.h"
#include "./building_tactics_deps.h"
#include "./unit_analysis.h"
#include "./unit_tactics.h"
#include "./resource_info_visitors.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./gamedata_analysis.h"
#include "./save_utils.h"
#include "./civ_log.h"
#include "./unit_log.h"

#include "../CvGameCoreDLL/CvDLLEngineIFaceBase.h"
#include "../CvGameCoreDLL/CvDLLFAStarIFaceBase.h"
#include "../CvGameCoreDLL/CvGameCoreUtils.h"
#include "../CvGameCoreDLL/FAStarNode.h"


namespace AltAI
{
    namespace
    {
        void writePlotBuildData(FDataStreamBase* pStream, const std::map<IDInfo, std::vector<PlotBuildData> >& buildDataMap)
        {
            pStream->Write(buildDataMap.size());
            for (std::map<IDInfo, std::vector<PlotBuildData> >::const_iterator ci(buildDataMap.begin()), ciEnd(buildDataMap.end()); ci != ciEnd; ++ci)
            {
                ci->first.write(pStream);
                writeComplexVector(pStream, ci->second);
            }
        }

        void readPlotBuildData(FDataStreamBase* pStream, std::map<IDInfo, std::vector<PlotBuildData> >& buildDataMap)
        {
            buildDataMap.clear();
            size_t count;
            pStream->Read(&count);
            for (size_t i = 0; i < count; ++i)
            {
                IDInfo city;
                city.read(pStream);
                std::vector<PlotBuildData> buildData;
                readComplexVector(pStream, buildData);
                buildDataMap.insert(std::make_pair(city, buildData));
            }
        }

        struct MissionHelperData
        {
            MissionHelperData() : turnsToPlot(-1), missionTurns(-1) {}
            int turnsToPlot, missionTurns;
            IDInfo unit;

            void debug(std::ostream& os) const
            {
                os << "\n\t " << unit << " " << turnsToPlot << " turns to plot, " << missionTurns << " mission turns ";
            }
        };

        struct MissionHelperComp
        {
            bool operator() (const MissionHelperData& first, const MissionHelperData& second) const
            {
                return std::max<int>(0, first.missionTurns - first.turnsToPlot) > std::max<int>(0, second.missionTurns - second.turnsToPlot);
            }
        };
    }

    class WorkerAnalysisImpl
    {
        friend class WorkerMission;
    public:
        explicit WorkerAnalysisImpl(Player& player) : player_(player)
        {
        }

        void deleteUnit(CvUnit* pUnit)
        {
            updateActiveRouteMissions_(pUnit, true);
        }

        void updatePlotData()
        {
            const PlayerTypes playerType = player_.getPlayerID();
            const TeamTypes teamType = player_.getTeamID();
            const CvMap& theMap = gGlobals.getMap();

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();

            for (std::list<const CvPlot*>::const_iterator ci(lostPlots_.begin()), ciEnd(lostPlots_.end()); ci != ciEnd; ++ci)
            {
                if (ci == lostPlots_.begin()) os <<"\nLost: ";
                os << (*ci)->getCoords() << ", ";
            }

            for (std::list<const CvPlot*>::const_iterator ci(newPlots_.begin()), ciEnd(newPlots_.end()); ci != ciEnd; ++ci)
            {
                if (ci == newPlots_.begin()) os <<"\nGained: ";
                os << (*ci)->getCoords() << ", ";
            }

            for (std::list<const CvPlot*>::const_iterator ci(newBonusPlots_.begin()), ciEnd(newBonusPlots_.end()); ci != ciEnd; ++ci)
            {
                if (ci == newBonusPlots_.begin()) os <<"\nRevealed bonus: ";
                os << (*ci)->getCoords() << " = " << gGlobals.getBonusInfo((*ci)->getBonusType(teamType)).getType() << ", ";
            }

            for (std::list<IDInfo>::const_iterator ci(lostCities_.begin()), ciEnd(lostCities_.end()); ci != ciEnd; ++ci)
            {
                os << "\nLost city: " << ci->iID;
            }

            for (std::list<IDInfo>::const_iterator ci(newCities_.begin()), ciEnd(newCities_.end()); ci != ciEnd; ++ci)
            {
                os << "\nGained city: " << ci->iID;
            }
#endif
            for (std::list<const CvPlot*>::const_iterator ci(newPlots_.begin()), ciEnd(newPlots_.end()); ci != ciEnd; ++ci)
            {
                BonusTypes bonusType = (*ci)->getBonusType(teamType);
                if (bonusType != NO_BONUS)
                {
                    BonusPlotData data(bonusType, (*ci)->getImprovementType());
                    subAreaBonusMap_[(*ci)->getSubArea()].insert(std::make_pair((*ci)->getCoords(), data));
                }
            }

            for (std::list<const CvPlot*>::const_iterator ci(newBonusPlots_.begin()), ciEnd(newBonusPlots_.end()); ci != ciEnd; ++ci)
            {
                BonusTypes bonusType = (*ci)->getBonusType(teamType);
                if (bonusType != NO_BONUS)
                {
                    BonusPlotData data(bonusType, (*ci)->getImprovementType());
                    subAreaBonusMap_[(*ci)->getSubArea()].insert(std::make_pair((*ci)->getCoords(), data));
                }
            }

            for (std::list<IDInfo>::const_iterator ci(newCities_.begin()), ciEnd(newCities_.end()); ci != ciEnd; ++ci)
            {
                City& city = player_.getCity(ci->iID);

#ifdef ALTAI_DEBUG
                os << "\nGained city: " << ci->iID << ", initial imps: ";
                CityImprovementManagerPtr pCityImps = city.getCityImprovementManager();
                pCityImps->logImprovements(os);
#endif
                for (BonusMap::iterator subAreaIter(subAreaBonusMap_.begin()), subAreaEndIter(subAreaBonusMap_.end());
                    subAreaIter != subAreaEndIter; ++subAreaIter)
                {
                    for (SubAreaBonusMap::iterator bonusIter(subAreaIter->second.begin()), bonusEndIter(subAreaIter->second.end());
                        bonusIter != bonusEndIter; ++bonusIter)
                    {
                        const CvPlot* pPlot = theMap.plot(bonusIter->first.iX, bonusIter->first.iY);
                        if (pPlot->getOwner() == playerType && pPlot->isConnectedTo(city.getCvCity()))
                        {
#ifdef ALTAI_DEBUG
                            os << "\nPlot: " << bonusIter->first <<
                                  " with bonus: " << gGlobals.getBonusInfo(bonusIter->second.bonusType).getType() <<
                                  " connected to: " << narrow(city.getCvCity()->getName());
#endif
                            // could also do this from updatedCityBonuses_
                            bonusIter->second.connectedCities.insert(*ci);
                        }
                    }
                }
            }

            for (std::list<IDInfo>::const_iterator ci(lostCities_.begin()), ciEnd(lostCities_.end()); ci != ciEnd; ++ci)
            {
#ifdef ALTAI_DEBUG
                os << "\nErasing resource data for city: " << ci->iID;
#endif
                for (BonusMap::iterator subAreaIter(subAreaBonusMap_.begin()), subAreaEndIter(subAreaBonusMap_.end());
                    subAreaIter != subAreaEndIter; ++subAreaIter)
                {
                    for (SubAreaBonusMap::iterator bonusIter(subAreaIter->second.begin()), bonusEndIter(subAreaIter->second.end());
                        bonusIter != bonusEndIter; ++bonusIter)
                    {
                        bonusIter->second.connectedCities.erase(*ci);
                    }
                }

                cityImprovements_.erase(*ci);
            }            

            CityIter cityIter(*player_.getCvPlayer());
            while (CvCity* pCity = cityIter())
            {
                City& city = player_.getCity(pCity->getID());
                if (city.getFlags() & City::NeedsImprovementCalcs)
                {
                    city.calcImprovements();
                }

                cityImprovements_[pCity->getIDInfo()] = city.getCityImprovementManager()->getImprovements();
            }

            for (std::list<const CvPlot*>::const_iterator ci(lostPlots_.begin()), ciEnd(lostPlots_.end()); ci != ciEnd; ++ci)
            {
                BonusTypes bonusType = (*ci)->getBonusType(teamType);
                if (bonusType != NO_BONUS)
                {
                    subAreaBonusMap_[(*ci)->getSubArea()].erase((*ci)->getCoords());
                }

                // remove plot from city improvements maps
                NeighbourPlotIter plotIter(*ci, 2, 2);

                while (IterPlot pLoopPlot = plotIter())
                {
                    if (pLoopPlot.valid() && pLoopPlot->getOwner() == playerType)
                    {
                        const CvCity* pCity = pLoopPlot->getPlotCity();
                        if (pCity)
                        {
                            CityImprovementsMap::iterator cityImpIter = cityImprovements_.find(pCity->getIDInfo());
                            if (cityImpIter != cityImprovements_.end())
                            {
                                std::vector<PlotImprovementData>::iterator improvementIter = 
                                    std::remove_if(cityImpIter->second.begin(), cityImpIter->second.end(), ImprovementCoordsFinder(pLoopPlot->getCoords()));

                                cityImpIter->second.erase(improvementIter, cityImpIter->second.end());
                            }
                            else
                            {
#ifdef ALTAI_DEBUG
                                os << "\nFailed to find improvement entry for city: " << narrow(pCity->getName());
#endif
                            }
                        }
                    }
                }
            }

            for (std::list<UpdatedImprovementData>::const_iterator impIter(updatedImprovements_.begin()), impEndIter(updatedImprovements_.end());
                impIter != impEndIter; ++impIter)
            {
#ifdef ALTAI_DEBUG
                os << "\nUpdating improvement at: " << impIter->pPlot->getCoords()
                    << " was: " << (impIter->oldImp == NO_IMPROVEMENT ? " none" : gGlobals.getImprovementInfo(impIter->oldImp).getType())
                    << " is: " << (impIter->newImp == NO_IMPROVEMENT ? " none" : gGlobals.getImprovementInfo(impIter->newImp).getType());
#endif
                if (impIter->pPlot->getBonusType(teamType) != NO_BONUS)
                {
                    BonusMap::iterator subAreaIter = subAreaBonusMap_.find(impIter->pPlot->getSubArea());
                    if (subAreaIter != subAreaBonusMap_.end())
                    {
                        SubAreaBonusMap::iterator bonusIter = subAreaIter->second.find(impIter->pPlot->getCoords());
                        if (bonusIter != subAreaIter->second.end())
                        {
                            bonusIter->second.updateImprovement(impIter->newImp);
                            if (!bonusIter->second.improvementReqd)
                            {
#ifdef ALTAI_DEBUG
                                os << "\nImprovement: " << gGlobals.getImprovementInfo(impIter->newImp).getType() <<
                                    " for bonus: " << gGlobals.getBonusInfo(bonusIter->second.bonusType).getType() <<
                                    " built at: " << impIter->pPlot->getCoords();
#endif
                            }
                        }
                    }
                }
            }

            for (std::list<UpdatedCityBonuses>::const_iterator ci(updatedCityBonuses_.begin()), ciEnd(updatedCityBonuses_.end()); ci != ciEnd; ++ci)
            {
                // safer to use CvCity here as can check for null, city could have gone - todo: use updated cities to remove entries from this list?
                const CvCity* pCity = player_.getCvPlayer()->getCity(ci->city.iID);
                if (pCity)
                {
                    for (BonusMap::iterator subAreaIter(subAreaBonusMap_.begin()), subAreaEndIter(subAreaBonusMap_.end());
                        subAreaIter != subAreaEndIter; ++subAreaIter)
                    {
                        for (SubAreaBonusMap::iterator bonusIter(subAreaIter->second.begin()), bonusEndIter(subAreaIter->second.end());
                            bonusIter != bonusEndIter; ++bonusIter)
                        {
                            if (bonusIter->second.bonusType == ci->bonusType)
                            {
                                const CvPlot* pPlot = theMap.plot(bonusIter->first.iX, bonusIter->first.iY);
                                const bool isConnected = pPlot->getOwner() == playerType && pPlot->isConnectedTo(pCity);
                                if (ci->delta > 0)
                                {
                                    if (bonusIter->second.connectedCities.find(ci->city) == bonusIter->second.connectedCities.end() && isConnected)
                                    {
                                        bonusIter->second.connectedCities.insert(ci->city);
#ifdef ALTAI_DEBUG
                                        os << "\nPlot: " << bonusIter->first <<
                                              " with bonus: " << gGlobals.getBonusInfo(bonusIter->second.bonusType).getType() <<
                                              " connected to: " << narrow(pCity->getName());
#endif
                                    }
                                }
                                else
                                {
                                    if (bonusIter->second.connectedCities.find(ci->city) != bonusIter->second.connectedCities.end() && !isConnected)
                                    {
                                        bonusIter->second.connectedCities.erase(ci->city);
#ifdef ALTAI_DEBUG
                                        os << "\nPlot: " << bonusIter->first <<
                                              " with bonus: " << gGlobals.getBonusInfo(bonusIter->second.bonusType).getType() <<
                                              " disconnected from: " << narrow(pCity->getName());
#endif
                                    }
                                }
                            }
                        }
                    }
                }
            }

            lostPlots_.clear();
            newPlots_.clear();
            newCities_.clear();
            lostCities_.clear();
            updatedImprovements_.clear();
            updatedCityBonuses_.clear();
            newBonusPlots_.clear();
        }

        void updateBonusData()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            const CvMap& theMap = gGlobals.getMap();
            // todo - make dynamic upon city loss/addition and tech/tactic change
            unitBonusMap_.clear();
            player_.getAnalysis()->getPlayerTactics()->getUnitsWithDep<BonusTypes>(unitBonusMap_, CityBonusDependency::ID);

            bonusValueMap_.clear();

            for (BonusMap::iterator subAreaIter(subAreaBonusMap_.begin()), subAreaEndIter(subAreaBonusMap_.end());
                    subAreaIter != subAreaEndIter; ++subAreaIter)
            {
                for (SubAreaBonusMap::iterator bonusIter(subAreaIter->second.begin()), bonusEndIter(subAreaIter->second.end());
                    bonusIter != bonusEndIter; ++bonusIter)
                {
                    std::map<BonusTypes, std::vector<UnitTypes> >::const_iterator unitsIter = unitBonusMap_.find(bonusIter->second.bonusType);
                    if (unitsIter != unitBonusMap_.end())
                    {
                        for (size_t unitIndex = 0, unitCount = unitsIter->second.size(); unitIndex < unitCount; ++unitIndex)
                        {
                            bonusValueMap_[bonusIter->second.bonusType] += player_.getAnalysis()->getUnitAnalysis()->getCurrentUnitValue(unitsIter->second[unitIndex]);
                        }
                    }

                    int resourceCount = player_.getAnalysis()->getMapAnalysis()->getControlledResourceCount(bonusIter->second.bonusType);
                    boost::shared_ptr<ResourceInfo> pResourceInfo = player_.getAnalysis()->getResourceInfo(bonusIter->second.bonusType);
                    ResourceHappyInfo happyInfo = getResourceHappyInfo(pResourceInfo, ResourceQuery::AssumeAllCitiesHaveResource);
                    ResourceHealthInfo healthInfo = getResourceHealthInfo(pResourceInfo);

                    bonusValueMap_[bonusIter->second.bonusType] += (happyInfo.actualHappy + happyInfo.unusedHappy + happyInfo.potentialHappy);
                    bonusValueMap_[bonusIter->second.bonusType] += (healthInfo.actualHealth + healthInfo.unusedHealth + healthInfo.potentialHealth);

#ifdef ALTAI_DEBUG

                    /*os << "\nBonus value data for: " << gGlobals.getBonusInfo(bonusIter->second.bonusType).getType();
                    happyInfo.debug(os);
                    os << "\n";
                    healthInfo.debug(os);*/
#endif
                    /*if (resourceCount == 1)
                    {
                        bonusValueMap_[bonusIter->second.bonusType] *= 2;
                    }*/
                }
            }

            CityIter cityIter(*player_.getCvPlayer());
            while (CvCity* pCity = cityIter())
            {
                PlayerTactics::CityBuildingTacticsMap::const_iterator ci = player_.getAnalysis()->getPlayerTactics()->cityBuildingTacticsMap_.find(pCity->getIDInfo());
                if (ci != player_.getAnalysis()->getPlayerTactics()->cityBuildingTacticsMap_.end())
                {
                    for (PlayerTactics::CityBuildingTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                    {
                        const std::vector<IDependentTacticPtr>& pDepItems = li->second->getDependencies();
                        for (size_t i = 0, count = pDepItems.size(); i < count; ++i)
                        {
                            const std::vector<DependencyItem>& thisDepItems = pDepItems[i]->getDependencyItems();
                            for (size_t j = 0, depItemCount = thisDepItems.size(); j < depItemCount; ++j)
                            {
                                if (thisDepItems[j].first == CityBonusDependency::ID)
                                {
                                    bonusValueMap_[(BonusTypes)thisDepItems[j].second] += 10;
                                }
                            }
                        }
                    }
                }

                // todo - add values for production multipliers
                for (PlayerTactics::LimitedBuildingsTacticsMap::const_iterator iter(player_.getAnalysis()->getPlayerTactics()->globalBuildingsTacticsMap_.begin()),
                    endIter(player_.getAnalysis()->getPlayerTactics()->globalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
                {
                    ICityBuildingTacticsPtr pCityBuildingTactics = iter->second->getCityTactics(pCity->getIDInfo());
                    if (!pCityBuildingTactics) continue;

                    const std::vector<IDependentTacticPtr>& pDepItems = pCityBuildingTactics->getDependencies();
                    for (size_t i = 0, count = pDepItems.size(); i < count; ++i)
                    {
                        const std::vector<DependencyItem>& thisDepItems = pDepItems[i]->getDependencyItems();
                        for (size_t j = 0, depItemCount = thisDepItems.size(); j < depItemCount; ++j)
                        {
                            if (thisDepItems[j].first == CityBonusDependency::ID)
                            {
                                bonusValueMap_[(BonusTypes)thisDepItems[j].second] += 50;
                            }
                        }
                    }
                }

                for (PlayerTactics::LimitedBuildingsTacticsMap::const_iterator iter(player_.getAnalysis()->getPlayerTactics()->nationalBuildingsTacticsMap_.begin()),
                    endIter(player_.getAnalysis()->getPlayerTactics()->nationalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
                {
                    ICityBuildingTacticsPtr pCityBuildingTactics = iter->second->getCityTactics(pCity->getIDInfo());
                    if (!pCityBuildingTactics) continue;

                    const std::vector<IDependentTacticPtr>& pDepItems = pCityBuildingTactics->getDependencies();
                    for (size_t i = 0, count = pDepItems.size(); i < count; ++i)
                    {
                        const std::vector<DependencyItem>& thisDepItems = pDepItems[i]->getDependencyItems();
                        for (size_t j = 0, depItemCount = thisDepItems.size(); j < depItemCount; ++j)
                        {
                            if (thisDepItems[j].first == CityBonusDependency::ID)
                            {
                                bonusValueMap_[(BonusTypes)thisDepItems[j].second] += 25;
                            }
                        }
                    }
                }
            }            

            bonusesByValueMap_.clear();
            std::vector<int> accessibleSubAreas = player_.getAnalysis()->getMapAnalysis()->getAccessibleSubAreas(DOMAIN_LAND);

            for (size_t subAreaIndex = 0, subAreaCount = accessibleSubAreas.size(); subAreaIndex < subAreaCount; ++subAreaIndex)
            {
                const int subAreaId = accessibleSubAreas[subAreaIndex];
                BonusMap::const_iterator bonusIter = subAreaBonusMap_.find(subAreaId);
                if (bonusIter != subAreaBonusMap_.end())
                {
                    for (std::map<BonusTypes, int>::const_iterator bonusValueIter(bonusValueMap_.begin()), bonusValueEndIter(bonusValueMap_.end());
                        bonusValueIter != bonusValueEndIter; ++bonusValueIter)
                    {
                        // iterate over plots with bonuses in this sub area
                        for (SubAreaBonusMap::const_iterator bonusPlotIter = bonusIter->second.begin(), bonusPlotEndIter = bonusIter->second.end();
                            bonusPlotIter != bonusPlotEndIter; ++bonusPlotIter)
                        {
                            if (bonusPlotIter->second.bonusType == bonusValueIter->first)
                            {
                                if (bonusPlotIter->second.improvementReqd)
                                {
                                    bonusesByValueMap_.insert(std::make_pair(bonusValueIter->second, bonusPlotIter->first));
                                }
                                else
                                {
                                    const CvPlot* pPlot = theMap.plot(bonusPlotIter->first.iX, bonusPlotIter->first.iY);
                                    CityIterP<IsSubAreaP> cityIter(*player_.getCvPlayer(), IsSubAreaP(subAreaId));
                                    bool isConnected = false;
                                    while (CvCity* pLoopCity = cityIter())
                                    {
                                        if (pPlot->isConnectedTo(pLoopCity))
                                        {
                                            isConnected = true;
                                            break;
                                        }
                                    }
                                    if (!isConnected)
                                    {
                                        bonusesByValueMap_.insert(std::make_pair(bonusValueIter->second, bonusPlotIter->first));
                                    }
                                }
                            }
                        }
                    }
                }
            }

#ifdef ALTAI_DEBUG
            os << "\nBonus value map: ";
            for (std::map<BonusTypes, int>::const_iterator ci(bonusValueMap_.begin()), ciEnd(bonusValueMap_.end()); ci != ciEnd; ++ci)
            {
                os << "\n\t" << gGlobals.getBonusInfo(ci->first).getType() << " = " << ci->second;
            }

            os << "\nUnit bonus map: ";
            for (std::map<BonusTypes, std::vector<UnitTypes> >::const_iterator ci = unitBonusMap_.begin(), ciEnd = unitBonusMap_.end(); ci != ciEnd; ++ci)
            {
                os << "\n\t" << gGlobals.getBonusInfo(ci->first).getType() << ": ";
                for (size_t unitIndex = 0, unitCount = ci->second.size(); unitIndex < unitCount; ++unitIndex)
                {
                    os << gGlobals.getUnitInfo(ci->second[unitIndex]).getType() << " ";
                }
            }

            os << "\nBonus plot value map: ";
            for (std::multimap<int, XYCoords>::const_iterator ci(bonusesByValueMap_.begin()), ciEnd(bonusesByValueMap_.end()); ci != ciEnd; ++ci)
            {
                const CvPlot* pPlot = theMap.plot(ci->second.iX, ci->second.iY);
                os << "\n\tValue = " << ci->first << " plot = " << ci->second
                   << " bonus = " << gGlobals.getBonusInfo(pPlot->getBonusType(player_.getTeamID())).getType()
                   << " improvement = " << (pPlot->getImprovementType() == NO_IMPROVEMENT ? " none" : gGlobals.getImprovementInfo(pPlot->getImprovementType()).getType());
            }
#endif
        }

        bool pushRouteMission(CvUnitAI* pUnit, const CvPlot* pStartPlot, const CvPlot* pEndPlot, RouteTypes routeType, bool moveToStart)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            if (player_.getCvPlayer()->AI_getPlotDanger((CvPlot *)pEndPlot) > 0)
            {
                return false;
            }

            BuildTypes routeBuildType = GameDataAnalysis::getBuildTypeForRouteType(routeType);
            if (routeBuildType == NO_BUILD)
            {
                return false;
            }

            const CvCity* pTargetCity = pEndPlot->getPlotCity();

            if (pStartPlot == pEndPlot)
            {
                if (pStartPlot->getRouteType() == routeType)
                {
                    return false;
                }
                else
                {
                    if (moveToStart)
                    {
                        player_.pushWorkerMission(pUnit, pTargetCity, pStartPlot, MISSION_MOVE_TO, NO_BUILD, MOVE_IGNORE_DANGER, "WorkerAnalysisImpl::pushRouteMission");
                    }
                    player_.pushWorkerMission(pUnit, pTargetCity, pStartPlot, MISSION_BUILD, routeBuildType, MOVE_IGNORE_DANGER, "WorkerAnalysisImpl::pushRouteMission");
                    return true;
                }
            }

            CvMap& theMap = gGlobals.getMap();

            FAStar* pRouteStepFinder = gDLL->getFAStarIFace()->create();
            const int stepFinderInfo = MAKEWORD((short)player_.getPlayerID(), 
                (short)(SubAreaStepFlags::Team_Territory | SubAreaStepFlags::Unowned_Territory | SubAreaStepFlags::Friendly_Territory));

            RouteStepFinderData routeData(routeType, routeBuildType, player_.getPlayerID(), CvTeamAI::getTeam(player_.getTeamID()).isBridgeBuilding());
            gDLL->getFAStarIFace()->Initialize(pRouteStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), subAreaStepDestValid, stepHeuristic, routeStepCost, areaStepValidWithFlags, stepAdd, NULL, &routeData);

            std::list<CvPlot*> pTargetPlots;

            if (gDLL->getFAStarIFace()->GeneratePath(pRouteStepFinder, pStartPlot->getX(), pStartPlot->getY(), pEndPlot->getX(), pEndPlot->getY(), false, stepFinderInfo))
            {
                FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pRouteStepFinder);
                while (pNode)
                {
                    CvPlot* pPlot = gGlobals.getMap().plot(pNode->m_iX, pNode->m_iY);
                    if (pPlot->getRouteType() != routeType)
                    {
                        pTargetPlots.push_front(pPlot);
#ifdef ALTAI_DEBUG
                        os << "\n(pushRouteMission) - Adding plot: " << pPlot->getCoords() << " for route from: " << pStartPlot->getCoords() << " to: " << pEndPlot->getCoords();
#endif
                    }
                    else
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(pushRouteMission) - Skipping plot: " << pPlot->getCoords() << " for route from: " << pStartPlot->getCoords() << " to: " << pEndPlot->getCoords();
#endif
                    }
                    pNode = pNode->m_pParent;
                }
            }

            gDLL->getFAStarIFace()->destroy(pRouteStepFinder);
            
            if (pTargetPlots.empty())
            {
                return false;
            }

            if (moveToStart || !pUnit->atPlot(*pTargetPlots.begin()))
            {
                player_.pushWorkerMission(pUnit, pTargetCity, *pTargetPlots.begin(), MISSION_MOVE_TO, NO_BUILD, MOVE_IGNORE_DANGER, "WorkerAnalysisImpl::pushRouteMission");
            }

            for (std::list<CvPlot*>::const_iterator plotIter(pTargetPlots.begin());;)
            {
                player_.pushWorkerMission(pUnit, pTargetCity, *plotIter, MISSION_BUILD, routeBuildType, MOVE_IGNORE_DANGER, "WorkerAnalysisImpl::pushRouteMission");
                plotIter++;
                if (plotIter == pTargetPlots.end())
                {
                    break;
                }
                player_.pushWorkerMission(pUnit, pTargetCity, *plotIter, MISSION_MOVE_TO, NO_BUILD, MOVE_IGNORE_DANGER, "WorkerAnalysisImpl::pushRouteMission");
            }

            activeRouteMissions_.push_back(ActiveRouteMission(player_.getOurUnit(pUnit), pStartPlot, pEndPlot));

            return true;
        }

        bool pushMission(CvUnitAI* pUnit, const CvCity* pCity, const PlotBuildData& buildData, bool onlyAddFirst = false)
        {
            const CvPlot* pPlot = gGlobals.getMap().plot(buildData.coords.iX, buildData.coords.iY);

            if (player_.getCvPlayer()->AI_getPlotDanger((CvPlot *)pPlot) > 0)
            {
                return false;
            }

            CvSelectionGroup* pGroup = pUnit->getGroup();
            bool unitAtPlot = pUnit->atPlot(pPlot); // todo - have flag for roading to the plot?
            std::vector<BuildTypes> builds;
            if (buildData.removedFeature != NO_FEATURE)
            {
                BuildTypes buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(buildData.removedFeature);
                if (pPlot->canBuild(buildType, pUnit->getOwner()))
                {
                    builds.push_back(buildType);
                }
            }
            else if (GameDataAnalysis::isBadFeature(pPlot->getFeatureType()))
            {
                BuildTypes featureRemoveBuildType = GameDataAnalysis::getBuildTypeToRemoveFeature(pPlot->getFeatureType());
                if (featureRemoveBuildType != NO_BUILD)
                {
                    if (pPlot->canBuild(featureRemoveBuildType, pUnit->getOwner()))
                    {
                        builds.push_back(featureRemoveBuildType);
                    }
                }
            }

            if (buildData.improvement != NO_IMPROVEMENT)
            {
                BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(buildData.improvement);
                if (pPlot->canBuild(buildType, pUnit->getOwner()))
                {
                    builds.push_back(buildType);
                }
            }

            bool haveMissions = !builds.empty();
            if (!unitAtPlot && haveMissions)
            {
                player_.pushWorkerMission(pUnit, pCity, pPlot, MISSION_MOVE_TO, NO_BUILD, MOVE_IGNORE_DANGER, "WorkerAnalysisImpl::pushMission");
                if (onlyAddFirst)
                {
                    return true;
                }
            }

            for (size_t i = 0, buildCount = builds.size(); i < buildCount; ++i)
            {
                player_.pushWorkerMission(pUnit, pCity, pPlot, MISSION_BUILD, builds[i], 0, "WorkerAnalysisImpl::pushMission");
                if (onlyAddFirst)
                {
                    return true;
                }
            }

            // don't apply onlyAddFirst check to route building for now - as only used by sea workers
            if (buildData.routeType != NO_ROUTE)
            {
                if (pushRouteMission(pUnit, pPlot, pCity->plot(), buildData.routeType, !unitAtPlot && !haveMissions))
                {
                    haveMissions = true;
                }
            }
            
            return haveMissions;
        }

        const CvCity* selectNextCity(const CvCity* pCurrentCity, const std::map<int, IDInfo>& areaCityDistances, 
            const std::map<IDInfo, std::vector<PlotBuildData> >& cityMissions, const std::map<IDInfo, int>& cityWorkerCounts)
        {
            const CvCity* pNextCity = NULL;

            // todo: loop with higher threshold worker counts if neither condition is met
            for (std::map<int, IDInfo>::const_iterator nextCityIter(areaCityDistances.begin()), nextCityEndIter(areaCityDistances.end());
                nextCityIter != nextCityEndIter; ++nextCityIter)
            {
                const CvCity* pCity = player_.getCvPlayer()->getCity(nextCityIter->second.iID);
                std::map<IDInfo, std::vector<PlotBuildData> >::const_iterator missionCountIter = cityMissions.find(nextCityIter->second);
                std::map<IDInfo, int>::const_iterator cityWorkerCountsIter = cityWorkerCounts.find(nextCityIter->second);

                // current city should be first city we encounter as it's nearest!
                if (pCurrentCity && nextCityIter->second == pCurrentCity->getIDInfo() && missionCountIter != cityMissions.end())
                {
                    for (size_t missionIndex = 0, missionCount = missionCountIter->second.size(); missionIndex < missionCount; ++missionIndex)
                    {
                        if (player_.getNumWorkersTargetingPlot(missionCountIter->second[missionIndex].coords) == 0)
                        {
                            break;
                        }
                    }
                    //if (!missionCountIter->second.empty() && cityWorkerCountsIter->second <= std::max<int>(1, std::min<int>(4, missionCountIter->second.size())))
                    //{
                    //    break;  // still stuff to do
                    //}
                }

                //if (missionCountIter != cityMissions.end() && cityWorkerCountsIter != cityWorkerCounts.end())
                if (missionCountIter != cityMissions.end())
                {
                    for (size_t missionIndex = 0, missionCount = missionCountIter->second.size(); missionIndex < missionCount; ++missionIndex)
                    {
                        if (player_.getNumWorkersTargetingPlot(missionCountIter->second[missionIndex].coords) == 0)
                        {
                            pNextCity = pCity;
                            break;
                        }
                    }
                    //if (!missionCountIter->second.empty() && cityWorkerCountsIter->second <= std::max<int>(1, std::min<int>(3, missionCountIter->second.size())))
                    //{
                    //    pNextCity = pCity;
                    //    break;
                    //}
                }
            }

            return pNextCity;
        }

        // returns true if route required (and possible)
        bool checkCityRoute(CvUnitAI* pUnit, const CvCity* pFromCity, const CvCity* pToCity, RouteTypes routeType)
        {
            if (pToCity->getIDInfo() == pFromCity->getIDInfo() || pToCity->plot()->getSubArea() != pUnit->plot()->getSubArea())
            {
                return false;
            }

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\n(checkCityRoute) route from: " << narrow(pFromCity->getName()) << " to: " << narrow(pToCity->getName());
#endif
            if (haveActiveRouteMissions_(pFromCity->plot(), pToCity->plot()))
            {
#ifdef ALTAI_DEBUG
                os << " found active missions...";
#endif
                return false;
            }

            CvMap& theMap = gGlobals.getMap();

            FAStar* pRouteStepFinder = gDLL->getFAStarIFace()->create();
            const int stepFinderInfo = MAKEWORD((short)player_.getPlayerID(), 
                (short)(SubAreaStepFlags::Team_Territory | SubAreaStepFlags::Unowned_Territory | SubAreaStepFlags::Friendly_Territory));
            BuildTypes routeBuildType = GameDataAnalysis::getBuildTypeForRouteType(routeType);
            if (routeBuildType == NO_BUILD)
            {
                return false;
            }
            RouteStepFinderData routeData(routeType, routeBuildType, player_.getPlayerID(), CvTeamAI::getTeam(player_.getTeamID()).isBridgeBuilding());
            gDLL->getFAStarIFace()->Initialize(pRouteStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), subAreaStepDestValid, stepHeuristic, routeStepCost, areaStepValidWithFlags, stepAdd, NULL, &routeData);

            bool foundPath = false, needRoute = false;

            if (gDLL->getFAStarIFace()->GeneratePath(pRouteStepFinder, pFromCity->getX(), pFromCity->getY(), pToCity->getX(), pToCity->getY(), false, stepFinderInfo))
            {
                foundPath = true;
#ifdef ALTAI_DEBUG
                os << "\nRoute finder found path from: " << XYCoords(pFromCity->getX(), pFromCity->getY()) << " to: " << XYCoords(pToCity->getX(), pToCity->getY());
#endif
                FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pRouteStepFinder);
                while (pNode)
                {
                    if (theMap.plot(pNode->m_iX, pNode->m_iY)->getRouteType() == NO_ROUTE)
                    {
                        needRoute = true;
#ifdef ALTAI_DEBUG
                        os << "\nRoute finder found route needed from: " << XYCoords(pFromCity->getX(), pFromCity->getY()) << " to: " << XYCoords(pToCity->getX(), pToCity->getY())
                            << " at: " << theMap.plot(pNode->m_iX, pNode->m_iY)->getCoords();
#endif
                        break;
                    }
//#ifdef ALTAI_DEBUG
//                    os << " (" << pNode->m_iX << ", " << pNode->m_iY << ")" << " c = " << pNode->m_iKnownCost << ", " << pNode->m_iTotalCost << ", " << pNode->m_iHeuristicCost
//                        << " r = " << (theMap.plot(pNode->m_iX, pNode->m_iY)->getRouteType() != NO_ROUTE);
//#endif
                    pNode = pNode->m_pParent;
                }
            }
            gDLL->getFAStarIFace()->destroy(pRouteStepFinder);

            return foundPath ? needRoute : false;
        }

        std::pair<int, int> getCompletedMissionCount(CvUnitAI* pUnit) const
        {
            Unit unit = player_.getOurUnit(pUnit);
            const std::vector<Unit::WorkerMission>& missions = unit.getWorkerMissions();
            size_t completedMissionCount = 0;
            for (size_t i = 0, count = missions.size(); i < count; ++i)
            {
                if (missions[i].isComplete())
                {
                    ++completedMissionCount;
                }
                else
                {
                    break;
                }
            }

            return std::make_pair(completedMissionCount, (int)missions.size());
        }

        CvPlot* getCurrentTarget(IDInfo unit) const
        {
            std::vector<Unit::WorkerMission> missions = player_.getOurUnit(unit).getWorkerMissions();
            CvPlot* pTarget = NULL;
            if (!missions.empty())
            {
                pTarget = missions[0].getTarget();
            }
            return pTarget;
        }

        bool tryMission(CvUnitAI* pUnit, const CvCity* pCurrentCity, const std::map<IDInfo, std::vector<PlotBuildData> >& buildsMap, const PlotSet& dangerPlots)
        {
            std::map<IDInfo, std::vector<PlotBuildData> >::const_iterator buildTargetsIter = buildsMap.find(pCurrentCity->getIDInfo());
            if (buildTargetsIter != buildsMap.end() && !buildTargetsIter->second.empty())
            {
                for (size_t i = 0, targetsCount = buildTargetsIter->second.size(); i < targetsCount; ++i)
                {
                    const CvPlot* targetPlot = gGlobals.getMap().plot(buildTargetsIter->second[i].coords.iX, buildTargetsIter->second[i].coords.iY);
                    if (dangerPlots.find(targetPlot) != dangerPlots.end())
                    {
#ifdef ALTAI_DEBUG
                        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
                        os << "\nSkipping potential worker mission plot: " << targetPlot->getCoords() << " - plot danger";
#endif
                        continue;
                    }

                    if (player_.getNumWorkersTargetingPlot(buildTargetsIter->second[i].coords) == 0)
                    {
                        if (pushMission(pUnit, pCurrentCity, buildTargetsIter->second[i]))
                        {
                            unitCityTargets_[pUnit->getIDInfo()] = pCurrentCity->getIDInfo();
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        bool tryMission(CvUnitAI* pUnit, const CvCity*& pCurrentCity, 
                        const std::map<int, IDInfo>& areaCityDistances, 
                        const std::map<IDInfo, std::vector<PlotBuildData> >& buildsMap,
                        const std::map<IDInfo, int>& cityWorkerCounts,
                        RouteTypes routeType,
                        const PlotSet& dangerPlots)
        {
            const CvCity* pNextCity = selectNextCity(pCurrentCity, areaCityDistances, buildsMap, cityWorkerCounts);

            if (pCurrentCity && pNextCity)
            {
                if (routeType != NO_ROUTE)
                {
                    if (checkCityRoute(pUnit, pCurrentCity, pNextCity, routeType))
                    {
                        if (pushRouteMission(pUnit, pCurrentCity->plot(), pNextCity->plot(), routeType, !pUnit->atPlot(pCurrentCity->plot())))
                        {
                            unitCityTargets_[pUnit->getIDInfo()] = pNextCity->getIDInfo();
                            return true;
                        }
                    }
                }
            }

            if (pNextCity)
            {
                pCurrentCity = pNextCity;
            }

            if (pCurrentCity && tryMission(pUnit, pCurrentCity, buildsMap, dangerPlots))
            {
                unitCityTargets_[pUnit->getIDInfo()] = pCurrentCity->getIDInfo();
                return true;
            }

            return false;
        }

        int executeWorkerMission(CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " Executing worker missions for unit: " << pUnit->getIDInfo();
#endif
            Unit& unit = player_.getOurUnit(pUnit);

            int missionFlag = Unit::None;
            CvSelectionGroup* pGroup = pUnit->getGroup();
                   
            for (; !unit.getWorkerMissions().empty() && pGroup->getLengthMissionQueue() == 0 && pGroup->getActivityType() != ACTIVITY_HOLD;)
            {
                if (pUnit->movesLeft() == 0)
                {
                    // no point in pushing missions if we have no moves left
                    break;
                }

                Unit::WorkerMission& workerMission = *unit.getWorkerMissions().begin();

                if (!workerMission.canStartMission())
                {
                    missionFlag = workerMission.missionFlags |= Unit::CantStart;
                    break;
                }

                if (workerMission.missionType != MISSION_SKIP && workerMission.isComplete())
                {
                    missionFlag = workerMission.missionFlags = Unit::Ok;
                    continue;
                }

                const CvPlot* pTargetPlot = workerMission.getTarget();
                if (workerMission.missionType == MISSION_MOVE_TO)
                {
                    const CvPlot* pLastMoveToPlot = NULL;
                    while (pUnit->plot() != pTargetPlot)
                    {
                        if (pUnit->movesLeft() == 0)  // we've used all our moves up
                        {
                            break;
                        }

                        const CvPlot* pMoveToPlot = getNextMovePlot(player_, pUnit->getGroup(), pTargetPlot);
                        // keep submitting missions until unit runs out of moves or we get stuck
                        if (pMoveToPlot && pMoveToPlot != pUnit->plot() && pMoveToPlot != pLastMoveToPlot)
                        {
                            pGroup->pushMission(MISSION_MOVE_TO, pMoveToPlot->getX(), pMoveToPlot->getY(), workerMission.iFlags, false, false, MISSIONAI_BUILD, (CvPlot *)pMoveToPlot, 0, __FUNCTION__);
                        }
                        else
                        {
                            workerMission.missionFlags |= Unit::CantStart;
                            //pGroup->pushMission(MISSION_SKIP, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                            return workerMission.missionFlags;
                        }

                        if (pGroup->getLengthMissionQueue() > 0)
                        {
                            break;
                        }
                        pLastMoveToPlot = pMoveToPlot;
                    }
                }
                else if (workerMission.missionType == MISSION_ROUTE_TO)
                {
                    pGroup->pushMission(MISSION_ROUTE_TO, pTargetPlot->getX(), pTargetPlot->getY(), workerMission.iFlags, false, false, MISSIONAI_BUILD, (CvPlot *)pTargetPlot, 0, __FUNCTION__);
                }
                else if (workerMission.missionType == MISSION_BUILD)
                {
                    pGroup->pushMission(MISSION_BUILD, workerMission.buildType, -1, workerMission.iFlags, false, false, MISSIONAI_BUILD, (CvPlot *)pTargetPlot, 0, __FUNCTION__);
                }
                else if (workerMission.missionType == MISSION_SKIP)
                {
                    pGroup->pushMission(MISSION_SKIP, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                }
            }

            return missionFlag;
        }

        void updatePossibleMissionsData()
        {
            bonusTargetPlots_.clear(), cityBonusTargetPlots_.clear(), cityWaterBonusTargetPlots_.clear(),
                nonCityBonusTargetPlots_.clear(), unconnectedBonusTargetPlots_.clear(), unbuiltNonBonusImps_.clear(), 
                unirrigatedIrrigatableBonuses_.clear(), badFeatureImps_.clear(),
                unbuiltSelectedImps_.clear(), unselectedImps_.clear(), irrigationChainImps_.clear();

            RouteTypes routeType = player_.getCvPlayer()->getBestRoute();
            const bool canChainIrrigation = CvTeamAI::getTeam(player_.getTeamID()).isIrrigation();
            const CvMap& theMap = gGlobals.getMap();

            for (CityImprovementsMap::const_iterator cityIter(cityImprovements_.begin()), cityEndIter(cityImprovements_.end()); cityIter != cityEndIter; ++cityIter)
            {
                const City& city = player_.getCity(cityIter->first.iID);

                std::vector<PlotCondPtr > conditions;
                conditions.push_back(PlotCondPtr(new IsLand()));
                conditions.push_back(PlotCondPtr(new IsSubArea(city.getCvCity()->plot()->getSubArea())));  // todo - handle cities with mulitple sub areas
                conditions.push_back(PlotCondPtr(new HasBonus(player_.getTeamID())));

                boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, int> bestImprovement = getNextImprovement_(city, conditions);

                if (boost::get<0>(bestImprovement) != XYCoords(-1, -1) && boost::get<2>(bestImprovement) == getBonusImprovementType(gGlobals.getMap().plot(boost::get<0>(bestImprovement))->getBonusType(player_.getTeamID())))
                {
                    PlotBuildData buildData(boost::get<0>(bestImprovement), boost::get<2>(bestImprovement), boost::get<1>(bestImprovement), routeType);
                    cityBonusTargetPlots_[cityIter->first].push_back(buildData);
                }

                std::vector<PlotCondPtr> seaConditions;
                seaConditions.push_back(PlotCondPtr(new IsWater()));
                seaConditions.push_back(PlotCondPtr(new HasBonus(player_.getTeamID())));

                bestImprovement = getNextImprovement_(city, seaConditions);

                if (boost::get<0>(bestImprovement) != XYCoords(-1, -1))
                {
                    PlotBuildData buildData(boost::get<0>(bestImprovement), boost::get<2>(bestImprovement), boost::get<1>(bestImprovement), NO_ROUTE);
                    cityWaterBonusTargetPlots_[cityIter->first].push_back(buildData);
                }

                if (routeType != NO_ROUTE)
                {
                    for (size_t plotImpIndex = 0, plotImpCount = cityIter->second.size(); plotImpIndex < plotImpCount; ++plotImpIndex)
                    {
                        if ((cityIter->second[plotImpIndex].state == PlotImprovementData::Built) && (cityIter->second[plotImpIndex].flags & PlotImprovementData::NeedsRoute))
                        {
                            if (player_.getNumWorkersTargetingPlot(cityIter->second[plotImpIndex].coords) == 0)
                            {
                                PlotBuildData buildData(cityIter->second[plotImpIndex].coords, NO_IMPROVEMENT, NO_FEATURE, routeType);
                                unconnectedBonusTargetPlots_[cityIter->first].push_back(buildData);
                            }
                        }
                    }
                }

                std::vector<PlotCondPtr > nonBonusConditions;
                nonBonusConditions.push_back(PlotCondPtr(new IsLand()));
                nonBonusConditions.push_back(PlotCondPtr(new IsSubArea(city.getCvCity()->plot()->getSubArea())));
                nonBonusConditions.push_back(PlotCondPtr(new NoBonus(player_.getTeamID())));

                for (;;)
                {                    
                    const int numImpNotBuilt = city.getCityImprovementManager()->getNumImprovementsNotBuilt();
                    const int numImpBuilt = city.getCityImprovementManager()->getNumImprovementsBuilt();
                    const int cityPop = city.getCvCity()->getPopulation();

                    bestImprovement = city.getCityImprovementManager()->getBestImprovementNotBuilt(false, false, nonBonusConditions);
                    
                    if (boost::get<2>(bestImprovement) == NO_IMPROVEMENT)
                    {
                        break;
                    }
                    XYCoords coords = boost::get<0>(bestImprovement);
                    const CvPlot* pThisPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                    nonBonusConditions.push_back(PlotCondPtr(new IgnorePlot(pThisPlot)));

                    if (boost::get<3>(bestImprovement) & PlotImprovementData::NeedsIrrigation || player_.getNumWorkersTargetingPlot(coords) > 0)
                    {                        
                        continue;
                    }
                    else
                    {
                        PlotBuildData buildData(pThisPlot->getCoords(), boost::get<2>(bestImprovement), boost::get<1>(bestImprovement), NO_ROUTE);
                        unbuiltNonBonusImps_[cityIter->first].push_back(buildData);
                    }
                }

                for (size_t plotImpIndex = 0, plotImpCount = cityIter->second.size(); plotImpIndex < plotImpCount; ++plotImpIndex)
                {
                    if (gGlobals.getMap().plot(cityIter->second[plotImpIndex].coords.iX, cityIter->second[plotImpIndex].coords.iY)->getSubArea() == city.getCvCity()->plot()->getSubArea() &&  // todo - better subarea handling
                        !(cityIter->second[plotImpIndex].flags & PlotImprovementData::NeedsIrrigation))  // skip plots where we need irrigation chain - handled further down
                    {
                        if (cityIter->second[plotImpIndex].isSelectedAndNotBuilt())
                        {
                            PlotBuildData buildData(cityIter->second[plotImpIndex].coords, cityIter->second[plotImpIndex].improvement, cityIter->second[plotImpIndex].removedFeature, NO_ROUTE);
                            unbuiltSelectedImps_[cityIter->first].push_back(buildData);
                        }
                        else if (cityIter->second[plotImpIndex].state == PlotImprovementData::Not_Selected || cityIter->second[plotImpIndex].state == PlotImprovementData::Not_Built)
                        {
                            // should be in rank order
                            PlotBuildData buildData(cityIter->second[plotImpIndex].coords, cityIter->second[plotImpIndex].improvement, cityIter->second[plotImpIndex].removedFeature, NO_ROUTE);
                            unselectedImps_[cityIter->first].push_back(buildData);
                        }
                    }                    
                }

                for (size_t plotImpIndex = 0, plotImpCount = cityIter->second.size(); plotImpIndex < plotImpCount; ++plotImpIndex)
                {
                    if (cityIter->second[plotImpIndex].state == PlotImprovementData::Built && cityIter->second[plotImpIndex].flags & PlotImprovementData::RemoveFeature)
                    {
                        if (player_.getNumWorkersTargetingPlot(cityIter->second[plotImpIndex].coords) == 0)
                        {
                            BuildTypes buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(cityIter->second[plotImpIndex].removedFeature);
                            const CvPlot* pThisPlot = gGlobals.getMap().plot(cityIter->second[plotImpIndex].coords.iX, cityIter->second[plotImpIndex].coords.iY);

                            if (player_.getCvPlayer()->canBuild(pThisPlot, buildType))
                            {
                                PlotBuildData buildData(cityIter->second[plotImpIndex].coords, NO_IMPROVEMENT, cityIter->second[plotImpIndex].removedFeature, NO_ROUTE);
                                badFeatureImps_[cityIter->first].push_back(buildData);
                            }
                        }
                    }

                    if (canChainIrrigation)
                    {
                        if (cityIter->second[plotImpIndex].flags & PlotImprovementData::NeedsIrrigation)
                        {
                            if (cityIter->second[plotImpIndex].state == PlotImprovementData::Built || cityIter->second[plotImpIndex].isSelectedAndNotBuilt())
                            {
                                const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(cityIter->second[plotImpIndex].improvement);
                                XYCoords buildCoords = city.getCityImprovementManager()->getIrrigationChainPlot(cityIter->second[plotImpIndex].coords);
                                if (buildCoords != XYCoords(-1, -1))
                                {
                                    CvPlot* pBuildPlot = gGlobals.getMap().plot(buildCoords.iX, buildCoords.iY);
                                    if (player_.getCvPlayer()->canBuild(pBuildPlot, GameDataAnalysis::getBuildTypeForImprovementType(cityIter->second[plotImpIndex].improvement)))
                                    {
                                        PlotBuildData buildData(buildCoords, cityIter->second[plotImpIndex].improvement, NO_FEATURE, NO_ROUTE);
                                        if (cityIter->second[plotImpIndex].state == PlotImprovementData::Built)
                                        {
                                            unirrigatedIrrigatableBonuses_[cityIter->first].push_back(buildData);
                                        }
                                        else
                                        {
                                            irrigationChainImps_[cityIter->first].push_back(buildData);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        
            // loop to handle bonus imps that are not on any city list from above
            for (std::multimap<int, XYCoords>::const_reverse_iterator bonusValueIter(bonusesByValueMap_.rbegin()), bonusValueEndIter(bonusesByValueMap_.rend());
                bonusValueIter != bonusValueEndIter; ++bonusValueIter)
            {
                if (player_.getNumWorkersTargetingPlot(bonusValueIter->second) == 0)
                {
                    const CvPlot* pPlot = theMap.plot(bonusValueIter->second.iX, bonusValueIter->second.iY);
                    IDInfo closestCity;
                    // may have no cities in the bonus's sub area  -- maybe make this flag up that we need overseas settler?
                    // this fn should work for water resources and find the closest coastal city
                    player_.getAnalysis()->getMapAnalysis()->getClosestCity(pPlot, pPlot->getSubArea(), false, closestCity);

                    BonusTypes bonusType = pPlot->getBonusType(player_.getTeamID());
                    ImprovementTypes requiredImprovement = getBonusImprovementType(bonusType);
                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(requiredImprovement);

                    // can build - or already have improvement - in which case we assume it's disconnected
                    if (!player_.getCvPlayer()->canBuild(pPlot, buildType) && pPlot->getImprovementType() != requiredImprovement)
                    {
                        continue;
                    }

                    FeatureTypes plotFeatureType = pPlot->getFeatureType();
                    FeatureTypes removedFeatureType = NO_FEATURE;
                    if (GameDataAnalysis::doesBuildTypeRemoveFeature(buildType, plotFeatureType))
                    {
                        removedFeatureType = plotFeatureType;
                    }
                    
                    if (closestCity != IDInfo() && cityBonusTargetPlots_.find(closestCity) == cityBonusTargetPlots_.end())
                    {
                        PlotBuildData buildData(bonusValueIter->second, requiredImprovement, removedFeatureType, routeType);
                        bonusTargetPlots_[closestCity].push_back(buildData);
                    }
                }
            }
        }

        void updateWorkerMission(CvUnitAI* pUnit)
        {
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner());
            if (!pPlayer->hasMission(pUnit))
            {
                UnitMissionPtr pUnitMission(new WorkerMission(pUnit, this));

                unitMissions_.insert(std::make_pair(pUnit->getIDInfo(), pUnitMission));
                if (pUnitMission->getTargetCity())
                {
                    unitCityTargets_[pUnit->getIDInfo()] = pUnitMission->getTargetCity()->getIDInfo();
                }

                pPlayer->pushMission(pUnit, pUnitMission);
            }

            DomainTypes domainType = pUnit->getDomainType();
            if (domainType == DOMAIN_LAND)
            {
                return updateLandWorkerMission_(pUnit);
            }
            else if (domainType == DOMAIN_SEA)
            {
                return updateSeaWorkerMission_(pUnit);
            }            
        }

        const CvCity* getTargetCity(const IDInfo& unit) const
        {
            std::map<IDInfo, IDInfo>::const_iterator unitCityTargetIter = unitCityTargets_.find(unit);
            if (unitCityTargetIter != unitCityTargets_.end())
            {
                return player_.getCvPlayer()->getCity(unitCityTargetIter->second.iID);
            }
            return (const CvCity*)0;
        }

        void updatePlotOwner(const CvPlot* pPlot, PlayerTypes previousRevealedOwner, PlayerTypes newRevealedOwner)
        {
//#ifdef ALTAI_DEBUG
//            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//#endif
            // can lose plots which are briefly gained when conquering a city, but then are lost until the city comes out of post conquest revolt
            if (previousRevealedOwner == player_.getPlayerID() && newRevealedOwner != previousRevealedOwner)
            {
//#ifdef ALTAI_DEBUG
//                os << "\nlost plot: " << pPlot->getCoords() << " prev owner: " << previousRevealedOwner << " new owner: " << newRevealedOwner;
//#endif
                lostPlots_.push_back(pPlot);

                // if we have this plot as a new plot, remove it
                std::list<const CvPlot*>::iterator newPlotsIter = std::find(newPlots_.begin(), newPlots_.end(), pPlot);
                if (newPlotsIter != newPlots_.end())
                {
                    newPlots_.erase(newPlotsIter);
                }
            }
            else if (newRevealedOwner == player_.getPlayerID())
            {
//#ifdef ALTAI_DEBUG
//                os << "\ngained plot: " << pPlot->getCoords() << " prev owner: " << previousRevealedOwner << " new owner: " << newRevealedOwner;;
//#endif
                // if we have this plot as a lost plot, remove it
                std::list<const CvPlot*>::iterator lostPlotsIter = std::find(lostPlots_.begin(), lostPlots_.end(), pPlot);
                if (lostPlotsIter != lostPlots_.end())
                {
                    lostPlots_.erase(lostPlotsIter);
                }

                newPlots_.push_back(pPlot);
            }
        }

        bool updatePlotBonus(const CvPlot* pPlot, BonusTypes revealedBonusType)
        {
            if (pPlot->getOwner() == player_.getPlayerID())
            {
                newBonusPlots_.push_back(pPlot);
                return true;
            }
            return false;
        }

        void updateOwnedPlotImprovement(const CvPlot* pPlot, ImprovementTypes oldImprovementType)
        {
//#ifdef ALTAI_DEBUG
//            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//#endif
            updatedImprovements_.push_back(UpdatedImprovementData(pPlot, oldImprovementType));
        }

        void updateCity(const CvCity* pCity, bool remove)
        {
            const IDInfo city = pCity->getIDInfo();
            if (remove)
            {
                std::list<IDInfo>::iterator newCitiesIter = std::find(newCities_.begin(), newCities_.end(), city);
                if (newCitiesIter != newCities_.end())
                {
                    newCities_.erase(newCitiesIter);
                }
                lostCities_.push_back(pCity->getIDInfo());
            }
            else
            {
                std::list<IDInfo>::iterator lostCitiesIter = std::find(lostCities_.begin(), lostCities_.end(), city);
                if (lostCitiesIter != lostCities_.end())
                {
                    lostCities_.erase(lostCitiesIter);
                }
                newCities_.push_back(city);
            }
        }

        void updateCityBonusCount(const CvCity* pCity, BonusTypes bonusType, int delta)
        {
            bool foundExistingEntry = false;
            for (std::list<UpdatedCityBonuses>::iterator iter(updatedCityBonuses_.begin()), iterEnd(updatedCityBonuses_.end()); iter != iterEnd; ++iter)
            {
                if (iter->city == pCity->getIDInfo() && iter->bonusType == bonusType)
                {
                    foundExistingEntry = true;
                    iter->delta += delta;
                    break;
                }
            }

            if (!foundExistingEntry)
            {
                updatedCityBonuses_.push_back(UpdatedCityBonuses(pCity, bonusType, delta));
            }
        }

        std::vector<Unit::WorkerMission> getWorkerMissions(CvUnitAI* pUnit) const
        {
            std::vector<Unit::WorkerMission> missions = player_.getOurUnit(pUnit).getWorkerMissions();

            return missions;
        }

        std::multimap<int, const CvPlot*> getUnitBonusValuePlotMap() const
        {
            std::multimap<int, const CvPlot*> unitBonusValuePlotMap;
            const CvMap& theMap = gGlobals.getMap();

            // std::map<BonusTypes, std::vector<UnitTypes> > unitBonusMap_;
            //std::map<BonusTypes, int> bonusValueMap_;
            //std::multimap<int, XYCoords> bonusesByValueMap_;

            for (std::multimap<int, XYCoords>::const_iterator bvIter(bonusesByValueMap_.begin()), bvEndIter(bonusesByValueMap_.end()); bvIter != bvEndIter; ++bvIter)
            {
                const CvPlot* pBonusPlot = theMap.plot(bvIter->second.iX, bvIter->second.iY);
                BonusTypes bonusType = pBonusPlot->getBonusType(player_.getTeamID());
                std::map<BonusTypes, std::vector<UnitTypes> >::const_iterator ubIter = unitBonusMap_.find(bonusType);
                if (ubIter != unitBonusMap_.end())
                {
                    unitBonusValuePlotMap.insert(std::make_pair(bvIter->first, pBonusPlot));
                }
            }

            return unitBonusValuePlotMap;
        }

        std::set<IDInfo> getUnitsToEscort() const
        {
            return unitsNeedingEscorts_;
        }

        void setEscort(const IDInfo& escortUnit)
        {
            unitsNeedingEscorts_.erase(escortUnit);
            unitsWithEscorts_.insert(escortUnit);
        }

        void logMissions(std::ostream& os) const
        {            
            for (std::map<IDInfo, UnitMissionPtr>::const_iterator unitIter(unitMissions_.begin()), unitEndIter(unitMissions_.end());
                unitIter != unitEndIter; ++unitIter)
            {
                CvUnitAI* pUnit = (CvUnitAI*)player_.getCvPlayer()->getUnit(unitIter->first.iID);
                if (pUnit)
                {
                    Unit unit = player_.getOurUnit(pUnit);
                    const std::vector<Unit::WorkerMission>& missions = unit.getWorkerMissions();
                    int totalLength = 0;

                    os << "\nUnit: " << pUnit->getID() << " has: " << missions.size() << " missions, now at: " << pUnit->plot()->getCoords();                    
                    for (size_t missionIndex = 0, missionCount = missions.size(); missionIndex < missionCount; ++missionIndex)
                    {
                        missions[missionIndex].debug(os);
                        totalLength += missions[missionIndex].length;
                    }
                    os << "\n\ttotal length = " << totalLength;
                }
            }

            for (std::list<ActiveRouteMission>::const_iterator iter(activeRouteMissions_.begin()), endIter(activeRouteMissions_.end()); iter != endIter; ++iter)
            {
                os << "\nactive route mission from: " << iter->missionData.from->getCoords() << " to: " << iter->missionData.to->getCoords()
                    << " with " << iter->missionData.missions.size() << " missions for unit: " << iter->unitId.iID;
            }
        }

        void write(FDataStreamBase* pStream) const
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            if (!lostPlots_.empty() || !newPlots_.empty() || !newCities_.empty() || !lostCities_.empty() ||
                !updatedImprovements_.empty() || !updatedCityBonuses_.empty() || !newBonusPlots_.empty())
            {
                os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " have dynamic worker tactic data during save? "
                    << lostPlots_.size() << " " << newPlots_.size() << " " << newCities_.size() << " " << lostCities_.size()
                    << " " << updatedImprovements_.size() << " " << updatedCityBonuses_.size() << " " << newBonusPlots_.size();
            }
#endif
            pStream->Write(subAreaBonusMap_.size());
            for (BonusMap::const_iterator ci(subAreaBonusMap_.begin()), ciEnd(subAreaBonusMap_.end()); ci != ciEnd; ++ci)
            {
                pStream->Write(ci->first);
                pStream->Write(ci->second.size());
                for (SubAreaBonusMap::const_iterator cii(ci->second.begin()), ciiEnd(ci->second.end()); cii != ciiEnd; ++cii)
                {
                    cii->first.write(pStream);
                    cii->second.write(pStream);
                }
            }

            pStream->Write(cityImprovements_.size());
            for (CityImprovementsMap::const_iterator ci(cityImprovements_.begin()), ciEnd(cityImprovements_.end()); ci != ciEnd; ++ci)
            {
                ci->first.write(pStream);
                writeComplexVector<PlotImprovementData>(pStream, ci->second);
            }
            
            pStream->Write(unitBonusMap_.size());
            for (std::map<BonusTypes, std::vector<UnitTypes> >::const_iterator ci(unitBonusMap_.begin()), ciEnd(unitBonusMap_.end()); ci != ciEnd; ++ci)
            {
                pStream->Write(ci->first);
                writeVector(pStream, ci->second);
            }

            writeMap(pStream, bonusValueMap_);
            pStream->Write(bonusesByValueMap_.size());
            for (std::multimap<int, XYCoords>::const_iterator ci(bonusesByValueMap_.begin()), ciEnd(bonusesByValueMap_.end()); ci != ciEnd; ++ci)
            {
                pStream->Write(ci->first);
                ci->second.write(pStream);
            }

            writeComplexSet(pStream, unitsNeedingEscorts_);
            writeComplexMap(pStream, unitCityTargets_);

            pStream->Write(unitMissions_.size());
            for (std::map<IDInfo, UnitMissionPtr>::const_iterator ci(unitMissions_.begin()), ciEnd(unitMissions_.end()); ci != ciEnd; ++ci)
            {
                ci->first.write(pStream);
                ci->second->write(pStream);
            }

            writePlotBuildData(pStream, bonusTargetPlots_);
            writePlotBuildData(pStream, cityBonusTargetPlots_);
            writePlotBuildData(pStream, cityWaterBonusTargetPlots_);
            writePlotBuildData(pStream, nonCityBonusTargetPlots_);            
            writePlotBuildData(pStream, unconnectedBonusTargetPlots_);
            writePlotBuildData(pStream, unbuiltNonBonusImps_);
            writePlotBuildData(pStream, badFeatureImps_);
            writePlotBuildData(pStream, unirrigatedIrrigatableBonuses_);
            writePlotBuildData(pStream, unbuiltSelectedImps_);
            writePlotBuildData(pStream, unselectedImps_);
            writePlotBuildData(pStream, irrigationChainImps_);
        }

        void read(FDataStreamBase* pStream)
        {
            size_t count = 0;
            pStream->Read(&count);
            subAreaBonusMap_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                int subArea;
                pStream->Read(&subArea);
                size_t bonusCount = 0;
                pStream->Read(&bonusCount);
                SubAreaBonusMap subAreaBonusMap;
                for (size_t j = 0; j < bonusCount; ++j)
                {
                    XYCoords coords;
                    coords.read(pStream);
                    BonusPlotData bonusPlotData;
                    bonusPlotData.read(pStream);
                    subAreaBonusMap.insert(std::make_pair(coords, bonusPlotData));
                }
                subAreaBonusMap_.insert(std::make_pair(subArea, subAreaBonusMap));
            }

            pStream->Read(&count);
            cityImprovements_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                IDInfo city;
                city.read(pStream);
                PlotImprovements plotImprovements;
                readComplexVector<PlotImprovementData>(pStream, plotImprovements);
                cityImprovements_.insert(std::make_pair(city, plotImprovements));
            }

            pStream->Read(&count);
            unitBonusMap_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                BonusTypes bonusType;
                pStream->Read((int*)&bonusType);
                std::vector<UnitTypes> unitTypes;
                readVector<UnitTypes, int>(pStream, unitTypes);
                unitBonusMap_.insert(std::make_pair(bonusType, unitTypes));
            }

            readMap<BonusTypes, int, int, int>(pStream, bonusValueMap_);

            pStream->Read(&count);
            bonusesByValueMap_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                int value;
                pStream->Read(&value);
                XYCoords coords;
                coords.read(pStream);
                bonusesByValueMap_.insert(std::make_pair(value, coords));
            }

            readComplexSet<IDInfo>(pStream, unitsNeedingEscorts_);
            readComplexMap<IDInfo, IDInfo>(pStream, unitCityTargets_);

            pStream->Read(&count);
            for (size_t i = 0; i < count; ++i)
            {
                IDInfo city;
                city.read(pStream);
                UnitMissionPtr pUnitMission = IUnitMission::factoryRead(pStream);
                boost::shared_ptr<WorkerMission> pWorkerMission = boost::dynamic_pointer_cast<WorkerMission>(pUnitMission);
                pWorkerMission->setWorkerAnalysis(this);
                unitMissions_.insert(std::make_pair(city, pUnitMission));
            }

            readPlotBuildData(pStream, bonusTargetPlots_);
            readPlotBuildData(pStream, cityBonusTargetPlots_);
            readPlotBuildData(pStream, cityWaterBonusTargetPlots_);
            readPlotBuildData(pStream, nonCityBonusTargetPlots_);
            readPlotBuildData(pStream, unconnectedBonusTargetPlots_);
            readPlotBuildData(pStream, unbuiltNonBonusImps_);
            readPlotBuildData(pStream, badFeatureImps_);
            readPlotBuildData(pStream, unirrigatedIrrigatableBonuses_);
            readPlotBuildData(pStream, unbuiltSelectedImps_);
            readPlotBuildData(pStream, unselectedImps_);
            readPlotBuildData(pStream, irrigationChainImps_);
        }

        class WorkerMission : public IUnitMission
        {
        public:
            virtual ~WorkerMission() {}

            WorkerMission() : pAnalysis_(NULL)
            {
            }

            WorkerMission(CvUnitAI* pUnit, WorkerAnalysisImpl* pAnalysis)
                : unit_(pUnit->getIDInfo()), unitAIType_(pUnit->getDomainType() == DOMAIN_LAND ? UNITAI_WORKER : UNITAI_WORKER_SEA), pAnalysis_(pAnalysis)
            {
            }

            void setWorkerAnalysis(WorkerAnalysisImpl* const pAnalysis)
            {
                pAnalysis_ = pAnalysis;
            }

            virtual void update()
            {
                if (unitAIType_ == UNITAI_WORKER)
                {
                    CvUnitAI* pUnit = (CvUnitAI*)::getUnit(unit_);
                    pAnalysis_->updateWorkerMission(pUnit);
                }
            }

            virtual const CvUnitAI* getUnit() const
            {
                return (const CvUnitAI*)::getUnit(unit_);
            }

            virtual CvPlot* getTarget() const
            {
                return pAnalysis_->getCurrentTarget(unit_);
            }

            virtual bool isComplete() const
            {
                int missionCount, completedMissionCount;
                CvUnitAI* pUnit = (CvUnitAI*)::getUnit(unit_);
                boost::tie(completedMissionCount, missionCount) = pAnalysis_->getCompletedMissionCount(pUnit);
                return completedMissionCount == missionCount;
            }

            virtual CvCity* getTargetCity() const
            {
                return (CvCity*)pAnalysis_->getTargetCity(unit_);
            }

            virtual UnitAITypes getAIType() const
            {
                return unitAIType_;
            }

            virtual void write(FDataStreamBase* pStream) const
            {
                pStream->Write(ID);
                unit_.write(pStream);
                pStream->Write(unitAIType_);
            }

            virtual void read(FDataStreamBase* pStream)
            {
                unit_.read(pStream);
                pStream->Read((int*)&unitAIType_);
            }

            static const int ID = 2;

        private:
            IDInfo unit_;
            UnitAITypes unitAIType_;
            WorkerAnalysisImpl* pAnalysis_;
        };

    private:
        void updateLandWorkerMission_(CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            boost::shared_ptr<MapAnalysis> pMapAnalysis = player_.getAnalysis()->getMapAnalysis();
            updateActiveRouteMissions_(pUnit, false);

            Unit& unit = player_.getOurUnit(pUnit);
            const bool hasEscort = unitsWithEscorts_.find(pUnit->getIDInfo()) != unitsWithEscorts_.end();

            const CvCity* pTargetCity = getTargetCity(pUnit->getIDInfo());
            
            std::vector<Unit::WorkerMission> missions = unit.getWorkerMissions();
            size_t completedMissionCount = getCompletedMissionCount(pUnit).first;

            player_.clearWorkerMission(pUnit);

            PlotSet moveToPlotSet;
            moveToPlotSet.insert(pUnit->plot());  // just add target plot - might be better to include transit plots?

            std::set<IDInfo> hostileUnits = player_.getAnalysis()->getMilitaryAnalysis()->getUnitsThreateningPlots(moveToPlotSet);            
            if (!hostileUnits.empty()) 
            {
                if (!hasEscort)
                {
                    unitsNeedingEscorts_.insert(pUnit->getIDInfo());
                    player_.clearWorkerMission(pUnit);
                }

                // todo - factor in calc of survival prob if attacked
                const CvPlot* pEscapePlot = getNextMovePlot(player_, pUnit->getGroup(), pUnit->plot());                
                if (pEscapePlot != pUnit->plot())
                {                    
                    player_.pushWorkerMission(pUnit, NULL, pEscapePlot, MISSION_MOVE_TO, NO_BUILD, MOVE_IGNORE_DANGER, "WorkerAnalysisImpl::updateLandWorkerMission_ - escape from hostiles");
                }
                else
                {
                    player_.pushWorkerMission(pUnit, NULL, pEscapePlot, MISSION_SKIP, NO_BUILD);
                }                
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\n Completed mission count for unit: " << pUnit->getIDInfo() << " = " << completedMissionCount;
#endif
                
                if (hasEscort)
                {
                    bool foundBorderMission = false;
                    for (size_t i = 0; i < missions.size() - completedMissionCount; ++i)
                    {
                        size_t missionIndex = i + completedMissionCount;
                        // only checking for border missions if haven't already found one and we are already escorted
                        if (hasEscort && !foundBorderMission)
                        {
                            foundBorderMission = missions[missionIndex].isBorderMission(pMapAnalysis);
                        }
                    }

                    if (!foundBorderMission)
                    {
                        UnitGroupIter groupIter(pUnit->getGroup());
                        while (CvUnit* pPlotUnit = groupIter())
                        {
                            if (pPlotUnit->canFight())  // cheesy - assumes workers can't fight and therefore this unit is an escort
                            {
                                // split off escort unit
                                MilitaryMissionDataPtr pMissionData = player_.getAnalysis()->getMilitaryAnalysis()->getMissionData((CvUnitAI*)pPlotUnit);
                                FAssertMsg(pMissionData && pMissionData->targets.find(pUnit->getIDInfo()) != pMissionData->targets.end(), "escorted worker not in escort's targets?")
                                unitsWithEscorts_.erase(pUnit->getIDInfo());  // todo - assumes we only have one escort
                                pMissionData->targets.clear();  // assumes a single unit to be escorted (prob reasonable)
                                pPlotUnit->joinGroup(NULL);
#ifdef ALTAI_DEBUG
                                os << "\nremoved worker unit escort: " << pPlotUnit->getIDInfo() << " from worker: " << pUnit->getIDInfo();
#endif
                            }
                        }
                    }
#ifdef ALTAI_DEBUG
                    else
                    {
                        os << "\nkeeping escort for worker: " << pUnit->getIDInfo();
                    }
#endif
                }

                // resubmit remaining missions                
                for (size_t i = 0; i < missions.size() - completedMissionCount; ++i)
                {
                    size_t missionIndex = i + completedMissionCount;
                    player_.pushWorkerMission(pUnit, getTargetCity(pUnit->getIDInfo()), missions[missionIndex].getTarget(), 
                        missions[missionIndex].missionType, missions[missionIndex].buildType, 
                        (missions[missionIndex].missionType == MISSION_MOVE_TO ? MOVE_IGNORE_DANGER : 0), "WorkerAnalysisImpl::continueWorkerMission");
                }

                if (unit.getWorkerMissions().empty())
                {
                    PlotSet dangerPlots = player_.getAnalysis()->getMilitaryAnalysis()->getThreatenedPlots();

                    RouteTypes routeType = player_.getCvPlayer()->getBestRoute();

                    std::map<IDInfo, int> cityWorkerCounts;
                    for (std::map<IDInfo, IDInfo>::const_iterator missionIter(unitCityTargets_.begin()), missionEndIter(unitCityTargets_.end());
                        missionIter != missionEndIter; ++missionIter)
                    {
                        const CvCity* pTargetCity = getTargetCity(missionIter->first);
                        if (pTargetCity)
                        {
                            ++cityWorkerCounts[pTargetCity->getIDInfo()];
                        }
                    }

                    std::map<int, IDInfo> areaCityDistances;
                    // todo - what about if the city has plots in this sub area, but is in a different area?
                    CityIterP<IsSubAreaP> cityIter(*player_.getCvPlayer(), IsSubAreaP(pUnit->plot()->getSubArea()));
                    while (const CvCity* pCity = cityIter())
                    {
                        areaCityDistances[stepDistance(pUnit->getX(), pUnit->getY(), pCity->getX(), pCity->getY())] = pCity->getIDInfo();
                        if (cityWorkerCounts.find(pCity->getIDInfo()) == cityWorkerCounts.end())
                        {
                            cityWorkerCounts[pCity->getIDInfo()] = 0;
                        }
                    }

                    const CvCity* pCurrentCity = getTargetCity(pUnit->getIDInfo());
#ifdef ALTAI_DEBUG
                    os << "\n" << __FUNCTION__ << " current city: " << safeGetCityName(pCurrentCity);
                    os << "\ncity worker counts: ";
                    for (std::map<IDInfo, int>::const_iterator ci(cityWorkerCounts.begin()), ciEnd(cityWorkerCounts.end()); ci != ciEnd; ++ci)
                    {
                        os << narrow(::getCity(ci->first)->getName()) << " = " << ci->second << " ";
                    }
#endif

                    if (tryMission(pUnit, pCurrentCity, areaCityDistances, bonusTargetPlots_, cityWorkerCounts, routeType, dangerPlots))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected bonus mission, city = " << safeGetCityName(pCurrentCity);
#endif
                        return;
                    }

                    if (tryMission(pUnit, pCurrentCity, areaCityDistances, cityBonusTargetPlots_, cityWorkerCounts, routeType, dangerPlots))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected city bonus mission, city = " << safeGetCityName(pCurrentCity);
#endif
                        return;
                    }

                    if (tryMission(pUnit, pCurrentCity, areaCityDistances, unconnectedBonusTargetPlots_, cityWorkerCounts, routeType, dangerPlots))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected unconnected bonus mission, city = " << safeGetCityName(pCurrentCity);
#endif
                        return;
                    }

                    if (tryMission(pUnit, pCurrentCity, areaCityDistances, unbuiltNonBonusImps_, cityWorkerCounts, routeType, dangerPlots))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected city imp mission, city = " << safeGetCityName(pCurrentCity);
#endif
                        return;
                    }

                    if (tryMission(pUnit, pCurrentCity, areaCityDistances, badFeatureImps_, cityWorkerCounts, NO_ROUTE, dangerPlots))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected bad feature imp mission, city = " << safeGetCityName(pCurrentCity);
#endif
                        return;
                    }

                    if (tryMission(pUnit, pCurrentCity, areaCityDistances, unirrigatedIrrigatableBonuses_, cityWorkerCounts, NO_ROUTE, dangerPlots))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected connect irrigation mission, city = " << safeGetCityName(pCurrentCity);
#endif
                        return;
                    }
            
                    if (tryMission(pUnit, pCurrentCity, areaCityDistances, irrigationChainImps_, cityWorkerCounts, NO_ROUTE, dangerPlots))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected chain irrigation mission, city = " << safeGetCityName(pCurrentCity);
#endif
                        return;
                    }

                    if (tryMission(pUnit, pCurrentCity, areaCityDistances, unbuiltSelectedImps_, cityWorkerCounts, NO_ROUTE, dangerPlots))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected unbuilt, selected imp mission, city = " << safeGetCityName(pCurrentCity);
#endif
                        return;
                    }

                    if (tryMission(pUnit, pCurrentCity, areaCityDistances, unselectedImps_, cityWorkerCounts, NO_ROUTE, dangerPlots))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected unselected imp mission, city = " << safeGetCityName(pCurrentCity);
#endif
                        return;
                    }

                    std::list<MissionHelperData> otherWorkerUnits;
                    for (std::map<IDInfo, UnitMissionPtr>::const_iterator unitIter(unitMissions_.begin()), unitEndIter(unitMissions_.end());
                        unitIter != unitEndIter; ++unitIter)
                    {                        
                        CvUnitAI* pWorkerUnit = (CvUnitAI*)player_.getCvPlayer()->getUnit(unitIter->first.iID);
                        if (pWorkerUnit && pWorkerUnit != pUnit && pWorkerUnit->plot()->getSubArea() == pUnit->plot()->getSubArea())
                        {
                            Unit workerUnit = player_.getOurUnit(pWorkerUnit);
                            const std::vector<Unit::WorkerMission>& missions = workerUnit.getWorkerMissions();
                            if (!missions.empty())
                            {
                                MissionHelperData helperData;
                                helperData.unit = unitIter->first;
                                helperData.missionTurns = 0;
                                for (size_t missionIndex = 0, missionCount = missions.size(); missionIndex < missionCount; ++missionIndex)
                                {
                                    helperData.missionTurns += missions[missionIndex].length;
                                }

                                UnitPathData unitPathData;
                                unitPathData.calculate(pUnit->getGroup(), pWorkerUnit->plot(), MOVE_MAX_MOVES);

                                if (unitPathData.valid)
                                {
                                    helperData.turnsToPlot = unitPathData.pathTurns;
                                }
                            }
                        }
                    }
                    otherWorkerUnits.sort(MissionHelperComp());

#ifdef ALTAI_DEBUG
                    os << " found " << otherWorkerUnits.size() << " other worker missions: ";
                    for (std::list<MissionHelperData>::const_iterator missionsIter(otherWorkerUnits.begin()); missionsIter != otherWorkerUnits.end(); ++missionsIter)
                    {
                        missionsIter->debug(os);
                    }
#endif

                    for (std::list<MissionHelperData>::const_iterator missionsIter(otherWorkerUnits.begin()); missionsIter != otherWorkerUnits.end(); ++missionsIter)
                    {
                        const Unit& workerUnit = player_.getOurUnit(missionsIter->unit);
                        const std::vector<Unit::WorkerMission>& missions = workerUnit.getWorkerMissions();
                        if (!missions.empty())
                        {
                            XYCoords coords = missions[0].targetCoords;
                            const CvCity* pMissionTargetCity = ::getCity(missions[0].city); // can be null
                            unitCityTargets_[pUnit->getIDInfo()] = missions[0].city;
                            if (pUnit->plot()->getCoords() != missions[0].targetCoords)
                            {
                                player_.pushWorkerMission(pUnit, pMissionTargetCity, gGlobals.getMap().plot(missions[0].targetCoords.iX, missions[0].targetCoords.iY), 
                                    MISSION_MOVE_TO, NO_BUILD, MOVE_IGNORE_DANGER, "WorkerAnalysisImpl::updateLandWorkerMission - move to assist unit");
                            }

                            for (size_t missionIndex = 0, count = missions.size(); missionIndex < count; ++missionIndex)
                            {
                                player_.pushWorkerMission(pUnit, pMissionTargetCity, missions[missionIndex].getTarget(), 
                                    missions[missionIndex].missionType, missions[missionIndex].buildType, 
                                    (missions[missionIndex].missionType == MISSION_MOVE_TO ? MOVE_IGNORE_DANGER : 0), "WorkerAnalysisImpl::updateLandWorkerMission - assist unit");
                            }
#ifdef ALTAI_DEBUG
                            os << " selected assist mission, assisting: " << missions[0].unit;
#endif
                            return;
                        }
                    }

#ifdef ALTAI_DEBUG
                    os << "\nFailed to work worker: " << pUnit->getID();
#endif
                    if (!pUnit->plot()->isCity())
                    {
                        if (pCurrentCity && pCurrentCity->plot()->getSubArea() == pUnit->plot()->getSubArea())
                        {
                            player_.pushWorkerMission(pUnit, pCurrentCity, pCurrentCity->plot(), MISSION_MOVE_TO, NO_BUILD, 0, "WorkerAnalysisImpl::updateLandWorkerMission_ - return to city base");
                        }
                    }

                    player_.pushWorkerMission(pUnit, NULL, pUnit->plot(), MISSION_SKIP, NO_BUILD, 0, "WorkerAnalysisImpl::updateLandWorkerMission_ - skip");
                }
            }

            int missionFlags = executeWorkerMission(pUnit);
            if (missionFlags & Unit::CantStart)
            {
#ifdef ALTAI_DEBUG
                os << "\nCan't start worker mission: " << pUnit->getID();
#endif
                player_.clearWorkerMission(pUnit);  // clear out missions and hopefully recalcuate one we can do
            }
        }

        void updateSeaWorkerMission_(CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            Unit unit = player_.getOurUnit(pUnit);
            std::vector<Unit::WorkerMission> missions = unit.getWorkerMissions();

            // shouldn't have any missions to begin with, and since successful mission means unit is used up
            // (todo - make this dynamic on unit build types like in city build selection)
            // we rely on being called back if we complete an escape mission and end up back with no missions again
            if (!missions.empty())
            {
                const std::vector<Unit::WorkerMission>& missions = unit.getWorkerMissions();
                size_t completedMissionCount = getCompletedMissionCount(pUnit).first;

                player_.clearWorkerMission(pUnit);

#ifdef ALTAI_DEBUG
                os << "\n Completed mission count for sea worker unit: " << pUnit->getIDInfo() << " = " << completedMissionCount;
#endif
                // resubmit remaining missions
                for (size_t i = 0; i < missions.size() - completedMissionCount; ++i)
                {
                    size_t missionIndex = i + completedMissionCount;
                    player_.pushWorkerMission(pUnit, getTargetCity(pUnit->getIDInfo()), missions[missionIndex].getTarget(), 
                        missions[missionIndex].missionType, missions[missionIndex].buildType, 
                        (missions[missionIndex].missionType == MISSION_MOVE_TO ? MOVE_NO_ENEMY_TERRITORY : 0), "WorkerAnalysisImpl::continueWorkerMission");
                }
                executeWorkerMission(pUnit);
                return;
            }            
            else
            {
                const CvArea* pUnitArea = NULL;
                if (pUnit->plot()->isCity())
                {
                    pUnitArea = pUnit->plot()->waterArea();  // todo - get list of subareas directly - this fn only returns largest adjacent water area
                }
                else
                {
                    pUnitArea = pUnit->plot()->area();
                }

                IDInfo bestTargetCity;
                int shortestPath = MAX_INT;
                PlotBuildData buildData;

                for (std::map<IDInfo, std::vector<PlotBuildData> >::const_iterator buildsIter(cityWaterBonusTargetPlots_.begin()), buildsEndIter(cityWaterBonusTargetPlots_.end());
                    buildsIter != buildsEndIter; ++buildsIter)
                {
                    for (size_t i = 0, count = buildsIter->second.size(); i < count; ++i)
                    {
                        CvPlot* pPlot = gGlobals.getMap().plot(buildsIter->second[i].coords.iX, buildsIter->second[i].coords.iY);
                        if (pPlot->area() == pUnitArea)
                        {
                            if (player_.getNumWorkersTargetingPlot(buildsIter->second[i].coords) == 0)
                            {
                                UnitPathData unitPathData;
                                unitPathData.calculate(pUnit->getGroup(), pPlot, MOVE_NO_ENEMY_TERRITORY);

                                if (unitPathData.valid && unitPathData.pathTurns < shortestPath)
                                {
                                    shortestPath = unitPathData.pathTurns;
                                    bestTargetCity = buildsIter->first;
                                    buildData = buildsIter->second[i];
                                }                                
                            }
                        }
                    }
                }

                if (shortestPath < MAX_INT && buildData.coords != XYCoords(-1, -1))
                {
                    if (pushMission(pUnit, ::getCity(bestTargetCity), buildData, true))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected city water bonus mission, city = " << safeGetCityName(bestTargetCity);
#endif
                        unitCityTargets_[pUnit->getIDInfo()] = bestTargetCity;
                        executeWorkerMission(pUnit);
                        return;
                    }
                }

                // try exploring instead - todo switch to a general ocean explore once can enter uncontrolled ocean plots
                if (doCoastalUnitExplore(pUnit))
                {
                    return;
                }
            
#ifdef ALTAI_DEBUG
                os << "\nFailed to work sea worker: " << pUnit->getID();
#endif
                player_.pushWorkerMission(pUnit, NULL, pUnit->plot(), MISSION_SKIP, NO_BUILD);
            }
        }

        void updateActiveRouteMissions_(CvUnit* pUnit, bool removeAll)
        {
            std::list<ActiveRouteMission>::iterator iter(activeRouteMissions_.begin());
            while (iter != activeRouteMissions_.end())
            {
                if (iter->unitId == pUnit->getIDInfo())
                {
                    if (!removeAll)
                    {
                        std::vector<Unit::WorkerMission>::iterator missionsIter = std::remove_if(iter->missionData.missions.begin(), iter->missionData.missions.end(), ActiveRouteMission::IsComplete());
                        iter->missionData.missions.erase(missionsIter, iter->missionData.missions.end());
                    }

                    if (removeAll || iter->missionData.missions.empty())
                    {
                        activeRouteMissions_.erase(iter++);
#ifdef ALTAI_DEBUG
                        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
                        os << "\nErasing active route mission for unit: " << pUnit->getIDInfo().iID;
#endif
                    }
                    else
                    {
                        ++iter;
                    }
                }
                else
                {
                    ++iter;
                }
            }
        }

        bool haveActiveRouteMissions_(const CvPlot* pFromPlot, const CvPlot* pToPlot) const
        {
            bool foundMatch = false;
            for (std::list<ActiveRouteMission>::const_iterator iter(activeRouteMissions_.begin()), endIter(activeRouteMissions_.end()); iter != endIter; ++iter)
            {
                if (iter->missionData.from == pFromPlot && iter->missionData.to == pToPlot ||
                    iter->missionData.from == pToPlot && iter->missionData.to == pFromPlot)
                {
                    foundMatch = true;
                    break;
                }
            }
            return foundMatch;
        }

        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, int> getNextImprovement_(const City& city, std::vector<PlotCondPtr>& conditions)
        {
            boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, int> bestImprovement;

            for (;;)
            {                    
                bestImprovement = city.getCityImprovementManager()->getBestImprovementNotBuilt(false, false, conditions);
                XYCoords coords = boost::get<0>(bestImprovement);
                if (boost::get<2>(bestImprovement) == NO_IMPROVEMENT)
                {
                    break;
                }
                if (player_.getNumWorkersTargetingPlot(coords) > 0)
                {
                    conditions.push_back(PlotCondPtr(new IgnorePlot(gGlobals.getMap().plot(coords.iX, coords.iY))));
                }
                else
                {
                    return bestImprovement;
                }
            }

            return boost::make_tuple(XYCoords(-1, -1), NO_FEATURE, NO_IMPROVEMENT, 0);
        }

        Player& player_;
        std::list<const CvPlot*> newPlots_, lostPlots_;        
        std::list<IDInfo> newCities_, lostCities_;
        std::list<const CvPlot*> newBonusPlots_;

        struct UpdatedImprovementData
        {
            UpdatedImprovementData() : oldImp(NO_IMPROVEMENT), newImp(NO_IMPROVEMENT) {}
            UpdatedImprovementData(const CvPlot* pPlot_, ImprovementTypes oldImp_) : pPlot(pPlot_), oldImp(oldImp_)
            {
                newImp = pPlot->getImprovementType();
            }

            const CvPlot* pPlot;
            ImprovementTypes oldImp, newImp;
        };

        std::list<UpdatedImprovementData> updatedImprovements_;

        struct UpdatedCityBonuses
        {
            UpdatedCityBonuses() : bonusType(NO_BONUS), delta(0) {}
            UpdatedCityBonuses(const CvCity* pCity, BonusTypes bonusType_, int delta_)
                : city(pCity->getIDInfo()), bonusType(bonusType_), delta(delta_)
            {
            }

            void write(FDataStreamBase* pStream) const
            {
                city.write(pStream);
                pStream->Write(bonusType);
                pStream->Write(delta);
            }

            void read(FDataStreamBase* pStream)
            {
                city.read(pStream);
                pStream->Read((int*)&bonusType);
                pStream->Read(&delta);
            }

            IDInfo city;
            BonusTypes bonusType;
            int delta;
        };

        std::list<UpdatedCityBonuses> updatedCityBonuses_;

        struct BonusPlotData
        {
            BonusPlotData() : bonusType(NO_BONUS), improvementType(NO_IMPROVEMENT), improvementReqd(false) {}
            BonusPlotData(BonusTypes bonusType_, ImprovementTypes improvementType_) : bonusType(bonusType_), improvementType(improvementType_)
            {
                improvementReqd = isImpReqd();
            } 

            void updateImprovement(ImprovementTypes newImp)
            {
                improvementType = newImp;
                improvementReqd = isImpReqd();
            }

            bool isImpReqd() const
            {
                const bool improvementActsAsCity = improvementType == NO_IMPROVEMENT ? false : gGlobals.getImprovementInfo(improvementType).isActsAsCity();
                ImprovementTypes requiredImprovement = getBonusImprovementType(bonusType);

                return (improvementType == NO_IMPROVEMENT || improvementActsAsCity) && requiredImprovement != NO_IMPROVEMENT;
            }

            void write(FDataStreamBase* pStream) const
            {
                pStream->Write(bonusType);
                pStream->Write(improvementType);
                writeComplexSet(pStream, connectedCities);
                pStream->Write(improvementReqd);
            }

            void read(FDataStreamBase* pStream)
            {
                pStream->Read((int*)&bonusType);
                pStream->Read((int*)&improvementType);
                readComplexSet(pStream, connectedCities);
                pStream->Read(&improvementReqd);
            }

            BonusTypes bonusType;
            ImprovementTypes improvementType;
            std::set<IDInfo> connectedCities;

            bool improvementReqd;
        };

        typedef std::map<XYCoords, BonusPlotData> SubAreaBonusMap;
        typedef std::map<int /* SubArea Id */, SubAreaBonusMap> BonusMap;
        BonusMap subAreaBonusMap_;

        typedef std::vector<PlotImprovementData> PlotImprovements;
        typedef std::map<IDInfo, PlotImprovements > CityImprovementsMap;
        // todo - deal with conflicting improvements from plots shared by cities (use shared plot logic in MapAnalysis?)
        CityImprovementsMap cityImprovements_;

        std::map<BonusTypes, std::vector<UnitTypes> > unitBonusMap_;
        std::map<BonusTypes, int> bonusValueMap_;
        std::multimap<int, XYCoords> bonusesByValueMap_;

        std::set<IDInfo> unitsNeedingEscorts_, unitsWithEscorts_;
        std::map<IDInfo, UnitMissionPtr> unitMissions_;
        std::map<IDInfo, IDInfo> unitCityTargets_;

        std::map<IDInfo, std::vector<PlotBuildData> > bonusTargetPlots_, cityBonusTargetPlots_, cityWaterBonusTargetPlots_,
            nonCityBonusTargetPlots_, unconnectedBonusTargetPlots_, unbuiltNonBonusImps_, badFeatureImps_, 
            unirrigatedIrrigatableBonuses_, unbuiltSelectedImps_, unselectedImps_, irrigationChainImps_;

        struct ActiveRouteMission
        {
            ActiveRouteMission(const Unit& unit, const CvPlot* from, const CvPlot* to)
            {
                unitId = unit.getUnit()->getIDInfo();
                missionData.from = from, missionData.to = to;
                missionData.missions = unit.getWorkerMissions();
            }

            struct IsComplete
            {
                bool operator() (const Unit::WorkerMission& mission) const
                {
                    return mission.isComplete();
                }
            };

            struct WorkerMissionData
            {
                const CvPlot* from, *to;
                std::vector<Unit::WorkerMission> missions;
            };
            IDInfo unitId;
            WorkerMissionData missionData;
        };
        std::list<ActiveRouteMission> activeRouteMissions_;
    };

    WorkerAnalysis::WorkerAnalysis(Player& player)
    {
        pImpl_ = boost::shared_ptr<WorkerAnalysisImpl>(new WorkerAnalysisImpl(player));
    }

    void WorkerAnalysis::updatePlotOwner(const CvPlot* pPlot, PlayerTypes previousRevealedOwner, PlayerTypes newRevealedOwner)
    {
        pImpl_->updatePlotOwner(pPlot, previousRevealedOwner, newRevealedOwner);
    }

    void WorkerAnalysis::updateOwnedPlotImprovement(const CvPlot* pPlot, ImprovementTypes oldImprovementType)
    {
        pImpl_->updateOwnedPlotImprovement(pPlot, oldImprovementType);
    }

    void WorkerAnalysis::updatePlotBonus(const CvPlot* pPlot, BonusTypes revealedBonusType)
    {
        if (pImpl_->updatePlotBonus(pPlot, revealedBonusType))
        {
            pImpl_->updateBonusData();
        }
    }

    void WorkerAnalysis::updateCity(const CvCity* pCity, bool remove)
    {
        pImpl_->updateCity(pCity, remove);
    }

    void WorkerAnalysis::updateCityBonusCount(const CvCity* pCity, BonusTypes bonusType, int delta)
    {
        pImpl_->updateCityBonusCount(pCity, bonusType, delta);
    }

    void WorkerAnalysis::logMissions(std::ostream& os) const
    {
        pImpl_->logMissions(os);
    }

    void WorkerAnalysis::updatePlotData()
    {
        pImpl_->updatePlotData();
        pImpl_->updateBonusData();
    }

    void WorkerAnalysis::updatePossibleMissionsData()
    {
        pImpl_->updatePossibleMissionsData();
    }

    void WorkerAnalysis::updateWorkerMission(CvUnitAI* pUnit)
    {
        pImpl_->updateWorkerMission(pUnit);
    }

    void WorkerAnalysis::deleteUnit(CvUnit* pUnit)
    {
        pImpl_->deleteUnit(pUnit);
    }

    std::vector<Unit::WorkerMission> WorkerAnalysis::getWorkerMissions(CvUnitAI* pUnit) const
    {
        return pImpl_->getWorkerMissions(pUnit);
    }

    std::multimap<int, const CvPlot*> WorkerAnalysis::getUnitBonusValuePlotMap() const
    {
        return pImpl_->getUnitBonusValuePlotMap();
    }

    std::set<IDInfo> WorkerAnalysis::getUnitsToEscort() const
    {
        return pImpl_->getUnitsToEscort();
    }

    void WorkerAnalysis::setEscort(const IDInfo& escortUnit)
    {
        return pImpl_->setEscort(escortUnit);
    }

    void WorkerAnalysis::write(FDataStreamBase* pStream) const
    {
        pImpl_->write(pStream);
    }

    void WorkerAnalysis::read(FDataStreamBase* pStream)
    {
        pImpl_->read(pStream);
    }

    void updateWorkerAnalysis(Player& player)
    {
        boost::shared_ptr<WorkerAnalysis> pWorkerAnalysis = player.getAnalysis()->getWorkerAnalysis();
        pWorkerAnalysis->updatePlotData();
        pWorkerAnalysis->updatePossibleMissionsData();
    }

    bool doWorkerMove(Player& player, CvUnitAI* pUnit)
    {        
        player.getAnalysis()->getWorkerAnalysis()->updateWorkerMission(pUnit);
        return true;
    }

    UnitMissionPtr workerMissionFactoryHelper()
    {
        return UnitMissionPtr(new WorkerAnalysisImpl::WorkerMission());
    }
}