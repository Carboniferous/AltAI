#pragma once

#include "./utils.h"
#include "./unit.h"
#include "./city_projections_ladder.h"

namespace AltAI
{
    class Player;
    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;

    class CityImprovementManager;
    typedef boost::shared_ptr<CityImprovementManager> CityImprovementManagerPtr;

    class ProjectionImprovementEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionImprovementEvent>
    {
    public:
        ~ProjectionImprovementEvent();
        ProjectionImprovementEvent(bool isLand, bool isConsumed, const std::vector<IDInfo>& targetCities, PlayerTypes playerType);

        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        ProjectionImprovementEvent(const CityImprovementManagerPtr& pCityImprovementManager,
            TotalOutputWeights outputWeights,
            bool isLand, bool isConsumed,
            const std::vector<IDInfo>& targetCities,
            BuildTypes currentBuild,
            ImprovementTypes improvementBeingBuilt,
            FeatureTypes featureBeingRemoved,
            XYCoords currentTarget,
            int accumulatedTurns,
            PlayerTypes playerType,
            bool currentBuildIsComplete);

        void calcNextImprovement_();

        CityDataPtr pCityData_;
        CityImprovementManagerPtr pCityImprovementManager_;
        TotalOutputWeights outputWeights_;
        bool isLand_, isConsumed_;
        std::vector<IDInfo> targetCities_;
        BuildTypes currentBuild_;
        ImprovementTypes improvementBeingBuilt_;
        FeatureTypes featureBeingRemoved_;
        XYCoords currentTarget_;
        int accumulatedTurns_;
        PlayerTypes playerType_;
        bool currentBuildIsComplete_;
    };

    class WorkerBuildEvent : public IProjectionEvent, public boost::enable_shared_from_this<WorkerBuildEvent>
    {
    public:
        ~WorkerBuildEvent();
        WorkerBuildEvent(const std::vector<std::pair<UnitTypes, std::vector<Unit::Mission> > >& missions);

        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        WorkerBuildEvent(const CityImprovementManagerPtr& pCityImprovementManager,
            TotalOutputWeights outputWeights,
            const std::vector<std::pair<UnitTypes, std::vector<Unit::Mission> > >& missions,
            size_t processedBuiltUnitsCount);
        void updateMissions_(std::vector<std::pair<UnitTypes, std::vector<Unit::Mission> > >& missions);
        void checkForNewMissions_(std::vector<std::pair<UnitTypes, std::vector<Unit::Mission> > >& missions, const size_t index);

        CityDataPtr pCityData_;
        CityImprovementManagerPtr pCityImprovementManager_;
        TotalOutputWeights outputWeights_;
        std::vector<std::pair<UnitTypes, std::vector<Unit::Mission> > > missions_;
        size_t processedBuiltUnitsCount_;
    };
}
