#pragma once

#include "./utils.h"
#include "./city_projections.h"

namespace AltAI
{
    class Player;
    class City;
    class CityData;
    struct TacticSelectionData;

    typedef std::pair<int, int> DependencyItem;

    void debugDepItem(const DependencyItem& depItem, std::ostream& os);

    struct DependencyItemComp
    {
        bool operator() (const DependencyItem& first, const DependencyItem& second) const
        {
            return first.first == second.first ? first.second < second.second : first.first < second.first;
        }
    };

    typedef std::map<DependencyItem, TacticSelectionData, DependencyItemComp> TacticSelectionDataMap;

    typedef boost::shared_ptr<CityData> CityDataPtr;

    class IDependentTactic;
    typedef boost::shared_ptr<IDependentTactic> IDependentTacticPtr;

    class ResearchTechDependency;
    typedef boost::shared_ptr<ResearchTechDependency> ResearchTechDependencyPtr;

    class IDependentTactic
    {
    public:

        enum IgnoreFlags
        {
            Ignore_None = 0, Ignore_Techs = (1 << 0), Ignore_City_Buildings = (1 << 1), Ignore_Civ_Buildings = (1 << 2), Ignore_Religions = (1 << 3), Ignore_Resources = (1 << 4)
        };

        virtual ~IDependentTactic() = 0 {}
        virtual void apply(const CityDataPtr&) = 0;
        virtual void remove(const CityDataPtr&) = 0;
        virtual bool required(const CvCity*, int) const = 0;
        virtual bool required(const Player&, int) const = 0;
        virtual bool removeable() const = 0;
        virtual std::pair<BuildQueueTypes, int> getBuildItem() const = 0;
        virtual std::pair<int, int> getDependencyItem() const = 0;

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

    class IUnitTactics;
    typedef boost::shared_ptr<IUnitTactics> IUnitTacticsPtr;

    class IUnitTactic;
    typedef boost::shared_ptr<IUnitTactic> IUnitTacticPtr;

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

    class IUnitTactic
    {
    public:
        virtual ~IUnitTactic() = 0 {}

        virtual void debug(std::ostream&) const = 0;
        virtual void apply(const IUnitTacticsPtr&, TacticSelectionData&) = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static IUnitTacticPtr factoryRead(FDataStreamBase* pStream);
    };

    class ICityBuildingTactics
    {
    public:
        virtual ~ICityBuildingTactics() = 0 {}

        virtual IDInfo getCity() const = 0;
        virtual void addTactic(const ICityBuildingTacticPtr&) = 0;
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void addTechDependency(const ResearchTechDependencyPtr&) = 0;
        virtual const std::vector<IDependentTacticPtr>& getDependencies() const = 0;
        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const = 0;
        virtual void update(const Player&, const CityDataPtr&) = 0;
        virtual void updateDependencies(const Player&, const CvCity*) = 0;
        virtual bool areDependenciesSatisfied(int) const = 0;
        virtual void apply(TacticSelectionData&) = 0;
        virtual void apply(TacticSelectionDataMap&, int) = 0;

        virtual BuildingTypes getBuildingType() const = 0;
        virtual ProjectionLadder getProjection() const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        std::vector<DependencyItem> getDepItems(int ignoreFlags) const;

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
        virtual void apply(TacticSelectionDataMap&, int) = 0;
        virtual void apply(TacticSelectionData&) = 0;
        virtual void removeCityTactics(IDInfo) = 0;
        virtual bool empty() const = 0;

        virtual BuildingTypes getBuildingType() const = 0;
        virtual void debug(std::ostream&) const = 0;

        virtual std::pair<int, IDInfo> getFirstBuildCity() const = 0;

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

        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const = 0;

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
        virtual void addTechDependency(const ResearchTechDependencyPtr&) = 0;
        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const = 0;
        virtual void updateDependencies(const Player&) = 0;
        virtual ProjectionLadder getProjection(IDInfo) const = 0;
        virtual ProcessTypes getProcessType() const = 0;
        virtual bool areDependenciesSatisfied(const Player& player, int ignoreFlags) const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static IProcessTacticsPtr factoryRead(FDataStreamBase* pStream);
    };

    class IUnitTactics
    {
    public:
        virtual ~IUnitTactics() = 0 {}

        virtual void addTactic(const IUnitTacticPtr& pPlayerTactic) = 0;
        virtual void addTactic(const ICityUnitTacticPtr& pBuildingTactic) = 0;
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void addTechDependency(const ResearchTechDependencyPtr&) = 0;
        virtual void update(const Player&) = 0;
        virtual void updateDependencies(const Player&) = 0;
        virtual void addCityTactic(IDInfo, const ICityUnitTacticsPtr&) = 0;
        virtual ICityUnitTacticsPtr getCityTactics(IDInfo) const = 0;
        virtual bool areDependenciesSatisfied(const Player& player, int) const = 0;
        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const = 0;
        virtual void apply(TacticSelectionDataMap&, int) = 0;
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
        virtual const std::vector<IDependentTacticPtr>& getDependencies() const = 0;
        virtual void update(const Player&, const CityDataPtr&) = 0;
        virtual void updateDependencies(const Player&, const CvCity*) = 0;
        virtual bool areDependenciesSatisfied(int) const = 0;
        virtual void apply(TacticSelectionDataMap&, int) = 0;
        virtual void apply(TacticSelectionData&) = 0;

        virtual UnitTypes getUnitType() const = 0;
        virtual ProjectionLadder getProjection() const = 0;

        std::vector<DependencyItem> getDepItems(int ignoreFlags) const;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static ICityUnitTacticsPtr factoryRead(FDataStreamBase* pStream);
    };

    class ITechTactic;
    typedef boost::shared_ptr<ITechTactic> ITechTacticPtr;

    class ITechTactics;
    typedef boost::shared_ptr<ITechTactics> ITechTacticsPtr;

    class ITechTactic
    {
    public:
        virtual ~ITechTactic() = 0 {}

        virtual void debug(std::ostream&) const = 0;
        virtual void apply(const ITechTacticsPtr&, TacticSelectionData&) = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static ITechTacticPtr factoryRead(FDataStreamBase* pStream);
    };

    class ITechTactics
    {
    public:
        virtual ~ITechTactics() = 0 {}

        virtual void debug(std::ostream&) const = 0;
        virtual TechTypes getTechType() const = 0;
        virtual PlayerTypes getPlayer() const = 0;

        virtual void addTactic(const ITechTacticPtr&) = 0;
        virtual void apply(TacticSelectionData&) = 0;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static ITechTacticsPtr factoryRead(FDataStreamBase* pStream);
    };

    struct IsNotRequired
    {
        explicit IsNotRequired(const Player& player_, const CvCity* pCity_ = NULL, int ignoreFlags_ = 0) : player(player_), pCity(pCity_), ignoreFlags(ignoreFlags_)
        {
        }

        bool operator() (const IDependentTacticPtr& pDependentTactic) const
        {
            return pDependentTactic->removeable() && (pCity ? !pDependentTactic->required(pCity, ignoreFlags) : !pDependentTactic->required(player, ignoreFlags));
        }

        const Player& player;
        const CvCity* pCity;
        int ignoreFlags;
    };
}