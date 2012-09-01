#pragma once

#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;

    struct PlayerTactics;

    ConstructList makeUnitTactics(Player& player);

    ConstructItem selectExpansionUnitTactics(const Player& player, const ConstructItem& constructItem);

    UnitTypes getConstructItem(const PlayerTactics& playerTactics);

    std::vector<UnitTypes> getPossibleCombatUnits(const Player& player, DomainTypes domainType);
}