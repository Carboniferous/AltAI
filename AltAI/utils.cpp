#include "AltAI.h"

#include "./utils.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const XYCoords& coords)
    {
        return os << "(" << coords.iX << ", " << coords.iY << ")";
    }

    YieldWeights makeYieldW(int y1, int y2, int y3)
    {
        YieldWeights w = boost::assign::list_of(y1)(y2)(y3);
        return w;
    }

    YieldPriority makeYieldP(YieldTypes y1, YieldTypes y2, YieldTypes y3)
    {
        YieldPriority p = boost::assign::list_of(y1)(y2)(y3);
        return p;
    }

    CommercePriority makeCommerceP(CommerceTypes c1, CommerceTypes c2, CommerceTypes c3, CommerceTypes c4)
    {
        CommercePriority p = boost::assign::list_of(c1)(c2)(c3)(c4);
        return p;
    }

    PlotYield makeYield(int f, int h, int c)
    {
        int yields[] = {f, h, c};
        return PlotYield(yields);
    }

    Commerce makeCommerce(int g, int r, int c, int e)
    {
        int commerce[] = {g, r, c, e};
        return Commerce(commerce);
    }

    TotalOutput makeOutput(int f, int h, int g, int r, int c, int e)
    {
        int output[] = {f, h, g, r, c, e};
        return TotalOutput(output);
    }

    TotalOutput makeOutput(PlotYield plotYield, YieldModifier yieldModifier, CommerceModifier commerceModifier, CommerceModifier commercePercent, int scaleFactor)
    {
        // e.g. PlotYield = (2, 2, 3) with YieldModifier(100, 125, 100) (food, hammers, commerce) and CommerceModifier(125, 150, 150, 100) (gold, research, culture, esp)
        // gives TotalOutput = (2 * 100, 2 * 125, 3 * 100 * 125 / 100, 3 * 100 * 150 / 100, 3 * 100 * 150 / 100, 3 * 100 * 100 / 100) =>
        // TotalOutput = (200, 250, 375, 450, 450, 300) (potential output - implies slider at 100% for each type!)
        return makeOutput(plotYield, Commerce(), yieldModifier, commerceModifier, commercePercent, scaleFactor);
    }

    TotalOutput makeOutput(PlotYield plotYield, Commerce commerce, YieldModifier yieldModifier, CommerceModifier commerceModifier, CommerceModifier commercePercent, int scaleFactor)
    {
        static const int outputCommerceScaleFactor = 100;
        TotalOutput totalOutput;

        for (int i = 0; i < NUM_YIELD_TYPES - 1; ++i)
        {
            totalOutput[i] = (plotYield[i] * yieldModifier[i] * scaleFactor) / 100;
        }

        for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
        {
            totalOutput[i + NUM_YIELD_TYPES - 1] = ((((plotYield[NUM_YIELD_TYPES - 1] * yieldModifier[NUM_YIELD_TYPES - 1] * commercePercent[i]) / 100) + commerce[i] * 100) * 
                commerceModifier[i] * scaleFactor) / (outputCommerceScaleFactor * 100);
        }
        return totalOutput;
    }

    TotalOutputPriority makeTotalOutputPriority(YieldPriority yieldPriority, CommercePriority commercePriority)
    {
        TotalOutputPriority totalOutputPriority;

        for (int i = 0; i < NUM_YIELD_TYPES - 1; ++i)
        {
            totalOutputPriority[i] = yieldPriority[i];
        }

        for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
        {
            totalOutputPriority[i + NUM_YIELD_TYPES - 1] = commercePriority[i];
        }

        return totalOutputPriority;
    }

    TotalOutputPriority makeTotalOutputSinglePriority(OutputTypes outputType)
    {
        TotalOutputPriority totalOutputPriority;
        totalOutputPriority.assign(-1);
        totalOutputPriority[0] = outputType;
        return totalOutputPriority;
    }

    TotalOutputPriority makeTotalOutputPriorities(const std::vector<OutputTypes>& outputTypes)
    {
        TotalOutputPriority totalOutputPriority;
        totalOutputPriority.assign(-1);

        for (size_t i = 0, count = outputTypes.size(); i < count; ++i)
        {
            totalOutputPriority[i] = outputTypes[i];
        }
        
        return totalOutputPriority;
    }

    TotalOutputOrdering makeOutputOrder(const std::vector<std::pair<OutputTypes, int> >& ordering)
    {
        TotalOutputOrdering outputOrdering;

        for (size_t i = 0, count = ordering.size(); i < count; ++i)
        {
            outputOrdering[ordering[i].first] = ordering[i].second; 
        }
        return outputOrdering;
    }

    TotalOutputWeights makeOutputW(int w1, int w2, int w3, int w4, int w5, int w6)
    {
        return boost::assign::list_of(w1)(w2)(w3)(w4)(w5)(w6);
    }

    TotalOutputWeights makeOutputW(TotalOutputOrdering outputOrdering)
    {
        TotalOutputWeights weights;
        weights.assign(0);

        // TODO

        return weights;
    }

    std::string narrow(const std::wstring& wstr)
    {
        std::ostringstream oss;
        const std::ctype<char>& facet = std::use_facet<std::ctype<char> >(oss.getloc());
        for (size_t i = 0, count = wstr.size(); i < count; ++i)
        {
            oss << facet.narrow(wstr[i]);
        }
        return oss.str();
    }
}