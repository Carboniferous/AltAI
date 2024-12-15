#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class CityBuildingTactic : public ICityBuildingTactics, public boost::enable_shared_from_this<CityBuildingTactic>
    {
    public:
        CityBuildingTactic() : buildingType_(NO_BUILDING) {}
        CityBuildingTactic(BuildingTypes buildingType, int buildingCost, IDInfo city, ComparisonFlags compFlag);

        virtual IDInfo getCity() const;
        virtual CityDataPtr getCityData() const;
        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void addTechDependency(const ResearchTechDependencyPtr& pDependentTactic);
        virtual const std::vector<IDependentTacticPtr>& getDependencies() const;
        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const;
        virtual void update(Player& player, const CityDataPtr& pCityData);
        virtual void updateDependencies(Player& player, const CvCity* pCity);
        virtual bool areDependenciesSatisfied(int depTacticFlags) const;
        virtual void apply(TacticSelectionDataMap& selectionDataMap, int depTacticFlags);
        virtual void apply(TacticSelectionData& selectionData);

        virtual BuildingTypes getBuildingType() const;
        virtual int getBuildingCost() const;
        virtual ProjectionLadder getProjection() const;
        virtual ComparisonFlags getComparisonFlag() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int CityBuildingTacticID = 0;

    private:
        void apply_(TacticSelectionData& selectionData);

        std::vector<IDependentTacticPtr> dependentTactics_;
        std::vector<ResearchTechDependencyPtr> techDependencies_;
        std::list<ICityBuildingTacticPtr> buildingTactics_;
        ProjectionLadder projection_;
        std::vector<ProjectionLadder> hurryProjections_;
        BuildingTypes buildingType_;
        int buildingCost_;
        IDInfo city_;
        CityDataPtr pCityData_;
        ComparisonFlags compFlag_;
    };

    class ProcessTactic : public IProcessTactics
    {
    public:
        ProcessTactic() : processType_(NO_PROCESS) {}
        explicit ProcessTactic(ProcessTypes processType);

        virtual void addTechDependency(const ResearchTechDependencyPtr& pDependentTactic);
        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const;
        virtual void updateDependencies(const Player& player);
        virtual ProjectionLadder getProjection(IDInfo city) const;
        virtual ProcessTypes getProcessType() const;
        virtual bool areDependenciesSatisfied(const Player& player, int depTacticFlags) const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ProcessTacticID = 0;

    private:
        std::vector<ResearchTechDependencyPtr> techDependencies_;
        ProcessTypes processType_;
    };

    // world + national wonders (limited buildings)
    class LimitedBuildingTactic : public IGlobalBuildingTactics
    {
    public:
        LimitedBuildingTactic() : buildingType_(NO_BUILDING), isGlobal_(false) {}
        explicit LimitedBuildingTactic(BuildingTypes buildingType);

        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void update(Player& player);
        virtual void update(Player&, const CityDataPtr&);
        virtual void updateDependencies(Player& player);
        virtual bool areDependenciesSatisfied(IDInfo city, int depTacticFlags) const;
        virtual void addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic);
        virtual ICityBuildingTacticsPtr getCityTactics(IDInfo city) const;
        virtual void apply(TacticSelectionDataMap& selectionDataMap, int depTacticFlags);
        virtual void apply(TacticSelectionData& selectionData);
        virtual void removeCityTactics(IDInfo city);
        virtual bool empty() const;

        virtual BuildingTypes getBuildingType() const;

        virtual void debug(std::ostream& os) const;

        virtual std::pair<int, IDInfo> getFirstBuildCity() const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int LimitedBuildingTacticID = 0;

    private:
        TotalOutput getGlobalDelta_(IDInfo builtCity, int buildTime);
        BuildingTypes buildingType_;
        bool isGlobal_;
        typedef std::map<IDInfo, ICityBuildingTacticsPtr> CityTacticsMap;
        CityTacticsMap cityTactics_;
    };
}