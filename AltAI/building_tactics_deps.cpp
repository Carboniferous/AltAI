#include "AltAI.h"

#include "./building_tactics_deps.h"
#include "./game.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./city_data.h"
#include "./civ_helper.h"
#include "./building_helper.h"
#include "./religion_helper.h"
#include "./bonus_helper.h"
#include "./resource_info_visitors.h"
#include "./iters.h"
#include "./save_utils.h"
#include "./civ_log.h"

namespace AltAI
{
    ResearchTechDependency::ResearchTechDependency(TechTypes techType) : techType_(techType)
    {
    }

    void ResearchTechDependency::apply(const CityDataPtr& pCityData)
    {
        pCityData->getCivHelper()->addTech(techType_);
        pCityData->recalcOutputs();
    }

    void ResearchTechDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getCivHelper()->removeTech(techType_);
        pCityData->recalcOutputs();
    }

    bool ResearchTechDependency::required(const CvCity* pCity, int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Tech_Dep ? false : !gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCivHelper()->hasTech(techType_);
    }

    bool ResearchTechDependency::required(const Player& player, int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Tech_Dep ? false : !player.getCivHelper()->hasTech(techType_);
    }

    bool ResearchTechDependency::removeable() const
    {
        return true;
    }

    bool ResearchTechDependency::matches(int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Tech_Dep;
    }

    BuildQueueItem ResearchTechDependency::getBuildItem() const
    {
        return std::make_pair(NoItem, -1);
    }

    std::vector<DependencyItem> ResearchTechDependency::getDependencyItems() const
    {
        return std::vector<DependencyItem>(1, std::make_pair(ID, techType_));
    }

    void ResearchTechDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\t dependent on tech: " << gGlobals.getTechInfo(techType_).getType();
#endif
    }

    void ResearchTechDependency::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(techType_);
    }

    void ResearchTechDependency::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&techType_);
    }


    CityBuildingDependency::CityBuildingDependency(BuildingTypes buildingType) : buildingType_(buildingType)
    {
    }

    void CityBuildingDependency::apply(const CityDataPtr& pCityData)
    {
        pCityData->getBuildingsHelper()->changeNumRealBuildings(buildingType_);
        pCityData->recalcOutputs();
    }

    void CityBuildingDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getBuildingsHelper()->changeNumRealBuildings(buildingType_, false);
        pCityData->recalcOutputs();
    }

    bool CityBuildingDependency::required(const CvCity* pCity, int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::City_Buildings_Dep ? false : pCity->getNumBuilding(buildingType_) == 0;
    }

    bool CityBuildingDependency::required(const Player& player, int depTacticFlags) const
    {
        if (depTacticFlags & IDependentTactic::City_Buildings_Dep)
        {
            return false;
        }
        // treat as meaning do we have the building anywhere
        int buildingCount = 0;
        CityIter iter(*player.getCvPlayer());
        while (CvCity* pCity = iter())
        {
            buildingCount += pCity->getNumBuilding(buildingType_);
        }
        return buildingCount == 0;
    }

    bool CityBuildingDependency::removeable() const
    {
        return true;
    }

    bool CityBuildingDependency::matches(int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::City_Buildings_Dep;
    }

    BuildQueueItem CityBuildingDependency::getBuildItem() const
    {
        return std::make_pair(BuildingItem, buildingType_);
    }

    std::vector<DependencyItem> CityBuildingDependency::getDependencyItems() const
    {
        return std::vector<DependencyItem>(1, std::make_pair(ID, buildingType_));
    }

    void CityBuildingDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\t dependent on city building: " << gGlobals.getBuildingInfo(buildingType_).getType();
#endif
    }

    void CityBuildingDependency::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(buildingType_);
    }

    void CityBuildingDependency::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&buildingType_);
    }


    CivBuildingDependency::CivBuildingDependency(BuildingTypes buildingType, int count, BuildingTypes sourceBuildingType)
        : buildingType_(buildingType), sourceBuildingType_(sourceBuildingType), count_(count)
    {        
    }

    void CivBuildingDependency::apply(const CityDataPtr& pCityData)
    {
        // nothing to do - presumes city will also get building through likely CityBuildingDependency (if not, should be done here)
    }

    void CivBuildingDependency::remove(const CityDataPtr& pCityData)
    {
        // nothing to do - presumes city will also get building through likely CityBuildingDependency
    }

    bool CivBuildingDependency::required(const CvCity* pCity, int depTacticFlags) const
    {
        if (depTacticFlags & IDependentTactic::Civ_Buildings_Dep)
        {
            return false;
        }

        const CvPlayerAI& player = CvPlayerAI::getPlayer(pCity->getOwner());

        const BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType_).getBuildingClassType(),
            sourceBuildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(sourceBuildingType_).getBuildingClassType();

        return player.getBuildingClassCount(buildingClassType) < (1 + player.getBuildingClassCountPlusMaking(sourceBuildingClassType)) * count_;
    }

    bool CivBuildingDependency::required(const Player& player, int depTacticFlags) const
    {
        if (depTacticFlags & IDependentTactic::Civ_Buildings_Dep)
        {
            return false;
        }

        const BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType_).getBuildingClassType(),
            sourceBuildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(sourceBuildingType_).getBuildingClassType();

        return player.getCvPlayer()->getBuildingClassCount(buildingClassType) < (1 + player.getCvPlayer()->getBuildingClassCountPlusMaking(sourceBuildingClassType)) * count_;
    }

    bool CivBuildingDependency::removeable() const
    {
        return false;  // todo - possible to remove for limited buildings?
    }

    bool CivBuildingDependency::matches(int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Civ_Buildings_Dep;
    }

    BuildQueueItem CivBuildingDependency::getBuildItem() const
    {
        return std::make_pair(BuildingItem, buildingType_);
    }

    std::vector<DependencyItem> CivBuildingDependency::getDependencyItems() const
    {
        return std::vector<DependencyItem>(1, std::make_pair(ID, buildingType_));
    }

    void CivBuildingDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\t dependent on civ building: " << gGlobals.getBuildingInfo(buildingType_).getType() << ", count = " << count_
           << " source building = " << gGlobals.getBuildingInfo(sourceBuildingType_).getType();
#endif
    }

    void CivBuildingDependency::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(buildingType_);
        pStream->Write(count_);
        pStream->Write(sourceBuildingType_);
    }

    void CivBuildingDependency::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&buildingType_);
        pStream->Read(&count_);
        pStream->Read((int*)&sourceBuildingType_);
    }


    ReligiousDependency::ReligiousDependency(ReligionTypes religionType, UnitTypes unitType) : religionType_(religionType), unitType_(unitType)
    {
    }

    void ReligiousDependency::apply(const CityDataPtr& pCityData)
    {
        pCityData->getReligionHelper()->setHasReligion(*pCityData, religionType_, true);
        pCityData->recalcOutputs();
    }

    void ReligiousDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getReligionHelper()->setHasReligion(*pCityData, religionType_, false);
        pCityData->recalcOutputs();
    }

    bool ReligiousDependency::required(const CvCity* pCity, int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Religion_Dep ? false : !pCity->isHasReligion(religionType_);
    }

    bool ReligiousDependency::required(const Player& player, int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Religion_Dep ? false : player.getCvPlayer()->getHasReligionCount(religionType_) == 0;
    }

    bool ReligiousDependency::removeable() const
    {
        return true;
    }

    bool ReligiousDependency::matches(int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Religion_Dep;
    }

    BuildQueueItem ReligiousDependency::getBuildItem() const
    {
        return std::make_pair(UnitItem, unitType_);
    }

    std::vector<DependencyItem> ReligiousDependency::getDependencyItems() const
    {
        return std::vector<DependencyItem>(1, std::make_pair(ID, religionType_));
    }

    void ReligiousDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\t dependent on religion: " << gGlobals.getReligionInfo(religionType_).getType();
#endif
    }

    void ReligiousDependency::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(religionType_);
        pStream->Write(unitType_);
    }

    void ReligiousDependency::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&religionType_);
        pStream->Read((int*)&unitType_);
    }


    void StateReligionDependency::apply(const CityDataPtr& pCityData)
    {
        ReligionTypes stateReligion = pCityData->getReligionHelper()->getStateReligion();
        if (stateReligion != NO_RELIGION)
        {
            pCityData->getReligionHelper()->setHasReligion(*pCityData, stateReligion, true);
            pCityData->recalcOutputs();
        }
    }

    void StateReligionDependency::remove(const CityDataPtr& pCityData)
    {
        ReligionTypes stateReligion = pCityData->getReligionHelper()->getStateReligion();
        if (stateReligion != NO_RELIGION)
        {
            pCityData->getReligionHelper()->setHasReligion(*pCityData, stateReligion, false);
            pCityData->recalcOutputs();
        }
    }

    bool StateReligionDependency::required(const CvCity* pCity, int depTacticFlags) const
    {
        const CvPlayerAI& player = CvPlayerAI::getPlayer(pCity->getOwner());
        ReligionTypes stateReligion = player.getStateReligion();

        return depTacticFlags & IDependentTactic::Religion_Dep ? false : !(stateReligion != NO_RELIGION && pCity->isHasReligion(stateReligion));
    }

    bool StateReligionDependency::required(const Player& player, int depTacticFlags) const
    {
        ReligionTypes stateReligion = player.getCvPlayer()->getStateReligion();

        // pretty sure you can't have a state religion with no cities actually having that religion, but perhaps if you lose a city? ... best to include count in check.
        return depTacticFlags & IDependentTactic::Religion_Dep ? false : !(stateReligion != NO_RELIGION && player.getCvPlayer()->getHasReligionCount(stateReligion) > 0);
    }

    bool StateReligionDependency::removeable() const
    {
        return true;
    }

    bool StateReligionDependency::matches(int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Religion_Dep;
    }

    BuildQueueItem StateReligionDependency::getBuildItem() const
    {
        return std::make_pair(NoItem, -1);
    }

    std::vector<DependencyItem> StateReligionDependency::getDependencyItems() const
    {
        return std::vector<DependencyItem>(1, std::make_pair(ID, -1));
    }

    void StateReligionDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n dependent on having state religion ";
#endif
    }

    void StateReligionDependency::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void StateReligionDependency::read(FDataStreamBase* pStream)
    {
    }


    CityBonusDependency::CityBonusDependency(BonusTypes bonusType, UnitTypes unitType, TechTypes bonusRevealTech)
        : andBonusTypes_(1, bonusType), unitType_(unitType)
    {
        if (bonusRevealTech != NO_TECH)
        {
            bonusTypesRevealTechsMap_[bonusType] = bonusRevealTech;
        }
    }

    CityBonusDependency::CityBonusDependency(const std::vector<BonusTypes>& andBonusTypes,
        const std::vector<BonusTypes>& orBonusTypes, 
        const std::map<BonusTypes, TechTypes>& bonusTypesRevealTechsMap,
        UnitTypes unitType)
        : andBonusTypes_(andBonusTypes), orBonusTypes_(orBonusTypes),
          bonusTypesRevealTechsMap_(bonusTypesRevealTechsMap), unitType_(unitType)
    {
    }

    void CityBonusDependency::apply(const CityDataPtr& pCityData)
    {
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCityData->getOwner());

        for (size_t i = 0, count = andBonusTypes_.size(); i < count; ++i)
        {
            pCityData->getBonusHelper()->changeNumBonuses(andBonusTypes_[i], 1);            
            updateRequestData(*pCityData,  pPlayer->getAnalysis()->getResourceInfo(andBonusTypes_[i]), true);
        }
        for (size_t i = 0, count = orBonusTypes_.size(); i < count; ++i)
        {
            pCityData->getBonusHelper()->changeNumBonuses(orBonusTypes_[i], 1);
            updateRequestData(*pCityData,  pPlayer->getAnalysis()->getResourceInfo(orBonusTypes_[i]), true);
        }

    }

    void CityBonusDependency::remove(const CityDataPtr& pCityData)
    {
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCityData->getOwner());

        for (size_t i = 0, count = andBonusTypes_.size(); i < count; ++i)
        {
            pCityData->getBonusHelper()->changeNumBonuses(andBonusTypes_[i], -1);
            updateRequestData(*pCityData,  pPlayer->getAnalysis()->getResourceInfo(andBonusTypes_[i]), false);
        }
        for (size_t i = 0, count = orBonusTypes_.size(); i < count; ++i)
        {
            pCityData->getBonusHelper()->changeNumBonuses(orBonusTypes_[i], -1);
            updateRequestData(*pCityData,  pPlayer->getAnalysis()->getResourceInfo(orBonusTypes_[i]), false);
        }
    }

    bool CityBonusDependency::required(const CvCity* pCity, int depTacticFlags) const
    {
        if (depTacticFlags & IDependentTactic::Resource_Dep)
        {
            return false;
        }

        const bool ignoreTechs = depTacticFlags & IDependentTactic::Tech_Dep;
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());

        bool foundAllAnds = andBonusTypes_.empty();
        for (size_t i = 0, count = andBonusTypes_.size(); i < count; ++i)
        {
            bool foundThisAnd = pCity->getNumBonuses(andBonusTypes_[i]) > 0;

            if (!foundThisAnd && ignoreTechs)
            {
                std::map<BonusTypes, TechTypes>::const_iterator ci = bonusTypesRevealTechsMap_.find(andBonusTypes_[i]);
                if (ci != bonusTypesRevealTechsMap_.end())
                {
                    foundThisAnd = Range<>(1, 3).contains(pPlayer->getAnalysis()->getTechResearchDepth(ci->second));
                }
            }

            if (!foundThisAnd)
            {
                foundAllAnds = false;
                break;
            }
            else
            {
                foundAllAnds = true;
            }
        }

        if (!foundAllAnds)
        {
            return true; // still a required dep
        }

        bool foundAnOr = orBonusTypes_.empty();
        for (size_t i = 0, count = orBonusTypes_.size(); i < count; ++i)
        {
            bool foundThisOr = pCity->getNumBonuses(orBonusTypes_[i]) > 0;

            if (!foundThisOr && ignoreTechs)
            {
                std::map<BonusTypes, TechTypes>::const_iterator ci = bonusTypesRevealTechsMap_.find(orBonusTypes_[i]);
                if (ci != bonusTypesRevealTechsMap_.end())
                {
                    foundThisOr = Range<>(1, 3).contains(pPlayer->getAnalysis()->getTechResearchDepth(ci->second));
                }
            }

            if (foundThisOr)
            {
                foundAnOr = true;
                break;
            }
        }

        return !foundAnOr;
    }

    bool CityBonusDependency::required(const Player& player, int depTacticFlags) const
    {
        if (depTacticFlags & IDependentTactic::Resource_Dep)
        {
            return false;
        }

        const bool ignoreTechs = depTacticFlags & IDependentTactic::Tech_Dep;

        bool foundAllAnds = andBonusTypes_.empty();
        for (size_t i = 0, count = andBonusTypes_.size(); i < count; ++i)
        {
            bool foundThisAnd = player.getCvPlayer()->getNumAvailableBonuses(andBonusTypes_[i]) > 0;

            if (!foundThisAnd && ignoreTechs)
            {
                std::map<BonusTypes, TechTypes>::const_iterator ci = bonusTypesRevealTechsMap_.find(andBonusTypes_[i]);
                if (ci != bonusTypesRevealTechsMap_.end())
                {
                    // if this resource requires another tech to reveal it, count dep as satisfied since we don't know if we have the resource yet
                    // only do this if the resource reveal tech is fairly close in depth (otherwise, we'd be including the value of the units
                    // even if they had another resource dependency which is several techs ahead - meaning we couldn't build it for a while anyway).
                    foundThisAnd = Range<>(1, 3).contains(player.getAnalysis()->getTechResearchDepth(ci->second));
                }
            }

            if (!foundThisAnd)
            {
                foundAllAnds = false;
                break;
            }
        }

        if (!foundAllAnds)
        {
            return true; // still a required dep
        }

        bool foundAnOr = orBonusTypes_.empty();
        for (size_t i = 0, count = orBonusTypes_.size(); i < count; ++i)
        {
            bool foundThisOr = player.getCvPlayer()->getNumAvailableBonuses(orBonusTypes_[i]) > 0;

            if (!foundThisOr && ignoreTechs)
            {
                std::map<BonusTypes, TechTypes>::const_iterator ci = bonusTypesRevealTechsMap_.find(orBonusTypes_[i]);
                if (ci != bonusTypesRevealTechsMap_.end())
                {
                    foundThisOr = Range<>(1, 3).contains(player.getAnalysis()->getTechResearchDepth(ci->second));
                }
            }

            if (foundThisOr)
            {
                foundAnOr = true;
                break;
            }
        }

        return !foundAnOr;
    }

    bool CityBonusDependency::removeable() const
    {
        return false;
    }

    bool CityBonusDependency::matches(int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Resource_Dep;
    }

    BuildQueueItem CityBonusDependency::getBuildItem() const
    {
        return std::make_pair(UnitItem, unitType_);
    }

    std::vector<DependencyItem> CityBonusDependency::getDependencyItems() const
    {
        std::vector<DependencyItem> items;
        for (size_t i = 0, count = andBonusTypes_.size(); i < count; ++i)
        {
            items.push_back(DependencyItem(ID, andBonusTypes_[i]));
        }
        for (size_t i = 0, count = orBonusTypes_.size(); i < count; ++i)
        {
            items.push_back(DependencyItem(ID, orBonusTypes_[i]));
        }
        return items;
    }

    void CityBonusDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n dependent on resources: ";
        for (size_t i = 0, count = andBonusTypes_.size(); i < count; ++i)
        {
            if (i > 0)
            {
                os << " and ";
            }
            os << gGlobals.getBonusInfo(andBonusTypes_[i]).getType();             
        }

        for (size_t i = 0, count = orBonusTypes_.size(); i < count; ++i)
        {
            if (i == 0 && !andBonusTypes_.empty())
            {
                os << " and ";
            }
            if (i > 0)
            {
                os << " or ";
            }
            os << gGlobals.getBonusInfo(orBonusTypes_[i]).getType();
        }
        for (std::map<BonusTypes, TechTypes>::const_iterator ci(bonusTypesRevealTechsMap_.begin()), ciEnd(bonusTypesRevealTechsMap_.end());
            ci != ciEnd; ++ci)
        {
            os << " " << gGlobals.getBonusInfo(ci->first).getType() << " revealed by: " << gGlobals.getTechInfo(ci->second).getType();
        }
#endif
    }

    void CityBonusDependency::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeVector(pStream, andBonusTypes_);
        writeVector(pStream, orBonusTypes_);
        writeMap(pStream, bonusTypesRevealTechsMap_);
        pStream->Write(unitType_);
    }

    void CityBonusDependency::read(FDataStreamBase* pStream)
    {
        readVector<BonusTypes, int>(pStream, andBonusTypes_);
        readVector<BonusTypes, int>(pStream, orBonusTypes_);
        readMap<BonusTypes, TechTypes, int, int>(pStream, bonusTypesRevealTechsMap_);
        pStream->Read((int*)&unitType_);
    }

    CivUnitDependency::CivUnitDependency(UnitTypes unitType)
        : unitType_(unitType)
    {
    }

    void CivUnitDependency::apply(const CityDataPtr& pCityData)
    {
        // nothing to do unless as the unit creates the building - so we just handle via depTacticFlags
    }

    void CivUnitDependency::remove(const CityDataPtr& pCityData)
    {
    }

    bool CivUnitDependency::required(const CvCity* pCity, int depTacticFlags) const
    {
        if (depTacticFlags & IDependentTactic::CivUnits_Dep)
        {
            return false;
        }

        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
        return unitType_ == NO_UNIT ? false : pPlayer->getUnitCount(unitType_) == 0;
    }

    bool CivUnitDependency::required(const Player& player, int depTacticFlags) const
    {
        if (depTacticFlags & IDependentTactic::CivUnits_Dep)
        {
            return false;
        }

        return unitType_ == NO_UNIT ? false : player.getUnitCount(unitType_) == 0;
    }

    bool CivUnitDependency::removeable() const
    {
        return false;
    }

    bool CivUnitDependency::matches(int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::CivUnits_Dep;
    }

    BuildQueueItem CivUnitDependency::getBuildItem() const
    {
        return std::make_pair(UnitItem, unitType_);
    }

    std::vector<DependencyItem> CivUnitDependency::getDependencyItems() const
    {
        return std::vector<DependencyItem>(1, std::make_pair(ID, unitType_));
    }

    void CivUnitDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\t dependent on civ having unit: ";
        if (unitType_ != NO_UNIT)
        {
            os << gGlobals.getUnitInfo(unitType_).getType();
        }
        else
        {
            os << " none? ";
        }
#endif
    }

    void CivUnitDependency::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(unitType_);
    }

    void CivUnitDependency::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&unitType_);
    }

    ResourceProductionBonusDependency::ResourceProductionBonusDependency(BonusTypes bonusType, int productionModifier)
        : bonusType_(bonusType), productionModifier_(productionModifier)
    {
    }

    void ResourceProductionBonusDependency::apply(const CityDataPtr& pCityData)
    {
        pCityData->getBonusHelper()->changeNumBonuses(bonusType_, 1);
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCityData->getOwner());
        updateRequestData(*pCityData,  pPlayer->getAnalysis()->getResourceInfo(bonusType_), true);
    }

    void ResourceProductionBonusDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getBonusHelper()->changeNumBonuses(bonusType_, -1);
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCityData->getOwner());
        updateRequestData(*pCityData,  pPlayer->getAnalysis()->getResourceInfo(bonusType_), false);
    }

    bool ResourceProductionBonusDependency::required(const CvCity* pCity, int depTacticFlags) const
    {
        return false;
    }

    bool ResourceProductionBonusDependency::required(const Player& player, int depTacticFlags) const
    {
        return false;
    }

    bool ResourceProductionBonusDependency::removeable() const
    {
        return true;
    }

    bool ResourceProductionBonusDependency::matches(int depTacticFlags) const
    {
        return depTacticFlags & IDependentTactic::Resource_Dep;
    }

    BuildQueueItem ResourceProductionBonusDependency::getBuildItem() const
    {
        return std::make_pair(NoItem, -1);
    }

    std::vector<DependencyItem> ResourceProductionBonusDependency::getDependencyItems() const
    {
        return std::vector<DependencyItem>(1, std::make_pair(ID, bonusType_));
    }

    void ResourceProductionBonusDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\t production modifier dependent on having resource: ";
        if (bonusType_ != NO_BONUS)
        {
            os << gGlobals.getBonusInfo(bonusType_).getType();
        }
        else
        {
            os << " none? ";
        }
#endif
    }

    void ResourceProductionBonusDependency::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(bonusType_);
        pStream->Write(productionModifier_);
    }

    void ResourceProductionBonusDependency::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&bonusType_);
        pStream->Read(&productionModifier_);
    }
}