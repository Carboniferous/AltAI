#pragma once

#include "./utils.h"
#include "./plot_data.h"

namespace AltAI
{
    class Player;

    struct DotMapItem
    {
        struct DotMapPlotData
        {
            DotMapPlotData()
                : workedImprovement(-1), neighbourCityCount(0), workedByNeighbour(false), bonusType(NO_BONUS),
                  featureType(NO_FEATURE), isPinned(false), isSelected(true), improvementMakesBonusValid(false)
            {
            }

            DotMapPlotData(const PlotData& plotData, PlayerTypes playerType, int lookAheadDepth);

            explicit DotMapPlotData(XYCoords coords_) : coords(coords_), workedImprovement(-1), neighbourCityCount(0), workedByNeighbour(false),
                bonusType(NO_BONUS), featureType(NO_FEATURE), isPinned(false), isSelected(true), improvementMakesBonusValid(false) {}

            XYCoords coords;
            int workedImprovement, neighbourCityCount;  // todo - move neighbour data into separate struct just used by dotmapping code
            bool workedByNeighbour;
            BonusTypes bonusType;
            FeatureTypes featureType;
            std::vector<std::pair<PlotYield, ImprovementTypes> > possibleImprovements;
            PlotYield currentYield, conditionalYield;
            bool isPinned, isSelected, improvementMakesBonusValid;
            bool operator < (const DotMapPlotData& other) const { return coords < other.coords; }
            PlotYield getPlotYield() const { return workedImprovement == -1 ? currentYield : possibleImprovements[workedImprovement].first; }
            PlotYield getProjectedPlotYield(const Player& player, const int timeHorizon) const;

            PlotYield getPlotYield(int index) const { return (index == -1 || possibleImprovements.empty()) ? currentYield : possibleImprovements[index].first; }
            ImprovementTypes getWorkedImprovement() const { return workedImprovement == -1 ? NO_IMPROVEMENT : possibleImprovements[workedImprovement].second; }
            ImprovementTypes getWorkedImprovement(int index) const { return index == -1 ? NO_IMPROVEMENT : possibleImprovements[index].second; }
            bool setWorkedImpIndex(ImprovementTypes currentImp);

            void debug(std::ostream& os) const;
        };

        template <typename P>
            struct PlotDataAdaptor
        {
            typedef typename P Pred;
            PlotDataAdaptor(P pred_) : pred(pred_) {}

            bool operator () (const DotMapPlotData& p1, const DotMapPlotData& p2) const
            {
                return pred(p1.getPlotYield(), p2.getPlotYield());
            }
            P pred;
        };

        typedef std::multiset<DotMapPlotData, PlotDataAdaptor<MixedOutputOrderFunctor<PlotYield> > > SortedPlots;

        XYCoords coords;
        PlotYield cityPlotYield, conditionalYield;
        bool isFreshWater;
        std::set<BonusTypes> bonusTypes;
        int numDeadLockedBonuses;
        std::vector<std::pair<BuildingTypes, PlotYield> > requiredAdditionalYieldBuildings;
        int areaID, subAreaID;
        SortedPlots selectedPlots;
        PlotYield projectedYield;
        int projectedTurns;

        typedef std::set<DotMapPlotData> PlotDataSet;
        PlotDataSet plotDataSet;

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

            for (PlotDataConstIter plotIter(plotDataSet.begin()), endIter(plotDataSet.end()); plotIter != endIter; ++plotIter)
            {
                int bestImprovementIndex = plotIter->workedImprovement;
                PlotYield bestYield = plotIter->getPlotYield();
                bool improvementMakesBonusValid = 
                    plotIter->bonusType != NO_BONUS && 
                    bestImprovementIndex != -1 &&
                    plotIter->possibleImprovements[bestImprovementIndex].second != NO_IMPROVEMENT &&
                    gGlobals.getImprovementInfo(plotIter->possibleImprovements[bestImprovementIndex].second).isImprovementBonusMakesValid(plotIter->bonusType);

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

        DotMapItem(XYCoords coords_, PlotYield cityPlotYield_, bool isFreshWater_ = false, BonusTypes bonusType = NO_BONUS);
        bool operator < (const DotMapItem& other) const { return coords < other.coords; }
        void calcOutput(const Player& player, int lookAheadTurns, int baseHealthyPop, bool debug = false);

        int getFoundValue(const Player& player) const;
        std::pair<PlotYield, int> getOutput() const;
        PlotYield getOutput(XYCoords coords, int improvementIndex) const;
        std::vector<BonusTypes> getNewBonuses(const std::set<BonusTypes>& existingBonuses) const;
        int getBaseFeatureHealth() const;

        DotMapPlotData getPlotData(XYCoords coords) const;
        std::vector<int> getGrowthRates(const CvPlayer& player, size_t maxPop, int baseHealthyPop, int cultureLevel);
        void debugOutputs(const Player& player, std::ostream& os) const;

        struct BuildTimesData
        {
            std::vector<int> impBuildTimes, impOutputs, foodOutputs;
            std::vector<size_t> impIndices;
        };

        void getBuildTimesData(BuildTimesData& impBuildData, PlayerTypes playerType, int cultureLevel, int baseHealthyPop);
        PlotYield getUsablePlots(const Player& player, SortedPlots& sortedPlots, int baseHealthyPop, int lookAheadTurns, bool debug = false);
    };
}