#include "./city_projections_ladder.h"
#include "./save_utils.h"

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
            if (!isEmpty(entries[i].processOutput))
            {
                os << ", process output = " << entries[i].processOutput;
            }
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

    void ProjectionLadder::write(FDataStreamBase* pStream) const
    {
        size_t buildingsCount = buildings.size();
        pStream->Write(buildingsCount);
        for (size_t i = 0; i < buildingsCount; ++i)
        {
            pStream->Write(buildings[i].first);
            pStream->Write(buildings[i].second);
        }

        size_t unitsCount = units.size();
        pStream->Write(unitsCount);
        for (size_t i = 0; i < unitsCount; ++i)
        {
            pStream->Write(units[i].first);
            pStream->Write(units[i].second);
        }

        size_t entriesCount = entries.size();
        pStream->Write(entriesCount);
        for (size_t i = 0; i < entriesCount; ++i)
        {
            entries[i].write(pStream);
        }
    }

    void ProjectionLadder::Entry::write(FDataStreamBase* pStream) const
    {
        pStream->Write(pop);
        pStream->Write(turns);
        pStream->Write(cost);
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

        size_t unitsCount;
        pStream->Read(&unitsCount);
        for (size_t i = 0; i < unitsCount; ++i)
        {
            int turn;
            UnitTypes unitType;
            pStream->Read(&turn);
            pStream->Read((int*)&unitType);
            units.push_back(std::make_pair(turn, unitType));
        }

        size_t entriesCount;
        pStream->Read(&entriesCount);
        for (size_t i = 0; i < entriesCount; ++i)
        {
            Entry entry;
            entry.read(pStream);
            entries.push_back(entry);
        }
    }

    void ProjectionLadder::Entry::read(FDataStreamBase* pStream)
    {
        pStream->Read(&pop);
        pStream->Read(&turns);
        pStream->Read(&cost);
        output.read(pStream);
        processOutput.read(pStream);

        readMap<UnitTypes, int, int, int>(pStream, gpp);
    }
}