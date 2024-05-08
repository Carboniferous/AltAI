#pragma once 

#include "./utils.h"

namespace AltAI
{
    class SpecialistHelper;
    typedef boost::shared_ptr<SpecialistHelper> SpecialistHelperPtr;

    class SpecialistHelper
    {
    public:
        SpecialistHelper(const CvCity* pCity);
        SpecialistHelperPtr clone() const;

        int getMaxSpecialistCount(SpecialistTypes specialistType) const;
        int getFreeSpecialistCount(SpecialistTypes specialistType) const;

        void changeFreeSpecialistCount(SpecialistTypes specialistType, int change);

        int getTotalFreeSpecialistSlotCount() const;
        int getFreeSpecialistCountPerImprovement(ImprovementTypes improvementType) const;

        std::vector<SpecialistTypes> getAvailableSpecialistTypes() const;

        int getPlayerGPPModifier() const;
        int getCityGPPModifier() const;
        int getStateReligionGPPModifier() const;

        void changePlayerFreeSpecialistSlotCount(int change);
        void changeImprovementFreeSpecialistSlotCount(int change);
        void changeFreeSpecialistCountPerImprovement(ImprovementTypes improvementType, int change);

        void changePlayerGPPModifier(int change);
        void changeCityGPPModifier(int change);
        void changeStateReligionGPPModifier(int change);

    private:
        std::vector<int> freeSpecialistCounts_;
        std::vector<int> maxSpecialistCounts_;
        std::vector<int> improvementFreeSpecialists_;

        int cityFreeSpecSlotCount_, areaFreeSpecSlotCount_, playerFreeSpecSlotCount_, improvementFreeSpecSlotCount_;
        int playerGPPModifier_, cityGPPModifier_, stateReligionGPPModifier_;
    };
}