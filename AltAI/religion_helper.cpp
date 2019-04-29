#include "AltAI.h"

#include "./religion_helper.h"

namespace AltAI
{
    ReligionHelper::ReligionHelper(const CvCity* pCity) : pCity_(pCity)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(pCity_->getOwner());
        religionType_ = player.getStateReligion();

        const int numReligions = gGlobals.getNumReligionInfos();
        religionCounts_.resize(numReligions);
        cityReligions_.resize(numReligions);

        for (int religionType = 0; religionType < numReligions; ++religionType)
        {
            religionCounts_[religionType] = gGlobals.getGame().countReligionLevels((ReligionTypes)religionType);
            cityReligions_[religionType] = pCity->isHasReligion((ReligionTypes)religionType);
        }
    }

    ReligionHelperPtr ReligionHelper::clone() const
    {
        ReligionHelperPtr copy = ReligionHelperPtr(new ReligionHelper(*this));
        return copy;
    }

    ReligionTypes ReligionHelper::getStateReligion() const
    {
        return religionType_;
    }

    bool ReligionHelper::hasStateReligion() const
    {
        return religionType_ != NO_RELIGION && religionCounts_[religionType_] > 0;
    }

    int ReligionHelper::getReligionCount(ReligionTypes religionType) const
    {
        return religionCounts_[religionType];
    }

    bool ReligionHelper::isHasReligion(ReligionTypes religionType) const
    {
        return cityReligions_[religionType] > 0;
    }

    void ReligionHelper::changeReligionCount(ReligionTypes religionType, int change)
    {
        cityReligions_[religionType] += change;
    }
}