#pragma once

#include "./utils.h"
#include "./city_data.h"
#include "./city_optimiser.h"
#include "./tactic_actions.h"
#include "./city_projections.h"

namespace AltAI
{
    class CityImprovementManager;
    typedef boost::shared_ptr<CityImprovementManager> CityImprovementManagerPtr;

    class Player;

    class City
    {
    public:
        enum Flags
        {
            NoCalc = 0, NeedsBuildingCalcs = (1 << 0), NeedsBuildSelection = (1 << 1), NeedsImprovementCalcs = (1 << 2), 
            CanReassignSharedPlots = (1 << 3), NeedsProjectionCalcs = (1 << 4), NeedsCityDataCalc = (1 << 5)
        };

        explicit City(CvCity* pCity);
        void init();

        void doTurn();

        int getID() const;

        XYCoords getCoords() const;
        const CvCity* getCvCity() const;

        boost::tuple<UnitTypes, BuildingTypes, ProcessTypes, ProjectTypes> getBuild();
        const ConstructItem& getConstructItem() const;

        bool selectImprovement(CvUnitAI* pUnit, bool simulatedOnly);
        bool connectCities(CvUnitAI* pUnit) const;
        bool connectCity(CvUnitAI* pUnit, CvCity* pDestCity) const;

        std::pair<XYCoords, BuildTypes> getBestImprovement(const std::string& sourceFunc, CvUnitAI* pUnit, bool simulatedOnly);
        std::pair<XYCoords, BuildTypes> getBestBonusImprovement(bool isWater);
        std::pair<BuildTypes, int> getBestImprovement(XYCoords coords, const std::string& sourceFunc);

        bool checkResourceConnections(CvUnitAI* pUnit) const;
        bool checkBadImprovementFeatures(CvUnitAI* pUnit) const;
        bool checkIrrigation(CvUnitAI* pUnit, bool onlyForResources);

        int getNumReqdWorkers() const;
        int getNumWorkers() const;
        int getNumWorkersTargetingPlot(XYCoords targetCoords) const;

        TotalOutput getMaxOutputs() const;
        TotalOutputWeights getMaxOutputWeights() const;

        void updateBuildings(BuildingTypes buildingType, int count);
        void updateUnits(UnitTypes unitType);
        void updateImprovements(const CvPlot* pPlot, ImprovementTypes improvementType);
        void updateRoutes(const CvPlot* pPlot, RouteTypes routeType);

        void logImprovements() const;

        void setFlag(int flags);
        int getFlags() const;

        void assignPlots();
        void optimisePlots(const CityDataPtr& pCityData, const ConstructItem& constructItem, bool debug = false) const;
        void calcImprovements();

        PlotAssignmentSettings getPlotAssignmentSettings() const;

        const CityDataPtr& getCityData();
        const CityImprovementManagerPtr getCityImprovementManager() const;
        const ProjectionLadder& getCurrentOutputProjection();
        const ProjectionLadder& getBaseOutputProjection();
        const CityDataPtr& getProjectionCityData();
        const CityDataPtr& getBaseProjectionCityData();

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        void updateProjections_();
        void calcMaxOutputs_();

        //bool sanityCheckBuilding_(BuildingTypes buildingType) const;
        //bool sanityCheckUnit_(UnitTypes unitType) const;
        void checkConstructItem_();

        std::pair<XYCoords, BuildTypes> getImprovementBuildOrder_(XYCoords coords, ImprovementTypes improvementType);

        Player& player_;
        CvCity* pCity_;
        CityDataPtr pCityData_;
        CityImprovementManagerPtr pCityImprovementManager_;

        TotalOutput maxOutputs_;
        TotalOutputWeights optWeights_;
        PlotAssignmentSettings plotAssignmentSettings_;
        ProjectionLadder currentOutputProjection_, baseOutputProjection_;
        CityDataPtr pProjectionCityData_, pBaseProjectionCityData_;

        int flags_;
        ConstructItem constructItem_;

        // debug
        void debugGreatPeople_();
    };
}