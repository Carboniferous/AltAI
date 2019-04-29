#include "AltAI.h"

#include "./great_people_tactics.h"
#include "./city.h"
#include "./religion_helper.h"
#include "./specialist_helper.h"

namespace AltAI
{
    namespace
    {
        struct CityGPPData
        {
            explicit CityGPPData(const City& city)
            {
                CityDataPtr pCityData = city.getCityData();
                cityGPPModifier = pCityData->getSpecialistHelper()->getPlayerGPPModifier()
                    + pCityData->getSpecialistHelper()->getCityGPPModifier();
                if (pCityData->getReligionHelper()->hasStateReligion())
                {
                    cityGPPModifier += pCityData->getSpecialistHelper()->getStateReligionGPPModifier();
                }

                for (int specialistType = 0, count = gGlobals.getNumSpecialistInfos(); specialistType < count; ++specialistType)
                {
                    slotsMap[(SpecialistTypes)specialistType] = pCityData->getNumPossibleSpecialists((SpecialistTypes)specialistType);
                }

                // todo - wire through city data as we should be simulating it anyway
                currentGPPProgress = city.getCvCity()->getGreatPeopleProgress();
            }

            int cityGPPModifier;
            std::map<SpecialistTypes, int> slotsMap;
            int currentGPPProgress;
        };
    }

    class GreatPeopleAnalysisImpl
    {
    public:
        explicit GreatPeopleAnalysisImpl(Player& player) : player_(player)
        {
            baseGPPModifier_ = player.getCvPlayer()->getGreatPeopleRateModifier();
            currentGPPThreshold_ = player.getCvPlayer()->greatPeopleThreshold(false);
        }

        void updateCity(const CvCity* pCity, bool remove)
        {
            if (remove)
            {
                cityData_.erase(pCity->getIDInfo());
            }
            else
            {
                std::map<IDInfo, CityGPPData>::iterator iter = cityData_.find(pCity->getIDInfo());
                if (iter == cityData_.end())
                {
                    cityData_.insert(std::make_pair(pCity->getIDInfo(), CityGPPData(player_.getCity(pCity))));
                }
                else
                {
                    iter->second = CityGPPData(player_.getCity(pCity));
                }
            }
        }

    private:
        Player& player_;
        int baseGPPModifier_;
        int currentGPPThreshold_;
        std::map<IDInfo, CityGPPData> cityData_;
    };


    GreatPeopleAnalysis::GreatPeopleAnalysis(Player& player)
    {
        pImpl_ = boost::shared_ptr<GreatPeopleAnalysisImpl>(new GreatPeopleAnalysisImpl(player));
    }

    void GreatPeopleAnalysis::updateCity(const CvCity* pCity, bool remove)
    {
        pImpl_->updateCity(pCity, remove);
    }
}