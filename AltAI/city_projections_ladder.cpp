#include "./city_projections_ladder.h"

namespace AltAI
{
    void ProjectionLadder::debug(std::ostream& os) const
    {
        int turn = 0;
        TotalOutput cumulativeOutput, lastOutput;
        int cumulativeCost = 0;

        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {
            os << "\n\tTurns = " << entries[i].turns << ", pop = " << entries[i].pop << ", " << entries[i].output << ", " << entries[i].cost;
        }

        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {         
            turn += entries[i].turns;
            cumulativeOutput += entries[i].output * entries[i].turns;
            cumulativeCost += entries[i].cost * entries[i].turns;

            os << "\n\tPop = " << entries[i].pop << " turn = " << turn << " output = " << cumulativeOutput << " cost = " << cumulativeCost;
            if (i > 0)
            {
                os << ", delta = " << entries[i].output - lastOutput;
                lastOutput = entries[i].output;
            }
        }

        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {
            if (!entries[i].gpp.empty()) os << "\n";
            for (GreatPersonOutputMap::const_iterator ci(entries[i].gpp.begin()), ciEnd(entries[i].gpp.end()); ci != ciEnd; ++ci)
            {
                os << gGlobals.getUnitInfo(ci->first).getType() << " = " << ci->second * entries[i].turns;
            }
        }

        for (size_t i = 0, count = buildings.size(); i < count; ++i)
        {
            os << "\n\tBuilding: " << gGlobals.getBuildingInfo(buildings[i].second).getType() << " built: " << buildings[i].first;
        }

        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            os << "\n\tUnit: " << gGlobals.getUnitInfo(units[i].second).getType() << " built: " << units[i].first;
        }
    }

    TotalOutput ProjectionLadder::getOutput() const
    {
        TotalOutput cumulativeOutput;
        int cumulativeCost = 0;
        for (size_t i = 0, count = entries.size(); i < count; ++i)
        {            
            cumulativeOutput += entries[i].output * entries[i].turns;
            cumulativeCost += entries[i].cost * entries[i].turns;
        }
        cumulativeOutput[OUTPUT_GOLD] -= cumulativeCost;
        return cumulativeOutput;
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

    TotalOutput ProjectionLadder::getOutputAfter(int turn) const
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
    }
}