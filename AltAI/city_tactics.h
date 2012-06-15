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

    struct TacticSelectionData
    {        
        std::set<CultureBuildingValue> smallCultureBuildings;
        std::set<EconomicBuildingValue> economicBuildings;
        std::map<IDInfo, std::vector<BuildingTypes> > buildingsCityCanAssistWith;
        std::map<BuildingTypes, EconomicWonderValue> economicWonders;
    };

    ConstructItem getConstructItem(const PlayerTactics& playerTactics, const City& city);
}