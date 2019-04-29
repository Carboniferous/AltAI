#include "AltAI.h"

#include "./resource_tactics.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./iters.h"
#include "./building_tactics_deps.h"
#include "./bonus_helper.h"
#include "./civ_helper.h"
#include "./resource_info_visitors.h"
#include "./civ_log.h"
#include "./helper_fns.h"

namespace AltAI
{
    void ResourceTactics::addTactic(const IResourceTacticPtr& pCivicTactic)
    {
        resourceTactics_.push_back(pCivicTactic);
    }

    void ResourceTactics::setTechDependency(const ResearchTechDependencyPtr& pTechDependency)
    {
        techDependency_ = pTechDependency;
    }

    void ResourceTactics::update(const Player& player)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nUpdating resource tactic for: " << gGlobals.getBonusInfo(bonusType_).getType() << " turn = " << gGlobals.getGame().getGameTurn();
#endif
        for (std::list<IResourceTacticPtr>::const_iterator tacticIter(resourceTactics_.begin()), tacticEndIter(resourceTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->update(shared_from_this(), player);
        }
    }

    void ResourceTactics::updateDependencies(const Player& player)
    {
        if (IsNotRequired(player)(techDependency_))
        {
            techDependency_.reset();
        }
    }

    void ResourceTactics::apply(TacticSelectionDataMap& selectionDataMap, int ignoreFlags)
    {
        if (!(ignoreFlags & IDependentTactic::Ignore_Techs))
        {
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
            if (pPlayer->getTechResearchDepth(techDependency_->getResearchTech()) > 3)
            {
                return;
            }
        }

        DependencyItemSet depSet;
        if (techDependency_)
        {
            const std::vector<DependencyItem>& thisDepItems = techDependency_->getDependencyItems();
            depSet.insert(thisDepItems.begin(), thisDepItems.end());
        }
        TacticSelectionData& techSelectionData = selectionDataMap[depSet];

        for (std::list<IResourceTacticPtr>::const_iterator tacticIter(resourceTactics_.begin()), tacticEndIter(resourceTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->apply(shared_from_this(), techSelectionData);
        }
    }

    void ResourceTactics::apply(TacticSelectionData& selectionData)
    {
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
        if (pPlayer->getTechResearchDepth(techDependency_->getResearchTech()) > 2)
        {
            return;
        }

        for (std::list<IResourceTacticPtr>::const_iterator tacticIter(resourceTactics_.begin()), tacticEndIter(resourceTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->apply(shared_from_this(), selectionData);
        }
    }

    BonusTypes ResourceTactics::getBonusType() const
    {
        return bonusType_;
    }

    PlayerTypes ResourceTactics::getPlayerType() const
    {
        return playerType_;
    }

    ResearchTechDependencyPtr ResourceTactics::getTechDependency() const
    {
        return techDependency_;
    }

    void ResourceTactics::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tresource tactics: " << (bonusType_ == NO_BONUS ? " none? " : gGlobals.getBonusInfo(bonusType_).getType());
        if (techDependency_)
        {
            techDependency_->debug(os);
        }
        for (std::list<IResourceTacticPtr>::const_iterator ci(resourceTactics_.begin()), ciEnd(resourceTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->debug(os);
        }        
#endif
    }

    void ResourceTactics::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(bonusType_);
    }

    void ResourceTactics::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&bonusType_);
    }

    ResourceTacticsPtr ResourceTactics::factoryRead(FDataStreamBase* pStream)
    {
        ResourceTacticsPtr pResourceTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pResourceTactics = ResourceTacticsPtr(new ResourceTactics());
            break;
        default:
            break;
        }

        pResourceTactics->read(pStream);
        return pResourceTactics;
    }

    void EconomicResourceTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tGeneral economic resource tactic ";
#endif
    }

    void EconomicResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, const City& city)
    {
        cityProjections_.erase(city.getCvCity()->getIDInfo());
        if (city.getCvCity()->hasBonus(pResourceTactics->getBonusType()))
        {
            return;
        }

        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(city.getCvCity()->getOwner());
        {
            CityDataPtr pCityData = city.getCityData()->clone();
            pCityData->getBonusHelper()->changeNumBonuses(pResourceTactics->getBonusType(), 1);
            updateCityData(*pCityData, pPlayer->getAnalysis()->getResourceInfo(pResourceTactics->getBonusType()), true);

            std::vector<IProjectionEventPtr> events;
            cityProjections_[city.getCvCity()->getIDInfo()] = getProjectedOutput(*pPlayer, pCityData, 30, events, ConstructItem(), __FUNCTION__, false, true);
        }
    }

    void EconomicResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, const Player& player)
    {
        cityProjections_.clear();
        CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {
            if (pCity->hasBonus(pResourceTactics->getBonusType()))
            {
                continue;
            }

            {
                CityDataPtr pCityData = player.getCity(pCity->getID()).getCityData()->clone();
                pCityData->getBonusHelper()->changeNumBonuses(pResourceTactics->getBonusType(), 1);
                updateCityData(*pCityData, player.getAnalysis()->getResourceInfo(pResourceTactics->getBonusType()), true);

                std::vector<IProjectionEventPtr> events;
                cityProjections_[pCity->getIDInfo()] = getProjectedOutput(player, pCityData, 30, events, ConstructItem(), __FUNCTION__, false, true);
            }
        }
    }

    void EconomicResourceTactic::apply(const ResourceTacticsPtr& pResourceTactics, TacticSelectionData& selectionData)
    {
        for (std::map<IDInfo, ProjectionLadder>::const_iterator ci(cityProjections_.begin()), ciEnd(cityProjections_.end()); ci != ciEnd; ++ci)
        {
            if (!::getCity(ci->first))  // city may have been deleted since update was last called
            {
                continue;
            }
            TotalOutput delta = ci->second.getOutput() - gGlobals.getGame().getAltAI()->getPlayer(ci->first.eOwner)->getCity(ci->first.iID).getBaseOutputProjection().getOutput();
            selectionData.potentialResourceOutputDeltas[pResourceTactics->getBonusType()] += delta;

            /*if (!isEmpty(delta))
            {
#ifdef ALTAI_DEBUG
                const CvPlayer& player = CvPlayerAI::getPlayer(ci->first.eOwner);
                std::ostream& os = CivLog::getLog(player)->getStream();
                os << "\nNon-zero resource delta for city: " << safeGetCityName(::getCity(ci->first)) << delta;
                ci->second.debug(os);
                gGlobals.getGame().getAltAI()->getPlayer(ci->first.eOwner)->getCity(ci->first.iID).getBaseOutputProjection().debug(os);
#endif
            }*/
        }
    }

    void EconomicResourceTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void EconomicResourceTactic::read(FDataStreamBase* pStream)
    {
    }

    void UnitResourceTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tUnit resource tactic ";
#endif
    }

    void UnitResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, const City& city)
    {
        if (city.getCvCity()->hasBonus(pResourceTactics->getBonusType()))
        {
            return;
        }
    }

    void UnitResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, const Player& player)
    {
        CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {
            if (pCity->hasBonus(pResourceTactics->getBonusType()))
            {
                continue;
            }
        }
    }

    void UnitResourceTactic::apply(const ResourceTacticsPtr& pResourceTactics, TacticSelectionData& selectionData)
    {
    }

    void UnitResourceTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void UnitResourceTactic::read(FDataStreamBase* pStream)
    {
    }
}

