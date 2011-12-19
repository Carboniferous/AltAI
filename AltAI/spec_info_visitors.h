#pragma once

#include "./utils.h"

namespace AltAI
{
    class Player;
    class City;

    std::pair<PlotYield, Commerce> getYieldAndCommerce(SpecialistTypes specialistType, PlayerTypes playerType);

    SpecialistTypes getBestSpecialist(const Player& player, const City& city, MixedTotalOutputOrderFunctor valueF);
    SpecialistTypes getBestSpecialist(const Player& player, MixedTotalOutputOrderFunctor valueF);
}