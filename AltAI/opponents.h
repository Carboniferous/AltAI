#pragma once

#include "./player.h"

namespace AltAI
{
    class AttitudeHelper;
    typedef boost::shared_ptr<AttitudeHelper> AttitudeHelperPtr;

    class OpponentsAnalysis
    {
    public:
        explicit OpponentsAnalysis(Player& player);
        void addPlayer(const CvPlayerAI* pPlayer);
        void updateAttitudes();

    private:
        Player& player_;
        std::vector<AttitudeHelperPtr> playerAttitudes_;
    };
}