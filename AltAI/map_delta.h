#pragma once

#include "./utils.h"

namespace AltAI
{
    class Player;

    class PlotUpdates
    {
    public:
        explicit PlotUpdates(Player& player) : player_(player)
        {
        }

        void clear();

        void updatePlotOwner(const CvPlot* pPlot, PlayerTypes previousRevealedOwner, PlayerTypes newRevealedOwner);
        void updatePlotBonus(const CvPlot* pPlot, BonusTypes revealedBonusType);

        const std::list<const CvPlot*>& getNewBonusPlots() const;

    private:
        Player& player_;
        std::list<const CvPlot*> newPlots_, lostPlots_;        
        std::list<IDInfo> newCities_, lostCities_;
        std::list<const CvPlot*> newBonusPlots_;
    };
}