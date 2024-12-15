#pragma once

#include "./utils.h"

namespace AltAI
{
    struct UnitHistory
    {
        UnitHistory() {}
        UnitHistory(const IDInfo& unit_, XYCoords coords);

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        IDInfo unit;
        std::list<std::pair<int, XYCoords> > locationHistory;
    };
}
