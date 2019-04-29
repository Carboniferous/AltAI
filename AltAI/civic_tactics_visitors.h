#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class Player;
    class City;
    class CivicInfo;

    CivicTacticsPtr makeCivicTactics(const Player& player, const boost::shared_ptr<CivicInfo>& pCivicInfo);
}