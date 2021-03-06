#include "AltAI.h"

#include "./plot_events.h"
#include "./map_analysis.h"

namespace AltAI
{
    RevealPlotEvent::RevealPlotEvent(const CvPlot* pPlot, bool terrainOnly) : pPlot_(pPlot), terrainOnly_(terrainOnly)
    {
    }

    void RevealPlotEvent::handle(MapAnalysis& mapAnalysis)
    {
        mapAnalysis.updatePlotRevealed(pPlot_);
    }

    RevealBonusEvent::RevealBonusEvent(const CvPlot* pPlot, BonusTypes bonusType) : pPlot_(pPlot), bonusType_(bonusType)
    {
    }

    void RevealBonusEvent::handle(MapAnalysis& mapAnalysis)
    {
        mapAnalysis.updatePlotBonus(pPlot_, bonusType_);
    }
}