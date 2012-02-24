#pragma once 

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class SpecialistHelper
    {
    public:
        SpecialistHelper(const CvCity* pCity, CityData& data);

        int getMaxSpecialistCount(SpecialistTypes specialistType) const;
        int getFreeSpecialistCount(SpecialistTypes specialistType) const;

        void changeFreeSpecialistCount(SpecialistTypes specialistType, int change);

        int getTotalFreeSpecialistSlotCount() const;
        int getFreeSpecialistCountPerImprovement(ImprovementTypes improvementType) const;

        void changePlayerFreeSpecialistSlotCount(int change);
        void changeImprovementFreeSpecialistSlotCount(int change);
        void changeFreeSpecialistCountPerImprovement(ImprovementTypes improvementType, int change);

    private:
        CityData& data_;
        std::vector<int> freeSpecialistCounts_;
        std::vector<int> maxSpecialistCounts_;
        std::vector<int> improvementFreeSpecialists_;

        int cityFreeSpecSlotCount_, areaFreeSpecSlotCount_, playerFreeSpecSlotCount_, improvementFreeSpecSlotCount_;
    };

    typedef boost::shared_ptr<SpecialistHelper> SpecialistHelperPtr;
}