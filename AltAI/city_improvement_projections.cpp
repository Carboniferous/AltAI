#include "AltAI.h"

#include "./unit.h"
#include "./city_improvement_projections.h"
#include "./city_improvements.h"
#include "./city_data.h"
#include "./helper_fns.h"
#include "./gamedata_analysis.h"
#include "./plot_info_visitors.h"
#include "./civ_log.h"
#include "./error_log.h"

namespace AltAI
{
    ProjectionImprovementEvent::~ProjectionImprovementEvent()
    {
        //ErrorLog::getLog(CvPlayerAI::getPlayer(playerType_))->getStream() << "\ndelete ProjectionImprovementEvent at: " << this
        //    << " ( init = " << (pCityData_ ? "true" : "false");
    }

    ProjectionImprovementEvent::ProjectionImprovementEvent(bool isLand, bool isConsumed, const std::vector<IDInfo>& targetCities, PlayerTypes playerType)
        : isLand_(true), isConsumed_(isConsumed), currentBuild_(NO_BUILD), outputWeights_(makeOutputW(3, 4, 3, 3, 1, 1)),
          improvementBeingBuilt_(NO_IMPROVEMENT), featureBeingRemoved_(NO_FEATURE),
          targetCities_(targetCities), accumulatedTurns_(0), playerType_(playerType),
          currentBuildIsComplete_(false)
    {
    }

    ProjectionImprovementEvent::ProjectionImprovementEvent(const CityImprovementManagerPtr& pCityImprovementManager,
            TotalOutputWeights outputWeights, bool isLand, bool isConsumed, const std::vector<IDInfo>& targetCities,
            BuildTypes currentBuild, ImprovementTypes improvementBeingBuilt, FeatureTypes featureBeingRemoved,
            XYCoords currentTarget, int accumulatedTurns, PlayerTypes playerType, bool currentBuildIsComplete)
        : pCityImprovementManager_(pCityImprovementManager), outputWeights_(outputWeights),
          isLand_(isLand), isConsumed_(isConsumed), targetCities_(targetCities),
          currentBuild_(currentBuild), improvementBeingBuilt_(improvementBeingBuilt), featureBeingRemoved_(featureBeingRemoved),
          currentTarget_(currentTarget), accumulatedTurns_(accumulatedTurns), playerType_(playerType), currentBuildIsComplete_(currentBuildIsComplete)
    {
    }

    void ProjectionImprovementEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
        pCityImprovementManager_ = CityImprovementManagerPtr(new CityImprovementManager(pCityData_->getCity()->getIDInfo()));
        //ErrorLog::getLog(CvPlayerAI::getPlayer(playerType_))->getStream() << "\nnew CityImprovementManager at: " << pCityImprovementManager_.get();
        pCityImprovementManager_->simulateImprovements(pCityData_, 0, __FUNCTION__);
        calcNextImprovement_();
    }

    IProjectionEventPtr ProjectionImprovementEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionImprovementEvent(pCityImprovementManager_,
            outputWeights_, isLand_, isConsumed_, targetCities_,
            currentBuild_, improvementBeingBuilt_, featureBeingRemoved_,
            currentTarget_, accumulatedTurns_, playerType_, currentBuildIsComplete_));
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionImprovementEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionImprovementEvent event: " << " next improvement at = " <<
            XYCoords(currentTarget_.iX, currentTarget_.iY) << " turns = " << getTurnsToEvent() <<
            " improvement = " << (improvementBeingBuilt_ == NO_IMPROVEMENT ? " NONE " : gGlobals.getImprovementInfo(improvementBeingBuilt_).getType()) <<
            " is land = " << isLand_ << ", is consumed = " << isConsumed_;
        os << " targets count = " << targetCities_.size();
        for (size_t i = 0, count = targetCities_.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            else os << " ";
            const CvCity* pCity = ::getCity(targetCities_[i]);
            if (pCity)
            {
                os << narrow(pCity->getName());
            }
        }
    }

    int ProjectionImprovementEvent::getTurnsToEvent() const
    {
        const CvMap& theMap = gGlobals.getMap();
        const CvPlot* pPlot = theMap.plot(currentTarget_.iX, currentTarget_.iY);
        // assumes constant workrate of 100 - this should be dynamic
        return currentBuild_ == NO_BUILD ? MAX_INT : std::max<int>(0, (getImprovementBuildTime(currentBuild_, pPlot->getFeatureType(), pPlot->getTerrainType()) / 100) - accumulatedTurns_);
    }

    bool ProjectionImprovementEvent::targetsCity(IDInfo city) const
    {
        return std::find(targetCities_.begin(), targetCities_.end(), city) != targetCities_.end();
    }

    bool ProjectionImprovementEvent::generateComparison() const
    {
        return false;
    }

    void ProjectionImprovementEvent::updateCityData(int nTurns)
    {
        if (currentBuildIsComplete_)
        {
            PlotDataListIter plotIter = pCityData_->findPlot(currentTarget_);
            plotIter->improvementType = improvementBeingBuilt_;
            if (plotIter->featureType == featureBeingRemoved_)
            {
                plotIter->featureType = NO_FEATURE; // TODO - add any hammers from chopping to output
            }

            std::pair<bool, PlotImprovementData*> plotData = pCityImprovementManager_->findImprovement(currentTarget_);
            if (plotData.first && plotData.second->improvement == improvementBeingBuilt_)
            {
                plotIter->plotYield = plotData.second->yield;
            }

            pCityData_->recalcOutputs();

            if (!isConsumed_)
            {                
                pCityImprovementManager_->simulateImprovements(pCityData_, 0, __FUNCTION__);
                calcNextImprovement_();
            }
            else
            {
                currentBuild_ = NO_BUILD;
            }
        }
    }

    IProjectionEventPtr ProjectionImprovementEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {
        if (currentBuild_ == NO_BUILD)
        {
            return IProjectionEventPtr();
        }
        else
        {
            if (getTurnsToEvent() > nTurns)
            {
                accumulatedTurns_ += nTurns;
            }
            else
            {
                currentBuildIsComplete_ = true;
            }

            return shared_from_this();
        }        
    }

    void ProjectionImprovementEvent::calcNextImprovement_()
    {
        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, int> bestImprovement;
        ImprovementTypes improvementType = NO_IMPROVEMENT;

        std::vector<PlotCondPtr > conditions;
        conditions.push_back(isLand_ ? PlotCondPtr(new IsLand()) : PlotCondPtr(new IsWater()));

        bestImprovement = pCityImprovementManager_->getBestImprovementNotBuilt(false, true, conditions);
        improvementBeingBuilt_ = boost::get<2>(bestImprovement);
        featureBeingRemoved_ = boost::get<1>(bestImprovement);
        if (improvementBeingBuilt_ != NO_IMPROVEMENT || featureBeingRemoved_ != NO_FEATURE)
        {
            currentBuild_ = GameDataAnalysis::getBuildTypeForImprovementType(improvementType);
            currentTarget_ = boost::get<0>(bestImprovement);
            accumulatedTurns_ = 0;
        }
        else
        {
            currentBuild_ = NO_BUILD;
        }
    }

    WorkerBuildEvent::~WorkerBuildEvent()
    {
        //ErrorLog::getLog(CvPlayerAI::getPlayer(pCityData_->getOwner()))->getStream() << "\ndelete WorkerBuildEvent at: " << this;
    }

    WorkerBuildEvent::WorkerBuildEvent(const std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > >& missions) :
        missions_(missions), outputWeights_(makeOutputW(3, 4, 3, 3, 1, 1)), processedBuiltUnitsCount_(0)
    {        
    }

    WorkerBuildEvent::WorkerBuildEvent(const CityImprovementManagerPtr& pCityImprovementManager,
        TotalOutputWeights outputWeights,
        const std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > >& missions,
        size_t processedBuiltUnitsCount)
        : pCityImprovementManager_(pCityImprovementManager),
          outputWeights_(outputWeights), missions_(missions), processedBuiltUnitsCount_(processedBuiltUnitsCount)
    {
    }

    void WorkerBuildEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
        pCityImprovementManager_ = CityImprovementManagerPtr(new CityImprovementManager(pCityData_->getCity()->getIDInfo()));
        //ErrorLog::getLog(CvPlayerAI::getPlayer(pCityData_->getOwner()))->getStream() << "\nnew CityImprovementManager at: " << pCityImprovementManager_.get();
        pCityImprovementManager_->simulateImprovements(pCityData_, 0, __FUNCTION__);
    }

    IProjectionEventPtr WorkerBuildEvent::clone(const CityDataPtr& pCityData) const
    {
        // todo - deep copy missions
        IProjectionEventPtr pCopy(new WorkerBuildEvent(pCityImprovementManager_,
            outputWeights_, missions_, processedBuiltUnitsCount_));
        pCopy->init(pCityData);
        return pCopy;
    }

    void WorkerBuildEvent::debug(std::ostream& os) const
    {
        os << "\n\tWorkerBuildEvent: " << missions_.size() << " units:";
        for (size_t i = 0, count = missions_.size(); i < count; ++i)
        {
            os << "(" << i + 1 << ") " << gGlobals.getUnitInfo(missions_[i].first).getType();
            for (size_t j = 0, count = missions_[i].second.size(); j < count; ++j)
            {
                missions_[i].second[j].debug(os);
            }
        }
    }

    int WorkerBuildEvent::getTurnsToEvent() const
    {
        int minTurnsToBuildEvent = MAX_INT;

        for (size_t i = 0, count = missions_.size(); i < count; ++i)
        {
            int turnsToBuildEvent = 0;
            for (size_t j = 0, unitMissionCount = missions_[i].second.size(); j < unitMissionCount; ++j)
            {
                turnsToBuildEvent += missions_[i].second[j].length;

                if (missions_[i].second[j].buildType != NO_BUILD)
                {
                    minTurnsToBuildEvent = std::min<int>(minTurnsToBuildEvent, turnsToBuildEvent);
                    break;
                }
            }
        }

        // todo - eliminate spurious 0 turn events
        return std::max<int>(1, minTurnsToBuildEvent);
    }

    bool WorkerBuildEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool WorkerBuildEvent::generateComparison() const
    {
        return false;
    }

    void WorkerBuildEvent::updateCityData(int nTurns)
    {
        std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > > missions;
        bool recalcOutputs = false;

        for (size_t i = 0, count = missions_.size(); i < count; ++i)
        {
            std::vector<Unit::WorkerMission> unitMissions;
            int remainingTurns = nTurns;
            bool consumed = false;

            for (size_t j = 0, unitMissionCount = missions_[i].second.size(); j < unitMissionCount; ++j)
            {
                const int currentMissionLength = missions_[i].second[j].length;
                missions_[i].second[j].length -= remainingTurns;
                remainingTurns = std::max<int>(remainingTurns - currentMissionLength, 0);

                if (missions_[i].second[j].length < 1) // completed
                {
                    if (missions_[i].second[j].buildType != NO_BUILD)
                    {
                        const CvBuildInfo& buildInfo = gGlobals.getBuildInfo(missions_[i].second[j].buildType);
                        recalcOutputs = true;
                        consumed = buildInfo.isKill();

                        const XYCoords& targetCoords = missions_[i].second[j].targetCoords;
                        PlotDataListIter plotIter = pCityData_->findPlot(targetCoords);
                        if (plotIter != pCityData_->getPlotOutputs().end())  // mission can be for plot outside city radius - e.g. connecting a bonus
                        {
                            const CvPlot* pTargetPlot = gGlobals.getMap().plot(targetCoords.iX, targetCoords.iY);
                            if (pTargetPlot->getOwner() != pCityData_->getOwner())
                            {
                                continue;
                            }

                            ImprovementTypes newImprovementType = (ImprovementTypes)buildInfo.getImprovement();
                            if (newImprovementType != NO_IMPROVEMENT)
                            {
                                plotIter->improvementType = newImprovementType;
                            }

                            bool isFeatureRemove = plotIter->featureType != NO_FEATURE && gGlobals.getBuildInfo(missions_[i].second[j].buildType).isFeatureRemove(plotIter->featureType);
                            if (plotIter->featureType != NO_FEATURE && isFeatureRemove)
                            {
                                plotIter->featureType = NO_FEATURE; // TODO - add hammers from forest to output
                            }
                            
                            RouteTypes newRouteType = (RouteTypes)gGlobals.getBuildInfo(missions_[i].second[j].buildType).getRoute();
                            bool isBuildRoute = newRouteType != NO_ROUTE;

                            pCityImprovementManager_->updateImprovements(pTargetPlot, newImprovementType, isFeatureRemove ? NO_FEATURE : plotIter->featureType, isBuildRoute ? newRouteType : plotIter->routeType, true);

                            std::pair<bool, PlotImprovementData*> plotData = pCityImprovementManager_->findImprovement(missions_[i].second[j].targetCoords);
                            if (plotData.first && GameDataAnalysis::getBuildTypeForImprovementType(plotData.second->improvement) == missions_[i].second[j].buildType)
                            {
#ifdef ALTAI_DEBUG
                                std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData_->getOwner()))->getStream();
                                os << "\nWorkerBuildEvent at: " << targetCoords << " build = " << buildInfo.getType()
                                    << " plot yield before = " << plotIter->plotYield;
#endif
                                updateCityOutputData(*pCityData_, *plotIter, isFeatureRemove ? plotData.second->removedFeature : NO_FEATURE, isBuildRoute ? newRouteType : plotIter->routeType, plotData.second->improvement);
#ifdef ALTAI_DEBUG
                                os << ", after = " << plotIter->plotYield;
#endif
                            }
                        }
                    }
                }
                else
                {
                    unitMissions.push_back(missions_[i].second[j]);
                }                
            }
            
            if (!consumed)
            {
                missions.push_back(std::make_pair(missions_[i].first, unitMissions));
            }
        }

        if (recalcOutputs)
        {
            pCityData_->recalcOutputs();
        }        

        updateMissions_(missions);
    }

    IProjectionEventPtr WorkerBuildEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {
        if (ladder.units.size() > processedBuiltUnitsCount_)
        {
            std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > > missions;
            for (size_t i = processedBuiltUnitsCount_; i < ladder.units.size(); ++i)
            {
                // did we build a worker unit?
                if (gGlobals.getUnitInfo(ladder.units[i].unitType).getDefaultUnitAIType() == UNITAI_WORKER)
                {
                    // will update new missions in updateCityData call
                    missions_.push_back(std::make_pair(ladder.units[i].unitType, std::vector<Unit::WorkerMission>()));
                }
            }

            processedBuiltUnitsCount_ = ladder.units.size();
        }

        if (missions_.empty())
        {
            return IProjectionEventPtr();
        }
        else
        {
            return shared_from_this();
        }
    }

    void WorkerBuildEvent::updateMissions_(std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > >& missions)
    {
        std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > > finalMissions;

        for (size_t i = 0, count = missions.size(); i < count; ++i)
        {
            if (missions[i].second.empty())
            {
                checkForNewMissions_(missions, i);
            }

            if (!missions[i].second.empty())
            {
                finalMissions.push_back(missions[i]);
            }
        }

        missions_ = finalMissions;
    }

    void WorkerBuildEvent::checkForNewMissions_(std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > >& missions, const size_t index)
    {
        std::vector<PlotCondPtr > conditions;

        conditions.push_back(gGlobals.getUnitInfo(missions[index].first).getDomainType() == DOMAIN_LAND ?
            PlotCondPtr(new IsLand()) : PlotCondPtr(new IsWater()));

        for (size_t i = 0, count = missions.size(); i < count; ++i)
        {
            for (size_t j = 0, unitMissionCount = missions[i].second.size(); j < unitMissionCount; ++j)
            {
                if (missions[i].second[j].buildType != NO_BUILD)
                {
                    conditions.push_back(PlotCondPtr(
                        new IgnorePlot(gGlobals.getMap().plot(missions[i].second[j].targetCoords.iX, missions[i].second[j].targetCoords.iY))));
                }
            }
        }

        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, int> bestImprovement = 
            pCityImprovementManager_->getBestImprovementNotBuilt(false, true, conditions);

        ImprovementTypes improvementType = boost::get<2>(bestImprovement);

        if (improvementType != NO_IMPROVEMENT)
        {
            FeatureTypes featureType = boost::get<1>(bestImprovement);
            XYCoords newTargetCoords = boost::get<0>(bestImprovement);
            BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(improvementType);

            // get this unit's location from last mission target in original list
            XYCoords currentLocation = missions[index].second.empty() ? 
                XYCoords(pCityData_->getCity()->plot()->getCoords()) : missions[index].second.rbegin()->targetCoords;

            const CvPlot* pTargetPlot = gGlobals.getMap().plot(newTargetCoords.iX, newTargetCoords.iY);

            if (currentLocation != newTargetCoords)
            {
                int stepDistance = gGlobals.getMap().calculatePathDistance(gGlobals.getMap().plot(currentLocation.iX, currentLocation.iY), 
                    gGlobals.getMap().plot(newTargetCoords.iX, newTargetCoords.iY));

                if (stepDistance < 0)
                {
                    return;
                }

                missions[index].second.push_back(Unit::WorkerMission(missions_[index].second.begin()->unit, pCityData_->getCity()->getIDInfo(), newTargetCoords, MISSION_MOVE_TO, NO_BUILD, 0, stepDistance, currentLocation));
            }

            bool buildRemovesFeature = featureType != NO_FEATURE ? GameDataAnalysis::doesBuildTypeRemoveFeature(buildType, featureType) : false;

            if (buildRemovesFeature)
            {
                missions[index].second.push_back(
					Unit::WorkerMission(missions_[index].second.begin()->unit, pCityData_->getCity()->getIDInfo(), newTargetCoords, MISSION_BUILD,
                        GameDataAnalysis::getBuildTypeToRemoveFeature(featureType), 0, 3, newTargetCoords));
            }

            missions[index].second.push_back(Unit::WorkerMission(missions_[index].second.begin()->unit, pCityData_->getCity()->getIDInfo(), newTargetCoords, MISSION_BUILD, buildType, 0, 5, newTargetCoords));

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData_->getOwner()))->getStream();
            os << "\nWorkerBuildEvent: added missions: ";
            for (size_t i = 0, count = missions[index].second.size(); i < count; ++i)
            {
                os << "\n\t";
                missions[index].second[i].debug(os);
            }
#endif
        }
    }
}