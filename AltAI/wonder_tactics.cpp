#include "AltAI.h"

#include "./wonder_tactics.h"
#include "./player.h"

namespace AltAI
{
    class WonderAnalysisImpl
    {
    public:
        explicit WonderAnalysisImpl(Player& player) : player_(player)
        {
        }

        void updateCity(const CvCity* pCity, bool remove)
        {
            if (remove)
            {
            }
            else
            {
            }
        }

    private:
        Player& player_;
    };

    WonderAnalysis::WonderAnalysis(Player& player)
    {
        pImpl_ = boost::shared_ptr<WonderAnalysisImpl>(new WonderAnalysisImpl(player));
    }
}