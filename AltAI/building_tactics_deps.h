#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class ResearchTechDependency : public IDependentTactic
    {
    public:
        ResearchTechDependency() : techType_(NO_TECH) {}
        explicit ResearchTechDependency(TechTypes techType);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity, int depTacticFlags) const;
        virtual bool required(const Player& player, int depTacticFlags) const;
        virtual bool removeable() const;
        virtual bool matches(int depTacticFlags) const;
        virtual BuildQueueItem getBuildItem() const;
        virtual std::vector<DependencyItem> getDependencyItems() const;

        virtual void debug(std::ostream& os) const;

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IDependentTactic::ResearchTechDependencyID;

        TechTypes getResearchTech() const { return techType_; }

    private:
        TechTypes techType_;
    };

    class CityBuildingDependency : public IDependentTactic
    {
    public:
        CityBuildingDependency() : buildingType_(NO_BUILDING) {}
        explicit CityBuildingDependency(BuildingTypes buildingType);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity, int depTacticFlags) const;
        virtual bool required(const Player& player, int depTacticFlags) const;
        virtual bool removeable() const;
        virtual bool matches(int depTacticFlags) const;
        virtual BuildQueueItem getBuildItem() const;
        virtual std::vector<DependencyItem> getDependencyItems() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IDependentTactic::CityBuildingDependencyID;

    private:
        BuildingTypes buildingType_;
    };

    class CivBuildingDependency : public IDependentTactic
    {
    public:
        CivBuildingDependency() : buildingType_(NO_BUILDING), sourceBuildingType_(NO_BUILDING), count_(0) {}
        CivBuildingDependency(BuildingTypes buildingType, int count, BuildingTypes sourcebuildingType);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity, int depTacticFlags) const;
        virtual bool required(const Player& player, int depTacticFlags) const;
        virtual bool removeable() const;
        virtual bool matches(int depTacticFlags) const;
        virtual BuildQueueItem getBuildItem() const;
        virtual std::vector<DependencyItem> getDependencyItems() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IDependentTactic::CivBuildingDependencyID;

    private:        
        BuildingTypes buildingType_, sourceBuildingType_;
        int count_;
    };

    class ReligiousDependency : public IDependentTactic
    {
    public:
        ReligiousDependency() : religionType_(NO_RELIGION), unitType_(NO_UNIT) {}
        explicit ReligiousDependency(ReligionTypes religionType, UnitTypes unitType);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity, int depTacticFlags) const;
        virtual bool required(const Player& player, int depTacticFlags) const;
        virtual bool removeable() const;
        virtual bool matches(int depTacticFlags) const;
        virtual BuildQueueItem getBuildItem() const;
        virtual std::vector<DependencyItem> getDependencyItems() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IDependentTactic::ReligiousDependencyID;

    private:        
        ReligionTypes religionType_;
        UnitTypes unitType_;
    };

    class StateReligionDependency : public IDependentTactic
    {
    public:
        StateReligionDependency() {}
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity, int depTacticFlags) const;
        virtual bool required(const Player& player, int depTacticFlags) const;
        virtual bool removeable() const;
        virtual bool matches(int depTacticFlags) const;
        virtual BuildQueueItem getBuildItem() const;
        virtual std::vector<DependencyItem> getDependencyItems() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IDependentTactic::StateReligionDependencyID;

    private:        
    };

    class CityBonusDependency : public IDependentTactic
    {
    public:
        CityBonusDependency() : unitType_(NO_UNIT) {}
        CityBonusDependency(BonusTypes bonusType, UnitTypes unitType, TechTypes bonusRevealTech);
        CityBonusDependency(const std::vector<BonusTypes>& andBonusTypes, const std::vector<BonusTypes>& orBonusTypes, 
            const std::map<BonusTypes, TechTypes>& bonusTypesRevealTechsMap, UnitTypes unitType);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity, int depTacticFlags) const;
        virtual bool required(const Player& player, int depTacticFlags) const;
        virtual bool removeable() const;
        virtual bool matches(int depTacticFlags) const;
        virtual BuildQueueItem getBuildItem() const;
        virtual std::vector<DependencyItem> getDependencyItems() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IDependentTactic::CityBonusDependencyID;

    private:        
        std::vector<BonusTypes> andBonusTypes_, orBonusTypes_;
        std::map<BonusTypes, TechTypes> bonusTypesRevealTechsMap_;
        UnitTypes unitType_;
    };

    class CivUnitDependency : public IDependentTactic
    {
    public:
        CivUnitDependency() : unitType_(NO_UNIT) {}
        explicit CivUnitDependency(UnitTypes unitType);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity, int depTacticFlags) const;
        virtual bool required(const Player& player, int depTacticFlags) const;
        virtual bool removeable() const;
        virtual bool matches(int depTacticFlags) const;
        virtual BuildQueueItem getBuildItem() const;
        virtual std::vector<DependencyItem> getDependencyItems() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IDependentTactic::CivUnitDependencyID;

    private:        
        UnitTypes unitType_;
    };

    class ResourceProductionBonusDependency : public IDependentTactic
    {
    public:
        ResourceProductionBonusDependency() : bonusType_(NO_BONUS), productionModifier_(0) {}
        ResourceProductionBonusDependency(BonusTypes bonusType, int productionModifier);
        virtual void apply(const CityDataPtr& pCityData);
        virtual void remove(const CityDataPtr& pCityData);
        virtual bool required(const CvCity* pCity, int depTacticFlags) const;
        virtual bool required(const Player& player, int depTacticFlags) const;
        virtual bool removeable() const;
        virtual bool matches(int depTacticFlags) const;
        virtual BuildQueueItem getBuildItem() const;
        virtual std::vector<DependencyItem> getDependencyItems() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IDependentTactic::ResouceProductionBonusDependencyID;

    private:        
        BonusTypes bonusType_;
        int productionModifier_;
    };
}