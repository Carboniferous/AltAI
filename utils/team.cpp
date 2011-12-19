#include "./utils.h"
#include "./team.h"
#include "./player.h"
#include "./city.h"
#include "./unit.h"

namespace AltAI
{
    void Team::addPlayer(const boost::shared_ptr<Player>& pPlayer)
    {
        players_.push_back(pPlayer);
    }

    void Team::pushPlotEvent(const boost::shared_ptr<IPlotEvent>& pPlotEvent)
    {
        for (size_t i = 0, count = players_.size(); i < count; ++i)
        {
            players_[i]->pushPlotEvent(pPlotEvent);
        }
    }
}