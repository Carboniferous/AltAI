#include "AltAI.h"

#include "./city_data.h"
#include "./iters.h"
#include "./plot_info_visitors.h"
#include "./spec_info_visitors.h"
#include "./city_simulator.h"
#include "./game.h"
#include "./player.h"
#include "./unit.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./maintenance_helper.h"
#include "./civ_helper.h"
#include "./happy_helper.h"
#include "./health_helper.h"
#include "./hurry_helper.h"
#include "./religion_helper.h"
#include "./culture_helper.h"
#include "./building_helper.h"
#include "./modifiers_helper.h"
#include "./trade_route_helper.h"
#include "./unit_helper.h"
#include "./vote_helper.h"
#include "./corporation_helper.h"
#include "./bonus_helper.h"
#include "./specialist_helper.h"
#include "./helper_fns.h"
#include "./gamedata_analysis.h"
#include "./city_improvements.h"
#include "./error_log.h"
#include "./civ_log.h"

// todo - specialist helper pCity->totalFreeSpecialists(), pCity->getMaxSpecialistCount
// bonushelper - pCity->hasBonus()
// goal is this class should not keep hold of its CvCity ptr, to allow it to run simulations in separate threads

namespace AltAI
{
    CityData::~CityData()
    {
        //ErrorLog::getLog(CvPlayerAI::getPlayer(owner_))->getStream() << "\ndelete CityData at: " << this;
    }

    CityData::CityData(const CityData& other)
    {
        cityPopulation_ = other.cityPopulation_, workingPopulation_ = other.workingPopulation_, happyCap_ = other.happyCap_;

        currentFood_ = other.currentFood_, storedFood_ = other.storedFood_, accumulatedProduction_ = other.accumulatedProduction_;
        growthThreshold_ = other.growthThreshold_, requiredProduction_ = other.requiredProduction_;
        foodKeptPercent_ = other.foodKeptPercent_;
        commerceYieldModifier_ = other.commerceYieldModifier_;

        specialConditions_ = other.specialConditions_;

        buildQueue_ = other.buildQueue_;

        pCity_ = other.pCity_;
        owner_ = other.owner_;
        coords_ = other.coords_;

        yieldModifier_ = other.yieldModifier_;
        commerceModifier_ = other.commerceModifier_, commercePercent_ = other.commercePercent_;

        plotOutputs_ = other.plotOutputs_, unworkablePlots_ = other.unworkablePlots_;
        freeSpecOutputs_ = other.freeSpecOutputs_;
        cityPlotOutput_ = other.cityPlotOutput_;
        cityGreatPersonOutput_ = other.cityGreatPersonOutput_;
        includeUnclaimedPlots_ = other.includeUnclaimedPlots_;
        goldenAgeTurns_ = other.goldenAgeTurns_;

        events_ = std::queue<CitySimulationEventPtr>();  // clear events queue

        // area and civ helpers are shared (for now)
        areaHelper_ = other.areaHelper_;
        civHelper_ = other.civHelper_;

        // copy state of unshared helpers
        bonusHelper_ = other.bonusHelper_->clone();
        buildingsHelper_ = other.buildingsHelper_->clone();
        corporationHelper_ = other.corporationHelper_->clone();
        cultureHelper_ = other.cultureHelper_->clone();
        happyHelper_ = other.happyHelper_->clone();
        healthHelper_ = other.healthHelper_->clone();
        hurryHelper_ = other.hurryHelper_->clone();
        maintenanceHelper_ = other.maintenanceHelper_->clone();
        modifiersHelper_ = other.modifiersHelper_->clone();
        religionHelper_ = other.religionHelper_->clone();
        specialistHelper_ = other.specialistHelper_->clone();
        tradeRouteHelper_ = other.tradeRouteHelper_->clone();
        unitHelper_ = other.unitHelper_->clone();
        voteHelper_ = other.voteHelper_->clone();

        bestMixedSpecialistTypes_ = other.bestMixedSpecialistTypes_;
    }

    CityData::CityData(const CvCity* pCity, bool includeUnclaimedPlots, int lookaheadDepth)
        : cityPopulation_(0), workingPopulation_(0), happyCap_(0), currentFood_(0), storedFood_(0),
          accumulatedProduction_(0), growthThreshold_(0), requiredProduction_(-1), foodKeptPercent_(0),
          commerceYieldModifier_(100), specialConditions_(None), pCity_(pCity), owner_(pCity->getOwner()),
          coords_(pCity->plot()->getCoords()), includeUnclaimedPlots_(includeUnclaimedPlots),
          goldenAgeTurns_(CvPlayerAI::getPlayer(pCity->getOwner()).getGoldenAgeTurns())
    {
        initHelpers_(pCity);
        init_(pCity);
        calcOutputsFromPlotData_(pCity, lookaheadDepth);
        calculateSpecialistOutput_();
    }

    CityData::CityData(const CvCity* pCity, const std::vector<PlotImprovementData>& improvements, bool includeUnclaimedPlots)
        : cityPopulation_(0), workingPopulation_(0), happyCap_(0), currentFood_(0), storedFood_(0),
          accumulatedProduction_(0), growthThreshold_(0), requiredProduction_(-1), foodKeptPercent_(0),
          commerceYieldModifier_(100), specialConditions_(None), pCity_(pCity), owner_(pCity_->getOwner()),
          coords_(pCity->plot()->getCoords()), includeUnclaimedPlots_(includeUnclaimedPlots),
          goldenAgeTurns_(CvPlayerAI::getPlayer(pCity->getOwner()).getGoldenAgeTurns())
    {
        initHelpers_(pCity);
        init_(pCity);
        calcOutputsFromPlannedImprovements_(improvements);
        calculateSpecialistOutput_();
    }

    void CityData::initHelpers_(const CvCity* pCity)
    {
        areaHelper_ = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAreaHelper(pCity->getArea());
        civHelper_ = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCivHelper();

        maintenanceHelper_ = MaintenanceHelperPtr(new MaintenanceHelper(pCity));

        bonusHelper_ = BonusHelperPtr(new BonusHelper(pCity));
        buildingsHelper_ = BuildingsHelperPtr(new BuildingsHelper(pCity));
        corporationHelper_ = CorporationHelperPtr(new CorporationHelper(pCity));
        cultureHelper_ = CultureHelperPtr(new CultureHelper(pCity));
        happyHelper_ = HappyHelperPtr(new HappyHelper(pCity));
        healthHelper_ = HealthHelperPtr(new HealthHelper(pCity));
        hurryHelper_ = HurryHelperPtr(new HurryHelper(pCity));
        modifiersHelper_ = ModifiersHelperPtr(new ModifiersHelper(pCity));
        religionHelper_ = ReligionHelperPtr(new ReligionHelper(pCity));
        specialistHelper_ = SpecialistHelperPtr(new SpecialistHelper(pCity));
        tradeRouteHelper_ = TradeRouteHelperPtr(new TradeRouteHelper(pCity));
        unitHelper_ = UnitHelperPtr(new UnitHelper(pCity));
        voteHelper_ = VoteHelperPtr(new VoteHelper(pCity));
    }

    void CityData::init_(const CvCity* pCity)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);

        currentFood_ = 100 * pCity->getFood();
        storedFood_ = 100 * pCity->getFoodKept();
        cityPopulation_ = pCity->getPopulation();
        workingPopulation_ = cityPopulation_ - getNonWorkingPopulation();
        happyCap_ = happyHelper_->happyPopulation(*this) - happyHelper_->angryPopulation(*this);

        growthThreshold_ = 100 * pCity->growthThreshold();
        foodKeptPercent_ = pCity->getMaxFoodKeptPercent();

        commerceYieldModifier_ = modifiersHelper_->getTotalYieldModifier(*this)[YIELD_COMMERCE];

        for (int commerceType = 0; commerceType < NUM_COMMERCE_TYPES; ++commerceType)
        {
            commercePercent_[commerceType] = player.getCommercePercent((CommerceTypes)commerceType);
        }
    }

    CityDataPtr CityData::clone() const
    {
        CityDataPtr copy = CityDataPtr(new CityData(*this));
        //ErrorLog::getLog(CvPlayerAI::getPlayer(owner_))->getStream() << "\nnew CityData at: " << copy.get();

        return copy;
    }

    void CityData::calcOutputsFromPlotData_(const CvCity* pCity, int lookaheadDepth)
    {
        boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getMapAnalysis();

        CityPlotIter iter(pCity);
        bool first = true;

        while (IterPlot pLoopPlot = iter())
        {
            if (pLoopPlot.valid())
            {
                if (first)
                {
                    calcCityOutput_();
                    first = false;
                }
                else
                {
                    const PlayerTypes plotOwner = pLoopPlot->getOwner();

                    if (plotOwner == pCity->getOwner() || plotOwner == NO_PLAYER)
                    {
                        XYCoords plotCoords(pLoopPlot->getCoords());
                        IDInfo assignedCity = pMapAnalysis->getSharedPlot(pCity->getIDInfo(), plotCoords);

                        if (assignedCity != pCity->getIDInfo()) // - can't take unassigned, as it might be grabbed by another city
                            // later in the turn, whilst we think we've got it! && assignedCity != IDInfo())
                        {
                            continue;
                        }

                        PlotYield plotYield(pLoopPlot->getYield());
                         // todo - check adding improvement later to plot will work (if there are any plots which have zero yield without improvement)
                        if (isEmpty(plotYield) || !canWork_(pLoopPlot, lookaheadDepth))
                        {
                            continue;  // no point in adding desert, or plots worked by other cities we own (add ones we don't own, in case we end up owning them)
                        }

                        initPlot_(pLoopPlot, plotYield, pLoopPlot->getImprovementType(), pLoopPlot->getBonusType(pCity_->getTeam()),
                            pLoopPlot->getFeatureType(), pLoopPlot->getRouteType());
                    }
                }
            }
        }
    }

    void CityData::calcOutputsFromPlannedImprovements_(const std::vector<PlotImprovementData>& plotImprovements)
    {
        CityPlotIter iter(pCity_);  // ok to use city here, as called from main ctor
        bool first = true;

        while (IterPlot pLoopPlot = iter())
        {
            if (pLoopPlot.valid())
            {
                if (first)
                {
                    calcCityOutput_();
                    first = false;
                }
                else
                {
                    XYCoords coords(pLoopPlot->getCoords());
                    ImprovementTypes improvementType = pLoopPlot->getImprovementType();
                    BonusTypes bonusType = pLoopPlot->getBonusType(pCity_->getTeam());
                    FeatureTypes featureType = pLoopPlot->getFeatureType();
                    PlotYield plotYield(pLoopPlot->getYield());
                    RouteTypes routeType = pLoopPlot->getRouteType();

                    std::vector<PlotImprovementData>::const_iterator improvementIter = 
                        std::find_if(plotImprovements.begin(), plotImprovements.end(), ImprovementCoordsFinder(coords));
                    if (improvementIter != plotImprovements.end())
                    {
                        // allow for improvements which have upgraded since they were built
                        if (improvementIter->state != PlotImprovementData::Built)
                        {
                            // todo - add route type to improvement manager data
                            improvementType = improvementIter->improvement;
                            plotYield = improvementIter->yield;
                            if (improvementIter->removedFeature != NO_FEATURE && pLoopPlot->getFeatureType() == improvementIter->removedFeature)
                            {
                                featureType = NO_FEATURE;
                            }
                        }
                    }
                    
                    if (isEmpty(plotYield))  // skip canWork_ check if setting up from planned improvements, as this is looking ahead
                    {
                        continue;  // no point in adding desert, or plots worked by other cities we own (add ones we don't own, in case we end up owning them)
                    }

                    if (goldenAgeTurns_ > 0)
                    {
                    }

                    initPlot_(pLoopPlot, plotYield, improvementType, bonusType, featureType, routeType);
                }
            }
        }
    }

    void CityData::addPlot(const CvPlot* pPlot)
    {
        // bypasses ownership/shared checks - for simulation/what-if scenarios
        // is not called when initialising from actual city data
        PlotYield plotYield(pPlot->getYield());

        initPlot_(pPlot, plotYield, pPlot->getImprovementType(), pPlot->getBonusType(pCity_->getTeam()), pPlot->getFeatureType(), pPlot->getRouteType());
    }

    void CityData::initPlot_(const CvPlot* pPlot, PlotYield plotYield, ImprovementTypes improvementType, BonusTypes bonusType, FeatureTypes featureType, RouteTypes routeType)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);
        const boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis = gGlobals.getGame().getAltAI()->getPlayer(player.getID())->getAnalysis();
        const boost::shared_ptr<MapAnalysis>& pMapAnalysis = pPlayerAnalysis->getMapAnalysis();
        const int upgradeRate = player.getImprovementUpgradeRate();
        const int timeHorizon = pPlayerAnalysis->getTimeHorizon();

        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

        TotalOutput plotOutput(makeOutput(plotYield, yieldModifier, commerceModifier, commercePercent_));

        PlotInfoPtr pPlotInfo(new PlotInfo(pPlot, owner_));

        PlotData plot(pPlotInfo, plotYield, Commerce(), plotOutput, GreatPersonOutput(), pPlot->getCoords(),
                      improvementType, bonusType, featureType, routeType, PlotData::CultureData(pPlot, owner_, pCity_));

        plot.controlled = plot.cultureData.ownerAndCultureTrumpFlag.first == owner_ || (plot.cultureData.ownerAndCultureTrumpFlag.first == NO_PLAYER && includeUnclaimedPlots_);
        plot.ableToWork = true;  //in the sense of having suitable techs - might not actually control the plot culturally (see previous line)

        if (plot.improvementType != NO_IMPROVEMENT)
        {
            ImprovementTypes upgradeImprovementType = (ImprovementTypes)gGlobals.getImprovementInfo(plot.improvementType).getImprovementUpgrade();

            if (upgradeImprovementType != NO_IMPROVEMENT)
            {
                const PlotInfo::PlotInfoNode& plotInfo = pMapAnalysis->getPlotInfoNode(pPlot);
                plot.upgradeData = PlotData::UpgradeData(timeHorizon,
                    getUpgradedImprovementsData(plotInfo, owner_, plot.improvementType, pPlot->getUpgradeTimeLeft(plot.improvementType, owner_), timeHorizon, upgradeRate));

                plot.output += plot.upgradeData.getExtraOutput(yieldModifier, commerceModifier, commercePercent_);
            }
        }

        plot.controlled ? plotOutputs_.push_back(plot) : unworkablePlots_.push_back(plot);
    }

    void CityData::calculateSpecialistOutput_()
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);
        int defaultSpecType = gGlobals.getDefineINT("DEFAULT_SPECIALIST");
        int freeSpecialistsCount = specialistHelper_->getTotalFreeSpecialistSlotCount();

        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);
        
        // TODO - deal with forced specialists? (if you can have specs which require other specs to be valid)
        for (int specialistType = 0, count = gGlobals.getNumSpecialistInfos(); specialistType < count; ++specialistType)
        {
            PlotYield yield(getSpecialistYield(player, (SpecialistTypes)specialistType));
            Commerce commerce(getSpecialistCommerce(player, (SpecialistTypes)specialistType));
            TotalOutput specialistOutput(makeOutput(yield, commerce, yieldModifier, commerceModifier, commercePercent_));

            PlotData specialistData(PlotInfoPtr(), yield, commerce, specialistOutput, 
                GameDataAnalysis::getSpecialistUnitTypeAndOutput((SpecialistTypes)specialistType, player.getID()), XYCoords(-1, specialistType));

            // default specialists are unlimited
            if (defaultSpecType == specialistType || player.isSpecialistValid((SpecialistTypes)specialistType))
            {
                // store specialist type in coords data so we know what we picked
                plotOutputs_.insert(plotOutputs_.end(), workingPopulation_, specialistData);

                if (freeSpecialistsCount > 0)
                {
                    freeSpecOutputs_.insert(freeSpecOutputs_.end(), freeSpecialistsCount, specialistData);
                }
            }
            else
            {
                int maxSpecialistCount = specialistHelper_->getMaxSpecialistCount((SpecialistTypes)specialistType);
                plotOutputs_.insert(plotOutputs_.end(), maxSpecialistCount, specialistData);

                if (freeSpecialistsCount > 0)
                {
                    freeSpecOutputs_.insert(freeSpecOutputs_.end(), std::min<int>(freeSpecialistsCount, maxSpecialistCount), specialistData);
                }
            }
        }

        recalcBestSpecialists_();
    }

    void CityData::recalcBestSpecialists_()
    {
        if (!getSpecialistHelper()->getAvailableSpecialistTypes().empty())
        {
            std::vector<OutputTypes> outputTypes;
            outputTypes.push_back(OUTPUT_PRODUCTION);
            outputTypes.push_back(OUTPUT_RESEARCH);
            outputTypes.push_back(OUTPUT_GOLD);

            TotalOutputWeights outputWeights = makeOutputW(1, 1, 1, 1, 1, 1);
            YieldModifier yieldModifier = modifiersHelper_->getTotalYieldModifier(*this);
            CommerceModifier commerceModifier = modifiersHelper_->getTotalCommerceModifier();

            bestMixedSpecialistTypes_ = AltAI::getBestSpecialists(*gGlobals.getGame().getAltAI()->getPlayer(owner_), yieldModifier, commerceModifier, 4, MixedWeightedOutputOrderFunctor<TotalOutput>(makeTotalOutputPriorities(outputTypes), outputWeights));
        }
    }

    std::vector<SpecialistTypes> CityData::getBestMixedSpecialistTypes() const
    {
        return bestMixedSpecialistTypes_;
    }

    void CityData::recalcOutputs()
    {
        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

        if (tradeRouteHelper_->needsRecalc())
        {
            tradeRouteHelper_->updateTradeRoutes();
        }

        calcCityOutput_();
        cityPlotOutput_.actualOutput = cityPlotOutput_.output = makeOutput(cityPlotOutput_.plotYield, cityPlotOutput_.commerce, yieldModifier, commerceModifier, commercePercent_);

        for (PlotDataListIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            iter->actualOutput = iter->output = makeOutput(iter->plotYield, iter->commerce, yieldModifier, commerceModifier, commercePercent_);
            if (!iter->upgradeData.upgrades.empty())
            {
                iter->output += iter->upgradeData.getExtraOutput(yieldModifier, commerceModifier, commercePercent_);
            }
        }

        for (PlotDataListIter iter(freeSpecOutputs_.begin()), endIter(freeSpecOutputs_.end()); iter != endIter; ++iter)
        {
            iter->actualOutput = iter->output = makeOutput(iter->plotYield, iter->commerce, yieldModifier, commerceModifier, commercePercent_);
        }

        // todo - add recalc check flag to spec helper and modifier helper
        recalcBestSpecialists_();
    }

    void CityData::setCommercePercent(CommerceModifier commercePercent)
    {
        commercePercent_ = commercePercent; 
        recalcOutputs();
    }

    void CityData::changeCommercePercent(CommerceModifier commercePercent)
    {
        commercePercent_ += commercePercent; 
        recalcOutputs();
    }

    void CityData::changeCommerceYieldModifier(int change)
    {
        commerceYieldModifier_ += change;
        recalcOutputs();
    }

    // first = pop change (+1, -1, 0), second = turns (MAX_INT if never)
    std::pair<int, int> CityData::getTurnsToPopChange() const
    {
        if (isFoodProductionBuildItem())
        {
            return std::make_pair(0, MAX_INT);
        }

        if (avoidGrowth_())
        {
            return std::make_pair(0, MAX_INT);
        }

        static const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        const int requiredFood = 100 * (cityPopulation_ * foodPerPop) + getLostFood();
        const int foodOutput = getFood();
        const int foodDelta = foodOutput - requiredFood;

        if (foodDelta > 0)
        {
            const int growthRate = (getGrowthThreshold() - getCurrentFood()) / foodDelta;
            const int growthDelta = (getGrowthThreshold() - getCurrentFood()) % foodDelta;
            return std::make_pair(1, growthRate + (growthDelta ? 1 : 0));
        }
        else if (foodDelta < 0)
        {
            const int starvationRate = -getCurrentFood() / foodDelta;
            const int starvationDelta = -getCurrentFood() % foodDelta;
            if (cityPopulation_ > 1)  // can't starve below 1 pop
            {
                return std::make_pair(-1, std::max<int>(1, starvationRate + (starvationDelta ? -1 : 0)));
            }
        }

        return std::make_pair(0, MAX_INT);
    }

    // current food, stored food - doesn't handle pop changes
    std::pair<int, int> CityData::getAccumulatedFood(int nTurns)
    {
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        const int requiredFood = 100 * (cityPopulation_ * foodPerPop) + getLostFood();
        const int foodOutput = getFood();
        const int foodDelta = foodOutput - requiredFood;

        int finalCurrentFood = currentFood_ + foodDelta * nTurns;

        int finalStoredFood = storedFood_ + foodDelta * nTurns;
        finalStoredFood = range(finalStoredFood, 0, (growthThreshold_ * foodKeptPercent_) / 100);

        return std::make_pair(finalCurrentFood, finalStoredFood);
    }

    void CityData::updateProduction(int nTurns)
    {
        TotalOutput totalOutput(getOutput());
        accumulatedProduction_ += getCurrentProduction_(totalOutput * nTurns);
    }

    void CityData::advanceTurn()
    {
        if (goldenAgeTurns_ > 0)
        {
            if (--goldenAgeTurns_ == 0)
            {
            }
        }

        // TODO create event when improvements upgrade
        doImprovementUpgrades(1);

        TotalOutput totalOutput(getOutput());

        // TODO handle builds that use food as well as production
        boost::tie(currentFood_, storedFood_) = getAccumulatedFood(1);

        if (currentFood_ >= growthThreshold_)
        {
            if (!avoidGrowth_())
            {
                changePopulation(1);
            }
            else
            {
                currentFood_ = growthThreshold_;
            }
        }
        else if (currentFood_ < 0)
        {
            currentFood_ = 0;
            changePopulation(-1);
        }

        updateProduction(1);

        cultureHelper_->advanceTurns(*this, 1);

        if (accumulatedProduction_ >= requiredProduction_ && !buildQueue_.empty())
        {
            // TODO handle overflow properly using logic in HurryHelper
            accumulatedProduction_ = 0;

            BuildQueueItem buildItem = buildQueue_.top();

            switch (buildItem.first)
            {
            case BuildingItem:
                {
                    BuildingTypes completedBuilding = (BuildingTypes)buildItem.second;
                    buildingsHelper_->changeNumRealBuildings(completedBuilding);
                    events_.push(CitySimulationEventPtr(new TestBuildingBuilt(completedBuilding)));
                    buildQueue_.pop();
                }
                break;
            case UnitItem:
                {
                    UnitTypes completedUnit = (UnitTypes)buildItem.second;
                    buildQueue_.pop();
                }                
                break;
            case ProjectItem:
                {
                    ProjectTypes completedProject = (ProjectTypes)buildItem.second;
                    buildQueue_.pop();
                }
                break;
            default: // and process - should never complete
                break;
            }

            requiredProduction_ = -1;
        }

        happyHelper_->advanceTurn(*this);
        healthHelper_->advanceTurn();
        
        changeWorkingPopulation();
        checkHappyCap();
    }

    void CityData::pushBuilding(BuildingTypes buildingType)
    {
        if (buildingType != NO_BUILDING)
        {
            buildQueue_.push(std::make_pair(BuildingItem, buildingType));
            requiredProduction_ = 100 * (pCity_->getProductionNeeded(buildingType) - pCity_->getBuildingProduction(buildingType));
        }
        else
        {
            requiredProduction_ = -1;
        }
    }

    void CityData::pushUnit(UnitTypes unitType)
    {
        if (unitType != NO_UNIT)
        {
            buildQueue_.push(std::make_pair(UnitItem, unitType));
            requiredProduction_ = 100 * (pCity_->getProductionNeeded(unitType) - pCity_->getUnitProduction(unitType));
        }
        else
        {
            requiredProduction_ = -1;
        }
    }

    void CityData::pushProcess(ProcessTypes processType)
    {
        if (processType != NO_PROCESS)
        {
            buildQueue_.push(std::make_pair(ProcessItem, processType));
        }
        requiredProduction_ = -1;
    }

    void CityData::clearBuildQueue()
    {
        while (!buildQueue_.empty())
        {
            buildQueue_.pop();
        }
    }

    bool CityData::isFoodProductionBuildItem() const
    {
        return !buildQueue_.empty() && buildQueue_.top().first == UnitItem && gGlobals.getUnitInfo((UnitTypes)buildQueue_.top().second).isFoodProduction();
    }

    void CityData::hurry(const HurryData& hurryData)
    {
        if (hurryData.hurryPopulation > 0)
        {
            changePopulation(-hurryData.hurryPopulation);
            hurryHelper_->updateAngryTimer();
        }
        
        accumulatedProduction_ = requiredProduction_ + hurryData.extraProduction;
    }

    int CityData::happyPopulation() const
    {
        return getHappyHelper()->happyPopulation(*this);
    }

    int CityData::angryPopulation() const
    {
        return getHappyHelper()->angryPopulation(*this);
    }

    std::pair<bool, HurryData> CityData::canHurry(HurryTypes hurryType) const
    {
        return hurryHelper_->canHurry(*this, hurryType);
    }

    int CityData::getNonWorkingPopulation() const
    {
        return range(happyHelper_->angryPopulation(*this) - happyHelper_->happyPopulation(*this), 0, cityPopulation_);
    }

    void CityData::changePopulation(int change)
    {
        cityPopulation_ += change;
        cityPopulation_ = std::max<int>(1, cityPopulation_);

        maintenanceHelper_->setPopulation(cityPopulation_);
        happyHelper_->setPopulation(*this, cityPopulation_);
        healthHelper_->setPopulation(cityPopulation_);
        tradeRouteHelper_->setPopulation(cityPopulation_);
        checkHappyCap();

        events_.push(CitySimulationEventPtr(new PopChange(change)));
        changeWorkingPopulation();

        if (change > 0)
        {
            currentFood_ -= std::max<int>(0, growthThreshold_ - storedFood_);
        }

        growthThreshold_ = 100 * CvPlayerAI::getPlayer(owner_).getGrowthThreshold(cityPopulation_);        
    }

    void CityData::changeWorkingPopulation()
    {
        checkHappyCap();
        int newWorkingPopulation = cityPopulation_ - getNonWorkingPopulation();
        if (newWorkingPopulation != workingPopulation_)
        {
            events_.push(CitySimulationEventPtr(new WorkingPopChange(newWorkingPopulation - workingPopulation_)));
            workingPopulation_ = newWorkingPopulation;
        }
    }

    void CityData::setStoredFood(int value)
    {
        storedFood_ = value;
    }

    void CityData::setCurrentFood(int value)
    {
        currentFood_ = value;
    }

    void CityData::changeFoodKeptPercent(int change)
    {
        foodKeptPercent_ += change;
    }

    void CityData::checkHappyCap()
    {
        int newHappyCap = happyHelper_->happyPopulation(*this) - happyHelper_->angryPopulation(*this);
        if (newHappyCap != happyCap_)
        {
            events_.push(CitySimulationEventPtr(new HappyCapChange(newHappyCap - happyCap_)));
            happyCap_ = newHappyCap;
        }
    }

    int CityData::getFood() const
    {
        int actualYield = cityPlotOutput_.actualOutput[YIELD_FOOD];
        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                actualYield += iter->actualOutput[YIELD_FOOD];
            }
        }

        for (PlotDataListConstIter iter(freeSpecOutputs_.begin()), endIter(freeSpecOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                actualYield += iter->actualOutput[YIELD_FOOD];
            }
        }

        //actualYield -= getLostFood();
        return actualYield;
    }

    int CityData::getLostFood() const
    {
        return 100 * ::abs(std::min<int>(0, healthHelper_->goodHealth() - healthHelper_->badHealth()));
    }

    int CityData::getNumUncontrolledPlots(bool includeOnlyOwnedPlots) const
    {
        if (includeOnlyOwnedPlots)
        {
            int count = 0;
            for (PlotDataListConstIter iter(unworkablePlots_.begin()), endIter(unworkablePlots_.end()); iter != endIter; ++iter)
            {
                // if this is true, plot is not controlled by us and is controlled by someone else (not us or it wouldn't be unworkable)
                if (!iter->cultureData.cultureSourcesMap.empty())
                {
                    ++count;
                }
            }
            return count;
        }
        else
        {
            return unworkablePlots_.size();
        }
    }

    void CityData::debugBasicData(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        os << "Pop: " << cityPopulation_ << ", angry = " << happyHelper_->angryPopulation(*this) << ", happy = " << happyHelper_->happyPopulation(*this)
           << ", working = " << workingPopulation_ << ", happyCap_ = " << happyCap_ << ", unhealthy = " << healthHelper_->badHealth() << ", healthy = " << healthHelper_->goodHealth()
           << ", current food = " << currentFood_ << ", stored food = " << storedFood_ << ", surplus = " << (getFood() - getLostFood() - 100 * (cityPopulation_ * foodPerPop))
           << ", threshold = " << growthThreshold_ << ", production = " << getOutput() << " ";
#endif
    }

    void CityData::debugCultureData(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n" << cityPlotOutput_.coords.iX << ", " << cityPlotOutput_.coords.iY;
        cityPlotOutput_.cultureData.debug(os);

        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot())
            {
                os << iter->coords.iX << ", " << iter->coords.iY << " = ";
                iter->cultureData.debug(os);
            }
        }

        for (PlotDataListConstIter iter(unworkablePlots_.begin()), endIter(unworkablePlots_.end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot())
            {
                os << iter->coords.iX << ", " << iter->coords.iY << " = ";
                iter->cultureData.debug(os);
            }
        }
#endif
    }

    void CityData::debugUpgradeData(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG

        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot())
            {
                os << iter->coords.iX << ", " << iter->coords.iY << " = ";
                iter->upgradeData.debug(os);
            }
        }

        for (PlotDataListConstIter iter(unworkablePlots_.begin()), endIter(unworkablePlots_.end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot())
            {
                os << iter->coords.iX << ", " << iter->coords.iY << " = ";
                iter->upgradeData.debug(os);
            }
        }
#endif
    }

    int CityData::getCurrentProductionModifier_() const
    {
        int productionModifier = 0;
        if (!buildQueue_.empty())
        {
            BuildQueueItem buildItem = buildQueue_.top();
            switch (buildItem.first)
            {
            case BuildingItem:
                productionModifier = buildingsHelper_->getProductionModifier(*this, (BuildingTypes)buildItem.second);
                {
                    int modifierHelperVersion = modifiersHelper_->getBuildingProductionModifier(*this, (BuildingTypes)buildItem.second);
                    if (productionModifier != modifierHelperVersion)
                    {
#ifdef ALTAI_DEBUG
                        CivLog::getLog(CvPlayerAI::getPlayer(owner_))->getStream() << "\nmismatched building modifiers: " << productionModifier << " " << modifierHelperVersion
                            << " for building: " << gGlobals.getBuildingInfo((BuildingTypes)buildItem.second).getType();
#endif
                    }
                }
                break;
            case UnitItem:
                productionModifier = modifiersHelper_->getUnitProductionModifier((UnitTypes)buildItem.second);
                break;
            case ProjectItem:
                productionModifier = modifiersHelper_->getProjectProductionModifier(*this, (ProjectTypes)buildItem.second);
                break;
            default:  // processes have no extra modifer
                break;
            }
        }

        return productionModifier;
    }

    TotalOutput CityData::getOutput() const
    {
        YieldModifier yieldModifier = modifiersHelper_->getTotalYieldModifier(*this);
        CommerceModifier commerceModifier = modifiersHelper_->getTotalCommerceModifier();

        YieldModifier commerceYieldModifier = makeYield(100, 100, commerceYieldModifier_);
        TotalOutput totalOutput = makeOutput(tradeRouteHelper_->getTradeYield(), commerceYieldModifier, makeCommerce(100, 100, 100, 100), commercePercent_);

        //yieldModifier[OUTPUT_PRODUCTION] += getCurrentProductionModifier_();  // get this separately

        totalOutput += cityPlotOutput_.actualOutput;

        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                totalOutput += iter->actualOutput;
            }
        }

        for (PlotDataListConstIter iter(freeSpecOutputs_.begin()), endIter(freeSpecOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                totalOutput += iter->actualOutput;
            }
        }

        // multiply by final yield and commerce modifiers
        totalOutput[OUTPUT_FOOD] = (totalOutput[OUTPUT_FOOD] * yieldModifier[YIELD_FOOD]) / 100;
        totalOutput[OUTPUT_PRODUCTION] = (totalOutput[OUTPUT_PRODUCTION] * yieldModifier[YIELD_PRODUCTION]) / 100;
        totalOutput[OUTPUT_RESEARCH] = (totalOutput[OUTPUT_RESEARCH] * commerceModifier[COMMERCE_RESEARCH]) / 100;
        totalOutput[OUTPUT_GOLD] = (totalOutput[OUTPUT_GOLD] * commerceModifier[COMMERCE_GOLD]) / 100;
        totalOutput[OUTPUT_CULTURE] = (totalOutput[OUTPUT_CULTURE] * commerceModifier[COMMERCE_CULTURE]) / 100;
        totalOutput[OUTPUT_ESPIONAGE] = (totalOutput[OUTPUT_ESPIONAGE] * commerceModifier[COMMERCE_ESPIONAGE]) / 100;

        return totalOutput;
    }

    TotalOutput CityData::getProcessOutput() const
    {
        TotalOutput processOutput;

        if (!buildQueue_.empty() && buildQueue_.top().first == ProcessItem)
        {
            TotalOutput output = getOutput();
            int currentProduction = getCurrentProduction_(output);

            const CvProcessInfo& processInfo = gGlobals.getProcessInfo((ProcessTypes)buildQueue_.top().second);
            for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
            {
                int modifier = processInfo.getProductionToCommerceModifier(i);
                if (modifier > 0)
                {
                    processOutput[NUM_YIELD_TYPES - 1 + i] = (currentProduction * modifier) / 100;
                }
            }
        }

        return processOutput;
    }

    // account for lost and consumed food
    TotalOutput CityData::getActualOutput() const
    {
        TotalOutput totalOutput = getOutput();

        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        int foodDelta = totalOutput[OUTPUT_FOOD] - 100 * (cityPopulation_ * foodPerPop) - getLostFood();
        totalOutput[OUTPUT_FOOD] = foodDelta;
        if (foodDelta > 0 && foodKeptPercent_ > 0)
        {
            totalOutput[OUTPUT_FOOD] += (foodDelta * foodKeptPercent_) / 100;
        }
        
        return totalOutput;
    }

    PlotYield CityData::getPlotYield() const
    {
        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);

        PlotYield totalPlotYield(cityPlotOutput_.plotYield);

        // add trade routes
        PlotYield tradeYield = tradeRouteHelper_->getTradeYield();
        for (int i = 0; i < NUM_YIELD_TYPES; ++i)
        {
            tradeYield[i] = (tradeYield[i] * yieldModifier[i]) / 100;
        }

        totalPlotYield += tradeYield;

        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                totalPlotYield += iter->plotYield;
            }
        }

        for (PlotDataListConstIter iter(freeSpecOutputs_.begin()), endIter(freeSpecOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                totalPlotYield += iter->plotYield;
            }
        }

        return totalPlotYield;
    }

    GreatPersonOutputMap CityData::getGPP() const
    {
        GreatPersonOutputMap greatPersonOutputMap(cityGreatPersonOutput_);

        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked && iter->greatPersonOutput.output != 0)
            {
                greatPersonOutputMap[iter->greatPersonOutput.unitType] += iter->greatPersonOutput.output;
            }
        }
        return greatPersonOutputMap;
    }

    std::set<UnitTypes> CityData::getAvailableGPTypes() const
    {
        // doesn't include city generated gpp (i.e. non-variable sources)
        std::set<UnitTypes> unitTypes;

        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->greatPersonOutput.output != 0)
            {
                unitTypes.insert(iter->greatPersonOutput.unitType);
            }
        }
        return unitTypes;
    }

    CitySimulationEventPtr CityData::getEvent()
    {
        if (events_.empty())
        {
            return CitySimulationEventPtr();
        }
        else
        {
            CitySimulationEventPtr event = events_.front();
            events_.pop();
            return event;
        }
    }

    void CityData::pushEvent(const CitySimulationEventPtr& event)
    {
        events_.push(event);
    }

    // todo - everything here needs to use helpers so simulated buildings are handled correctly
    void CityData::calcCityOutput_()
    {
        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

        CvPlot* pPlot = gGlobals.getMap().plot(coords_.iX, coords_.iY);
        PlotYield cityYield(pPlot->getYield());
        Commerce cityCommerce;

        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            BuildingTypes buildingType = (BuildingTypes)gGlobals.getCivilizationInfo(pCity_->getCivilizationType()).getCivilizationBuildings(i);
            if (buildingType == NO_BUILDING)
            {
                continue;
            }

            int buildingCount = buildingsHelper_->getNumBuildings(buildingType) + buildingsHelper_->getNumFreeBuildings(buildingType);
            if (buildingCount > 0)
            {
                const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo((BuildingTypes)buildingType);

                cityYield += buildingCount * (PlotYield(buildingInfo.getYieldChangeArray()) + buildingsHelper_->getBuildingYieldChange((BuildingClassTypes)buildingInfo.getBuildingClassType()));
                cityCommerce += buildingsHelper_->getBuildingCommerce(*this, buildingType);

                // add building great people points
                int gppRate = buildingInfo.getGreatPeopleRateChange();

                if (gppRate != 0)
                {
                    UnitClassTypes unitClassType = (UnitClassTypes)buildingInfo.getGreatPeopleUnitClass();
                    if (unitClassType != NO_UNITCLASS)
                    {
                        UnitTypes unitType = getPlayerVersion(owner_, unitClassType);
                        if (unitType != NO_UNIT)
                        {
                            cityGreatPersonOutput_[unitType] += buildingCount * gppRate;
                        }
                    }
                }
            }
        }

        // commerce (culture usually) from religions present in city
        cityCommerce += religionHelper_->getCityReligionCommerce();

        // add free specialists (e.g. free priest from TofA, scientists from GLib, but not free spec slots)
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);
        for (int specialistType = 0, count = gGlobals.getNumSpecialistInfos(); specialistType < count; ++specialistType)
        {
            int specCount = specialistHelper_->getFreeSpecialistCount((SpecialistTypes)specialistType);
            if (specCount > 0)
            {
                cityYield += specCount * getSpecialistYield(player, (SpecialistTypes)specialistType);
                cityCommerce += specCount * getSpecialistCommerce(player, (SpecialistTypes)specialistType);

                GreatPersonOutput greatPersonOutput = GameDataAnalysis::getSpecialistUnitTypeAndOutput((SpecialistTypes)specialistType, owner_);
                if (greatPersonOutput.output != 0)
                {
                    cityGreatPersonOutput_[greatPersonOutput.unitType] += specCount * greatPersonOutput.output;
                }
            }
        }

        // TODO corps
        PlotInfoPtr pPlotInfo(new PlotInfo(pPlot, owner_));

        cityPlotOutput_ = PlotData(pPlotInfo, cityYield, cityCommerce, makeOutput(cityYield, cityCommerce, yieldModifier, commerceModifier, commercePercent_),
            GreatPersonOutput(), coords_, NO_IMPROVEMENT, NO_BONUS, NO_FEATURE, pPlot->getRouteType(), PlotData::CultureData(pPlot, owner_, pCity_));
    }

    int CityData::getNextImprovementUpgradeTime() const
    {
        std::map<int, std::vector<XYCoords> > upgradesMap;

        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked && !iter->upgradeData.upgrades.empty())
            {
                int timeToUpgrade = iter->upgradeData.timeHorizon - iter->upgradeData.upgrades.begin()->remainingTurns;
                upgradesMap[timeToUpgrade].push_back(iter->coords);
            }
        }

        return upgradesMap.empty() ? MAX_INT : upgradesMap.begin()->first;
    }

    void CityData::doImprovementUpgrades(int nTurns)
    {
        std::vector<std::pair<ImprovementTypes, XYCoords> > upgrades;

        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

        for (PlotDataListIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked && !iter->upgradeData.upgrades.empty())
            {
                PlotData::UpgradeData::Upgrade upgrade = iter->upgradeData.advanceTurn(nTurns);
                if (upgrade.improvementType != NO_IMPROVEMENT)
                {
                    iter->improvementType = upgrade.improvementType;
                    iter->plotYield += upgrade.extraYield;

                    TotalOutput outputChange = makeOutput(upgrade.extraYield, yieldModifier, commerceModifier, commercePercent_);
                    iter->output += outputChange;
                    iter->actualOutput += outputChange;

                    upgrades.push_back(std::make_pair(iter->improvementType, iter->coords));
                }
            }
        }

        if (!upgrades.empty())
        {
            events_.push(CitySimulationEventPtr(new ImprovementUpgrade(upgrades)));
        }
    }   

    // todo - cross check these fns with SpecialistHelper?
    int CityData::getSpecialistSlotCount() const
    {
        int specialistCount = 0;
        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (!iter->isActualPlot() && (SpecialistTypes)iter->coords.iY != NO_SPECIALIST && iter->greatPersonOutput.output > 0)
            {
                ++specialistCount;
            }
        }
        return specialistCount;
    }

    int CityData::getNumPossibleSpecialists(SpecialistTypes specialistType) const
    {
        int specialistCount = 0;
        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (!iter->isActualPlot() && (SpecialistTypes)iter->coords.iY == specialistType)
            {
                ++specialistCount;
            }
        }
        return specialistCount;
    }

    void CityData::addSpecialistSlots(SpecialistTypes specialistType, int count)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);
        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

        PlotYield yield(getSpecialistYield(player, specialistType));
        Commerce commerce(getSpecialistCommerce(player, specialistType));
        TotalOutput specialistOutput(makeOutput(yield, commerce, yieldModifier, commerceModifier, commercePercent_));

        plotOutputs_.insert(plotOutputs_.end(), count, 
            PlotData(PlotInfoPtr(), yield, commerce, specialistOutput, GameDataAnalysis::getSpecialistUnitTypeAndOutput(specialistType, owner_), XYCoords(-1, specialistType)));
    }

    void CityData::changePlayerFreeSpecialistSlotCount(int change)
    {
        specialistHelper_->changePlayerFreeSpecialistSlotCount(change);
        updateFreeSpecialistSlots_(change);
    }

    void CityData::changeImprovementFreeSpecialistSlotCount(int change)
    {
        specialistHelper_->changeImprovementFreeSpecialistSlotCount(change);
        updateFreeSpecialistSlots_(change);
    }

    void CityData::changeFreeSpecialistCountPerImprovement(ImprovementTypes improvementType, int change)
    {
        // todo - include assignment of plots to city in count (only our plots should count)
        int improvementCount = 0;
        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot() && iter->improvementType == improvementType)
            {
                ++improvementCount;
            }
        }
        specialistHelper_->changeFreeSpecialistCountPerImprovement(improvementType, change);
        specialistHelper_->changeImprovementFreeSpecialistSlotCount(change * improvementCount);
        updateFreeSpecialistSlots_(change);
    }

    void CityData::updateFreeSpecialistSlots_(int change)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);
        int defaultSpecType = gGlobals.getDefineINT("DEFAULT_SPECIALIST");
        freeSpecOutputs_.clear();

        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

        for (int specialistType = 0, specialistCount = gGlobals.getNumSpecialistInfos(); specialistType < specialistCount; ++specialistType)
        {
            int availableSlots = 0;

            if (defaultSpecType == specialistType || player.isSpecialistValid((SpecialistTypes)specialistType))
            {
                availableSlots = std::max<int>(0, std::min<int>(workingPopulation_, specialistHelper_->getTotalFreeSpecialistSlotCount()));
            }
            else
            {
                int maxSpecialistCount = specialistHelper_->getMaxSpecialistCount((SpecialistTypes)specialistType);
                availableSlots = std::max<int>(0, std::min<int>(maxSpecialistCount, specialistHelper_->getTotalFreeSpecialistSlotCount()));
            }

            if (availableSlots > 0)
            {
                PlotYield yield(getSpecialistYield(player, (SpecialistTypes)specialistType));
                Commerce commerce(getSpecialistCommerce(player, (SpecialistTypes)specialistType));
                TotalOutput specialistOutput(makeOutput(yield, commerce, yieldModifier, commerceModifier, commercePercent_));
                PlotData specialistData(PlotInfoPtr(), yield, commerce, specialistOutput, 
                    GameDataAnalysis::getSpecialistUnitTypeAndOutput((SpecialistTypes)specialistType, player.getID()), XYCoords(-1, specialistType));

                freeSpecOutputs_.insert(freeSpecOutputs_.end(), availableSlots, specialistData);
            }
        }
    }

    PlotDataListIter CityData::findPlot(XYCoords coords)
    {
        PlotDataListIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end());

        for (; iter != endIter; ++iter)
        {
            if (iter->coords == coords)
            {
                return iter;
            }
        }
        return endIter;
    }

    PlotDataListConstIter CityData::findPlot(XYCoords coords) const
    {
        PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end());

        for (; iter != endIter; ++iter)
        {
            if (iter->coords == coords)
            {
                return iter;
            }
        }
        return endIter;
    }

    bool CityData::canWork_(const CvPlot* pPlot, int lookaheadDepth) const
    {
        // skip check here - rely on assigned city check in plot setup
        // this allows easier simulation of adding removing shared plots
        //const CvCity* pWorkingCity = pPlot->getWorkingCity();
        //// can't consider if definitely worked by another city which is ours
        //// otherwise use cultural control to decide
        //if (pWorkingCity && pWorkingCity != pCity_ && pWorkingCity->getOwner() == pCity_->getOwner())
        //{
        //    return false;
        //}

        if (pPlot->isUnit() && pPlot->plotCheck(PUF_canSiege, owner_))
        {
            return false;
        }

        if (pPlot->isWater())
        {
            if (!civHelper_->hasTech(GameDataAnalysis::getCanWorkWaterTech()) && 
                lookaheadDepth == 0 &&
                gGlobals.getGame().getAltAI()->getPlayer(owner_)->getAnalysis()->getTechResearchDepth(GameDataAnalysis::getCanWorkWaterTech()) > lookaheadDepth)
            {
                return false;
            }

            if (pPlot->getBlockadedCount(pCity_->getTeam()) > 0)
            {
                return false;
            }
        }

        return true;
    }

    bool CityData::avoidGrowth_() const
    {
        return happyCap_ + (CvPlayerAI::getPlayer(owner_).canPopRush() ? 1 : 0) <= 0;
    }

    int CityData::getCurrentProduction() const
    {
        return getCurrentProduction_(getOutput());
    }

    int CityData::getCurrentProduction_(TotalOutput currentOutput) const
    {
        int production = currentOutput[OUTPUT_PRODUCTION];
        const int baseModifier = modifiersHelper_->getTotalYieldModifier(*this)[YIELD_PRODUCTION];
        const int modifier = getCurrentProductionModifier_();

        // divide by baseModifier as the raw output has already been modified by it
        production = ((production * 100) / baseModifier) * (baseModifier + modifier);
        production /= 100;

        if (isFoodProductionBuildItem())
        {
            const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
            int requiredYield = 100 * (getPopulation() - std::max<int>(0, angryPopulation() - happyPopulation())) * foodPerPop + getLostFood();
            production += currentOutput[OUTPUT_FOOD] - requiredYield;
        }

        return production;
    }

    void CityData::debugSummary(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << " " << cityPlotOutput_.plotYield << "\n\t";
        for (PlotDataListConstIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                if (iter != plotOutputs_.begin())
                {
                    os << "; ";
                }

                iter->debugSummary(os);
            }
        }
#endif
    }
}