#pragma once

#ifndef BOOST_VARIANT_VARIANT_HPP
    #ifndef CVGAMECOREDLL_EXPORTS
        #include "boost/variant.hpp"
    #endif
#endif

#include <iostream>
#ifndef CVGAMECOREDLL_EXPORTS
    #include <fstream>
#endif
#include <sstream>
#include <map>
#include <vector>
#include <set>
#include <string>
#include <stack>
#include <queue>

#include "../CvGameCoreDLL/CvGameCoreDLL.h"
#include "../CvGameCoreDLL/CvStructs.h"
#include "../CvGameCoreDLL/CvMap.h"
#include "../CvGameCoreDLL/CvPlayer.h"
#include "../CvGameCoreDLL/CvPlotGroup.h"

#include "boost/shared_ptr.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/noncopyable.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/array.hpp"
#include "boost/assign/list_of.hpp"

#define NUM_DOMAIN_TYPES 4  // hidden by _USRDLL in CvEnums.h
#define NUM_COMMERCE_TYPES 4
#define NUM_YIELD_TYPES 3

namespace boost
{
    template <typename T, std::size_t N> std::ostream& operator << (std::ostream& os, boost::array<T, N> arr)
    {
        os << "(";
        for (int i = 0; i < N; ++i)
        {
            os << (i > 0 ? ", " : "") << arr[i];
        }
        return os << ")";
    }
}

namespace AltAI
{
    enum OutputTypes
    {
        NO_OUTPUT = -1,
        OUTPUT_FOOD,
        OUTPUT_PRODUCTION,
        OUTPUT_GOLD,
        OUTPUT_RESEARCH,
        OUTPUT_CULTURE,
        OUTPUT_ESPIONAGE,
        NUM_OUTPUT_TYPES
    };

    enum TechSources
    {
        NOT_KNOWN = -1,
        RESEARCH_TECH,
        INITIAL_TECH,
        TEAM_TECH,
        SHARE_TECH,
        TRADE_TECH,
        GOODY_TECH,
        STOLEN_TECH,
        FREE_TECH,
        CHEAT_TECH
    };

    template <int N, typename T = int>
        struct Output
    {
        static const int numTypes = N;
        typedef boost::array<T, N> ArrayT;
        ArrayT data;

        Output()
        {
            data.assign(0);
        }

        explicit Output(const T* data_)
        {
            for (int i = 0; i < N; ++i)
            {
                data.elems[i] = data_[i];
            }
        }

        template <typename U>
            Output(const U* data_)
        {
            for (int i = 0; i < N; ++i)
            {
                data.elems[i] = data_[i];
            }
        }

        const Output operator += (const Output& other)
        {
            for (size_t i = 0; i < N; ++i)
            {
                data[i] += other.data[i];
            }
            return *this;
        }

        const Output operator -= (const Output& other)
        {
            for (size_t i = 0; i < N; ++i)
            {
                data[i] -= other.data[i];
            }
            return *this;
        }

        Output operator - () const
        {
            Output output;
            for (size_t i = 0; i < N; ++i)
            {
                output.data[i] = -data[i];
            }
            return output;
        }

        const Output operator *= (int factor)
        {
            for (size_t i = 0; i < N; ++i)
            {
                data[i] *= factor;
            }
            return *this;
        }

        const Output operator /= (int factor)
        {
            for (size_t i = 0; i < N; ++i)
            {
                data[i] *= 100;
                data[i] /= factor;
                data[i] /= 100;
            }
            return *this;
        }

        T& operator[] (size_t i)
        {
            return data[i];
        }

        const T& operator[] (size_t i) const
        {
            return data[i];
        }

        bool isNone() const
        {
            return Output<N, T>() == *this;
        }

        // save/load functions
        void write(FDataStreamBase* pStream) const
        {
            for (size_t i = 0; i < N; ++i)
            {
                pStream->Write(data[i]);
            }
        }

        void read(FDataStreamBase* pStream)
        {
            for (size_t i = 0; i < N; ++i)
            {
                pStream->Read(&data[i]);
            }
        }
    };

    template <int N, typename T> 
        std::istream& operator >> (std::istream& is, Output<N, T>& output)
    {
        for (size_t i = 0; i < N; ++i)
        {
            is >> output.data[i];
        }
        return is;
    }

    template <int N, typename T> 
        std::ostream& operator << (std::ostream& os, const Output<N, T>& output)
    {
        os << "(";
        for (size_t i = 0; i < N; ++i)
        {
            os << (i > 0 ? ", " : "") << output[i];
        }
        return os << ")";
    }

    template <int N, typename T> 
        inline Output<N, T> operator + (const Output<N, T>& output1, const Output<N, T>& output2)
    {
        Output<N, T> output3(output1);
        return output3 += output2;
    }

    template <int N, typename T> 
        inline Output<N, T> operator - (const Output<N, T>& output1, const Output<N, T>& output2)
    {
        Output<N, T> output3(output1);
        return output3 -= output2;
    }

    template <int N, typename T> 
        inline Output<N, T> operator * (const Output<N, T>& output1, int factor)
    {
        Output<N, T> output2(output1);
        return output2 *= factor;
    }

    template <int N, typename T> 
        inline Output<N, T> operator * (int factor, const Output<N, T>& output1)
    {
        Output<N, T> output2(output1);
        return output2 *= factor;
    }

    template <int N, typename T> 
        inline Output<N, T> operator / (const Output<N, T>& output1, int factor)
    {
        Output<N, T> output2(output1);
        return output2 /= factor;
    }

    template <int N, typename T> 
        inline bool operator == (const Output<N, T>& output1, const Output<N, T>& output2)
    {
        bool equal = true;
        for (size_t i = 0; i < N; ++i)
        {
            equal = output1.data[i] == output2.data[i];
            if (!equal) break;
        }
        return equal;
    }

    template <int N, typename T> 
        inline bool operator != (const Output<N, T>& output1, const Output<N, T>& output2)
    {
        return !(output1 == output2);
    }

    template <int N, typename T>  // calc first output as %-age of the second
        inline Output<N, T> asPercentageOf(const Output<N, T>& output1, const Output<N, T>& output2)
    {
        Output<N, T> delta;
        for (size_t i = 0; i < N; ++i)
        {
            delta[i] = output2[i] == 0 ? 0 : (output1[i] * 100) / output2[i];
        }
        return delta;
    }

    template <typename T> 
        inline int asPercentageOf(const T output1, const T output2)
    {
        return output2 == 0 ? 0 : output1 * 100 / output2;
    }

    typedef Output<NUM_YIELD_TYPES> PlotYield;
    typedef Output<NUM_YIELD_TYPES> YieldModifier;
    typedef Output<NUM_COMMERCE_TYPES> Commerce;
    typedef Output<NUM_COMMERCE_TYPES> CommerceModifier;

    typedef Output<NUM_YIELD_TYPES + NUM_COMMERCE_TYPES - 1> TotalOutput;

    PlotYield makeYield(int f, int h, int c);
    Commerce makeCommerce(int g, int r, int c, int e);
    TotalOutput makeOutput(int f, int h, int g, int r, int c, int e);

    TotalOutput makeOutput(PlotYield plotYield, YieldModifier yieldModifier, CommerceModifier commerceModifier, CommerceModifier commercePercent, int scaleFactor = 100);
    TotalOutput makeOutput(PlotYield plotYield, Commerce commerce, YieldModifier yieldModifier, CommerceModifier commerceModifier, CommerceModifier commercePercent, int scaleFactor = 100);

    template <int N, typename T>
        boost::array<T, N> mergeMax(boost::array<T, N> a1, boost::array<T, N> a2)
    {
        boost::array<T, N> merged;

        for (size_t i = 0; i < N; ++i)
        {
            merged[i] = std::max<T>(a1[i], a2[i]);
        }
        return merged;
    }

    // a way to generate priority types for Output types which are not necessarily based on a single enum type
    template <typename T>
        struct OutputPriorities
    {
        typedef typename T::ArrayT OutputPriority;
    };

    typedef OutputPriorities<PlotYield>::OutputPriority YieldPriority;
    typedef OutputPriorities<Commerce>::OutputPriority CommercePriority;
    typedef OutputPriorities<TotalOutput>::OutputPriority TotalOutputPriority;

    TotalOutputPriority makeTotalOutputPriority(YieldPriority yieldPriority, CommercePriority commercePriority);
    TotalOutputPriority makeTotalOutputSinglePriority(OutputTypes outputType);
    TotalOutputPriority makeTotalOutputPriorities(const std::vector<OutputTypes>& outputTypes);

    // a more general priority type, where different types can have equivalent priorities
    template <typename T>
        struct OutputOrdering
    {
        typedef typename T::ArrayT OutputOrder;
    };

    typedef OutputOrdering<PlotYield>::OutputOrder YieldOrdering;
    typedef OutputOrdering<Commerce>::OutputOrder CommerceOrdering;
    typedef OutputOrdering<TotalOutput>::OutputOrder TotalOutputOrdering;

    TotalOutputOrdering makeOutputOrder(const std::vector<std::pair<OutputTypes, int> >& ordering);

    // weightings...
    template <typename T>
        struct OutputWeights
    {
        typedef typename T::ArrayT OutputWeight;
    };

    typedef OutputWeights<PlotYield>::OutputWeight YieldWeights;
    typedef OutputWeights<Commerce>::OutputWeight CommerceWeights;
    typedef OutputWeights<TotalOutput>::OutputWeight TotalOutputWeights;

    template <class T>
        class OutputUtils
    {
    public:
        typedef typename T::ArrayT::value_type ValueType;
        static const int count = T::numTypes;

        template <typename Pred>
            static T compareOutput(const T& output1, const T& output2, Pred p)
        {
            return p(output1, output2) ? output1 : output2;
        }

        static boost::array<ValueType, count> getDefaultWeights()
        {
            boost::array<ValueType, count> defaults;
            defaults.assign(1);
            return defaults;
        }
    };

    template <>
        boost::array<typename PlotYield::ArrayT::value_type, PlotYield::numTypes> OutputUtils<PlotYield>::getDefaultWeights()
    {
        // food, hammers, commerce
        return boost::assign::list_of(4)(2)(1);
    }

    template <>
        boost::array<typename Commerce::ArrayT::value_type, Commerce::numTypes> OutputUtils<Commerce>::getDefaultWeights()
    {
        // gold, research, culture, espionage
        return boost::assign::list_of(2)(3)(1)(1);
    }

    template <>
        boost::array<typename TotalOutput::ArrayT::value_type, TotalOutput::numTypes> OutputUtils<TotalOutput>::getDefaultWeights()
    {
        return boost::assign::list_of(4)(2)(2)(3)(1)(1);
    }

    YieldWeights makeYieldW(int y1, int y2, int y3);
    CommercePriority makeCommerceP(CommerceTypes c1, CommerceTypes c2, CommerceTypes c3, CommerceTypes c4);
    TotalOutputWeights makeOutputW(int w1 = 0, int w2 = 0, int w3 = 0, int w4 = 0, int w5 = 0, int w6 = 0);
    TotalOutputWeights makeOutputW(TotalOutputOrdering outputOrdering);

    YieldPriority makeYieldP(YieldTypes y1, YieldTypes y2 = NO_YIELD, YieldTypes y3 = NO_YIELD);

    typedef OutputUtils<PlotYield> PlotUtils;

    // values a pair of outputs or single output according to specified weights for each output element
    template <typename T>
        struct OutputValueFunctor
    {
        typedef typename T::ArrayT ArrayT;

        explicit OutputValueFunctor(ArrayT outputWeights_) : outputWeights(outputWeights_) {}

        int operator() (const T& output) const
        {
            int result = 0;
            for (size_t i = 0, count = outputWeights.size(); i < count; ++i)
            {
                result += outputWeights[i] * output[i];
            }
            return result;
        }

        bool operator() (const T& output1, const T& output2) const
        {
            int result1 = 0, result2 = 0;
            for (size_t i = 0, count = outputWeights.size(); i < count; ++i)
            {
                result1 += outputWeights[i] * output1[i];
                result2 += outputWeights[i] * output2[i];
            }
            return result1 > result2;
        }

        ArrayT outputWeights;
    };

    typedef OutputValueFunctor<PlotYield> YieldValueFunctor;
    typedef OutputValueFunctor<Commerce> CommerceValueFunctor;
    typedef OutputValueFunctor<TotalOutput> TotalOutputValueFunctor;

    // orders a pair of outputs according to specified output priorities which specify order in which each element should be considered
    template <typename T>
        struct OutputOrder
    {
        typedef typename T::ArrayT::value_type ValueType;
        static const int count = T::numTypes;

        explicit OutputOrder(typename OutputPriorities<T>::OutputPriority priorities_) : priorities(priorities_) {}

        bool operator() (const T& output1, const T& output2) const
        {
            int i = 0;
            while (i < count)
            {
                if (output1[priorities[i]] == output2[priorities[i]])
                {
                    ++i;
                }
                else
                {
                    return output1[priorities[i]] > output2[priorities[i]];
                }
            }
            return false;
        }

        typename OutputPriorities<T>::OutputPriority priorities;
    };

    typedef OutputOrder<PlotYield> PlotYieldOrder;
    typedef OutputOrder<Commerce> CommerceOrder;
    typedef OutputOrder<TotalOutput> TotalOutputOrder;

    template <typename T>
        struct MixedOutputOrderFunctor
    {
        typedef typename T::ArrayT ArrayT;
        static const int count = T::numTypes;

        MixedOutputOrderFunctor(typename OutputPriorities<T>::OutputPriority priorities_, ArrayT outputWeights_)
            : priorities(priorities_), outputWeights(outputWeights_)
        {}

        bool operator() (const T& output1, const T& output2) const
        {
            int i = 0;
            while (i < count && priorities[i] >= 0)
            {
                if (output1[priorities[i]] == output2[priorities[i]])
                {
                    ++i;
                }
                else
                {
                    return output1[priorities[i]] > output2[priorities[i]];
                }
            }

            int result1 = 0, result2 = 0;
            for (size_t i = 0, count = outputWeights.size(); i < count; ++i)
            {
                result1 += outputWeights[i] * output1[i];
                result2 += outputWeights[i] * output2[i];
            }
            return result1 > result2;
        }

        typename OutputPriorities<T>::OutputPriority priorities;
        ArrayT outputWeights;
    };

    typedef MixedOutputOrderFunctor<TotalOutput> MixedTotalOutputOrderFunctor;

    template <typename T>
        struct MixedWeightedOutputOrderFunctor
    {
        typedef typename T::ArrayT ArrayT;
        static const int count = T::numTypes;

        MixedWeightedOutputOrderFunctor(typename OutputPriorities<T>::OutputPriority priorities_, ArrayT outputWeights_)
            : priorities(priorities_), outputWeights(outputWeights_)
        {}

        bool operator() (const T& output1, const T& output2) const
        {
            int i = 0;
            int result1 = 0, result2 = 0;
            while (i < count && priorities[i] >= 0)
            {
                result1 += outputWeights[priorities[i]] * output1[priorities[i]];
                result2 += outputWeights[priorities[i]] * output2[priorities[i]];
                ++i;
            }

            if (result1 != result2)
            {
                return result1 > result2;
            }

            result1 = 0, result2 = 0;
            for (size_t i = 0, count = outputWeights.size(); i < count; ++i)
            {
                result1 += outputWeights[i] * output1[i];
                result2 += outputWeights[i] * output2[i];
            }
            return result1 > result2;
        }

        typename OutputPriorities<T>::OutputPriority priorities;
        ArrayT outputWeights;
    };

    typedef MixedWeightedOutputOrderFunctor<TotalOutput> MixedWeightedTotalOutputOrderFunctor;

    template <typename F>
        struct ProcessValueAdaptorFunctor
    {
        ProcessValueAdaptorFunctor(F valueF_, CommerceModifier modifier_)
            : valueF(valueF_), modifier(modifier_), outputWeights(valueF_.outputWeights)
        {
        }

        bool operator() (const TotalOutput& output1, const TotalOutput& output2) const
        {
            TotalOutput processOutput1(output1), processOutput2(output2);

            Commerce processCommerce1 = (modifier * processOutput1[OUTPUT_PRODUCTION]) / 100;
            Commerce processCommerce2 = (modifier * processOutput2[OUTPUT_PRODUCTION]) / 100;

            for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
            {
                processOutput1[NUM_YIELD_TYPES - 1 + i] += processCommerce1[i];
                processOutput2[NUM_YIELD_TYPES - 1 + i] += processCommerce2[i];
            }

            processOutput1[OUTPUT_PRODUCTION] = 0;
            processOutput2[OUTPUT_PRODUCTION] = 0;

            return valueF(processOutput1, processOutput2);
        }

        F valueF;
        CommerceModifier modifier;
        TotalOutputWeights outputWeights;
    };

    std::ostream& operator << (std::ostream& os, const XYCoords& coords);
    std::ostream& operator << (std::ostream& os, const IDInfo& idInfo);

    typedef bool (CvPlot::*CvPlotFnPtr)(void) const;

    struct CvPlotOrderF
    {
        bool operator() (const CvPlot* pPlot1, const CvPlot* pPlot2) const
        {
            return pPlot1->getCoords() < pPlot2->getCoords();
        }
    };

    struct CvUnitIDInfoOrderF
    {
        bool operator() (const CvUnit* pUnit1, const CvUnit* pUnit2) const
        {
            return pUnit1->getIDInfo() < pUnit2->getIDInfo();
        }
    };

    struct CvSelectionGroupOrderF
    {
        bool operator() (const CvSelectionGroup* pGroup1, const CvSelectionGroup* pGroup2) const
        {
            return pGroup1->getID() < pGroup2->getID();
        }
    };

    typedef std::set<const CvPlot*, CvPlotOrderF> PlotSet;
    typedef std::map<const CvPlot*, std::vector<const CvUnit*>, CvPlotOrderF> PlotUnitsMap;

    struct LessThanZero
    {
        template <typename T>
            bool operator() (T val) const
        {
            return val < 0;
        }
    };

    template <class F> 
        struct YieldAndCommerceFunctor
    {
        template <typename T>
            bool operator() (const T& output) const
        {
            bool result = false;
            for (size_t i = 0, count = output.data.size(); i < count; ++i)
            {
                if (F()(output[i]))
                {
                    result = true;
                    break;
                }
            }
            return result;
        }
    };

    // simple inclusive range type
    template <typename T = int>
        struct Range
    {
        enum BoundType
        {
            LowerBound, UpperBound
        };

        T lower, upper;

        Range(T lower_, T upper_) : lower(lower_), upper(upper_) {}

        Range() : lower(), upper() {}

        explicit Range(T value, BoundType boundType = LowerBound)
        {
            if (boundType == UpperBound)
            {
                lower = std::numeric_limits<T>::min();
                upper = value;
            }
            else
            {
                lower = value;
                upper = std::numeric_limits<T>::max();
            }
        }

        bool contains(T value) const
        {
            return lower <= value && value <= upper;
        }

        bool valueBelow(T value) const
        {
            return value < lower;
        }

        bool valueAbove(T value) const
        {
            return value > upper;
        }
    };

    template <typename T>
        inline int bound(int value, Range<T> range)
    {
        return std::max<T>(range.lower, std::min<T>(value, range.upper));
    }

    template <typename T>
        inline bool operator == (Range<T> first, Range<T> second)
    {
        return first.lower == second.lower && first.upper == second.upper;
    }
    
    template <typename T>
        inline bool operator != (Range<T> first, Range<T> second)
    {
        return !(first == second);
    }

    template <typename T>
        inline std::ostream& operator << (std::ostream& os, Range<T> range)
    {
        return os << " (" << range.lower << ", " << range.upper << ")";
    }

    inline void applyPercentModifier(int& value, int modifier)
    {
        value = (value * modifier) / 100;
    }

    template <typename T> bool isEmpty(const T& value)
    {
        return value == T();
    }

    template <typename T> bool isStrictlyGreater(const T& t1, const T& t2)
    {
        bool foundGreaterThanItem = false;
        for (int i = 0, count = t1.data.size(); i < count; ++i)
        {
            if (t1[i] > t2[i])
            {
                foundGreaterThanItem = true;
            }
            else if (t1[i] < t2[i])
            {
                return false;
            }
        }
        return foundGreaterThanItem;
    }

    std::string narrow(const std::wstring& wstr);

    struct ShowPos : boost::noncopyable
    {
        explicit ShowPos(std::ostream& os) : os_(os) { os_ << std::showpos; }
        ~ShowPos() { os_ << std::noshowpos; }
        std::ostream& os_;
    };

    enum BuildQueueTypes
    {
        NoItem = -1, BuildingItem, UnitItem, ProjectItem, ProcessItem
    };
}