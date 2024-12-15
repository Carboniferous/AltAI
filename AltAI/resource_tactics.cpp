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
#include "./resource_tactics_visitors.h"
#include "./civ_log.h"
#include "./helper_fns.h"

namespace AltAI
{
    void ResourceTactics::addTactic(const IResourceTacticPtr& pResourceTactic)
    {
        resourceTactics_.push_back(pResourceTactic);
    }

    void ResourceTactics::setTechDependency(const ResearchTechDependencyPtr& pTechDependency)
    {
        techDependency_ = pTechDependency;
    }

    void ResourceTactics::update(Player& player)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nUpdating resource tactic for: " << gGlobals.getBonusInfo(bonusType_).getType() << " turn = " << gGlobals.getGame().getGameTurn();
        /*CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {   
            os << "\n\nbase city output: " << narrow(pCity->getName()) << ' ';
            player.getCity(pCity->getID()).getBaseOutputProjection().debug(os);
        }
        os << '\n';*/
#endif
        for (std::list<IResourceTacticPtr>::const_iterator tacticIter(resourceTactics_.begin()), tacticEndIter(resourceTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->update(shared_from_this(), player);
        }

        lastTurnCalculated_ = gGlobals.getGame().getGameTurn();
    }

    void ResourceTactics::updateDependencies(const Player& player)
    {
        if (IsNotRequired(player)(techDependency_))
        {
            techDependency_.reset();
        }
    }

    void ResourceTactics::apply(TacticSelectionDataMap& selectionDataMap, int depTacticFlags)
    {
        if (!(depTacticFlags & IDependentTactic::Tech_Dep))
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

    int ResourceTactics::getTurnLastUpdated() const
    {
        return lastTurnCalculated_;
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
        pStream->Write(ResourceTacticsID);
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
            FAssertMsg(false, "Unexpected ID in ResourceTactics::factoryRead");
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

    void EconomicResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, City& city)
    {
        cityProjections_.erase(city.getCvCity()->getIDInfo());
        if (city.getCvCity()->hasBonus(pResourceTactics->getBonusType()))
        {
            return;
        }

        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(city.getCvCity()->getOwner());
        if (!resourceCanAffectCity(city, pPlayer->getAnalysis()->getResourceInfo(pResourceTactics->getBonusType())))
        {
            return;
        }

        ProjectionLadder base = gGlobals.getGame().getAltAI()->getPlayer(city.getCvCity()->getOwner())->getCity(city.getID()).getBaseOutputProjection();

        std::vector<IProjectionEventPtr> events;
        CityDataPtr pCityData = city.getCityData()->clone();
        pCityData->getBonusHelper()->changeNumBonuses(pResourceTactics->getBonusType(), 1);
        updateRequestData(*pCityData, pPlayer->getAnalysis()->getResourceInfo(pResourceTactics->getBonusType()), true);
            
        ProjectionLadder delta = getProjectedOutput(*pPlayer, pCityData, pPlayer->getAnalysis()->getNumSimTurns(), events, ConstructItem(), __FUNCTION__, false, false);
        cityProjections_[city.getCvCity()->getIDInfo()] = std::make_pair(delta, base);

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
        std::map<IDInfo, std::pair<ProjectionLadder, ProjectionLadder> >::const_iterator ci = cityProjections_.find(city.getCvCity()->getIDInfo());
        os << '\n' << __FUNCTION__ << ' ' << narrow(pPlayer->getCity(ci->first.iID).getCvCity()->getName()) << " = " << ci->second.first.getOutput() - ci->second.second.getOutput();
#endif

    }

    void EconomicResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, Player& player)
    {
        cityProjections_.clear();
        CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {
            update(pResourceTactics, player.getCity(pCity->getID()));
        }
    }

    void EconomicResourceTactic::apply(const ResourceTacticsPtr& pResourceTactics, TacticSelectionData& selectionData)
    {
        selectionData.potentialResourceOutputDeltas[pResourceTactics->getBonusType()] = std::make_pair(TotalOutput(), TotalOutput());

        TotalOutput accumulatedBase, accumulatedDelta;
        for (std::map<IDInfo, std::pair<ProjectionLadder, ProjectionLadder> >::const_iterator ci(cityProjections_.begin()), ciEnd(cityProjections_.end()); ci != ciEnd; ++ci)
        {
            if (!::getCity(ci->first))  // city may have been deleted since update was last called
            {
                continue;
            }

            accumulatedDelta += ci->second.first.getOutput();
            accumulatedBase += ci->second.second.getOutput();

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
        selectionData.potentialResourceOutputDeltas[pResourceTactics->getBonusType()].first += accumulatedDelta;
        selectionData.potentialResourceOutputDeltas[pResourceTactics->getBonusType()].second += accumulatedBase;
    }

    void EconomicResourceTactic::write(FDataStreamBase* pStream) const
    {
        // todo - write projection ladders
        pStream->Write(ID);
    }

    void EconomicResourceTactic::read(FDataStreamBase* pStream)
    {
    }

    void UnitResourceTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tUnit resource tactic: " << gGlobals.getUnitInfo(unitType_).getType();
#endif
    }

    void UnitResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, City& city)
    {
        if (city.getCvCity()->hasBonus(pResourceTactics->getBonusType()))
        {
            return;
        }
        // else... todo
    }

    void UnitResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, Player& player)
    {
        CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {
            if (pCity->hasBonus(pResourceTactics->getBonusType()))
            {
                continue;
            }
            // else... todo
        }        
    }

    void UnitResourceTactic::apply(const ResourceTacticsPtr& pResourceTactics, TacticSelectionData& selectionData)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pResourceTactics->getPlayerType()))->getStream();
        os << "\n\tApplying unit resource tactic for: " << gGlobals.getUnitInfo(unitType_).getType();
#endif
    }

    void UnitResourceTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void UnitResourceTactic::read(FDataStreamBase* pStream)
    {
    }

    void BuildingResourceTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tBuilding resource tactic " << gGlobals.getBuildingInfo(buildingType_).getType();
#endif
    }

    void BuildingResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, City& city)
    {
        if (city.getCvCity()->hasBonus(pResourceTactics->getBonusType()))
        {
            return;
        }
        // else... todo
    }

    void BuildingResourceTactic::update(const ResourceTacticsPtr& pResourceTactics, Player& player)
    {
        CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {
            if (pCity->hasBonus(pResourceTactics->getBonusType()))
            {
                continue;
            }
            // else... todo
        }
    }

    void BuildingResourceTactic::apply(const ResourceTacticsPtr& pResourceTactics, TacticSelectionData& selectionData)
    {
        //selectionData.potentialAcceleratedBuildings[pResourceTactics->getBonusType()].push_back(buildingType_);
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pResourceTactics->getPlayerType()))->getStream();
//        os << " found building: " << gGlobals.getBuildingInfo(buildingType_).getType()
//            << " accelerated with resource: " << gGlobals.getBonusInfo(pResourceTactics->getBonusType()).getType() << " factor = " << resourceModifier_
//            << " orig value = ";
//#endif
    }

    void BuildingResourceTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void BuildingResourceTactic::read(FDataStreamBase* pStream)
    {
    }
}

