#include "AltAI.h"

#include "./city_defence.h"
#include "./game.h"
#include "./unit_tactics.h"
#include "./tactics_interfaces.h"
#include "./player_analysis.h"
#include "./tictacs.h"
#include "./unit_log.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
    }

    class CityDefenceAnalysisImpl
    {
    public:
        explicit CityDefenceAnalysisImpl(Player& player) : player_(player)
        {
        }

        void addOurUnit(const CvUnit* pUnit, const CvCity* pCity)
        {
            assignedUnits_[pCity->getIDInfo()].insert(pUnit->getIDInfo());
        }

        void addOurCity(IDInfo city)
        {
            ourCities_.insert(city);
        }

        void deleteOurCity(IDInfo city)
        {
            ourCities_.erase(city);
        }

        void noteHostileStack(IDInfo city, const std::vector<const CvUnit*> enemyStack)
        {
            const CvCity* pCity = ::getCity(city);
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nChecking hostile stack near city: " << safeGetCityName(pCity);
#endif
            if (pCity)
            {
                UnitData::CombatDetails combatDetails(pCity->plot());

                std::vector<UnitData> ourUnitData, enemyUnitData;
                std::vector<const CvUnit*> ourUnits;
                for (size_t i = 0, count = enemyStack.size(); i < count; ++i)
                {
                    if (enemyStack[i]->canFight())
                    {
                        enemyUnitData.push_back(UnitData(enemyStack[i]));
                        combatDetails.unitAttackDirectionsMap[enemyStack[i]->getIDInfo()] = directionXY(enemyStack[i]->plot(), pCity->plot());
                    }
                }

                std::map<IDInfo, std::set<IDInfo> >::const_iterator cityIter = assignedUnits_.find(city);
                if (cityIter != assignedUnits_.end())
                {
                    for (std::set<IDInfo>::const_iterator ourUnitsIter(cityIter->second.begin()), ourUnitsEndIter(cityIter->second.end()); ourUnitsIter != ourUnitsEndIter; ++ourUnitsIter)
                    {
                        const CvUnit* pOurUnit = ::getUnit(*ourUnitsIter);
                        if (pOurUnit)
                        {
                            ourUnits.push_back(pOurUnit);
                            ourUnitData.push_back(UnitData(pOurUnit));
                        }
                    }
                }

                CombatGraph combatGraph = getCombatGraph(player_, combatDetails, enemyUnitData, ourUnitData);
                combatGraph.analyseEndStates();
#ifdef ALTAI_DEBUG
                os << "\nOdds data defending at plot: " << pCity->plot()->getCoords() << " for group: ";
                debugUnitVector(os, ourUnits);
                os << " winning odds = " << combatGraph.endStatesData.pLoss << ", losing odds = " << combatGraph.endStatesData.pWin;
                if (combatGraph.endStatesData.pDraw > 0)
                {
                    os << " draw = " << combatGraph.endStatesData.pDraw;
                }
                for (size_t i = 0, count = ourUnits.size(); i < count; ++i)
                {
                    os << " " << ourUnits[i]->getIDInfo() << " survival odds = " << combatGraph.getSurvivalOdds(ourUnits[i]->getIDInfo(), false);
                }
                for (size_t i = 0, count = ourUnits.size(); i < count; ++i)
                {
                    os << " " << ourUnits[i]->getIDInfo() << " survival odds (2) = " << combatGraph.endStatesData.defenderUnitOdds[i];
                }
#endif
            }
        }

        std::vector<RequiredUnitStack::UnitDataChoices> getRequiredUnits(IDInfo city)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = UnitLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nCityDefenceAnalysis: getRequiredUnits: " << safeGetCityName(city);
#endif
            std::vector<RequiredUnitStack::UnitDataChoices> requiredUnits;

            DependencyItemSet di;
            TacticSelectionData& currentSelectionData = player_.getAnalysis()->getPlayerTactics()->tacticSelectionDataMap[di];

            std::map<int, std::set<UnitTypes>, std::greater<int> > unitValuesMap;
            for (std::set<UnitTacticValue>::const_iterator ci(currentSelectionData.cityDefenceUnits.begin()),
                ciEnd(currentSelectionData.cityDefenceUnits.end()); ci != ciEnd; ++ci)
            {
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(ci->unitType);
                if (!unitInfo.isNoDefensiveBonus())  // todo: move this check into unit tactics for city defence
                {
#ifdef ALTAI_DEBUG
                    os << " adding: " << gGlobals.getUnitInfo(ci->unitType).getType() << " value = " << ci->unitAnalysisValue;
#endif
                    unitValuesMap[ci->unitAnalysisValue].insert(ci->unitType);
                }
            }

            std::set<UnitTypes> unitsToConsider;

            int maxValue = unitValuesMap.empty() ? 0 : unitValuesMap.begin()->first;
            for (std::map<int, std::set<UnitTypes>, std::greater<int> >::const_iterator iter(unitValuesMap.begin()), endIter(unitValuesMap.end());
                iter != endIter; ++iter)
            {
                if (iter->first > (maxValue * 80 / 100))
                {
                    unitsToConsider.insert(iter->second.begin(), iter->second.end());
                }
            }

            if (unitsToConsider.empty())
            {
                std::vector<UnitTypes> actualLandCombatUnits, possibleLandCombatUnits;
                boost::tie(actualLandCombatUnits, possibleLandCombatUnits) = getActualAndPossibleCombatUnits(player_, NULL, DOMAIN_LAND);
                unitsToConsider.insert(actualLandCombatUnits.begin(), actualLandCombatUnits.end());
            }

#ifdef ALTAI_DEBUG
            os << " possible guard units:";
            for (std::set<UnitTypes>::const_iterator ci(unitsToConsider.begin()), ciEnd(unitsToConsider.end()); ci != ciEnd; ++ci)
            {
                os << (ci != unitsToConsider.begin() ? ", " : " ") << gGlobals.getUnitInfo(*ci).getType();
            }
#endif
            requiredUnits.push_back(RequiredUnitStack::UnitDataChoices());
            for (std::set<UnitTypes>::const_iterator ci(unitsToConsider.begin()), ciEnd(unitsToConsider.end()); ci != ciEnd; ++ci)
            {
                requiredUnits[0].push_back(UnitData(*ci));
            }

            return requiredUnits;
        }

    private:
        Player& player_;
        std::map<IDInfo, std::set<IDInfo> > assignedUnits_;
        std::set<IDInfo> ourCities_;
    };

    CityDefenceAnalysis::CityDefenceAnalysis(Player& player)
    {
        pImpl_ = boost::shared_ptr<CityDefenceAnalysisImpl>(new CityDefenceAnalysisImpl(player));
    }

    void CityDefenceAnalysis::addOurUnit(const CvUnit* pUnit, const CvCity* pCity)
    {
        pImpl_->addOurUnit(pUnit, pCity);
    }

    void CityDefenceAnalysis::addOurCity(IDInfo city)
    {
        pImpl_->addOurCity(city);
    }

    void CityDefenceAnalysis::deleteOurCity(IDInfo city)
    {
        pImpl_->deleteOurCity(city);
    }

    void CityDefenceAnalysis::noteHostileStack(IDInfo city, const std::vector<const CvUnit*> enemyStack)
    {
        pImpl_->noteHostileStack(city, enemyStack);
    }

    std::vector<RequiredUnitStack::UnitDataChoices> CityDefenceAnalysis::getRequiredUnits(IDInfo city)
    {
        return pImpl_->getRequiredUnits(city);
    }
}