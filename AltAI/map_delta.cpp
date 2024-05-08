#include "AltAI.h"

#include "./map_delta.h"
#include "./game.h"
#include "./player.h"
#include "./civ_log.h"

namespace AltAI
{
    void PlotUpdates::clear()
    {
        lostPlots_.clear();
        newPlots_.clear();
        newCities_.clear();
        lostCities_.clear();
        newBonusPlots_.clear();
    }

    void PlotUpdates::updatePlotOwner(const CvPlot* pPlot, PlayerTypes previousRevealedOwner, PlayerTypes newRevealedOwner)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
        // can lose plots which are briefly gained when conquering a city, but then are lost until the city comes out of post conquest revolt
        if (previousRevealedOwner == player_.getPlayerID() && newRevealedOwner != previousRevealedOwner)
        {
#ifdef ALTAI_DEBUG
            os << "\nlost plot: " << pPlot->getCoords() << " prev owner: " << previousRevealedOwner << " new owner: " << newRevealedOwner;
#endif
            lostPlots_.push_back(pPlot);

            // if we have this plot as a new plot, remove it
            std::list<const CvPlot*>::iterator newPlotsIter = std::find(newPlots_.begin(), newPlots_.end(), pPlot);
            if (newPlotsIter != newPlots_.end())
            {
                newPlots_.erase(newPlotsIter);
            }
        }
        else if (newRevealedOwner == player_.getPlayerID())
        {
#ifdef ALTAI_DEBUG
            os << "\ngained plot: " << pPlot->getCoords() << " prev owner: " << previousRevealedOwner << " new owner: " << newRevealedOwner;;
#endif
            // if we have this plot as a lost plot, remove it
            std::list<const CvPlot*>::iterator lostPlotsIter = std::find(lostPlots_.begin(), lostPlots_.end(), pPlot);
            if (lostPlotsIter != lostPlots_.end())
            {
                lostPlots_.erase(lostPlotsIter);
            }

            newPlots_.push_back(pPlot);
        }
    }

    void PlotUpdates::updatePlotBonus(const CvPlot* pPlot, BonusTypes revealedBonusType)
    {
        if (pPlot->getOwner() == player_.getPlayerID())
        {
            newBonusPlots_.push_back(pPlot);
        }
    }

    const std::list<const CvPlot*>& PlotUpdates::getNewBonusPlots() const
    {
        return newBonusPlots_;
    }
}