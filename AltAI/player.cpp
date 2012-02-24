#include "./utils.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./unit.h"
#include "./plot_info.h"
#include "./gamedata_analysis.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./settler_manager.h"
#include "./events.h"
#include "./civ_helper.h"
#include "./area_helper.h"
#include "./civ_log.h"
#include "./unit_log.h"
#include "./error_log.h"
#include "./map_log.h"
#include "./civic_info_visitors.h"
#include "./tech_info_visitors.h"
#include "./plot_info_visitors.h"
#include "./city_simulator.h"
#include "./tictacs.h"
#include "./save_utils.h"

namespace AltAI
{
    Player::Player(CvPlayer* pPlayer)
        : pPlayer_(pPlayer)
    {
        
        pPlayerAnalysis_ = boost::shared_ptr<PlayerAnalysis>(new PlayerAnalysis(*this));
        pCivHelper_ = boost::shared_ptr<CivHelper>(new CivHelper(*pPlayer_));
        pSettlerManager_ = boost::shared_ptr<SettlerManager>(new SettlerManager(pPlayerAnalysis_->getMapAnalysis()));
    }

    void Player::init()
    {
        AreaIter iter(&gGlobals.getMap());

        while (CvArea* pArea = iter())
        {
            if (pArea->getCitiesPerPlayer(pPlayer_->getID()) > 0)
            {
                areaHelpersMap_[pArea->getID()] = boost::shared_ptr<AreaHelper>(new AreaHelper(*pPlayer_, pArea));
            }
        }

        pPlayerAnalysis_->init();

        pPlayerAnalysis_->getMapAnalysis()->init();

#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();
#endif
        
        GameDataAnalysis::getInstance()->analyseForPlayer(*this);

        pPlayerAnalysis_->getMapAnalysis()->analyseSharedPlots();
        pSettlerManager_->analysePlotValues();

#ifdef ALTAI_DEBUG
        pPlayerAnalysis_->getMapAnalysis()->debug(os);
        pPlayerAnalysis_->getMapAnalysis()->debugSharedPlots();
#endif

        for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
            // initialise city
            iter->second.init();

#ifdef ALTAI_DEBUG
            os << "\n" << narrow(iter->second.getCvCity()->getName()) << ":\n";
            os << "Max outputs = " << iter->second.getMaxOutputs() << " weights = " << iter->second.getMaxOutputWeights() << "\n";

            iter->second.logImprovements();
#endif
        }

        citiesToInit_.clear();

        pPlayerAnalysis_->postCityInit();

        calcCivics_();

        for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
            iter->second.connectCities(NULL);
        }
    }

    void Player::doTurn()
    {
        if (pPlayer_->isAlive())
        {
            for (std::set<int>::const_iterator ci(citiesToInit_.begin()), ciEnd(citiesToInit_.end()); ci != ciEnd; ++ci)
            {
                CityMap::iterator iter(cities_.find(*ci));
                if (iter != cities_.end())
                {
                    iter->second.init();
                }
            }
            citiesToInit_.clear();

            // handle plot updates
            //pPlayerAnalysis_->getMapAnalysis()->update();

            calcMaxResearchRate_();

            for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
            {
                iter->second.doTurn();
            }

#ifdef ALTAI_DEBUG
            {
                std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
                for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
                {
                    std::pair<int, int> rank = getCityRank(iter->second.getCvCity()->getIDInfo(), OUTPUT_PRODUCTION);
                    os << "\n" << narrow(iter->second.getCvCity()->getName()) << " rank = " << rank.first << " output (prod) = " << rank.second;
                }
            }
#endif

#ifdef ALTAI_DEBUG
            pPlayerAnalysis_->getMapAnalysis()->debugResourceData();
#endif

#ifdef ALTAI_DEBUG
            {
                boost::shared_ptr<UnitLog> pUnitLog = UnitLog::getLog(*pPlayer_);
                std::ostream& os = pUnitLog->getStream();
                os << "\nTurn = " << gGlobals.getGame().getGameTurn();
                pUnitLog->logSelectionGroups();
            }
#endif
        }
    }

    void Player::addCity(CvCity* pCity)
    {
        cities_.insert(std::make_pair(pCity->getID(), City(pCity)));
        citiesToInit_.insert(pCity->getID());

        // updates shared plot data (if not init yet - init() above will call MapAnalysis)
        if (gGlobals.getGame().getAltAI()->isInit())
        {
            pPlayerAnalysis_->getMapAnalysis()->addCity(pCity);
        }
    }

    void Player::deleteCity(CvCity* pCity)
    {
        pPlayerAnalysis_->getMapAnalysis()->deleteCity(pCity);
        cities_.erase(pCity->getID());
    }

    City& Player::getCity(int ID)
    {
        CityMap::iterator iter = cities_.find(ID);
        if (iter != cities_.end())
        {
            return iter->second;
        }

        {
            boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(*pPlayer_);
            std::ostream& os = pErrorLog->getStream();

            std::ostringstream oss;
            oss << "\nFailed to find city with ID: " << ID << "\n";
            os << oss.str();
            throw std::runtime_error(oss.str());  // todo - change this to fatal error message
        }
    }

    const City& Player::getCity(int ID) const
    {
        CityMap::const_iterator iter = cities_.find(ID);
        if (iter != cities_.end())
        {
            return iter->second;
        }

        {
            boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(*pPlayer_);
            std::ostream& os = pErrorLog->getStream();

            std::ostringstream oss;
            oss << "\nFailed to find city with ID: " << ID << "\n";
            os << oss.str();
            throw std::runtime_error(oss.str());
        }
    }

    void Player::calcCivics_()
    {

#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();
        os << "\nAvailable civics: ";
#endif

        std::vector<std::pair<CivicTypes, TotalOutput> > outputs;

        for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
        {
            if (pCivHelper_->civicIsAvailable((CivicTypes)i))
            {
                TotalOutput civWideOutput;
                
                std::vector<boost::shared_ptr<CityData> > cityData;
                for (CityMap::const_iterator ci(cities_.begin()), ciEnd(cities_.end()); ci != ciEnd; ++ci)
                {
                    cityData.push_back(boost::shared_ptr<CityData>(new CityData(ci->second.getCvCity())));
                }

                bool hasEconomicValue = false;
                for (size_t j = 0, count = cityData.size(); j < count; ++j)
                {
                    hasEconomicValue = civicHasEconomicImpact(*cityData[j], getAnalysis()->getCivicInfo((CivicTypes)i));
                    if (hasEconomicValue)
                    {
                        break;
                    }
                }

                if (hasEconomicValue)
                {
                    for (size_t j = 0, count = cityData.size(); j < count; ++j)
                    {
                        updateRequestData(cityData[j]->getCity(), *cityData[j], getAnalysis(), (CivicTypes)i);

                        CitySimulation simulation(cityData[j]->getCity(), cityData[j]);
                        SimulationOutput simOutput = simulation.simulateAsIs(20);
                        civWideOutput += *simOutput.cumulativeOutput.rbegin();
                    }
                    outputs.push_back(std::make_pair((CivicTypes)i, civWideOutput));
                }
#ifdef ALTAI_DEBUG
                else
                {
                    os << "\n" << gGlobals.getCivicInfo((CivicTypes)i).getType() << " has no current ecomonic value\n";
                }
#endif
            }
        }

#ifdef ALTAI_DEBUG
        for (size_t i = 0, count = outputs.size(); i < count; ++i)
        {
            os << "\n" << gGlobals.getCivicInfo(outputs[i].first).getType() << " = " << outputs[i].second;
        }
#endif

        TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 1, 1);
        std::map<CivicOptionTypes, std::pair<CivicTypes, TotalOutput> > bestCivics;

        for (int i = 0; i < NUM_OUTPUT_TYPES; ++i)
        {
            weights[i] = 10;
            TotalOutputValueFunctor valueF(weights);
            TotalOutput bestOutput;

            for (size_t j = 0, count = outputs.size(); j < count; ++j)
            {
                CivicOptionTypes civicOptionType = (CivicOptionTypes)gGlobals.getCivicInfo(outputs[j].first).getCivicOptionType();

                if (valueF(outputs[j].second, bestCivics[civicOptionType].second))
                {
                    bestCivics[civicOptionType] = outputs[j];
                }
            }

#ifdef ALTAI_DEBUG
            os << "\n" << weights;
            for (std::map<CivicOptionTypes, std::pair<CivicTypes, TotalOutput> >::const_iterator ci(bestCivics.begin()), ciEnd(bestCivics.end()); ci != ciEnd; ++ci)
            {
                os << " (" << gGlobals.getCivicOptionInfo(ci->first).getType() << ") " << gGlobals.getCivicInfo(ci->second.first).getType();
            }
            os << "\n";
#endif

            weights[i] = 1;
        }

#ifdef ALTAI_DEBUG
        os << "\n";
#endif
    }

    void Player::addUnit(CvUnitAI* pUnit)
    {
        units_.insert(Unit(pUnit));

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();
            os << "\nUnit (ID=" << pUnit->getID() << ") created, player = " << pUnit->getOwner() << "\n";
        }
#endif
    }

    void Player::deleteUnit(CvUnitAI* pUnit)
    {
        units_.erase(Unit(pUnit));

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();
            os << "Unit (ID=" << pUnit->getID() << ") deleted, player = " << pUnit->getOwner() << "\n";
        }
#endif
    }

    void Player::moveUnit(CvUnitAI* pUnit, CvPlot* pFromPlot, CvPlot* pToPlot)
    {
        UnitsCIter ci(units_.find(Unit(pUnit)));

#ifdef ALTAI_DEBUG
        if (ci != units_.end())
        {
            std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();
            XYCoords from(pFromPlot->getX(), pFromPlot->getY()), to(pToPlot->getX(), pToPlot->getY());
            os << "\nUnit (ID = " << pUnit->getID() << ") moved from: " << from << " to: " << to << "\n";
        }
#endif
    }

    void Player::movePlayerUnit(CvUnitAI* pUnit, CvPlot* pPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();

        os << "\nPlayer: " << pUnit->getOwner() << " moves unit: " << pUnit->getUnitInfo().getType()
            << " to: " << XYCoords(pPlot->getX(), pPlot->getY());
#endif
    }

    void Player::hidePlayerUnit(CvUnitAI* pUnit, CvPlot* pOldPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();

        os << "\nPlayer: " << pUnit->getOwner() << " moves unit: " << pUnit->getUnitInfo().getType()
            << " at: " << XYCoords(pOldPlot->getX(), pOldPlot->getY()) << " out of view";
#endif
    }

    const CvPlayer* Player::getCvPlayer() const
    {
        return pPlayer_;
    }

    CvPlayer* Player::getCvPlayer()
    {
        return pPlayer_;
    }

    PlayerTypes Player::getPlayerID() const
    {
        return pPlayer_->getID();
    }

    TeamTypes Player::getTeamID() const
    {
        return pPlayer_->getTeam();
    }

    const boost::shared_ptr<PlayerAnalysis>& Player::getAnalysis() const
    {
        return pPlayerAnalysis_;
    }

    const boost::shared_ptr<CivHelper>& Player::getCivHelper() const
    {
        return pCivHelper_;
    }

    const boost::shared_ptr<AreaHelper>& Player::getAreaHelper(int areaID)
    {
        std::map<int, boost::shared_ptr<AreaHelper> >::iterator iter = areaHelpersMap_.find(areaID);
        if (iter == areaHelpersMap_.end())
        {
            return areaHelpersMap_.insert(std::make_pair(areaID, boost::shared_ptr<AreaHelper>(new AreaHelper(*pPlayer_, gGlobals.getMap().getArea(areaID))))).first->second;
        }
        else
        {
            return iter->second;
        }
    }

    bool Player::checkResourcesOutsideCities(CvUnitAI* pUnit) const
    {
        const int subAreaID = pUnit->plot()->getSubArea();
        CvMap& theMap = gGlobals.getMap();
        
        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
	    {
		    CvPlot* pPlot = theMap.plotByIndex(i);
            if (pPlot->getOwner() == pPlayer_->getID() && pPlot->getSubArea() == subAreaID)
            {
                BonusTypes bonusType = pPlot->getBonusType(pPlayer_->getTeam());
                if (bonusType != NO_BONUS)
                {
                    ImprovementTypes improvementType = pPlot->getImprovementType();
                    ImprovementTypes requiredImprovement = getBonusImprovementType(bonusType);
                    // todo - does this mean we'll get/leave forts?
                    if (improvementType == NO_IMPROVEMENT && requiredImprovement != NO_IMPROVEMENT)
                    {
                        if (getNumWorkersAtPlot(pPlot) > 0)
                        {
                            continue;
                        }

                        CityIter cityIter(*pPlayer_);
                        CvCity* pLoopCity, *pBestCity = NULL;
                        int distanceToNearestCity = MAX_INT;
                        while (pLoopCity = cityIter())
                        {
                            if (pLoopCity->plot()->getSubArea() == subAreaID)
                            {
                                int distance = theMap.calculatePathDistance(pPlot, pLoopCity->plot());
                                if (distance < distanceToNearestCity)
                                {
                                    distanceToNearestCity = distance;
                                    pBestCity = pLoopCity;
                                }
                            }
                        }

                        BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(requiredImprovement);

                        if (buildType != NO_BUILD && pPlayer_->canBuild(pPlot, buildType))
                        {
                            if (!pUnit->atPlot(pPlot))
                            {
                                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pPlot->getX(), pPlot->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
                            }

                            pUnit->getGroup()->pushMission(MISSION_BUILD, buildType, -1, 0, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);

                            if (pBestCity)
                            {
                                pUnit->getGroup()->pushMission(MISSION_ROUTE_TO, pBestCity->getX(), pBestCity->getY(), MOVE_SAFE_TERRITORY, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);
                            }
#ifdef ALTAI_DEBUG
                            {   // debug
                                boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pPlayer_->getID()));
                                std::ostream& os = pCivLog->getStream();
                                os << "\nRequested connection of resource: " << (bonusType == NO_BONUS ? " none? " : gGlobals.getBonusInfo(bonusType).getType()) << " at: " << XYCoords(pPlot->getX(), pPlot->getY());

                                if (pBestCity)
                                {
                                    os << " to: " << narrow(pBestCity->getName());
                                }
                            }
#endif
    					    return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    int Player::getNumWorkersAtPlot(const CvPlot* pTargetPlot) const
    {
        int count = 0;

        SelectionGroupIter iter(*pPlayer_);
        CvSelectionGroup* pGroup = NULL;

        while (pGroup = iter())
        {
            bool potentialWorkerGroup = false;
            bool idleWorkers = false;
            const CvPlot* pPlot = NULL;
            if (pGroup->AI_getMissionAIType() == NO_MISSIONAI)
            {
                pPlot = pGroup->plot();
                if (pPlot && pTargetPlot == pPlot)
                {
                    potentialWorkerGroup = true;
                    idleWorkers = true;
                }
            }
            else
            {
                pPlot = pGroup->AI_getMissionAIPlot();
                if (pPlot && pTargetPlot == pPlot && pGroup->AI_getMissionAIType() == MISSIONAI_BUILD)
                {
                    potentialWorkerGroup = true;
                }
            }
            if (potentialWorkerGroup)
            {
                int thisCount = 0;
                UnitGroupIter unitIter(pGroup);
                const CvUnit* pUnit = NULL;
                while (pUnit = unitIter())
                {
                    if (pUnit->AI_getUnitAIType() == UNITAI_WORKER)
                    {
                        ++thisCount;
                    }
                }
                count += thisCount;

#ifdef ALTAI_DEBUG
                // debug
                if (thisCount > 0)
                {
                    boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
                    std::ostream& os = pCivLog->getStream();
                    os << "\nFound: " << thisCount << (idleWorkers ? " idle " : "") << " workers at: " << XYCoords(pPlot->getX(), pPlot->getY());
                }
#endif
            }
        }
        return count;
    }

    int Player::getNumSettlersTargetingPlot(CvUnit* pIgnoreUnit, const CvPlot* pTargetPlot) const
    {
        CvPlot* pUnitTargetPlot = pIgnoreUnit->getGroup()->AI_getMissionAIPlot();
        int count = 0;

        SelectionGroupIter iter(*pPlayer_);
        CvSelectionGroup* pGroup = NULL;

        while (pGroup = iter())
        {
            bool potentialSettlerGroup = false;
            bool idleWorkers = false;
            CvPlot* pPlot = pGroup->AI_getMissionAIPlot();
            if (pPlot && pUnitTargetPlot != pTargetPlot && pTargetPlot == pPlot && pGroup->AI_getMissionAIType() == MISSIONAI_FOUND)
            {
                potentialSettlerGroup = true;
            }
            
            if (potentialSettlerGroup)
            {
                int thisCount = 0;
                UnitGroupIter unitIter(pGroup);
                const CvUnit* pUnit = NULL;
                while (pUnit = unitIter())
                {
                    if (pUnit->AI_getUnitAIType() == UNITAI_SETTLE)  // TODO - what about UNITAI_SETTLER_SEA?
                    {
                        ++thisCount;
                    }
                }
                count += thisCount;

#ifdef ALTAI_DEBUG
                // debug
                if (thisCount > 0)
                {
                    boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
                    std::ostream& os = pCivLog->getStream();
                    os << "\nFound: " << thisCount << " settlers targeting: " << XYCoords(pTargetPlot->getX(), pTargetPlot->getY());
                }
#endif
            }
        }
        return count;
    }

    std::vector<BuildTypes> Player::addAdditionalPlotBuilds(const CvPlot* pPlot, BuildTypes buildType) const
    {
        std::vector<BuildTypes> additionalBuilds;

        if (buildType == NO_BUILD)
        {
            return additionalBuilds;
        }

        const CvPlayerAI& player = CvPlayerAI::getPlayer(pPlayer_->getID());

        const CvBuildInfo& buildInfo = gGlobals.getBuildInfo(buildType);
        ImprovementTypes improvementType = (ImprovementTypes)buildInfo.getImprovement();
        FeatureTypes featureType = pPlot->getFeatureType();
        BonusTypes bonusType = pPlot->getBonusType(getTeamID());

        if (improvementType == NO_IMPROVEMENT)  // building a route
        {
            return std::vector<BuildTypes>(1, buildType);
        }

        bool improvementMakesBonusValid = bonusType != NO_BONUS && gGlobals.getImprovementInfo(improvementType).isImprovementBonusMakesValid(bonusType);

        bool buildRemovesFeature = featureType != NO_FEATURE ? GameDataAnalysis::doesBuildTypeRemoveFeature(buildType, featureType) : false;
        bool willRemoveFeature = buildRemovesFeature;

        bool improvementActsAsCity = false;
        if (improvementType != NO_IMPROVEMENT)
        {
            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(improvementType);
            improvementActsAsCity = improvementInfo.isActsAsCity();
        }

        if (buildRemovesFeature)  // push build to remove feature first, as this clears the plot sooner, I think (todo double check this)
        {
            BuildTypes featureRemoveBuildType = GameDataAnalysis::getBuildTypeToRemoveFeature(featureType);
            additionalBuilds.push_back(featureRemoveBuildType);
        }
        // see if want to remove the feature - don't bother if improvement is a fort - this presumes we don't build (or keep) forts in city radii
        else if (featureType != NO_FEATURE && !improvementActsAsCity)
	    {
            BuildTypes featureRemoveBuildType = GameDataAnalysis::getBuildTypeToRemoveFeature(featureType);
            if (featureRemoveBuildType != NO_BUILD)
            {
                if (GameDataAnalysis::isBadFeature(featureType) && player.canBuild(pPlot, featureRemoveBuildType))
                {
                    additionalBuilds.push_back(featureRemoveBuildType);
                    willRemoveFeature = true;
                }
            }
        }

        // add the actual improvement build
        additionalBuilds.push_back(buildType);

        const PlotInfo::PlotInfoNode& node = getAnalysis()->getMapAnalysis()->getPlotInfoNode(pPlot);

        // todo - check route yield changes actually only apply to improvments
        std::vector<std::pair<RouteTypes, PlotYield> > routeYieldChanges = getRouteYieldChanges(node, pPlayer_->getID(), improvementType, willRemoveFeature ? NO_FEATURE : featureType);
        RouteTypes bestRouteType = NO_ROUTE;
        int bestRouteValue = 0;
        std::vector<BuildTypes> availableRouteBuildTypes;
        YieldValueFunctor yieldF(makeYieldW(2, 1, 1));

        // find best route type
        for (size_t i = 0, count = routeYieldChanges.size(); i < count; ++i)
        {
            BuildTypes buildType = GameDataAnalysis::getBuildTypeForRouteType(routeYieldChanges[i].first);
            if (player.canBuild(pPlot, buildType))
            {
                availableRouteBuildTypes.push_back(buildType);

                int thisRoutesValue = yieldF(routeYieldChanges[i].second);
                if (thisRoutesValue > bestRouteValue)
                {
                    bestRouteValue = thisRoutesValue;
                    bestRouteType = routeYieldChanges[i].first;
                }
            }
        }

        if (bestRouteType != NO_ROUTE)
        {
            if (bestRouteValue > 0 || improvementMakesBonusValid)
            {
                additionalBuilds.push_back(GameDataAnalysis::getBuildTypeForRouteType(bestRouteType));
            }
        }
        else
        {
            // routes for bonuses added by checkResourceConnections
            // add route type if possible, as plot has bonus or is a fort
            //if ((pPlot->getBonusType(player.getTeam()) != NO_BONUS || improvementActsAsCity) && !availableRouteBuildTypes.empty())
            //{
            //    additionalBuilds.push_back(*availableRouteBuildTypes.rbegin());  // take latest build - todo - make sure this is the best one
            //}
        }
#ifdef ALTAI_DEBUG
        {
            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
            std::ostream& os = pCivLog->getStream();
            os << "\n";
            for (size_t i = 0, count = additionalBuilds.size(); i < count; ++i)
            {
                os << "Build: " << i << " = " << (additionalBuilds[i] == NO_BUILD ? " none " : gGlobals.getBuildInfo(additionalBuilds[i]).getType()) << ", ";
            }
        }
#endif
        return additionalBuilds;
    }

    CvPlot* Player::getBestPlot(CvUnitAI* pUnit, int subAreaID) const
    {
        CvPlot* pPlot = pSettlerManager_->getBestPlot(pUnit, subAreaID);

        /*bool foundUntargetedPlot = false;
        std::vector<CvPlot*> ignorePlots;

        CvPlot* pPlot = pSettlerManager_->getBestPlot(subAreaID, ignorePlots);

        if (pPlot)
        {
            while (!foundUntargetedPlot)
            {
                if (getNumSettlersTargetingPlot(pUnit, pPlot) > 0)
                {
                    ignorePlots.push_back(pPlot);
                    pPlot = pSettlerManager_->getBestPlot(subAreaID, ignorePlots);
                    if (!pPlot)
                    {
                        break;
                    }
                }
                else
                {
                    foundUntargetedPlot = true;
                }
            }
        }*/
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " getBestPlot() returning: ";
        //if (foundUntargetedPlot)
        if (pPlot)
        {
            os << XYCoords(pPlot->getX(), pPlot->getY());
        }
        else
        {
            os << " null";
        }
#endif
        //return foundUntargetedPlot ? pPlot : NULL;
        return pPlot;
    }

    int Player::getMaxResearchRate(std::pair<int, int> fixedIncomeAndExpenses) const
    {
        std::vector<std::pair<int, int> > outputs(11, std::make_pair(0, 0));
        int processGold = 0;

        CityIter cityIter(*pPlayer_);
        CvCity* pCity;
        while (pCity = cityIter())
        {
            const ConstructItem& constructItem = getCity(pCity->getID()).getConstructItem();
            ProcessTypes processType = constructItem.processType;
            if (processType != NO_PROCESS)
            {
                int modifier = gGlobals.getProcessInfo(processType).getProductionToCommerceModifier(COMMERCE_GOLD);
                processGold += (100 * modifier * pCity->getYieldRate(YIELD_PRODUCTION)) / 100;
            }

            boost::shared_ptr<CityData> pCityData(new CityData(pCity));
            CityOptimiser opt(pCityData);

            for (int i = 0; i <= 10; ++i)
            {
                pCityData->setCommercePercent(makeCommerce(10 * i, 100 - 10 * i, 0, 0));

                opt.optimise(NO_OUTPUT, CityOptimiser::Not_Set);

                TotalOutput output = pCityData->getOutput();
                outputs[i].first += output[OUTPUT_GOLD];
                outputs[i].second += output[OUTPUT_RESEARCH];
            }
        }

#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();
        os << "\nProcess gold = " << processGold;
        //os << "\nFixed income = " << fixedIncomeAndExpenses.first << ", fixed expenses = " << fixedIncomeAndExpenses.second;
#endif

        int maxRate = 0, maxRateWithProcesses = 0;
        for (int i = 10; i >= 0; --i)
        {
            if (outputs[i].first + fixedIncomeAndExpenses.first - fixedIncomeAndExpenses.second >= 0)
            {
                maxRate = 100 - 10 * i;
            }

            if (outputs[i].first + fixedIncomeAndExpenses.first + processGold - fixedIncomeAndExpenses.second >= 0)
            {
                maxRateWithProcesses = 100 - 10 * i;
            }
        }

#ifdef ALTAI_DEBUG
        //for (int i = 0; i <= 10; ++i)
        //{
        //    os << "\nResearch = " << (100 - 10 * i) << "%, city gold total = " << outputs[i].first << ", research total = " << outputs[i].second;
        //}
        os << "\nMax research rate = " << maxRate << "%, max rate with processes= " << maxRateWithProcesses;
#endif
        return maxRate;
    }

    // get max research rate, assuming remainder goes to gold
    void Player::calcMaxResearchRate_()
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();
#endif

        if (pPlayer_->isAnarchy())
        {
#ifdef ALTAI_DEBUG
            os << "\nAnarchy - max research rate = 0";
#endif
            maxRate_ = maxRateWithProcesses_ = 0;
            return;
        }

        int processGold = 0;

        const int fixedIncome = 100 * std::max<int>(0, pPlayer_->getGoldPerTurn());
        const int fixedExpenses = 100 * (pPlayer_->calculateInflatedCosts() + std::min<int>(0, pPlayer_->getGoldPerTurn()));
#ifdef ALTAI_DEBUG
        os << "\nFixed income = " << fixedIncome << ", fixed expenses = " << fixedExpenses;
#endif
        std::vector<std::pair<int, int> > outputs(11, std::make_pair(0, 0));

        CityIter cityIter(*pPlayer_);
        CvCity* pCity;
        while (pCity = cityIter())
        {
            const ConstructItem& constructItem = getCity(pCity->getID()).getConstructItem();
            ProcessTypes processType = constructItem.processType;
            if (processType != NO_PROCESS)
            {
                int modifier = gGlobals.getProcessInfo(processType).getProductionToCommerceModifier(COMMERCE_GOLD);
                processGold += (100 * modifier * pCity->getYieldRate(YIELD_PRODUCTION)) / 100;
            }

            boost::shared_ptr<CityData> pCityData(new CityData(pCity));
            CityOptimiser opt(pCityData);

            for (int i = 0; i <= 10; ++i)
            {
                pCityData->setCommercePercent(makeCommerce(10 * i, 100 - 10 * i, 0, 0));

                opt.optimise(NO_OUTPUT, CityOptimiser::Not_Set);

                TotalOutput output = pCityData->getOutput();
                outputs[i].first += output[OUTPUT_GOLD];
                outputs[i].second += output[OUTPUT_RESEARCH];
            }
        }

        int maxRate = 0, maxRateWithProcesses = 0;
        for (int i = 10; i >= 0; --i)
        {
            if (outputs[i].first + fixedIncome - fixedExpenses >= 0)
            {
                maxRate = 100 - 10 * i;
            }

            if (outputs[i].first + fixedIncome + processGold - fixedExpenses >= 0)
            {
                maxRateWithProcesses = 100 - 10 * i;
            }
        }

#ifdef ALTAI_DEBUG
        os << "\nProcess gold = " << processGold;
        for (int i = 0; i <= 10; ++i)
        {
            os << "\nResearch = " << (100 - 10 * i) << "%, city gold total = " << outputs[i].first << ", research total = " << outputs[i].second;
        }
        os << "\nMax research rate = " << maxRate << "%, max research rate with process = " << maxRateWithProcesses;
#endif

        maxRate_ = maxRate;
        maxRateWithProcesses_ = maxRateWithProcesses;
    }

    int Player::getMaxResearchRate() const
    {
        return maxRate_;
    }

    int Player::getMaxResearchRateWithProcesses() const
    {
        return maxRateWithProcesses_;
    }

    void Player::logMission(CvSelectionGroup* pGroup, MissionData missionData, MissionAITypes eMissionAI, CvPlot* pMissionAIPlot, CvUnit* pMissionAIUnit) const
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();

        CvWString missionString, missionTypeString;
        getMissionAIString(missionString, eMissionAI);
        getMissionTypeString(missionTypeString, missionData.eMissionType);

        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << ", group = " << pGroup->getID()
           << ", pushed mission: " << narrow(missionString.GetCString())
           << " type = " << narrow(missionTypeString.GetCString())
           << " to plot: " << (pMissionAIPlot ? XYCoords(pMissionAIPlot->getX(), pMissionAIPlot->getY()) : XYCoords(-1, -1))
           << " for unit: " << (pMissionAIUnit ? gGlobals.getUnitInfo(pMissionAIUnit->getUnitType()).getType() : "NO_UNIT")
           << " (iData1 = " << missionData.iData1 << ", iData2 = " << missionData.iData2 << ", iFlags = " << missionData.iFlags << ")";

        boost::shared_ptr<UnitLog> pUnitLog = UnitLog::getLog(*pPlayer_);
        std::ostream& osUnit = pUnitLog->getStream();
        osUnit << "\nTurn = " << gGlobals.getGame().getGameTurn();
        pUnitLog->logSelectionGroup(pGroup);
#endif
    }

    void Player::logClearMissions(const CvSelectionGroup* pGroup) const
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();

        CLLNode<MissionData>* pHeadMission = pGroup->headMissionQueueNode();

        CvWString missionTypeString;
        if (pHeadMission)
        {
            getMissionTypeString(missionTypeString, pHeadMission->m_data.eMissionType);
        }

        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << ", group = " << pGroup->getID()
           << " cancelled mission: " << (pHeadMission ? "none" : narrow(missionTypeString.GetCString()));
#endif
    }

    void Player::logScrapUnit(const CvUnit* pUnit) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nScrapping unit at: " << XYCoords(pUnit->plot()->getX(), pUnit->plot()->getY()) << " type = " << pUnit->getUnitInfo().getType();
#endif
    }

    void Player::addTech(TechTypes techType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        {
            os << "\nTech acquired: " << gGlobals.getTechInfo(techType).getType() << " turn = " << gGlobals.getGame().getGameTurn();
        }
#endif

        pPlayerAnalysis_->recalcTechDepths();

        pPlayerAnalysis_->getMapAnalysis()->reinitDotMap();

        // store tech requirement against buildings - so need to update these (before updating tech tactics)
        pPlayerAnalysis_->getPlayerTactics()->updateBuildingTactics();
        // similarly for units (potentially)
        pPlayerAnalysis_->getPlayerTactics()->updateUnitTactics();
        pPlayerAnalysis_->getPlayerTactics()->updateTechTactics();
        pPlayerAnalysis_->getPlayerTactics()->updateProjectTactics();

        boost::shared_ptr<TechInfo> pTechInfo = pPlayerAnalysis_->getTechInfo(techType);

        if (techAffectsBuildings(pTechInfo))
        {
            for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
            {
                iter->second.setFlag(City::NeedsBuildingCalcs);
            }
        }

        if (techAffectsImprovements(pTechInfo))
        {
            for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
            {
                iter->second.setFlag(City::NeedsImprovementCalcs);
            }
        }

        std::vector<BonusTypes> revealedBonusTypes = getRevealedBonuses(pTechInfo);
        if (!revealedBonusTypes.empty())
        {
#ifdef ALTAI_DEBUG
            {
                for (size_t i = 0, count = revealedBonusTypes.size(); i < count; ++i)
                {
                    os << " reveals bonus: " << gGlobals.getBonusInfo(revealedBonusTypes[i]).getType() << ", ";
                }
            }
#endif
            getAnalysis()->getMapAnalysis()->updateResourceData(revealedBonusTypes);
        }

        boost::shared_ptr<IEvent<NullRecv> > pEvent = boost::shared_ptr<IEvent<NullRecv> >(new DiscoverTech(techType));
        pPlayerAnalysis_->update(pEvent);
    }

    int Player::getTechResearchDepth(TechTypes techType) const
    {
        return pPlayerAnalysis_->getTechResearchDepth(techType);
    }

    void Player::pushPlotEvent(const boost::shared_ptr<IPlotEvent>& pPlotEvent)
    {
        getAnalysis()->getMapAnalysis()->pushPlotEvent(pPlotEvent);
    }

    void Player::updatePlotFeature(const CvPlot* pPlot, FeatureTypes oldFeatureType)
    {
        getAnalysis()->getMapAnalysis()->updatePlotFeature(pPlot, oldFeatureType);
    }

    void Player::updatePlotCulture(const CvPlot* pPlot, bool remove)
    {
        getAnalysis()->getMapAnalysis()->updatePlotCulture(pPlot, remove);
    }

    void Player::notifyReligionFounded(ReligionTypes religionType, bool isOurs)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nNotifying religion founded: " << gGlobals.getReligionInfo(religionType).getType() << (isOurs ? " by us " : " by someone else ");
#endif
        //getAnalysis()->getPlayerTactics()->unselectTechReligionTactics((TechTypes)(gGlobals.getReligionInfo(religionType).getTechPrereq()));
    }

    void Player::notifyFirstToTechDiscovered(TeamTypes teamType, TechTypes techType)
    {
        pPlayerAnalysis_->getPlayerTactics()->updateFirstToTechTactics(techType);
    }

    void Player::setCityDirty(IDInfo city)
    {
        cityFlags_.insert(city);
    }

    void Player::updateCityData()
    {
        for (std::set<IDInfo>::const_iterator ci(cityFlags_.begin()), ciEnd(cityFlags_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = pPlayer_->getCity(ci->iID);
            if (pCity)
            {
                getAnalysis()->analyseCity(pCity);
            }
        }
        getAnalysis()->getMapAnalysis()->analyseSharedPlots(cityFlags_);
        cityFlags_.clear();
    }

    void Player::setWorkingCityOverride(const CvPlot* pPlot, const CvCity* pOldCity, const CvCity* pNewCity)
    {
        getAnalysis()->getMapAnalysis()->setWorkingCity(pPlot, pOldCity, pNewCity);
    }

    void Player::updatePlotValues()
    {
        //pPlayerAnalysis_->getMapAnalysis()->reinitDotMap();
        pSettlerManager_->analysePlotValues();
    }

    const boost::shared_ptr<SettlerManager>& Player::getSettlerManager() const
    {
        return pSettlerManager_;
    }

    std::vector<int> Player::getBestCitySites(int minValue, int count)
    {
        return pSettlerManager_->getBestCitySites(minValue, count);
    }

    std::set<BonusTypes> Player::getBonusesForSites(int siteCount) const
    {
        return pSettlerManager_->getBonusesForSites(siteCount);
    }

    TechTypes Player::getResearchTech(TechTypes ignoreTechType)
    {
        ResearchTech researchTech = pPlayerAnalysis_->getResearchTech(ignoreTechType);
        if (ignoreTechType == NO_TECH)
        {
            researchTech_ = researchTech;
        }
        return researchTech.techType;
    }

    ResearchTech Player::getCurrentResearchTech() const
    {
        TechTypes currentResearchTech = pPlayer_->getCurrentResearch();
        if (currentResearchTech == NO_TECH)
        {
            return ResearchTech();
        }

        if (researchTech_.techType == currentResearchTech)
        {
            return researchTech_;
        }
        else
        {
            return pPlayerAnalysis_->getPlayerTactics()->getResearchTechData(currentResearchTech);
        }
    }

    std::pair<int, int> Player::getCityRank(IDInfo city, OutputTypes outputType) const
    {
        // todo - single pass algorithm should be possible
        std::multimap<int, IDInfo, std::greater<int> > outputs;
        for (CityMap::const_iterator ci(cities_.begin()), ciEnd(cities_.end()); ci != ciEnd; ++ci)
        {
            outputs.insert(std::make_pair(ci->second.getMaxOutputs()[outputType], ci->second.getCvCity()->getIDInfo()));
        }

        int rank = 1;
        for (std::multimap<int, IDInfo, std::greater<int> >::const_iterator ci(outputs.begin()), ciEnd(outputs.end()); ci != ciEnd; ++ci)
        {
            if (ci->second == city)
            {
                return std::make_pair(rank, ci->first);
            }
            else
            {
                ++rank;
            }
        }
        return std::make_pair(0, 0);  // indicates city not found
    }

    int Player::getUnitCount(UnitTypes unitType) const
    {
        int count = 0;
        SelectionGroupIter iter(*pPlayer_);

        while (CvSelectionGroup* pGroup = iter())
        {
            UnitGroupIter unitIter(pGroup);
            while (const CvUnit* pUnit = unitIter())
            {
                if (pUnit->getUnitType() == unitType)
                {
                    ++count;
                }
            }
        }

        if (true)  // todo add flag for inProduction
        {
            CityIter iter(*pPlayer_);

            while (CvCity* pCity = iter())
            {
                const City& city = getCity(pCity->getID());
                if (city.getConstructItem().unitType == unitType)
                {
                    ++count;
                }
            }
        }
        return count;
    }

    int Player::getCombatUnitCount(DomainTypes domainType, bool inProduction) const
    {
        int count = 0;
        SelectionGroupIter iter(*pPlayer_);

        while (CvSelectionGroup* pGroup = iter())
        {
            UnitGroupIter unitIter(pGroup);
            while (const CvUnit* pUnit = unitIter())
            {
                if (pUnit->getDomainType() == DOMAIN_LAND && pUnit->getUnitCombatType() != NO_UNITCOMBAT)
                {
                    ++count;
                }
            }
        }

        if (inProduction)
        {
            CityIter iter(*pPlayer_);

            while (CvCity* pCity = iter())
            {
                const City& city = getCity(pCity->getID());
                if (city.getConstructItem().militaryFlags & MilitaryFlags::Output_Combat_Unit)
                {
                    ++count;
                }
            }
        }
        return count;
    }

    int Player::getUnitCount(const std::vector<UnitAITypes>& AITypes, int militaryFlags) const
    {
        int count = 0;
        SelectionGroupIter iter(*pPlayer_);

        while (CvSelectionGroup* pGroup = iter())
        {
            UnitGroupIter unitIter(pGroup);
            while (const CvUnit* pUnit = unitIter())
            {
                if (pUnit->getDomainType() == DOMAIN_LAND && pUnit->getUnitCombatType() != NO_UNITCOMBAT)
                {
                    ++count;
                }
            }
        }

        if (militaryFlags)
        {
            CityIter iter(*pPlayer_);

            while (CvCity* pCity = iter())
            {
                const City& city = getCity(pCity->getID());
                if (city.getConstructItem().militaryFlags & militaryFlags)
                {
                    ++count;
                }
            }
        }
        return count;
    }

    int Player::getCollateralUnitCount(int militaryFlags) const
    {
        int count = 0;
        SelectionGroupIter iter(*pPlayer_);

        while (CvSelectionGroup* pGroup = iter())
        {
            UnitGroupIter unitIter(pGroup);
            while (const CvUnit* pUnit = unitIter())
            {
                if (pUnit->getDomainType() == DOMAIN_LAND && pUnit->getUnitInfo().getCollateralDamage() > 0)
                {
                    ++count;
                }
            }
        }

        if (militaryFlags)
        {
            CityIter iter(*pPlayer_);

            while (CvCity* pCity = iter())
            {
                const City& city = getCity(pCity->getID());
                if (city.getConstructItem().militaryFlags & militaryFlags)
                {
                    ++count;
                }
            }
        }
        return count;
    }

    int Player::getScoutUnitCount(DomainTypes domainType, bool inProduction) const
    {
        int count = 0;
        SelectionGroupIter iter(*pPlayer_);

        while (CvSelectionGroup* pGroup = iter())
        {
            UnitGroupIter unitIter(pGroup);
            while (const CvUnit* pUnit = unitIter())
            {
                if (pUnit->getDomainType() == DOMAIN_LAND && pUnit->AI_getUnitAIType() == UNITAI_EXPLORE)
                {
                    ++count;
                }
            }
        }

        if (inProduction)
        {
            CityIter iter(*pPlayer_);

            while (CvCity* pCity = iter())
            {
                const City& city = getCity(pCity->getID());
                if (city.getConstructItem().militaryFlags & MilitaryFlags::Output_Extra_Mobility)
                {
                    ++count;
                }
            }
        }
        return count;
    }

    void Player::write(FDataStreamBase* pStream) const
    {
        pStream->Write(maxRate_);
        pStream->Write(maxRateWithProcesses_);

        pPlayerAnalysis_->write(pStream);
    }

    void Player::read(FDataStreamBase* pStream)
    {
        pStream->Read(&maxRate_);
        pStream->Read(&maxRateWithProcesses_);

        pPlayerAnalysis_->read(pStream);
    }

    void Player::logCitySites() const
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();
        os << "\nCity site count = " << ((CvPlayerAI*)pPlayer_)->AI_getNumCitySites();

        for (int i = 0, count = ((CvPlayerAI*)pPlayer_)->AI_getNumCitySites(); i < count; ++i)
        {
            CvPlot* pPlot = ((CvPlayerAI*)pPlayer_)->AI_getCitySite(i);
            os << "\nSite at: " << pPlot->getX() << ", " << pPlot->getY() << " value = " << pPlot->getFoundValue(pPlayer_->getID());
        }
#endif
    }

    void Player::logSettlerMission(const CvPlot* pBestPlot, const CvPlot* pBestFoundPlot, int bestFoundValue, int pathTurns, bool foundSite) const
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();

        if (foundSite)
        {
            os << "\nPushed Settler Mission to ";
            if (pBestPlot)
            {
                os << "move to: " << pBestPlot->getX() << ", " << pBestPlot->getY() << " and ";
            }
            os << "found city at: " << pBestFoundPlot->getX() << ", " << pBestFoundPlot->getY();
        }
        else
        {
            os << "\nFailed to find suitable settler mission. ";
            if (pBestFoundPlot)
            {
                os << "Best found plot = " << pBestFoundPlot->getX() << ", " << pBestFoundPlot->getY();
            }
            if (pBestPlot)
            {
                os << " Best move to plot = " << pBestPlot->getX() << ", " << pBestPlot->getY();
            }
        }
        os << " path turns = " << pathTurns;
#endif
    }

    void Player::logUnitAIChange(const CvUnitAI* pUnit, UnitAITypes oldAI, UnitAITypes newAI) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();

        CvWString oldAITypeString, newAITypeString;
        getUnitAIString(oldAITypeString, oldAI);
        getUnitAIString(newAITypeString, newAI);

        os << "\nUnit: " << pUnit->getUnitInfo().getType() << " ID = " << pUnit->getID() << ", AI changed from: " << narrow(oldAITypeString) << " to: " << narrow(newAITypeString);
#endif
    }

    void Player::logBestResearchTech(TechTypes techType) const
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();

        if (techType != NO_TECH)
        {
            os << "\nBest Research = " << gGlobals.getTechInfo(techType).getType() << ", turn = " << gGlobals.getGame().getGameTurn();
        }
        else
        {
            os << "\nBest Research = NO_TECH, turn = " << gGlobals.getGame().getGameTurn();
        }
#endif
    }

    void Player::logPushResearchTech(TechTypes techType) const
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();

        if (techType != NO_TECH)
        {
            os << "\nTech pushed: " << gGlobals.getTechInfo(techType).getType() << ", turn = " << gGlobals.getGame().getGameTurn();
        }
        else
        {
            os << "\nTech pushed: NO_TECH, turn = " << gGlobals.getGame().getGameTurn();
        }

        const CvPlayer* pPlayer = getCvPlayer();
        os << "\nResearch queue now: ";
        for (CLLNode<TechTypes>* pResearchNode = pPlayer->headResearchQueueNode(); pResearchNode; pResearchNode = pPlayer->nextResearchQueueNode(pResearchNode))
        {
            os << gGlobals.getTechInfo(pResearchNode->m_data).getType() << ", ";
        }
#endif
    }

    void Player::logStuckSelectionGroup(CvUnit* pHeadUnit) const
    {
        boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(*pPlayer_);
        std::ostream& os = pErrorLog->getStream();

        os << "\nSelection group at: " << XYCoords(pHeadUnit->getX(), pHeadUnit->getY()) << " stuck. Head unit is: " << gGlobals.getUnitInfo(pHeadUnit->getUnitType()).getType();
    }

    void Player::logInvalidUnitBuild(const CvUnit* pUnit, BuildTypes buildType) const
    {
        std::ostream& os = ErrorLog::getLog(*pPlayer_)->getStream();
        os << "\nInvalid build for unit: " << pUnit->getID() << " = " << gGlobals.getBuildInfo(buildType).getType();
    }
}