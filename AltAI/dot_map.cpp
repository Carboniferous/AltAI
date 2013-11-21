#include "AltAI.h"
#include "./dot_map.h"
#include "./plot_info.h"
#include "./plot_info_visitors.h"
#include "./helper_fns.h"

namespace AltAI
{
    DotMapItem::DotMapItem(XYCoords coords_, PlotYield cityPlotYield_) : coords(coords_), cityPlotYield(cityPlotYield_),
        numDeadLockedBonuses(0), areaID(-1), subAreaID(-1)
    {
    }

    DotMapItem::DotMapPlotData::DotMapPlotData(const CvPlot* pPlot, PlayerTypes playerType, int lookAheadDepth) :
        workedImprovement(-1), neighbourCityCount(0), workedByNeighbour(false), isPinned(false), isSelected(true), improvementMakesBonusValid(false)
    {
        TeamTypes teamType = PlayerIDToTeamID(playerType);

        coords = XYCoords(pPlot->getX(), pPlot->getY());
        possibleImprovements = getYields(PlotInfo(pPlot, playerType).getInfo(), playerType, lookAheadDepth);
        bonusType = pPlot->getBonusType(teamType);
        featureType = pPlot->getFeatureType();

        ImprovementTypes currentPlotImprovement = pPlot->getImprovementType();
        for (size_t i = 0, count = possibleImprovements.size(); i < count; ++i)
        {
            // todo - check featuretype too
            if (possibleImprovements[i].second == currentPlotImprovement)
            {
                workedImprovement = i;
                if (currentPlotImprovement != NO_IMPROVEMENT && bonusType != NO_BONUS && 
                    gGlobals.getImprovementInfo(currentPlotImprovement).isImprovementBonusMakesValid(bonusType))
                {
                    improvementMakesBonusValid = true;
                }
                break;
            }
        }
    }

    DotMapItem::DotMapPlotData DotMapItem::getPlotData(XYCoords coords) const
    {
        DotMapPlotData key(coords);
        PlotDataSet::const_iterator ci = plotData.find(key);
        return ci != plotData.end() ? *ci : DotMapPlotData();
    }

    PlotYield DotMapItem::getWeightedOutput(PlayerTypes playerType) const
    {
        YieldPriority yieldPriority;
        yieldPriority.assign(-1);
        yieldPriority[0] = YIELD_FOOD;

        MixedOutputOrderFunctor<PlotYield> mixedF(yieldPriority, OutputUtils<PlotYield>::getDefaultWeights());

        typedef std::multiset<DotMapPlotData, PlotDataAdaptor<MixedOutputOrderFunctor<PlotYield> > > SortedPlots;
        SortedPlots sortedPlots(mixedF);

        for (std::set<DotMapPlotData>::const_iterator pi(plotData.begin()), piEnd(plotData.end()); pi != piEnd; ++pi)
        {
            sortedPlots.insert(*pi);
        }

        PlotYield plotYield = 100 * cityPlotYield;
        int surplusFood = 100 * cityPlotYield[YIELD_FOOD];
        const int foodPerPop = 100 * gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        for (SortedPlots::iterator it(sortedPlots.begin()), itEnd(sortedPlots.end()); it != itEnd; ++it)
        {
            if (it->neighbourCityCount > 0)
            {
                continue;
            }

            PlotYield thisYield = 100 * it->getPlotYield();
            surplusFood += thisYield[YIELD_FOOD] - foodPerPop;
            if (surplusFood < 0)
            {
                break;
            }

            if (it->neighbourCityCount > 0)
            {
                for (int i = 0; i < NUM_YIELD_TYPES; ++i)
                {
                    thisYield[i] /= (2 + it->neighbourCityCount);
                }
            }
            else if (it->bonusType != NO_BONUS)
            {
                thisYield *= (thisYield[YIELD_FOOD] >= 300 ? 4 : 2);
            }

            plotYield += thisYield;
        }

        if (surplusFood >= foodPerPop)
        {
            plotYield += (surplusFood / foodPerPop) * makeYield(0, 150, 500);  // todo - write some code to give average specialist implied plotyield
        }

        for (int i = 0; i < NUM_YIELD_TYPES; ++i)
        {
            plotYield[i] = plotYield[i] / (1 + numDeadLockedBonuses);
            plotYield[i] /= 100;
        }

        return plotYield;
    }

    std::pair<PlotYield, int> DotMapItem::getOutput(PlayerTypes playerType) const
    {
        YieldPriority yieldPriority;
        yieldPriority.assign(-1);
        yieldPriority[0] = YIELD_FOOD;

        MixedOutputOrderFunctor<PlotYield> mixedF(yieldPriority, OutputUtils<PlotYield>::getDefaultWeights());

        typedef std::multiset<DotMapPlotData, PlotDataAdaptor<MixedOutputOrderFunctor<PlotYield> > > SortedPlots;
        SortedPlots sortedPlots(mixedF);

        for (std::set<DotMapPlotData>::const_iterator pi(plotData.begin()), piEnd(plotData.end()); pi != piEnd; ++pi)
        {
            sortedPlots.insert(*pi);
        }

        PlotYield plotYield = 100 * cityPlotYield;
        int surplusFood = 100 * cityPlotYield[YIELD_FOOD];
        const int foodPerPop = 100 * gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        for (SortedPlots::iterator it(sortedPlots.begin()), itEnd(sortedPlots.end()); it != itEnd; ++it)
        {
            if (it->neighbourCityCount > 0 && it->workedByNeighbour)
            {
                continue;
            }

            PlotYield thisYield = 100 * it->getPlotYield();
            surplusFood += thisYield[YIELD_FOOD] - foodPerPop;
            if (surplusFood < 0)
            {
                break;
            }

            plotYield += thisYield;
        }

        for (int i = 0; i < NUM_YIELD_TYPES; ++i)
        {
            plotYield[i] /= 100;
        }

        return std::make_pair<PlotYield, int>(plotYield, surplusFood / 100);
    }

    PlotYield DotMapItem::getOutput(PlayerTypes playerType, YieldWeights yieldWeights) const
    {
        YieldPriority yieldPriority;
        yieldPriority.assign(-1);
        yieldPriority[0] = YIELD_FOOD;

        MixedOutputOrderFunctor<PlotYield> mixedF(yieldPriority, yieldWeights);

        typedef std::multiset<DotMapPlotData, PlotDataAdaptor<MixedOutputOrderFunctor<PlotYield> > > SortedPlots;
        SortedPlots sortedPlots(mixedF);

        for (std::set<DotMapPlotData>::const_iterator pi(plotData.begin()), piEnd(plotData.end()); pi != piEnd; ++pi)
        {
            sortedPlots.insert(*pi);
        }

        int surplusFood = cityPlotYield[YIELD_FOOD];
        PlotYield totalYield(cityPlotYield);

        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();

        for (SortedPlots::iterator it(sortedPlots.begin()), itEnd(sortedPlots.end()); it != itEnd; ++it)
        {
            PlotYield thisYield = it->getPlotYield();
            surplusFood += thisYield[YIELD_FOOD] - foodPerPop;

            if (surplusFood < 0)
            {
                break;
            }

            totalYield += thisYield;
        }
        return totalYield;
    }

    PlotYield DotMapItem::getActualOutput() const
    {
        MixedOutputOrderFunctor<PlotYield> mixedF(makeYieldP(YIELD_FOOD), OutputUtils<PlotYield>::getDefaultWeights());

        typedef std::multiset<DotMapPlotData, PlotDataAdaptor<MixedOutputOrderFunctor<PlotYield> > > SortedPlots;
        SortedPlots sortedPlots(mixedF);

        for (std::set<DotMapPlotData>::const_iterator pi(plotData.begin()), piEnd(plotData.end()); pi != piEnd; ++pi)
        {
            sortedPlots.insert(*pi);
        }

        int surplusFood = cityPlotYield[YIELD_FOOD];
        PlotYield totalYield(cityPlotYield);

        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();

        for (SortedPlots::iterator it(sortedPlots.begin()), itEnd(sortedPlots.end()); it != itEnd; ++it)
        {
            if (!it->isSelected)
            {
                continue;
            }
            PlotYield thisYield = it->getPlotYield();
            surplusFood += thisYield[YIELD_FOOD] - foodPerPop;

            if (surplusFood < 0)
            {
                break;
            }

            totalYield += thisYield;
        }
        return totalYield;
    }

    PlotYield DotMapItem::getOutput(XYCoords coords, int improvementIndex) const
    {
        DotMapPlotData tmp(coords);
        PlotDataConstIter ci = plotData.find(tmp);
        if (ci != plotData.end())
        {
            return ci->getPlotYield(improvementIndex);
        }
        else
        {
            return PlotYield();
        }
    }

    void DotMapItem::debugOutputs(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        YieldPriority yieldPriority;
        yieldPriority.assign(-1);
        yieldPriority[0] = YIELD_FOOD;

        MixedOutputOrderFunctor<PlotYield> mixedF(yieldPriority, OutputUtils<PlotYield>::getDefaultWeights());

        typedef std::multiset<DotMapPlotData, PlotDataAdaptor<MixedOutputOrderFunctor<PlotYield> > > SortedPlots;
        SortedPlots sortedPlots(mixedF);

        for (std::set<DotMapPlotData>::const_iterator pi(plotData.begin()), piEnd(plotData.end()); pi != piEnd; ++pi)
        {
            sortedPlots.insert(*pi);
        }

        int surplusFood = cityPlotYield[YIELD_FOOD];
        PlotYield totalYield(cityPlotYield);

        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();

        for (SortedPlots::iterator it(sortedPlots.begin()), itEnd(sortedPlots.end()); it != itEnd; ++it)
        {
            if (!it->isSelected)
            {
                continue;
            }

            PlotYield thisYield = it->getPlotYield();
            surplusFood += thisYield[YIELD_FOOD] - foodPerPop;

            if (surplusFood < 0)
            {
                break;
            }

            totalYield += thisYield;

            os << "\n" << it->coords << " = " << thisYield;

            BonusTypes bonusType = it->bonusType;
            if (bonusType != NO_BONUS)
            {
                os << " (bonus = " << gGlobals.getBonusInfo(bonusType).getType() << ") ";
            }

            ImprovementTypes workedImprovement = it->getWorkedImprovement();
            if (workedImprovement != NO_IMPROVEMENT)
            {
                os << " (improvement = " << gGlobals.getImprovementInfo(workedImprovement).getType() << ") (surplus = " << surplusFood << " ) ";
            }
            else
            {
                os << " (no improvement, surplus = " << surplusFood << " ) ";
            }

            if (it->featureType != NO_FEATURE)
            {
                os << " (feature = " << gGlobals.getFeatureInfo(it->featureType).getType() << ")";
            }

            if (bonusType != NO_BONUS)
            {
                os << " (possible improvements = ";
                for (size_t i = 0, count = it->possibleImprovements.size(); i < count; ++i)
                {
                    if (it->possibleImprovements[i].second != NO_IMPROVEMENT)
                    {
                        os << gGlobals.getImprovementInfo(it->possibleImprovements[i].second).getType() << ", ";
                    }
                    else
                    {
                        os << "NO_IMPROVEMENT, ";
                    }
                }
                os << ")";
            }
            //else if (bonusType != NO_BONUS) // debug
            //{
            //    const CvPlot* pPlot = gGlobals.getMap().plot(it->coords.iX, it->coords.iY);
            //    PlotInfo plotInfo(pPlot, pPlot->getOwner());
            //    os << "\n" << plotInfo.getInfo();
            //}
        }
        //os << "\nTotal yield = " << totalYield << "\n";
#endif
    }
}