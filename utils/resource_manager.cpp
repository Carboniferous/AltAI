#include "./resource_manager.h"
#include "./player.h"
#include "./city.h"

namespace AltAI
{
    ResourceManager::ResourceManager(const boost::shared_ptr<MapAnalysis>& pMapAnalysis) : pMapAnalysis_(pMapAnalysis)
    {
        playerType_ = pMapAnalysis->getPlayer().getPlayerID();
        teamType_ = pMapAnalysis->getPlayer().getTeamID();
    }
}