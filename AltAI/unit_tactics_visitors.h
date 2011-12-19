#pragma once

#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class UnitInfo;

    ConstructItem getEconomicUnitTactics(const Player& player, UnitTypes unitType, const boost::shared_ptr<UnitInfo>& pUnitInfo);
    ConstructItem getMilitaryExpansionUnitTactics(const Player& player, UnitTypes unitType, const boost::shared_ptr<UnitInfo>& pUnitInfo);
}