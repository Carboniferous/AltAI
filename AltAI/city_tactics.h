#pragma once

#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;
    struct PlayerTactics;

    struct CultureBuildingValue
    {
        CultureBuildingValue() : buildingType(NO_BUILDING), nTurns(0)
        {
        }

        BuildingTypes buildingType;
        int nTurns;
        TotalOutput output;

        bool operator < (const CultureBuildingValue& other) const;
    };

    struct EconomicBuildingValue
    {
        EconomicBuildingValue() : buildingType(NO_BUILDING), nTurns(0)
        {
        }

        BuildingTypes buildingType;
        int nTurns;
        TotalOutput output;

        bool operator < (const EconomicBuildingValue& other) const;
    };

    struct EconomicWonderValue
    {
        std::vector<std::pair<IDInfo, EconomicBuildingValue> > buildCityValues;
    };

    struct MilitaryBuildingValue
    {
        MilitaryBuildingValue() : buildingType(NO_BUILDING), nTurns(0)
        {
        }

        BuildingTypes buildingType;
        int freeExperience, globalFreeExperience;
        std::map<DomainTypes, int> domainFreeExperience;
        std::map<UnitCombatTypes, int> combatTypeFreeExperience;
        PromotionTypes freePromotion;
        int nTurns;

        bool operator < (const MilitaryBuildingValue& other) const;
    };

    struct UnitTacticValue
    {
        UnitTacticValue() : unitType(NO_UNIT), nTurns(0), unitAnalysisValue(0)
        {
        }

        UnitTypes unitType;
        int nTurns, unitAnalysisValue;

        bool operator < (const UnitTacticValue& other) const;
    };

    struct WorkerUnitValue
    {
        WorkerUnitValue() : unitType(NO_UNIT), nTurns(0)
        {
        }

        UnitTypes unitType;
        int nTurns;
        typedef boost::tuple<XYCoords, TotalOutput, std::vector<TechTypes> > BuildData;
        typedef std::map<BuildTypes, std::vector<BuildData> > BuildsMap;
        BuildsMap buildsMap, consumedBuildsMap;

        void addBuild(BuildTypes buildType, BuildData buildData);

        bool operator < (const WorkerUnitValue& other) const;
        int getBuildValue() const;
        int getHighestConsumedBuildValue() const;
        int getBuildsCount() const;
        void debug(std::ostream& os) const;

    private:
        void addBuild_(BuildsMap& buildsMap_, BuildTypes buildType, BuildData buildData);
        void debugBuildsMap_(std::ostream& os, const BuildsMap& buildsMap_) const;
    };

    struct TacticSelectionData
    {
        TacticSelectionData(IDInfo city_) : city(city_)
        {
        }

        IDInfo city;

        std::set<CultureBuildingValue> smallCultureBuildings, largeCultureBuildings;
        std::set<EconomicBuildingValue> economicBuildings;
        std::map<IDInfo, std::vector<BuildingTypes> > buildingsCityCanAssistWith;
        std::map<BuildingTypes, std::vector<BuildingTypes> > dependentBuildings;
        std::map<BuildingTypes, EconomicWonderValue> economicWonders, nationalWonders;
        std::set<MilitaryBuildingValue> militaryBuildings;

        std::set<UnitTacticValue> cityDefenceUnits, cityAttackUnits;

        std::map<UnitTypes, WorkerUnitValue> workerUnits;

        std::set<BuildingTypes> exclusions;

        TotalOutput getEconomicBuildingOutput(BuildingTypes buildingType, IDInfo city) const;
    };

    ConstructItem getConstructItem(const PlayerTactics& playerTactics, const City& city);
}