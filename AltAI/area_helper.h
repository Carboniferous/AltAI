#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;
    class AreaHelper;

    typedef boost::shared_ptr<AreaHelper> AreaHelperPtr;

    class AreaHelper
    {
    public:
        AreaHelper(const CvPlayer& player, const CvArea* pArea);
        AreaHelperPtr clone() const;

        int getCleanPowerCount() const;
        void changeCleanPowerCount(bool isAdding);

    private:
        const CvPlayer& player_;
        const CvArea* pArea_;

        int cleanPowerCount_;
    };
}