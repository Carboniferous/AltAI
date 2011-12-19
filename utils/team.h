#pragma once

#include "CvGameCoreDLL.h"
#include "CvStructs.h"

#include "boost/shared_ptr.hpp"

class CvTeam;

namespace AltAI
{
    class Player;
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

        void addPlayer(const boost::shared_ptr<Player>& pPlayer);

        void pushPlotEvent(const boost::shared_ptr<IPlotEvent>& pPlotEvent);

    private:
        const CvTeam* pTeam_;
        std::vector<boost::shared_ptr<Player> > players_;
    };
}