#include "AltAI.h"

#include "./tactic_actions.h"
#include "./unit.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./unit.h"
#include "./plot_info.h"
#include "./gamedata_analysis.h"
#include "./player_analysis.h"
#include "./opponents.h"
#include "./map_analysis.h"
#include "./military_tactics.h"
#include "./unit_tactics.h"
#include "./worker_tactics.h"
#include "./great_people_tactics.h"
#include "./resource_tactics.h"
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
#include "./unit_explore.h"

namespace AltAI
{
    namespace
    {
    }

    Player::Player(CvPlayer* pPlayer)
        : pPlayer_(pPlayer)
    {
        // set these based on starting the game with 100% research rate (need a non-zero value for tech rate initial city site maintenance calcs)
        maxGold_ = maxGoldWithProcesses_ = 0;
        maxRate_ = maxRateWithProcesses_ = 100;
        goldAndResearchOutputsByCommerceRate_ = std::vector<std::pair<int, int> >(11, std::make_pair(0, 0));
        pPlayerAnalysis_ = boost::shared_ptr<PlayerAnalysis>(new PlayerAnalysis(*this));
        pOpponentsAnalysis_ = boost::shared_ptr<OpponentsAnalysis>(new OpponentsAnalysis(*this));
        pCivHelper_ = boost::shared_ptr<CivHelper>(new CivHelper(*this));
        pSettlerManager_ = boost::shared_ptr<SettlerManager>(new SettlerManager(*this));
    }

    void Player::init()
    {
        pPlayerAnalysis_->init();
        pCivHelper_->init();

        AreaIter iter(&gGlobals.getMap());

        while (CvArea* pArea = iter())
        {
            if (pArea->getCitiesPerPlayer(pPlayer_->getID()) > 0)
            {
                areaHelpersMap_[pArea->getID()] = boost::shared_ptr<AreaHelper>(new AreaHelper(*pPlayer_, pArea));
            }
        }

        pPlayerAnalysis_->getMapAnalysis()->init();

#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();
#endif
        
        GameDataAnalysis::getInstance()->analyseForPlayer(*this);        

#ifdef ALTAI_DEBUG
        pPlayerAnalysis_->getMapAnalysis()->debug(os);
        //pPlayerAnalysis_->getMapAnalysis()->debugSharedPlots();
#endif

        for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
            // initialise city
            iter->second->init();
            setCityDirty(iter->second->getCvCity()->getIDInfo());

#ifdef ALTAI_DEBUG
            os << "\n" << narrow(iter->second->getCvCity()->getName()) << ":\n";
            os << "Max outputs = " << iter->second->getMaxOutputs() << " weights = " << iter->second->getMaxOutputWeights() << "\n";

            iter->second->logImprovements();
#endif
        }

        citiesToInit_.clear();

        pPlayerAnalysis_->postCityInit();

        //pPlayerAnalysis_->getMapAnalysis()->analyseSharedPlots();
        pSettlerManager_->analysePlotValues();

        //calcCivics_();
    }

    void Player::doTurn()
    {
        if (pPlayer_->isAlive())
        {
            initCities();

            // handle plot updates
            //pPlayerAnalysis_->getMapAnalysis()->update();

            calcMaxResearchRate_();

            for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
            {
                iter->second->doTurn();
            }

#ifdef ALTAI_DEBUG
            {
                std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
                for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
                {
                    std::pair<int, int> rank = getCityRank(iter->second->getCvCity()->getIDInfo(), OUTPUT_PRODUCTION);
                    os << "\n" << narrow(iter->second->getCvCity()->getName()) << " rank = " << rank.first << " output (prod) = " << rank.second;
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
                os << "\nTurn = " << gGlobals.getGame().getGameTurn() << __FUNCTION__;
                pUnitLog->logSelectionGroups();
            }

            pPlayerAnalysis_->getWorkerAnalysis()->logMissions(CivLog::getLog(*pPlayer_)->getStream());
#endif
        }
    }

    // called from load game and init game
    void Player::addCity(CvCity* pCity)
    {
        CityPtr ptrCity(new City(pCity));
        CityMap::iterator cityIter = cities_.insert(std::make_pair(pCity->getID(), ptrCity)).first;

        // updates shared plot data (if not init yet - init() above will call MapAnalysis)
        if (gGlobals.getGame().getAltAI()->isInit() && pPlayer_->isUsingAltAI())
        {
            // specialist analysis uses a city if available - so if started a new game,
            // this needs to update when first city is founded (clunky - todo - improve)
            /*if (cities_.size() == 1)
            {
                pPlayerAnalysis_->analyseSpecialists();
            }*/
            pPlayerAnalysis_->getMilitaryAnalysis()->addCity(pCity);
        }

        citiesToInit_.insert(pCity->getID());
    }

    void Player::deleteCity(CvCity* pCity)
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();
        os << "\nDeleting city: " << safeGetCityName(pCity);
#endif
        pPlayerAnalysis_->getMapAnalysis()->deleteCity(pCity);
        
        if (pPlayer_->isUsingAltAI())
        {
            pPlayerAnalysis_->getPlayerTactics()->deleteCity(pCity);
            pPlayerAnalysis_->getMilitaryAnalysis()->deleteCity(pCity);
            pPlayerAnalysis_->getWorkerAnalysis()->updateCity(pCity, true);
            pPlayerAnalysis_->getGreatPeopleAnalysis()->updateCity(pCity, true);
        }

        // set flag to update other cities as maintenance costs will have changed
        CityIter iter(CvPlayerAI::getPlayer(pCity->getOwner()));
        while (CvCity* pLoopCity = iter())
        {
            if (pLoopCity->getIDInfo() == pCity->getIDInfo())
            {
                continue;
            }
            getCity(pLoopCity).setFlag(City::NeedsProjectionCalcs | City::NeedsCityDataCalc);
        }

        cities_.erase(pCity->getID());
    }

    void Player::initCities()
    {
        for (std::set<int>::const_iterator ci(citiesToInit_.begin()), ciEnd(citiesToInit_.end()); ci != ciEnd; ++ci)
        {
            CityMap::iterator iter(cities_.find(*ci));
            if (iter != cities_.end())
            {
                iter->second->init();
                const CvCity* pCity = iter->second->getCvCity();
                pPlayerAnalysis_->analyseSpecialists();  // only call after we've initialised at least one city
                pPlayerAnalysis_->getPlayerTactics()->addNewCityBuildingTactics(pCity->getIDInfo());
                pPlayerAnalysis_->getPlayerTactics()->addNewCityUnitTactics(pCity->getIDInfo());
                pPlayerAnalysis_->getPlayerTactics()->addCityImprovementTactics(pCity->getIDInfo());

                pPlayerAnalysis_->getWorkerAnalysis()->updateCity(pCity, false);
                pPlayerAnalysis_->getGreatPeopleAnalysis()->updateCity(pCity, false);
            }
        }
        citiesToInit_.clear();
    }

    void Player::recalcPlotInfo()
    {
        pPlayerAnalysis_->getMapAnalysis()->recalcPlotInfo();
    }

    City& Player::getCity(const CvCity* pCity)
    {
        return getCity(pCity->getID());
    }

    const City& Player::getCity(const CvCity* pCity) const
    {
        return getCity(pCity->getID());
    }

    bool Player::isCity(const int ID) const
    {
        return cities_.find(ID) != cities_.end();
    }

    City& Player::getCity(const int ID)
    {
        CityMap::iterator iter = cities_.find(ID);
        if (iter != cities_.end())
        {
            return *iter->second;
        }

        {
            FAssertMsg(false, "Failed to find city entry");
            boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(*pPlayer_);
            std::ostream& os = pErrorLog->getStream();

            std::ostringstream oss;
            oss << "\nFailed to find city with ID: " << ID << "\n";
            os << oss.str();
            throw std::runtime_error(oss.str());  // todo - change this to fatal error message
        }
    }

    const City& Player::getCity(const int ID) const
    {
        CityMap::const_iterator iter = cities_.find(ID);
        if (iter != cities_.end())
        {
            return *iter->second;
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

    std::vector<IUnitEventGeneratorPtr> Player::getCityUnitEvents(IDInfo city)
    {
        std::vector<IUnitEventGeneratorPtr> unitEvents;
        const CityDataPtr& pCityData = getCity(city.iID).getCityData();

        for (UnitsCIter ci(units_.begin()), ciEnd(units_.end()); ci != ciEnd; ++ci)
        {
            IUnitEventGeneratorPtr pUnitEventGenerator = ci->second.getUnitEventGenerator();
            if (pUnitEventGenerator && pUnitEventGenerator->getProjectionEvent(pCityData)->targetsCity(city))
            {
                unitEvents.push_back(pUnitEventGenerator);
            }
        }

        return unitEvents;
    }

    void Player::addOurUnit(CvUnitAI* pUnit, const CvUnit* pUpgradingUnit, bool loading)
    {
        Unit unit(pUnit);
        units_.insert(std::make_pair(pUnit->getIDInfo(), unit));

        if (!loading)
        {
            unit.setAction(pPlayerAnalysis_->getPlayerTactics()->getConstructedUnitAction(pUnit));

#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();
            CvWString unitAITypeString;
            getUnitAIString(unitAITypeString, pUnit->AI_getUnitAIType());

            os << "\nUnit (ID=" << pUnit->getID() << ") created, player = " << pUnit->getOwner()
                << ", ai type = " << narrow(unitAITypeString)
                << ", target = " << unit.getAction().targetPlot << "\n";
#endif
            const CvCity* pCity = pUnit->plot()->getPlotCity();
            // can add a unit before we have a city - if we capture workers, for example - todo fix this problem
            if (pCity && cities_.find(pCity->getID()) != cities_.end())
            {
                City& city = getCity(pCity->getID());
                if (city.getConstructItem().pUnitEventGenerator)
                {
                    unit.setUnitEventGenerator(city.getConstructItem().pUnitEventGenerator);
#ifdef ALTAI_DEBUG
                    city.getConstructItem().pUnitEventGenerator->getProjectionEvent(city.getCityData())->debug(os);
#endif
                }
            }

            pPlayerAnalysis_->getMilitaryAnalysis()->addOurUnit(pUnit, pUpgradingUnit);
        }
    }

    void Player::deleteOurUnit(CvUnit* pUnit, const CvPlot* pPlot)
    {
        pPlayerAnalysis_->getMilitaryAnalysis()->deleteOurUnit(pUnit, pPlot);
        pPlayerAnalysis_->getWorkerAnalysis()->deleteUnit(pUnit);

        units_.erase(pUnit->getIDInfo());

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();
            os << "\nUnit (ID=" << pUnit->getID() << ") deleted, player = " << pUnit->getOwner() << "\n";
        }
#endif
    }

    Unit& Player::getOurUnit(CvUnitAI* pUnit)
    {
        return getOurUnit(pUnit->getIDInfo());
    }

    Unit& Player::getOurUnit(IDInfo unit)
    {
		UnitsIter iter(units_.find(unit));
        if (iter != units_.end())
        {
            return iter->second;
        }
        else
        {
            FAssertMsg(false, "Failed to find unit entry");
            boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(*pPlayer_);
            std::ostream& os = pErrorLog->getStream();
            os << "\nFailed to find unit: " << unit << " added back to map...";
            addOurUnit((CvUnitAI*)::getUnit(unit));
            return getOurUnit(unit);
        }
    }

    void Player::moveOurUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot)
    {
#ifdef ALTAI_DEBUG
        UnitsCIter ci(units_.find(pUnit->getIDInfo()));

        if (ci != units_.end())
        {
            std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();
            XYCoords from(pFromPlot->getCoords()), to(pToPlot->getCoords());
            os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " Unit (ID = " << pUnit->getID() << ") moved from: " << from << " to: " << to << "\n";
        }
#endif
    }

    void Player::withdrawOurUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot)
    {
        pPlayerAnalysis_->getMilitaryAnalysis()->withdrawOurUnit(pUnit, pAttackPlot);
    }

    CvUnit* Player::getNextAttackUnit()
    {
        return getAnalysis()->getMilitaryAnalysis()->getNextAttackUnit();
    }

    std::pair<IDInfo, UnitTypes> Player::getPriorityUnitBuild(IDInfo city)
    {
        return getAnalysis()->getMilitaryAnalysis()->getPriorityUnitBuild(city);
    }

    void Player::updateMilitaryAnalysis()
    {
        AltAI::updateMilitaryAnalysis(*this);
    }

    void Player::updateWorkerAnalysis()
    {
        AltAI::updateWorkerAnalysis(*this);
    }

    void Player::pushWorkerMission(CvUnitAI* pUnit, const CvCity* pCity, const CvPlot* pTargetPlot, 
        MissionTypes missionType, BuildTypes buildType, int iFlags, const std::string& caller)
    {
#ifdef ALTAI_DEBUG
        CvWString missionTypeString;
        getMissionTypeString(missionTypeString, missionType);

        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pPlayer_->getID()))->getStream();
        os << "\nPush worker mission for unit: " << pUnit->getID() << " at: " << pUnit->plot()->getCoords()
           << " targeting plot: " << pTargetPlot->getCoords()
           << " turn = " << gGlobals.getGame().getGameTurn()
           << " caller = " << caller
           << " mission = " << narrow(missionTypeString)
           << " build = " << (buildType == NO_BUILD ? " none " : gGlobals.getBuildInfo(buildType).getType())
           << " flags = " << iFlags;
#endif

        UnitsIter iter(units_.find(pUnit->getIDInfo()));

        if (iter != units_.end())
        {
            iter->second.pushWorkerMission(gGlobals.getGame().getGameTurn(), pCity, pTargetPlot, missionType, buildType, iFlags);
        }
    }

    /*void Player::updateWorkerMission(CvUnitAI* pUnit, BuildTypes buildType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pPlayer_->getID()))->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " Updating worker mission, build type = "
           << (buildType == NO_BUILD ? "none" : gGlobals.getBuildInfo(buildType).getType())
           << " mission before = \n\t";
#endif
        UnitsIter iter(units_.find(pUnit->getIDInfo()));

        if (iter != units_.end())
        {
#ifdef ALTAI_DEBUG
            for (size_t i = 0, count = iter->second.getWorkerMissions().size(); i < count; ++i)
            {
                iter->second.getWorkerMissions()[i].debug(os);
            }
#endif
            iter->second.updateMission();

#ifdef ALTAI_DEBUG
            os << "\n\tmission after = \n\t";
            for (size_t i = 0, count = iter->second.getWorkerMissions().size(); i < count; ++i)
            {
                iter->second.getWorkerMissions()[i].debug(os);
            }
#endif
        }
    }*/

    void Player::clearWorkerMission(CvUnitAI* pUnit)
    {
        UnitsIter iter(units_.find(pUnit->getIDInfo()));

        if (iter != units_.end())
        {
            iter->second.clearMission(gGlobals.getGame().getGameTurn());
        }
    }

    void Player::updateMission(CvUnitAI* pUnit, CvPlot* pOldPlot, CvPlot* pNewPlot)
    {
        UnitsIter iter(units_.find(pUnit->getIDInfo()));

        if (iter != units_.end())
        {
//#ifdef ALTAI_DEBUG
//            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pPlayer_->getID()))->getStream();
//            os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " Updating unit: " << pUnit->getIDInfo() << ", from " << pOldPlot->getCoords() << " to "
//               << pNewPlot->getCoords() << "\n\tmission before = ";
//            for (size_t i = 0, count = iter->second.getWorkerMissions().size(); i < count; ++i)
//            {
//                iter->second.getWorkerMissions()[i].debug(os);
//            }
//#endif
            iter->second.updateMission(pOldPlot, pNewPlot);

//#ifdef ALTAI_DEBUG
//            os << "\n\tmission after = ";
//            for (size_t i = 0, count = iter->second.getWorkerMissions().size(); i < count; ++i)
//            {
//                iter->second.getWorkerMissions()[i].debug(os);
//            }
//#endif
        }
    }

    bool Player::hasMission(CvUnitAI* pUnit)
    {
        UnitsIter iter(units_.find(pUnit->getIDInfo()));

        if (iter != units_.end())
        {
            return !iter->second.getMissions().empty();
        }
        return false;
    }

    void Player::pushMission(CvUnitAI* pUnit, const UnitMissionPtr& pMission)
    {
        UnitsIter iter(units_.find(pUnit->getIDInfo()));

        if (iter != units_.end())
        {
            iter->second.pushMission(pMission);
        }
    }

    bool Player::executeMission(CvUnitAI* pUnit)
    {
        UnitsIter iter(units_.find(pUnit->getIDInfo()));

        if (iter != units_.end())
        {
            if (iter->second.getMissions().empty())
            {
                return false;
            }

            iter->second.getMissions()[0]->update();
            return !iter->second.getMissions()[0]->isComplete();
        }
        return false;
    }

    PromotionTypes Player::promoteUnit(CvUnitAI* pUnit)
    {
        return pPlayerAnalysis_->getMilitaryAnalysis()->promoteUnit(pUnit);
    }

    int Player::getNumWorkersTargetingPlot(XYCoords targetCoords) const
    {
        int count = 0;

        for (UnitsCIter ci(units_.begin()), ciEnd(units_.end()); ci != ciEnd; ++ci)
        {
            if (ci->second.hasWorkerMissionAt(targetCoords))
            {
                ++count;
            }
        }
        return count;
    }

    int Player::getNumWorkersTargetingCity(const CvCity* pCity) const
    {
        int count = 0;

        for (UnitsCIter ci(units_.begin()), ciEnd(units_.end()); ci != ciEnd; ++ci)
        {
            if (ci->second.isWorkerMissionFor(pCity))
            {
                ++count;
            }
        }
        return count;
    }

    int Player::getNumActiveWorkers(UnitTypes unitType) const
    {
        int count = 0;

        for (UnitsCIter ci(units_.begin()), ciEnd(units_.end()); ci != ciEnd; ++ci)
        {
            if (ci->second.getUnitType() == unitType && ci->second.hasBuildOrRouteMission())
            {
                ++count;
            }
        }
        return count;
    }

    CvCity* Player::getNextCityForWorkerToImprove(const CvCity* pCurrentCity) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pPlayer_->getID()))->getStream();
#endif
        CvCity* pNextCity = NULL;

        CityIter cityIter(*pPlayer_);
        CvCity* pLoopCity;
        int maxNumWorkersReqd = 0;
        while (pLoopCity = cityIter())
        {
            // todo can have reachable sub areas around a city even if areas differ (e.g. plots across the sea which a city can work)
            if (!pCurrentCity || (pLoopCity != pCurrentCity && pLoopCity->plot()->getArea() == pCurrentCity->plot()->getArea()))
            {
                int totalNumWorkersReqdForCity = getCity(pLoopCity).getNumReqdWorkers();
                int numWorkersTargetingCity = getNumWorkersTargetingCity(pLoopCity);

                int numWorkersReqdForCity = totalNumWorkersReqdForCity - numWorkersTargetingCity;
#ifdef ALTAI_DEBUG
                os << "\nCity: " << narrow(pLoopCity->getName()) << " total reqd = " << totalNumWorkersReqdForCity
                    << " targetting = " << numWorkersTargetingCity;
#endif
                if (numWorkersReqdForCity > maxNumWorkersReqd)
                {
                    maxNumWorkersReqd = numWorkersReqdForCity;
                    pNextCity = pLoopCity;
                }
            }
        }

        return pNextCity;
    }

    std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > > Player::getWorkerMissionsForCity(const CvCity* pCity) const
    {
        std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > > missions;

        for (UnitsCIter ci(units_.begin()), ciEnd(units_.end()); ci != ciEnd; ++ci)
        {
            if (ci->second.isWorkerMissionFor(pCity))
            {
                missions.push_back(std::make_pair(ci->second.getUnitType(), ci->second.getWorkerMissions()));
            }
        }
        return missions;
    }

    std::vector<std::pair<std::pair<int, UnitTypes>, std::vector<Unit::WorkerMission> > > Player::getWorkerMissions() const
    {
        std::vector<std::pair<std::pair<int, UnitTypes>, std::vector<Unit::WorkerMission> > > missions;

        for (UnitsCIter ci(units_.begin()), ciEnd(units_.end()); ci != ciEnd; ++ci)
        {
            missions.push_back(std::make_pair(std::make_pair(ci->second.getUnit()->getID(), ci->second.getUnitType()), ci->second.getWorkerMissions()));
        }
        return missions;
    }

    void Player::debugWorkerMissions(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os <<  "\nWorker Missions: ";
        std::vector<std::pair<std::pair<int, UnitTypes>, std::vector<Unit::WorkerMission> > > missions = getWorkerMissions();
        os << " size = " << missions.size();
        for (size_t i = 0, count = missions.size(); i < count; ++i)
        {
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(missions[i].first.second);
            if (unitInfo.getDefaultUnitAIType() == UNITAI_WORKER || unitInfo.getDefaultUnitAIType() == UNITAI_WORKER_SEA)
            {
                os << "\n\tUnit: " << unitInfo.getType() << missions[i].first.first << " mission count = " << missions[i].second.size();
                for (size_t j = 0, missionCount = missions[i].second.size(); j < missionCount; ++j)
                {
                    missions[i].second[j].debug(os);
                }
            }    
            /*else
            {
                os << "\n\tUnit AI type = " << gGlobals.getUnitAIInfo((UnitAITypes)unitInfo.getDefaultUnitAIType()).getType();
            }*/
        }
#endif
    }

    void Player::addPlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();
        os << "\nTurn: " << gGlobals.getGame().getGameTurn() << " Player: " << pUnit->getOwner() << " - unit added: " << pUnit->getID() << " "
            << pUnit->getUnitInfo().getType() << " at: " << pPlot->getCoords();
#endif
        pPlayerAnalysis_->getMilitaryAnalysis()->addPlayerUnit(pUnit, pPlot);
    }

    void Player::deletePlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();
        os << "\nPlayer: " << pUnit->getOwner() << " - unit deleted: " << pUnit->getID() << " " 
            << pUnit->getUnitInfo().getType() << " at: " << pPlot->getCoords();
#endif
        pPlayerAnalysis_->getMilitaryAnalysis()->deletePlayerUnit(pUnit, pPlot);
    }

    void Player::movePlayerUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();

        os << "\nTurn: " << gGlobals.getGame().getGameTurn() << " Player: " << pUnit->getOwner() << " moves unit: " << pUnit->getID() << " " << pUnit->getUnitInfo().getType();
        if (pFromPlot)
        {
            os << " " << pFromPlot->getCoords();
        }
        else
        {
            os << " unknown ";
        }
        os << " to: " << pToPlot->getCoords();
#endif
        pPlayerAnalysis_->getMilitaryAnalysis()->movePlayerUnit(pUnit, pFromPlot, pToPlot);
    }

    void Player::hidePlayerUnit(CvUnitAI* pUnit, const CvPlot* pOldPlot, bool moved)
    {        
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*pPlayer_)->getStream();

        os << "\nTurn: " << gGlobals.getGame().getGameTurn() << (moved ? " Player moves unit out of view: " : " Lost sight of player unit: ") << pUnit->getIDInfo() << " " << pUnit->getUnitInfo().getType()
            << " at: " << pOldPlot->getCoords() << " visible = " << pOldPlot->isVisible(getTeamID(), false);
        if (pUnit->getTeam() == getTeamID())
        {
            os << " team unit? ";
        }

#endif
        if (pUnit->getTeam() != getTeamID())
        {
            pPlayerAnalysis_->getMilitaryAnalysis()->hidePlayerUnit(pUnit, pOldPlot, moved);
        }
    }

    void Player::withdrawPlayerUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot)
    {
        pPlayerAnalysis_->getMilitaryAnalysis()->withdrawPlayerUnit(pUnit, pAttackPlot);
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

    bool Player::checkResourcesOutsideCities(CvUnitAI* pUnit, const std::multimap<int, const CvPlot*>& resourceHints)
    {
        const int subAreaID = pUnit->plot()->getSubArea();
        CvMap& theMap = gGlobals.getMap();
        std::vector<CvPlot*> plotsToCheck;

        for (std::multimap<int, const CvPlot*>::const_reverse_iterator plotIter(resourceHints.rbegin()), plotEndIter(resourceHints.rend()); plotIter != plotEndIter; ++plotIter)
        {
            BonusTypes bonusType = plotIter->second->getBonusType(pPlayer_->getTeam());

            bool isConnected = false;
            CityIter cityIter(*pPlayer_);
            while (CvCity* pCity = cityIter())
            {
                if (plotIter->second->getOwner() == pUnit->getOwner() && plotIter->second->isConnectedTo(pCity))
                {
                    isConnected = true;
                    break;
                }
            }

            ImprovementTypes improvementType = plotIter->second->getImprovementType();
            const bool improvementActsAsCity = improvementType == NO_IMPROVEMENT ? false : gGlobals.getImprovementInfo(improvementType).isActsAsCity();
            ImprovementTypes requiredImprovement = getBonusImprovementType(bonusType);
            // will convert forts to actual improvement (useful if later used by city, but may want to keep fort in some cases)
            if ((improvementType == NO_IMPROVEMENT || improvementActsAsCity || !isConnected) && requiredImprovement != NO_IMPROVEMENT)
            {
                return checkResourcesPlot(pUnit, plotIter->second);
            }
        }
        
        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
        {
            CvPlot* pPlot = theMap.plotByIndex(i);
            if (pPlot->getOwner() == pPlayer_->getID() && pPlot->getSubArea() == subAreaID)
            {                
                BonusTypes bonusType = pPlot->getBonusType(pPlayer_->getTeam());
                if (bonusType != NO_BONUS)
                {
                    bool isConnected = false;
                    CityIter cityIter(*pPlayer_);
                    while (CvCity* pCity = cityIter())
                    {
                        if (pPlot->isConnectedTo(pCity))
                        {
                            isConnected = true;
                            break;
                        }
                    }

                    ImprovementTypes improvementType = pPlot->getImprovementType();
                    const bool improvementActsAsCity = improvementType == NO_IMPROVEMENT ? false : gGlobals.getImprovementInfo(improvementType).isActsAsCity();
                    ImprovementTypes requiredImprovement = getBonusImprovementType(bonusType);
                    // will convert forts to actual improvement (useful if later used by city, but may want to keep fort in some cases)
                    if ((improvementType == NO_IMPROVEMENT || improvementActsAsCity || !isConnected) && requiredImprovement != NO_IMPROVEMENT)
                    {
                        return checkResourcesPlot(pUnit, pPlot);
                    }
                }
            }
        }
        return false;
    }

    bool Player::checkResourcesPlot(CvUnitAI* pUnit, const CvPlot* pPlot)
    {
        CvMap& theMap = gGlobals.getMap();
        const int subAreaID = pUnit->plot()->getSubArea();
        bool ownTarget = false;
		UnitsIter iter(units_.find(pUnit->getIDInfo()));
        if (iter != units_.end())
        {
            if (iter->second.hasWorkerMissionAt(pPlot->getCoords()))
            {
                ownTarget = true;
            }
        }

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pPlayer_->getID()))->getStream();
        os << "\nChecking plot: " << pPlot->getCoords() << " for worker missions: " << getNumWorkersTargetingPlot(pPlot->getCoords())
            << " at plot: " << (pUnit->atPlot(pPlot) ? "true" : "false")
            << " own target = " << (ownTarget ? "true" : "false");
#endif                        

        if (!ownTarget && getNumWorkersTargetingPlot(pPlot->getCoords()) > 0)
        {
            return false;
        }

        CityIter cityIter(*pPlayer_);
        CvCity* pLoopCity, *pBestCity = NULL;
        int distanceToNearestCity = MAX_INT;
        while (pLoopCity = cityIter())
        {
            if (pLoopCity->plot()->getSubArea() == subAreaID)
            {
                int distance = theMap.calculatePathDistance((CvPlot *)pPlot, pLoopCity->plot());
                if (distance < distanceToNearestCity)
                {
                    distanceToNearestCity = distance;
                    pBestCity = pLoopCity;
                }
            }
        }

        BonusTypes bonusType = pPlot->getBonusType(pUnit->getTeam());
        ImprovementTypes requiredImprovement = getBonusImprovementType(bonusType);
        BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(requiredImprovement);

        if (buildType != NO_BUILD && pPlayer_->canBuild(pPlot, buildType))
        {
            if (!pUnit->atPlot(pPlot))
            {
                if (pUnit->generatePath(pPlot, MOVE_SAFE_TERRITORY, false))
                {
                    //pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pPlot->getX(), pPlot->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
                    pushWorkerMission(pUnit, NULL, pPlot, MISSION_MOVE_TO, NO_BUILD, MOVE_SAFE_TERRITORY, "Player::checkResourcesOutsideCities");
                }
                else
                {
                    return false;
                }
            }

            //pUnit->getGroup()->pushMission(MISSION_BUILD, buildType, -1, 0, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);
            pushWorkerMission(pUnit, NULL, pPlot, MISSION_BUILD, buildType, 0, "Player::checkResourcesOutsideCities");

            if (pBestCity && pUnit->canBuildRoute())
            {
                //pUnit->getGroup()->pushMission(MISSION_ROUTE_TO, pBestCity->getX(), pBestCity->getY(), MOVE_SAFE_TERRITORY, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);
                BuildTypes routeBuildType = GameDataAnalysis::getBuildTypeForRouteType(pUnit->getGroup()->getBestBuildRoute((CvPlot *)pPlot));
                if (routeBuildType != NO_BUILD)
                {
                    pushWorkerMission(pUnit, pBestCity, pBestCity->plot(), MISSION_ROUTE_TO, routeBuildType, MOVE_SAFE_TERRITORY, "Player::checkResourcesOutsideCities");
                }
            }
#ifdef ALTAI_DEBUG
            {   // debug
                os << "\nRequested connection of resource: " << (bonusType == NO_BONUS ? " none? " : gGlobals.getBonusInfo(bonusType).getType()) << " at: " << pPlot->getCoords();

                if (pBestCity)
                {
                    os << " to: " << narrow(pBestCity->getName());
                }
                debugWorkerMissions(os);
            }
#endif
            return true;
        }
        return false;
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
                    os << "\nFound: " << thisCount << " settlers targeting: " << pTargetPlot->getCoords();
                }
#endif
            }
        }
        return count;
    }

    std::vector<IDInfo> Player::getCitiesTargetingPlot(UnitTypes unitType, XYCoords buildTarget) const
    {
        std::vector<IDInfo> cities;

        for (CityMap::const_iterator ci(cities_.begin()), ciEnd(cities_.end()); ci != ciEnd; ++ci)
        {
            if (ci->second->getConstructItem().unitType == unitType && ci->second->getConstructItem().buildTarget == buildTarget)
            {
                cities.push_back(ci->second->getCvCity()->getIDInfo());
            }
        }

        return cities;
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

        if (improvementType == NO_IMPROVEMENT)  // building a route or chopping forest/jungle
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
                if (bestRouteType != NO_ROUTE)
                {
                    os << " best route = " << gGlobals.getRouteInfo(bestRouteType).getType();
                }
            }
        }
#endif
        return additionalBuilds;
    }

    CvPlot* Player::getBestPlot(CvUnit* pUnit, int subAreaID) const
    {
        CvPlot* pPlot = pSettlerManager_->getBestPlot(pUnit, subAreaID);

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " getBestPlot() returning: ";

        if (pPlot)
        {
            os << pPlot->getCoords();
            pSettlerManager_->debugPlot(pPlot->getCoords(), os);
        }
        else
        {
            os << " null";
        }
#endif
        return pPlot;
    }

    bool Player::doSpecialistMove(CvUnitAI* pUnit)
    {
        return pPlayerAnalysis_->getPlayerTactics()->getSpecialistBuild(pUnit);
    }

    CommerceModifier Player::getCommercePercentages() const
    {
        CommerceModifier commercePercentages;

        for (int commerceType = 0; commerceType < NUM_COMMERCE_TYPES; ++commerceType)
        {
            commercePercentages[commerceType] = pPlayer_->getCommercePercent((CommerceTypes)commerceType);
        }

        return commercePercentages;
    }

    // called from CvPlayerAI::AI_getMinFoundValue
    int Player::getMaxResearchPercent(std::pair<int, int> fixedIncomeAndExpenses) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
#endif
        // don't bother to store the results for this version
        std::vector<std::pair<int, int> > outputs(11, std::make_pair(0, 0));
        int processGold = 0;

        CityIter cityIter(*pPlayer_);
        CvCity* pCity;
        while (pCity = cityIter())
        {
            const ConstructItem& constructItem = getCity(pCity).getConstructItem();
            ProcessTypes processType = constructItem.processType;
            if (processType != NO_PROCESS)
            {
                int modifier = gGlobals.getProcessInfo(processType).getProductionToCommerceModifier(COMMERCE_GOLD);
                processGold += (100 * modifier * pCity->getYieldRate(YIELD_PRODUCTION)) / 100;
            }

            CityDataPtr pCityData(new CityData(pCity));
            //ErrorLog::getLog(*getCvPlayer())->getStream() << "\nnew CityData at: " << pCityData.get();

            CityOptimiser opt(pCityData);

#ifdef ALTAI_DEBUG
            //os << "\n\tCity = " << narrow(pCity->getName());
#endif

            for (int i = 0; i <= 10; ++i)
            {
                pCityData->setCommercePercent(makeCommerce(10 * i, 100 - 10 * i, 0, 0));

                opt.optimise(NO_OUTPUT, CityOptimiser::Not_Set);

                TotalOutput output = pCityData->getOutput();
                outputs[i].first += output[OUTPUT_GOLD];
                outputs[i].second += output[OUTPUT_RESEARCH];
#ifdef ALTAI_DEBUG
                //os << "\n\ti = " << i << " gold = " << output[OUTPUT_GOLD] << ", research = " << output[OUTPUT_RESEARCH];
#endif
            }
        }

#ifdef ALTAI_DEBUG
        //os << "\nProcess gold = " << processGold;
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
        //    //os << "\nResearch = " << (100 - 10 * i) << "%, city gold total = " << outputs[i].first << ", research total = " << outputs[i].second;
        //}
        os << "\n(min found value) max research rate = " << maxRate << "%, max rate with processes= " << maxRateWithProcesses;
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
            maxGold_ = maxGoldWithProcesses_ = maxRate_ = maxRateWithProcesses_ = 0;
            goldAndResearchOutputsByCommerceRate_ = std::vector<std::pair<int, int> >(11, std::make_pair(0, 0));
            return;
        }

        int processGold = 0;

        const int fixedIncome = 100 * std::max<int>(0, pPlayer_->getGoldPerTurn());
        const int fixedExpenses = 100 * (pPlayer_->calculateInflatedCosts() + std::min<int>(0, pPlayer_->getGoldPerTurn()));
#ifdef ALTAI_DEBUG
        os << "\nFixed income = " << fixedIncome << ", fixed expenses = " << fixedExpenses;
#endif
        goldAndResearchOutputsByCommerceRate_ = std::vector<std::pair<int, int> >(11, std::make_pair(0, 0));

        CityIter cityIter(*pPlayer_);
        CvCity* pCity;
        while (pCity = cityIter())
        {
            const ConstructItem& constructItem = getCity(pCity).getConstructItem();
            ProcessTypes processType = constructItem.processType;
            if (processType != NO_PROCESS)
            {
                int modifier = gGlobals.getProcessInfo(processType).getProductionToCommerceModifier(COMMERCE_GOLD);
                processGold += (100 * modifier * pCity->getYieldRate(YIELD_PRODUCTION)) / 100;
            }

            CityDataPtr pCityData(new CityData(pCity));
            //ErrorLog::getLog(*getCvPlayer())->getStream() << "\nnew CityData at: " << pCityData.get();

            CityOptimiser opt(pCityData);

            for (int i = 0; i <= 10; ++i)
            {
                pCityData->setCommercePercent(makeCommerce(10 * i, 100 - 10 * i, 0, 0));

                opt.optimise(NO_OUTPUT, CityOptimiser::Not_Set);

                TotalOutput output = pCityData->getOutput();
                goldAndResearchOutputsByCommerceRate_[i].first += output[OUTPUT_GOLD];
                goldAndResearchOutputsByCommerceRate_[i].second += output[OUTPUT_RESEARCH];
            }
        }

        int maxRate = 0, maxRateWithProcesses = 0, maxGold = 0, maxGoldWithProcesses = 0;
        for (int i = 10; i >= 0; --i)
        {
            const int thisRatesGoldTotal = goldAndResearchOutputsByCommerceRate_[i].first + fixedIncome - fixedExpenses;
            if (thisRatesGoldTotal >= 0)
            {
                maxRate = 100 - 10 * i;
                maxGold = std::max<int>(thisRatesGoldTotal, maxGold);
            }

            const int thisRatesGoldTotalInclProcesses = thisRatesGoldTotal + processGold;
            if (goldAndResearchOutputsByCommerceRate_[i].first + fixedIncome + processGold - fixedExpenses >= 0)
            {
                maxRateWithProcesses = 100 - 10 * i;
                maxGoldWithProcesses = std::max<int>(thisRatesGoldTotalInclProcesses, maxGoldWithProcesses);
            }
        }

#ifdef ALTAI_DEBUG
        os << "\nProcess gold = " << processGold;
        for (int i = 0; i <= 10; ++i)
        {
            os << "\nResearch = " << (100 - 10 * i) << "%, city gold total = " 
                << goldAndResearchOutputsByCommerceRate_[i].first << ", research total = " << goldAndResearchOutputsByCommerceRate_[i].second;
        }
        os << "\nMax research rate = " << maxRate << "%, max research rate with process = " << maxRateWithProcesses;
        os << "\nMax gold = " << maxGold << ", with processes = " << maxGoldWithProcesses;
#endif

        maxRate_ = maxRate;
        maxGold_ = maxGold;
        maxRateWithProcesses_ = maxRateWithProcesses;
        maxGoldWithProcesses_ = maxGoldWithProcesses;
    }

    int Player::getMaxResearchPercent() const
    {
        return maxRate_;
    }

    int Player::getMaxResearchPercentWithProcesses() const
    {
        return maxRateWithProcesses_;
    }

    int Player::getMaxGoldRate() const
    {
        return maxGold_;
    }

    int Player::getMaxGoldRateWithProcesses() const
    {
        return maxGoldWithProcesses_;
    }

    const std::vector<std::pair<int, int> >& Player::getGoldAndResearchRates() const
    {
        return goldAndResearchOutputsByCommerceRate_;
    }

    TotalOutput Player::getCurrentOutput()
    {
        TotalOutput currentOutput;
        CityIter cityIter(*pPlayer_);
        while (CvCity* pCity = cityIter())
        {
            City& city = getCity(pCity);
            currentOutput += city.getCityData()->getOutput();
        }
        return currentOutput;
    }

    TotalOutput Player::getCurrentProjectedOutput()
    {
        TotalOutput currentProjectedOutput;
        CityIter cityIter(*pPlayer_);
        while (CvCity* pCity = cityIter())
        {
            City& city = getCity(pCity);
            currentProjectedOutput += city.getBaseOutputProjection().getOutput();
        }
        return currentProjectedOutput;
    }

    void Player::logMission(CvSelectionGroup* pGroup, MissionData missionData, MissionAITypes eMissionAI, CvPlot* pMissionAIPlot, CvUnit* pMissionAIUnit, const char* callingFunction) const
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();

        CvWString missionString, missionTypeString;
        getMissionAIString(missionString, eMissionAI);
        getMissionTypeString(missionTypeString, missionData.eMissionType);

        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << ", group = " << pGroup->getID();
        debugSelectionGroup(pGroup, os);
        os << ", pushed mission: " << narrow(missionString.GetCString())
           << " type = " << narrow(missionTypeString.GetCString())
           << " to plot: " << (pMissionAIPlot ? pMissionAIPlot->getCoords() : XYCoords(-1, -1))
           << " for unit: " << (pMissionAIUnit ? gGlobals.getUnitInfo(pMissionAIUnit->getUnitType()).getType() : "NO_UNIT")
           << " (iData1 = " << missionData.iData1 << ", iData2 = " << missionData.iData2 << ", iFlags = " << missionData.iFlags << ")";
        if (callingFunction)
        {
            os << " caller = " << callingFunction;
        }

        boost::shared_ptr<UnitLog> pUnitLog = UnitLog::getLog(*pPlayer_);
        std::ostream& osUnit = pUnitLog->getStream();
        osUnit << "\nTurn = " << gGlobals.getGame().getGameTurn() << " " << __FUNCTION__;
        pUnitLog->logSelectionGroup(pGroup);
#endif
    }

    void Player::logClearMissions(const CvSelectionGroup* pGroup, const std::string& caller) const
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

        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << ", group = " << pGroup->getID();
        debugSelectionGroup(pGroup, os);
        os << " cancelled mission: " << (pHeadMission ? narrow(missionTypeString.GetCString()) : "none") << " caller = " << caller;
#endif
    }

    void Player::logFailedPath(const CvSelectionGroup* pGroup, const CvPlot* pFromPlot, const CvPlot* pToPlot, int iFlags) const
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer_);
        std::ostream& os = pCivLog->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << ", group = " << pGroup->getID() << " failed to find path from: " << pFromPlot->getCoords() << " to: " << pToPlot->getCoords()
            << ", flags = " << iFlags;
#endif
    }

    void Player::logEmphasis(IDInfo city, EmphasizeTypes eIndex, bool bNewValue) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << ", city = " << safeGetCityName(city) << " set emphasis: "
           << gGlobals.getEmphasizeInfo(eIndex).getType() << " to: " << bNewValue;
#endif
    }

    void Player::logScrapUnit(const CvUnit* pUnit) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nScrapping unit at: " << pUnit->plot()->getCoords() << " type = " << pUnit->getUnitInfo().getType();
#endif
    }

    void Player::addTech(TechTypes techType, TechSources source)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        {
            os << "\nTech acquired: " << gGlobals.getTechInfo(techType).getType() << " turn = " << gGlobals.getGame().getGameTurn() << " source: ";
            switch (source)
            {
                case RESEARCH_TECH:
                    os << " research";
                    break;
                case INITIAL_TECH:
                    os << " starting tech";
                    break;
                case TEAM_TECH:
                    os << " team";
                    break;
                case SHARE_TECH:
                    os << " tech share";
                    break;
                case TRADE_TECH:
                    os << " trade";
                    break;
                case GOODY_TECH:
                    os << " goody hut";
                    break;
                case STOLEN_TECH:
                    os << " stolen";
                    break;
                case FREE_TECH:
                    os << " free";
                    break;
                case CHEAT_TECH:
                    os << " cheat";
                    break;
                case NOT_KNOWN:
                    os << " unknown";
                    break;
                default:
                    os << " unset";
            }
        }
#endif

        pPlayerAnalysis_->recalcTechDepths();

        pPlayerAnalysis_->getMapAnalysis()->reinitDotMap();

        // store tech requirement against buildings - so need to update these (before updating tech tactics)
        // similarly for units (potentially)
        //pPlayerAnalysis_->getPlayerTactics()->updateUnitTactics();
        pPlayerAnalysis_->getPlayerTactics()->updateTechTactics();
        //pPlayerAnalysis_->getPlayerTactics()->updateProjectTactics();

        boost::shared_ptr<TechInfo> pTechInfo = pPlayerAnalysis_->getTechInfo(techType);

        if (techAffectsBuildings(pTechInfo))
        {
            for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
            {
                iter->second->setFlag(City::NeedsBuildingCalcs);
            }            
        }

        // add any new buildings this tech makes available, and update existing building tactics data
        pPlayerAnalysis_->getPlayerTactics()->updateCityBuildingTactics(pTechInfo->getTechType());

        // add any new units this tech makes available, and update existing unit tactics data
        pPlayerAnalysis_->getPlayerTactics()->updateCityUnitTactics(pTechInfo->getTechType());

        std::vector<ImprovementTypes> affectedImprovements;
        std::vector<FeatureTypes> removeFeatureTypes;
        // todo - make use of the list of imps/features to possibly filter setting flag (e.g. an inland city doesn't care about fishing boats)
        if (techAffectsImprovements(pTechInfo, affectedImprovements, removeFeatureTypes))
        {
            for (CityMap::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
            {
                iter->second->setFlag(City::NeedsImprovementCalcs);
            }
        }

        std::vector<BonusTypes> revealedBonusTypes = getRevealedBonuses(pTechInfo);
        if (!revealedBonusTypes.empty())
        {
            getAnalysis()->getMapAnalysis()->updateResourceData(revealedBonusTypes);
        }

        boost::shared_ptr<IEvent<NullRecv> > pEvent = boost::shared_ptr<IEvent<NullRecv> >(new DiscoverTech(techType));
        pPlayerAnalysis_->update(pEvent);
    }

    void Player::gaveTech(TechTypes techType, PlayerTypes fromPlayer, PlayerTypes toPlayer)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nTech shared: " << gGlobals.getTechInfo(techType).getType() << " turn = " << gGlobals.getGame().getGameTurn()
           << " from player: " << narrow(CvPlayerAI::getPlayer(fromPlayer).getName()) << " to player: " << narrow(CvPlayerAI::getPlayer(toPlayer).getName());
#endif
    }

    void Player::meetTeam(TeamTypes teamType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        PlayerIter playerIter(teamType);
        while (const CvPlayerAI* player = playerIter())
        {
            os << "\nMet team: " << teamType << " player: " << narrow(player->getName());
        }        
#endif
    }

    int Player::getTechResearchDepth(TechTypes techType) const
    {
        return pPlayerAnalysis_->getTechResearchDepth(techType);
    }

    void Player::updatePlotInfo(const CvPlot* pPlot, bool isNew, const std::string& caller)
    {
        getAnalysis()->getMapAnalysis()->updatePlotInfo(pPlot, isNew, caller);
    }

    void Player::updatePlotRevealed(const CvPlot* pPlot, bool isNew, bool isRevealed)
    {
        if (isRevealed)
        {
            bool isNewSubArea = getAnalysis()->getMapAnalysis()->updatePlotRevealed(pPlot, isNew);
            if (isNewSubArea)
            {
                pPlayerAnalysis_->getMilitaryAnalysis()->addNewSubArea(pPlot->getSubArea());
            }
        }

        UnitPlotIter unitIter(pPlot);
        while (CvUnit* pUnit = unitIter())
        {
            if (!pUnit->isInvisible(getTeamID(), false))
            {
                if (isRevealed)
                {
                    addPlayerUnit((CvUnitAI*)pUnit, pPlot);
                }
                else
                {
                    hidePlayerUnit((CvUnitAI*)pUnit, pPlot, false);
                }
            }
        }
    }

    void Player::updatePlotBonus(const CvPlot* pPlot, BonusTypes revealedBonusType)
    {
        getAnalysis()->getMapAnalysis()->updatePlotBonus(pPlot, revealedBonusType);
        getAnalysis()->getWorkerAnalysis()->updatePlotBonus(pPlot, revealedBonusType);
        getAnalysis()->getMapDelta()->updatePlotBonus(pPlot, revealedBonusType);
    }

    void Player::updatePlotFeature(const CvPlot* pPlot, FeatureTypes oldFeatureType)
    {
        getAnalysis()->getMapAnalysis()->updatePlotFeature(pPlot, oldFeatureType);
    }

    void Player::updatePlotImprovement(const CvPlot* pPlot, ImprovementTypes oldImprovementType)
    {
        getAnalysis()->getMapAnalysis()->updatePlotImprovement(pPlot, oldImprovementType);
        if (pPlot->getOwner() == pPlayer_->getID())
        {
            getAnalysis()->getWorkerAnalysis()->updateOwnedPlotImprovement(pPlot, oldImprovementType);
        }        
    }

    void Player::updatePlotCulture(const CvPlot* pPlot, PlayerTypes previousRevealedOwner, PlayerTypes newRevealedOwner)
    {
        getAnalysis()->getMapAnalysis()->updatePlotCulture(pPlot, previousRevealedOwner, newRevealedOwner);
        getAnalysis()->getWorkerAnalysis()->updatePlotOwner(pPlot, previousRevealedOwner, newRevealedOwner);
        getAnalysis()->getMapDelta()->updatePlotOwner(pPlot, previousRevealedOwner, newRevealedOwner);
    }

    void Player::updateCityBonusCount(const CvCity* pCity, BonusTypes bonusType, int delta)
    {
        getAnalysis()->getWorkerAnalysis()->updateCityBonusCount(pCity, bonusType, delta);
    }

    void Player::updateCityGreatPeople(IDInfo city)
    {
#ifdef ALTAI_DEBUG
        CivLog::getLog(*pPlayer_)->getStream() << "\nGPP born in: " << narrow(::getCity(city)->getName());
#endif
    }

    void Player::eraseLimitedBuildingTactics(BuildingTypes buildingType)
    {
#ifdef ALTAI_DEBUG
        CivLog::getLog(*pPlayer_)->getStream() << "\nErasing limited building tactic: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
        getAnalysis()->getPlayerTactics()->eraseLimitedBuildingTactics(buildingType);
    }

    void Player::notifyReligionFounded(ReligionTypes religionType, bool isOurs)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nNotifying religion founded: " << gGlobals.getReligionInfo(religionType).getType() << (isOurs ? " by us " : " by someone else ");
#endif
        //getAnalysis()->getPlayerTactics()->unselectTechReligionTactics((TechTypes)(gGlobals.getReligionInfo(religionType).getTechPrereq()));
    }

    void Player::notifyCommerceRateChanged(CommerceTypes commerceType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nCommerce rate: " << gGlobals.getCommerceInfo(commerceType).getType() << " changed to: " << pPlayer_->getCommercePercent(commerceType);
#endif
    }

    void Player::notifyFirstToTechDiscovered(TeamTypes teamType, TechTypes techType)
    {
        //pPlayerAnalysis_->getPlayerTactics()->updateFirstToTechTactics(techType);
    }

    void Player::logHurry(const CvCity* pCity, const HurryData& hurryData) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer_)->getStream();
        os << "\nCity: " << narrow(pCity->getName()) << " hurries: ";
        if (pCity->isProductionBuilding())
        {
            os << gGlobals.getBuildingInfo(pCity->getProductionBuilding()).getType();
        }
        else if (pCity->isProductionUnit())
        {
            os << gGlobals.getUnitInfo(pCity->getProductionUnit()).getType();
        }
        os << hurryData;
#endif
    }

    void Player::setCityDirty(IDInfo city)
    {
        cityFlags_.insert(city);
    }

    void Player::updateCityData()
    {
        std::set<int> subAreas;
        std::vector<int> subAreaIds = getAnalysis()->getMapAnalysis()->getAccessibleSubAreas(DOMAIN_LAND);
        subAreas.insert(subAreaIds.begin(), subAreaIds.end());
        std::vector<int> waterSubAreaIds = getAnalysis()->getMapAnalysis()->getAccessibleSubAreas(DOMAIN_SEA);
        subAreas.insert(waterSubAreaIds.begin(), waterSubAreaIds.end());

        getAnalysis()->getMapAnalysis()->analyseSharedPlots(cityFlags_);

        std::set<BonusTypes> possiblyReachableBonusTypes;
        for (std::set<int>::const_iterator si(subAreas.begin()), siEnd(subAreas.end()); si != siEnd; ++si)
        {
            std::vector<CvPlot*> subAreaResourcePlots = getAnalysis()->getMapAnalysis()->getResourcePlots(*si);
            for (size_t i = 0, count = subAreaResourcePlots .size(); i < count; ++i)
            {
                possiblyReachableBonusTypes.insert(subAreaResourcePlots[i]->getBonusType(pPlayer_->getTeam()));
            }
        }

        for (std::set<BonusTypes>::const_iterator bi(possiblyReachableBonusTypes.begin()), biEnd(possiblyReachableBonusTypes.end()); bi != biEnd; ++bi)
        {
            pPlayerAnalysis_->getPlayerTactics()->resourceTacticsMap_[*bi]->update(*this);
        }

        cityFlags_.clear();
    }

    bool Player::isSharedPlot(const CvPlot* pPlot) const
    {
        return getAnalysis()->getMapAnalysis()->isSharedPlot(pPlot->getCoords());
    }

    const CvCity* Player::getSharedPlotAssignedCity(const CvPlot* pPlot) const
    {
        return ::getCity(getAnalysis()->getMapAnalysis()->getSharedPlotAssignedCity(pPlot->getCoords()));
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

    std::vector<int /* plot num */> Player::getBestCitySites(int minValue, int count)
    {
        return pSettlerManager_->getBestCitySites(minValue, count);
    }

    /*std::set<BonusTypes> Player::getBonusesForSites(int siteCount) const
    {
        return pSettlerManager_->getBonusesForSites(siteCount);
    }*/

    TechTypes Player::getResearchTech(TechTypes ignoreTechType)
    {
        // try and wait until we found a city as early techs are driven by potential city improvements
        // we could try and precalc based on city spots, but it's quite complex for an edge case and
        // we can just try and the use the tech overflow. Same limit used here as applies to the human player for defering tech selection
        if (pPlayer_->getNumCities() == 0 && gGlobals.getGame().getElapsedGameTurns() <= 4)
        {
            return NO_TECH;
        }

        ResearchTech researchTech = pPlayerAnalysis_->getResearchTech(ignoreTechType);
        if (ignoreTechType == NO_TECH)
        {
            researchTech_ = researchTech;
        }
        return researchTech.techType;
    }

    std::pair<int, int> Player::getCityRank(IDInfo city, OutputTypes outputType) const
    {
        // todo - single pass algorithm should be possible
        std::multimap<int, IDInfo, std::greater<int> > outputs;
        for (CityMap::const_iterator ci(cities_.begin()), ciEnd(cities_.end()); ci != ciEnd; ++ci)
        {
            outputs.insert(std::make_pair(ci->second->getMaxOutputs()[outputType], ci->second->getCvCity()->getIDInfo()));
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
        return std::make_pair(MAX_INT, 0);  // indicates city not found
    }

    int Player::getUnitCount(UnitTypes unitType, bool includeUnderConstruction) const
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

        if (includeUnderConstruction)
        {
            CityIter iter(*pPlayer_);

            while (CvCity* pCity = iter())
            {
                const City& city = getCity(pCity);
                //if (city.getConstructItem().unitType == unitType)
                if (city.getCvCity()->getProductionUnit() == unitType)
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
                const City& city = getCity(pCity);
                if (city.getConstructItem().unitType != NO_UNIT)
                {
                    if (domainType != DOMAIN_AIR)
                    {
                        if (gGlobals.getUnitInfo(city.getConstructItem().unitType).getCombat() > 0)
                        {
                            ++count;
                        }
                    }
                    else if (gGlobals.getUnitInfo(city.getConstructItem().unitType).getAirCombat() > 0)
                    {
                        ++count;
                    }
                }
            }
        }
        return count;
    }

    int Player::getUnitCount(UnitAITypes unitAIType) const
    {
        int count = 0;
        SelectionGroupIter iter(*pPlayer_);

        while (CvSelectionGroup* pGroup = iter())
        {
            UnitGroupIter unitIter(pGroup);
            while (const CvUnit* pUnit = unitIter())
            {
				const CvUnitAI* pUnitAI = (const CvUnitAI*)pUnit;
                if (pUnitAI)
				{
					if (pUnitAI->AI_getUnitAIType() == unitAIType)
					{
						++count;
					}
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
                const City& city = getCity(pCity);
                if (city.getConstructItem().unitType != NO_UNIT)
                {
                    if (gGlobals.getUnitInfo(city.getConstructItem().unitType).getCollateralDamageMaxUnits() > 0)
                    {
                        ++count;
                    }
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
                const City& city = getCity(pCity);
                if (city.getConstructItem().unitType != NO_UNIT)
                {
                    if (gGlobals.getUnitInfo(city.getConstructItem().unitType).isNoBadGoodies())
                    {
                        ++count;
                    }
                }
            }
        }
        return count;
    }

    void Player::notifyHaveReligion(ReligionTypes religionType)
    {
        pPlayerAnalysis_->getPlayerTactics()->updateCityReligionBuildingTactics(religionType);
    }

    void Player::notifyLostReligion(ReligionTypes religionType)
    {
        // todo
    }

    int Player::getNumKnownPlayers() const
    {
        // note this function only looks at the player level
        // todo: a team version, e.g. isolation check should be team v. team
        const PlayerTypes ourself = pPlayer_->getID();
        int count = 0;
        PlayerIter playerIter;
        while (const CvPlayerAI* player = playerIter())
        {
            // count civs we've met, ignoring barbs
            if (player->getID() != ourself && player->AI_isFirstContact(ourself) && !player->isMinorCiv() && !player->isBarbarian())
            {
                ++count;
            }
        }
        return count;
    }

    void Player::write(FDataStreamBase* pStream) const
    {
        pStream->Write(maxRate_);
        pStream->Write(maxRateWithProcesses_);
        pStream->Write(maxGold_);
        pStream->Write(maxGoldWithProcesses_);

        pPlayerAnalysis_->write(pStream);
    }

    void Player::read(FDataStreamBase* pStream)
    {
        pStream->Read(&maxRate_);
        pStream->Read(&maxRateWithProcesses_);
        pStream->Read(&maxGold_);
        pStream->Read(&maxGoldWithProcesses_);

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
            os << "\nSite at: " << pPlot->getCoords() << " value = " << pPlot->getFoundValue(pPlayer_->getID());
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
                os << "move to: " << pBestPlot->getCoords() << " and ";
            }
            os << "found city at: " << pBestFoundPlot->getCoords();
        }
        else
        {
            os << "\nFailed to find suitable settler mission. ";
            if (pBestFoundPlot)
            {
                os << "Best found plot = " << pBestFoundPlot->getCoords();
            }
            if (pBestPlot)
            {
                os << " Best move to plot = " << pBestPlot->getCoords();
            }
        }
        os << " path turns = " << pathTurns;
#endif
    }

    void Player::logUnitAIChange(const CvUnitAI* pUnit, UnitAITypes oldAI, UnitAITypes newAI) const
    {
#ifdef ALTAI_DEBUG
        if (!pPlayer_) return;  // maybe true when loading saved game
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

        os << "\nSelection group at: " << pHeadUnit->plot()->getCoords() << " stuck. Head unit is: " << gGlobals.getUnitInfo(pHeadUnit->getUnitType()).getType();
    }

    void Player::logInvalidUnitBuild(const CvUnit* pUnit, BuildTypes buildType) const
    {
        std::ostream& os = ErrorLog::getLog(*pPlayer_)->getStream();
        os << "\nInvalid build for unit: " << pUnit->getID() << " = " << gGlobals.getBuildInfo(buildType).getType();
    }
}