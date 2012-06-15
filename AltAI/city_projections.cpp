#include "./city_projections.h"
#include "./building_info_visitors.h"
#include "./player.h"
#include "./city.h"
#include "./maintenance_helper.h"
#include "./building_helper.h"
#include "./modifiers_helper.h"
#include "./buildings_info.h"
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
    }

    ProjectionBuildingEvent::ProjectionBuildingEvent(const CityDataPtr& pCityData, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
        : pCity_(pCityData->getCity()), pCityData_(pCityData), pBuildingInfo_(pBuildingInfo)
    {
        requiredProduction_ = 100 * pCity_->getProductionNeeded(pBuildingInfo_->getBuildingType());
        accumulatedTurns_ = 0;
    }
    
    void ProjectionBuildingEvent::debug(std::ostream& os) const
    {
        os << "\nProjectionBuildingEvent event: " << " reqd prod = " << requiredProduction_ << " accumulated turns = " << accumulatedTurns_;
    }

    int ProjectionBuildingEvent::getTurnsToEvent() const
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

    IProjectionEventPtr ProjectionBuildingEvent::update(int nTurns, ProjectionLadder& ladder)
    {
        int turnsToComplete = getTurnsToEvent();

        if (turnsToComplete <= nTurns)
        {
            requiredProduction_ = 0;
            pCityData_->getBuildingsHelper()->changeNumRealBuildings(pBuildingInfo_->getBuildingType());
            updateRequestData(*pCityData_, pBuildingInfo_);
            ladder.buildings.push_back(std::make_pair(turnsToComplete + accumulatedTurns_, pBuildingInfo_->getBuildingType()));
            return IProjectionEventPtr();  // we're done
        }
        else 
        {
            accumulatedTurns_ += nTurns;
            if (turnsToComplete < MAX_INT)
            {   
                int production = pCityData_->getCurrentProduction();

//#ifdef ALTAI_DEBUG                
//                std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData_->getOwner()))->getStream();
//                os << "\nBuilding: " << gGlobals.getBuildingInfo(pBuildingInfo_->getBuildingType()).getType() << " adding production: " << production;
//
//                YieldModifier yieldModifier = pCityData_->getModifiersHelper()->getTotalYieldModifier(*pCityData_);
//                int modifier = pCityData_->getBuildingsHelper()->getProductionModifier(*pCityData_, pBuildingInfo_->getBuildingType());
//                os << " modifier = " << yieldModifier[OUTPUT_PRODUCTION] + modifier;
//#endif
                requiredProduction_ -= nTurns * production;
            }
            return shared_from_this();
        }
    }

    ProjectionUnitEvent::ProjectionUnitEvent(const CityDataPtr& pCityData, const boost::shared_ptr<UnitInfo>& pUnitInfo)
        : pCity_(pCityData->getCity()), pCityData_(pCityData), pUnitInfo_(pUnitInfo)
    {
        isFoodProduction_ = gGlobals.getUnitInfo(pUnitInfo->getUnitType()).isFoodProduction();
        requiredProduction_ = 100 * pCity_->getProductionNeeded(pUnitInfo_->getUnitType());
        accumulatedTurns_ = 0;
    }
    
    void ProjectionUnitEvent::debug(std::ostream& os) const
    {
        os << "\nProjectionUnitEvent event: " << " reqd prod = " << requiredProduction_ << " accumulated turns = " << accumulatedTurns_;
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

    IProjectionEventPtr ProjectionUnitEvent::update(int nTurns, ProjectionLadder& ladder)
    {
        int turnsToComplete = getTurnsToEvent();

        if (turnsToComplete <= nTurns)
        {
            requiredProduction_ = 0;
            ladder.units.push_back(std::make_pair(turnsToComplete + accumulatedTurns_, pUnitInfo_->getUnitType()));
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

    ProjectionGlobalBuildingEvent::ProjectionGlobalBuildingEvent(const CityDataPtr& pCityData, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int turnBuilt, const CvCity* pBuiltInCity)
        : pCityData_(pCityData), pBuildingInfo_(pBuildingInfo), remainingTurns_(turnBuilt), pCity_(pCityData->getCity()), pBuiltInCity_(pBuiltInCity)
    {
    }

    void ProjectionGlobalBuildingEvent::debug(std::ostream& os) const
    {
        os << "\nProjectionGlobalBuildingEvent event: " << " remaining turns = " << remainingTurns_;
    }

    int ProjectionGlobalBuildingEvent::getTurnsToEvent() const
    {
        return remainingTurns_;
    }

    IProjectionEventPtr ProjectionGlobalBuildingEvent::update(int nTurns, ProjectionLadder& ladder)
    {
        if (remainingTurns_ <= nTurns)
        {
            updateGlobalRequestData(pCityData_, pBuiltInCity_, pBuildingInfo_);
            return IProjectionEventPtr();
        }
        else 
        {
            remainingTurns_ -= nTurns;
            return shared_from_this();
        }
    }

    ProjectionPopulationEvent::ProjectionPopulationEvent(const CityDataPtr& pCityData)
        : pCityData_(pCityData)
    {
    }

    void ProjectionPopulationEvent::debug(std::ostream& os) const
    {
        os << "\nProjectionPopulationEvent event - pop = " << pCityData_->getPopulation()
           << " food = " << pCityData_->getFood() << ", stored = " << pCityData_->getStoredFood();
    }

    int ProjectionPopulationEvent::getTurnsToEvent() const
    {
        return pCityData_->getTurnsToPopChange().second;
    }
    
    IProjectionEventPtr ProjectionPopulationEvent::update(int nTurns, ProjectionLadder& ladder)
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
        else
        {
            boost::tie(currentFood, storedFood) = pCityData_->getAccumulatedFood(nTurns);
            pCityData_->setCurrentFood(currentFood);
            pCityData_->setStoredFood(storedFood);
        }

        return shared_from_this();
    }

    ProjectionLadder getProjectedOutput(const Player& player, const CityDataPtr& pCityData, int nTurns, std::vector<IProjectionEventPtr>& events)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData->getOwner()))->getStream();
//#endif
        CityOptimiser cityOptimiser(pCityData);

        std::vector<OutputTypes> outputTypes = boost::assign::list_of(OUTPUT_PRODUCTION)(OUTPUT_RESEARCH);
        TotalOutputPriority outputPriorities = makeTotalOutputPriorities(outputTypes);
        TotalOutputWeights outputWeights = makeOutputW(2, 2, 2, 2, 1, 1);

        ProjectionLadder ladder;

        while (nTurns > 0)
        {
            cityOptimiser.optimise<MixedWeightedTotalOutputOrderFunctor>(outputPriorities, outputWeights, cityOptimiser.getGrowthType());

            std::sort(events.begin(), events.end(), EventTimeOrderF());

            int turnsToFirstEvent = MAX_INT, population = pCityData->getPopulation(), maintenance = pCityData->getMaintenanceHelper()->getMaintenance();
            TotalOutput output = pCityData->getOutput();
            output[OUTPUT_FOOD] -= pCityData->getLostFood();

            std::vector<IProjectionEventPtr> newEvents;
            for (size_t i = 0, count = events.size(); i < count; ++i)
            {
//#ifdef ALTAI_DEBUG
//                os << "\nTurns to first event: " << events[i]->getTurnsToEvent() << " (nTurns = " << nTurns << ")";
//                events[i]->debug(os);
//#endif
                if (events[i]->getTurnsToEvent() <= turnsToFirstEvent)
                {
                    turnsToFirstEvent = events[i]->getTurnsToEvent();                   
                }
                IProjectionEventPtr pNewEvent = events[i]->update(std::min<int>(nTurns, turnsToFirstEvent), ladder);
                if (pNewEvent)
                {
                    newEvents.push_back(pNewEvent);
                }
            }

            ladder.entries.push_back(ProjectionLadder::Entry(population, std::min<int>(nTurns, turnsToFirstEvent), output, maintenance, pCityData->getGPP()));

            if (turnsToFirstEvent <= nTurns)
            {
                // opt plots if completed an event
                cityOptimiser.optimise<MixedWeightedTotalOutputOrderFunctor>(outputPriorities, outputWeights, cityOptimiser.getGrowthType());
            }
            nTurns -= turnsToFirstEvent;

            events = newEvents;
        }

        return ladder;
    }
}