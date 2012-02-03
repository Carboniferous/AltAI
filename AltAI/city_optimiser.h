#pragma once

#include "./utils.h"
#include "./city_data.h"

namespace AltAI
{
    class CityOptimiser
    {
    public:
        enum GrowthType
        {
            Not_Set = -1, MajorStarve = 0, MinorStarve, FlatGrowth, MinorGrowth, MajorGrowth 
        };

        enum OptState
        {
            OptStateNotSet = -1, OK = 0, FailedInsufficientFood, FailedExcessFood
        };

        explicit CityOptimiser(const boost::shared_ptr<CityData>& data, std::pair<TotalOutput, TotalOutputWeights> maxOutputs = std::pair<TotalOutput, TotalOutputWeights>());

        OptState optimise(OutputTypes outputType = NO_OUTPUT, GrowthType growthType = Not_Set, bool debug = false);
        OptState optimise(UnitTypes specType, GrowthType growthType = Not_Set, bool debug = false);
        OptState optimise(TotalOutputWeights outputWeights, GrowthType growthType, bool debug = false);

        template <typename P, typename F>
            OptState optimise(TotalOutputPriority outputPriorities, TotalOutputWeights outputWeights, GrowthType growthType, CommerceModifier processModifier, bool debug = false);

        template <typename F>
            OptState optimise(TotalOutputPriority outputPriorities, TotalOutputWeights outputWeights, Range targetYield, bool debug = false);

        template <typename F>
            OptState optimise(TotalOutputPriority outputPriorities, TotalOutputWeights outputWeights, GrowthType growthType, bool debug = false);

        OptState optimiseFoodProduction(UnitTypes unitType, bool debug = false);

        const boost::shared_ptr<CityData>& getOutput() const { return data_; }
        Range getTargetYield() const { return targetYield_; }

        GrowthType getGrowthType() const;
        Range calcTargetYieldSurplus(GrowthType growthType) const;
 
        TotalOutput getMaxOutputs() const;
        TotalOutputWeights getMaxOutputWeights() const;
        int getMaxFood();

        void debug(std::ostream& os, bool printAllPlots = true) const;
        static std::string getGrowthTypeString(GrowthType growthType);

        typedef std::vector<std::pair<PlotData, PlotData> > SwapData;

    private:

        boost::shared_ptr<CityData> data_;
        std::pair<TotalOutput, TotalOutputWeights> maxOutputs_;
        int foodPerPop_;
        Range targetYield_;
        bool isFoodProduction_;
 
        void calibrate_(bool debug = false);
        void setTargetYieldSurplus_(GrowthType growthType);
        void removeSpecialistSlot_(SpecialistTypes specialistType);
        void reclaimSpecialistSlots_();

        template <class ValueAdaptor>
            OptState optimise_(ValueAdaptor valueAdaptor, bool debugSwaps = false);

        template <class ValueAdaptor>
            std::pair<int, SwapData> optimiseOutputs_(ValueAdaptor adaptor);

        template <class ValueAdaptor>
            std::pair<int, SwapData> optimiseExcessFood_(ValueAdaptor adaptor);

        template <class ValueAdaptor>
            std::pair<int, SwapData> juggle_(ValueAdaptor adaptor, const SwapData& swapData, bool debug = false);

        template <class ValueAdaptor>
            void handleRounding_(ValueAdaptor adaptor, bool debug = false);
    };

    struct PlotAssignmentSettings
    {
        PlotAssignmentSettings();
        TotalOutputWeights outputWeights;
        TotalOutputPriority outputPriorities;
        CityOptimiser::GrowthType growthType;
        Range targetFoodYield;
        SpecialistTypes specialistType;
    };

    struct ConstructItem;
    PlotAssignmentSettings makePlotAssignmentSettings(const boost::shared_ptr<CityData>& pCityData, const CvCity* pCity, const ConstructItem& constructItem);

    struct DotMapItem;

    class DotMapOptimiser
    {
    public:
        DotMapOptimiser(DotMapItem& dotMapItem, PlayerTypes playerType);
        DotMapOptimiser(DotMapItem& dotMapItem, YieldWeights weights, YieldWeights ignoreFoodWeights);

        void optimise(std::vector<YieldWeights> ignoreFoodWeights = std::vector<YieldWeights>());
        void optimise(const std::vector<YieldTypes>& yieldTypes, int availablePopulation);
        const DotMapItem& getDotMapItem() const;

    private:
        DotMapItem& dotMapItem_;
        const int foodPerPop_;
        YieldWeights weights_, ignoreFoodWeights_;
        PlayerTypes playerType_;
    };
}