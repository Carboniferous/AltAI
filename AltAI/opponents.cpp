#include "AltAI.h"

#include "./opponents.h"
#include "./player.h"
#include "./attitude_helper.h"
#include "./helper_fns.h"

namespace AltAI
{
    OpponentsAnalysis::OpponentsAnalysis(Player& player) : player_(player), playerAttitudesToUs_(MAX_PLAYERS), ourAttitudesToPlayers_(MAX_PLAYERS)
    {
    }

    void OpponentsAnalysis::addPlayer(const CvPlayerAI* pPlayer)
    {
        playerAttitudesToUs_[pPlayer->getID()] = AttitudeHelperPtr(new AttitudeHelper(pPlayer, player_.getPlayerID(), true));
        ourAttitudesToPlayers_[pPlayer->getID()] = AttitudeHelperPtr(new AttitudeHelper(player_.getCvPlayer(), pPlayer->getID(), false));
    }

    void OpponentsAnalysis::updateAttitudes()
    {
        attitudeMap_.clear();
        for (size_t i = 0, count = playerAttitudesToUs_.size(); i < count; ++i)
        {
            if (playerAttitudesToUs_[i])
            {
                playerAttitudesToUs_[i]->updateAttitude();
                ourAttitudesToPlayers_[i]->updateAttitude();
                attitudeMap_[(PlayerTypes)i] = playerAttitudesToUs_[i]->getCurrentAttitude().getAttitude();
            }
        }
    }

    void OpponentsAnalysis::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nAttitudes towards us: ";
        for (std::map<PlayerTypes, int>::const_iterator aIter(attitudeMap_.begin()), aEndIter(attitudeMap_.end()); aIter != aEndIter; ++aIter)
        {
            os << "\n\t" << aIter->first << " (" << safeGetPlayerName(aIter->first) << ") = " << aIter->second;
        }
#endif
    }
}
