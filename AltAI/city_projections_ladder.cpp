#include "AltAI.h"

#include "./city_projections_ladder.h"
#include "./save_utils.h"

namespace AltAI
{
    ProjectionLadder::PlotDiff::PlotDiff(const PlotData& plotData, bool isNewWorked_, bool isOldWorked_)
        : coords(plotData.coords), improvementType(plotData.improvementType), 
          plotYield(plotData.plotYield), actualOutput(plotData.actualOutput),
          isWorked(isNewWorked_), wasWorked(isOldWorked_)
    {
    }

    void ProjectionLadder::PlotDiff::debug(std::ostream& os) const
    {
        bool isActualPlot = coords.iX != -1;
        if (isWorked && !wasWorked)
        {
            os << " new: ";
        }
        else if (!isWorked && wasWorked)
        {
            os << " old: ";
        }
        else
        {
            os << " change: ";
        }

        if (isActualPlot)
        {            
            os << coords << " " << plotYield << "" << actualOutput;
        }
        else
        {
            const CvSpecialistInfo& specInfo = gGlobals.getSpecialistInfo((SpecialistTypes)coords.iY);
            os << " " << specInfo.getType() << " " << actualOutput;
        }
    }

    void ProjectionLadder::ConstructedUnit::debug(std::ostream& os) const
    {
        os << " Unit: " << (unitType == NO_UNIT ? "none" : gGlobals.getUnitInfo(unitType).getType())
            << " turns = " << turns
            << " level = " << level << " exp = " << experience;
        for (Promotions::const_iterator ci(promotions.begin()), ciEnd(promotions.end()); ci != ciEnd; ++ci)
        {
            os << gGlobals.getPromotionInfo(*ci).getType() << " ";
        }
    }

    void ProjectionLadder::debug(std::ostream& os) const
    {
        int turn = 0;
        TotalOutput cumulativeOutput;
        int cumulativeCost = 0;

//        for (size_t i = 0, count = entries.size(); i < count; ++i)
//        {
//            os << "\n\tTurns = " << entries[i].turns << ", pop = " << entries[i].pop << ", output = " << entries[i].output << ", cost = " << entries[i].cost;
//            if (!isEmpty(entries[i].processOutput))
//            {
//                os << ", process output = " << entries[i].processOutput;
//            }
//#ifdef ALTAI_DEBUG
//            os << entries[i].debugSummary;
//#endif
//        }

        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {         
            turn += entries[i].turns;
            cumulativeOutput += entries[i].output * entries[i].turns;
            cumulativeCost += entries[i].cost * entries[i].turns;

            os << "\nPop = " << entries[i].pop << " turn = " << turn << " output = " << entries[i].output << " * " << entries[i].turns << " cost = " << entries[i].cost
               << " prod = " << entries[i].accumulatedProduction;
#ifdef ALTAI_DEBUG
            os << " " << entries[i].debugSummary;
#endif

            if (!isEmpty(entries[i].processOutput))
            {
                os << ", process output = " << entries[i].processOutput;
            }

            if (!entries[i].gpp.empty()) os << ", GPP data: ";
            for (GreatPersonOutputMap::const_iterator ci(entries[i].gpp.begin()), ciEnd(entries[i].gpp.end()); ci != ciEnd; ++ci)
            {
                os << gGlobals.getUnitInfo(ci->first).getType() << " = " << ci->second * entries[i].turns;
            }

            for (size_t j = 0, hurryCount = entries[i].hurryData.size(); j < hurryCount; ++j)
            {
                os << entries[i].hurryData[j];
            }

            if (i > 0)
            {
                os << ", delta = " << entries[i].output - entries[i - 1].output;
            }
        }

        for (size_t i = 0, count = buildings.size(); i < count; ++i)
        {
            os << "\n\tBuilding: " << gGlobals.getBuildingInfo(buildings[i].second).getType() << " built: " << buildings[i].first;
        }

        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            os << "\n\t";
            units[i].debug(os);
        }

        /*if (!comparisons.empty())
        {
            os << "\n\t" << comparisons.size() << " comparisons: ";
            comparisons[0].debug(os);
        }*/
    }

    void ProjectionLadder::debugDiffs(std::ostream& os) const
    {
        int turn = 0;
        for (size_t i = 0, count = workedPlotDiffs.size(); i < count; ++i)
        {
            turn += entries[i].turns;
            os << "\n\t t = " << turn;
            if (workedPlotDiffs[i].empty())
            {
                os << " (none) ";
            }
            for (PlotDiffList::const_iterator iter(workedPlotDiffs[i].begin()), endIter(workedPlotDiffs[i].end());
                iter != endIter; ++iter)
            {
                iter->debug(os);
            }
        }
    }

    TotalOutput ProjectionLadder::getOutput() const
    {
        TotalOutput cumulativeOutput;
        int storedFood = 0, cumulativeCost = 0;
        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {            
            cumulativeOutput += entries[i].output * entries[i].turns;
            cumulativeCost += entries[i].cost * entries[i].turns;
            storedFood = entries[i].storedFood;  // just take the final entry's value for stored food
        }

        cumulativeOutput[OUTPUT_FOOD] += storedFood;
        cumulativeOutput[OUTPUT_GOLD] -= cumulativeCost;
        return cumulativeOutput;
    }

    TotalOutput ProjectionLadder::getProcessOutput() const
    {
        TotalOutput cumulativeOutput;
        int cumulativeCost = 0;
        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {            
            cumulativeOutput += entries[i].processOutput * entries[i].turns;
        }
        return cumulativeOutput;
    }

    int ProjectionLadder::getAccumulatedProduction() const
    {
        int totalAccumulatedProduction = 0;
        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {
            totalAccumulatedProduction += entries[i].accumulatedProduction;
        }
        return totalAccumulatedProduction;
    }

    int ProjectionLadder::getGPPTotal() const
    {
        int total = 0;
        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {
            for (GreatPersonOutputMap::const_iterator ci(entries[i].gpp.begin()), ciEnd(entries[i].gpp.end()); ci != ciEnd; ++ci)
            {
                total += ci->second * entries[i].turns;
            }
        }
        return total;
    }

    /*TotalOutput ProjectionLadder::getOutputAfter(int turn) const
    {
        bool includeAll = false;
        int currentTurn = 0;
        TotalOutput cumulativeOutput;
        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {   
            currentTurn += entries[i].turns;
            if (entries[i].turns > turn)
            {
                if (includeAll)
                {
                    cumulativeOutput += entries[i].turns * entries[i].output;
                }
                else
                {
                    cumulativeOutput += (currentTurn - turn) * entries[i].output;
                    includeAll = true;
                }
            }            
        }
        return cumulativeOutput;
    }*/

    int ProjectionLadder::getPopChange() const
    {
        return entries.empty() ? 0 : entries.rbegin()->pop - entries.begin()->pop;
    }

    std::pair<int, int> ProjectionLadder::getWorkedTurns(XYCoords coords) const
    {
        int firstTurnWorked = -1;
        int totalTurnsWorked = 0;
        int entryTurnStart = 0;

        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {
            PlotDataListConstIter iter(entries[i].workedPlots.begin()), endIter(entries[i].workedPlots.end());

            for (; iter != endIter; ++iter)
            {
                if (iter->coords == coords)
                {
                    if (firstTurnWorked == -1)
                    {
                        firstTurnWorked = entryTurnStart;
                    }
                    totalTurnsWorked += entries[i].turns;
                }
            }
            entryTurnStart += entries[i].turns;
        }

        return std::make_pair(firstTurnWorked, totalTurnsWorked);
    }

    int ProjectionLadder::getExpectedTurnBuilt(int cost, int itemProductionModifier, int baseModifier) const
    {
        // todo - add itemProductionModifier into calculation
        int currentTurn = 0;
        int remainingProduction = cost * 100;
        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {
            if (remainingProduction <= 0)
            {
                break;
            }

            int actualProductionRate = entries[i].output[OUTPUT_PRODUCTION];
            actualProductionRate = ((actualProductionRate * 100) / baseModifier) * (baseModifier + itemProductionModifier) / 100;

            int thisEntryProduction = entries[i].turns * actualProductionRate;
            if (thisEntryProduction <= remainingProduction)
            {
                currentTurn += entries[i].turns;
                remainingProduction -= thisEntryProduction;
            }
            else
            {
                currentTurn += remainingProduction / actualProductionRate;
                if (remainingProduction % actualProductionRate)
                {
                    ++currentTurn;
                }
                remainingProduction = 0;
            }
        }

        return remainingProduction <= 0 ? currentTurn : -1;
    }

    void ProjectionLadder::write(FDataStreamBase* pStream) const
    {
        size_t buildingsCount = buildings.size();
        pStream->Write(buildingsCount);
        for (size_t i = 0; i < buildingsCount; ++i)
        {
            pStream->Write(buildings[i].first);
            pStream->Write(buildings[i].second);
        }

        writeComplexVector(pStream, units);

        size_t entriesCount = entries.size();
        pStream->Write(entriesCount);
        for (size_t i = 0; i < entriesCount; ++i)
        {
            entries[i].write(pStream);
        }
    }

    void ProjectionLadder::ConstructedUnit::write(FDataStreamBase* pStream) const
    {
        pStream->Write(unitType);
        writeSet(pStream, promotions);
        pStream->Write(turns);
        pStream->Write(level);
        pStream->Write(experience);
    }

    void ProjectionLadder::Entry::write(FDataStreamBase* pStream) const
    {
        pStream->Write(pop);
        pStream->Write(turns);
        pStream->Write(cost);
        pStream->Write(storedFood);
        pStream->Write(accumulatedProduction);
        output.write(pStream);
        processOutput.write(pStream);

        writeMap(pStream, gpp);
    }

    void ProjectionLadder::read(FDataStreamBase* pStream)
    {
        size_t buildingsCount;
        pStream->Read(&buildingsCount);
        for (size_t i = 0; i < buildingsCount; ++i)
        {
            int turn;
            BuildingTypes buildingType;
            pStream->Read(&turn);
            pStream->Read((int*)&buildingType);
            buildings.push_back(std::make_pair(turn, buildingType));
        }

        readComplexVector<ConstructedUnit>(pStream, units);

        size_t entriesCount;
        pStream->Read(&entriesCount);
        for (size_t i = 0; i < entriesCount; ++i)
        {
            Entry entry;
            entry.read(pStream);
            entries.push_back(entry);
        }
    }

    void ProjectionLadder::ConstructedUnit::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&unitType);
        readSet<PromotionTypes, int>(pStream, promotions);
        pStream->Read(&turns);
        pStream->Read(&level);
        pStream->Read(&experience);
    }

    void ProjectionLadder::Entry::read(FDataStreamBase* pStream)
    {
        pStream->Read(&pop);
        pStream->Read(&turns);
        pStream->Read(&cost);
        pStream->Read(&storedFood);
        pStream->Read(&accumulatedProduction);
        output.read(pStream);
        processOutput.read(pStream);

        readMap<UnitTypes, int, int, int>(pStream, gpp);
    }
}