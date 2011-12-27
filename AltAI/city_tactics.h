#pragma once

#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;
    struct PlayerTactics;

    ConstructItem getConstructItem(const PlayerTactics& playerTactics, const City& city);
}