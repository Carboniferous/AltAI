#pragma once

#include "./utils.h"

namespace AltAI
{
    class TradeRouteHelper;
    typedef boost::shared_ptr<TradeRouteHelper> TradeRouteHelperPtr;

    class TradeRouteHelper
    {
    public:
        explicit TradeRouteHelper(const CvCity* pCity);
        TradeRouteHelperPtr clone() const;

        PlotYield getTradeYield() const;
        int getNumRoutes() const;
        void updateTradeRoutes();

        void setPopulation(int population);
        void changeNumRoutes(int change);
        void changeTradeRouteModifier(int change);
        void changeForeignTradeRouteModifier(int change);
        void setAllowForeignTradeRoutes(bool value);

        bool couldHaveForeignRoutes() const;

        bool needsRecalc() const;

    private:
        void updateTradeYield_();

        int totalTradeModifier_(const CvCity* pTradeCity) const;
        int getBaseTradeProfit_(const CvCity* pTradeCity) const;
        int calculateTradeProfit_(const CvCity* pTradeCity) const;
        int getPopulationTradeModifier_() const;

        bool canHaveTradeRoutesWith_(PlayerTypes otherPlayerType) const;

        void logTradeRoutes_() const;

        const CvCity* pCity_;

        int numTradeRoutes_;
        bool isConnectedToCapital_;
        bool allowForeignTradeRoutes_;

        int OUR_POPULATION_TRADE_MODIFIER_OFFSET_, OUR_POPULATION_TRADE_MODIFIER_;
        int TRADE_PROFIT_PERCENT_, THEIR_POPULATION_TRADE_PERCENT_, CAPITAL_TRADE_MODIFIER_, OVERSEAS_TRADE_MODIFIER_;

        int population_;
        int tradeProfitPercent_;
        int tradeRouteModifier_, foreignTradeRouteModifier_;
        YieldModifier tradeYieldModifier_;

        std::vector<IDInfo> tradeCities_;
        std::vector<const CvCity*> origCities_;
        PlotYield totalTradeYield_;

        bool isDirty_;
    };
}