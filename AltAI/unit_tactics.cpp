#include "AltAI.h"

#include "./unit_tactics.h"
#include "./city_unit_tactics.h"
#include "./military_tactics.h"
#include "./unit_tactics_visitors.h"
#include "./unit_info_visitors.h"
#include "./tech_info_visitors.h"
#include "./tactic_streams.h"
#include "./game.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./gamedata_analysis.h"
#include "./settler_manager.h"
#include "./helper_fns.h"
#include "./unit_analysis.h"
#include "./tactic_selection_data.h"
#include "./civ_helper.h"
#include "./civ_log.h"
#include "./unit_log.h"
#include "./iters.h"
#include "./save_utils.h"

#include "FAStarNode.h"

namespace AltAI
{
    namespace
    {
        struct EscapePlotData
        {
            EscapePlotData(TeamTypes teamID, const CvPlot* pPlot_, const CvPlot* pTargetCityPlot, 
                const PlotUnitsMap& nearbyHostiles) : pPlot(pPlot_)
            {
                isCity = pPlot->isCity(false, teamID) && isFriendlyCity(teamID, pPlot->getPlotCity());
                isRevealed = pPlot->isRevealed(teamID, false);
                minSafePlotDistance = pTargetCityPlot ? stepDistance(pPlot->getX(), pPlot->getY(), pTargetCityPlot->getX(), pTargetCityPlot->getY()) : MAX_INT;

                minHostileDistance = MAX_INT;
                for (PlotUnitsMap::const_iterator hIter(nearbyHostiles.begin()), hEndIter(nearbyHostiles.end());
                    hIter != hEndIter; ++hIter)
                {
                    minHostileDistance = std::min<int>(minHostileDistance, stepDistance(pPlot->getX(), pPlot->getY(), hIter->first->getX(), hIter->first->getY()));
                }
            }

            bool operator < (const EscapePlotData& other) const
            {
                if (isCity == other.isCity)
                {
                    if (isRevealed == other.isRevealed)
                    {
                        if (minSafePlotDistance == other.minSafePlotDistance)
                        {
                            if (minHostileDistance == other.minHostileDistance)
                            {
                                return false;
                            }
                            else
                            {
                                return minHostileDistance > other.minHostileDistance;
                            }
                        }
                        else
                        {
                            return minSafePlotDistance < other.minSafePlotDistance;
                        }
                    }
                    else
                    {
                        return isRevealed;
                    }
                }
                else
                {
                    return isCity;
                }
            }

            void debug(std::ostream& os) const
            {
                os << " escape plot data: " << pPlot->getCoords() << " is city: " << isCity << " r = " << isRevealed
                    << ", h = " << minHostileDistance << ", s = " << minSafePlotDistance;
            }

            bool isCity, isRevealed;
            int minHostileDistance, minSafePlotDistance;
            const CvPlot* pPlot;
        };

        bool couldMoveIntoCommon(const CvUnit* pUnit, const CvPlot* pPlot, bool includeDeclareWarPlots)
        {
            if (pUnit->atPlot(pPlot))
            {
                return true;  // differs from logic in canMoveInto() - want to include any valid plot
            }

            if (pPlot->isImpassable())
            {
                if (!pUnit->canMoveImpassable())
                {
                    return false;
                }
            }

            const CvUnitInfo& unitInfo = pUnit->getUnitInfo();

            // Cannot move around in unrevealed land freely
            if (unitInfo.isNoRevealMap() && pUnit->willRevealByMove(pPlot))
            {
                return false;
            }

            if (pPlot->isWater() && !pUnit->canMoveAllTerrain())
            {            
                return false;
            }

            // skip spy check in orig fn - USE_SPIES_NO_ENTER_BORDERS is false in any case    

            const CvArea *pPlotArea = pPlot->area();
            PlayerTypes unitOwner = pUnit->getOwner();
            TeamTypes unitTeam = pUnit->getTeam();
            TeamTypes plotTeam = pPlot->getTeam();  // note not revealed team - but won't call this fn on unrevealed plots anyway
            bool bCanEnterArea = pUnit->canEnterArea(plotTeam, pPlotArea);  // great wall...
            if (bCanEnterArea)
            {
                // features trump terrain for impassable check
                if (pPlot->getFeatureType() != NO_FEATURE)
                {
                    if (unitInfo.getFeatureImpassable(pPlot->getFeatureType()))
                    {
                        TechTypes eTech = (TechTypes)unitInfo.getFeaturePassableTech(pPlot->getFeatureType());
                        if (NO_TECH == eTech || !GET_TEAM(unitTeam).isHasTech(eTech))
                        {                    
                            return false;
                        }
                    }
                }
                else
                {
                    if (unitInfo.getTerrainImpassable(pPlot->getTerrainType()))
                    {
                        TechTypes eTech = (TechTypes)unitInfo.getTerrainPassableTech(pPlot->getTerrainType());
                        if (NO_TECH == eTech || !GET_TEAM(unitTeam).isHasTech(eTech))
                        {   
                            return false;
                        }
                    }
                }
            }
            else
            {
                FAssert(plotTeam != NO_TEAM);

                if (!includeDeclareWarPlots || !(GET_TEAM(unitTeam).canDeclareWar(plotTeam)))
                {
                    return false;
                }
            }

            return true;
        }

        bool couldMoveInfoDomainSpecific(const CvUnit* pUnit, const CvPlot* pPlot)
        {
            static const bool bLAND_UNITS_CAN_ATTACK_WATER_CITIES = gGlobals.getDefineINT("LAND_UNITS_CAN_ATTACK_WATER_CITIES") != 0;

            switch (pUnit->getDomainType())
	        {
                case DOMAIN_LAND:
		            if (pPlot->isWater() && !pUnit->canMoveAllTerrain())
		            {
			            if (!pPlot->isCity() || !bLAND_UNITS_CAN_ATTACK_WATER_CITIES)
			            {
    			            return false;
			            }
		            }
		            break;

	            case DOMAIN_SEA:
                    if (!pPlot->isWater() && !pUnit->canMoveAllTerrain())
		            {
			            if (!pPlot->isFriendlyCity(*pUnit, true) || !pPlot->isCoastalLand()) 
			            {
				            return false;
			            }
		            }
		            break;
                default:
                    return false;
            }

            return true;
        }

        bool stackCanAttack(const std::vector<UnitData>& units)
        {
            for (size_t i = 0, count = units.size(); i < count; ++i)
            {
                if (units[i].baseCombat > 0 && !units[i].pUnitInfo->isOnlyDefensive() && units[i].moves > 0)
                {
                    if (!units[i].hasAttacked || units[i].isBlitz)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool canAttack(const CvUnit* pUnit, bool useMaxMoves, bool ignoreIfAlreadyAttacked)
        {
            return (useMaxMoves || pUnit->movesLeft() > 0) && (ignoreIfAlreadyAttacked || (!pUnit->isMadeAttack() || pUnit->isBlitz()));
        }

        void debugUnitMovementDataMap(std::ostream& os, const UnitMovementDataMap& unitMovementDataMap)
        {
            os << "\n";
            for (UnitMovementDataMap::const_iterator plotIter(unitMovementDataMap.begin()), plotEndIter(unitMovementDataMap.end()); plotIter != plotEndIter; ++plotIter)
            {
                os << "\n\tplot: " << plotIter->first->getCoords() << " units: ";
                for (std::map<const CvUnit*, int, CvUnitIDInfoOrderF>::const_iterator unitIter(plotIter->second.begin()), unitEndIter(plotIter->second.end()); unitIter != unitEndIter; ++unitIter)
                {
                    os << " " << unitIter->first->getIDInfo() << " = " << unitIter->second;
                }
            }
        }

        struct StackCombatDataComp
        {
            // return true if first unit's attack odds better than second's
            bool operator() (const StackCombatData& first, const StackCombatData& second) const
            {
                // attack odds = AttackerKillOdds + PullOutOdds + RetreatOdds = 1.0 - DefenderKillOdds
                // so return:  1.0 - first.odds.DefenderKillOdds > 1.0 - second.odds.DefenderKillOdds : which simplifies to: (ignoring equality case)
                return second.odds.DefenderKillOdds > first.odds.DefenderKillOdds;
            }
        };
    }

    int landMovementCost(const UnitMovementData& unit, const CvPlot* pFromPlot, const CvPlot* pToPlot, const CvTeamAI& unitsTeam)
    {            
        static const int MOVE_DENOMINATOR = gGlobals.getMOVE_DENOMINATOR();
        static const int HILLS_EXTRA_MOVEMENT = gGlobals.getHILLS_EXTRA_MOVEMENT();

        const TeamTypes fromPlotTeam = pFromPlot->getTeam(), toPlotTeam = pToPlot->getTeam();
        const RouteTypes fromRouteType = pFromPlot->getRouteType(), toRouteType = pToPlot->getRouteType();
            
        const bool fromPlotIsValidRoute = fromRouteType != NO_ROUTE && (fromPlotTeam == NO_TEAM || !unitsTeam.isAtWar(fromPlotTeam) || unit.isEnemyRoute),
            toPlotIsValidRoute = toRouteType != NO_ROUTE && (toPlotTeam == NO_TEAM || !unitsTeam.isAtWar(toPlotTeam) || unit.isEnemyRoute);

        int iRegularCost;
	    int iRouteCost;
	    int iRouteFlatCost;

        if (unit.ignoresTerrainCost)
	    {
		    iRegularCost = 1; 
	    }
	    else
	    {
		    iRegularCost = pToPlot->getFeatureType() == NO_FEATURE ? 
                gGlobals.getTerrainInfo(pToPlot->getTerrainType()).getMovementCost() : gGlobals.getFeatureInfo(pToPlot->getFeatureType()).getMovementCost();

		    if (pToPlot->isHills())
		    {
			    iRegularCost += HILLS_EXTRA_MOVEMENT;
		    }

		    if (iRegularCost > 0)
		    {
                iRegularCost = std::max<int>(1, (iRegularCost - unit.extraMovesDiscount));
		    }
	    }

	    const bool bHasTerrainCost = iRegularCost > 1;

        iRegularCost = std::min<int>(iRegularCost, unit.moves);

	    iRegularCost *= MOVE_DENOMINATOR;

	    if (bHasTerrainCost)
	    {
		    if (((pFromPlot->getFeatureType() == NO_FEATURE) ? 
                    unit.terrainDoubleMoves[pFromPlot->getTerrainType()] : unit.featureDoubleMoves[pFromPlot->getFeatureType()]) ||
                (pFromPlot->isHills() && unit.isHillsDoubleMoves))
		    {
			    iRegularCost /= 2;
		    }
	    }

	    if (fromPlotIsValidRoute && toPlotIsValidRoute && (unitsTeam.isBridgeBuilding() || !(pFromPlot->isRiverCrossing(directionXY(pFromPlot, pToPlot)))))
	    {
		    iRouteCost = std::max<int>((gGlobals.getRouteInfo(fromRouteType).getMovementCost() + unitsTeam.getRouteChange(fromRouteType)),
			                    (gGlobals.getRouteInfo(toRouteType).getMovementCost() + unitsTeam.getRouteChange(toRouteType)));
            iRouteFlatCost = std::max<int>((gGlobals.getRouteInfo(fromRouteType).getFlatMovementCost() * unit.moves),
			                        (gGlobals.getRouteInfo(toRouteType).getFlatMovementCost() * unit.moves));
	    }
	    else
	    {
		    iRouteCost = MAX_INT;
		    iRouteFlatCost = MAX_INT;
	    }

	    return std::max<int>(1, std::min<int>(iRegularCost, std::min<int>(iRouteCost, iRouteFlatCost)));
    }

    // movement cost for ships - optimised on the basis that we don't have routes or hills at sea
    int seaMovementCost(const UnitMovementData& unit, const CvPlot* pFromPlot, const CvPlot* pToPlot, const CvTeamAI& unitsTeam)
    {            
        static const int MOVE_DENOMINATOR = gGlobals.getMOVE_DENOMINATOR();

        const TeamTypes fromPlotTeam = pFromPlot->getTeam(), toPlotTeam = pToPlot->getTeam();
            
        int iRegularCost;

        if (unit.ignoresTerrainCost)
	    {
		    iRegularCost = 1; 
	    }
	    else
	    {
		    iRegularCost = pToPlot->getFeatureType() == NO_FEATURE ? 
                gGlobals.getTerrainInfo(pToPlot->getTerrainType()).getMovementCost() : gGlobals.getFeatureInfo(pToPlot->getFeatureType()).getMovementCost();

		    if (iRegularCost > 0)
		    {
                iRegularCost = std::max<int>(1, (iRegularCost - unit.extraMovesDiscount));
		    }
	    }

	    const bool bHasTerrainCost = iRegularCost > 1;

        iRegularCost = std::min<int>(iRegularCost, unit.moves);

	    iRegularCost *= MOVE_DENOMINATOR;

	    if (bHasTerrainCost)
	    {
		    if (((pFromPlot->getFeatureType() == NO_FEATURE) ? 
                    unit.terrainDoubleMoves[pFromPlot->getTerrainType()] : unit.featureDoubleMoves[pFromPlot->getFeatureType()]) ||
                (pFromPlot->isHills() && unit.isHillsDoubleMoves))
		    {
			    iRegularCost /= 2;
		    }
	    }

	    return std::max<int>(1, iRegularCost);
    }

    void ReachablePlotsData::debug(std::ostream& os) const
    {
        for (std::map<UnitMovementData, std::vector<const CvUnit*> >::const_iterator ugIter(unitGroupingMap.begin()), ugEndIter(unitGroupingMap.end()); ugIter != ugEndIter; ++ugIter)
        {
            os << "\n\t";
            ugIter->first.debug(os);
            debugUnitVector(os, ugIter->second);
        }

        debugUnitMovementDataMap(os, unitMovementDataMap);

        os << "\n\tall reachable plots: ";
        bool first = true;
        for (PlotSet::const_iterator ci(allReachablePlots.begin()), ciEnd(allReachablePlots.end()); ci != ciEnd; ++ci)
        {
            if (!first) os << ", "; else first = false;
            os << (*ci)->getCoords();
            
        }
    }

    bool AttackMoveDataPred::operator () (const CombatMoveData& first, const CombatMoveData& second) const
    {
        if (first.attackOdds.pWin > threshold)
        {
            if (first.stepDistanceFromTarget == second.stepDistanceFromTarget)
            {
                return first.attackOdds.pWin > second.attackOdds.pWin;
            }
            else
            {
                return first.stepDistanceFromTarget < second.stepDistanceFromTarget;
            }
        }
        else
        {
            if ((int)(1000.0f * first.attackOdds.pWin) == (int)(1000.0f * second.attackOdds.pWin))
            {
                return first.stepDistanceFromTarget < second.stepDistanceFromTarget;
            }
            else
            {
                return first.attackOdds.pWin > second.attackOdds.pWin;
            }
        }
    }

    bool DefenceMoveDataPred::operator () (const CombatMoveData& first, const CombatMoveData& second) const
    {
        if (first.defenceOdds.pWin < threshold)
        {
            if (first.stepDistanceFromTarget == second.stepDistanceFromTarget)
            {
                return first.defenceOdds.pWin < second.defenceOdds.pWin;
            }
            else
            {
                return first.stepDistanceFromTarget < second.stepDistanceFromTarget;
            }
        }
        else  // reverse order to consider prob survival first
        {
            if ((int)(1000.0f * first.defenceOdds.pWin) == (int)(1000.0f * second.defenceOdds.pWin))
            {
                return first.stepDistanceFromTarget < second.stepDistanceFromTarget;
            }
            else
            {
                return first.defenceOdds.pWin < second.defenceOdds.pWin;
            }
        }
    }

    void UnitPathData::calculate(const CvSelectionGroup* pGroup, const CvPlot* pTargetPlot, const int flags)
    {
        nodes.clear();
        valid = pGroup->generatePath(pGroup->plot(), pTargetPlot, flags, false, &pathTurns);
        if (valid)
        {            
            FAStarNode* pNode = pGroup->getPathLastNode();
            while (pNode)
            {
                XYCoords stepCoords(pNode->m_iX, pNode->m_iY);
                const CvPlot* thisStepPlot = gGlobals.getMap().plot(pNode->m_iX, pNode->m_iY);
                const bool plotIsVisible = thisStepPlot->isVisible(pGroup->getTeam(), false);
                // path plots are in reverse order (end->start)
                nodes.push_front(Node(pNode->m_iData2, pNode->m_iData1, stepCoords, plotIsVisible));
                pNode = pNode->m_pParent;
            }
        }
    }

    XYCoords UnitPathData::getLastVisiblePlotWithMP() const
    {
        XYCoords coords(-1, -1);
        if (nodes.size() > 1)
        {
            std::list<Node>::const_iterator nodeIter = nodes.begin();
            ++nodeIter;  // skip first node which is our starting point
            for (; nodeIter != nodes.end(); ++nodeIter)
            {
                if (nodeIter->movesLeft > 0 && nodeIter->visible)
                {
                    coords = nodeIter->coords;
                }
                else
                {
                    break;
                }
            }
        }
        return coords;
    }

    XYCoords UnitPathData::getFirstStepCoords() const
    {
        XYCoords coords(-1, -1);
        if (nodes.size() > 1)
        {
            std::list<Node>::const_iterator nodeIter = nodes.begin();
            ++nodeIter;
            coords = nodeIter->coords;
        }
        return coords;
    }

    XYCoords UnitPathData::getFirstTurnEndCoords() const
    {
        XYCoords coords(-1, -1);
        for (std::list<Node>::const_iterator nodeIter(nodes.begin()); nodeIter != nodes.end(); ++nodeIter)
        {            
            if (nodeIter->turn > 1)
            {
                break;
            }
            coords = nodeIter->coords;
        }
        return coords;
    }

    void UnitPathData::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        if (valid)
        {
            os << "\nunit path data: from: " << nodes.begin()->coords << " to: " << nodes.rbegin()->coords
               << " path turns = " << pathTurns;

            for (std::list<Node>::const_iterator nodeIter(nodes.begin()); nodeIter != nodes.end(); ++nodeIter)
            {
                os << "\n\t turn = " << nodeIter->turn << ", moves left = " << nodeIter->movesLeft << ", plot = " << nodeIter->coords << " v = " << nodeIter->visible;
            }
        }
        else
        {
            os << "\nunit path data is not valid";
        }
#endif
    }

    void CombatGraph::analyseEndStates()
    {
        endStatesData = Data(*this);

        for (std::list<StackCombatDataNodePtr>::const_iterator ci(endStates.begin()), ciEnd(endStates.end()); ci != ciEnd; ++ci)
        {
            float pEndState = 1;
            const StackCombatDataNode* pCurrentNode = (*ci).get();
                
            for (const StackCombatDataNode* pNode = (*ci)->parentNode; pNode; pNode = pNode->parentNode)
            {
                pEndState *= pCurrentNode == pNode->winNode.get() ? pNode->data.odds.AttackerKillOdds : // win
                    (pCurrentNode == pNode->drawNode.get() ? pNode->data.odds.PullOutOdds + pNode->data.odds.RetreatOdds : // draw
                        pNode->data.odds.DefenderKillOdds);  // loss
                pCurrentNode = pNode;
            }

            const bool noAttackers = (*ci)->attackers.empty(), noDefenders = (*ci)->defenders.empty();
            if (!noDefenders && !noAttackers)  // draw
            {
                endStatesData.pDraw += pEndState;
            }
            else if (noDefenders)  // win
            {
                endStatesData.pWin += pEndState;
            }
            else // loss
            {
                endStatesData.pLoss += pEndState;
            }

            int attackerIndex = 0;
            for (size_t i = 0, count = (*ci)->attackers.size(); i < count; ++i)
            {
                while (pRootNode->attackers[attackerIndex].unitId != (*ci)->attackers[i].unitId)
                {
                    ++attackerIndex;
                }
                endStatesData.attackerUnitOdds[attackerIndex++] += pEndState;
            }

            int defenderIndex = 0;
            for (size_t i = 0, count = (*ci)->defenders.size(); i < count; ++i)
            {
                while (pRootNode->defenders[defenderIndex].unitId != (*ci)->defenders[i].unitId)
                {
                    ++defenderIndex;
                }
                endStatesData.defenderUnitOdds[defenderIndex++] += pEndState;
            }
        }
    }

    void CombatGraph::debugEndStates(std::ostream& os) const
    {
        for (std::list<StackCombatDataNodePtr>::const_iterator ci(endStates.begin()), ciEnd(endStates.end()); ci != ciEnd; ++ci)
        {
            float pEndState = 1;
            const StackCombatDataNode* pCurrentNode = (*ci).get();
                
            for (const StackCombatDataNode* pNode = (*ci)->parentNode; pNode; pNode = pNode->parentNode)
            {
                pEndState *= pCurrentNode == pNode->winNode.get() ? pNode->data.odds.AttackerKillOdds : pNode->data.odds.DefenderKillOdds;
                pCurrentNode = pNode;
            }
            os << "\n\t P(endstate) = " << pEndState;

            const bool noAttackers = (*ci)->attackers.empty(), noDefenders = (*ci)->defenders.empty();
            if (!noDefenders && !noAttackers)  // draw
            {
                os << " (draw) ";
            }
            else if (noDefenders)  // win
            {
                os << " (win) ";
            }
            else // loss
            {
                os << " (loss) ";
            }
            os << " attackers:";
            for (size_t i = 0, count = (*ci)->attackers.size(); i < count; ++i)
            {
                os << ' ' << (*ci)->attackers[i].pUnitInfo->getType() << " hp=" << (*ci)->attackers[i].hp << " ";
                (*ci)->attackers[i].debugPromotions(os);
            }
            os << " defenders:";
            for (size_t i = 0, count = (*ci)->defenders.size(); i < count; ++i)
            {
                os << ' ' << (*ci)->defenders[i].pUnitInfo->getType() << " hp=" << (*ci)->defenders[i].hp << " ";
                (*ci)->defenders[i].debugPromotions(os);
            }
        }
    }
    
    void CombatGraph::Data::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << ": w = " << pWin << " d = " << pDraw << " l = " << pLoss;
        if (!attackerUnitOdds.empty()) os << " att odds: ";
        for (size_t i = 0, count = attackerUnitOdds.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << " (";
            os << attackers[i].pUnitInfo->getType();
            attackers[i].debugPromotions(os);
            os << " - " << attackerUnitOdds[i] << ")";                
        }
        if (!defenderUnitOdds.empty()) os << " def odds: ";
        for (size_t i = 0, count = defenderUnitOdds.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << " (";
            os << defenders[i].pUnitInfo->getType();
            defenders[i].debugPromotions(os);
            os << " - " << defenderUnitOdds[i] << ")";
        }
#endif
    }

    void CombatGraph::Data::write(FDataStreamBase* pStream) const
    {
        writeComplexVector(pStream, attackers);
        writeComplexVector(pStream, defenders);
        writeVector(pStream, attackerUnitOdds);
        writeVector(pStream, defenderUnitOdds);
        writeComplexList(pStream, longestAndShortestAttackOrder.first);
        writeComplexList(pStream, longestAndShortestAttackOrder.second);
        pStream->Write(pWin);
        pStream->Write(pLoss);
        pStream->Write(pDraw);
    }

    void CombatGraph::Data::read(FDataStreamBase* pStream)
    {
        readComplexVector(pStream, attackers);
        readComplexVector(pStream, defenders);
        readVector<float, float>(pStream, attackerUnitOdds);
        readVector<float, float>(pStream, defenderUnitOdds);
        readComplexList(pStream, longestAndShortestAttackOrder.first);
        readComplexList(pStream, longestAndShortestAttackOrder.second);
        /*pStream->Read(&pWin);
        pStream->Read(&pLoss);
        pStream->Read(&pDraw);*/
    }

    size_t CombatGraph::getFirstUnitIndex(bool isAttacker) const
    {
        return isAttacker ? pRootNode->data.attackerIndex : pRootNode->data.defenderIndex;
    }

    std::list<IDInfo> CombatGraph::getUnitOrdering(std::list<StackCombatDataNodePtr>::const_iterator endStateIter) const
    {
        std::list<IDInfo> attackUnits;
        for (const StackCombatDataNode* pNode = (*endStateIter)->parentNode; pNode; pNode = pNode->parentNode)
        {
            attackUnits.push_front(pNode->attackers[pNode->data.attackerIndex].unitId);
        }
        return attackUnits;
    }

    std::pair<std::list<IDInfo>, std::list<IDInfo> > CombatGraph::getLongestAndShortestAttackOrder() const
    {
        std::pair<std::list<IDInfo>, std::list<IDInfo> > attackUnitIndicesPair;

        // pick path with most losses
        for (StackCombatDataNodePtr pNode = pRootNode; pNode && !pNode->isEndState(); pNode = pNode->getWorstOutcome())
        {
            attackUnitIndicesPair.first.push_back(pNode->attackers[pNode->data.attackerIndex].unitId);
        }

        // pick path with most wins
        for (StackCombatDataNodePtr pNode = pRootNode; pNode && !pNode->isEndState(); pNode = pNode->getBestOutcome())
        {
            attackUnitIndicesPair.second.push_back(pNode->attackers[pNode->data.attackerIndex].unitId);
        }

        return attackUnitIndicesPair;
    }

    float CombatGraph::getSurvivalOdds(IDInfo unitId, bool isAttacker) const
    {
        float odds = 0;
        for (std::list<StackCombatDataNodePtr>::const_iterator ci(endStates.begin()), ciEnd(endStates.end()); ci != ciEnd; ++ci)
        {
            for (size_t i = 0, count = isAttacker ? (*ci)->attackers.size() : (*ci)->defenders.size(); i < count; ++i)
            {
                if (isAttacker ? (*ci)->attackers[i].unitId == unitId : (*ci)->defenders[i].unitId == unitId)
                {                        
                    float pEndState = 1;
                    const StackCombatDataNode* pCurrentNode = (*ci).get();
                    for (const StackCombatDataNode* pNode = (*ci)->parentNode; pNode; pNode = pNode->parentNode)
                    {
                        // todo - check this logic for when we are defenders and if we want to consider survival odds of hostiles as well as ourselves
                        pEndState *= pCurrentNode == pNode->winNode.get() ? pNode->data.odds.AttackerKillOdds : // win
                            (pCurrentNode == pNode->drawNode.get() ? pNode->data.odds.PullOutOdds + pNode->data.odds.RetreatOdds : // draw
                                pNode->data.odds.DefenderKillOdds);  // loss

                        pCurrentNode = pNode;
                    }
                    odds += pEndState;
                }
            }
        }
        return odds;
    }

    CombatGraph getCombatGraph(const Player& player, const UnitData::CombatDetails& combatDetails, 
        const std::vector<UnitData>& attackers, const std::vector<UnitData>& defenders, double oddsThreshold)
    {   
        std::list<StackCombatDataNodePtr> openNodes, endNodes;

        StackCombatDataNodePtr pRootNode(new StackCombatDataNode());
        pRootNode->attackers = attackers;
        pRootNode->defenders = defenders;

        openNodes.push_front(pRootNode);

        while (!openNodes.empty())
        {
            StackCombatDataNodePtr pCurrentNode = *openNodes.begin();
            // expand node if required
            if (!pCurrentNode->attackers.empty() && !pCurrentNode->defenders.empty() && stackCanAttack(pCurrentNode->attackers))
            {
                pCurrentNode->data = getBestUnitOdds(player, combatDetails, pCurrentNode->attackers, pCurrentNode->defenders);

                // oddsThreshold limits expansion of very unlikely paths - which significantly reduces the no. of nodes for large stack combinations
                // better than 99.99% (0.9999) (if oddsThreshold is 0.0001) chance of attacker dying
                bool ignoreAttackerSurvival = pCurrentNode->data.odds.DefenderKillOdds > 1.0 - oddsThreshold;
                // better than 99.99% chance of defender dying
                bool ignoreDefenderSurvival = pCurrentNode->data.odds.AttackerKillOdds > 1.0 - oddsThreshold;

                if (!ignoreAttackerSurvival)
                {
                    StackCombatDataNodePtr pWinNode(new StackCombatDataNode(pCurrentNode.get()));
                    pWinNode->attackers = pCurrentNode->attackers;
                    UnitData& attackingUnitData = pWinNode->attackers[pCurrentNode->data.attackerIndex];
                    attackingUnitData.hp = std::max<int>(1, (int)pCurrentNode->data.odds.E_HP_Att);  // prevent div by zero errors from rounding down hp to zero
                    attackingUnitData.hasAttacked = true;
                    attackingUnitData.moves = std::max<int>(0, attackingUnitData.moves - 1); // todo calc exact movement cost for attack to plot
                    for (size_t i = 0, count = pCurrentNode->defenders.size(); i < count; ++i)
                    {
                        if (i != pCurrentNode->data.defenderIndex)
                        {
                            pWinNode->defenders.push_back(pCurrentNode->defenders[i]);
                        }
                    }
                    pCurrentNode->winNode = pWinNode;
                    openNodes.push_back(pWinNode);  // attacker wins (or survives?)
                }
                else
                {
                    pCurrentNode->data.odds.AttackerKillOdds = 0.0;
                    pCurrentNode->data.odds.DefenderKillOdds = 1.0;
                }

                if (!ignoreDefenderSurvival)
                {
                    StackCombatDataNodePtr pLossNode(new StackCombatDataNode(pCurrentNode.get()));
                    pLossNode->defenders = pCurrentNode->defenders;
                    UnitData& defendingUnitData = pLossNode->defenders[pCurrentNode->data.defenderIndex];
                    defendingUnitData.hp = std::max<int>(1, (int)pCurrentNode->data.odds.E_HP_Def);
                    for (size_t i = 0, count = pCurrentNode->attackers.size(); i < count; ++i)
                    {
                        if (i != pCurrentNode->data.attackerIndex)
                        {
                            pLossNode->attackers.push_back(pCurrentNode->attackers[i]);
                        }
                    }
                    pCurrentNode->lossNode = pLossNode;
                    openNodes.push_back(pLossNode);  // defender wins
                }
                else
                {
                    pCurrentNode->data.odds.AttackerKillOdds = 1.0;
                    pCurrentNode->data.odds.DefenderKillOdds = 0.0;
                }

                // withdrawal (for units that have a damage max % limit) and retreat (units with chance to withdraw from combat)
                // in both cases, neither defender nor attacker is killed
                if (pCurrentNode->data.odds.PullOutOdds + pCurrentNode->data.odds.RetreatOdds > oddsThreshold)
                {
                    StackCombatDataNodePtr pDrawNode(new StackCombatDataNode(pCurrentNode.get()));
                    pDrawNode->attackers = pCurrentNode->attackers;
                    pDrawNode->defenders = pCurrentNode->defenders;
                    UnitData& attackingUnitData = pDrawNode->attackers[pCurrentNode->data.attackerIndex];
                    // average of E_HP_Att_Withdraw and E_HP_Att_Retreat weighted by prob
                    // todo - check the expected HP values for retreat and withdrawal as they seem to be always 0 even when retreat odds are non trivial
                    attackingUnitData.hp = std::max<int>(1, (int)((pCurrentNode->data.odds.PullOutOdds * pCurrentNode->data.odds.E_HP_Att_Withdraw + 
                        pCurrentNode->data.odds.RetreatOdds * pCurrentNode->data.odds.E_HP_Att_Retreat) / (pCurrentNode->data.odds.PullOutOdds + pCurrentNode->data.odds.RetreatOdds)));
                    UnitData& defendingUnitData = pDrawNode->defenders[pCurrentNode->data.defenderIndex];
                    defendingUnitData.hp = std::max<int>(1, (int)pCurrentNode->data.odds.E_HP_Def_Withdraw);  // + average in expected retreat (hp = max damage limit of attacker?)
                    attackingUnitData.hasAttacked = true;
                    attackingUnitData.moves = std::max<int>(0, attackingUnitData.moves - 1); // todo calc exact movement cost for attack to plot
                    pCurrentNode->drawNode = pDrawNode;
                    openNodes.push_back(pDrawNode);  // attacker retreats or withdraws
                }
            }
            else
            {
                endNodes.push_back(pCurrentNode);
            }
            openNodes.erase(openNodes.begin());
        }

        CombatGraph combatGraph;
        combatGraph.pRootNode = pRootNode;
        combatGraph.endStates = endNodes;
        return combatGraph;
    };

    std::map<PromotionTypes, float> getPromotionValues(const Player& player, const CvUnit* pUnit, const std::vector<PromotionTypes>& availablePromotions,
        const UnitData::CombatDetails& combatDetails, bool isAttacker,
        const std::vector<UnitData>& attackers, const std::vector<UnitData>& defenders, double oddsThreshold)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        std::map<PromotionTypes, float> promotionOddsMap;
        const std::vector<UnitData>& ourUnits(isAttacker ? attackers : defenders), &theirUnits(isAttacker ? defenders : attackers);
        std::vector<UnitData>::const_iterator unitDataIter = std::find_if(ourUnits.begin(), ourUnits.end(), UnitDataIDInfoP(pUnit->getIDInfo()));
        if (unitDataIter != ourUnits.end())
        {
            size_t unitIndex = std::distance(ourUnits.begin(), unitDataIter);
            CombatGraph combatGraph = getCombatGraph(player, combatDetails, attackers, defenders, oddsThreshold);
            combatGraph.analyseEndStates();
            float baseSurvivalOdds = combatGraph.getSurvivalOdds(pUnit->getIDInfo(), isAttacker);
            promotionOddsMap[NO_PROMOTION] = baseSurvivalOdds;
#ifdef ALTAI_DEBUG
            os << "\nUnit: " << pUnit->getIDInfo() << " base survival odds = " << baseSurvivalOdds;
#endif
            std::vector<UnitData> ourUnitsCopy(ourUnits);
            for (size_t i = 0, count = availablePromotions.size(); i < count; ++i)
            {
                UnitData ourUnit(pUnit);
                ourUnit.applyPromotion(availablePromotions[i]);
                ourUnitsCopy[unitIndex] = ourUnit;

                CombatGraph combatGraph = getCombatGraph(player, combatDetails, isAttacker ? ourUnitsCopy : theirUnits, isAttacker ? theirUnits : ourUnitsCopy, oddsThreshold);
                combatGraph.analyseEndStates();
                float promotionSurvivalOdds = combatGraph.getSurvivalOdds(pUnit->getIDInfo(), isAttacker);
                promotionOddsMap[availablePromotions[i]] = promotionSurvivalOdds;
#ifdef ALTAI_DEBUG
                os << "\n\t odds with promotion: " << gGlobals.getPromotionInfo(availablePromotions[i]).getType() << " = " << promotionSurvivalOdds;
#endif
            }
        }

        return promotionOddsMap;
    }

    RequiredUnitStack getRequiredUnits(const Player& player, const CvPlot* pTargetPlot, const std::vector<const CvUnit*>& enemyStack, const std::set<IDInfo>& availableUnits)
    {
        const bool isWater = pTargetPlot->isWater();
        std::vector<UnitTypes> landCombatUnits, seaCombatUnits, possibleLandCombatUnits, possibleSeaCombatUnits;
        if (isWater)
        {
            boost::tie(seaCombatUnits, possibleSeaCombatUnits) = getActualAndPossibleCombatUnits(player, NULL, DOMAIN_SEA);                
        }
        else
        {
            boost::tie(landCombatUnits, possibleLandCombatUnits) = getActualAndPossibleCombatUnits(player, NULL, DOMAIN_LAND);
        }

        return getRequiredAttackStack(player, isWater ? seaCombatUnits : landCombatUnits, pTargetPlot, enemyStack, availableUnits);
    }

    UnitMovementData::UnitMovementData(const CvUnit* pUnit)
    {
        moves = pUnit->baseMoves();
        extraMovesDiscount = pUnit->getExtraMoveDiscount();
        for (int i = 0, infoCount = gGlobals.getNumFeatureInfos(); i < infoCount; ++i)
        {
            featureDoubleMoves.push_back(pUnit->isFeatureDoubleMove((FeatureTypes)i));
        }
        for (int i = 0, infoCount = gGlobals.getNumTerrainInfos(); i < infoCount; ++i)
        {
            terrainDoubleMoves.push_back(pUnit->isTerrainDoubleMove((TerrainTypes)i));
        }
        isHillsDoubleMoves = pUnit->getHillsDoubleMoveCount();
        ignoresTerrainCost = pUnit->ignoreTerrainCost();
        isEnemyRoute = pUnit->isEnemyRoute();
        ignoresImpassable = pUnit->canMoveImpassable();
        canAttack = pUnit->canAttack() && (!pUnit->isMadeAttack() || pUnit->isBlitz());
    }

    UnitMovementData::UnitMovementData(const UnitData& unitData) :
        featureDoubleMoves(unitData.featureDoubleMoves), terrainDoubleMoves(unitData.terrainDoubleMoves)
    {
        moves = unitData.pUnitInfo->getMoves();
        extraMovesDiscount = 0;  // todo: driven by promotions if we include those with basic UnitData info
        // same for featureDoubleMoves and terrainDoubleMoves and isHillsDoubleMoves and isEnemyRoute
        ignoresTerrainCost = unitData.pUnitInfo->isIgnoreTerrainCost();
        ignoresImpassable = unitData.pUnitInfo->isCanMoveImpassable();
        canAttack = unitData.baseCombat > 0 && (!unitData.hasAttacked || unitData.isBlitz);
    }

    // allows unique differentiation of any combination of unit movement attributes
    // useful to efficiently calculate possible moves for a large stack without checking all its units
    bool UnitMovementData::operator < (const UnitMovementData& other) const
    {
        if (moves != other.moves)
        {
            return moves < other.moves;
        }

        if (extraMovesDiscount != other.extraMovesDiscount)
        {
            return extraMovesDiscount < other.extraMovesDiscount;
        }

        if (isHillsDoubleMoves != other.isHillsDoubleMoves)
        {
            return isHillsDoubleMoves < other.isHillsDoubleMoves;
        }

        if (ignoresTerrainCost != other.ignoresTerrainCost)
        {
            return ignoresTerrainCost < other.ignoresTerrainCost;
        }

        if (isEnemyRoute != other.isEnemyRoute)
        {
            return isEnemyRoute < other.isEnemyRoute;
        }
                
        if (ignoresImpassable != other.ignoresImpassable)
        {
            return ignoresImpassable < other.ignoresImpassable;
        }

        if (canAttack != other.canAttack)
        {
            return canAttack < other.canAttack;
        }

        for (int i = 0, infoCount = gGlobals.getNumFeatureInfos(); i < infoCount; ++i)
        {
            if (featureDoubleMoves[i] != other.featureDoubleMoves[i])
            {
                return featureDoubleMoves[i] < other.featureDoubleMoves[i];
            }
        }

        for (int i = 0, infoCount = gGlobals.getNumTerrainInfos(); i < infoCount; ++i)
        {
            if (terrainDoubleMoves[i] != other.terrainDoubleMoves[i])
            {
                return terrainDoubleMoves[i] < other.terrainDoubleMoves[i];
            }
        }                                

        return false;
    }
    
    void UnitMovementData::debug(std::ostream& os) const
    {        
        os << " moves = " << moves;
        if (extraMovesDiscount != 0) os << " extraMovesDiscount = " << extraMovesDiscount;
        if (isHillsDoubleMoves) os << " double movement on hills ";
        if (ignoresTerrainCost) os << " ignores terrain movement cost ";
        if (isEnemyRoute) os << " can use enemy roads ";
        if (ignoresImpassable) os << " ignores impassable terrain flag ";
        if (!canAttack) os << " already attacked ";
        for (int i = 0, infoCount = gGlobals.getNumFeatureInfos(); i < infoCount; ++i)
        {
            if (featureDoubleMoves[i]) os << " double moves for feature: " << gGlobals.getFeatureInfo((FeatureTypes)i).getType();
        }
        for (int i = 0, infoCount = gGlobals.getNumTerrainInfos(); i < infoCount; ++i)
        {
            if (terrainDoubleMoves[i]) os << " double moves for terrain: " << gGlobals.getTerrainInfo((TerrainTypes)i).getType();
        }
    }

    void UnitValueHelper::debug(const UnitValueHelper::MapT& unitCombatData, std::ostream& os) const
    {
        for (MapT::const_iterator ci(unitCombatData.begin()), ciEnd(unitCombatData.end()); ci != ciEnd; ++ci)
        {
            os << "\nUnit: " << gGlobals.getUnitInfo(ci->first).getType() << " (cost = " << ci->second.first << ")";
            for (size_t i = 0, count = ci->second.second.size(); i < count; ++i)
            {
                os << "\n\t" << gGlobals.getUnitInfo(ci->second.second[i].first).getType() << " odds = " << ci->second.second[i].second;
            }
            os << "\n\t\tvalue = " << getValue(ci->second);
        }
    }

    void UnitValueHelper::addMapEntry(MapT& unitCombatData, UnitTypes unitType, const std::vector<UnitTypes>& possibleCombatUnits, const std::vector<int>& odds) const
    {
        unitCombatData[unitType].first = gGlobals.getUnitInfo(unitType).getProductionCost();

        for (size_t j = 0, oddsCounter = odds.size(); j < oddsCounter; ++j)
        {
            if (odds[j] > 0)
            {
                unitCombatData[unitType].second.push_back(std::make_pair(possibleCombatUnits[j], odds[j]));
            }
        }
    }

    int UnitValueHelper::getValue(const std::pair<int, std::vector<std::pair<UnitTypes, int> > >& mapEntry) const
    {
        const int maxTries = 1;
        const double oddsThreshold = 0.65;
        //const int cost = mapEntry.first;

        double value = 0;

        for (size_t i = 0, count = mapEntry.second.size(); i < count; ++i)
        {
            //int thisCost = cost;
            const double thisOdds = (double)(mapEntry.second[i].second) * 0.001;
            double odds = thisOdds;
            int nTries = 1;

            while (odds < oddsThreshold && nTries++ < maxTries)
            {
                odds += (1.0 - odds) * thisOdds;
                //thisCost += cost;
            }

            if (odds > oddsThreshold)
            {
                value += 1000.0 * odds; // / (double)thisCost;
            }
        }

        return (int)value;
    }

    std::pair<std::vector<UnitTypes>, std::vector<UnitTypes> > getActualAndPossibleCombatUnits(const Player& player, const CvCity* pCity, DomainTypes domainType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        std::vector<UnitTypes> combatUnits, possibleCombatUnits;

        boost::shared_ptr<PlayerTactics> pTactics = player.getAnalysis()->getPlayerTactics();

        for (PlayerTactics::UnitTacticsMap::const_iterator ci(pTactics->unitTacticsMap_.begin()), ciEnd(pTactics->unitTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            if (ci->first != NO_UNIT)
            {
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(ci->first);
                if (unitInfo.getDomainType() == domainType && unitInfo.getProductionCost() >= 0 && unitInfo.getCombat() > 0)
                {
                    if (ci->second && !isUnitObsolete(player, player.getAnalysis()->getUnitInfo(ci->first)))
                    {
#ifdef ALTAI_DEBUG
                        if (domainType != DOMAIN_LAND)
                        {
                            os << "\nChecking unit: " << gGlobals.getUnitInfo(ci->first).getType();
                        }
#endif
                        const boost::shared_ptr<UnitInfo> pUnitInfo = player.getAnalysis()->getUnitInfo(ci->first);
                        bool depsSatisfied = ci->second->areDependenciesSatisfied(player, IDependentTactic::Ignore_None);
                        bool techDepsSatisfied = ci->second->areTechDependenciesSatisfied(player);

                        if (pCity)
                        {
                            CityUnitTacticsPtr pCityTactics = ci->second->getCityTactics(pCity->getIDInfo());
                            depsSatisfied = pCityTactics && depsSatisfied && pCityTactics->areDependenciesSatisfied(IDependentTactic::Ignore_None);
                        }

                        bool couldConstruct = (pCity ? couldConstructUnit(player, player.getCity(pCity->getID()), 0, pUnitInfo, false, false) : couldConstructUnit(player, 0, pUnitInfo, false, false));
#ifdef ALTAI_DEBUG
                        if (domainType != DOMAIN_LAND)
                        {
                            os << " " << (pCity ? narrow(pCity->getName()) : "(none)");
                            os << " deps = " << depsSatisfied << ", could construct = " << couldConstruct << " tech deps: " << techDepsSatisfied
                               << " potential construct for player: " << couldConstructUnit(player, 3, player.getAnalysis()->getUnitInfo(ci->first), true, true);
                        }
#endif
                        if (depsSatisfied)
                        {
                            combatUnits.push_back(ci->first);
                            possibleCombatUnits.push_back(ci->first);
                        }
                        else if (couldConstructUnit(player, 3, player.getAnalysis()->getUnitInfo(ci->first), true, true))
                        {
                            possibleCombatUnits.push_back(ci->first);
                        }
                    }
                }
            }
        }

        return std::make_pair(combatUnits, possibleCombatUnits);
    }

    StackCombatData getBestUnitOdds(const Player& player, const UnitData::CombatDetails& combatDetails, const std::vector<UnitData>& attackers, const std::vector<UnitData>& defenders, bool debug)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
#endif
        const size_t attackUnitsCount = attackers.size();
        std::vector<size_t> defenderIndex(attackUnitsCount);  // index of defending unit each of attacking unit would face
        std::vector<int> attackerOdds(attackUnitsCount);  // odds defending unit has in each of these theoretical battles
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis = player.getAnalysis()->getUnitAnalysis();

        for (size_t i = 0, count = attackUnitsCount; i < count; ++i)
        {
            std::vector<int> odds = pUnitAnalysis->getOdds(attackers[i], defenders, combatDetails, true);
                
            // find best defender v. attacking unit
            std::vector<int>::const_iterator oddsIter = std::min_element(odds.begin(), odds.end());  // odds are for attacking unit - so find minimum value
            attackerOdds[i] = *oddsIter;
            // find defending unit index for unit attacker would fight
            defenderIndex[i] = std::distance<std::vector<int>::const_iterator>(odds.begin(), oddsIter);

#ifdef ALTAI_DEBUG
            if (debug)
            {
                os << "\n " << attackers[i].pUnitInfo->getType();
                attackers[i].debugPromotions(os);
                for (size_t j = 0; j < odds.size(); ++j)
                {
                    os << " v. " << defenders[j].pUnitInfo->getType() << " = " << odds[j];
                    defenders[j].debugPromotions(os);
                }
                os << " best def = " << defenders[defenderIndex[i]].pUnitInfo->getType();
                if (defenders[defenderIndex[i]].hp < 100)
                {
                    os << " def hp = " << defenders[defenderIndex[i]].hp;
                }
            }
#endif
            if (attackers[i].pUnitInfo->getCollateralDamage() > 0)
            {
                std::vector<int> unitsCollateralDamage = pUnitAnalysis->getCollateralDamage(attackers[i], defenders, defenderIndex[i], combatDetails);
#ifdef ALTAI_DEBUG
                if (debug)
                {
                    for (size_t j = 0; j < unitsCollateralDamage.size(); ++j)
                    {
                        os << " unit = " << defenders[j].pUnitInfo->getType() << " c.damage = " << unitsCollateralDamage[j];
                    }
                }
#endif
            }
        }

        // find attacker with best odds
        std::vector<int>::const_iterator oddsIter = std::max_element(attackerOdds.begin(), attackerOdds.end());
        size_t attackerIndex = std::distance<std::vector<int>::const_iterator>(attackerOdds.begin(), oddsIter);

        StackCombatData data;
        UnitOddsData oddsDetail = pUnitAnalysis->getCombatOddsDetail(attackers[attackerIndex], defenders[defenderIndex[attackerIndex]], combatDetails);

#ifdef ALTAI_DEBUG
        if (debug)
        {
            os << "\n\tattacker odds = " << *oddsIter << " best att = " << attackers[attackerIndex].pUnitInfo->getType() << " best def = " << defenders[defenderIndex[attackerIndex]].pUnitInfo->getType();
            oddsDetail.debug(os);
        }
#endif      
        data.attackerIndex = attackerIndex;
        data.defenderIndex = defenderIndex[attackerIndex];
        data.odds = oddsDetail;

        return data;
    }

    std::list<StackCombatData> getBestUnitOdds(const Player& player, const UnitData::CombatDetails& combatDetails, 
        const std::vector<UnitData>& attackers, const std::vector<UnitData>& defenders, const int oddsThreshold, bool debug)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
#endif
        const size_t attackUnitsCount = attackers.size();
        std::vector<size_t> defenderIndex(attackUnitsCount);  // index of defending unit each of attacking unit would face
        std::vector<int> attackerOdds(attackUnitsCount);  // odds defending unit has in each of these theoretical battles
        int bestOddsIndex = -1;
        int bestOdds = -1;

        for (size_t i = 0, count = attackUnitsCount; i < count; ++i)
        {
            std::vector<int> odds = player.getAnalysis()->getUnitAnalysis()->getOdds(attackers[i], defenders, combatDetails, true);
                
            // find best defender v. attacking unit
            std::vector<int>::const_iterator oddsIter = std::min_element(odds.begin(), odds.end());  // odds are for attacking unit - so find minimum value
            attackerOdds[i] = *oddsIter;
            // find defending unit index for unit attacker would fight
            defenderIndex[i] = std::distance<std::vector<int>::const_iterator>(odds.begin(), oddsIter);

            if (attackerOdds[i] > bestOdds)
            {
                bestOddsIndex = i;
                bestOdds = attackerOdds[i];
            }
#ifdef ALTAI_DEBUG
            if (debug)
            {
                os << " " << attackers[i].pUnitInfo->getType();
                attackers[i].debugPromotions(os);
                for (size_t j = 0; j < odds.size(); ++j)
                {
                    os << " v. " << defenders[j].pUnitInfo->getType() << " = " << odds[j];
                    defenders[j].debugPromotions(os);
                }
                os << " best def = " << defenders[defenderIndex[i]].pUnitInfo->getType();
                if (defenders[defenderIndex[i]].hp < 100)
                {
                    os << " def hp = " << defenders[defenderIndex[i]].hp;
                }
                os << "\n";
            }
#endif
            if (attackers[i].pUnitInfo->getCollateralDamage() > 0)
            {
                std::vector<int> unitsCollateralDamage = player.getAnalysis()->getUnitAnalysis()->getCollateralDamage(attackers[i], defenders, defenderIndex[i], combatDetails);
#ifdef ALTAI_DEBUG
                if (debug)
                {
                    for (size_t j = 0; j < unitsCollateralDamage.size(); ++j)
                    {
                        os << " unit = " << defenders[j].pUnitInfo->getType() << " c.damage = " << unitsCollateralDamage[j];
                    }
                }
#endif
            }
        }

        std::list<StackCombatData> combatsData;

        if (bestOdds > 0 && bestOdds < oddsThreshold)
        {
            StackCombatData data;
            UnitOddsData oddsDetail = player.getAnalysis()->getUnitAnalysis()->getCombatOddsDetail(attackers[bestOddsIndex], defenders[defenderIndex[bestOddsIndex]], combatDetails);
#ifdef ALTAI_DEBUG
            if (debug)
            {
                os << "\n\tbest attacker odds under threshold (" << oddsThreshold << ") " << attackerOdds[bestOddsIndex] << " a = "
                    << attackers[bestOddsIndex].pUnitInfo->getType() << " d = " << defenders[defenderIndex[bestOddsIndex]].pUnitInfo->getType();
                oddsDetail.debug(os);
            }
#endif
            data.attackerIndex = bestOddsIndex;
            data.defenderIndex = defenderIndex[bestOddsIndex];
            data.odds = oddsDetail;
            combatsData.push_back(data);
        }
        else
        {
            const int minThreshold = bestOdds * 4 / 5;
            // find attackers with odds greater than threshold
            for (size_t i = 0; i < attackUnitsCount; ++i)
            {
                if (attackerOdds[i] >= oddsThreshold && attackerOdds[i] > minThreshold) // (odds are 0 -> 1000) odds at least threshold and at least 80% of best odds 
                {
                    StackCombatData data;
                    UnitOddsData oddsDetail = player.getAnalysis()->getUnitAnalysis()->getCombatOddsDetail(attackers[i], defenders[defenderIndex[i]], combatDetails);
#ifdef ALTAI_DEBUG
                    if (debug)
                    {
                        os << "\n\tbest attacker odds over threshold (" << oddsThreshold << ") " << attackerOdds[i] << " a = "
                            << attackers[i].pUnitInfo->getType() << " d = " << defenders[defenderIndex[i]].pUnitInfo->getType();
                        oddsDetail.debug(os);
                    }
#endif
                    data.attackerIndex = i;
                    data.defenderIndex = defenderIndex[i];
                    data.odds = oddsDetail;
                    combatsData.push_back(data);
                }
            }
        }

        return combatsData;
    }

    RequiredUnitStack getRequiredAttackStack(const Player& player, const std::vector<UnitTypes>& availableUnits, const CvPlot* pTargetPlot, 
            const std::vector<const CvUnit*>& enemyStack, const std::set<IDInfo>& existingUnits)
    {
        RequiredUnitStack requiredUnitStack;

#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
            os << "\ngetRequiredAttackStack: buildable units: ";
            for (size_t i = 0, count = availableUnits.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << gGlobals.getUnitInfo(availableUnits[i]).getType();
            }
            os << "\n\tavailable units: ";
            debugUnitSet(os, existingUnits);
            os << "\n\tenemy units: ";
            debugUnitVector(os, enemyStack);
#endif
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis = player.getAnalysis()->getUnitAnalysis();

        UnitData::CombatDetails combatDetails(pTargetPlot);
        std::vector<const CvUnit*> remainingExistingUnits;
        std::vector<UnitData> hostileUnits, ourExistingUnits, ourPotentialUnits;

        for (size_t i = 0, count = enemyStack.size(); i < count; ++i)
        {
            if (enemyStack[i]->canFight())
            {
                hostileUnits.push_back(UnitData(enemyStack[i]));
            }
        }

        // skip defensive only (no attack) units - need to add flag for them, as combat calc will default them as surviving which confuses our logic
        for (std::set<IDInfo>::const_iterator ci(existingUnits.begin()), ciEnd(existingUnits.end()); ci != ciEnd; ++ci)
        {
            const CvUnit* pUnit = ::getUnit(*ci);
            if (pUnit && pUnit->canFight() && !pUnit->isOnlyDefensive())
            {
                ourExistingUnits.push_back(UnitData(pUnit));
                remainingExistingUnits.push_back(pUnit);  // keep in sync with unit data to track which units we select
            }
        }

        for (size_t i = 0, count = availableUnits.size(); i < count; ++i)
        {
            if (!gGlobals.getUnitInfo(availableUnits[i]).isOnlyDefensive())
            {
                // todo - derive promotion level dynamically
                ourPotentialUnits.push_back(UnitData(availableUnits[i], pUnitAnalysis->getCombatPromotions(availableUnits[i], 1).second));
            }
        }

        const int maxIterations = 2 * enemyStack.size();  // no point in trying to build a huge stack of outclassed units
        int iterations = 0;
        while (!hostileUnits.empty() && iterations++ < maxIterations)
        {
            bool haveExistingUnits = !ourExistingUnits.empty();
            std::vector<UnitData>& ourUnits(haveExistingUnits ? ourExistingUnits : ourPotentialUnits);
            size_t ourUnitsCount = ourUnits.size();
            std::vector<size_t> defenderIndex(ourUnitsCount);
            std::vector<int> defenderOdds(ourUnitsCount);

            // No counters!
            if (!haveExistingUnits && ourPotentialUnits.empty())
            {
                break;
            }

            // want the odds of our possible units v. all the hostiles to see what our best unit choice is
            // select all those >= threshold - if none - returns single entry with best odds
            std::list<StackCombatData> attackData = getBestUnitOdds(player, combatDetails, ourUnits, hostileUnits, 650);

            // sort possible attackers by survival odds
            attackData.sort(StackCombatDataComp());

            // best attacker has collateral damage:
            if (!attackData.empty() && ourUnits[attackData.begin()->attackerIndex].pUnitInfo->getCollateralDamage() > 0)
            {
                std::vector<int> unitsCollateralDamage = pUnitAnalysis->getCollateralDamage(ourUnits[attackData.begin()->attackerIndex], hostileUnits,
                    attackData.begin()->defenderIndex, combatDetails);
                for (size_t j = 0; j < unitsCollateralDamage.size(); ++j)
                {
#ifdef ALTAI_DEBUG
                    os << " apply collateral damage: " << unitsCollateralDamage[j] << " to unit: " << hostileUnits[j].pUnitInfo->getType();
#endif
                    hostileUnits[j].hp -= unitsCollateralDamage[j];
                }
            }

            if (!attackData.empty())
            {
                if (haveExistingUnits)
                {
                    // if odds are too poor, stop considering any remaining existing units
                    if (1.0 - attackData.begin()->odds.DefenderKillOdds < 0.2)
                    {
                        ourExistingUnits.clear();
                        remainingExistingUnits.clear();
                        continue;
                    }
                    else
                    {
                        requiredUnitStack.existingUnits.push_back(remainingExistingUnits[attackData.begin()->attackerIndex]->getIDInfo());
                        if (attackData.begin()->attackerIndex != ourUnitsCount - 1)
                        {
                            ourExistingUnits[attackData.begin()->attackerIndex] = ourExistingUnits.back();
                            remainingExistingUnits[attackData.begin()->attackerIndex] = remainingExistingUnits.back();
                        }
                        ourExistingUnits.pop_back();
                        remainingExistingUnits.pop_back();
                    }
                }
                else
                {
                    std::list<UnitData> potentialUnits;
                    for (std::list<StackCombatData>::const_iterator iter(attackData.begin()), endIter(attackData.end()); iter != endIter; ++iter)
                    {
                        potentialUnits.push_back(ourPotentialUnits[iter->attackerIndex]);
                    }
                    requiredUnitStack.unitsToBuild.push_back(potentialUnits);
                }

                if (attackData.begin()->odds.AttackerKillOdds >= 0.5)
                {
                    hostileUnits.erase(hostileUnits.begin() + attackData.begin()->defenderIndex);
                }
                else
                {
                    hostileUnits[attackData.begin()->defenderIndex].hp = (int)attackData.begin()->odds.E_HP_Def;
                }
            }
            else
            {
                break;
            }
        }
#ifdef ALTAI_DEBUG
        os << "\n combat details: ";
        combatDetails.debug(os);
        os << "\nRequired stack: ";
        if (!requiredUnitStack.existingUnits.empty())
        {
            os << "\n\t existing units: ";
            debugUnitIDInfoVector(os, requiredUnitStack.existingUnits);
        }
        if (!requiredUnitStack.unitsToBuild.empty())
        {
            os << "\n\t units to build: ";
        }
        for (size_t i = 0, count = requiredUnitStack.unitsToBuild.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << "{";
            for (std::list<UnitData>::const_iterator unitChoiceIter(requiredUnitStack.unitsToBuild[i].begin()), 
                unitChoiceEndIter(requiredUnitStack.unitsToBuild[i].end()); unitChoiceIter != unitChoiceEndIter; ++unitChoiceIter)
            {
                if (unitChoiceIter != requiredUnitStack.unitsToBuild[i].begin()) os << " or ";
                os << unitChoiceIter->pUnitInfo->getType();
                if (!unitChoiceIter->promotions.empty()) os << " [";
                for (size_t j = 0, count = unitChoiceIter->promotions.size(); j < count; ++j)
                {
                    if (j > 0) os << " + ";
                    os << gGlobals.getPromotionInfo(unitChoiceIter->promotions[j]).getType();
                }
                if (!unitChoiceIter->promotions.empty()) os << "]";
            }
            os << "}";
        }
#endif
        return requiredUnitStack;
    }

    void getReachablePlotsData(ReachablePlotsData& reachablePlotsData, const Player& player, const std::vector<const CvUnit*>& unitStack, bool useMaxMoves, bool allowAttack)
    {
        // split units into their stacks
        PlotUnitsMap stackPlotsMap;
        for (size_t i = 0, count = unitStack.size(); i < count; ++i)
        {
            stackPlotsMap[unitStack[i]->plot()].push_back(unitStack[i]);
        }

        for (PlotUnitsMap::const_iterator stackPlotIter(stackPlotsMap.begin()), stackPlotEndIter(stackPlotsMap.end()); stackPlotIter != stackPlotEndIter; ++stackPlotIter)
        {
            reachablePlotsData.allReachablePlots.insert(stackPlotIter->first);

            DomainTypes stackDomain = NO_DOMAIN;
            std::map<UnitMovementData, std::vector<const CvUnit*> > thisStacksGroupingMap;

            for (size_t i = 0, count = stackPlotIter->second.size(); i < count; ++i)
            {
                UnitMovementData unitMovementData(stackPlotIter->second[i]);
                thisStacksGroupingMap[unitMovementData].push_back(stackPlotIter->second[i]);
                reachablePlotsData.unitGroupingMap[unitMovementData].push_back(stackPlotIter->second[i]);

                DomainTypes unitDomain = stackPlotIter->second[i]->getDomainType();            
                FAssert(stackDomain == NO_DOMAIN || stackDomain == unitDomain);  // no mixed domain stacks
                stackDomain = unitDomain;
            }
            FAssert(stackDomain == DOMAIN_LAND || stackDomain == DOMAIN_SEA);

            const CvTeamAI& unitsTeam = CvTeamAI::getTeam(stackPlotIter->second[0]->getTeam());

            for (std::map<UnitMovementData, std::vector<const CvUnit*> >::const_iterator mIter(thisStacksGroupingMap.begin()), mEndIter(thisStacksGroupingMap.end());
                mIter != mEndIter; ++mIter)
            {
                std::map<const CvPlot*, int, CvPlotOrderF> reachablePlots;
                PlotSet openList;

                const bool canAttack = mIter->first.canAttack;
                const int movesLeft = useMaxMoves ? mIter->second[0]->maxMoves() : mIter->second[0]->movesLeft();
                if (movesLeft > 0)
                {
                    openList.insert(stackPlotIter->first);            
                }

                reachablePlots[stackPlotIter->first] = movesLeft;
                std::map<const CvUnit*, int, CvUnitIDInfoOrderF>& unitMovements = reachablePlotsData.unitMovementDataMap[stackPlotIter->first];
                for (size_t unitIndex = 0, unitCount = mIter->second.size(); unitIndex < unitCount; ++unitIndex)
                {
                    unitMovements.insert(std::make_pair(mIter->second[unitIndex], movesLeft));
                }

                while (!openList.empty())
                {
                    const CvPlot* pPlot = *openList.begin();
                    NeighbourPlotIter plotIter(pPlot);

                    while (IterPlot pLoopPlot = plotIter())
                    {
                        // todo - switch on whether we include plots where we need to attack
                        // isVisibleEnemyUnit will return true if, for that unit, any unit in the plot is from a team that unit's owner is at war with
                        if (pLoopPlot.valid() && pLoopPlot->isRevealed(player.getTeamID(), false) &&
                            pLoopPlot->isValidDomainForLocation(*mIter->second[0]) && (canAttack || !pLoopPlot->isVisibleEnemyUnit(mIter->second[0])))
                        {
                            if (!couldMoveUnitIntoPlot(mIter->second[0], pLoopPlot, allowAttack, false, useMaxMoves, allowAttack))
                            {
                                continue;
                            }

                            // todo - track which units this stack can reach which are its enemies
                            if (canAttack && pLoopPlot->isVisibleEnemyUnit(mIter->second[0]))
                            {
                            }
                        
                            int cost = stackDomain == DOMAIN_LAND ? landMovementCost(mIter->first, pPlot, pLoopPlot, unitsTeam) : seaMovementCost(mIter->first, pPlot, pLoopPlot, unitsTeam);
                            int thisMoveMovesLeft = std::max<int>(0, reachablePlots[pPlot] - cost);
                        
                            bool updateMovementData = true;
                            std::map<const CvPlot*, int, CvPlotOrderF>::iterator openPlotsIter = reachablePlots.find(pLoopPlot);
                            if (openPlotsIter != reachablePlots.end())
                            {
                                // already got here as cheaply or cheaper
                                int currentMoveMovesLeft = openPlotsIter->second;
                                if (currentMoveMovesLeft >= thisMoveMovesLeft)
                                {
                                    updateMovementData = false;
                                    continue;
                                }
                                else
                                {
                                    openPlotsIter->second = thisMoveMovesLeft;
                                }
                            }
                            else
                            {
                                openPlotsIter = reachablePlots.insert(std::make_pair(pLoopPlot, thisMoveMovesLeft)).first;
                            }

                            if (updateMovementData)
                            {
                                std::map<const CvUnit*, int, CvUnitIDInfoOrderF>& unitMovements = reachablePlotsData.unitMovementDataMap[pLoopPlot];
                                for (size_t unitIndex = 0, unitCount = mIter->second.size(); unitIndex < unitCount; ++unitIndex)
                                {
                                    unitMovements[mIter->second[unitIndex]] = openPlotsIter->second;
                                }
                            }

                            if (openPlotsIter->second > 0)
                            {
                                openList.insert(pLoopPlot);                                
                            }
                            reachablePlotsData.allReachablePlots.insert(pLoopPlot);
                        }
                    }
                    openList.erase(pPlot);
                }
            }
        }

//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
//        reachablePlotsData.debug(os);
//#endif
    }

    const CvPlot* getEscapePlot(const Player& player, const CvSelectionGroup* pGroup, const PlotSet& ourReachablePlots, const PlotSet& dangerPlots,
        const PlotUnitsMap& nearbyHostiles)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        const CvPlot* pTargetCityPlot = player.getAnalysis()->getMapAnalysis()->getClosestCity(pGroup->plot(), pGroup->plot()->getSubArea(), true);

        std::set<EscapePlotData> plotData;

        for (PlotSet::const_iterator plotsIter(ourReachablePlots.begin()), plotsEndIter(ourReachablePlots.end()); plotsIter != plotsEndIter; ++plotsIter)
        {
            // todo: add check for friendly stacks
            if ((*plotsIter)->isCity(false, pGroup->getTeam()) || dangerPlots.find(*plotsIter) == dangerPlots.end())
            {
                plotData.insert(EscapePlotData(pGroup->getTeam(), *plotsIter, pTargetCityPlot, nearbyHostiles));
            }
        }

        if (!plotData.empty())
        {
#ifdef ALTAI_DEBUG
            plotData.begin()->debug(os);
#endif
            return plotData.begin()->pPlot;
        }
        else
        {
#ifdef ALTAI_DEBUG
            os << "\ngetEscapePlot: stuck!! unit group = " << pGroup->getID() << " at: " << pGroup->plot()->getCoords();
#endif
            return pGroup->plot();  // stuck
        }
    }

    void GroupCombatData::calculate(const Player& player, const CvSelectionGroup* pGroup)
    {
        std::vector<const CvUnit*> ourUnits;

        UnitGroupIter unitIter(pGroup);
        {
            const CvUnit* pUnit = NULL;
            while (pUnit = unitIter())
            {
                ourUnits.push_back(pUnit);
            }
        }
        return calculate(player, ourUnits);
    }

    void GroupCombatData::calculate(const Player& player, const std::vector<const CvUnit*>& units)
    {
        attackCombatResultsMap.clear();
        defenceCombatResultsMap.clear();

        bool groupCanFight = false;
        std::vector<const CvUnit*> ourUnits;
        std::vector<UnitData> ourBaseUnitData, ourAttackUnitBaseData;

        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            const CvUnit* pUnit = units[i];
            bool combatUnit = pUnit->canFight();
            if (!groupCanFight && combatUnit)
            {
                groupCanFight = true;
            }
            ourUnits.push_back(pUnit);
            ourBaseUnitData.push_back(UnitData(pUnit));
            if (canAttack(pUnit, false, false))
            {
                ourAttackUnitBaseData.push_back(UnitData(pUnit));
            }
        }

        ReachablePlotsData reachablePlotsData;
        getReachablePlotsData(reachablePlotsData, player, ourUnits, false, groupCanFight);
        std::set<IDInfo> hostileUnits = player.getAnalysis()->getMilitaryAnalysis()->getUnitsThreateningPlots(reachablePlotsData.allReachablePlots);

        PlotUnitsMap hostilesLocationMap, captureUnitsMap;

        for (std::set<IDInfo>::const_iterator ci(hostileUnits.begin()), ciEnd(hostileUnits.end()); ci != ciEnd; ++ci)
        {
            const CvUnit* pEnemyUnit = ::getUnit(*ci);
            if (pEnemyUnit && pEnemyUnit->canFight())
            {
                if (reachablePlotsData.allReachablePlots.find(pEnemyUnit->plot()) != reachablePlotsData.allReachablePlots.end())
                {
                    // units we could potentially attack this move
                    hostilesLocationMap[pEnemyUnit->plot()].push_back(pEnemyUnit);
                }
            }
        }

        // plots we can reach mapped to hostile units which could reach us at that plot
        PlotUnitsMap potentialTargetsMap = 
            player.getAnalysis()->getMilitaryAnalysis()->getPlotsThreatenedByUnits(reachablePlotsData.allReachablePlots);        

        for (PlotUnitsMap::const_iterator plotIter(potentialTargetsMap.begin()),
            plotEndIter(potentialTargetsMap.end()); plotIter != plotEndIter; ++plotIter)
        {
            if (hostilesLocationMap.find(plotIter->first) != hostilesLocationMap.end())
            {
                continue; // plot contains a unit we would need to attack
            }
            UnitPlotIterP<OurUnitsPred> ourUnitsIter(plotIter->first, OurUnitsPred(player.getPlayerID()));
            std::vector<UnitData> ourUnitData(ourBaseUnitData);
            while (CvUnit* pPlotUnit = ourUnitsIter())
            {
                // unit of ours which can fight at this plot and is not in the group we started with
                if (pPlotUnit->canFight() && std::find_if(ourBaseUnitData.begin(), ourBaseUnitData.end(), UnitDataIDInfoP(pPlotUnit->getIDInfo())) != ourBaseUnitData.end())
                {
                    ourUnitData.push_back(UnitData(pPlotUnit));
                }
            }

            UnitData::CombatDetails combatDetails(plotIter->first);
            std::vector<UnitData> enemyUnitData;
            for (size_t i = 0, count = plotIter->second.size(); i < count; ++i)
            {
                if (plotIter->second[i]->canFight())
                {
                    enemyUnitData.push_back(UnitData(plotIter->second[i]));
                    // an approximation if not adjacent - todo - use path logic to deduce likely attack from plot
                    // attack direction: we are at plotIter->first - being attacked by units in enemyUnitData
                    combatDetails.unitAttackDirectionsMap[plotIter->second[i]->getIDInfo()] = directionXY(plotIter->second[i]->plot(), plotIter->first);
                }
            }

            CombatGraph combatGraph = getCombatGraph(player, combatDetails, enemyUnitData, ourUnitData);
            combatGraph.analyseEndStates();
            defenceCombatResultsMap[plotIter->first->getCoords()] = combatGraph.endStatesData;
        }

        //const std::map<XYCoords, CombatData>& attackableUnitsMap = player.getAnalysis()->getMilitaryAnalysis()->getAttackableUnitsMap();

        for (PlotUnitsMap::const_iterator hi(hostilesLocationMap.begin()), hiEnd(hostilesLocationMap.end());
            hi != hiEnd; ++hi)
        {
            UnitData::CombatDetails combatDetails(hi->first);
            std::vector<UnitData> enemyUnitData;
            for (size_t i = 0, count = hi->second.size(); i < count; ++i)
            {
                if (hi->second[i]->canFight())
                {
                    enemyUnitData.push_back(UnitData(hi->second[i]));
                    //combatDetails.unitAttackDirectionsMap[hi->second[i]->getIDInfo()] = directionXY(pGroup->plot(), hi->first);
                }
                else if (hi->second[i]->getCaptureUnitType(player.getCvPlayer()->getCivilizationType()) != NO_UNIT)
                {
                    captureUnitsMap[hi->first].push_back(hi->second[i]);
                }
            }

            if (!enemyUnitData.empty())
            {
                std::vector<UnitData> ourUnitData(ourAttackUnitBaseData);
                //// add in unit of ours which can reach this plot, but are not in this group or on its plot
                //std::map<XYCoords, CombatData>::const_iterator attIter = attackableUnitsMap.find(hi->first->getCoords());
                //if (attIter != attackableUnitsMap.end())
                //{
                //    for (size_t i = 0, count = attIter->second.attackers.size(); i < count; ++i)
                //    {
                //        const CvUnit* pUnit = ::getUnit(attIter->second.attackers[i].unitId);
                //        if (pUnit && pUnit->getGroupID() != pGroup->getID() && canAttack(pUnit))
                //        {
                //            ourUnitData.push_back(attIter->second.attackers[i]);
                //        }
                //    }
                //}

                for (size_t i = 0, count = ourUnits.size(); i < count; ++i)
                {
                    // an approximation if not adjacent - todo - use path logic to deduce likely attack from plot
                    // we are attacking hostile units at plot: hi->first
                    combatDetails.unitAttackDirectionsMap[ourUnits[i]->getIDInfo()] = directionXY(ourUnits[i]->plot(), hi->first);
                }
                CombatGraph combatGraph = getCombatGraph(player, combatDetails, ourUnitData, enemyUnitData);
                combatGraph.analyseEndStates();
                attackCombatResultsMap[hi->first->getCoords()] = combatGraph.endStatesData;
            }            
        }
    }

    void GroupCombatData::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        if (attackCombatResultsMap.empty() && defenceCombatResultsMap.empty())
        {
            os << "\n no combat data";
        }

        if (!attackCombatResultsMap.empty())
        {
            os << "\n attack results: ";
            debugCombatResultsMap(os, attackCombatResultsMap);
        }

        if (!defenceCombatResultsMap.empty())
        {
            os << "\n defence results: ";
            debugCombatResultsMap(os, defenceCombatResultsMap);
        }
#endif
    }

    void GroupCombatData::debugCombatResultsMap(std::ostream& os, const std::map<XYCoords, CombatGraph::Data>& resultsMap) const
    {
#ifdef ALTAI_DEBUG        
        for (std::map<XYCoords, CombatGraph::Data>::const_iterator ci(resultsMap.begin()), ciEnd(resultsMap.end()); ci != ciEnd; ++ci)
        {
            os << "\n\t " << ci->first;
            ci->second.debug(os);
        }
#endif
    }

    CombatGraph::Data GroupCombatData::getCombatData(XYCoords coords, bool isAttack) const
    {
        CombatGraph::Data combatData;
        if (isAttack)
        {
            std::map<XYCoords, CombatGraph::Data>::const_iterator ci(attackCombatResultsMap.find(coords));
            if (ci != attackCombatResultsMap.end())
            {
                combatData = ci->second;
            }
        }
        else
        {
            std::map<XYCoords, CombatGraph::Data>::const_iterator ci(defenceCombatResultsMap.find(coords));
            if (ci != defenceCombatResultsMap.end())
            {
                combatData = ci->second;
            }
        }
        return combatData;
    }

    const CvPlot* getNextMovePlot(const Player& player, const CvSelectionGroup* pGroup, const CvPlot* pTargetPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
#endif
        std::vector<const CvUnit*> ourUnits;
        bool groupCanFight = false;
        UnitGroupIter unitIter(pGroup);
        {
            const CvUnit* pUnit = NULL;
            while (pUnit = unitIter())
            {
                if (!groupCanFight && pUnit->canFight())
                {
                    groupCanFight = true;
                }
                ourUnits.push_back(pUnit);
            }
        }

        // ok to call this fn on current plot - unit will try to move if danger
        XYCoords moveToCoords(-1, -1);
        const XYCoords currentCoords = pGroup->plot()->getCoords(), targetCoords(pTargetPlot->getX(), pTargetPlot->getY());
        ReachablePlotsData ourReachablePlotsData;
        getReachablePlotsData(ourReachablePlotsData, player, ourUnits, false, groupCanFight);
        if (pTargetPlot != pGroup->plot())
        {
            // might not be able to generate a path - e.g. if hostiles are in the way
            UnitPathData unitPathData;
            unitPathData.calculate(pGroup, pTargetPlot, groupCanFight ? MOVE_IGNORE_DANGER : MOVE_NO_ENEMY_TERRITORY);
#ifdef ALTAI_DEBUG
            os << "\ngetNextMovePlot: turn = " << gGlobals.getGame().getGameTurn() << " group: " << pGroup->getID();
            unitPathData.debug(os);
#endif
            if (unitPathData.valid)
            {
                XYCoords lastVisibleStepWithMP = unitPathData.getLastVisiblePlotWithMP();
                if (lastVisibleStepWithMP != XYCoords(-1, -1))  // if we have any visible plots with moves - pick the last one as moveCoords
                {
                    moveToCoords = lastVisibleStepWithMP;
                }
                else
                {
                    moveToCoords = unitPathData.getFirstStepCoords();
                }                
            }
            else
            {
                int currentTargetDistance = plotDistance(currentCoords.iX, currentCoords.iY, targetCoords.iX, targetCoords.iY);
                for (PlotSet::const_iterator ci(ourReachablePlotsData.allReachablePlots.begin()), ciEnd(ourReachablePlotsData.allReachablePlots.end()); ci != ciEnd; ++ci)
                {
                    int thisPlotDistance = plotDistance((*ci)->getX(), (*ci)->getY(), targetCoords.iX, targetCoords.iY);
                    if (thisPlotDistance < currentTargetDistance)
                    {
                        currentTargetDistance = thisPlotDistance;
                        moveToCoords = (*ci)->getCoords();
                    }
                }
#ifdef ALTAI_DEBUG
                os << "\ngetNextMovePlot: turn = " << gGlobals.getGame().getGameTurn()
                   << " unable to generate full path from " << currentCoords << " to " << targetCoords
                   << " picked move to coords: " << moveToCoords;
#endif
            }
        }
        else
        {
            moveToCoords = targetCoords;
        }

        if (moveToCoords != XYCoords(-1, -1))
        {
            const CvPlot* pMoveToPlot = gGlobals.getMap().plot(moveToCoords.iX, moveToCoords.iY);
            PlotSet moveToPlotSet;
            moveToPlotSet.insert(pMoveToPlot);  // just add target plot - might be better to include transit plots?

            std::set<IDInfo> hostileUnits = player.getAnalysis()->getMilitaryAnalysis()->getUnitsThreateningPlots(moveToPlotSet);

            if (hostileUnits.empty())
            {
#ifdef ALTAI_DEBUG
                os << " getNextMovePlot returning: " << pMoveToPlot->getCoords();
#endif
                return pMoveToPlot;
            }
            else
            {
                // todo: also determine when even if we could fight, we would be overwhelmed - and so should run away
                if (!groupCanFight)
                {
#ifdef ALTAI_DEBUG
                    os << "\nGroup at: " << pGroup->plot()->getCoords() << " could reach hostile units (count = " << hostileUnits.size() << ") ";
                    for (std::set<IDInfo>::const_iterator ci(hostileUnits.begin()), ciEnd(hostileUnits.end()); ci != ciEnd; ++ci)
                    {
                        os << " id = " << *ci;
                        const CvUnit* pEnemyUnit = ::getUnit(*ci);
                        if (pEnemyUnit)
                        {
                            os << " at: " << pEnemyUnit->plot()->getCoords() << ", ";
                        }
                    }
#endif
                    return getEscapePlot(player, pGroup, ourReachablePlotsData.allReachablePlots, player.getAnalysis()->getMilitaryAnalysis()->getThreatenedPlots(),
                        getNearbyHostileStacks(player, pGroup->plot(), 2));
                }

                PlotUnitsMap hostilesLocationMap, captureUnitsMap;
                std::map<const CvPlot*, float, CvPlotOrderF> defenceOddsMap, attackOddsMap;

                std::vector<UnitData> ourUnitData;
                for (size_t i = 0, count = ourUnits.size(); i < count; ++i)
                {
                    if (ourUnits[i]->canFight())
                    {
                        ourUnitData.push_back(UnitData(ourUnits[i]));
                    }
                }
                std::vector<const CvUnit*> enemyStack;
                std::vector<UnitData> enemyUnitData;

                for (std::set<IDInfo>::const_iterator ci(hostileUnits.begin()), ciEnd(hostileUnits.end()); ci != ciEnd; ++ci)
                {
                    const CvUnit* pEnemyUnit = ::getUnit(*ci);
                    if (pEnemyUnit && pEnemyUnit->canFight())
                    {
                        enemyStack.push_back(pEnemyUnit);
                        enemyUnitData.push_back(UnitData(pEnemyUnit));

                        if (ourReachablePlotsData.allReachablePlots.find(pEnemyUnit->plot()) != ourReachablePlotsData.allReachablePlots.end())
                        {
                            // units we could potentially attack this move
                            hostilesLocationMap[pEnemyUnit->plot()].push_back(pEnemyUnit);
                        }
                    }
                }
                
                // plots we can reach mapped to hostile units which could reach us at that plot
                PlotUnitsMap potentialTargetsMap = 
                    player.getAnalysis()->getMilitaryAnalysis()->getPlotsThreatenedByUnits(ourReachablePlotsData.allReachablePlots);

                for (PlotUnitsMap::const_iterator plotIter(potentialTargetsMap.begin()), plotEndIter(potentialTargetsMap.end());
                    plotIter != plotEndIter; ++plotIter)
                {
                    if (hostilesLocationMap.find(plotIter->first) != hostilesLocationMap.end())
                    {
                        continue; // plot contains a unit we would need to attack
                    }

                    UnitData::CombatDetails combatDetails(plotIter->first);
                    std::vector<UnitData> enemyUnitData;
                    for (size_t i = 0, count = plotIter->second.size(); i < count; ++i)
                    {
                        if (plotIter->second[i]->canFight())
                        {
                            enemyUnitData.push_back(UnitData(plotIter->second[i]));
                            // an approximation if not adjacent - todo - use path logic to deduce likely attack from plot
                            // attack direction: we are at plotIter->first - being attacked by units in enemyUnitData
                            combatDetails.unitAttackDirectionsMap[plotIter->second[i]->getIDInfo()] = directionXY(plotIter->second[i]->plot(), plotIter->first);
                        }
                    }

                    CombatGraph combatGraph = getCombatGraph(player, combatDetails, enemyUnitData, ourUnitData);
                    combatGraph.analyseEndStates();

                    defenceOddsMap[plotIter->first] = combatGraph.endStatesData.pLoss + combatGraph.endStatesData.pDraw;  // odds we survive attack

#ifdef ALTAI_DEBUG
                    os << "\nOdds data defending at plot: " << plotIter->first->getCoords() << " for group: ";
                    debugUnitVector(os, ourUnits);
                    combatDetails.debug(os);
                    os << " winning odds = " << combatGraph.endStatesData.pLoss << ", losing odds = " << combatGraph.endStatesData.pWin;
                    if (combatGraph.endStatesData.pDraw > 0)
                    {
                        os << " draw = " << combatGraph.endStatesData.pDraw;
                    }
                    for (size_t i = 0, count = ourUnits.size(); i < count; ++i)
                    {
                        os << " " << ourUnits[i]->getIDInfo() << " survival odds = " << combatGraph.getSurvivalOdds(ourUnits[i]->getIDInfo(), false);
                    }
                    for (size_t i = 0, count = ourUnits.size(); i < count; ++i)
                    {
                        os << " " << ourUnits[i]->getIDInfo() << " survival odds (2) = " << combatGraph.endStatesData.defenderUnitOdds[i];
                    }
                    os << "\n";
                    combatGraph.debugEndStates(os);
#endif
                }

                for (PlotUnitsMap::const_iterator hi(hostilesLocationMap.begin()), hiEnd(hostilesLocationMap.end());
                    hi != hiEnd; ++hi)
                {
                    UnitData::CombatDetails combatDetails(hi->first);
                    enemyStack.clear();
                    enemyUnitData.clear();
                    for (size_t i = 0, count = hi->second.size(); i < count; ++i)
                    {
                        if (hi->second[i]->canFight())
                        {
                            enemyStack.push_back(hi->second[i]);
                            enemyUnitData.push_back(UnitData(hi->second[i]));
                        }
                        else if (hi->second[i]->getCaptureUnitType(player.getCvPlayer()->getCivilizationType()) != NO_UNIT)
                        {
                            captureUnitsMap[hi->first].push_back(hi->second[i]);
                        }
                    }

                    float pWin, pLoss;
                    CombatGraph combatGraph;
                    if (!enemyUnitData.empty())
                    {
                        for (size_t i = 0, count = ourUnits.size(); i < count; ++i)
                        {
                            // an approximation if not adjacent - todo - use path logic to deduce likely attack from plot
                            // we are attacking hostile units at plot: hi->first
                            combatDetails.unitAttackDirectionsMap[ourUnits[i]->getIDInfo()] = directionXY(ourUnits[i]->plot(), hi->first);
                        }
                        combatGraph = getCombatGraph(player, combatDetails, ourUnitData, enemyUnitData);
                        combatGraph.analyseEndStates();

                        attackOddsMap[hi->first] = combatGraph.endStatesData.pWin + combatGraph.endStatesData.pDraw;
                    }
                    else
                    {
                        pWin = 1.0, pLoss = 0.0;
                        attackOddsMap[hi->first] = 1.0;
                    }

#ifdef ALTAI_DEBUG  
                    if (!enemyUnitData.empty())
                    {
                        os << "\nOdds data attacking plot: " << hi->first->getCoords() << " for group: " << pGroup->getID();
                        combatDetails.debug(os);
                        os << " win odds = " << combatGraph.endStatesData.pWin << ", losing odds = " << combatGraph.endStatesData.pLoss;
                        if (combatGraph.endStatesData.pDraw > 0)
                        {
                            os << ", survival odds = " << attackOddsMap[hi->first];
                        }

                        for (size_t i = 0, count = ourUnits.size(); i < count; ++i)
                        {
                            os << " " << ourUnits[i]->getIDInfo() << " survival odds = " << combatGraph.getSurvivalOdds(ourUnits[i]->getIDInfo(), false);
                        }
                        for (size_t i = 0, count = ourUnits.size(); i < count; ++i)
                        {
                            os << " " << ourUnits[i]->getIDInfo() << " survival odds = " << combatGraph.endStatesData.attackerUnitOdds[i];
                        }
                    }
                    else
                    {
                        os << "\nOdds data attacking plot: " << hi->first->getCoords() << " for group: " << pGroup->getID();
                        os << " win odds = " << pWin << ", losing odds = " << pLoss << ", survival odds = " << attackOddsMap[hi->first];
                    }
                    os << "\n";
                    combatGraph.debugEndStates(os);
                    
                    if (captureUnitsMap.find(hi->first) != captureUnitsMap.end())
                    {
                        os << "\nCould capture units: ";
                        debugUnitVector(os, captureUnitsMap[hi->first]);
                    }
#endif
                }

                const CvPlot* pBestAttackPlot = NULL, *pBestOddsPlot = NULL, *pBestCaptureOddsPlot = NULL;
                float bestAttackOdds = 0, bestCaptureOdds = 0;
                if (!attackOddsMap.empty())
                {
                    for (std::map<const CvPlot*, float, CvPlotOrderF>::const_iterator ci(attackOddsMap.begin()), ciEnd(attackOddsMap.end()); ci != ciEnd; ++ci)
                    {
                        PlotUnitsMap::const_iterator captureIter = captureUnitsMap.find(ci->first) ;
                        if (captureIter != captureUnitsMap.end() && ci->second > bestCaptureOdds)
                        {
                            bestCaptureOdds = ci->second;
                            pBestCaptureOddsPlot = ci->first;
                        }

                        if (ci->second > bestAttackOdds)
                        {
                            bestAttackOdds = ci->second;
                            pBestOddsPlot = ci->first;
                        }

                        if (ci->second < 0.8)
                        {
                            ourReachablePlotsData.allReachablePlots.erase(ci->first);
                        }
                    }

#ifdef ALTAI_DEBUG
                     os << "\nBest attack odds = " << bestAttackOdds << " for plot: "
                        << (pBestOddsPlot ? pBestOddsPlot->getCoords() : XYCoords());
                     if (pBestCaptureOddsPlot)
                     {
                         os << " best capture odds plot " << pBestCaptureOddsPlot->getCoords() << " = " << bestCaptureOdds;
                     }
#endif
                    if (pBestCaptureOddsPlot && bestCaptureOdds > 0.8f)
                    {
                        pBestAttackPlot = pBestCaptureOddsPlot;
                    }
                    else if (bestAttackOdds > 0.9f)
                    {
                        pBestAttackPlot = pBestOddsPlot;
                    }
                }

                if (!pBestAttackPlot)
                {
                    float bestOdds = 0.0f, targetPlotOdds = 0.0f;
                    int bestStepDistanceToTarget = MAX_INT;
                    const CvPlot* pBestOddsPlot = NULL, *pBestPlot = NULL;

                    for (PlotSet::const_iterator reachablePlotsIter(ourReachablePlotsData.allReachablePlots.begin()); reachablePlotsIter != ourReachablePlotsData.allReachablePlots.end(); ++reachablePlotsIter)
                    {
                        std::map<const CvPlot*, float, CvPlotOrderF>::const_iterator oddsIter = defenceOddsMap.find(*reachablePlotsIter);

                        if ((*reachablePlotsIter)->getCoords() == pMoveToPlot->getCoords())
                        {
                            targetPlotOdds = oddsIter == defenceOddsMap.end() ? 1.0f : oddsIter->second;
                        }
                        
                        if (oddsIter != defenceOddsMap.end())
                        {
                            if (oddsIter->second > bestOdds)
                            {
                                bestOdds = oddsIter->second;
                                pBestOddsPlot = *reachablePlotsIter;
                            }

                            if (bestOdds < 0.8f)
                            {
                                continue;
                            }
                        }

                        // shouldn't pick any plots based on distance to target if thheir odds are less than 0.9
                        int thisPlotsDistanceToTarget = stepDistance((*reachablePlotsIter)->getX(), (*reachablePlotsIter)->getY(), pTargetPlot->getX(), pTargetPlot->getY());
                        if (thisPlotsDistanceToTarget <= bestStepDistanceToTarget)
                        {                            
                            if (thisPlotsDistanceToTarget == bestStepDistanceToTarget)
                            {
                                // best plot is currently our current plot, and the stepdistance we've calculated is the same - then replace pBestPlot
                                if (pBestPlot && pBestPlot->getCoords() == currentCoords)
                                {
                                    pBestPlot = *reachablePlotsIter;
                                }
                            }
                            else
                            {
                                bestStepDistanceToTarget = thisPlotsDistanceToTarget;
                                pBestPlot = *reachablePlotsIter;
                            }
                        }
                    }
#ifdef ALTAI_DEBUG
                    os << "\nTarget plot defence odds = " << targetPlotOdds << ", best odds plot = " << (pBestOddsPlot ? pBestOddsPlot->getCoords() : XYCoords()) 
                        << " with odds = " << bestOdds << " best step distance plot = " << (pBestPlot ? pBestPlot->getCoords() : XYCoords());
#endif
                    if (targetPlotOdds < 0.8f)
                    {
                        if (pBestPlot)
                        {
                            pMoveToPlot = pBestPlot;
                        }
                        else if (pBestOddsPlot)
                        {
                            pMoveToPlot = pBestOddsPlot;
                        }
                    }
                }
                else
                {
                    pMoveToPlot = pBestAttackPlot;
                }
            }
#ifdef ALTAI_DEBUG
            os << " getNextMovePlot returning: " << pMoveToPlot->getCoords();
#endif
            return pMoveToPlot;
        }
        else
        {
#ifdef ALTAI_DEBUG
            os << " getNextMovePlot returning no plot! ";
#endif
            return (const CvPlot*)0;
        }
    }

    bool couldMoveUnitIntoPlot(const CvUnit* pUnit, const CvPlot* pPlot, bool includeAttackPlots, bool includeDeclareWarPlots, bool useMaxMoves, bool ignoreIfAlreadyAttacked)
    {
        if (!couldMoveIntoCommon(pUnit, pPlot, includeDeclareWarPlots))
        {
            return false;
        }

        if (!couldMoveInfoDomainSpecific(pUnit, pPlot))
        {
            return false;
        }

        PlayerTypes unitOwner = pUnit->getOwner();
        // use player's view rather than original fn's unit's view
        bool plotHasEnemyUnit = pPlot->isVisibleEnemyUnit(unitOwner);

        if (pUnit->isAnimal())
        {
            if (pPlot->isOwned())  // animals can't enter players' borders
            {
                return false;
            }

            if (!plotHasEnemyUnit)
            {
                if (pPlot->getBonusType() != NO_BONUS)
                {
                    return false;
                }

                if (pPlot->getImprovementType() != NO_IMPROVEMENT)
                {
                    return false;
                }

                if (pPlot->getNumUnits() > 0)
                {
                    return false;
                }
            }
        }

        // unguarded enemy city, but unit which can't capture it
        if (pUnit->isNoCapture() && !plotHasEnemyUnit && pPlot->isEnemyCity(*pUnit))
        {
            return false;
        }

        if (includeAttackPlots && plotHasEnemyUnit)
        {
            if (!canAttack(pUnit, useMaxMoves, ignoreIfAlreadyAttacked))
            {
                return false;
            }
        }

        // not an invisible unit (i.e. spy)
        if (!pUnit->canCoexistWithEnemyUnit(NO_TEAM))
        {    
            if (pUnit->canAttack())  // not defensive only unit
            {
                if (plotHasEnemyUnit)
                {
                    if (!includeAttackPlots)
                    {
                        return false;
                    }
                    else
                    {
                        const CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, unitOwner, pUnit, true);
                        if (pDefender && !pUnit->canAttack(*pDefender)) // check ships in otherwise undefended city
                        {
                            return false;
                        }
                    }
                }
            }
            else
            {
                if (pPlot->isEnemyCity(*pUnit) || pPlot->isVisibleEnemyUnit(pUnit))
                {
                    return false;
                }                
            }
        }

        return true;
    }
}