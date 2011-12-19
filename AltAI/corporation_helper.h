#pragma once 

#include "./utils.h"

namespace AltAI
{
    class CorporationHelper
    {
    public:
        explicit CorporationHelper(const CvCity* pCity);

    private:
        const CvCity* pCity_;
    };
}