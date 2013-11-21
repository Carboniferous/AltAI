#include "AltAI.h"

#include "./corporation_helper.h"
#include "./city_data.h"

namespace AltAI
{
    CorporationHelper::CorporationHelper(const CvCity* pCity) : pCity_(pCity)
    {
    }

    CorporationHelperPtr CorporationHelper::clone() const
    {
        CorporationHelperPtr copy = CorporationHelperPtr(new CorporationHelper(*this));
        return copy;
    }
}
