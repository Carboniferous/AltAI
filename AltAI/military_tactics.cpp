#include "AltAI.h"

#include "./military_tactics.h"
#include "./unit_history.h"
#include "./unit_mission.h"
#include "./unit_mission_helper.h"
#include "./unit_counter_mission.h"
#include "./unit_escort_mission.h"
#include "./unit_city_defence_mission.h"
#include "./coastal_defence_mission.h"
#include "./city_defence.h"
#include "./worker_tactics.h"
#include "./player_analysis.h"
#include "./religion_tactics.h"
#include "./unit_analysis.h"
#include "./unit_tactics.h"
#include "./city_unit_tactics.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./settler_manager.h"
#include "./unit_explore.h"
#include "./map_analysis.h"
#include "./sub_area.h"
#include "./iters.h"
#include "./save_utils.h"
#include "./civ_log.h"
#include "./unit_log.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
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

        struct CityDistanceComp
        {
            bool operator() (const std::pair<IDInfo, int>& first, const std::pair<IDInfo, int>& second) const
            {
                return first.second < second.second;
            }
        };

        struct CounterMissionPriorityComp
        {
            bool operator() (const MilitaryMissionDataPtr& pFirstMission, const MilitaryMissionDataPtr& pSecondMission) const
            {
                // if missions have calculated city attack odds, choose one with highest chances
                float firstMissionMaxHostileAttackOdds = 0.0f, secondMissionMaxHostileAttackOdds = 0.0f;
                for (std::map<IDInfo, CombatGraph::Data>::const_iterator aIter(pFirstMission->cityAttackOdds.begin()), aEndIter(pFirstMission->cityAttackOdds.end()); aIter != aEndIter; ++aIter)
                {
                    firstMissionMaxHostileAttackOdds = std::max<float>(firstMissionMaxHostileAttackOdds, aIter->second.pWin);
                }
                for (std::map<IDInfo, CombatGraph::Data>::const_iterator aIter(pSecondMission->cityAttackOdds.begin()), aEndIter(pSecondMission->cityAttackOdds.end()); aIter != aEndIter; ++aIter)
                {
                    secondMissionMaxHostileAttackOdds = std::max<float>(secondMissionMaxHostileAttackOdds, aIter->second.pWin);
                }
                if (firstMissionMaxHostileAttackOdds > 0.0 || secondMissionMaxHostileAttackOdds > 0.0)
                {
                    return firstMissionMaxHostileAttackOdds > secondMissionMaxHostileAttackOdds;
                }

                int firstClosestCityDistance = MAX_INT, secondClosestCityDistance = MAX_INT;
                if (!isEmpty(pFirstMission->closestCity))
                {
                    const CvCity* pCity = ::getCity(pFirstMission->closestCity);
                    firstClosestCityDistance = stepDistance(pCity->plot()->getX(), pCity->plot()->getY(), newUnitCoords.iX, newUnitCoords.iY);
                }
                if (!isEmpty(pSecondMission->closestCity))
                {
                    const CvCity* pCity = ::getCity(pSecondMission->closestCity);
                    secondClosestCityDistance = stepDistance(pCity->plot()->getX(), pCity->plot()->getY(), newUnitCoords.iX, newUnitCoords.iY);
                }
                if (firstClosestCityDistance != secondClosestCityDistance)
                {
                    return firstClosestCityDistance < secondClosestCityDistance;
                }

                int firstMinTargetDistance = MAX_INT, secondMinTargetDistance = MAX_INT;
                for (std::set<IDInfo>::const_iterator tIter(pFirstMission->targetUnits.begin()), tEndIter(pFirstMission->targetUnits.end()); tIter != tEndIter; ++tIter)
                {
                    const CvUnit* pUnit = ::getUnit(*tIter);
                    if (pUnit)
                    {
                        int thisTargetDistance = stepDistance(pUnit->plot()->getX(), pUnit->plot()->getY(), newUnitCoords.iX, newUnitCoords.iY);
                        firstMinTargetDistance = std::min<int>(firstMinTargetDistance, thisTargetDistance);
                    }
                }
                for (std::set<IDInfo>::const_iterator tIter(pSecondMission->targetUnits.begin()), tEndIter(pSecondMission->targetUnits.end()); tIter != tEndIter; ++tIter)
                {
                    const CvUnit* pUnit = ::getUnit(*tIter);
                    if (pUnit)
                    {
                        int thisTargetDistance = stepDistance(pUnit->plot()->getX(), pUnit->plot()->getY(), newUnitCoords.iX, newUnitCoords.iY);
                        secondMinTargetDistance = std::min<int>(secondMinTargetDistance, thisTargetDistance);
                    }
                }
                if (firstMinTargetDistance != secondMinTargetDistance)
                {
                    return firstMinTargetDistance < secondMinTargetDistance;
                }

                const CvPlot* pFirstTarget = pFirstMission->getTargetPlot();
                const CvPlot* pSecondTarget = pSecondMission->getTargetPlot();
                firstMinTargetDistance = pFirstTarget ? stepDistance(pFirstTarget->getX(), pFirstTarget->getY(), newUnitCoords.iX, newUnitCoords.iY) : MAX_INT;
                secondMinTargetDistance = pSecondTarget ? stepDistance(pSecondTarget->getX(), pSecondTarget->getY(), newUnitCoords.iX, newUnitCoords.iY) : MAX_INT;
                return firstMinTargetDistance < secondMinTargetDistance;
            }

            XYCoords newUnitCoords;
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

        explicit MilitaryAnalysisImpl(Player& player)
            : player_(player), pCityDefenceAnalysis_(new CityDefenceAnalysis(player))
        {   
        }

        void update()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " " << __FUNCTION__;
#endif
            updateStacks_();
            updateMissionRequirements_();
            
            updateAttackCombatMap_();
            updateDefenceCombatMap_();            

            updateMissions_();

            checkUnassignedUnits_();
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

            if (!pUnitMission && (pUnit->AI_getUnitAIType() == UNITAI_MISSIONARY || pUnit->AI_getUnitAIType() == UNITAI_MISSIONARY_SEA))
            {
                pUnitMission = addMissionaryUnitMission_(pUnit);
            }

            // check any cities need guard unit (logic is a bit too greedy)
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
                pUnitMission->debug(os, true);
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
                os << "\nDeleted unit from analysis: "
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
            if (!isEmpty(lastKnownCoords))
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
                        os << "\n erasing mission from map for unit: " << unit;
#endif
                        eraseMission_(missionDataIter->second);
                    }
#ifdef ALTAI_DEBUG
                    else
                    {
                        os << " remaining targets count = " << missionDataIter->second->targetUnits.size();
                        missionDataIter->second->debug(os, true);
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
                // todo - check this/tidy up logic to merge missions, etc... but in general if we already have a mission, keep the unit associated with it rather than creating a new one
                if (hostileUnitsMissionMap_.find(pUnit->getIDInfo()) == hostileUnitsMissionMap_.end())
                {
                    addHostileUnitMission_(pToPlot->getSubArea(), pToPlot->getCoords(), pUnit->getIDInfo());
                }
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

            if (isCapture)
            {
                //deleteCity(
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
                MilitaryMissionDataPtr pMission = UnitCounterMission::createMission(pCity->plot()->getSubArea(), true);
                addMission_(pMission);
                pMission->targetCity = pCity->getIDInfo();

                UnitPlotIter unitPlotIter(pCity->plot());
                while (CvUnit* pUnit = unitPlotIter())
                {
                    if (pUnit->getTeam() == pCity->getTeam())
                    {
                        std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionDataIter = hostileUnitsMissionMap_.find(pUnit->getIDInfo());
                        if (missionDataIter != hostileUnitsMissionMap_.end())
                        {
                            missionDataIter->second->targetUnits.erase(missionDataIter->first);
                            pMission->targetUnits.insert(missionDataIter->first);

                            if (missionDataIter->second->targetUnits.empty())
                            {
                                for (std::set<IDInfo>::iterator assignedUnitsIter(missionDataIter->second->assignedUnits.begin()),
                                    assignedUnitsEndIter(missionDataIter->second->assignedUnits.end()); assignedUnitsIter != assignedUnitsEndIter;
                                    ++assignedUnitsIter)
                                {
                                    CvUnit* pUnit = ::getUnit(*assignedUnitsIter);
                                    if (pUnit)
                                    {
                                        reassignOurUnit_(pUnit, pMission);
                                    }
                                }
                                eraseMission_(missionDataIter->second);
                            }
                        }
                    }
                }

                // need to remove any missions in the vicinity that are targetting coords (rather than units)
                // and reassign any of their units to this new mission
                finder.missionType = MISSIONAI_COUNTER;
                finder.city = IDInfo();
                finder.proximityData = MissionPlotProximity(4, pCity->plot()->getCoords());

                for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()), missionsEndIter(missionList_.end());
                    missionsIter != missionsEndIter;)
                {
                    if (finder(*missionsIter))
                    {
                        //if (isEmpty((*missionsIter)->getTargetCity()))
                        //{
                        // copy assigned units as reassignOurUnit_ calls unassignUnit which erases the units from the original set
                        std::set<IDInfo> assignedUnits = (*missionsIter)->assignedUnits;

                        for (std::set<IDInfo>::iterator assignedUnitsIter(assignedUnits.begin()),
                            assignedUnitsEndIter(assignedUnits.end()); assignedUnitsIter != assignedUnitsEndIter;
                            ++assignedUnitsIter)
                        {
                            CvUnit* pUnit = ::getUnit(*assignedUnitsIter);
                            if (pUnit)
                            {
                                reassignOurUnit_(pUnit, pMission);
                            }
                        }
                        eraseMission_(*missionsIter++);
                        continue;
                        //}
                    }
                    ++missionsIter;
                }
            }
        }

        void deletePlayerCity(const CvCity* pCity)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\n deleting player city: " << safeGetCityName(pCity);
#endif
            MissionFinder finder;
            finder.city = pCity->getIDInfo();

            std::list<MilitaryMissionDataPtr>::iterator missionsIter = std::find_if(missionList_.begin(), missionList_.end(), finder);
            if (missionsIter != missionList_.end())
            {
                removeMission_(missionsIter);
            }
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
                const CvPlot* pPlot = gGlobals.getMap().plot(*remainingCoords.begin());
                MilitaryMissionDataPtr pMission = UnitCounterMission::createMission(pPlot->getSubArea(), false);
                pMission->targetCoords = *remainingCoords.begin();
                addMission_(pMission);                
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
                MilitaryMissionDataPtr pMission = GuardBonusesMission::createMission(subArea);
                addMission_(pMission);
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

            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = ourUnitsMissionMap_.find(pUnit->getIDInfo());
            if (missionIter != ourUnitsMissionMap_.end())
            {   
#ifdef ALTAI_DEBUG
                os << "\nUpdating unit: " << pUnit->getID() << " with assigned mission: (" << missionIter->second << ") ";
                missionIter->second->debug(os, true);
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

                bool checkedMissionResult = false;
                bool checkedMissionExecuted = false;
                if (missionIter->second->missionType == MISSIONAI_BUILD)
                {
                    bool hasWorkerMission = AltAI::doWorkerMove(player_, pUnit);
                    if (!hasWorkerMission && pUnit->getDomainType() == DOMAIN_SEA)
                    {
                        return AltAI::doCoastalUnitExplore(pUnit);
                    }
                    else
                    {
                        return hasWorkerMission;
                    }
                }
                else if (missionIter->second->missionType == MISSIONAI_SPREAD)
                {
                    return AltAI::doMissionaryMove(player_, pUnit);
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
                    if (!missionIter->second->pUnitsMission)
                    {
                        missionIter->second->pUnitsMission = IUnitsMissionPtr(new SettlerEscortMission(player_.getCvPlayer()->getStartingPlot()->getSubArea()));
                    }
                    checkedMissionResult = missionIter->second->pUnitsMission->doUnitMission(pUnit, missionIter->second.get());
                    checkedMissionExecuted = true;
                }
                else if (missionIter->second->missionType == MISSIONAI_ESCORT_WORKER)
                {
                    if (!missionIter->second->pUnitsMission)
                    {
                        missionIter->second->pUnitsMission = IUnitsMissionPtr(new WorkerEscortMission(player_.getCvPlayer()->getStartingPlot()->getSubArea()));
                    }
                    checkedMissionResult = missionIter->second->pUnitsMission->doUnitMission(pUnit, missionIter->second.get());
                    checkedMissionExecuted = true;
                }
                else if (missionIter->second->missionType == MISSIONAI_GUARD_CITY)
                {
                    if (!missionIter->second->pUnitsMission)
                    {
                        missionIter->second->pUnitsMission = IUnitsMissionPtr(new CityDefenceMission());
                    }
                    checkedMissionResult = missionIter->second->pUnitsMission->doUnitMission(pUnit, missionIter->second.get());
                    checkedMissionExecuted = true;
                }
                else if (missionIter->second->missionType == MISSIONAI_COUNTER || missionIter->second->missionType == MISSIONAI_COUNTER_CITY)
                {
                    if (!missionIter->second->pUnitsMission)
                    {
                        missionIter->second->pUnitsMission = IUnitsMissionPtr(new UnitCounterMission(player_.getCvPlayer()->getStartingPlot()->getSubArea()));
                    }
                    checkedMissionResult = missionIter->second->pUnitsMission->doUnitMission(pUnit, missionIter->second.get());
                    checkedMissionExecuted = true;
                }
                else if (missionIter->second->missionType == MISSIONAI_RESERVE)
                {
                    if (!missionIter->second->pUnitsMission)
                    {
                        missionIter->second->pUnitsMission = IUnitsMissionPtr(new ReserveMission());
                    }
                    return missionIter->second->pUnitsMission->doUnitMission(pUnit, missionIter->second.get());
                }

                if (checkedMissionExecuted && !checkedMissionResult)
                {
                    const CvCity* pCapital = player_.getCvPlayer()->getCapitalCity();
                    MilitaryMissionDataPtr pNewMission = addReserveLandUnitMission_(pUnit, pCapital ? pCapital->plot() : pUnit->plot());
                    pNewMission->pUnitsMission = IUnitsMissionPtr(new ReserveMission());
                    if (pNewMission)
                    {
                        return pNewMission->pUnitsMission->doUnitMission(pUnit, pNewMission.get());
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    return true;
                }
            }
            // no assigned mission
            // includes great people currently - uses default civ core logic for now
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

        MilitaryMissionDataPtr getCityDefenceMission(IDInfo city)
        {
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator cityGuardMissionIter = cityGuardMissionsMap_.find(city);
            if (cityGuardMissionIter != cityGuardMissionsMap_.end())  // may be in process of capturing city
            {
                return cityGuardMissionIter->second;
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
            PlayerTactics& playerTactics = *player_.getAnalysis()->getPlayerTactics();
            City& refCity = player_.getCity(city.iID);

            std::pair<IDInfo, UnitTypes> cityAndUnit(IDInfo(), NO_UNIT);
            std::map<IDInfo, std::vector<std::list<UnitData> > > requiredUnitsCityMap;  // map of units required for counter mission against closest city threatened by that missions's hostiles
            int totalRequiredBuilds = 0;
            std::map<IDInfo, int> timesToTargetMap;  // how long for hostile units to reach closest threatened cities
            std::list<std::pair<IDInfo, int> > cityRoutingMap;  // how long for cities to reach threatened city (todo: too basic for multiple threats)

            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                if (((*missionsIter)->missionType == MISSIONAI_COUNTER || (*missionsIter)->missionType == MISSIONAI_COUNTER_CITY) && !(*missionsIter)->cityAttackOdds.empty())
                {
                    for (std::map<IDInfo, CombatGraph::Data>::const_iterator aIter((*missionsIter)->cityAttackOdds.begin()), aEndIter((*missionsIter)->cityAttackOdds.end()); aIter != aEndIter; ++aIter)
                    {
                        if (aIter->second.pWin > hostileAttackThreshold)
                        {
                            const CvCity* pOurTargetCity = ::getCity((*missionsIter)->closestCity);

                            std::vector<UnitData> hostileUnitData = makeUnitData((*missionsIter)->targetUnits);
                            if (hostileUnitData.empty() || !pOurTargetCity)
                            {
                                continue;
                            }
                            const CvPlot* pHostileUnitsPlot = ::getUnit(hostileUnitData.begin()->unitId)->plot();

                            // copy required units for missions which are tracking hostiles which threaten cities (and potentially other targets) into map keyed by city under threat
                            std::copy((*missionsIter)->requiredUnits.begin(), (*missionsIter)->requiredUnits.end(), std::back_inserter(requiredUnitsCityMap[(*missionsIter)->closestCity]));
                            totalRequiredBuilds += (*missionsIter)->requiredUnits.size();
                            for (size_t i = 0, count = (*missionsIter)->tooDistantAssignedUnits.size(); i < count; ++i)
                            {
                                const CvUnit* pLateUnit = ::getUnit((*missionsIter)->tooDistantAssignedUnits[i]);                                
                                requiredUnitsCityMap[(*missionsIter)->closestCity].push_back(std::list<UnitData>(1, UnitData(pLateUnit)));
                            }
                            totalRequiredBuilds += (*missionsIter)->tooDistantAssignedUnits.size();

                            if (requiredUnitsCityMap[(*missionsIter)->closestCity].rbegin()->empty())
                            {
#ifdef ALTAI_DEBUG
                                os << "\n\t unable to find any units which mission requires even though city: " << safeGetCityName(pOurTargetCity)
                                    << " is threatened with odds: " << aIter->second.pWin;
#endif
                                continue;
                            }

                            std::vector<UnitData> pathfinderUnitData(1, *requiredUnitsCityMap[(*missionsIter)->closestCity].rbegin()->begin());

                            PlayerTactics::UnitTacticsMap::const_iterator tacticsIter = playerTactics.unitTacticsMap_.find(pathfinderUnitData[0].unitType);
                            if (tacticsIter == playerTactics.unitTacticsMap_.end() || (tacticsIter->second && !tacticsIter->second->areDependenciesSatisfied(playerTactics.player, IDependentTactic::Ignore_None)))
                            {
                                continue;  // can't train unit anywhere (possibly an error)
                            }                            

                            // work out transit times for units
                            // currently based on first required unit - todo: group units by movement data (UnitMovementData)
                            CityIter cityIter(*player_.getCvPlayer());
                            while (CvCity* pLoopCity = cityIter())
                            {
                                if (pLoopCity->plot()->getSubArea() == pOurTargetCity->plot()->getSubArea())
                                {
                                    CityUnitTacticsPtr pCityUnitTacticsPtr = tacticsIter->second->getCityTactics(pLoopCity->getIDInfo());
                                    if (!pCityUnitTacticsPtr || !pCityUnitTacticsPtr->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\n\t can't train unit: " << gGlobals.getUnitInfo(tacticsIter->first).getType() << " in city: " << safeGetCityName(pLoopCity);
#endif
                                        if (pLoopCity->getIDInfo() == city)
                                        {
                                            return cityAndUnit;  // for now, if can't train return empty, todo: need to consider other, possibly defensive units
                                        }
                                        continue;
                                    }

                                    if (pLoopCity->getIDInfo() == city)  // update tactic data
                                    {
                                        pCityUnitTacticsPtr->update(player_, refCity.getCityData());
                                        pCityUnitTacticsPtr->apply(playerTactics.tacticSelectionDataMap[DependencyItemSet()], IDependentTactic::Ignore_None);
                                    }

                                    UnitPathData unitPathData;
                                    unitPathData.calculate(pathfinderUnitData, pLoopCity->plot(), pOurTargetCity->plot(), MOVE_MAX_MOVES | MOVE_IGNORE_DANGER, player_.getPlayerID(), player_.getTeamID());
                                    if (unitPathData.valid)
                                    {
                                        cityRoutingMap.push_back(std::make_pair(pLoopCity->getIDInfo(), unitPathData.pathTurns));
#ifdef ALTAI_DEBUG
                                        os << "\n movement from: " << safeGetCityName(pLoopCity) << " to: " << safeGetCityName(pOurTargetCity);
                                        unitPathData.debug(os);
#endif
                                    }
                                }
                            }
                            cityRoutingMap.sort(CityDistanceComp());
#ifdef ALTAI_DEBUG
                            os << "\n city move times: ";
                            for (std::list<std::pair<IDInfo, int> >::const_iterator crIter(cityRoutingMap.begin()), crEndIter(cityRoutingMap.end()); crIter != crEndIter; ++crIter)
                            {
                                os << " " << safeGetCityName(crIter->first) << " = " << crIter->second;
                            }
#endif

                            UnitPathData hostileUnitPathData;
                            hostileUnitPathData.calculate(hostileUnitData, pHostileUnitsPlot, pOurTargetCity->plot(), MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY, player_.getPlayerID(), player_.getTeamID());
                            if (hostileUnitPathData.valid)
                            {
                                std::map<IDInfo, int>::iterator tIter = timesToTargetMap.find((*missionsIter)->closestCity);
                                if (tIter != timesToTargetMap.end())
                                {                                    
                                    tIter->second = std::min<int>(tIter->second, hostileUnitPathData.pathTurns);  // if city has closer threat - keep its entry time
                                }
                                else
                                {
                                    timesToTargetMap[(*missionsIter)->closestCity] = hostileUnitPathData.pathTurns;
                                }
#ifdef ALTAI_DEBUG
                                os << "\nCalculated hostile units time to target city: " << safeGetCityName(pOurTargetCity) << " = " << hostileUnitPathData.pathTurns;
#endif
                            }
#ifdef ALTAI_DEBUG
                            else
                            {
                                os << "\nFailed for hostile units to calculate time to target city: " << safeGetCityName(pOurTargetCity) << ", 1st unit: " << hostileUnitData[0].unitId;
                            }
#endif
                        }
                    }
                }
            }

#ifdef ALTAI_DEBUG
            for (std::map<IDInfo, std::vector<std::list<UnitData> > >::const_iterator rudmIter(requiredUnitsCityMap.begin()), rudmEndIter(requiredUnitsCityMap.end());
                rudmIter != rudmEndIter; ++rudmIter)
            {
                os << "\n" << __FUNCTION__ << " required units for city: " << safeGetCityName(rudmIter->first);
                for (size_t uIndex = 0, uCount = rudmIter->second.size(); uIndex < uCount; ++uIndex)
                {
                    os << " {";
                    for (std::list<UnitData>::const_iterator lIter(rudmIter->second[uIndex].begin()), lEndIter(rudmIter->second[uIndex].end()); lIter != lEndIter; ++lIter)
                    {
                        os << " unit: " << gGlobals.getUnitInfo(lIter->unitType).getType();
                    }
                    os << "} ";
                }
            }
#endif
            if (!requiredUnitsCityMap.empty())
            {
                int ourPos = 1;
                for (std::list<std::pair<IDInfo, int> >::const_iterator ci(cityRoutingMap.begin()), ciEnd(cityRoutingMap.end()); ci != ciEnd; ++ci)
                {
                    if (ci->first != city)
                    {
                        ++ourPos;
                    }
                    else
                    {
                        break;
                    }
                }
                if (ourPos > totalRequiredBuilds)
                {
#ifdef ALTAI_DEBUG
                    os << "\n\t skipping city: " << safeGetCityName(city) << " as " << ourPos << " out of " << cityRoutingMap.size()
                       << " by distance from threat and only require: " << totalRequiredBuilds << " units";
#endif
                    return cityAndUnit;
                }

                for (std::map<IDInfo, std::vector<std::list<UnitData> > >::const_iterator ci(requiredUnitsCityMap.begin()), ciEnd(requiredUnitsCityMap.end()); ci != ciEnd; ++ci)
                {
                    for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                    {
                        std::vector<UnitData> pathfinderUnitData(1, *ci->second[i].begin());
                        UnitTypes desiredUnitType = ci->second[i].begin()->unitType;

                        PlayerTactics::UnitTacticsMap::iterator tacticIter = playerTactics.unitTacticsMap_.find(desiredUnitType);
                        if (tacticIter != playerTactics.unitTacticsMap_.end())
                        {
                            CityUnitTacticsPtr pCityUnitTactics = tacticIter->second->getCityTactics(city);
                            if (pCityUnitTactics)
                            {
                                //std::vector<DependencyItem> unitDeps = pCityUnitTactics->getDepItems(IDependentTactic::All_Deps);
                                DependencyItemSet depSet; // (unitDeps.begin(), unitDeps.end());

                                const TacticSelectionData& tacticData = playerTactics.tacticSelectionDataMap[depSet];
                                // includes build time
                                UnitTacticValue unitTacticValue = TacticSelectionData::getUnitValue(tacticData.cityAttackUnits, desiredUnitType);

                                for (std::map<IDInfo, int>::const_iterator tIter(timesToTargetMap.begin()), tEndIter(timesToTargetMap.end()); tIter != tEndIter; ++tIter)
                                {
                                    if (unitTacticValue.nTurns <= 2 * tIter->second)
                                    {
                                        cityAndUnit = std::make_pair(city, desiredUnitType);
#ifdef ALTAI_DEBUG
                                        os << "\n" << __FUNCTION__ << " returning: " << gGlobals.getUnitInfo(desiredUnitType).getType() << " for city: " << safeGetCityName(city)
                                            << " build turns: " << unitTacticValue.nTurns << " req time: " << tIter->second;
#endif
                                        return cityAndUnit;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return cityAndUnit;
        }

        UnitRequestData getUnitRequestBuild(const CvCity* pCity, const TacticSelectionData& tacticSelectionData)
        {
#ifdef ALTAI_DEBUG
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity->getOwner()))->getStream();
            os << "\nChecking unit builds for: " << narrow(pCity->getName());
#endif
            UnitRequestData requestData;

            std::map<UnitTypes, int> unitRequestCounts;
            std::map<UnitTypes, std::pair<IDInfo, int> > unitBestBuildCities;

            IDInfo bestCity;
            int bestBuildTime = -1;
            for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.cityAttackUnits.begin()), ciEnd(tacticSelectionData.cityAttackUnits.end()); ci != ciEnd; ++ci)
            {
                unitRequestCounts.insert(std::make_pair(ci->unitType, 0));
                unitBestBuildCities[ci->unitType] = updateUnitBuildData_(pCity->getIDInfo(), *ci); 
#ifdef ALTAI_DEBUG
                os << "\nUnit: " << gGlobals.getUnitInfo(ci->unitType).getType() << ", turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue
                    << ", count = " << pPlayer->getUnitCount(ci->unitType) << ", incl. under construction count = " << pPlayer->getUnitCount(ci->unitType, true);

                os << " best build time = " << unitBestBuildCities[ci->unitType].second << ", city = " << safeGetCityName(unitBestBuildCities[ci->unitType].first);
#endif
            }

            if (pCity->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
            {
                for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.seaCombatUnits.begin()), ciEnd(tacticSelectionData.seaCombatUnits.end()); ci != ciEnd; ++ci)
                {
                    unitRequestCounts.insert(std::make_pair(ci->unitType, 0));
                    unitBestBuildCities[ci->unitType] = updateUnitBuildData_(pCity->getIDInfo(), *ci); 
#ifdef ALTAI_DEBUG
                    os << "\nUnit: " << gGlobals.getUnitInfo(ci->unitType).getType() << ", turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue
                        << ", count = " << pPlayer->getUnitCount(ci->unitType) << ", incl. under construction count = " << pPlayer->getUnitCount(ci->unitType, true);

                    os << " best build time = " << unitBestBuildCities[ci->unitType].second << ", city = " << safeGetCityName(unitBestBuildCities[ci->unitType].first);
#endif
                }
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
                if (uIter->second > 0)
                {
                    os << "\n\t unit " << gGlobals.getUnitInfo(uIter->first).getType() << " req count = " << uIter->second;
                }
#endif
                if (uIter->second > mostRequestedCount)
                {
                    mostRequestedCount = uIter->second;
                    mostRequestedUnit = uIter->first;
                }
            }
            requestData.unitType = mostRequestedUnit;
            requestData.missionRequestCount = unitRequestCounts[mostRequestedUnit];
            requestData.bestCity = unitBestBuildCities[mostRequestedUnit].first;
            requestData.bestCityBuildTime = unitBestBuildCities[mostRequestedUnit].second;

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
            return requestData;
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

        const std::map<XYCoords, std::list<IDInfo> >& getEnemyStacks() const
        {
            return enemyStacks_;
        }

        PlotUnitDataMap getEnemyUnitData(int subArea) const
        {
            PlotUnitDataMap unitDataMap;
            const CvMap& map = gGlobals.getMap();

            for (std::map<XYCoords, std::list<IDInfo> >::const_iterator coordIter(enemyStacks_.begin()), coordEndIter(enemyStacks_.end());
                coordIter != coordEndIter; ++coordIter)
            {
                const CvPlot* pPlot = map.plot(coordIter->first);
                // todo - deal with ships in port
                if (pPlot->getSubArea() == subArea)
                {
                    for (std::list<IDInfo>::const_iterator lIter(coordIter->second.begin()), lEndIter(coordIter->second.end()); lIter != lEndIter; ++lIter)
                    {
                        const CvUnit* pUnit = ::getUnit(*lIter);
                        if (pUnit)
                        {
                            unitDataMap[pPlot].push_back(UnitData(pUnit));
                        }
                    }
                }
            }

            return unitDataMap;
        }

        const std::map<XYCoords, CombatData>& getAttackCombatMap() const
        {
            return attackCombatMap_;
        }

        const std::map<XYCoords, CombatData>& getDefenceCombatMap() const
        {
            return defenceCombatMap_;
        }

        const std::map<XYCoords, CombatGraph::Data>& getAttackCombatData() const
        {
            return attackCombatData_;
        }

        const std::map<XYCoords, CombatGraph::Data>& getDefenceCombatData() const
        {
            return defenceCombatData_;
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
#ifdef ALTAI_DEBUG
                        os << "\nChecking possible attack coords: " << iter->first << " odds: " << iter->second.pWin;
#endif
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
                                    pPossibleAttackUnit->getGroup()->clearMissionQueue(__FUNCTION__);
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
                                        const CvCity* pTargetCity = missionIter->second->getTargetCity();
                                        if (pTargetCity && pTargetCity->isBarbarian() && pTargetCity->getCultureLevel() == 1 && pTargetCity->getPopulation() == 1)
                                        {
#ifdef ALTAI_DEBUG
                                            os << "\nDelaying attack on target to avoid razing city...";
                                            pAttackUnit = NULL;
                                            continue;
#endif
                                        }
                                        if (isEmpty(missionIter->second->nextAttack.first) || 
                                            missionIter->second->nextAttack.first == pAttackUnit->getIDInfo() && missionIter->second->nextAttack.second == iter->first)
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
                                            pAttackUnit = NULL;
                                            continue;
                                        }
                                    }
                                    if (pAttackUnit) break;
                                }
                            }
                        }

                        if (pAttackUnit)
                        {
                            break;
                        }
                    }
                    else
                    {
#ifdef ALTAI_DEBUG
                        os << "\nSkipping possible attack coords: " << iter->first << " as odds too low: " << iter->second.pWin;
#endif 
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

        const std::map<IDInfo, UnitHistory>& getUnitHistories() const
        {
            return unitHistories_;
        }

        const std::list<MilitaryMissionDataPtr>& getMissions() const
        {
            return missionList_;
        }

        std::map<int /* sub area */, std::set<XYCoords> > getLandScoutMissionRefPlots() const
        {
            std::map<int /* sub area */, std::set<XYCoords> > subAreaRefPlotsMap;

            for (std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator missionIter(ourUnitsMissionMap_.begin()), endIter(ourUnitsMissionMap_.end());
                missionIter != endIter; ++missionIter)
            {
                if (missionIter->second->missionType == MISSIONAI_EXPLORE)
                {
                    if (!isEmpty(missionIter->second->targetCoords))
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

        void checkUnassignedUnits_()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
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

        void updateMissionRequirements_()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif 
            const int currentTurn = gGlobals.getGame().getGameTurn();
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
                    bool updateRequiredUnits = false;
                    int subAreaId = -1;

                    if (missionAIType == MISSIONAI_COUNTER || missionAIType == MISSIONAI_COUNTER_CITY)
                    {
                        if ((*missionsIter)->targetUnits.empty() && isEmpty((*missionsIter)->targetCity))
                        {
                            if (!isEmpty((*missionsIter)->targetCoords))
                            {
                                const CvPlot* pTargetPlot = (*missionsIter)->getTargetPlot();
                                bool plotIsNoLongerATarget = false;
                                if (pTargetPlot && pTargetPlot->isVisible(player_.getTeamID(), false))
                                {
                                    CvTeamAI& ourTeam = CvTeamAI::getTeam(player_.getTeamID());
                                    PlayerTypes revealedOwner = pTargetPlot->getRevealedOwner(player_.getTeamID(), false);
                                    if (revealedOwner == NO_PLAYER || revealedOwner == player_.getPlayerID())
                                    {
                                        plotIsNoLongerATarget = true;
                                    }
                                    else
                                    {
                                        if (!ourTeam.isAtWar(PlayerIDToTeamID(revealedOwner)))
                                        {
                                            plotIsNoLongerATarget = true;
                                        }
                                    }
                                    if (plotIsNoLongerATarget)
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\n clearing required units for mission: ";
                                        (*missionsIter)->debug(os);
#endif
                                        (*missionsIter)->requiredUnits.clear();
                                        (*missionsIter)->targetCoords = XYCoords();
                                    }
                                }
                            }
                        }
                        updateRequiredUnits = (*missionsIter)->assignedUnits.empty();
                        bool updateHostiles = false;
                        PlotUnitsMap targetStacks;
                        const CvPlot* pFirstStackPlot = NULL;

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
                                bool stackPlotIsCity = pStackPlot->isCity(); // presume we've seen the city if there is one
#ifdef ALTAI_DEBUG
                                os << "\nTarget: " << *targetsIter << " last seen " << whenLastSeen << " turn(s) ago at: " << whereLastSeen;
#endif

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
                                            os << "\n erasing mission target from map " << *targetsIter;
                                        }
#endif
                                        targetsToErase.push_back(std::make_pair(*targetsIter, whereLastSeen));
                                    }
                                }
                                else
                                {
                                    // probably want to move to UnitData objects for tracking targets
                                    // as removes this reliance on getting the real unit
                                    const CvUnit* pTargetUnit = ::getUnit(*targetsIter);
                                    if (pTargetUnit)  // a bit of a cheat as we know the unit is still alive even if we have the wrong location
                                    {
                                        if (!pFirstStackPlot)
                                        {
                                            pFirstStackPlot = pStackPlot;
                                        }
                                        // may want to be smarter here to handle a partial update...?
                                        // as unit was seen in order to be in unitHistory, don't need to recheck for visibility
                                        targetStacks[pStackPlot].push_back(pTargetUnit);  
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

                        float closestCityHostileAttackOdds = 0.0f;
                        for (PlotUnitsMap::iterator stackIter(targetStacks.begin()), stackEndIter(targetStacks.end()); stackIter != stackEndIter; ++stackIter)
                        {
                            const CvPlot* pStackPlot = stackIter->first;
                            bool stackPlotIsCity = pStackPlot->isCity();
#ifdef ALTAI_DEBUG
                            if (pStackPlot->isWater())
                            {
                                os << "\nPotential water based stack";
                            }
#endif
                            CombatGraph::Data endStates = getNearestCityAttackOdds(player_, *missionsIter, pStackPlot);
                            closestCityHostileAttackOdds = endStates.pWin;
                            updateRequiredUnits = stackPlotIsCity || endStates.pWin > hostileAttackThreshold;  // indicates less than 80% chance of our units' survival
#ifdef ALTAI_DEBUG
                            const CvPlot* pClosestCityPlot = player_.getAnalysis()->getMapAnalysis()->getClosestCity(pStackPlot, pStackPlot->getSubArea(), false);
                            os << "\nHostile units at: " << pStackPlot->getCoords() << ", our closest city: " << safeGetCityName(pClosestCityPlot->getPlotCity());
                            if (stackPlotIsCity) os << " hostile units in city: " << safeGetCityName(pStackPlot->getPlotCity());

                            if (pClosestCityPlot)
                            {
                                os << "\nHostile units' distance to our closest city = " << plotDistance(pStackPlot->getX(), pStackPlot->getY(), pClosestCityPlot->getX(), pClosestCityPlot->getY());
                                endStates.debug(os);
                            }
#endif
                            if (pClosestCityPlot)
                            {
#ifdef ALTAI_DEBUG
                                if (!isEmpty((*missionsIter)->closestCity) && (*missionsIter)->closestCity != pClosestCityPlot->getPlotCity()->getIDInfo())
                                {
                                    os << "\n mission: " << *missionsIter << " closest city changing from: " << safeGetCityName((*missionsIter)->closestCity) << " to: " << safeGetCityName(pClosestCityPlot->getPlotCity()->getIDInfo());
                                }
                                else
                                {
                                    os << "\n mission: " << *missionsIter << " setting closest city to: " << safeGetCityName(pClosestCityPlot->getPlotCity()->getIDInfo());
                                }
#endif
                                (*missionsIter)->closestCity = pClosestCityPlot->getPlotCity()->getIDInfo();
                            }

                            if ((*missionsIter)->getTargetPlot() && (*missionsIter)->getTargetPlot()->getCoords() != stackIter->first->getCoords())
                            {
#ifdef ALTAI_DEBUG
                                os << "\n target plot changing from: " << (*missionsIter)->getTargetPlot()->getCoords() << " to: " << stackIter->first->getCoords();
#endif
                            }
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

                        // see if we grab any nearby units
                        if (closestCityHostileAttackOdds > hostileAttackThreshold && !(*missionsIter)->tooDistantAssignedUnits.empty() && !isEmpty((*missionsIter)->closestCity) && pFirstStackPlot)
                        {
                            const CvCity* pRallyCity = ::getCity((*missionsIter)->closestCity);
                            const CvPlot* pRallyTarget = pRallyCity ? pRallyCity->plot() : pFirstStackPlot;
                            std::set<IDInfo> availableUnits = getAssignedUnits(MISSIONAI_RESERVE, subAreaId);
                            MissionFinder missionFinder;
                            missionFinder.missionType = MISSIONAI_ESCORT_WORKER;
                            missionFinder.unitProximityData = UnitPlotProximity(5, pRallyTarget->getCoords());

                            std::list<MilitaryMissionDataPtr>::iterator helperMissionsIter = std::find_if(missionList_.begin(), missionList_.end(), missionFinder);

                            while (helperMissionsIter != missionList_.end())
                            {
                                availableUnits.insert((*helperMissionsIter)->assignedUnits.begin(), (*helperMissionsIter)->assignedUnits.end());
                                std::advance(helperMissionsIter, 1);
                                helperMissionsIter = std::find_if(helperMissionsIter, missionList_.end(), missionFinder);
                            }

                            missionFinder.missionType = MISSIONAI_ESCORT;
                            helperMissionsIter = std::find_if(missionList_.begin(), missionList_.end(), missionFinder);
                            while (helperMissionsIter != missionList_.end())
                            {
                                for (std::set<IDInfo>::iterator aIter((*helperMissionsIter)->assignedUnits.begin()), aEndIter((*helperMissionsIter)->assignedUnits.end());
                                    aIter != aEndIter; ++aIter)
                                {
                                    const CvUnit* pUnit = ::getUnit(*aIter);
                                    if (pUnit && pUnit->canFight())
                                    {
                                        availableUnits.insert(*aIter);
                                    }                                    
                                }
                                std::advance(helperMissionsIter, 1);
                                helperMissionsIter = std::find_if(helperMissionsIter, missionList_.end(), missionFinder);
                            }

                            for (std::set<IDInfo>::iterator aIter(availableUnits.begin()), aEndIter(availableUnits.end()); aIter != aEndIter; ++aIter)
                            {
                                CvUnit* pUnit = ::getUnit(*aIter);
                                if (pUnit)
                                {
                                    std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator unitMissionMapIter = ourUnitsMissionMap_.find(*aIter);
                                    FAssertMsg(unitMissionMapIter != ourUnitsMissionMap_.end() && unitMissionMapIter->second.get() != missionsIter->get(), "reassigning unit to its own mission?");
                                    reassignOurUnit_(pUnit, *missionsIter);
                                }
                            }
#ifdef ALTAI_DEBUG
                            os << "\n available extra units: ";
                            debugUnitSet(os, availableUnits);
#endif                            
                        }
                    }
                    else if (missionAIType == MISSIONAI_GUARD_CITY)
                    {
                        bool updateRequiredUnits = (*missionsIter)->assignedUnits.empty();
                        subAreaId = (*missionsIter)->getTargetPlot()->getSubArea();
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

                // temp hack to unlink escort missions for incompatible unit domains
                if (missionAIType == MISSIONAI_ESCORT_WORKER)
                {
                    if (!(*missionsIter)->targetUnits.empty())
                    {
                        const CvUnit* pTargetUnit = ::getUnit(*(*missionsIter)->targetUnits.begin());
                        if (pTargetUnit && pTargetUnit->getDomainType() != DOMAIN_LAND)
                        {
#ifdef ALTAI_DEBUG
                            os << "\n erasing escort mission with incompatible unit domains: ";                      
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
        }

        void updateMissions_()
        {
            std::list<MilitaryMissionDataPtr> missionsToErase;
            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                // #temp remove obsolete missions - todo ensure these cases are properly cleaned up
                if ((*missionsIter)->missionType == MISSIONAI_EXPLORE && (*missionsIter)->assignedUnits.empty())
                {
                    missionsToErase.push_back(*missionsIter);
                    continue;
                }
                if ((*missionsIter)->missionType == MISSIONAI_COUNTER_CITY)
                {
                    const CvCity* pCity = ::getCity((*missionsIter)->targetCity);
                    if (!pCity)
                    {
                        missionsToErase.push_back(*missionsIter);
                        continue;
                    }
                }
                if ((*missionsIter)->missionType == MISSIONAI_COUNTER && (*missionsIter)->targetUnits.empty() && (*missionsIter)->assignedUnits.empty())
                {
                    missionsToErase.push_back(*missionsIter);
                    continue;
                }
                if ((*missionsIter)->missionType == MISSIONAI_COUNTER && (*missionsIter)->targetUnits.empty() && !(*missionsIter)->getTargetPlot())
                {
                    missionsToErase.push_back(*missionsIter);
                    continue;
                }

                // ##temp hack to add in missing UnitsMission handler
                if ((*missionsIter)->missionType == MISSIONAI_COUNTER || (*missionsIter)->missionType == MISSIONAI_COUNTER_CITY)
                {
                    (*missionsIter)->pUnitsMission = IUnitsMissionPtr(new UnitCounterMission(player_.getCvPlayer()->getStartingPlot()->getSubArea()));
                }
                else if ((*missionsIter)->missionType == MISSIONAI_ESCORT_WORKER)
                {
                    (*missionsIter)->pUnitsMission = IUnitsMissionPtr(new WorkerEscortMission(player_.getCvPlayer()->getStartingPlot()->getSubArea()));
                }
                else if ((*missionsIter)->missionType == MISSIONAI_ESCORT)
                {
                    (*missionsIter)->pUnitsMission = IUnitsMissionPtr(new SettlerEscortMission(player_.getCvPlayer()->getStartingPlot()->getSubArea()));
                }
                else if ((*missionsIter)->missionType == MISSIONAI_RESERVE)
                {
                    (*missionsIter)->pUnitsMission = IUnitsMissionPtr(new ReserveMission());
                }
                else if ((*missionsIter)->missionType == MISSIONAI_GUARD_CITY)
                {
                    (*missionsIter)->pUnitsMission = IUnitsMissionPtr(new CityDefenceMission());
                }

                if ((*missionsIter)->pUnitsMission)
                {
                    (*missionsIter)->pUnitsMission->update(player_);
                }
            }

            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionsToErase.begin()); missionsIter != missionsToErase.end();)
            {
                eraseMission_(*missionsIter++);
            }
        }

        void updateAttackCombatMap_()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
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
            }
        }

        void updateDefenceCombatMap_()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
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
            }
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
            XYCoords previousCoords;
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
            XYCoords foundCoords;

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
            bool isLandDomain = pUnit->getDomainType() == DOMAIN_LAND;
            // todo - filter by team id of passed in unit
            // track separate list of capture units
            for (std::set<IDInfo>::const_iterator unitsIter = stackIter->second.begin(), unitsEndIter = stackIter->second.end(); unitsIter != unitsEndIter; ++unitsIter)
            {
                const CvUnit* pUnit = ::getUnit(*unitsIter);
                if (pUnit && pUnit->canFight())
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

#ifdef ALTAI_DEBUG
            os << "\nunitHasExistingMission: " << unitHasExistingMission << ", foundMissionAtPlot: " << foundMissionAtPlot;
#endif

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

            // for coastal units - search nearby plots and join to mission for nearby hostiles if found
            if (!foundMissionAtPlot && !isLandDomain)
            {
                MissionFinder missionFinder;
                missionFinder.missionType = MISSIONAI_COUNTER_COASTAL;
                missionFinder.domainType = DOMAIN_SEA;
                missionFinder.proximityData = MissionPlotProximity(10, pUnit->plot()->getCoords());
            }

            const bool unitHasNewMission = !foundMissionAtPlot && !unitHasExistingMission;
#ifdef ALTAI_DEBUG
            os << ", foundMissionAtPlot2: " << foundMissionAtPlot << ", unitHasNewMission: " << unitHasNewMission;
#endif
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
                pMission = isLandDomain ? UnitCounterMission::createMission(subArea, false) : CoastalDefenceMission::createMission(subArea);
                addMission_(pMission);
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
                    CombatGraph::Data endStates = getNearestCityAttackOdds(player_, pMission, pUnitPlot);
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
                pMission->debug(os, true);
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
                    pMission = SettlerEscortMission::createMission(pSettlerUnit->plot()->getSubArea());
                    addMission_(pMission);
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
                os << " bestReserveUnitScore: " << bestReserveUnitScore << ", bestReserveEscortUnit: " << bestReserveEscortUnit << "\n";
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
                        os << " checking escort unit: " << gGlobals.getUnitInfo(vi->first).getType() << " with value: " << vi->second;
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
                    if (!isEmpty(refCoords))
                    {
                        MilitaryMissionDataPtr pMission(new MilitaryMissionData(MISSIONAI_EXPLORE));
                        addMission_(pMission);
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
                    MilitaryMissionDataPtr pMission(new MilitaryMissionData(MISSIONAI_EXPLORE));
                    addMission_(pMission);
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
                MilitaryMissionDataPtr pMission(new MilitaryMissionData(MISSIONAI_BUILD));
                addMission_(pMission);
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
                            MilitaryMissionDataPtr pEscortMission = WorkerEscortMission::createMission(pWorkerUnit->plot()->getSubArea());
                            addMission_(pEscortMission);
                            pEscortMission->targetUnits.insert(pWorkerUnit->getIDInfo());
                            reassignOurUnit_(pEscortUnit, pEscortMission);
                        }
                    }
                }

                return pMission;
            }

            return MilitaryMissionDataPtr();
        }

        MilitaryMissionDataPtr addMissionaryUnitMission_(const CvUnitAI* pMissionaryUnit)
        {
            UnitAITypes unitAIType = pMissionaryUnit->AI_getUnitAIType();
            if (unitAIType == UNITAI_MISSIONARY || unitAIType == UNITAI_MISSIONARY_SEA)
            {
                MilitaryMissionDataPtr pMission(new MilitaryMissionData(MISSIONAI_SPREAD));
                addMission_(pMission);
                pMission->assignUnit(pMissionaryUnit, false);
                addOurUnitMission_(pMission, pMissionaryUnit->getIDInfo());
#ifdef ALTAI_DEBUG
                std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
                os << "\nnew missionary mission for unit: " << pMissionaryUnit->getIDInfo();
                pMission->debug(os);
#endif
            }
            return MilitaryMissionDataPtr();
        }

        MilitaryMissionDataPtr addCityGuardMission_(const CvCity* pCity)
        {
            MilitaryMissionDataPtr pMission(new MilitaryMissionData(MISSIONAI_GUARD_CITY));
            addMission_(pMission);
            pMission->targetCity = pCity->getIDInfo();

            pMission->requiredUnits = pCityDefenceAnalysis_->getRequiredUnits(pCity->getIDInfo());

            return pMission;
        }

        MilitaryMissionDataPtr addEscortWorkerUnitMission_(const CvUnitAI* pUnit, const CvUnitAI* pWorkerUnit)
        {
            MilitaryMissionDataPtr pMission = WorkerEscortMission::createMission(pWorkerUnit->plot()->getSubArea());            
            addMission_(pMission);
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

        MilitaryMissionDataPtr addReserveLandUnitMission_(CvUnit* pUnit, const CvPlot* pTarget)
        {
            MilitaryMissionDataPtr pMission;

            //MissionFinder missionFinder;
            //missionFinder.missionType = MISSIONAI_RESERVE;
            //if (pCapital)
            //{
            //    missionFinder.city = pCapital->getIDInfo();
            //}
            //else
            //{
            //    // might be initial unit assignment and we have no capital yet
            //    // units start on the same plot (except for human players)
            //    // but add proximity check anyway to avoid creating multiple reserver missions for one sub area
            //    missionFinder.proximityData = MissionPlotProximity(2, pUnit->plot()->getCoords());
            //}
            //std::list<MilitaryMissionDataPtr>::const_iterator missionsIter = std::find_if(missionList_.begin(), missionList_.end(), missionFinder);

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
                pMission = ReserveMission::createMission();
                addMission_(pMission);
                if (pTarget->isCity())
                {
                    pMission->targetCity = pTarget->getPlotCity()->getIDInfo();
                }
                else
                {
                    pMission->targetCoords = pTarget->getCoords();
                }
            }
            reassignOurUnit_(pUnit, pMission);
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

            MissionFinder finder;
            finder.domainType = pUnit->getDomainType();
            finder.requestedUnitType = pUnit->getUnitType();
            if (finder.domainType == DOMAIN_LAND)
            {
                finder.subArea = subArea;
            }

            std::list<MilitaryMissionDataPtr> possibleMissions;

            std::list<MilitaryMissionDataPtr>::iterator candidateMissionsIter = std::find_if(missionList_.begin(), missionList_.end(), finder);
            while (candidateMissionsIter != missionList_.end())
            {
                possibleMissions.push_back(*candidateMissionsIter);
                std::advance(candidateMissionsIter, 1);
                candidateMissionsIter = std::find_if(candidateMissionsIter, missionList_.end(), finder);
            }

            CounterMissionPriorityComp comp;
            comp.newUnitCoords = pUnit->plot()->getCoords();
            possibleMissions.sort(comp);

            if (!possibleMissions.empty())
            {
                (*possibleMissions.begin())->assignUnit(pUnit);
                ourUnitsMissionMap_[pUnit->getIDInfo()] = *possibleMissions.begin();
                return *possibleMissions.begin();
            }

            //for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            //{
            //    if (!((*missionsIter)->missionType == MISSIONAI_COUNTER || (*missionsIter)->missionType == MISSIONAI_COUNTER_CITY))
            //    {
            //        continue;
            //    }

            //    for (std::set<IDInfo>::const_iterator targetsIter((*missionsIter)->targetUnits.begin()); targetsIter != (*missionsIter)->targetUnits.end(); ++targetsIter)
            //    {
            //        // need a way to ask if the mission relates to this sub area
            //        if (unitsAreaIter->second.find(*targetsIter) != unitsAreaIter->second.end() || hiddenUnitsAreaIter->second.find(*targetsIter) != hiddenUnitsAreaIter->second.end())
            //        {
            //            int requiredCount = (*missionsIter)->getRequiredUnitCount(pUnit->getUnitType());
            //            if (requiredCount > 0)
            //            {
            //                (*missionsIter)->assignUnit(pUnit);
            //                ourUnitsMissionMap_[pUnit->getIDInfo()] = *missionsIter;
            //                return *missionsIter;
            //            }
            //        }
            //    }
            //}
            return MilitaryMissionDataPtr();
        }

        void reassignOurUnit_(CvUnit* pUnit, const MilitaryMissionDataPtr& pMission)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            CvSelectionGroup* pGroup = pUnit->getGroup();
            pGroup->clearMissionQueue(__FUNCTION__);

            if (pGroup->getNumUnits() > 1)
            {
                pUnit->joinGroup(NULL);  // so we don't have units in same group in different missions
#ifdef ALTAI_DEBUG
                os << "\nRemoving unit from group: " << pGroup->getID() << " new group: " << pUnit->getGroupID();
#endif
            }
            
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
                os << "\n to mission: ";
                pMission->debug(os);
#endif
            }
            else
            {
                addOurUnitMission_(pMission, pUnit->getIDInfo());
            }
            pMission->assignUnit(pUnit);
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
            os << "\n erasing mission from list (size = " << missionList_.size() << ") ";
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
                        os << "\nReassigned our unit: " << pUnit->getIDInfo() << " to mission: ";
                        pMission->debug(os);
#endif
                    }
                }
            }
        }

        MilitaryMissionDataPtr addMission_(const MilitaryMissionDataPtr& pMission)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nAdding mission: " << pMission << " " << getMissionAIString(pMission->missionType);
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

        std::pair<IDInfo, int> updateUnitBuildData_(IDInfo city, const UnitTacticValue& unitTactic)
        {
            cityBuildsTimes_[city][unitTactic.unitType] = unitTactic.nTurns;

            int bestBuildTime = unitTactic.nTurns;
            IDInfo bestCity = city;

            for (std::map<IDInfo, std::map<UnitTypes, int> >::const_iterator buildTimesIter(cityBuildsTimes_.begin()), buildTimesEndIter(cityBuildsTimes_.end());
                buildTimesIter != buildTimesEndIter; ++buildTimesIter)
            {
                std::map<UnitTypes, int>::const_iterator unitBuildTimeIter = buildTimesIter->second.find(unitTactic.unitType);
                if (unitBuildTimeIter != buildTimesIter->second.end())
                {
                    if (unitBuildTimeIter->second < bestBuildTime)
                    {
                        bestBuildTime = unitBuildTimeIter->second;
                        bestCity = buildTimesIter->first;
                    }
                }
            }

            return std::make_pair(bestCity, bestBuildTime);
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

    float MilitaryAnalysis::attThreshold = 0.85f, MilitaryAnalysis::defAttackThreshold = 0.50f, 
        MilitaryAnalysis::defThreshold = 0.90f, MilitaryAnalysis::hostileAttackThreshold = 0.20f;

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

    const std::map<IDInfo, UnitHistory>& MilitaryAnalysis::getUnitHistories() const
    {
        return pImpl_->getUnitHistories();
    }

    const std::list<MilitaryMissionDataPtr>& MilitaryAnalysis::getMissions() const
    {
        return pImpl_->getMissions();
    }

    MilitaryMissionDataPtr MilitaryAnalysis::getMissionData(CvUnitAI* pUnit)
    {
        return pImpl_->getMissionData(pUnit);
    }

    MilitaryMissionDataPtr MilitaryAnalysis::getCityDefenceMission(IDInfo city)
    {
        return pImpl_->getCityDefenceMission(city);
    }

    UnitRequestData MilitaryAnalysis::getUnitRequestBuild(const CvCity* pCity, const TacticSelectionData& tacticSelectionData)
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

    const std::map<XYCoords, std::list<IDInfo> >& MilitaryAnalysis::getEnemyStacks() const
    {
        return pImpl_->getEnemyStacks();
    }

    PlotUnitDataMap MilitaryAnalysis::getEnemyUnitData(int subArea) const
    {
        return pImpl_->getEnemyUnitData(subArea);
    }

    const std::map<XYCoords, CombatData>& MilitaryAnalysis::getAttackCombatMap() const
    {
        return pImpl_->getAttackCombatMap();
    }

    const std::map<XYCoords, CombatData>& MilitaryAnalysis::getDefenceCombatMap() const
    {
        return pImpl_->getDefenceCombatMap();
    }

    const std::map<XYCoords, CombatGraph::Data>& MilitaryAnalysis::getAttackCombatData() const
    {
        return pImpl_->getAttackCombatData();
    }

    const std::map<XYCoords, CombatGraph::Data>& MilitaryAnalysis::getDefenceCombatData() const
    {
        return pImpl_->getDefenceCombatData();
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
        MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
        return pMilitaryAnalysis->getImpl()->getNearbyHostileStacks(pPlot, range);
    }

    std::set<XYCoords> getUnitHistory(const Player& player, IDInfo unit)
    {
        MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
        return pMilitaryAnalysis->getImpl()->getUnitHistory(unit);
    }

    void updateMilitaryAnalysis(Player& player)
    {
        MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
        pMilitaryAnalysis->update();
    }

    bool doUnitAnalysis(Player& player, CvUnitAI* pUnit)
    {
        MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
        return pMilitaryAnalysis->updateOurUnit(pUnit);
    }
}