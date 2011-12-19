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
}