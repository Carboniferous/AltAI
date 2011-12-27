#include "./city_simulator.h"
#include "./building_info_visitors.h"
#include "./plot_info_visitors.h"
#include "./hurry_helper.h"
#include "./helper_fns.h"
#include "./civ_log.h"
#include "./game.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./city.h"

#include "boost/lexical_cast.hpp"

namespace AltAI
{
    namespace
    {
        const GreatPersonOutputMap& addGppOutput(GreatPersonOutputMap& cumulativeMap, const GreatPersonOutputMap& turnOutput)
        {
            for (GreatPersonOutputMap::const_iterator ci(turnOutput.begin()), ciEnd(turnOutput.end()); ci != ciEnd; ++ci)
            {
                cumulativeMap[ci->first] += ci->second;
            }
            return cumulativeMap;
        }
    }

    CitySimulator::CitySimulator(const CvCity* pCity)
        : pCity_(pCity)
    {
    }

    BuildingSimulationResults CitySimulator::evaluateBasicBuildings(int nTurns)
    {
        BuildingSimulationResults results;

        // simulate baseline
        CitySimulation simulation(pCity_, boost::shared_ptr<CityData>(new CityData(pCity_)));
        results.noBuildingBaseline = simulation.simulateAsIs(nTurns, OUTPUT_PRODUCTION);

        const boost::shared_ptr<PlayerAnalysis>& pPlayerAnalysis = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getAnalysis();

        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            BuildingTypes buildingType = (BuildingTypes)gGlobals.getCivilizationInfo(pCity_->getCivilizationType()).getCivilizationBuildings(i);

            if ((pCity_->canConstruct(buildingType) || pCity_->getFirstBuildingOrder(buildingType) != -1) && !isLimitedWonderClass(getBuildingClass(buildingType)))
            {
                boost::shared_ptr<CityData> pCityData = boost::shared_ptr<CityData>(new CityData(pCity_));
                if (buildingHasEconomicImpact(*pCityData, pPlayerAnalysis->getBuildingInfo(buildingType)))
                {
                    CitySimulation simulation(pCity_, pCityData, ConstructItem(buildingType));
                    simulation.simulate(results, nTurns);
                }
                else
                {
#ifdef ALTAI_DEBUG
                    CityLog::getLog(pCity_)->getStream() << "\n" << gGlobals.getBuildingInfo((BuildingTypes)buildingType).getType() << " has no current ecomonic value\n";
#endif
                }
            }
        }

        return results;
    }

    void CitySimulator::evaluateBuilding(BuildingTypes buildingType, int turns, BuildingSimulationResults& results, bool doBaseLine)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
#endif
        // simulate baseline
        CitySimulation simulation(pCity_, boost::shared_ptr<CityData>(new CityData(pCity_)));

        if (doBaseLine)
        {
#ifdef ALTAI_DEBUG
            os << "\nDoing baseline simulation ";
#endif
            results.noBuildingBaseline = simulation.simulateAsIs(turns, OUTPUT_PRODUCTION);
        }

#ifdef ALTAI_DEBUG
        os << "\nCity: " << narrow(pCity_->getName()) << " can construct = " << pCity_->canConstruct(buildingType)
           << " first building order = " << pCity_->getFirstBuildingOrder(buildingType);
#endif
        if (pCity_->canConstruct(buildingType) || pCity_->getFirstBuildingOrder(buildingType) != -1)
        {
#ifdef ALTAI_DEBUG
            os << "\nDoing building: " << gGlobals.getBuildingInfo(buildingType).getType() << " simulation";
#endif
            boost::shared_ptr<CityData> pCityData = boost::shared_ptr<CityData>(new CityData(pCity_));
            CitySimulation simulation(pCity_, pCityData, ConstructItem(buildingType));
            simulation.simulate(results, turns);
        }
    }

    BuildingSimulationResults CitySimulator::evaluateHurryBuilding(BuildingTypes buildingType, int nTurns)
    {
        BuildingSimulationResults results;

        // simulate baseline
        CitySimulation simulation(pCity_, boost::shared_ptr<CityData>(new CityData(pCity_)));
        results.noBuildingBaseline = simulation.simulateAsIs(nTurns, OUTPUT_PRODUCTION);

        for (int i = 0, count = gGlobals.getNumHurryInfos(); i < count; ++i)
        {
            CitySimulation simulation(pCity_, boost::shared_ptr<CityData>(new CityData(pCity_)), ConstructItem(buildingType));
            simulation.simulate(results, nTurns, (HurryTypes)i);
        }

        return results;
    }

    std::pair<BuildingTypes, TotalOutput> CitySimulator::getBestBuilding(TotalOutputWeights outputWeights, const BuildingSimulationResults& simulationResults) const
    {
        BuildingTypes bestBuilding = NO_BUILDING;
        TotalOutput bestOutput;
        TotalOutputValueFunctor valueF(outputWeights);

        for (size_t i = 0, count = simulationResults.buildingResults.size(); i < count; ++i)
        {
            TotalOutput thisOutput = SimulationOutput::getDelta(simulationResults.buildingResults[i].normalBuild, simulationResults.buildingResults[i].baseline);

            if (valueF(thisOutput, bestOutput))
            {
                bestBuilding = simulationResults.buildingResults[i].buildingType;
                bestOutput = thisOutput;
            }
        }
        return std::make_pair(bestBuilding, bestOutput);
    }

    std::map<BuildingTypes, int> CitySimulator::getBuildingValues(TotalOutputWeights outputWeights, const BuildingSimulationResults& simulationResults) const
    {
        std::map<BuildingTypes, int> results;
        TotalOutputValueFunctor valueF(outputWeights);

        for (size_t i = 0, count = simulationResults.buildingResults.size(); i < count; ++i)
        {
            TotalOutput thisOutput = SimulationOutput::getDelta(simulationResults.buildingResults[i].normalBuild, simulationResults.buildingResults[i].baseline);
            results.insert(std::make_pair(simulationResults.buildingResults[i].buildingType, valueF(thisOutput)));
        }
        return results;
    }

    int BuildingSimulationResults::getNormalBuildTime(BuildingTypes buildingType) const
    {
        for (size_t i = 0, count = buildingResults.size(); i < count; ++i)
        {
            if (buildingType == buildingResults[i].buildingType)
            {
                const size_t baselineSize = buildingResults[i].baseline.cumulativeOutput.size();
                const size_t buildSize = buildingResults[i].normalBuild.cumulativeOutput.size();

                if (baselineSize == 0 || buildSize == 0 || buildSize > baselineSize)
                {
                    return std::numeric_limits<int>::max();
                }
                else
                {
                    return baselineSize - buildSize;
                }
            }
        }

        return std::numeric_limits<int>::max();
    }

    std::vector<boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, TotalOutput> > 
        CitySimulator::getBestImprovements(TotalOutputWeights outputWeights, const PlotImprovementSimulationResults& simulationResults) const
    {
        typedef std::multimap<int, boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, TotalOutput>, std::greater<int> > BestImprovementsMap;
        BestImprovementsMap bestImprovements;
        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, TotalOutput> bestImprovment = boost::make_tuple(XYCoords(), NO_FEATURE, NO_IMPROVEMENT, TotalOutput());
        TotalOutputValueFunctor valueF(outputWeights);
        TotalOutput baseline;

        for (size_t i = 0, count = simulationResults.size(); i < count; ++i)
        {
            if (i == 0)
            {
                baseline = *boost::get<2>(simulationResults[0].second[0]).cumulativeOutput.rbegin();

				boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, TotalOutput> noImprovement(
					boost::make_tuple(simulationResults[0].first, NO_FEATURE, NO_IMPROVEMENT, baseline));
				bestImprovements.insert(std::make_pair(valueF(baseline), noImprovement));
            }
            else
            {
                for (size_t j = 0, count = simulationResults[i].second.size(); j < count; ++j)
                {
                    TotalOutput thisOutput = *boost::get<2>(simulationResults[i].second[j]).cumulativeOutput.rbegin() - baseline;
                    
                    if (!isEmpty(thisOutput))
                    {
                        bool notAllNegative = false;
                        for (int k = 0; k < NUM_OUTPUT_TYPES; ++k)
                        {
                            if (thisOutput[k] > 0)
                            {
                                notAllNegative = true;
                                break;
                            }
                        }

                        if (notAllNegative)
                        {
                            int thisOutputValue = valueF(thisOutput);

                            boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, TotalOutput> bestImprovement(
								boost::make_tuple(simulationResults[i].first, boost::get<0>(simulationResults[i].second[j]), boost::get<1>(simulationResults[i].second[j]), thisOutput));

                            bestImprovements.insert(std::make_pair(thisOutputValue, bestImprovement));
                        }
                    }
                }
            }
        }

        std::vector<boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, TotalOutput> > sortedImprovements;
        for (BestImprovementsMap::const_iterator ci(bestImprovements.begin()), ciEnd(bestImprovements.end()); ci != ciEnd; ++ci)
        {
            sortedImprovements.push_back(ci->second);
        }
        return sortedImprovements;
    }

    namespace
    {
        struct PlotFinder
        {
            explicit PlotFinder(XYCoords coords_) : coords(coords_) {}

            bool operator() (const std::pair<XYCoords, std::vector<std::pair<FeatureTypes, ImprovementTypes> > >& possibleImprovement) const
            {
                return possibleImprovement.first == coords;
            }

            XYCoords coords;
        };
    }

    // todo - eliminate similar improvements (e.g. only do cottage on plains once - if both plains have same plotinfokey)
    PlotImprovementSimulationResults CitySimulator::evaluateAllImprovements(int nTurns, bool ignoreExisting)
    {
        boost::shared_ptr<CityData> pCityData(new CityData(pCity_));
        
        PlotsAndImprovements possibleImprovements = getPossibleImprovements(*pCityData, ignoreExisting);

#ifdef ALTAI_DEBUG
        CityLog::getLog(pCity_)->getStream() << "\nFound: " << possibleImprovements.size() << " possible improvements";
#endif

        return evaluateImprovements(possibleImprovements, *pCityData, nTurns, ignoreExisting);
    }

    PlotImprovementSimulationResults CitySimulator::evaluateImprovements(const PlotsAndImprovements& improvements, CityData& cityData, int nTurns, bool ignoreExisting)
    {
        TotalOutputWeights outputWeights;
        PlotImprovementSimulationResults outputs;

        if (!improvements.empty())
        {
            // simulate baseline
            CitySimulation simulation(pCity_, boost::shared_ptr<CityData>(new CityData(pCity_)));

            PlotImprovementSimulationResult plotResults;
            SimulationOutput simOutput = simulation.simulateAsIs(nTurns);
            plotResults.push_back(boost::make_tuple(NO_FEATURE, NO_IMPROVEMENT, simOutput));
            outputs.push_back(std::make_pair(cityData.cityPlotOutput.coords, plotResults));

            outputWeights = simulation.getCityOptimiser()->getMaxOutputWeights();

#ifdef ALTAI_DEBUG
            //CityLog::getLog(pCity_)->getStream() << "\n(Baseline)\n";
            //simOutput.debugResults(CityLog::getLog(pCity_)->getStream());
#endif
        }

        for (PlotDataListIter iter(cityData.plotOutputs.begin()), endIter(cityData.plotOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->controlled && (!ignoreExisting || iter->improvementType == NO_IMPROVEMENT))
            {
                PlotsAndImprovements::const_iterator improvementsIter = std::find_if(improvements.begin(), improvements.end(), PlotFinder(iter->coords));
                if (improvementsIter != improvements.end())
                {
                    PlotImprovementSimulationResult plotResults;
                    for (size_t j = 0, count = improvementsIter->second.size(); j < count; ++j)
                    {
                        PlotData savedPlotData = *iter;

                        updateCityOutputData(cityData, *iter, improvementsIter->second[j].first, iter->routeType, improvementsIter->second[j].second);

                        boost::shared_ptr<CityData> pModifiedCityData(new CityData(cityData));
                        CitySimulation simulation(pCity_, pModifiedCityData);

#ifdef ALTAI_DEBUG
                        // debug
                        {
                            CityLog::getLog(pCity_)->getStream() << " after = " << pModifiedCityData->findPlot(iter->coords).actualOutput;
                        }
#endif
                        SimulationOutput simOutput = simulation.simulateAsIs(nTurns);
                        plotResults.push_back(boost::make_tuple(improvementsIter->second[j].first, improvementsIter->second[j].second, simOutput));

#ifdef ALTAI_DEBUG
                        CityLog::getLog(pCity_)->getStream() << "\nPlot: " << iter->coords << " imp = " << gGlobals.getImprovementInfo(improvementsIter->second[j].second).getType() << "\n";
                        simOutput.debugResults(CityLog::getLog(pCity_)->getStream());
#endif

                        // restore plot - todo - restore city data
                        *iter = savedPlotData;
                    }
                    outputs.push_back(std::make_pair(iter->coords, plotResults));
                }
                else
                {
#ifdef ALTAI_DEBUG
                    //CityLog::getLog(pCity_)->getStream() << "\nFailed to find plot: " << iter->coords << " in improvements list.";
#endif
                }
            }
        }

#ifdef ALTAI_DEBUG
        // debug summary
        {
            TotalOutput baseline; 
            for (size_t i = 0, count = outputs.size(); i < count; ++i)
            {
                if (i == 0)
                {
                    baseline = *boost::get<2>(outputs[0].second[0]).cumulativeOutput.rbegin();
                    CityLog::getLog(pCity_)->getStream() << "\nBaseline = " << baseline << " weights = " << outputWeights << "\n";
                }
                else
                {
                    for (size_t j = 0, count = outputs[i].second.size(); j < count; ++j)
                    {
                        TotalOutput output = *boost::get<2>(outputs[i].second[j]).cumulativeOutput.rbegin();
                        CityLog::getLog(pCity_)->getStream() << outputs[i].first << " (" << gGlobals.getImprovementInfo(boost::get<1>(outputs[i].second[j])).getType()
                            << ") " << output << " Delta = " << output - baseline << " value = " << TotalOutputValueFunctor(outputWeights)(output - baseline) << "\n";
                    }
                }
            }
        }
#endif

        return outputs;
    }

    CitySimulation::CitySimulation(const CvCity* pCity, const boost::shared_ptr<CityData>& pCityData, const ConstructItem& constructItem)
        : pCity_(pCity), pCityData_(pCityData), turn_(0), constructItem_(constructItem), buildingBuilt_(false)
    {
        pCityOptimiser_ = boost::shared_ptr<CityOptimiser>(new CityOptimiser(pCityData_));
        pLog_ = CityLog::getLog(pCity_);
    }

    boost::shared_ptr<CityOptimiser> CitySimulation::getCityOptimiser() const
    {
        return pCityOptimiser_;
    }

    boost::shared_ptr<CityData> CitySimulation::getCityData() const
    {
        return pCityData_;
    }

    void BuildingSimulationResults::debugResults(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\nNo building baseline output = ";
        noBuildingBaseline.debugResults(os);

        for (size_t i = 0, count = buildingResults.size(); i < count; ++i)
        {
            os << "\n" << gGlobals.getBuildingInfo(buildingResults[i].buildingType).getType() << ", ";
        
            os << "Normal build output = ";
            if (buildingResults[i].normalBuild.pCityData)
            {
                buildingResults[i].normalBuild.debugResults(os);
            }
            else
            {
                os << " (not built) ";
            }

            TotalOutput delta = SimulationOutput::getDelta(buildingResults[i].normalBuild, buildingResults[i].baseline);
            os << "Delta = " << delta;
            delta[OUTPUT_FOOD] += buildingResults[i].normalBuild.pCityData->currentFood;
            delta[OUTPUT_FOOD] -= buildingResults[i].baseline.pCityData->currentFood;
            os << " delta with food difference = " << delta;

            os << " Turn built = " << buildingResults[i].baseline.cumulativeOutput.size() - buildingResults[i].normalBuild.cumulativeOutput.size();
            os << "\n";

            // Hurry results:
            for (size_t j = 0, count = buildingResults[i].hurryBuilds.size(); j < count; ++j)
            {
                os << buildingResults[i].hurryBuilds[j].first;
                buildingResults[i].hurryBuilds[j].second.debugResults(os);
                os << "Delta = " << SimulationOutput::getDelta(buildingResults[i].hurryBuilds[j].second, buildingResults[i].baseline);
                os << " Turn built = " << buildingResults[i].baseline.cumulativeOutput.size() - buildingResults[i].hurryBuilds[j].second.cumulativeOutput.size();
                os << "\n";
            }
        }
#endif
    }

    TotalOutput SimulationOutput::getDelta(const SimulationOutput& comparison, const SimulationOutput& baseline)
    {
        //std::ostream& os = CityLog::getLog(comparison.pCityData->pCity)->getStream();

        const size_t baselineSize = baseline.cumulativeOutput.size();
        const size_t comparisonSize = comparison.cumulativeOutput.size();

        //os << "\nbaseline = " << baselineSize << ", comparison = " << comparisonSize;

        if (baselineSize == 0 || comparisonSize == 0 || comparisonSize > baselineSize)
        {
            return TotalOutput(); // strictly these conditions are errors
        }

        if (baselineSize == comparisonSize)
        {
            //os << " outputs = " << comparison.cumulativeOutput[comparisonSize - 1] << ", " << baseline.cumulativeOutput[baselineSize - 1];
            return comparison.cumulativeOutput[comparisonSize - 1] - baseline.cumulativeOutput[baselineSize - 1];
        }
        else
        {
            const size_t firstBaseLineIndex = baselineSize - comparisonSize - 1;
            //os << " first base line index = " << firstBaseLineIndex;
            //os << " outputs = " << comparison.cumulativeOutput[comparisonSize - 1] << ", " << baseline.cumulativeOutput[baselineSize - 1] << ", " << baseline.cumulativeOutput[firstBaseLineIndex];
            return comparison.cumulativeOutput[comparisonSize - 1] - (baseline.cumulativeOutput[baselineSize - 1] - baseline.cumulativeOutput[firstBaseLineIndex]);
        }
    }

    // TODO add fn to handle adding hurry cost to correct place in cumulative cost
    void SimulationOutput::addTurn(const boost::shared_ptr<CityData>& cityOutputData)
    {
        TotalOutput output = cityOutputData->getActualOutput();
        int cost = cityOutputData->maintenanceHelper->getMaintenance();

        cumulativeOutput.push_back(cumulativeOutput.empty() ? output : output + *cumulativeOutput.rbegin());
        cumulativeCost.push_back(cumulativeCost.empty() ? cost : cost + *cumulativeCost.rbegin());
        cumulativeGPP.push_back(cumulativeGPP.empty() ? cityOutputData->getGPP() : addGppOutput(*cumulativeGPP.rbegin(), cityOutputData->getGPP()));
    }

    void SimulationOutput::debugResults(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n";
        if (cumulativeOutput.empty())
        {
            os << TotalOutput();
        }
        else
        {
            os << *cumulativeOutput.rbegin();
        }
        os << " Pop changes: ";
        for (size_t i = 0, popChangeCount = popHistory.size(); i < popChangeCount; ++i)
        {
            os << "(" << popHistory[i].first << ", " << popHistory[i].second << ") ";
        }
        if (pCityData)
        {
            pCityData->debugBasicData(os);
        }

        if (pCityOptimiser)
        {
            pCityOptimiser->debug(os, false);
        }

        if (!cumulativeGPP.empty())
        {
            for (GreatPersonOutputMap::const_iterator ci(cumulativeGPP.rbegin()->begin()), ciEnd(cumulativeGPP.rbegin()->end()); ci != ciEnd; ++ci)
            {
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(ci->first);
                os << " " << unitInfo.getType() << " " << ci->second;
            }
            os << " ";
        }
#endif
    }

    void CitySimulation::simulate(BuildingSimulationResults& results, int nTurns)
    {
        BuildingSimulationResults::BuildingResult simulationResults(constructItem_.buildingType);

        pLog_->logBuilding(constructItem_.buildingType);

        pCityData_->setBuilding(constructItem_.buildingType);
        optimisePlots();

        while (!buildingBuilt_ && turn_ < nTurns)
        {
            doTurn_();
        }

        if (buildingBuilt_)
        {
            // copy current city data for baseline
            CitySimulation completedBuildingSimulation(pCity_, boost::shared_ptr<CityData>(new CityData(*pCityData_)), constructItem_);

            completedBuildingSimulation.simulateNoHurry_(simulationResults, nTurns - turn_);

            while (turn_ < nTurns)
            {
                doTurn_();
            }
            simulationResults.baseline = simulationOutput_;
            simulationResults.baseline.pCityData = pCityData_;
            simulationResults.baseline.pCityOptimiser = pCityOptimiser_;

            results.buildingResults.push_back(simulationResults);
        }
    }

    void CitySimulation::simulateNoHurry_(BuildingSimulationResults::BuildingResult& buildingResult, int nTurns)
    {
        updateRequestData(pCity_, *pCityData_, gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getAnalysis()->getBuildingInfo(constructItem_.buildingType));
        buildingBuilt_ = true;

        optimisePlots();

        while (turn_ < nTurns)
        {
            doTurn_();
        }

        buildingResult.normalBuild = simulationOutput_;
        buildingResult.normalBuild.pCityData = pCityData_;
        buildingResult.normalBuild.pCityOptimiser = pCityOptimiser_;
    }

    SimulationOutput CitySimulation::simulateAsIs(int nTurns, OutputTypes outputType)
    {
        SimulationOutput simulationResults;

        //pCityOptimiser_->optimise(outputType);
        optimisePlots();

        while (turn_ < nTurns)
        {
            doTurn_();
        }

        simulationOutput_.pCityData = pCityData_;
        simulationOutput_.pCityOptimiser = pCityOptimiser_;

        return simulationOutput_;
    }

    void CitySimulation::simulate(BuildingSimulationResults& results, int nTurns, HurryTypes hurryType)
    {
        BuildingSimulationResults::BuildingResult simulationResults(constructItem_.buildingType);

        pLog_->logBuilding(constructItem_.buildingType);

        pCityData_->setBuilding(constructItem_.buildingType);

        optimisePlots();
        //pCityOptimiser_->optimise(OUTPUT_PRODUCTION);

        int currentPopCost = 0, currentGoldCost = 0;

        bool canHurry;
        HurryData hurryData(hurryType);
        boost::tie(canHurry, hurryData) = pCityData_->canHurry(hurryType);

        currentPopCost = hurryData.hurryPopulation;
        currentGoldCost = hurryData.hurryGold;
        int goldIncrement = currentGoldCost / 5;

        bool usesPop = currentPopCost > 0, usesGold = currentGoldCost > 0, doHurry = canHurry;
        bool didHurry = false;

        // loop whilst building not built via normal production
        while (!buildingBuilt_ && turn_ < nTurns)
        {
            if (doHurry)
            {
                didHurry = true;
                currentPopCost = hurryData.hurryPopulation;
                currentGoldCost = hurryData.hurryGold;
                
                // copy current city data for hurry
                CitySimulation hurrySimulation(pCity_, boost::shared_ptr<CityData>(new CityData(*pCityData_)), constructItem_);
                hurrySimulation.simulateHurry_(simulationResults, hurryData, nTurns - turn_);
            }

            doTurn_();

            boost::tie(canHurry, hurryData) = pCityData_->canHurry(hurryType);

            doHurry = canHurry && ((usesPop && hurryData.hurryPopulation < currentPopCost) || (usesGold && !usesPop && hurryData.hurryGold < currentGoldCost - goldIncrement));
        }

        // built through normal production
        if (buildingBuilt_)
        {
            // copy current city data for baseline
            CitySimulation completedBuilding(pCity_, boost::shared_ptr<CityData>(new CityData(*pCityData_)), constructItem_);
            completedBuilding.simulateNoHurry_(simulationResults, nTurns - turn_);
        }

        if (didHurry || buildingBuilt_)
        {
            while (turn_ < nTurns)
            {
                doTurn_();
            }
            simulationResults.baseline = simulationOutput_;
            simulationResults.baseline.pCityData = pCityData_;
            simulationResults.baseline.pCityOptimiser = pCityOptimiser_;

            results.buildingResults.push_back(simulationResults);
        }
    }

    void CitySimulation::simulateHurry_(BuildingSimulationResults::BuildingResult& buildingResult, const HurryData& hurryData, int nTurns)
    {
        pLog_->logHurryBuilding(constructItem_.buildingType, hurryData);
        pCityData_->hurry(hurryData);

        simulationOutput_.cumulativeCost.rbegin() += hurryData.hurryGold;

        handleEvents_();

        doTurn_();

        // something wrong if this isn't the case
        if (buildingBuilt_)
        {
            updateRequestData(pCity_, *pCityData_, gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getAnalysis()->getBuildingInfo(constructItem_.buildingType));

            optimisePlots();

            pLog_->logPlots(*pCityOptimiser_);
        }

        while (turn_ < nTurns)
        {
            doTurn_();
        }
        
        simulationOutput_.pCityData = pCityData_;
        simulationOutput_.pCityOptimiser = pCityOptimiser_;
        buildingResult.hurryBuilds.push_back(std::make_pair(hurryData, simulationOutput_));
    }

    void CitySimulation::doTurn_()
    {
        pCityData_->advanceTurn();
        handleEvents_();

        simulationOutput_.addTurn(pCityData_);

        pLog_->logTurn(turn_, pCityData_->getOutput());
        pLog_->logCityData(pCityData_);

        ++turn_;
    }

    OutputTypes CitySimulation::getOptType_() const
    {
        return buildingBuilt_ || constructItem_.buildingType == NO_BUILDING ? NO_OUTPUT : OUTPUT_PRODUCTION;
    }

    void CitySimulation::handleEvents_()
    {
        while (CitySimulationEventPtr pEvent = pCityData_->getEvent())
        {
#ifdef ALTAI_DEBUG
            pEvent->stream(CityLog::getLog(pCityData_->pCity)->getStream());
#endif
            pEvent->handleEvent(*this);
        }

        if (needsOpt_)
        {
            needsOpt_ = false;
            optimisePlots();
#ifdef ALTAI_DEBUG
            logPlots();
#endif
        }
    }

    void CitySimulation::optimisePlots()
    {
        plotAssignmentSettings_ = makePlotAssignmentSettings(pCityData_, pCity_, constructItem_);
        //// similar to assignPlots() logic - need to handle processes properly and integrate this code with City::assignPlots()
        //CityOptimiser::GrowthType growthType = CityOptimiser::Not_Set;

        //bool isWonder = constructItem_.buildingType != NO_BUILDING && isLimitedWonderClass(getBuildingClass(buildingType_));
        //const int maxResearchRate = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getMaxResearchRate();

        //std::vector<OutputTypes> outputTypes;

        //if (maxResearchRate < 30)
        //{
        //    if (outputTypes.empty())
        //    {
        //        outputTypes.push_back(OUTPUT_GOLD);
        //    }
        //}
        //else if (isWonder)
        //{
        //    outputTypes.push_back(OUTPUT_PRODUCTION);
        //}
        //else
        //{
        //    // todo - get isNoUnhappiness data from pCityData_
        //    const int angryPop = pCity_->angryPopulation();
        //    const int happyCap = pCityData_->happyCap;
        //    //if (pCity_->isNoUnhappiness() || angryPop <= (CvPlayerAI::getPlayer(pCity_->getOwner()).canPopRush() ? 2 : 0))
        //    if (pCity_->isNoUnhappiness() || happyCap > (CvPlayerAI::getPlayer(pCity_->getOwner()).canPopRush() ? -2 : 0))
        //    {
        //        growthType = CityOptimiser::MajorGrowth;
        //        outputTypes.push_back(OUTPUT_FOOD);
        //        outputTypes.push_back(OUTPUT_PRODUCTION);
        //        outputTypes.push_back(OUTPUT_RESEARCH);
        //        outputTypes.push_back(OUTPUT_GOLD);
        //    }
        //    else
        //    {
        //        if (angryPop > 4)
        //        {
        //            growthType = CityOptimiser::MinorStarve;
        //        }
        //        outputTypes.push_back(OUTPUT_PRODUCTION);
        //        outputTypes.push_back(OUTPUT_RESEARCH);
        //        outputTypes.push_back(OUTPUT_GOLD);
        //    }
        //}

        //if (growthType == CityOptimiser::Not_Set)
        //{
        //    growthType = pCityOptimiser_->getGrowthType();
        //}

        //pCityOptimiser_->optimise(NO_OUTPUT, growthType, false);
        //TotalOutputWeights outputWeights(pCityOptimiser_->getMaxOutputWeights());
        //outputWeights[OUTPUT_CULTURE] = std::max<int>(1, --outputWeights[OUTPUT_CULTURE]);
        //outputWeights[OUTPUT_ESPIONAGE] = std::max<int>(1, --outputWeights[OUTPUT_ESPIONAGE]);

        pCityOptimiser_->optimise(NO_OUTPUT, plotAssignmentSettings_.growthType, false);
        pCityOptimiser_->optimise<MixedWeightedTotalOutputOrderFunctor>(plotAssignmentSettings_.outputPriorities, plotAssignmentSettings_.outputWeights, plotAssignmentSettings_.growthType, false);

        //CityOptimiser::OptState optState = pCityOptimiser_->optimise(getOptType_());

#ifdef ALTAI_DEBUG
        //std::ostream& os = CityLog::getLog(pCity_)->getStream() ;
        //pCityOptimiser_->debug(os, true);
        /*{
            Range target = pCityOptimiser_->getTargetYield();
            CityLog::getLog(pCity_)->getStream() << "\n range = " << target 
                << " opt = " << optState << ", growthType = " << pCityOptimiser_->getGrowthType()
                << " actualYield = " << pCityData_->getFood();
        }*/
#endif
    }

    void CitySimulation::logPlots(bool printAllPlots) const
    {
#ifdef ALTAI_DEBUG
        pLog_->logPlots(*pCityOptimiser_, printAllPlots);
#endif
    }

    void CitySimulation::handlePopChange_(int change)
    {
        simulationOutput_.popHistory.push_back(std::make_pair(turn_, change));
    }

    void CitySimulation::handleBuildingBuilt_(BuildingTypes buildingType)
    {
        buildingBuilt_ = true;
    }

    void PopChange::handleEvent(CitySimulation& simulation)
    {
        simulation.handlePopChange_(change_);
        simulation.setNeedsOpt();
    }

    void PopChange::stream(std::ostream& os)
    {
#ifdef ALTAI_DEBUG
        os << "\n Pop change: " << change_;
#endif
    }

    void WorkingPopChange::handleEvent(CitySimulation& simulation)
    {
        simulation.setNeedsOpt();
    }

    void WorkingPopChange::stream(std::ostream& os)
    {
#ifdef ALTAI_DEBUG
        os << "\n Working pop change: " << change_;
#endif
    }

    void HappyCapChange::handleEvent(CitySimulation& simulation)
    {
        simulation.setNeedsOpt();
    }

    void HappyCapChange::stream(std::ostream& os)
    {
#ifdef ALTAI_DEBUG
        os << "\n Happy cap changed by: " << change_;
#endif
    }

    void TestBuildingBuilt::handleEvent(CitySimulation& simulation)
    {
        simulation.setNeedsOpt();
        simulation.handleBuildingBuilt_(buildingType_);
    }

    void TestBuildingBuilt::stream(std::ostream& os)
    {
#ifdef ALTAI_DEBUG
        os << "\n Building constructed: " << gGlobals.getBuildingInfo(buildingType_).getType();
#endif
    }

    void ImprovementUpgrade::handleEvent(CitySimulation& simulation)
    {
        simulation.setNeedsOpt();
    }

    void ImprovementUpgrade::stream(std::ostream& os)
    {
#ifdef ALTAI_DEBUG
        os << "\n Improvement upgraded: ";
        for (size_t i = 0, count = data_.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << "( improvement = " << gGlobals.getImprovementInfo(data_[i].first).getType() << ", coords = " << data_[i].second << " ) ";
        }
#endif
    }

    void CultureBorderExpansion::handleEvent(CitySimulation& simulation)
    {
        simulation.setNeedsOpt();
    }

    void CultureBorderExpansion::stream(std::ostream& os)
    {
#ifdef ALTAI_DEBUG
        os << "\n Cultural borders expanded";
#endif
    }

    void PlotControlChange::handleEvent(CitySimulation& simulation)
    {
        simulation.setNeedsOpt();
    }

    void PlotControlChange::stream(std::ostream& os)
    {
#ifdef ALTAI_DEBUG
        os << "\n Plot(s) control change: ";
        for (size_t i = 0, count = data_.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << "( player = " << data_[i].first << ", coords = " << data_[i].second << " ) ";
        }
#endif
    }
}