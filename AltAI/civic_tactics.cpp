#include "AltAI.h"

#include "./civic_tactics.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./iters.h"
#include "./building_tactics_deps.h"
#include "./civ_helper.h"
#include "./helper_fns.h"

namespace AltAI
{
    void CivicTactics::addTactic(const ICivicTacticPtr& pCivicTactic)
    {
        civicTactics_.push_back(pCivicTactic);
    }

    void CivicTactics::setTechDependency(const ResearchTechDependencyPtr& pTechDependency)
    {
        techDependency_ = pTechDependency;
    }

    void CivicTactics::update(Player& player)
    {
        for (std::list<ICivicTacticPtr>::const_iterator tacticIter(civicTactics_.begin()), tacticEndIter(civicTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->update(shared_from_this(), player);
        }

        civicCost_ = player.getCvPlayer()->getSingleCivicUpkeep(civicType_);
    }

    void CivicTactics::updateDependencies(Player& player)
    {
        if (IsNotRequired(player)(techDependency_))
        {
            techDependency_.reset();
        }
    }

    void CivicTactics::apply(TacticSelectionDataMap& selectionDataMap, int ignoreFlags)
    {
        DependencyItemSet depSet;
        if (techDependency_)
        {
            const std::vector<DependencyItem>& thisDepItems = techDependency_->getDependencyItems();
            depSet.insert(thisDepItems.begin(), thisDepItems.end());
        }
        TacticSelectionData& techSelectionData = selectionDataMap[depSet];

        CivicValue civicValue;
        civicValue.civicType = civicType_;
        civicValue.cost = civicCost_;

        techSelectionData.civicValues.push_back(civicValue);

        for (std::list<ICivicTacticPtr>::const_iterator tacticIter(civicTactics_.begin()), tacticEndIter(civicTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->apply(shared_from_this(), techSelectionData);
        }
    }

    void CivicTactics::apply(TacticSelectionData& selectionData)
    {
    }

    CivicTypes CivicTactics::getCivicType() const
    {
        return civicType_;
    }

    PlayerTypes CivicTactics::getPlayerType() const
    {
        return playerType_;
    }

    ResearchTechDependencyPtr CivicTactics::getTechDependency() const
    {
        return techDependency_;
    }

    void CivicTactics::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tcivic tactics: " << (civicType_ == NO_CIVIC ? " none " : gGlobals.getCivicInfo(civicType_).getType());
        if (techDependency_)
        {
            techDependency_->debug(os);
        }
        for (std::list<ICivicTacticPtr>::const_iterator ci(civicTactics_.begin()), ciEnd(civicTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->debug(os);
        }        
        os << " civic cost = " << civicCost_;
#endif
    }

    void CivicTactics::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(civicType_);
        pStream->Write(playerType_);
    }

    void CivicTactics::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&civicType_);
        pStream->Read((int*)&playerType_);
    }

    CivicTacticsPtr CivicTactics::factoryRead(FDataStreamBase* pStream)
    {
        CivicTacticsPtr pCivicTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pCivicTactics = CivicTacticsPtr(new CivicTactics());
            break;
        default:
            FAssertMsg(true, "Unexpected ID in CivicTactics::factoryRead");
            break;
        }

        pCivicTactics->read(pStream);
        return pCivicTactics;
    }

    void EconomicCivicTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        for (std::map<IDInfo, ProjectionLadder>::const_iterator ci(cityProjections_.begin()), ciEnd(cityProjections_.end()); ci != ciEnd; ++ci)
        {
            TotalOutput civicDelta = ci->second.getOutput() - gGlobals.getGame().getAltAI()->getPlayer(ci->first.eOwner)->getCity(ci->first.iID).getBaseOutputProjection().getOutput();

            const CvCity* pCity = ::getCity(ci->first);
            os << " city: " << safeGetCityName(pCity) << " delta = " << civicDelta;
        }
        os << "\n\tGeneral economic civic tactic ";
#endif
    }

    void EconomicCivicTactic::update(const CivicTacticsPtr& pCivicTactics, Player& player)
    {
        CivicOptionTypes civicOptionType = (CivicOptionTypes)gGlobals.getCivicInfo(pCivicTactics->getCivicType()).getCivicOptionType();
        CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {
            CityDataPtr pCityData = player.getCity(pCity->getID()).getCityData()->clone();
            std::vector<IProjectionEventPtr> events;
            events.push_back(IProjectionEventPtr(new ProjectionChangeCivicEvent(civicOptionType, pCivicTactics->getCivicType(), 0)));
            cityProjections_[pCity->getIDInfo()] = getProjectedOutput(player, pCityData, player.getAnalysis()->getNumSimTurns(), events, ConstructItem(), __FUNCTION__, true, false);
            // need to reset civic as CivHelper data is currently shared - probably want to fix this
            // otherwise, when the civic is adopted next time round the civic which is un-applied is probably going to be wrong
            player.getCivHelper()->adoptCivic(player.getCvPlayer()->getCivics(civicOptionType));
        }
    }

    void EconomicCivicTactic::apply(const CivicTacticsPtr& pCivicTactics, TacticSelectionData& selectionData)
    {
        CivicValue& civicValue = *selectionData.civicValues.rbegin();

        for (std::map<IDInfo, ProjectionLadder>::const_iterator ci(cityProjections_.begin()), ciEnd(cityProjections_.end()); ci != ciEnd; ++ci)
        {
            if (!::getCity(ci->first))  // city may have been deleted since update was last called
            {
                continue;
            }
            civicValue.outputDelta += ci->second.getOutput() - gGlobals.getGame().getAltAI()->getPlayer(ci->first.eOwner)->getCity(ci->first.iID).getBaseOutputProjection().getOutput();
        }
    }

    void EconomicCivicTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void EconomicCivicTactic::read(FDataStreamBase* pStream)
    {
    }

    HurryCivicTactic::HurryCivicTactic(HurryTypes hurryType) : 
            hurryType_(hurryType), productionPerPopulation_(0), goldPerProduction_(0)
    {
        if (hurryType_ != NO_HURRY)
        {
            const CvHurryInfo& hurryInfo = gGlobals.getHurryInfo(hurryType_);
            productionPerPopulation_ = hurryInfo.getProductionPerPopulation();
            goldPerProduction_ = hurryInfo.getGoldPerProduction();
        }
    }

    void HurryCivicTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHurry civic tactic: " << (hurryType_ == NO_HURRY ? " none " : gGlobals.getHurryInfo(hurryType_).getType()) << " ";
#endif
    }

    void HurryCivicTactic::update(const CivicTacticsPtr& pCivicTactics, Player& player)
    {
        CivicOptionTypes civicOptionType = (CivicOptionTypes)gGlobals.getCivicInfo(pCivicTactics->getCivicType()).getCivicOptionType();
        CityIter cityIter(*player.getCvPlayer());
        HurryData hurryData(hurryType_);
        while (CvCity* pCity = cityIter())
        {
            ProjectionLadder base = player.getCity(pCity->getID()).getBaseOutputProjection();

            CityDataPtr pCityData = player.getCity(pCity->getID()).getCityData()->clone();
            std::vector<IProjectionEventPtr> events;
            events.push_back(IProjectionEventPtr(new ProjectionChangeCivicEvent(civicOptionType, pCivicTactics->getCivicType(), 0)));

            hurryData.hurryPopulation = 2; //  todo - derive dynamically
            events.push_back(IProjectionEventPtr(new ProjectionHurryEvent(hurryData)));
            ProjectionLadder delta = getProjectedOutput(player, pCityData, player.getAnalysis()->getNumSimTurns(), events, ConstructItem(), __FUNCTION__, true, false);
            cityProjections_[pCity->getIDInfo()] = std::make_pair(delta, base);

            // need to reset civic as CivHelper data is currently shared - probably want to fix this
            // otherwise, when the civic is adopted next time round the civic which is un-applied is probably going to be wrong
            player.getCivHelper()->adoptCivic(player.getCvPlayer()->getCivics(civicOptionType));
        }
    }

    void HurryCivicTactic::apply(const CivicTacticsPtr& pCivicTactics, TacticSelectionData& selectionData)
    {
        CivicValue& civicValue = *selectionData.civicValues.rbegin();
        civicValue.yieldOutputDelta = std::make_pair(TotalOutput(), TotalOutput());

        TotalOutput accumulatedBase, accumulatedDelta;
        for (std::map<IDInfo, std::pair<ProjectionLadder, ProjectionLadder> >::const_iterator ci(cityProjections_.begin()), ciEnd(cityProjections_.end()); ci != ciEnd; ++ci)
        {
            if (!::getCity(ci->first))  // city may have been deleted since update was last called
            {
                continue;
            }

            accumulatedBase += ci->second.second.getOutput();
            accumulatedDelta += ci->second.first.getOutput() - ci->second.second.getOutput();
            accumulatedDelta[OUTPUT_PRODUCTION] += ci->second.first.getAccumulatedProduction();
        }
        civicValue.yieldOutputDelta.first += accumulatedDelta;
        civicValue.yieldOutputDelta.second += asPercentageOf(accumulatedDelta, accumulatedBase);
    }

    void HurryCivicTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void HurryCivicTactic::read(FDataStreamBase* pStream)
    {
    }
}