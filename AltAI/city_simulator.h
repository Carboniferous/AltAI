#pragma once

#include "./utils.h"
#include "./city_optimiser.h"
#include "./events.h"
#include "./city_log.h"
#include "./plot_info.h"
#include "./tactic_actions.h"

namespace AltAI
{
    //class CitySimulator;

    class PopChange : public IEvent<CitySimulation>
    {
    public:
        explicit PopChange(int change) : change_(change)
        {
        }
        virtual void handleEvent(CitySimulation&);
        virtual void stream(std::ostream& os);

    private:
        int change_;
    };

    class TestBuildingBuilt : public IEvent<CitySimulation>
    {
    public:
        TestBuildingBuilt(BuildingTypes buildingType) : buildingType_(buildingType)
        {
        }
        virtual void handleEvent(CitySimulation&);
        virtual void stream(std::ostream& os);

    private:
        BuildingTypes buildingType_;
    };

    class WorkingPopChange : public IEvent<CitySimulation>
    {
    public:
        explicit WorkingPopChange(int change) : change_(change)
        {
        }
        virtual void handleEvent(CitySimulation&);
        virtual void stream(std::ostream& os);

    private:
        int change_;
    };

    class HappyCapChange : public IEvent<CitySimulation>
    {
    public:
        explicit HappyCapChange(int change) : change_(change)
        {
        }
        virtual void handleEvent(CitySimulation&);
        virtual void stream(std::ostream& os);

    private:
        int change_;
    };

    class ImprovementUpgrade : public IEvent<CitySimulation>
    {
    public:
        ImprovementUpgrade(const std::vector<std::pair<ImprovementTypes, XYCoords> >& data) : data_(data)
        {
        }
        virtual void handleEvent(CitySimulation&);
        virtual void stream(std::ostream& os);

    private:
        std::vector<std::pair<ImprovementTypes, XYCoords> > data_;
    };

    class CultureBorderExpansion : public IEvent<CitySimulation>
    {
    public:
        CultureBorderExpansion()
        {
        }
        virtual void handleEvent(CitySimulation&);
        virtual void stream(std::ostream& os);

    private:
    };

    class PlotControlChange : public IEvent<CitySimulation>
    {
    public:
        explicit PlotControlChange(const std::vector<std::pair<PlayerTypes, XYCoords> >& data) : data_(data)
        {
        }
        virtual void handleEvent(CitySimulation&);
        virtual void stream(std::ostream& os);

    private:
        std::vector<std::pair<PlayerTypes, XYCoords> > data_;
    };

    struct SimulationOutput
    {
        SimulationOutput() : hurryData(NO_HURRY) {}
        void addTurn(const CityDataPtr& pCityData);

        std::vector<TotalOutput> cumulativeOutput;
        std::vector<int> cumulativeCost;
        std::vector<GreatPersonOutputMap> cumulativeGPP;
        std::vector<std::pair<int, int> > popHistory;
        CityDataPtr pCityData;
        boost::shared_ptr<CityOptimiser> pCityOptimiser;
        HurryData hurryData;

        static TotalOutput getDelta(const SimulationOutput& comparison, const SimulationOutput& baseline);
        void debugResults(std::ostream& os) const;
    };

    struct BuildingSimulationResults
    {
        SimulationOutput noBuildingBaseline;
        struct BuildingResult
        {
            explicit BuildingResult(BuildingTypes buildingType_) : buildingType(buildingType_) {}
            BuildingTypes buildingType;
            SimulationOutput baseline, normalBuild;
            std::vector<std::pair<HurryData, SimulationOutput> > hurryBuilds;
        };
        std::vector<BuildingResult> buildingResults;

        int getNormalBuildTime(BuildingTypes buildingType) const;
        void debugResults(std::ostream& os) const;
    };

    typedef std::vector<boost::tuple<FeatureTypes, ImprovementTypes, SimulationOutput> > PlotImprovementSimulationResult;
    typedef std::vector<std::pair<XYCoords, PlotImprovementSimulationResult> > PlotImprovementSimulationResults;

    class CitySimulation
    {
    public:
        friend class TestBuildingBuilt;
        friend class PopChange;

        CitySimulation(const CvCity* pCity, const CityDataPtr& pCityData, const ConstructItem& constructItem = ConstructItem(NO_BUILDING));

        SimulationOutput simulateAsIs(int nTurns, OutputTypes outputType = NO_OUTPUT);

        void simulate(BuildingSimulationResults& results, int nTurns);
        void simulate(BuildingSimulationResults& results, int nTurns, HurryTypes hurryType);

        void optimisePlots();
        void setNeedsOpt() { needsOpt_ = true; }
        void logPlots(bool printAllPlots = false) const;
        boost::shared_ptr<CityOptimiser> getCityOptimiser() const;
        CityDataPtr getCityData() const;

    private:
        void simulateNoHurry_(BuildingSimulationResults::BuildingResult& results, int nTurns);
        void simulateHurry_(BuildingSimulationResults::BuildingResult& results, const HurryData& hurryData, int nTurns);

        const CvCity* pCity_;
        CityDataPtr pCityData_;
        boost::shared_ptr<CityOptimiser> pCityOptimiser_;
        boost::shared_ptr<CityLog> pLog_;
        int turn_;
        SimulationOutput simulationOutput_;
        ConstructItem constructItem_;
        bool buildingBuilt_;
        bool needsOpt_;
        PlotAssignmentSettings plotAssignmentSettings_;

        OutputTypes getOptType_() const;
        
        void doTurn_();
        void handleEvents_();
        void handleBuildingBuilt_(BuildingTypes buildingType);
        void handlePopChange_(int change);
    };
}