#include "./city_optimiser.h"
#include "./city_log.h"
#include "./civ_log.h"
#include "./map_analysis.h"
#include "./specialist_helper.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./tactic_actions.h"

namespace AltAI
{
    namespace
    {
        MixedTotalOutputOrderFunctor makeMixedF(TotalOutputWeights weights)
        {
            TotalOutputPriority priorities(makeTotalOutputSinglePriority(OUTPUT_FOOD));
            return MixedTotalOutputOrderFunctor(priorities, weights);
        }

        MixedOutputOrderFunctor<PlotYield> makeMixedF(YieldWeights weights)
        {
            YieldPriority yieldPriority;
            yieldPriority.assign(-1);
            yieldPriority[0] = YIELD_FOOD;

            return MixedOutputOrderFunctor<PlotYield>(yieldPriority, weights);
        }

        struct IsWorked
        {
            bool operator() (const PlotData& plotData) const
            {
                return plotData.isWorked;
            }
        };

        struct Unworked
        {
            bool operator() (const PlotData& plotData) const
            {
                return !plotData.isWorked;
            }
        };

        struct IsWorkable
        {
            bool operator() (const PlotData& plotData) const
            {
                return plotData.ableToWork && plotData.controlled;
            }
        };

        struct PlotDataFinder
        {
            explicit PlotDataFinder(const PlotData& plot_) : plot(plot_) {}

            bool operator() (const PlotData& other) const
            {
                return plot.coords == other.coords && plot.isWorked == other.isWorked;
            }

            const PlotData& plot;
        };

        struct PlotCoordsFinder
        {
            explicit PlotCoordsFinder(const PlotData& plot_) : plot(plot_) {}

            bool operator() (const PlotData& other) const
            {
                return plot.coords == other.coords;
            }

            const PlotData& plot;
        };

        bool PlotWasSwappedOut(const CityOptimiser::SwapData& swapData, const PlotData& plot)
        {
            for (size_t i = 0, count = swapData.size(); i < count; ++i)
            {
                if (swapData[i].second.coords == plot.coords)
                {
                    return true;
                }
            }
            return false;
        }

        bool PlotWasSwappedIn(const CityOptimiser::SwapData& swapData, const PlotData& plot)
        {
            for (size_t i = 0, count = swapData.size(); i < count; ++i)
            {
                if (swapData[i].first.coords == plot.coords)
                {
                    return true;
                }
            }
            return false;
        }

        template <typename P>
            struct SpecialFoodOutputOrderF
        {
            typedef typename P Pred;

            explicit SpecialFoodOutputOrderF(P pred_, bool ignoreDeficitFoodDiffs_ = true) 
                : pred(pred_), foodPerPop(100 * gGlobals.getFOOD_CONSUMPTION_PER_POPULATION()), ignoreDeficitFoodDiffs(ignoreDeficitFoodDiffs_)
            {
            }

            bool operator() (const PlotData& p1, const PlotData& p2) const
            {
                if (!(p1.output[OUTPUT_FOOD] == p2.output[OUTPUT_FOOD] ||
                    (ignoreDeficitFoodDiffs && p1.output[OUTPUT_FOOD] < foodPerPop && p2.output[OUTPUT_FOOD] < foodPerPop)))
                {
                    return p1.output[OUTPUT_FOOD] > p2.output[OUTPUT_FOOD];
                }

                return pred(p1.output, p2.output);
            }

            P pred;
            const int foodPerPop;
            bool ignoreDeficitFoodDiffs;
        };

        template <typename P>
            struct ReverseSpecialFoodOutputOrderF
        {
            typedef typename P Pred;

            explicit ReverseSpecialFoodOutputOrderF(P pred_) 
                : pred(pred_)
            {
            }

            bool operator() (const PlotData& p1, const PlotData& p2) const
            {
                if (p1.output[OUTPUT_FOOD] != p2.output[OUTPUT_FOOD])
                {
                    return p1.output[OUTPUT_FOOD] < p2.output[OUTPUT_FOOD];
                }

                return pred(p1.output, p2.output);
            }

            P pred;
        };


        template <typename P>
            struct PlotDataAdaptor
        {
            typedef typename P Pred;
            PlotDataAdaptor(P pred_) : pred(pred_) {}

            bool operator () (const PlotData& p1, const PlotData& p2) const
            {
                return pred(p1.output, p2.output);
            }
            P pred;
        };

        template <typename P>
            struct SpecialistPlotDataAdaptor
        {
            typedef typename P Pred;
            SpecialistPlotDataAdaptor(P pred_, UnitTypes specType_)
                : pred(pred_), specType(specType_)
            {
            }

            bool operator () (const PlotData& p1, const PlotData& p2) const
            {
                if (p1.greatPersonOutput.unitType == specType || p2.greatPersonOutput.unitType == specType)
                {
                    if (p1.greatPersonOutput.unitType == p2.greatPersonOutput.unitType)
                    {
                        return p1.greatPersonOutput.output > p2.greatPersonOutput.output;
                    }
                    else
                    {
                        return p1.greatPersonOutput.unitType == specType;
                    }
                }
                return pred(p1.output, p2.output);
            }

            P pred;
            UnitTypes specType;
        };

        std::pair<CityOptimiser::GrowthType, std::vector<OutputTypes> > convertEmphasis(TotalOutputWeights weights, const CvCity* pCity)
        {
            std::vector<OutputTypes> outputTypes;

            for (int i = 0; i < NUM_YIELD_TYPES - 1; ++i)  // not commerce
            {
                if (((CvCityAI*)pCity)->AI_isEmphasizeYield((YieldTypes)i))
                {
                    outputTypes.push_back((OutputTypes)i);
                }
            }

            int commerceBaseWeight = 0;
            bool isEmphasisCommerce = ((CvCityAI*)pCity)->AI_isEmphasizeYield((YieldTypes)YIELD_COMMERCE);
            if (isEmphasisCommerce)
            {
                commerceBaseWeight = 1;
            }

            bool noSpecificCommerceEmphasis = true;
            for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
            {
                weights[i + NUM_YIELD_TYPES - 1] += commerceBaseWeight;
                if (((CvCityAI*)pCity)->AI_isEmphasizeCommerce((CommerceTypes)i))
                {
                    outputTypes.push_back((OutputTypes)(i + NUM_YIELD_TYPES - 1));
                    noSpecificCommerceEmphasis = false;
                }
            }

            if (isEmphasisCommerce && noSpecificCommerceEmphasis)
            {
                outputTypes.push_back(OUTPUT_GOLD);
            }

            CityOptimiser::GrowthType growthType = ((CvCityAI*)pCity)->AI_avoidGrowth() ? CityOptimiser::FlatGrowth : 
                ((CvCityAI*)pCity)->AI_isEmphasizeYield((YieldTypes)YIELD_FOOD) ? CityOptimiser::MajorGrowth : CityOptimiser::Not_Set;

            return std::make_pair(growthType, outputTypes);
        }
    }

    PlotAssignmentSettings::PlotAssignmentSettings()
        : outputWeights(makeOutputW()), outputPriorities(makeTotalOutputPriorities(std::vector<OutputTypes>())), growthType(CityOptimiser::Not_Set), specialistType(NO_SPECIALIST)
    {
    }

    template CityOptimiser::OptState CityOptimiser::optimise<MixedWeightedTotalOutputOrderFunctor>(TotalOutputPriority, TotalOutputWeights, Range, bool);
    template CityOptimiser::OptState CityOptimiser::optimise<MixedWeightedTotalOutputOrderFunctor>(TotalOutputPriority, TotalOutputWeights, CityOptimiser::GrowthType, bool);

    template CityOptimiser::OptState CityOptimiser::optimise<MixedTotalOutputOrderFunctor>(TotalOutputPriority, TotalOutputWeights, Range, bool);
    template CityOptimiser::OptState CityOptimiser::optimise<MixedTotalOutputOrderFunctor>(TotalOutputPriority, TotalOutputWeights, CityOptimiser::GrowthType, bool);

    template CityOptimiser::OptState CityOptimiser::optimise<ProcessValueAdaptorFunctor<MixedWeightedTotalOutputOrderFunctor>, MixedWeightedTotalOutputOrderFunctor>
        (TotalOutputPriority, TotalOutputWeights, CityOptimiser::GrowthType, CommerceModifier, bool);


    CityOptimiser::CityOptimiser(const boost::shared_ptr<CityData>& data, std::pair<TotalOutput, TotalOutputWeights> maxOutputs)
        : data_(data), maxOutputs_(maxOutputs)
    {
        foodPerPop_ = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
    }

    PlotAssignmentSettings makePlotAssignmentSettings(const boost::shared_ptr<CityData>& pCityData, const CvCity* pCity, const ConstructItem& constructItem)
    {
        PlotAssignmentSettings plotAssignmentSettings;
        boost::shared_ptr<Player> pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
        bool haveConstructItem = constructItem.buildingType != NO_BUILDING || constructItem.unitType != NO_UNIT || constructItem.processType != NO_PROCESS;

        const int angryPop = pCity->angryPopulation();
        const int happyCap = pCityData->happyCap;
        const bool noUnhappiness = pCity->isNoUnhappiness();
        const bool canPopRush = pPlayer->getCvPlayer()->canPopRush();
        const int maxResearchRate = pPlayer->getMaxResearchRate();
        const int cityCount = pPlayer->getCvPlayer()->getNumCities();

        TotalOutput maxOutputs = pPlayer->getCity(pCity->getID()).getMaxOutputs();

        const std::pair<int, int> rankAndMaxProduction = pPlayer->getCityRank(pCity->getIDInfo(), OUTPUT_PRODUCTION);
        const std::pair<int, int> rankAndMaxGold = pPlayer->getCityRank(pCity->getIDInfo(), OUTPUT_GOLD);
        const std::pair<int, int> rankAndMaxResearch = pPlayer->getCityRank(pCity->getIDInfo(), OUTPUT_RESEARCH);

        std::vector<OutputTypes> outputTypes;
        TotalOutputWeights outputWeights = makeOutputW(1, 1, 1, 1, 1, 1);
        Range targetFoodYield;
        CityOptimiser::GrowthType growthType = CityOptimiser::Not_Set;
        bool isWonder = false;

        if (constructItem.buildingType != NO_BUILDING || pCity->isProductionBuilding())
        {
            bool isWonder = constructItem.buildingFlags & (BuildingFlags::Building_World_Wonder | BuildingFlags::Building_National_Wonder);
            if (!haveConstructItem)
            {
                BuildingClassTypes buildingClassType = (BuildingClassTypes)(gGlobals.getBuildingInfo(pCity->getProductionBuilding()).getBuildingClassType());
                isWonder = isWorldWonderClass(buildingClassType) || isNationalWonderClass(buildingClassType) || isTeamWonderClass(buildingClassType);
            }

            if (isWonder)
            {
                targetFoodYield = Range(pCity->foodConsumption() * 100, maxOutputs[OUTPUT_FOOD]);
                outputTypes.push_back(OUTPUT_PRODUCTION);
                isWonder = true;
            }
            else if (constructItem.economicFlags & EconomicFlags::Output_Culture)
            {
                outputTypes.push_back(OUTPUT_PRODUCTION);
            }
            else if (canPopRush && angryPop < 4)
            {
                outputTypes.push_back(OUTPUT_FOOD);
                outputTypes.push_back(OUTPUT_PRODUCTION);
            }
            else
            {
                if (maxResearchRate < 30)
                {
                    outputTypes.push_back(OUTPUT_GOLD);
                    outputTypes.push_back(OUTPUT_PRODUCTION);
                }
                else
                {
                    outputTypes.push_back(OUTPUT_PRODUCTION);
                    outputTypes.push_back(OUTPUT_RESEARCH);
                }
            }
        }
        else if (constructItem.unitType != NO_UNIT || pCity->isProductionUnit())
        {
            // Building a unit which can build improvments (likely a workboat, since worker production consumes food and won't hit this check)
            if (!constructItem.possibleBuildTypes.empty())
            {
                outputTypes.push_back(OUTPUT_PRODUCTION);
                growthType = CityOptimiser::MinorGrowth;
            }
            else if (constructItem.militaryFlags & MilitaryFlags::Output_Defence)
            {
                outputTypes.push_back(OUTPUT_PRODUCTION);
            }
            else
            {
                if (maxResearchRate < 30)
                {
                    outputTypes.push_back(OUTPUT_GOLD);
                    outputTypes.push_back(OUTPUT_PRODUCTION);
                }
                else
                {
                    outputTypes.push_back(OUTPUT_PRODUCTION);
                }
            }
        }
        else if (constructItem.processType != NO_PROCESS || pCity->isProductionProcess())
        {
            //outputTypes.push_back(OUTPUT_PRODUCTION);

            ProcessTypes processType = constructItem.processType;
            if (processType == NO_PROCESS)
            {
                processType = pCity->getProductionProcess();
            }

            const CvProcessInfo& processInfo = gGlobals.getProcessInfo(processType);
            if (processInfo.getProductionToCommerceModifier(COMMERCE_GOLD) > 0)
            {
                plotAssignmentSettings.specialistType = pPlayer->getAnalysis()->getBestSpecialist(OUTPUT_GOLD);
                outputTypes.push_back(OUTPUT_GOLD);
            }
            else if (processInfo.getProductionToCommerceModifier(COMMERCE_RESEARCH) > 0)
            {
                plotAssignmentSettings.specialistType = pPlayer->getAnalysis()->getBestSpecialist(OUTPUT_RESEARCH);
                outputTypes.push_back(OUTPUT_RESEARCH);
            }
            else if (processInfo.getProductionToCommerceModifier(COMMERCE_CULTURE) > 0)
            {
                plotAssignmentSettings.specialistType = pPlayer->getAnalysis()->getBestSpecialist(OUTPUT_CULTURE);
                outputTypes.push_back(OUTPUT_CULTURE);
            }
            else if (processInfo.getProductionToCommerceModifier(COMMERCE_ESPIONAGE) > 0)
            {
                plotAssignmentSettings.specialistType = pPlayer->getAnalysis()->getBestSpecialist(OUTPUT_ESPIONAGE);
                outputTypes.push_back(OUTPUT_ESPIONAGE);
            }
        }
        // todo projects, spaceships, etc...
        else  // means we are building something we didn't choose 
        {
            outputWeights = makeOutputW(3, 3, 2, 2, 1, 1);
            outputTypes.push_back(OUTPUT_PRODUCTION);
            if (gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getMaxResearchRate() > 50)
            {
                outputTypes.push_back(OUTPUT_RESEARCH);
            }
            else
            {
                outputTypes.push_back(OUTPUT_GOLD);
            }
            growthType = CityOptimiser::MinorGrowth;
        }

        if (noUnhappiness || happyCap > (canPopRush ? -2 : 0))
        {
            if (growthType == CityOptimiser::Not_Set && !isWonder)
            {
                plotAssignmentSettings.growthType = CityOptimiser::MajorGrowth;
            }
        }

        if (std::find(outputTypes.begin(), outputTypes.end(), OUTPUT_PRODUCTION) == outputTypes.end() && rankAndMaxProduction.first < cityCount / 2)
        {
            outputWeights[OUTPUT_PRODUCTION] += 2;
        }

        if (std::find(outputTypes.begin(), outputTypes.end(), OUTPUT_RESEARCH) == outputTypes.end() && rankAndMaxResearch.first < cityCount / 2)
        {
            outputWeights[OUTPUT_RESEARCH] += 2;
        }
        else if (std::find(outputTypes.begin(), outputTypes.end(), OUTPUT_PRODUCTION) == outputTypes.end() && rankAndMaxGold.first < cityCount / 2)
        {
            outputWeights[OUTPUT_GOLD] += 2;
        }

        // allow emphasis buttons to do something
        // TODO emphasis GP doesn't work at the moment
        if (pCity->isHuman())
        {
            std::pair<CityOptimiser::GrowthType, std::vector<OutputTypes> > emphasisOptions = convertEmphasis(outputWeights, pCity);
            if (!emphasisOptions.second.empty())
            {
                for (size_t i = 0, count = outputTypes.size(); i < count; ++i)
                {
                    if (std::find(emphasisOptions.second.begin(), emphasisOptions.second.end(), outputTypes[i]) == emphasisOptions.second.end())
                    {
                        emphasisOptions.second.push_back(outputTypes[i]);
                    }
                }
                plotAssignmentSettings.outputPriorities = makeTotalOutputPriorities(emphasisOptions.second);
            }
            else
            {
                plotAssignmentSettings.outputPriorities = makeTotalOutputPriorities(outputTypes);
            }

            if (emphasisOptions.first != CityOptimiser::GrowthType::Not_Set)
            {
                plotAssignmentSettings.growthType = emphasisOptions.first;
            }
            else
            {
                plotAssignmentSettings.growthType = growthType;
            }

            plotAssignmentSettings.outputWeights = outputWeights;
            plotAssignmentSettings.targetFoodYield = targetFoodYield;
        }
        else
        {
            plotAssignmentSettings.outputPriorities = makeTotalOutputPriorities(outputTypes);
            plotAssignmentSettings.outputWeights = outputWeights;
            plotAssignmentSettings.growthType = growthType;
            plotAssignmentSettings.targetFoodYield = targetFoodYield;
        }

        return plotAssignmentSettings;
    }

    CityOptimiser::OptState CityOptimiser::optimise(OutputTypes outputType, GrowthType growthType, bool debug)
    {
        calibrate_(debug);
        targetYield_ = calcTargetYieldSurplus(growthType);

        if (outputType == NO_OUTPUT)
        {
            return optimise_(PlotDataAdaptor<TotalOutputValueFunctor>(TotalOutputValueFunctor(maxOutputs_.second)), debug);
        }
        else
        {
            TotalOutputWeights outputWeights;
            outputWeights.assign(1);
            outputWeights[outputType] = maxOutputs_.second[outputType];
            return optimise_(PlotDataAdaptor<TotalOutputValueFunctor>(TotalOutputValueFunctor(outputWeights)), debug);
        }
    }

    CityOptimiser::OptState CityOptimiser::optimise(TotalOutputWeights outputWeights, GrowthType growthType, bool debug)
    {
        targetYield_ = calcTargetYieldSurplus(growthType);
        return optimise_(PlotDataAdaptor<TotalOutputValueFunctor>(TotalOutputValueFunctor(outputWeights)), debug);
    }

    template <typename F>
        CityOptimiser::OptState CityOptimiser::optimise(TotalOutputPriority outputPriorities, TotalOutputWeights outputWeights, GrowthType growthType, bool debug)
    {
        targetYield_ = calcTargetYieldSurplus(growthType);

#ifdef ALTAI_DEBUG
        if (debug)
        {
            std::ostream& os = CityLog::getLog(data_->pCity)->getStream();
            os << "\nOutput priorities: " << outputPriorities << " output weights = " << outputWeights << " target yield = " << targetYield_ << " growthtype = " << getGrowthTypeString(growthType);
        }
#endif
        return optimise_(PlotDataAdaptor<F>(F(outputPriorities, outputWeights)), debug);
    }

    template <typename F>
        CityOptimiser::OptState CityOptimiser::optimise(TotalOutputPriority outputPriorities, TotalOutputWeights outputWeights, Range targetYield, bool debug)
    {
        targetYield_ = targetYield;

#ifdef ALTAI_DEBUG
        if (debug)
        {
            std::ostream& os = CityLog::getLog(data_->pCity)->getStream();
            os << "\nOutput priorities: " << outputPriorities << " output weights = " << outputWeights << " target yield = " << targetYield_;
        }
#endif
        return optimise_(PlotDataAdaptor<F>(F(outputPriorities, outputWeights)), debug);
    }

    template <typename P, typename F>
            CityOptimiser::OptState CityOptimiser::optimise(TotalOutputPriority outputPriorities, TotalOutputWeights outputWeights, GrowthType growthType, CommerceModifier processModifier, bool debug)
    {
        targetYield_ = calcTargetYieldSurplus(growthType);

#ifdef ALTAI_DEBUG
        if (debug)
        {
            std::ostream& os = CityLog::getLog(data_->pCity)->getStream();
            os << "\nOutput priorities: " << outputPriorities << " output weights = " << outputWeights << " target yield = " << targetYield_ << " growthtype = " << getGrowthTypeString(growthType)
                << " process modifier = " << processModifier;
        }
#endif
        return optimise_(PlotDataAdaptor<P>(P(F(outputPriorities, outputWeights), processModifier)), debug);
    }

    // todo - correct for case where hammers and food together are sufficient, but food alone isn't (leads to starvation in some cases - although this may actually be the best option)
    CityOptimiser::OptState CityOptimiser::optimiseFoodProduction(bool debug)
    {
        // angry people don't need feeding for food production builds
        int requiredYield = 100 * (data_->cityPopulation - std::max<int>(0, data_->angryPopulation() - data_->happyPopulation())) * foodPerPop_ + data_->getLostFood();
        targetYield_ = Range(requiredYield, Range::LowerBound);
        TotalOutputWeights outputWeights;
        outputWeights.assign(1);
        outputWeights[OUTPUT_FOOD] = outputWeights[OUTPUT_PRODUCTION] = 10;

#ifdef ALTAI_DEBUG
        if (debug)
        {
            std::ostream& os = CityLog::getLog(data_->pCity)->getStream();
            os << "\n(Food Production) Output weights = " << outputWeights;
        }
#endif
        return optimise_(PlotDataAdaptor<TotalOutputValueFunctor>(TotalOutputValueFunctor(outputWeights)), debug);
    }

    CityOptimiser::OptState CityOptimiser::optimise(UnitTypes specType, GrowthType growthType, bool debug)
    {
        calibrate_();
        targetYield_ = calcTargetYieldSurplus(growthType);
        return optimise_(SpecialistPlotDataAdaptor<TotalOutputValueFunctor>(TotalOutputValueFunctor(maxOutputs_.second), specType), debug);
    }

    void CityOptimiser::calibrate_(bool debug)
    {
        targetYield_ = Range(100 * (data_->cityPopulation * foodPerPop_));

        TotalOutputWeights weights;
        maxOutputs_.first = TotalOutput();
        maxOutputs_.second.assign(0);
        weights.assign(1);

        for (int i = 0; i < NUM_OUTPUT_TYPES; ++i)
        {
            TotalOutputPriority priorities(makeTotalOutputSinglePriority((OutputTypes)i));
            optimise_<PlotDataAdaptor<MixedTotalOutputOrderFunctor> >(MixedTotalOutputOrderFunctor(priorities, weights), debug);
            maxOutputs_.first[i] = data_->getOutput()[i];

            // no output even with this type set as priority
            if (maxOutputs_.first[i] == 0)
            {
                maxOutputs_.second[i] = 0;
#ifdef ALTAI_DEBUG
                if (debug)
                {
                    CityLog::getLog(data_->pCity)->logPlots(*this);
                    CityLog::getLog(data_->pCity)->logOptimisedWeights(maxOutputs_.first, maxOutputs_.second);
                }
#endif
                continue;
            }

            int maxWeightedOutput = 0;

            int repeatCount = 0;
            while (true)
            {
                ++weights[i];
                optimise_<PlotDataAdaptor<TotalOutputValueFunctor> >(TotalOutputValueFunctor(weights));
                TotalOutput output = data_->getOutput();

                if (output[i] == maxWeightedOutput)
                {
                    ++repeatCount;
                }
                else
                {
                    repeatCount = 0;
                }

                if (repeatCount > 3)  // not increasing
                {
                    maxOutputs_.second[i] = 1 + weights[i] - repeatCount;
                    break;
                }

                if (output[i] >= maxWeightedOutput)
                {
                    maxOutputs_.second[i] = weights[i];
                    maxWeightedOutput = output[i];
#ifdef ALTAI_DEBUG
                    if (debug)
                    {
                        CityLog::getLog(data_->pCity)->logPlots(*this);
                        CityLog::getLog(data_->pCity)->logOptimisedWeights(maxOutputs_.first, maxOutputs_.second);
                    }
#endif
                    if (output[i] > maxOutputs_.first[i])  // don't expect this to happen much, if at all
                    {
                        maxOutputs_.first[i] = output[i];

                        CityLog::getLog(data_->pCity)->logPlots(*this);
                        CityLog::getLog(data_->pCity)->logOptimisedWeights(maxOutputs_.first, maxOutputs_.second);

                        break;
                    }
                }
                else if (weights[i] > 20)  // do stricter cutoff (just on outputs not increasing?)
                {
                    maxOutputs_.second[i] = weights[i];
                    break;
                }
            }
            weights.assign(1);
        }
    }

    TotalOutput CityOptimiser::getMaxOutputs() const
    {
        return maxOutputs_.first;
    }

    TotalOutputWeights CityOptimiser::getMaxOutputWeights() const
    {
        return maxOutputs_.second;
    }

    template <class ValueAdaptor>
        CityOptimiser::OptState CityOptimiser::optimise_(ValueAdaptor valueAdaptor, bool debug)
    {
        OptState optState = OK;
        const int plotCount = data_->plotOutputs.size();

        reclaimSpecialistSlots_();

        for (PlotDataIter iter(data_->plotOutputs.begin()), endIter(data_->plotOutputs.end()); iter != endIter; ++iter)
        {
            iter->isWorked = false;
        }

        for (PlotDataIter iter(data_->freeSpecOutputs.begin()), endIter(data_->freeSpecOutputs.end()); iter != endIter; ++iter)
        {
            iter->isWorked = false;
        }

        // todo: if free specs give food, need to include those in food optimisation
        // this is not that likely, as leads to runaway specialist count (if health and happy not issue)
        data_->freeSpecOutputs.sort(valueAdaptor);
        PlotDataIter plotIter = data_->freeSpecOutputs.begin();
        for (int i = 0; i < data_->specialistHelper->getTotalFreeSpecialistSlotCount(); ++i)
        {
            plotIter->isWorked = true;
            removeSpecialistSlot_((SpecialistTypes)plotIter->coords.iY);  // used a 'free' slot, so make sure we don't use it again in the main plots' slots
            ++plotIter;
        }

        // Nothing to do (more population than plots/specialists available or only one/zero plots)
        if (plotCount < 2 || data_->cityPopulation > plotCount)
        {
            for (PlotDataIter iter(data_->plotOutputs.begin()), endIter(data_->plotOutputs.end()); iter != endIter; ++iter)
            {
                iter->isWorked = true;
            }
        }
        else
        {
            data_->plotOutputs.sort(valueAdaptor);

            plotIter = data_->plotOutputs.begin();
            for (int i = 0; i < data_->workingPopulation && i < plotCount; ++i)
            {
                plotIter->isWorked = true;
                ++plotIter;
            }
                
            int actualYield = data_->getFood();
            SwapData swapData;

#ifdef ALTAI_DEBUG
            if (debug)
            {
                std::ostream& os = CityLog::getLog(data_->pCity)->getStream();
                os << "\nPop = " << data_->cityPopulation << " (working = " << data_->workingPopulation << ") "
                    << "Reqd = " << targetYield_ << ", actual = " << actualYield;
            }
#endif
            // actual food yield is below desired range
            if (targetYield_.valueBelow(actualYield))
            {
                //CityLog::getLog(data_->pCity)->getStream() << "\n before = " << actualYield << " target = " << targetYield_;
                boost::tie(actualYield, swapData) = optimiseOutputs_(valueAdaptor);
                if (targetYield_.valueBelow(actualYield)) optState = FailedInsufficientFood;

#ifdef ALTAI_DEBUG
                if (debug)
                {
                    CityLog::getLog(data_->pCity)->getStream() << " after = " << actualYield;
                
                    if (!swapData.empty()) CityLog::getLog(data_->pCity)->getStream() << "\nSwaps: ";
                    for (size_t i = 0, count = swapData.size(); i < count; ++i)
                    {
                        CityLog::getLog(data_->pCity)->getStream() << " (" << swapData[i].first.output << " instead of " << swapData[i].second.output << ") ";
                    }
                }
#endif
                juggle_(valueAdaptor, swapData, debug);
            }

            if (targetYield_.valueAbove(actualYield))
            {
#ifdef ALTAI_DEBUG
                if (debug)
                {
                    CityLog::getLog(data_->pCity)->getStream() << "\n before = " << actualYield << " target = " << targetYield_;
                }
#endif
                boost::tie(actualYield, swapData) = optimiseExcessFood_(valueAdaptor);
                if (targetYield_.valueBelow(actualYield))
                {
                    optState = FailedExcessFood;
                }

#ifdef ALTAI_DEBUG
                if (debug)
                {
                    CityLog::getLog(data_->pCity)->getStream() << " after = " << actualYield;

                    if (!swapData.empty()) CityLog::getLog(data_->pCity)->getStream() << "\nSwaps: ";
                    for (size_t i = 0, count = swapData.size(); i < count; ++i)
                    {
                        CityLog::getLog(data_->pCity)->getStream() << " (" << swapData[i].first.output << " instead of " << swapData[i].second.output << ") ";
                    }
                }
#endif
                juggle_(valueAdaptor, swapData, debug);
            }

            // todo handle case of going over, then under
#ifdef ALTAI_DEBUG
            if (debug)
            {
                std::ostream& os = CityLog::getLog(data_->pCity)->getStream();
                if (targetYield_.valueBelow(actualYield) || targetYield_.valueAbove(actualYield))
                {
                    os << "\nPop = " << data_->cityPopulation << " (working = " << data_->workingPopulation << ") "
                       << "Reqd = " << 100 * (data_->cityPopulation * foodPerPop_) << ", actual = " << actualYield
                       << ", target = " << targetYield_
                       << " achieved = " << actualYield << " weights = " << valueAdaptor.pred.outputWeights
                       << " swaps = " << swapData.size();

                    for (size_t i = 0, count = swapData.size(); i < count; ++i)
                    {
                        os << " (" << swapData[i].first.output << ", " << swapData[i].second.output << ") ";
                    }
                    os << " (" << data_->getFood() << ", food = " << data_->currentFood << ")\n";
                    this->debug(os, true);
                }
            }
#endif
        }
        return optState;
    }

    template <class ValueAdaptor>
        std::pair<int, CityOptimiser::SwapData> CityOptimiser::juggle_(ValueAdaptor valueAdaptor, const CityOptimiser::SwapData& swapData, bool debug)
    {
        // is there a pair of unselected plots which taken together are better than a pair of selected plots
        SwapData juggledSwapData;
        
        if (data_->plotOutputs.size() < 4 || data_->cityPopulation < 2)
        {
            return std::make_pair(0, juggledSwapData);
        }

        // find worst worked plots not in swap data according to pred
        data_->plotOutputs.sort(valueAdaptor);

        SpecialFoodOutputOrderF<ValueAdaptor::Pred> foodP(valueAdaptor.pred);

        while (true)
        {
            
            PlotDataIter endIter = data_->plotOutputs.end();
            std::pair<PlotDataIter, PlotDataIter> worstPlots(endIter, endIter);
            std::pair<PlotDataIter, PlotDataIter> bestUnworkedPlots(endIter, endIter);

            for (PlotDataIter iter(data_->plotOutputs.begin()); iter != endIter; ++iter)
            {
                if (iter->isWorked)
                {
                    if (!PlotWasSwappedIn(swapData, *iter))
                    {
                        if (worstPlots.first != endIter)
                        {
                            worstPlots.second = worstPlots.first;
                        }
                        worstPlots.first = iter;
                    }
                }
                else if (bestUnworkedPlots.first == endIter)
                {
                    bestUnworkedPlots.first = iter;
                }
                else if (bestUnworkedPlots.second == endIter || foodP(*iter, *bestUnworkedPlots.second))
                {
                    bestUnworkedPlots.second = iter;
                }
            }

            if (worstPlots.first != endIter && worstPlots.second != endIter && bestUnworkedPlots.first != endIter && bestUnworkedPlots.second != endIter)
            {
                TotalOutput worstPlotsOutput = worstPlots.first->actualOutput + worstPlots.second->actualOutput;
                TotalOutput bestPlotsOutput = bestUnworkedPlots.first->actualOutput + bestUnworkedPlots.second->actualOutput;

                if (worstPlotsOutput[OUTPUT_FOOD] <= bestPlotsOutput[OUTPUT_FOOD] && valueAdaptor.pred(bestPlotsOutput, worstPlotsOutput))
                {
                    worstPlots.first->isWorked = false;
                    worstPlots.second->isWorked = false;

                    bestUnworkedPlots.first->isWorked = true;
                    bestUnworkedPlots.second->isWorked = true;
    
                    juggledSwapData.push_back(std::make_pair(*bestUnworkedPlots.first, *worstPlots.first));
                    juggledSwapData.push_back(std::make_pair(*bestUnworkedPlots.second, *worstPlots.second));
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }

#ifdef ALTAI_DEBUG
        if (debug)
        {
            if (!juggledSwapData.empty()) CityLog::getLog(data_->pCity)->getStream() << "\nSwaps (juggled): ";
            for (size_t i = 0, count = juggledSwapData.size(); i < count; ++i)
            {
                CityLog::getLog(data_->pCity)->getStream() << " (" << juggledSwapData[i].first.output << " instead of " << juggledSwapData[i].second.output << ") ";
            }
        }
#endif
        return std::make_pair(0, juggledSwapData);
    }

    template <class ValueAdaptor>
        std::pair<int, CityOptimiser::SwapData> CityOptimiser::optimiseOutputs_(ValueAdaptor valueAdaptor)
    {
        typedef std::list<PlotData>::reverse_iterator PlotDataRIter;

        bool ignoreDeficitFoodDiffs = true;

        data_->plotOutputs.sort(SpecialFoodOutputOrderF<ValueAdaptor::Pred>(valueAdaptor.pred));

        SwapData swapData;
        int actualYield = data_->getFood();
        const int plotCount = data_->plotOutputs.size();
        const int maxSwapCount = data_->plotOutputs.size() - data_->workingPopulation;
        int swapCount = 1;

        // try to get actual yield up into our target range by repeatedly swapping worst used value with best unused one
        while (targetYield_.valueBelow(actualYield) && swapCount < maxSwapCount)
        {
            PlotDataRIter worstWorkedPlotIter = std::find_if(data_->plotOutputs.rbegin(), data_->plotOutputs.rend(), IsWorked());
            PlotDataIter bestUnworkedPlotIter = std::find_if(data_->plotOutputs.begin(), data_->plotOutputs.end(), Unworked());
            if (worstWorkedPlotIter == data_->plotOutputs.rend() || bestUnworkedPlotIter == data_->plotOutputs.end())
            {
                break;
            }

            if (bestUnworkedPlotIter->actualOutput[OUTPUT_FOOD] > worstWorkedPlotIter->actualOutput[OUTPUT_FOOD])
            {
                actualYield = actualYield - worstWorkedPlotIter->actualOutput[OUTPUT_FOOD] + bestUnworkedPlotIter->actualOutput[OUTPUT_FOOD];
                
                bestUnworkedPlotIter->isWorked = true;
                worstWorkedPlotIter->isWorked = false;

                swapData.push_back(std::make_pair(*bestUnworkedPlotIter, *worstWorkedPlotIter));
                ++swapCount;
            }
            else
            {
                if (ignoreDeficitFoodDiffs)
                {
                    ignoreDeficitFoodDiffs = false;
                    data_->plotOutputs.sort(SpecialFoodOutputOrderF<ValueAdaptor::Pred>(valueAdaptor.pred, false));
                    swapCount = 0;
                }
                else
                {
                    break;
                }
            }
        }

        return std::make_pair(actualYield, swapData);
    }

    template <class ValueAdaptor>
        std::pair<int, CityOptimiser::SwapData> CityOptimiser::optimiseExcessFood_(ValueAdaptor valueAdaptor)
    {
        typedef std::list<PlotData>::reverse_iterator PlotDataRIter;

        bool ignoreDeficitFoodDiffs = true;

        data_->plotOutputs.sort(ReverseSpecialFoodOutputOrderF<ValueAdaptor::Pred>(valueAdaptor.pred));

        SwapData swapData;
        int actualYield = data_->getFood();
        const int plotCount = data_->plotOutputs.size();
        const int maxSwapCount = data_->plotOutputs.size() - data_->workingPopulation;
        int swapCount = 1;

        // try to get actual yield down into our target range by repeatedly swapping worst (i.e. most food) used value with best unused one
        while (targetYield_.valueAbove(actualYield) && swapCount < maxSwapCount)
        {
            PlotDataRIter worstWorkedPlotIter = std::find_if(data_->plotOutputs.rbegin(), data_->plotOutputs.rend(), IsWorked());

            PlotDataIter bestUnworkedPlotIter = data_->plotOutputs.begin(), endIter = data_->plotOutputs.end();
            // e.g. plot we are removing has food yield 6(00) and we are 4(00) over our target (targetYield_.upper), want min yield on replacement plot of 200
            int minFoodToKeep = std::max<int>(0, worstWorkedPlotIter->output[YIELD_FOOD] - (actualYield - targetYield_.upper));
            //while (!bestUnworkedPlotIter->isWorked && bestUnworkedPlotIter->output[YIELD_FOOD] < minFoodToKeep && ++bestUnworkedPlotIter != endIter);
            while (bestUnworkedPlotIter != endIter)
            {
                if (!bestUnworkedPlotIter->isWorked && bestUnworkedPlotIter->output[YIELD_FOOD] >= minFoodToKeep)
                {
                    break;
                }
                ++bestUnworkedPlotIter;
            }

            if (worstWorkedPlotIter == data_->plotOutputs.rend() || bestUnworkedPlotIter == endIter)
            {
                break;
            }

            if (bestUnworkedPlotIter->actualOutput[OUTPUT_FOOD] < worstWorkedPlotIter->actualOutput[OUTPUT_FOOD])
            {
                actualYield = actualYield - worstWorkedPlotIter->actualOutput[OUTPUT_FOOD] + bestUnworkedPlotIter->actualOutput[OUTPUT_FOOD];
                
                bestUnworkedPlotIter->isWorked = true;
                worstWorkedPlotIter->isWorked = false;

                swapData.push_back(std::make_pair(*bestUnworkedPlotIter, *worstWorkedPlotIter));
                ++swapCount;
            }
            else
            {
                break;
            }
        }

        return std::make_pair(actualYield, swapData);
    }

    void CityOptimiser::debug(std::ostream& os, bool printAllPlots) const
    {
#ifdef ALTAI_DEBUG
        if (printAllPlots)
        {
            int i = 0;
            os << "\n (home plot): " << data_->cityPlotOutput.output << " ";
            for (PlotDataConstIter iter(data_->plotOutputs.begin()), endIter(data_->plotOutputs.end()); iter != endIter; ++iter)
            {
                if (i++ > 0) os << ", ";
                os << iter->output;
            }
            os << "\n";
        }

        os << "\n Chosen: ";
        TotalOutput totalOutput;
        int j = 0;
        for (PlotDataConstIter iter(data_->plotOutputs.begin()), endIter(data_->plotOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                if (j++ > 0) os << ", ";
                os << iter->output << " " << iter->coords;
                totalOutput += iter->output;
            }
        }

        if (data_->specialistHelper->getTotalFreeSpecialistSlotCount() > 0)
        {
            os << "\n Free Specs: ";
            for (PlotDataConstIter iter(data_->freeSpecOutputs.begin()), endIter(data_->freeSpecOutputs.end()); iter != endIter; ++iter)
            {
                if (iter->isWorked)
                {
                    if (j++ > 0) os << ", ";
                    os << iter->output;
                    totalOutput += iter->output;
                }
            }
        }

        os << "\n Total = " << totalOutput << "\n";
#endif
    }

    CityOptimiser::GrowthType CityOptimiser::getGrowthType() const
    {
        GrowthType growthType = Not_Set;

        int requiredYield = 100 * data_->cityPopulation * foodPerPop_ + data_->getLostFood();
        int angryPopulation = data_->angryPopulation();
        int happyPopulation = data_->happyPopulation();
        if (angryPopulation >= happyPopulation)  // unhappy or about to be
        {
            growthType = FlatGrowth;
        }
        else if (angryPopulation + 1 == happyPopulation)  // close to unhappiness
        {
            growthType = MinorGrowth;
        }
        else
        {
            growthType = MajorGrowth;
        }

        return growthType;
    }

    Range CityOptimiser::calcTargetYieldSurplus(GrowthType growthType) const
    {
        int requiredYield = 100 * data_->cityPopulation * foodPerPop_ + data_->getLostFood();
        const int angryPop = data_->happyHelper->angryPopulation();

        if (growthType == Not_Set)
        {
            growthType = getGrowthType();
        }

        // bump up target if we have a lot of food (maxOutputs_ will be zero when calibrating - so this won't be triggered)
        if (growthType == MajorGrowth)
        {
            if (maxOutputs_.first[OUTPUT_FOOD] > requiredYield + 200 * foodPerPop_)
            {
                requiredYield += (300 * foodPerPop_) / 2;
            }
            else if (maxOutputs_.first[OUTPUT_FOOD] > requiredYield)
            {
                requiredYield = (maxOutputs_.first[OUTPUT_FOOD] + requiredYield) / 2;
            }
        }
        else if (growthType == MinorGrowth)
        {
            if (maxOutputs_.first[OUTPUT_FOOD] >= requiredYield + 200 * foodPerPop_)
            {
                requiredYield += 200 * foodPerPop_;
            }
        }

        switch (growthType)
        {
        case MajorStarve:
        case MinorStarve:
            return Range(requiredYield, Range::UpperBound);
        case FlatGrowth:
            return Range(requiredYield, requiredYield);
        case MinorGrowth:
        case MajorGrowth:
            return Range(requiredYield, Range::LowerBound);
        default:
            return Range(requiredYield, requiredYield);
        }
    }

    void CityOptimiser::removeSpecialistSlot_(SpecialistTypes specialistType)
    {
        PlotDataIter iter(data_->plotOutputs.begin()), endIter(data_->plotOutputs.end());
        while (iter != endIter)
        {
            if (!iter->isActualPlot() && (SpecialistTypes)iter->coords.iY == specialistType)
            {
                data_->unworkablePlots.splice(data_->unworkablePlots.begin(), data_->plotOutputs, iter);
                break;
            }
            ++iter;
        }
    }

    void CityOptimiser::reclaimSpecialistSlots_()
    {
        PlotDataIter iter(data_->unworkablePlots.begin()), endIter(data_->unworkablePlots.end());
        while (iter != endIter)
        {
            if (!iter->isActualPlot())
            {
                PlotDataIter removeIter(iter++);
                data_->plotOutputs.splice(data_->plotOutputs.begin(), data_->unworkablePlots, removeIter);
            }
            else
            {
                ++iter;
            }
        }
    }

    std::string CityOptimiser::getGrowthTypeString(GrowthType growthType)
    {
        switch (growthType)
        {
        case MajorStarve:
            return "MajorStarve";
        case MinorStarve:
            return "MinorStarve";
        case FlatGrowth:
            return "FlatGrowth";
        case MinorGrowth:
            return "MinorGrowth";
        case MajorGrowth:
            return "MajorGrowth";
        case Not_Set:
            return "Not_Set";
        default:
            return "Unknown/Invalid Growthtype?";
        }
    }

    DotMapOptimiser::DotMapOptimiser(DotMapItem& dotMapItem, PlayerTypes playerType)
        : dotMapItem_(dotMapItem), foodPerPop_(gGlobals.getFOOD_CONSUMPTION_PER_POPULATION()), playerType_(playerType)
    {
        weights_ = OutputUtils<PlotYield>::getDefaultWeights();
        ignoreFoodWeights_ = makeYieldW(0, 2, 1);
    }

    DotMapOptimiser::DotMapOptimiser(DotMapItem& dotMapItem, YieldWeights weights, YieldWeights ignoreFoodWeights)
        : dotMapItem_(dotMapItem), foodPerPop_(gGlobals.getFOOD_CONSUMPTION_PER_POPULATION()),
          weights_(weights), ignoreFoodWeights_(ignoreFoodWeights), playerType_(NO_PLAYER)
    {
    }

    void DotMapOptimiser::optimise(std::vector<YieldWeights> ignoreFoodWeights)
    {
        if (ignoreFoodWeights.empty())
        {
            ignoreFoodWeights = std::vector<YieldWeights>(1, ignoreFoodWeights_);
        }

        int targetYield = foodPerPop_ * dotMapItem_.plotData.size();
        DotMapItem::PlotDataIter endPlotIter(dotMapItem_.plotData.end());

        PlotYield plotYield = dotMapItem_.cityPlotYield;
        std::set<XYCoords> donePlots;

        YieldValueFunctor valueF(weights_);
        MixedOutputOrderFunctor<PlotYield> mixedF = makeMixedF(weights_); // OUTPUT_FOOD is set as priority type

        for (DotMapItem::PlotDataIter plotIter(dotMapItem_.plotData.begin()); plotIter != endPlotIter; ++plotIter)
        {
            if (plotIter->possibleImprovements.size() < 2)
            {
                plotIter->workedImprovement = plotIter->possibleImprovements.empty() ? -1 : 0;
                donePlots.insert(plotIter->coords);
            }
            else
            {
                int bestImprovementIndex = -1;
                for (size_t i = 0, count = plotIter->possibleImprovements.size(); i < count; ++i)
                {
                    if (plotIter->bonusType != NO_BONUS && plotIter->possibleImprovements[i].second != NO_IMPROVEMENT &&
                        gGlobals.getImprovementInfo(plotIter->possibleImprovements[i].second).isImprovementBonusMakesValid(plotIter->bonusType))
                    {
                        bestImprovementIndex = i;
                        break;
                    }

                    if (valueF(plotIter->getPlotYield(i), plotIter->getPlotYield(bestImprovementIndex)))
                    {
                        bestImprovementIndex = i;
                    }
                }
                FAssertMsg(bestImprovementIndex == -1 || bestImprovementIndex < plotIter->possibleImprovements.size(), "Invalid improvement selected")
                plotIter->workedImprovement = bestImprovementIndex;
            }

            plotYield += plotIter->getPlotYield();
        }

        while (plotYield[YIELD_FOOD] < targetYield)
        {
            // find first plot not tried yet
            DotMapItem::PlotDataIter worstPlotIter = dotMapItem_.plotData.begin();
            while (donePlots.find(worstPlotIter->coords) != donePlots.end() && worstPlotIter != endPlotIter)
            {
                ++worstPlotIter;
            }

            if (worstPlotIter == endPlotIter)
            {
                break;
            }

            int worstPlotValue = valueF(worstPlotIter->getPlotYield());

            // compare first plot not done with all subsequent ones not yet done to find worst
            DotMapItem::PlotDataIter plotIter(worstPlotIter);
            ++plotIter;
            for (;plotIter != endPlotIter; ++plotIter)
            {
                if (donePlots.find(plotIter->coords) != donePlots.end())
                {
                    continue;
                }

                int thisPlotValue = valueF(plotIter->getPlotYield());
                if (thisPlotValue < worstPlotValue)
                {
                    worstPlotValue = thisPlotValue;
                    worstPlotIter = plotIter;
                }
            }

            if (donePlots.find(worstPlotIter->coords) != donePlots.end())
            {
                break;
            }

            // find best (food) improvement for worst plot
            int bestImprovementIndex = worstPlotIter->workedImprovement;
            for (size_t i = 0, count = worstPlotIter->possibleImprovements.size(); i < count; ++i)
            {
                if (worstPlotIter->bonusType != NO_BONUS && worstPlotIter->possibleImprovements[i].second != NO_IMPROVEMENT &&
                    gGlobals.getImprovementInfo(worstPlotIter->possibleImprovements[i].second).isImprovementBonusMakesValid(worstPlotIter->bonusType))
                {
                    bestImprovementIndex = i;
                    break;
                }

                if (mixedF(worstPlotIter->getPlotYield(i), worstPlotIter->getPlotYield(bestImprovementIndex)))
                {
                    bestImprovementIndex = i;
                }
            }

            donePlots.insert(worstPlotIter->coords);  // done
            if (bestImprovementIndex != worstPlotIter->workedImprovement)
            {
                // update yield
                plotYield -= worstPlotIter->getPlotYield();
                FAssertMsg(bestImprovementIndex == -1 || bestImprovementIndex < worstPlotIter->possibleImprovements.size(), "Invalid improvement selected")
                worstPlotIter->workedImprovement = bestImprovementIndex;
                plotYield += worstPlotIter->getPlotYield();
            }
        }

        if (!ignoreFoodWeights.empty())
        {
            int ignoreFunctorCounter = 0;
            const int ignoreFunctorCount = ignoreFoodWeights.size();

            while (plotYield[YIELD_FOOD] > targetYield)
            {
                DotMapItem::PlotDataIter plotIter = dotMapItem_.plotData.begin();
                // find plot which hasn't been swapped and doesn't produce a large food surplus
                while (plotIter != endPlotIter && (donePlots.find(plotIter->coords) != donePlots.end() || plotIter->bonusType != NO_BONUS))// || plotIter->getPlotYield()[YIELD_FOOD] > foodPerPop_))
                {
                    ++plotIter;
                }

                if (plotIter == endPlotIter)
                {
                    break;
                }

                // find best improvement for this plot, ignoring food
                int bestImprovementIndex = plotIter->workedImprovement;
                for (size_t i = 0, count = plotIter->possibleImprovements.size(); i < count; ++i)
                {
                    if (plotIter->bonusType != NO_BONUS && plotIter->possibleImprovements[i].second != NO_IMPROVEMENT &&
                        gGlobals.getImprovementInfo(plotIter->possibleImprovements[i].second).isImprovementBonusMakesValid(plotIter->bonusType))
                    {
                        bestImprovementIndex = i;
                        break;
                    }

                    // this one is better or...
                    YieldValueFunctor ignoreFoodF(ignoreFoodWeights[ignoreFunctorCounter % ignoreFunctorCount]);
                    if (ignoreFoodF(plotIter->getPlotYield(i), plotIter->getPlotYield(bestImprovementIndex)))
                        //|| plotIter->workedImprovement == i && // already chosen this one and...
                        //// value ignoring food is equal to the best one found so far (so don't change the chosen improvement)
                        //ignoreFoodF(plotIter->getPlotYield(i)) == ignoreFoodF(plotIter->getPlotYield(bestImprovementIndex)))
                    {
                        bestImprovementIndex = i;
                    }
                }
                ++ignoreFunctorCounter;

                donePlots.insert(plotIter->coords);  // done
                if (bestImprovementIndex != plotIter->workedImprovement)
                {
                    // update yield
                    plotYield -= plotIter->getPlotYield();
                    FAssertMsg(bestImprovementIndex == -1 || bestImprovementIndex < plotIter->possibleImprovements.size(), "Invalid improvement selected")
                    plotIter->workedImprovement = bestImprovementIndex;
                    plotYield += plotIter->getPlotYield();
                }
            }
        }
    }

    void DotMapOptimiser::optimise(const std::vector<YieldTypes>& yieldTypes, int availablePopulation)
    {
#ifdef ALTAI_DEBUG
        const CvCity* pCity = gGlobals.getMap().plot(dotMapItem_.coords.iX, dotMapItem_.coords.iY)->getPlotCity();
        std::ostream& os = pCity ? CityLog::getLog(pCity)->getStream() : CivLog::getLog(CvPlayerAI::getPlayer(playerType_))->getStream();
#endif

        const int targetYield = foodPerPop_ * availablePopulation;
        DotMapItem::PlotDataIter endPlotIter(dotMapItem_.plotData.end());

#ifdef ALTAI_DEBUG
        os << "\n\nPlot: " << dotMapItem_.coords << " target Yield  = " << targetYield;
#endif
        PlotYield plotYield = dotMapItem_.cityPlotYield;
        std::set<XYCoords> donePlots;

        std::vector<MixedOutputOrderFunctor<PlotYield> > valueFunctors;
        std::vector<std::list<DotMapItem::SelectedImprovement> > bestImprovementSelections;

        for (DotMapItem::PlotDataIter iter(dotMapItem_.plotData.begin()), endIter(dotMapItem_.plotData.end()); iter != endIter; ++iter)
        {
            iter->isSelected = false;
        }

        std::vector<std::set<XYCoords> > goodImprovements(1 + yieldTypes.size());

        for (size_t i = 0, count = yieldTypes.size(); i < count; ++i)
        {
            valueFunctors.push_back(MixedOutputOrderFunctor<PlotYield>(makeYieldP(yieldTypes[i]), makeYieldW(2, 1, 1)));

            std::list<DotMapItem::SelectedImprovement> selectedImprovements = dotMapItem_.getBestImprovements(valueFunctors[i]);
            bestImprovementSelections.push_back(selectedImprovements);

            for (std::list<DotMapItem::SelectedImprovement>::const_iterator ci(selectedImprovements.begin()), ciEnd(selectedImprovements.end()); ci != ciEnd; ++ci)
            {
                PlotYield delta = ci->plotYield - dotMapItem_.getOutput(ci->coords, 0);
                if (!ci->improvementMakesBonusValid && delta[YIELD_FOOD] >= 0 && delta[yieldTypes[i]] > 0)
                {
                    goodImprovements[i].insert(ci->coords);
                }
            }
        }

        // add food improvements
        std::list<DotMapItem::SelectedImprovement> selectedImprovements = dotMapItem_.getBestImprovements(MixedOutputOrderFunctor<PlotYield>(makeYieldP(YIELD_FOOD), makeYieldW(1, 1, 1)));
        bestImprovementSelections.push_back(selectedImprovements);
        for (std::list<DotMapItem::SelectedImprovement>::const_iterator ci(selectedImprovements.begin()), ciEnd(selectedImprovements.end()); ci != ciEnd; ++ci)
        {
            PlotYield delta = ci->plotYield - dotMapItem_.getOutput(ci->coords, 0);
            if (!ci->improvementMakesBonusValid && delta[YIELD_FOOD] > 0)
            {
                goodImprovements[yieldTypes.size()].insert(ci->coords);
            }
        }

        std::pair<YieldTypes, int> fewestGoodImprovementsInfo(NO_YIELD, MAX_INT);
        int worstYieldTypeindex = 0;
        for (size_t i = 0, count = yieldTypes.size(); i < count; ++i)
        {
            int goodImprovementCount = goodImprovements[i].size();
            if (goodImprovementCount < fewestGoodImprovementsInfo.second)
            {
                fewestGoodImprovementsInfo = std::make_pair(yieldTypes[i], goodImprovementCount);
                worstYieldTypeindex = i;
            }
        }

        if (fewestGoodImprovementsInfo.first != NO_YIELD)
        {
            for (size_t i = 0, count = yieldTypes.size(); i < count; ++i)
            {
                if (yieldTypes[i] != fewestGoodImprovementsInfo.first)
                {
                    const int removeCount = std::min<int>(goodImprovements[i].size() - fewestGoodImprovementsInfo.second, fewestGoodImprovementsInfo.second);
#ifdef ALTAI_DEBUG
                    os << "\nRemoving: " << removeCount << " improvements for type: " << gGlobals.getYieldInfo(yieldTypes[i]).getType()
                       << " and keeping type: " << gGlobals.getYieldInfo(yieldTypes[worstYieldTypeindex]).getType();
#endif
                    int removedCount = 0;
                    std::list<DotMapItem::SelectedImprovement>::iterator iter(bestImprovementSelections[i].begin()), iterEnd(bestImprovementSelections[i].end());
                    while (iter != iterEnd)
                    {
                        if (goodImprovements[worstYieldTypeindex].find(iter->coords) != goodImprovements[worstYieldTypeindex].end())
                        {
#ifdef ALTAI_DEBUG
                            /*os << "\nErasing improvement at: " << iter->coords << " with yield: " << iter->plotYield;

                            DotMapItem::PlotDataIter plotIter(dotMapItem_.plotData.find(DotMapItem::PlotData(iter->coords)));
                            ImprovementTypes improvementType = plotIter->possibleImprovements[iter->improvementIndex].second;
                            if (improvementType != NO_IMPROVEMENT)
                            {
                                os << " imp = " << gGlobals.getImprovementInfo(improvementType).getType();
                            }*/
#endif
                            bestImprovementSelections[i].erase(iter++);
                            ++removedCount;
                            
                            if (removedCount == removeCount)
                            {
                                break;
                            }
                        }
                        else
                        {
                            ++iter;
                        }
                    }
                }
            }
        }
#ifdef ALTAI_DEBUG
        //{   // debug
        //    for (size_t i = 0, count = bestImprovementSelections.size(); i < count; ++i)
        //    {
        //        for (std::list<DotMapItem::SelectedImprovement>::const_iterator ci(bestImprovementSelections[i].begin()), ciEnd(bestImprovementSelections[i].end()); ci != ciEnd; ++ci)
        //        {
        //            DotMapItem::PlotDataIter plotIter(dotMapItem_.plotData.find(DotMapItem::PlotData(ci->coords)));
        //            os << "\n" << ci->coords << " " << ci->plotYield << " ";
        //            ImprovementTypes improvementType = plotIter->getWorkedImprovement();
        //            if (improvementType != NO_IMPROVEMENT)
        //            {
        //                os << gGlobals.getImprovementInfo(improvementType).getType();
        //            }
        //            else
        //            {
        //                os << " (no improvements)";
        //            }
        //        }
        //        os << "\n";
        //    }
        //}
#endif
        std::vector<std::pair<std::list<DotMapItem::SelectedImprovement>::iterator, std::list<DotMapItem::SelectedImprovement>::iterator> > iters;
        for (size_t i = 0, count = bestImprovementSelections.size(); i < count; ++i)
        {
            iters.push_back(std::make_pair(bestImprovementSelections[i].begin(), bestImprovementSelections[i].end()));
        }

        int selectedPlotCount = 0;

        bool done = false;
        while (!done)
        {
            done = true;
            DotMapItem::PlotDataIter plotIter(endPlotIter);

            // don't do YIELD_FOOD opt yet
            for (size_t i = 0, count = bestImprovementSelections.size() - 1; i < count; ++i)
            {
                // loop over ordered plot iterators for this set of improvement selections to find first plot not yet done
                while (iters[i].first != iters[i].second)
                {
                    plotIter = dotMapItem_.plotData.find(DotMapItem::PlotData(iters[i].first->coords));
                    if (donePlots.find(iters[i].first->coords) == donePlots.end())
                    {
                        break;  // this plot not done yet
                    }
                    ++iters[i].first;
                }

                // if plot OK, set improvement, mark as selected and add to done list
                if (iters[i].first != iters[i].second && donePlots.find(iters[i].first->coords) == donePlots.end() && plotIter != endPlotIter)
                {
                    if (!plotIter->isPinned)
                    {
                        plotIter->workedImprovement = iters[i].first->improvementIndex;
                        plotIter->improvementMakesBonusValid = iters[i].first->improvementMakesBonusValid;
                    }
                    if (selectedPlotCount < availablePopulation)
                    {
                        plotIter->isSelected = true;
                        plotYield += iters[i].first->plotYield;
                    }
                    
                    donePlots.insert(iters[i].first->coords);
                    ++iters[i].first;
                    ++selectedPlotCount;
                }

                done = done && (iters[i].first == iters[i].second);
            }
        }
#ifdef ALTAI_DEBUG
        //{   // debug
        //    for (size_t i = 0, count = bestImprovementSelections.size(); i < count; ++i)
        //    {
        //        for (std::list<DotMapItem::SelectedImprovement>::const_iterator ci(bestImprovementSelections[i].begin()), ciEnd(bestImprovementSelections[i].end()); ci != ciEnd; ++ci)
        //        {
        //            DotMapItem::PlotDataIter plotIter(dotMapItem_.plotData.find(DotMapItem::PlotData(ci->coords)));
        //            os << "\n" << ci->coords << " " << ci->plotYield << " ";
        //            if (ci->improvementIndex != -1 && plotIter->possibleImprovements[ci->improvementIndex].second != NO_IMPROVEMENT)
        //            {
        //                os << gGlobals.getImprovementInfo(plotIter->possibleImprovements[ci->improvementIndex].second).getType();
        //            }
        //            else
        //            {
        //                os << " (no improvements)";
        //            }
        //            if (!plotIter->isSelected)
        //            {
        //                os << " (selected = false)";
        //            }
        //            if (plotIter->isPinned)
        //            {
        //                os << " (pinned = true)";
        //            }
        //        }
        //        os << "\nGood improvement count = " << goodImprovements[i].size() << ": ";
        //        for (std::set<XYCoords>::const_iterator ci(goodImprovements[i].begin()), ciEnd(goodImprovements[i].end()); ci != ciEnd; ++ci)
        //        {
        //            os << *ci << ", ";
        //        }
        //        os << "\n\n";
        //    }
        //}
#endif
        const size_t foodIndex = bestImprovementSelections.size() - 1;
        // select while less than target, still got improvements to switch...
        while (plotYield[YIELD_FOOD] < targetYield && iters[foodIndex].first != iters[foodIndex].second)
        {
            DotMapItem::PlotDataIter plotIter(dotMapItem_.plotData.find(DotMapItem::PlotData(iters[foodIndex].first->coords)));
            if (!plotIter->isPinned && (plotIter->workedImprovement != iters[foodIndex].first->improvementIndex || !plotIter->isSelected))
            {
                //  ...and improvement can at least break even (don't switch workshops to cottages on plains for example)
                if (plotIter->getPlotYield(iters[foodIndex].first->improvementIndex)[YIELD_FOOD] < foodPerPop_)
                {
                    //{
                    //    os << "\nExiting food loop, plot: " << plotIter->coords << " with yield: " << plotIter->getPlotYield()
                    //       << " best food yield: " << plotIter->getPlotYield(iters[foodIndex].first->improvementIndex);
                    //}
                    break;
                }
                if (!plotIter->isSelected)  // need to unselect another plot -- choose worst food plot
                {
                    DotMapItem::PlotDataIter worstFoodIter = dotMapItem_.plotData.end();
                    int worstFoodYield = MAX_INT;

                    for (DotMapItem::PlotDataIter worstFoodPlotIter(dotMapItem_.plotData.begin()), worstFoodPlotEndIter(dotMapItem_.plotData.end()); worstFoodPlotIter != worstFoodPlotEndIter; ++worstFoodPlotIter)
                    {
                        if (worstFoodPlotIter->isSelected)
                        {
                            int thisPlotYield = worstFoodPlotIter->getPlotYield()[YIELD_FOOD];
                            if (thisPlotYield < worstFoodYield)
                            {
                                worstFoodYield = worstFoodPlotIter->getPlotYield()[YIELD_FOOD];
                                worstFoodIter = worstFoodPlotIter;
                            }
                        }
                    }

                    if (worstFoodIter != dotMapItem_.plotData.end())
                    {
#ifdef ALTAI_DEBUG
                        //{
                        //    os << "\nUnselecting plot: " << worstFoodIter->coords << " with yield: " << worstFoodIter->getPlotYield();
                        //}
#endif
                        worstFoodIter->isSelected = false;
                        plotYield -= worstFoodIter->getPlotYield();

                        plotIter->workedImprovement = iters[foodIndex].first->improvementIndex;
                        plotYield += plotIter->getPlotYield();
                        plotIter->isSelected = true;
#ifdef ALTAI_DEBUG
                        //{
                        //    os << "\nSelected plot: " << plotIter->coords << " with yield: " << plotIter->getPlotYield();
                        //}
#endif
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
#ifdef ALTAI_DEBUG
                    /*{
                        os << "\nSwapping selected improvement for plot: " << plotIter->coords << " from yield: " << plotIter->getPlotYield()
                           << " to yield: " << plotIter->getPlotYield(iters[foodIndex].first->improvementIndex);
                    }*/
#endif
                    plotYield -= plotIter->getPlotYield();
                    plotIter->workedImprovement = iters[foodIndex].first->improvementIndex;
                    plotYield += plotIter->getPlotYield();
                }
            }
            ++iters[foodIndex].first;
        }
    }

    const DotMapItem& DotMapOptimiser::getDotMapItem() const
    {
        return dotMapItem_;
    }
}