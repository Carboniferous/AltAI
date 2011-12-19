#pragma once

#include "./utils.h"

class CvCity;

namespace AltAI
{
    struct CityData;

    // simulates logic for religious buildings including their commerce effects
    class ReligionHelper
    {
    public:
        explicit ReligionHelper(const CvCity* pCity);

        bool isHasReligion(ReligionTypes religionType) const;
        int getReligionCount(ReligionTypes religionType) const;
        ReligionTypes getStateReligion() const;

    private:
        const CvCity* pCity_;
        Commerce existingStateReligionCommerce_, extraStateReligionCommerce_;
        ReligionTypes religionType_;

        std::vector<int> religionCounts_;
        std::vector<int> cityReligions_;
    };
}