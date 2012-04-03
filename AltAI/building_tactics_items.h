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

        virtual void debug(std::ostream& os) const;

    private:        
        BuildingTypes buildingType_;
        int count_;
    };

    class CityBuildingTactic : public ICityBuildingTactics
    {
    public:
        explicit CityBuildingTactic(BuildingTypes buildingType);

        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void update(const Player& player, const CityDataPtr& pCityData);

        virtual BuildingTypes getBuildingType() const;
        virtual ProjectionLadder getProjection() const;

        virtual void debug(std::ostream& os) const;

    private:
        std::vector<IDependentTacticPtr> dependentTactics_;
        std::list<ICityBuildingTacticPtr> buildingTactics_;
        ProjectionLadder projection_;
        BuildingTypes buildingType_;
    };

    class HappyBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
    private:
    };

    class HealthBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
    private:
    };

    class ScienceBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
    private:
    };

    class GoldBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
    private:
    };

    class CultureBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
    private:
    };

    class EspionageBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
    private:
    };

    class SpecialistBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
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
        virtual void addCityTactic(const ICityBuildingTacticsPtr& pCityTactic);

        virtual BuildingTypes getBuildingType() const;

        virtual void debug(std::ostream& os) const;

    private:
        BuildingTypes buildingType_;
        std::list<ICityBuildingTacticsPtr> cities_;
    };

    //national wonders
    class NationalBuildingTactic : public IGlobalBuildingTactics
    {
    public:
        explicit NationalBuildingTactic(BuildingTypes buildingType);

        virtual void addTactic(const ICityBuildingTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void update(const Player& player);
        virtual void addCityTactic(const ICityBuildingTacticsPtr& pCityTactic);

        virtual BuildingTypes getBuildingType() const;

        virtual void debug(std::ostream& os) const;

    private:
        BuildingTypes buildingType_;
        std::list<ICityBuildingTacticsPtr> cities_;
    };
}