#include "AltAI.h"

#include "./coastal_defence_mission.h"
#include "./unit_mission_helper.h"
#include "./helper_fns.h"
#include "./iters.h"
#include "./unit_log.h"
#include "./civ_log.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./military_tactics.h"
#include "./unit_tactics.h"

namespace AltAI
{
    namespace
    {
        struct GroupMoveData
        {
            GroupMoveData() : joinGroupAtPlotIfNotAttacking(false), pClosestGroupToHostiles(NULL) {}
            XYCoords stepCoords, closestHostileCoords;
            bool joinGroupAtPlotIfNotAttacking;
            CvSelectionGroup* pClosestGroupToHostiles;
        };

        void getPossibleTargets(const Player& player, MilitaryMissionData* pMission, PlotUnitDataMap& possibleTargetStacks)
        {
            MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();

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

        GroupMoveData updateMissionGroupData(const Player& player, MilitaryMissionData* pMission, CvUnit* pUnit, const PlotUnitDataMap& possibleTargetStacks)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
            const int currentTurn = gGlobals.getGame().getGameTurn();
#endif
            GroupMoveData moveData;

            const CvPlot* pUnitPlot = pUnit->plot();
            std::set<CvSelectionGroup*, CvSelectionGroupOrderF> groupsInMission = pMission->getAssignedGroups();
            std::list<std::pair<CvSelectionGroup*, std::list<UnitPathData> > > groupPathData;
            std::list<std::pair<CvSelectionGroup*, std::list<UnitPathData> > >::const_iterator ourGroupIter;                
            int closestGroupTurnsToTarget = MAX_INT, ourDistanceToClosestGroup = MAX_INT;
            CvSelectionGroup* pOurClosestGroup = NULL;
            std::list<UnitPathData> thisGroupsTargets;

            pMission->tooDistantAssignedUnits.clear();

            // iterate over all groups in mission
            for (std::set<CvSelectionGroup*, CvSelectionGroupOrderF>::const_iterator groupIter(groupsInMission.begin()), groupEndIter(groupsInMission.end());
                groupIter != groupEndIter; ++groupIter)
            {
                for (std::set<CvSelectionGroup*, CvSelectionGroupOrderF>::const_iterator otherGroupIter(groupIter), otherGroupEndIter(groupsInMission.end());
                    groupIter != groupEndIter; ++groupIter)
                {
                    // calculate this group's path to other group in mission
                    UnitPathData unitPathData;
                    unitPathData.calculate(*groupIter, (*otherGroupIter)->plot(), MOVE_MAX_MOVES | MOVE_IGNORE_DANGER);
                    if (unitPathData.valid)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nDistance from group: " << (*groupIter)->getID() << " to group: " << (*otherGroupIter)->getID() << ": " << unitPathData.pathTurns;
#endif
                        // if our unit's group...
                        if (((*groupIter)->getID() == pUnit->getGroupID() || (*otherGroupIter)->getID() == pUnit->getGroupID())
                            && unitPathData.pathTurns < ourDistanceToClosestGroup)
                        {
                            ourDistanceToClosestGroup = unitPathData.pathTurns;
                            pOurClosestGroup = *otherGroupIter;
                        }
                    }
                }
                
                for (PlotUnitDataMap::const_iterator targetPlotsIter(possibleTargetStacks.begin()), targetPlotsEndIter(possibleTargetStacks.end());
                    targetPlotsIter != targetPlotsEndIter; ++targetPlotsIter)
                {
                    // calculate this group's path to this target
                    UnitPathData unitPathData;
                    unitPathData.calculate(*groupIter, targetPlotsIter->first, MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY);
                    if (unitPathData.valid)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nDistance from group: " << (*groupIter)->getID() << " to hostile plot: " << targetPlotsIter->first->getCoords() << ": " << unitPathData.pathTurns;
#endif
                        thisGroupsTargets.push_back(unitPathData);
                        if (unitPathData.pathTurns < closestGroupTurnsToTarget)
                        {
                            moveData.pClosestGroupToHostiles = *groupIter;
                            moveData.closestHostileCoords = targetPlotsIter->first->getCoords();
                            closestGroupTurnsToTarget = unitPathData.pathTurns;
                        }
                    }
                }
                thisGroupsTargets.sort(UnitPathDataComp());
                groupPathData.push_front(std::make_pair(*groupIter, thisGroupsTargets));
                    
                if (*groupIter ==  pUnit->getGroup())
                {
                    // note our group's list positions
                    ourGroupIter = groupPathData.begin();
                }                    
#ifdef ALTAI_DEBUG
                os << "\nAdded group: " << (*groupIter)->getID() << " turns to closest target: " << thisGroupsTargets.rbegin()->pathTurns;
#endif
            }            
                
            if (moveData.pClosestGroupToHostiles && pUnit->getGroup() != moveData.pClosestGroupToHostiles && pUnitPlot == moveData.pClosestGroupToHostiles->plot())
            {
#ifdef ALTAI_DEBUG
                os << "\nUnit: " << pUnit->getIDInfo() << " will join group: " << moveData.pClosestGroupToHostiles->getID() << " if not attacking";
#endif
                moveData.joinGroupAtPlotIfNotAttacking = true;
            }

            XYCoords currentCoords = pUnit->plot()->getCoords();

            // closer to another mission group than nearest target
            // if thisGroupsTargets is empty, no path to any target
            if (pOurClosestGroup && (thisGroupsTargets.empty() || ourDistanceToClosestGroup < thisGroupsTargets.begin()->pathTurns))
            {
                UnitPathData unitPathData;
                unitPathData.calculate(pUnit->getGroup(), pOurClosestGroup->plot(), MOVE_MAX_MOVES | MOVE_IGNORE_DANGER);
                if (unitPathData.valid)
                {
                    // move towards the closer group
                    moveData.stepCoords = unitPathData.getFirstTurnEndCoords();
                    moveData.joinGroupAtPlotIfNotAttacking = true;
                }
            }

            if (moveData.stepCoords == currentCoords && !thisGroupsTargets.empty())
            {
                moveData.stepCoords = thisGroupsTargets.begin()->getFirstTurnEndCoords();
            }

            // still not found a move => pOurClosestGroup is NULL and thisGroupsTargets is empty
            if (moveData.stepCoords == currentCoords)
            {
#ifdef ALTAI_DEBUG
                os << "\nUnit: " << pUnit->getIDInfo() << " cannot find a move towards target or other mission group";
#endif
            }

            return moveData;
        }
    }

    MilitaryMissionDataPtr CoastalDefenceMission::createMission(int subArea)
    {
        MilitaryMissionDataPtr pMission(new MilitaryMissionData(MISSIONAI_COUNTER_COASTAL));
        pMission->pUnitsMission = IUnitsMissionPtr(new CoastalDefenceMission(subArea));
        return pMission;
    }

    CoastalDefenceMission::CoastalDefenceMission(int subArea) : subArea_(subArea)
    {
    }

    bool CoastalDefenceMission::doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission)
    {
        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner());
        MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();

#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
        const int currentTurn = gGlobals.getGame().getGameTurn();
#endif
        const CvPlot* pUnitPlot = pUnit->plot();
        // ourUnits = true, useMaxMoves = false, canAttack = true
        pMission->updateReachablePlots(player, true, false, true);
        pMission->pushedAttack = std::pair<IDInfo, XYCoords>();  // clear

        PlotUnitDataMap possibleTargetStacks;
        getPossibleTargets(player, pMission, possibleTargetStacks);

        // ourUnits = false, useMaxMoves = false, canAttack = true
        pMission->updateReachablePlots(player, false, false, true);
        pMission->updateRequiredUnits(player, std::set<IDInfo>());   // no reserve units passed here

        GroupMoveData groupMoveData = updateMissionGroupData(player, pMission, pUnit, possibleTargetStacks);

        bool finishedAttack = pUnit->isMadeAttack() && !pUnit->isBlitz();
        bool isAttackMove = !isEmpty(pMission->nextAttack.first);
        XYCoords moveToCoords = !isEmpty(pMission->nextAttack.second) ? pMission->nextAttack.second : XYCoords();
        if (isAttackMove)
        {
#ifdef ALTAI_DEBUG
            os << "\n attack plot set to: " << pMission->nextAttack.second;
            if (finishedAttack)
            {
                os << " error - unit has already attacked this turn";
            }
#endif
        }

        if (finishedAttack)
        {
            isAttackMove = false;
        }

        GroupCombatData combatData;
        // need this as attack and defence combat map only cover static defence and active attacks, not moving for defence
        combatData.calculate(player, pMission->getAssignedUnits(true), possibleTargetStacks);

        // attack not already set and we can move somewhere...
        if (!isAttackMove && !isEmpty(groupMoveData.stepCoords))
        {
            std::list<CombatMoveData> noAttackMoveData, attackMoveData;
            combineMoveData(player, pMission, pUnit, groupMoveData.stepCoords, combatData, noAttackMoveData, attackMoveData);

            if (!finishedAttack && !attackMoveData.empty())
            {
                attackMoveData.sort(AttackMoveDataPred(MilitaryAnalysis::attThreshold));
                const CombatMoveData& moveData = *attackMoveData.begin();

                boost::tie(isAttackMove, moveToCoords) = checkAttackOdds(moveData, attackMoveData.begin()->coords);

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
            }

            // not attacking
            if (!isAttackMove && isEmpty(moveToCoords))
            {
                // sort by distance and chance of successful hostile attack
                noAttackMoveData.sort(DefenceMoveDataPred(MilitaryAnalysis::hostileAttackThreshold));
                std::list<CombatMoveData>::const_iterator noAttackIter = noAttackMoveData.begin();

#ifdef ALTAI_DEBUG
                logNoAttackMoveCombatData(os, noAttackMoveData);
                os << "\n\t no attack move data first coords: " << noAttackMoveData.begin()->coords;
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

        if (groupMoveData.joinGroupAtPlotIfNotAttacking && (!isAttackMove || finishedAttack))
        {
            // need to rename this and make more general to grouping anywhere, not just close to target(s)
            pUnit->joinGroup(groupMoveData.pClosestGroupToHostiles);
            return true;
        }
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

        std::pair<IDInfo, XYCoords> attackData = pMission->nextAttack;
        pMission->nextAttack = std::make_pair(IDInfo(), XYCoords());  // clear next attacker so we don't loop

        if (!isEmpty(moveToCoords))
        {
            const CvPlot* pMovePlot = gGlobals.getMap().plot(moveToCoords.iX, moveToCoords.iY);

            if (pMovePlot != pUnitPlot && !(finishedAttack && !possibleTargetStacks[pMovePlot].empty()))
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
                    os << "\nMission: " << pMission << " turn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " pushed coastal counter mission attack move to: " << moveToCoords;
#endif
                    pMission->pushedAttack = attackData;  // store this as useful for updating combat data after combat is resolved
                    pUnit->getGroup()->pushMission(MISSION_MOVE_TO, moveToCoords.iX, moveToCoords.iY, MOVE_IGNORE_DANGER, false, false, MISSIONAI_COUNTER,
                        (CvPlot*)pMovePlot, 0, __FUNCTION__);
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

                        if (firstStep != pUnitPlot->getCoords())
                        {
#ifdef ALTAI_DEBUG
                            os << "\nMission: " << pMission << " turn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " pushed coastal counter mission move to: " << firstStep;
#endif
                            pUnit->getGroup()->pushMission(MISSION_MOVE_TO, firstStep.iX, firstStep.iY, MOVE_IGNORE_DANGER, false, false, MISSIONAI_COUNTER,
                                (CvPlot*)pMovePlot, 0, __FUNCTION__);
                            return true;
                        }
                    }
                }
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\nMission: " << pMission << " turn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " pushed coastal counter mission skip ";
#endif
                pUnit->getGroup()->pushMission(MISSION_SKIP, pUnitPlot->getX(), pUnitPlot->getY(), 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                return true;
            }
        }
        else
        {
#ifdef ALTAI_DEBUG
            os << "\nMission: " << pMission << " turn = " << currentTurn << " Unit: " << pUnit->getIDInfo() << " failed to determine move coords.";
#endif
        }

        return false;
    }

    void CoastalDefenceMission::update(Player& player)
    {
    }

    void CoastalDefenceMission::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(subArea_);
    }

    void CoastalDefenceMission::read(FDataStreamBase* pStream)
    {
        pStream->Read(&subArea_);
    }
}