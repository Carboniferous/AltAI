#pragma once

#include "./utils.h"

namespace AltAI
{
    class UnitInfo;
    class Player;
	class City;
    class CityData;

    typedef std::set<PromotionTypes> Promotions;

    boost::shared_ptr<UnitInfo> makeUnitInfo(UnitTypes unitType, PlayerTypes playerType);

    void streamUnitInfo(std::ostream& os, const boost::shared_ptr<UnitInfo>& pUnitInfo);

    std::vector<TechTypes> getRequiredTechs(const boost::shared_ptr<UnitInfo>& pUnitInfo);

    void updateRequestData(CityData& data, SpecialistTypes specialistType);

    boost::tuple<bool, int, std::set<PromotionTypes> > canGainPromotion(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo, PromotionTypes promotionType, 
        const std::set<PromotionTypes>& existingPromotions);

    bool couldConstructUnit(const Player& player, int lookaheadDepth, const boost::shared_ptr<UnitInfo>& pUnitInfo, bool ignoreRequiredResources, bool ignoreRequiredBuildings);

    bool couldConstructUnit(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<UnitInfo>& pUnitInfo, bool ignoreRequiredResources, bool ignoreRequiredBuildings);

    bool couldEverConstructUnit(const Player& player, const City& city, const boost::shared_ptr<UnitInfo>& pUnitInfo, int lookAheadDepth);

    bool isUnitObsolete(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo);

    Promotions getFreePromotions(const boost::shared_ptr<UnitInfo>& pUnitInfo);

    std::vector<SpecialistTypes> getCityJoinSpecs(const boost::shared_ptr<UnitInfo>& pUnitInfo);

    std::vector<BuildingTypes> getUnitSpecialBuildings(const boost::shared_ptr<UnitInfo>& pUnitInfo);
}