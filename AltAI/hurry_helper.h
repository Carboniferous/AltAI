#pragma once

#include "./utils.h"

class CvCity;

namespace AltAI
{
    class CityData;

    class HurryHelper;
    typedef boost::shared_ptr<HurryHelper> HurryHelperPtr;

    struct HurryData
    {
        HurryData() : hurryType(NO_HURRY), hurryPopulation(0), hurryGold(0), extraProduction(0) {}
        explicit HurryData(HurryTypes hurryType_) : hurryType(hurryType_), hurryPopulation(0), hurryGold(0), extraProduction(0) {}
        HurryTypes hurryType;
        int hurryPopulation, hurryGold;
        int extraProduction;
    };

    std::ostream& operator << (std::ostream& os, const HurryData& hurryData);

    // simulates logic in CvCity for hurry anger, but for simulating whiping and rush-buying, changing anger modifier and hurry cost modifier
    class HurryHelper
    {
    public:
        explicit HurryHelper(const CvCity* pCity);
        HurryHelperPtr clone() const;

        void changeModifier(int change)
        {
            hurryAngryModifier_ += change;
            updateFlatHurryAngerLength_();
        }

        void changeCostModifier(int change)
        {
            globalHurryCostModifier_ += change;
        }

        void setPopulation(int population)
        {
            population_ = population;
        }

        void updateAngryTimer();

        int getHurryUnhappiness() const;
        int getHurryPercentAnger() const;
        int getFlatHurryAngryLength() const;
        int getAngryTimer() const;

        std::pair<bool, HurryData> canHurry(const CityData& data, HurryTypes hurryType, bool ignoreBuildQueue = false) const;

        void advanceTurns(int nTurns);

    private:
        void updateFlatHurryAngerLength_();
        int calcHurryPercentAnger_() const;
        HurryData getHurryCosts_(const CityData& data, HurryTypes hurryType, bool ignoreBuildQueue) const;
        int getHurryCostModifier_(int baseHurryCostModifier, bool isNew) const;

        int globalHurryCostModifier_;

        int hurryAngryTimer_;
        int hurryAngryModifier_;
        int flatHurryAngerLength_;

        int population_;

        std::vector<int> productionPerPopulation_, goldPerProduction_;

        int HURRY_ANGER_DIVISOR_, HURRY_POP_ANGER_, PERCENT_ANGER_DIVISOR_, NEW_HURRY_MODIFIER_;
        int hurryConscriptAngerPercent_;
    };
}