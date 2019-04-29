#pragma once

#include "./utils.h"

class CvCity;

namespace AltAI
{
    class ReligionHelper;
    typedef boost::shared_ptr<ReligionHelper> ReligionHelperPtr;

    class CityData;

    // simulates logic for religious buildings including their commerce effects
    class ReligionHelper
    {
    public:
        explicit ReligionHelper(const CvCity* pCity);
        ReligionHelperPtr clone() const;

        void changeReligionCount(ReligionTypes religionType, int change = 1);

        bool isHasReligion(ReligionTypes religionType) const;
        int getReligionCount(ReligionTypes religionType) const;
        ReligionTypes getStateReligion() const;
        bool hasStateReligion() const;

    private:
        const CvCity* pCity_;
        Commerce existingStateReligionCommerce_, extraStateReligionCommerce_;
        ReligionTypes religionType_;

        std::vector<int> religionCounts_;
        std::vector<int> cityReligions_;
    };
}