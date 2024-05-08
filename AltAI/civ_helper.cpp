#include "AltAI.h"

#include "./civ_helper.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./civic_info_visitors.h"
#include "./iters.h"
#include "./civ_log.h"

namespace AltAI
{
    CivHelper::CivHelper(const Player& player) : player_(player)
    {
    }

    CivHelperPtr CivHelper::clone() const
    {
        CivHelperPtr copy = CivHelperPtr(new CivHelper(*this));
        return copy;
    }

    void CivHelper::init()
    {
        const int numCivicOptions = gGlobals.getNumCivicOptionInfos();
        currentCivics_.resize(numCivicOptions, NO_CIVIC);
        specialBuildingNotRequiredCounts_.resize(gGlobals.getNumSpecialBuildingInfos(), 0);

        for (int i = 0; i < numCivicOptions; ++i)
        {
            currentCivics_[i] = player_.getCvPlayer()->getCivics((CivicOptionTypes)i);
            updateSpecialBuildingNotRequiredCount(specialBuildingNotRequiredCounts_, player_.getAnalysis()->getCivicInfo(currentCivics_[i]), true);
        }
    }

    bool CivHelper::hasTech(TechTypes techType) const
    {
        return CvTeamAI::getTeam(player_.getTeamID()).isHasTech(techType) || techs_.find(techType) != techs_.end();
    }

    void CivHelper::addTech(TechTypes techType)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//        os << "\nCivHelper adding tech: " << gGlobals.getTechInfo(techType).getType();
//#endif
        techs_.insert(techType);
    }

    // only removes techs we added
    void CivHelper::removeTech(TechTypes techType)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//        os << "\nCivHelper removing tech: " << gGlobals.getTechInfo(techType).getType();
//#endif

        std::set<TechTypes>::iterator iter = techs_.find(techType);
        if (iter != techs_.end())
        {
            techs_.erase(iter);
        }
    }

    void CivHelper::addAllTechs()
    {
        for (int i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            if (player_.getCvPlayer()->canEverResearch((TechTypes)i))
            {
                techs_.insert((TechTypes)i);
            }
        }
    }

    bool CivHelper::isResearchingTech(TechTypes techType)
    {
        return std::find(techsToResearch_.begin(), techsToResearch_.end(), techType) != techsToResearch_.end();
    }

    void CivHelper::pushResearchTech(TechTypes techType)
    {
        techsToResearch_.push_back(techType);
    }

    const std::list<TechTypes>& CivHelper::getResearchTechs() const
    {
        return techsToResearch_;
    }

    void CivHelper::clearResearchTechs()
    {
        techsToResearch_.clear();
    }

    bool CivHelper::isInCivic(CivicTypes civicType) const
    {
        return currentCivics_[gGlobals.getCivicInfo(civicType).getCivicOptionType()] == civicType;
    }

    CivicTypes CivHelper::currentCivic(CivicOptionTypes civicOptionType) const
    {
        return currentCivics_[civicOptionType];
    }

    /*bool CivHelper::civicIsAvailable(CivicTypes civicType)
    {
        return player_.getCvPlayer()->canDoCivics(civicType) || availableCivics_.find(civicType) != availableCivics_.end();
    }*/

    /*void CivHelper::addCivic(CivicTypes civicType)
    {
        availableCivics_.insert(civicType);
    }*/

    void CivHelper::adoptCivic(CivicTypes civicType)
    {
        CivicOptionTypes civicOptionType = (CivicOptionTypes)gGlobals.getCivicInfo(civicType).getCivicOptionType();
        if (currentCivics_[civicOptionType] != civicType)
        {
            updateSpecialBuildingNotRequiredCount(specialBuildingNotRequiredCounts_, player_.getAnalysis()->getCivicInfo(currentCivics_[civicOptionType]), false);
            currentCivics_[gGlobals.getCivicInfo(civicType).getCivicOptionType()] = civicType;
            updateSpecialBuildingNotRequiredCount(specialBuildingNotRequiredCounts_, player_.getAnalysis()->getCivicInfo(civicType), true);
        }
    }

    /*void CivHelper::makeAllCivicsAvailable()
    {
        for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
        {
            availableCivics_.insert((CivicTypes)i);
        }
    }*/

    const std::vector<CivicTypes>& CivHelper::getCurrentCivics() const
    {
        return currentCivics_;
    }

    int CivHelper::getSpecialBuildingNotRequiredCount(SpecialBuildingTypes specialBuildingType) const
    {
        return specialBuildingNotRequiredCounts_[specialBuildingType];
    }
}