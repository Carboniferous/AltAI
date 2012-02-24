#pragma once

#include "./utils.h"

class CvCity;

namespace AltAI
{
    class CityData;

    // simulates logic for religious buildings including their commerce effects
    class ReligionHelper
    {
    public:
        ReligionHelper(const CvCity* pCity, CityData& data);

        bool isHasReligion(ReligionTypes religionType) const;
        int getReligionCount(ReligionTypes religionType) const;
        ReligionTypes getStateReligion() const;

    private:
        CityData& data_;
        const CvCity* pCity_;
        Commerce existingStateReligionCommerce_, extraStateReligionCommerce_;
        ReligionTypes religionType_;

        std::vector<int> religionCounts_;
        std::vector<int> cityReligions_;
    };

    typedef boost::shared_ptr<ReligionHelper> ReligionHelperPtr;
}