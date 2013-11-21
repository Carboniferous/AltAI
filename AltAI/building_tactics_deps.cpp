#include "AltAI.h"

#include "./building_tactics_deps.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./city_data.h"
#include "./civ_helper.h"
#include "./building_helper.h"
#include "./religion_helper.h"
#include "./bonus_helper.h"
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
    }

    void ResearchTechDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getCivHelper()->removeTech(techType_);
    }

    bool ResearchTechDependency::required(const CvCity* pCity, int ignoreFlags) const
    {
        return ignoreFlags & IDependentTactic::Ignore_Techs ? false : !gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCivHelper()->hasTech(techType_);
    }

    bool ResearchTechDependency::required(const Player& player, int ignoreFlags) const
    {
        return ignoreFlags & IDependentTactic::Ignore_Techs ? false : !player.getCivHelper()->hasTech(techType_);
    }

    bool ResearchTechDependency::removeable() const
    {
        return true;
    }

    std::pair<BuildQueueTypes, int> ResearchTechDependency::getBuildItem() const
    {
        return std::make_pair(NoItem, -1);
    }

    std::pair<int, int> ResearchTechDependency::getDependencyItem() const
    {
        return std::make_pair(ID, techType_);
    }

    void ResearchTechDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on tech: " << gGlobals.getTechInfo(techType_).getType();
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
    }

    void CityBuildingDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getBuildingsHelper()->changeNumRealBuildings(buildingType_, false);
    }

    bool CityBuildingDependency::required(const CvCity* pCity, int ignoreFlags) const
    {
        return ignoreFlags & IDependentTactic::Ignore_City_Buildings ? false : pCity->getNumBuilding(buildingType_) == 0;
    }

    bool CityBuildingDependency::required(const Player& player, int ignoreFlags) const
    {
        if (ignoreFlags & IDependentTactic::Ignore_City_Buildings)
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

    std::pair<BuildQueueTypes, int> CityBuildingDependency::getBuildItem() const
    {
        return std::make_pair(BuildingItem, buildingType_);
    }

    std::pair<int, int> CityBuildingDependency::getDependencyItem() const
    {
        return std::make_pair(ID, buildingType_);
    }

    void CityBuildingDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on city building: " << gGlobals.getBuildingInfo(buildingType_).getType();
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

    bool CivBuildingDependency::required(const CvCity* pCity, int ignoreFlags) const
    {
        if (ignoreFlags & IDependentTactic::Ignore_Civ_Buildings)
        {
            return false;
        }

        const CvPlayerAI& player = CvPlayerAI::getPlayer(pCity->getOwner());

        const BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType_).getBuildingClassType(),
            sourceBuildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(sourceBuildingType_).getBuildingClassType();

        return player.getBuildingClassCount(buildingClassType) < (1 + player.getBuildingClassCountPlusMaking(sourceBuildingClassType)) * count_;
    }

    bool CivBuildingDependency::required(const Player& player, int ignoreFlags) const
    {
        if (ignoreFlags & IDependentTactic::Ignore_Civ_Buildings)
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

    std::pair<BuildQueueTypes, int> CivBuildingDependency::getBuildItem() const
    {
        return std::make_pair(BuildingItem, buildingType_);
    }

    std::pair<int, int> CivBuildingDependency::getDependencyItem() const
    {
        return std::make_pair(ID, buildingType_);
    }

    void CivBuildingDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on civ building: " << gGlobals.getBuildingInfo(buildingType_).getType() << ", count = " << count_
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
        pCityData->getReligionHelper()->changeReligionCount(religionType_);
    }

    void ReligiousDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getReligionHelper()->changeReligionCount(religionType_, -1);
    }

    bool ReligiousDependency::required(const CvCity* pCity, int ignoreFlags) const
    {
        return ignoreFlags & IDependentTactic::Ignore_Religions ? false : !pCity->isHasReligion(religionType_);
    }

    bool ReligiousDependency::required(const Player& player, int ignoreFlags) const
    {
        return ignoreFlags & IDependentTactic::Ignore_Religions ? false : player.getCvPlayer()->getHasReligionCount(religionType_) == 0;
    }

    bool ReligiousDependency::removeable() const
    {
        return true;
    }

    std::pair<BuildQueueTypes, int> ReligiousDependency::getBuildItem() const
    {
        return std::make_pair(UnitItem, unitType_);
    }

    std::pair<int, int> ReligiousDependency::getDependencyItem() const
    {
        return std::make_pair(ID, religionType_);
    }

    void ReligiousDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on religion: " << gGlobals.getReligionInfo(religionType_).getType();
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


    CityBonusDependency::CityBonusDependency(BonusTypes bonusType, UnitTypes unitType, bool isOr)
        : bonusTypes_(1, bonusType), unitType_(unitType), isOr_(isOr)
    {
    }

    CityBonusDependency::CityBonusDependency(const std::vector<BonusTypes>& bonusTypes, UnitTypes unitType, bool isOr)
        : bonusTypes_(bonusTypes), unitType_(unitType), isOr_(isOr)
    {
    }

    void CityBonusDependency::apply(const CityDataPtr& pCityData)
    {
        for (size_t i = 0, count = bonusTypes_.size(); i < count; ++i)
        {
            pCityData->getBonusHelper()->changeNumBonuses(bonusTypes_[i], 1);
        }
    }

    void CityBonusDependency::remove(const CityDataPtr& pCityData)
    {
        for (size_t i = 0, count = bonusTypes_.size(); i < count; ++i)
        {
            pCityData->getBonusHelper()->changeNumBonuses(bonusTypes_[i], -1);
        }
    }

    bool CityBonusDependency::required(const CvCity* pCity, int ignoreFlags) const
    {
        if (ignoreFlags & IDependentTactic::Ignore_Resources)
        {
            return false;
        }

        bool foundAny = false;
        for (size_t i = 0, count = bonusTypes_.size(); i < count; ++i)
        {
            const int bonusCount = pCity->getNumBonuses(bonusTypes_[i]);
            if (bonusCount > 0)
            {
                foundAny = true;
            }
            else if (!isOr_)
            {
                return true;
            }
        }
        return isOr_ ? !foundAny : false;
    }

    bool CityBonusDependency::required(const Player& player, int ignoreFlags) const
    {
        if (ignoreFlags & IDependentTactic::Ignore_Resources)
        {
            return false;
        }

        bool foundAny = false;
        for (size_t i = 0, count = bonusTypes_.size(); i < count; ++i)
        {
            const int bonusCount = player.getCvPlayer()->getNumAvailableBonuses(bonusTypes_[i]);
            if (bonusCount > 0)
            {
                foundAny = true;
            }
            else if (!isOr_)
            {
                return true;
            }
        }
        return isOr_ ? !foundAny : false;
    }

    bool CityBonusDependency::removeable() const
    {
        return false;
    }

    std::pair<BuildQueueTypes, int> CityBonusDependency::getBuildItem() const
    {
        return std::make_pair(UnitItem, unitType_);
    }

    std::pair<int, int> CityBonusDependency::getDependencyItem() const
    {
        return std::make_pair(ID, bonusTypes_.empty() ? NO_BONUS : bonusTypes_[0]);
    }

    void CityBonusDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on resources: ";
        for (size_t i = 0, count = bonusTypes_.size(); i < count; ++i)
        {
             if (i > 0)
             {
                 if (isOr_)
                 {
                     os << " or ";
                 }
                 else
                 {
                     os << " and ";
                 }
             }
             os << gGlobals.getBonusInfo(bonusTypes_[i]).getType();
             
        }
#endif
    }

    void CityBonusDependency::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeVector(pStream, bonusTypes_);
        pStream->Write(unitType_);
    }

    void CityBonusDependency::read(FDataStreamBase* pStream)
    {
        readVector<BonusTypes, int>(pStream, bonusTypes_);
        pStream->Read((int*)&unitType_);
    }
}