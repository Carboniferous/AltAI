#include "./specialist_helper.h"
#include "./city_data.h"

namespace AltAI
{
    SpecialistHelper::SpecialistHelper(const CvCity* pCity, CityData& data) : data_(data)
    {
        const int specialistCount = gGlobals.getNumSpecialistInfos();
        maxSpecialistCounts_.resize(specialistCount);
        freeSpecialistCounts_.resize(specialistCount);

        for (int i = 0; i < specialistCount; ++i)
        {
            maxSpecialistCounts_[i] = pCity->getMaxSpecialistCount((SpecialistTypes)i);
            freeSpecialistCounts_[i] = pCity->getFreeSpecialistCount((SpecialistTypes)i);
        }

        cityFreeSpecSlotCount_ = pCity->getFreeSpecialist();
        areaFreeSpecSlotCount_ = pCity->area()->getFreeSpecialist(pCity->getOwner());
        playerFreeSpecSlotCount_ = CvPlayerAI::getPlayer(pCity->getOwner()).getFreeSpecialist();
        improvementFreeSpecSlotCount_ = 0;

        improvementFreeSpecialists_.resize(gGlobals.getNumImprovementInfos());
        for (int improvementType = 0; improvementType < gGlobals.getNumImprovementInfos(); ++improvementType)
		{
			improvementFreeSpecialists_[improvementType] = pCity->getImprovementFreeSpecialists((ImprovementTypes)improvementType);
			if (improvementFreeSpecialists_[improvementType] > 0)
			{
				improvementFreeSpecSlotCount_ += improvementFreeSpecialists_[improvementType] * pCity->countNumImprovedPlots((ImprovementTypes)improvementType);
			}
		}
    }

    int SpecialistHelper::getMaxSpecialistCount(SpecialistTypes specialistType) const
    {
        return maxSpecialistCounts_[specialistType];
    }

    int SpecialistHelper::getFreeSpecialistCount(SpecialistTypes specialistType) const
    {
        return freeSpecialistCounts_[specialistType];
    }

    void SpecialistHelper::changeFreeSpecialistCount(SpecialistTypes specialistType, int change)
    {
        freeSpecialistCounts_[specialistType] = std::max<int>(0, freeSpecialistCounts_[specialistType] + change);
    }

    int SpecialistHelper::getTotalFreeSpecialistSlotCount() const
    {
        return cityFreeSpecSlotCount_ + areaFreeSpecSlotCount_ + playerFreeSpecSlotCount_ + improvementFreeSpecSlotCount_;
    }

    int SpecialistHelper::getFreeSpecialistCountPerImprovement(ImprovementTypes improvementType) const
    {
        return improvementFreeSpecialists_[improvementType];
    }

    void SpecialistHelper::changePlayerFreeSpecialistSlotCount(int change)
    {
        playerFreeSpecSlotCount_ = std::max<int>(0, playerFreeSpecSlotCount_ + change);
    }

    void SpecialistHelper::changeFreeSpecialistCountPerImprovement(ImprovementTypes improvementType, int change)
    {
        improvementFreeSpecialists_[improvementType] = std::max<int>(0, improvementFreeSpecialists_[improvementType] + change);
    }

    void SpecialistHelper::changeImprovementFreeSpecialistSlotCount(int change)
    {
        improvementFreeSpecSlotCount_ = std::max<int>(0, improvementFreeSpecSlotCount_ + change);
    }
}
