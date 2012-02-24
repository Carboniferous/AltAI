#include "./trade_route_helper.h"
#include "./city_data.h"
#include "./iters.h"
#include "./city_log.h"

namespace AltAI
{
    // does not take into account flag: "IGNORE_PLOT_GROUP_FOR_TRADE_ROUTES" (false for unmodified game)
    TradeRouteHelper::TradeRouteHelper(const CvCity* pCity, CityData& data) : pCity_(pCity), data_(data), isDirty_(true)
    {
        const CvPlayerAI& player = CvPlayerAI::getPlayer(pCity_->getOwner());

        OUR_POPULATION_TRADE_MODIFIER_OFFSET_ = gGlobals.getDefineINT("OUR_POPULATION_TRADE_MODIFIER_OFFSET");
        OUR_POPULATION_TRADE_MODIFIER_ = gGlobals.getDefineINT("OUR_POPULATION_TRADE_MODIFIER");

        TRADE_PROFIT_PERCENT_ = gGlobals.getDefineINT("TRADE_PROFIT_PERCENT");
        THEIR_POPULATION_TRADE_PERCENT_ = gGlobals.getDefineINT("THEIR_POPULATION_TRADE_PERCENT");
        CAPITAL_TRADE_MODIFIER_ = gGlobals.getDefineINT("CAPITAL_TRADE_MODIFIER");
        OVERSEAS_TRADE_MODIFIER_ = gGlobals.getDefineINT("OVERSEAS_TRADE_MODIFIER");

        population_ = pCity_->getPopulation();
        tradeRouteModifier_ = pCity_->getTradeRouteModifier();

        tradeProfitPercent_ = gGlobals.getWorldInfo(gGlobals.getMap().getWorldSize()).getTradeProfitPercent();
        foreignTradeRouteModifier_ = pCity_->getForeignTradeRouteModifier();

        for (int i = 0; i < NUM_YIELD_TYPES; ++i)
        {
            tradeYieldModifier_[i] = player.getTradeYieldModifier((YieldTypes)i);
        }

        isConnectedToCapital_ = pCity_->isConnectedToCapital();

        allowForeignTradeRoutes_ = !player.isNoForeignTrade();

        numTradeRoutes_ = pCity->getTradeRoutes();

        // track original cities as we can't reset their trade routes for simulation purposes
        for (int i = 0; i < numTradeRoutes_; ++i)
        {
            origCities_.push_back(pCity->getTradeCity(i));
        }

        updateTradeRoutes();
    }

    void TradeRouteHelper::updateTradeRoutes()
    {
        tradeCities_.clear();
        tradeCities_.resize(numTradeRoutes_);
        std::vector<int> bestValueRoutes(numTradeRoutes_);

        PlayerTypes playerType = pCity_->getOwner();
        TeamTypes teamType = pCity_->getTeam();
        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType);

        PlayerIDIter otherPlayerIter;
        PlayerTypes otherPlayerType = NO_PLAYER;

        while ((otherPlayerType = otherPlayerIter()) != NO_PLAYER)
        {
            if (canHaveTradeRoutesWith_(otherPlayerType))
            {
                const CvPlayerAI& otherPlayer = CvPlayerAI::getPlayer(otherPlayerType);
                CityIter cityIter(otherPlayer);

                while (CvCity* pOtherCity = cityIter())
                {
                    if (pOtherCity != pCity_)
                    {
                        if (std::find(origCities_.begin(), origCities_.end(), pOtherCity) != origCities_.end() || !pOtherCity->isTradeRoute(playerType) || teamType == otherPlayer.getTeam())
                        {
                            if (pOtherCity->plotGroup(playerType) == pCity_->plotGroup(playerType))
                            {
                                int routeValue = calculateTradeProfit_(pOtherCity);

                                for (int i = 0; i < numTradeRoutes_; ++i)
                                {
                                    if (routeValue > bestValueRoutes[i])
                                    {
	                                    for (int j = numTradeRoutes_ - 1; j > i; --j)
	                                    {
		                                    bestValueRoutes[j] = bestValueRoutes[j - 1];
		                                    tradeCities_[j] = tradeCities_[j - 1];
	                                    }

	                                    bestValueRoutes[i] = routeValue;
	                                    tradeCities_[i] = pOtherCity->getIDInfo();
	                                    break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        updateTradeYield_();
        isDirty_ = false;

#ifdef ALTAI_DEBUG
        logTradeRoutes_();
#endif
    }

    PlotYield TradeRouteHelper::getTradeYield() const
    {
        return totalTradeYield_;
    }

    int TradeRouteHelper::getNumRoutes() const
    {
        return numTradeRoutes_;
    }

    void TradeRouteHelper::updateTradeYield_()
    {
        int totalTradeProfit = 0;
        for (int i = 0; i < numTradeRoutes_; ++i)
        {
            CvCity* pTradeCity = getCity(tradeCities_[i]);
            if (pTradeCity)
            {
                totalTradeProfit += calculateTradeProfit_(pTradeCity);
            }
        }

	    for (int i = 0; i < NUM_YIELD_TYPES; ++i)
	    {
            totalTradeYield_[i] = totalTradeProfit * tradeYieldModifier_[i] / 100;
        }
    }

    int TradeRouteHelper::calculateTradeProfit_(const CvCity* pTradeCity) const
    {
	    return (getBaseTradeProfit_(pTradeCity) * totalTradeModifier_(pTradeCity)) / 10000;
    }

    int TradeRouteHelper::getBaseTradeProfit_(const CvCity* pTradeCity) const
    {
    	int profit = std::min<int>(pTradeCity->getPopulation() * THEIR_POPULATION_TRADE_PERCENT_, 
            plotDistance(pCity_->getX(), pCity_->getY(), pTradeCity->getX(), pTradeCity->getY()) * tradeProfitPercent_);

	    profit *= TRADE_PROFIT_PERCENT_;
	    profit /= 100;
        profit = std::max<int>(100, profit);

	    return profit;
    }

    int TradeRouteHelper::totalTradeModifier_(const CvCity* pTradeCity) const
    {
        int modifier = 100;
    	modifier += tradeRouteModifier_;
    	modifier += getPopulationTradeModifier_();

    	if (isConnectedToCapital_)
	    {
		    modifier += CAPITAL_TRADE_MODIFIER_;
	    }

	    if (pTradeCity && pCity_->area() != pTradeCity->area())
	    {
			modifier += OVERSEAS_TRADE_MODIFIER_;
		}

		if (pCity_->getTeam() != pTradeCity->getTeam())
		{
			modifier += foreignTradeRouteModifier_;

			modifier += pCity_->getPeaceTradeModifier(pTradeCity->getTeam());
		}

    	return modifier;
    }

    int TradeRouteHelper::getPopulationTradeModifier_() const
    {
        return std::max<int>(0, population_ + OUR_POPULATION_TRADE_MODIFIER_OFFSET_ * OUR_POPULATION_TRADE_MODIFIER_);
    }

    bool TradeRouteHelper::canHaveTradeRoutesWith_(PlayerTypes otherPlayerType) const
    {
        const CvPlayer& ourPlayer = CvPlayerAI::getPlayer(pCity_->getOwner());
        TeamTypes ourTeamType = ourPlayer.getTeam();
        const CvTeam& ourTeam = CvTeamAI::getTeam(ourTeamType);

        const CvPlayer& otherPlayer = CvPlayerAI::getPlayer(otherPlayerType);
        TeamTypes otherTeamType = otherPlayer.getTeam();
        const CvTeam& otherTeam = CvTeamAI::getTeam(otherTeamType);

	    if (!otherPlayer.isAlive())
	    {
		    return false;
	    }

	    if (ourTeamType == otherTeamType)
	    {
		    return true;
	    }

	    if (ourTeam.isFreeTrade(otherTeamType))  // open borders or free trade passed
	    {
		    if (ourTeam.isVassal(otherTeamType) || otherTeam.isVassal(ourTeamType) || (allowForeignTradeRoutes_ && !otherPlayer.isNoForeignTrade()))
		    {
			    return true;
		    }
	    }

	    return false;
    }

    void TradeRouteHelper::setPopulation(int population)
    {
        population_ = population;
        isDirty_ = true;
    }

    void TradeRouteHelper::changeNumRoutes(int change)
    {
        numTradeRoutes_ = std::max<int>(0, numTradeRoutes_ + change);
        isDirty_ = true;
    }

    void TradeRouteHelper::changeTradeRouteModifier(int change)
    {
        tradeRouteModifier_ += change;
        isDirty_ = true;
    }

    void TradeRouteHelper::changeForeignTradeRouteModifier(int change)
    {
        foreignTradeRouteModifier_ += change;
        isDirty_ = true;
    }

    void TradeRouteHelper::setAllowForeignTradeRoutes(bool value)
    {
        allowForeignTradeRoutes_ = value;
        isDirty_ = true;
    }

    bool TradeRouteHelper::couldHaveForeignRoutes() const
    {
        const CvPlayer& ourPlayer = CvPlayerAI::getPlayer(pCity_->getOwner());
        TeamTypes ourTeamType = ourPlayer.getTeam();
        const CvTeam& ourTeam = CvTeamAI::getTeam(ourTeamType);

        PlayerIter playerIter;
        bool foundOtherPlayer = false;

        while (const CvPlayerAI* otherPlayer = playerIter())
        {
            TeamTypes otherTeamType = otherPlayer->getTeam();
            const CvTeam& otherTeam = CvTeamAI::getTeam(otherTeamType);

    	    if (!otherPlayer->isAlive())
	        {
		        continue;
	        }

	        if (ourTeamType == otherTeamType)
	        {
		        foundOtherPlayer = true;
                break;
	        }

	        if (ourTeam.isFreeTrade(otherTeamType))  // open borders or free trade passed
	        {
		        if (ourTeam.isVassal(otherTeamType) || otherTeam.isVassal(ourTeamType) || (allowForeignTradeRoutes_ && !otherPlayer->isNoForeignTrade()))
		        {
			        foundOtherPlayer = true;
                    break;
		        }
	        }
        }

	    return foundOtherPlayer;
    }

    bool TradeRouteHelper::needsRecalc() const
    {
        return isDirty_;
    }

    void TradeRouteHelper::logTradeRoutes_() const
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
        std::ostream& os = pCityLog->getStream();
        os << "\nNumber of route(s) = " << numTradeRoutes_ << " ";

        for (int i = 0, count = tradeCities_.size(); i < count; ++i)
        {
            const CvCity* pTradeCity = ::getCity(tradeCities_[i]);
            if (pTradeCity)
            {
                if (i > 0)
                {
                    os << ", ";
                }
                os << narrow(pTradeCity->getName()) << " profit = " << calculateTradeProfit_(pTradeCity);
            }
        }

        os << " trade route yield modifier = " << tradeYieldModifier_ << " total trade yield = " << totalTradeYield_ << "\n";
#endif
    }
}