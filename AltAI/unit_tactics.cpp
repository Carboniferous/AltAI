#include "AltAI.h"

#include "./unit_tactics.h"
#include "./city_unit_tactics.h"
#include "./military_tactics.h"
#include "./unit_tactics_visitors.h"
#include "./unit_info_visitors.h"
#include "./tech_info_visitors.h"
#include "./tactic_streams.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./gamedata_analysis.h"
#include "./settler_manager.h"
#include "./helper_fns.h"
#include "./unit_analysis.h"
#include "./tactic_selection_data.h"
#include "./civ_helper.h"
#include "./civ_log.h"
#include "./iters.h"

namespace AltAI
{
    namespace
    {
        struct EscapePlotData
        {
            EscapePlotData(TeamTypes teamID, const CvPlot* pPlot_, const CvPlot* pTargetCityPlot, 
                const std::map<const CvPlot*, std::vector<const CvUnit*> >& nearbyHostiles) : pPlot(pPlot_)
            {
                isCity = pPlot->isCity(false, teamID) && isFriendlyCity(teamID, pPlot->getPlotCity());
                isRevealed = pPlot->isRevealed(teamID, false);
                minSafePlotDistance = pTargetCityPlot ? stepDistance(pPlot->getX(), pPlot->getY(), pTargetCityPlot->getX(), pTargetCityPlot->getY()) : MAX_INT;

                minHostileDistance = MAX_INT;
                for (std::map<const CvPlot*, std::vector<const CvUnit*> >::const_iterator hIter(nearbyHostiles.begin()), hEndIter(nearbyHostiles.end());
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
                        bool techDepsSatisfied = ci->second->areDependenciesSatisfied(player, IDependentTactic::Ignore_Techs);

                        if (pCity)
                        {
                            CityUnitTacticsPtr pCityTactics = ci->second->getCityTactics(pCity->getIDInfo());
                            depsSatisfied = pCityTactics && depsSatisfied && pCityTactics->areDependenciesSatisfied(IDependentTactic::Ignore_None);
                            techDepsSatisfied = pCityTactics && techDepsSatisfied && pCityTactics->areDependenciesSatisfied(IDependentTactic::Ignore_Techs);
                        }

                        bool couldConstruct = (pCity ? couldConstructUnit(player, player.getCity(pCity->getID()), 0, pUnitInfo, false) : couldConstructUnit(player, 0, pUnitInfo, false));
#ifdef ALTAI_DEBUG
                        if (domainType != DOMAIN_LAND)
                        {
                            os << " " << (pCity ? narrow(pCity->getName()) : "(none)");
                            os << " deps = " << depsSatisfied << ", could construct = " << couldConstruct << " tech deps: " << techDepsSatisfied
                                << " potential construct: " << couldConstructUnit(player, 3, player.getAnalysis()->getUnitInfo(ci->first), true);
                        }
#endif
                        if (depsSatisfied)
                        {
                            combatUnits.push_back(ci->first);
                            possibleCombatUnits.push_back(ci->first);
                        }
                        else if (couldConstructUnit(player, 3, player.getAnalysis()->getUnitInfo(ci->first), true))
                        {
                            possibleCombatUnits.push_back(ci->first);
                        }
                    }
                }
            }
        }

        return std::make_pair(combatUnits, possibleCombatUnits);
    }

    bool getSpecialistBuild(const PlayerTactics& playerTactics, CvUnitAI* pUnit)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*playerTactics.player.getCvPlayer())->getStream();
#endif
        PlayerTactics::UnitTacticsMap::const_iterator unitIter = playerTactics.specialUnitTacticsMap_.find(pUnit->getUnitType());
        TacticSelectionData selectionData;
        
        if (unitIter != playerTactics.specialUnitTacticsMap_.end())
        {
            unitIter->second->update(playerTactics.player);
            unitIter->second->apply(selectionData);
#ifdef ALTAI_DEBUG
            os << "\nSpec selection tactics: ";
            unitIter->second->debug(os);
            os << "\nSpec selection data:";
            selectionData.debug(os);
#endif
            TotalOutputWeights weights = makeOutputW(3, 4, 3, 3, 1, 1);
            TotalOutputValueFunctor valueF(weights);
            int bestBuildingValue = 0;
            IDInfo bestBuildingCity;
            BuildingTypes bestBuilding = NO_BUILDING;

            for (std::multiset<EconomicBuildingValue>::const_iterator ci(selectionData.economicBuildings.begin()), ciEnd(selectionData.economicBuildings.end()); ci != ciEnd; ++ci)
            {
                int thisValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                if (thisValue > bestBuildingValue)
                {
                    bestBuildingCity = ci->city;
                    bestBuildingValue = thisValue;
                    bestBuilding = ci->buildingType;
                }
#ifdef ALTAI_DEBUG
                os << "\n(Economic Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType()
                    << " city: " << narrow(::getCity(ci->city)->getName()) << " turns = " << ci->nTurns << ", delta = " << ci->output << " value = " << thisValue;
#endif
            }

            int bestSpecValue = 0;
            IDInfo bestSettleCity;
            SpecialistTypes bestSettledSpecialist = NO_SPECIALIST;
            for (std::multiset<SettledSpecialistValue>::const_iterator ci(selectionData.settledSpecialists.begin()), ciEnd(selectionData.settledSpecialists.end()); ci != ciEnd; ++ci)
            {
                int thisValue = valueF(ci->output);
                if (thisValue > bestSpecValue)
                {
                    bestSettleCity = ci->city;
                    bestSpecValue = thisValue;
                    bestSettledSpecialist = ci->specType;
                }
#ifdef ALTAI_DEBUG
                os << "\n(Settled Spec): " << gGlobals.getSpecialistInfo(ci->specType).getType()
                    << " city: " << narrow(::getCity(ci->city)->getName()) << ci->output << " value = " << thisValue;
#endif
            }

            int techValue = weights[OUTPUT_RESEARCH] * selectionData.freeTechValue * 30;
#ifdef ALTAI_DEBUG
            os << "\nBest building: " << (bestBuilding == NO_BUILDING ? " none " : gGlobals.getBuildingInfo(bestBuilding).getType())
                << " city: " << (bestBuildingCity == IDInfo() ? " none " : narrow(::getCity(bestBuildingCity)->getName()))
                << " value = " << bestBuildingValue
                << " best settle city: " << (bestSettleCity == IDInfo() ? " none " : narrow(::getCity(bestSettleCity)->getName()))
                << " value = " << bestSpecValue
                << " discover tech value: " << techValue;
#endif
            if (techValue > bestBuildingValue && techValue > bestSpecValue)
            {
                pUnit->getGroup()->pushMission(MISSION_DISCOVER);
                return true;
            }

            if (bestBuildingValue > bestSpecValue && bestBuildingValue > techValue)
            {
                CvPlot* pBestConstructPlot = ::getCity(bestBuildingCity)->plot();
                if (pUnit->atPlot(pBestConstructPlot))
		        {
			        pUnit->getGroup()->pushMission(MISSION_CONSTRUCT, bestBuilding);
			        return true;
		        }
		        else
		        {
			        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBestConstructPlot->getX(), pBestConstructPlot->getY(), 0, false, false, MISSIONAI_CONSTRUCT, pBestConstructPlot);
			        return true;
		        }
            }

            if (bestSpecValue > bestBuildingValue && bestSpecValue > techValue)
            {
                CvPlot* pBestSettlePlot = ::getCity(bestSettleCity)->plot();
                if (pUnit->atPlot(pBestSettlePlot))
		        {
			        pUnit->getGroup()->pushMission(MISSION_JOIN, bestSettledSpecialist);
			        return true;
		        }
		        else
		        {
			        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBestSettlePlot->getX(), pBestSettlePlot->getY());
			        return true;
		        }
            }
        }
        return false;
    }

    const CvPlot* getEscapePlot(const Player& player, CvUnit* pUnit, const std::set<const CvPlot*>& dangerPlots, const std::map<const CvPlot*, std::vector<const CvUnit*> >& nearbyHostiles)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        std::set<const CvPlot*> ourReachablePlots = getReachablePlots(player, pUnit->plot(), std::vector<const CvUnit*>(1, pUnit));

        const CvPlot* pTargetCityPlot = player.getAnalysis()->getMapAnalysis()->getClosestCity(pUnit->plot(), pUnit->plot()->getSubArea(), true);

        std::set<EscapePlotData> plotData;

        for (std::set<const CvPlot*>::const_iterator plotsIter(ourReachablePlots.begin()), plotsEndIter(ourReachablePlots.end()); plotsIter != plotsEndIter; ++plotsIter)
        {
            // todo: add check for friendly stacks
            if ((*plotsIter)->isCity(false, pUnit->getTeam()) || dangerPlots.find(*plotsIter) == dangerPlots.end())
            {
                plotData.insert(EscapePlotData(pUnit->getTeam(), *plotsIter, pTargetCityPlot, nearbyHostiles));
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
            os << "\ngetEscapePlot: stuck!! unit = " << pUnit->getIDInfo() << " " << pUnit->getUnitInfo().getType() << " at: " << pUnit->plot()->getCoords();
#endif
            return pUnit->plot();  // stuck
        }
    }
}