#include "./area_helper.h"

namespace AltAI
{
    AreaHelper::AreaHelper(const CvPlayer& player, const CvArea* pArea)
        : player_(player), pArea_(pArea)
    {        
        cleanPowerCount_ = pArea->getCleanPowerCount(player.getTeam());
    }

    AreaHelperPtr AreaHelper::clone() const
    {
        AreaHelperPtr copy = AreaHelperPtr(new AreaHelper(*this));
        return copy;
    }

    int AreaHelper::getCleanPowerCount() const
    {
        return cleanPowerCount_;
    }

    void AreaHelper::changeCleanPowerCount(bool isAdding)
    {
        cleanPowerCount_ += isAdding ? 1 : -1;
    }
}