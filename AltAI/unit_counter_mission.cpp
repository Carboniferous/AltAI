#include "AltAI.h"

#include "./unit_counter_mission.h"
#include "./unit_mission_helper.h"
#include "./helper_fns.h"
#include "./save_utils.h"
#include "./iters.h"
#include "./unit_log.h"
#include "./civ_log.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./military_tactics.h"

namespace AltAI
{
    namespace
    {
        std::list<CombatMoveData>::const_iterator checkRiverCrossings(const CvPlot* pTargetPlot, const std::list<CombatMoveData>& noAttackMoveData)
        {
            std::list<CombatMoveData>::const_iterator noAttackIter = noAttackMoveData.begin();
            float bestSurvivalOdds = noAttackIter->defenceOdds.pLoss;  // odds are for our units surviving hostiles attacking us
            // don't wait in position where have river crossing attack penalty on target
            // prob want to adjust this to wait until just before ready to attack, e.g. still grouping units
            // also don't care if units are amphibious (not v. likely)
            while (pTargetPlot->isRiverCrossing(directionXY(pTargetPlot, gGlobals.getMap().plot(noAttackIter->coords.iX, noAttackIter->coords.iY))))
            {
                ++noAttackIter;
                if (noAttackIter == noAttackMoveData.end())
                {
                    --noAttackIter;
                    break;
                }
                // our survival odds less than 0.9 and less than the best value from above - if so - move back one plot in the list
                if (noAttackIter != noAttackMoveData.begin() && noAttackIter->defenceOdds.pLoss < MilitaryAnalysis::defThreshold && noAttackIter->defenceOdds.pLoss < bestSurvivalOdds)
                {
                    --noAttackIter;
                    break;
                }
            }
            return noAttackIter;
        }

        void updateTargetStacks(const Player& player, MilitaryMissionData* pMission, 
            PlotUnitsMap& targetStacks, PlotUnitDataMap& possibleTargetStacks)
        {
            MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
            TeamTypes ourTeam = player.getTeamID();

            for (std::set<IDInfo>::const_iterator targetsIter(pMission->targetUnits.begin()), targetsEndIter(pMission->targetUnits.end());
                targetsIter != targetsEndIter; ++targetsIter)
            {
                const CvUnit* pTargetUnit = ::getUnit(*targetsIter);
                if (pTargetUnit)
                {
                    const CvPlot* pTargetPlot = pTargetUnit->plot();
                    if (pTargetPlot->isVisible(ourTeam, false))
                    {
                        targetStacks[pTargetPlot].push_back(pTargetUnit);
                    }
                }
            }

            const std::map<IDInfo, UnitHistory>& unitHistories = pMilitaryAnalysis->getUnitHistories();
            for (std::set<IDInfo>::const_iterator targetsIter(pMission->targetUnits.begin()), targetsEndIter(pMission->targetUnits.end());
                targetsIter != targetsEndIter; ++targetsIter)
            {
                std::map<IDInfo, UnitHistory>::const_iterator histIter = unitHistories.find(*targetsIter);
                FAssertMsg(histIter != unitHistories.end() && !histIter->second.locationHistory.empty(), "Target unit without any history?");
                if (histIter != unitHistories.end() && !histIter->second.locationHistory.empty())
                {
                    const XYCoords coords = histIter->second.locationHistory.begin()->second;                    
                    const CvUnit* pPossibleHostileUnit = ::getUnit(*targetsIter);
                    if (pPossibleHostileUnit)  // need to modify UnitHistory to store UnitData objects
                    {
                        possibleTargetStacks[gGlobals.getMap().plot(coords.iX, coords.iY)].push_back(UnitData(pPossibleHostileUnit));
                    }
                }
            }
        }

        std::pair<XYCoords, bool> updateMissionGroupData(const Player& player, MilitaryMissionData* pMission, CvUnit* pUnit,
            const CvPlot* pTargetPlot, CvSelectionGroup*& pClosestGroup, PlotUnitDataMap& possibleTargetStacks, int remainingTurnsToCounter)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
            const int currentTurn = gGlobals.getGame().getGameTurn();
#endif
            const CvPlot* pUnitPlot = pUnit->plot();
            std::set<CvSelectionGroup*, CvSelectionGroupOrderF> groupsInMission = pMission->getAssignedGroups();
            std::list<std::pair<CvSelectionGroup*, UnitPathData> > groupPathData;
            std::list<std::pair<CvSelectionGroup*, UnitPathData> >::const_iterator ourGroupIter;                
            int closestGroupTurnsToTarget = MAX_INT;
            bool joinClosestGroupIfNotAttacking = false;

            pMission->tooDistantAssignedUnits.clear();

            // iterate over all groups in mission
            for (std::set<CvSelectionGroup*, CvSelectionGroupOrderF>::const_iterator groupIter(groupsInMission.begin()), groupEndIter(groupsInMission.end());
                groupIter != groupEndIter; ++groupIter)
            {
                // calculate this group's path to target
                UnitPathData unitPathData;
                unitPathData.calculate(*groupIter, pTargetPlot, MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY);

                if (unitPathData.valid)
                {
                    if (unitPathData.pathTurns < closestGroupTurnsToTarget)
                    {
                        pClosestGroup = *groupIter;
                        closestGroupTurnsToTarget = unitPathData.pathTurns;
                    }
                    if (unitPathData.pathTurns > remainingTurnsToCounter)
                    {
                        UnitGroupIter unitIter(*groupIter);
                        while (CvUnit* pUnit = unitIter())
                        {
                            pMission->tooDistantAssignedUnits.push_back(pUnit->getIDInfo());
                        }
                    }
                }

                std::list<std::pair<CvSelectionGroup*, UnitPathData> >::iterator groupPathIter = groupPathData.end();

                if (!isEmpty(pMission->closestCity))
                {
                    const CvCity* pClosestCity = ::getCity(pMission->closestCity);
                    if (pClosestCity)
                    {
                        UnitPathData altUnitPathData;
                        altUnitPathData.calculate(*groupIter, pClosestCity->plot(), MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY);
                        if (altUnitPathData.valid)
                        {
#ifdef ALTAI_DEBUG
                            os << "\n turns to: " << pTargetPlot->getCoords() << " = " << unitPathData.pathTurns << ", turns to: " << pClosestCity->plot()->getCoords() << " = " << altUnitPathData.pathTurns;
#endif
                            if (altUnitPathData.pathTurns <= unitPathData.pathTurns)
                            {
#ifdef ALTAI_DEBUG
                                os << "\nFor group: " << (*groupIter)->getID() << " picked city target: "
                                    << safeGetCityName(pMission->closestCity) << " (" << unitPathData.pathTurns << ")";
#endif
                                groupPathIter = groupPathData.insert(groupPathData.begin(), std::make_pair(*groupIter, altUnitPathData));
                            }
                        }
                    }
                }
                    
                if (groupPathIter == groupPathData.end())
                {
                    groupPathIter = groupPathData.insert(groupPathData.begin(), std::make_pair(*groupIter, unitPathData));
                }

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

#ifdef ALTAI_DEBUG
            for (size_t i = 0, count = pMission->tooDistantAssignedUnits.size(); i < count; ++i)
            {
                const CvUnit* pUnit = ::getUnit(pMission->tooDistantAssignedUnits[i]);
                os << "\n unit: " << pMission->tooDistantAssignedUnits[i] << " at: " << pUnit->plot()->getCoords()
                    << " is too far away to defend nearest threatened city";
            }
#endif
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
            return std::make_pair(stepCoords, joinClosestGroupIfNotAttacking);
        }

        void combineMoveData(const Player& player, MilitaryMissionData* pMission, CvUnit* pUnit, const CvPlot* pTargetPlot, 
            XYCoords stepCoords, bool deferAttack, GroupCombatData& combatData, std::list<CombatMoveData>& noAttackMoveData, std::list<CombatMoveData>& attackMoveData)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
#endif
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

                if (deferAttack && coords == pTargetPlot->getCoords())
                {
#ifdef ALTAI_DEBUG
                    os << "\n\tskipping plot: " << coords << " as defer attack is true";
#endif
                    continue;  // skip attack data for target (todo - make protected coords dynamic)
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
        }
    }

    MilitaryMissionDataPtr UnitCounterMission::createMission(int subArea, bool isCity)
    {
        MilitaryMissionDataPtr pMission(new MilitaryMissionData(isCity ? MISSIONAI_COUNTER_CITY: MISSIONAI_COUNTER));
        pMission->pUnitsMission = IUnitsMissionPtr(new UnitCounterMission(subArea));
        return pMission;
    }

    UnitCounterMission::UnitCounterMission(int subArea) : subArea_(subArea)
    {
    }

    bool UnitCounterMission::doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission)
    {
        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner());
        MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();

#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
        const int currentTurn = gGlobals.getGame().getGameTurn();
#endif
        TeamTypes ourTeam = player.getTeamID();
        const CvPlot* pUnitPlot = pUnit->plot();
        // ourUnits = true, useMaxMoves = false, canAttack = true
        pMission->updateReachablePlots(player, true, false, true);
        pMission->pushedAttack = std::pair<IDInfo, XYCoords>();

        PlotUnitsMap targetStacks;
        PlotUnitDataMap possibleTargetStacks;
        updateTargetStacks(player, pMission, targetStacks, possibleTargetStacks);

        const CvPlot* pTargetPlot = pMission->getTargetPlot();
        if (!targetStacks.empty())
        {            
            if (!targetStacks.empty())
            {
                // todo - flag up multiple targets
                pTargetPlot = targetStacks.begin()->first;
            }
            else
            {
                pTargetPlot = possibleTargetStacks.begin()->first;
#ifdef ALTAI_DEBUG
                os << "\n selected possible target: " << possibleTargetStacks.begin()->first->getCoords();
#endif
            }
            // don't reset coords as this could lead to mission being erased if nearby city mission is created
            //pMission->targetCoords = pTargetPlot->getCoords(); 
        }

//        if (pMission->targetUnits.empty() && isEmpty(pMission->targetCity) && pUnit->plot()->getCoords() == pMission->targetCoords)
//        {
//            // done
//#ifdef ALTAI_DEBUG
//            os << "\nUnit at target coords - counter mission complete";
//#endif
//            return false;
//        }

        bool targetIsCity = pTargetPlot && pTargetPlot->isCity(false, ourTeam);
        
        // ourUnits = false, useMaxMoves = false, canAttack = true
        pMission->updateReachablePlots(player, false, false, true);
        pMission->updateRequiredUnits(player, std::set<IDInfo>());   // no reserve units passed here

        PlotSet sharedPlots, borderPlots;
        getOverlapAndNeighbourPlots(pMission->hostilesReachablePlots, pMission->ourReachablePlots, sharedPlots, borderPlots);
        int remainingTurnsToCounter = std::numeric_limits<int>::max();

        bool deferAnyAttack = false;
        if (targetIsCity)
        {
            const CvCity* pTargetCity = pTargetPlot->getPlotCity();
            if (pTargetCity && pTargetCity->isBarbarian() && pTargetCity->getCultureLevel() == 1 && pTargetCity->getPopulation() == 1)
            {
                // don't want to raze a city we could capture if we wait a few turns
                // todo - check city is growing
#ifdef ALTAI_DEBUG
                os << "\nDelaying counter mission attack on target to avoid razing city..."; 
#endif
                deferAnyAttack = true;
            }
        }

        if (!isEmpty(pMission->closestCity))
        {
            for (PlotUnitsMap::const_iterator targetStackIter(targetStacks.begin()), targetStackEndIter(targetStacks.end()); targetStackIter != targetStackEndIter; ++targetStackIter)
            {
                UnitPathData hostileUnitPaths;
                hostileUnitPaths.calculate(makeUnitData(targetStackIter->second), targetStackIter->first,
                    ::getCity(pMission->closestCity)->plot(), MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY, (*targetStackIter->second.begin())->getOwner(), player.getTeamID());
                if (hostileUnitPaths.valid)
                {
                    remainingTurnsToCounter = std::min<int>(remainingTurnsToCounter, hostileUnitPaths.pathTurns);
                }
            }
        }

#ifdef ALTAI_DEBUG
        pMission->debugReachablePlots(os);
        os << "\n attackable plots: " << sharedPlots.size() << ", bordering plots: " << borderPlots.size() << ", assigned units count: " << pMission->assignedUnits.size();
        if (!isEmpty(pMission->closestCity))
        {
            os << "\n remaining turns to counter = " << remainingTurnsToCounter << " v. city: " << safeGetCityName(pMission->closestCity);
        }
#endif

        bool isAttackMove = !isEmpty(pMission->nextAttack.first);
        if (isAttackMove)
        {
            if (pTargetPlot && pMission->nextAttack.second != pTargetPlot->getCoords())
            {
#ifdef ALTAI_DEBUG
                os << "\n target plot from mission: " << pTargetPlot->getCoords() << " overriding to: " << pMission->nextAttack.second;
#endif
                pTargetPlot = gGlobals.getMap().plot(pMission->nextAttack.second);
            }
        }

        bool joinClosestGroupIfNotAttacking = false;
        XYCoords moveToCoords = !isEmpty(pMission->nextAttack.second) ? pMission->nextAttack.second : (pTargetPlot ? pTargetPlot->getCoords() : XYCoords());        

        if (!pTargetPlot && isEmpty(moveToCoords))
        {
#ifdef ALTAI_DEBUG
            os << "\nUnable to determine target plot for counter mission";
#endif
            return false;
        }

        CvSelectionGroup* pClosestGroup = NULL;

        bool finishedAttack = pUnit->isMadeAttack() && !pUnit->isBlitz();
        if (finishedAttack)
        {
            isAttackMove = false;
        }

        const std::map<XYCoords, CombatGraph::Data>& attackCombatData = pMilitaryAnalysis->getAttackCombatData();

        // should we attack based on calculated attackCombatData?
        std::map<XYCoords, CombatGraph::Data>::const_iterator oddsIter = attackCombatData.find(pTargetPlot->getCoords());
        if (!deferAnyAttack && !finishedAttack && oddsIter != attackCombatData.end() && (oddsIter->second.pWin > MilitaryAnalysis::attThreshold || !isEmpty(pMission->nextAttack.first)))
        {
            IDInfo expectedAttackUnit = *oddsIter->second.longestAndShortestAttackOrder.first.begin();
            if (expectedAttackUnit == pUnit->getIDInfo())
            {
                moveToCoords = pTargetPlot->getCoords();
                isAttackMove = true;
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\nUnexpected attack unit: expected: " << expectedAttackUnit << " got: " << pUnit->getIDInfo();
#endif
            }
            // think this is not required now as getNextAttackUnit() should update
            //oddsIter->second.longestAndShortestAttackOrder.first.erase(oddsIter->second.longestAndShortestAttackOrder.first.begin());
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
            isAttackMove = false; // for now
        }

        GroupCombatData combatData;
        // need this as attack and defence combat map only cover static defence and active attacks, not moving for defence
        combatData.calculate(player, pMission->getAssignedUnits(true), possibleTargetStacks);

        // not got an attack set from MilitaryMissionData analysis
        if (!isAttackMove)
        {
            XYCoords stepCoords;
            boost::tie(stepCoords, joinClosestGroupIfNotAttacking) = updateMissionGroupData(player, pMission, pUnit, pTargetPlot, pClosestGroup, possibleTargetStacks, remainingTurnsToCounter);

            std::list<CombatMoveData> noAttackMoveData, attackMoveData;
            combineMoveData(player, pMission, pUnit, pTargetPlot, stepCoords, deferAnyAttack, combatData, noAttackMoveData, attackMoveData);
            
            // can attack something...
            if (!finishedAttack && !attackMoveData.empty())
            {
                attackMoveData.sort(AttackMoveDataPred(MilitaryAnalysis::attThreshold));
                const CombatMoveData& moveData = *attackMoveData.begin();

                if (!deferAnyAttack)
                {
                    boost::tie(isAttackMove, moveToCoords) = checkAttackOdds(moveData, pTargetPlot->getCoords());

#ifdef ALTAI_DEBUG
                    if (isAttackMove)
                    {
                        os << "\n matched attack move from mission: pWin: " << moveData.attackOdds.pWin;
                        moveData.attackOdds.debug(os);
                    }
#endif
                }

                if (!isAttackMove && !isEmpty(pMission->closestCity))
                {
                    // don't check deferAnyAttack here (unlikely anyway as it's set when attacking a barb city, not when units thereaten our cities)
                    // todo - take into account whether hostiles have moved
                    boost::tie(isAttackMove, moveToCoords) = checkCityAttackOdds(moveData, pMission->cityAttackOdds);

#ifdef ALTAI_DEBUG
                    if (isAttackMove)
                    {
                        os << "\n selected attack move as city threatened: pLoss: " << moveData.attackOdds.pLoss;
                        moveData.attackOdds.debug(os);
                    }
#endif
                }

                if (isAttackMove)
                {
                    if (*moveData.attackOdds.longestAndShortestAttackOrder.first.begin() != pUnit->getIDInfo())
                    {
#ifdef ALTAI_DEBUG
                        os << "\nneed to split group before attack: " << *moveData.attackOdds.longestAndShortestAttackOrder.first.begin() << ", " << pUnit->getIDInfo();
#endif
                        if (pUnit->getGroup()->getNumUnits() > 1)
                        {
                            pUnit->joinGroup(NULL);
                        }
                        else
                        {
                            pUnit->getGroup()->pushMission(MISSION_SKIP, pUnitPlot->getX(), pUnitPlot->getY(), 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                        }
                        return true;
                    }
                }
                    
                if (!isAttackMove)
                {
                    moveToCoords = XYCoords();
                }
            }

            // not attacking
            if (!isAttackMove && isEmpty(moveToCoords))
            {
                // sort by distance and chance of successful hostile attack
                noAttackMoveData.sort(DefenceMoveDataPred(MilitaryAnalysis::hostileAttackThreshold));

#ifdef ALTAI_DEBUG
                logNoAttackMoveCombatData(os, noAttackMoveData);
#endif
                std::list<CombatMoveData>::const_iterator noAttackIter = checkRiverCrossings(pTargetPlot, noAttackMoveData);
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

        if (!isEmpty(moveToCoords)) // just log odds to check they match attackCombatData from Military Analysis
        {
#ifdef ALTAI_DEBUG
            logMoveToCoordsCombatData(os, combatData, moveToCoords);
#endif
        }

        // if we have a nearby group and we've already attacked (and aren't blitz) join it.
        if (joinClosestGroupIfNotAttacking && (!isAttackMove || finishedAttack))
        {
            pUnit->joinGroup(pClosestGroup);
            return true;
        }
        // have other units still to attack
        else if (finishedAttack && !isEmpty(pMission->nextAttack.first))  // units with moves remaining that have attacked but no coincident stack to rejoin
        {
            // attempt to create a new group to effectively hold unit until rest of attack is finished
            // which means we can avoid finishing this unit's moves and instead return to the next selection group in the cycle
            // can then hopefully move or hold as required - this is basically a clunky way of saying wait for orders
            std::set<IDInfo>::iterator holdingUnitsIter = holdingUnits_.find(pUnit->getIDInfo());
            if (holdingUnitsIter != holdingUnits_.end())
            {
#ifdef ALTAI_DEBUG
                os << "\nremoving unit: " << pUnit->getIDInfo() << " from hold list";
#endif
                holdingUnits_.erase(holdingUnitsIter);
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\nadding unit: " << pUnit->getIDInfo() << " to hold list";
#endif
                holdingUnits_.insert(pUnit->getIDInfo());
                pUnit->joinGroup(NULL);
                return true;
            }
        }

        const CvPlot* pMovePlot = gGlobals.getMap().plot(moveToCoords.iX, moveToCoords.iY);
            
        std::pair<IDInfo, XYCoords> attackData = pMission->nextAttack;
        pMission->nextAttack = std::make_pair(IDInfo(), XYCoords());  // clear next attacker so we don't loop

        if (pMovePlot != pUnitPlot && !(finishedAttack && !targetStacks[pMovePlot].empty()))
        {
            if (isAttackMove)
            {
                if (pUnit->getGroupSize() > 1)
                {
#ifdef ALTAI_DEBUG
                    os << "\nsplitting group for attack unit: " << pUnit->getIDInfo();
#endif
                    pUnit->joinGroup(NULL);
                }
                
#ifdef ALTAI_DEBUG
                
                os << "\nMission: " << pMission << " turn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " pushed counter mission attack move to: " << moveToCoords;
#endif
                pMission->pushedAttack = attackData;  // store this as useful for updating combat data after combat is resolved
                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, moveToCoords.iX, moveToCoords.iY, MOVE_IGNORE_DANGER, false, false, MISSIONAI_COUNTER,
                    (CvPlot*)pTargetPlot, 0, __FUNCTION__);
                return true;
            }
            else
            {
                // calculate a final set of movement data (not sure this is always needed - can use stepCoords perhaps?)
                UnitPathData moveData;
                moveData.calculate(pUnit->getGroup(), pMovePlot, MOVE_IGNORE_DANGER);
                if (moveData.valid)
                {
                    XYCoords firstStep = moveData.getFirstStepCoords();
                    if (deferAnyAttack && firstStep == pTargetPlot->getCoords())
                    {
#ifdef ALTAI_DEBUG
                        os << "\nMission: " << pMission << " turn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " pushed counter mission skip to avoid razing target";
#endif
                        pUnit->getGroup()->pushMission(MISSION_SKIP, pUnitPlot->getX(), pUnitPlot->getY(), 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                        return true;
                    }

                    if (firstStep != pUnitPlot->getCoords())
                    {
#ifdef ALTAI_DEBUG
                        os << "\nMission: " << pMission << " turn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " pushed counter mission move to: " << firstStep;
#endif
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, firstStep.iX, firstStep.iY, MOVE_IGNORE_DANGER, false, false, MISSIONAI_COUNTER,
                            (CvPlot*)pTargetPlot, 0, __FUNCTION__);
                        return true;
                    }
                }
            }
        }

        if (pUnit->atPlot(pTargetPlot) && pMission->targetUnits.empty() && possibleTargetStacks.empty())
        {
#ifdef ALTAI_DEBUG
            os << "\nMission: " << pMission << " turn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " failed to find any targets and at target plot.";
#endif
            return false;  // reassign unit
        }

#ifdef ALTAI_DEBUG
        os << "\nMission: " << pMission << " turn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " pushed counter mission skip ";
#endif
        pUnit->getGroup()->pushMission(MISSION_SKIP, pUnitPlot->getX(), pUnitPlot->getY(), 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
        return true;

    }

    void UnitCounterMission::update(Player& player)
    {
    }

    void UnitCounterMission::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(subArea_);
        writeComplexSet(pStream, holdingUnits_);
    }

    void UnitCounterMission::read(FDataStreamBase* pStream)
    {
        pStream->Read(&subArea_);
        readComplexSet(pStream, holdingUnits_);
    }


}
