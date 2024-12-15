#include "AltAI.h"

#include "./religion_helper.h"
#include "./happy_helper.h"
#include "./city_data.h"
#include "./civ_log.h"

namespace AltAI
{
    ReligionHelper::ReligionHelper(const CvCity* pCity) : pCity_(pCity), stateReligionType_(NO_RELIGION)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(pCity_->getOwner());
        stateReligionType_ = player.getStateReligion();

        const int numReligions = gGlobals.getNumReligionInfos();
        religionCounts_.resize(numReligions);
        cityReligions_.resize(numReligions);

        for (int religionType = 0; religionType < numReligions; ++religionType)
        {
            cityReligions_[religionType] = pCity->isHasReligion((ReligionTypes)religionType);
            religionCounts_[religionType] = gGlobals.getGame().countReligionLevels((ReligionTypes)religionType);            
        }
    }

    ReligionHelperPtr ReligionHelper::clone() const
    {
        ReligionHelperPtr copy = ReligionHelperPtr(new ReligionHelper(*this));
        return copy;
    }

    void ReligionHelper::setHasReligion(CityData& data, ReligionTypes religionType, bool newValue)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(data.getOwner()))->getStream();
        const bool hadReligion = isHasReligion(religionType);
#endif
        Commerce previousCityReligionCommerce = getCityReligionCommerce();
        cityReligions_[religionType] = newValue ? 1 : 0;
        Commerce newCityReligionCommerce = getCityReligionCommerce();
        data.getCityPlotData().commerce += (newCityReligionCommerce - previousCityReligionCommerce);

#ifdef ALTAI_DEBUG
        os << "\n" << __FUNCTION__ << " religion: " << gGlobals.getReligionInfo(religionType).getType() << " was: " << hadReligion << ", nv: " << newValue
           << " now: " << cityReligions_[religionType] << " prev crc: " << previousCityReligionCommerce << " new crc: " << newCityReligionCommerce;
#endif

        updateReligionHappy_(data);
    }

    void ReligionHelper::setStateReligion(CityData& data, ReligionTypes religionType)
    {
        if (religionType != stateReligionType_)
        {
            Commerce previousCityReligionCommerce = getCityReligionCommerce();
            stateReligionType_ = religionType;  // can be NO_RELIGION
            Commerce newCityReligionCommerce = getCityReligionCommerce();

            updateReligionHappy_(data);
        }
    }

    ReligionTypes ReligionHelper::getStateReligion() const
    {
        return stateReligionType_;
    }

    bool ReligionHelper::hasStateReligion() const
    {
        return stateReligionType_ != NO_RELIGION && cityReligions_[stateReligionType_] > 0;
    }

    int ReligionHelper::getReligionCount(ReligionTypes religionType) const
    {
        return religionCounts_[religionType];
    }

    bool ReligionHelper::isHasReligion(ReligionTypes religionType) const
    {
        return cityReligions_[religionType] > 0;
    }

    Commerce ReligionHelper::getCityReligionCommerce() const
    {
        Commerce cityReligionCommerce;
        // city's religious commerce output
        for (int religionIndex = 0, religionCount = gGlobals.getNumReligionInfos(); religionIndex < religionCount; ++religionIndex)
        {
            if (isHasReligion((ReligionTypes)religionIndex))  // city has this religion present
            {
                // either we are in the state religion or there is no state religion
                // in both cases count the commerce (culture) from this religion
                if ((ReligionTypes)religionIndex == stateReligionType_ || stateReligionType_ == NO_RELIGION)
                {
                    const CvReligionInfo& info = gGlobals.getReligionInfo((ReligionTypes)religionIndex);
                    cityReligionCommerce += info.getStateReligionCommerceArray();
                    if (pCity_->isHolyCity((ReligionTypes)religionIndex))
                    {
                        cityReligionCommerce += info.getHolyCityCommerceArray();
                    }
                }
            }
        }

        return cityReligionCommerce;
    }

    void ReligionHelper::updateReligionHappy_(CityData& data)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(data.getOwner()))->getStream();
#endif
        int stateReligionHappiness = data.getHappyHelper()->getStateReligionHappiness();
        int nonStateReligionHappiness = data.getHappyHelper()->getNonStateReligionHappiness();        

        int totalGoodHappy = 0, totalBadHappy = 0;
        for (int religionType = 0; religionType < gGlobals.getNumReligionInfos(); ++religionType)
        {
            if (isHasReligion((ReligionTypes)religionType))  // religion present in this city
            {
                if (religionType == stateReligionType_)  // state religion
                {
                    if (stateReligionHappiness > 0)
                    {
                        totalGoodHappy += stateReligionHappiness;
                    }
                    else
                    {
                        totalBadHappy += stateReligionHappiness;
                    }
                }
                else
                {
                    if (nonStateReligionHappiness > 0)
                    {
                        totalGoodHappy += nonStateReligionHappiness;
                    }
                    else
                    {
                        totalBadHappy += nonStateReligionHappiness;
                    }
                }
            }
        }
#ifdef ALTAI_DEBUG
        os << " tgh: " << totalGoodHappy << ", tbh: " << totalBadHappy << ", srh: "
           << stateReligionHappiness << ", nsrh: " << nonStateReligionHappiness;
#endif

        if (totalGoodHappy != data.getHappyHelper()->getReligionGoodHappiness())
        {
            data.getHappyHelper()->setReligionGoodHappiness(totalGoodHappy);
        }
        if (totalBadHappy != data.getHappyHelper()->getReligionBadHappiness())
        {
            data.getHappyHelper()->setReligionBadHappiness(totalBadHappy);
        }
    }
}