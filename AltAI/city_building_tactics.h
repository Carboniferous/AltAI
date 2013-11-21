#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class CityBuildingTactic : public ICityBuildingTactics, public boost::enable_shared_from_this<CityBuildingTactic>
    {
    public:
        CityBuildingTactic() : buildingType_(NO_BUILDING) {}
        CityBuildingTactic(BuildingTypes buildingType, IDInfo city);

        virtual IDInfo getCity() const;
        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void addTechDependency(const ResearchTechDependencyPtr& pDependentTactic);
        virtual const std::vector<IDependentTacticPtr>& getDependencies() const;
        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const;
        virtual void update(const Player& player, const CityDataPtr& pCityData);
        virtual void updateDependencies(const Player& player, const CvCity* pCity);
        virtual bool areDependenciesSatisfied(int ignoreFlags) const;
        virtual void apply(TacticSelectionDataMap& selectionDataMap, int ignoreFlags);
        virtual void apply(TacticSelectionData& selectionData);

        virtual BuildingTypes getBuildingType() const;
        virtual ProjectionLadder getProjection() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;

    private:
        void apply_(TacticSelectionData& selectionData);

        std::vector<IDependentTacticPtr> dependentTactics_;
        std::vector<ResearchTechDependencyPtr> techDependencies_;
        std::list<ICityBuildingTacticPtr> buildingTactics_;
        ProjectionLadder projection_;
        BuildingTypes buildingType_;
        IDInfo city_;
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
        virtual bool areDependenciesSatisfied(const Player& player, int ignoreFlags) const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;

    private:
        std::vector<ResearchTechDependencyPtr> techDependencies_;
        ProcessTypes processType_;
    };

    // world wonders
    class GlobalBuildingTactic : public IGlobalBuildingTactics
    {
    public:
        GlobalBuildingTactic() : buildingType_(NO_BUILDING) {}
        explicit GlobalBuildingTactic(BuildingTypes buildingType);

        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void update(const Player& player);
        virtual void updateDependencies(const Player& player);
        virtual void addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic);
        virtual ICityBuildingTacticsPtr getCityTactics(IDInfo city) const;
        virtual void apply(TacticSelectionDataMap& selectionDataMap, int ignoreFlags);
        virtual void apply(TacticSelectionData& selectionData);
        virtual void removeCityTactics(IDInfo city);
        virtual bool empty() const;

        virtual BuildingTypes getBuildingType() const;

        virtual void debug(std::ostream& os) const;

        virtual std::pair<int, IDInfo> getFirstBuildCity() const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;

    private:
        BuildingTypes buildingType_;
        typedef std::map<IDInfo, ICityBuildingTacticsPtr> CityTacticsMap;
        CityTacticsMap cityTactics_;
    };

    //national wonders
    class NationalBuildingTactic : public IGlobalBuildingTactics
    {
    public:
        NationalBuildingTactic() : buildingType_(NO_BUILDING) {}
        explicit NationalBuildingTactic(BuildingTypes buildingType);

        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void update(const Player& player);
        virtual void updateDependencies(const Player& player);
        virtual void addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic);
        virtual ICityBuildingTacticsPtr getCityTactics(IDInfo city) const;
        virtual void apply(TacticSelectionDataMap& selectionDataMap, int ignoreFlags);
        virtual void apply(TacticSelectionData& selectionData);
        virtual void removeCityTactics(IDInfo city);
        virtual bool empty() const;

        virtual BuildingTypes getBuildingType() const;

        virtual void debug(std::ostream& os) const;

        virtual std::pair<int, IDInfo> getFirstBuildCity() const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 1;

    private:
        BuildingTypes buildingType_;
        typedef std::map<IDInfo, ICityBuildingTacticsPtr> CityTacticsMap;
        CityTacticsMap cityTactics_;
    };
}