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
    typedef std::set<DependencyItem> DependencyItemSet;

    // todo - combine build items into an enhanced dependency class
    void debugDepItemSet(const DependencyItemSet& depItemSet, std::ostream& os);
    void debugDepItem(const DependencyItem& depItem, std::ostream& os);
    void debugBuildItem(const BuildQueueItem& buildQueueItem, std::ostream& os);

    DependencyItemSet getUnsatisfiedDeps(const DependencyItemSet& depItemSet, Player& player, City& city);

    inline bool operator < (const DependencyItem& first, const DependencyItem& second)
    {
        return first.first == second.first ? first.second < second.second : first.first < second.first;
    }

    inline bool operator == (const DependencyItem& first, const DependencyItem& second)
    {
        return first.first == second.first && first.second == second.second;
    }

    inline bool operator != (const DependencyItem& first, const DependencyItem& second)
    {
        return !(first == second);
    }

    struct DependencyItemsComp
    {
        bool operator() (const DependencyItemSet& first, const DependencyItemSet& second) const;
    };

    typedef std::map<DependencyItemSet, TacticSelectionData, DependencyItemsComp> TacticSelectionDataMap;

    typedef boost::shared_ptr<CityData> CityDataPtr;

    class IDependentTactic;
    typedef boost::shared_ptr<IDependentTactic> IDependentTacticPtr;

    class ResearchTechDependency;
    typedef boost::shared_ptr<ResearchTechDependency> ResearchTechDependencyPtr;

    class IDependentTactic
    {
    public:

        enum DepTacticFlags
        {
            Ignore_None = 0, Tech_Dep = (1 << 0), City_Buildings_Dep = (1 << 1), Civ_Buildings_Dep = (1 << 2),
            Religion_Dep = (1 << 3), Resource_Dep = (1 << 4), CivUnits_Dep = (1 << 5), Resource_Tech_Dep = (1 << 6),
            All_Deps = 0xffffffff
        };

        enum DependentTacticTypes
        {
            ResearchTechDependencyID = 0, CityBuildingDependencyID = 1, CivBuildingDependencyID = 2,
            ReligiousDependencyID = 3, StateReligionDependencyID = 4, CityBonusDependencyID = 5,
            CivUnitDependencyID = 6, ResouceProductionBonusDependencyID = 7
        };

        virtual ~IDependentTactic() = 0 {}
        virtual void apply(const CityDataPtr&) = 0;
        virtual void remove(const CityDataPtr&) = 0;
        // flags are those dependencies to ignore
        virtual bool required(const CvCity*, int depTacticFlags) const = 0;
        virtual bool required(const Player&, int depTacticFlags) const = 0;
        virtual bool removeable() const = 0;
        virtual bool matches(int depTacticFlags) const = 0;
        virtual BuildQueueItem getBuildItem() const = 0;
        virtual std::vector<DependencyItem> getDependencyItems() const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static IDependentTacticPtr factoryRead(FDataStreamBase*);
    };

    std::string depTacticFlagsToString(int depTacticFlags);

    struct IsNotRequired
    {
        explicit IsNotRequired(const Player& player_, const CvCity* pCity_ = NULL, int ignoreFlags_ = 0) : player(player_), pCity(pCity_), depTacticFlags(ignoreFlags_)
        {
        }

        bool operator() (const IDependentTacticPtr& pDependentTactic) const;

        const Player& player;
        const CvCity* pCity;
        int depTacticFlags;
    };

    class IWorkerBuildTactic;
    typedef boost::shared_ptr<IWorkerBuildTactic> IWorkerBuildTacticPtr;

    class IWorkerBuildTactic
    {
    public:
        virtual ~IWorkerBuildTactic() = 0 {}

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static IWorkerBuildTacticPtr factoryRead(FDataStreamBase*);

        enum WorkerBuildTacticTypes
        {
            EconomicImprovementTacticID = 0, RemoveFeatureTacticID = 1, ProvidesResourceTacticID = 2, HappyImprovementTacticID = 3, 
            HealthImprovementTacticID = 4, MilitaryImprovementTacticID = 5
        };
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
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static ICityBuildingTacticPtr factoryRead(FDataStreamBase*);

        enum CityBuildingTacticTypes
        {
            EconomicBuildingTacticID = 0, FoodBuildingTacticID = 1, HappyBuildingTacticID = 2,
            HealthBuildingTacticID = 3, ScienceBuildingTacticID = 4, GoldBuildingTacticID = 5,
            CultureBuildingTacticID = 6, EspionageBuildingTacticID = 7, SpecialistBuildingTacticID = 8,
            GovCenterTacticID = 9, UnitExperienceTacticID = 10, CityDefenceBuildingTacticID = 11,
            FreeTechBuildingTacticID = 12, CanTrainUnitBuildingTacticID = 13
        };
    };

    class UnitTactics;
    typedef boost::shared_ptr<UnitTactics> UnitTacticsPtr;

    class CityUnitTactics;
    typedef boost::shared_ptr<CityUnitTactics> CityUnitTacticsPtr;

    class ICityUnitTactic;
    typedef boost::shared_ptr<ICityUnitTactic> ICityUnitTacticPtr;

    class IBuiltUnitTactic;
    typedef boost::shared_ptr<IBuiltUnitTactic> IBuiltUnitTacticPtr;

    class IBuiltUnitTactic
    {
    public:
        virtual ~IBuiltUnitTactic() = 0 {}

        virtual void debug(std::ostream&) const = 0;
        virtual void apply(const UnitTacticsPtr&, TacticSelectionData&) = 0;

        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static IBuiltUnitTacticPtr factoryRead(FDataStreamBase*);

        enum BuiltUnitTacticTypes
        {
            DiscoverTechUnitTacticID = 0, BuildSpecialBuildingUnitTacticID = 1, CreateGreatWorkUnitTacticID = 2,
            TradeMissionUnitTacticID = 3, JoinCityUnitTacticID = 4, HurryBuildingUnitTacticID = 5 
        };
    };

    class ICityUnitTactic
    {
    public:
        virtual ~ICityUnitTactic() = 0 {}
        
        virtual void debug(std::ostream&) const = 0;
        virtual void apply(const CityUnitTacticsPtr&, TacticSelectionData&) = 0;
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city) = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static ICityUnitTacticPtr factoryRead(FDataStreamBase*);

        enum UnitTacticTypes
        {
            CityDefenceUnitTacticID = 0, ThisCityDefenceUnitTacticID = 1, CityAttackUnitTacticID = 2, CollateralUnitTacticID = 3,
            FieldDefenceUnitTacticID = 4, FieldAttackUnitTacticID = 5, BuildCityUnitTacticID = 6, BuildImprovementsUnitTacticID = 7,
            SeaAttackUnitTacticID = 8, ScoutUnitTacticID = 9, SpreadReligionUnitTacticID = 10
        };
    };

	struct IProjectionEvent;
    typedef boost::shared_ptr<IProjectionEvent> IProjectionEventPtr;

    class IUnitEventGenerator
    {
    public:
        virtual ~IUnitEventGenerator() = 0 {}

        virtual IProjectionEventPtr getProjectionEvent(const CityDataPtr& pCityData) = 0;
    };

    typedef boost::shared_ptr<IUnitEventGenerator> IUnitEventGeneratorPtr;

    class ICityBuildingTactics
    {
    public:
        enum ComparisonFlags
        {
            No_Comparison = 0, City_Comparison = 1, Area_Comparison = 2, Global_Comparison = 3
        };

        virtual ~ICityBuildingTactics() = 0 {}

        virtual IDInfo getCity() const = 0;
        virtual CityDataPtr getCityData() const = 0;
        virtual void addTactic(const ICityBuildingTacticPtr&) = 0;
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void addTechDependency(const ResearchTechDependencyPtr&) = 0;
        virtual const std::vector<IDependentTacticPtr>& getDependencies() const = 0;
        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const = 0;
        virtual void update(Player&, const CityDataPtr&) = 0;
        virtual void updateDependencies(Player&, const CvCity*) = 0;
        virtual bool areDependenciesSatisfied(int depTacticFlags) const = 0;
        virtual void apply(TacticSelectionData&) = 0;
        virtual void apply(TacticSelectionDataMap&, int) = 0;

        virtual BuildingTypes getBuildingType() const = 0;
        virtual int getBuildingCost() const = 0;
        virtual ProjectionLadder getProjection() const = 0;
        virtual ComparisonFlags getComparisonFlag() const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        // given the set of ignore flags, what dependent items do we have left?
        std::vector<DependencyItem> getDepItems(int depTacticFlags) const;
        // given the set of ignore flags, what build items does that generate?
        std::vector<BuildQueueItem > getBuildItems(int depTacticFlags) const;

        static ICityBuildingTacticsPtr factoryRead(FDataStreamBase*);
    };

    struct ICityBuildingTacticsBuildingComp
    {
        bool operator() (const ICityBuildingTacticsPtr& pFirstTactic, const ICityBuildingTacticsPtr& pSecondTactic) const;
    };

    class IGlobalBuildingTactics;
    typedef boost::shared_ptr<IGlobalBuildingTactics> ILimitedBuildingTacticsPtr;

    class IGlobalBuildingTactics
    {
    public:
        virtual ~IGlobalBuildingTactics() = 0 {}
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void update(Player&) = 0;
        virtual void update(Player&, const CityDataPtr&) = 0;
        virtual void updateDependencies(Player&) = 0;
        virtual bool areDependenciesSatisfied(IDInfo city, int depTacticFlags) const = 0;
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
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static ILimitedBuildingTacticsPtr factoryRead(FDataStreamBase*);
    };

    class CityImprovementTactics;
    typedef boost::shared_ptr<CityImprovementTactics> CityImprovementTacticsPtr;

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
        virtual bool areDependenciesSatisfied(const Player& player, int depTacticFlags) const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static IProcessTacticsPtr factoryRead(FDataStreamBase*);
    };

    class ITechTactic;
    typedef boost::shared_ptr<ITechTactic> ITechTacticPtr;

    class ITechTactics;
    typedef boost::shared_ptr<ITechTactics> ITechTacticsPtr;

    typedef std::list<ITechTacticPtr> TechTacticPtrList;
    typedef TechTacticPtrList::iterator TechTacticPtrListIter;
    typedef TechTacticPtrList::const_iterator TechTacticPtrListConstIter;

    class ITechTactic
    {
    public:
        virtual ~ITechTactic() = 0 {}

        virtual void debug(std::ostream&) const = 0;
        virtual void apply(const ITechTacticsPtr&, TacticSelectionData&) = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        virtual const int getID() const = 0;

        static ITechTacticPtr factoryRead(FDataStreamBase*);

        enum TechTacticTypes
        {
            FreeTechTacticID = 0, FoundReligionTechTacticID = 1, ConnectsResourcesTechTacticID = 2, 
            ConstructBuildingTechTacticID = 3, ProvidesResourceTechTacticID = 4, EconomicTechTacticID = 5
        };
    };

    class ITechTactics
    {
    public:
        virtual ~ITechTactics() = 0 {}

        virtual void debug(std::ostream&) const = 0;
        virtual TechTypes getTechType() const = 0;
        virtual PlayerTypes getPlayer() const = 0;

        virtual void addTactic(const ITechTacticPtr&) = 0;
        virtual void removeTactic(const int tacticID) = 0;
        virtual void apply(TacticSelectionData&) = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static ITechTacticsPtr factoryRead(FDataStreamBase*);
    };

    class ICultureSourceTactics
    {
    public:
        virtual ~ICultureSourceTactics() = 0 {}
    };

    class CivicTactics;
    typedef boost::shared_ptr<CivicTactics> CivicTacticsPtr;

    class ICivicTactic;
    typedef boost::shared_ptr<ICivicTactic> ICivicTacticPtr;

    class ICivicTactic
    {
    public:
        virtual ~ICivicTactic() = 0 {}

        virtual void debug(std::ostream&) const = 0;
        virtual void update(const CivicTacticsPtr&, Player&) = 0;
        virtual void apply(const CivicTacticsPtr&, TacticSelectionData&) = 0;

        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static ICivicTacticPtr factoryRead(FDataStreamBase*);

        enum CivicTacticTypes
        {
            EconomicCivicTacticID = 0, HurryCivicTacticID = 1, HappyPoliceCivicTacticID = 2            
        };
    };

    class ResourceTactics;
    typedef boost::shared_ptr<ResourceTactics> ResourceTacticsPtr;

    class IResourceTactic;
    typedef boost::shared_ptr<IResourceTactic> IResourceTacticPtr;

    class IResourceTactic
    {
    public:
        virtual ~IResourceTactic() = 0 {}

        virtual void debug(std::ostream& os) const = 0;

        virtual void update(const ResourceTacticsPtr&, Player&) = 0;
        virtual void update(const ResourceTacticsPtr&, City&) = 0;
        virtual void apply(const ResourceTacticsPtr&, TacticSelectionData&) = 0;

        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static IResourceTacticPtr factoryRead(FDataStreamBase*);

        enum ResourceTacticTypes
        {
            EconomicResourceTacticID = 0, UnitResourceTacticID = 1, BuildingResourceTacticID = 2
        };
    };

    class ReligionTactics;
    typedef boost::shared_ptr<ReligionTactics> ReligionTacticsPtr;

    class IReligionTactic;
    typedef boost::shared_ptr<IReligionTactic> IReligionTacticPtr;

    class IReligionTactic
    {
    public:
        virtual ~IReligionTactic() = 0 {}

        virtual void debug(std::ostream& os) const = 0;

        virtual void update(const ReligionTacticsPtr&, Player&) = 0;
        virtual void update(const ReligionTacticsPtr&, City&) = 0;
        virtual void apply(const ReligionTacticsPtr&, TacticSelectionData&) = 0;

        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static IReligionTacticPtr factoryRead(FDataStreamBase*);

        enum ReligionTacticTypes
        {
            EconomicReligionTacticID = 0, UnitReligionTacticID = 1
        };
    };
}