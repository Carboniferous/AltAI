#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class Player;
    class City;
    class ResourceInfo;

    ResourceTacticsPtr makeResourceTactics(const Player& player, const boost::shared_ptr<ResourceInfo>& pResourceInfo);
}