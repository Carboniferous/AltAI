#include "AltAI.h"

#include "./hurry_helper.h"
#include "./modifiers_helper.h"
#include "./city_data.h"

namespace AltAI
{
    HurryHelper::HurryHelper(const CvCity* pCity)
    {
        HURRY_ANGER_DIVISOR_ = gGlobals.getDefineINT("HURRY_ANGER_DIVISOR");  // 10
        PERCENT_ANGER_DIVISOR_ = gGlobals.getPERCENT_ANGER_DIVISOR();  // 1000
        HURRY_POP_ANGER_ = gGlobals.getDefineINT("HURRY_POP_ANGER");  // 1
        NEW_HURRY_MODIFIER_ = gGlobals.getDefineINT("NEW_HURRY_MODIFIER");  // 50

        const CvPlayer& player = CvPlayerAI::getPlayer(pCity->getOwner());
        globalHurryCostModifier_ = player.getHurryModifier();

        // 67, 100, 150, 300
        hurryConscriptAngerPercent_ = gGlobals.getGameSpeedInfo(gGlobals.getGame().getGameSpeedType()).getHurryConscriptAngerPercent();
        
        hurryAngryTimer_ = pCity->getHurryAngerTimer();
        hurryAngryModifier_ = pCity->getHurryAngerModifier();
        population_ = pCity->getPopulation();

        for (int i = 0, count = gGlobals.getNumHurryInfos(); i < count; ++i)
        {
            const CvHurryInfo& hurryInfo = gGlobals.getHurryInfo((HurryTypes)i);
            productionPerPopulation_.push_back(hurryInfo.getProductionPerPopulation());
            goldPerProduction_.push_back(hurryInfo.getGoldPerProduction());
        }

        updateFlatHurryAngerLength_();
    }

    HurryHelperPtr HurryHelper::clone() const
    {
        HurryHelperPtr copy = HurryHelperPtr(new HurryHelper(*this));
        return copy;
    }

    int HurryHelper::getHurryUnhappiness() const
    {
        return (calcHurryPercentAnger_() * population_) / PERCENT_ANGER_DIVISOR_;
    }

    int HurryHelper::getHurryPercentAnger() const
    {
        return calcHurryPercentAnger_();
    }

    int HurryHelper::getFlatHurryAngryLength() const
    {
        return flatHurryAngerLength_;
    }

    int HurryHelper::getAngryTimer() const
    {
        return hurryAngryTimer_;
    }

    void HurryHelper::updateAngryTimer()
    {
        hurryAngryTimer_ += flatHurryAngerLength_;
    }

    void HurryHelper::advanceTurns(int nTurns)
    {
        hurryAngryTimer_ = std::max<int>(0, hurryAngryTimer_ - nTurns);
    }

    void HurryHelper::updateFlatHurryAngerLength_()
    {
        // e.g.
        // max(1, (10 * 100) / 100) * max(0, 100 + 0) / 100 => 10 * 100 / 100 = 10
        flatHurryAngerLength_ = std::max<int>(1, (((HURRY_ANGER_DIVISOR_ * hurryConscriptAngerPercent_) / 100) * std::max<int>(0, 100 + hurryAngryModifier_)) / 100);
    }

    int HurryHelper::calcHurryPercentAnger_() const
    {
        // CvCity: return ((((((getHurryAngerTimer() - 1) / flatHurryAngerLength()) + 1) * GC.getDefineINT("HURRY_POP_ANGER") * GC.getPERCENT_ANGER_DIVISOR()) / std::max(1, getPopulation() + iExtra)) + 1);
        return hurryAngryTimer_ > 0 ? 1 + ((1 + (hurryAngryTimer_ - 1) / flatHurryAngerLength_) * HURRY_POP_ANGER_ * PERCENT_ANGER_DIVISOR_) / std::max<int>(1, population_) : 0;
    }

    std::pair<bool, HurryData> HurryHelper::canHurry(const CityData& data, HurryTypes hurryType, bool ignoreBuildQueue) const
    {
        HurryData hurryData(hurryType);
        bool costsPopulation = productionPerPopulation_[hurryType] != 0, costsGold = goldPerProduction_[hurryType] != 0;

        if ((!costsPopulation && !costsGold) || 
            (!ignoreBuildQueue && (data.getBuildQueue().empty() || data.getBuildQueue().top().first == ProcessItem)))
        {
            return std::make_pair(false, hurryData);
        }
        else
        {
            hurryData = getHurryCosts_(data, hurryType, ignoreBuildQueue);
    
            // even if can't hurry - return how many pop we would need
            return std::make_pair(costsPopulation ? population_ / 2 >= hurryData.hurryPopulation : true, hurryData);
        }
    }

    HurryData HurryHelper::getHurryCosts_(const CityData& data, HurryTypes hurryType, bool ignoreBuildQueue) const
    {
        bool costsPopulation = productionPerPopulation_[hurryType] != 0, costsGold = goldPerProduction_[hurryType] != 0;

        int hurryCostModifierModifier = 0;
        int productionModifier = data.getModifiersHelper()->getTotalYieldModifier(data)[OUTPUT_PRODUCTION];
        if (!ignoreBuildQueue)
        {
            std::pair<BuildQueueTypes, int> buildItem(data.getBuildQueue().top());
            if (buildItem.first == BuildingItem)
            {
                hurryCostModifierModifier = gGlobals.getBuildingInfo((BuildingTypes)buildItem.second).getHurryCostModifier();
                productionModifier += data.getModifiersHelper()->getBuildingProductionModifier(data, (BuildingTypes)buildItem.second);
            }
            else if (buildItem.first == UnitItem)
            {
                hurryCostModifierModifier = gGlobals.getUnitInfo((UnitTypes)buildItem.second).getHurryCostModifier();
                productionModifier += data.getModifiersHelper()-> getUnitProductionModifier((UnitTypes)buildItem.second);
            }
        }

        int hurryCostModifier = getHurryCostModifier_(hurryCostModifierModifier, data.getAccumulatedProduction() == 0);

        int production = (((data.getRequiredProduction() - data.getAccumulatedProduction()) / 100) * hurryCostModifier + 99) / 100;

        if (costsPopulation) // this is only included for whipping, not rush-buying
        {
            int extraProduction = (production * productionModifier) / 100;

            if (extraProduction > 0)
            {
                production = (production * production + (extraProduction - 1)) / extraProduction;
            }
        }
        
        int hurryCost = std::max<int>(0, production);
        HurryData hurryData(hurryType);

        if (costsPopulation)
        {
            hurryData.hurryPopulation = (hurryCost - 1) / productionPerPopulation_[hurryType];
            hurryData.hurryPopulation = std::max<int>(1, 1 + hurryData.hurryPopulation);

            hurryData.extraProduction = 100 * (hurryData.hurryPopulation * productionPerPopulation_[hurryType] * productionModifier) / std::max<int>(1, hurryCostModifier) -
                (data.getRequiredProduction() - data.getAccumulatedProduction());
        }
        
        if (costsGold)
        {
            hurryData.hurryGold = std::max<int>(1, hurryCost * goldPerProduction_[hurryType]);
        }
    
        return hurryData;
    }

    int HurryHelper::getHurryCostModifier_(int baseHurryCostModifier, bool isNew) const
    {
        int hurryCostModifier = std::max<int>(0, 100 + baseHurryCostModifier);

        if (isNew)
        {
            hurryCostModifier *= std::max<int>(0, NEW_HURRY_MODIFIER_ + 100);
            hurryCostModifier /= 100;
        }

        hurryCostModifier *= std::max<int>(0, globalHurryCostModifier_ + 100);
        hurryCostModifier /= 100;

        return hurryCostModifier;
    }

    std::ostream& operator << (std::ostream& os, const HurryData& hurryData)
    {
        if (hurryData.hurryType != NO_HURRY)
        {
            const CvHurryInfo& hurryInfo = gGlobals.getHurryInfo(hurryData.hurryType);
            os << " " << hurryInfo.getType() << " ";
            if (hurryData.hurryPopulation > 0)
            {
                os << "Pop cost = " << hurryData.hurryPopulation << " ";
            }
            if (hurryData.hurryGold > 0)
            {
                os << "Gold cost = " << hurryData.hurryGold << " ";
            }
            if (hurryData.extraProduction != 0)
            {
                os << "Overflow production = " << hurryData.extraProduction << " ";
            }
        }
        else
        {
            os << "No rush ";
        }
        return os;
    }
}