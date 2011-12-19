#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;
    struct PlayerTactics;

    ConstructList makeBuildingTactics(Player& player);

    ConstructItem selectExpansionBuildingTactics(const Player& player, const City& city, const ConstructItem& constructItem);
    ConstructItem selectExpansionMilitaryBuildingTactics(const Player& player, const City& city, const ConstructItem& constructItem);

    ConstructItem getConstructItem(const PlayerTactics& playerTactics, const City& city);
}