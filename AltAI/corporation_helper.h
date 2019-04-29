#pragma once 

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class CorporationHelper;
    typedef boost::shared_ptr<CorporationHelper> CorporationHelperPtr;

    class CorporationHelper
    {
    public:
        explicit CorporationHelper(const CvCity* pCity);
        CorporationHelperPtr clone() const;

    private:
        const CvCity* pCity_;
    };
}