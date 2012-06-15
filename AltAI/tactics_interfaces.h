#pragma once

#include "./utils.h"
#include "./city_projections.h"

namespace AltAI
{
    class Player;
    class City;
    class CityData;
    struct TacticSelectionData;

    typedef boost::shared_ptr<CityData> CityDataPtr;

    class IDependentTactic
    {
    public:
        virtual ~IDependentTactic() = 0 {}
        virtual void apply(const CityDataPtr&) = 0;
        virtual void remove(const CityDataPtr&) = 0;
        virtual bool required(const CvCity*) const = 0;
        virtual std::pair<BuildQueueTypes, int> getBuildItem() const = 0;

        virtual void debug(std::ostream&) const = 0;
    };

    typedef boost::shared_ptr<IDependentTactic> IDependentTacticPtr;

    class IWorkerBuildTactic
    {
    public:
        virtual ~IWorkerBuildTactic() = 0 {}

        virtual void debug(std::ostream&) const = 0;
    };

    typedef boost::shared_ptr<IWorkerBuildTactic> IWorkerBuildTacticPtr;

    class ICityBuildingTactics;
    typedef boost::shared_ptr<ICityBuildingTactics> ICityBuildingTacticsPtr;

    class ICityBuildingTactic
    {
    public:
        virtual ~ICityBuildingTactic() = 0 {}
        
        virtual void debug(std::ostream&) const = 0;
        virtual void apply(const ICityBuildingTacticsPtr&, TacticSelectionData&) = 0;
    };

    typedef boost::shared_ptr<ICityBuildingTactic> ICityBuildingTacticPtr;

    class ICityBuildingTactics
    {
    public:
        virtual ~ICityBuildingTactics() = 0 {}

        virtual IDInfo getCity() const = 0;
        virtual void addTactic(const ICityBuildingTacticPtr&) = 0;
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual std::vector<IDependentTacticPtr> getDependencies() const = 0;
        virtual void update(const Player&, const CityDataPtr&) = 0;
        virtual void updateDependencies(const Player&, const CvCity*) = 0;
        virtual void apply(TacticSelectionData&) = 0;

        virtual BuildingTypes getBuildingType() const = 0;
        virtual ProjectionLadder getProjection() const = 0;

        virtual void debug(std::ostream&) const = 0;
    };

    class IGlobalBuildingTactics
    {
    public:
        virtual ~IGlobalBuildingTactics() = 0 {}
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void update(const Player&) = 0;
        virtual void updateDependencies(const Player&) = 0;
        virtual void addCityTactic(IDInfo, const ICityBuildingTacticsPtr&) = 0;
        virtual std::list<ICityBuildingTacticsPtr> getCityTactics(IDInfo) const = 0;
        virtual void apply(TacticSelectionData&) = 0;
        virtual void removeCityTactics(IDInfo) = 0;
        virtual bool empty() const = 0;

        virtual BuildingTypes getBuildingType() const = 0;
        virtual void debug(std::ostream&) const = 0;
    };

    typedef boost::shared_ptr<IGlobalBuildingTactics> ILimitedBuildingTacticsPtr;

    class ICityImprovementTactics
    {
    public:
        virtual ~ICityImprovementTactics() = 0 {}
        virtual void addTactic(const IWorkerBuildTacticPtr&) = 0;
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void update(const Player&, const CityDataPtr&) = 0;

        virtual ProjectionLadder getProjection() const = 0;
        virtual void debug(std::ostream& os) const = 0;
    };

    typedef boost::shared_ptr<ICityImprovementTactics> ICityImprovementTacticsPtr;
}