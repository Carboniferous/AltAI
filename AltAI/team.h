#pragma once

#include "../CvGameCoreDLL/CvGameCoreDLL.h"
#include "../CvGameCoreDLL/CvStructs.h"

#include "boost/shared_ptr.hpp"

class CvTeam;

namespace AltAI
{
    class Player;
    typedef boost::shared_ptr<Player> PlayerPtr;
    class IPlotEvent;

    class Team
    {
    public:
        Team() : pTeam_(NULL)
        {
        }

        explicit Team(const CvTeam* pTeam) : pTeam_(pTeam)
        {
        }

        void addPlayer(const PlayerPtr& pPlayer);

        //void pushPlotEvent(const boost::shared_ptr<IPlotEvent>& pPlotEvent);
        void updatePlotRevealed(const CvPlot* pPlot, bool isNew, bool isRevealed);
        void updatePlotBonus(const CvPlot* pPlot, BonusTypes revealedBonusType);

    private:
        const CvTeam* pTeam_;
        std::vector<PlayerPtr > players_;
    };
}