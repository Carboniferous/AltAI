#include "AltAI.h"

#include "./iters.h"
#include "./game.h"
#include "./player.h"
#include "./unit_tactics.h"
#include "./utils.h"
#include "./gamedata_analysis.h"
#include "./map_analysis.h"
#include "./unit.h"
#include "./unit_log.h"
#include "./civ_log.h"
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
        struct MoveToRouteData
        {
            MoveToRouteData() : moveCost(MAX_INT), movesLeft(MAX_INT) {}
            MoveToRouteData(XYCoords coords_, size_t moveCost_, size_t movesLeft_)
                : coords(coords_), moveCost(moveCost_), movesLeft(movesLeft_) {}

            XYCoords coords;
            size_t moveCost, movesLeft;
        };

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
            std::vector<MoveToRouteData> routeData;
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
            gDLL->getFAStarIFace()->Initialize(pStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), stepDestValid, pathHeuristic, pathCost, areaStepValidWithFlags, stepAdd, NULL, NULL);
            gDLL->getFAStarIFace()->SetData(pStepFinder, pUnit_->getGroup());

            int totalCost = MAX_INT;
            if (gDLL->getFAStarIFace()->GeneratePath(pStepFinder, pStartPlot->getX(), pStartPlot->getY(), pTargetPlot->getX(), pTargetPlot->getY(), false, stepFinderInfo_, false))
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
                    routeData.push_back(MoveToRouteData((*pathIter)->getCoords(), plotMovementCost, availableMoves));
#endif
                    if (availableMoves == 0)
                    {
                        ++actualCost; // cost us a move to get onto the plot
                        availableMoves = unitBaseMoves_;
                    }

                    totalCost += actualCost;
                    pPreviousPlot = pPlot;
                }                
            }

            if (pUnit_->getDomainType() != DOMAIN_SEA)
            {
                gDLL->getFAStarIFace()->destroy(pStepFinder);
            }
#ifdef ALTAI_DEBUG
            for (size_t i = 0; i < routeData.size(); ++i)
            {
                if (i > 0) os << ", ";
                os << routeData[i].coords << " l=" << routeData[i].movesLeft << " c=" << routeData[i].moveCost;
            }
#endif
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
        int calculateBuildMissionLength(const CvUnitAI* pUnit, const CvPlot* pStartPlot, const CvPlot* pTargetPlot, BuildTypes buildType, int iFlags)
        {
            int missionLength = 0;

            if (pTargetPlot->getX() != pStartPlot->getX() || pTargetPlot->getY() != pStartPlot->getY())
            {
                UnitPathData unitPathData;
                unitPathData.calculate(pUnit->getGroup(), pTargetPlot, iFlags);
                //MoveToMission moveToMission(pUnit);
                missionLength += unitPathData.pathTurns; // moveToMission(pStartPlot, pTargetPlot);
            }

            if (missionLength != MAX_INT)
            {
                const int buildTime = pTargetPlot->getBuildTime(buildType), buildProgress = pTargetPlot->getBuildProgress(buildType), workRate = pUnit->workRate(true);
                missionLength += (buildTime - buildProgress) / workRate;
                if ((buildTime - buildProgress) % workRate > 0)
                {
                    ++missionLength;
                }
            }

            return missionLength;
        }

        int calculateRouteToMissionLength(const CvUnitAI* pUnit, const CvPlot* pStartPlot, const CvPlot* pTargetPlot, RouteTypes routeType)
        {
            int missionLength = 0;

            if (pStartPlot->getCoords() != pTargetPlot->getCoords())
            {
                RouteToMission routeToMission(pUnit, routeType);
                missionLength += routeToMission(pStartPlot, pTargetPlot);
            }

            return missionLength;
        }

        int calculateMoveToMissionLength(const CvUnitAI* pUnit, const CvPlot* pStartPlot, const CvPlot* pTargetPlot)
        {
            int missionLength = 0;

            if (pStartPlot->getCoords() != pTargetPlot->getCoords())
            {
                MoveToMission moveToMission(pUnit);
                missionLength += moveToMission(pStartPlot, pTargetPlot);
            }

            return missionLength;
        }
    }

    Unit::Unit(CvUnitAI* pUnit) : pUnit_(pUnit)
    {
    }

    bool Unit::operator < (const Unit& other) const
    {
        return pUnit_->getID() < other.pUnit_->getID();
    }

    void Unit::pushWorkerMission(size_t turn, const CvCity* pCity, const CvPlot* pTargetPlot, MissionTypes missionType, BuildTypes buildType, int iFlags)
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
            if (stepDistance(pTargetPlot->getX(), pTargetPlot->getY(), startCoords.iX, startCoords.iY) <= 1)
            {
                thisMissionLength = 1;
            }
            else
            {
                // todo - add flags if have escort
                UnitPathData unitPathData;
                unitPathData.calculate(pUnit_->getGroup(), pTargetPlot, iFlags);
                // try and pick out the steps from startCoords
                int length = unitPathData.getLengthToEndFrom(startCoords);
                thisMissionLength = length == -1 ? unitPathData.pathTurns : length;
            }
        }
        else if (missionType == MISSION_BUILD)
        {
            thisMissionLength = calculateBuildMissionLength(pUnit_, pStartPlot, pTargetPlot, buildType, iFlags);
        }
        else if (missionType == MISSION_MOVE_TO_UNIT)
        {
            thisMissionLength = calculateMoveToMissionLength(pUnit_, pStartPlot, pTargetPlot);
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

        workerMissions_.push_back(WorkerMission(pUnit_, pCity, pTargetPlot, missionType, buildType, iFlags, thisMissionLength, startCoords));

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
    /*void Unit::updateMission()
    {
        if (workerMissions_.empty())
        {
            return;
        }

        if (workerMissions_[0].missionType == MISSION_BUILD)
        {
            workerMissions_[0].length = !pUnit_->canBuild(workerMissions_[0].getTarget(), workerMissions_[0].buildType)
                ? 0 : calculateBuildMissionLength(pUnit_, pUnit_->plot(), workerMissions_[0].getTarget(), workerMissions_[0].buildType, workerMissions_[0].iFlags);
        }
        else if (workerMissions_[0].missionType == MISSION_ROUTE_TO)
        {
            workerMissions_[0].length = calculateRouteToMissionLength(pUnit_, pUnit_->plot(),workerMissions_[0].getTarget(), (RouteTypes)gGlobals.getBuildInfo(workerMissions_[0].buildType).getRoute());
        }

        if (workerMissions_[0].length == 0)
        {
            std::vector<WorkerMission> remainingMissions;
            for (size_t i = 1, count = workerMissions_.size(); i < count; ++i)
            {
                remainingMissions.push_back(workerMissions_[i]);
            }
            workerMissions_ = remainingMissions;
        }
    }*/

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
            std::vector<WorkerMission> remainingMissions;
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

    bool Unit::hasWorkerMissionAt(XYCoords targetCoords) const
    {
        for (std::vector<WorkerMission>::const_iterator ci(workerMissions_.begin()), ciEnd(workerMissions_.end()); ci != ciEnd; ++ci)
        {
            if (ci->isWorkerMissionAt(targetCoords))
            {
                return true;
            }
        }

        return false;
    }

    bool Unit::hasBuildOrRouteMission() const
    {
        for (std::vector<WorkerMission>::const_iterator ci(workerMissions_.begin()), ciEnd(workerMissions_.end()); ci != ciEnd; ++ci)
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
        for (std::vector<WorkerMission>::const_iterator ci(workerMissions_.begin()), ciEnd(workerMissions_.end()); ci != ciEnd; ++ci)
        {
            if (ci->isWorkerMissionFor(pCity))
            {
                return true;
            }
        }

        return false;
    }

    const std::vector<Unit::WorkerMission>& Unit::getWorkerMissions() const
    {
        return workerMissions_;
    }

    std::vector<Unit::WorkerMission>& Unit::getWorkerMissions()
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
            workerMissions_.push_back(WorkerMission(pUnit_));
            int typeID;
            pStream->Read(&typeID);  // for factory creation - ignore here
            workerMissions_.rbegin()->read(pStream);
        }
    }

    Unit::WorkerMission::WorkerMission() :
        targetCoords(), missionType(NO_MISSION), buildType(NO_BUILD), iFlags(0), length(-1), startCoords()
    {
    }

    Unit::WorkerMission::WorkerMission(const CvUnitAI* pUnit)
        : unit(pUnit->getIDInfo()), 
          targetCoords(), 
          missionType(NO_MISSION), buildType(NO_BUILD), iFlags(0), length(-1), startCoords()
    {
    }

    Unit::WorkerMission::WorkerMission(IDInfo unit_, IDInfo city_, XYCoords targetCoords_, MissionTypes missionType_, BuildTypes buildType_, int iFlags_, int length_, XYCoords startCoords_)
        : unit(unit_), city(city_), targetCoords(targetCoords_), 
          missionType(missionType_), buildType(buildType_), iFlags(iFlags_),
          length(length_), startCoords(startCoords_)
    {
    }

    Unit::WorkerMission::WorkerMission(const CvUnitAI* pUnit, const CvCity* pCity, const CvPlot* pTargetPlot, MissionTypes missionType_, BuildTypes buildType_, int iFlags_, int length_, XYCoords startCoords_)
        : unit(pUnit->getIDInfo()), city(pCity ? pCity->getIDInfo() : IDInfo()), 
          targetCoords(pTargetPlot->getCoords()), 
          missionType(missionType_), buildType(buildType_), iFlags(iFlags_), length(length_), startCoords(startCoords_)
    {
    }

    bool Unit::WorkerMission::isWorkerMissionAt(XYCoords coords) const
    {
        return (missionType == MISSION_ROUTE_TO || missionType == MISSION_BUILD) && buildType != NO_BUILD && targetCoords == coords;
    }

    bool Unit::WorkerMission::isWorkerMissionFor(const CvCity* pCity) const
    {
        return pCity->getIDInfo() == city;
    }

    bool Unit::WorkerMission::canStartMission() const
    {
        const CvUnit* pUnit = ::getUnit(unit);
        if (pUnit)
        {
            const bool unitAtPlot = pUnit->plot()->getCoords() == targetCoords;
            const bool unitCanBuild = buildType != NO_BUILD && pUnit->canBuild(gGlobals.getMap().plot(targetCoords.iX, targetCoords.iY), buildType);
            if (missionType == MISSION_MOVE_TO)
            {
                if (!unitAtPlot)
                {
                    UnitPathData unitPathData;
                    // todo - add flags if have escort
                    unitPathData.calculate(pUnit->getGroup(), gGlobals.getMap().plot(targetCoords.iX, targetCoords.iY), iFlags);

                    return unitPathData.valid;
                }
                else
                {
                    return false;
                }
            }
            else if (missionType == MISSION_BUILD)  // used for road building now too - so MISSION_ROUTE_TO is probably not required here
            {
                return unitAtPlot && unitCanBuild;
            }
            else if (missionType == MISSION_ROUTE_TO)
            {
                // either we're at the target plot, in which case check we need to build a route there, or..
                // unit is not at the target plot - in which case - can we build the route type on this plot before moving to the target
                return (unitAtPlot && unitCanBuild) || (!unitAtPlot && pUnit->canBuild(pUnit->plot(), buildType));
            }
            else if (missionType == MISSION_SKIP)
            {
                return true;
            }
        }
        return false;
    }

    bool Unit::WorkerMission::isBorderMission(const boost::shared_ptr<MapAnalysis>& pMapAnalysis) const
    {
        const CvPlot* pMissionTarget = getTarget();
        bool isBorderMission = false;
        if (pMissionTarget)
        {                                        
            PlayerTypes targetOwner = pMissionTarget->getOwner();
            isBorderMission = targetOwner == NO_PLAYER || pMapAnalysis->isOurBorderPlot(pMissionTarget->getSubArea(), pMissionTarget->getCoords());
        }
        return isBorderMission;
    }

    const CvUnitAI* Unit::WorkerMission::getUnit() const
    {
        return (const CvUnitAI*)::getUnit(unit);
    }

    CvPlot* Unit::WorkerMission::getTarget() const
    {
        return gGlobals.getMap().plot(targetCoords.iX, targetCoords.iY);
    }

    bool Unit::WorkerMission::isComplete() const
    {
        const CvUnit* pUnit = ::getUnit(unit);
        if (!pUnit)
        {
            return false;
        }

        switch (missionType)
        {
            case MISSION_MOVE_TO:
                {
                    bool missionComplete = pUnit->at(targetCoords.iX, targetCoords.iY);
#ifdef ALTAI_DEBUG
                    if (!missionComplete)
                    {
                        std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner())->getCvPlayer())->getStream();
                        os << "\nmission not completed for move to: " << targetCoords;
                    }
#endif
                    return missionComplete;
                }
            case MISSION_ROUTE_TO:
                {
#ifdef ALTAI_DEBUG
                    std::ostream& os = UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner())->getCvPlayer())->getStream();
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
                        const int stepFinderInfo = MAKEWORD((short)pUnit->getOwner(), (short)(SubAreaStepFlags::Team_Territory | SubAreaStepFlags::Unowned_Territory));

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
                    std::ostream& os = CivLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner())->getCvPlayer())->getStream(); //UnitLog::getLog(*gGlobals.getGame().getAltAI()->getPlayer(pUnit_->getOwner())->getCvPlayer())->getStream();
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
                return pUnit->getGroup()->getActivityType() == ACTIVITY_AWAKE;
            default:
                return false;
        }
    }

    CvCity* Unit::WorkerMission::getTargetCity() const
    {
        return city.eOwner == NO_PLAYER ? NULL : ::getCity(city);
    }

    UnitAITypes Unit::WorkerMission::getAIType() const
    {
        return UNITAI_WORKER;
    }

    void Unit::WorkerMission::update()
    {
        const CvUnitAI* pUnit = (const CvUnitAI*)::getUnit(unit);
        if (pUnit)
        {
            const CvPlot* pTargetPlot = gGlobals.getMap().plot(targetCoords.iX, targetCoords.iY),
                *pStartPlot = gGlobals.getMap().plot(startCoords.iX, startCoords.iY);

            if (missionType == MISSION_BUILD)
            {
                length = !pUnit->canBuild(pTargetPlot, buildType) ? 0 : calculateBuildMissionLength(pUnit, pStartPlot, pTargetPlot, buildType, iFlags);
            }
            else if (missionType == MISSION_ROUTE_TO)
            {
                //UnitPathData unitPathData;
                //unitPathData.calculate(pUnit->getGroup(), pTargetPlot, iFlags);
                //length = unitPathData.pathTurns; // calculateRouteToMissionLength(pUnit, pUnit->plot(), pTargetPlot, (RouteTypes)gGlobals.getBuildInfo(buildType).getRoute());
                length = calculateRouteToMissionLength(pUnit, pUnit->plot(), pTargetPlot, (RouteTypes)gGlobals.getBuildInfo(buildType).getRoute());
            }
            else if (missionType == MISSION_MOVE_TO)
            {
                length = calculateMoveToMissionLength(pUnit, pUnit->plot(), pTargetPlot);
            }
            // skip MISSION_SKIP
        }        
    }

    void Unit::WorkerMission::debug(std::ostream& os) const
    {
        CvWString missionTypeString;
        getMissionTypeString(missionTypeString, missionType);
        const CvCity* pTargetCity = ::getCity(city);

        os << "\n\t WorkerMission: unit: " << unit << " city = " << (pTargetCity ? narrow(pTargetCity->getName()) : "none")
           << " target = " << targetCoords << " mission = " << narrow(missionTypeString);
        if (missionType == MISSION_MOVE_TO)
        {
            os << " coords = " << targetCoords;
        }
        else if (missionType == MISSION_BUILD)
        {
            os << " build = " << (buildType == NO_BUILD ? "none" : gGlobals.getBuildInfo(buildType).getType());
        }
        os << " mission length = " << length;
    }

    void Unit::WorkerMission::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        unit.write(pStream);
        city.write(pStream);
        startCoords.write(pStream);
        targetCoords.write(pStream);        
        pStream->Write(missionType);
        pStream->Write(buildType);
        pStream->Write(iFlags);
        pStream->Write(missionFlags);
        pStream->Write(length);
    }

    void Unit::WorkerMission::read(FDataStreamBase* pStream)
    {
        unit.read(pStream);
        city.read(pStream);
        startCoords.read(pStream);
        targetCoords.read(pStream);
        pStream->Read((int*)&missionType);
        pStream->Read((int*)&buildType);
        pStream->Read(&iFlags);
        pStream->Read(&missionFlags);
        pStream->Read(&length);
    }

    void debugUnit(std::ostream& os, const CvUnit* pUnit)
    {
        os << pUnit->getUnitInfo().getType() << " at: " << pUnit->plot()->getCoords();
        if (pUnit->currHitPoints() < pUnit->maxHitPoints())
        {
            os << " hp = " << pUnit->currHitPoints() << ' ';
        }

        bool hasPromotions = false;
        for (size_t i = 0, count = gGlobals.getNumPromotionInfos(); i < count; ++i)
        {
            if (pUnit->isHasPromotion((PromotionTypes)i))
            {
                if (!hasPromotions) { hasPromotions = true; os << "{"; } else { os << " "; }
                os << gGlobals.getPromotionInfo((PromotionTypes)i).getType();
            }
        }
        if (hasPromotions) os << "}";
    }

    void debugUnitVector(std::ostream& os, const std::vector<const CvUnit*>& units)
    {
        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            debugUnit(os, units[i]);
        }
    }

    void debugUnitIDInfoVector(std::ostream& os, const std::vector<IDInfo>& units)
    {
        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << units[i] << " - ";
            const CvUnit* pUnit = ::getUnit(units[i]);
            if (pUnit)
            {
                debugUnit(os, pUnit);
            }
            else
            {
                os << " (not found) ";
            }
        }
    }

    void debugUnitSet(std::ostream& os, const std::set<IDInfo>& units)
    {
        bool first = true;
        for (std::set<IDInfo>::const_iterator unitsIter(units.begin()), unitsEndIter(units.end()); unitsIter != unitsEndIter; ++unitsIter)
        {
            if (!first) os << ", ";
            os << *unitsIter << " - ";
            const CvUnit* pUnit = ::getUnit(*unitsIter);
            if (pUnit)
            {
                debugUnit(os, pUnit);
            }
            else
            {
                os << " (not found)";
            }
            first = false;
        }
    }
}