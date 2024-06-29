#include "AltAI.h"

#include "./city_projections.h"
#include "./player_analysis.h"
#include "./building_info_visitors.h"
#include "./civic_info_visitors.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./maintenance_helper.h"
#include "./building_helper.h"
#include "./culture_helper.h"
#include "./modifiers_helper.h"
#include "./happy_helper.h"
#include "./unit_helper.h"
#include "./civ_helper.h"
#include "./buildings_info.h"
#include "./civic_info.h"
#include "./unit_info.h"
#include "./civ_log.h"

namespace AltAI
{
    namespace
    {
        struct EventTimeOrderF
        {
            bool operator() (const IProjectionEventPtr& pEvent1, const IProjectionEventPtr& pEvent2) const
            {
                return pEvent1->getTurnsToEvent() < pEvent2->getTurnsToEvent();
            }
        };

        void updateProjections(const Player& player, const CityDataPtr& pCityData, int nTurns, std::vector<IProjectionEventPtr>& events, 
            const ConstructItem& constructItem, const bool doComparison, const bool debug, ProjectionLadder& ladder)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData->getOwner()))->getStream();
            //if (debug)
            //{
            //    os << "\nupdateProjections - turns left = " << nTurns << ", " << events.size() << " events";
            //}
#endif
            //CityOptimiser cityOptimiser(pCityData);
            std::vector<TotalOutputPriority> outputPriorities = makeSimpleOutputPriorities(pCityData);
            ProjectionLadder comparisonLadder;
            CityDataPtr pComparisonCityData;
            std::vector<IProjectionEventPtr> comparisonEvents;

            while (nTurns > 0)
            {
                player.getCity(pCityData->getCity()->getID()).optimisePlots(pCityData, constructItem);
                //outputPriorities = makeSimpleOutputPriorities(pCityData);
                //cityOptimiser.optimise(outputPriorities);
#ifdef ALTAI_DEBUG
                //if (debug)
                //{
                    //os << " target yield = " << cityOptimiser.getTargetYield();
                //}
#endif
                std::sort(events.begin(), events.end(), EventTimeOrderF());

                int turnsToFirstEvent = (events.empty() ? MAX_INT : events[0]->getTurnsToEvent());
#ifdef ALTAI_DEBUG
                std::ostringstream oss;
                if (debug)
                {
                    pCityData->debugBasicData(oss);
                    pCityData->debugSummary(oss);
                }
#endif
                TotalOutput output = pCityData->getOutput(), processOutput = pCityData->getProcessOutput();
                output[OUTPUT_FOOD] -= pCityData->getLostFood();

                ladder.entries.push_back(ProjectionLadder::Entry(pCityData->getPopulation(), std::min<int>(nTurns, turnsToFirstEvent),
                    pCityData->getStoredFood(), pCityData->getCurrentProduction() * std::min<int>(nTurns, turnsToFirstEvent),
                    output, processOutput, 
                    pCityData->getMaintenanceHelper()->getMaintenance(), pCityData->getGPP()));

                // add in production from hurrying - if not building anything
                if (constructItem.isEmpty() && pCityData->getAccumulatedProduction() > 0)
                {
                    // 100 is from dummy hammer to allow hurry calc to not think hurrying on first turn
                    ladder.entries.rbegin()->accumulatedProduction += (pCityData->getAccumulatedProduction() - 100);
                    pCityData->setAccumulatedProduction(0);
                }

                const PlotDataList& plots = pCityData->getPlotOutputs();
                for (PlotDataList::const_iterator plotIter(plots.begin()), plotEndIter(plots.end()); plotIter != plotEndIter; ++plotIter)
                {
                    if (plotIter->isWorked)
                    {
                        ladder.entries.rbegin()->workedPlots.push_back(*plotIter);
                    }
                }

                /*for (size_t hurryIndex = 0, hurryCount = gGlobals.getNumHurryInfos(); hurryIndex < hurryCount; ++hurryIndex)
                {
                    if (player.getCvPlayer()->canHurry((HurryTypes)hurryIndex))
                    {
                        bool canHurry;
                        HurryData hurryData((HurryTypes)hurryIndex);
                        boost::tie(canHurry, hurryData) = pCityData->canHurry((HurryTypes)hurryIndex);
                        if (canHurry)
                        {
                            ladder.entries.rbegin()->hurryData.push_back(hurryData);
                        }
                    }
                }*/

                std::vector<IProjectionEventPtr> newEvents;

                /*bool makeComparison = false;
                if (doComparison && turnsToFirstEvent < nTurns)
                {
                    for (size_t i = 0, count = events.size(); i < count; ++i)
                    {
                        if (events[i]->getTurnsToEvent() > turnsToFirstEvent)
                        {
                            break;
                        }
                        if (events[i]->generateComparison())
                        {
                            makeComparison = true;
                            break;
                        }
                    }
                }*/

//                if (makeComparison)
//                {
//                    comparisonLadder = ladder;
//                    pComparisonCityData = pCityData->clone();
//                    
//                    for (size_t i = 0, count = events.size(); i < count; ++i)
//                    {
//                        if (events[i]->generateComparison())
//                        {
//                            // skip this event, as this is the one we are comparing with
//                            continue;
//                        }
//
//                        IProjectionEventPtr pCloneEvent = events[i]->clone(pComparisonCityData);
//                        pCloneEvent->init(pComparisonCityData);
//                        IProjectionEventPtr pNewEvent = pCloneEvent->updateEvent(turnsToFirstEvent, comparisonLadder);
//                        if (pNewEvent)
//                        {
//                            comparisonEvents.push_back(pNewEvent);
//                        }
//                    }
//
//                    for (size_t i = 0, count = comparisonEvents.size(); i < count; ++i)
//                    {
//                        comparisonEvents[i]->updateCityData(std::min<int>(nTurns, turnsToFirstEvent));
//                    }
//#ifdef ALTAI_DEBUG
//                    //comparisonLadder.entries.rbegin()->debugSummary = oss.str();
//                    //os << "\nPerforming comparison...";
//#endif
//                    // nTurns - turnsToFirstEvent is > 0, otherwise we won't have triggered the comparison
//                    updateProjections(player, pComparisonCityData, nTurns - turnsToFirstEvent, comparisonEvents, constructItem, false, debug, comparisonLadder);
//                }

                // copy events before update city data, as updates could change them
                for (size_t i = 0, count = events.size(); i < count; ++i)
                {
                    IProjectionEventPtr pNewEvent = events[i]->updateEvent(std::min<int>(nTurns, turnsToFirstEvent), ladder);
                    if (pNewEvent)
                    {
                        newEvents.push_back(pNewEvent);
                    }
                }

                // update city data...
                for (size_t i = 0, count = events.size(); i < count; ++i)
                {
                    events[i]->updateCityData(std::min<int>(nTurns, turnsToFirstEvent));
#ifdef ALTAI_DEBUG
                    //if (debug)
                    //{
                        /*if (events[i]->getTurnsToEvent() == turnsToFirstEvent)
                        {
                            events[i]->debug(oss);
                        }*/
                        //if (i == 0) os << "\n\nProjection events for city: " << narrow(pCityData->getCity()->getName()) << " turns to next event = " << turnsToFirstEvent;
                        //events[i]->debug(os);
                    //}
#endif
                }


#ifdef ALTAI_DEBUG
                if (debug)
                {
                    ladder.entries.rbegin()->debugSummary = oss.str();
                }
#endif
                if (turnsToFirstEvent <= nTurns)
                {
                    //outputPriorities = makeSimpleOutputPriorities(pCityData);
                    //cityOptimiser.optimise(outputPriorities);
                    player.getCity(pCityData->getCity()->getID()).optimisePlots(pCityData, constructItem);
                }
                nTurns -= turnsToFirstEvent;
                events = newEvents;
            }

            /*if (!comparisonLadder.entries.empty())
            {
                ladder.comparisons.push_back(comparisonLadder);
            }*/

#ifdef ALTAI_DEBUG
            if (debug)
            {
                ladder.debug(os);
            }
#endif
        }
    }

    ProjectionBuildingEvent::ProjectionBuildingEvent(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
        : pBuildingInfo_(pBuildingInfo), accumulatedTurns_(0), hurryEvent_(NO_HURRY, -1)
    {
    }

    ProjectionBuildingEvent::ProjectionBuildingEvent(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, const std::pair<HurryTypes, int>& hurryEvent)
        : pBuildingInfo_(pBuildingInfo), accumulatedTurns_(0), hurryEvent_(hurryEvent)
    {
    }

    ProjectionBuildingEvent::ProjectionBuildingEvent(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int accumulatedTurns, const std::pair<HurryTypes, int>& hurryEvent)
        : pBuildingInfo_(pBuildingInfo), accumulatedTurns_(accumulatedTurns), hurryEvent_(hurryEvent)
    {
    }
    
    void ProjectionBuildingEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
    }

    IProjectionEventPtr ProjectionBuildingEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionBuildingEvent(pBuildingInfo_, accumulatedTurns_, hurryEvent_));
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionBuildingEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionBuildingEvent event: " << gGlobals.getBuildingInfo(pBuildingInfo_->getBuildingType()).getType()
           << " reqd prod = " << pCityData_->getRequiredProduction() - pCityData_->getAccumulatedProduction()
           << " accumulated turns = " << accumulatedTurns_;
    }

    int ProjectionBuildingEvent::getTurnsToEvent() const
    {
        int production = pCityData_->getCurrentProduction();
        int turnsToComplete = MAX_INT;

        if (production > 0)
        {
            const int remainingProduction = std::max<int>(0, (pCityData_->getRequiredProduction() - pCityData_->getAccumulatedProduction()));
            if (remainingProduction <= 0)
            {
                turnsToComplete = 1;  // we hurried the building - it will complete next turn
            }
            else
            {
                const int productionRate = remainingProduction / production;
                const int productionDelta = remainingProduction % production;
                
                turnsToComplete = productionRate + (productionDelta ? 1 : 0);
            }
        }

        return hurryEvent_.first != NO_HURRY ? std::min<int>(turnsToComplete, hurryEvent_.second) : turnsToComplete;
    }

    bool ProjectionBuildingEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool ProjectionBuildingEvent::generateComparison() const
    {
        return true;
    }

    void ProjectionBuildingEvent::updateCityData(int nTurns)
    {
        // todo - handle overflow properly here, including from hurrying
        if (pCityData_->getRequiredProduction() - pCityData_->getAccumulatedProduction() > 0)
        {
            pCityData_->updateProduction(nTurns);
        }

        if (pCityData_->getRequiredProduction() - pCityData_->getAccumulatedProduction() <= 0)
        {
            pCityData_->getBuildingsHelper()->changeNumRealBuildings(pBuildingInfo_->getBuildingType());
            updateRequestData(*pCityData_, pBuildingInfo_);
            pCityData_->clearBuildQueue();
        }
        else if (hurryEvent_.first != NO_HURRY && accumulatedTurns_ >= hurryEvent_.second)
        {
            bool canHurry;
            HurryData hurryData;
            boost::tie(canHurry, hurryData) = pCityData_->canHurry(hurryEvent_.first);

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData_->getOwner()))->getStream();
            os << "\nHurrying building: ";
            debug(os);
            os << " can hurry = " << canHurry << ", hurryData = " << hurryData;
#endif

            if (canHurry)
            {
                pCityData_->hurry(hurryData);
                hurryEvent_ = std::make_pair(NO_HURRY, -1);
            }
        }
    }

    IProjectionEventPtr ProjectionBuildingEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {
        int turnsToComplete = getTurnsToEvent();

        if (turnsToComplete <= nTurns || hurryEvent_.first != NO_HURRY)  // hacky way to tell we've hurried the building - so it should complete next turn
        {
            ladder.buildings.push_back(std::make_pair(turnsToComplete + accumulatedTurns_, pBuildingInfo_->getBuildingType()));
            
            return IProjectionEventPtr();  // we're done
        }
        else 
        {
            accumulatedTurns_ += nTurns;
            return shared_from_this();
        }
    }


    ProjectionUnitEvent::ProjectionUnitEvent(const CvCity* pCity, const boost::shared_ptr<UnitInfo>& pUnitInfo)
        : pUnitInfo_(pUnitInfo)
    {
        isFoodProduction_ = gGlobals.getUnitInfo(pUnitInfo->getUnitType()).isFoodProduction();
        requiredProduction_ = 100 * pCity->getProductionNeeded(pUnitInfo_->getUnitType());
        accumulatedTurns_ = 0;
    }
    
    ProjectionUnitEvent::ProjectionUnitEvent(const boost::shared_ptr<UnitInfo>& pUnitInfo, bool isFoodProduction, int requiredProduction, int accumulatedTurns)
        : pUnitInfo_(pUnitInfo), isFoodProduction_(isFoodProduction), requiredProduction_(requiredProduction), accumulatedTurns_(accumulatedTurns)
    {
    }

    void ProjectionUnitEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
    }

    IProjectionEventPtr ProjectionUnitEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionUnitEvent(pUnitInfo_, isFoodProduction_, requiredProduction_, accumulatedTurns_));
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionUnitEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionUnitEvent event: " << " reqd prod = " << requiredProduction_ << " accumulated turns = " << accumulatedTurns_;
    }

    int ProjectionUnitEvent::getTurnsToEvent() const
    {
        int production = pCityData_->getCurrentProduction();
        int turnsToComplete = MAX_INT;

        if (production > 0)
        {
            const int productionRate = requiredProduction_ / production;
            const int productionDelta = requiredProduction_ % production;
                
            turnsToComplete = productionRate + (productionDelta ? 1 : 0);
        }

        return turnsToComplete;
    }

    bool ProjectionUnitEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool ProjectionUnitEvent::generateComparison() const
    {
        return false;
    }

    void ProjectionUnitEvent::updateCityData(int nTurns)
    {
        pCityData_->updateProduction(nTurns);
        if (requiredProduction_ <= 0)
        {
            pCityData_->clearBuildQueue();
        }
    }

    IProjectionEventPtr ProjectionUnitEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {
        int turnsToComplete = getTurnsToEvent();

        if (turnsToComplete <= nTurns)
        {
            requiredProduction_ = 0;

            ProjectionLadder::ConstructedUnit unit(pUnitInfo_->getUnitType(), turnsToComplete + accumulatedTurns_);
            unit.experience = pCityData_->getUnitHelper()->getUnitFreeExperience(pUnitInfo_->getUnitType());
            unit.level = gGlobals.getGame().getAltAI()->getPlayer(pCityData_->getOwner())->getAnalysis()->getUnitLevel(unit.experience);

            ladder.units.push_back(unit);
            
            return IProjectionEventPtr();  // we're done
        }
        else 
        {
            accumulatedTurns_ += nTurns;
            if (turnsToComplete < MAX_INT)
            {
                requiredProduction_ -= nTurns * pCityData_->getCurrentProduction();
            }
            return shared_from_this();
        }
    }


    ProjectionGlobalBuildingEvent::ProjectionGlobalBuildingEvent(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int turnBuilt, const CvCity* pBuiltInCity)
        : pBuildingInfo_(pBuildingInfo), remainingTurns_(turnBuilt), pBuiltInCity_(pBuiltInCity), complete_(false)
    {
    }

    void ProjectionGlobalBuildingEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
    }

    IProjectionEventPtr ProjectionGlobalBuildingEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionGlobalBuildingEvent(pBuildingInfo_, remainingTurns_, pBuiltInCity_));
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionGlobalBuildingEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionGlobalBuildingEvent event: " << " remaining turns = " << remainingTurns_;
    }

    int ProjectionGlobalBuildingEvent::getTurnsToEvent() const
    {
        return remainingTurns_;
    }

    bool ProjectionGlobalBuildingEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool ProjectionGlobalBuildingEvent::generateComparison() const
    {
        return true;
    }

    void ProjectionGlobalBuildingEvent::updateCityData(int nTurns)
    {
        if (complete_)
        {
            updateGlobalRequestData(pCityData_, pBuiltInCity_, pBuildingInfo_);
        }
    }

    IProjectionEventPtr ProjectionGlobalBuildingEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {
        if (remainingTurns_ <= nTurns)
        {
            complete_ = true;            
            return IProjectionEventPtr();
        }
        else 
        {
            remainingTurns_ -= nTurns;
            return shared_from_this();
        }
    }


    void ProjectionPopulationEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
    }

    IProjectionEventPtr ProjectionPopulationEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionPopulationEvent());
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionPopulationEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionPopulationEvent event - pop = " << pCityData_->getPopulation()
           << " food = " << pCityData_->getFood() << ", stored = " << pCityData_->getStoredFood()
           << " turns left = " << pCityData_->getTurnsToPopChange().second;
    }

    int ProjectionPopulationEvent::getTurnsToEvent() const
    {
        return pCityData_->getTurnsToPopChange().second;
    }
    
    bool ProjectionPopulationEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool ProjectionPopulationEvent::generateComparison() const
    {
        return false;
    }

    void ProjectionPopulationEvent::updateCityData(int nTurns)
    {
    }

    IProjectionEventPtr ProjectionPopulationEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {
        int turnsToPopChange = MAX_INT, popChange = 0;
        boost::tie(popChange, turnsToPopChange) = pCityData_->getTurnsToPopChange();

        int currentFood = 0, storedFood = 0;
        if (turnsToPopChange <= nTurns)
        {            
            boost::tie(currentFood, storedFood) = pCityData_->getAccumulatedFood(turnsToPopChange);
            pCityData_->setCurrentFood(currentFood);
            pCityData_->setStoredFood(storedFood);
            pCityData_->changePopulation(popChange);
        }
        else if (!pCityData_->isFoodProductionBuildItem())
        {
            boost::tie(currentFood, storedFood) = pCityData_->getAccumulatedFood(nTurns);
            pCityData_->setCurrentFood(currentFood);
            pCityData_->setStoredFood(storedFood);
        }

        return shared_from_this();
    }


    void ProjectionCultureLevelEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
    }

    IProjectionEventPtr ProjectionCultureLevelEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionCultureLevelEvent());
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionCultureLevelEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionCultureLevelEvent event - level = " << (int)pCityData_->getCultureHelper()->getCultureLevel()
            << " turns left = " << pCityData_->getCultureHelper()->getTurnsToNextLevel(*pCityData_);
    }

    int ProjectionCultureLevelEvent::getTurnsToEvent() const
    {
        return pCityData_->getCultureHelper()->getTurnsToNextLevel(*pCityData_);
    }
    
    bool ProjectionCultureLevelEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool ProjectionCultureLevelEvent::generateComparison() const
    {
        return false;
    }

    void ProjectionCultureLevelEvent::updateCityData(int nTurns)
    {
        pCityData_->getCultureHelper()->advanceTurns(*pCityData_, nTurns);
    }

    IProjectionEventPtr ProjectionCultureLevelEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {        
        return shared_from_this();
    }


    void ProjectionImprovementUpgradeEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
    }

    IProjectionEventPtr ProjectionImprovementUpgradeEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionImprovementUpgradeEvent());
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionImprovementUpgradeEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionImprovementUpgradeEvent event - next upgrade in " << pCityData_->getNextImprovementUpgradeTime() << " turns ";
    }

    int ProjectionImprovementUpgradeEvent::getTurnsToEvent() const
    {
        return pCityData_->getNextImprovementUpgradeTime();
    }
    
    bool ProjectionImprovementUpgradeEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool ProjectionImprovementUpgradeEvent::generateComparison() const
    {
        return false;
    }

    void ProjectionImprovementUpgradeEvent::updateCityData(int nTurns)
    {
        pCityData_->doImprovementUpgrades(nTurns);
    }

    IProjectionEventPtr ProjectionImprovementUpgradeEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {        
        return shared_from_this();
    }


    ProjectionChangeCivicEvent::ProjectionChangeCivicEvent(CivicOptionTypes civicOptionType, CivicTypes civicType, int turnsToChange)
        : civicOptionType_(civicOptionType), civicType_(civicType), turnsToChange_(turnsToChange)
    {
    }

    void ProjectionChangeCivicEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
    }

    IProjectionEventPtr ProjectionChangeCivicEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionChangeCivicEvent(civicOptionType_, civicType_, turnsToChange_));
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionChangeCivicEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionChangeCivicEvent event: " << " remaining turns = " << turnsToChange_ << " for civic: "
           << gGlobals.getCivicInfo(civicType_).getType()
           << " compare civic = "
           << gGlobals.getCivicInfo(pCityData_->getCivHelper()->currentCivic(civicOptionType_)).getType();
    }

    int ProjectionChangeCivicEvent::getTurnsToEvent() const
    {
        return turnsToChange_;
    }

    bool ProjectionChangeCivicEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool ProjectionChangeCivicEvent::generateComparison() const
    {
        return true;
    }

    void ProjectionChangeCivicEvent::updateCityData(int nTurns)
    {
        if (turnsToChange_ <= nTurns)
        {
            boost::shared_ptr<PlayerAnalysis> pAnalysis = gGlobals.getGame().getAltAI()->getPlayer(pCityData_->getCity()->getOwner())->getAnalysis();            
            updateRequestData(pCityData_->getCity(), *pCityData_, pAnalysis, civicType_);
        }
    }

    IProjectionEventPtr ProjectionChangeCivicEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {
        if (turnsToChange_ <= nTurns)
        {            
            return IProjectionEventPtr();
        }
        else 
        {
            turnsToChange_ -= nTurns;
            return shared_from_this();
        }
    }


    void ProjectionHappyTimerEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
    }

    IProjectionEventPtr ProjectionHappyTimerEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionHappyTimerEvent());
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionHappyTimerEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionHappyTimerEvent event - "
            << " turns left = " << getTurnsToEvent();
    }

    int ProjectionHappyTimerEvent::getTurnsToEvent() const
    {
        const int hurryAngerTimer = pCityData_->getHurryHelper()->getAngryTimer();
        if (hurryAngerTimer > 0)
        {
            const int flatHurryAngerLength = pCityData_->getHurryHelper()->getFlatHurryAngryLength();
            return hurryAngerTimer / flatHurryAngerLength + hurryAngerTimer % flatHurryAngerLength;
        }
        else
        {
            return MAX_INT;
        }
    }
    
    bool ProjectionHappyTimerEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool ProjectionHappyTimerEvent::generateComparison() const
    {
        return false;
    }

    void ProjectionHappyTimerEvent::updateCityData(int nTurns)
    {
        pCityData_->getHurryHelper()->advanceTurns(nTurns);
        pCityData_->changeWorkingPopulation();
    }

    IProjectionEventPtr ProjectionHappyTimerEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {        
        return shared_from_this();
    }

    void ProjectionHurryEvent::init(const CityDataPtr& pCityData)
    {
        pCityData_ = pCityData;
    }

    IProjectionEventPtr ProjectionHurryEvent::clone(const CityDataPtr& pCityData) const
    {
        IProjectionEventPtr pCopy(new ProjectionHurryEvent(hurryData_));
        pCopy->init(pCityData);
        return pCopy;
    }

    void ProjectionHurryEvent::debug(std::ostream& os) const
    {
        os << "\n\tProjectionHurryEvent event - "
            << " turns left = " << getTurnsToEvent();
    }

    int ProjectionHurryEvent::getTurnsToEvent() const
    {
        if (!havePopulation_())
        {
            return pCityData_->getTurnsToPopChange().second;
        }
        else  // have enough pop to hurry something - is timer expired?
        {            
            if (!angryTimerExpired_())
            {
                const int hurryAngerTimer = pCityData_->getHurryHelper()->getAngryTimer();
                const int flatHurryAngerLength = pCityData_->getHurryHelper()->getFlatHurryAngryLength();
                return hurryAngerTimer / flatHurryAngerLength + hurryAngerTimer % flatHurryAngerLength;
            }
            else
            {
                return 0;
            }
        }
    }
    
    bool ProjectionHurryEvent::targetsCity(IDInfo city) const
    {
        return pCityData_->getCity()->getIDInfo() == city;
    }

    bool ProjectionHurryEvent::generateComparison() const
    {
        return false;
    }

    void ProjectionHurryEvent::updateCityData(int nTurns)
    {
        if (havePopulation_() && angryTimerExpired_())
        {
            if (getTurnsToEvent() <= nTurns)
            {            
                // create dummy build to hurry (todo - pick based on possible small unit builds)
                pCityData_->setRequiredProduction(6000);
                pCityData_->setAccumulatedProduction(100);

                bool canHurry;
                boost::tie(canHurry, hurryData_) = pCityData_->getHurryHelper()->canHurry(*pCityData_, hurryData_.hurryType, true);
                if (canHurry)
                {
                    pCityData_->hurry(hurryData_);
                }
            }
        }
    }

    IProjectionEventPtr ProjectionHurryEvent::updateEvent(int nTurns, ProjectionLadder& ladder)
    {        
        return shared_from_this();
    }

    bool ProjectionHurryEvent::havePopulation_() const
    {
        return pCityData_->getPopulation() / 2 >= hurryData_.hurryPopulation;
    }

    bool ProjectionHurryEvent::angryTimerExpired_() const
    {
        return pCityData_->getHurryHelper()->getAngryTimer() == 0;
    }

    ProjectionLadder getProjectedOutput(const Player& player, const CityDataPtr& pCityData, int nTurns, std::vector<IProjectionEventPtr>& events, 
        const ConstructItem& constructItem, const std::string& sourceFunc, bool doComparison, bool debug)
    {
        events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent()));
        events.push_back(IProjectionEventPtr(new ProjectionCultureLevelEvent()));
        events.push_back(IProjectionEventPtr(new ProjectionImprovementUpgradeEvent()));
        events.push_back(IProjectionEventPtr(new ProjectionHappyTimerEvent()));

#ifdef ALTAI_DEBUG
//        if (debug)
//        {            
//            os << "\ngetProjectedOutput - caller = " << sourceFunc << " turns = " << nTurns << " events count = " << events.size()
//                << " item = ";
//            constructItem.debug(os);
//            os << ", compare = " << doComparison;
//        }
#endif
        for (size_t i = 0, count = events.size(); i < count; ++i)
        {
            events[i]->init(pCityData);
        }

        ProjectionLadder ladder;
        updateProjections(player, pCityData, nTurns, events, constructItem, doComparison, debug, ladder);

#ifdef ALTAI_DEBUG
        // don't bother to log if we didn't finish the item - unless we have no item to start with
        // - to avoid cluttering up the log with small cities not building expensive items
        if (debug && (constructItem.isEmpty() || !ladder.buildings.empty() || !ladder.units.empty()))
        {
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData->getOwner()))->getStream();
            os << "\n\ngetProjectedOutput: caller = " << sourceFunc << " city: " << narrow(pCityData->getCity()->getName());
            constructItem.debug(os);
            if (!ladder.buildings.empty())
            {
                os << " " << ladder.buildings[0].first << " turns ";
            }
            else if (!ladder.units.empty())
            {
                os << " " << ladder.units[0].turns << " turns ";
            }

            /*if (doComparison && !ladder.comparisons.empty())
            {
                os << " delta = " << ladder.getOutput() - ladder.comparisons[0].getOutput();            
            }
            else if (doComparison && !ladder.buildings.empty() && ladder.comparisons.empty() && ladder.buildings[0].first < nTurns)
            {
                os << " built building but missing comparison? ";
            }*/
        }
#endif
        return ladder;
    }
}