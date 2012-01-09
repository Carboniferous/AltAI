#pragma once

#include "./utils.h"

namespace AltAI
{
    struct DotMapItem
    {
        struct PlotData
        {
            PlotData()
                : workedImprovement(-1), neighbourCityCount(0), workedByNeighbour(false), bonusType(NO_BONUS),
                  featureType(NO_FEATURE), isPinned(false), isSelected(true), improvementMakesBonusValid(false)
            {
            }

            PlotData(const CvPlot* pPlot, PlayerTypes playerType, int lookAheadDepth);

            explicit PlotData(XYCoords coords_) : coords(coords_), workedImprovement(-1), neighbourCityCount(0), workedByNeighbour(false),
                bonusType(NO_BONUS), featureType(NO_FEATURE), isPinned(false), isSelected(true), improvementMakesBonusValid(false) {}

            XYCoords coords;
            int workedImprovement, neighbourCityCount;  // todo - move neighbour data into separate struct just used by dotmapping code
            bool workedByNeighbour;
            BonusTypes bonusType;
            FeatureTypes featureType;
            std::vector<std::pair<PlotYield, ImprovementTypes> > possibleImprovements;
            bool isPinned, isSelected, improvementMakesBonusValid;
            bool operator < (const PlotData& other) const { return coords < other.coords; }
            PlotYield getPlotYield() const { return workedImprovement == -1 ? PlotYield() : possibleImprovements[workedImprovement].first; }

            PlotYield getPlotYield(int index) const { return (index == -1 || possibleImprovements.empty()) ? PlotYield() : possibleImprovements[index].first; }
            ImprovementTypes getWorkedImprovement() const { return workedImprovement == -1 ? NO_IMPROVEMENT : possibleImprovements[workedImprovement].second; }
            ImprovementTypes getWorkedImprovement(int index) const { return index == -1 ? NO_IMPROVEMENT : possibleImprovements[index].second; }
        };

        template <typename P>
            struct PlotDataAdaptor
        {
            typedef typename P Pred;
            PlotDataAdaptor(P pred_) : pred(pred_) {}

            bool operator () (const PlotData& p1, const PlotData& p2) const
            {
                return pred(p1.getPlotYield(), p2.getPlotYield());
            }
            P pred;
        };

        XYCoords coords;
        PlotYield cityPlotYield;
        std::set<BonusTypes> bonusTypes;
        int numDeadLockedBonuses;
        int areaID, subAreaID;
        typedef std::set<PlotData> PlotDataSet;
        typedef PlotDataSet::const_iterator PlotDataConstIter;
        typedef PlotDataSet::iterator PlotDataIter;
        typedef PlotDataSet::reverse_iterator PlotDataRIter;

        struct SelectedImprovement
        {
            SelectedImprovement(XYCoords coords_, int improvementIndex_, PlotYield plotYield_, bool improvementMakesBonusValid_)
                : coords(coords_), improvementIndex(improvementIndex_), plotYield(plotYield_), improvementMakesBonusValid(improvementMakesBonusValid_) {}
            XYCoords coords;
            int improvementIndex;
            PlotYield plotYield;
            bool improvementMakesBonusValid;
        };

        template <typename P>
            struct SelectedImprovementAdaptor
        {
            typedef typename P Pred;
            SelectedImprovementAdaptor(P pred_) : pred(pred_) {}

            bool operator () (const SelectedImprovement& p1, const SelectedImprovement& p2) const
            {
                return pred(p1.plotYield, p2.plotYield);
            }
            P pred;
        };

        template <typename Pred>
            std::list<SelectedImprovement> getBestImprovements(Pred pred) const
        {
            std::list<SelectedImprovement> bestImprovements;

            for (PlotDataConstIter plotIter(plotData.begin()), endIter(plotData.end()); plotIter != endIter; ++plotIter)
            {
                int bestImprovementIndex = plotIter->workedImprovement;
                PlotYield bestYield = plotIter->getPlotYield();
                bool improvementMakesBonusValid = false;

                if (!plotIter->isPinned)
                {
                    for (size_t i = 0, count = plotIter->possibleImprovements.size(); i < count; ++i)
                    {
                        if (plotIter->bonusType != NO_BONUS && plotIter->possibleImprovements[i].second != NO_IMPROVEMENT &&
                            gGlobals.getImprovementInfo(plotIter->possibleImprovements[i].second).isImprovementBonusMakesValid(plotIter->bonusType))
                        {
                            bestImprovementIndex = i;
                            improvementMakesBonusValid = true;
                            break;
                        }
                        else if (pred(plotIter->getPlotYield(i), bestYield))
                        {
                            bestYield = plotIter->getPlotYield(i);
                            bestImprovementIndex = i;
                        }
                    }
                }

                bestImprovements.push_front(SelectedImprovement(plotIter->coords, bestImprovementIndex, plotIter->getPlotYield(bestImprovementIndex), improvementMakesBonusValid));
            }

            bestImprovements.sort(SelectedImprovementAdaptor<Pred>(pred));

            return bestImprovements;
        }

        PlotDataSet plotData;

        DotMapItem(XYCoords coords_, PlotYield cityPlotYield_);
        bool operator < (const DotMapItem& other) const { return coords < other.coords; }
        PlotYield getWeightedOutput(PlayerTypes playerType) const;
        std::pair<PlotYield, int> getOutput(PlayerTypes playerType) const;
        PlotYield getActualOutput() const;
        PlotYield getOutput(PlayerTypes playerType, YieldWeights yieldWeights) const;
        PlotYield getOutput(XYCoords coords, int improvementIndex) const;
        PlotData getPlotData(XYCoords coords) const;
        void debugOutputs(std::ostream& os) const;
    };
}