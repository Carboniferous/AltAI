#include "AltAI.h"

#include "./unit_explore.h"
#include "./sub_area.h"
#include "./iters.h"
#include "./unit.h"
#include "./unit_log.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./helper_fns.h"
#include "./settler_manager.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./military_tactics.h"

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
                if (!pLoopPlot.valid())
                {
                    continue;
                }
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

        std::vector<std::pair<CvPlot*, int> > getExplorePlotsValues(const CvUnitAI* pUnit, const CvPlot* pRefPlot, const int minUnrevealedDistanceFromRefPoint)
        {
            const CvPlot* pPlot = pUnit->plot();

            const TeamTypes teamType = pUnit->getTeam();
            const DomainTypes domainType = pUnit->getDomainType();
            const int unitMoves = pUnit->getUnitInfo().getMoves();

            std::vector<std::pair<CvPlot*, int> > plotValues;

            AreaPlotIter iter(pUnit->area());

            while (IterPlot pLoopPlot = iter())
            {
                if (!pLoopPlot.valid() || !pLoopPlot->isRevealed(teamType, false) || !pUnit->canMoveInto(pLoopPlot))
                {
                    continue;
                }

                int thisValue = 0;

                // always want goody huts
                if (pLoopPlot->isGoody(teamType))
                {
                    thisValue += 10;
                }
                else if (pRefPlot && stepDistance(pRefPlot->getX(), pRefPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY()) > 5 + minUnrevealedDistanceFromRefPoint)
                {
                    continue;
                }
                else if (stepDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY()) > 2)
                {
                    continue;
                }

                thisValue += getNeighbourInvisiblePlotCount(pLoopPlot, teamType);

                // todo - add value for plots owned by other civs which give contact (and for exploring other civs' lands)                
                if (thisValue == 0)
                {
                    continue;
                }

                if (unitMoves > 1)
                {
                    // favour plots where we only use up one move if we have more
                    if (pLoopPlot->movementCost(pUnit, pPlot) == 1)
                    {
                        ++thisValue;
                    }
                }

                // better visibility from hills
                if (pLoopPlot->isHills())
                {
                    ++thisValue;
                }

                const bool isWater = pLoopPlot->isWater();
                // try and favour revealing the coast
                if ((isWater && domainType == DOMAIN_LAND) || (!isWater && domainType == DOMAIN_SEA))
                {
                    ++thisValue;
                }

                plotValues.push_back(std::make_pair(pLoopPlot, thisValue));
            }

            int bestPlotValue = -1;
            for (size_t i = 0, count = plotValues.size(); i < count; ++i)
            {
                bestPlotValue = std::max<int>(plotValues[i].second, bestPlotValue);
            }

            return plotValues;
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
                const std::map<const CvPlot*, std::vector<const CvUnit*> >& nearbyHostiles) : pPlot(pPlot_)
            {
                isCity = pPlot->isCity(false, teamID) && isFriendlyCity(teamID, pPlot->getPlotCity());
                isRevealed = pPlot->isRevealed(teamID, false);
                minSafePlotDistance = pTargetCity ? stepDistance(pPlot->getX(), pPlot->getY(), pTargetCity->getX(), pTargetCity->getY()) : MAX_INT;

                minHostileDistance = MAX_INT;
                for (std::map<const CvPlot*, std::vector<const CvUnit*> >::const_iterator hIter(nearbyHostiles.begin()), hEndIter(nearbyHostiles.end());
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
        explicit LandExploreMission(CvUnitAI* pUnit) : pUnit_(pUnit), targetCoords_(NULL), isComplete_(false), minimumRefDistance_(MAX_INT)
        {
            setTarget_();
        }

        virtual ~LandExploreMission() {}

        virtual void update()
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
#endif
            // todo - upgrade if possible
            if (pUnit_->getDamage() > 0)
            {
                const FeatureTypes featureType = pUnit_->plot()->getFeatureType();
                if ((featureType == NO_FEATURE) || (gGlobals.getFeatureInfo(featureType).getTurnDamage() == 0))
                {
                    pUnit_->getGroup()->pushMission(MISSION_HEAL);
                    return;
                }
            }

            // set a new target plot
            if (minimumRefDistance_ != MAX_INT && minimumRefDistance_ > 10)
            {
                setTarget_();
            }

            if (isComplete_) return;

            const int maxPlotDistance = gGlobals.getMap().maxPlotDistance();
            const int maxPlotDeviation = 5;  // max plot distance used to consider value of exploring a plot v. one closer to source explore point
            int minimumRefDistance = MAX_INT, closestBorderDistance = MAX_INT;
            CvPlot* pBestPlot = NULL, *pUnitPlot = pUnit_->plot();
            // closest border plot to us which is still 'close' (< 5 plot distance, currently) to the source explore plot
            // and overall closest border plot from source plot
            CvPlot* pClosestClosestBorderPlot = NULL, *pClosestBorderPlotToTarget = NULL;

            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner());

            std::set<const CvPlot*> ourReachablePlots = getReachablePlots(*pPlayer, pUnitPlot, std::vector<const CvUnit*>(1, pUnit_));
            std::set<XYCoords> ourHistory = getUnitHistory(*pPlayer, pUnit_->getIDInfo());

            // find closest unrevealed plot in same area as unit to closest reference point
            // todo - handle sea units in port
            const std::map<int /* sub area id */, std::set<XYCoords> >& borders = 
                gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getAnalysis()->getMapAnalysis()->getUnrevealedBorderMap();
            std::map<int, std::set<XYCoords> >::const_iterator subAreaBorderIter = borders.find(pUnitPlot->getSubArea());

            const bool haveBorders = subAreaBorderIter != borders.end();
            
            for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end());
                ci != ciEnd; ++ci)
            {
                int thisPlotDistance = plotDistance(targetCoords_->getX(), targetCoords_->getY(), ci->iX, ci->iY);
                if (thisPlotDistance <= minimumRefDistance)
                {                    
                    pClosestBorderPlotToTarget = gGlobals.getMap().plot(ci->iX, ci->iY);
                    minimumRefDistance = thisPlotDistance;
                }
            }

            for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end());
                ci != ciEnd; ++ci)
            {
                int thisPlotDistance = plotDistance(targetCoords_->getX(), targetCoords_->getY(), ci->iX, ci->iY);
                if (thisPlotDistance <= minimumRefDistance + maxPlotDeviation)
                {
                    int thisDistanceToUs = plotDistance(pUnitPlot->getX(), pUnitPlot->getY(), ci->iX, ci->iY);
                    if (thisDistanceToUs < closestBorderDistance)
                    {
                        pClosestClosestBorderPlot = gGlobals.getMap().plot(ci->iX, ci->iY);
                        closestBorderDistance = thisDistanceToUs;
                    }
                }
            }

            std::set<const CvPlot*> reachableBorderPlots;
            
            for (std::set<const CvPlot*>::const_iterator ci(ourReachablePlots.begin()), ciEnd(ourReachablePlots.end()); ci != ciEnd; ++ci)
            {
                if (haveBorders && subAreaBorderIter->second.find((*ci)->getCoords()) != subAreaBorderIter->second.end())
                {
                    reachableBorderPlots.insert(*ci);
                }

                if ((*ci)->isCoastalLand() && ourHistory.find((*ci)->getCoords()) == ourHistory.end()
                    && hasInvisibleSecondNeighbourPlots(*ci, pUnit_->getTeam()))
                {
                    // todo - check if visiting this plot can reveal more plots
                    reachableBorderPlots.insert(*ci);
                }
            }

            if (!reachableBorderPlots.empty())
            {
                int bestValue = 0;                
                for (std::set<const CvPlot*>::const_iterator ci(reachableBorderPlots.begin()), ciEnd(reachableBorderPlots.end()); ci != ciEnd; ++ci)
                {
                    if (*ci == pUnitPlot)
                    {
                        continue;
                    }

                    int thisPlotDistance = plotDistance(targetCoords_->getX(), targetCoords_->getY(), (*ci)->getX(), (*ci)->getY());
                    // 5 - is max deviation from minimum distance from target - try to                     
                    // force exploration in circular fashion around target - todo - make 5 depend on map size?
                    if (thisPlotDistance > minimumRefDistance + maxPlotDeviation)
                    {
                        continue;
                    }
                    int thisValue = maxPlotDistance - thisPlotDistance;
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
                    }

                    if (thisValue > bestValue)
                    {
                        bestValue = thisValue;
                        pBestPlot = (CvPlot*)*ci;
                    }
                }
#ifdef ALTAI_DEBUG
                os << "\nexplore border plots:";
                if (pBestPlot) os << " best plot = " << pBestPlot->getCoords(); else os << " best plot = NULL";
                if (pClosestClosestBorderPlot) os << " closest close border plot = " << pClosestClosestBorderPlot->getCoords(); else os << " closest close border plot = NULL";
                if (pClosestBorderPlotToTarget) os << " closest border plot to target = " << pClosestBorderPlotToTarget->getCoords(); else os << " closest close border plot to target = NULL";
#endif
                if (pBestPlot && pUnit_->generatePath(pBestPlot, MOVE_IGNORE_DANGER, true))
                {
                    pUnit_->getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, pBestPlot);
                }
                else if (pClosestClosestBorderPlot && pUnit_->generatePath(pClosestClosestBorderPlot, MOVE_IGNORE_DANGER, true))
                {
                    pUnit_->getGroup()->pushMission(MISSION_MOVE_TO, pClosestClosestBorderPlot->getX_INLINE(), pClosestClosestBorderPlot->getY_INLINE(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, pClosestClosestBorderPlot);
                }
                else if (pClosestBorderPlotToTarget && pUnit_->generatePath(pClosestBorderPlotToTarget, MOVE_IGNORE_DANGER, true))
                {
                    pUnit_->getGroup()->pushMission(MISSION_MOVE_TO, pClosestBorderPlotToTarget->getX_INLINE(), pClosestBorderPlotToTarget->getY_INLINE(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, pClosestBorderPlotToTarget);
                }
                else
                {
                    pUnit_->getGroup()->pushMission(MISSION_SKIP);
                }
                return;
            }
            else
            {
                if (pClosestClosestBorderPlot && pUnit_->generatePath(pClosestClosestBorderPlot, MOVE_IGNORE_DANGER, true))
                {
                    pUnit_->getGroup()->pushMission(MISSION_MOVE_TO, pClosestClosestBorderPlot->getX_INLINE(), pClosestClosestBorderPlot->getY_INLINE(), 
                        MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, pClosestClosestBorderPlot);
                    return;
                }
            }

//            if (!pBestPlot)
//            {
//                if (subAreaBorderIter != borders.end())
//                {
//                    for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end()); ci != ciEnd; ++ci)
//                    {
//                        const int refDistance = stepDistance(targetCoords_->getX(), targetCoords_->getY(), ci->iX, ci->iY);
//                        minimumRefDistance = std::min<int>(minimumRefDistance, refDistance);
//                    }
//                }
//
//                //AreaPlotIter plotIter(pUnit_->area());
//                //const TeamTypes teamType = pUnit_->getTeam();
//                //
//                //while (CvPlot* pPlot = plotIter())
//                //{
//                //    if (pUnitPlot != pPlot &&
//                //        (!pPlot->isRevealed(teamType, false))) //|| 
//                //         //(pPlot->isCoastalLand() && (hasInvisibleNeighbourPlots(pPlot, teamType) || hasInvisibleSecondNeighbourPlots(pPlot, teamType)))))
//                //    {
//                //        const int refDistance = stepDistance(targetCoords_->getX(), targetCoords_->getY(), pPlot->getX(), pPlot->getY());
//                //        minimumRefDistance = std::min<int>(minimumRefDistance, refDistance);
//                //    }
//                //}
//
//#ifdef ALTAI_DEBUG
//                os << "\nexplore1: min ref distance = " << minimumRefDistance << ", ref coords = ";
//                if (targetCoords_)
//                {
//                    os << targetCoords_->getCoords();
//                }
//                else
//                {
//                    os << "NULL";
//                }
//#endif
//
//                // pBestPlot can be NULL
//                std::vector<std::pair<CvPlot*, int> > plotValues = getExplorePlotsValues(pUnit_, targetCoords_, minimumRefDistance);
//                int bestPlotValue = -1;
//
//                for (size_t i = 0, count = plotValues.size(); i < count; ++i)
//                {
//                    if (plotValues[i].second > bestPlotValue)
//                    {
//                        bestPlotValue = plotValues[i].second;
//                        pBestPlot = plotValues[i].first;
//                    }
//                }
//
//#ifdef ALTAI_DEBUG
//                os << "\nexplore2: plot = ";
//                if (pBestPlot)
//                {
//                    os << pBestPlot->getCoords();
//                }
//                else
//                {
//                    os << "NULL";
//                }
//#endif
//
//                if (!pBestPlot && targetCoords_)
//                {
//                    pBestPlot = targetCoords_;
//                }
//            }

            /*if (pBestPlot && pBestPlot != pUnitPlot)
            {
                if (pUnit_->generatePath(pBestPlot, MOVE_NO_ENEMY_TERRITORY, true))
                {
                    pUnit_->getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_EXPLORE, pBestPlot);
                }
                else if (pUnit_->generatePath(pBestPlot, MOVE_IGNORE_DANGER, true))
                {
                    pUnit_->getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, pBestPlot);
                }
                else
                {
                    pUnit_->getGroup()->pushMission(MISSION_SKIP);
                }
            }
            else*/
            
            if (!haveBorders)
            {                
                isComplete_ = true;
            }
            else
            {
                pUnit_->getGroup()->pushMission(MISSION_SKIP);
            }
        }

        virtual const CvUnitAI* getUnit() const
        {
            return pUnit_;
        }

        virtual CvPlot* getTarget() const
        {
            return targetCoords_;
        }

        virtual bool isComplete() const
        {
            return isComplete_;
        }

        virtual CvCity* getTargetCity() const
        {
            return targetCoords_ ? targetCoords_->getPlotCity() : NULL;
        }

        virtual UnitAITypes getAIType() const
        {
            return UNITAI_EXPLORE;
        }

    private:
        void setTarget_()
        {
            CityIter cityIter(CvPlayerAI::getPlayer(pUnit_->getOwner()));

            const TeamTypes teamType = pUnit_->getTeam();
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner());            

            std::vector<CvPlot*> referencePlots;
            while (CvCity* pCity = cityIter())
            {
                if (pCity->getArea() == pUnit_->area()->getID())
                {
                    referencePlots.push_back(pCity->plot());
                }
            }

            // ignore ourself as fine to keep the same target coords if they are valid
            std::vector<UnitMissionPtr> matchingMissions = pPlayer->getMissions(LandExplorePred(pUnit_->area()), pUnit_);

            for (size_t i = 0, count = referencePlots.size(); i < count; ++i)
            {
                if (std::find_if(matchingMissions.begin(), matchingMissions.end(), HasTarget(referencePlots[i])) == matchingMissions.end())
                {
                    targetCoords_ = referencePlots[i];
                    break;
                }
            }

            if (!targetCoords_)
            {
                std::vector<CvPlot*> ignorePlots;

                for (;;)
                {
                    // target best city site not already targetted
                    CvPlot* pTarget = pPlayer->getSettlerManager()->getBestPlot(pUnit_->plot()->getSubArea(), ignorePlots);
                    if (!pTarget)
                    {
                        // no good sites
                        break;
                    }

                    if (std::find_if(matchingMissions.begin(), matchingMissions.end(), HasTarget(pTarget)) == matchingMissions.end())
                    {
                        targetCoords_ = pTarget;
                        break;
                    }
                    else
                    {
                        ignorePlots.push_back(pTarget);
                    }
                }
            }

            AreaPlotIter plotIter(pUnit_->area());
            int minDistance = MAX_INT;

            while (CvPlot* pPlot = plotIter())
            {
                if (!pPlot->isRevealed(teamType, false) || pPlot->isCoastalLand() && getNeighbourInvisiblePlotCount(pPlot,teamType) > 0)
                {
                    const int distance = stepDistance(pUnit_->getX(), pUnit_->getY(), pPlot->getX(), pPlot->getY());

                    if (distance < minDistance && std::find_if(matchingMissions.begin(), matchingMissions.end(), IsCloserThan(pPlot, 5)) == matchingMissions.end())
                    {
                        minDistance = distance;
                        targetCoords_ = pPlot;
                    }
                }
            }
            
            // found no targets to explore
            if (!targetCoords_)
            {
                isComplete_ = true;
            }

#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
            os << "\nexplore: target = ";
            if (targetCoords_)
            {
                os << targetCoords_->getCoords();
            }
            else
            {
                os << "NULL";
            }
#endif
        }

        const CvUnitAI* pUnit_;
        CvCity* pReferenceCity_;
        CvPlot* targetCoords_;
        bool isComplete_;
        int minimumRefDistance_;
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
        const CvPlot* pUnitPlot = pUnit->plot();
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*(pPlayer->getCvPlayer()))->getStream();
#endif
        const CvPlot* pClosestCityPlot = pPlayer->getAnalysis()->getMapAnalysis()->getClosestCity(pUnitPlot, pUnitPlot->getSubArea(), true);
        std::set<const CvPlot*> ourReachablePlots = getReachablePlots(*pPlayer, pUnitPlot, std::vector<const CvUnit*>(1, pUnit));
        std::set<const CvPlot*> dangerPlots = pPlayer->getAnalysis()->getMilitaryAnalysis()->getThreatenedPlots();
        std::set<XYCoords> ourHistory = getUnitHistory(*pPlayer, pUnit->getIDInfo());

        const bool plotDanger = dangerPlots.find(pUnit->plot()) != dangerPlots.end();

        // todo - upgrade if possible
        if (pUnit->getDamage() > 0)  // doesn't apply to work boats
        {
            if (pClosestCityPlot)
            {
                int iPathTurns = 0;
                if (pUnit->generatePath(pClosestCityPlot, 0, false, &iPathTurns))
                {
                    bool moveToPort = iPathTurns <= 1;
                    if (iPathTurns <= 2)
                    {
                        const CvPlot* pEndTurnPlot = pUnit->getPathEndTurnPlot();
                        if (dangerPlots.find(pEndTurnPlot) == dangerPlots.end())
                        {
                            moveToPort = true;
                        }
                    }

                    if (moveToPort)
                    {
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pClosestCityPlot->getX_INLINE(), pClosestCityPlot->getY_INLINE(), 
                            MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pClosestCityPlot);
                        return true;
                    }
                }
            }
            const FeatureTypes featureType = pUnit->plot()->getFeatureType();
            if (!plotDanger && (featureType == NO_FEATURE || gGlobals.getFeatureInfo(featureType).getTurnDamage() == 0))
            {
                pUnit->getGroup()->pushMission(MISSION_HEAL);
                return true;
            }
        }

        if (plotDanger && !pUnit->canFight())
        {
            std::map<const CvPlot*, std::vector<const CvUnit*> > hostiles = getNearbyHostileStacks(*pPlayer, pUnit->plot(), 2);                
            const CvPlot* pEscapePlot = getEscapePlot(*pPlayer, pUnit, dangerPlots, hostiles);
            if (pEscapePlot && !pUnit->atPlot(pEscapePlot))
            {
                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pEscapePlot->getX_INLINE(), pEscapePlot->getY_INLINE(), 
                    MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pEscapePlot);
                return true;
            }            
        }

        const std::map<int /* sub area id */, std::set<XYCoords> >& borders = 
                gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner())->getAnalysis()->getMapAnalysis()->getUnrevealedBorderMap();
        std::map<int, std::set<XYCoords> >::const_iterator subAreaBorderIter = borders.find(pUnitPlot->getSubArea());
        const bool haveBorders = subAreaBorderIter != borders.end();

        std::set<const CvPlot*> reachableBorderPlots;
            
        for (std::set<const CvPlot*>::const_iterator ci(ourReachablePlots.begin()), ciEnd(ourReachablePlots.end()); ci != ciEnd; ++ci)
        {
            if (haveBorders && subAreaBorderIter->second.find((*ci)->getCoords()) != subAreaBorderIter->second.end())
            {
                reachableBorderPlots.insert(*ci);
            }
        }

        const CvPlot* pBestPlot = (const CvPlot*)0;
        if (!reachableBorderPlots.empty())
        {
            int bestValue = 0;                
            for (std::set<const CvPlot*>::const_iterator ci(reachableBorderPlots.begin()), ciEnd(reachableBorderPlots.end()); ci != ciEnd; ++ci)
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
            if (pBestPlot && pUnit->generatePath(pBestPlot, MOVE_NO_ENEMY_TERRITORY, true))
            {
                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pBestPlot);
                return true;
            }
        }

        if (haveBorders)
        {
            int bestValue = MAX_INT;
            for (std::set<XYCoords>::const_iterator ci(subAreaBorderIter->second.begin()), ciEnd(subAreaBorderIter->second.end());
                ci != ciEnd; ++ci)
            {
                int thisPlotDistance = plotDistance(pUnit->getX(), pUnit->getY(), ci->iX, ci->iY);
                if (thisPlotDistance <= bestValue)
                {                    
                    pBestPlot = gGlobals.getMap().plot(ci->iX, ci->iY);
                    bestValue = thisPlotDistance;
                }
            }

            if (pBestPlot && pUnit->generatePath(pBestPlot, MOVE_IGNORE_DANGER, true))
            {
                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_EXPLORE, (CvPlot*)pBestPlot);
                return true;
            }
        }

        // just try and go to closest port
        if (pClosestCityPlot)
        {
            int iPathTurns = 0;
            if (pUnit->generatePath(pClosestCityPlot, 0, false, &iPathTurns))
            {
                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pClosestCityPlot->getX_INLINE(), pClosestCityPlot->getY_INLINE(), 
                    MOVE_IGNORE_DANGER, false, false, MISSIONAI_EXPLORE, (CvPlot*)pClosestCityPlot);
                return true;
            }
        }

        pUnit->getGroup()->pushMission(MISSION_SKIP);
        return true;
    }

    XYCoords getLandExploreRefPlot(const CvUnitAI* pUnit)
    {
        const PlayerTypes playerType = pUnit->getOwner();
        const TeamTypes teamType = pUnit->getTeam();
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType);
        CityIter cityIter(*pPlayer->getCvPlayer());
        const CvPlot* pUnitPlot = pUnit->plot();

        const std::map<int /* sub area id */, std::set<XYCoords> >& borders = 
                gGlobals.getGame().getAltAI()->getPlayer(playerType)->getAnalysis()->getMapAnalysis()->getUnrevealedBorderMap();

        std::map<int /* sub area */, std::set<XYCoords> > subAreaRefPlotsMap = 
            pPlayer->getAnalysis()->getMilitaryAnalysis()->getLandScoutMissionRefPlots();

        const int maxPlotDistance = gGlobals.getMap().maxPlotDistance();
        int closestBorderDistance = MAX_INT;

        // todo - detect unreachable border plots (due to no open borders - only reason a plot in the same sub area would be inaccessible)
        std::map<int, std::set<XYCoords> >::const_iterator subAreaBorderIter = borders.find(pUnitPlot->getSubArea());
        if (subAreaBorderIter == borders.end())
        {            
            XYCoords closestOtherAreaBorderCoords(-1, -1);
            // pick nearest (coastal) plot
            for (subAreaBorderIter = borders.begin(); subAreaBorderIter != borders.end(); ++subAreaBorderIter)
            {
                boost::shared_ptr<SubArea> pSubArea = gGlobals.getMap().getSubArea(subAreaBorderIter->first);
                if (!pSubArea->isWater())
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