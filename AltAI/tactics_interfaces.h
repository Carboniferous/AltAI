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

    typedef std::set<DependencyItem> DependencyItemSet;

    struct DependencyItemsComp
    {
        bool operator() (const DependencyItemSet& first, const DependencyItemSet& second) const
        {
            if (first.size() != second.size())
            {
                return first.size() < second.size();
            }
            else
            {
                for (DependencyItemSet::const_iterator ci1(first.begin()), ci2(second.begin()), ci1End(first.end());
                    ci1 != ci1End; ++ci1, ++ci2)
                {
                    if (*ci1 != *ci2)
                    {
                        return *ci1 < *ci2;
                    }
                }
                return false;
            }
        }
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

        enum IgnoreFlags
        {
            Ignore_None = 0, Ignore_Techs = (1 << 0), Ignore_City_Buildings = (1 << 1), Ignore_Civ_Buildings = (1 << 2),
            Ignore_Religions = (1 << 3), Ignore_Resources = (1 << 4), Ignore_CivUnits = (1 << 5)
        };

        virtual ~IDependentTactic() = 0 {}
        virtual void apply(const CityDataPtr&) = 0;
        virtual void remove(const CityDataPtr&) = 0;
        virtual bool required(const CvCity*, int) const = 0;
        virtual bool required(const Player&, int) const = 0;
        virtual bool removeable() const = 0;
        virtual std::pair<BuildQueueTypes, int> getBuildItem() const = 0;
        virtual std::vector<DependencyItem> getDependencyItems() const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static IDependentTacticPtr factoryRead(FDataStreamBase*);
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
    };

    class ICityUnitTactic
    {
    public:
        virtual ~ICityUnitTactic() = 0 {}
        
        virtual void debug(std::ostream&) const = 0;
        virtual void apply(const CityUnitTacticsPtr&, TacticSelectionData&) = 0;
        virtual std::vector<XYCoords> getPossibleTargets(const Player& player, IDInfo city) = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static ICityUnitTacticPtr factoryRead(FDataStreamBase*);
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
        virtual int getBuildingCost() const = 0;
        virtual ProjectionLadder getProjection() const = 0;
        virtual ComparisonFlags getComparisonFlag() const = 0;

        virtual void debug(std::ostream&) const = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        std::vector<DependencyItem> getDepItems(int ignoreFlags) const;

        static ICityBuildingTacticsPtr factoryRead(FDataStreamBase*);
    };

    class IGlobalBuildingTactics;
    typedef boost::shared_ptr<IGlobalBuildingTactics> ILimitedBuildingTacticsPtr;

    class IGlobalBuildingTactics
    {
    public:
        virtual ~IGlobalBuildingTactics() = 0 {}
        virtual void addDependency(const IDependentTacticPtr&) = 0;
        virtual void update(const Player&) = 0;
        virtual void update(const Player&, const CityDataPtr&) = 0;
        virtual void updateDependencies(const Player&) = 0;
        virtual bool areDependenciesSatisfied(IDInfo, int) const = 0;
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
        virtual bool areDependenciesSatisfied(const Player& player, int ignoreFlags) const = 0;

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
        virtual void update(const CivicTacticsPtr&, const Player&) = 0;
        virtual void apply(const CivicTacticsPtr&, TacticSelectionData&) = 0;

        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static ICivicTacticPtr factoryRead(FDataStreamBase*);
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

        virtual void update(const ResourceTacticsPtr&, const Player&) = 0;
        virtual void update(const ResourceTacticsPtr&, const City&) = 0;
        virtual void apply(const ResourceTacticsPtr&, TacticSelectionData&) = 0;

        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static IResourceTacticPtr factoryRead(FDataStreamBase*);
    };
}