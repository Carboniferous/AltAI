#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;
    class PlayerAnalysis;
    class CivicInfo;

    boost::shared_ptr<CivicInfo> makeCivicInfo(CivicTypes civicType, PlayerTypes playerType);

    void streamCivicInfo(std::ostream& os, const boost::shared_ptr<CivicInfo>& pCivicInfo);

    void updateRequestData(const CvCity* pCity, CityData& data, const boost::shared_ptr<PlayerAnalysis>& pPlayerAnalysis, CivicTypes newCivic);

    bool civicHasEconomicImpact(const CityData& data, const boost::shared_ptr<CivicInfo>& pCivicInfo);

    bool civicHasPotentialEconomicImpact(PlayerTypes playerType, const boost::shared_ptr<CivicInfo>& pCivicInfo);
    bool civicHasPotentialMilitaryImpact(PlayerTypes playerType, const boost::shared_ptr<CivicInfo>& pCivicInfo);

    void updateSpecialBuildingNotRequiredCount(std::vector<int>& specialBuildingNotRequiredCounts, const boost::shared_ptr<CivicInfo>& pCivicInfo, bool isAdding);
}