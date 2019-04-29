#include "AltAI.h"

#include "./utils.h"
#include "./team.h"
#include "./player.h"
#include "./city.h"
#include "./unit.h"
#include "./player_analysis.h"
#include "./map_analysis.h"

namespace AltAI
{
    void Team::addPlayer(const PlayerPtr& pPlayer)
    {
        players_.push_back(pPlayer);
    }

    void Team::updatePlotRevealed(const CvPlot* pPlot, bool isNew)
    {
        for (size_t i = 0, count = players_.size(); i < count; ++i)
        {
            if (players_[i]->getCvPlayer()->isUsingAltAI())
            {
                players_[i]->updatePlotRevealed(pPlot, isNew);
            }
        }
    }

    void Team::updatePlotBonus(const CvPlot* pPlot, BonusTypes revealedBonusType)
    {
        for (size_t i = 0, count = players_.size(); i < count; ++i)
        {
            if (players_[i]->getCvPlayer()->isUsingAltAI())
            {
                players_[i]->updatePlotBonus(pPlot, revealedBonusType);
            }
        }
    }
}