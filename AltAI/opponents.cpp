#include "AltAI.h"

#include "./opponents.h"
#include "./player.h"
#include "./attitude_helper.h"

namespace AltAI
{
    OpponentsAnalysis::OpponentsAnalysis(Player& player) : player_(player), playerAttitudes_(MAX_PLAYERS)
    {
    }

    void OpponentsAnalysis::addPlayer(const CvPlayerAI* pPlayer)
    {
        playerAttitudes_[pPlayer->getID()] = AttitudeHelperPtr(new AttitudeHelper(pPlayer));
    }

    void OpponentsAnalysis::updateAttitudes()
    {
        for (size_t i = 0, count = playerAttitudes_.size(); i < count; ++i)
        {
            if (playerAttitudes_[i])
            {
                playerAttitudes_[i]->updateAttitudeTowards((CvPlayerAI*)(player_.getCvPlayer()));
            }
        }
    }
}
