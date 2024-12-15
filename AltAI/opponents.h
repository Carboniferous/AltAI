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
        void debug(std::ostream& os) const;

    private:
        Player& player_;
        std::vector<AttitudeHelperPtr> playerAttitudesToUs_, ourAttitudesToPlayers_;
        std::map<PlayerTypes, int> attitudeMap_;
    };
}