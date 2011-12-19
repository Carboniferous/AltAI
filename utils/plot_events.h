#pragma once

#include "boost/shared_ptr.hpp"

class CvPlot;

namespace AltAI
{
    class MapAnalysis;

    class IPlotEvent
    {
    public:
        virtual ~IPlotEvent() {}
        virtual void handle(MapAnalysis&) = 0;
    };

    class RevealPlotEvent : public IPlotEvent
    {
    public:
        RevealPlotEvent(const CvPlot* pPlot, bool terrainOnly);
        virtual void handle(MapAnalysis& mapAnalysis);

    private:
        const CvPlot* pPlot_;
        bool terrainOnly_;
    };
}