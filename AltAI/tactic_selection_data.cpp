#include "AltAI.h"

#include "./tactic_selection_data.h"

namespace AltAI
{
    bool CultureBuildingValue::operator < (const CultureBuildingValue& other) const
    {
        TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 20, 1);
        TotalOutputValueFunctor valueF(weights);

        return valueF(output) / std::max<int>(1, nTurns) > valueF(other.output) / std::max<int>(1, other.nTurns);
    }

    void CultureBuildingValue::debug(std::ostream& os) const
    {
        os << "\n\t" << gGlobals.getBuildingInfo(buildingType).getType() << " turns = " << nTurns << ", output = " << output;
    }

    bool EconomicBuildingValue::operator < (const EconomicBuildingValue& other) const
    {
        TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 1, 1);
        TotalOutputValueFunctor valueF(weights);

        return valueF(output) / std::max<int>(1, nTurns) > valueF(other.output) / std::max<int>(1, other.nTurns);
    }

    void EconomicBuildingValue::debug(std::ostream& os) const
    {
        os << "\n\t" << gGlobals.getBuildingInfo(buildingType).getType() << " turns = " << nTurns << ", output = " << output;
    }

    void EconomicWonderValue::debug(std::ostream& os) const
    {
        for (size_t i = 0, count = buildCityValues.size(); i < count; ++i)
        {
            os << narrow(getCity(buildCityValues[i].first)->getName());
            buildCityValues[i].second.debug(os);
        }
    }

    bool MilitaryBuildingValue::operator < (const MilitaryBuildingValue& other) const
    {
        if (globalFreeExperience != other.globalFreeExperience)
        {
            return globalFreeExperience > other.globalFreeExperience;
        }

        if (freeExperience != other.freeExperience)
        {
            return freeExperience > other.freeExperience;
        }

        if (!domainFreeExperience.empty() || !other.domainFreeExperience.empty())
        {
            // compare matched entries
            int ourTotal = 0;
            for (std::map<DomainTypes, int>::const_iterator ci(domainFreeExperience.begin()), ciEnd(domainFreeExperience.end()); ci != ciEnd; ++ci)
            {
                ourTotal += ci->second;

                std::map<DomainTypes, int>::const_iterator otherIter = other.domainFreeExperience.find(ci->first);
                if (otherIter != other.domainFreeExperience.end())
                {
                    if (otherIter->second != ci->second)
                    {
                        return ci->second > otherIter->second;
                    }
                }
            }

            // and the other way round
            int theirTotal = 0;
            for (std::map<DomainTypes, int>::const_iterator ci(other.domainFreeExperience.begin()), ciEnd(other.domainFreeExperience.end()); ci != ciEnd; ++ci)
            {
                theirTotal += ci->second;

                std::map<DomainTypes, int>::const_iterator thisIter = domainFreeExperience.find(ci->first);
                if (thisIter != domainFreeExperience.end())
                {
                    if (thisIter->second != ci->second)
                    {
                        return thisIter->second > ci->second;
                    }
                }
            }

            return ourTotal > theirTotal;
        }

        if (!combatTypeFreeExperience.empty() || !other.combatTypeFreeExperience.empty())
        {
            // compare matched entries
            int ourTotal = 0;
            for (std::map<UnitCombatTypes, int>::const_iterator ci(combatTypeFreeExperience.begin()), ciEnd(combatTypeFreeExperience.end()); ci != ciEnd; ++ci)
            {
                ourTotal += ci->second;

                std::map<UnitCombatTypes, int>::const_iterator otherIter = other.combatTypeFreeExperience.find(ci->first);
                if (otherIter != other.combatTypeFreeExperience.end())
                {
                    if (otherIter->second != ci->second)
                    {
                        return ci->second > otherIter->second;
                    }
                }
            }

            // and the other way round
            int theirTotal = 0;
            for (std::map<UnitCombatTypes, int>::const_iterator ci(other.combatTypeFreeExperience.begin()), ciEnd(other.combatTypeFreeExperience.end()); ci != ciEnd; ++ci)
            {
                theirTotal += ci->second;

                std::map<UnitCombatTypes, int>::const_iterator thisIter = combatTypeFreeExperience.find(ci->first);
                if (thisIter != combatTypeFreeExperience.end())
                {
                    if (thisIter->second != ci->second)
                    {
                        return thisIter->second > ci->second;
                    }
                }
            }

            return ourTotal > theirTotal;
        }

        // TODO - use data from unit analysis here
        return freePromotion != NO_PROMOTION && other.freePromotion == NO_PROMOTION;
    }

    void MilitaryBuildingValue::debug(std::ostream& os) const
    {
        os << gGlobals.getBuildingInfo(buildingType).getType() << " turns = " << nTurns;

        if (freeExperience > 0)
        {
            os << " free exp = " << freeExperience;
        }

        if (globalFreeExperience > 0)
        {
            os << " global free exp = " << globalFreeExperience;
        }

        for (std::map<DomainTypes, int>::const_iterator ci(domainFreeExperience.begin()), ciEnd(domainFreeExperience.end()); ci != ciEnd; ++ci)
        {
            os << " " << gGlobals.getDomainInfo(ci->first).getType() << " gets free exp = " << ci->second << " ";
        }

        for (std::map<UnitCombatTypes, int>::const_iterator ci(combatTypeFreeExperience.begin()), ciEnd(combatTypeFreeExperience.end()); ci != ciEnd; ++ci)
        {
            os << " " << gGlobals.getUnitCombatInfo(ci->first).getType() << " gets free exp = " << ci->second << " ";
        }

        if (freePromotion != NO_PROMOTION)
        {
            os << " free promotion = " << gGlobals.getPromotionInfo(freePromotion).getType();
        }
    }

    bool UnitTacticValue::operator < (const UnitTacticValue& other) const
    {
        return unitAnalysisValue / std::max<int>(1, nTurns) > other.unitAnalysisValue / std::max<int>(1, other.nTurns);
    }

    void UnitTacticValue::debug(std::ostream& os) const
    {
        os << gGlobals.getUnitInfo(unitType).getType() << " turns = " << nTurns << " value = " << unitAnalysisValue;
    }

    bool WorkerUnitValue::isReusable() const
    {
        return !buildsMap.empty();
    }

    bool WorkerUnitValue::isConsumed() const
    {
        return !consumedBuildsMap.empty() || !nonCityBuildsMap.empty();
    }

    int WorkerUnitValue::getBuildValue() const
    {
        TotalOutputWeights weights = makeOutputW(4, 3, 2, 2, 1, 1);
        TotalOutputValueFunctor valueF(weights);

        int totalBuildValue = 0;
        for (WorkerUnitValue::BuildsMap::const_iterator ci(buildsMap.begin()), ciEnd(buildsMap.end()); ci != ciEnd; ++ci)
        {
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                totalBuildValue += valueF(boost::get<1>(ci->second[i]));
            }
        }
        return totalBuildValue;
    }

    int WorkerUnitValue::getHighestConsumedBuildValue() const
    {
        TotalOutputWeights weights = makeOutputW(4, 3, 2, 2, 1, 1);
        TotalOutputValueFunctor valueF(weights);

        int highestConsumedBuildValue = 0;
        for (WorkerUnitValue::BuildsMap::const_iterator ci(consumedBuildsMap.begin()), ciEnd(consumedBuildsMap.end()); ci != ciEnd; ++ci)
        {
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                int thisBuildValue = valueF(boost::get<1>(ci->second[i]));
                if (thisBuildValue > highestConsumedBuildValue)
                {
                    highestConsumedBuildValue = thisBuildValue;
                }
            }
        }
        return highestConsumedBuildValue;
    }

    int WorkerUnitValue::getBuildsCount() const
    {
        TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 1, 1);
        TotalOutputValueFunctor valueF(weights);

        int buildCount = 0;
        for (BuildsMap::const_iterator ci(buildsMap.begin()), ciEnd(buildsMap.end()); ci != ciEnd; ++ci)
        {
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                if (valueF(boost::get<1>(ci->second[i])) > 0)
                {
                    ++buildCount;
                }
            }
        }
        for (BuildsMap::const_iterator ci(consumedBuildsMap.begin()), ciEnd(consumedBuildsMap.end()); ci != ciEnd; ++ci)
        {
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                if (valueF(boost::get<1>(ci->second[i])) > 0)
                {
                    ++buildCount;
                }
            }
        }
        return buildCount;
    }

    void WorkerUnitValue::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nWorker unit value data: " << gGlobals.getUnitInfo(unitType).getType() << ", turns = " << nTurns;

        os << "\nBuilds map:";
        debugBuildsMap_(os, buildsMap);
        os << "\nConsumed builds map:";
        debugBuildsMap_(os, consumedBuildsMap);
        os << "\nNon city builds map:";
        debugBuildsMap_(os, nonCityBuildsMap);
#endif
    }

    void WorkerUnitValue::debugBuildsMap_(std::ostream& os, const WorkerUnitValue::BuildsMap& buildsMap_) const
    {
        

        for (BuildsMap::const_iterator ci(buildsMap_.begin()), ciEnd(buildsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nBuild: " << gGlobals.getBuildInfo(ci->first).getType();
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << boost::get<0>(ci->second[i]) << " = " << boost::get<1>(ci->second[i]);
                for (size_t j = 0, techCount = boost::get<2>(ci->second[i]).size(); j < techCount; ++j)
                {
                    if (j == 0) os << " techs = ";
                    else os << ", ";
                    os << gGlobals.getTechInfo(boost::get<2>(ci->second[i])[j]).getType();
                }
            }
        }
    }

    void WorkerUnitValue::addBuild(BuildTypes buildType, WorkerUnitValue::BuildData buildData)
    {
        const bool isConsumed = gGlobals.getBuildInfo(buildType).isKill();

        if (isConsumed)
        {
            addBuild_(consumedBuildsMap, buildType, buildData);
        }
        else
        {
            addBuild_(buildsMap, buildType, buildData);
        }
    }

    void WorkerUnitValue::addNonCityBuild(BuildTypes buildType, WorkerUnitValue::BuildData buildData)
    {
        const bool isConsumed = gGlobals.getBuildInfo(buildType).isKill();
        // only expect to add consumed builds, as other out of city radius improvements can use existing reusable workers
        // if there were only out of city radius builds, might want to rethink this
        if (isConsumed)  
        {
            addBuild_(nonCityBuildsMap, buildType, buildData);
        }
    }

    void WorkerUnitValue::addBuild_(WorkerUnitValue::BuildsMap& buildsMap_, BuildTypes buildType, WorkerUnitValue::BuildData buildData)
    {
        BuildsMap::iterator iter = buildsMap_.find(buildType);
        bool found = false;

        if (iter == buildsMap_.end())
        {
            found = false;  // new build type                
        }
        else
        {
            // do we already have an entry for this build type - no point in replacing it
            // eventually may be worth storing all outputs, and picking the best according to the value functor being used
            for (size_t i = 0, count = iter->second.size(); i < count; ++i)
            {
                if (boost::get<0>(iter->second[i]) == boost::get<0>(buildData))
                {
                    found = true;  
                    break;
                }
            }
        }

        if (!found)
        {
            buildsMap_[buildType].push_back(buildData);
        }
    }

    bool WorkerUnitValue::operator < (const WorkerUnitValue& other) const
    {
        return getBuildValue() + getHighestConsumedBuildValue() < other.getBuildValue() + other.getHighestConsumedBuildValue();
    }

    void SettlerUnitValue::debug(std::ostream& os) const
    {
        os << "\nSettler unit value data: " << gGlobals.getUnitInfo(unitType).getType() << ", turns = " << nTurns;
    }

    TotalOutput TacticSelectionData::getEconomicBuildingOutput(BuildingTypes buildingType, IDInfo city) const
    {
        TotalOutput output;
        if (buildingType == NO_BUILDING)
        {
            return output;
        }

        BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType).getBuildingClassType();
        const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = isNationalWonderClass(buildingClassType);
        if (isWorldWonder)
        {
            std::map<BuildingTypes, EconomicWonderValue>::const_iterator ci = economicWonders.find(buildingType);
            if (ci != economicWonders.end())
            {
                for (size_t i = 0, count = ci->second.buildCityValues.size(); i < count; ++i)
                {
                    if (ci->second.buildCityValues[i].first == city && ci->second.buildCityValues[i].second.buildingType == buildingType)
                    {
                        return ci->second.buildCityValues[i].second.output;
                    }
                }
            }
        }
        else if (isNationalWonder)
        {
            std::map<BuildingTypes, EconomicWonderValue>::const_iterator ci = nationalWonders.find(buildingType);
            if (ci != nationalWonders.end())
            {
                for (size_t i = 0, count = ci->second.buildCityValues.size(); i < count; ++i)
                {
                    if (ci->second.buildCityValues[i].first == city && ci->second.buildCityValues[i].second.buildingType == buildingType)
                    {
                        return ci->second.buildCityValues[i].second.output;
                    }
                }
            }
        }
        else
        {
            for (std::multiset<EconomicBuildingValue>::const_iterator ci(economicBuildings.begin()), ciEnd(economicBuildings.end()); ci != ciEnd; ++ci)
            {
                if (ci->buildingType == buildingType)
                {
                    return ci->output;
                }
            }
        }

        return output;
    }

    void TacticSelectionData::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        if (!smallCultureBuildings.empty())
        {
            os << "\n\tSmall culture buildings: ";
        }
        for (std::multiset<CultureBuildingValue>::const_iterator ci(smallCultureBuildings.begin()), ciEnd(smallCultureBuildings.end());
            ci != ciEnd; ++ci)
        {
            ci->debug(os);
        }

        if (!largeCultureBuildings.empty())
        {
            os << "\n\tLarge culture buildings: ";
        }
        for (std::multiset<CultureBuildingValue>::const_iterator ci(largeCultureBuildings.begin()), ciEnd(largeCultureBuildings.end());
            ci != ciEnd; ++ci)
        {
            ci->debug(os);
        }

        if (!economicBuildings.empty())
        {
            os << "\n\tEconomic buildings: ";
        }
        for (std::multiset<EconomicBuildingValue>::const_iterator ci(economicBuildings.begin()), ciEnd(economicBuildings.end());
            ci != ciEnd; ++ci)
        {
            ci->debug(os);
        }

        for (std::map<IDInfo, std::vector<BuildingTypes> >::const_iterator ci(buildingsCityCanAssistWith.begin()), ciEnd(buildingsCityCanAssistWith.end());
            ci != ciEnd; ++ci)
        {
            os << narrow(getCity(ci->first)->getName()) << " can assist with: ";
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << gGlobals.getBuildingInfo(ci->second[i]).getType();
            }
        }

        for (std::map<BuildingTypes, std::vector<BuildingTypes> >::const_iterator ci(dependentBuildings.begin()), ciEnd(dependentBuildings.end());
            ci != ciEnd; ++ci)
        {
            os << gGlobals.getBuildingInfo(ci->first).getType() << " depends on: ";
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << gGlobals.getBuildingInfo(ci->second[i]).getType();
            }
        }

        if (!economicWonders.empty())
        {
            os << "\n\tGlobal wonders: ";
        }
        for (std::map<BuildingTypes, EconomicWonderValue>::const_iterator ci(economicWonders.begin()), ciEnd(economicWonders.end());
            ci != ciEnd; ++ci)
        {
            ci->second.debug(os);
        }

        if (!nationalWonders.empty())
        {
            os << "\n\tNational wonders: ";
        }
        for (std::map<BuildingTypes, EconomicWonderValue>::const_iterator ci(nationalWonders.begin()), ciEnd(nationalWonders.end());
            ci != ciEnd; ++ci)
        {
            ci->second.debug(os);
        }

        if (!militaryBuildings.empty())
        {
            os << "\n\tMilitary buildings: ";
        }
        for (std::set<MilitaryBuildingValue>::const_iterator ci(militaryBuildings.begin()), ciEnd(militaryBuildings.end());
            ci != ciEnd; ++ci)
        {
            ci->debug(os);
        }

        if (!cityDefenceUnits.empty())
        {
            os << "\n\tCity defence units: ";
        }
        for (std::set<UnitTacticValue>::const_iterator ci(cityDefenceUnits.begin()), ciEnd(cityDefenceUnits.end());
            ci != ciEnd; ++ci)
        {
            ci->debug(os);
        }

        if (!cityAttackUnits.empty())
        {
            os << "\n\tCity attack units: ";
        }
        for (std::set<UnitTacticValue>::const_iterator ci(cityAttackUnits.begin()), ciEnd(cityAttackUnits.end());
            ci != ciEnd; ++ci)
        {
            ci->debug(os);
        }

        if (!workerUnits.empty())
        {
            os << "\n\tWorker units: ";
        }
        for (std::map<UnitTypes, WorkerUnitValue>::const_iterator ci(workerUnits.begin()), ciEnd(workerUnits.end());
            ci != ciEnd; ++ci)
        {
            ci->second.debug(os);
        }

        if (!settlerUnits.empty())
        {
            os << "\n\tSettler units: ";
        }
        for (std::map<UnitTypes, SettlerUnitValue>::const_iterator ci(settlerUnits.begin()), ciEnd(settlerUnits.end());
            ci != ciEnd; ++ci)
        {
            ci->second.debug(os);
        }

        if (!exclusions.empty())
        {
            os << "\n\tExcluded buildings: ";
        }
        for (std::set<BuildingTypes>::const_iterator ci(exclusions.begin()), ciEnd(exclusions.end()); ci != ciEnd; ++ci)
        {
            os << gGlobals.getBuildingInfo(*ci).getType() << ", ";
        }
       
        os << "\nCity Improvements Delta = " << cityImprovementsDelta;

        for (std::map<ProcessTypes, TotalOutput>::const_iterator ci(processOutputsMap.begin()), ciEnd(processOutputsMap.end());
            ci != ciEnd; ++ci)
        {
            os << "\nProcess: " << gGlobals.getProcessInfo(ci->first).getType() << " output = " << ci->second;
        }

        if (getFreeTech)
        {
            os << "\nGives free tech, value = " << freeTechValue;
        }
#endif
    }
}