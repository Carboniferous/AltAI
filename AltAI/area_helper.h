#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class AreaHelper
    {
    public:
        AreaHelper(const CvPlayer& player, const CvArea* pArea);

        int getCleanPowerCount() const;
        void changeCleanPowerCount(bool isAdding);

    private:
        const CvPlayer& player_;
        const CvArea* pArea_;

        int cleanPowerCount_;
    };

    typedef boost::shared_ptr<AreaHelper> AreaHelperPtr;
}