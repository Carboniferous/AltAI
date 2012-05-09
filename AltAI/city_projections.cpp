#include "./city_projections.h"
#include "./building_info_visitors.h"
#include "./player.h"
#include "./city.h"
#include "./maintenance_helper.h"
#include "./building_helper.h"
#include "./buildings_info.h"
#include "./civ_log.h"

namespace AltAI
{
    void ProjectionLadder::debug(std::ostream& os) const
    {
        int turn = 0;
        TotalOutput cumulativeOutput, lastOutput;
        int cumulativeCost = 0;

        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {
            os << "\n\tTurns = " << entries[i].turns << ", pop = " << entries[i].pop << ", " << entries[i].output << ", " << entries[i].cost;
        }

        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {         
            turn += entries[i].turns;
            cumulativeOutput += entries[i].output * entries[i].turns;
            cumulativeCost += entries[i].cost * entries[i].turns;

            os << "\n\tPop = " << entries[i].pop << " turn = " << turn << " output = " << cumulativeOutput << " cost = " << cumulativeCost;
            if (i > 0)
            {
                os << ", delta = " << entries[i].output - lastOutput;
                lastOutput = entries[i].output;
            }
        }

        for (size_t i = 0, count = buildings.size(); i < count; ++i)
        {
            os << "\n\tBuilding: " << gGlobals.getBuildingInfo(buildings[i].second).getType() << " built: " << buildings[i].first;
        }
    }

    TotalOutput ProjectionLadder::getOutput() const
    {
        TotalOutput cumulativeOutput;
        int cumulativeCost = 0;
        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {            
            cumulativeOutput += entries[i].output * entries[i].turns;
            cumulativeCost += entries[i].cost * entries[i].turns;
        }
        cumulativeOutput[OUTPUT_GOLD] -= cumulativeCost;
        return cumulativeOutput;
    }

    TotalOutput ProjectionLadder::getOutputAfter(int turn) const
    {
        bool includeAll = false;
        int currentTurn = 0;
        TotalOutput cumulativeOutput;
        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {   
            currentTurn += entries[i].turns;
            if (entries[i].turns > turn)
            {
                if (includeAll)
                {
                    cumulativeOutput += entries[i].turns * entries[i].output;
                }
                else
                {
                    cumulativeOutput += (currentTurn - turn) * entries[i].output;
                    includeAll = true;
                }
            }            
        }
        return cumulativeOutput;
    }

    ProjectionLadder getProjectedOutput(const Player& player, const CityDataPtr& pCityData, int nTurns)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData->getOwner()))->getStream();
#endif
        CityOptimiser cityOptimiser(pCityData);

        std::vector<OutputTypes> outputTypes = boost::assign::list_of(OUTPUT_PRODUCTION)(OUTPUT_RESEARCH);
        TotalOutputPriority outputPriorities = makeTotalOutputPriorities(outputTypes);
        TotalOutputWeights outputWeights = makeOutputW(2, 2, 2, 2, 1, 1);

        ProjectionLadder ladder;
        TotalOutput lastOutput;

//#ifdef ALTAI_DEBUG
//        os << "\nProjected output for city: " << narrow(pCityData->getCity()->getName());
//#endif

        while (nTurns > 0)
        {
            cityOptimiser.optimise<MixedWeightedTotalOutputOrderFunctor>(outputPriorities, outputWeights, cityOptimiser.getGrowthType());
            TotalOutput thisOutput = pCityData->getOutput();

//#ifdef ALTAI_DEBUG
//            os << "\n final output = " << thisOutput << " ";
//            pCityData->debugBasicData(os);
//            cityOptimiser.debug(os, true);
//#endif
            int turnsToPopChange = MAX_INT, popChange = 0;
            boost::tie(popChange, turnsToPopChange) = pCityData->getTurnsToPopChange();
               
            ladder.entries.push_back(
                ProjectionLadder::Entry(pCityData->getPopulation(), turnsToPopChange > nTurns ? nTurns : turnsToPopChange, thisOutput,
                    pCityData->getMaintenanceHelper()->getMaintenance()));
            lastOutput = thisOutput;

            if (turnsToPopChange < MAX_INT)
            {
                int currentFood = 0, storedFood = 0;
                boost::tie(currentFood, storedFood) = pCityData->getAccumulatedFood(turnsToPopChange);
                pCityData->setCurrentFood(currentFood);
                pCityData->setStoredFood(storedFood);
                pCityData->changePopulation(popChange);
//#ifdef ALTAI_DEBUG
//                os << "\nSetting current food = " << currentFood << ", stored food = " << storedFood << ", pop = " << popChange
//                   << ", growth threshold = " << pCityData->getGrowthThreshold() << " turn = " << nTurns << " food = " << pCityData->getCurrentFood();
//#endif

                nTurns -= turnsToPopChange;
            }
            else
            {
                nTurns = 0;
            }
        }

//#ifdef ALTAI_DEBUG
//        ladder.debug(os);
//#endif
        return ladder;
    }

    ProjectionLadder getProjectedOutput(const Player& player, const CityDataPtr& pCityData, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int nTurns)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData->getOwner()))->getStream();
#endif
        CityOptimiser cityOptimiser(pCityData);

        std::vector<OutputTypes> outputTypes = boost::assign::list_of(OUTPUT_PRODUCTION)(OUTPUT_RESEARCH);
        TotalOutputPriority outputPriorities = makeTotalOutputPriorities(outputTypes);
        TotalOutputWeights outputWeights = makeOutputW(2, 2, 2, 2, 1, 1);

        ProjectionLadder ladder;
        TotalOutput lastOutput;

        int requiredProduction = 100 * pCityData->getCity()->getProductionNeeded(pBuildingInfo->getBuildingType());
        int turnBuilt = MAX_INT;
//#ifdef ALTAI_DEBUG
//        os << "\nProjected output for city: " << narrow(pCityData->getCity()->getName()) << " building: "
//           << gGlobals.getBuildingInfo(pBuildingInfo->getBuildingType()).getType() << " reqd prod = " << requiredProduction;
//#endif
        const int totalTurns = nTurns;
        while (nTurns > 0)
        {
            cityOptimiser.optimise<MixedWeightedTotalOutputOrderFunctor>(outputPriorities, outputWeights, cityOptimiser.getGrowthType(), false);
            TotalOutput thisOutput = pCityData->getOutput();            

//#ifdef ALTAI_DEBUG
//            os << "\n final output = " << thisOutput << " ";
//            pCityData->debugBasicData(os);
//            cityOptimiser.debug(os, true);
//#endif
            int turnsToPopChange = MAX_INT, popChange = 0;
            boost::tie(popChange, turnsToPopChange) = pCityData->getTurnsToPopChange();

            if (requiredProduction > 0)
            {
                int turnsToComplete = MAX_INT;

                if (thisOutput[OUTPUT_PRODUCTION] > 0)
                {
                    const int productionRate = requiredProduction / thisOutput[OUTPUT_PRODUCTION];
                    const int productionDelta = requiredProduction % thisOutput[OUTPUT_PRODUCTION];
                
                    turnsToComplete = productionRate + (productionDelta ? 1 : 0);
                }

                if (turnsToComplete <= turnsToPopChange)
                {
                    ladder.entries.push_back(
                        ProjectionLadder::Entry(
                            pCityData->getPopulation(), turnsToComplete > nTurns ? nTurns : turnsToComplete, thisOutput, pCityData->getMaintenanceHelper()->getMaintenance()));
                    if (turnsToComplete <= nTurns)
                    {
                        ladder.buildings.push_back(std::make_pair(totalTurns - nTurns + turnsToComplete, pBuildingInfo->getBuildingType()));
                    }
                    lastOutput = thisOutput;

                    int currentFood = 0, storedFood = 0;
                    boost::tie(currentFood, storedFood) = pCityData->getAccumulatedFood(turnsToComplete);
                    pCityData->setCurrentFood(currentFood);
                    pCityData->setStoredFood(storedFood);
//#ifdef ALTAI_DEBUG
//                    os << "\nSetting current food = " << currentFood << ", stored food = " << storedFood
//                       << ", growth threshold = " << pCityData->getGrowthThreshold() << " turn = " << nTurns << " food = " << pCityData->getCurrentFood();
//#endif

                    requiredProduction = 0;
                    pCityData->getBuildingsHelper()->changeNumRealBuildings(pBuildingInfo->getBuildingType());
                    updateRequestData(*pCityData, pBuildingInfo);
                    cityOptimiser.optimise<MixedWeightedTotalOutputOrderFunctor>(outputPriorities, outputWeights, cityOptimiser.getGrowthType());
                    nTurns -= turnsToComplete;
                    turnBuilt = nTurns;
                    continue;
                }
                else if (turnsToComplete < MAX_INT)
                {
                    requiredProduction -= turnsToPopChange * thisOutput[OUTPUT_PRODUCTION];
                }
            }
                
            ladder.entries.push_back(
                ProjectionLadder::Entry(
                    pCityData->getPopulation(), turnsToPopChange > nTurns ? nTurns : turnsToPopChange, thisOutput, pCityData->getMaintenanceHelper()->getMaintenance()));
            lastOutput = thisOutput;

            if (turnsToPopChange < MAX_INT)
            {
                int currentFood = 0, storedFood = 0;
                boost::tie(currentFood, storedFood) = pCityData->getAccumulatedFood(turnsToPopChange);
                pCityData->setCurrentFood(currentFood);
                pCityData->setStoredFood(storedFood);
                pCityData->changePopulation(popChange);
//#ifdef ALTAI_DEBUG
//                os << "\nSetting current food = " << currentFood << ", stored food = " << storedFood << ", pop = " << popChange
//                   << ", growth threshold = " << pCityData->getGrowthThreshold() << " turn = " << nTurns << " food = " << pCityData->getCurrentFood();
//#endif
                

                nTurns -= turnsToPopChange;
            }
            else
            {
                nTurns = 0;
            }
        }

//#ifdef ALTAI_DEBUG
//        ladder.debug(os);
//#endif
        return ladder;
    }
}