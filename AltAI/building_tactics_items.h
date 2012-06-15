#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"
#include "./city_projections.h"

namespace AltAI
{
    class ResearchTechDependency : public IDependentTactic
    {
    public:
        explicit ResearchTechDependency(TechTypes techType);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity) const;
        virtual std::pair<BuildQueueTypes, int> getBuildItem() const;

        virtual void debug(std::ostream& os) const;

    private:
        TechTypes techType_;
    };

    class CityBuildingDependency : public IDependentTactic
    {
    public:
        explicit CityBuildingDependency(BuildingTypes buildingType);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity) const;
        virtual std::pair<BuildQueueTypes, int> getBuildItem() const;

        virtual void debug(std::ostream& os) const;

    private:
        BuildingTypes buildingType_;
    };

    class CivBuildingDependency : public IDependentTactic
    {
    public:
        CivBuildingDependency(BuildingTypes buildingType, int count);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity) const;
        virtual std::pair<BuildQueueTypes, int> getBuildItem() const;

        virtual void debug(std::ostream& os) const;

    private:        
        BuildingTypes buildingType_;
        int count_;
    };

    class ReligiousDependency : public IDependentTactic
    {
    public:
        ReligiousDependency(ReligionTypes religionType, UnitTypes unitType);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity) const;
        virtual std::pair<BuildQueueTypes, int> getBuildItem() const;

        virtual void debug(std::ostream& os) const;

    private:        
        ReligionTypes religionType_;
        UnitTypes unitType_;
    };

    class CityBuildingTactic : public ICityBuildingTactics, public boost::enable_shared_from_this<CityBuildingTactic>
    {
    public:
        CityBuildingTactic(BuildingTypes buildingType, IDInfo city);

        virtual IDInfo getCity() const;
        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual std::vector<IDependentTacticPtr> getDependencies() const;
        virtual void update(const Player& player, const CityDataPtr& pCityData);
        virtual void updateDependencies(const Player& player, const CvCity* pCity);
        virtual void apply(TacticSelectionData& selectionData);

        virtual BuildingTypes getBuildingType() const;
        virtual ProjectionLadder getProjection() const;

        virtual void debug(std::ostream& os) const;

    private:
        std::vector<IDependentTacticPtr> dependentTactics_;
        std::list<ICityBuildingTacticPtr> buildingTactics_;
        ProjectionLadder projection_;
        BuildingTypes buildingType_;
        IDInfo city_;
    };

    class FoodBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);
    private:
    };

    class HappyBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);
    private:
    };

    class HealthBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);
    private:
    };

    class ScienceBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);
    private:
    };

    class GoldBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);
    private:
    };

    class CultureBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);
    private:
    };

    class EspionageBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);
    private:
    };

    class SpecialistBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);
    private:
    };

    // world wonders
    class GlobalBuildingTactic : public IGlobalBuildingTactics
    {
    public:
        explicit GlobalBuildingTactic(BuildingTypes buildingType);

        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void update(const Player& player);
        virtual void updateDependencies(const Player& player);
        virtual void addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic);
        virtual std::list<ICityBuildingTacticsPtr> getCityTactics(IDInfo city) const;
        virtual void apply(TacticSelectionData& selectionData);
        virtual void removeCityTactics(IDInfo city);
        virtual bool empty() const;

        virtual BuildingTypes getBuildingType() const;

        virtual void debug(std::ostream& os) const;

    private:
        BuildingTypes buildingType_;
        typedef std::map<IDInfo, std::list<ICityBuildingTacticsPtr> > CityTacticsMap;
        CityTacticsMap cityTactics_;
    };

    //national wonders
    class NationalBuildingTactic : public IGlobalBuildingTactics
    {
    public:
        explicit NationalBuildingTactic(BuildingTypes buildingType);

        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void update(const Player& player);
        virtual void updateDependencies(const Player& player);
        virtual void addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic);
        virtual std::list<ICityBuildingTacticsPtr> getCityTactics(IDInfo city) const;
        virtual void apply(TacticSelectionData& selectionData);
        virtual void removeCityTactics(IDInfo city);
        virtual bool empty() const;

        virtual BuildingTypes getBuildingType() const;

        virtual void debug(std::ostream& os) const;

    private:
        BuildingTypes buildingType_;
        typedef std::map<IDInfo, std::list<ICityBuildingTacticsPtr> > CityTacticsMap;
        CityTacticsMap cityTactics_;
    };
}