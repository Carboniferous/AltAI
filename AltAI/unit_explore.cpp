#include "AltAI.h"

#include "./unit_explore.h"
#include "./sub_area.h"
#include "./iters.h"
#include "./unit.h"
#include "./unit_log.h"
#include "./game.h"
#include "./player.h"
#include "./helper_fns.h"
#include "./settler_manager.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./unit_tactics.h"
#include "./military_tactics.h"

#include "FAStarNode.h"

namespace AltAI
{
    namespace
    {
        int getNeighbourInvisiblePlotCount(const CvPlot* pPlot, const TeamTypes teamType)
        {
            NeighbourPlotIter iter(pPlot);

            int invisibleCount = 0;
            while (IterPlot pLoopPlot = iter())
            {
                if (!pLoopPlot.valid()) continue;
                if (!pLoopPlot->isRevealed(teamType, false))
                {
                    ++invisibleCount;
                }
            }
            return invisibleCount;
        }

        bool hasInvisibleNeighbourPlots(const CvPlot* pPlot, const TeamTypes teamType)
        {
            NeighbourPlotIter iter(pPlot);

            int invisibleCount = 0;
            while (IterPlot pLoopPlot = iter())
            {
                if (!pLoopPlot.valid()) continue;
                if (!pLoopPlot->isRevealed(teamType, false))
                {
                    return true;
                }
            }
            return false;
        } 

        bool hasInvisibleSecondNeighbourPlots(const CvPlot* pPlot, const TeamTypes teamType)
        {
            NeighbourPlotIter iter(pPlot);

            int invisibleCount = 0;
            while (IterPlot pLoopPlot = iter())
            {
                if (!pLoopPlot.valid()) continue;
                if (pLoopPlot->isRevealed(teamType, false))
                {
                    if (hasInvisibleNeighbourPlots(pLoopPlot, teamType))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        int getVisibleFromCoastCount(const CvPlot* pPlot, const TeamTypes teamType, const int visibilityRange)
        {
            NeighbourPlotIter iter(pPlot, pPlot->seeFromLevel(teamType) + visibilityRange, pPlot->seeFromLevel(teamType) + visibilityRange);
            int visibleCount = 0;
            while (IterPlot pLoopPlot = iter())
            {
                if (!pLoopPlot.valid()) continue;
                // can this coastal plot (that we know) see passed in plot?
                if (pLoopPlot->isRevealed(teamType, false) && pLoopPlot->isCoastalLand() && pLoopPlot->canSeePlot((CvPlot*)pPlot, teamType, visibilityRange, NO_DIRECTION))
                {
                    ++visibleCount;  // count up each known coastal plot which can view passed in plot
                }
            }
            return visibleCount;
        }

        int getCoastalUniqueVisibleCount(const CvPlot* pPlot, const TeamTypes teamType, const int visibilityRange)
        {
            NeighbourPlotIter iter(pPlot, pPlot->seeFromLevel(teamType) + visibilityRange, pPlot->seeFromLevel(teamType) + visibilityRange);

            int uniqueCount = 0;
            while (IterPlot pLoopPlot = iter())
            {
                if (!pLoopPlot.valid()) continue;
                if (!pLoopPlot->isRevealed(teamType, false))
                {
                    if (pPlot->canSeePlot(pLoopPlot, teamType, visibilityRange, NO_DIRECTION))
                    {
                        // is this plot only visible from this one coastal spot (as far as we know)?
                        if (getVisibleFromCoastCount(pLoopPlot, teamType, visibilityRange) == 1) ++uniqueCount;
                    }
                }
            }
            return uniqueCount;
        }

        bool isTargetted(const std::vector<UnitMissionPtr>& missions, const CvPlot* pPlot)
        {
            bool isTargetted = false;
            for (size_t i = 0, count = missions.size(); !isTargetted && i < count; ++i)
            {
                isTargetted = missions[i]->getTarget() == pPlot;
            }
            return isTargetted;
        }

        CvPlot* getClosestUntargettedPlot(const std::vector<UnitMissionPtr>& missions, const std::vector<XYCoords>& possibleTargets, const CvPlot* pOurPlot)
        {
            int closestTargetDistance = MAX_INT;
            CvPlot* pClosestTarget = NULL;
            for (size_t i = 0, count = possibleTargets.size(); i < count; ++i)
            {
                int thisTargetDistance = plotDistance(pOurPlot->getX(), pOurPlot->getY(), possibleTargets[i].iX, possibleTargets[i].iY);
                CvPlot* pTargetPlot = gGlobals.getMap().plot(possibleTargets[i].iX, possibleTargets[i].iY);

                if (!isTargetted(missions, pTargetPlot) && thisTargetDistance <= closestTargetDistance)
                {
                    closestTargetDistance = thisTargetDistance;
                    pClosestTarget = pTargetPlot;
                }
            }
            return pClosestTarget;
        }

        CvPlot* getClosestBestFoundValuePlot(PlayerTypes playerType, const std::vector<UnitMissionPtr>& missions, const ReachablePlotsData& reachablePlotsData, const CvPlot* pOurPlot)
        {
            int maxFoundValue = 0, maxInvisibleNeighbourCount = 0;
            CvPlot* pBestPlot = NULL;
            //std::map<int, const CvPlot*> plotsByFoundValue;
            for (PlotSet::const_iterator ci(reachablePlotsData.allReachablePlots.begin()), ciEnd(reachablePlotsData.allReachablePlots.end()); ci != ciEnd; ++ci)
            {
                CvPlot* thisPlot = (CvPlot*)(*ci);
                if (pOurPlot->getCoords() == thisPlot->getCoords())
                {
                    continue;
                }

                int thisFoundValue = thisPlot->getFoundValue(playerType);
                if (thisFoundValue > maxFoundValue)
                {
                    maxFoundValue = thisFoundValue;
                    pBestPlot = thisPlot;
                }
                else if (thisFoundValue == maxFoundValue)
                {
                    int invisibleNeighbourCount = getNeighbourInvisiblePlotCount(thisPlot, PlayerIDToTeamID(playerType));
                    if (invisibleNeighbourCount > maxInvisibleNeighbourCount)
                    {
                        maxInvisibleNeighbourCount = invisibleNeighbourCount;
                        pBestPlot = thisPlot;
                    }
                }
            }
            return pBestPlot;
        }

        CvPlot* getBestBFCInitialMove(const CvUnit* pOurUnit, const std::vector<UnitMissionPtr>& missions, const ReachablePlotsData& reachablePlotsData, const CvPlot* pOurPlot, const CvPlot* pInitialPlot)
        {
            static const int MOVE_DENOMINATOR = gGlobals.getMOVE_DENOMINATOR();
            CultureRangePlotIter plotIter(pInitialPlot, (CultureLevelTypes)2);
            CvPlot* pBestPlot = NULL;
            int bestValue = 0;

            for (PlotSet::const_iterator ci(reachablePlotsData.allReachablePlots.begin()), ciEnd(reachablePlotsData.allReachablePlots.end()); ci != ciEnd; ++ci)
            {
                if (*ci == pOurPlot) continue;
                if (plotDistance((*ci)->getX(), (*ci)->getY(), pInitialPlot->getX(), pInitialPlot->getY()) > 2) continue;

                UnitMovementDataMap::const_iterator ui = reachablePlotsData.unitMovementDataMap.find(*ci);
                std::map<const CvUnit*, int, CvUnitIDInfoOrderF>::const_iterator uci = ui->second.find(pOurUnit);
                int thisValue = ((8 * uci->second) + (MOVE_DENOMINATOR * getNeighbourInvisiblePlotCount(*ci, pOurUnit->getTeam()))) / MOVE_DENOMINATOR;
                if (thisValue > bestValue)
                {
                    bestValue = thisValue;
                    pBestPlot = (CvPlot*)*ci;
                }
            }

            /*while (IterPlot pLoopPlot = plotIter())
            {
                if (!pLoopPlot.valid())
                {
                    continue;
                }

                PlotSet::const_iterator ci = reachablePlotsData.allReachablePlots.find(pLoopPlot);
                if (ci != reachablePlotsData.allReachablePlots.end())
                {
                    UnitMovementDataMap::const_iterator ui = reachablePlotsData.unitMovementDataMap.find(pLoopPlot);
                    if (ui != reachablePlotsData.unitMovementDataMap.end())
                    {
                        std::map<const CvUnit*, int, CvUnitIDInfoOrderF>::const_iterator uci = ui->second.find(pOurUnit);
                        if (uci != ui->second.end())
                        {
                            int thisValue = ((8 * uci->second) + (MOVE_DENOMINATOR * getNeighbourInvisiblePlotCount(pLoopPlot, pOurUnit->getTeam()))) / MOVE_DENOMINATOR;
                            if (thisValue > bestValue)
                            {
                                bestValue = thisValue;
                                pBestPlot = pLoopPlot;
                            }
                        }
                    }
                }
            }*/
            return pBestPlot;
        }

        struct LandExplorePred
        {
            explicit LandExplorePred(const CvArea* pArea) : areaID(pArea->getID())
            {
            }

            explicit LandExplorePred(int areaID_) : areaID(areaID_)
            {
            }

            bool operator() (const UnitMissionPtr& pMission) const
            {
                return pMission->getAIType() == UNITAI_EXPLORE && pMission->getTarget()->area()->getID() == areaID;
            }

            const int areaID;
        };

        struct ExplorePlotData
        {
            ExplorePlotData(TeamTypes teamID, const CvPlot* pPlot_, const CvCity* pTargetCity, 
                const PlotUnitsMap& nearbyHostiles) : pPlot(pPlot_)
            {
                isCity = pPlot->isCity(false, teamID) && isFriendlyCity(teamID, pPlot->getPlotCity());
                isRevealed = pPlot->isRevealed(teamID, false);
                minSafePlotDistance = pTargetCity ? stepDistance(pPlot->getX(), pPlot->getY(), pTargetCity->getX(), pTargetCity->getY()) : MAX_INT;

                minHostileDistance = MAX_INT;
                for (PlotUnitsMap::const_iterator hIter(nearbyHostiles.begin()), hEndIter(nearbyHostiles.end());
                    hIter != hEndIter; ++hIter)
                {
                    minHostileDistance = std::min<int>(minHostileDistance, stepDistance(pPlot->getX(), pPlot->getY(), hIter->first->getX(), hIter->first->getY()));
                }
            }

            bool operator < (const ExplorePlotData& other) const
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
                os << " explore plot data: " << pPlot->getCoords() << " is city: " << isCity << " r = " << isRevealed
                    << ", h = " << minHostileDistance << ", s = " << minSafePlotDistance;
            }

            bool isCity, isRevealed;
            int minHostileDistance, minSafePlotDistance;
            const CvPlot* pPlot;
        };
    }

    class LandExploreMission : public IUnitMission
    {
    public:
        explicit LandExploreMission(CvUnitAI* pUnit) : pUnit_(pUnit), targetPlot_(NULL), isComplete_(false), maxPlotDistance_(gGlobals.getMap().maxPlotDistance()), maxPlotDeviation_(5)
        {
            setTarget_(false);
        }

        virtual ~LandExploreMission() {}

        virtual void update()
        {
            // todo - upgrade if possible
            if (pUnit_->getDamage() > 0)
            {
                const FeatureTypes featureType = pUnit_->plot()->getFeatureType();
                if ((featureType == NO_FEATURE) || (gGlobals.getFeatureInfo(featureType).getTurnDamage() == 0))
                {
                    pUnit_->getGroup()->pushMission(MISSION_HEAL, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                    return;
                }
            }

            if (doExplore_())
            {
                return;
            }

            if (doBorderPatrol_())
            {
                return;
            }

            isComplete_ = true;
            pUnit_->getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
        }

        virtual const CvUnitAI* getUnit() const
        {
            return pUnit_;
        }

        virtual CvPlot* getTarget() const
        {
            return targetPlot_;
        }

        virtual bool isComplete() const
        {
            return isComplete_;
        }

        virtual CvCity* getTargetCity() const
        {
            return targetPlot_ ? targetPlot_->getPlotCity() : NULL;
        }

        virtual UnitAITypes getAIType() const
        {
            return UNITAI_EXPLORE;
        }

        virtual void write(FDataStreamBase* pStream) const { pStream->Write(ID); }
        virtual void read(FDataStreamBase* pStream) {}

        static const int ID = 1;

    private:
        void setTarget_(bool ignoreSelf)
        {
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner());
            boost::shared_ptr<MapAnalysis> pMapAnalysis = pPlayer->getAnalysis()->getMapAnalysis();
            CityIter cityIter(CvPlayerAI::getPlayer(pUnit_->getOwner()));
            const TeamTypes teamType = pUnit_->getTeam();
            const CvPlot* pUnitPlot = pUnit_->plot();

            std::vector<CvPlot*> referencePlots;
            while (CvCity* pCity = cityIter())
            {
                if (pCity->getArea() == pUnit_->area()->getID())
                {
                    referencePlots.push_back(pCity->plot());
                }
            }

            // ignore ourself as fine to keep the same target coords if they are valid
            std::vector<UnitMissionPtr> matchingMissions = pPlayer->getMissions(LandExplorePred(pUnit_->area()), ignoreSelf ? NULL : pUnit_);

            // find first ref plot which is not a mission target
            for (size_t i = 0, count = referencePlots.size(); i < count; ++i)
            {
                if (std::find_if(matchingMissions.begin(), matchingMissions.end(), HasTarget(referencePlots[i])) == matchingMissions.end())
                {
                    targetPlot_ = referencePlots[i];
                    break;
                }
            }

            if (!targetPlot_)
            {
                std::vector<CvPlot*> ignorePlots;

                for (;;)
                {
                    // target best city site not already targetted
                    CvPlot* pTarget = pPlayer->getSettlerManager()->getBestPlot(pUnitPlot->getSubArea(), ignorePlots);
                    if (!pTarget)
                    {
                        // no good sites
                        break;
                    }

                    if (std::find_if(matchingMissions.begin(), matchingMissions.end(), HasTarget(pTarget)) == matchingMissions.end())
                    {
                        targetPlot_ = pTarget;
                        break;
                    }
                    else
                    {
                        ignorePlots.push_back(pTarget);
                    }
                }
            }

            const std::map<int /* sub area id */, std::set<XYCoords> >& borders = pMapAnalysis->getUnrevealedBorderMap();
            std::map<int, std::set<XYCoords> >::const_iterator subAreaBorderIter = borders.find(pUnit_->plot()->getSubArea());

            int closestBorderDistance = MAX_INT;
            // find (plot) distance closest border plot to current target coords
            for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end());
                ci != ciEnd; ++ci)
            {
                // distance from border plot to ourself
                int thisPlotBorderDistance = plotDistance(pUnitPlot->getX(), pUnitPlot->getY(), ci->iX, ci->iY);
                if (thisPlotBorderDistance <= closestBorderDistance)
                {                    
                    targetPlot_ = gGlobals.getMap().plot(ci->iX, ci->iY);
                    closestBorderDistance = thisPlotBorderDistance;
                }
            }
            
            // found no targets to explore
            if (!targetPlot_)
            {
                isComplete_ = true;
            }

#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
            os << "\nexplore: target = ";
            if (targetPlot_)
            {
                os << targetPlot_->getCoords();
            }
            else
            {
                os << "NULL";
            }
#endif
        }

        bool doExplore_()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
#endif
            boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getAnalysis()->getMapAnalysis();
            int minimumRefDistance = MAX_INT, closestBorderDistance = MAX_INT, closestGoodHutDistance = MAX_INT;
            CvPlot* pBestPlot = NULL, *pUnitPlot = pUnit_->plot();

            // closest border plot to us which is still 'close' (< maxPlotDeviation_, currently) to the source explore plot
            // and overall closest border plot from source plot
            CvPlot* pClosestClosestBorderPlot = NULL, *pClosestBorderPlotToTarget = NULL, *pBestGoodyHutPlot = NULL, *pBestFoundValuePlot = NULL;

            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner());

            // including attackable plots - maybe limit this? (already takes into account units which can't attack)
            ReachablePlotsData reachablePlotsData;
            getReachablePlotsData(reachablePlotsData, *pPlayer, std::vector<const CvUnit*>(1, pUnit_), false, true);
            std::set<XYCoords> ourHistory = getUnitHistory(*pPlayer, pUnit_->getIDInfo());

            // find closest unrevealed plot in same area as unit to closest reference point
            // todo - handle sea units in port
            const std::map<int /* sub area id */, std::set<XYCoords> >& borders = pMapAnalysis->getUnrevealedBorderMap();
            std::map<int, std::set<XYCoords> >::const_iterator subAreaBorderIter = borders.find(pUnitPlot->getSubArea());

            std::vector<XYCoords> goodyHuts = pMapAnalysis->getGoodyHuts(pUnitPlot->getSubArea());

            const bool haveBorders = subAreaBorderIter != borders.end() && !subAreaBorderIter->second.empty();

            if (!haveBorders && goodyHuts.empty())
            {
                return false;
            }

            const bool noCities = pPlayer->getCvPlayer()->getNumCities() == 0;

            if (noCities)
            {
                // get other scout units in this area
                std::vector<UnitMissionPtr> areaExploreMissions = pPlayer->getMissions(LandExplorePred(pUnit_->area()), pUnit_);
                // 
                pBestFoundValuePlot = getBestBFCInitialMove(pUnit_, areaExploreMissions, reachablePlotsData, pUnit_->plot(), pPlayer->getCvPlayer()->getStartingPlot());
                if (!pBestFoundValuePlot)
                {
                    pBestFoundValuePlot = getClosestBestFoundValuePlot(pUnit_->getOwner(), areaExploreMissions, reachablePlotsData, pUnit_->plot());
                }
            }

            // check we aren't already targetted on a hut
            if (!goodyHuts.empty() && (!targetPlot_ || !targetPlot_->isGoody()))
            {
                // get other scout units in this area
                std::vector<UnitMissionPtr> areaExploreMissions = pPlayer->getMissions(LandExplorePred(pUnit_->area()), pUnit_);
                pBestGoodyHutPlot = getClosestUntargettedPlot(areaExploreMissions, goodyHuts, pUnitPlot);
            }
            
            // find (plot) distance closest border plot to current target coords
            for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end());
                ci != ciEnd; ++ci)
            {   
                // distance from border plot to target
                int thisPlotDistance = plotDistance(targetPlot_->getX(), targetPlot_->getY(), ci->iX, ci->iY);
                if (thisPlotDistance <= minimumRefDistance)
                {                    
                    CvPlot* pBorderPlot = gGlobals.getMap().plot(ci->iX, ci->iY);
                    if (pUnit_->canMoveInto(pBorderPlot))  // ignore plots we can't enter
                    {
                        minimumRefDistance = thisPlotDistance;
                        pClosestBorderPlotToTarget = pBorderPlot;
                    }
                }
            }

            if (minimumRefDistance == MAX_INT)
            {
                // if have no accessible border plots - still want to consider moves which reveal additional tiles
                // even if they may involve a longer path to reach
                minimumRefDistance = maxPlotDistance_;  
            }

            // now find closest border plot to our unit which is within maxPlotDeviation_ of the closest border plot to target
            for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end());
                ci != ciEnd; ++ci)
            {
                int thisPlotDistance = plotDistance(targetPlot_->getX(), targetPlot_->getY(), ci->iX, ci->iY);
                // border plots which no more than maxPlotDeviation_ away from closest border plot to target coords
                if (thisPlotDistance <= minimumRefDistance + maxPlotDeviation_)  
                {
                    // of these plots, find the closest border plot to our unit
                    int thisDistanceToUs = plotDistance(pUnitPlot->getX(), pUnitPlot->getY(), ci->iX, ci->iY);
                    if (thisDistanceToUs < closestBorderDistance)
                    {
                        pClosestClosestBorderPlot = gGlobals.getMap().plot(ci->iX, ci->iY);
                        closestBorderDistance = thisDistanceToUs;
                    }
                }
            }

            PlotSet reachableBorderPlots, reachableHostilePlots;
            
            for (PlotSet::const_iterator ci(reachablePlotsData.allReachablePlots.begin()), ciEnd(reachablePlotsData.allReachablePlots.end()); ci != ciEnd; ++ci)
            {
                bool reachablePlotIsBorderPlot = haveBorders && subAreaBorderIter->second.find((*ci)->getCoords()) != subAreaBorderIter->second.end();
                if (reachablePlotIsBorderPlot)
                {
                    reachableBorderPlots.insert(*ci);
                }

                PlayerTypes plotOwner = (*ci)->getOwner();
                if (plotOwner != NO_PLAYER && CvTeamAI::getTeam(pPlayer->getTeamID()).isAtWar(PlayerIDToTeamID(plotOwner)))
                {
                    if (hasInvisibleNeighbourPlots(*ci, pPlayer->getTeamID()))
                    {
                        reachableHostilePlots.insert(*ci);
                    }
                }

                // also add coastal plots we've not visited that could reveal new plots (effectively proxy border plots)
                if (!reachablePlotIsBorderPlot && (*ci)->isCoastalLand() && ourHistory.find((*ci)->getCoords()) == ourHistory.end()
                    && hasInvisibleSecondNeighbourPlots(*ci, pUnit_->getTeam()))
                {
                    // todo - check if visiting this plot can reveal more plots
                    reachableBorderPlots.insert(*ci);
                }
            }

#ifdef ALTAI_DEBUG
            os << "\nexplore border plots:";
            //if (pBestPlot) os << " best plot = " << pBestPlot->getCoords(); else os << " best plot = NULL";
            if (pClosestClosestBorderPlot) os << " closest close border plot = " << pClosestClosestBorderPlot->getCoords(); else os << " closest close border plot = NULL";
            if (pClosestBorderPlotToTarget) os << " closest border plot to target = " << pClosestBorderPlotToTarget->getCoords(); else os << " closest close border plot to target = NULL";
            if (pBestGoodyHutPlot) os << " closest hut plot to target = " << pBestGoodyHutPlot->getCoords();
            if (pBestFoundValuePlot) os << " best found value plot to target: " << pBestFoundValuePlot->getCoords();
#endif
            bool pushedMission = false;
            if (pBestFoundValuePlot)
            {
                pushedMission = pushMoveToMission_(pBestFoundValuePlot);
#ifdef ALTAI_DEBUG
                os << " - set best found value plot as target";
#endif
                targetPlot_ = pBestFoundValuePlot;
            }
            else if (pBestGoodyHutPlot)
            {
                const CvPlot* pMoveToPlot = getNextMovePlot(*pPlayer, pUnit_->getGroup(), pBestGoodyHutPlot);
                if (pMoveToPlot)
                {
                    pushedMission = pushMoveToMission_(pMoveToPlot);
#ifdef ALTAI_DEBUG
                    os << " - set goody hut as target";
#endif
                    targetPlot_ = pBestGoodyHutPlot;  // set target so avoid other scouts heading for same hut
                }
            }

            if (!pushedMission && !reachableBorderPlots.empty() || !reachableHostilePlots.empty())
            {
                int bestValue = 0;

                for (PlotSet::const_iterator ci(reachableHostilePlots.begin()), ciEnd(reachableHostilePlots.end()); ci != ciEnd; ++ci)
                {
                    int thisValue = getNeighbourInvisiblePlotCount(*ci, pPlayer->getTeamID());
                    if (thisValue > bestValue)
                    {
                        bestValue = thisValue;
                        pBestPlot = (CvPlot*)*ci;
                    }
                }

                if (reachableHostilePlots.empty())
                {
                    for (PlotSet::const_iterator ci(reachableBorderPlots.begin()), ciEnd(reachableBorderPlots.end()); ci != ciEnd; ++ci)
                    {
                        if (*ci == pUnitPlot)
                        {
                            continue;
                        }

                        int thisPlotDistance = plotDistance(targetPlot_->getX(), targetPlot_->getY(), (*ci)->getX(), (*ci)->getY());
                        // maxPlotDeviation_ - is max deviation from minimum distance from target -
                        // try to force exploration in circular fashion around target - todo - make maxPlotDeviation_ depend on map size?
                        if (thisPlotDistance > minimumRefDistance + maxPlotDeviation_)
                        {
                            continue;
                        }
                        int thisValue = maxPlotDistance_ - thisPlotDistance;
                        thisValue += getNeighbourInvisiblePlotCount(*ci, pPlayer->getTeamID());
                        if ((*ci)->isGoody(pPlayer->getTeamID()))
                        {
                            thisValue += 10;
                        }
                        if ((*ci)->isHills())
                        {
                            ++thisValue;
                        }
                        if ((*ci)->isCoastalLand())
                        {
                            thisValue += 2;
                            // plots which we can only reveal from this plot (according to our current view)
                            // essentially encourages more complete coastal exploration - so we don't end up not spotting ocean fish
                            int uniqueVisCount = getCoastalUniqueVisibleCount(*ci, pUnit_->getTeam(), pUnit_->visibilityRange());
                            if (uniqueVisCount > 0)
                            {
                                thisValue *= 2;
#ifdef ALTAI_DEBUG
                                os << "\n\tplot: " << (*ci)->getCoords() << " has: " << uniqueVisCount << " plots which it reveals";
#endif
                            }
                        }

                        if (thisValue > bestValue)
                        {
                            bestValue = thisValue;
                            pBestPlot = (CvPlot*)*ci;
                        }
                    }
                }

                const CvPlot* pMoveToPlot = NULL;
                if (pBestPlot)
                {                    
                    pMoveToPlot = getNextMovePlot(*pPlayer, pUnit_->getGroup(), pBestPlot);
                }
                else if (pClosestClosestBorderPlot)
                {
                    pMoveToPlot = getNextMovePlot(*pPlayer, pUnit_->getGroup(), pClosestClosestBorderPlot);
                }
                else if (pClosestBorderPlotToTarget)
                {
                    pMoveToPlot = getNextMovePlot(*pPlayer, pUnit_->getGroup(), pClosestBorderPlotToTarget);
                }

                if (pMoveToPlot)
                {
                    pushedMission = pushMoveToMission_(pMoveToPlot);
                }

            }
            
            if (!pushedMission)
            {
                if (pClosestClosestBorderPlot && pUnit_->canMoveInto(pClosestClosestBorderPlot))
                {
                    pushedMission = pushMoveToMission_(pClosestClosestBorderPlot);
                }
                else if (pClosestBorderPlotToTarget && pUnit_->canMoveInto(pClosestBorderPlotToTarget))
                {
                    pushedMission = pushMoveToMission_(pClosestBorderPlotToTarget);
                }
            }

            if (!pushedMission && !reachableBorderPlots.empty())
            {
                pUnit_->getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                pushedMission = true;
            }

            return pushedMission;
        }

        bool doBorderPatrol_()
        {
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner());
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*pPlayer->getCvPlayer())->getStream();
#endif
            int minimumRefDistance = MAX_INT, closestBorderDistance = MAX_INT;
            CvPlot* pBestPlot = NULL, *pUnitPlot = pUnit_->plot();

            const std::map<int /* sub area id */, std::set<XYCoords> >& borders = pPlayer->getAnalysis()->getMapAnalysis()->getBorderMap();
            std::map<int, std::set<XYCoords> >::const_iterator subAreaBorderIter = borders.find(pUnitPlot->getSubArea());

            const bool haveBorders = subAreaBorderIter != borders.end() && !subAreaBorderIter->second.empty();

            if (!haveBorders)
            {
                return false;
            }
            
            std::vector<UnitMissionPtr> matchingMissions = pPlayer->getMissions(LandExplorePred(pUnit_->area()), pUnit_);
            SubAreaPlotIter plotIter(pUnit_->plot()->getSubArea());
            const TeamTypes teamType = pUnit_->getTeam();
            int bestPlotsValue = 0;
            CvPlot* pBestPatrolPlot = NULL;

            while (CvPlot* pPlot = plotIter())
            {
                if (pPlot->isRevealed(teamType, false) && pPlot->getRevealedOwner(teamType, false) == NO_PLAYER)
                {
                    if (std::find_if(matchingMissions.begin(), matchingMissions.end(), IsCloserThan(pPlot, 5)) != matchingMissions.end())
                    {
                        continue;
                    }
                    
                    NeighbourPlotIter neighbourIter(pPlot, pPlot->seeFromLevel(teamType) + pUnit_->visibilityRange(), pPlot->seeFromLevel(teamType) + pUnit_->visibilityRange());

                    int thisPlotsVisiblePlotsCount = 0;
                    while (IterPlot pNeighbourPlot = neighbourIter())
                    {
                        if (!pNeighbourPlot.valid() || pNeighbourPlot->isImpassable() || !pNeighbourPlot->isRevealed(teamType, false))
                        {
                            continue;
                        }

                        PlayerTypes neighbourPlotOwner = pNeighbourPlot->getRevealedOwner(teamType, false);
                        // don't count squares other players are fogbusting, unless it's barbs
                        if (neighbourPlotOwner != NO_PLAYER && neighbourPlotOwner != BARBARIAN_PLAYER)
                        {
                            continue;
                        }

                        if (pPlot->canSeePlot(pNeighbourPlot, teamType, pUnit_->visibilityRange(), NO_DIRECTION))
                        {
                            ++thisPlotsVisiblePlotsCount;
                        }
                    }

                    int closestDistanceToBorder = MAX_INT;
                    for (std::set<XYCoords>::const_iterator borderIter(subAreaBorderIter->second.begin()), borderEndIter(subAreaBorderIter->second.end());
                        borderIter != borderEndIter; ++borderIter)
                    {
                        int thisDistance = stepDistance(borderIter->iX, borderIter->iY, pPlot->getX(), pPlot->getY());
                        if (thisDistance < closestDistanceToBorder)
                        {
                            closestDistanceToBorder = thisDistance;
                        }
                    }
                    int thisPlotsValue = maxPlotDistance_ - closestDistanceToBorder + 2 * thisPlotsVisiblePlotsCount;

                    if (thisPlotsValue > bestPlotsValue)
                    {
                        bestPlotsValue = thisPlotsValue;
                        pBestPatrolPlot = pPlot;
                    }
                }
            }

#ifdef ALTAI_DEBUG
            os << "\n border patrol plot = ";
            
            if (pBestPatrolPlot) os << pBestPatrolPlot->getCoords();
            else os << " none ";

            os << " bestPlotsValue = " << bestPlotsValue;
#endif

            if (pBestPatrolPlot)
            {
                
                targetPlot_ = pBestPatrolPlot;

                if (pUnit_->atPlot(pBestPatrolPlot))
                {
                    pUnit_->getGroup()->pushMission(MISSION_SENTRY, pBestPatrolPlot->getX(), pBestPatrolPlot->getY(), 0, TRUE, false, MISSIONAI_EXPLORE, pBestPatrolPlot, 0, __FUNCTION__);
                    return true;
                }
                else
                {
                    const CvPlot* pMoveToPlot = getNextMovePlot(*pPlayer, pUnit_->getGroup(), pBestPatrolPlot);
                    if (pMoveToPlot)
                    {                                   
                        pUnit_->getGroup()->pushMission(MISSION_MOVE_TO, pMoveToPlot->getX(), pMoveToPlot->getY(), 
                            MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pMoveToPlot, 0, __FUNCTION__);                    
                        return true;
                    }
                }
            }

            return false;
        }

        bool pushMoveToMission_(const CvPlot* pDestPlot)
        {
            bool pushedMission = false;
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner());

            UnitPathData unitPathData;
            unitPathData.calculate(pUnit_->getGroup(), pDestPlot, MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY);

            if (unitPathData.valid)
            {
                const CvPlot* pMoveToPlot = getNextMovePlot(*pPlayer, pUnit_->getGroup(), pDestPlot);
                if (pMoveToPlot && !pUnit_->at(pMoveToPlot->getX(), pMoveToPlot->getY()))
                {
                    pUnit_->getGroup()->pushMission(MISSION_MOVE_TO, pMoveToPlot->getX(), pMoveToPlot->getY(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pMoveToPlot, 0, __FUNCTION__);
                    pushedMission = true;
                }
            }

            return pushedMission;
        }

        const CvUnitAI* pUnit_;
        CvCity* pReferenceCity_;
        CvPlot* targetPlot_;
        bool isComplete_;
        const int maxPlotDistance_, maxPlotDeviation_; // max plot distance used to consider value of exploring a plot v. one closer to source explore point
    };

    bool doLandUnitExplore(CvUnitAI* pUnit)
    {
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner());
        if (!pPlayer->hasMission(pUnit))
        {
            pPlayer->pushMission(pUnit, UnitMissionPtr(new LandExploreMission(pUnit)));
        }

        return pPlayer->executeMission(pUnit);
    }

    bool doCoastalUnitExplore(CvUnitAI* pUnit)
    {
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner());
        boost::shared_ptr<MapAnalysis> pMapAnalysis = pPlayer->getAnalysis()->getMapAnalysis();
        const CvPlot* pUnitPlot = pUnit->plot();
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*(pPlayer->getCvPlayer()))->getStream();
#endif
        const CvPlot* pClosestCityPlot = pMapAnalysis->getClosestCity(pUnitPlot, pUnitPlot->getSubArea(), true);
        ReachablePlotsData reachablePlotsData;
        getReachablePlotsData(reachablePlotsData, *pPlayer, std::vector<const CvUnit*>(1, pUnit), false, pUnit->canFight());
        PlotSet dangerPlots = pPlayer->getAnalysis()->getMilitaryAnalysis()->getThreatenedPlots();
        std::set<XYCoords> ourHistory = getUnitHistory(*pPlayer, pUnit->getIDInfo());

        const bool plotDanger = dangerPlots.find(pUnit->plot()) != dangerPlots.end();

        // todo - upgrade if possible
        if (pUnit->getDamage() > 0)  // doesn't apply to work boats
        {
            if (pClosestCityPlot)
            {
                UnitPathData unitPathData;
                unitPathData.calculate(pUnit->getGroup(), pClosestCityPlot, 0);

                if (unitPathData.valid)
                {
                    bool moveToPort = unitPathData.pathTurns < 2;
                    if (unitPathData.pathTurns < 3)
                    {
                        XYCoords endTurnPlot = unitPathData.getFirstTurnEndCoords();
                        const CvPlot* pEndTurnPlot = gGlobals.getMap().plot(endTurnPlot.iX, endTurnPlot.iY);
                        if (dangerPlots.find(pEndTurnPlot) == dangerPlots.end())
                        {
                            moveToPort = true;
                        }
                    }

                    if (moveToPort)
                    {
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pClosestCityPlot->getX_INLINE(), pClosestCityPlot->getY_INLINE(), 
                            MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pClosestCityPlot, 0, __FUNCTION__);
                        return true;
                    }
                }
            }
            const FeatureTypes featureType = pUnit->plot()->getFeatureType();
            if (!plotDanger && (featureType == NO_FEATURE || gGlobals.getFeatureInfo(featureType).getTurnDamage() == 0))
            {
                pUnit->getGroup()->pushMission(MISSION_HEAL, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                return true;
            }
        }

        if (plotDanger && !pUnit->canFight())
        {
            PlotUnitsMap hostiles = getNearbyHostileStacks(*pPlayer, pUnit->plot(), 2);                
            const CvPlot* pEscapePlot = getEscapePlot(*pPlayer, pUnit->getGroup(), reachablePlotsData.allReachablePlots, dangerPlots, hostiles);
            if (pEscapePlot && !pUnit->atPlot(pEscapePlot))
            {
#ifdef ALTAI_DEBUG
                os << "\ncoastal explore: trying closest port for healing: " << pClosestCityPlot->getCoords();
#endif
                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pEscapePlot->getX_INLINE(), pEscapePlot->getY_INLINE(), 
                    MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pEscapePlot, 0, __FUNCTION__);
                return true;
            }            
        }

        const std::map<int /* sub area id */, std::set<XYCoords> >& borders = pMapAnalysis->getUnrevealedBorderMap();
        std::vector<int> accessibleSubAreas;
        if (pUnitPlot->isCoastalLand())
        {
            for (PlotSet::const_iterator rIter(reachablePlotsData.allReachablePlots.begin()), rEndIter(reachablePlotsData.allReachablePlots.end()); rIter != rEndIter; ++rIter)
            {
                if ((*rIter)->isWater())
                {
                    if (std::find(accessibleSubAreas.begin(), accessibleSubAreas.end(), (*rIter)->getSubArea()) == accessibleSubAreas.end())
                    {
                        accessibleSubAreas.push_back((*rIter)->getSubArea());
                    }
                }
            }
        }
        else
        {
            accessibleSubAreas.push_back(pUnitPlot->getSubArea());
        }

        PlotSet reachableBorderPlots;
        for (size_t i = 0, count = accessibleSubAreas.size(); i < count; ++i)
        {
            std::map<int, std::set<XYCoords> >::const_iterator subAreaBorderIter = borders.find(accessibleSubAreas[i]);
            const bool haveBorders = subAreaBorderIter != borders.end() && !subAreaBorderIter->second.empty();
            
            for (PlotSet::const_iterator ci(reachablePlotsData.allReachablePlots.begin()), ciEnd(reachablePlotsData.allReachablePlots.end()); haveBorders && ci != ciEnd; ++ci)
            {
                PlayerTypes plotOwner = (*ci)->getRevealedOwner(pUnit->getTeam(), false);
                if (plotOwner == NO_PLAYER || plotOwner == pUnit->getOwner() || CvTeamAI::getTeam(PlayerIDToTeamID(plotOwner)).isOpenBorders(pUnit->getTeam()))
                {
                    if (subAreaBorderIter->second.find((*ci)->getCoords()) != subAreaBorderIter->second.end())
                    {
                        reachableBorderPlots.insert(*ci);
                    }
                }
            }
        }

        const CvPlot* pBestPlot = (const CvPlot*)0;
        if (!reachableBorderPlots.empty())
        {
            int bestValue = 0;                
            for (PlotSet::const_iterator ci(reachableBorderPlots.begin()), ciEnd(reachableBorderPlots.end()); ci != ciEnd; ++ci)
            {
                if (*ci == pUnitPlot)
                {
                    continue;
                }
                
                int thisValue = getNeighbourInvisiblePlotCount(*ci, pPlayer->getTeamID());                
                if (thisValue > bestValue)
                {
                    bestValue = thisValue;
                    pBestPlot = *ci;
                }
            }
#ifdef ALTAI_DEBUG
            os << "\ncoastal explore border plots:";
            if (pBestPlot) os << " best plot = " << pBestPlot->getCoords(); else os << " best plot = NULL";
#endif
            if (pBestPlot)
            {
                UnitPathData unitPathData;
                unitPathData.calculate(pUnit->getGroup(), pBestPlot, MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY);

                if (unitPathData.valid)
                {
                    const CvPlot* pMoveToPlot = getNextMovePlot(*pPlayer, pUnit->getGroup(), pBestPlot);
                    if (pMoveToPlot && !pUnit->at(pMoveToPlot->getX(), pMoveToPlot->getY()))
                    {
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMoveToPlot->getX(), pMoveToPlot->getY(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pMoveToPlot, 0, __FUNCTION__);
#ifdef ALTAI_DEBUG
                        os << "\npushed mission to plot: " << pBestPlot->getCoords();
#endif
                        return true;
                    }
                }
            }
        }

        for (size_t i = 0, count = accessibleSubAreas.size(); i < count; ++i)
        {
            std::map<int, std::set<XYCoords> >::const_iterator subAreaBorderIter = borders.find(accessibleSubAreas[i]);
            const bool haveBorders = subAreaBorderIter != borders.end() && !subAreaBorderIter->second.empty();
            if (haveBorders)
            {
                int bestValue = MAX_INT;
                PlotSet excludedPlots;
                for (;;)
                {
                    for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end());
                        ci != ciEnd; ++ci)
                    {
                        const CvPlot* pThisPlot = gGlobals.getMap().plot(ci->iX, ci->iY);
                        // could relax the second check to just same team
                        if (excludedPlots.find(pThisPlot) != excludedPlots.end() || (!pThisPlot->isAdjacentToLand() && pThisPlot->getOwner() != pUnit->getOwner()))
                        {
                            continue;
                        }

                        int thisPlotDistance = plotDistance(pUnit->getX(), pUnit->getY(), ci->iX, ci->iY);
                        if (thisPlotDistance <= bestValue)
                        {                    
                            pBestPlot = pThisPlot;
                            bestValue = thisPlotDistance;
                        }
                    }

                    if (!pBestPlot)
                    {
                        break;
                    }
#ifdef ALTAI_DEBUG
                    os << "\ncoastal explore: trying plot: " << pBestPlot->getCoords();
#endif
                    UnitPathData unitPathData;
                    unitPathData.calculate(pUnit->getGroup(), pBestPlot, MOVE_MAX_MOVES | MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY);

                    if (unitPathData.valid)
                    {
                        const CvPlot* pMoveToPlot = getNextMovePlot(*pPlayer, pUnit->getGroup(), pBestPlot);
                        if (pMoveToPlot && !pUnit->at(pMoveToPlot->getX(), pMoveToPlot->getY()))
                        {
                            pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMoveToPlot->getX(), pMoveToPlot->getY(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pMoveToPlot, 0, __FUNCTION__);
#ifdef ALTAI_DEBUG
                            os << "\npushed mission to plot: " << pBestPlot->getCoords();
#endif
                            return true;
                        }
                    }
                    else
                    {
#ifdef ALTAI_DEBUG
                        os << "\nfailed to generate path to plot, excluding it from search";
#endif
                        excludedPlots.insert(pBestPlot);
                        bestValue = MAX_INT;
                        pBestPlot = (const CvPlot*)0;
                    }
                }
            }
        }

        // just try and go to closest port
        if (pClosestCityPlot)
        {
            int iPathTurns = 0;
            if (pUnit->generatePath(pClosestCityPlot, 0, false, &iPathTurns))
            {
#ifdef ALTAI_DEBUG
                os << "\ncoastal explore: trying closest port: " << pClosestCityPlot->getCoords();
#endif
                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pClosestCityPlot->getX_INLINE(), pClosestCityPlot->getY_INLINE(), 
                    MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pClosestCityPlot, 0, __FUNCTION__);
                return true;
            }
        }

        pUnit->getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
        return true;
    }

    XYCoords getLandExploreRefPlot(const CvUnitAI* pUnit)
    {
        const PlayerTypes playerType = pUnit->getOwner();
        const TeamTypes teamType = pUnit->getTeam();
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType);
        CityIter cityIter(*pPlayer->getCvPlayer());
        const CvPlot* pUnitPlot = pUnit->plot();

        const std::map<int /* sub area id */, std::set<XYCoords> >& borders = pPlayer->getAnalysis()->getMapAnalysis()->getUnrevealedBorderMap();

        std::map<int /* sub area */, std::set<XYCoords> > subAreaRefPlotsMap = 
            pPlayer->getAnalysis()->getMilitaryAnalysis()->getLandScoutMissionRefPlots();

        int closestBorderDistance = MAX_INT;

        // todo - detect unreachable border plots (due to no open borders - only reason a plot in the same sub area would be inaccessible)
        std::map<int, std::set<XYCoords> >::const_iterator subAreaBorderIter = borders.find(pUnitPlot->getSubArea());
        if (subAreaBorderIter == borders.end())  // no unrevealed borders in this sub area...
        { // ...so look for another sub area
            XYCoords closestOtherAreaBorderCoords(-1, -1); 
            // pick nearest (coastal) plot
            for (subAreaBorderIter = borders.begin(); subAreaBorderIter != borders.end(); ++subAreaBorderIter)
            {
                boost::shared_ptr<SubArea> pSubArea = gGlobals.getMap().getSubArea(subAreaBorderIter->first);
                if (pSubArea->getID() != pUnitPlot->getSubArea() && !pSubArea->isWater())
                {
                    // find ref plots in this sub area
                    std::map<int, std::set<XYCoords> >::const_iterator missionAreaIter = subAreaRefPlotsMap.find(pSubArea->getAreaID());

                    // scale by length of border - up to max of 3 missions per area
                    if (missionAreaIter != subAreaRefPlotsMap.end() &&
                        missionAreaIter->second.size() > std::min<int>(3, 1 + subAreaBorderIter->second.size() / 4))
                    {
                        continue;
                    }

                    for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end()); ci != ciEnd; ++ci)
                    {
                        int thisPlotDistance = plotDistance(pUnitPlot->getX(), pUnitPlot->getY(), ci->iX, ci->iY);
                        if (thisPlotDistance < closestBorderDistance)
                        {
                            closestBorderDistance = thisPlotDistance;
                            closestOtherAreaBorderCoords = *ci;
                        }
                    }
                }
            }

            return closestOtherAreaBorderCoords;
        }
        else
        {
            // find closest city with no assigned exploration mission in this sub area
            std::map<int, std::set<XYCoords> >::const_iterator missionAreaIter = subAreaRefPlotsMap.find(pUnitPlot->getSubArea());

            std::vector<CvPlot*> referencePlots;
            while (CvCity* pCity = cityIter())
            {
                if (pCity->getArea() == pUnit->area()->getID())
                {
                    bool refPlotHasMission = false;
                    if (missionAreaIter != subAreaRefPlotsMap.end())
                    {
                        for (size_t i = 0, count = referencePlots.size(); i < count; ++i)
                        {
                            if (missionAreaIter->second.find(referencePlots[i]->getCoords()) != missionAreaIter->second.end())
                            {
                                refPlotHasMission = true;                            
                                break;
                            }
                        }
                    }
                    if (!refPlotHasMission)
                    {
                        referencePlots.push_back(pCity->plot());
                    }
                }
            }

            // todo - deal with case of multiple units with no cities
            if (!referencePlots.empty())
            {
                size_t refPlotIndex = 0;
                for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end()); ci != ciEnd; ++ci)
                {
                    for (size_t i = 0, count = referencePlots.size(); i < count; ++i)
                    {
                        int thisPlotDistance = plotDistance(referencePlots[i]->getX(), referencePlots[i]->getY(), ci->iX, ci->iY);
                        if (thisPlotDistance < closestBorderDistance)
                        {
                            closestBorderDistance = thisPlotDistance;
                            refPlotIndex = i;
                        }
                    }
                }

                return referencePlots[refPlotIndex]->getCoords();
            }
            else
            {
                return pUnitPlot->getCoords();
            }
        }
    }
}