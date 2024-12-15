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

        void setHasReligion(CityData& data, ReligionTypes religionType, bool newValue);
        void setStateReligion(CityData& data, ReligionTypes religionType);

        bool isHasReligion(ReligionTypes religionType) const;
        int getReligionCount(ReligionTypes religionType) const;
        ReligionTypes getStateReligion() const;
        bool hasStateReligion() const;

        Commerce getCityReligionCommerce() const;

    private:
        void updateReligionHappy_(CityData& data);

        const CvCity* pCity_;
        ReligionTypes stateReligionType_;

        std::vector<int> religionCounts_;
        std::vector<int> cityReligions_;
    };
}