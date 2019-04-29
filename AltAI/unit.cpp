#include "AltAI.h"

#include "./iters.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./utils.h"
#include "./gamedata_analysis.h"
#include "./unit.h"
#include "./unit_log.h"
#include "./save_utils.h"

#include "../CvGameCoreDLL/CvDLLEngineIFaceBase.h"
#include "../CvGameCoreDLL/CvDLLFAStarIFaceBase.h"
#include "../CvGameCoreDLL/CvGameCoreUtils.h"
#include "../CvGameCoreDLL/FAStarNode.h"

namespace AltAI
{
    struct RouteToMission
    {
        RouteToMission(const CvUnitAI* pUnit, RouteTypes routeType)
            : stepFinderInfo_(MAKEWORD((short)pUnit->getOwner(), (short)(SubAreaStepFlags::Team_Territory | SubAreaStepFlags::Unowned_Territory))),
              pUnit_(pUnit), routeType_(routeType), buildType_(GameDataAnalysis::getBuildTypeForRouteType(routeType)), 
              playerType_(pUnit->getOwner()), teamType_(pUnit->getTeam()), unitBaseMoves_(pUnit->baseMoves())
        {
            unitWorkRate_ = pUnit->getUnitInfo().getWorkRate();

            const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);
            unitWorkRate_ *= std::max<int>(0, (player.getWorkerSpeedModifier() + 100));
            unitWorkRate_ /= 100;

            if (!player.isHuman() && !player.isBarbarian())
            {
                unitWorkRate_ *= std::max<int>(0, (gGlobals.getHandicapInfo(gGlobals.getGame().getHandicapType()).getAIWorkRateModifier() + 100));
                unitWorkRate_ /= 100;
            }

            hasBridgeBuilding_ = CvTeamAI::getTeam(teamType_).isBridgeBuilding();
        }

        int operator() (const CvPlot* pStartPlot, const CvPlot* pTargetPlot) const
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
            os << "\nCost for route: " << gGlobals.getRouteInfo(routeType_).getType() << " from: "
               << pStartPlot->getCoords() << " to: " << pTargetPlot->getCoords();
#endif
            FAStar* pStepFinder = gDLL->getFAStarIFace()->create();
            CvMap& theMap = gGlobals.getMap();
            gDLL->getFAStarIFace()->Initialize(pStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), stepDestValid, stepHeuristic, stepCost, areaStepValidWithFlags, stepAdd, NULL, NULL);

            int totalCost = MAX_INT;
            if (gDLL->getFAStarIFace()->GeneratePath(pStepFinder, pStartPlot->getX(), pStartPlot->getY(), pTargetPlot->getX(), pTargetPlot->getY(), false, stepFinderInfo_, true))
            {
                std::list<CvPlot*> path;
                FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pStepFinder);
                bool routeCompleteFromThisPointToEnd = true;
                while (pNode)
                {
                    CvPlot* pPlot = gGlobals.getMap().plot(pNode->m_iX, pNode->m_iY);
                    RouteTypes routeType = pPlot->getRouteType();

                    if (routeType != routeType_)
                    {
                        routeCompleteFromThisPointToEnd = false;
                    }

                    if (!routeCompleteFromThisPointToEnd)
                    {
                        path.push_front(pPlot);
                    }

                    pNode = pNode->m_pParent;
                }

                CvPlot* pPreviousPlot = NULL;
                
                totalCost = 0;                
                int availableMoves = unitBaseMoves_;

                for (std::list<CvPlot*>::const_iterator pathIter(path.begin()), endIter(path.end()); pathIter != endIter; ++pathIter)
                {                    
                    int actualCost = 0;
                    CvPlot* pPlot = *pathIter;
                    
                    RouteTypes routeType = pPlot->getRouteType();

                    int plotMovementCost = pPreviousPlot ? movementCost(pPreviousPlot, pPlot) : 0;

                    availableMoves = std::max<int>(availableMoves - plotMovementCost, 0);
#ifdef ALTAI_DEBUG
                    os << "\n\tmoves after moving to plot: " << pPlot->getCoords() << " = " << availableMoves
                       << " actual movement cost = " << plotMovementCost;
#endif
                    if (availableMoves == 0)
                    {
                        ++actualCost; // cost us a move to get onto the plot
                        availableMoves = unitBaseMoves_;
                    }

                    if (routeType != routeType_)
                    {
                        // add the cost of building the route
                        actualCost += (pPlot->getBuildTime(buildType_) - pPlot->getBuildProgress(buildType_)) / unitWorkRate_;
#ifdef ALTAI_DEBUG
                        os << "\n\tcost for route for plot: " << pPlot->getCoords() << " = " 
                           << (pPlot->getBuildTime(buildType_) - pPlot->getBuildProgress(buildType_)) / unitWorkRate_
                           << ", " << pPlot->getBuildTime(buildType_) << ", " << pPlot->getBuildProgress(buildType_) << ", " << unitWorkRate_;
#endif
                        // reset moves ready for next plot
                        availableMoves = unitBaseMoves_;
                    }

#ifdef ALTAI_DEBUG
                    os << "\n\tactual cost = " << actualCost;
#endif
                    totalCost += actualCost;
                    pPreviousPlot = pPlot;
                }                
            }

            gDLL->getFAStarIFace()->destroy(pStepFinder);

            return totalCost;
        }

        // only designed for land workers and assumes they don't get promotions like double hills movement, etc...
        // this allows a slightly simpler implementation than CvPlot::movementCost, on which this (and the MoveToMission version) are based
        int movementCost(const CvPlot* pFromPlot, const CvPlot* pToPlot) const
        {
            int iRegularCost;
            int iRouteCost;
            int iRouteFlatCost;

            FeatureTypes featureType = pToPlot->getFeatureType();
            TerrainTypes terrainType = pToPlot->getTerrainType();
            RouteTypes routeType = pToPlot->getRouteType();

            iRegularCost = ((featureType == NO_FEATURE) ? 
                gGlobals.getTerrainInfo(terrainType).getMovementCost() : gGlobals.getFeatureInfo(featureType).getMovementCost());

            if (pToPlot->isHills())
            {
                iRegularCost += gGlobals.getHILLS_EXTRA_MOVEMENT();
            }

            if (iRegularCost > 0)
            {
                iRegularCost = std::max<int>(1, (iRegularCost - pUnit_->getExtraMoveDiscount()));
            }

            iRegularCost = std::min<int>(iRegularCost, pUnit_->baseMoves()) * gGlobals.getMOVE_DENOMINATOR();

            if (routeType != NO_ROUTE && (hasBridgeBuilding_ || !pFromPlot->isRiverCrossing(directionXY(pFromPlot, pToPlot))))
            {
                iRouteCost = std::max<int>(gGlobals.getRouteInfo(routeType_).getMovementCost() + CvTeamAI::getTeam(teamType_).getRouteChange(routeType_),
                    gGlobals.getRouteInfo(routeType).getMovementCost() + CvTeamAI::getTeam(teamType_).getRouteChange(routeType));
                iRouteFlatCost = std::max<int>(gGlobals.getRouteInfo(routeType_).getFlatMovementCost() * unitBaseMoves_,
                    gGlobals.getRouteInfo(routeType).getFlatMovementCost() * unitBaseMoves_);
            }
            else
            {
                iRouteCost = MAX_INT;
                iRouteFlatCost = MAX_INT;
            }

            return std::max<int>(1, std::min<int>(iRegularCost, std::min<int>(iRouteCost, iRouteFlatCost))) / gGlobals.getMOVE_DENOMINATOR();
        }

        
        const int stepFinderInfo_;
        const CvUnit* pUnit_;
        RouteTypes routeType_;
        BuildTypes buildType_;
        PlayerTypes playerType_;
        TeamTypes teamType_;
        bool hasBridgeBuilding_;
        int unitBaseMoves_;
        int unitWorkRate_;
        int routeMovementCost_, flatRouteMovementCost_;
    };

    struct MoveToMission
    {
        explicit MoveToMission(const CvUnitAI* pUnit)
            : pUnit_(pUnit), playerType_(pUnit->getOwner()), teamType_(pUnit->getTeam()), unitBaseMoves_(pUnit->baseMoves())
        {
            if (pUnit->getDomainType() == DOMAIN_SEA)
            {
                
                stepFinderInfo_ = MOVE_SAFE_TERRITORY;
            }
            else
            {
                stepFinderInfo_ = MAKEWORD((short)pUnit->getOwner(), 
                    (short)(SubAreaStepFlags::Team_Territory | SubAreaStepFlags::Unowned_Territory | SubAreaStepFlags::Friendly_Territory));
            }

            hasBridgeBuilding_ = CvTeamAI::getTeam(teamType_).isBridgeBuilding();
        }

        int operator() (const CvPlot* pStartPlot, const CvPlot* pTargetPlot) const
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
            os << "\nCost for moving from: " << pStartPlot->getCoords() << " to: " << pTargetPlot->getCoords();
#endif
            FAStar* pStepFinder;
            if (pUnit_->getDomainType() == DOMAIN_SEA)
            {
                pStepFinder = &gGlobals.getStepFinder();
            }
            else
            {
                pStepFinder = gDLL->getFAStarIFace()->create();
            }
            CvMap& theMap = gGlobals.getMap();
            gDLL->getFAStarIFace()->Initialize(pStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), stepDestValid, stepHeuristic, stepCost, areaStepValidWithFlags, stepAdd, NULL, NULL);

            int totalCost = MAX_INT;
            if (gDLL->getFAStarIFace()->GeneratePath(pStepFinder, pStartPlot->getX(), pStartPlot->getY(), pTargetPlot->getX(), pTargetPlot->getY(), false, stepFinderInfo_, true))
            {
                std::list<CvPlot*> path;
                FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pStepFinder);
                while (pNode)
                {
                    path.push_front(gGlobals.getMap().plot(pNode->m_iX, pNode->m_iY));
                    pNode = pNode->m_pParent;
                }

                CvPlot* pPreviousPlot = NULL;
                
                totalCost = 0;
                int availableMoves = unitBaseMoves_;

                for (std::list<CvPlot*>::const_iterator pathIter(path.begin()), endIter(path.end()); pathIter != endIter; ++pathIter)
                {                    
                    int actualCost = 0;
                    CvPlot* pPlot = *pathIter;
                    
                    int plotMovementCost = pPreviousPlot ? movementCost(pPreviousPlot, pPlot) : 0;

                    availableMoves = std::max<int>(availableMoves - plotMovementCost, 0);
#ifdef ALTAI_DEBUG
                    os << "\n\tmoves after moving to plot: " << pPlot->getCoords() << " = " << availableMoves
                       << " actual movement cost = " << plotMovementCost;
#endif
                    if (availableMoves == 0)
                    {
                        ++actualCost; // cost us a move to get onto the plot
                        availableMoves = unitBaseMoves_;
                    }

#ifdef ALTAI_DEBUG
                    os << "\n\tactual cost = " << actualCost;
#endif
                    totalCost += actualCost;
                    pPreviousPlot = pPlot;
                }                
            }

            if (pUnit_->getDomainType() != DOMAIN_SEA)
            {
                gDLL->getFAStarIFace()->destroy(pStepFinder);
            }

            return totalCost;
        }

        int movementCost(const CvPlot* pFromPlot, const CvPlot* pToPlot) const
        {
            int iRegularCost;
            int iRouteCost;
            int iRouteFlatCost;

            FeatureTypes featureType = pToPlot->getFeatureType();
            TerrainTypes terrainType = pToPlot->getTerrainType();
            RouteTypes fromRouteType = pFromPlot->getRouteType(), toRouteType = pToPlot->getRouteType();

            iRegularCost = ((featureType == NO_FEATURE) ? 
                gGlobals.getTerrainInfo(terrainType).getMovementCost() : gGlobals.getFeatureInfo(featureType).getMovementCost());

            if (pToPlot->isHills())
            {
                iRegularCost += gGlobals.getHILLS_EXTRA_MOVEMENT();
            }

            if (iRegularCost > 0)
            {
                iRegularCost = std::max<int>(1, (iRegularCost - pUnit_->getExtraMoveDiscount()));
            }

            const bool bHasTerrainCost = (iRegularCost > 1);

            if (bHasTerrainCost)
            {
                if (((featureType == NO_FEATURE) ? pUnit_->isTerrainDoubleMove(terrainType) : pUnit_->isFeatureDoubleMove(featureType)) ||
                    (pToPlot->isHills() && pUnit_->isHillsDoubleMove()))
                {
                    iRegularCost /= 2;
                }
            }

            iRegularCost = std::min<int>(iRegularCost, pUnit_->baseMoves()) * gGlobals.getMOVE_DENOMINATOR();

            if (toRouteType != NO_ROUTE && fromRouteType != NO_ROUTE && (hasBridgeBuilding_ || !pFromPlot->isRiverCrossing(directionXY(pFromPlot, pToPlot))))
            {
                iRouteCost = std::max<int>(gGlobals.getRouteInfo(fromRouteType).getMovementCost() + CvTeamAI::getTeam(teamType_).getRouteChange(fromRouteType),
                    gGlobals.getRouteInfo(toRouteType).getMovementCost() + CvTeamAI::getTeam(teamType_).getRouteChange(toRouteType));
                iRouteFlatCost = std::max<int>(gGlobals.getRouteInfo(fromRouteType).getFlatMovementCost() * unitBaseMoves_,
                    gGlobals.getRouteInfo(toRouteType).getFlatMovementCost() * unitBaseMoves_);
            }
            else
            {
                iRouteCost = MAX_INT;
                iRouteFlatCost = MAX_INT;
            }

            return std::max<int>(1, std::min<int>(iRegularCost, std::min<int>(iRouteCost, iRouteFlatCost))) / gGlobals.getMOVE_DENOMINATOR();
        }

        int stepFinderInfo_;
        const CvUnit* pUnit_;
        PlayerTypes playerType_;
        TeamTypes teamType_;
        bool hasBridgeBuilding_;
        int unitBaseMoves_;
    };

    namespace
    {
        int calculateBuildMissionLength(const CvUnitAI* pUnit, const CvPlot* pStartPlot, const CvPlot* pTargetPlot, BuildTypes buildType)
        {
            int missionLength = 0;

            if (pTargetPlot->getX() != pUnit->getX() || pTargetPlot->getY() != pUnit->getY())
            {
                MoveToMission moveToMission(pUnit);
                missionLength += moveToMission(pStartPlot, pTargetPlot);
            }

            if (missionLength != MAX_INT)
            {
                missionLength += (pTargetPlot->getBuildTime(buildType) - pTargetPlot->getBuildProgress(buildType)) / pUnit->workRate(true);
            }

            return missionLength;
        }

        int calculateRouteToMissionLength(const CvUnitAI* pUnit, const CvPlot* pStartPlot, const CvPlot* pTargetPlot, RouteTypes routeType)
        {
            int missionLength = 0;

            RouteToMission routeToMission(pUnit, routeType);
            missionLength += routeToMission(pStartPlot, pTargetPlot);

            return missionLength;
        }

        int calculateMoveToMissionLength(const CvUnitAI* pUnit, const CvPlot* pStartPlot, const CvPlot* pTargetPlot)
        {
            int missionLength = 0;

            MoveToMission moveToMission(pUnit);
            missionLength += moveToMission(pStartPlot, pTargetPlot);

            return missionLength;
        }
    }

    Unit::Unit(CvUnitAI* pUnit) : pUnit_(pUnit), unitHistory_(pUnit)
    {
    }

    bool Unit::operator < (const Unit& other) const
    {
        return pUnit_->getID() < other.pUnit_->getID();
    }

    void Unit::pushWorkerMission(size_t turn, const CvCity* pCity, const CvPlot* pTargetPlot, MissionTypes missionType, BuildTypes buildType)
    {
        int thisMissionLength = 1;
        XYCoords startCoords;
        if (workerMissions_.empty())
        {
            startCoords = pUnit_->plot()->getCoords();
        }
        else
        {
            startCoords = workerMissions_.rbegin()->targetCoords;
        }

        const CvPlot* pStartPlot = gGlobals.getMap().plot(startCoords.iX, startCoords.iY);

        if (missionType == MISSION_ROUTE_TO)
        {
            thisMissionLength = calculateRouteToMissionLength(pUnit_, pStartPlot, pTargetPlot, (RouteTypes)gGlobals.getBuildInfo(buildType).getRoute());            
        }
        else if (missionType == MISSION_MOVE_TO)
        {
            thisMissionLength = calculateMoveToMissionLength(pUnit_, pStartPlot, pTargetPlot);
        }
        else if (missionType == MISSION_BUILD)
        {
            thisMissionLength = calculateBuildMissionLength(pUnit_, pStartPlot, pTargetPlot, buildType);
        }
        else if (missionType == MISSION_SKIP)
        {
            thisMissionLength = 1;
        }

        // todo - log error here
        //if (thisMissionLength == MAX_INT)
        //{
        //    return;
        //}

        workerMissions_.push_back(Mission(pUnit_, pCity, pTargetPlot, missionType, buildType, thisMissionLength, startCoords));

#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();

        CvWString missionTypeString;
        getMissionTypeString(missionTypeString, missionType);

        int totalMissionLength = 0;
        for (size_t i = 0, count = workerMissions_.size(); i < count; ++i)
        {
            totalMissionLength += workerMissions_[i].length;
        }

        os << "\nTurn = " << turn << " unit = " << pUnit_->getID() << " city = " << (pCity ? narrow(pCity->getName()) : " none ")
           << " target = " << pTargetPlot->getCoords() << " mission = " << narrow(missionTypeString)
           << " build = " << (buildType == NO_BUILD ? " none " : gGlobals.getBuildInfo(buildType).getType())
           << " mission length = " << totalMissionLength;

#endif
    }

    // todo - is this always called when unit has moves available?
    void Unit::updateMission()
    {
        if (workerMissions_.empty())
        {
            return;
        }

        if (workerMissions_[0].missionType == MISSION_BUILD)
        {
            workerMissions_[0].length = !pUnit_->canBuild(workerMissions_[0].getTarget(), workerMissions_[0].buildType)
                ? 0 : calculateBuildMissionLength(pUnit_, pUnit_->plot(), workerMissions_[0].getTarget(), workerMissions_[0].buildType);
        }
        else if (workerMissions_[0].missionType == MISSION_ROUTE_TO)
        {
            workerMissions_[0].length = calculateRouteToMissionLength(pUnit_, pUnit_->plot(),workerMissions_[0].getTarget(), (RouteTypes)gGlobals.getBuildInfo(workerMissions_[0].buildType).getRoute());
        }

        if (workerMissions_[0].length == 0)
        {
            std::vector<Mission> remainingMissions;
            for (size_t i = 1, count = workerMissions_.size(); i < count; ++i)
            {
                remainingMissions.push_back(workerMissions_[i]);
            }
            workerMissions_ = remainingMissions;
        }
    }

    void Unit::updateMission(const CvPlot* pOldPlot, const CvPlot* pNewPlot)
    {
        if (workerMissions_.empty())
        {
            return;
        }

        if (!pUnit_->at(workerMissions_[0].targetCoords.iX, workerMissions_[0].targetCoords.iY))
        {
            int iPathTurns = 0;
            if (pUnit_->generatePath(gGlobals.getMap().plot(workerMissions_[0].targetCoords.iX, workerMissions_[0].targetCoords.iY), MOVE_SAFE_TERRITORY, true, &iPathTurns))
            {
                workerMissions_[0].length = iPathTurns / pUnit_->getUnitInfo().getMoves();
            }
        }
        else
        {
            std::vector<Mission> remainingMissions;
            for (size_t i = 1, count = workerMissions_.size(); i < count; ++i)
            {
                remainingMissions.push_back(workerMissions_[i]);
            }
            workerMissions_ = remainingMissions;
        }
    }

    void Unit::clearMission(size_t turn)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
        os << "\nTurn = " << turn << " unit = " << pUnit_->getID() << " clearing mission (before = ";
        for (size_t i = 0, count = workerMissions_.size(); i < count; ++i)
        {
            workerMissions_[i].debug(os);
        }
#endif
        workerMissions_.clear();
        missionLength_ = 0;
    }

    void Unit::pushMission(const UnitMissionPtr& pMission)
    {
        missions_.push_back(pMission);
    }

    bool Unit::hasWorkerMissionAt(const CvPlot* pTargetPlot) const
    {
        for (std::vector<Mission>::const_iterator ci(workerMissions_.begin()), ciEnd(workerMissions_.end()); ci != ciEnd; ++ci)
        {
            if (ci->isWorkerMissionAt(pTargetPlot->getCoords()))
            {
                return true;
            }
        }

        return false;
    }

    bool Unit::hasBuildOrRouteMission() const
    {
        for (std::vector<Mission>::const_iterator ci(workerMissions_.begin()), ciEnd(workerMissions_.end()); ci != ciEnd; ++ci)
        {
            if (ci->buildType != NO_BUILD || ci->missionType == MISSION_ROUTE_TO)
            {
                return true;
            }
        }

        return false;
    }

    bool Unit::isWorkerMissionFor(const CvCity* pCity) const
    {
        for (std::vector<Mission>::const_iterator ci(workerMissions_.begin()), ciEnd(workerMissions_.end()); ci != ciEnd; ++ci)
        {
            if (ci->isWorkerMissionFor(pCity))
            {
                return true;
            }
        }

        return false;
    }

    const std::vector<Unit::Mission>& Unit::getWorkerMissions() const
    {
        return workerMissions_;
    }

    const std::vector<UnitMissionPtr>& Unit::getMissions() const
    {
        return missions_;
    }

    void Unit::write(FDataStreamBase* pStream) const
    {
        writeComplexVector(pStream, workerMissions_);
    }

    void Unit::read(FDataStreamBase* pStream)
    {
        int workerMissionCount = 0;
        pStream->Read(&workerMissionCount);
        for (size_t i = 0; i < workerMissionCount; ++i)
        {
            workerMissions_.push_back(Mission(pUnit_));
            workerMissions_.rbegin()->read(pStream);
        }
    }

    Unit::Mission::Mission(const CvUnitAI* pUnit)
        : pUnit_(pUnit), 
          targetCoords(-1, -1), 
          missionType(NO_MISSION), buildType(NO_BUILD), length(-1), startCoords(-1, -1)
    {
    }

    Unit::Mission::Mission(const CvUnitAI* pUnit, const CvCity* pCity, const CvPlot* pTargetPlot, MissionTypes missionType_, BuildTypes buildType_, int length_, XYCoords startCoords_)
        : pUnit_(pUnit), city(pCity ? pCity->getIDInfo() : IDInfo()), 
          targetCoords(pTargetPlot->getCoords()), 
          missionType(missionType_), buildType(buildType_), length(length_), startCoords(startCoords_)
    {
    }

    bool Unit::Mission::isWorkerMissionAt(XYCoords coords) const
    {
        return (missionType == MISSION_ROUTE_TO || missionType == MISSION_BUILD) && buildType != NO_BUILD && targetCoords == coords;
    }

    bool Unit::Mission::isWorkerMissionFor(const CvCity* pCity) const
    {
        return pCity->getIDInfo() == city;
    }

    const CvUnitAI* Unit::Mission::getUnit() const
    {
        return pUnit_;
    }

    CvPlot* Unit::Mission::getTarget() const
    {
        return gGlobals.getMap().plot(targetCoords.iX, targetCoords.iY);
    }

    bool Unit::Mission::isComplete() const
    {
        switch (missionType)
        {
            case MISSION_MOVE_TO:
                return pUnit_->at(targetCoords.iX, targetCoords.iY);
            case MISSION_ROUTE_TO:
                {
#ifdef ALTAI_DEBUG
                    std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
                    os << "\nChecking mission complete for route from: " << startCoords << " to " << targetCoords;
#endif
                    CvMap& theMap = gGlobals.getMap();
                    CvPlot* pStartPlot = theMap.plot(startCoords.iX, startCoords.iY);
                    //RouteTypes routeType = pUnit_->getGroup()->getBestBuildRoute(pStartPlot);
                    //if (routeType == NO_ROUTE)
                    //{
                    //    return true;  // can't build a route for some reason - no point in continuing the mission
                    //}

                    if (pStartPlot->getRouteType() == NO_ROUTE)
                    {
#ifdef ALTAI_DEBUG
                        os << "\n\treturning false - no route at start plot";
#endif
                        return false;
                    }

                    if (stepDistance(pStartPlot->getX(), pStartPlot->getY(), targetCoords.iX, targetCoords.iY) == 1)
                    {
#ifdef ALTAI_DEBUG
                        os << "\n\treturning " << (theMap.plot(targetCoords.iX, targetCoords.iY)->getRouteType() != NO_ROUTE) << " - adjacent plots";
#endif
                        return theMap.plot(targetCoords.iX, targetCoords.iY)->getRouteType() != NO_ROUTE;
                    }
                    else
                    {
                        FAStar* pSubAreaStepFinder = gDLL->getFAStarIFace()->create();
                    
                        gDLL->getFAStarIFace()->Initialize(pSubAreaStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), subAreaStepDestValid, stepHeuristic, stepCost, subAreaStepValid, stepAdd, NULL, NULL);
                        const int stepFinderInfo = MAKEWORD((short)pUnit_->getOwner(), (short)(SubAreaStepFlags::Team_Territory | SubAreaStepFlags::Unowned_Territory));

                        bool complete = true;  // assume we're done if no path is found since we don't want to repeat the mission anyway in that case (e.g. border change could cause this)
                        gDLL->getFAStarIFace()->GeneratePath(pSubAreaStepFinder, startCoords.iX, startCoords.iY, targetCoords.iX, targetCoords.iY, false, stepFinderInfo);
                        FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pSubAreaStepFinder);
                        while (pNode)
                        {
                            CvPlot* pPlot = gGlobals.getMap().plot(pNode->m_iX, pNode->m_iY);
                            if (pPlot->getRouteType() == NO_ROUTE)
                            {
                                complete = false;
                                break;
                            }
                            pNode = pNode->m_pParent;
                        }

                        gDLL->getFAStarIFace()->destroy(pSubAreaStepFinder);
#ifdef ALTAI_DEBUG
                        os << "\n\treturning " << complete << " - non-adjacent plots";
#endif
                        return complete;
                    }
                }
            case MISSION_BUILD:
                {
                    const CvPlot* pPlot = gGlobals.getMap().plot(targetCoords.iX, targetCoords.iY);
                    const CvBuildInfo& buildInfo = gGlobals.getBuildInfo(buildType);
#ifdef ALTAI_DEBUG
                    std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
                    os << "\nChecking mission complete for build: " << buildInfo.getType() << " at: " << targetCoords;
#endif
                    ImprovementTypes buildImprovementType = (ImprovementTypes)buildInfo.getImprovement();
                    FeatureTypes plotFeature = pPlot->getFeatureType();
                    bool unremovedFeature = plotFeature != NO_FEATURE && buildInfo.isFeatureRemove(pPlot->getFeatureType());
                    RouteTypes buildRouteType = (RouteTypes)buildInfo.getRoute();

                    if (buildImprovementType != NO_IMPROVEMENT)
                    {
#ifdef ALTAI_DEBUG
                        os << ", return: " << (buildImprovementType == pPlot->getImprovementType() && !unremovedFeature);
#endif
                        return buildImprovementType == pPlot->getImprovementType() && !unremovedFeature;
                    }
                    else if (buildRouteType != NO_ROUTE)
                    {
#ifdef ALTAI_DEBUG
                        os << ", return: " << (buildRouteType == pPlot->getRouteType() && !unremovedFeature);
#endif
                        return buildRouteType == pPlot->getRouteType() && !unremovedFeature;
                    }
#ifdef ALTAI_DEBUG
                    os << ", return: " << (!unremovedFeature);
#endif
                    return !unremovedFeature;
                }
            case MISSION_SKIP:
                return true;
            default:
                return false;
        }
    }

    CvCity* Unit::Mission::getTargetCity() const
    {
        return city.eOwner == NO_PLAYER ? NULL : ::getCity(city);
    }

    UnitAITypes Unit::Mission::getAIType() const
    {
        return UNITAI_WORKER;
    }

    void Unit::Mission::update()
    {
        const CvPlot* pTargetPlot = gGlobals.getMap().plot(targetCoords.iX, targetCoords.iY);

        if (missionType == MISSION_BUILD)
        {
            length = !pUnit_->canBuild(pTargetPlot, buildType) ? 0 : calculateBuildMissionLength(pUnit_, pUnit_->plot(), pTargetPlot, buildType);
        }
        else if (missionType == MISSION_ROUTE_TO)
        {
            length = calculateRouteToMissionLength(pUnit_, pUnit_->plot(), pTargetPlot, (RouteTypes)gGlobals.getBuildInfo(buildType).getRoute());
        }
        else if (missionType == MISSION_MOVE_TO)
        {
            length = calculateMoveToMissionLength(pUnit_, pUnit_->plot(), pTargetPlot);
        }
    }

    void Unit::Mission::debug(std::ostream& os) const
    {
        CvWString missionTypeString;
        getMissionTypeString(missionTypeString, missionType);
        const CvCity* pTargetCity = ::getCity(city);

        os << " Mission: city = " << (pTargetCity ? narrow(pTargetCity->getName()) : "none")
           << " target = " << targetCoords << " mission = " << narrow(missionTypeString)
           << " build = " << (buildType == NO_BUILD ? "none" : gGlobals.getBuildInfo(buildType).getType()) << " mission length = " << length;
    }

    void Unit::Mission::write(FDataStreamBase* pStream) const
    {
        city.write(pStream);
        pStream->Write(startCoords.iX);
        pStream->Write(startCoords.iY);
        pStream->Write(targetCoords.iX);
        pStream->Write(targetCoords.iY);
        pStream->Write(missionType);
        pStream->Write(buildType);
        pStream->Write(length);
    }

    void Unit::Mission::read(FDataStreamBase* pStream)
    {
        city.read(pStream);
        pStream->Read(&startCoords.iX);
        pStream->Read(&startCoords.iY);
        pStream->Read(&targetCoords.iX);
        pStream->Read(&targetCoords.iY);
        pStream->Read((int*)&missionType);
        pStream->Read((int*)&buildType);
        pStream->Read(&length);
    }
}