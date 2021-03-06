#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;
    class TechInfo;
    class Player;

    boost::shared_ptr<TechInfo> makeTechInfo(TechTypes techType, PlayerTypes playerType);
    int calculateTechResearchDepth(TechTypes techType, PlayerTypes playerType);
    int calculateTechResearchCost(TechTypes techType, PlayerTypes playerType);
    std::list<TechTypes> pushTechAndPrereqs(TechTypes techType, const Player& player);

    void streamTechInfo(std::ostream& os, const boost::shared_ptr<TechInfo>& pTechInfo);

    void updateRequestData(const CvCity* pCity, CityData& data, const boost::shared_ptr<TechInfo>& pTechInfo);

    bool techAffectsBuildings(const boost::shared_ptr<TechInfo>& pTechInfo);
    bool techAffectsImprovements(const boost::shared_ptr<TechInfo>& pTechInfo, 
        std::vector<ImprovementTypes>& affectedImprovements, std::vector<FeatureTypes>& removeFeatureTypes);
    bool techIsWorkerTech(const boost::shared_ptr<TechInfo>& pTechInfo);

    std::vector<BonusTypes> getRevealedBonuses(const boost::shared_ptr<TechInfo>& pTechInfo);
    std::vector<BonusTypes> getWorkableBonuses(const boost::shared_ptr<TechInfo>& pTechInfo);
    std::vector<RouteTypes> availableRoutes(const boost::shared_ptr<TechInfo>& pTechInfo);
    
    std::vector<TechTypes> getOrTechs(const boost::shared_ptr<TechInfo>& pTechInfo);

    std::vector<BuildingTypes> getPossibleBuildings(const boost::shared_ptr<TechInfo>& pTechInfo);
    std::vector<BuildingTypes> getObsoletedBuildings(const boost::shared_ptr<TechInfo>& pTechInfo);

    std::vector<ProcessTypes> getPossibleProcesses(const boost::shared_ptr<TechInfo>& pTechInfo);

    std::vector<UnitTypes> getPossibleUnits(const boost::shared_ptr<TechInfo>& pTechInfo);
}