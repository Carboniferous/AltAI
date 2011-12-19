#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;

    struct PlayerTactics;

    std::list<ResearchTech> makeTechTactics(Player& player);

    ResearchTech selectReligionTechTactics(const Player& player, const ResearchTech& researchTech);
    ResearchTech selectWorkerTechTactics(const Player& player, const ResearchTech& researchTech);
    ResearchTech selectExpansionTechTactics(const Player& player, const ResearchTech& researchTech);

    TechTypes getResearchTech(const PlayerTactics& playerTactics, TechTypes ignoreTechType = NO_TECH);
}