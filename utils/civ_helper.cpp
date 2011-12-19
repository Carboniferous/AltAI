#include "./civ_helper.h"

namespace AltAI
{
    CivHelper::CivHelper(const CvPlayer& player) : player_(player)
    {
        init_();
    }

    CivHelper::CivHelper(PlayerTypes playerType) : player_(CvPlayerAI::getPlayer(playerType))
    {
        init_();
    }

    CivHelper::CivHelper(const CivHelper& other)
        : player_(other.player_), techs_(other.techs_), techsToResearch_(other.techsToResearch_),
          availableCivics_(other.availableCivics_), currentCivics_(other.currentCivics_)
    {
    }

    void CivHelper::init_()
    {
        const int numCivicOptions = gGlobals.getNumCivicOptionInfos();
        currentCivics_.resize(numCivicOptions, NO_CIVIC);

        for (int i = 0; i < numCivicOptions; ++i)
        {
            currentCivics_[i] = player_.getCivics((CivicOptionTypes)i);
        }
    }

    bool CivHelper::hasTech(TechTypes techType) const
    {
        return CvTeamAI::getTeam(player_.getTeam()).isHasTech(techType) || techs_.find(techType) != techs_.end();
    }

    void CivHelper::addTech(TechTypes techType)
    {
        techs_.insert(techType);
    }

    // only removes techs we added
    void CivHelper::removeTech(TechTypes techType)
    {
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
            if (player_.canEverResearch((TechTypes)i))
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

    bool CivHelper::civicIsAvailable(CivicTypes civicType)
    {
        return player_.canDoCivics(civicType) || availableCivics_.find(civicType) != availableCivics_.end();
    }

    void CivHelper::addCivic(CivicTypes civicType)
    {
        availableCivics_.insert(civicType);
    }

    void CivHelper::adoptCivic(CivicTypes civicType)
    {
        currentCivics_[gGlobals.getCivicInfo(civicType).getCivicOptionType()] = civicType;
    }

    void CivHelper::makeAllCivicsAvailable()
    {
        for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
        {
            availableCivics_.insert((CivicTypes)i);
        }
    }
}