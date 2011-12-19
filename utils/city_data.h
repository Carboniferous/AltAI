#pragma once

#include "./utils.h"
#include "./plot_data.h"
#include "./maintenance_helper.h"
#include "./happy_helper.h"

#include "boost/enable_shared_from_this.hpp"

#include <stack>
#include <queue>

namespace AltAI
{
    class CityOptimiser;
    class CitySimulation;
    class ReligionHelper;
    class CultureHelper;
    class CivHelper;
    class HealthHelper;
    class TradeRouteHelper;
    class BuildingHelper;
    class BonusHelper;
    class SpecialistHelper;
    class CorporationHelper;

    template <class T> class IEvent;

    typedef boost::shared_ptr<IEvent<CitySimulation> > CitySimulationEventPtr;

    enum SpecialConditions
    {
        None, FavourGreatPeople, FavourImprovementUpgrade
    };

    struct HurryData;

    typedef std::list<PlotData> PlotDataList;
    typedef PlotDataList::const_iterator PlotDataConstIter;
    typedef PlotDataList::iterator PlotDataIter;

    struct CityData : boost::enable_shared_from_this<CityData>
    {
        explicit CityData(const CvCity* pCity_, bool includeUnclaimedPlots = false);
        CityData(const CityData& other);

        boost::shared_ptr<CityData> clone() const;

        void calcOutputs();
        void recalcOutputs();

        void advanceTurn();

        void setBuilding(BuildingTypes buildingType);
        int getNumBuildings(BuildingTypes buildingType) const;
        void hurry(const HurryData& hurryData);
        std::pair<bool, HurryData> canHurry(HurryTypes hurryType) const;

        void changePopulation(int change);
        void changeWorkingPopulation();
        void checkHappyCap();

        int happyPopulation() const
        {
            return happyHelper->happyPopulation();
        }

        int angryPopulation() const
        {
            return happyHelper->angryPopulation();
        }

        int getNonWorkingPopulation() const;

        TotalOutput getOutput() const;
        TotalOutput getActualOutput() const;  // account for lost and consumed food
        PlotYield getPlotYield() const;
        GreatPersonOutputMap getGPP() const;
        std::set<UnitTypes> getAvailableGPTypes() const;
        int getFood() const;
        int getLostFood() const;

        int getNumUncontrolledPlots(bool includeOnlyOwnedPlots) const;

        void debugBasicData(std::ostream& os) const;
        void debugCultureData(std::ostream& os) const;
        void debugUpgradeData(std::ostream& os) const;

        PlotDataList plotOutputs, unworkablePlots;
        PlotDataList freeSpecOutputs;
        PlotData cityPlotOutput;
        GreatPersonOutputMap cityGreatPersonOutput;

        PlotData findPlot(XYCoords coords) const;

        int getNumPossibleSpecialists(SpecialistTypes specialistType) const;
        // todo - call this when grow if have unlimited specs
        void addSpecialistSlots(SpecialistTypes specialistType, int count);
        void changePlayerFreeSpecialistSlotCount(int change);
        void changeImprovementFreeSpecialistSlotCount(int change);
        void changeFreeSpecialistCountPerImprovement(ImprovementTypes improvementType, int change);

        int cityPopulation, workingPopulation, happyCap;

        int currentFood, storedFood, currentProduction;
        int growthThreshold, requiredProduction;
        int foodKeptPercent;

        YieldModifier getYieldModifier() const { return yieldModifier_; }
        void setYieldModifier(YieldModifier yieldModifier);
        void changeYieldModifier(YieldModifier yieldModifier);

        CommerceModifier getCommerceModifier() const { return commerceModifier_; }
        void setCommerceModifier(CommerceModifier commerceModifier);
        void changeCommerceModifier(CommerceModifier commerceModifier);

        CommerceModifier getCommercePercent() const { return commercePercent_; }
        void setCommercePercent(CommerceModifier commercePercent);
        void changeCommercePercent(CommerceModifier commercePercent);

        int currentProductionModifier;

        boost::shared_ptr<CivHelper> civHelper;
        boost::shared_ptr<HappyHelper> happyHelper;
        boost::shared_ptr<HealthHelper> healthHelper;
        boost::shared_ptr<MaintenanceHelper> maintenanceHelper;
        boost::shared_ptr<ReligionHelper> religionHelper;
        boost::shared_ptr<CultureHelper> cultureHelper;
        boost::shared_ptr<TradeRouteHelper> tradeRouteHelper;
        boost::shared_ptr<BuildingHelper> buildingHelper;
        boost::shared_ptr<BonusHelper> bonusHelper;
        boost::shared_ptr<SpecialistHelper> specialistHelper;
        boost::shared_ptr<CorporationHelper> corporationHelper;

        SpecialConditions specialConditions;

        std::stack<BuildingTypes> queuedBuildings;

        const CvCity* pCity;
        PlayerTypes owner;
        XYCoords coords;

        CitySimulationEventPtr getEvent();
        void pushEvent(const CitySimulationEventPtr& event);

    private:
        int getBuildingProductionModifier_();
        void completeBuilding_();
        int setBuilding_();
        void calcCityOutput_();
        void doUpgrades_();
        void updateFreeSpecialistSlots_(int change);
        bool canWork_(const CvPlot* pPlot) const;

        YieldModifier yieldModifier_;
        CommerceModifier commerceModifier_, commercePercent_;

        std::queue<CitySimulationEventPtr> events_;
        bool includeUnclaimedPlots_;
    };
}