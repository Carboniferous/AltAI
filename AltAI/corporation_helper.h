#pragma once 

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class CorporationHelper
    {
    public:
        CorporationHelper(const CvCity* pCity, CityData& data);

    private:
        CityData& data_;
        const CvCity* pCity_;
    };

    typedef boost::shared_ptr<CorporationHelper> CorporationHelperPtr;
}