#include "./tech_manager.h"
#include "./player.h"
#include "./city.h"

namespace AltAI
{
    TechManager::TechManager(const boost::shared_ptr<MapAnalysis>& pMapAnalysis) : pMapAnalysis_(pMapAnalysis)
    {
        playerType_ = pMapAnalysis->getPlayer().getPlayerID();
        teamType_ = pMapAnalysis->getPlayer().getTeamID();
    }

    TechTypes TechManager::getBestWorkerTech() const
    {
        return NO_TECH;
    }
}