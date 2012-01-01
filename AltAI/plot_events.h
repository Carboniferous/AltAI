#pragma once

#include "./utils.h"

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

    class RevealBonusEvent : public IPlotEvent
    {
    public:
        RevealBonusEvent(const CvPlot* pPlot, BonusTypes bonusType);
        virtual void handle(MapAnalysis& mapAnalysis);

    private:
        const CvPlot* pPlot_;
        BonusTypes bonusType_;
    };
}