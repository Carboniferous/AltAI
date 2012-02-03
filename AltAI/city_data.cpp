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
#include "./trade_route_helper.h"
#include "./corporation_helper.h"
#include "./bonus_helper.h"
#include "./specialist_helper.h"
#include "./helper_fns.h"
#include "./gamedata_analysis.h"
#include "./city_improvements.h"

// todo - specialist helper pCity->totalFreeSpecialists(), pCity->getMaxSpecialistCount
// bonushelper - pCity->hasBonus()
// goal is this class should not keep hold of its CvCity ptr, to allow it to run simulations in separate threads

namespace AltAI
{
    CityData::CityData(const CvCity* pCity_, bool includeUnclaimedPlots)
        : cityPopulation(0), workingPopulation(0), happyCap(0), currentFood(0), storedFood(0),
          currentProduction(0), growthThreshold(0), requiredProduction(-1), foodKeptPercent(0), currentProductionModifier(0),
          civHelper(gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getCivHelper()),
          happyHelper(boost::shared_ptr<HappyHelper>(new HappyHelper(pCity_))),
          healthHelper(boost::shared_ptr<HealthHelper>(new HealthHelper(pCity_))),
          maintenanceHelper(boost::shared_ptr<MaintenanceHelper>(new MaintenanceHelper(pCity_))),
          religionHelper(boost::shared_ptr<ReligionHelper>(new ReligionHelper(pCity_))),
          cultureHelper(boost::shared_ptr<CultureHelper>(new CultureHelper(pCity_))),
          tradeRouteHelper(boost::shared_ptr<TradeRouteHelper>(new TradeRouteHelper(pCity_))),
          buildingHelper(boost::shared_ptr<BuildingHelper>(new BuildingHelper(pCity_))),
          bonusHelper(boost::shared_ptr<BonusHelper>(new BonusHelper(pCity_))),
          specialistHelper(boost::shared_ptr<SpecialistHelper>(new SpecialistHelper(pCity_))),
          corporationHelper(boost::shared_ptr<CorporationHelper>(new CorporationHelper(pCity_))),
          specialConditions(None),
          pCity(pCity_), owner(pCity_->getOwner()), coords(pCity->getX(), pCity->getY()), includeUnclaimedPlots_(includeUnclaimedPlots)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner);

        currentFood = 100 * pCity->getFood();
        storedFood = 100 * pCity->getFoodKept();
        cityPopulation = pCity->getPopulation();
        workingPopulation = cityPopulation - getNonWorkingPopulation();
        happyCap = happyHelper->happyPopulation() - happyHelper->angryPopulation();

        growthThreshold = 100 * pCity->growthThreshold();
        foodKeptPercent = pCity->getMaxFoodKeptPercent();

        for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
        {
            yieldModifier_[yieldType] = pCity->getBaseYieldRateModifier((YieldTypes)yieldType);
        }
        currentProductionModifier = yieldModifier_[YIELD_PRODUCTION];

        for (int commerceType = 0; commerceType < NUM_COMMERCE_TYPES; ++commerceType)
        {
            // modifier is just extra % here, so add 100 in
            commerceModifier_[commerceType] = 100 + pCity->getCommerceRateModifier((CommerceTypes)commerceType);
            commercePercent_[commerceType] = player.getCommercePercent((CommerceTypes)commerceType);
        }

        calcOutputsFromPlotData_();
    }

    CityData::CityData(const CvCity* pCity_, const CityImprovementManager& improvements)
        : cityPopulation(0), workingPopulation(0), happyCap(0), currentFood(0), storedFood(0),
          currentProduction(0), growthThreshold(0), requiredProduction(-1), foodKeptPercent(0), currentProductionModifier(0),
          civHelper(gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getCivHelper()),
          happyHelper(boost::shared_ptr<HappyHelper>(new HappyHelper(pCity_))),
          healthHelper(boost::shared_ptr<HealthHelper>(new HealthHelper(pCity_))),
          maintenanceHelper(boost::shared_ptr<MaintenanceHelper>(new MaintenanceHelper(pCity_))),
          religionHelper(boost::shared_ptr<ReligionHelper>(new ReligionHelper(pCity_))),
          cultureHelper(boost::shared_ptr<CultureHelper>(new CultureHelper(pCity_))),
          tradeRouteHelper(boost::shared_ptr<TradeRouteHelper>(new TradeRouteHelper(pCity_))),
          buildingHelper(boost::shared_ptr<BuildingHelper>(new BuildingHelper(pCity_))),
          bonusHelper(boost::shared_ptr<BonusHelper>(new BonusHelper(pCity_))),
          specialistHelper(boost::shared_ptr<SpecialistHelper>(new SpecialistHelper(pCity_))),
          corporationHelper(boost::shared_ptr<CorporationHelper>(new CorporationHelper(pCity_))),
          specialConditions(None),
          pCity(pCity_), owner(pCity_->getOwner()), coords(pCity->getX(), pCity->getY()), includeUnclaimedPlots_(improvements.getIncludeUnclaimedPlots)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner);

        currentFood = 100 * pCity->getFood();
        storedFood = 100 * pCity->getFoodKept();
        cityPopulation = pCity->getPopulation();
        workingPopulation = cityPopulation - getNonWorkingPopulation();
        happyCap = happyHelper->happyPopulation() - happyHelper->angryPopulation();

        growthThreshold = 100 * pCity->growthThreshold();
        foodKeptPercent = pCity->getMaxFoodKeptPercent();

        for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
        {
            yieldModifier_[yieldType] = pCity->getBaseYieldRateModifier((YieldTypes)yieldType);
        }
        currentProductionModifier = yieldModifier_[YIELD_PRODUCTION];

        for (int commerceType = 0; commerceType < NUM_COMMERCE_TYPES; ++commerceType)
        {
            // modifier is just extra % here, so add 100 in
            commerceModifier_[commerceType] = 100 + pCity->getCommerceRateModifier((CommerceTypes)commerceType);
            commercePercent_[commerceType] = player.getCommercePercent((CommerceTypes)commerceType);
        }

        calcOutputsFromPlannedImprovements_(improvements);
    }

    // events queue is not copied
    CityData::CityData(const CityData& other)
        : plotOutputs(other.plotOutputs),
          unworkablePlots(other.unworkablePlots),
          freeSpecOutputs(other.freeSpecOutputs),
          cityPlotOutput(other.cityPlotOutput),
          cityGreatPersonOutput(other.cityGreatPersonOutput), 
          cityPopulation(other.cityPopulation),
          workingPopulation(other.workingPopulation),
          happyCap(other.happyCap),
          currentFood(other.currentFood),
          storedFood(other.storedFood),
          currentProduction(other.currentProduction),
          growthThreshold(other.growthThreshold),
          requiredProduction(other.requiredProduction),
          foodKeptPercent(other.foodKeptPercent),
          yieldModifier_(other.yieldModifier_),
          commerceModifier_(other.commerceModifier_),
          commercePercent_(other.commercePercent_), 
          currentProductionModifier(other.currentProductionModifier), 
          civHelper(other.civHelper),
          happyHelper(boost::shared_ptr<HappyHelper>(new HappyHelper(*other.happyHelper))),
          healthHelper(boost::shared_ptr<HealthHelper>(new HealthHelper(*other.healthHelper))),
          maintenanceHelper(boost::shared_ptr<MaintenanceHelper>(new MaintenanceHelper(*other.maintenanceHelper))),
          religionHelper(boost::shared_ptr<ReligionHelper>(new ReligionHelper(*other.religionHelper))),
          cultureHelper(boost::shared_ptr<CultureHelper>(new CultureHelper(*other.cultureHelper))),
          tradeRouteHelper(boost::shared_ptr<TradeRouteHelper>(new TradeRouteHelper(*other.tradeRouteHelper))),
          buildingHelper(boost::shared_ptr<BuildingHelper>(new BuildingHelper(*other.buildingHelper))),
          bonusHelper(boost::shared_ptr<BonusHelper>(new BonusHelper(*other.bonusHelper))),
          specialistHelper(boost::shared_ptr<SpecialistHelper>(new SpecialistHelper(*other.specialistHelper))),
          corporationHelper(boost::shared_ptr<CorporationHelper>(new CorporationHelper(*other.corporationHelper))),
          specialConditions(other.specialConditions),
          queuedBuildings(other.queuedBuildings),
          pCity(other.pCity), owner(other.owner), coords(other.coords), includeUnclaimedPlots_(other.includeUnclaimedPlots_)
    {
    }

    boost::shared_ptr<CityData> CityData::clone() const
    {
        return boost::shared_ptr<CityData>(new CityData(*this));
    }

    void CityData::calcOutputsFromPlotData_()
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner);
        const boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis = gGlobals.getGame().getAltAI()->getPlayer(player.getID())->getAnalysis();
        const boost::shared_ptr<MapAnalysis>& pMapAnalysis = pPlayerAnalysis->getMapAnalysis();
        const int timeHorizon = pPlayerAnalysis->getTimeHorizon();

        CityPlotIter iter(pCity);  // ok to use city here, as called from main ctor
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
                    ImprovementTypes improvementType = pLoopPlot->getImprovementType();
                    FeatureTypes featureType = pLoopPlot->getFeatureType();
                    PlotYield plotYield(pLoopPlot->getYield());
                    TotalOutput plotOutput(makeOutput(plotYield, yieldModifier_, commerceModifier_, commercePercent_));
                     // todo - check adding improvement later to plot will work (if there are any plots which have zero yield without improvement)
                    if (isEmpty(plotYield) || !canWork_(pLoopPlot))  // todo use controlled and abletowork flags properly
                        //pLoopPlot->getOwner() == owner && pLoopPlot->getWorkingCity() && pMapAnalysis->getWorkingCity(pCity->getIDInfo(), XYCoords(pLoopPlot->getX(), pLoopPlot->getY())) != pCity)
                    {
                        continue;  // no point in adding desert, or plots worked by other cities we own (add ones we don't own, in case we end up owning them)
                    }

                    PlotData plot(plotYield, Commerce(), plotOutput, GreatPersonOutput(), XYCoords(pLoopPlot->getX(), pLoopPlot->getY()),
                        improvementType, featureType, pLoopPlot->getRouteType(), PlotData::CultureData(pLoopPlot, player.getID(), pCity));

                    // TODO - incorporate into ctor
                    plot.controlled = plot.cultureData.ownerAndCultureTrumpFlag.first == player.getID() || plot.cultureData.ownerAndCultureTrumpFlag.first == NO_PLAYER && includeUnclaimedPlots_;
                    plot.ableToWork = true;
                    
                    if (improvementType != NO_IMPROVEMENT)
                    {
                        ImprovementTypes upgradeImprovementType = (ImprovementTypes)gGlobals.getImprovementInfo(improvementType).getImprovementUpgrade();

                        if (upgradeImprovementType != NO_IMPROVEMENT)
			            {
                            const PlotInfo::PlotInfoNode& plotInfo = pMapAnalysis->getPlotInfoNode(pLoopPlot);
                            plot.upgradeData = PlotData::UpgradeData(timeHorizon,
                                getUpgradedImprovementsData(plotInfo, player.getID(), improvementType, pLoopPlot->getUpgradeTimeLeft(improvementType, player.getID()), 
                                    timeHorizon, player.getImprovementUpgradeRate()));

                            plot.output += plot.upgradeData.getExtraOutput(yieldModifier_, commerceModifier_, commercePercent_);
                        }
                    }

                    plot.controlled ? plotOutputs.push_back(plot) : unworkablePlots.push_back(plot);
                }
            }
        }

        calculateSpecialistOutput_();
    }

    void CityData::calcOutputsFromPlannedImprovements_(const CityImprovementManager& improvements)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner);
        const boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis = gGlobals.getGame().getAltAI()->getPlayer(player.getID())->getAnalysis();
        const boost::shared_ptr<MapAnalysis>& pMapAnalysis = pPlayerAnalysis->getMapAnalysis();
        const int timeHorizon = pPlayerAnalysis->getTimeHorizon();

        calcCityOutput_();

        // typedef boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, PlotYield, TotalOutput, ImprovementState, int /*ImprovementFlags*/> PlotImprovementData;
        const std::vector<CityImprovementManager::PlotImprovementData>& plotImprovements = improvements.getImprovements();

        CityPlotIter iter(pCity);  // ok to use city here, as called from main ctor
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

                    TotalOutput plotOutput(makeOutput(plotYield, yieldModifier_, commerceModifier_, commercePercent_));

                    if (isEmpty(plotYield))  // skip canWork_ check if setting up from planned improvements, as this is looking ahead
                    {
                        continue;  // no point in adding desert, or plots worked by other cities we own (add ones we don't own, in case we end up owning them)
                    }

                    PlotData plot(plotYield, Commerce(), plotOutput, GreatPersonOutput(), XYCoords(pLoopPlot->getX(), pLoopPlot->getY()),
                        improvementType, featureType, pLoopPlot->getRouteType(), PlotData::CultureData(pLoopPlot, player.getID(), pCity));

                    // TODO - incorporate into ctor
                    plot.controlled = plot.cultureData.ownerAndCultureTrumpFlag.first == player.getID() || plot.cultureData.ownerAndCultureTrumpFlag.first == NO_PLAYER && includeUnclaimedPlots_;
                    plot.ableToWork = true;
                    
                    if (improvementType != NO_IMPROVEMENT)
                    {
                        ImprovementTypes upgradeImprovementType = (ImprovementTypes)gGlobals.getImprovementInfo(improvementType).getImprovementUpgrade();

                        if (upgradeImprovementType != NO_IMPROVEMENT)
			            {
                            const PlotInfo::PlotInfoNode& plotInfo = pMapAnalysis->getPlotInfoNode(pLoopPlot);
                            plot.upgradeData = PlotData::UpgradeData(timeHorizon,
                                getUpgradedImprovementsData(plotInfo, player.getID(), improvementType, pLoopPlot->getUpgradeTimeLeft(improvementType, player.getID()), 
                                    timeHorizon, player.getImprovementUpgradeRate()));

                            plot.output += plot.upgradeData.getExtraOutput(yieldModifier_, commerceModifier_, commercePercent_);
                        }
                    }

                    plot.controlled ? plotOutputs.push_back(plot) : unworkablePlots.push_back(plot);
                }
            }
        }

        calculateSpecialistOutput_();
    }

    void CityData::calculateSpecialistOutput_()
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner);
        int defaultSpecType = gGlobals.getDefineINT("DEFAULT_SPECIALIST");
        int freeSpecialistsCount = specialistHelper->getTotalFreeSpecialistSlotCount();
        
        // TODO - deal with forced specialists? (if you can have specs which require other specs to be valid)
        for (int specialistType = 0, count = gGlobals.getNumSpecialistInfos(); specialistType < count; ++specialistType)
        {
            PlotYield yield(getSpecialistYield(player, (SpecialistTypes)specialistType));
            Commerce commerce(getSpecialistCommerce(player, (SpecialistTypes)specialistType));
            TotalOutput specialistOutput(makeOutput(yield, commerce, yieldModifier_, commerceModifier_, commercePercent_));

            PlotData specialistData(yield, commerce, specialistOutput, 
                GameDataAnalysis::getSpecialistUnitTypeAndOutput((SpecialistTypes)specialistType, player.getID()), XYCoords(-1, specialistType));

            // default specialists are unlimited
            if (defaultSpecType == specialistType || player.isSpecialistValid((SpecialistTypes)specialistType))
            {
                // store specialist type in coords data so we know what we picked
                plotOutputs.insert(plotOutputs.end(), workingPopulation, specialistData);

                if (freeSpecialistsCount > 0)
                {
                    freeSpecOutputs.insert(freeSpecOutputs.end(), freeSpecialistsCount, specialistData);
                }
            }
            else
            {
                int maxSpecialistCount = specialistHelper->getMaxSpecialistCount((SpecialistTypes)specialistType);
                plotOutputs.insert(plotOutputs.end(), maxSpecialistCount, specialistData);

                if (freeSpecialistsCount > 0)
                {
                    freeSpecOutputs.insert(freeSpecOutputs.end(), std::min<int>(freeSpecialistsCount, maxSpecialistCount), specialistData);
                }
            }
        }
    }

    void CityData::recalcOutputs()
    {
        if (tradeRouteHelper->needsRecalc())
        {
            tradeRouteHelper->updateTradeRoutes();
        }

        calcCityOutput_();
        cityPlotOutput.actualOutput = cityPlotOutput.output = makeOutput(cityPlotOutput.plotYield, cityPlotOutput.commerce, yieldModifier_, commerceModifier_, commercePercent_);

        for (PlotDataListIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
        {
            iter->actualOutput = iter->output = makeOutput(iter->plotYield, iter->commerce, yieldModifier_, commerceModifier_, commercePercent_);
            if (!iter->upgradeData.upgrades.empty())
            {
                iter->output += iter->upgradeData.getExtraOutput(yieldModifier_, commerceModifier_, commercePercent_);
            }
        }

        for (PlotDataListIter iter(freeSpecOutputs.begin()), endIter(freeSpecOutputs.end()); iter != endIter; ++iter)
        {
            iter->actualOutput = iter->output = makeOutput(iter->plotYield, iter->commerce, yieldModifier_, commerceModifier_, commercePercent_);
        }
    }

    void CityData::setYieldModifier(YieldModifier yieldModifier)
    { 
        yieldModifier_ = yieldModifier; 
        recalcOutputs();
    }

    void CityData::changeYieldModifier(YieldModifier yieldModifier)
    { 
        yieldModifier_ += yieldModifier; 
        recalcOutputs();
    }

    void CityData::setCommerceModifier(CommerceModifier commerceModifier)
    {
        commerceModifier_ = commerceModifier; 
        recalcOutputs();
    }

    void CityData::changeCommerceModifier(CommerceModifier commerceModifier)
    { 
        commerceModifier_ += commerceModifier; 
        recalcOutputs();
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

    void CityData::advanceTurn()
    {
        // TODO create event when improvements upgrade
        doUpgrades_();

        TotalOutput totalOutput(getOutput());

        // TODO handle builds that use food as well as production
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        int foodDelta = totalOutput[OUTPUT_FOOD] - 100 * (cityPopulation * foodPerPop) - getLostFood();
        currentFood += foodDelta;
        storedFood += foodDelta;
        storedFood = range(storedFood, 0, (growthThreshold * foodKeptPercent) / 100);

        if (currentFood >= growthThreshold)
        {
            changePopulation(1);
        }
        else if (currentFood < 0)
        {
            currentFood = 0;
            changePopulation(-1);
        }

        currentProduction += (totalOutput[OUTPUT_PRODUCTION] * currentProductionModifier) / yieldModifier_[YIELD_PRODUCTION];

        cultureHelper->advanceTurn(*this, includeUnclaimedPlots_);

        if (!queuedBuildings.empty())
        {
            // TODO - consider food consumption too
            totalOutput[OUTPUT_PRODUCTION] = 0;  // don't count production consumed by building as output
        }

        if (currentProduction >= requiredProduction && !queuedBuildings.empty())
        {
            BuildingTypes completedBuilding = queuedBuildings.top();
            buildingHelper->changeNumRealBuildings(completedBuilding);
            events_.push(CitySimulationEventPtr(new TestBuildingBuilt(completedBuilding)));

            // TODO handle overflow properly using logic in HurryHelper
            currentProduction = currentProduction - requiredProduction;
            completeBuilding_();
            requiredProduction = -1;
        }

        happyHelper->advanceTurn();
        healthHelper->advanceTurn();
        
        changeWorkingPopulation();
        checkHappyCap();
    }

    void CityData::setBuilding(BuildingTypes buildingType)
    {
        currentProductionModifier -= getBuildingProductionModifier_();

        if (buildingType != NO_BUILDING)
        {
            queuedBuildings.push(buildingType);
            currentProductionModifier += getBuildingProductionModifier_();

            requiredProduction = 100 * (pCity->getProductionNeeded(buildingType) - pCity->getBuildingProduction(buildingType));
        }
        else
        {
            requiredProduction = -1;
        }
    }

    void CityData::hurry(const HurryData& hurryData)
    {
        if (hurryData.hurryPopulation > 0)
        {
            changePopulation(-hurryData.hurryPopulation);
            happyHelper->getHurryHelper().updateAngryTimer();
        }
        
        currentProduction = requiredProduction + hurryData.extraProduction;
    }

    std::pair<bool, HurryData> CityData::canHurry(HurryTypes hurryType) const
    {
        return happyHelper->getHurryHelper().canHurry(hurryType, shared_from_this());
    }

    void CityData::completeBuilding_()
    {
        currentProductionModifier -= getBuildingProductionModifier_();
        queuedBuildings.pop();
    }

    int CityData::getNonWorkingPopulation() const
    {
        return range(happyHelper->angryPopulation() - happyHelper->happyPopulation(), 0, cityPopulation);
    }

    void CityData::changePopulation(int change)
    {
        cityPopulation += change;
        cityPopulation = std::max<int>(1, cityPopulation);

        maintenanceHelper->setPopulation(cityPopulation);
        happyHelper->setPopulation(cityPopulation);
        healthHelper->setPopulation(cityPopulation);
        tradeRouteHelper->setPopulation(cityPopulation);

        events_.push(CitySimulationEventPtr(new PopChange(change)));
        changeWorkingPopulation();

        growthThreshold = 100 * CvPlayerAI::getPlayer(owner).getGrowthThreshold(cityPopulation);

        if (change > 0)
        {
            currentFood = storedFood;
        }
    }

    void CityData::changeWorkingPopulation()
    {
        int newWorkingPopulation = cityPopulation - getNonWorkingPopulation();
        if (newWorkingPopulation != workingPopulation)
        {
            events_.push(CitySimulationEventPtr(new WorkingPopChange(newWorkingPopulation - workingPopulation)));
            workingPopulation = newWorkingPopulation;
        }
    }

    void CityData::checkHappyCap()
    {
        int newHappyCap = happyHelper->happyPopulation() - happyHelper->angryPopulation();
        if (newHappyCap != happyCap)
        {
            events_.push(CitySimulationEventPtr(new HappyCapChange(newHappyCap - happyCap)));
            happyCap = newHappyCap;
        }
    }

    int CityData::getFood() const
    {
        int actualYield = cityPlotOutput.actualOutput[YIELD_FOOD];
        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                actualYield += iter->actualOutput[YIELD_FOOD];
            }
        }

        for (PlotDataListConstIter iter(freeSpecOutputs.begin()), endIter(freeSpecOutputs.end()); iter != endIter; ++iter)
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
        return 100 * ::abs(std::min<int>(0, healthHelper->goodHealth() - healthHelper->badHealth()));
    }

    int CityData::getNumUncontrolledPlots(bool includeOnlyOwnedPlots) const
    {
        if (includeOnlyOwnedPlots)
        {
            int count = 0;
            for (PlotDataListConstIter iter(unworkablePlots.begin()), endIter(unworkablePlots.end()); iter != endIter; ++iter)
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
            return unworkablePlots.size();
        }
    }

    void CityData::debugBasicData(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        os << "Pop: " << cityPopulation << ", angry = " << happyHelper->angryPopulation() << ", happy = " << happyHelper->happyPopulation()
           << ", working = " << workingPopulation << ", happyCap = " << happyCap << ", unhealthy = " << healthHelper->badHealth() << ", healthy = " << healthHelper->goodHealth()
           << ", currentFood = " << currentFood << ", surplus = " << (getFood() - getLostFood() - 100 * (cityPopulation * foodPerPop)) << ", production = " << getOutput() << " ";
#endif
    }

    void CityData::debugCultureData(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n" << cityPlotOutput.coords.iX << ", " << cityPlotOutput.coords.iY;
        cityPlotOutput.cultureData.debug(os);

        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot())
            {
                os << iter->coords.iX << ", " << iter->coords.iY << " = ";
                iter->cultureData.debug(os);
            }
        }

        for (PlotDataListConstIter iter(unworkablePlots.begin()), endIter(unworkablePlots.end()); iter != endIter; ++iter)
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

        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot())
            {
                os << iter->coords.iX << ", " << iter->coords.iY << " = ";
                iter->upgradeData.debug(os);
            }
        }

        for (PlotDataListConstIter iter(unworkablePlots.begin()), endIter(unworkablePlots.end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot())
            {
                os << iter->coords.iX << ", " << iter->coords.iY << " = ";
                iter->upgradeData.debug(os);
            }
        }
#endif
    }

    int CityData::getBuildingProductionModifier_()
    {
        int productionModifier = 0;
        if (!queuedBuildings.empty())
        {
            BuildingTypes buildingType = queuedBuildings.top();
            productionModifier = buildingHelper->getProductionModifier(buildingType, bonusHelper, religionHelper);
        }

        return productionModifier;
    }

    TotalOutput CityData::getOutput() const
    {
        TotalOutput totalOutput(cityPlotOutput.actualOutput);

        // add trade routes
        totalOutput += makeOutput(tradeRouteHelper->getTradeYield(), yieldModifier_, commerceModifier_, commercePercent_);

        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                totalOutput += iter->actualOutput;
            }
        }

        for (PlotDataListConstIter iter(freeSpecOutputs.begin()), endIter(freeSpecOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                totalOutput += iter->actualOutput;
            }
        }

        return totalOutput;
    }

    // account for lost and consumed food
    TotalOutput CityData::getActualOutput() const
    {
        TotalOutput totalOutput = getOutput();

        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        int foodDelta = totalOutput[OUTPUT_FOOD] - 100 * (cityPopulation * foodPerPop) - getLostFood();
        totalOutput[OUTPUT_FOOD] = foodDelta;
        if (foodDelta > 0 && foodKeptPercent > 0)
        {
            totalOutput[OUTPUT_FOOD] += (foodDelta * foodKeptPercent) / 100;
        }
        
        return totalOutput;
    }

    PlotYield CityData::getPlotYield() const
    {
        PlotYield totalPlotYield(cityPlotOutput.plotYield);

        // add trade routes
        PlotYield tradeYield = tradeRouteHelper->getTradeYield();
        for (int i = 0; i < NUM_YIELD_TYPES; ++i)
        {
            tradeYield[i] = (tradeYield[i] * yieldModifier_[i]) / 100;
        }

        totalPlotYield += tradeYield;

        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                totalPlotYield += iter->plotYield;
            }
        }

        for (PlotDataListConstIter iter(freeSpecOutputs.begin()), endIter(freeSpecOutputs.end()); iter != endIter; ++iter)
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
        GreatPersonOutputMap greatPersonOutputMap(cityGreatPersonOutput);

        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
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

        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
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
        CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
        PlotYield cityYield(pPlot->getYield());
        Commerce cityCommerce;

        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            BuildingTypes buildingType = (BuildingTypes)gGlobals.getCivilizationInfo(pCity->getCivilizationType()).getCivilizationBuildings(i);
            if (buildingType == NO_BUILDING)
            {
                continue;
            }

            int buildingCount = buildingHelper->getNumBuildings(buildingType);
            if (buildingCount > 0)
            {
                const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo((BuildingTypes)buildingType);

                cityYield += buildingCount * (PlotYield(buildingInfo.getYieldChangeArray()) + buildingHelper->getBuildingYieldChange((BuildingClassTypes)buildingInfo.getBuildingClassType()));
                cityCommerce += buildingHelper->getBuildingCommerce(buildingType, religionHelper);

                // add building great people points
                int gppRate = buildingInfo.getGreatPeopleRateChange();

                if (gppRate != 0)
                {
                    UnitClassTypes unitClassType = (UnitClassTypes)buildingInfo.getGreatPeopleUnitClass();
                    if (unitClassType != NO_UNITCLASS)
                    {
                        UnitTypes unitType = getPlayerVersion(owner, unitClassType);
                        if (unitType != NO_UNIT)
	                	{
                            cityGreatPersonOutput[unitType] += buildingCount * gppRate;
                        }
                    }
                }
            }
        }

        // add free specialists (e.g. free priest from TofA, scientists from GLib, but not free spec slots)
        const CvPlayer& player = CvPlayerAI::getPlayer(owner);
        for (int specialistType = 0, count = gGlobals.getNumSpecialistInfos(); specialistType < count; ++specialistType)
        {
            int specCount = specialistHelper->getFreeSpecialistCount((SpecialistTypes)specialistType);
            if (specCount > 0)
            {
                cityYield += specCount * getSpecialistYield(player, (SpecialistTypes)specialistType);
                cityCommerce += specCount * getSpecialistCommerce(player, (SpecialistTypes)specialistType);

                GreatPersonOutput greatPersonOutput = GameDataAnalysis::getSpecialistUnitTypeAndOutput((SpecialistTypes)specialistType, player.getID());
                if (greatPersonOutput.output != 0)
                {
                    cityGreatPersonOutput[greatPersonOutput.unitType] += specCount * greatPersonOutput.output;
                }
            }
        }

        // TODO corps
        cityPlotOutput = PlotData(cityYield, cityCommerce, makeOutput(cityYield, cityCommerce, yieldModifier_, commerceModifier_, commercePercent_), GreatPersonOutput(),
                              coords, NO_IMPROVEMENT, NO_FEATURE, pPlot->getRouteType(), PlotData::CultureData(pPlot, player.getID(), pCity));
    }

    void CityData::doUpgrades_()
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner);
        const boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis = gGlobals.getGame().getAltAI()->getPlayer(player.getID())->getAnalysis();
        const int timeHorizon = pPlayerAnalysis->getTimeHorizon();
        std::vector<std::pair<ImprovementTypes, XYCoords> > upgrades;

        for (PlotDataListIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
        {
            if (!iter->upgradeData.upgrades.empty())
            {
                PlotData::UpgradeData::Upgrade upgrade = iter->upgradeData.advanceTurn();
                if (upgrade.improvementType != NO_IMPROVEMENT)
                {
                    iter->improvementType = upgrade.improvementType;
                    iter->plotYield += upgrade.extraYield;

                    TotalOutput outputChange = makeOutput(upgrade.extraYield, yieldModifier_, commerceModifier_, commercePercent_);
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
        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
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
        const CvPlayer& player = CvPlayerAI::getPlayer(owner);
        PlotYield yield(getSpecialistYield(player, specialistType));
        Commerce commerce(getSpecialistCommerce(player, specialistType));
        TotalOutput specialistOutput(makeOutput(yield, commerce, yieldModifier_, commerceModifier_, commercePercent_));

        plotOutputs.insert(plotOutputs.end(), count, 
            PlotData(yield, commerce, specialistOutput, GameDataAnalysis::getSpecialistUnitTypeAndOutput(specialistType, owner), XYCoords(-1, specialistType)));
    }

    void CityData::changePlayerFreeSpecialistSlotCount(int change)
    {
        specialistHelper->changePlayerFreeSpecialistSlotCount(change);
        updateFreeSpecialistSlots_(change);
    }

    void CityData::changeImprovementFreeSpecialistSlotCount(int change)
    {
        specialistHelper->changeImprovementFreeSpecialistSlotCount(change);
        updateFreeSpecialistSlots_(change);
    }

    void CityData::changeFreeSpecialistCountPerImprovement(ImprovementTypes improvementType, int change)
    {
        // todo - include assignment of plots to city in count (only our plots should count)
        int improvementCount = 0;
        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot() && iter->improvementType == improvementType)
            {
                ++improvementCount;
            }
        }
        specialistHelper->changeFreeSpecialistCountPerImprovement(improvementType, change);
        specialistHelper->changeImprovementFreeSpecialistSlotCount(change * improvementCount);
        updateFreeSpecialistSlots_(change);
    }

    void CityData::updateFreeSpecialistSlots_(int change)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(owner);
        int defaultSpecType = gGlobals.getDefineINT("DEFAULT_SPECIALIST");
        freeSpecOutputs.clear();

        for (int specialistType = 0, specialistCount = gGlobals.getNumSpecialistInfos(); specialistType < specialistCount; ++specialistType)
        {
            int availableSlots = 0;

            if (defaultSpecType == specialistType || player.isSpecialistValid((SpecialistTypes)specialistType))
            {
                availableSlots = std::max<int>(0, std::min<int>(workingPopulation, specialistHelper->getTotalFreeSpecialistSlotCount()));
            }
            else
            {
                int maxSpecialistCount = specialistHelper->getMaxSpecialistCount((SpecialistTypes)specialistType);
                availableSlots = std::max<int>(0, std::min<int>(maxSpecialistCount, specialistHelper->getTotalFreeSpecialistSlotCount()));
            }

            if (availableSlots > 0)
            {
                PlotYield yield(getSpecialistYield(player, (SpecialistTypes)specialistType));
                Commerce commerce(getSpecialistCommerce(player, (SpecialistTypes)specialistType));
                TotalOutput specialistOutput(makeOutput(yield, commerce, yieldModifier_, commerceModifier_, commercePercent_));
                PlotData specialistData(yield, commerce, specialistOutput, 
                    GameDataAnalysis::getSpecialistUnitTypeAndOutput((SpecialistTypes)specialistType, player.getID()), XYCoords(-1, specialistType));

                freeSpecOutputs.insert(freeSpecOutputs.end(), availableSlots, specialistData);
            }
        }
    }

    PlotData CityData::findPlot(XYCoords coords) const
    {
        for (PlotDataListConstIter iter(plotOutputs.begin()), endIter(plotOutputs.end()); iter != endIter; ++iter)
        {
            if (iter->coords == coords)
            {
                return *iter;
            }
        }
        return PlotData();
    }

    bool CityData::canWork_(const CvPlot* pPlot) const
    {
        const CvCity* pWorkingCity = pPlot->getWorkingCity();
        // can't consider if definitely worked by another city which is ours
        // otherwise use cultural control to decide
        if (pWorkingCity && pWorkingCity != pCity && pWorkingCity->getOwner() == pCity->getOwner())
  	    {
    	    return false;
        }

        if (pPlot->plotCheck(PUF_canSiege, owner))
	    {
		    return false;
	    }

        if (pPlot->isWater())
        {
            if (!civHelper->hasTech(GameDataAnalysis::getCanWorkWaterTech()))
            {
                return false;
            }

            if (pPlot->getBlockadedCount(pCity->getTeam()) > 0)
            {
                return false;
            }
        }

        return true;
    }
}