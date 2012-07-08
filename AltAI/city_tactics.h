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

    struct CityDefenceUnitValue
    {
        CityDefenceUnitValue() : unitType(NO_UNIT), nTurns(0), unitAnalysisValue(0)
        {
        }

        UnitTypes unitType;
        int nTurns, unitAnalysisValue;

        bool operator < (const CityDefenceUnitValue& other) const;
    };

    struct TacticSelectionData
    {
        std::set<CultureBuildingValue> smallCultureBuildings;
        std::set<EconomicBuildingValue> economicBuildings;
        std::map<IDInfo, std::vector<BuildingTypes> > buildingsCityCanAssistWith;
        std::map<BuildingTypes, std::vector<BuildingTypes> > dependentBuildings;
        std::map<BuildingTypes, EconomicWonderValue> economicWonders, nationalWonders;
        std::set<MilitaryBuildingValue> militaryBuildings;

        std::set<CityDefenceUnitValue> cityDefenceUnits;

        std::set<BuildingTypes> exclusions;

        TotalOutput getEconomicBuildingOutput(BuildingTypes buildingType, IDInfo city) const;
    };

    ConstructItem getConstructItem(const PlayerTactics& playerTactics, const City& city);
}