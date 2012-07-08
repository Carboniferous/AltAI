#pragma once

#include "./utils.h"

namespace AltAI
{
    class UnitInfo;
    class Player;

    typedef std::set<PromotionTypes> Promotions;

    boost::shared_ptr<UnitInfo> makeUnitInfo(UnitTypes unitType, PlayerTypes playerType);

    void streamUnitInfo(std::ostream& os, const boost::shared_ptr<UnitInfo>& pUnitInfo);

    std::vector<TechTypes> getRequiredTechs(const boost::shared_ptr<UnitInfo>& pUnitInfo);

    boost::tuple<bool, int, std::set<PromotionTypes> > canGainPromotion(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo, PromotionTypes promotionType, 
        const std::set<PromotionTypes>& existingPromotions);

    bool couldConstructUnit(const Player& player, int lookaheadDepth, const boost::shared_ptr<UnitInfo>& pUnitInfo, bool ignoreRequiredResources);

    Promotions getFreePromotions(const boost::shared_ptr<UnitInfo>& pUnitInfo);
}