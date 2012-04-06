#pragma once

#include "./utils.h"
#include "./city_projections.h"

namespace AltAI
{
    class Player;
    class City;
    class CityData;

    typedef boost::shared_ptr<CityData> CityDataPtr;

    class IDependentTactic
    {
    public:
        virtual ~IDependentTactic() = 0 {}
        virtual void apply(const CityDataPtr&) = 0;
        virtual void remove(const CityDataPtr&) = 0;

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

    class ICityBuildingTactic
    {
    public:
        virtual ~ICityBuildingTactic() = 0 {}
        
        virtual void debug(std::ostream&) const = 0;
    };

    typedef boost::shared_ptr<ICityBuildingTactic> ICityBuildingTacticPtr;

    class ICityBuildingTactics
    {
    public:
        virtual ~ICityBuildingTactics() = 0 {}

        virtual void addTactic(const ICityBuildingTacticPtr&) = 0;
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void update(const Player&, const CityDataPtr&) = 0;

        virtual BuildingTypes getBuildingType() const = 0;
        virtual ProjectionLadder getProjection() const = 0;

        virtual void debug(std::ostream&) const = 0;
    };

    typedef boost::shared_ptr<ICityBuildingTactics> ICityBuildingTacticsPtr;

    class IGlobalBuildingTactics
    {
    public:
        virtual ~IGlobalBuildingTactics() = 0 {}
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void update(const Player&) = 0;
        virtual void addCityTactic(const ICityBuildingTacticsPtr& pCityTactic) = 0;

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