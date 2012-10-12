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

    class IDependentTactic;
    typedef boost::shared_ptr<IDependentTactic> IDependentTacticPtr;

    class ResearchTechDependency;
    typedef boost::shared_ptr<ResearchTechDependency> ResearchTechDependencyPtr;

    class IDependentTactic
    {
    public:
        virtual ~IDependentTactic() = 0 {}
        virtual void apply(const CityDataPtr&) = 0;
        virtual void remove(const CityDataPtr&) = 0;
        virtual bool required(const CvCity*) const = 0;
        virtual bool required(const Player&) const = 0;
        virtual bool removeable() const = 0;
        virtual std::pair<BuildQueueTypes, int> getBuildItem() const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static IDependentTacticPtr factoryRead(FDataStreamBase* pStream);
    };    

    class IWorkerBuildTactic;
    typedef boost::shared_ptr<IWorkerBuildTactic> IWorkerBuildTacticPtr;

    class IWorkerBuildTactic
    {
    public:
        virtual ~IWorkerBuildTactic() = 0 {}

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static IWorkerBuildTacticPtr factoryRead(FDataStreamBase* pStream);
    };

    class ICityBuildingTactics;
    typedef boost::shared_ptr<ICityBuildingTactics> ICityBuildingTacticsPtr;

    class ICityBuildingTactic;
    typedef boost::shared_ptr<ICityBuildingTactic> ICityBuildingTacticPtr;

    class ICityBuildingTactic
    {
    public:
        virtual ~ICityBuildingTactic() = 0 {}
        
        virtual void debug(std::ostream&) const = 0;
        virtual void apply(const ICityBuildingTacticsPtr&, TacticSelectionData&) = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static ICityBuildingTacticPtr factoryRead(FDataStreamBase* pStream);
    };

    class ICityUnitTactics;
    typedef boost::shared_ptr<ICityUnitTactics> ICityUnitTacticsPtr;

    class ICityUnitTactic;
    typedef boost::shared_ptr<ICityUnitTactic> ICityUnitTacticPtr;

    class ICityUnitTactic
    {
    public:
        virtual ~ICityUnitTactic() = 0 {}
        
        virtual void debug(std::ostream&) const = 0;
        virtual void apply(const ICityUnitTacticsPtr&, TacticSelectionData&) = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static ICityUnitTacticPtr factoryRead(FDataStreamBase* pStream);
    };    

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
        virtual bool areDependenciesSatisfied() const = 0;
        virtual void apply(TacticSelectionData&) = 0;

        virtual BuildingTypes getBuildingType() const = 0;
        virtual ProjectionLadder getProjection() const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static ICityBuildingTacticsPtr factoryRead(FDataStreamBase* pStream);
    };

    class IGlobalBuildingTactics;
    typedef boost::shared_ptr<IGlobalBuildingTactics> ILimitedBuildingTacticsPtr;

    class IGlobalBuildingTactics
    {
    public:
        virtual ~IGlobalBuildingTactics() = 0 {}
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void update(const Player&) = 0;
        virtual void updateDependencies(const Player&) = 0;
        virtual void addCityTactic(IDInfo, const ICityBuildingTacticsPtr&) = 0;
        virtual ICityBuildingTacticsPtr getCityTactics(IDInfo) const = 0;
        virtual void apply(TacticSelectionData&) = 0;
        virtual void removeCityTactics(IDInfo) = 0;
        virtual bool empty() const = 0;

        virtual BuildingTypes getBuildingType() const = 0;
        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static ILimitedBuildingTacticsPtr factoryRead(FDataStreamBase* pStream);
    };

    class ICityImprovementTactics;
    typedef boost::shared_ptr<ICityImprovementTactics> ICityImprovementTacticsPtr;

    class ICityImprovementTactics
    {
    public:
        virtual ~ICityImprovementTactics() = 0 {}
        virtual void addTactic(const IWorkerBuildTacticPtr&) = 0;
        virtual void addDependency(const ResearchTechDependencyPtr&) = 0;
        virtual void update(const Player&, const CityDataPtr&) = 0;
        virtual void apply(const ICityUnitTacticsPtr&, TacticSelectionData&) = 0;

        virtual ProjectionLadder getProjection() const = 0;
        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static ICityImprovementTacticsPtr factoryRead(FDataStreamBase* pStream);
    };

    class IProcessTactics;
    typedef boost::shared_ptr<IProcessTactics> IProcessTacticsPtr;

    class IProcessTactics
    {
    public:
        virtual ~IProcessTactics() = 0 {}
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void updateDependencies(const Player&) = 0;
        virtual ProjectionLadder getProjection(IDInfo) const = 0;
        virtual ProcessTypes getProcessType() const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static IProcessTacticsPtr factoryRead(FDataStreamBase* pStream);
    };

    class IUnitTactics;
    typedef boost::shared_ptr<IUnitTactics> IUnitTacticsPtr;

    class IUnitTactics
    {
    public:
        virtual ~IUnitTactics() = 0 {}
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void addTechDependency(const ResearchTechDependencyPtr&) = 0;
        virtual void update(const Player&) = 0;
        virtual void updateDependencies(const Player&) = 0;
        virtual void addCityTactic(IDInfo, const ICityUnitTacticsPtr&) = 0;
        virtual ICityUnitTacticsPtr getCityTactics(IDInfo) const = 0;
        virtual bool areDependenciesSatisfied(const Player& player) const = 0;
        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const = 0;
        virtual void apply(TacticSelectionData&) = 0;
        virtual void removeCityTactics(IDInfo) = 0;
        virtual bool empty() const = 0;

        virtual UnitTypes getUnitType() const = 0;
        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static IUnitTacticsPtr factoryRead(FDataStreamBase* pStream);
    };

    class ICityUnitTactics
    {
    public:
        virtual ~ICityUnitTactics() = 0 {}

        virtual IDInfo getCity() const = 0;
        virtual void addTactic(const ICityUnitTacticPtr&) = 0;
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual std::vector<IDependentTacticPtr> getDependencies() const = 0;
        virtual void update(const Player&, const CityDataPtr&) = 0;
        virtual void updateDependencies(const Player&, const CvCity*) = 0;
        virtual bool areDependenciesSatisfied() const = 0;
        virtual void apply(TacticSelectionData&) = 0;

        virtual UnitTypes getUnitType() const = 0;
        virtual ProjectionLadder getProjection() const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static ICityUnitTacticsPtr factoryRead(FDataStreamBase* pStream);
    };

    struct IsNotRequired
    {
        explicit IsNotRequired(const Player& player_, const CvCity* pCity_ = NULL) : player(player_), pCity(pCity_)
        {
        }

        bool operator() (const IDependentTacticPtr& pDependentTactic) const
        {
            return pDependentTactic->removeable() && (pCity ? !pDependentTactic->required(pCity) : !pDependentTactic->required(player));
        }

        const Player& player;
        const CvCity* pCity;
    };
}