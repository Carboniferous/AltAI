#pragma once

#include "./utils.h"

namespace AltAI
{
    class Player;
    class City;
    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;

    std::pair<PlotYield, Commerce> getYieldAndCommerce(SpecialistTypes specialistType, PlayerTypes playerType);

    template <typename P>
        SpecialistTypes getBestSpecialist(const Player& player, YieldModifier yieldModifier, CommerceModifier commerceModifier, P valueF);
    template <typename P>
        SpecialistTypes getBestSpecialist(const Player& player, P valueF);

    template <typename P>
        std::vector<SpecialistTypes> getBestSpecialists(const Player& player, YieldModifier yieldModifier, CommerceModifier commerceModifier, size_t count, P valueF);
}