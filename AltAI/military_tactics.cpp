#include "AltAI.h"

#include "./military_tactics.h"
#include "./unit_mission.h"
#include "./city_defence.h"
#include "./worker_tactics.h"
#include "./player_analysis.h"
#include "./unit_analysis.h"
#include "./unit_tactics.h"
#include "./game.h"
#include "./player.h"
#include "./settler_manager.h"
#include "./unit_explore.h"
#include "./map_analysis.h"
#include "./sub_area.h"
#include "./iters.h"
#include "./save_utils.h"
#include "./civ_log.h"
#include "./unit_log.h"
#include "./helper_fns.h"

#include "../CvGameCoreDLL/CvDLLEngineIFaceBase.h"
#include "../CvGameCoreDLL/CvDLLFAStarIFaceBase.h"
#include "../CvGameCoreDLL/CvGameCoreUtils.h"
#include "../CvGameCoreDLL/FAStarNode.h"


namespace AltAI
{
    namespace
    {
        void getOverlapAndNeighbourPlots(const PlotSet& theirPlots, const PlotSet& ourPlots, 
            PlotSet& sharedPlots, PlotSet& neighbourPlots)
        {
            for (PlotSet::const_iterator ourIter(ourPlots.begin()), ourEndIter(ourPlots.end()); ourIter != ourEndIter; ++ourIter)
            {
                PlotSet::const_iterator theirIter(theirPlots.find(*ourIter));
                if (theirIter != theirPlots.end())
                {
                    sharedPlots.insert(*ourIter);
                }
                else
                {
                    NeighbourPlotIter plotIter(*ourIter);

                    while (IterPlot pLoopPlot = plotIter())
                    {
                        if (pLoopPlot.valid())
                        {
                            theirIter = theirPlots.find(pLoopPlot);
                            if (theirIter != theirPlots.end())
                            {
                                neighbourPlots.insert(*ourIter);
                                break;
                            }
                        }
                    }
                }
            }
        }    

        std::set<IDInfo> getNearbyCities(const Player& player, int subArea, const PlotSet& plots)
        {
            std::set<IDInfo> nearbyCities;

            for (PlotSet::const_iterator plotIter(plots.begin()), plotEndIter(plots.end()); plotIter != plotEndIter; ++plotIter)
            {
                if ((*plotIter)->getOwner() == player.getPlayerID() && (*plotIter)->isCity())
                {
                    const CvCity* pNearbyCity = (*plotIter)->getPlotCity();
                    nearbyCities.insert(pNearbyCity->getIDInfo());
                }

                NeighbourPlotIter nPlotIter(*plotIter, 2);
                while (IterPlot pNeighbourPlot = nPlotIter())
                {
                    if (pNeighbourPlot->getSubArea() == subArea && pNeighbourPlot->isVisible(player.getTeamID(), false) && pNeighbourPlot->getOwner() == player.getPlayerID())
                    {
                        if (pNeighbourPlot->isCity())
                        {
                            const CvCity* pNearbyCity = pNeighbourPlot->getPlotCity();
                            nearbyCities.insert(pNearbyCity->getIDInfo());
                        }
                    }
                }
            }

            return nearbyCities;
        }

        struct RequiredUnits
        {
            std::vector<UnitData> unitsToBuild;
            std::vector<IDInfo> existingUnits;
        };

        struct CanFoundP
        {
            bool operator () (IDInfo unit) const
            {
                const CvUnit* pUnit = ::getUnit(unit);
                return pUnit && pUnit->isFound();
            }
        };

        struct GroupPathComp
        {
            bool operator () (const std::pair<CvSelectionGroup*, UnitPathData>& groupPathData1, const std::pair<CvSelectionGroup*, UnitPathData>& groupPathData2) const
            {
                return groupPathData1.second.pathTurns < groupPathData2.second.pathTurns;
            }
        };

        struct UnitAttackOrderComp
        {
            bool operator () (const CvUnit* pUnit1, const CvUnit* pUnit2) const
            {
            }
        };

        struct AttackThreatComp
        {
            bool operator () (const std::pair<IDInfo, float>& ourDefenceOdds1, const std::pair<IDInfo, float>& ourDefenceOdds2) const
            {
                return ourDefenceOdds1.second < ourDefenceOdds2.second;
            }
        };
    
        struct CombatResultsData
        {
            std::pair<XYCoords, CombatGraph::Data> attackData;
            std::list<std::pair<XYCoords, CombatGraph::Data> > defenceData;
        };

        struct MissionFinder
        {
            MissionFinder() : missionType(NO_MISSIONAI) {}
            MissionAITypes missionType;
            IDInfo city, targetUnit, ourUnit;
            XYCoords targetCoords;

            bool operator() (const MilitaryMissionDataPtr& pMission) const
            {
                bool isMatch = true;
                if (missionType != NO_MISSIONAI)
                {
                    isMatch = isMatch && pMission->missionType == missionType;
                }

                if (city != IDInfo())
                {
                    isMatch = isMatch && pMission->targetCity == city;
                }

                if (targetUnit != IDInfo())
                {
                    bool found = pMission->targetUnits.find(targetUnit) != pMission->targetUnits.end();
                    if (!found && !pMission->specialTargets.empty())
                    {
                        found = pMission->specialTargets.find(targetUnit) != pMission->specialTargets.end();
                    }
                    isMatch = isMatch && found;
                }

                if (ourUnit != IDInfo())
                {
                    isMatch = isMatch && pMission->assignedUnits.find(ourUnit) != pMission->assignedUnits.end();;
                }

                return isMatch;
            }
        };

        struct MissionPlotProximity
        {
            MissionPlotProximity(int maxStepDistance_, XYCoords targetCoords_) : maxStepDistance(maxStepDistance_), targetCoords(targetCoords_) {}

            bool operator() ( XYCoords coords) const
            {
                return stepDistance(coords.iX, coords.iY, targetCoords.iX, targetCoords.iY) <= maxStepDistance;
            }

            int maxStepDistance;
            XYCoords targetCoords;
        };
    }

    class MilitaryAnalysisImpl
    {
    public:
        enum UnitCategories
        {
            Unknown = -1, Settler = 0, Worker, Scout, LandCombat, Missionary, GreatPerson, Spy, Missile, SeaCombat, AirCombat
        };
        static const size_t CategoryCount = 9;

        struct UnitHistory
        {
            UnitHistory() {}
            UnitHistory(const IDInfo& unit_, XYCoords coords) : unit(unit_)
            {
                locationHistory.push_front(std::make_pair(gGlobals.getGame().getGameTurn(), coords));
            }

            void write(FDataStreamBase* pStream) const
            {
                unit.write(pStream);
                pStream->Write(locationHistory.size());
                for (std::list<std::pair<int, XYCoords> >::const_iterator ci(locationHistory.begin()), ciEnd(locationHistory.end()); ci != ciEnd; ++ci)
                {
                    pStream->Write(ci->first);
                    ci->second.write(pStream);
                }
            }

            void read(FDataStreamBase* pStream)
            {
                unit.read(pStream);
                locationHistory.clear();
                size_t count = 0;
                pStream->Read(&count);
                for (size_t i = 0; i < count; ++i)
                {
                    int turn;
                    XYCoords coords;
                    pStream->Read(&turn);
                    coords.read(pStream);
                    locationHistory.push_back(std::make_pair(turn, coords));
                }
            }

            IDInfo unit;
            std::list<std::pair<int, XYCoords> > locationHistory;
        };

        explicit MilitaryAnalysisImpl(Player& player)
            : player_(player), pCityDefenceAnalysis_(new CityDefenceAnalysis(player))
        {   
        }

        void update()
        {
            const int currentTurn = gGlobals.getGame().getGameTurn();
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nTurn = " << currentTurn << " " << __FUNCTION__;
#endif
            updateStacks_();
            
            std::vector<std::pair<IDInfo, XYCoords> > targetsToErase;
            // loop over all our missions - updating requirements
            // todo - sort mission list by priority
            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end();)
            {
                MissionAITypes missionAIType = (*missionsIter)->missionType;
                if (missionAIType == MISSIONAI_COUNTER || missionAIType == MISSIONAI_COUNTER_CITY || 
                    missionAIType == MISSIONAI_GUARD_CITY || missionAIType == MISSIONAI_RESERVE || missionAIType == MISSIONAI_EXPLORE)
                {
                    (*missionsIter)->resetDynamicData();

                    if (missionAIType == MISSIONAI_COUNTER || missionAIType == MISSIONAI_COUNTER_CITY)
                    {
                        int subAreaId = -1;
                        bool updateRequiredUnits = (*missionsIter)->assignedUnits.empty(), updateHostiles = false;
                        PlotUnitsMap targetStacks;

                        // update our reachable plots
                        (*missionsIter)->updateReachablePlots(player_, true, true, true);

                        for (std::set<IDInfo>::iterator targetsIter((*missionsIter)->targetUnits.begin()), targetsEndIter((*missionsIter)->targetUnits.end());
                            targetsIter != targetsEndIter;)
                        {
                            std::map<IDInfo, UnitHistory>::iterator hIter = unitHistories_.find(*targetsIter);
                            if (hIter != unitHistories_.end() && !hIter->second.locationHistory.empty())
                            {
                                int whenLastSeen = currentTurn - hIter->second.locationHistory.begin()->first;
                                updateHostiles = whenLastSeen == 0;

                                XYCoords whereLastSeen = hIter->second.locationHistory.begin()->second;
                                const CvPlot* pStackPlot = gGlobals.getMap().plot(whereLastSeen.iX, whereLastSeen.iY);
                                subAreaId = pStackPlot->getSubArea();

                                const CvUnit* pTargetUnit = ::getUnit(*targetsIter);
                                if (pTargetUnit)
                                {
                                    // may want to be smarter here to handle a partial update...?
                                    // as unit was seen in order to be in unitHistory, don't need to recheck for visibility
                                    targetStacks[pStackPlot].push_back(pTargetUnit);  
                                }

                                bool stackPlotIsCity = pStackPlot->isCity(); // presume we've seen the city if there is one
                                CombatGraph::Data endStates = getNearestCityAttackOdds(*missionsIter);
                                updateRequiredUnits = stackPlotIsCity || endStates.pWin > hostileAttackThreshold;  // indicates less than 80% chance of our units' survival
#ifdef ALTAI_DEBUG
                            
                                const CvPlot* pClosestCityPlot = player_.getAnalysis()->getMapAnalysis()->getClosestCity(pStackPlot, pStackPlot->getSubArea(), false);
                                os << "\nTarget: " << *targetsIter << " last seen " << whenLastSeen << " turn(s) ago at: " << whereLastSeen
                                    << " closest city: " << safeGetCityName(pClosestCityPlot->getPlotCity());
                                if (stackPlotIsCity) os << " target in city: " << safeGetCityName(pStackPlot->getPlotCity());
                                if (pClosestCityPlot)
                                {
                                    os << " distance = " << plotDistance(whereLastSeen.iX, whereLastSeen.iY, pClosestCityPlot->getX(), pClosestCityPlot->getY());
                                    endStates.debug(os);
                                }
#endif
                                if (pClosestCityPlot)
                                {
                                    (*missionsIter)->closestCity = pClosestCityPlot->getPlotCity()->getIDInfo();
                                }
                                
                                if (!stackPlotIsCity && whenLastSeen > 2 || stackPlotIsCity && whenLastSeen > 10)
                                {
#ifdef ALTAI_DEBUG
                                    os << "\n clearing hostile's reachable plots for mission: " << (unsigned long)(missionsIter->get());
#endif
                                    (*missionsIter)->hostilesReachablePlots.clear();

                                    if (whenLastSeen > 4)
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\nRemoving target from mission (" << *missionsIter << ") - target lost " << *targetsIter;
                                        if (hostileUnitsMissionMap_.find(*targetsIter) != hostileUnitsMissionMap_.end())
                                        {
                                            os << " erasing mission target from map " << *targetsIter;
                                        }
#endif
                                        targetsToErase.push_back(std::make_pair(*targetsIter, whereLastSeen));
                                    }
                                }
                            }
                            else
                            {
#ifdef ALTAI_DEBUG
                                os << "\nTarget unit for mission not found in history for mission: " << *missionsIter;
#endif
                                if (!(*missionsIter)->hostilesReachablePlots.empty())
                                {
#ifdef ALTAI_DEBUG
                                    os << "\n clearing hostile's reachable plots for mission: " << (unsigned long)(missionsIter->get());
#endif
                                    (*missionsIter)->hostilesReachablePlots.clear();
                                }

                            }
                            ++targetsIter;
                        }

                        const CvPlot* pTargetPlot = (*missionsIter)->getTargetPlot();
                        if (subAreaId == -1 && pTargetPlot)
                        {
                            subAreaId = pTargetPlot->getSubArea();
                        }

                        if (updateHostiles)  // update reachable plots
                        {
                            (*missionsIter)->hostilesReachablePlots.clear();
#ifdef ALTAI_DEBUG
                            os << "\n recalculating reachable plots for mission: " << *missionsIter;
#endif
                            for (PlotUnitsMap::const_iterator targetsIter(targetStacks.begin()), targetsEndIter(targetStacks.end()); targetsIter != targetsEndIter; ++targetsIter)
                            {
                                (*missionsIter)->updateReachablePlots(player_, false, true, true);
                            }
                        }

                        if (subAreaId != -1 && updateRequiredUnits)
                        {
                            std::set<IDInfo> availableUnits = getAssignedUnits(MISSIONAI_RESERVE, subAreaId);
                            std::vector<IDInfo> unitsToReassign = (*missionsIter)->updateRequiredUnits(player_, availableUnits);
                            for (size_t i = 0, count = unitsToReassign.size(); i < count; ++i)
                            {
                                reassignOurUnit_(::getUnit(unitsToReassign[i]), *missionsIter);
                            }
                        }
                    }
                }

                // temp hack to unlink escort missions for incompatible unit domains
                if (missionAIType == MISSIONAI_ESCORT_WORKER)
                {
                    if (!(*missionsIter)->targetUnits.empty())
                    {
                        const CvUnit* pTargetUnit = ::getUnit(*(*missionsIter)->targetUnits.begin());
                        if (pTargetUnit && pTargetUnit->getDomainType() != DOMAIN_LAND)
                        {
#ifdef ALTAI_DEBUG
                            os << "\nerasing escort mission with incompatible unit domains: ";                      
#endif
                            eraseMission_(*missionsIter++);
                            continue;
                        }
                    }
                }

#ifdef ALTAI_DEBUG
                (*missionsIter)->debug(os);
#endif
                ++missionsIter;
            }

            for (size_t i = 0, count = targetsToErase.size(); i < count; ++i)
            {
                // deletes history completely - may want to reconsider this
                deletePlayerUnit(targetsToErase[i].first, gGlobals.getMap().plot(targetsToErase[i].second));
            }

            attackCombatData_.clear();
            attackCombatMap_ = getAttackableUnitsMap_();
            for (std::map<XYCoords, CombatData>::const_iterator attIter(attackCombatMap_.begin()), attEndIter(attackCombatMap_.end()); attIter != attEndIter; ++attIter)
            {
                CombatGraph combatGraph = getCombatGraph(player_, attIter->second.combatDetails, attIter->second.attackers, attIter->second.defenders);
                combatGraph.analyseEndStates();

                std::set<MilitaryMissionDataPtr> updatedMissions;
                for (size_t i = 0, count = attIter->second.defenders.size(); i < count; ++i)
                {
                    std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = hostileUnitsMissionMap_.find(attIter->second.defenders[i].unitId);
                    FAssertMsg(missionIter != hostileUnitsMissionMap_.end(), "Missing hostile unit mission?");
                    // store which of our units hostile units can attack in hostiles' missions
                    /*for (size_t j = 0, count = attIter->second.attackers.size(); j < count; ++j)
                    {                        
                        missionIter->second->attackableUnits.push_back(attIter->second.attackers[j].unitId);
                    }*/

                    if (updatedMissions.find(missionIter->second) == updatedMissions.end())
                    {
                        missionIter->second->ourAttackOdds = combatGraph.endStatesData;
                        missionIter->second->ourAttackers = attIter->second.attackers;
                        if (!attIter->second.attackers.empty())
                        {
                            missionIter->second->firstAttacker = attIter->second.attackers[combatGraph.getFirstUnitIndex(true)].unitId;
                        }
                        updatedMissions.insert(missionIter->second);
                        break;
                    }
                }

                for (size_t i = 0, count = attIter->second.attackers.size(); i < count; ++i)
                {
                    // store which units we can attack in our units' missions
                    std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(attIter->second.attackers[i].unitId);
                    FAssertMsg(missionIter != ourUnitsMissionMap_.end(), "Missing our unit mission?");
                    for (size_t j = 0, count = attIter->second.defenders.size(); j < count; ++j)
                    {                        
                        missionIter->second->attackableUnits.insert(attIter->second.defenders[j].unitId);
                    }
                }
                attackCombatData_[attIter->first] = combatGraph.endStatesData;

#ifdef ALTAI_DEBUG
                os << " \npotential attack combat results for plot: " << attIter->first;
                combatGraph.endStatesData.debug(os);
                combatGraph.debugEndStates(os);
                std::list<IDInfo> attackOrder = combatGraph.getUnitOrdering(combatGraph.endStates.begin());
                size_t index = 0;
                for (std::list<IDInfo>::const_iterator attackOrderIter = attackOrder.begin(); attackOrderIter != attackOrder.end(); ++attackOrderIter)
                {
                    if (attackOrderIter == attackOrder.begin()) os << "\nAttack order: ";
                    else os << ", ";
                    const CvUnit* pUnit = ::getUnit(*attackOrderIter);
                    os << index++ << ": " << *attackOrderIter << pUnit->getUnitInfo().getType();
                }

                std::pair<std::list<IDInfo>, std::list<IDInfo> > longestAndShortestAttackOrder = combatGraph.getLongestAndShortestAttackOrder();

                for (std::list<IDInfo>::const_iterator attackOrderIter = longestAndShortestAttackOrder.first.begin(); attackOrderIter != longestAndShortestAttackOrder.first.end(); ++attackOrderIter)
                {
                    if (attackOrderIter == longestAndShortestAttackOrder.first.begin()) os << "\nLongest attack order: ";
                    else os << ", ";
                    const CvUnit* pUnit = ::getUnit(*attackOrderIter);
                    os << index++ << ": " << *attackOrderIter << pUnit->getUnitInfo().getType();
                }

                for (std::list<IDInfo>::const_iterator attackOrderIter = longestAndShortestAttackOrder.second.begin(); attackOrderIter != longestAndShortestAttackOrder.second.end(); ++attackOrderIter)
                {
                    if (attackOrderIter == longestAndShortestAttackOrder.second.begin()) os << "\nShortest attack order: ";
                    else os << ", ";
                    const CvUnit* pUnit = ::getUnit(*attackOrderIter);
                    os << index++ << ": " << *attackOrderIter << pUnit->getUnitInfo().getType();
                }
#endif
                //for (size_t defenderIndex = 0, defenderCount = attIter->second.defenders.size(); defenderIndex < defenderCount; ++defenderIndex)
                //{
                //    // store combat data v. each hostile
                //    hostilesCombatData[attIter->second.defenders[defenderIndex].unitId].attackData = std::make_pair(attIter->first, combatGraph.endStatesData);
                //}
            }

            defenceCombatData_.clear();
            defenceCombatMap_ = getAttackingUnitsMap_();
            std::list<std::pair<XYCoords, double> > plotSurvivalOdds;
            for (std::map<XYCoords, CombatData>::const_iterator defIter(defenceCombatMap_.begin()), defEndIter(defenceCombatMap_.end()); defIter != defEndIter; ++defIter)
            {
                CombatGraph combatGraph = getCombatGraph(player_, defIter->second.combatDetails, defIter->second.attackers, defIter->second.defenders);
                combatGraph.analyseEndStates();
                plotSurvivalOdds.push_back(std::make_pair(defIter->first, combatGraph.endStatesData.pLoss));  // we are the defenders; odds are wrt attackers

                std::set<MilitaryMissionDataPtr> updatedHostilesMissions, ourUpdatedMissions;
                for (size_t i = 0, count = defIter->second.attackers.size(); i < count; ++i)
                {
                    std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = hostileUnitsMissionMap_.find(defIter->second.attackers[i].unitId);
                    FAssertMsg(missionIter != hostileUnitsMissionMap_.end(), "Missing hostile unit mission?");
                    if (updatedHostilesMissions.find(missionIter->second) == updatedHostilesMissions.end())
                    {
                        // todo - check logic if missions's units end up spread over more than one plot and how we handle this in general
                        missionIter->second->hostileAttackOdds = combatGraph.endStatesData;
                        missionIter->second->ourDefenders = defIter->second.defenders;
                        updatedHostilesMissions.insert(missionIter->second);
                        break;
                    }
                }
                for (size_t i = 0, count = defIter->second.defenders.size(); i < count; ++i)
                {
                    std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(defIter->second.defenders[i].unitId);
                    FAssertMsg(missionIter != ourUnitsMissionMap_.end(), "Missing our unit mission?");
                    if (ourUpdatedMissions.find(missionIter->second) == ourUpdatedMissions.end())
                    {
                        // todo - check logic if missions's units end up spread over more than one plot and how we handle this in general
                        missionIter->second->ourAttackOdds = combatGraph.endStatesData;
                        missionIter->second->ourAttackers = defIter->second.attackers;
                        ourUpdatedMissions.insert(missionIter->second);
                        break;
                    }
                }
                defenceCombatData_[defIter->first] = combatGraph.endStatesData;

#ifdef ALTAI_DEBUG
                os << " \npotential defensive combat results for plot: " << defIter->first;
                combatGraph.endStatesData.debug(os);
                combatGraph.debugEndStates(os);
#endif
                /*for (size_t attackerIndex = 0, attackerCount = defIter->second.attackers.size(); attackerIndex < attackerCount; ++attackerIndex)
                {
                    std::map<IDInfo, CombatResultsData>::iterator hostilesCombatMapIter = hostilesCombatData.find(defIter->second.attackers[attackerIndex].unitId);
                    if (hostilesCombatMapIter == hostilesCombatData.end())
                    {
						hostilesCombatMapIter = hostilesCombatData.insert(std::make_pair(defIter->second.attackers[attackerIndex].unitId, CombatResultsData())).first;
                    }
                    hostilesCombatMapIter->second.defenceData.push_back(std::make_pair(defIter->first, combatGraph.endStatesData));
                }*/
            }

            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                if ((*missionsIter)->pUnitsMission)
                {
                    (*missionsIter)->pUnitsMission->update(player_);
                }
            }

            std::map<UnitCategories, std::set<IDInfo> >::const_iterator landCombatUnitsIter = units_.find(LandCombat);
            std::map<int, std::set<IDInfo> > unassignedUnitsBySubarea;
            if (landCombatUnitsIter != units_.end())
            {
                for (std::set<IDInfo>::const_iterator iter(landCombatUnitsIter->second.begin()), endIter(landCombatUnitsIter->second.end());
                    iter != endIter; ++iter)
                {
                    std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator missionIter = ourUnitsMissionMap_.find(*iter);
                    if (missionIter == ourUnitsMissionMap_.end())
                    {
                        const CvUnit* pUnit = ::getUnit(*iter);
                        if (pUnit)
                        {
                            unassignedUnitsBySubarea[pUnit->plot()->getSubArea()].insert(*iter);
                        }
                    }
                }
            }
#ifdef ALTAI_DEBUG
            for (std::map<int, std::set<IDInfo> >::const_iterator uIter(unassignedUnitsBySubarea.begin()), uEndIter(unassignedUnitsBySubarea.end());
                uIter != uEndIter; ++uIter)
            {
                os << "\nUnassigned units for area: " << uIter->first;
                debugUnitSet(os, uIter->second);
            }
#endif
        }

        void addOurUnit(CvUnitAI* pUnit, const CvUnit* pUpgradingUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nAdding our unit: " << pUnit->getIDInfo();
            if (pUpgradingUnit)
            {
                os << " upgrading from unit: " << pUpgradingUnit->getIDInfo();
            }
#endif
            UnitCategories unitCategory = addOurUnit_(pUnit);

            MilitaryMissionDataPtr pUnitMission;
            if (pUpgradingUnit)
            {
                pUnitMission = upgradeOurUnit_(pUnit, pUpgradingUnit);
            }

            if (!pUnitMission && pUnit->isFound())
            {
                // assign settler to an escort mission
                pUnitMission = addEscortUnitMission_(pUnit, NULL);                
            }

            if (!pUnitMission && (pUnit->AI_getUnitAIType() == UNITAI_WORKER || pUnit->AI_getUnitAIType() == UNITAI_WORKER_SEA))
            {
                pUnitMission = addWorkerUnitMission_(pUnit);
            }

            const CvPlot* pPlot = pUnit->plot();
            if (!pUnitMission && pPlot->isCity() && unitCategory == LandCombat)
            {
                // try our own city first
                pUnitMission = checkCityGuard_(pUnit, pPlot->getPlotCity());
                if (!pUnitMission)
                {
                    CityIterP<IsSubAreaP> cityIter(*player_.getCvPlayer(), IsSubAreaP(pUnit->plot()->getSubArea()));
                    while (CvCity* pLoopCity = cityIter())
                    {
                        if (pLoopCity == pPlot->getPlotCity())
                        {
                            continue;
                        }
                        // check other cities in this sub area
                        pUnitMission = checkCityGuard_(pUnit, pLoopCity);
                        if (pUnitMission)
                        {
                            break;
                        }
                    }
                }
            }

            if (!pUnitMission)
            {
                pUnitMission = assignUnitToCombatMission_(pUnit);
            }

            if (!pUnitMission)
            {
                // look for any escort missions on units at this plot, and join if required
                pUnitMission = checkEscort_(pUnit, pPlot);
            }

            if (!pUnitMission && pUnit->canFight())
            {
                boost::shared_ptr<MapAnalysis> pMapAnalysis = player_.getAnalysis()->getMapAnalysis();
                if (!pMapAnalysis->isAreaComplete(pPlot->getArea()))
                {
                    int unrevealedBorderPlotCount = pMapAnalysis->getUnrevealedBorderCount(pUnit->plot()->getSubArea());
                    int missionCount = getMissionCount(MISSIONAI_EXPLORE, pUnit->plot()->getSubArea());
                    if (missionCount < 1 + unrevealedBorderPlotCount / 20 || pUnit->getUnitInfo().getUnitAIType(UNITAI_EXPLORE))
                    {
#ifdef ALTAI_DEBUG
                        os << "\n assigning scout mission - current sub area mission count = " << missionCount << ", unrevealed border count: " << unrevealedBorderPlotCount;
#endif
                        pUnitMission = addScoutUnitMission_(pUnit);
                    }
                }
            }

            if (!pUnitMission && pUnit->canFight())
            {
                std::list<MilitaryMissionDataPtr> workerEscortMissions = getMatchingMissions(MISSIONAI_ESCORT_WORKER, pUnit->plot()->getSubArea());
                std::set<IDInfo> escortedWorkers, unescortedWorkers;
                for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(workerEscortMissions.begin()); missionsIter != workerEscortMissions.end(); ++missionsIter)
                {
                    escortedWorkers.insert((*missionsIter)->targetUnits.begin(), (*missionsIter)->targetUnits.end());
                }

                std::list<MilitaryMissionDataPtr> workerMissions = getMatchingMissions(MISSIONAI_BUILD, pUnit->plot()->getSubArea());
                boost::shared_ptr<WorkerAnalysis> pWorkerAnalysis = player_.getAnalysis()->getWorkerAnalysis();
                boost::shared_ptr<MapAnalysis> pMapAnalysis = player_.getAnalysis()->getMapAnalysis();
                for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(workerMissions.begin()); missionsIter != workerMissions.end(); ++missionsIter)
                {
                    for (std::set<IDInfo>::const_iterator aIter((*missionsIter)->assignedUnits.begin()), aEndIter((*missionsIter)->assignedUnits.end()); aIter != aEndIter; ++aIter)
                    {
                        if (escortedWorkers.find(*aIter) == escortedWorkers.end())
                        {
                            CvUnit* pWorkerUnit = ::getUnit(*aIter);
                            if (pWorkerUnit)
                            {
                                std::vector<Unit::WorkerMission> missions = pWorkerAnalysis->getWorkerMissions((CvUnitAI*)pWorkerUnit);
                                for (size_t i = 0, missionCount = missions.size(); i < missionCount; ++i)
                                {
                                    if (missions[i].isBorderMission(pMapAnalysis))
                                    {
                                        unescortedWorkers.insert(*aIter);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if (!unescortedWorkers.empty())
                {
                    // maybe pick closest instead?
                    const CvUnit* pWorkerUnit = ::getUnit(*unescortedWorkers.begin());
                    if (pWorkerUnit)
                    {
                        pUnitMission = addEscortWorkerUnitMission_(pUnit, (const CvUnitAI*)pWorkerUnit);
                    }
                }
            }

            if (!pUnitMission && pUnit->canFight() && pUnit->getDomainType() == DOMAIN_LAND)
            {
                const CvCity* pCapital = player_.getCvPlayer()->getCapitalCity();
                // might be initial unit assignment and we have no capital yet
                pUnitMission = addReserveLandUnitMission_(pUnit, pCapital ? pCapital->plot() : pUnit->plot());
            }

#ifdef ALTAI_DEBUG
            if (pUnitMission)
            {
                os << "\nAssigned unit to mission structure";
                pUnitMission->debug(os);
            }
            else
            {
                os << "\nFailed to assign unit: " << pUnit->getIDInfo() << " to mission";
            }
#endif
        }

        void deleteOurUnit(CvUnit* pUnit, const CvPlot* pPlot)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            if (pUnit->getDomainType() == DOMAIN_LAND)
            {
                IDInfo ourUnitInfo = pUnit->getIDInfo();
                if (pUnit->isFound())
                {
                    player_.getSettlerManager()->eraseUnit(ourUnitInfo);  // remove any entry from settler dest map
                }
                std::map<IDInfo, UnitCategories>::iterator categoryIter = unitCategoryMap_.find(ourUnitInfo);
                if (categoryIter != unitCategoryMap_.end())
                {
                    units_[categoryIter->second].erase(ourUnitInfo);
                    unitCategoryMap_.erase(categoryIter);
#ifdef ALTAI_DEBUG
                    os << "\nDeleted land unit from analysis: "
                       << gGlobals.getUnitInfo(pUnit->getUnitType()).getType()
                       << " with category: " << categoryIter->second;
#endif
                }
                else
                {
#ifdef ALTAI_DEBUG
                    os << "\nError deleting land unit from analysis: "
                       << gGlobals.getUnitInfo(pUnit->getUnitType()).getType();
#endif
                }

                std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(ourUnitInfo);
                if (missionIter != ourUnitsMissionMap_.end())
                {
                    std::set<IDInfo>::iterator assignedUnitsIter(missionIter->second->assignedUnits.find(ourUnitInfo));
                
                    if (assignedUnitsIter != missionIter->second->assignedUnits.end())
                    {
#ifdef ALTAI_DEBUG
                        os << "\nDeleting assigned unit: " << ourUnitInfo << " from mission";
#endif
                        missionIter->second->assignedUnits.erase(assignedUnitsIter);
                        missionIter->second->recalcOdds = true;

                        if (eraseDefenceCombatMapEntry_(pPlot->getCoords(), pUnit->getIDInfo(), false))
                        {
                            std::map<XYCoords, CombatData>::iterator defCombatIter = defenceCombatMap_.find(pPlot->getCoords());
                            if (defCombatIter != defenceCombatMap_.end())
                            {
                                CombatGraph combatGraph = getCombatGraph(player_, defCombatIter->second.combatDetails, defCombatIter->second.attackers, defCombatIter->second.defenders);
                                combatGraph.analyseEndStates();
                                missionIter->second->hostileAttackOdds = combatGraph.endStatesData;
                                defenceCombatData_[pPlot->getCoords()] = combatGraph.endStatesData;
                            }
                        }

                        if (pUnit->isMadeAttack())
                        {
                            // annoyingly the attack plot is gone from the unit's data by this stage, so we don't know where we attacked
                            // but, since we want to update the combat odds for all the possible attacks this unit could have made, 
                            // check all missions's attackable units (should prob filter by this unit's formerly reachable plots)
                            std::set<XYCoords> attackablePlots;
                            for (std::set<IDInfo>::const_iterator attackableUnitsIter(missionIter->second->attackableUnits.begin()), attackableUnitsEndIter(missionIter->second->attackableUnits.end());
                                attackableUnitsIter != attackableUnitsEndIter; ++attackableUnitsIter)
                            {
                                const CvUnit* pAttackableUnit = ::getUnit(*attackableUnitsIter);
                                if (pAttackableUnit && pAttackableUnit->canFight())
                                {
                                    attackablePlots.insert(pAttackableUnit->plot()->getCoords());
                                }
                            }

                            for (std::set<XYCoords>::const_iterator attackCoordsIter(attackablePlots.begin()), attackCoordsEndIter(attackablePlots.end());
                                attackCoordsIter != attackCoordsEndIter; ++attackCoordsIter)
                            {
                                if (eraseAttackCombatMapEntry_(*attackCoordsIter, pUnit->getIDInfo(), true))
                                {
                                    std::map<XYCoords, CombatData>::iterator attCombatIter(attackCombatMap_.find(*attackCoordsIter));
                                    if (attCombatIter != attackCombatMap_.end())
                                    {
                                        updateUnitData(attCombatIter->second.defenders);
                                        CombatGraph combatGraph = getCombatGraph(player_, attCombatIter->second.combatDetails, attCombatIter->second.attackers, attCombatIter->second.defenders);
                                        combatGraph.analyseEndStates();
                                        attackCombatData_[*attackCoordsIter] = combatGraph.endStatesData;
                                    }
                                }
                            }
                        }

                        ourUnitsMissionMap_.erase(ourUnitInfo);

                        if (missionIter->second->missionType == MISSIONAI_ESCORT)
                        {
                            if (pUnit->isFound())  // assume only one settler per mission
                            {
                                eraseMission_(missionIter->second);                                
                            }
                            else
                            {
                                for (std::set<IDInfo>::const_iterator missionUnitsIter(missionIter->second->assignedUnits.begin()); missionUnitsIter != missionIter->second->assignedUnits.end(); ++missionUnitsIter)
                                {
                                    CvUnit* pMissionUnit = player_.getCvPlayer()->getUnit(missionUnitsIter->iID);
                                    if (pMissionUnit && pMissionUnit->isFound())
                                    {
                                        addEscortUnitMission_(pMissionUnit, NULL);  // try and find a new escort -  will update required units
                                        break;
                                    }
                                }
                            }
                        }
                        else if (missionIter->second->missionType == MISSIONAI_COUNTER || missionIter->second->missionType == MISSIONAI_COUNTER_CITY)
                        {
                            std::set<IDInfo> availableUnits = getAssignedUnits(MISSIONAI_RESERVE, pPlot->getSubArea());
                            std::vector<IDInfo> unitsToReassign = missionIter->second->updateRequiredUnits(player_, availableUnits);
                            for (size_t i = 0, count = unitsToReassign.size(); i < count; ++i)
                            {
                                reassignOurUnit_(::getUnit(unitsToReassign[i]), missionIter->second);
                            }
                        }
                        else if (missionIter->second->missionType == MISSIONAI_GUARD_CITY)
                        {
                            if (!isEmpty(missionIter->second->targetCity))
                            {
                                // reset required units (todo - check this is ok if city is also being captured due to loss of this unit
                                missionIter->second->requiredUnits = pCityDefenceAnalysis_->getRequiredUnits(missionIter->second->targetCity);
                            }
                        }
                    }
                }

                eraseUnit_(ourUnitInfo, pPlot);  // don't use unit plot fn as it's set to NULL in unit deletion
            }
        }

        void withdrawOurUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            if (pUnit->getDomainType() == DOMAIN_LAND)
            {
                if (eraseAttackCombatMapEntry_(pAttackPlot->getCoords(), pUnit->getIDInfo(), true))
                {
                    std::map<XYCoords, CombatData>::iterator attCombatIter(attackCombatMap_.find(pAttackPlot->getCoords()));
                    // same as code in deleteOurUnit - todo merge...
                    if (attCombatIter != attackCombatMap_.end())
                    {
                        updateUnitData(attCombatIter->second.defenders);
                        CombatGraph combatGraph = getCombatGraph(player_, attCombatIter->second.combatDetails, attCombatIter->second.attackers, attCombatIter->second.defenders);
                        combatGraph.analyseEndStates();
                        attackCombatData_[pAttackPlot->getCoords()] = combatGraph.endStatesData;
                    }
                }
                // need to update our defenceCombatMap, but not erase the withdrawing unit...
            }
        }

        void addPlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            XYCoords lastKnownCoords = unhideUnit_(pUnit->getIDInfo(), pPlot->getSubArea());
            if (lastKnownCoords != XYCoords(-1, -1))
            {
#ifdef ALTAI_DEBUG
                os << " unit previously seen at: " << lastKnownCoords;
#endif
            }

            XYCoords previousCoords = addUnit_(pUnit->getIDInfo(), pPlot);
            if (previousCoords != lastKnownCoords)
            {
#ifdef ALTAI_DEBUG
                os << " but unit also previously seen at: " << previousCoords;
#endif
            }

            if (CvTeamAI::getTeam(player_.getTeamID()).isAtWar(pUnit->getTeam()))
            {
                addHostileUnitMission_(pPlot->getSubArea(), pPlot->getCoords(), pUnit->getIDInfo());
            }
        }

        void deletePlayerUnit(const IDInfo& unit, const CvPlot* pPlot)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nErasing player unit: " << unit;
#endif      
            if (CvTeamAI::getTeam(player_.getTeamID()).isAtWar(PlayerIDToTeamID(unit.eOwner)))
            {
                // erase unit from attackCombatMap_ (if present)
                if (eraseAttackCombatMapEntry_(pPlot->getCoords(), unit, false))
                {
                    std::map<XYCoords, CombatData>::iterator attCombatIter(attackCombatMap_.find(pPlot->getCoords()));
                    if (attCombatIter != attackCombatMap_.end())
                    {                        
                        // todo - store last attacker/defender (although have to deal with units that create collateral damage)
                        // also - did we kill this unit? - if so - we need to remove the attacking unit unless it's blitz capable
                        updateUnitData(attCombatIter->second.attackers);
                        CombatGraph combatGraph = getCombatGraph(player_, attCombatIter->second.combatDetails, attCombatIter->second.attackers, attCombatIter->second.defenders);
                        combatGraph.analyseEndStates();
                        attackCombatData_[pPlot->getCoords()] = combatGraph.endStatesData;
                    }
                }

                std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionDataIter = hostileUnitsMissionMap_.find(unit);
            
                if (missionDataIter != hostileUnitsMissionMap_.end())
                {
                    // erase from list of attackable units in mission
                    missionDataIter->second->attackableUnits.erase(unit);

                    // erase entry in defenceCombatMap_ for any plots this unit could have attacked
                    for (PlotSet::iterator reachableHostilePlotsIter(missionDataIter->second->hostilesReachablePlots.begin()),
                        reachableHostilePlotsEndIter(missionDataIter->second->hostilesReachablePlots.end()); 
                        reachableHostilePlotsIter != reachableHostilePlotsEndIter; ++reachableHostilePlotsIter)
                    {
                        if (eraseDefenceCombatMapEntry_((*reachableHostilePlotsIter)->getCoords(), unit, true))
                        {
                            std::map<XYCoords, CombatData>::iterator defCombatIter(defenceCombatMap_.find((*reachableHostilePlotsIter)->getCoords()));
                            if (defCombatIter != defenceCombatMap_.end())
                            {                        
                                updateUnitData(defCombatIter->second.defenders);
                                CombatGraph combatGraph = getCombatGraph(player_, defCombatIter->second.combatDetails, defCombatIter->second.attackers, defCombatIter->second.defenders);
                                combatGraph.analyseEndStates();
                                defenceCombatData_[(*reachableHostilePlotsIter)->getCoords()] = combatGraph.endStatesData;
                            }
                        }
                    }

                    if (!isEmpty(missionDataIter->second->pushedAttack.first))
                    {
                        // this data tells us which unit we attacked with
                        // use it to remove the attacker from other combats
                        // (todo - unless it's blitz in which case update its hitpoints)

                        if (eraseAttackCombatMapEntry_(pPlot->getCoords(), missionDataIter->second->pushedAttack.first, true))
                        {
                            std::map<XYCoords, CombatData>::iterator attCombatIter(attackCombatMap_.find(pPlot->getCoords()));
                            if (attCombatIter != attackCombatMap_.end())
                            {
                                CombatGraph combatGraph = getCombatGraph(player_, attCombatIter->second.combatDetails, attCombatIter->second.attackers, attCombatIter->second.defenders);
                                combatGraph.analyseEndStates();
                                attackCombatData_[pPlot->getCoords()] = combatGraph.endStatesData;
                            }
                        }

                        std::set<XYCoords> attackablePlots;
                        for (std::set<IDInfo>::const_iterator attackableUnitsIter(missionDataIter->second->attackableUnits.begin()), attackableUnitsEndIter(missionDataIter->second->attackableUnits.end());
                            attackableUnitsIter != attackableUnitsEndIter; ++attackableUnitsIter)
                        {
                            const CvUnit* pAttackableUnit = ::getUnit(*attackableUnitsIter);
                            if (pAttackableUnit && pAttackableUnit->canFight())
                            {
                                attackablePlots.insert(pAttackableUnit->plot()->getCoords());
                            }
                        }

                        for (std::set<XYCoords>::const_iterator attackCoordsIter(attackablePlots.begin()), attackCoordsEndIter(attackablePlots.end());
                            attackCoordsIter != attackCoordsEndIter; ++attackCoordsIter)
                        {
                            if (eraseAttackCombatMapEntry_(*attackCoordsIter, missionDataIter->second->pushedAttack.first, true))
                            {
                                std::map<XYCoords, CombatData>::iterator attCombatIter(attackCombatMap_.find(*attackCoordsIter));
                                if (attCombatIter != attackCombatMap_.end())
                                {
                                    updateUnitData(attCombatIter->second.defenders);
                                    CombatGraph combatGraph = getCombatGraph(player_, attCombatIter->second.combatDetails, attCombatIter->second.attackers, attCombatIter->second.defenders);
                                    combatGraph.analyseEndStates();
                                    attackCombatData_[*attackCoordsIter] = combatGraph.endStatesData;
                                }
                            }
                        }                        
                    }
                    
                    missionDataIter->second->targetUnits.erase(unit);
                    if (missionDataIter->second->targetUnits.empty())
                    {
#ifdef ALTAI_DEBUG
                        os << " erasing mission from map for unit: " << unit;
#endif
                        eraseMission_(missionDataIter->second);
                    }
#ifdef ALTAI_DEBUG
                    else
                    {
                        os << " remaining targets count = " << missionDataIter->second->targetUnits.size();
                        missionDataIter->second->debug(os);
                    }
#endif
                    hostileUnitsMissionMap_.erase(missionDataIter);
                    std::map<XYCoords, std::list<IDInfo> >::iterator enemyStackIter = enemyStacks_.find(pPlot->getCoords());
                    if (enemyStackIter != enemyStacks_.end())
                    {
                        enemyStackIter->second.remove(unit);
                    }
                }
#ifdef ALTAI_DEBUG
                else
                {
                    os << " mission missing from hostile unit map for: " << unit;
                }
#endif
            }

            // removes unit from unitsMap_ and unitStacks_
            eraseUnit_(unit, pPlot);
            unitHistories_.erase(unit);
        }

        void withdrawPlayerUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot)
        {
        }

        void movePlayerUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot)
        {
            if (pFromPlot)
            {
                eraseUnit_(pUnit->getIDInfo(), pFromPlot);
            }
            addUnit_(pUnit->getIDInfo(), pToPlot);

            if (CvTeamAI::getTeam(player_.getTeamID()).isAtWar(pUnit->getTeam()))  // hostile unit
            {
                addHostileUnitMission_(pToPlot->getSubArea(), pToPlot->getCoords(), pUnit->getIDInfo());
                // update attackCombatMap_ - hostile unit can no longer be attacked at the 'from' plot
                if (pFromPlot)
                {
                    if (eraseAttackCombatMapEntry_(pFromPlot->getCoords(), pUnit->getIDInfo(), false))
                    {
                        std::map<XYCoords, CombatData>::iterator attCombatIter(attackCombatMap_.find(pFromPlot->getCoords()));
                        if (attCombatIter != attackCombatMap_.end())
                        {
                            updateUnitData(attCombatIter->second.defenders);
                            CombatGraph combatGraph = getCombatGraph(player_, attCombatIter->second.combatDetails, attCombatIter->second.attackers, attCombatIter->second.defenders);
                            combatGraph.analyseEndStates();
                            attackCombatData_[pFromPlot->getCoords()] = combatGraph.endStatesData;
                        }
                    }
                }
            }
            else if (pUnit->getOwner() == player_.getPlayerID())  // our unit
            {
                // update reachable plots + combat map
                std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(pUnit->getIDInfo());
                FAssertMsg(missionIter != ourUnitsMissionMap_.end(), "Missing our unit mission?");
                if (missionIter != ourUnitsMissionMap_.end())  // todo - add missionaries and other stuff
                {
                    if (pUnit->canFight())
                    {
                        missionIter->second->updateReachablePlots(player_, true, false, true);

                        // update defenceCombatMap_ - remove from any entry for pFromPlot...
                        if (eraseDefenceCombatMapEntry_(pFromPlot->getCoords(), pUnit->getIDInfo(), false))
                        {
                            std::map<XYCoords, CombatData>::iterator defCombatIter(defenceCombatMap_.find(pFromPlot->getCoords()));
                            if (defCombatIter != defenceCombatMap_.end())
                            {
                                // no need to update defender's hp as it's not at this plot anymore
                                CombatGraph combatGraph = getCombatGraph(player_, defCombatIter->second.combatDetails, defCombatIter->second.attackers, defCombatIter->second.defenders);
                                combatGraph.analyseEndStates();
                                defenceCombatData_[pFromPlot->getCoords()] = combatGraph.endStatesData;
                            }
                        }

                        // ...and add to any entry for pToPlot
                        std::map<XYCoords, CombatData>::iterator defCombatIter(defenceCombatMap_.find(pToPlot->getCoords()));
                        if (defCombatIter != defenceCombatMap_.end())
                        {
                            defCombatIter->second.defenders.push_back(UnitData(pUnit));
                            CombatGraph combatGraph = getCombatGraph(player_, defCombatIter->second.combatDetails, defCombatIter->second.attackers, defCombatIter->second.defenders);
                            combatGraph.analyseEndStates();
                            defenceCombatData_[pToPlot->getCoords()] = combatGraph.endStatesData;
                        }
                        else
                        {
                            // todo - may want to add a new entry if the dest plot is reachable by hostiles
                        }

                        if (eraseAttackCombatMapEntry_(pFromPlot->getCoords(), pUnit->getIDInfo(), true))
                        {
                            std::map<XYCoords, CombatData>::iterator attCombatIter(attackCombatMap_.find(pFromPlot->getCoords()));
                            if (attCombatIter != attackCombatMap_.end())
                            {
                                // no need to update attacker's hp as it's not at this plot anymore
                                CombatGraph combatGraph = getCombatGraph(player_, attCombatIter->second.combatDetails, attCombatIter->second.attackers, attCombatIter->second.defenders);
                                combatGraph.analyseEndStates();
                                attackCombatData_[pFromPlot->getCoords()] = combatGraph.endStatesData;
                            }
                        }

                        // if we can still attack, update attackCombatMap_
                        if (pUnit->movesLeft() > 0 && (!pUnit->isMadeAttack() || pUnit->isBlitz()))
                        {
                            std::map<XYCoords, CombatData>::iterator attCombatIter(attackCombatMap_.find(pToPlot->getCoords()));
                            if (attCombatIter != attackCombatMap_.end())
                            {
                                attCombatIter->second.attackers.push_back(UnitData(pUnit));

                                CombatGraph combatGraph = getCombatGraph(player_, attCombatIter->second.combatDetails, attCombatIter->second.attackers, attCombatIter->second.defenders);
                                combatGraph.analyseEndStates();
                                attackCombatData_[pToPlot->getCoords()] = combatGraph.endStatesData;
                            }
                            else
                            {
                                // todo - may want to add a new entry if the dest plot is reachable by hostiles
                            }
                        }
                    }
                }
            }
        }

        void hidePlayerUnit(CvUnitAI* pUnit, const CvPlot* pOldPlot, bool moved)
        {
            // todo - treat differently from unit deletion? - add to list of untracked units?
            eraseUnit_(pUnit->getIDInfo(), pOldPlot);
            hideUnit_(pUnit->getIDInfo(), pOldPlot);
        }

        void addOurCity(const CvCity* pCity)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            pCityDefenceAnalysis_->addOurCity(pCity->getIDInfo());

            std::map<IDInfo, MilitaryMissionDataPtr>::iterator cityGuardMissionIter = cityGuardMissionsMap_.find(pCity->getIDInfo());
            FAssertMsg(cityGuardMissionIter == cityGuardMissionsMap_.end(), "adding city with existing guard mission?");
            cityGuardMissionIter = cityGuardMissionsMap_.insert(std::make_pair(pCity->getIDInfo(), addCityGuardMission_(pCity))).first;

            bool isCapture = pCity->getPreviousOwner() != NO_PLAYER;

            std::list<IDInfo> potentialGuards;
            const CvPlot* pCityPlot = pCity->plot();

            UnitPlotIterP<OurUnitsPred> ourUnitsIter(pCity->plot(), OurUnitsPred(player_.getPlayerID()));

            while (CvUnit* pPlotUnit = ourUnitsIter())
            {
                if (pPlotUnit->canFight())
                {
                    if (pPlotUnit->isOnlyDefensive())
                    {
                        MilitaryMissionDataPtr pPossibleDefenderCurrentMission = getMissionData((CvUnitAI*)pPlotUnit);
                        if (pPossibleDefenderCurrentMission->missionType == MISSIONAI_EXPLORE)
                        {
#ifdef ALTAI_DEBUG
                            os << "\n\t skipping potential defender as can only defend and set to explore mission. " << pPlotUnit->getIDInfo();
#endif
                            continue;
                        }
                    }
#ifdef ALTAI_DEBUG
                    os << "\n\t adding potential guard unit at city plot: " << pPlotUnit->getIDInfo();
#endif
                    potentialGuards.push_back(pPlotUnit->getIDInfo());
                }
            }

            if (potentialGuards.empty())
            {
                NeighbourPlotIter plotIter(pCityPlot);
                while (IterPlot pLoopPlot = plotIter())
                {
                    if (pLoopPlot.valid())
                    {
                        UnitPlotIterP<OurUnitsPred> ourUnitsIter(pLoopPlot, OurUnitsPred(player_.getPlayerID()));
                        while (CvUnit* pPlotUnit = ourUnitsIter())
                        {
                            if (pPlotUnit->canFight())
                            {
                                if (pPlotUnit->isOnlyDefensive())
                                {
                                    MilitaryMissionDataPtr pPossibleDefenderCurrentMission = getMissionData((CvUnitAI*)pPlotUnit);
                                    if (pPossibleDefenderCurrentMission->missionType == MISSIONAI_EXPLORE)
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\n\t skipping potential defender as can only defend and on explore mission. " << pPlotUnit->getIDInfo();
#endif
                                        continue;
                                    }
                                }
#ifdef ALTAI_DEBUG
                                os << "\n\t adding potential guard unit at neighbouring plot: " << pPlotUnit->getIDInfo();
#endif
                                potentialGuards.push_back(pPlotUnit->getIDInfo());
                            }
                        }
                    }
                }
            }

            if (potentialGuards.empty())
            {
                std::set<IDInfo> reserveUnits = getAssignedUnits(MISSIONAI_RESERVE, pCity->plot()->getSubArea());
                std::copy(reserveUnits.begin(), reserveUnits.end(), std::back_inserter(potentialGuards));
            }

            UnitTypes bestGuardUnit = NO_UNIT;
            {
                DependencyItemSet di;
                di.insert(DependencyItem(-1, -1));
                const TacticSelectionData& tacticData = player_.getAnalysis()->getPlayerTactics()->tacticSelectionDataMap[di];
                std::set<UnitTacticValue>::const_iterator tacticIter = TacticSelectionData::getBestUnitTactic(tacticData.cityDefenceUnits);
                if (tacticIter != tacticData.cityDefenceUnits.end())
                {
                    bestGuardUnit = tacticIter->unitType;
                }
            }
#ifdef ALTAI_DEBUG
            os << "\nCity: " << safeGetCityName(pCity) << " has " << potentialGuards.size() << " potential guard units"
                << " is capture = " << isCapture << " best guard unit : " << (bestGuardUnit == NO_UNIT ? "none" : gGlobals.getUnitInfo(bestGuardUnit).getType());
#endif
            // todo - pick best guard out of set
            for (std::list<IDInfo>::iterator iter(potentialGuards.begin()); iter != potentialGuards.end(); ++iter)
            {
                MilitaryMissionDataPtr pMission = checkCityGuard_((CvUnitAI*)::getUnit(*iter), pCity);
                if (pMission)
                {
#ifdef ALTAI_DEBUG
                    os << "\n city guard mission unit assigned. ";
                    pMission->debug(os);
#endif
                    break;
                }
            }

            if (!cityGuardMissionIter->second->requiredUnits.empty())
            {
                for (std::list<IDInfo>::iterator iter(potentialGuards.begin()); iter != potentialGuards.end(); ++iter)
                {
                    CvUnit* pPotentialGuardUnit = ::getUnit(*iter);
                    if (pPotentialGuardUnit && pPotentialGuardUnit->isMilitaryHappiness())
                    {
                        reassignOurUnit_(pPotentialGuardUnit, cityGuardMissionIter->second);
#ifdef ALTAI_DEBUG
                        os << "\n city guard mission military happiness unit assigned. ";
                        cityGuardMissionIter->second->debug(os);
#endif
                    }
                }
            }
        }

        void deleteCity(const CvCity* pCity)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nDeleting city: " << safeGetCityName(pCity);
#endif
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = cityGuardMissionsMap_.find(pCity->getIDInfo());
            FAssertMsg(missionIter != cityGuardMissionsMap_.end(), "Missing city guard mission?");
            if (missionIter != cityGuardMissionsMap_.end())
            {
                eraseMission_(missionIter->second);
                cityGuardMissionsMap_.erase(missionIter);
            }
            pCityDefenceAnalysis_->deleteOurCity(pCity->getIDInfo());
        }

        void addPlayerCity(const CvCity* pCity)
        {
            if (!CvTeamAI::getTeam(player_.getTeamID()).isAtWar(PlayerIDToTeamID(pCity->getOwner())))
            {
                return;
            }
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            MissionFinder finder;
            finder.missionType = MISSIONAI_COUNTER_CITY;
            finder.city = pCity->getIDInfo();
            std::list<MilitaryMissionDataPtr>::const_iterator missionsIter = std::find_if(missionList_.begin(), missionList_.end(), finder);
            if (missionsIter == missionList_.end())
            {
#ifdef ALTAI_DEBUG
                os << "\nadded new mission to target city: " << safeGetCityName(pCity);
#endif
                MilitaryMissionDataPtr pMission = addMission_(MISSIONAI_COUNTER_CITY);
                pMission->targetCity = pCity->getIDInfo();
            }
        }

        void deletePlayerCity(const CvCity* pCity)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            // todo (remove mission against city possibly?)
        }

        void addHostilePlotsWithUnknownCity(const std::vector<XYCoords>& coords)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            std::list<XYCoords> remainingCoords;
            std::copy(coords.begin(), coords.end(), std::back_inserter(remainingCoords));

            for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(missionList_.begin()), missionsEndIter(missionList_.end());
                missionsIter != missionsEndIter && !remainingCoords.empty(); ++missionsIter)
            {
                if ((*missionsIter)->missionType == MISSIONAI_COUNTER)
                {
                    const CvPlot* pTargetPlot = (*missionsIter)->getTargetPlot();
                    if (pTargetPlot)
                    {
                        std::list<XYCoords>::iterator rcIter = std::find_if(remainingCoords.begin(), remainingCoords.end(),
                            MissionPlotProximity(4, (*missionsIter)->getTargetPlot()->getCoords()));
                        //std::list<XYCoords>::iterator rcIter = std::find(remainingCoords.begin(), remainingCoords.end(), pTargetPlot->getCoords());
                        if (rcIter != remainingCoords.end())
                        {
                            remainingCoords.remove_if(MissionPlotProximity(4, (*missionsIter)->getTargetPlot()->getCoords()));
                        }
                    }
                }
            }

            if (!remainingCoords.empty())
            {
#ifdef ALTAI_DEBUG
                os << "\nadded new mission to target hostile coords: " << *remainingCoords.begin();
#endif
                MilitaryMissionDataPtr pMission = addMission_(MISSIONAI_COUNTER);
                pMission->targetCoords = *remainingCoords.begin();
            }
        }

        void addNewSubArea(int subArea)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            boost::shared_ptr<SubArea> pSubArea = gGlobals.getMap().getSubArea(subArea);
            if (!pSubArea->isImpassable())
            {
                FAssertMsg(subAreaResourceMissionsMap_.find(subArea) == subAreaResourceMissionsMap_.end(), "Expected not to find existing mission for sub area");
                MilitaryMissionDataPtr pMission = addMission_(MISSIONAI_GUARD_BONUS);
                pMission->pUnitsMission = IUnitsMissionPtr(new GuardBonusesMission(subArea));
#ifdef ALTAI_DEBUG
                os << "\nAdded new guard bonus mission for sub area: " << subArea;
#endif
                subAreaResourceMissionsMap_[subArea] = pMission;
            }
        }

        bool updateOurUnit(CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            CvPlot* pUnitPlot = pUnit->plot();

            for (std::map<XYCoords, std::list<IDInfo> >::const_iterator iter(enemyStacks_.begin()), endIter(enemyStacks_.end()); iter != endIter; ++iter)
            {
                const CvPlot* stackPlot = gGlobals.getMap().plot(iter->first.iX, iter->first.iY);
                if (stackPlot->getSubArea() != pUnitPlot->getSubArea())
                {
                    continue;
                }
                else if (stepDistance(pUnit->getX(), pUnit->getY(), stackPlot->getX(), stackPlot->getY()) < 4)
                {
#ifdef ALTAI_DEBUG
                    os << "\nOur unit at: " << pUnitPlot->getCoords() << " ai = " << getUnitAIString(pUnit->AI_getUnitAIType()) << ", near stack at: " << stackPlot->getCoords()
                        << " step dist = " << stepDistance(pUnit->getX(), pUnit->getY(), stackPlot->getX(), stackPlot->getY());
#endif
                }
            }

            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(pUnit->getIDInfo());
            if (missionIter != ourUnitsMissionMap_.end())
            {   
#ifdef ALTAI_DEBUG
                os << "\nUpdating unit: " << pUnit->getID() << " with assigned mission: (" << missionIter->second << ") ";
                missionIter->second->debug(os);
#endif
                if (!missionIter->second->specialTargets.empty())
                {
#ifdef ALTAI_DEBUG
                    os << "\nChecking special targets: ";
                    debugUnitSet(os, missionIter->second->specialTargets);
#endif
                    std::set<IDInfo>::iterator targetIter = missionIter->second->specialTargets.begin();
                    const CvUnit* pEnemyUnit = ::getUnit(*targetIter);
                    // erase regardless if whether unit exists still (and whether we manage to kill it if it does)
                    // will be re-added in getNextAttack unit if this mission has more available attack units
                    missionIter->second->specialTargets.erase(targetIter);
                    if (pEnemyUnit)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " pushed adhoc attack mission special for unit: " << pEnemyUnit->getIDInfo();
#endif
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pEnemyUnit->getX(), pEnemyUnit->getY(), MOVE_IGNORE_DANGER, false, false, 
                            pEnemyUnit->plot()->isCity() ? MISSIONAI_COUNTER_CITY : MISSIONAI_COUNTER, (CvPlot*)pEnemyUnit->plot(), 0, __FUNCTION__);
                        return true;
                    }
                }

                if (missionIter->second->missionType == MISSIONAI_BUILD)
                {
                    bool hasWorkerMission = doWorkerMove(player_, pUnit);
                    if (!hasWorkerMission && pUnit->getDomainType() == DOMAIN_SEA)
                    {
                        return AltAI::doCoastalUnitExplore(pUnit);
                    }
                    else
                    {
                        return hasWorkerMission;
                    }
                }
                else if (missionIter->second->missionType == MISSIONAI_EXPLORE)
		        {
                    if (pUnit->getDomainType() == DOMAIN_LAND && AltAI::doLandUnitExplore(pUnit))
                    {
                        return true;
                    }
                    else if (pUnit->getDomainType() == DOMAIN_SEA && AltAI::doCoastalUnitExplore(pUnit))
                    {
                        return true;
                    }
		        }
                // todo - if unit is assigned to a new mission - want to go round this loop again
                else if (missionIter->second->missionType == MISSIONAI_ESCORT)
                {   
                    return doUnitEscortMission(missionIter->second, pUnit);
                }
                else if (missionIter->second->missionType == MISSIONAI_ESCORT_WORKER)
                {   
                    return doUnitEscortWorkerMission(missionIter->second, pUnit);
                }
                else if (missionIter->second->missionType == MISSIONAI_GUARD_CITY)
                {
                    return doUnitCityDefendMission(missionIter->second, pUnit);
                }
                else if (missionIter->second->missionType == MISSIONAI_COUNTER || missionIter->second->missionType == MISSIONAI_COUNTER_CITY)
                {
                    return doUnitCounterMission(missionIter->second, pUnit);
                }
                else if (missionIter->second->missionType == MISSIONAI_RESERVE)
                {
                    return doUnitReserveMission(missionIter->second, pUnit);
                }
            }
            else  // no assigned mission
            {
            }
            return false;
        }

        PromotionTypes promoteUnit(CvUnitAI* pUnit)
        {            
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            PromotionTypes chosenPromotion = NO_PROMOTION;
            const bool isDamaged = pUnit->getDamage() > 0, newUnit = pUnit->getGameTurnCreated() == gGlobals.getGame().getGameTurn();

            std::vector<PromotionTypes> availablePromotions = getAvailablePromotions(pUnit);
#ifdef ALTAI_DEBUG
            if (!availablePromotions.empty())
            {
                os << "\navailable promotions for unit:";
                debugUnit(os, pUnit);
                for (size_t i = 0, count = availablePromotions.size(); i < count; ++i)
                {
                    if (i == 0) os << "\t"; else os << ", ";
                    os << gGlobals.getPromotionInfo(availablePromotions[i]).getType();
                }
            }
#endif

            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(pUnit->getIDInfo());
            if (missionIter != ourUnitsMissionMap_.end())
            {
                if (missionIter->second->missionType == MISSIONAI_GUARD_CITY)
                {
                    UnitAnalysis::RemainingLevelsAndPromotions cityGuardPromotions = player_.getAnalysis()->getUnitAnalysis()->getCityDefencePromotions(pUnit->getUnitType(), pUnit->getLevel());
                    for (size_t i = 0, count = availablePromotions.size(); i < count; ++i)
                    {
                        if (cityGuardPromotions.second.find(availablePromotions[i]) != cityGuardPromotions.second.end())
                        {
                            chosenPromotion = availablePromotions[i];
                            break;
                        }
                    }
                }
                else // if (missionIter->second->missionType == MISSIONAI_COUNTER/MISSIONAI_COUNTER_CITY)
                {
                    UnitAnalysis::RemainingLevelsAndPromotions combatPromotions = player_.getAnalysis()->getUnitAnalysis()->getCombatPromotions(pUnit->getUnitType(), pUnit->getLevel());
                    for (size_t i = 0, count = availablePromotions.size(); i < count; ++i)
                    {
                        if (combatPromotions.second.find(availablePromotions[i]) != combatPromotions.second.end())
                        {
                            chosenPromotion = availablePromotions[i];
                            break;
                        }
                    }
                }
            }

#ifdef ALTAI_DEBUG
            if (chosenPromotion != NO_PROMOTION)
            {
                os << " chosen promotion = " << gGlobals.getPromotionInfo((PromotionTypes)chosenPromotion).getType();
            }
#endif
            return chosenPromotion;
        }

        MilitaryMissionDataPtr getMissionData(CvUnitAI* pUnit)
        {
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(pUnit->getIDInfo());
            if (missionIter != ourUnitsMissionMap_.end())
            {
                return missionIter->second;
            }
            else
            {
                return MilitaryMissionDataPtr();
            }
        }

        std::pair<IDInfo, UnitTypes> getPriorityUnitBuild(IDInfo city)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            std::pair<IDInfo, UnitTypes> cityAndUnit(IDInfo(), NO_UNIT);
            FAStar* pStepFinder = gDLL->getFAStarIFace()->create();
            CvMap& theMap = gGlobals.getMap();
            gDLL->getFAStarIFace()->Initialize(pStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), unitDataPathDestValid, stepHeuristic, unitDataPathCost, stepValid, unitDataPathAdd, NULL, NULL);

            std::map<IDInfo, std::vector<std::list<UnitData> > > requiredUnitsCityMap;  // map of units required for counter mission against closest city threatened by that missions's hostiles
            std::map<IDInfo, int> timesToTargetMap;  // how long for hostile units to reach closest threatened cities

            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                if (((*missionsIter)->missionType == MISSIONAI_COUNTER || (*missionsIter)->missionType == MISSIONAI_COUNTER_CITY) && !(*missionsIter)->cityAttackOdds.empty())
                {
                    for (std::map<IDInfo, CombatGraph::Data>::const_iterator aIter((*missionsIter)->cityAttackOdds.begin()), aEndIter((*missionsIter)->cityAttackOdds.end()); aIter != aEndIter; ++aIter)
                    {
                        if (aIter->second.pWin > hostileAttackThreshold)
                        {
                            const CvCity* pTargetCity = ::getCity((*missionsIter)->closestCity);
                            std::vector<UnitData> hostileUnitData = makeUnitData((*missionsIter)->targetUnits);
                            if (hostileUnitData.empty() || !pTargetCity)
                            {
                                continue;
                            }
                            int stepFinderInfo = MAKEWORD((short)hostileUnitData[0].unitId.eOwner, 0);
                            const CvUnit* pFirstHostileUnit = ::getUnit(hostileUnitData[0].unitId);

                            // copy required units for missions which are tracking hostiles which threaten cities (and potentially other targets) into map keyed by city under threat
                            std::copy((*missionsIter)->requiredUnits.begin(), (*missionsIter)->requiredUnits.end(), std::back_inserter(requiredUnitsCityMap[(*missionsIter)->closestCity]));

                            gDLL->getFAStarIFace()->SetData(pStepFinder, &hostileUnitData);

                            // just assume one unit plot for hostile units for now
                            if (gDLL->getFAStarIFace()->GeneratePath(pStepFinder, pFirstHostileUnit->getX(), pFirstHostileUnit->getY(), pTargetCity->getX(), pTargetCity->getY(), false, stepFinderInfo, true))
                            {
                                FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pStepFinder);
                                // todo - make this work off multiple missions against the city with different turns to attack - also need to combine hostiles properly across missions in calculating attack prob in the first place
                                // for now - take the worst case - i.e. shortest time
                                std::map<IDInfo, int>::iterator tIter = timesToTargetMap.find((*missionsIter)->closestCity);
                                if (tIter != timesToTargetMap.end())
                                {                                    
                                    tIter->second = std::min<int>(tIter->second, pNode->m_iData2);  // if city has closer threat - keep its entry time
                                }
                                else
                                {
                                    timesToTargetMap[(*missionsIter)->closestCity] = pNode->m_iData2;
                                }
#ifdef ALTAI_DEBUG
                                os << "\nCalculated hostile units time to target city: " << safeGetCityName(pTargetCity) << " = " << pNode->m_iData2;
#endif
                            }
#ifdef ALTAI_DEBUG
                            else
                            {
                                os << "\nFailed to hostile units calculate time to target city: " << safeGetCityName(pTargetCity) << " for unit: " << pFirstHostileUnit->getIDInfo();
                            }
#endif
                        }
                    }
                }
            }

            if (!requiredUnitsCityMap.empty())
            {
                DependencyItemSet di;
                di.insert(DependencyItem(-1, -1));
                const TacticSelectionData& tacticData = player_.getAnalysis()->getPlayerTactics()->tacticSelectionDataMap[di];
                std::map<UnitTypes, std::list<std::pair<IDInfo, size_t> > > unitCityBuildTimes;
                
                int stepFinderInfo = MAKEWORD((short)player_.getPlayerID(), 0);               

                // loop over each threatened city and see if we can get the required counter units there in time:
                // 1. check the city's entry in timesToTargetMap to see how long we have before the hostiles arrive)
                // 2. for each required unit, see how long each city takes to build it and for those for which the build time < time to hostiles arrival...
                // 3. ... compute how long to reach threatened city from build city (if they are not one and the same)
                // 4. if total time < time to hostiles arrival, store city
                for (std::map<IDInfo, std::vector<std::list<UnitData> > >::const_iterator ci(requiredUnitsCityMap.begin()), ciEnd(requiredUnitsCityMap.end()); ci != ciEnd; ++ci)
                {
                    const CvCity* pTargetCity = ::getCity(ci->first);
                    if (!pTargetCity) // might have just lost city
                    {
                        continue;
                    }
                    std::map<IDInfo, int>::const_iterator tIter = timesToTargetMap.find(ci->first);
                    if (tIter == timesToTargetMap.end())
                    {
                        continue;
                    }
                    const int hostileTurnsToCity = tIter->second;
                    // for each required unit
                    for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                    {
                        // for each possible required unit choice
                        for (std::list<UnitData>::const_iterator li(ci->second[i].begin()), liEnd(ci->second[i].end()); li != liEnd; ++li)
                        {
                            std::list<IDInfo> candidateCities;
                            int minTimeToIntercept = MAX_INT;
                            // get city build times for this choice of unit
                            std::map<UnitTypes, std::list<std::pair<IDInfo, size_t> > >::const_iterator ubIter = unitCityBuildTimes.find(li->unitType);
                            if (ubIter == unitCityBuildTimes.end())
                            {
                                // todo - make a separate fn to get unit builds times for a given city
                                std::list<std::pair<IDInfo, size_t> > buildTimes = TacticSelectionData::getCityBuildTimes(tacticData.fieldAttackUnits, li->unitType);
                                if (buildTimes.empty())
                                {
                                    buildTimes = TacticSelectionData::getCityBuildTimes(tacticData.cityDefenceUnits, li->unitType);
                                }
                                if (buildTimes.empty())
                                {
                                    continue;
                                }
                                ubIter = unitCityBuildTimes.insert(std::make_pair(li->unitType, buildTimes)).first;
                            }
                            // filter those which are close enough to the target city given how long they take to build and move                            
                            std::vector<UnitData> pathfinderUnitData(1, *li);
                            gDLL->getFAStarIFace()->SetData(pStepFinder, &pathfinderUnitData);

                            // iterate over each city that can build the given unit
                            for (std::list<std::pair<IDInfo, size_t> >::const_iterator buildCityIter(ubIter->second.begin()), endBuildCityIter(ubIter->second.end()); buildCityIter != endBuildCityIter; ++buildCityIter)
                            {
                                int thisCityBuildTime = buildCityIter->second;
                                bool isTargetCity = buildCityIter->first == ci->first;

                                int thisCityInterceptTime = thisCityBuildTime;
                                if (thisCityBuildTime <= hostileTurnsToCity)
                                {
                                    if (!isTargetCity)  // skip path logic if this is the target city
                                    {
                                        const CvCity* pBuildCity = ::getCity(buildCityIter->first);
                                        if (!pBuildCity)  // might have lost city
                                        {
                                            continue;
                                        }

                                        // check for path for unit from build city to target city
                                        if (gDLL->getFAStarIFace()->GeneratePath(pStepFinder, pBuildCity->getX(), pBuildCity->getY(), pTargetCity->getX(), pTargetCity->getY(), false, stepFinderInfo, true))
                                        {
                                            FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pStepFinder);
                                            thisCityInterceptTime += pNode->m_iData2;  // add on cost of moving from build city to target city
#ifdef ALTAI_DEBUG
                                            os << "\n\t" << safeGetCityName(pBuildCity) << " time to build unit: " << pathfinderUnitData[0].pUnitInfo->getType() << " = " << buildCityIter->second << ", time to target city = " << pNode->m_iData2;
#endif                                            
                                        }
                                        else
                                        {
#ifdef ALTAI_DEBUG                                        
                                            os << "\nFailed to find path from city: " << safeGetCityName(buildCityIter->first) << " for unit: " << pathfinderUnitData[0].pUnitInfo->getType();
#endif
                                            continue;
                                        }
                                    }
#ifdef ALTAI_DEBUG
                                    else
                                    {
                                        os << "\n\t" << safeGetCityName(buildCityIter->first) << " time to build unit: " << pathfinderUnitData[0].pUnitInfo->getType() << " = " << buildCityIter->second;
                                    }
#endif

                                    minTimeToIntercept = std::min<int>(minTimeToIntercept, thisCityInterceptTime);
                                    if (thisCityInterceptTime <= hostileTurnsToCity)
                                    {
                                        candidateCities.push_back(buildCityIter->first);
#ifdef ALTAI_DEBUG
                                        os << " possible candidate " << safeGetCityName(buildCityIter->first) << " ";
#endif
                                    }
                                }
                                else
                                {
#ifdef ALTAI_DEBUG
                                    os << " city is too slow to build unit - "<< safeGetCityName(buildCityIter->first) << " i'cept t = " << thisCityBuildTime << " required: " << hostileTurnsToCity;
#endif
                                }
                            }

                            std::list<IDInfo>::const_iterator candidatesIter = std::find(candidateCities.begin(), candidateCities.end(), city);
                            if (candidatesIter != candidateCities.end())
                            {
#ifdef ALTAI_DEBUG
                                os << "\n\tmatched to candidate city: " << safeGetCityName(city) << " for unit: " << li->pUnitInfo->getType();
#endif
                                cityAndUnit = std::make_pair(city, li->unitType);
                                break;  // don't want to build all the unit choices - todo - order choices by best odds
                            }
                            // todo - add check for ability to rush units
                            else if (city == ci->first)  // targetted city is city we are checking emergency build for, but we can't slow-build in time
                            {
#ifdef ALTAI_DEBUG
                                os << "\n\tpushed build for targetted city: " << safeGetCityName(city) << " for unit: " << li->pUnitInfo->getType();
#endif
                                // return anyway - hopefully can build one turn and then rush
                                cityAndUnit = std::make_pair(city, li->unitType);
                                break;
                            }
                        }
                    }                    
                }
            }
            gDLL->getFAStarIFace()->destroy(pStepFinder);

            return cityAndUnit;
        }

        UnitTypes getUnitRequestBuild(const CvCity* pCity, const TacticSelectionData& tacticSelectionData)
        {
#ifdef ALTAI_DEBUG
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity->getOwner()))->getStream();
            os << "\nChecking unit builds for: " << narrow(pCity->getName());
#endif
            std::map<UnitTypes, int> unitRequestCounts;
            for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.cityAttackUnits.begin()), ciEnd(tacticSelectionData.cityAttackUnits.end()); ci != ciEnd; ++ci)
            {
#ifdef ALTAI_DEBUG
                os << "\nUnit: " << gGlobals.getUnitInfo(ci->unitType).getType() << ", turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue
                    << ", count = " << pPlayer->getUnitCount(ci->unitType) << ", incl. under construction count = " << pPlayer->getUnitCount(ci->unitType, true);
#endif
                unitRequestCounts.insert(std::make_pair(ci->unitType, 0));
                cityBuildsTimes_[pCity->getIDInfo()][ci->unitType] = ci->nTurns;

                int bestBuildTime = ci->nTurns;
                IDInfo bestCity = pCity->getIDInfo();

                for (std::map<IDInfo, std::map<UnitTypes, int> >::const_iterator buildTimesIter(cityBuildsTimes_.begin()), buildTimesEndIter(cityBuildsTimes_.end());
                    buildTimesIter != buildTimesEndIter; ++buildTimesIter)
                {
                    std::map<UnitTypes, int>::const_iterator unitBuildTimeIter = buildTimesIter->second.find(ci->unitType);
                    if (unitBuildTimeIter != buildTimesIter->second.end())
                    {
                        if (unitBuildTimeIter->second < bestBuildTime)
                        {
                            bestBuildTime = unitBuildTimeIter->second;
                            bestCity = buildTimesIter->first;
                        }
                    }
                }
#ifdef ALTAI_DEBUG
                const CvCity* pBestCity = CvPlayerAI::getPlayer(pCity->getOwner()).getCity(bestCity.iID);
                os << " best build time = " << bestBuildTime << ", city = " << (pBestCity ? narrow(pBestCity->getName()) : " null");
#endif
                //getOddsAgainstHostiles_(pCity, *ci);
            }
            
            std::map<UnitTypes, std::vector<MilitaryMissionDataPtr> > unitMissionRequests;
            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                for (std::map<UnitTypes, int>::iterator unitTypesIter(unitRequestCounts.begin()), unitTypesEndIter(unitRequestCounts.end());
                    unitTypesIter != unitTypesEndIter; ++unitTypesIter)
                {
                    int thisUnitRequiredCount = (*missionsIter)->getRequiredUnitCount(unitTypesIter->first);
                    if (thisUnitRequiredCount > 0)
                    {
                        unitTypesIter->second += thisUnitRequiredCount;
                        unitMissionRequests[unitTypesIter->first].push_back(*missionsIter);
                    }
                }
            }

            for (std::map<UnitTypes, int>::iterator uIter(unitRequestCounts.begin()), uEndIter(unitRequestCounts.end()); uIter != uEndIter; ++uIter)
            {
                int underConstructionCount = player_.getUnitCount(uIter->first, true) - player_.getUnitCount(uIter->first);
                uIter->second = std::max<int>(uIter->second - underConstructionCount, 0);
            }

            UnitTypes mostRequestedUnit = NO_UNIT;
            int mostRequestedCount = 0;
            for (std::map<UnitTypes, int>::const_iterator uIter(unitRequestCounts.begin()), uEndIter(unitRequestCounts.end()); uIter != uEndIter; ++uIter)
            {
#ifdef ALTAI_DEBUG
                os << " unit " << gGlobals.getUnitInfo(uIter->first).getType() << " req count = " << uIter->second;
#endif
                if (uIter->second > mostRequestedCount)
                {
                    mostRequestedCount = uIter->second;
                    mostRequestedUnit = uIter->first;
                }
            }
#ifdef ALTAI_DEBUG
            if (mostRequestedUnit != NO_UNIT)
            {
                os << "\nmissions requesting unit: " << gGlobals.getUnitInfo(mostRequestedUnit).getType();
                std::map<UnitTypes, std::vector<MilitaryMissionDataPtr> >::const_iterator rIter = unitMissionRequests.find(mostRequestedUnit);
                if (rIter != unitMissionRequests.end())
                {
                    for (size_t i = 0, count = rIter->second.size(); i < count; ++i)
                    {                    
                        rIter->second[i]->debug(os);
                    }
                }
                else
                {
                    os << " none? ";
                }
            }
            os << "\ngetUnitRequestBuild returning: " << (mostRequestedUnit == NO_UNIT ? " (none) " : gGlobals.getUnitInfo(mostRequestedUnit).getType());
#endif
            return mostRequestedUnit;
        }

        PlotUnitsMap getNearbyHostileStacks(const CvPlot* pPlot, int range)
        {
            PlotUnitsMap stacks;

            for (std::map<XYCoords, std::list<IDInfo> >::const_iterator si(enemyStacks_.begin()), siEnd(enemyStacks_.end()); si != siEnd; ++si)
            {
                const CvPlot* pHostilePlot = gGlobals.getMap().plot(si->first.iX, si->first.iY);
                // only looks at water-water or land-land combinations
                if (pHostilePlot->isWater() == pPlot->isWater() && stepDistance(pPlot->getX(), pPlot->getY(), si->first.iX, si->first.iY) <= range)
                {
                    std::vector<const CvUnit*> stackUnits;
                    for (std::list<IDInfo>::const_iterator unitStackIter(si->second.begin()), unitStackEndIter(si->second.end());
                                        unitStackIter != unitStackEndIter; ++unitStackIter)
                    {
                        const CvUnit* pStackUnit = ::getUnit(*unitStackIter);
                        stackUnits.push_back(pStackUnit);
                    }
                    stacks.insert(std::make_pair(pHostilePlot, stackUnits));
                }
            }

            return stacks;
        }

        PlotSet getThreatenedPlots() const
        {
            PlotSet plots;
            for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                plots.insert((*missionsIter)->hostilesReachablePlots.begin(), (*missionsIter)->hostilesReachablePlots.end());
            }

            return plots;
        }

        std::set<IDInfo> getUnitsThreateningPlots(const PlotSet& plots) const
        {
            std::set<IDInfo> units;

            for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                for (PlotSet::const_iterator pi(plots.begin()), piEnd(plots.end()); pi != piEnd; ++pi)
                {
                    if ((*missionsIter)->hostilesReachablePlots.find(*pi) != (*missionsIter)->hostilesReachablePlots.end())
                    {
                        units.insert((*missionsIter)->targetUnits.begin(), (*missionsIter)->targetUnits.end());
                        break;
                    }
                }
            }

            return units;
        }

        std::vector<UnitData> getPossibleCounterAttackers(const CvPlot* pPlot) const
        {
            std::vector<UnitData> possibleAttackers;

            for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                MissionAITypes missionAIType = (*missionsIter)->missionType;
                if (missionAIType == MISSIONAI_COUNTER || missionAIType == MISSIONAI_COUNTER_CITY)
                {
                    MilitaryMissionData::ReachablePlotDetails::const_iterator rIter = (*missionsIter)->hostileReachablePlotDetails.find(pPlot);
                    if (rIter != (*missionsIter)->hostileReachablePlotDetails.end())
                    {
                        for (std::list<IDInfo>::const_iterator lIter(rIter->second.begin()), lEndIter(rIter->second.end()); lIter != lEndIter; ++lIter)
                        {
                            const CvUnit* pUnit = ::getUnit(*lIter);
                            // condition is units we can see, but are not at the given plot - we want to know what can attack if we capture the plot
                            if (pUnit && pUnit->plot()->getCoords() != pPlot->getCoords() && pUnit->plot()->isVisible(player_.getTeamID(), false))
                            {
                                possibleAttackers.push_back(UnitData(pUnit));
                            }
                        }
                    }
                }
            }

            return possibleAttackers;
        }

        PlotUnitsMap getPlotsThreatenedByUnits(const PlotSet& plots) const
        {
            PlotUnitsMap targetsMap;
            // for each mission...
            for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                // for each plot we can reach...
                for (PlotSet::const_iterator pi(plots.begin()), piEnd(plots.end()); pi != piEnd; ++pi)
                {
                    // see if this mission has targets which can reach that plot...
                    if ((*missionsIter)->hostilesReachablePlots.find(*pi) != (*missionsIter)->hostilesReachablePlots.end())
                    {
                        // copy each target unit into the targetUnits map
                        for (std::set<IDInfo>::const_iterator ti((*missionsIter)->targetUnits.begin()), tiEnd((*missionsIter)->targetUnits.end());
                            ti != tiEnd; ++ti)
                        {
                            const CvUnit* pTargetUnit = ::getUnit(*ti);
                            if (pTargetUnit)
                            {
                                targetsMap[*pi].push_back(pTargetUnit);
                            }
                        }
                    }
                }
            }

            return targetsMap;
        }

        const std::map<XYCoords, CombatData>& getAttackableUnitsMap()
        {
            return attackCombatMap_;
        }

        CvUnit* getNextAttackUnit()
        {
            CvUnit* pAttackUnit = NULL;  // fine to return no unit
            if (enemyStacks_.empty())
            {
                return pAttackUnit;
            }
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            //std::map<IDInfo, CombatResultsData> hostilesCombatData, uncombatableHostilesCombatData;

            std::list<std::pair<IDInfo, float> > cityDefenceOddsList;
            for (std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator gmIter(cityGuardMissionsMap_.begin()), gmEndIter(cityGuardMissionsMap_.end()); gmIter != gmEndIter; ++gmIter)
            {
                const CvCity* pCity = ::getCity(gmIter->first);
                if (pCity)
                {
                    if (!gmIter->second->ourAttackers.empty())
                    {
                        if (gmIter->second->assignedUnits.empty())
                        {
                            cityDefenceOddsList.push_back(std::make_pair(gmIter->first, gmIter->second->ourAttackOdds.pLoss));
                        }
                        else
                        {
                            for (std::set<IDInfo>::const_iterator aIter(gmIter->second->assignedUnits.begin()), aEndIter(gmIter->second->assignedUnits.end()); aIter != aEndIter; ++aIter)
                            {
                                const CvUnit* pMissionUnit = ::getUnit(*aIter);
                                if (pMissionUnit && pMissionUnit->at(pCity->getX(), pCity->getY()))
                                {
                                    // only add if unit is actually at the city (todo - add logic in update() to add entry for undefended cities)
                                    cityDefenceOddsList.push_back(std::make_pair(gmIter->first, gmIter->second->ourAttackOdds.pLoss));
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            cityDefenceOddsList.sort(AttackThreatComp());
            
            // loop over threatened cities
            for (std::list<std::pair<IDInfo, float> >::const_iterator threatIter(cityDefenceOddsList.begin()), threatEndIter(cityDefenceOddsList.end()); threatIter != threatEndIter; ++threatIter)
            {
                // find possible attack odds
                std::map<XYCoords, float> battleOddsMap;
                std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator cityMissionIter(cityGuardMissionsMap_.find(threatIter->first));
                if (cityMissionIter != cityGuardMissionsMap_.end())
                {
                    std::set<IDInfo> accountedForHostiles, unaccountedForHostiles, ourAttackUnitsNotDefending;
                    IDInfo firstAttacker;
                    float worstOdds = 1.0;
                    for (size_t attackerIndex = 0, attackerCount = cityMissionIter->second->ourAttackers.size(); attackerIndex < attackerCount; ++attackerIndex)
                    {
                        // find hostile unit mission containing this attacker
                        std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator hostileMissionIter = hostileUnitsMissionMap_.find(cityMissionIter->second->ourAttackers[attackerIndex].unitId);
                        if (hostileMissionIter != hostileUnitsMissionMap_.end())
                        {
                            for (size_t foundAttackersIndex = 0, foundAttackersCount = hostileMissionIter->second->ourAttackOdds.defenders.size(); foundAttackersIndex < foundAttackersCount; ++foundAttackersIndex)
                            {
                                accountedForHostiles.insert(hostileMissionIter->second->ourAttackOdds.defenders[foundAttackersIndex].unitId);
                                const CvUnit* pHostileUnit = ::getUnit(hostileMissionIter->second->ourAttackOdds.defenders[foundAttackersIndex].unitId);
                                if (pHostileUnit)
                                {
                                    const XYCoords unitCoords = pHostileUnit->plot()->getCoords();
                                    if (battleOddsMap.find(pHostileUnit->plot()->getCoords()) == battleOddsMap.end())
                                    {
                                        battleOddsMap.insert(std::make_pair(pHostileUnit->plot()->getCoords(), hostileMissionIter->second->ourAttackOdds.pWin));
                                        worstOdds = std::min<float>(worstOdds, hostileMissionIter->second->ourAttackOdds.pWin);
                                    }
                                }
                            }
                            for (size_t ourAttackersIndex = 0, ourAttackersCount = hostileMissionIter->second->ourAttackers.size(); ourAttackersIndex < ourAttackersCount; ++ourAttackersIndex)
                            {
                                const IDInfo ourAttackUnit = hostileMissionIter->second->ourAttackers[ourAttackersIndex].unitId;
                                if (std::find_if(cityMissionIter->second->ourDefenders.begin(), cityMissionIter->second->ourDefenders.end(),
                                    UnitDataIDInfoP(ourAttackUnit)) != cityMissionIter->second->ourDefenders.end())
                                {
                                    ourAttackUnitsNotDefending.insert(ourAttackUnit);
                                }
                            }
                            firstAttacker = hostileMissionIter->second->firstAttacker;
                        } 
                    }

                    for (size_t attackerIndex = 0, attackerCount = cityMissionIter->second->ourAttackers.size(); attackerIndex < attackerCount; ++attackerIndex)
                    {
                        if (accountedForHostiles.find(cityMissionIter->second->ourAttackers[attackerIndex].unitId) == accountedForHostiles.end())
                        {
                            unaccountedForHostiles.insert(cityMissionIter->second->ourAttackers[attackerIndex].unitId);
                        }
                    }
#ifdef ALTAI_DEBUG
                    os << "\ncity: " << safeGetCityName(cityMissionIter->first) << " survive attack odds: " << threatIter->second;
                    os << "\n\t can attack: " << accountedForHostiles.size() << " out of: " << cityMissionIter->second->ourAttackers.size() << " with worst odds of: " << worstOdds;
                    for (std::map<XYCoords, float>::const_iterator bi(battleOddsMap.begin()); bi != battleOddsMap.end(); ++bi)
                    {
                        os << " coords: " << bi->first << " attack odds: " << bi->second;
                    }
                    os << " accounted for hostiles: ";
                    for (std::set<IDInfo>::const_iterator ahIter(accountedForHostiles.begin()), ahEndIter(accountedForHostiles.end()); ahIter != ahEndIter; ++ahIter)
                    {
                        os << *ahIter << " ";
                    }
                    if (!unaccountedForHostiles.empty())
                    {
                        os << " unaccounted for hostiles: ";
                        for (std::set<IDInfo>::const_iterator uhIter(unaccountedForHostiles.begin()), uhEndIter(unaccountedForHostiles.end()); uhIter != uhEndIter; ++uhIter)
                        {
                            os << *uhIter << " ";
                        }
                    }
                    if (!ourAttackUnitsNotDefending.empty())
                    {
                        os << " our attack units not defending: ";
                        for (std::set<IDInfo>::const_iterator aIter(ourAttackUnitsNotDefending.begin()), aEndIter(ourAttackUnitsNotDefending.end()); aIter != aEndIter; ++aIter)
                        {
                            os << *aIter << " ";
                        }
                    }
                    if (firstAttacker != IDInfo())
                    {
                        os << " first attacker: " << firstAttacker;
                    }
#endif
                    // if attack and survival odds are both less than ~0.8 we're in trouble
                    // scenarios:
                    // survival odds > attack odds => sit tight; if survival odds < 0.8 => determine priority units to build
                    // survival odds < attack odds => if attack odds > 0.8 -> attack, else if > 0.6(ish - todo) attack - maybe without best defender
                    //                                if survival odds < 0.8 => determine priority units to build
                    // for free defenders -> compute standalone attack odds or attacks which don't leave units exposed
                    if (worstOdds > threatIter->second && worstOdds > defAttackThreshold)
                    {
                        if (firstAttacker != IDInfo())
                        {
                            CvUnit* pPossibleAttackUnit = ::getUnit(firstAttacker);
                            if (pPossibleAttackUnit && pPossibleAttackUnit->canMove() && (!pPossibleAttackUnit->isMadeAttack() || pPossibleAttackUnit->isBlitz()))
                            {
                                pAttackUnit = pPossibleAttackUnit;

                                for (std::set<IDInfo>::const_iterator ahIter(accountedForHostiles.begin()), ahEndIter(accountedForHostiles.end()); ahIter != ahEndIter; ++ahIter)
                                {
                                    std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator hostileMissionIter(hostileUnitsMissionMap_.find(*ahIter));
                                    if (hostileMissionIter != hostileUnitsMissionMap_.end())
                                    {
                                        if (std::find_if(hostileMissionIter->second->ourAttackers.begin(), hostileMissionIter->second->ourAttackers.end(),
                                            UnitDataIDInfoP(firstAttacker)) != hostileMissionIter->second->ourAttackers.end())
                                        {
                                            std::map<IDInfo, MilitaryMissionDataPtr>::iterator ourMissionIter(ourUnitsMissionMap_.find(firstAttacker));
                                            if (ourMissionIter != ourUnitsMissionMap_.end() && !hostileMissionIter->second->targetUnits.empty())
                                            {
                                                const CvUnit* pTargetUnit = ::getUnit(*hostileMissionIter->second->targetUnits.begin());
                                                std::vector<UnitData> counterUnits = getPossibleCounterAttackers(pTargetUnit->plot());
                                                if (!counterUnits.empty())
                                                {
                                                    UnitData::CombatDetails combatDetails(pTargetUnit->plot());
                                                    combatDetails.attackDirection = directionXY(pAttackUnit->plot(), pTargetUnit->plot());
                                                    std::vector<UnitData> ourUnitData(1, UnitData(pAttackUnit));                                                    
                                                    CombatGraph combatGraph = getCombatGraph(player_, combatDetails, counterUnits, ourUnitData);
                                                    combatGraph.analyseEndStates();
#ifdef ALTAI_DEBUG
                                                    os << "\ncounter attack data: ";
                                                    combatGraph.debugEndStates(os);
#endif
                                                }
#ifdef ALTAI_DEBUG
                                                os << " setting special targets: ";
                                                debugUnitSet(os, hostileMissionIter->second->targetUnits);
#endif
                                                ourMissionIter->second->specialTargets.insert(hostileMissionIter->second->targetUnits.begin(), hostileMissionIter->second->targetUnits.end());
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (pAttackUnit)
            {
                pAttackUnit->getGroup()->setForceUpdate(true);  // so we can update with our adhoc attack mission
            }
            else if (!attackCombatData_.empty())
            {
                for (std::map<XYCoords, CombatGraph::Data>::iterator iter(attackCombatData_.begin()), iterEnd(attackCombatData_.end()); iter != iterEnd; ++iter)
                {
                    if (iter->second.pWin > attThreshold)
                    {
                        for (std::list<IDInfo>::iterator li(iter->second.longestAndShortestAttackOrder.first.begin()); li != iter->second.longestAndShortestAttackOrder.first.end(); ++li)
                        {
                            CvUnit* pPossibleAttackUnit = ::getUnit(*li);
                            // canMove() check as we might have moved this unit already as part of a seperate response
                            // todo - handle this case more cleanly
                            if (!pPossibleAttackUnit || pPossibleAttackUnit->isMadeAttack() || !pPossibleAttackUnit->canMove())  // todo - deal with blitz edge case if unit is scheduled to attack more than once
                            {
                                iter->second.longestAndShortestAttackOrder.first.erase(li);
                            }
                            else
                            {
                                if (pPossibleAttackUnit)
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nReturning next attacker: " << pPossibleAttackUnit->getIDInfo() << " remaining count = " << iter->second.longestAndShortestAttackOrder.first.size()
                                        << " with attack coords: " << iter->first << " and pWin = " << iter->second.pWin;
#endif
                                    pAttackUnit = pPossibleAttackUnit;
                                    pAttackUnit->getGroup()->setActivityType(ACTIVITY_AWAKE);
                                    // find mission for unit
                                    std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(pAttackUnit->getIDInfo());
                                    if (missionIter != ourUnitsMissionMap_.end())
                                    {
                                        if (isEmpty(missionIter->second->nextAttack.first))
                                        {
#ifdef ALTAI_DEBUG
                                            os << "\nSet attack unit and coords for unit's mission";
#endif
                                            missionIter->second->nextAttack = std::make_pair(pAttackUnit->getIDInfo(), iter->first);
                                        }
                                        else
                                        {
#ifdef ALTAI_DEBUG
                                            os << "\nUnit's mission already has different attack unit set: " << missionIter->second->nextAttack.first
                                               << " for coords: " << missionIter->second->nextAttack.second << " clearing and not returning an attack unit ";
#endif
                                            missionIter->second->nextAttack = std::make_pair(IDInfo(), XYCoords());
                                            return NULL;
                                        }
                                    }
                                    break;
                                }
                            }
                        }

                        if (pAttackUnit)
                        {
                            break;
                        }
                    }
                }
            }
            return pAttackUnit;
        }

        std::set<XYCoords> getUnitHistory(IDInfo unit) const
        {
            std::set<XYCoords> coords;

            std::map<IDInfo, UnitHistory>::const_iterator historyIter = unitHistories_.find(unit);
            if (historyIter != unitHistories_.end())
            {
                for (std::list<std::pair<int, XYCoords> >::const_iterator listIter(historyIter->second.locationHistory.begin()); 
                    listIter != historyIter->second.locationHistory.end(); ++listIter)
                {
                    coords.insert(listIter->second);
                }
            }

            return coords;
        }

        std::map<int /* sub area */, std::set<XYCoords> > getLandScoutMissionRefPlots() const
        {
            std::map<int /* sub area */, std::set<XYCoords> > subAreaRefPlotsMap;

            for (std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator missionIter(ourUnitsMissionMap_.begin()), endIter(ourUnitsMissionMap_.end());
                missionIter != endIter; ++missionIter)
            {
                if (missionIter->second->missionType == MISSIONAI_EXPLORE)
                {
                    if (missionIter->second->targetCoords != XYCoords(-1, -1))
                    {
                        const CvPlot* pTargetPlot = gGlobals.getMap().plot(missionIter->second->targetCoords.iX, missionIter->second->targetCoords.iY);
                        if (!pTargetPlot->isWater())
                        {
                            subAreaRefPlotsMap[pTargetPlot->getSubArea()].insert(missionIter->second->targetCoords);
                        }
                    }
                }
            }
            return subAreaRefPlotsMap;
        }

        size_t getUnitCount(MissionAITypes missionType, UnitTypes unitType = NO_UNIT, int subAreaId = -1) const
        {
            size_t unitCount = 0;
            for (std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator ci(ourUnitsMissionMap_.begin()), ciEnd(ourUnitsMissionMap_.end()); ci != ciEnd; ++ci)
            {
                if (ci->second->missionType == missionType)
                {
                    CvUnit* pUnit = ::getUnit(ci->first);
                    if (pUnit && (unitType == NO_UNIT || pUnit->getUnitType() == unitType) && (subAreaId == -1 || pUnit->plot()->getSubArea() == subAreaId))
                    {
                        ++unitCount;
                    }
                }
            }
            return unitCount;
        }

        int getMissionCount(MissionAITypes missionType, int subAreaId = -1) const
        {
            int matchingMissionCount = 0;
            for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                if ((*missionsIter)->missionType == missionType)
                {
                     if (!(*missionsIter)->assignedUnits.empty())
                     {
                         CvUnit* pUnit = ::getUnit(*(*missionsIter)->assignedUnits.begin());
                         if (pUnit && (subAreaId == -1 || pUnit->plot()->getSubArea() == subAreaId))
                         {
                             ++matchingMissionCount;
                         }
                     }
                }
            }
            return matchingMissionCount;
        }

        std::list<MilitaryMissionDataPtr> getMatchingMissions(MissionAITypes missionType, int subAreaId = -1) const
        {
            std::list<MilitaryMissionDataPtr> matchingMissions;
            for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                const MilitaryMissionDataPtr pMission = *missionsIter;
                if (pMission->missionType == missionType)
                {
                     if (!pMission->assignedUnits.empty())
                     {
                         CvUnit* pUnit = ::getUnit(*pMission->assignedUnits.begin());
                         if (pUnit && (subAreaId == -1 || pUnit->plot()->getSubArea() == subAreaId))
                         {
                             matchingMissions.push_back(pMission);
                         }
                     }
                }
            }
            return matchingMissions;
        }

        std::set<IDInfo> getAssignedUnits(MissionAITypes missionType, int subAreaId = -1) const
        {
            std::set<IDInfo> assignedUnits;
            std::list<MilitaryMissionDataPtr> matchingMissions = getMatchingMissions(missionType, subAreaId);
            for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(matchingMissions.begin()); missionsIter != matchingMissions.end(); ++missionsIter)
            {
                assignedUnits.insert((*missionsIter)->assignedUnits.begin(), (*missionsIter)->assignedUnits.end());
            }
            return assignedUnits;
        }

        void write(FDataStreamBase* pStream) const
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            pStream->Write(enemyStacks_.size());
            for (std::map<XYCoords, std::list<IDInfo> >::const_iterator si(enemyStacks_.begin()), siEnd(enemyStacks_.end()); si != siEnd; ++si)
            {
                si->first.write(pStream);
                writeComplexList(pStream, si->second);
            }

            writeComplexKeyMap(pStream, unitCategoryMap_);

            pStream->Write(units_.size());
            for (std::map<UnitCategories, std::set<IDInfo> >::const_iterator ci(units_.begin()), ciEnd(units_.end()); ci != ciEnd; ++ci)
            {                
                pStream->Write(ci->first);
                writeComplexSet(pStream, ci->second);
            }

            pStream->Write(cityBuildsTimes_.size());
            for (std::map<IDInfo, std::map<UnitTypes, int> >::const_iterator ci(cityBuildsTimes_.begin()), ciEnd(cityBuildsTimes_.end()); ci != ciEnd; ++ci)
            {
                ci->first.write(pStream);
                writeMap(pStream, ci->second);
            }

            pStream->Write(unitsMap_.size());
            for (std::map<int, std::map<IDInfo, XYCoords> >::const_iterator ci(unitsMap_.begin()), ciEnd(unitsMap_.end()); ci != ciEnd; ++ci)
            {
                pStream->Write(ci->first);
                writeComplexMap(pStream, ci->second);
            }

            pStream->Write(hiddenUnitsMap_.size());
            for (std::map<int, std::map<IDInfo, XYCoords> >::const_iterator ci(hiddenUnitsMap_.begin()), ciEnd(hiddenUnitsMap_.end()); ci != ciEnd; ++ci)
            {
                pStream->Write(ci->first);
                writeComplexMap(pStream, ci->second);
            }
            
            pStream->Write(unitStacks_.size());
            for (std::map<int, std::map<XYCoords, std::set<IDInfo> > >::const_iterator ci(unitStacks_.begin()), ciEnd(unitStacks_.end()); ci != ciEnd; ++ci)
            {
                pStream->Write(ci->first);
                pStream->Write(ci->second.size());
                for (std::map<XYCoords, std::set<IDInfo> >::const_iterator cii(ci->second.begin()), ciiEnd(ci->second.end()); cii != ciiEnd; ++cii)
                {
                    cii->first.write(pStream);
                    writeComplexSet(pStream, cii->second);
                }
            }

            writeComplexMap(pStream, unitHistories_);

            // mission pointers are shared between missionList_ and the two unit mission maps
            // so we need to just serialise the missions in the main list
            // and index them so we put copies back into the right maps when loading
            // for the maps, just serialise the keys and the mission indices
            std::map<unsigned long, int> missionPointersMap;
            pStream->Write(missionList_.size());
            int missionIndex = 0;
            for (std::list<MilitaryMissionDataPtr>::const_iterator ci(missionList_.begin()), ciEnd(missionList_.end()); ci != ciEnd; ++ci)
            {
                std::map<unsigned long, int>::iterator indexIter = missionPointersMap.find((unsigned long)ci->get());
                if (indexIter == missionPointersMap.end())
                {
                    (*ci)->write(pStream);
                    indexIter = missionPointersMap.insert(std::make_pair((unsigned long)(*ci).get(), missionIndex++)).first;
                }
#ifdef ALTAI_DEBUG
                else
                {
                    os << "\nError - duplicate mission found in mission list?? ";
                    (*ci)->debug(os);
                }
#endif
            }

            writeMissionIndexMap_(pStream, missionPointersMap, ourUnitsMissionMap_, "our units");
            writeMissionIndexMap_(pStream, missionPointersMap, hostileUnitsMissionMap_, "hostile units");
            writeMissionIndexMap_(pStream, missionPointersMap, cityGuardMissionsMap_, "city guard units");

            // save intra-turn data (needed as autosave is called after update)
            writeComplexMap(pStream, attackCombatMap_);
            writeComplexMap(pStream, defenceCombatMap_);

            writeComplexMap(pStream, attackCombatData_);
            writeComplexMap(pStream, defenceCombatData_);
        }

        void read(FDataStreamBase* pStream)
        {
            size_t count = 0;
            pStream->Read(&count);
            enemyStacks_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                XYCoords coords;
                coords.read(pStream);
                std::list<IDInfo> units;
                readComplexList(pStream, units);
                enemyStacks_.insert(std::make_pair(coords, units));
            }

            unitCategoryMap_.clear();
            readComplexKeyMap<IDInfo, UnitCategories, int>(pStream, unitCategoryMap_);
            
            pStream->Read(&count);
            units_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                UnitCategories category;
                pStream->Read((int*)&category);
                std::set<IDInfo> units;
                readComplexSet(pStream, units);
                units_.insert(std::make_pair(category, units));
            }

            pStream->Read(&count);
            cityBuildsTimes_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                IDInfo city;
                city.read(pStream);
                std::map<UnitTypes, int> unitTypesMap;
                readMap<UnitTypes, int, int, int>(pStream, unitTypesMap);
                cityBuildsTimes_.insert(std::make_pair(city, unitTypesMap));
            }

            pStream->Read(&count);
            unitsMap_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                int subArea;
                pStream->Read(&subArea);
                std::map<IDInfo, XYCoords> subAreaUnitsMap;
                readComplexMap(pStream, subAreaUnitsMap);
                unitsMap_.insert(std::make_pair(subArea, subAreaUnitsMap));
            }

            pStream->Read(&count);
            hiddenUnitsMap_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                int subArea;
                pStream->Read(&subArea);
                std::map<IDInfo, XYCoords> subAreaUnitsMap;
                readComplexMap(pStream, subAreaUnitsMap);
                hiddenUnitsMap_.insert(std::make_pair(subArea, subAreaUnitsMap));
            }

            pStream->Read(&count);
            unitStacks_.clear();
            for (size_t i = 0; i < count; ++i)
            {
                int subArea;
                pStream->Read(&subArea);
                std::map<XYCoords, std::set<IDInfo> > subAreaStacksMap;
                size_t stackCount = 0;
                pStream->Read(&stackCount);
                for (size_t j = 0; j < stackCount; ++j)
                {
                    XYCoords coords;
                    coords.read(pStream);
                    std::set<IDInfo> units;
                    readComplexSet(pStream, units);
                    subAreaStacksMap.insert(std::make_pair(coords, units));
                }
                unitStacks_.insert(std::make_pair(subArea, subAreaStacksMap));
            }

            readComplexMap(pStream, unitHistories_);

            missionList_.clear();
            std::map<int, MilitaryMissionDataPtr> missionPointersMap;
            pStream->Read(&count);
            for (size_t i = 0; i < count; ++i)
            {
                MilitaryMissionDataPtr pMission(new MilitaryMissionData());
                pMission->read(pStream);
                missionList_.push_back(pMission);
                missionPointersMap[i] = pMission;
            }

            ourUnitsMissionMap_.clear();
            hostileUnitsMissionMap_.clear();
            cityGuardMissionsMap_.clear();
            readMissionIndexMap_(pStream, missionPointersMap, ourUnitsMissionMap_);
            readMissionIndexMap_(pStream, missionPointersMap, hostileUnitsMissionMap_);
            readMissionIndexMap_(pStream, missionPointersMap, cityGuardMissionsMap_);

            // read intra-turn data (needed as autosave is called after update)
            readComplexMap(pStream, attackCombatMap_);
            readComplexMap(pStream, defenceCombatMap_);

            readComplexMap(pStream, attackCombatData_);
            readComplexMap(pStream, defenceCombatData_);
        }

    private:

        UnitCategories getCategory_(UnitAITypes unitAIType) const
        {
            switch (unitAIType)
            {
            case UNITAI_SETTLE:
            case UNITAI_SETTLER_SEA:
                return Settler;

            case UNITAI_WORKER:
            case UNITAI_WORKER_SEA:
                return Worker;

            case UNITAI_EXPLORE:
            case UNITAI_EXPLORE_SEA:
                return Scout;

            case UNITAI_ATTACK:
            case UNITAI_ATTACK_CITY:
            case UNITAI_COLLATERAL:
            case UNITAI_PILLAGE:
            case UNITAI_RESERVE:
            case UNITAI_COUNTER:
            case UNITAI_CITY_DEFENSE:
            case UNITAI_CITY_COUNTER:
            case UNITAI_PARADROP:
                return LandCombat;

            case UNITAI_ESCORT_SEA:
            case UNITAI_ATTACK_SEA:
            case UNITAI_RESERVE_SEA:
            case UNITAI_ASSAULT_SEA:
            case UNITAI_CARRIER_SEA:
                return SeaCombat;

            case UNITAI_ATTACK_AIR:
            case UNITAI_DEFENSE_AIR:
            case UNITAI_CARRIER_AIR:
                return AirCombat;

            case UNITAI_MISSIONARY:
            case UNITAI_MISSIONARY_SEA:
                return Missionary;

            case UNITAI_SCIENTIST:
            case UNITAI_PROPHET:
            case UNITAI_ARTIST:
            case UNITAI_ENGINEER:
            case UNITAI_MERCHANT:
            case UNITAI_GENERAL:
                return GreatPerson;

            case UNITAI_SPY:
                return Spy;

            case UNITAI_ICBM:
            case UNITAI_MISSILE_AIR:
                return Missile;

            // todo - check if these are used
            case UNITAI_CITY_SPECIAL:
            case UNITAI_ATTACK_CITY_LEMMING:
            default:
                return Unknown;
            }
        }

        UnitCategories addOurUnit_(CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            UnitAITypes unitAIType = pUnit->AI_getUnitAIType();
            UnitCategories category = getCategory_(unitAIType);
            units_[category].insert(pUnit->getIDInfo());
            unitCategoryMap_[pUnit->getIDInfo()] = category;

 #ifdef ALTAI_DEBUG
            os << "\nAdded unit: " << pUnit->getIDInfo() << " "
                << gGlobals.getUnitInfo(pUnit->getUnitType()).getType()
                << " with category: " << category;                
#endif
            return category;
        }

        MilitaryMissionDataPtr checkCityGuard_(CvUnitAI* pUnit, const CvCity* pCity)
        {
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator cityGuardMissionIter = cityGuardMissionsMap_.find(pCity->getIDInfo());
            FAssertMsg(cityGuardMissionIter != cityGuardMissionsMap_.end(), "Missing city guard mission?");

            MilitaryMissionDataPtr pCityGuardMission = cityGuardMissionIter->second;

#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\ncheckCityGuard_: " << cityGuardMissionIter->first << "(" << safeGetCityName(cityGuardMissionIter->first) << ") required units: ";
            debugUnitDataLists(os, pCityGuardMission->requiredUnits);
#endif
            int requiredCount = pCityGuardMission->getRequiredUnitCount(pUnit->getUnitType());
            if (requiredCount > 0)
            {
                
                reassignOurUnit_(pUnit, pCityGuardMission);
                pCityDefenceAnalysis_->addOurUnit(pUnit, pCity);
                return pCityGuardMission;
            }

            return MilitaryMissionDataPtr();
        }

        MilitaryMissionDataPtr checkEscort_(CvUnitAI* pUnit, const CvPlot* pUnitPlot)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            MilitaryMissionDataPtr pUnitMission;
            std::map<int, MilitaryMissionDataPtr> possibleEscortMissionsMap;
            XYCoords escortCoords(pUnit->plot()->getCoords());

            SelectionGroupIter iter(*player_.getCvPlayer());

            // when we have map of missions by type - just iterate over that
            while (CvSelectionGroup* pGroup = iter())
            {
                UnitGroupIter unitIter(pGroup);
                while (const CvUnit* pPlotUnit = unitIter())
                {
                    if (pPlotUnit->isFound())
                    {
                        std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(pPlotUnit->getIDInfo());
                        if (missionIter != ourUnitsMissionMap_.end() && missionIter->second->getRequiredUnitCount(pUnit->getUnitType()) > 0)
                        {
                            XYCoords settlerCoords(pPlotUnit->plot()->getCoords());
                            possibleEscortMissionsMap[stepDistance(escortCoords.iX, escortCoords.iY, settlerCoords.iX, settlerCoords.iY)] = missionIter->second;
                        }
                    }
                }
            }

            if (!possibleEscortMissionsMap.empty())
            {
                pUnitMission = possibleEscortMissionsMap.begin()->second;
                reassignOurUnit_(pUnit, pUnitMission);
            }
            
            if (pUnitMission)
            {
#ifdef ALTAI_DEBUG
                std::vector<IDInfo> settlerUnits = pUnitMission->getOurMatchingUnits(CanFoundP());
                if (!settlerUnits.empty())
                {
                    const CvUnit* pSettlerUnit = player_.getCvPlayer()->getUnit(settlerUnits[0].iID);
                    if (pSettlerUnit && pSettlerUnit->plot()->isCity())
                    {
                        if (pSettlerUnit->plot()->isCity())
                        {
                            os << "\nAssigned unit to escort settler from city: " << safeGetCityName(pSettlerUnit->plot()->getPlotCity());
                        }
                        else
                        {
                            os << "\nAssigned unit to escort settler from plot: " << pSettlerUnit->plot()->getCoords();
                        }
                    }
                }
                
                pUnitMission->debug(os);
#endif
            }
            return pUnitMission;
        }

        MilitaryMissionDataPtr upgradeOurUnit_(CvUnitAI* pUnit, const CvUnit* pUpgradingUnit)
        {
            MilitaryMissionDataPtr pUnitMission;
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nSearching for mission for upgraded unit: " << pUpgradingUnit->getIDInfo();
#endif
            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                std::set<IDInfo>::iterator targetsIter((*missionsIter)->assignedUnits.find(pUpgradingUnit->getIDInfo()));
                if (targetsIter != (*missionsIter)->assignedUnits.end())
                {
#ifdef ALTAI_DEBUG
                    os << " found mission for upgrading unit: " << pUnit->getIDInfo();
#endif
                    pUnitMission = *missionsIter;
                    pUnitMission->assignUnit(pUnit, false);
                    pUnitMission->unassignUnit(pUpgradingUnit, false);
                    break;
                }
            }
            // update mission map with upgraded unit's IDInfo
            ourUnitsMissionMap_.erase(pUpgradingUnit->getIDInfo());
            if (pUnitMission)
            {
                ourUnitsMissionMap_.insert(std::make_pair(pUnit->getIDInfo(), pUnitMission));
            }

            return pUnitMission;
        }

        void updateStacks_()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif 
            enemyStacks_.clear();
            CvTeamAI& ourTeam = CvTeamAI::getTeam(player_.getTeamID());

            for (std::map<int, std::map<XYCoords, std::set<IDInfo> > >::iterator ci(unitStacks_.begin()), ciEnd(unitStacks_.end()); ci != ciEnd; ++ci)
            {
                std::map<XYCoords, std::set<IDInfo> >::iterator cii(ci->second.begin());
                while (cii != ci->second.end())
                {
                    const CvPlot* pPlot = gGlobals.getMap().plot(cii->first.iX, cii->first.iY);
                    if (!pPlot->isVisible(player_.getTeamID(), false))
                    {
#ifdef ALTAI_DEBUG
                        os << "\nStack at: " << cii->first << " no longer visible, size =  " << cii->second.size();
#endif
                        hideUnits_(pPlot);
                        ci->second.erase(cii++);
                    }
                    else
                    {
                        bool foundHostiles = false, foundOurUnits = false, foundTeamUnits = false, foundOtherUnits = false;
                    
                        for (std::set<IDInfo>::iterator ui(cii->second.begin()), uiEnd(cii->second.end()); ui != uiEnd; ++ui)
                        {
                            const CvUnit* pUnit = ::getUnit(*ui);
                            if (pUnit)
                            {
                                if (pUnit->getOwner() == player_.getPlayerID())
                                {
                                    if (!foundOurUnits)
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\nOur stack at: " << cii->first << " ";
#endif
                                        foundOurUnits = true;
                                    }
                                }
                                else if (pUnit->getTeam() == player_.getTeamID())
                                {
                                    if (!foundTeamUnits)
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\nTeam stack at: " << cii->first << " ";
#endif
                                        foundTeamUnits = true;
                                    }
                                }
                                else if (ourTeam.isAtWar(pUnit->getTeam()))
                                {
                                    if (!foundHostiles)
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\nHostile stack at: " << cii->first << " ";
                                        if (pUnit->plot()->getCoords() != cii->first)
                                        {
                                            os << " bad location - unit actually at: " << pUnit->plot()->getCoords();
                                        }
#endif
                                        foundHostiles = true;
                                    }

                                    enemyStacks_[cii->first].push_back(pUnit->getIDInfo());
                                    addHistory_(pUnit->getIDInfo(), pUnit->plot());  // bit of a hack to make sure units which don't move are still up to date - need to improve this logic
                                }
                                else
                                {
                                    if (!foundOtherUnits)
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\nOther stack at: " << cii->first << " ";
#endif
                                        foundOtherUnits = true;
                                    }
                                }
#ifdef ALTAI_DEBUG
                                os << " " << pUnit->getIDInfo() << ":" << gGlobals.getUnitInfo(pUnit->getUnitType()).getType() << ",";
#endif
                            }
                        }
                        ++cii;
                    }                    
                }
            }
#ifdef ALTAI_DEBUG
            for (std::map<int, std::map<IDInfo, XYCoords> >::const_iterator areaIter(hiddenUnitsMap_.begin()); areaIter != hiddenUnitsMap_.end(); ++areaIter)
            {
                os << "\nSub area: " << areaIter->first << " has: " << areaIter->second.size() << " hidden units";
                for (std::map<IDInfo, XYCoords>::const_iterator unitsIter(areaIter->second.begin()); unitsIter != areaIter->second.end(); ++unitsIter)
                {
                    const CvUnit* pUnit = ::getUnit(unitsIter->first);
                    if (pUnit)
                    {
                        os << "\n\t player: " << unitsIter->first.eOwner << " " << gGlobals.getUnitInfo(pUnit->getUnitType()).getType() << ", "
                            << " last seen at: " << unitsIter->second << ", now at: " << pUnit->plot()->getCoords();
                    }
                    else
                    {
                        os << "\n\t unit not found: " << unitsIter->first << " last seen at: " << unitsIter->second;
                    }
                }
            }
#endif
        }

        void eraseUnit_(const IDInfo& info, const CvPlot* pPlot)
        {
            std::map<int, std::map<IDInfo, XYCoords> >::iterator unitsAreaIter = unitsMap_.find(pPlot->getSubArea());
            if (unitsAreaIter != unitsMap_.end())
            {
                std::map<IDInfo, XYCoords>::iterator unitIter = unitsAreaIter->second.find(info);
                if (unitIter != unitsAreaIter->second.end())
                {
                    unitsAreaIter->second.erase(unitIter);
                }
            }

            std::map<int, std::map<XYCoords, std::set<IDInfo> > >::iterator stackIter = unitStacks_.find(pPlot->getSubArea());
            if (stackIter != unitStacks_.end())
            {
                std::map<XYCoords, std::set<IDInfo> >::iterator subAreaStackIter = stackIter->second.find(pPlot->getCoords());
                if (subAreaStackIter != stackIter->second.end())
                {
                    subAreaStackIter->second.erase(info);
                    if (subAreaStackIter->second.empty())
                    {
                        stackIter->second.erase(subAreaStackIter);
                    }
                }
            }
        }

        XYCoords addUnit_(const IDInfo& info, const CvPlot* pPlot)
        {
            unitsMap_[pPlot->getSubArea()][info] = pPlot->getCoords();
            unitStacks_[pPlot->getSubArea()][pPlot->getCoords()].insert(info);
            return addHistory_(info, pPlot);
        }

        void hideUnit_(const IDInfo& info, const CvPlot* pPlot)
        {
            hiddenUnitsMap_[pPlot->getSubArea()][info] = pPlot->getCoords();
        }

        void hideUnits_(const CvPlot* pPlot)
        {
            std::map<int, std::map<XYCoords, std::set<IDInfo> > >::iterator areaIter = unitStacks_.find(pPlot->getSubArea());
            if (areaIter != unitStacks_.end())
            {
                std::map<XYCoords, std::set<IDInfo> >::iterator plotsIter = areaIter->second.find(pPlot->getCoords());
                if (plotsIter != areaIter->second.end())
                {
                    for (std::set<IDInfo>::iterator unitsIter(plotsIter->second.begin()), unitsEndIter(plotsIter->second.end()); unitsIter != unitsEndIter; ++unitsIter)
                    {
                        hideUnit_(*unitsIter, pPlot);
                    }
                }
            }
        }

        XYCoords addHistory_(const IDInfo& info, const CvPlot* pPlot)
        {
            XYCoords previousCoords(-1, -1);
            std::map<IDInfo, UnitHistory>::iterator hIter = unitHistories_.find(info);
            if (hIter != unitHistories_.end())
            {                
                if (!hIter->second.locationHistory.empty())
                {
                    previousCoords = hIter->second.locationHistory.begin()->second;
                    if (previousCoords == pPlot->getCoords())
                    {
                        // updating last turn seen to current one
                        hIter->second.locationHistory.begin()->first = gGlobals.getGame().getGameTurn();
                        return previousCoords;  // only add new coordinates
                    }
                }
                hIter->second.locationHistory.push_front(std::make_pair(gGlobals.getGame().getGameTurn(), pPlot->getCoords()));
            }
            else
            {
                unitHistories_.insert(std::make_pair(info, UnitHistory(info, pPlot->getCoords())));
            }
            return previousCoords;
        }

        XYCoords unhideUnit_(const IDInfo& info, int areaHint)
        {
            std::map<int, std::map<IDInfo, XYCoords> >::iterator unitsAreaMapIter = hiddenUnitsMap_.end();
            std::map<IDInfo, XYCoords>::iterator unitsMapIter;
            XYCoords foundCoords(-1, -1);

            if (areaHint != FFreeList::INVALID_INDEX)
            {
                unitsAreaMapIter = hiddenUnitsMap_.find(areaHint);
                if (unitsAreaMapIter != hiddenUnitsMap_.end())
                {
                    unitsMapIter = unitsAreaMapIter->second.find(info);
                    if (unitsMapIter != unitsAreaMapIter->second.end())
                    {
                        foundCoords = unitsMapIter->second;
                        unitsAreaMapIter->second.erase(unitsMapIter);
                        return foundCoords;
                    }
                }
            }            
            
            for (unitsAreaMapIter = hiddenUnitsMap_.begin(); unitsAreaMapIter != hiddenUnitsMap_.end(); ++unitsAreaMapIter)
            {
                unitsMapIter = unitsAreaMapIter->second.find(info);
                if (unitsMapIter != unitsAreaMapIter->second.end())
                {
                    foundCoords = unitsMapIter->second;
                    unitsAreaMapIter->second.erase(unitsMapIter);
                    return foundCoords;
                }
            }
            return foundCoords;
        }

        MilitaryMissionDataPtr addHostileUnitMission_(int subArea, XYCoords currentCoords, const IDInfo& info)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            MilitaryMissionDataPtr pMission, pExistingMission, pFoundMission;

            const CvPlot* pUnitPlot = gGlobals.getMap().plot(currentCoords.iX, currentCoords.iY);
            const CvUnit* pUnit = ::getUnit(info);            
            FAssert(pUnit != NULL && pUnit->plot() == pUnitPlot && pUnitPlot->getSubArea() == subArea);
            bool isCity = pUnitPlot->isCity();

            std::vector<const CvUnit*> enemyStack;
            std::map<int, std::map<XYCoords, std::set<IDInfo> > >::const_iterator stackAreaIter = unitStacks_.find(subArea);
            FAssert(stackAreaIter != unitStacks_.end());
            std::map<XYCoords, std::set<IDInfo> >::const_iterator stackIter = stackAreaIter->second.find(currentCoords);
            FAssert(stackIter != stackAreaIter->second.end());  // expect unit to exist in the unit stacks map if we're here
            bool isAnimalStack = true;
            for (std::set<IDInfo>::const_iterator unitsIter = stackIter->second.begin(), unitsEndIter = stackIter->second.end(); unitsIter != unitsEndIter; ++unitsIter)
            {
                const CvUnit* pUnit = ::getUnit(*unitsIter);
                if (pUnit)
                {
                    enemyStack.push_back(pUnit);
                    isAnimalStack = isAnimalStack && pUnit->isAnimal();
                }
            }
            
            // look for an existing mission for this (hostile) unit
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator thisUnitMissionIter = hostileUnitsMissionMap_.find(info);
            const bool unitHasExistingMission = thisUnitMissionIter != hostileUnitsMissionMap_.end();
            if (unitHasExistingMission)
            {
                // should we check for other missions on this plot and merge if found?
                pExistingMission = thisUnitMissionIter->second;
            }

            // any missions on the same plot? look at stack on this plot, and check each unit's mission
            bool foundMissionAtPlot = false;
            for (std::set<IDInfo>::const_iterator unitsIter = stackIter->second.begin(), unitsEndIter = stackIter->second.end(); unitsIter != unitsEndIter; ++unitsIter)
            {
                if (*unitsIter != info)  // add unit to existing (counter?) mission as targets share the same plot
                {
                    std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = hostileUnitsMissionMap_.find(*unitsIter);
                    if (missionIter != hostileUnitsMissionMap_.end())
                    {
                        pFoundMission = missionIter->second;  // found (hostile unit) mission at this plot
                        foundMissionAtPlot = true;
                        break;
                    }
                }
            }

            if (!foundMissionAtPlot && pUnitPlot->isCity())
            {
                MissionFinder missionFinder;
                missionFinder.missionType = MISSIONAI_COUNTER_CITY;
                missionFinder.city = pUnitPlot->getPlotCity()->getIDInfo();
                std::list<MilitaryMissionDataPtr>::const_iterator missionsIter = std::find_if(missionList_.begin(), missionList_.end(), missionFinder);
                if (missionsIter != missionList_.end())
                {
                    pFoundMission = *missionsIter;
                    foundMissionAtPlot = true;
                }
            }

            const bool unitHasNewMission = !foundMissionAtPlot && !unitHasExistingMission;
            if (unitHasExistingMission) // existing mission
            {
                pMission = pExistingMission;  // means we don't join even if we could - todo sort this out
            }
            else if (foundMissionAtPlot)  // no existing mission for unit, but found a mission on the unit's plot
            {
                if (pFoundMission->missionType == MISSIONAI_COUNTER_CITY)
                {
                    pFoundMission->missionType = MISSIONAI_COUNTER;
                }
                pFoundMission->targetUnits.insert(info);
                pMission = pFoundMission;
            }
            else if (unitHasNewMission) // no existing mission and no mission found at plot to join                
            {
                pMission = addMission_(MISSIONAI_COUNTER);
                pMission->targetUnits.insert(info);
            }
            else  // mission found to join and no existing mission
            {
                pMission = pFoundMission;
                pMission->targetUnits.insert(info);
            }

            if (foundMissionAtPlot || unitHasNewMission)
            {
                bool isImmininentThreat = false;

                if (!isAnimalStack)
                {
                    CombatGraph::Data endStates = getNearestCityAttackOdds(pMission);
                    //if (endStates.pWin > hostileAttackThreshold)
                    {
                        isImmininentThreat = true;
                    }
                }

                // update required units to reflect possibly new hostile stack composition
                std::set<IDInfo> availableUnits = getAssignedUnits(MISSIONAI_RESERVE, pUnitPlot->getSubArea());
                availableUnits.insert(pMission->assignedUnits.begin(), pMission->assignedUnits.end());  // and any units already assigned if existing mission
                RequiredUnitStack requiredUnitStack = isImmininentThreat ? getRequiredUnits(player_, pUnitPlot, enemyStack, availableUnits) : RequiredUnitStack();
                pMission->requiredUnits = requiredUnitStack.unitsToBuild;

                for (size_t i = 0, count = requiredUnitStack.existingUnits.size(); i < count; ++i)
                {
                    // check not already assigned
                    if (pMission->assignedUnits.find(requiredUnitStack.existingUnits[i]) == pMission->assignedUnits.end())
                    {
                        reassignOurUnit_(::getUnit(requiredUnitStack.existingUnits[i]), pMission);
                    }
                }

                if (!unitHasExistingMission)
                {
                    addHostileUnitMissionToMap_(pMission, info);
                }
            }
#ifdef ALTAI_DEBUG
            if (unitHasNewMission)
            {
                os << "\nnew counter mission for target unit: " << info;
                pMission->debug(os);
            }
            if (foundMissionAtPlot && unitHasExistingMission)
            {
                os << " had existing mission and also found mission on plot ";
                pFoundMission->debug(os);
            }                
#endif
            FAssert(pMission != NULL);

            pMission->updateReachablePlots(player_, false, false, true);

            if (!isAnimalStack)
            {
                std::set<IDInfo> nearbyCities = getNearbyCities(player_, subArea, pMission->hostilesReachablePlots);
                for (std::set<IDInfo>::const_iterator iter(nearbyCities.begin()), endIter(nearbyCities.end()); iter != endIter; ++iter)
                {
                    pCityDefenceAnalysis_->noteHostileStack(*iter, enemyStack);
                }
            }

            return pMission;
        }

        MilitaryMissionDataPtr addEscortUnitMission_(CvUnit* pSettlerUnit, CvUnit* pPotentialEscortUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            MilitaryMissionDataPtr pMission;
            bool isNewMission = false;
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(pSettlerUnit->getIDInfo());
            if (missionIter == ourUnitsMissionMap_.end())
            {
                if (pSettlerUnit->isFound())
                {
                    pMission = addMission_(MISSIONAI_ESCORT);
                    pMission->assignUnit(pSettlerUnit, false);
                    addOurUnitMission_(pMission, pSettlerUnit->getIDInfo());
                    isNewMission = true;
#ifdef ALTAI_DEBUG
                    os << "\nnew escort mission for unit: " << pSettlerUnit->getIDInfo();
                    pMission->debug(os);
#endif
                }
            }
            else
            {
                pMission = missionIter->second;
            }

            if (pMission)
            {
                // see if we can find an escort unit
                std::set<IDInfo> reserveUnits = getAssignedUnits(MISSIONAI_RESERVE, pSettlerUnit->plot()->getSubArea());
#ifdef ALTAI_DEBUG
                os << "\n(addEscortUnitMission_) available reserve units: ";
                for (std::set<IDInfo>::const_iterator ci(reserveUnits.begin()), ciEnd(reserveUnits.end()); ci != ciEnd; ++ci)
                {
                    os << *ci << " ";
                }
#endif

                int bestUnitScore = 0, bestReserveUnitScore = 0, potentialEscortScore = 0;
                DependencyItemSet di;
                di.insert(DependencyItem(-1, -1));
                const TacticSelectionData& tacticData = player_.getAnalysis()->getPlayerTactics()->tacticSelectionDataMap[di];
                UnitTypes bestEscortUnitType = NO_UNIT;
                std::map<UnitTypes, int> unitValues;
                if (tacticData.cityDefenceUnits.empty())  // initially no cities - so no unit tactics created against them to use here - just work off unit's combat value
                {
                    for (std::set<IDInfo>::const_iterator ri(reserveUnits.begin()), riEnd(reserveUnits.end()); ri != riEnd; ++ri)
                    {
                        const CvUnit* pReserveUnit = ::getUnit(*ri);
                        if (pReserveUnit)
                        {
                            unitValues[pReserveUnit->getUnitType()] = std::max<int>(unitValues[pReserveUnit->getUnitType()], pReserveUnit->getUnitInfo().getCombat());
                        }
                    }
                }
                else
                {
                    for (std::set<UnitTacticValue>::const_iterator ti(tacticData.cityDefenceUnits.begin()), tiEnd(tacticData.cityDefenceUnits.end()); ti != tiEnd; ++ti)
                    {
                        // just take the max tactic value (some city's values may be higher if they can build more experienced units)
                        unitValues[ti->unitType] = std::max<int>(unitValues[ti->unitType], ti->unitAnalysisValue);
                        if (ti->unitAnalysisValue > bestUnitScore)
                        {
                            bestUnitScore = ti->unitAnalysisValue;
                            bestEscortUnitType = ti->unitType;
                        }
                    }
                }

                if (pPotentialEscortUnit)
                {
                    potentialEscortScore = unitValues[pPotentialEscortUnit->getUnitType()];
                }
#ifdef ALTAI_DEBUG
                os << "\nbest unit score: " << bestUnitScore << ", best unit type: " << bestEscortUnitType << ", potentialEscortScore = " << potentialEscortScore;
#endif

                IDInfo bestReserveEscortUnit;
                for (std::set<IDInfo>::const_iterator ri(reserveUnits.begin()), riEnd(reserveUnits.end()); ri != riEnd; ++ri)
                {
                    CvUnit* pReserveUnit = ::getUnit(*ri);
                    if (pReserveUnit)
                    {
                        int thisUnitScore = unitValues[pReserveUnit->getUnitType()];
                        if (thisUnitScore > bestReserveUnitScore)
                        {
                            bestReserveUnitScore = thisUnitScore;
                            bestReserveEscortUnit = *ri;
                            pPotentialEscortUnit = pReserveUnit;
                        }
                    }
                }
#ifdef ALTAI_DEBUG
                os << " bestReserveUnitScore: " << bestReserveUnitScore << ", bestReserveEscortUnit: " << bestReserveEscortUnit;
#endif

                if (pPotentialEscortUnit && bestReserveUnitScore > 0 && bestReserveUnitScore > 2 * bestUnitScore / 3)  // at least 2/3 of our best escort unit's score
                {
                    reassignOurUnit_(pPotentialEscortUnit, pMission);
                }
                else if (bestEscortUnitType != NO_UNIT && isNewMission)
                {
                    RequiredUnitStack::UnitDataChoices unitChoices;
                    for (std::map<UnitTypes, int>::const_iterator vi(unitValues.begin()), viEnd(unitValues.end()); vi != viEnd; ++vi)
                    {
#ifdef ALTAI_DEBUG
                        os << " checking escort unit: " << vi->first << " with value: " << vi->second;
#endif
                        if (vi->second > 2 * bestUnitScore / 3)
                        {
#ifdef ALTAI_DEBUG
                            os << " adding possible escort unit: " << vi->first;
#endif
                            unitChoices.push_back(UnitData(vi->first));
                        }
                    }
                    pMission->requiredUnits.push_back(unitChoices);
                }
            }
            
            return pMission;
        }

        MilitaryMissionDataPtr addScoutUnitMission_(const CvUnitAI* pUnit)
        {
            UnitAITypes unitAIType = pUnit->AI_getUnitAIType();
            if (unitAIType == UNITAI_EXPLORE || unitAIType == UNITAI_EXPLORE_SEA || pUnit->baseCombatStr() > 0)
            {
                DomainTypes unitDomain = pUnit->getDomainType();
                if (unitDomain == DOMAIN_LAND)
                {
                    XYCoords refCoords = getLandExploreRefPlot(pUnit);
                    if (refCoords != XYCoords(-1, -1))
                    {
                        MilitaryMissionDataPtr pMission = addMission_(MISSIONAI_EXPLORE);
                        pMission->assignUnit(pUnit);
                        pMission->targetCoords = refCoords;
                        addOurUnitMission_(pMission, pUnit->getIDInfo());
#ifdef ALTAI_DEBUG
                        std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
                        os << "\nnew explore mission for unit: " << pUnit->getIDInfo();
                        pMission->debug(os);
#endif
                        return pMission;
                    }
                }
                else if (unitDomain == DOMAIN_SEA)
                {
                    // todo - track number of sea explore units
                    MilitaryMissionDataPtr pMission = addMission_(MISSIONAI_EXPLORE);
                    pMission->assignUnit(pUnit);
                    addOurUnitMission_(pMission, pUnit->getIDInfo());
#ifdef ALTAI_DEBUG
                    std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
                    os << "\nnew sea explore mission for unit: " << pUnit->getIDInfo();
                    pMission->debug(os);
#endif
                    return pMission;
                }

                return MilitaryMissionDataPtr();
            }
            else
            {
                return MilitaryMissionDataPtr();
            }
        }

        MilitaryMissionDataPtr addWorkerUnitMission_(const CvUnitAI* pWorkerUnit)
        {
            UnitAITypes unitAIType = pWorkerUnit->AI_getUnitAIType();
            if (unitAIType == UNITAI_WORKER || unitAIType == UNITAI_WORKER_SEA)
            {
                MilitaryMissionDataPtr pMission = addMission_(MISSIONAI_BUILD);
                pMission->assignUnit(pWorkerUnit, false);
                addOurUnitMission_(pMission, pWorkerUnit->getIDInfo());
#ifdef ALTAI_DEBUG
                std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
                os << "\nnew worker mission for unit: " << pWorkerUnit->getIDInfo();
                pMission->debug(os);
#endif
                // see if we have a spare reserve unit to escort
                if (pWorkerUnit->getDomainType() == DOMAIN_LAND)
                {
                    // need to enhance this logic to work for coastal sub areas
                    std::set<IDInfo> reserveUnits = getAssignedUnits(MISSIONAI_RESERVE, pWorkerUnit->plot()->getSubArea());
                    if (!reserveUnits.empty())
                    {
                        CvUnit* pEscortUnit = ::getUnit(*reserveUnits.begin());
                        if (pEscortUnit)
                        {
#ifdef ALTAI_DEBUG
                            // might want to limit this largesse to early game or other particular situations
                            os << " assigning escort unit: " << pEscortUnit->getIDInfo();
#endif
                            MilitaryMissionDataPtr pEscortMission = addMission_(MISSIONAI_ESCORT_WORKER);
                            pEscortMission->targetUnits.insert(pWorkerUnit->getIDInfo());
                            reassignOurUnit_(pEscortUnit, pEscortMission);
                        }
                    }
                }

                return pMission;
            }

            return MilitaryMissionDataPtr();
        }

        MilitaryMissionDataPtr addCityGuardMission_(const CvCity* pCity)
        {
            MilitaryMissionDataPtr pMission = addMission_(MISSIONAI_GUARD_CITY);
            pMission->targetCity = pCity->getIDInfo();

            pMission->requiredUnits = pCityDefenceAnalysis_->getRequiredUnits(pCity->getIDInfo());

            return pMission;
        }

        MilitaryMissionDataPtr addEscortWorkerUnitMission_(const CvUnitAI* pUnit, const CvUnitAI* pWorkerUnit)
        {
            MilitaryMissionDataPtr pMission = addMission_(MISSIONAI_ESCORT_WORKER);
            pMission->assignUnit(pUnit, false);
            pMission->targetUnits.insert(pWorkerUnit->getIDInfo());
            addOurUnitMission_(pMission, pUnit->getIDInfo());
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nnew escort worker mission for unit: " << pUnit->getIDInfo() << " and worker unit: " << pWorkerUnit->getIDInfo();
            pMission->debug(os);
#endif
            return pMission;
        }

        MilitaryMissionDataPtr addReserveLandUnitMission_(const CvUnit* pUnit, const CvPlot* pTarget)
        {
            MilitaryMissionDataPtr pMission;
            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                if ((*missionsIter)->missionType == MISSIONAI_RESERVE)
                {
                    const CvCity* pTargetCity = (*missionsIter)->getTargetCity();
                    if (pTargetCity && pTargetCity->plot() == pTarget && pTargetCity->plot()->getSubArea() == pUnit->plot()->getSubArea())
                    {
                        pMission = *missionsIter;
                        break;
                    }
                    else if ((*missionsIter)->targetCoords == pTarget->getCoords())
                    {
                        pMission = *missionsIter;
                        break;
                    }
                }
            }

            if (!pMission)
            {
                pMission = addMission_(MISSIONAI_RESERVE);
                if (pTarget->isCity())
                {
                    pMission->targetCity = pTarget->getPlotCity()->getIDInfo();
                }
                else
                {
                    pMission->targetCoords = pTarget->getCoords();
                }
            }
            pMission->assignUnit(pUnit, false);
            addOurUnitMission_(pMission, pUnit->getIDInfo());
            return pMission;
        }

        MilitaryMissionDataPtr assignUnitToCombatMission_(const CvUnitAI* pUnit)
        {
            const int subArea = pUnit->plot()->getSubArea();
            std::map<int, std::map<IDInfo, XYCoords> >::const_iterator unitsAreaIter = unitsMap_.find(subArea);
            std::map<int, std::map<IDInfo, XYCoords> >::const_iterator hiddenUnitsAreaIter = hiddenUnitsMap_.find(subArea);
            if (unitsAreaIter == unitsMap_.end())
            {
                return MilitaryMissionDataPtr();  //assert this? shouldn't ever happen as we should at least have our own unit?
            }            

            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                if (!((*missionsIter)->missionType == MISSIONAI_COUNTER || (*missionsIter)->missionType == MISSIONAI_COUNTER_CITY))
                {
                    continue;
                }

                for (std::set<IDInfo>::const_iterator targetsIter((*missionsIter)->targetUnits.begin()); targetsIter != (*missionsIter)->targetUnits.end(); ++targetsIter)
                {
                    // need a way to ask if the mission relates to this sub area
                    if (unitsAreaIter->second.find(*targetsIter) != unitsAreaIter->second.end() || hiddenUnitsAreaIter->second.find(*targetsIter) != hiddenUnitsAreaIter->second.end())
                    {
                        int requiredCount = (*missionsIter)->getRequiredUnitCount(pUnit->getUnitType());
                        if (requiredCount > 0)
                        {
                            (*missionsIter)->assignUnit(pUnit);
                            ourUnitsMissionMap_[pUnit->getIDInfo()] = *missionsIter;
                            return *missionsIter;
                        }
                    }
                }
            }
            return MilitaryMissionDataPtr();
        }

        void reassignOurUnit_(CvUnit* pUnit, const MilitaryMissionDataPtr& pMission)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            CvSelectionGroup* pGroup = pUnit->getGroup();
            if (pGroup->getNumUnits() > 1)
            {
                pUnit->joinGroup(NULL);  // so we don't have units in same group in different missions
#ifdef ALTAI_DEBUG
                os << "\nRemoving unit from group: " << pGroup->getID() << " new group: " << pUnit->getGroupID();
#endif
            }

            pMission->assignUnit(pUnit);
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator unitMissionIter = ourUnitsMissionMap_.find(pUnit->getIDInfo());
            if (unitMissionIter != ourUnitsMissionMap_.end())
            {
#ifdef ALTAI_DEBUG
                os << "\nReassigning unit: " << pUnit->getIDInfo() << " from mission: ";
                unitMissionIter->second->debug(os);
#endif
                unitMissionIter->second->unassignUnit(pUnit, false);
                unitMissionIter->second = pMission;
#ifdef ALTAI_DEBUG
                os << " to mission: ";
                pMission->debug(os);
#endif
            }
            else
            {
                addOurUnitMission_(pMission, pUnit->getIDInfo());
            }
        }

        void eraseMission_(const MilitaryMissionDataPtr& pMission)
        {                   
            std::list<MilitaryMissionDataPtr>::iterator missionListIter = std::find(missionList_.begin(), missionList_.end(), pMission);
            FAssert(missionListIter != missionList_.end());
            if (missionListIter != missionList_.end())
            {
                removeMission_(missionListIter);
            }
#ifdef ALTAI_DEBUG
            else
            {
                std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
                os << " mission missing from list: ";
                pMission->debug(os);
            }
#endif
        }

        void removeMission_(const std::list<MilitaryMissionDataPtr>::iterator& missionsIter)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nRemoving mission: (" << *missionsIter << ")";
            (*missionsIter)->debug(os);
#endif
            (*missionsIter)->requiredUnits.clear();
            (*missionsIter)->ourReachablePlots.clear();
            (*missionsIter)->hostilesReachablePlots.clear();

            std::set<IDInfo> assignedUnits = (*missionsIter)->assignedUnits;
#ifdef ALTAI_DEBUG
            os << " erasing mission from list (size = " << missionList_.size() << ") ";
#endif
            missionList_.erase(missionsIter);
#ifdef ALTAI_DEBUG
            os << "... (size = " << missionList_.size() << ") ";
#endif

            for (std::set<IDInfo>::iterator assignedUnitsIter(assignedUnits.begin()),
                assignedUnitsEndIter(assignedUnits.end()); assignedUnitsIter != assignedUnitsEndIter;
                ++assignedUnitsIter)
            {
                ourUnitsMissionMap_.erase(*assignedUnitsIter);
                CvUnitAI* pUnit = (CvUnitAI*)player_.getCvPlayer()->getUnit(assignedUnitsIter->iID);
                if (pUnit)
                {
                    // try and find a new mission
                    MilitaryMissionDataPtr pMission;
                    if (pUnit->plot()->isCity(false, player_.getTeamID()) && pUnit->plot()->getPlotCity()->getOwner() == player_.getPlayerID())
                    {
                        pMission = checkCityGuard_(pUnit, pUnit->plot()->getPlotCity());
                    }
                    if (!pMission)
                    {
                        pMission = assignUnitToCombatMission_(pUnit);
                    }
                    if (!pMission)
                    {
                        pMission = checkEscort_(pUnit, pUnit->plot());
                    }
                    if (!pMission)
                    {
                        pMission = addReserveLandUnitMission_(pUnit, player_.getCvPlayer()->getCapitalCity()->plot());
                    }
                    if (pMission)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nReassigned our unit to mission structure: ";
                        pMission->debug(os);
#endif
                    }
                }
            }
        }

        MilitaryMissionDataPtr addMission_(MissionAITypes missionType)
        {
            MilitaryMissionDataPtr pMission(new MilitaryMissionData(missionType));
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nAdding mission: " << pMission << " " << getMissionAIString(missionType);
#endif
            missionList_.push_back(pMission);
            return pMission;
        }

        void addOurUnitMission_(const MilitaryMissionDataPtr& pMission, IDInfo unit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();            
#endif
            bool inserted =  ourUnitsMissionMap_.insert(std::make_pair(unit, pMission)).second;
#ifdef ALTAI_DEBUG
            os << " result of adding mission to our units map for unit: " << unit << ", mission: " << pMission << " = " << inserted;
#endif
        }

        void addHostileUnitMissionToMap_(const MilitaryMissionDataPtr pMission, IDInfo unit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();            
#endif
            bool inserted = hostileUnitsMissionMap_.insert(std::make_pair(unit, pMission)).second;
#ifdef ALTAI_DEBUG
            os << " result of adding mission to hostile units map for unit: " << unit << ", mission: " << pMission << " = " << inserted;
#endif
        }

        bool doUnitCounterMission(const MilitaryMissionDataPtr& pMission, CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            const int currentTurn = gGlobals.getGame().getGameTurn();
#endif
            const CvPlot* pUnitPlot = pUnit->plot();
            pMission->updateReachablePlots(player_, true, false, true);
            pMission->pushedAttack = std::pair<IDInfo, XYCoords>();

            const CvPlot* pTargetPlot = NULL;
            bool targetIsCity = false;
            PlotUnitsMap targetStacks;

            for (std::set<IDInfo>::const_iterator targetsIter(pMission->targetUnits.begin()), targetsEndIter(pMission->targetUnits.end());
                targetsIter != targetsEndIter; ++targetsIter)
            {
                const CvUnit* pTargetUnit = ::getUnit(*targetsIter);
                if (pTargetUnit)
                {
                    pTargetPlot = pTargetUnit->plot();
                    if (pTargetPlot->isVisible(player_.getTeamID(), false))
                    {
                        targetStacks[pTargetPlot].push_back(pTargetUnit);
                    }
                }
            }

            if (!targetStacks.empty())
            {
                pMission->updateReachablePlots(player_, false, false, true);
                pMission->updateRequiredUnits(player_, std::set<IDInfo>());
            }
            else
            {
                std::map<const CvPlot*, std::vector<IDInfo>, CvPlotOrderF> possibleTargetStacks;
                for (std::set<IDInfo>::const_iterator targetsIter(pMission->targetUnits.begin()), targetsEndIter(pMission->targetUnits.end());
                targetsIter != targetsEndIter; ++targetsIter)
                {
                    std::map<IDInfo, UnitHistory>::const_iterator histIter = unitHistories_.find(*targetsIter);
                    FAssertMsg(histIter != unitHistories_.end() && !histIter->second.locationHistory.empty(), "Target unit without any history?");
                    if (histIter != unitHistories_.end() && !histIter->second.locationHistory.empty())
                    {
                        const XYCoords coords = histIter->second.locationHistory.begin()->second;
                        possibleTargetStacks[gGlobals.getMap().plot(coords.iX, coords.iY)].push_back(*targetsIter);
                    }
                }

                if (!possibleTargetStacks.empty())
                {
#ifdef ALTAI_DEBUG
                    os << "\n selected possible target: " << possibleTargetStacks.begin()->first->getCoords();
#endif
                    pTargetPlot = possibleTargetStacks.begin()->first;
                    targetIsCity = pTargetPlot->isCity();
                }
                else
                {
                    pTargetPlot = pMission->getTargetPlot();
                }
            }

            PlotSet sharedPlots, borderPlots;
            getOverlapAndNeighbourPlots(pMission->hostilesReachablePlots, pMission->ourReachablePlots, sharedPlots, borderPlots);

#ifdef ALTAI_DEBUG
            pMission->debugReachablePlots(os);
            os << "\n attackable plots: " << sharedPlots.size() << ", bordering plots: " << borderPlots.size() << ", assigned units count: " << pMission->assignedUnits.size();
#endif
            bool isAttackMove = !isEmpty(pMission->nextAttack.first);
            if (isAttackMove)
            {

                if (pTargetPlot && pMission->nextAttack.second != pTargetPlot->getCoords())
                {
#ifdef ALTAI_DEBUG
                    os << "\n target plot from mission: " << pTargetPlot->getCoords() << " overridding to: " << pMission->nextAttack.second;
#endif
                    pTargetPlot = gGlobals.getMap().plot(pMission->nextAttack.second);
                }
            }

            bool joinClosestGroupIfNotAttacking = false;
            XYCoords moveToCoords = !isEmpty(pMission->nextAttack.second) ? pMission->nextAttack.second : (pTargetPlot ? pTargetPlot->getCoords() : XYCoords(-1, -1));
            CvSelectionGroup* pClosestGroup = NULL;

            if (!pTargetPlot && isEmpty(moveToCoords))
            {
#ifdef ALTAI_DEBUG
                os << "\nUnable to determine target plot for counter mission";
#endif
                return false;
            }

            bool finishedAttack = pUnit->isMadeAttack() && !pUnit->isBlitz();
            if (finishedAttack)
            {
                isAttackMove = false;
            }

            std::map<XYCoords, CombatGraph::Data>::iterator oddsIter = attackCombatData_.find(pTargetPlot->getCoords());
            if (!finishedAttack && oddsIter != attackCombatData_.end() && (oddsIter->second.pWin > attThreshold || !isEmpty(pMission->nextAttack.first)))
            {
                moveToCoords = pTargetPlot->getCoords();
                isAttackMove = true;
                FAssertMsg(*oddsIter->second.longestAndShortestAttackOrder.first.begin() == pUnit->getIDInfo(), "Unexpected attack unit?");
                oddsIter->second.longestAndShortestAttackOrder.first.erase(oddsIter->second.longestAndShortestAttackOrder.first.begin());
#ifdef ALTAI_DEBUG
                os << "\nPlot: " << moveToCoords << " has combined attack odds of: ";
                pMission->ourAttackOdds.debug(os);
                os << "\n|attackCombatData_: ";
                oddsIter->second.debug(os);
                os << "\n\t first attacker: " << pMission->firstAttacker << " ";
                const CvUnit* pDebugUnit = ::getUnit(pMission->firstAttacker);
                if (pDebugUnit) os << pDebugUnit->getUnitInfo().getType();
#endif
            }
            else
            {
                std::set<CvSelectionGroup*, CvSelectionGroupOrderF> groupsInMission = pMission->getAssignedGroups();
                std::list<std::pair<CvSelectionGroup*, UnitPathData> > groupPathData;
                std::list<std::pair<CvSelectionGroup*, UnitPathData> >::const_iterator ourGroupIter;                
                int closestGroupTurnsToTarget = MAX_INT;
                GroupCombatData combatData;
                // need this as attack and defence combat map only cover static defence and active attacks, not moving for defence
                combatData.calculate(player_, pMission->getAssignedUnits(true));

                // iterate over all groups in mission
                for (std::set<CvSelectionGroup*, CvSelectionGroupOrderF>::const_iterator groupIter(groupsInMission.begin()), groupEndIter(groupsInMission.end());
                    groupIter != groupEndIter; ++groupIter)
                {                    
                    UnitPathData unitPathData;
                    unitPathData.calculate(*groupIter, pTargetPlot, MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY);

                    if (unitPathData.valid)
                    {
                        if (unitPathData.pathTurns < closestGroupTurnsToTarget)
                        {
                            pClosestGroup = *groupIter;
                            closestGroupTurnsToTarget = unitPathData.pathTurns;
                        }
                    }

                    std::list<std::pair<CvSelectionGroup*, UnitPathData> >::iterator groupPathIter = groupPathData.insert(groupPathData.begin(), std::make_pair(*groupIter, unitPathData));

                    if (*groupIter ==  pUnit->getGroup())
                    {
                        // note our group's list positions
                        ourGroupIter = groupPathIter;
                    }
#ifdef ALTAI_DEBUG
                    os << "\nAdded group: " << (*groupIter)->getID() << " turns to target: " << unitPathData.pathTurns;
#endif
                }
            
                
                if (pClosestGroup && pUnit->getGroup() != pClosestGroup && pUnitPlot == pClosestGroup->plot())
                {
#ifdef ALTAI_DEBUG
                    os << "\nUnit: " << pUnit->getIDInfo() << " will join group: " << pClosestGroup->getID() << " if not attacking";
#endif
                    joinClosestGroupIfNotAttacking = true;
                    // now we split the group containing selected attack unit to prevent group attack
                    // need to avoid this rejoin logic as it causes regrouping of remaining units back to the surviving attack unit
                    // this means they skip their attack slot as getNextAttackUnit will move onto the next unit when called again
                    // (the joinGroup uses up their first update slice)
                    //pUnit->joinGroup(pClosestGroup);  
                    //return true;
                }

                // determine in general where we want to go this turn
                XYCoords stepCoords = ourGroupIter->second.getFirstTurnEndCoords();
                groupPathData.sort(GroupPathComp());  // sort based on path turns
                std::list<std::pair<CvSelectionGroup*, UnitPathData> >::const_iterator refGroupIter = groupPathData.begin();  // closest group            

                // if one group - just direct towards target
                // if multiple groups, possibly direct those further away to group with closest group
                if (refGroupIter->first != ourGroupIter->first)  // our unit is not in the closest group - see if it's better to try and join the closest group to the target along its route
                {
                    const int ourGroupTurnsToReachTarget = ourGroupIter->second.pathTurns;
                    int ourBestTurnCount = ourGroupTurnsToReachTarget;
                    // iterate over each node in closest group's path to target
                    for (std::list<UnitPathData::Node>::const_iterator nodeIter(refGroupIter->second.nodes.begin()); nodeIter != refGroupIter->second.nodes.end(); ++nodeIter)
                    {
                        if (nodeIter->movesLeft == 0)  // an end of turn plot
                        {
                            XYCoords refGroupStepCoords = nodeIter->coords;
                            UnitPathData unitPathData;
                            unitPathData.calculate(ourGroupIter->first, gGlobals.getMap().plot(refGroupStepCoords.iX, refGroupStepCoords.iY), 
                                MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY);
                            // if our turn count to this plot on closest group's route is less than or equal than that group (so we can wait or meet)
                            // and turn count is less than our turn count to the target (so we're not going past the target to meet up)
                            // of possible coords - pick those which are closest to our current location
                            if (unitPathData.pathTurns <= nodeIter->turn && unitPathData.pathTurns < ourGroupTurnsToReachTarget && unitPathData.pathTurns < ourBestTurnCount)
                            {
                                ourBestTurnCount = unitPathData.pathTurns;
                                stepCoords = unitPathData.getFirstTurnEndCoords();
#ifdef ALTAI_DEBUG
                                os << "\nSelected wait coords: " << stepCoords << " with turn count: " << ourBestTurnCount;
#endif
                            }
                        }
                    }
                }
            
                std::list<CombatMoveData> noAttackMoveData, attackMoveData;
                
                for (PlotSet::const_iterator ci(pMission->ourReachablePlots.begin()), ciEnd(pMission->ourReachablePlots.end()); ci != ciEnd; ++ci)
                {
                    XYCoords coords = (*ci)->getCoords();
                    const CvPlot* pReachablePlot = gGlobals.getMap().plot(coords);
                    MilitaryMissionData::ReachablePlotDetails::const_iterator ri = pMission->ourReachablePlotDetails.find(pReachablePlot);
                    if (ri != pMission->ourReachablePlotDetails.end())
                    {
                        if (std::find(ri->second.begin(), ri->second.end(), pUnit->getIDInfo()) == ri->second.end())
                        {
                            continue;  // this plot is not reachable by this unit
                        }
                    }

                    CombatGraph::Data thisPlotDefenceOdds = combatData.getCombatData(coords, false), thisPlotAttackOdds = combatData.getCombatData(coords, true);
                    CombatMoveData moveData(coords, stepDistance(coords.iX, coords.iY, stepCoords.iX, stepCoords.iY), thisPlotDefenceOdds, thisPlotAttackOdds);
#ifdef ALTAI_DEBUG
                    os << "\nodds for plot: " << coords;
                    if (!thisPlotAttackOdds.isEmpty())
                    {
                        os << "\n\tattack: ";
                        thisPlotAttackOdds.debug(os);
                    }
                    if (!thisPlotDefenceOdds.isEmpty())
                    {
                        os << "\n\tdefence: ";
                        thisPlotDefenceOdds.debug(os);
                    }
                    if (thisPlotAttackOdds.isEmpty() && thisPlotDefenceOdds.isEmpty())
                    {
                        os << " (none) ";
                    }
#endif
                    if (thisPlotAttackOdds.isEmpty())
                    {                                
                        noAttackMoveData.push_back(moveData);
                    }
                    else
                    {
                        attackMoveData.push_back(moveData);
                    }
                }
            
                if (!finishedAttack && !attackMoveData.empty())
                {
                    attackMoveData.sort(AttackMoveDataPred(attThreshold));
                    const CombatMoveData& moveData = *attackMoveData.begin();
                    if (moveData.attackOdds.pWin > attThreshold)
                    {
                        if (moveData.coords == pTargetPlot->getCoords())
                        {
#ifdef ALTAI_DEBUG
                            os << " matched attack move from mission: ";
                            moveData.attackOdds.debug(os);
#endif
                            moveToCoords = moveData.coords;
                            isAttackMove = true;
                        }
                    }
                }

                if (moveToCoords == XYCoords())
                {
                    noAttackMoveData.sort(DefenceMoveDataPred(hostileAttackThreshold));
#ifdef ALTAI_DEBUG
                    for (std::list<CombatMoveData>::const_iterator cmi(noAttackMoveData.begin()), cmiEnd(noAttackMoveData.end()); cmi != cmiEnd; ++cmi)
                    {
                        os << "\n\t" << cmi->coords << " sd = " << cmi->stepDistanceFromTarget;
                        if (!cmi->defenceOdds.isEmpty())
                        {
                            os << "\n";
                            cmi->defenceOdds.debug(os);
                        }
                    }
#endif
                    std::list<CombatMoveData>::const_iterator noAttackIter = noAttackMoveData.begin();
                    float bestSurvivalOdds = noAttackIter->defenceOdds.pLoss;  // odds are for us surviving hostiles attacking us
                    // don't wait in position where have river crossing attack penalty on target
                    // prob want to adjust this to wait until just before ready to attack, e.g. still grouping units
                    while (pTargetPlot->isRiverCrossing(directionXY(pTargetPlot, gGlobals.getMap().plot(noAttackIter->coords.iX, noAttackIter->coords.iY))))
                    {
                        ++noAttackIter;
                        if (noAttackIter == noAttackMoveData.end())
                        {
                            --noAttackIter;
                            break;
                        }
                        if (noAttackIter != noAttackMoveData.begin() && noAttackIter->defenceOdds.pLoss < defThreshold && noAttackIter->defenceOdds.pLoss < bestSurvivalOdds)
                        {
                            --noAttackIter;
                            break;
                        }
                    }
#ifdef ALTAI_DEBUG
                    os << "\n\t no attack move data first coords: " << noAttackMoveData.begin()->coords;
                    if (noAttackIter != noAttackMoveData.begin())
                    {
                        os << "\nmoving coords from: " << noAttackMoveData.begin()->coords << " to avoid river crossing: " << noAttackIter->coords;
                    }
#endif
                    moveToCoords = noAttackIter->coords;
                }
            }

            // if we have a nearby group and we've already attacked (and aren't blitz) join it.
            

            if (joinClosestGroupIfNotAttacking && (!isAttackMove || finishedAttack))
            {
                pUnit->joinGroup(pClosestGroup);
                return true;
            }
            else if (finishedAttack)  // units with moves remaining that have attacked but no coincident stack to rejoin
            {
                // attempt to create a new group to effectively hold unit until rest of attack is finished
                // which means we can avoid finishing this unit's moves and instead return to the next selection group in the cycle
                // can then hopefully move or hold as required - this is basically a clunky way of saying wait for orders
                std::set<IDInfo>::iterator holdingUnitsIter = holdingUnits_.find(pUnit->getIDInfo());
                if (holdingUnitsIter != holdingUnits_.end())
                {
#ifdef ALTAI_DEBUG
                    os << "\nremoving unit: " << pUnit->getIDInfo() << " to hold list";
#endif
                    holdingUnits_.erase(holdingUnitsIter);
                }
                else
                {
#ifdef ALTAI_DEBUG
                    os << "\nadding unit: " << pUnit->getIDInfo() << " from hold list";
#endif
                    holdingUnits_.insert(pUnit->getIDInfo());
                    pUnit->joinGroup(NULL);
                    return true;
                }
            }

            const CvPlot* pMovePlot = gGlobals.getMap().plot(moveToCoords.iX, moveToCoords.iY);
            std::pair<IDInfo, XYCoords> attackData = pMission->nextAttack;
            pMission->nextAttack = std::make_pair(IDInfo(), XYCoords());  // clear next attacker so we don't loop
            if (pMovePlot != pUnitPlot)
            {
                if (isAttackMove && pUnit->getGroupSize() > 1)
                {
#ifdef ALTAI_DEBUG
                    os << "\nsplitting group for attack unit: " << pUnit->getIDInfo();
#endif
                    pUnit->joinGroup(NULL);
                }
#ifdef ALTAI_DEBUG
                
                os << "\nTurn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " pushed counter mission "
                   << (isAttackMove ? "attack" : "") << " move to: " << moveToCoords;
#endif
                pMission->pushedAttack = attackData;  // store this as useful for updating combat data after combat is resolved
                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, moveToCoords.iX, moveToCoords.iY, MOVE_IGNORE_DANGER, false, false, MISSIONAI_COUNTER,
                    (CvPlot*)pMovePlot, 0, __FUNCTION__);
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\nTurn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " pushed counter mission skip ";
#endif
                pUnit->getGroup()->pushMission(MISSION_SKIP, pUnitPlot->getX(), pUnitPlot->getY(), 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
            }
            return true;
        }

        bool doUnitEscortMission(const MilitaryMissionDataPtr& pMission, CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            const CvPlot* pUnitPlot = pUnit->plot();

            std::vector<IDInfo> settlerUnits = pMission->getOurMatchingUnits(CanFoundP());
            const bool haveSettler = !settlerUnits.empty(), isSettler = pUnit->isFound();

            if (haveSettler)
            {
                CvUnit* pSettlerUnit = ::getUnit(settlerUnits[0]);

                if (!isSettler) // then we're an escort unit
                {
                    const CvPlot* pSettlerPlot = pSettlerUnit->plot();
                    // at plot with settler - join its group
                    if (pUnitPlot == pSettlerPlot && pUnit->getGroup() != pSettlerUnit->getGroup())
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " joined group: " << pSettlerUnit->getGroupID();
#endif
                        pUnit->joinGroup(pSettlerUnit->getGroup());
                        return true;
                    }
                    // not at settler's plot, but in same sub area - try and move towards settler
                    else if (pUnitPlot->getSubArea() == pSettlerPlot->getSubArea())
                    {
                        const CvPlot* pMovePlot = getNextMovePlot(player_, pUnit->getGroup(), pSettlerUnit->plot());
                        CvUnit* pSettlerGroupHeadUnit = pSettlerUnit->getGroup()->getHeadUnit();
                        if (pMovePlot && pSettlerGroupHeadUnit)
                        {
#ifdef ALTAI_DEBUG
                            os << "\nUnit: " << pUnit->getIDInfo() << " pushed settler escort mission move to: " << pMovePlot->getCoords();
#endif
                            pUnit->getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pSettlerGroupHeadUnit->getOwner(), pSettlerGroupHeadUnit->getID(),
                                MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY, false, false, MISSIONAI_GROUP, (CvPlot*)pMovePlot, pSettlerGroupHeadUnit, __FUNCTION__);
                            return true;
                        }
                    }
                    else // todo - find transport
                    {
                        return false;
                    }
                }
                else
                {
                    // assume just one settler per mission for now at least                            
                    bool haveCurrentTarget = pMission->targetCoords != XYCoords(-1, -1);
                    const CvPlot* pTargetPlot;
                    if (haveCurrentTarget)
                    {
                        pTargetPlot = gGlobals.getMap().plot(pMission->targetCoords.iX, pMission->targetCoords.iY);
                    }
                    CvPlot* pDestPlot = player_.getBestPlot(pSettlerUnit, 
                        haveCurrentTarget ? pTargetPlot->getSubArea() : pSettlerUnit->plot()->getSubArea());
                    if (pDestPlot && pUnit->plot() == pDestPlot &&
                        !pUnit->canFound(pDestPlot))  // don't want to push mission if we can't actually found here - otherwise risk getting stuck in a loop - need to find a new destination
                    {
                        // may want to mark this plot, as it's a plot we can't found at, but we can't see the offending city blocking it
                        pDestPlot = player_.getSettlerManager()->getBestPlot(pSettlerUnit->plot()->getSubArea(), std::vector<CvPlot*>(1, pDestPlot));
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " reselecting target plot to: " << (pDestPlot ? pDestPlot->getCoords() : XYCoords());
#endif
                    }

                    bool mayNeedEscort = player_.getCvPlayer()->getNumCities() > 0 && !pMission->requiredUnits.empty();
                    if (pMission->assignedUnits.size() > 1)
                    {
                        std::set<CvSelectionGroup*, CvSelectionGroupOrderF> assignedGroups = pMission->getAssignedGroups();
                        mayNeedEscort = assignedGroups.size() > 1;
                    }
                            
                    if (pDestPlot)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " updating target plot to: " << pDestPlot->getCoords();
#endif                        
                        pMission->targetCoords = pDestPlot->getCoords();

                        // if at best plot - found if safe
                        // otherwise move towards best plot, if not owned area step await escort
                        if (pUnit->plot() == pDestPlot)
                        {                            
                            PlotSet moveToPlotSet;
                            moveToPlotSet.insert(pDestPlot);
                            std::set<IDInfo> hostileUnits = getUnitsThreateningPlots(moveToPlotSet);
                            if (hostileUnits.empty())
                            {
                                pUnit->getGroup()->pushMission(MISSION_FOUND, -1, -1, 0, false, false, MISSIONAI_FOUND, 0, 0, __FUNCTION__);
                                return true;
                            }
                            else
                            {
                                const CvPlot* pMovePlot = getNextMovePlot(player_, pUnit->getGroup(), pDestPlot);
                                if (pMovePlot != pDestPlot)
                                {
                                    std::map<XYCoords, std::list<IDInfo> >::const_iterator stackIter = enemyStacks_.find(pMovePlot->getCoords());
                                    if (stackIter != enemyStacks_.end())
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\nUnit: " << pUnit->getIDInfo() << " move plot has hostiles - pushed attack mission move to: " << pMovePlot->getCoords();
#endif
                                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMovePlot->getX(), pMovePlot->getY(), MOVE_IGNORE_DANGER, false, false, NO_MISSIONAI,
                                        (CvPlot*)pMovePlot, 0, __FUNCTION__);
                                        return true;
                                    }
                                    else
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\nUnit: " << pUnit->getIDInfo() << " settle plot has hostiles - pushed mission move to: " << pMovePlot->getCoords();
#endif
                                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMovePlot->getX(), pMovePlot->getY(), MOVE_IGNORE_DANGER, false, false, NO_MISSIONAI,
                                            (CvPlot*)pMovePlot, 0, __FUNCTION__);
                                        return true;
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (pDestPlot->getRevealedOwner(player_.getTeamID(), false) == NO_PLAYER && mayNeedEscort)
                            {
                                if (pUnitPlot->isCity())
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nUnit: " << pUnit->getIDInfo() << " settle plot needs escort - pushed mission skip, settle plot = " << pDestPlot->getCoords();
#endif
                                    pUnit->getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                                    return true;
                                }
                                else if (pUnitPlot->getOwner() != player_.getPlayerID())
                                {
                                    PlotSet moveToPlotSet;
                                    moveToPlotSet.insert(pUnitPlot);
                                    const CvPlot* pMovePlot = getNextMovePlot(player_, pUnit->getGroup(), pDestPlot);
                                    if (pMovePlot != pUnitPlot)
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\nUnit: " << pUnit->getIDInfo() << " pushed escape mission move to: " << pMovePlot->getCoords();
#endif
                                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMovePlot->getX(), pMovePlot->getY(), MOVE_IGNORE_DANGER, false, false, NO_MISSIONAI,
                                            (CvPlot*)pMovePlot, 0, __FUNCTION__);
                                        return true;
                                    }
                                    else
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\nUnit: " << pUnit->getIDInfo() << " settle plot needs escort (outside city!) - pushed mission skip, settle plot = " << pDestPlot->getCoords();
#endif
                                        pUnit->getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                                        return true;
                                    }
                                }
                            }

                            const CvPlot* pMovePlot = getNextMovePlot(player_, pUnit->getGroup(), pDestPlot);

                            if (pMovePlot)
                            {
#ifdef ALTAI_DEBUG
                                os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission move to: " << pMovePlot->getCoords();
#endif
                                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMovePlot->getX(), pMovePlot->getY(), MOVE_IGNORE_DANGER, false, false, NO_MISSIONAI,
                                    (CvPlot*)pMovePlot, 0, __FUNCTION__);
                                return true;
                            }
                        }                            
                    }
                }
            }
            else
            {
                // check for other missions which needs escort, otherwise maybe add to reserve?
            }
            return false;
        }

        bool doUnitEscortWorkerMission(const MilitaryMissionDataPtr& pMission, CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            const CvPlot* pUnitPlot = pUnit->plot();

            if (!pMission->targetUnits.empty())
            {
                const CvUnit* pTargetUnit = ::getUnit(*pMission->targetUnits.begin());
                if (pTargetUnit)
                {
                    const CvPlot* pTargetUnitPlot = pTargetUnit->plot();
                    if (pUnitPlot == pTargetUnitPlot && pUnit->getGroup() != pTargetUnit->getGroup())
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " joined group: " << pTargetUnit->getGroupID();
#endif
                        pUnit->joinGroup(pTargetUnit->getGroup());
                        player_.getAnalysis()->getWorkerAnalysis()->setEscort(pTargetUnit->getIDInfo());
                        return true;
                    }
                    else
                    {
                        const CvPlot* pMovePlot = getNextMovePlot(player_, pTargetUnit->getGroup(), pTargetUnit->plot());
                        CvUnit* pTargetGroupHeadUnit = pTargetUnit->getGroup()->getHeadUnit();
                        if (pMovePlot && pTargetGroupHeadUnit)
                        {
#ifdef ALTAI_DEBUG
                            os << "\nUnit: " << pUnit->getIDInfo() << " pushed escort mission move to: " << pMovePlot->getCoords();
#endif
                            pUnit->getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pTargetGroupHeadUnit->getOwner(), pTargetGroupHeadUnit->getID(),
                                MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY, false, false, MISSIONAI_GROUP, (CvPlot*)pMovePlot, pTargetGroupHeadUnit, __FUNCTION__);
                            return true;
                        }
                    }
                }
            }
            else // lost our target (hopefully because an escort is no longer needed, rather than it being killed)
            {
                const CvCity* pCapital = player_.getCvPlayer()->getCapitalCity();
                // might be initial unit assignment and we have no capital yet
                reassignOurUnit_(pUnit, addReserveLandUnitMission_(pUnit, pCapital ? pCapital->plot() : pUnit->plot()));
                return true;
            }

            return false;
        }

        bool doUnitCityDefendMission(const MilitaryMissionDataPtr& pMission, CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            if (!pMission->assignedUnits.empty())
            {
                const CvCity* pTargetCity = pMission->getTargetCity();
                if (pTargetCity)
                {                        
                    if (pUnit->at(pTargetCity->getX(), pTargetCity->getY()))
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission skip for city (guard): " << safeGetCityName(pTargetCity);
#endif
                       
                        pUnit->getGroup()->pushMission(MISSION_SKIP, pUnit->plot()->getX(), pUnit->plot()->getY(), 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                    }
                    else
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission move to for city (guard): " << safeGetCityName(pTargetCity);
#endif
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pTargetCity->getX(), pTargetCity->getY(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_GUARD_CITY,
                            (CvPlot*)pTargetCity->plot(), 0, __FUNCTION__);
                    }
                    return true;
                }
            }
            else
            {
                ourUnitsMissionMap_.erase(pUnit->getIDInfo());
                removeMission_(std::find(missionList_.begin(), missionList_.end(), pMission));
                return false;
            }
            // todo - give unit a chance to get new mission before returning false
            return false;
        }

        bool doUnitReserveMission(const MilitaryMissionDataPtr& pMission, CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            const CvPlot* pTargetPlot = pMission->getTargetPlot();

            if (!pTargetPlot->isCity())  // might not initially have had a city target ('spare' units from first turn)
            {
                const CvPlot* pClosestCityPlot = player_.getAnalysis()->getMapAnalysis()->getClosestCity(pTargetPlot, pTargetPlot->getSubArea(), false);
                if (pClosestCityPlot)
                {
                    pMission->targetUnits.insert(pClosestCityPlot->getPlotCity()->getIDInfo());
                    pTargetPlot = pClosestCityPlot;
                }
            }

            if (pUnit->at(pTargetPlot->getX(), pTargetPlot->getY()))
            {
#ifdef ALTAI_DEBUG
                os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission sleep for city: " << safeGetCityName(pTargetPlot->getPlotCity());
#endif
                pUnit->getGroup()->pushMission(MISSION_SKIP, pUnit->plot()->getX(), pUnit->plot()->getY(), 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                return true;
            }
            else
            {
                if (pUnit->isHurt())
                {
                    const CvPlot* pClosestCityPlot = player_.getAnalysis()->getMapAnalysis()->getClosestCity(pUnit->plot(), pUnit->plot()->getSubArea(), true);
                    if (pClosestCityPlot && pUnit->plot() != pClosestCityPlot)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission move to for city (for healing): " << safeGetCityName(pClosestCityPlot->getPlotCity());
#endif
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pClosestCityPlot->getX(), pClosestCityPlot->getY(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_COUNTER,
                            (CvPlot*)pClosestCityPlot, 0, __FUNCTION__);
                        return true;
                            
                    }
#ifdef ALTAI_DEBUG
                    os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission heal ";
#endif
                    pUnit->getGroup()->pushMission(MISSION_HEAL, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                    return true;
                }

                CvSelectionGroup* pTargetGroup = NULL;
                for (std::set<IDInfo>::const_iterator assignedIter(pMission->assignedUnits.begin()), assignedEndIter(pMission->assignedUnits.end());
                    assignedIter != assignedEndIter; ++assignedIter)
                {
                    const CvUnit* pAssignedUnit = ::getUnit(*assignedIter);
                    if (pAssignedUnit)
                    {
                        if (pAssignedUnit->at(pTargetPlot->getX(), pTargetPlot->getY()))
                        {
                            pTargetGroup = pAssignedUnit->getGroup();
                            break;
                        }
                    }
                }

                if (pTargetGroup)
                {
                    CvUnit* pGroupHeadUnit = pTargetGroup->getHeadUnit();
                    if (pGroupHeadUnit)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " pushed move to group mission for group: " << pGroupHeadUnit->getGroupID() << " led by: " << pGroupHeadUnit->getIDInfo();
#endif
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pGroupHeadUnit->getOwner(), pGroupHeadUnit->getID(),
                            MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY, false, false, MISSIONAI_GROUP, pGroupHeadUnit->plot(), pGroupHeadUnit, __FUNCTION__);
                        return true;
                    }
                }
                else
                {
#ifdef ALTAI_DEBUG
                    os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission move to for city: " << safeGetCityName(pTargetPlot->getPlotCity());
#endif
                    pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pTargetPlot->getX(), pTargetPlot->getY(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_COUNTER,
                        (CvPlot*)pTargetPlot, 0, __FUNCTION__);
                    return true;
                }
            }

            return false;
        }

        std::map<XYCoords, CombatData> getAttackableUnitsMap_()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            std::map<XYCoords, CombatData> possibleCombatMap;

            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                MissionAITypes missionAIType = (*missionsIter)->missionType;
                if (missionAIType == MISSIONAI_COUNTER || missionAIType == MISSIONAI_COUNTER_CITY || missionAIType == MISSIONAI_GUARD_CITY || missionAIType == MISSIONAI_RESERVE || missionAIType == MISSIONAI_EXPLORE)
                {
                    std::vector<const CvUnit*> plotUnits((*missionsIter)->getAssignedUnits(false));
                    if (!plotUnits.empty())
                    {
                        for (PlotSet::const_iterator rIter((*missionsIter)->ourReachablePlots.begin()), rEndIter((*missionsIter)->ourReachablePlots.end()); rIter != rEndIter; ++rIter)
                        {
                            std::map<XYCoords, CombatData>::iterator combatMapIter = possibleCombatMap.find((*rIter)->getCoords());
                            // no existing entry for this plot, so search for enemy units (the potential defenders)
                            if (combatMapIter == possibleCombatMap.end())
                            {
                                std::map<XYCoords, std::list<IDInfo> >::const_iterator enemyStackIter = enemyStacks_.find((*rIter)->getCoords());
                                if (enemyStackIter != enemyStacks_.end())
                                {
                                    for (std::list<IDInfo>::const_iterator unitsIter(enemyStackIter->second.begin()), unitsEndIter(enemyStackIter->second.end()); unitsIter != unitsEndIter; ++unitsIter)
                                    {
                                        // we should be able to see the unit, as it's in enemyStacks_
                                        const CvUnit* pUnit = ::getUnit(*unitsIter);
                                        if (pUnit && pUnit->canFight())
                                        {                                        
                                            if (combatMapIter == possibleCombatMap.end())
                                            {
                                                combatMapIter = possibleCombatMap.insert(std::make_pair((*rIter)->getCoords(), CombatData(*rIter))).first;
                                            }
                                            // because there is an entry in reachablePlotsData, assume we can attack (although we don't know precisely which units until the loop below)
                                            combatMapIter->second.defenders.push_back(UnitData(pUnit));
#ifdef ALTAI_DEBUG
                                            os << "\n" << __FUNCTION__ << " found attackable unit: " << pUnit->getIDInfo() << " at: " << (*rIter)->getCoords();
#endif
                                        }
                                    }
                                }
                            }

                            // if we have a valid combat map entry for this plot (we may have added one above), add the new attackers from this mission
                            if (combatMapIter != possibleCombatMap.end())
                            {
                                MilitaryMissionData::ReachablePlotDetails::const_iterator reachablePlotIter = (*missionsIter)->ourReachablePlotDetails.find(*rIter);
                                if (reachablePlotIter != (*missionsIter)->ourReachablePlotDetails.end())
                                {
                                    for (std::list<IDInfo>::const_iterator unitsIter(reachablePlotIter->second.begin()); unitsIter != reachablePlotIter->second.end(); ++unitsIter)
                                    {
                                        const CvUnit* pPlotUnit = ::getUnit(*unitsIter);
                                        if (pPlotUnit)
                                        {
                                            combatMapIter->second.attackers.push_back(UnitData(pPlotUnit));
                                            combatMapIter->second.combatDetails.unitAttackDirectionsMap[*unitsIter] = directionXY(pPlotUnit->plot(), *rIter);
#ifdef ALTAI_DEBUG
                                            os << "\n" << __FUNCTION__ << " found potential attack unit: " << *unitsIter << " at: " << pPlotUnit->plot()->getCoords() << " to attack: " << (*rIter)->getCoords();
#endif
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return possibleCombatMap;
        }

        std::map<XYCoords, CombatData> getAttackingUnitsMap_()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            std::map<XYCoords, CombatData> possibleCombatMap;
            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                MissionAITypes missionAIType = (*missionsIter)->missionType;
                if (missionAIType == MISSIONAI_COUNTER || missionAIType == MISSIONAI_COUNTER_CITY)
                {
                    std::vector<const CvUnit*> plotUnits((*missionsIter)->getHostileUnits(player_.getTeamID(), true));
                    if (!plotUnits.empty())
                    {
                        for (PlotSet::const_iterator rIter((*missionsIter)->hostilesReachablePlots.begin()), rEndIter((*missionsIter)->hostilesReachablePlots.end()); rIter != rEndIter; ++rIter)
                        {
                            std::map<XYCoords, CombatData>::iterator combatMapIter = possibleCombatMap.find((*rIter)->getCoords());

                            // no existing entry for this plot, so search for our units (the potential defenders)
                            if (combatMapIter == possibleCombatMap.end())
                            {
                                std::map<int, std::map<XYCoords, std::set<IDInfo> > >::iterator stackIter = unitStacks_.find((*rIter)->getSubArea());
                                if (stackIter != unitStacks_.end())
                                {
                                    std::map<XYCoords, std::set<IDInfo> >::iterator subAreaStackIter = stackIter->second.find((*rIter)->getCoords());
                                    if (subAreaStackIter != stackIter->second.end())
                                    {
                                        for (std::set<IDInfo>::const_iterator unitsIter(subAreaStackIter->second.begin()), unitsEndIter(subAreaStackIter->second.end()); unitsIter != unitsEndIter; ++unitsIter)
                                        {
                                            const CvUnit* pUnit = ::getUnit(*unitsIter);
                                            if (pUnit && pUnit->getOwner() == player_.getPlayerID() && pUnit->canFight())
                                            {                                            
                                                if (combatMapIter == possibleCombatMap.end())
                                                {
                                                    combatMapIter = possibleCombatMap.insert(std::make_pair((*rIter)->getCoords(), CombatData(*rIter))).first;
                                                }
                                                // because there is an entry in reachablePlotsData, assume we can be attacked (although we don't know precisely which units until the loop below)
                                                combatMapIter->second.defenders.push_back(UnitData(pUnit));
#ifdef ALTAI_DEBUG
                                                os << "\n" << __FUNCTION__ << " found attackable unit: " << pUnit->getIDInfo() << " at: " << (*rIter)->getCoords();
#endif
                                            }
                                        }
                                    }
                                }
                            }

                            if (combatMapIter != possibleCombatMap.end())
                            {
                                MilitaryMissionData::ReachablePlotDetails::const_iterator reachablePlotIter = (*missionsIter)->hostileReachablePlotDetails.find(*rIter);

                                if (reachablePlotIter != (*missionsIter)->hostileReachablePlotDetails.end())
                                {
                                    for (std::list<IDInfo>::const_iterator unitsIter(reachablePlotIter->second.begin()); unitsIter != reachablePlotIter->second.end(); ++unitsIter)
                                    {
                                        const CvUnit* pPlotUnit = ::getUnit(*unitsIter);
                                        if (pPlotUnit)
                                        {
                                            combatMapIter->second.attackers.push_back(UnitData(pPlotUnit));
                                            combatMapIter->second.combatDetails.unitAttackDirectionsMap[*unitsIter] = directionXY(pPlotUnit->plot(), *rIter);
#ifdef ALTAI_DEBUG
                                            os << "\n" << __FUNCTION__ << " found potential attack unit: " << *unitsIter << " at: " << pPlotUnit->plot()->getCoords() << " to attack: " << (*rIter)->getCoords();
#endif
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return possibleCombatMap;
        }

        bool eraseAttackCombatMapEntry_(XYCoords coords, IDInfo unit, bool isAttacker)
        {
            return eraseCombatMapEntry_(coords, unit, true, isAttacker);
        }

        bool eraseDefenceCombatMapEntry_(XYCoords coords, IDInfo unit, bool isAttacker)
        {
            return eraseCombatMapEntry_(coords, unit, false, isAttacker);
        }

        bool eraseCombatMapEntry_(XYCoords coords, IDInfo unit, bool isAttackCombatMap, bool isAttacker)
        {
            bool erasedEntry = false, foundUnit = false;
            std::map<XYCoords, CombatData>& combatMap(isAttackCombatMap ? attackCombatMap_: defenceCombatMap_);
            std::map<XYCoords, CombatGraph::Data>& combatDataMap(isAttackCombatMap ? attackCombatData_ : defenceCombatData_);

            std::map<XYCoords, CombatData>::iterator combatMapIter = combatMap.find(coords);
            if (combatMapIter != combatMap.end())
            {
                // for attackCombatMap_ we are the attacking units, for defenceCombatMap_ we are the defenders
                // four cases: 
                // 1. attackCombatMap_ && isAttacker => attackers
                // 2. attackCombatMap_ && !isAttacker => defenders
                // 3. defenceCombatMap_ && isAttacker => attackers
                // 4. defenceCombatMap_ && !isAttacker => defenders
                std::vector<UnitData>& units(isAttacker ? combatMapIter->second.attackers : combatMapIter->second.defenders);
                foundUnit = eraseUnitById(units, unit);

                if (units.empty())
                {
                    combatMap.erase(combatMapIter);
                    combatDataMap.erase(coords);
                    erasedEntry = true;
                }
#ifdef ALTAI_DEBUG
                if (foundUnit)
                {
                    std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
                    if (isAttackCombatMap)
                    {
                        if (isAttacker) // erase our unit as a potential attacker at given plot  
                        {
                            os << " erasing our unit as attacker: " << unit << " from attackCombatMap_ map at: " << coords;
                            if (erasedEntry) os << " erased map entry as no more of our attackers for plot";
                        }
                        else // erase hostile unit as a defender from given plot
                        {
                            os << " erasing hostile unit as defender: " << unit << " from attackCombatMap_ map at: " << coords;
                            if (erasedEntry) os << " erased map entry as no more hostile defenders at plot";
                        }
                    }
                    else  // defenceCombatMap_
                    {
                        if (isAttacker) // erase hostile unit as a potential attacker at given plot 
                        {
                            os << " erasing hostile unit: " << unit << " from defenceCombatMap_ map at: " << coords;
                            if (erasedEntry) os << " erased map entry as no more hostile attackers for plot";
                        }
                        else // erase our unit as a defender at given plot
                        {
                            os << " erasing our defending unit: " << unit << " from defenceCombatMap_ map at: " << coords;
                            if (erasedEntry) os << " erased map entry as no more of our defenders at plot";
                        }
                    }
                }
#endif
            }
            return foundUnit && !erasedEntry;  // whether need to recalc combat map entry (true if we found unit, unless all entries are removed)
        }

        CombatGraph::Data getNearestCityAttackOdds(const MilitaryMissionDataPtr& pMission)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            PlotSet targetPlots = pMission->getTargetPlots();
            if (targetPlots.empty())
            {
                return CombatGraph::Data();
            }
            const CvPlot* pUnitPlot = *targetPlots.begin();

            IDInfo nearestCity;
            const CvPlot* pClosestCityPlot = player_.getAnalysis()->getMapAnalysis()->getClosestCity(pUnitPlot, pUnitPlot->getSubArea(), false, nearestCity);
            if (pClosestCityPlot)
            {
                CombatData combatData(pClosestCityPlot);
                for (std::set<IDInfo>::const_iterator hIter(pMission->targetUnits.begin()), hEndIter(pMission->targetUnits.end());  hIter != hEndIter; ++hIter)
                {
                    const CvUnit* pHostileUnit = ::getUnit(*hIter);
                    if (pHostileUnit && !pHostileUnit->isAnimal())  // animals can't attack cities
                    {
                        combatData.attackers.push_back(UnitData(pHostileUnit));
                        // an approximation if not adjacent - todo - use path logic to deduce likely attack from plot
                        // attack direction: we are at pClosestCityPlot - being attacked by pMission->targetUnits
                        combatData.combatDetails.unitAttackDirectionsMap[pHostileUnit->getIDInfo()] = directionXY(pHostileUnit->plot(), pClosestCityPlot);
                    }
                }

                // take units from assigned city guard mission, as other units on the city plot may be passing through                
                std::map<IDInfo, MilitaryMissionDataPtr>::iterator cityGuardMissionIter = cityGuardMissionsMap_.find(nearestCity);
                if (cityGuardMissionIter != cityGuardMissionsMap_.end())  // may be in process of capturing city
                {
                    for (std::set<IDInfo>::const_iterator uIter(cityGuardMissionIter->second->assignedUnits.begin()), uEndIter(cityGuardMissionIter->second->assignedUnits.end()); uIter != uEndIter; ++uIter)
                    {
                        const CvUnit* pOurUnit = ::getUnit(*uIter);
                        combatData.defenders.push_back(UnitData(pOurUnit));
                    }
                }

                // add in any assigned units for the passed in counter mission which are in the city
                for (std::set<IDInfo>::const_iterator auIter(pMission->assignedUnits.begin()), auEndIter(pMission->assignedUnits.end()); auIter != auEndIter; ++auIter)
                {
                    const CvUnit* pOurUnit = ::getUnit(*auIter);
                    if (pOurUnit && pOurUnit->plot()->getCoords() == pClosestCityPlot->getCoords())
                    {
                        combatData.defenders.push_back(UnitData(pOurUnit));
                    }
                }

                CombatGraph combatGraph = getCombatGraph(player_, combatData.combatDetails, combatData.attackers, combatData.defenders);
                combatGraph.analyseEndStates();
#ifdef ALTAI_DEBUG
                os << "\ncombat odds v. nearest city: " << safeGetCityName(nearestCity);
                combatGraph.debugEndStates(os);
#endif
                pMission->cityAttackOdds[nearestCity] = combatGraph.endStatesData;
                return combatGraph.endStatesData;
            }
            else
            {
                return CombatGraph::Data();
            }
        }

        void getPromotionCombatValues_(const CvUnit* pUnit, const std::vector<PromotionTypes>& availablePromotions) const
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator missionIter = ourUnitsMissionMap_.find(pUnit->getIDInfo());

            if (missionIter != ourUnitsMissionMap_.end())
            {
                XYCoords coords = pUnit->plot()->getCoords();
                std::set<XYCoords> reachablePlots = missionIter->second->getReachablePlots(pUnit->getIDInfo());

                for (std::set<XYCoords>::const_iterator rIter(reachablePlots.begin()), rEndIter(reachablePlots.end()); rIter != rEndIter; ++rIter)
                {
                    std::map<XYCoords, CombatData>::const_iterator defCombatIter = defenceCombatMap_.find(*rIter);
                    if (defCombatIter != defenceCombatMap_.end())
                    {
                        std::map<PromotionTypes, float> promotionValues = getPromotionValues(player_, pUnit, availablePromotions, defCombatIter->second.combatDetails, false, defCombatIter->second.attackers, defCombatIter->second.defenders);
                    }

                    std::map<XYCoords, CombatData>::const_iterator attCombatIter = attackCombatMap_.find(*rIter);
                    if (attCombatIter != attackCombatMap_.end())
                    {
                        std::map<PromotionTypes, float> promotionValues = getPromotionValues(player_, pUnit, availablePromotions, attCombatIter->second.combatDetails, true, attCombatIter->second.attackers, attCombatIter->second.defenders);
                    }
                }
            }
        }

        std::map<PromotionTypes, float> getPromotionCombatValues_(const CvUnit* pUnit, const std::vector<PromotionTypes>& availablePromotions, bool isAttacker, std::map<XYCoords, CombatData>::const_iterator combatMapIter)
        {
            return getPromotionValues(player_, pUnit, availablePromotions, combatMapIter->second.combatDetails, isAttacker, combatMapIter->second.attackers, combatMapIter->second.defenders);
        }

        void writeMissionIndexMap_(FDataStreamBase* pStream, const std::map<unsigned long, int>& missionPointersMap, const std::map<IDInfo, MilitaryMissionDataPtr>& unitsMissionMap, const std::string& mapName) const
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            std::map<IDInfo, int> missionIndexMap;
            for (std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator ci(unitsMissionMap.begin()), ciEnd(unitsMissionMap.end()); ci != ciEnd; ++ci)
            {
                std::map<unsigned long, int>::const_iterator indexIter = missionPointersMap.find((unsigned long)ci->second.get());
                if (indexIter != missionPointersMap.end())
                {
                    missionIndexMap[ci->first] = indexIter->second;
                }
#ifdef ALTAI_DEBUG
                else
                {
                    os << "\nError - " << mapName << " unit mission not found in mission list?? " << ci->first << " ";
                    ci->second->debug(os);
                }
#endif
            }
            writeComplexKeyMap(pStream, missionIndexMap);
        }

        void readMissionIndexMap_(FDataStreamBase* pStream, const std::map<int, MilitaryMissionDataPtr>& missionPointersMap, std::map<IDInfo, MilitaryMissionDataPtr>& unitsMissionMap)
        {
            size_t count;
            pStream->Read(&count);
            for (size_t i = 0; i < count; ++i)
            {
                IDInfo unit;
                unit.read(pStream);
                int missionIndex;
                pStream->Read(&missionIndex);
                std::map<int, MilitaryMissionDataPtr>::const_iterator missionIter = missionPointersMap.find(missionIndex);
                if (missionIter != missionPointersMap.end())
                {
                    unitsMissionMap[unit] = missionIter->second;
                }
            }
        }

        Player& player_;
        std::map<IDInfo, UnitCategories> unitCategoryMap_;
        std::map<UnitCategories, std::set<IDInfo> > units_;

        std::map<IDInfo, std::map<UnitTypes, int> > cityBuildsTimes_;

        // {sub area, {unit idinfo, coords}}
        std::map<int, std::map<IDInfo, XYCoords> > unitsMap_, hiddenUnitsMap_;
        std::map<int, std::map<XYCoords, std::set<IDInfo> > > unitStacks_;

        std::map<XYCoords, std::list<IDInfo> > enemyStacks_;

        std::map<XYCoords, CombatData> attackCombatMap_, defenceCombatMap_;
        std::map<XYCoords, CombatGraph::Data> attackCombatData_, defenceCombatData_;

        std::map<IDInfo, UnitHistory> unitHistories_;

        // mission maps keyed by unit's IDInfo, except for cityGuardMissionsMap_ which is keyed by city's IDInfo
        std::map<IDInfo, MilitaryMissionDataPtr> ourUnitsMissionMap_, hostileUnitsMissionMap_, cityGuardMissionsMap_;
        // key = sub area id
        std::map<int, MilitaryMissionDataPtr> subAreaResourceMissionsMap_;
        std::list<MilitaryMissionDataPtr> missionList_;

        boost::shared_ptr<CityDefenceAnalysis> pCityDefenceAnalysis_;

        // transient data for when attacking
        IDInfo nextAttackUnit_;
        std::set<IDInfo> holdingUnits_;

        // base threshold success probablity for attacking, base threshold for attacking when threatened
        // base threshold for defence, mi threshold for safe spots for moving stacks, base threshold for hostiles attacking
        static float attThreshold, defAttackThreshold, defThreshold, hostileAttackThreshold;
    };

    float MilitaryAnalysisImpl::attThreshold = 0.85f, MilitaryAnalysisImpl::defAttackThreshold = 0.50f, 
        MilitaryAnalysisImpl::defThreshold = 0.90f, MilitaryAnalysisImpl::hostileAttackThreshold = 0.20f;

    MilitaryAnalysis::MilitaryAnalysis(Player& player)
    {
        pImpl_ = boost::shared_ptr<MilitaryAnalysisImpl>(new MilitaryAnalysisImpl(player));
    }

    boost::shared_ptr<MilitaryAnalysisImpl> MilitaryAnalysis::getImpl() const
    {
        return pImpl_;
    }

    void MilitaryAnalysis::update()
    {
        pImpl_->update();
    }

    bool MilitaryAnalysis::updateOurUnit(CvUnitAI* pUnit)
    {
        return pImpl_->updateOurUnit(pUnit);
    }

    PromotionTypes MilitaryAnalysis::promoteUnit(CvUnitAI* pUnit)
    {
        return pImpl_->promoteUnit(pUnit);
    }

    MilitaryMissionDataPtr MilitaryAnalysis::getMissionData(CvUnitAI* pUnit)
    {
        return pImpl_->getMissionData(pUnit);
    }

    UnitTypes MilitaryAnalysis::getUnitRequestBuild(const CvCity* pCity, const TacticSelectionData& tacticSelectionData)
    {
        return pImpl_->getUnitRequestBuild(pCity, tacticSelectionData);
    }

    std::pair<IDInfo, UnitTypes> MilitaryAnalysis::getPriorityUnitBuild(IDInfo city)
    {
        return pImpl_->getPriorityUnitBuild(city);
    }

    void MilitaryAnalysis::addOurUnit(CvUnitAI* pUnit, const CvUnit* pUpgradingUnit)
    {
        pImpl_->addOurUnit(pUnit, pUpgradingUnit);
    }

    void MilitaryAnalysis::deleteOurUnit(CvUnit* pUnit, const CvPlot* pPlot)
    {
        pImpl_->deleteOurUnit(pUnit, pPlot);
    }

    void MilitaryAnalysis::withdrawOurUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot)
    {
        pImpl_->withdrawOurUnit(pUnit, pAttackPlot);
    }

    void MilitaryAnalysis::movePlayerUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot)
    {
        pImpl_->movePlayerUnit(pUnit, pFromPlot, pToPlot);
    }

    void MilitaryAnalysis::hidePlayerUnit(CvUnitAI* pUnit, const CvPlot* pOldPlot, bool moved)
    {
        pImpl_->hidePlayerUnit(pUnit, pOldPlot, moved);
    }

    void MilitaryAnalysis::addPlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot)
    {
        pImpl_->addPlayerUnit(pUnit, pPlot);
    }

    void MilitaryAnalysis::deletePlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot)
    {
        pImpl_->deletePlayerUnit(pUnit->getIDInfo(), pPlot);
    }

    void MilitaryAnalysis::withdrawPlayerUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot)
    {
        pImpl_->withdrawPlayerUnit(pUnit, pAttackPlot);
    }

    void MilitaryAnalysis::addOurCity(const CvCity* pCity)
    {
        pImpl_->addOurCity(pCity);
    }

    void MilitaryAnalysis::deleteCity(const CvCity* pCity)
    {
        pImpl_->deleteCity(pCity);
    }

    void MilitaryAnalysis::addPlayerCity(const CvCity* pCity)
    {
        pImpl_->addPlayerCity(pCity);
    }

    void MilitaryAnalysis::deletePlayerCity(const CvCity* pCity)
    {
        pImpl_->deletePlayerCity(pCity);
    }

    void MilitaryAnalysis::addHostilePlotsWithUnknownCity(const std::vector<XYCoords>& coords)
    {
        pImpl_->addHostilePlotsWithUnknownCity(coords);
    }

    void MilitaryAnalysis::addNewSubArea(int subArea)
    {
        pImpl_->addNewSubArea(subArea);
    }

    void MilitaryAnalysis::write(FDataStreamBase* pStream) const
    {
        pImpl_->write(pStream);
    }

    void MilitaryAnalysis::read(FDataStreamBase* pStream)
    {
        pImpl_->read(pStream);
    }

    PlotSet MilitaryAnalysis::getThreatenedPlots() const
    {
        return pImpl_->getThreatenedPlots();
    }

    std::set<IDInfo> MilitaryAnalysis::getUnitsThreateningPlots(const PlotSet& plots) const
    {
        return pImpl_->getUnitsThreateningPlots(plots);
    }

    PlotUnitsMap MilitaryAnalysis::getPlotsThreatenedByUnits(const PlotSet& plots) const
    {
        return pImpl_->getPlotsThreatenedByUnits(plots);
    }

    const std::map<XYCoords, CombatData>& MilitaryAnalysis::getAttackableUnitsMap() const
    {
        return pImpl_->getAttackableUnitsMap();
    }

    CvUnit* MilitaryAnalysis::getNextAttackUnit()
    {
        return pImpl_->getNextAttackUnit();
    }

    std::map<int /* sub area */, std::set<XYCoords> > MilitaryAnalysis::getLandScoutMissionRefPlots() const
    {
        return pImpl_->getLandScoutMissionRefPlots();
    }

    size_t MilitaryAnalysis::getUnitCount(MissionAITypes missionType, UnitTypes unitType, int subArea) const
    {
        return pImpl_->getUnitCount(missionType, unitType, subArea);
    }

    PlotUnitsMap getNearbyHostileStacks(const Player& player, const CvPlot* pPlot, int range)
    {
        boost::shared_ptr<MilitaryAnalysis> pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
        return pMilitaryAnalysis->getImpl()->getNearbyHostileStacks(pPlot, range);
    }

    std::set<XYCoords> getUnitHistory(const Player& player, IDInfo unit)
    {
        boost::shared_ptr<MilitaryAnalysis> pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
        return pMilitaryAnalysis->getImpl()->getUnitHistory(unit);
    }

    void updateMilitaryAnalysis(Player& player)
    {
        boost::shared_ptr<MilitaryAnalysis> pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
        pMilitaryAnalysis->update();
    }

    bool doUnitAnalysis(Player& player, CvUnitAI* pUnit)
    {
        boost::shared_ptr<MilitaryAnalysis> pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
        return pMilitaryAnalysis->updateOurUnit(pUnit);
    }
}