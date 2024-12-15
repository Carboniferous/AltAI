#include "AltAI.h"

#include "./unit_history.h"

namespace AltAI
{
    UnitHistory::UnitHistory(const IDInfo& unit_, XYCoords coords) : unit(unit_)
    {
        locationHistory.push_front(std::make_pair(gGlobals.getGame().getGameTurn(), coords));
    }

    void UnitHistory::write(FDataStreamBase* pStream) const
    {
        unit.write(pStream);
        pStream->Write(locationHistory.size());
        for (std::list<std::pair<int, XYCoords> >::const_iterator ci(locationHistory.begin()), ciEnd(locationHistory.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            ci->second.write(pStream);
        }
    }

    void UnitHistory::read(FDataStreamBase* pStream)
    {
        unit.read(pStream);
        locationHistory.clear();
        size_t count = 0;
        pStream->Read(&count);
        for (size_t i = 0; i < count; ++i)
        {
            int turn;
            XYCoords coords;
            pStream->Read(&turn);
            coords.read(pStream);
            locationHistory.push_back(std::make_pair(turn, coords));
        }
    }
}
