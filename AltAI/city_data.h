#pragma once

#include "./utils.h"
#include "./plot_data.h"
#include "./hurry_helper.h"
#include "./city_improvements.h"

namespace AltAI
{
    class CityOptimiser;
    class CitySimulation;

    class AreaHelper;
    class BonusHelper;
    class BuildingsHelper;
    class CivHelper;
    class CorporationHelper;
    class CultureHelper;
    class ModifiersHelper;
    class HappyHelper;
    class HealthHelper;
    class HurryHelper;      
    class MaintenanceHelper;
    class ReligionHelper;
    class SpecialistHelper;
    class TradeRouteHelper;
    class UnitHelper;
    class VoteHelper;

    typedef boost::shared_ptr<AreaHelper> AreaHelperPtr;
    typedef boost::shared_ptr<BonusHelper> BonusHelperPtr;
    typedef boost::shared_ptr<BuildingsHelper> BuildingsHelperPtr;
    typedef boost::shared_ptr<CivHelper> CivHelperPtr;
    typedef boost::shared_ptr<CorporationHelper> CorporationHelperPtr;
    typedef boost::shared_ptr<CultureHelper> CultureHelperPtr;
    typedef boost::shared_ptr<ModifiersHelper> ModifiersHelperPtr;
    typedef boost::shared_ptr<HappyHelper> HappyHelperPtr;
    typedef boost::shared_ptr<HealthHelper> HealthHelperPtr;
    typedef boost::shared_ptr<HurryHelper> HurryHelperPtr;
    typedef boost::shared_ptr<MaintenanceHelper> MaintenanceHelperPtr;
    typedef boost::shared_ptr<ReligionHelper> ReligionHelperPtr;
    typedef boost::shared_ptr<SpecialistHelper> SpecialistHelperPtr;
    typedef boost::shared_ptr<TradeRouteHelper> TradeRouteHelperPtr;
    typedef boost::shared_ptr<UnitHelper> UnitHelperPtr;
    typedef boost::shared_ptr<VoteHelper> VoteHelperPtr;

    template <class T> class IEvent;

    typedef boost::shared_ptr<IEvent<CitySimulation> > CitySimulationEventPtr;

    enum SpecialConditions
    {
        None, FavourGreatPeople, FavourImprovementUpgrade
    };

    typedef std::list<PlotData> PlotDataList;
    typedef PlotDataList::const_iterator PlotDataListConstIter;
    typedef PlotDataList::iterator PlotDataListIter;
    
    typedef boost::shared_ptr<CityData> CityDataPtr;
    typedef boost::shared_ptr<const CityData> ConstCityDataPtr;

    class CityData// : public boost::enable_shared_from_this<CityData>
    {
        friend class CultureHelper;

    public:
        ~CityData();

        explicit CityData(const CvCity* pCity_, bool includeUnclaimedPlots = false, int lookaheadDepth = 0);
        CityData(const CvCity* pCity_, const std::vector<PlotImprovementData>& improvements, bool includeUnclaimedPlots = false);

        void addPlot(const CvPlot* pPlot);

        CityDataPtr clone() const;

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
        void doImprovementUpgrades(int nTurns);

        // current food, stored food
        std::pair<int, int> getAccumulatedFood(int nTurns);

        void pushBuilding(BuildingTypes buildingType);
        void pushUnit(UnitTypes unitType);
        void pushProcess(ProcessTypes processType);
        void clearBuildQueue();

        bool isFoodProductionBuildItem() const;

        int getNumBuildings(BuildingTypes buildingType) const;
        void hurry(const HurryData& hurryData);
        std::pair<bool, HurryData> canHurry(HurryTypes hurryType) const;

        const std::stack<std::pair<BuildQueueTypes, int> >& getBuildQueue() const
        {
            return buildQueue_;
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

        void setCurrentFood(int value);
        void setStoredFood(int value);

        int getGrowthThreshold() const
        {
            return growthThreshold_;
        }

        // first = turns (MAX_INT if never), second = pop change (+1, -1, 0)
        std::pair<int, int> getTurnsToPopChange() const;

        int happyPopulation() const;
        int angryPopulation() const;

        int getHappyCap() const
        {
            return happyCap_;
        }

        int getNonWorkingPopulation() const;
        
        TotalOutput getOutput() const;
        TotalOutput getProcessOutput() const;
        TotalOutput getActualOutput() const;  // account for lost and consumed food
        PlotYield getPlotYield() const;
        GreatPersonOutputMap getGPP() const;
        std::set<UnitTypes> getAvailableGPTypes() const;
        int getFood() const;
        int getLostFood() const;

        int getGoldenAgeTurns() const
        {
            return goldenAgeTurns_;
        }

        int getCurrentProduction() const;

        int getRequiredProduction() const
        {
            return requiredProduction_;
        }

        int getAccumulatedProduction() const
        {
            return accumulatedProduction_;
        }

        void updateProduction(int nTurns);

        int getCommerceYieldModifier() const
        {
            return commerceYieldModifier_;
        }

        void changeCommerceYieldModifier(int change);

        int getNumUncontrolledPlots(bool includeOnlyOwnedPlots) const;

        void debugBasicData(std::ostream& os) const;
        void debugCultureData(std::ostream& os) const;
        void debugUpgradeData(std::ostream& os) const;

        const PlotData& getCityPlotData() const
        {
            return cityPlotOutput_;
        }

        PlotData& getCityPlotData()
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

        int getNextImprovementUpgradeTime() const;

        PlotDataListIter findPlot(XYCoords coords);
        PlotDataListConstIter findPlot(XYCoords coords) const;

        int getSpecialistSlotCount() const;
        int getNumPossibleSpecialists(SpecialistTypes specialistType) const;
        // todo - call this when grow if have unlimited specs
        void addSpecialistSlots(SpecialistTypes specialistType, int count);
        void changePlayerFreeSpecialistSlotCount(int change);
        void changeImprovementFreeSpecialistSlotCount(int change);
        void changeFreeSpecialistCountPerImprovement(ImprovementTypes improvementType, int change);

        std::vector<SpecialistTypes> getBestMixedSpecialistTypes() const;

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

        UnitHelperPtr& getUnitHelper()
        {
            return unitHelper_;
        }

        const UnitHelperPtr& getUnitHelper() const
        {
            return unitHelper_;
        }

        VoteHelperPtr& getVoteHelper()
        {
            return voteHelper_;
        }

        const VoteHelperPtr& getVoteHelper() const
        {
            return voteHelper_;
        }

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        void debugSummary(std::ostream& os) const;

    private:
        CityData(const CityData& other);

        void initHelpers_(const CvCity* pCity);
        void init_(const CvCity* pCity);
        void initPlot_(const CvPlot* pPlot, PlotYield plotYield, ImprovementTypes improvementType, 
                       BonusTypes bonusType, FeatureTypes featureType, RouteTypes routeType);

        int getCurrentProduction_(TotalOutput currentOutput) const;
        int getCurrentProductionModifier_() const;

        void calcOutputsFromPlotData_(const CvCity* pCity, int lookaheadDepth);
        void calcOutputsFromPlannedImprovements_(const std::vector<PlotImprovementData>& improvements);
        void calcCityOutput_();
        void calculateSpecialistOutput_();
        void recalcBestSpecialists_();
        
        void updateFreeSpecialistSlots_(int change);
        
        bool canWork_(const CvPlot* pPlot, int lookaheadDepth) const;
        bool avoidGrowth_() const;

        int cityPopulation_, workingPopulation_, happyCap_;

        int currentFood_, storedFood_, accumulatedProduction_;
        int growthThreshold_, requiredProduction_;
        int foodKeptPercent_;
        int commerceYieldModifier_;

        SpecialConditions specialConditions_;

        std::stack<std::pair<BuildQueueTypes, int> > buildQueue_;

        const CvCity* pCity_;
        PlayerTypes owner_;
        XYCoords coords_;

        YieldModifier yieldModifier_;
        CommerceModifier commerceModifier_, commercePercent_;

        PlotDataList plotOutputs_, unworkablePlots_;
        PlotDataList freeSpecOutputs_;
        PlotData cityPlotOutput_;
        GreatPersonOutputMap cityGreatPersonOutput_;
        std::vector<SpecialistTypes> bestMixedSpecialistTypes_;

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
        UnitHelperPtr unitHelper_;
        VoteHelperPtr voteHelper_;

        std::queue<CitySimulationEventPtr> events_;
        bool includeUnclaimedPlots_;
        int goldenAgeTurns_;
    };
}