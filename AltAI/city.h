#pragma once

#include "./utils.h"
#include "./city_data.h"
#include "./city_optimiser.h"
#include "./tactic_actions.h"
#include "./city_projections.h"

namespace AltAI
{
    class CityImprovementManager;
    class Player;

    class City
    {
    public:
        enum Flags
        {
            NoCalc = 0, NeedsBuildingCalcs = (1 << 0), NeedsBuildSelection = (1 << 2), NeedsImprovementCalcs = (1 << 3), CanReassignSharedPlots = (1 << 4)
        };

        explicit City(CvCity* pCity);
        void init();

        void doTurn();

        int getID() const;

        XYCoords getCoords() const;
        const CvCity* getCvCity() const;

        boost::tuple<UnitTypes, BuildingTypes, ProcessTypes, ProjectTypes> getBuild();
        const ConstructItem& getConstructItem() const;

        bool selectImprovement(CvUnit* pUnit, bool simulatedOnly);
        bool connectCities(CvUnitAI* pUnit) const;

        std::pair<XYCoords, BuildTypes> getBestImprovement(const std::string& sourceFunc, bool simulatedOnly);
        std::pair<XYCoords, BuildTypes> getBestBonusImprovement(bool isWater);
        std::pair<BuildTypes, int> getBestImprovement(XYCoords coords, const std::string& sourceFunc);

        bool checkResourceConnections(CvUnitAI* pUnit) const;
        bool checkBadImprovementFeatures(CvUnitAI* pUnit) const;
        bool checkIrrigation(CvUnitAI* pUnit, bool onlyForResources) const;

        int getNumReqdWorkers() const;
        int getNumWorkers() const;
        int getNumWorkersAtPlot(const CvPlot* pTargetPlot) const;

        TotalOutput getMaxOutputs() const;
        TotalOutputWeights getMaxOutputWeights() const;

        void updateBuildings(BuildingTypes buildingType, int count);
        void updateUnits(UnitTypes unitType);
        void updateImprovements(const CvPlot* pPlot, ImprovementTypes improvementType);

        void logImprovements() const;

        void setFlag(Flags flags);

        void assignPlots();

        PlotAssignmentSettings getPlotAssignmentSettings() const;

        const CityDataPtr& getCityData() const;
        const ProjectionLadder& getCurrentOutputProjection() const;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        void calcMaxOutputs_();
        void calcImprovements_();
        void calcBuildings_();

        bool sanityCheckBuilding_(BuildingTypes buildingType) const;
        bool sanityCheckUnit_(UnitTypes unitType) const;

        std::pair<XYCoords, BuildTypes> getImprovementBuildOrder_(XYCoords coords, ImprovementTypes improvementType, bool wantIrrigationForBonus = false) const;

        Player& player_;
        CvCity* pCity_;
        CityDataPtr pCityData_;

        TotalOutput maxOutputs_;
        TotalOutputWeights optWeights_;
        PlotAssignmentSettings plotAssignmentSettings_;
        ProjectionLadder currentOutputProjection_;

        int flags_;
        ConstructItem constructItem_;

        // debug
        void debugGreatPeople_();
    };
}