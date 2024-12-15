#pragma once

#include "./utils.h"

namespace AltAI
{
    class Player;
    class City;

    struct PlayerTactics;

    CivicTypes chooseCivic(PlayerTactics& playerTactics, CivicOptionTypes civicOptionType);
}