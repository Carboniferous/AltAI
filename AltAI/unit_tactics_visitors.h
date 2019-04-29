#pragma once

#include "./utils.h"
#include "./tactic_actions.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class City;
    class Player;
    class UnitInfo;

    //ConstructItem getEconomicUnitTactics(const Player& player, UnitTypes unitType, const boost::shared_ptr<UnitInfo>& pUnitInfo);
    //ConstructItem getMilitaryExpansionUnitTactics(const Player& player, UnitTypes unitType, const boost::shared_ptr<UnitInfo>& pUnitInfo);

    CityUnitTacticsPtr makeCityUnitTactics(const Player& player, const City& city, const boost::shared_ptr<UnitInfo>& pUnitInfo);
    UnitTacticsPtr makeUnitTactics(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo);

    UnitTacticsPtr makeSpecialUnitTactics(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo);
}