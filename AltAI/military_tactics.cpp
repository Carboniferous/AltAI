#include "AltAI.h"

#include "./military_tactics.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./unit_explore.h"
#include "./map_analysis.h"
#include "./iters.h"
#include "./player_analysis.h"
#include "./unit_analysis.h"
#include "./civ_log.h"
#include "./unit_log.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        void getOverlapAndNeighbourPlots(const std::set<const CvPlot*>& theirPlots, const std::set<const CvPlot*>& ourPlots, 
            std::set<const CvPlot*>& sharedPlots, std::set<const CvPlot*>& neighbourPlots)
        {
            for (std::set<const CvPlot*>::const_iterator ourIter(ourPlots.begin()), ourEndIter(ourPlots.end()); ourIter != ourEndIter; ++ourIter)
            {
                std::set<const CvPlot*>::const_iterator theirIter(theirPlots.find(*ourIter));
                if (theirIter != theirPlots.end())
                {
                    sharedPlots.insert(*ourIter);
                }
                else
                {
                    NeighbourPlotIter plotIter(*ourIter, 1, 1);

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

        struct UnitMovementData
        {
            UnitMovementData() : moves(0), extraMovesDiscount(0),
                featureDoubleMoves(gGlobals.getNumFeatureInfos(), 0), terrainDoubleMoves(gGlobals.getNumTerrainInfos(), 0),
                isHillsDoubleMoves(false), ignoresTerrainCost(false), isEnemyRoute(false), ignoresImpassable(false)
            {
            }

            explicit UnitMovementData(const CvUnit* pUnit)
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
            }

            // allows unique differentiation of any combination of unit movement attributes
            // useful to efficiently calculate possible moves for a large stack without checking all its units
            bool operator < (const UnitMovementData& other) const
            {
                if (moves != other.moves)
                {
                    return moves < other.moves;
                }

                if (extraMovesDiscount != other.extraMovesDiscount)
                {
                    return extraMovesDiscount < other.extraMovesDiscount;
                }

                if (isHillsDoubleMoves == other.isHillsDoubleMoves)
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

            // ignore flatMovementCost flag as not used for any standard game units
            int moves, extraMovesDiscount;
            std::vector<char> featureDoubleMoves, terrainDoubleMoves;
            bool isHillsDoubleMoves, ignoresTerrainCost, isEnemyRoute, ignoresImpassable;
        };

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

        struct StackAttackData
        {
            size_t attackerIndex, defenderIndex;
            UnitOddsData odds;
        };

        StackAttackData getBestUnitOdds(const Player& player, const UnitData::CombatDetails& combatDetails, const std::vector<UnitData>& ourUnits, const std::vector<UnitData>& hostileUnits)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
#endif
            const size_t ourUnitsCount = ourUnits.size();
            std::vector<size_t> defenderIndex(ourUnitsCount);
            std::vector<int> defenderOdds(ourUnitsCount);

            for (size_t i = 0, count = ourUnitsCount; i < count; ++i)
            {
                std::vector<int> odds = player.getAnalysis()->getUnitAnalysis()->getOdds(ourUnits[i], hostileUnits, combatDetails, true);
                
                // which is best hostile defender v. this unit
                std::vector<int>::const_iterator oddsIter = std::min_element(odds.begin(), odds.end());
                defenderOdds[i] = 1000 - *oddsIter;
                defenderIndex[i] = std::distance<std::vector<int>::const_iterator>(odds.begin(), oddsIter);
                for (size_t j = 0; j < odds.size(); ++j)
                {
#ifdef ALTAI_DEBUG
                    os << " " << ourUnits[i].pUnitInfo->getType() << " v. " << hostileUnits[j].pUnitInfo->getType() << " = " << odds[j];
                    ourUnits[i].debug(os);
                    hostileUnits[j].debug(os);
#endif
                }
                
#ifdef ALTAI_DEBUG
                os << " best def = " << hostileUnits[defenderIndex[i]].pUnitInfo->getType() << "\n";
                if (hostileUnits[defenderIndex[i]].hp < 100)
                {
                    os << " def hp = " << hostileUnits[defenderIndex[i]].hp;
                }
#endif
                if (ourUnits[i].pUnitInfo->getCollateralDamage() > 0)
                {
                    std::vector<int> unitsCollateralDamage = player.getAnalysis()->getUnitAnalysis()->getCollateralDamage(ourUnits[i], hostileUnits, defenderIndex[i], combatDetails);
#ifdef ALTAI_DEBUG
                    for (size_t j = 0; j < unitsCollateralDamage.size(); ++j)
                    {
                        os << " unit = " << hostileUnits[defenderIndex[i]].pUnitInfo->getType() << " c.damage = " << unitsCollateralDamage[j];
                    }
#endif
                }
            }

            // find defender with worst odds
            std::vector<int>::const_iterator oddsIter = std::min_element(defenderOdds.begin(), defenderOdds.end());
            size_t foundIndex = std::distance<std::vector<int>::const_iterator>(defenderOdds.begin(), oddsIter);
            UnitOddsData oddsDetail = player.getAnalysis()->getUnitAnalysis()->getCombatOddsDetail(ourUnits[foundIndex], hostileUnits[defenderIndex[foundIndex]], combatDetails);

#ifdef ALTAI_DEBUG            
            os << "\n\tworst def. odds = " << *oddsIter << " best attacker = " << ourUnits[foundIndex].pUnitInfo->getType() << " def = " << hostileUnits[defenderIndex[foundIndex]].pUnitInfo->getType();
            oddsDetail.debug(os);
#endif
            StackAttackData data;
            data.attackerIndex = foundIndex;
            data.defenderIndex = defenderIndex[foundIndex];
            data.odds = oddsDetail;
            return data;
        }

        struct RequiredUnits
        {
            std::vector<UnitData> unitsToBuild;
            std::vector<IDInfo> existingUnits;
        };

        RequiredUnits getRequiredLandAttackStack(const Player& player, const std::vector<UnitTypes>& availableUnits, const CvPlot* pTargetPlot, 
            const std::vector<const CvUnit*>& enemyStack, const std::set<IDInfo>& existingUnits)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
#endif
            boost::shared_ptr<UnitAnalysis> pUnitAnalysis = player.getAnalysis()->getUnitAnalysis();

            UnitData::CombatDetails combatDetails(pTargetPlot);
            std::vector<const CvUnit*> remainingExistingUnits;
            std::vector<UnitData> hostileUnits, ourExistingUnits, ourPotentialUnits;
            RequiredUnits requiredUnits;

            for (size_t i = 0, count = enemyStack.size(); i < count; ++i)
            {
                if (enemyStack[i]->canFight())
                {
                    hostileUnits.push_back(UnitData(enemyStack[i]));
                }
            }

            for (std::set<IDInfo>::const_iterator ci(existingUnits.begin()), ciEnd(existingUnits.end()); ci != ciEnd; ++ci)
            {
                const CvUnit* pUnit = ::getUnit(*ci);
                if (pUnit && pUnit->canFight())
                {
                    ourExistingUnits.push_back(UnitData(pUnit));
                    remainingExistingUnits.push_back(pUnit);  // keep in sync with unit data to track which units we select
                }
            }

            for (size_t i = 0, count = availableUnits.size(); i < count; ++i)
            {
                if (!gGlobals.getUnitInfo(availableUnits[i]).isOnlyDefensive())
                {
                    ourPotentialUnits.push_back(UnitData(availableUnits[i], pUnitAnalysis->getCombatPromotions(availableUnits[i], 1).second));
                }
            }

            while (!hostileUnits.empty())
            {
                bool haveExistingUnits = !ourExistingUnits.empty();
                size_t ourUnitsCount = haveExistingUnits ? ourPotentialUnits.size() : ourExistingUnits.size();
                std::vector<size_t> defenderIndex(ourUnitsCount);
                std::vector<int> defenderOdds(ourUnitsCount);

                // want the highest minimum odds of our possible units v. all the hostiles to see what our best unit choice is
                StackAttackData attackData = getBestUnitOdds(player, combatDetails, haveExistingUnits ? ourExistingUnits : ourPotentialUnits, hostileUnits);

                if (haveExistingUnits)
                {
                    // if odds are too poor, stop considering any remaining existing units
                    if (attackData.odds.AttackerKillOdds < 0.2)
                    {
                        ourExistingUnits.clear();
                        remainingExistingUnits.clear();
                        continue;
                    }
                    else
                    {
                        requiredUnits.existingUnits.push_back(remainingExistingUnits[attackData.attackerIndex]->getIDInfo());
                        if (attackData.attackerIndex != ourUnitsCount - 1)
                        {
                            ourExistingUnits[attackData.attackerIndex] = ourExistingUnits.back();
                            remainingExistingUnits[attackData.attackerIndex] = remainingExistingUnits.back();
                        }
                        ourExistingUnits.pop_back();
                        remainingExistingUnits.pop_back();
                    }
                }
                else
                {
                    requiredUnits.unitsToBuild.push_back(ourPotentialUnits[attackData.attackerIndex]);
                }

                if (attackData.odds.AttackerKillOdds >= 0.5)
                {
                    hostileUnits.erase(hostileUnits.begin() + attackData.defenderIndex);
                }
                else
                {
                    hostileUnits[attackData.defenderIndex].hp = attackData.odds.E_HP_Def;
                }
            }
#ifdef ALTAI_DEBUG
            os << "\nRequired stack: ";
            if (!requiredUnits.existingUnits.empty())
            {
                os << "\n\t existing units: ";
            }
            for (size_t i = 0, count = requiredUnits.existingUnits.size(); i < count; ++i)
            {
                const CvUnit* pUnit = ::getUnit(requiredUnits.existingUnits[i]);
                os << pUnit->getUnitInfo().getType() << " ";
                for (int j = 0, promotionCount = gGlobals.getNumPromotionInfos(); j < promotionCount; ++j)
                {
                    if (pUnit->isHasPromotion((PromotionTypes)j))
                    {
                        os << gGlobals.getPromotionInfo((PromotionTypes)j).getType() << " ";
                    }
                }
            }
            if (!requiredUnits.unitsToBuild.empty())
            {
                os << "\n\t units to build: ";
            }
            for (size_t i = 0, count = requiredUnits.unitsToBuild.size(); i < count; ++i)
            {
                os << requiredUnits.unitsToBuild[i].pUnitInfo->getType();
                for (size_t j = 0, count = requiredUnits.unitsToBuild[i].promotions.size(); j < count; ++j)
                {
                    os << gGlobals.getPromotionInfo(requiredUnits.unitsToBuild[i].promotions[j]).getType() << " ";
                }
            }
#endif
            return requiredUnits;
        }        
    }

    class MilitaryAnalysisImpl
    {
    public:
        enum LandUnitCategories
        {
            Unknown = -1, Settler = 0, Worker, Scout, Combat, Missionary, GreatPerson
        };
        static const size_t CategoryCount = 5;

        struct UnitHistory
        {
            UnitHistory(const IDInfo& unit_, XYCoords coords) : unit(unit_)
            {
                locationHistory.push_front(std::make_pair(gGlobals.getGame().getGameTurn(), coords));
            }

            IDInfo unit;
            std::list<std::pair<int, XYCoords> > locationHistory;
        };

        struct MilitaryMissionData
        {
            MilitaryMissionData(MissionAITypes missionType_) : plotTarget(-1, -1), pClosestCity(NULL), missionType(missionType_) {}

            std::set<IDInfo> targets;
            XYCoords plotTarget;
            std::set<IDInfo> assignedUnits;
            std::vector<UnitData> requiredUnits;

            std::set<const CvPlot*> reachablePlots;
            CvCity* pClosestCity;

            MissionAITypes missionType;

            int getRequiredUnitCount(UnitTypes unitType) const
            {
                return std::count_if(requiredUnits.begin(), requiredUnits.end(), UnitTypeP(unitType));
            }

            int getAssignedUnitCount(const CvPlayer* pPlayer, UnitTypes unitType) const
            {
                int count = 0;
                for (std::set<IDInfo>::const_iterator ci(assignedUnits.begin()), ciEnd(assignedUnits.end()); ci != ciEnd; ++ci)
                {
                    const CvUnit* pUnit = pPlayer->getUnit(ci->iID);
                    if (pUnit && pUnit->getUnitType() == unitType)
                    {
                        ++count;
                    }
                }
                return count;
            }

            void debug(std::ostream& os) const
            {
#ifdef ALTAI_DEBUG
                os << "\nMilitary mission data:" << " unit ai = " << getMissionAIString(missionType) << "\nTargets: ";
                for (std::set<IDInfo>::const_iterator targetsIter(targets.begin()), targetsEndIter(targets.end()); targetsIter != targetsEndIter; ++targetsIter)
                {
                    os << *targetsIter << ", ";

                    if (missionType == MISSIONAI_GUARD_CITY)
                    {
                        os << " city = " << safeGetCityName(*targetsIter);
                    }
                    else
                    {
                        const CvUnit* pUnit = ::getUnit(*targetsIter);
                        if (pUnit)
                        {
                            os << " at: " << pUnit->plot()->getCoords() << " ";
                        }
                        else
                        {
                            os << " (not found) ";
                        }
                    }
                }
                os << "\nAssigned units : ";
                for (std::set<IDInfo>::const_iterator ourUnitsIter(assignedUnits.begin()), ourUnitsEndIter(assignedUnits.end()); ourUnitsIter != ourUnitsEndIter; ++ourUnitsIter)
                {
                    os << *ourUnitsIter << ", ";
                    const CvUnit* pUnit = ::getUnit(*ourUnitsIter);
                    if (pUnit)
                    {
                        os << " at: " << pUnit->plot()->getCoords() << " ";
                    }
                    else
                    {
                        os << " (not found) ";
                    }
                }
                os << "\nRequired units: ";
                for (size_t i = 0, count = requiredUnits.size(); i < count; ++i)
                {
                    if (i > 0) os << ", ";
                    os << requiredUnits[i].pUnitInfo->getType();
                    requiredUnits[i].debug(os);                    
                }
#endif
            }
        };

        typedef boost::shared_ptr<MilitaryMissionData> MilitaryMissionDataPtr;

        explicit MilitaryAnalysisImpl(Player& player) : player_(player)
        {
        }

        void update()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            std::map<XYCoords, int> stacks;
            
            std::vector<UnitTypes> landCombatUnits, seaCombatUnits, possibleLandCombatUnits, possibleSeaCombatUnits;
            boost::tie(landCombatUnits, possibleLandCombatUnits) = getActualAndPossibleCombatUnits(player_, NULL, DOMAIN_LAND);
            boost::tie(seaCombatUnits, possibleSeaCombatUnits) = getActualAndPossibleCombatUnits(player_, NULL, DOMAIN_SEA);

            CvTeamAI& ourTeam = CvTeamAI::getTeam(player_.getTeamID());
            enemyStacks_.clear();

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
                        UnitData::CombatDetails combatDetails(pPlot);
                    
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
#endif
                                        foundHostiles = true;
                                    }

                                    UnitData hostileUnit(pUnit);
                                    
                                    // get odds with hostile as defender - odds will come back as the defender's chance of surviving attack, so need to subtract them from 1000 to get our units' odds
                                    std::vector<int> odds = player_.getAnalysis()->getUnitAnalysis()->getOdds(hostileUnit, 
                                        pUnit->getDomainType() == DOMAIN_LAND ? landCombatUnits : seaCombatUnits, 1, combatDetails, false);

                                    int bestOdds = 0, bestUnitIndex = -1;
                                    for (size_t i = 0; i < odds.size(); ++i)
                                    {
                                        if (1000 - odds[i] > bestOdds)
                                        {
                                            bestOdds = 1000 - odds[i];
                                            bestUnitIndex = i;
                                        }
                                    }
#ifdef ALTAI_DEBUG
                                    if (bestUnitIndex != -1)
                                    {
                                        os << "\nBest counter = " << gGlobals.getUnitInfo(pUnit->getDomainType() == DOMAIN_LAND ? landCombatUnits[bestUnitIndex] : seaCombatUnits[bestUnitIndex]).getType()
                                            << " odds = " << bestOdds;
                                    }
                                    else
                                    {
                                        os << "\nNo counters!";
                                    }

#endif
                                    stacks[cii->first] += 1000 - bestOdds;
                                    enemyStacks_[cii->first].push_back(pUnit);
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
            const int currentTurn = gGlobals.getGame().getGameTurn();
            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end();)
            {
                if ((*missionsIter)->missionType == MISSIONAI_COUNTER)
                {
                    for (std::set<IDInfo>::iterator targetsIter((*missionsIter)->targets.begin()), targetsEndIter((*missionsIter)->targets.end());
                        targetsIter != targetsEndIter;)
                    {
                        std::map<IDInfo, UnitHistory>::iterator hIter = unitHistories_.find(*targetsIter);
                        if (hIter != unitHistories_.end() && !hIter->second.locationHistory.empty())
                        {
                            int whenLastSeen = currentTurn - hIter->second.locationHistory.begin()->first;
#ifdef ALTAI_DEBUG
                            os << "\nTarget last seen " << whenLastSeen << " turn(s) ago";
#endif
                            if (whenLastSeen > 1)
                            {
                                (*missionsIter)->reachablePlots.clear();
                                if (whenLastSeen > 3)
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nRemoving target from mission - target(s) lost ";
#endif
                                    targetsIter = (*missionsIter)->targets.erase(targetsIter);
                                    continue;
                                }
                            }
                        }
                        ++targetsIter;
                    }

                    if ((*missionsIter)->targets.empty())
                    {
                        missionsIter = removeMission_(missionsIter);
                        continue;
                    }
                }

#ifdef ALTAI_DEBUG
                (*missionsIter)->debug(os);
#endif
                ++missionsIter;
            }

            std::map<LandUnitCategories, std::set<IDInfo> >::const_iterator landCombatUnitsIter = units_.find(Combat);
            std::map<int, std::set<IDInfo> > unassignedUnitsBySubarea;
            if (landCombatUnitsIter != units_.end())
            {
                for (std::set<IDInfo>::const_iterator iter(landCombatUnitsIter->second.begin()), endIter(landCombatUnitsIter->second.end());
                    iter != endIter; ++iter)
                {
                    std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator missionIter = unitMissionMap_.find(*iter);
                    if (missionIter == unitMissionMap_.end())
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
                for (std::set<IDInfo>::const_iterator ci(uIter->second.begin()), ciEnd(uIter->second.end()); ci != ciEnd; ++ci)
                {
                    const CvUnit* pUnit = ::getUnit(*ci);
                    os << "\n\t" << *ci << " " << pUnit->getUnitInfo().getType() << " at: " << pUnit->plot()->getCoords();
                }
            }
#endif

            for (std::map<XYCoords, int>::const_iterator si(stacks.begin()), siEnd(stacks.end()); si != siEnd; ++si)
            {
#ifdef ALTAI_DEBUG
                os << "\nStack at: " << si->first << " danger value = " << si->second;
#endif
                const CvPlot* pStackPlot = gGlobals.getMap().plot(si->first.iX, si->first.iY);
                if (!pStackPlot->isWater())
                {
                    const CvPlot* pClosestCityPlot = player_.getAnalysis()->getMapAnalysis()->getClosestCity(pStackPlot, pStackPlot->getSubArea(), false);
                    if (pClosestCityPlot)
                    {
#ifdef ALTAI_DEBUG
                        os << " closest city: " << safeGetCityName(pClosestCityPlot->getPlotCity());
#endif
                    }
                }

                MilitaryMissionDataPtr pMission;
                for (size_t i = 0, count = enemyStacks_[si->first].size(); i < count; ++i)
                {
                    std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionDataIter = unitMissionMap_.find(enemyStacks_[si->first][i]->getIDInfo());
                    if (missionDataIter == unitMissionMap_.end())
                    {
                        pMission = addHostileUnitMission_(pStackPlot->getSubArea(), si->first, enemyStacks_[si->first][i]->getIDInfo());
                    }
                    else
                    {
                        pMission = missionDataIter->second;
                    }
                }

                //if (!pStackPlot->isWater())
                {
                    pMission->pClosestCity = player_.getAnalysis()->getMapAnalysis()->getClosestCity(pStackPlot, pStackPlot->getSubArea(), false)->getPlotCity();
                    RequiredUnits requiredUnits = getRequiredLandAttackStack(player_, landCombatUnits, pStackPlot, enemyStacks_[si->first], unassignedUnitsBySubarea[pStackPlot->getSubArea()]);
                    for (size_t i = 0, count = requiredUnits.existingUnits.size(); i < count; ++i)
                    {
                        const CvUnit* pUnit = ::getUnit(requiredUnits.existingUnits[i]);
                        if (pUnit)
                        {
                            pMission->assignedUnits.insert(requiredUnits.existingUnits[i]);
                        }
                    }
                    pMission->requiredUnits = requiredUnits.unitsToBuild;

                    pMission->reachablePlots = getReachablePlots(player_, pStackPlot, enemyStacks_[si->first]);
#ifdef ALTAI_DEBUG
                    os << "\nThreatens: " << pMission->reachablePlots.size() << " plots: ";
                    os << " closest city: " << safeGetCityName(pMission->pClosestCity);
                    
                    for (std::set<const CvPlot*>::const_iterator ci(pMission->reachablePlots.begin()), ciEnd(pMission->reachablePlots.end()); ci != ciEnd; ++ci)
                    {
                        if ((*ci)->getOwner() == player_.getPlayerID())
                        {
                            os << (*ci)->getCoords() << ", ";
                            if ((*ci)->isCity())
                            {
                                os << " city!: " << safeGetCityName((*ci)->getPlotCity());
                            }
                        }
                    }
#endif
                }
            }
        }

        void addUnit(CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            const bool isLandUnit = pUnit->getDomainType() == DOMAIN_LAND;
            if (isLandUnit)
            {
                UnitAITypes unitAIType = pUnit->AI_getUnitAIType();
                LandUnitCategories category = getCategory_(unitAIType);
                units_[category].insert(pUnit->getIDInfo());
                unitCategoryMap_[pUnit->getIDInfo()] = category;
#ifdef ALTAI_DEBUG
                os << "\nAdded land unit: " << pUnit->getIDInfo() << " "
                   << gGlobals.getUnitInfo(pUnit->getUnitType()).getType()
                   << " with category: " << category;
#endif
            }

            const CvPlot* pPlot = pUnit->plot();
            int areaId = isLandUnit ? pPlot->getArea() : (pPlot->isCity() && pPlot->isCoastalLand() ? pPlot->getPlotCity()->getArea() : pPlot->getArea());

            MilitaryMissionDataPtr pUnitMission;

            if (pPlot->isCity() && pUnit->canFight())
            {
                const CvCity* pCity = pPlot->getPlotCity();
                std::map<IDInfo, std::list<IDInfo> >::iterator guardMissionsIter = cityGuardMissionList_.find(pCity->getIDInfo());
                if (guardMissionsIter == cityGuardMissionList_.end())
                {
                    cityGuardMissionMap_[pUnit->getIDInfo()] = pCity->getIDInfo();
                    cityGuardMissionList_[pCity->getIDInfo()].push_back(pUnit->getIDInfo());
                    pUnitMission = addDefendCityUnitMission_(pUnit, pCity);
#ifdef ALTAI_DEBUG
                    os << "\nAssigned unit to guard city: " << safeGetCityName(pCity);
#endif
                }
                else
                {
                    UnitPlotIterP<OurUnitsPred> ourUnitsIter(pPlot, OurUnitsPred(player_.getPlayerID()));

                    while (CvUnit* pPlotUnit = ourUnitsIter())
                    {
                        if (pPlotUnit->AI_getUnitAIType() == UNITAI_SETTLE)
                        {
                            pUnitMission = addEscortUnitMission_(pPlotUnit);
                            if (pUnitMission && pUnitMission->assignedUnits.empty())
                            {
                                pUnitMission->assignedUnits.insert(pUnit->getIDInfo());
#ifdef ALTAI_DEBUG
                                os << "\nAssigned unit to escort settler from: " << safeGetCityName(pCity);
                                pUnitMission->debug(os);
#endif
                                break;
                            }
                        }
                    }
                }
            }

            if (!pUnitMission)
            {
                pUnitMission = assignUnitToCombatMission_(pUnit);

                if (!pUnitMission)
                {
                    boost::shared_ptr<MapAnalysis> pMapAnalysis = player_.getAnalysis()->getMapAnalysis();
                    if (!pMapAnalysis->isAreaComplete(areaId))
                    {
                        pUnitMission = addScoutUnitMission_(pUnit);
                    }
                }
            }

#ifdef ALTAI_DEBUG
            if (pUnitMission)
            {
                os << "\nAssigned unit to mission structure";
                pUnitMission->debug(os);

            }
            else
            {
                os << "\nFailed to assign unit to mission";
            }
#endif
        }

        void deleteUnit(CvUnit* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            if (pUnit->getDomainType() == DOMAIN_LAND)
            {
                IDInfo info = pUnit->getIDInfo();
                std::map<IDInfo, LandUnitCategories>::iterator categoryIter = unitCategoryMap_.find(info);
                if (categoryIter != unitCategoryMap_.end())
                {
                    units_[categoryIter->second].erase(info);
                    unitCategoryMap_.erase(categoryIter);
#ifdef ALTAI_DEBUG
                    os << "\nDeleted land combat unit from analysis: "
                       << gGlobals.getUnitInfo(pUnit->getUnitType()).getType()
                       << " with category: " << categoryIter->second;
#endif
                }
                else
                {
#ifdef ALTAI_DEBUG
                    os << "\nError deleting land combat unit from analysis: "
                       << gGlobals.getUnitInfo(pUnit->getUnitType()).getType();
#endif
                }

                for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
                {
                    std::set<IDInfo>::iterator targetsIter((*missionsIter)->targets.find(info));
                    if (targetsIter != (*missionsIter)->targets.end())
                    {
#ifdef ALTAI_DEBUG
                        os << "\nDeleting target unit: " << info << " from mission";
#endif
                        (*missionsIter)->targets.erase(targetsIter);
                        if ((*missionsIter)->targets.empty())
                        {
                            removeMission_(missionsIter);
                            break;  // unit should only be in one mission
                        }
                    }
                }
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
                MilitaryMissionDataPtr pMission;
                std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionDataIter = unitMissionMap_.find(pUnit->getIDInfo());
                if (missionDataIter == unitMissionMap_.end())
                {
                    pMission = addHostileUnitMission_(pPlot->getSubArea(), pPlot->getCoords(), pUnit->getIDInfo());
                }
                else
                {
                    pMission = missionDataIter->second;
                }
                std::set<const CvPlot*> threatenedPlots = getReachablePlots(player_, pPlot, std::vector<const CvUnit*>(1, pUnit));
                pMission->reachablePlots.insert(threatenedPlots.begin(), threatenedPlots.end());
            }
        }

        void deletePlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot)
        {
            eraseUnit_(pUnit->getIDInfo(), pPlot);
            unitHistories_.erase(pUnit->getIDInfo());
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionDataIter = unitMissionMap_.find(pUnit->getIDInfo());
            if (missionDataIter != unitMissionMap_.end())
            {
                missionDataIter->second->targets.erase(pUnit->getIDInfo());
                unitMissionMap_.erase(missionDataIter);
            }
        }

        void movePlayerUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            PlayerTypes plotOwner = pToPlot->getOwner();
            if (::atWar(player_.getTeamID(), pUnit->getTeam()))
            {
                if (plotOwner == player_.getPlayerID())
                {
#ifdef ALTAI_DEBUG
                    os << "\nHostile unit in terrority - " << gGlobals.getUnitInfo(pUnit->getUnitType()).getType() 
                       << " (" << pUnit->getOwner() << ") at: " << pFromPlot->getCoords();
#endif
                }
                else if (plotOwner == NO_PLAYER)
                {
#ifdef ALTAI_DEBUG
                    os << "\nHostile unit in unowned terrority - " << gGlobals.getUnitInfo(pUnit->getUnitType()).getType() 
                       << " at: " << pToPlot->getCoords();
#endif
                }
                else
                {
#ifdef ALTAI_DEBUG
                    os << "\nHostile unit in another player's terrority - " << gGlobals.getUnitInfo(pUnit->getUnitType()).getType() 
                       << " (" << pUnit->getOwner() << ") at: " << pToPlot->getCoords();
#endif
                }
            }
            else
            {                
#ifdef ALTAI_DEBUG
                os << "\n" << (player_.getPlayerID() == pUnit->getOwner() ? "Our " : "Non-hostile ")
                   << "unit - " << gGlobals.getUnitInfo(pUnit->getUnitType()).getType() 
                   << " (" << pUnit->getOwner() << ") at: " << pToPlot->getCoords();
#endif
            }

            moveUnit_(pUnit->getIDInfo(), pFromPlot, pToPlot);            
        }

        void hidePlayerUnit(CvUnitAI* pUnit, const CvPlot* pOldPlot)
        {
            // todo - treat differently from unit deletion? - add to list of untracked units?
            eraseUnit_(pUnit->getIDInfo(), pOldPlot);
            hideUnit_(pUnit->getIDInfo(), pOldPlot);
        }

        bool updateUnit(CvUnitAI* pUnit)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            CvPlot* pUnitPlot = pUnit->plot();

            for (std::map<XYCoords, std::vector<const CvUnit*> >::const_iterator iter(enemyStacks_.begin()), endIter(enemyStacks_.end()); iter != endIter; ++iter)
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

            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = unitMissionMap_.find(pUnit->getIDInfo());
            if (missionIter != unitMissionMap_.end())
            {
                std::set<const CvPlot*> ourReachablePlots = getReachablePlots(player_, pUnitPlot, std::vector<const CvUnit*>(1, pUnit));
                std::set<const CvPlot*> sharedPlots, borderPlots;

                getOverlapAndNeighbourPlots(missionIter->second->reachablePlots, ourReachablePlots, sharedPlots, borderPlots);
#ifdef ALTAI_DEBUG
                os << "\nUnit is assigned mission against targets: ";
                for (std::set<IDInfo>::iterator targetsIter(missionIter->second->targets.begin()), targetsEndIter(missionIter->second->targets.end());
                    targetsIter != targetsEndIter; ++targetsIter)
                {
                    os << *targetsIter << ", ";
                }
                os << " attackable plots: " << sharedPlots.size() << ", bordering plots: " << borderPlots.size();
#endif
                if (missionIter->second->missionType == MISSIONAI_EXPLORE)
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
            }
            return false;
        }

        UnitTypes getUnitRequestBuild(const CvCity* pCity, const TacticSelectionData& tacticSelectionData)
        {
#ifdef ALTAI_DEBUG
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity->getOwner()))->getStream();
            os << "\nChecking unit builds for: " << narrow(pCity->getName());
#endif
            for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.cityAttackUnits.begin()), ciEnd(tacticSelectionData.cityAttackUnits.end()); ci != ciEnd; ++ci)
            {
#ifdef ALTAI_DEBUG
                os << "\nUnit: " << gGlobals.getUnitInfo(ci->unitType).getType() << ", turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue
                    << ", count = " << pPlayer->getUnitCount(ci->unitType);
#endif
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

            std::map<UnitTypes, size_t> unitRequestCounts;
            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                for (size_t i = 0, count = (*missionsIter)->requiredUnits.size(); i < count; ++i)
                {
                    int assignedCount = (*missionsIter)->getAssignedUnitCount(player_.getCvPlayer(), (*missionsIter)->requiredUnits[i].unitType);
                    int requiredCount = (*missionsIter)->getRequiredUnitCount((*missionsIter)->requiredUnits[i].unitType);
                    if (assignedCount < requiredCount)
                    {
                        unitRequestCounts[(*missionsIter)->requiredUnits[i].unitType] += requiredCount - assignedCount;
                    }
                }
            }

            UnitTypes mostRequestedUnit = NO_UNIT;
            size_t mostRequestedCount = 0;
            for (std::map<UnitTypes, size_t>::const_iterator uIter(unitRequestCounts.begin()), uEndIter(unitRequestCounts.end()); uIter != uEndIter; ++uIter)
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

            return mostRequestedUnit;
        }

        std::map<const CvPlot*, std::vector<const CvUnit*> > getNearbyHostileStacks(const CvPlot* pPlot, int range)
        {
            std::map<const CvPlot*, std::vector<const CvUnit*> > stacks;

            for (std::map<XYCoords, std::vector<const CvUnit*> >::const_iterator si(enemyStacks_.begin()), siEnd(enemyStacks_.end()); si != siEnd; ++si)
            {
                const CvPlot* pHostilePlot = gGlobals.getMap().plot(si->first.iX, si->first.iY);
                // only looks at water-water or land-land combinations
                if (pHostilePlot->isWater() == pPlot->isWater() && stepDistance(pPlot->getX(), pPlot->getY(), si->first.iX, si->first.iY) <= range)
                {
                    stacks.insert(std::make_pair(pHostilePlot, si->second));
                }
            }

            return stacks;
        }

        std::set<const CvPlot*> getThreatenedPlots() const
        {
            std::set<const CvPlot*> plots;
            for (std::list<MilitaryMissionDataPtr>::const_iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                plots.insert((*missionsIter)->reachablePlots.begin(), (*missionsIter)->reachablePlots.end());
            }

            return plots;
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

            for (std::map<IDInfo, MilitaryMissionDataPtr>::const_iterator missionIter(unitMissionMap_.begin()), endIter(unitMissionMap_.end());
                missionIter != endIter; ++missionIter)
            {
                if (missionIter->second->missionType == MISSIONAI_EXPLORE)
                {
                    if (missionIter->second->plotTarget != XYCoords(-1, -1))
                    {
                        const CvPlot* pTargetPlot = gGlobals.getMap().plot(missionIter->second->plotTarget.iX, missionIter->second->plotTarget.iY);
                        if (!pTargetPlot->isWater())
                        {
                            subAreaRefPlotsMap[pTargetPlot->getSubArea()].insert(missionIter->second->plotTarget);
                        }
                    }
                }
            }
            return subAreaRefPlotsMap;
        }

    private:

        LandUnitCategories getCategory_(UnitAITypes unitAIType) const
        {
            switch (unitAIType)
            {
            case UNITAI_SETTLE:
                return Settler;
            case UNITAI_WORKER:
                return Worker;
            case UNITAI_EXPLORE:
                return Scout;
            case UNITAI_ATTACK:
            case UNITAI_ATTACK_CITY:
            case UNITAI_COLLATERAL:
            case UNITAI_PILLAGE:
            case UNITAI_RESERVE:
            case UNITAI_COUNTER:
            case UNITAI_CITY_DEFENSE:
            case UNITAI_CITY_COUNTER:
                return Combat;
            case UNITAI_MISSIONARY:
                return Missionary;
            case UNITAI_SCIENTIST:
            case UNITAI_PROPHET:
            case UNITAI_ARTIST:
            case UNITAI_ENGINEER:
            case UNITAI_MERCHANT:
                return GreatPerson;
            default:
                return Unknown;
            }
        }

        void eraseUnit_(const IDInfo& info, const CvPlot* pPlot)
        {
            unitStacks_[pPlot->getSubArea()][pPlot->getCoords()].erase(info);
            if (unitStacks_[pPlot->getSubArea()][pPlot->getCoords()].empty())
            {
                unitStacks_[pPlot->getSubArea()].erase(pPlot->getCoords());
            }
            unitsMap_[pPlot->getSubArea()].erase(info);            
        }

        XYCoords addUnit_(const IDInfo& info, const CvPlot* pPlot)
        {
            unitsMap_[pPlot->getSubArea()][info] = pPlot->getCoords();
            unitStacks_[pPlot->getSubArea()][pPlot->getCoords()].insert(info);
            return addHistory_(info, pPlot);
        }

        void moveUnit_(const IDInfo& info, const CvPlot* pFromPlot, const CvPlot* pToPlot)
        {
            addUnit_(info, pToPlot);
            if (pFromPlot)
            {
                eraseUnit_(info, pFromPlot);
            }
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
            std::map<int, std::map<XYCoords, std::set<IDInfo> > >::const_iterator stackAreaIter = unitStacks_.find(subArea);
            if (stackAreaIter != unitStacks_.end())
            {
                std::map<XYCoords, std::set<IDInfo> >::const_iterator stackIter = stackAreaIter->second.find(currentCoords);
                if (stackIter != stackAreaIter->second.end())
                {
                    for (std::set<IDInfo>::const_iterator unitsIter = stackIter->second.begin(), unitsEndIter = stackIter->second.end(); unitsIter != unitsEndIter; ++unitsIter)
                    {
                        if (*unitsIter != info)
                        {
                            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = unitMissionMap_.find(*unitsIter);
                            if (missionIter != unitMissionMap_.end())
                            {
                                missionIter->second->targets.insert(info);
                                return missionIter->second;
                            }
                        }
                    }
                }
            }

            // new mission data object
            MilitaryMissionDataPtr pMissionData(new MilitaryMissionData(MISSIONAI_COUNTER));
            pMissionData->targets.insert(info);
            addMission_(pMissionData, info);

            return pMissionData;
        }

        MilitaryMissionDataPtr addEscortUnitMission_(const CvUnit* pUnit)
        {
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = unitMissionMap_.find(pUnit->getIDInfo());
            if (missionIter == unitMissionMap_.end())
            {
                if (pUnit->AI_getUnitAIType() == UNITAI_SETTLE)
                {
                    MilitaryMissionDataPtr pMissionData(new MilitaryMissionData(MISSIONAI_ESCORT));
                    pMissionData->targets.insert(pUnit->getIDInfo());
                    addMission_(pMissionData, pUnit->getIDInfo());

                    return pMissionData;
                }
                else
                {
                    return MilitaryMissionDataPtr();
                }
            }
            else
            {
                return missionIter->second;
            }
        }

        MilitaryMissionDataPtr addScoutUnitMission_(const CvUnitAI* pUnit)
        {
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = unitMissionMap_.find(pUnit->getIDInfo());
            if (missionIter == unitMissionMap_.end())
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
                            MilitaryMissionDataPtr pMissionData(new MilitaryMissionData(MISSIONAI_EXPLORE));
                            pMissionData->assignedUnits.insert(pUnit->getIDInfo());
                            pMissionData->plotTarget = refCoords;
                            addMission_(pMissionData, pUnit->getIDInfo());
                            return pMissionData;
                        }
                    }
                    else if (unitDomain == DOMAIN_SEA)
                    {
                        // todo - track number of sea explore units
                        MilitaryMissionDataPtr pMissionData(new MilitaryMissionData(MISSIONAI_EXPLORE));
                        pMissionData->assignedUnits.insert(pUnit->getIDInfo());
                        addMission_(pMissionData, pUnit->getIDInfo());
                        return pMissionData;
                    }

                    return MilitaryMissionDataPtr();
                }
                else
                {
                    return MilitaryMissionDataPtr();
                }
            }
            else
            {
                return missionIter->second;
            }
        }

        MilitaryMissionDataPtr addDefendCityUnitMission_(const CvUnit* pUnit, const CvCity* pCity)
        {
            std::map<IDInfo, MilitaryMissionDataPtr>::iterator missionIter = unitMissionMap_.find(pUnit->getIDInfo());
            if (missionIter == unitMissionMap_.end())
            {
                UnitAITypes unitAIType = pUnit->AI_getUnitAIType();
                if (pUnit->baseCombatStr() > 0)
                {
                    MilitaryMissionDataPtr pMissionData(new MilitaryMissionData(MISSIONAI_GUARD_CITY));
                    pMissionData->assignedUnits.insert(pUnit->getIDInfo());
                    pMissionData->targets.insert(pCity->getIDInfo());
                    addMission_(pMissionData, pUnit->getIDInfo());

                    return pMissionData;
                }
                else
                {
                    return MilitaryMissionDataPtr();
                }
            }
            else
            {
                return missionIter->second;
            }
        }

        MilitaryMissionDataPtr assignUnitToCombatMission_(const CvUnitAI* pUnit)
        {
            const int subArea = pUnit->plot()->getSubArea();
            std::map<int, std::map<IDInfo, XYCoords> >::const_iterator unitsAreaIter = unitsMap_.find(subArea);
            if (unitsAreaIter == unitsMap_.end())
            {
                return MilitaryMissionDataPtr();  //assert this? shouldn't ever happen as we should at least have our own unit?
            }            

            for (std::list<MilitaryMissionDataPtr>::iterator missionsIter(missionList_.begin()); missionsIter != missionList_.end(); ++missionsIter)
            {
                if (!((*missionsIter)->missionType == MISSIONAI_COUNTER))
                {
                    continue;
                }

                for (std::set<IDInfo>::const_iterator targetsIter((*missionsIter)->targets.begin()); targetsIter != (*missionsIter)->targets.end(); ++targetsIter)
                {
                    // need a way to ask if the mission relates to this sub area
                    if (unitsAreaIter->second.find(*targetsIter) != unitsAreaIter->second.end())
                    {
                        int requiredCount = (*missionsIter)->getRequiredUnitCount(pUnit->getUnitType());
                        if (requiredCount > 0)
                        {
                            int assignedCount = (*missionsIter)->getAssignedUnitCount(player_.getCvPlayer(), pUnit->getUnitType());
                            if (assignedCount < requiredCount)
                            {
                                (*missionsIter)->assignedUnits.insert(pUnit->getIDInfo());
                                unitMissionMap_[pUnit->getIDInfo()] = *missionsIter;
                                return *missionsIter;
                            }                        
                        }
                    }
                }
            }
            return MilitaryMissionDataPtr();
        }

        std::list<MilitaryMissionDataPtr>::iterator removeMission_(const std::list<MilitaryMissionDataPtr>::iterator& missionsIter)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nRemoving mission ";
#endif
            (*missionsIter)->requiredUnits.clear();
            for (std::set<IDInfo>::iterator assignedUnitsIter((*missionsIter)->assignedUnits.begin()),
                assignedUnitsEndIter((*missionsIter)->assignedUnits.end()); assignedUnitsIter != assignedUnitsEndIter;
                ++assignedUnitsIter)
            {
                CvUnitAI* pUnit = (CvUnitAI*)player_.getCvPlayer()->getUnit(assignedUnitsIter->iID);
                if (pUnit)
                {
                    // try and find a new mission
                    MilitaryMissionDataPtr pMission = assignUnitToCombatMission_(pUnit);
                    if (pMission)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nReassigned unit to mission structure";
                        pMission->debug(os);
#endif
                    }
                }
            }
            return missionList_.erase(missionsIter);
        }

        void addMission_(const MilitaryMissionDataPtr pMission, IDInfo unit)
        {
            unitMissionMap_.insert(std::make_pair(unit, pMission));
            missionList_.push_back(pMission);
        }

        void getOddsAgainstHostiles_(const CvCity* pCity, const UnitTacticValue& unit) const
        {
            const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
            int subArea = pCity->plot()->getSubArea();
            std::set<int> waterSubAreas;

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity->getOwner()))->getStream();
#endif
            if (pCity->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
            {
                NeighbourPlotIter iter(pCity->plot());

                while (IterPlot pLoopPlot = iter())
                {
                    if (pLoopPlot.valid() && pLoopPlot->isWater())
                    {
                        waterSubAreas.insert(pLoopPlot->getSubArea());
                    }
                }
            }

            std::map<int, std::map<IDInfo, XYCoords> >::const_iterator areaIter = unitsMap_.find(subArea);
            if (areaIter != unitsMap_.end())
            {
                for (std::map<IDInfo, XYCoords>::const_iterator ci(areaIter->second.begin()), ciEnd(areaIter->second.end()); ci != ciEnd; ++ci)
                {
                    UnitData::CombatDetails combatDetails;
                    CvUnit* pUnit = CvPlayerAI::getPlayer(ci->first.eOwner).getUnit(ci->first.iID);
                    if (pUnit && pUnit->canFight())
                    {
#ifdef ALTAI_DEBUG
                        //os << "\nChecking unit odds v. " << gGlobals.getUnitInfo(pUnit->getUnitType()).getType();
#endif
                        UnitData us(unit.unitType);
                        player.getAnalysis()->getUnitAnalysis()->promote(us, combatDetails, true, unit.level, Promotions());
                        std::vector<int> odds = player.getAnalysis()->getUnitAnalysis()->getOdds(us, 
                            std::vector<UnitData>(1, UnitData(pUnit)), combatDetails, true);
#ifdef ALTAI_DEBUG
                        //os << " odds = " << odds[0];
#endif
                    }
                }
            }
        }

        Player& player_;
        std::map<IDInfo, LandUnitCategories> unitCategoryMap_;
        std::map<LandUnitCategories, std::set<IDInfo> > units_;

        std::map<IDInfo, std::map<UnitTypes, int> > cityBuildsTimes_;

        // {sub area, {unit idinfo, coords}}
        std::map<int, std::map<IDInfo, XYCoords> > unitsMap_, hiddenUnitsMap_;
        std::map<int, std::map<XYCoords, std::set<IDInfo> > > unitStacks_;
        std::map<int, std::set<UnitTypes> > seenForeignUnits_;

        std::map<XYCoords, std::vector<const CvUnit*> > enemyStacks_;

        std::map<IDInfo, UnitHistory> unitHistories_;

        std::map<IDInfo, MilitaryMissionDataPtr> unitMissionMap_;        
        std::list<MilitaryMissionDataPtr> missionList_;

        // unit id -> city id
        std::map<IDInfo, IDInfo> cityGuardMissionMap_;
        // city id -> units
        std::map<IDInfo, std::list<IDInfo> > cityGuardMissionList_;
    };


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

    bool MilitaryAnalysis::updateUnit(CvUnitAI* pUnit)
    {
        return pImpl_->updateUnit(pUnit);
    }

    UnitTypes MilitaryAnalysis::getUnitRequestBuild(const CvCity* pCity, const TacticSelectionData& tacticSelectionData)
    {
        return pImpl_->getUnitRequestBuild(pCity, tacticSelectionData);
    }

    void MilitaryAnalysis::addUnit(CvUnitAI* pUnit)
    {
        pImpl_->addUnit(pUnit);
    }

    void MilitaryAnalysis::deleteUnit(CvUnit* pUnit)
    {
        pImpl_->deleteUnit(pUnit);
    }

    void MilitaryAnalysis::movePlayerUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot)
    {
        pImpl_->movePlayerUnit(pUnit, pFromPlot, pToPlot);
    }

    void MilitaryAnalysis::hidePlayerUnit(CvUnitAI* pUnit, const CvPlot* pOldPlot)
    {
        pImpl_->hidePlayerUnit(pUnit, pOldPlot);
    }

    void MilitaryAnalysis::addPlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot)
    {
        pImpl_->addPlayerUnit(pUnit, pPlot);
    }

    void MilitaryAnalysis::deletePlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot)
    {
        pImpl_->deletePlayerUnit(pUnit, pPlot);
    }

    std::set<const CvPlot*> MilitaryAnalysis::getThreatenedPlots() const
    {
        return pImpl_->getThreatenedPlots();
    }

    std::map<int /* sub area */, std::set<XYCoords> > MilitaryAnalysis::getLandScoutMissionRefPlots() const
    {
        return pImpl_->getLandScoutMissionRefPlots();
    }

    std::set<const CvPlot*> getReachablePlots(const Player& player, const CvPlot* pStackPlot, const std::vector<const CvUnit*>& unitStack)
    {
        std::set<const CvPlot*> plots;
        std::map<UnitMovementData, std::vector<const CvUnit*> > unitMovementMap;
        bool canAttack = false;
        DomainTypes stackDomain = NO_DOMAIN;
        for (size_t i = 0, count = unitStack.size(); i < count; ++i)
        {
            unitMovementMap[UnitMovementData(unitStack[i])].push_back(unitStack[i]);
            canAttack = canAttack || unitStack[i]->canAttack();

            DomainTypes unitDomain = unitStack[i]->getDomainType();            
            FAssert(stackDomain == NO_DOMAIN || stackDomain == unitDomain);  // no mixed domain stacks
            stackDomain = unitDomain;
        }
        FAssert(stackDomain == DOMAIN_LAND || stackDomain == DOMAIN_SEA);

        const CvTeamAI& unitsTeam = CvTeamAI::getTeam(unitStack[0]->getTeam());

        for (std::map<UnitMovementData, std::vector<const CvUnit*> >::const_iterator mIter(unitMovementMap.begin()), mEndIter(unitMovementMap.end());
            mIter != mEndIter; ++mIter)
        {
            std::map<const CvPlot*, int> reachablePlots;
            std::set<const CvPlot*> openList;
            openList.insert(pStackPlot);
            plots.insert(pStackPlot);

            reachablePlots[pStackPlot] = mIter->second[0]->maxMoves();

            while (!openList.empty())
            {
                const CvPlot* pPlot = *openList.begin();
                NeighbourPlotIter plotIter(pPlot, 1, 1);

                while (IterPlot pLoopPlot = plotIter())
                {
                    // todo - switch on whether we include plots where we need to attack
                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(player.getTeamID(), false) &&
                        pLoopPlot->isValidDomainForLocation(*mIter->second[0]) && (canAttack || !pLoopPlot->isVisibleEnemyUnit(mIter->second[0])))
                    {
                        // todo - factor out the esoteric cases in canMoveInto() (and handle those separately, e.g. spies), and handle basic land/sea movement logic in
                        // a more optimised function. canMoveInto() is ~300 lines long!
                        if (!mIter->second[0]->canMoveInto(pLoopPlot, false))
                        {
                            continue;
                        }
                        int cost = stackDomain == DOMAIN_LAND ? landMovementCost(mIter->first, pPlot, pLoopPlot, unitsTeam) : seaMovementCost(mIter->first, pPlot, pLoopPlot, unitsTeam);
                        int thisMoveMovesLeft = std::max<int>(0, reachablePlots[pPlot] - cost);
                            
                        std::map<const CvPlot*, int>::iterator openPlotsIter = reachablePlots.find(pLoopPlot);
                        if (openPlotsIter != reachablePlots.end())
                        {
                            // already got here cheaper
                            int currentMoveMovesLeft = openPlotsIter->second;
                            if (currentMoveMovesLeft >= thisMoveMovesLeft)
                            {
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

                        if (openPlotsIter->second > 0)
                        {
                            openList.insert(pLoopPlot);                                
                        }
                        plots.insert(pLoopPlot);
                    }
                }
                openList.erase(pPlot);
            }
        }

        return plots;
    }

    std::map<const CvPlot*, std::vector<const CvUnit*> > getNearbyHostileStacks(Player& player, const CvPlot* pPlot, int range)
    {
        boost::shared_ptr<MilitaryAnalysis> pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
        return pMilitaryAnalysis->getImpl()->getNearbyHostileStacks(pPlot, range);
    }

    std::set<XYCoords> getUnitHistory(Player& player, IDInfo unit)
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
        return pMilitaryAnalysis->updateUnit(pUnit);
    }
}