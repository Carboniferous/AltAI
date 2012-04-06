#include "./city_data.h"
#include "./iters.h"
#include "./plot_info_visitors.h"
#include "./building_info_visitors.h"
#include "./city_simulator.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
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
#include "./corporation_helper.h"
#include "./bonus_helper.h"
#include "./specialist_helper.h"
#include "./helper_fns.h"
#include "./gamedata_analysis.h"
#include "./city_improvements.h"
#include "./error_log.h"

// todo - specialist helper pCity->totalFreeSpecialists(), pCity->getMaxSpecialistCount
// bonushelper - pCity->hasBonus()
// goal is this class should not keep hold of its CvCity ptr, to allow it to run simulations in separate threads

namespace AltAI
{
    CityData::CityData(const CityData& other)
    {
        cityPopulation_ = other.cityPopulation_, workingPopulation_ = other.workingPopulation_, happyCap_ = other.happyCap_;

        currentFood_ = other.currentFood_, storedFood_ = other.storedFood_, currentProduction_ = other.currentProduction_;
        growthThreshold_ = other.growthThreshold_, requiredProduction_ = other.requiredProduction_;
        foodKeptPercent_ = other.foodKeptPercent_;
        commerceYieldModifier_ = other.commerceYieldModifier_;

        specialConditions_ = other.specialConditions_;

        queuedBuildings_ = other.queuedBuildings_;

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
    }

    CityData::CityData(const CvCity* pCity, bool includeUnclaimedPlots)
        : cityPopulation_(0), workingPopulation_(0), happyCap_(0), currentFood_(0), storedFood_(0),
          currentProduction_(0), growthThreshold_(0), requiredProduction_(-1), foodKeptPercent_(0),
          commerceYieldModifier_(100), specialConditions_(None), pCity_(pCity), owner_(pCity->getOwner()),
          coords_(pCity->getX(), pCity->getY()), includeUnclaimedPlots_(includeUnclaimedPlots)
    {
        initHelpers_(pCity);
        init_(pCity);
        calcOutputsFromPlotData_(pCity);
        calculateSpecialistOutput_();
    }

    CityData::CityData(const CvCity* pCity, const std::vector<CityImprovementManager::PlotImprovementData>& improvements, bool includeUnclaimedPlots)
        : cityPopulation_(0), workingPopulation_(0), happyCap_(0), currentFood_(0), storedFood_(0),
          currentProduction_(0), growthThreshold_(0), requiredProduction_(-1), foodKeptPercent_(0),
          commerceYieldModifier_(100), specialConditions_(None), pCity_(pCity), owner_(pCity_->getOwner()),
          coords_(pCity->getX(), pCity->getY()), includeUnclaimedPlots_(includeUnclaimedPlots)
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
    }

    void CityData::init_(const CvCity* pCity)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);

        currentFood_ = 100 * pCity->getFood();
        storedFood_ = 100 * pCity->getFoodKept();
        cityPopulation_ = pCity->getPopulation();
        workingPopulation_ = cityPopulation_ - getNonWorkingPopulation();
        happyCap_ = happyHelper_->happyPopulation() - happyHelper_->angryPopulation(*this);

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
        return copy;
    }

    void CityData::calcOutputsFromPlotData_(const CvCity* pCity)
    {
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
                    PlotYield plotYield(pLoopPlot->getYield());
                     // todo - check adding improvement later to plot will work (if there are any plots which have zero yield without improvement)
                    if (isEmpty(plotYield) || !canWork_(pLoopPlot))  // todo use controlled and abletowork flags properly
                        //pLoopPlot->getOwner() == owner_ && pLoopPlot->getWorkingCity() && pMapAnalysis->getWorkingCity(pCity->getIDInfo(), XYCoords(pLoopPlot->getX(), pLoopPlot->getY())) != pCity)
                    {
                        continue;  // no point in adding desert, or plots worked by other cities we own (add ones we don't own, in case we end up owning them)
                    }

                    initPlot_(pLoopPlot, plotYield, pLoopPlot->getImprovementType(), pLoopPlot->getFeatureType(), pLoopPlot->getRouteType());
                }
            }
        }
    }

    void CityData::calcOutputsFromPlannedImprovements_(const std::vector<CityImprovementManager::PlotImprovementData>& plotImprovements)
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
                    XYCoords coords(pLoopPlot->getX(), pLoopPlot->getY());
                    ImprovementTypes improvementType = pLoopPlot->getImprovementType();
                    FeatureTypes featureType = pLoopPlot->getFeatureType();
                    PlotYield plotYield(pLoopPlot->getYield());
                    RouteTypes routeType = pLoopPlot->getRouteType();

                    std::vector<CityImprovementManager::PlotImprovementData>::const_iterator improvementIter = 
                        std::find_if(plotImprovements.begin(), plotImprovements.end(), ImprovementCoordsFinder(coords));
                    if (improvementIter != plotImprovements.end())
                    {
                        // allow for improvements which have upgraded since they were built
                        if (boost::get<5>(*improvementIter) != CityImprovementManager::Built)
                        {
                            // todo - add route type to improvement manager data
                            improvementType = boost::get<2>(*improvementIter);
                            plotYield = boost::get<3>(*improvementIter);
                            featureType = boost::get<1>(*improvementIter);
                        }
                    }
                    
                    if (isEmpty(plotYield))  // skip canWork_ check if setting up from planned improvements, as this is looking ahead
                    {
                        continue;  // no point in adding desert, or plots worked by other cities we own (add ones we don't own, in case we end up owning them)
                    }

                    initPlot_(pLoopPlot, plotYield, improvementType, featureType, routeType);
                }
            }
        }
    }

    void CityData::initPlot_(const CvPlot* pPlot, PlotYield plotYield, ImprovementTypes improvementType, FeatureTypes featureType, RouteTypes routeType)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);
        const boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis = gGlobals.getGame().getAltAI()->getPlayer(player.getID())->getAnalysis();
        const boost::shared_ptr<MapAnalysis>& pMapAnalysis = pPlayerAnalysis->getMapAnalysis();
        const int upgradeRate = player.getImprovementUpgradeRate();
        const int timeHorizon = pPlayerAnalysis->getTimeHorizon();

        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

        TotalOutput plotOutput(makeOutput(plotYield, yieldModifier, commerceModifier, commercePercent_));

        PlotData plot(plotYield, Commerce(), plotOutput, GreatPersonOutput(), XYCoords(pPlot->getX(), pPlot->getY()),
                          improvementType, featureType, routeType, PlotData::CultureData(pPlot, owner_, pCity_));

        plot.controlled = plot.cultureData.ownerAndCultureTrumpFlag.first == owner_ || plot.cultureData.ownerAndCultureTrumpFlag.first == NO_PLAYER && includeUnclaimedPlots_;
        plot.ableToWork = true;
                    
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

            PlotData specialistData(yield, commerce, specialistOutput, 
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

    // first = turns (MAX_INT if never), second = pop change (+1, -1, 0)
    std::pair<int, int> CityData::getTurnsToPopChange() const
    {
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
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
        int finalStoredFood = storedFood_ + (foodDelta > 0 ? ((foodDelta * nTurns * foodKeptPercent_) / 100) : storedFood_ - foodDelta * nTurns);

        finalCurrentFood = bound(finalCurrentFood, Range<>(0, getGrowthThreshold()));
        finalStoredFood = bound(finalStoredFood, Range<>(0, (getGrowthThreshold() * foodKeptPercent_) / 100));

        return std::make_pair(finalCurrentFood, finalStoredFood);
    }

    void CityData::advanceTurn()
    {
        // TODO create event when improvements upgrade
        doUpgrades_();

        TotalOutput totalOutput(getOutput());

        // TODO handle builds that use food as well as production
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        int foodDelta = totalOutput[OUTPUT_FOOD] - 100 * (cityPopulation_ * foodPerPop) - getLostFood();
        currentFood_ += foodDelta;
        storedFood_ += foodDelta;
        storedFood_ = range(storedFood_, 0, (growthThreshold_ * foodKeptPercent_) / 100);

        if (currentFood_ >= growthThreshold_)
        {
            changePopulation(1);
        }
        else if (currentFood_ < 0)
        {
            currentFood_ = 0;
            changePopulation(-1);
        }

        currentProduction_ += totalOutput[OUTPUT_PRODUCTION];

        cultureHelper_->advanceTurn(*this, includeUnclaimedPlots_);

        if (currentProduction_ >= requiredProduction_ && !queuedBuildings_.empty())
        {
            BuildingTypes completedBuilding = queuedBuildings_.top();
            buildingsHelper_->changeNumRealBuildings(completedBuilding);
            events_.push(CitySimulationEventPtr(new TestBuildingBuilt(completedBuilding)));

            // TODO handle overflow properly using logic in HurryHelper
            currentProduction_ = currentProduction_ - requiredProduction_;
            completeBuilding_();
            requiredProduction_ = -1;
        }

        happyHelper_->advanceTurn(*this);
        healthHelper_->advanceTurn();
        
        changeWorkingPopulation();
        checkHappyCap();
    }

    void CityData::setBuilding(BuildingTypes buildingType)
    {
        if (buildingType != NO_BUILDING)
        {
            queuedBuildings_.push(buildingType);
            requiredProduction_ = 100 * (pCity_->getProductionNeeded(buildingType) - pCity_->getBuildingProduction(buildingType));
        }
        else
        {
            requiredProduction_ = -1;
        }
    }

    void CityData::hurry(const HurryData& hurryData)
    {
        if (hurryData.hurryPopulation > 0)
        {
            changePopulation(-hurryData.hurryPopulation);
            hurryHelper_->updateAngryTimer();
        }
        
        currentProduction_ = requiredProduction_ + hurryData.extraProduction;
    }

    int CityData::happyPopulation() const
    {
        return getHappyHelper()->happyPopulation();
    }

    int CityData::angryPopulation() const
    {
        return getHappyHelper()->angryPopulation(*this);
    }

    std::pair<bool, HurryData> CityData::canHurry(HurryTypes hurryType) const
    {
        return hurryHelper_->canHurry(*this, hurryType);
    }

    void CityData::completeBuilding_()
    {
        queuedBuildings_.pop();
    }

    int CityData::getNonWorkingPopulation() const
    {
        return range(happyHelper_->angryPopulation(*this) - happyHelper_->happyPopulation(), 0, cityPopulation_);
    }

    void CityData::changePopulation(int change)
    {
        cityPopulation_ += change;
        cityPopulation_ = std::max<int>(1, cityPopulation_);

        maintenanceHelper_->setPopulation(cityPopulation_);
        happyHelper_->setPopulation(*this, cityPopulation_);
        healthHelper_->setPopulation(cityPopulation_);
        tradeRouteHelper_->setPopulation(cityPopulation_);

        events_.push(CitySimulationEventPtr(new PopChange(change)));
        changeWorkingPopulation();

        growthThreshold_ = 100 * CvPlayerAI::getPlayer(owner_).getGrowthThreshold(cityPopulation_);

        if (change > 0)
        {
            currentFood_ = storedFood_;
        }
    }

    void CityData::changeWorkingPopulation()
    {
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
        int newHappyCap = happyHelper_->happyPopulation() - happyHelper_->angryPopulation(*this);
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
        os << "Pop: " << cityPopulation_ << ", angry = " << happyHelper_->angryPopulation(*this) << ", happy = " << happyHelper_->happyPopulation()
           << ", working = " << workingPopulation_ << ", happyCap_ = " << happyCap_ << ", unhealthy = " << healthHelper_->badHealth() << ", healthy = " << healthHelper_->goodHealth()
           << ", currentFood_ = " << currentFood_ << ", surplus = " << (getFood() - getLostFood() - 100 * (cityPopulation_ * foodPerPop)) << ", production = " << getOutput() << " ";
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

    int CityData::getBuildingProductionModifier_() const
    {
        int productionModifier = 0;
        if (!queuedBuildings_.empty())
        {
            BuildingTypes buildingType = queuedBuildings_.top();
            productionModifier = buildingsHelper_->getProductionModifier(*this, buildingType);
        }

        return productionModifier;
    }

    TotalOutput CityData::getOutput() const
    {
        YieldModifier yieldModifier = modifiersHelper_->getTotalYieldModifier(*this);
        CommerceModifier commerceModifier = modifiersHelper_->getTotalCommerceModifier();

        YieldModifier commerceYieldModifier = makeYield(100, 100, commerceYieldModifier_);
        TotalOutput totalOutput = makeOutput(tradeRouteHelper_->getTradeYield(), commerceYieldModifier, makeCommerce(100, 100, 100, 100), commercePercent_);

        yieldModifier[OUTPUT_PRODUCTION] += getBuildingProductionModifier_();

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

            int buildingCount = buildingsHelper_->getNumBuildings(buildingType);
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
        cityPlotOutput_ = PlotData(cityYield, cityCommerce, makeOutput(cityYield, cityCommerce, yieldModifier, commerceModifier, commercePercent_),
            GreatPersonOutput(), coords_, NO_IMPROVEMENT, NO_FEATURE, pPlot->getRouteType(), PlotData::CultureData(pPlot, owner_, pCity_));
    }

    void CityData::doUpgrades_()
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);
        const boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis = gGlobals.getGame().getAltAI()->getPlayer(player.getID())->getAnalysis();
        const int timeHorizon = pPlayerAnalysis->getTimeHorizon();
        std::vector<std::pair<ImprovementTypes, XYCoords> > upgrades;

        YieldModifier yieldModifier = makeYield(100, 100, commerceYieldModifier_);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

        const bool hasPower = buildingsHelper_->isPower() || buildingsHelper_->isDirtyPower();

        for (PlotDataListIter iter(plotOutputs_.begin()), endIter(plotOutputs_.end()); iter != endIter; ++iter)
        {
            if (!iter->upgradeData.upgrades.empty())
            {
                PlotData::UpgradeData::Upgrade upgrade = iter->upgradeData.advanceTurn();
                if (upgrade.improvementType != NO_IMPROVEMENT)
                {
                    iter->improvementType = upgrade.improvementType;
                    iter->plotYield += upgrade.extraYield;

                    TotalOutput outputChange = makeOutput(upgrade.extraYield, yieldModifier, commerceModifier, commercePercent_);
                    iter->output = iter->actualOutput + outputChange;

                    upgrades.push_back(std::make_pair(iter->improvementType, iter->coords));
                }
            }
        }

        if (!upgrades.empty())
        {
            events_.push(CitySimulationEventPtr(new ImprovementUpgrade(upgrades)));
        }
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
            PlotData(yield, commerce, specialistOutput, GameDataAnalysis::getSpecialistUnitTypeAndOutput(specialistType, owner_), XYCoords(-1, specialistType)));
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
                PlotData specialistData(yield, commerce, specialistOutput, 
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

    bool CityData::canWork_(const CvPlot* pPlot) const
    {
        const CvCity* pWorkingCity = pPlot->getWorkingCity();
        // can't consider if definitely worked by another city which is ours
        // otherwise use cultural control to decide
        if (pWorkingCity && pWorkingCity != pCity_ && pWorkingCity->getOwner() == pCity_->getOwner())
  	    {
    	    return false;
        }

        if (pPlot->plotCheck(PUF_canSiege, owner_))
	    {
		    return false;
	    }

        if (pPlot->isWater())
        {
            if (!civHelper_->hasTech(GameDataAnalysis::getCanWorkWaterTech()))
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
}