#include "AltAI.h"

#include "./unit_mission_helper.h"
#include "./helper_fns.h"
#include "./iters.h"
#include "./unit_log.h"
#include "./civ_log.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./military_tactics.h"

namespace AltAI
{
    bool MissionFinder::operator() (const MilitaryMissionDataPtr& pMission) const
    {
        bool isMatch = true;
        if (missionType != NO_MISSIONAI)
        {
            isMatch = isMatch && pMission->missionType == missionType;
        }

        if (domainType != NO_DOMAIN)
        {
            for (std::set<IDInfo>::const_iterator ci(pMission->targetUnits.begin()), ciEnd(pMission->targetUnits.end()); ci != ciEnd; ++ci)
            {
                const CvUnit* pUnit = ::getUnit(*ci);
                if (pUnit)
                {
                    isMatch = isMatch && pUnit->getDomainType() == domainType;
                }
                if (!isMatch)
                {
                    break;
                }
            }
        }

        if (!isEmpty(city))
        {
            isMatch = isMatch && pMission->targetCity == city;
        }

        if (!isEmpty(targetUnit))
        {
            bool found = pMission->targetUnits.find(targetUnit) != pMission->targetUnits.end();
            if (!found && !pMission->specialTargets.empty())
            {
                found = pMission->specialTargets.find(targetUnit) != pMission->specialTargets.end();
            }
            isMatch = isMatch && found;
        }

        if (requestedUnitType != NO_UNIT)
        {
            std::vector<RequiredUnitStack::UnitDataChoices>::const_iterator reqUnitIter =
                std::find_if(pMission->requiredUnits.begin(), pMission->requiredUnits.end(), UnitTypeInList(requestedUnitType));
            isMatch = isMatch && reqUnitIter != pMission->requiredUnits.end();
        }

        if (!isEmpty(ourUnit))
        {
            isMatch = isMatch && pMission->assignedUnits.find(ourUnit) != pMission->assignedUnits.end();;
        }

        if (!isEmpty(targetCoords))
        {
            const CvPlot* pTargetPlot = pMission->getTargetPlot();
            isMatch = isMatch && pTargetPlot && pTargetPlot->getCoords() == targetCoords;
        }

        if (!isEmpty(proximityData.targetCoords))
        {
            const CvPlot* pTargetPlot = pMission->getTargetPlot();
            isMatch = isMatch && pTargetPlot && proximityData(pTargetPlot->getCoords());
        }

        if (!isEmpty(unitProximityData.targetCoords) && !pMission->assignedUnits.empty())
        {
            for (std::set<IDInfo>::const_iterator aIter(pMission->assignedUnits.begin()), aEndIter(pMission->assignedUnits.end()); aIter != aEndIter; ++aIter)
            {
                const CvUnit* pAssignedUnit = ::getUnit(*aIter);
                isMatch = isMatch && pAssignedUnit && unitProximityData(pAssignedUnit);
                if (isMatch)
                {
                    break;
                }
            }
        }

        return isMatch;
    }

    CombatGraph::Data getNearestCityAttackOdds(const Player& player, const MilitaryMissionDataPtr& pMission, const CvPlot* pTargetPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
        os << "\ncalling getNearestCityAttackOdds for mission: (" << pMission << ") ";
#endif
        IDInfo nearestCity;
        const CvPlot* pClosestCityPlot = player.getAnalysis()->getMapAnalysis()->getClosestCity(pTargetPlot, pTargetPlot->getSubArea(), false, nearestCity);
        if (pClosestCityPlot)
        {
            CombatData combatData(pClosestCityPlot);
            for (std::set<IDInfo>::const_iterator hIter(pMission->targetUnits.begin()), hEndIter(pMission->targetUnits.end());  hIter != hEndIter; ++hIter)
            {
                const CvUnit* pHostileUnit = ::getUnit(*hIter);
                if (pHostileUnit && !pHostileUnit->isAnimal() && pHostileUnit->canAttack())  // animals can't attack cities
                {
                    if (pHostileUnit->getDomainType() != DOMAIN_LAND)  // boats can't either - although of course they may have cargo
                    {
                        continue;
                    }
                    combatData.attackers.push_back(UnitData(pHostileUnit));
                    // an approximation if not adjacent - todo - use path logic to deduce likely attack from plot
                    // attack direction: we are at pClosestCityPlot - being attacked by pMission->targetUnits
                    combatData.combatDetails.unitAttackDirectionsMap[pHostileUnit->getIDInfo()] = directionXY(pHostileUnit->plot(), pClosestCityPlot);
                }
            }

            if (combatData.attackers.empty())
            {
                return CombatGraph::Data();
            }

            // take units from assigned city guard mission, as other units on the city plot may be passing through                
            MilitaryMissionDataPtr pCityGuardMission = player.getAnalysis()->getMilitaryAnalysis()->getCityDefenceMission(nearestCity);

            if (pCityGuardMission)  // may be in process of capturing city
            {
                for (std::set<IDInfo>::const_iterator uIter(pCityGuardMission->assignedUnits.begin()), uEndIter(pCityGuardMission->assignedUnits.end()); uIter != uEndIter; ++uIter)
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

            CombatGraph combatGraph = getCombatGraph(player, combatData.combatDetails, combatData.attackers, combatData.defenders);
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

    void getPromotionCombatValues(const Player& player, CvUnit* pUnit, const std::vector<PromotionTypes>& availablePromotions)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        MilitaryMissionDataPtr pMission = player.getAnalysis()->getMilitaryAnalysis()->getMissionData((CvUnitAI*)pUnit);

        if (pMission)
        {
            XYCoords coords = pUnit->plot()->getCoords();
            std::set<XYCoords> reachablePlots = pMission->getReachablePlots(pUnit->getIDInfo());

            for (std::set<XYCoords>::const_iterator rIter(reachablePlots.begin()), rEndIter(reachablePlots.end()); rIter != rEndIter; ++rIter)
            {
                const std::map<XYCoords, CombatData>& defenceCombatMap = player.getAnalysis()->getMilitaryAnalysis()->getDefenceCombatMap();
                std::map<XYCoords, CombatData>::const_iterator defCombatIter = defenceCombatMap.find(*rIter);
                if (defCombatIter != defenceCombatMap.end())
                {
                    std::map<PromotionTypes, float> promotionValues = getPromotionValues(player, pUnit, availablePromotions, defCombatIter->second.combatDetails, false, defCombatIter->second.attackers, defCombatIter->second.defenders);
                }

                const std::map<XYCoords, CombatData>& attackCombatMap = player.getAnalysis()->getMilitaryAnalysis()->getAttackCombatMap();
                std::map<XYCoords, CombatData>::const_iterator attCombatIter = attackCombatMap.find(*rIter);
                if (attCombatIter != attackCombatMap.end())
                {
                    std::map<PromotionTypes, float> promotionValues = getPromotionValues(player, pUnit, availablePromotions, attCombatIter->second.combatDetails, true, attCombatIter->second.attackers, attCombatIter->second.defenders);
                }
            }
        }
        // todo - process promotion values
    }

    void getOverlapAndNeighbourPlots(const PlotSet& theirPlots, const PlotSet& ourPlots, PlotSet& sharedPlots, PlotSet& neighbourPlots)
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

    std::pair<bool, XYCoords> checkAttackOdds(const CombatMoveData& moveData, XYCoords targetCoords)
    {
        XYCoords moveToCoords;
        bool isAttackMove = false;

        if (moveData.attackOdds.pWin > MilitaryAnalysis::attThreshold)
        {
            if (moveData.coords == targetCoords)
            {
                moveToCoords = moveData.coords;
                isAttackMove = true;
            }
        }

        return std::make_pair(isAttackMove, moveToCoords);
    }

    std::pair<bool, XYCoords> checkCityAttackOdds(const CombatMoveData& moveData, const std::map<IDInfo, CombatGraph::Data>& cityAttackOdds)
    {
        XYCoords moveToCoords;
        bool isAttackMove = false;

        for (std::map<IDInfo, CombatGraph::Data>::const_iterator aIter(cityAttackOdds.begin()), aEndIter(cityAttackOdds.end()); aIter != aEndIter; ++aIter)
        {
            // was hostileAttackThreshold - prob. want to be smarter here and distinguish case of hostile stack moving and static city we are attacking
            if (aIter->second.pWin > MilitaryAnalysis::defAttackThreshold)  // city threatened
            {
                if (moveData.attackOdds.pLoss < 1.0f - MilitaryAnalysis::attThreshold)
                {
                    moveToCoords = moveData.coords;
                    isAttackMove = true;  // ignores deferAnyAttack in this case
                }
            }
        }

        return std::make_pair(isAttackMove, moveToCoords);
    }

    void combineMoveData(const Player& player, MilitaryMissionData* pMission, CvUnit* pUnit, XYCoords stepCoords, 
        GroupCombatData& combatData, std::list<CombatMoveData>& noAttackMoveData, std::list<CombatMoveData>& attackMoveData)
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

            CombatGraph::Data thisPlotDefenceOdds = combatData.getCombatData(coords, false), thisPlotAttackOdds = combatData.getCombatData(coords, true);
            CombatMoveData moveData(coords, stepDistance(coords.iX, coords.iY, stepCoords.iX, stepCoords.iY), thisPlotDefenceOdds, thisPlotAttackOdds);

#ifdef ALTAI_DEBUG
            logMoveToCoordsCombatData(os, combatData, coords);
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

    void logMoveToCoordsCombatData(std::ostream& os, const GroupCombatData& combatData, XYCoords moveToCoords)
    {
#ifdef ALTAI_DEBUG
        CombatGraph::Data thisPlotDefenceOdds = combatData.getCombatData(moveToCoords, false), thisPlotAttackOdds = combatData.getCombatData(moveToCoords, true);

        os << "\nodds for plot: " << moveToCoords;
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
    }

    void logNoAttackMoveCombatData(std::ostream& os, const std::list<CombatMoveData>& noAttackMoveData)
    {
#ifdef ALTAI_DEBUG
        for (std::list<CombatMoveData>::const_iterator cmi(noAttackMoveData.begin()), cmiEnd(noAttackMoveData.end()); cmi != cmiEnd; ++cmi)
        {
            os << "\n\t" << cmi->coords << " sd = " << cmi->stepDistanceFromTarget;
            if (!cmi->defenceOdds.isEmpty())
            {
                os << " : ";
                cmi->defenceOdds.debug(os);
            }
        }
#endif
    }
}
