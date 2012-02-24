#pragma once

#include "./utils.h"
#include "./plot_data.h"
#include "./area_helper.h"
#include "./bonus_helper.h"
#include "./building_helper.h"
#include "./civ_helper.h"
#include "./corporation_helper.h"
#include "./culture_helper.h"
#include "./happy_helper.h"
#include "./health_helper.h"
#include "./hurry_helper.h"
#include "./maintenance_helper.h"
#include "./modifiers_helper.h"
#include "./religion_helper.h"
#include "./specialist_helper.h"
#include "./trade_route_helper.h"

#include <stack>
#include <queue>

namespace AltAI
{
    class CityOptimiser;
    class CitySimulation;

    template <class T> class IEvent;

    typedef boost::shared_ptr<IEvent<CitySimulation> > CitySimulationEventPtr;

    enum SpecialConditions
    {
        None, FavourGreatPeople, FavourImprovementUpgrade
    };

    class CityImprovementManager;

    typedef std::list<PlotData> PlotDataList;
    typedef PlotDataList::const_iterator PlotDataListConstIter;
    typedef PlotDataList::iterator PlotDataListIter;
    typedef boost::shared_ptr<CityData> CityDataPtr;

    class CityData
    {
        friend class CultureHelper;
    public:
        explicit CityData(const CvCity* pCity_, bool includeUnclaimedPlots = false);
        CityData(const CvCity* pCity_, const CityImprovementManager& improvements);

        boost::shared_ptr<CityData> clone() const;

        const CvCity* getCity() const
        {
            return pCity_;
        }

        PlayerTypes getOwner() const
        {
            return owner_;
        }

        void changeFoodKeptPercent(int change);
        
        void recalcOutputs();

        void advanceTurn();

        void setBuilding(BuildingTypes buildingType);
        int getNumBuildings(BuildingTypes buildingType) const;
        void hurry(const HurryData& hurryData);
        std::pair<bool, HurryData> canHurry(HurryTypes hurryType) const;

        const std::stack<BuildingTypes>& getQueuedBuildings() const
        {
            return queuedBuildings_;
        }

        void changePopulation(int change);
        void changeWorkingPopulation();
        void checkHappyCap();

        int getPopulation() const
        {
            return cityPopulation_;
        }

        int getWorkingPopulation() const
        {
            return workingPopulation_;
        }

        int getCurrentFood() const
        {
            return currentFood_;
        }

        int getStoredFood() const
        {
            return storedFood_;
        }

        int getFoodKeptPercent() const
        {
            return foodKeptPercent_;
        }

        void setStoredFood(int value);

        int getGrowthThreshold() const
        {
            return growthThreshold_;
        }

        int happyPopulation() const
        {
            return getHappyHelper()->happyPopulation();
        }

        int angryPopulation() const
        {
            return getHappyHelper()->angryPopulation();
        }

        int getHappyCap() const
        {
            return happyCap_;
        }

        int getNonWorkingPopulation() const;

        TotalOutput getOutput() const;
        TotalOutput getActualOutput() const;  // account for lost and consumed food
        PlotYield getPlotYield() const;
        GreatPersonOutputMap getGPP() const;
        std::set<UnitTypes> getAvailableGPTypes() const;
        int getFood() const;
        int getLostFood() const;

        int getCurrentProduction() const
        {
            return currentProduction_;
        }

        int getRequiredProduction() const
        {
            return requiredProduction_;
        }

        int getCommerceYieldModifier() const
        {
            return commerceYieldModifier_;
        }

        void changeCommerceYieldModifier(int change);

        int getNumUncontrolledPlots(bool includeOnlyOwnedPlots) const;

        void debugBasicData(std::ostream& os) const;
        void debugCultureData(std::ostream& os) const;
        void debugUpgradeData(std::ostream& os) const;

        const PlotData& getCityPlotOutput() const
        {
            return cityPlotOutput_;
        }

        PlotData& getCityPlotOutput()
        {
            return cityPlotOutput_;
        }

        PlotDataList& getPlotOutputs()
        {
            return plotOutputs_;
        }

        const PlotDataList& getPlotOutputs() const
        {
            return plotOutputs_;
        }

        PlotDataList& getUnworkablePlots()
        {
            return unworkablePlots_;
        }

        const PlotDataList& getUnworkablePlots() const
        {
            return unworkablePlots_;
        }

        PlotDataList& getFreeSpecOutputs()
        {
            return freeSpecOutputs_;
        }

        const PlotDataList& getFreeSpecOutputs() const
        {
            return freeSpecOutputs_;
        }

        PlotData findPlot(XYCoords coords) const;

        int getNumPossibleSpecialists(SpecialistTypes specialistType) const;
        // todo - call this when grow if have unlimited specs
        void addSpecialistSlots(SpecialistTypes specialistType, int count);
        void changePlayerFreeSpecialistSlotCount(int change);
        void changeImprovementFreeSpecialistSlotCount(int change);
        void changeFreeSpecialistCountPerImprovement(ImprovementTypes improvementType, int change);

        CommerceModifier getCommercePercent() const { return commercePercent_; }
        void setCommercePercent(CommerceModifier commercePercent);
        void changeCommercePercent(CommerceModifier commercePercent);

        CitySimulationEventPtr getEvent();
        void pushEvent(const CitySimulationEventPtr& event);

        AreaHelperPtr& getAreaHelper()
        {
            return areaHelper_;
        }

        const AreaHelperPtr& getAreaHelper() const
        {
            return areaHelper_;
        }

        BonusHelperPtr& getBonusHelper()
        {
            return bonusHelper_;
        }

        const BonusHelperPtr& getBonusHelper() const
        {
            return bonusHelper_;
        }

        BuildingsHelperPtr& getBuildingsHelper()
        {
            return buildingsHelper_;
        }

        const BuildingsHelperPtr& getBuildingsHelper() const
        {
            return buildingsHelper_;
        }

        CivHelperPtr& getCivHelper()
        {
            return civHelper_;
        }

        const CivHelperPtr& getCivHelper() const
        {
            return civHelper_;
        }

        CorporationHelperPtr& getCorporationHelper()
        {
            return corporationHelper_;
        }

        const CorporationHelperPtr& getCorporationHelper() const
        {
            return corporationHelper_;
        }

        CultureHelperPtr& getCultureHelper()
        {
            return cultureHelper_;
        }

        const CultureHelperPtr& getCultureHelper() const
        {
            return cultureHelper_;
        }

        ModifiersHelperPtr& getModifiersHelper()
        {
            return modifiersHelper_;
        }

        const ModifiersHelperPtr& getModifiersHelper() const
        {
            return modifiersHelper_;
        }

        HappyHelperPtr& getHappyHelper()
        {
            return happyHelper_;
        }

        const HappyHelperPtr& getHappyHelper() const
        {
            return happyHelper_;
        }

        HealthHelperPtr& getHealthHelper()
        {
            return healthHelper_;
        }

        const HealthHelperPtr& getHealthHelper() const
        {
            return healthHelper_;
        }

        HurryHelperPtr& getHurryHelper()
        {
            return hurryHelper_;
        }

        const HurryHelperPtr& getHurryHelper() const
        {
            return hurryHelper_;
        }

        MaintenanceHelperPtr& getMaintenanceHelper()
        {
            return maintenanceHelper_;
        }

        const MaintenanceHelperPtr& getMaintenanceHelper() const
        {
            return maintenanceHelper_;
        }

        ReligionHelperPtr& getReligionHelper()
        {
            return religionHelper_;
        }

        const ReligionHelperPtr& getReligionHelper() const
        {
            return religionHelper_;
        }

        SpecialistHelperPtr& getSpecialistHelper()
        {
            return specialistHelper_;
        }

        const SpecialistHelperPtr& getSpecialistHelper() const
        {
            return specialistHelper_;
        }

        TradeRouteHelperPtr& getTradeRouteHelper()
        {
            return tradeRouteHelper_;
        }

        const TradeRouteHelperPtr& getTradeRouteHelper() const
        {
            return tradeRouteHelper_;
        }

    private:
        void initHelpers_(const CvCity* pCity);
        void init_(const CvCity* pCity);
        void initPlot_(const CvPlot* pPlot, PlotYield plotYield, ImprovementTypes improvementType, FeatureTypes featureType, RouteTypes routeType);

        int getBuildingProductionModifier_();
        void completeBuilding_();
        int setBuilding_();

        void calcOutputsFromPlotData_(const CvCity* pCity);
        void calcOutputsFromPlannedImprovements_(const CityImprovementManager& improvements);
        void calcCityOutput_();
        void calculateSpecialistOutput_();

        void doUpgrades_();
        void updateFreeSpecialistSlots_(int change);
        bool canWork_(const CvPlot* pPlot) const;

        int cityPopulation_, workingPopulation_, happyCap_;

        int currentFood_, storedFood_, currentProduction_;
        int growthThreshold_, requiredProduction_;
        int foodKeptPercent_;
        int commerceYieldModifier_;

        SpecialConditions specialConditions_;

        std::stack<BuildingTypes> queuedBuildings_;

        const CvCity* pCity_;
        PlayerTypes owner_;
        XYCoords coords_;

        YieldModifier yieldModifier_;
        CommerceModifier commerceModifier_, commercePercent_;

        PlotDataList plotOutputs_, unworkablePlots_;
        PlotDataList freeSpecOutputs_;
        PlotData cityPlotOutput_;
        GreatPersonOutputMap cityGreatPersonOutput_;

        AreaHelperPtr areaHelper_;
        BonusHelperPtr bonusHelper_;
        BuildingsHelperPtr buildingsHelper_;
        CivHelperPtr civHelper_;
        CorporationHelperPtr corporationHelper_;
        CultureHelperPtr cultureHelper_;
        ModifiersHelperPtr modifiersHelper_;
        HappyHelperPtr happyHelper_;
        HealthHelperPtr healthHelper_;
        HurryHelperPtr hurryHelper_;      
        MaintenanceHelperPtr maintenanceHelper_;
        ReligionHelperPtr religionHelper_;
        SpecialistHelperPtr specialistHelper_;
        TradeRouteHelperPtr tradeRouteHelper_;

        std::queue<CitySimulationEventPtr> events_;
        bool includeUnclaimedPlots_;
    };
}