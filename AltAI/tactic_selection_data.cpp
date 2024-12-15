#include "AltAI.h"

#include "./tactic_selection_data.h"
#include "./city_improvement_projections.h"
#include "./error_log.h"
#include "./civ_log.h"
#include "./city_data.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        bool areLandBuilds(const WorkerUnitValue::BuildsMap& buildsMap)
        {
            const CvMap& theMap = gGlobals.getMap();
            for (WorkerUnitValue::BuildsMap::const_iterator ci(buildsMap.begin()), ciEnd(buildsMap.end()); ci != ciEnd; ++ci)
            {
                if (!ci->second.empty())
                {
                    const CvPlot* pPlot = theMap.plot(boost::get<0>(ci->second[0]).iX, boost::get<0>(ci->second[0]).iY);
                    return !pPlot->isWater();
                }
            }

            return false;
        }

        std::vector<IDInfo> getTargetCities(const WorkerUnitValue::BuildsMap& buildsMap)
        {
            std::set<IDInfo> targetCitiesSet;
            for (WorkerUnitValue::BuildsMap::const_iterator ci(buildsMap.begin()), ciEnd(buildsMap.end()); ci != ciEnd; ++ci)
            {
                for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                {
                    targetCitiesSet.insert(boost::get<1>(ci->second[i]));
                }
            }
            return std::vector<IDInfo>(targetCitiesSet.begin(), targetCitiesSet.end());
        }

        struct UnitCityPred
        {
            bool operator() (const UnitTacticValue* p1, const UnitTacticValue* p2) const
            {
                if (p1->unitType == p2->unitType)
                {
                    return p1->city < p2->city;
                }
                else
                {
                    return p1->unitType < p2->unitType;
                }
            }
        };

        struct CityBuildTimePred
        {
            bool operator() (const std::pair<IDInfo, int>& firstCityAndBuildTime, const std::pair<IDInfo, int>& secondCityAndBuildTime) const
            {
                return firstCityAndBuildTime.second < secondCityAndBuildTime.second;
            }
        };

        void debugUnitTacticSet(const std::set<UnitTacticValue>& unitTacticValues, std::ostream& os, const std::string& prefix)
        {
            if (!unitTacticValues.empty())
            {
                os << "\n\t" << prefix;

                std::list<const UnitTacticValue*> unitTacticsList;
            
                for (std::set<UnitTacticValue>::const_iterator ci(unitTacticValues.begin()), ciEnd(unitTacticValues.end()); ci != ciEnd; ++ci)
                {
                    unitTacticsList.push_back(&*ci);
                }

                unitTacticsList.sort(UnitCityPred());

                UnitTypes prevUnitType = NO_UNIT;

                for (std::list<const UnitTacticValue*>::const_iterator li(unitTacticsList.begin()), liEnd(unitTacticsList.end()); li != liEnd; ++li)
                {
                    if ((*li)->unitType != prevUnitType)
                    {
                        prevUnitType = (*li)->unitType;
                        os << "\n\t\t" << gGlobals.getUnitInfo((*li)->unitType).getType();
                    }
                    else
                    {
                        os << ",";
                    }
                    (*li)->debug(os);
                }
            }
        }

        /*template <typename TacticValueT>
            struct IsCityTactic
        {
            explicit IsCityTactic(IDInfo city_) : city(city_) {}

            bool operator() (const TacticValueT& tacticValue) const
            {
                return tacticValue.city == city;
            }

            IDInfo city;
        };*/

        template <typename TacticValueContainer>
            void eraseCityTacticValues(TacticValueContainer& container, IDInfo city)
            {
                for (TacticValueContainer::iterator iter(container.begin()); iter != container.end();)
                {
                    if (iter->city == city)
                    {
                        container.erase(iter++);
                    }
                    else
                    {
                        ++iter;
                    }
                }
            }

        template <typename TacticValueContainer>
            void eraseCityBuildingTacticValues(TacticValueContainer& container, IDInfo city, BuildingTypes buildingType)
            {
                // inefficient, although these are v. small sets of values and buildings are not constructed that frequently
                // (same argument applies with above erase city version)
                // todo - make the TacticValue containers lists as we want flexible sorting anyway
                for (TacticValueContainer::iterator iter(container.begin()); iter != container.end();)
                {
                    if (iter->city == city && iter->buildingType == buildingType)
                    {
                        container.erase(iter++);  // building + city is unique
                        break;
                    }
                    else
                    {
                        ++iter;
                    }
                }
            }

        void eraseCityUnitTacticValues(std::set<UnitTacticValue>& tacticValues, IDInfo city, UnitTypes unitType)
        {
            for (std::set<UnitTacticValue>::iterator iter(tacticValues.begin()); iter != tacticValues.end();)
            {
                if (iter->city == city && iter->unitType == unitType)
                {
                    tacticValues.erase(iter++);  // unit + city is unique
                }
                else
                {
                    ++iter;
                }
            }
        }

        template <typename T> 
            typename T::value_type getBuildingValue(const T& container, BuildingTypes buildingType)
        {
            for (T::const_iterator ci(container.begin()), ciEnd(container.end()); ci != ciEnd; ++ci)
            {
                if (ci->buildingType == buildingType)
                {
                    return *ci;
                }
            }
            return T::value_type();
        }
    }

    void TacticSelectionData::merge(const TacticSelectionData& other)
    {
        smallCultureBuildings.insert(other.smallCultureBuildings.begin(), other.smallCultureBuildings.end());
        largeCultureBuildings.insert(other.largeCultureBuildings.begin(), other.largeCultureBuildings.end());
        economicBuildings.insert(other.economicBuildings.begin(), other.economicBuildings.end());
        settledSpecialists.insert(other.settledSpecialists.begin(), other.settledSpecialists.end());
        buildingsCityCanAssistWith.insert(other.buildingsCityCanAssistWith.begin(), other.buildingsCityCanAssistWith.end());
        // buildingsCityCanAssistWith
        // dependentBuildings
        // economicWonders, nationalWonders
        militaryBuildings.insert(other.militaryBuildings.begin(), other.militaryBuildings.end());

        cityDefenceUnits.insert(other.cityDefenceUnits.begin(), other.cityDefenceUnits.end());
        thisCityDefenceUnits.insert(other.thisCityDefenceUnits.begin(), other.thisCityDefenceUnits.end());
        cityAttackUnits.insert(other.cityAttackUnits.begin(), other.cityAttackUnits.end());
        collateralUnits.insert(other.collateralUnits.begin(), other.collateralUnits.end());
        scoutUnits.insert(other.scoutUnits.begin(), other.scoutUnits.end());

        // workerUnits, settlerUnits, connectableResources

        std::copy(other.cultureSources.begin(), other.cultureSources.end(), std::back_inserter(cultureSources));
        // exclusions
        cityImprovementsDelta += other.cityImprovementsDelta;

        // free techs from buildings
        possibleFreeTechs.insert(other.possibleFreeTechs.begin(), other.possibleFreeTechs.end());
        // free techs from tech or Great People (tactic selection data is stored by tech or city - hence multiple buildings map)
        // still not ideal and doesn't express GP dependency well - todo re-think
        if (possibleFreeTech == NO_TECH)
        {
            possibleFreeTech = other.possibleFreeTech;
        }
        resourceOutput += other.resourceOutput;
        // processOutputsMap
    }

    MilitaryBuildingValue TacticSelectionData::getMilitaryBuildingValue(BuildingTypes buildingType) const
    {
        return getBuildingValue(militaryBuildings, buildingType);
    }

    EconomicBuildingValue TacticSelectionData::getEconomicBuildingValue(BuildingTypes buildingType) const
    {
        return getBuildingValue(economicBuildings, buildingType);
    }

    //template <typename T> 
    //    typename T::value_type TacticSelectionData::getUnitValue(const T& container, UnitTypes unitType)
    //{
    //    for (T::const_iterator ci(container.begin()), ciEnd(container.end()); ci != ciEnd; ++ci)
    //    {
    //        if (ci->unitType == unitType)
    //        {
    //            return *ci;
    //        }
    //    }
    //    return T::value_type();
    //}

    UnitTacticValue TacticSelectionData::getUnitValue(const std::set<UnitTacticValue>& unitTacticValues, UnitTypes unitType)
    {
        for (std::set<UnitTacticValue>::const_iterator ci(unitTacticValues.begin()), ciEnd(unitTacticValues.end()); ci != ciEnd; ++ci)
        {
            if (ci->unitType == unitType)
            {
                return *ci;
            }
        }
        return UnitTacticValue();
    }

    bool CultureBuildingValue::operator < (const CultureBuildingValue& other) const
    {
        if (city == other.city)
        {
            return buildingType < other.buildingType;
        }
        else
        {
            return city < other.city;
        }
    }

    bool CultureBuildingTacticValueComp::operator () (const CultureBuildingValue& first, const CultureBuildingValue& second) const
    {
        TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 20, 1);
        TotalOutputValueFunctor valueF(weights);

        return valueF(first.output) / std::max<int>(1, first.nTurns) > valueF(second.output) / std::max<int>(1, second.nTurns);
    }

    void CultureBuildingValue::debug(std::ostream& os) const
    {
        os << gGlobals.getBuildingInfo(buildingType).getType() << " city = " << safeGetCityName(city) << " turns = " << nTurns << ", output = " << output << " ";
    }

    bool EconomicBuildingValue::operator < (const EconomicBuildingValue& other) const
    {
        if (city == other.city)
        {
            return buildingType < other.buildingType;
        }
        else
        {
            return city < other.city;
        }
    }

    bool EconomicBuildingTacticValueComp::operator () (const EconomicBuildingValue* first, const EconomicBuildingValue* second) const
    {
        //TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 1, 1);
        //TotalOutputValueFunctor valueF(weights);

        //return valueF(first.output) / std::max<int>(1, first.nTurns) > valueF(second.output) / std::max<int>(1, second.nTurns);

        return first->getScaledEconomicValue(numSimTurns, timeHorizon, weights) > second->getScaledEconomicValue(numSimTurns, timeHorizon, weights);
    }

    bool EconomicBuildingValue::hasPositiveEconomicValue(const int numSimTurns, TotalOutputWeights outputWeights) const
    {
        return TacticSelectionData::hasEconomicValue(numSimTurns - nTurns, output, outputWeights);
    }

    int EconomicBuildingValue::getEconomicValue(const int numSimTurns, TotalOutputWeights outputWeights) const
    {
        return TacticSelectionData::getEconomicValue(numSimTurns - nTurns, output, outputWeights);
    }

    int EconomicBuildingValue::getScaledEconomicValue(const int numSimTurns, const int numHorizonTurns, TotalOutputWeights outputWeights) const
    {
        // e.g. sim for 30t, building takes 10t to construct, and make horizon = 50t
        // 30t - 10t is to determine period for which sim has building effective
        return TacticSelectionData::getScaledEconomicValue(numSimTurns, nTurns, numHorizonTurns, output, outputWeights);
    }

    void EconomicBuildingValue::debug(std::ostream& os) const
    {
        os << gGlobals.getBuildingInfo(buildingType).getType() << " city = " << safeGetCityName(city) << " turns = " << nTurns << ", output = " << output << " ";
    }

    void EconomicWonderValue::debug(std::ostream& os) const
    {
        for (size_t i = 0, count = buildCityValues.size(); i < count; ++i)
        {
            os << "\n\t" << " city = " << safeGetCityName(buildCityValues[i].first) << " ";
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

        // treat walls, etc... as secondary to promotions (passive v. active)
        if (cityDefence != other.cityDefence || globalCityDefence != other.globalCityDefence || bombardDefence != other.bombardDefence)
        {
            const int numCities = CvPlayerAI::getPlayer(city.eOwner).getNumCities();
            return cityDefence + numCities * globalCityDefence + bombardDefence > other.cityDefence + numCities * other.globalCityDefence + other.bombardDefence;
        }

        // TODO - use data from unit analysis here
        return freePromotion != NO_PROMOTION && other.freePromotion == NO_PROMOTION;
    }

    void MilitaryBuildingValue::debug(std::ostream& os) const
    {
        os << "\n\t" << gGlobals.getBuildingInfo(buildingType).getType() << " city = " << safeGetCityName(city) << " turns = " << nTurns;

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

        debugUnitTacticSet(thisCityDefenceUnits, os, "This City defence units:");
        debugUnitTacticSet(cityDefenceUnits, os, "City defence units:");        
        debugUnitTacticSet(cityAttackUnits, os, "City attack units:");        
        os << "\n";
    }

    void CivicValue::debug(std::ostream& os) const
    {
        os << " civic: " << (civicType == NO_CIVIC ? " none " : gGlobals.getCivicInfo(civicType).getType())
            << " delta = " << outputDelta << ", cost = " << cost;

        if (!isEmpty(yieldOutputDelta.first))
        {
            os << "\n\tPossible civic yield output delta: " << " delta = " << yieldOutputDelta.first << ", %age = " << yieldOutputDelta.second;
        }

        os << " Possible output % incr = " << outputRateDelta;
    }

    bool UnitTacticValue::operator < (const UnitTacticValue& other) const
    {
        if (city == other.city)
        {
            return unitType < other.unitType;
        }
        else
        {
            return city < other.city;
        }
    }

    bool UnitTacticValueComp::operator () (const UnitTacticValue& first, const UnitTacticValue& second) const
    {
        return first.unitAnalysisValue / std::max<int>(1, first.nTurns) > second.unitAnalysisValue / std::max<int>(1, second.nTurns);
    }

    bool UnitTacticTypeLevelComp::operator () (const UnitTacticValue& first, const UnitTacticValue& second) const
    {
        if (first.unitType == second.unitType)
        {
            return first.level < second.level;
        }
        else
        {
            return first.unitType < second.unitType;
        }
    }    

    void ReligionUnitRequestData::debug(std::ostream& os) const
    {
        os << (unitType != NO_UNIT ? gGlobals.getUnitInfo(unitType).getType() : " no unit") << ' '
           << (depBuildingType != NO_BUILDING ? gGlobals.getBuildingInfo(depBuildingType).getType() : " no building") << ' '
           << safeGetCityName(bestCity) << " build time: " << bestCityBuildTime;
    }

    void UnitTacticValue::debug(std::ostream& os) const
    {
        os << " city = " << safeGetCityName(city) << " level = " << level << " turns = " << nTurns << " value = " << unitAnalysisValue;
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
                totalBuildValue += valueF(boost::get<2>(ci->second[i]));
            }
        }
        return totalBuildValue;
    }

    int WorkerUnitValue::getHighestConsumedBuildValue(const std::vector<int>& accessibleSubAreas) const
    {
        TotalOutputWeights weights = makeOutputW(4, 3, 2, 2, 1, 1);
        TotalOutputValueFunctor valueF(weights);
        const CvMap& theMap = gGlobals.getMap();

        int highestConsumedBuildValue = 0;
        for (WorkerUnitValue::BuildsMap::const_iterator ci(consumedBuildsMap.begin()), ciEnd(consumedBuildsMap.end()); ci != ciEnd; ++ci)
        {
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                XYCoords thisBuildCoords = boost::get<0>(ci->second[i]);
                int subArea = theMap.plot(thisBuildCoords)->getSubArea();
                if (std::find(accessibleSubAreas.begin(), accessibleSubAreas.end(), subArea) == accessibleSubAreas.end())
                {
                    continue;
                }

                int thisBuildValue = valueF(boost::get<2>(ci->second[i]));
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
                if (valueF(boost::get<2>(ci->second[i])) > 0)
                {
                    ++buildCount;
                }
            }
        }
        for (BuildsMap::const_iterator ci(consumedBuildsMap.begin()), ciEnd(consumedBuildsMap.end()); ci != ciEnd; ++ci)
        {
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                if (valueF(boost::get<2>(ci->second[i])) > 0)
                {
                    ++buildCount;
                }
            }
        }
        return buildCount;
    }

    IUnitEventGeneratorPtr WorkerUnitValue::getUnitEventGenerator() const
    {
        if (isConsumed())
        {
            return IUnitEventGeneratorPtr(new BuildImprovementsUnitEventGenerator(areLandBuilds(consumedBuildsMap), true, getTargetCities(consumedBuildsMap)));
        }
        else
        {
            if (!buildsMap.empty())
            {
                return IUnitEventGeneratorPtr(new BuildImprovementsUnitEventGenerator(areLandBuilds(buildsMap), false, getTargetCities(buildsMap)));
            }
            else
            {
                return IUnitEventGeneratorPtr(new BuildImprovementsUnitEventGenerator(areLandBuilds(nonCityBuildsMap), true, getTargetCities(nonCityBuildsMap)));
            }
        }
    }

    void WorkerUnitValue::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nWorker unit value data: " << gGlobals.getUnitInfo(unitType).getType() << ", turns = " << nTurns << ", lost output = " << lostOutput;

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
                os << boost::get<0>(ci->second[i]) << " = " << boost::get<2>(ci->second[i]);

                const CvCity* pTargetCity = ::getCity(boost::get<1>(ci->second[i]));
                os << " target city = " << (pTargetCity ? narrow(pTargetCity->getName()) : "NONE");

                for (size_t j = 0, techCount = boost::get<3>(ci->second[i]).size(); j < techCount; ++j)
                {
                    if (j == 0) os << " techs = ";
                    else os << ", ";
                    os << gGlobals.getTechInfo(boost::get<3>(ci->second[i])[j]).getType();
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
        return getBuildValue() + getHighestConsumedBuildValue(accessibleSubAreas) < other.getBuildValue() + other.getHighestConsumedBuildValue(other.accessibleSubAreas);
    }

    BuildImprovementsUnitEventGenerator::BuildImprovementsUnitEventGenerator(bool isLand, bool isConsumed, const std::vector<IDInfo>& targetCities)
        : isLand_(isLand), isConsumed_(isConsumed), targetCities_(targetCities)
    {
    }

    IProjectionEventPtr BuildImprovementsUnitEventGenerator::getProjectionEvent(const CityDataPtr& pCityData)
    {
        IProjectionEventPtr pEvent = IProjectionEventPtr(new ProjectionImprovementEvent(isLand_, isConsumed_, targetCities_, pCityData->getOwner()));
        //ErrorLog::getLog(CvPlayerAI::getPlayer(pCityData->getOwner()))->getStream() << "\new ProjectionImprovementEvent at: " << pEvent.get();
        return pEvent;
    }

    void SettlerUnitValue::debug(std::ostream& os) const
    {
        os << "\nSettler unit value data: " << gGlobals.getUnitInfo(unitType).getType() << ", turns = " << nTurns << ", lost output = " << lostOutput;
    }

    CultureSourceValue::CultureSourceValue(const CvBuildingInfo& buildingInfo)
    {
        cityCost = buildingInfo.getProductionCost();
        cityValue = buildingInfo.getCommerceChangeArray()[COMMERCE_CULTURE];
        globalValue = buildingInfo.getObsoleteSafeCommerceChangeArray()[COMMERCE_CULTURE];
    }

    void CultureSourceValue::debug(std::ostream& os) const
    {
        os << "\n\tCulture source data: city cost = " << cityCost << ", city base culture = "
            << cityValue << ", global city culture = " << globalValue;
    }

    bool SettledSpecialistValue::operator < (const SettledSpecialistValue& other) const
    {
        TotalOutputWeights weights = makeOutputW(3, 4, 3, 3, 1, 1);
        TotalOutputValueFunctor valueF(weights);

        return valueF(output) > valueF(other.output);
    }

    void SettledSpecialistValue::debug(std::ostream& os) const
    {
        os << "\n\tSettled specialist: type = " 
            << (specType == NO_SPECIALIST ? " none " : gGlobals.getSpecialistInfo(specType).getType())
            << " city: " << (city == IDInfo() ? " none " : safeGetCityName(city))
            << ", output = " << output;
    }

    std::vector<std::pair<IDInfo, int> > TacticSelectionData::getCityBuildTimes(BuildingTypes buildingType) const
    {
        std::vector<std::pair<IDInfo, int> > cityBuildTimes;
        for (std::set<EconomicBuildingValue>::const_iterator buildingsIter(economicBuildings.begin()), buildingsEndIter(economicBuildings.end()); 
            buildingsIter != buildingsEndIter; ++buildingsIter)
        {
            if (buildingsIter->buildingType == buildingType)
            {
                cityBuildTimes.push_back(std::make_pair(buildingsIter->city, buildingsIter->nTurns));
            }
        }

        std::sort(cityBuildTimes.begin(), cityBuildTimes.end(), CityBuildTimePred());

        return cityBuildTimes;
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
            for (std::set<EconomicBuildingValue>::const_iterator ci(economicBuildings.begin()), ciEnd(economicBuildings.end()); ci != ciEnd; ++ci)
            {
                if (ci->buildingType == buildingType)
                {
                    return ci->output;
                }
            }
        }

        return output;
    }

    std::pair<int, int> TacticSelectionData::calculateBestAndTotalEconomicBuildingValues(const int numSimTurns, const int timeHorizon) const
    {
        TotalOutputWeights unitWeights = makeOutputW(1, 1, 1, 1, 0, 0);
        int totalEconomicValue = 0;
        int bestEconomicScaledValue = 0;
        for (std::set<EconomicBuildingValue>::const_iterator econBuildingsIter(economicBuildings.begin()), econBuildingsEndIter(economicBuildings.end());
            econBuildingsIter != econBuildingsEndIter; ++econBuildingsIter)
        {
            if (econBuildingsIter->hasPositiveEconomicValue(numSimTurns, unitWeights))
            {
                int unitValue = econBuildingsIter->getEconomicValue(numSimTurns, unitWeights);
                int scaledValue = econBuildingsIter->getScaledEconomicValue(numSimTurns, timeHorizon, unitWeights);
                bestEconomicScaledValue = std::max<int>(scaledValue, bestEconomicScaledValue);

                totalEconomicValue += unitValue;
            }
        }
        return std::make_pair(bestEconomicScaledValue, totalEconomicValue);
    }

    std::pair<int, TotalOutput> TacticSelectionData::calculateReligionSpreadValue(ReligionTypes religionType, IDInfo city, const int numSimTurns, const int timeHorizon) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(city.eOwner))->getStream();
#endif
        TotalOutputWeights unitWeights = makeOutputW(1, 1, 1, 1, 0, 0);
        TotalOutput baseReligionCityOutput;
        int scaledValue = 0;

        std::map<ReligionTypes, std::map<IDInfo, std::pair<TotalOutput, TotalOutput> > >::const_iterator baseReligionDiffIter = potentialReligionOutputDeltas.find(religionType);
        if (baseReligionDiffIter != potentialReligionOutputDeltas.end())
        {
            std::map<IDInfo, std::pair<TotalOutput, TotalOutput> >::const_iterator baseReligionDiffCityIter = baseReligionDiffIter->second.find(city);
            if (baseReligionDiffCityIter != baseReligionDiffIter->second.end())
            {
                TotalOutput religionOutputDiff = baseReligionDiffCityIter->second.first - baseReligionDiffCityIter->second.second;
                baseReligionCityOutput = baseReligionDiffCityIter->second.first;

                if (hasEconomicValue(numSimTurns, religionOutputDiff, unitWeights))
                {
                    int unitValue = getEconomicValue(numSimTurns, religionOutputDiff, unitWeights);
                    scaledValue = getScaledEconomicValue(numSimTurns, 0, timeHorizon, religionOutputDiff, unitWeights);
                    //cityReligionsValueMap[depReligion] = scaledValue;
#ifdef ALTAI_DEBUG
                    os << "\n\tadded base delta for religion: " << gGlobals.getReligionInfo(religionType).getType() << " = "
                        << religionOutputDiff << " unitValue: " << unitValue << ", scaled: " << scaledValue;
#endif
                }
                else
                {
#ifdef ALTAI_DEBUG
                    os << "\n\t minimal unit value for religion: " << gGlobals.getReligionInfo(religionType).getType() << " = "
                        << getEconomicValue(numSimTurns, religionOutputDiff, unitWeights);
#endif
                }
            }
        }
        return std::make_pair(scaledValue, baseReligionCityOutput);
    }

    std::pair<BuildingTypes, int> TacticSelectionData::calculateBestAndTotalEconomicReligionBuildingValues(IDInfo city, TotalOutput cityBaseReligionOutputDiff, const int numSimTurns, const int timeHorizon) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(city.eOwner))->getStream();
#endif
        TotalOutputWeights unitWeights = makeOutputW(1, 1, 1, 1, 0, 0);
        int cityReligionsBestBuildingValue = 0;
        BuildingTypes bestReligionBuildingType = NO_BUILDING;

        for (std::set<EconomicBuildingValue>::const_iterator religionBuildingsIter(economicBuildings.begin()), religionBuildingsEndIter(economicBuildings.end());
            religionBuildingsIter != religionBuildingsEndIter; ++religionBuildingsIter)
        {
            if (religionBuildingsIter->city == city)
            {
                int numTurnsActive = numSimTurns - religionBuildingsIter->nTurns;
                int unitValue = getEconomicValue(numTurnsActive, religionBuildingsIter->output - cityBaseReligionOutputDiff, unitWeights);
                int scaledValue = getScaledEconomicValue(numSimTurns, religionBuildingsIter->nTurns, timeHorizon, religionBuildingsIter->output - cityBaseReligionOutputDiff, unitWeights);

                if (hasEconomicValue(numTurnsActive, religionBuildingsIter->output - cityBaseReligionOutputDiff, unitWeights))
                {
                    if (scaledValue > cityReligionsBestBuildingValue)
                    {
                        cityReligionsBestBuildingValue = scaledValue;
                        bestReligionBuildingType = religionBuildingsIter->buildingType;
                    }
                    
#ifdef ALTAI_DEBUG
                    os << "\n\tchecked base delta for religion dependent building: " << gGlobals.getBuildingInfo(religionBuildingsIter->buildingType).getType() << " = "
                        << " t = " << religionBuildingsIter->nTurns << " unitValue: " << unitValue << ", scaled value: " << scaledValue;
#endif
                }
                else
                {
#ifdef ALTAI_DEBUG
                    os << "\n\tskipped religion dependent building: " << gGlobals.getBuildingInfo(religionBuildingsIter->buildingType).getType()
                        << " t = " << religionBuildingsIter->nTurns << " unitValue: " << unitValue << ", scaled value: " << scaledValue;
#endif
                }
            }
        }

        return std::make_pair(bestReligionBuildingType, cityReligionsBestBuildingValue);
    }

    void TacticSelectionData::eraseCityEntries(IDInfo city)
    {
        eraseCityTacticValues(smallCultureBuildings, city);
        eraseCityTacticValues(largeCultureBuildings, city);
        eraseCityTacticValues(economicBuildings, city);
        eraseCityTacticValues(militaryBuildings, city);
        eraseCityTacticValues(cityDefenceUnits, city);
        eraseCityTacticValues(thisCityDefenceUnits, city);
        eraseCityTacticValues(cityAttackUnits, city);
        eraseCityTacticValues(fieldAttackUnits, city);
        eraseCityTacticValues(fieldDefenceUnits, city);
        eraseCityTacticValues(collateralUnits, city);
        eraseCityTacticValues(scoutUnits, city);
        eraseCityTacticValues(seaCombatUnits, city);
    }

    void TacticSelectionData::eraseCityBuildingEntries(IDInfo city, BuildingTypes buildingType)
    {
        eraseCityBuildingTacticValues(smallCultureBuildings, city, buildingType);
        eraseCityBuildingTacticValues(largeCultureBuildings, city, buildingType);
        eraseCityBuildingTacticValues(economicBuildings, city, buildingType);
        eraseCityBuildingTacticValues(militaryBuildings, city, buildingType);
    }

    void TacticSelectionData::eraseCityUnitEntries(IDInfo city, UnitTypes unitType)
    {
        eraseCityUnitTacticValues(cityDefenceUnits, city, unitType);
        eraseCityUnitTacticValues(thisCityDefenceUnits, city, unitType);
        eraseCityUnitTacticValues(cityAttackUnits, city, unitType);
        eraseCityUnitTacticValues(fieldAttackUnits, city, unitType);
        eraseCityUnitTacticValues(fieldDefenceUnits, city, unitType);
        eraseCityUnitTacticValues(collateralUnits, city, unitType);
        eraseCityUnitTacticValues(scoutUnits, city, unitType);
        eraseCityUnitTacticValues(seaCombatUnits, city, unitType);        
    }

    bool TacticSelectionData::isSignificantTacticItem(const EconomicBuildingValue& tacticItemValue, TotalOutput currentOutput, const int numTurns, const std::vector<OutputTypes>& outputTypes)
    {
        bool isSignificant = false;
        for (int i = 0, count = outputTypes.size(); i < count; ++i)
        {
            if (tacticItemValue.output[outputTypes[i]] > currentOutput[outputTypes[i]] * numTurns / 50) // > 2% current total
            {
                isSignificant = true;
                break;
            }
        }
        return isSignificant;
    }

    bool TacticSelectionData::hasEconomicValue(const int numTurns, TotalOutput outputChange, TotalOutputWeights outputWeights)
    {
        return getEconomicValue(numTurns, outputChange, outputWeights) > 100;
    }

    // numTurns is the number of turns the output delta applied for
    int TacticSelectionData::getEconomicValue(const int numTurns, TotalOutput outputChange, TotalOutputWeights outputWeights)
    {
        if (numTurns == 0)
        {
            return 0;
        }
        else
        {
            TotalOutput perTurnOutput = outputChange / numTurns;
            return TotalOutputValueFunctor(outputWeights)(perTurnOutput);
        }
    }

    int TacticSelectionData::getScaledEconomicValue(const int numSimTurns, const int numBuildTurns, const int numHorizonTurns, TotalOutput outputChange, TotalOutputWeights outputWeights)
    {
        int unitValue = getEconomicValue(numSimTurns - numBuildTurns, outputChange, outputWeights);
        return ((numHorizonTurns - numBuildTurns) * unitValue) / 100;
    }

    int TacticSelectionData::getUnitTacticValue(const std::set<UnitTacticValue>& unitValues)
    {
        int value = 0;
        for (std::set<UnitTacticValue>::const_iterator ci(unitValues.begin()), ciEnd(unitValues.end()); ci != ciEnd; ++ci)
        {
            value += ci->unitAnalysisValue;
        }
        return value;
    }

    std::set<UnitTacticValue>::const_iterator TacticSelectionData::getBestUnitTactic(const std::set<UnitTacticValue>& unitValues)
    {
        std::set<UnitTacticValue>::const_iterator mIter(unitValues.begin());
        for (std::set<UnitTacticValue>::const_iterator ci(++mIter), ciEnd(unitValues.end()); ci != ciEnd; ++ci)
        {
            if (ci->unitAnalysisValue > mIter->unitAnalysisValue)
            {
                mIter = ci;
            }
        }
        return mIter;
    }

    /*std::list<std::pair<IDInfo, size_t> > TacticSelectionData::getCityBuildTimes(const std::set<UnitTacticValue>& unitValues, UnitTypes unitType)
    {
        std::list<std::pair<IDInfo, size_t> > buildTimes;
        for (std::set<UnitTacticValue>::const_iterator ci(unitValues.begin()), ciEnd(unitValues.end()); ci != ciEnd; ++ci)
        {
            if (ci->unitType == unitType && ci->nTurns >= 0)
            {
                buildTimes.push_back(std::make_pair(ci->city, ci->nTurns));
            }
        }
        return buildTimes;
    }*/

    std::list<std::pair<UnitTypes, int> > TacticSelectionData::getUnitValueDiffs(const std::set<UnitTacticValue>& tacticUnitValues, const std::set<UnitTacticValue>& refUnitValues)
    {
        std::list<std::pair<UnitTypes, int> > unitValueDiffs;

        for (std::set<UnitTacticValue>::const_iterator ui(tacticUnitValues.begin()), uiEnd(tacticUnitValues.end()); ui != uiEnd; ++ui)
        {
            std::set<UnitTacticValue>::const_iterator baseIter = refUnitValues.find(*ui);
            if (baseIter != refUnitValues.end())
            {
                unitValueDiffs.push_back(std::make_pair(baseIter->unitType, asPercentageOf(ui->unitAnalysisValue - baseIter->unitAnalysisValue, baseIter->unitAnalysisValue)));
            }
        }

        return unitValueDiffs;
    }

    void TacticSelectionData::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        if (!potentialReligionOutputDeltas.empty())
        {
            for (std::map<ReligionTypes, std::map<IDInfo, std::pair<TotalOutput, TotalOutput> > >::const_iterator rIter(potentialReligionOutputDeltas.begin()), rEndIter(potentialReligionOutputDeltas.end());
                rIter != rEndIter; ++rIter)
            {
                os << "\n\t potential religion city adoption deltas: " << gGlobals.getReligionInfo(rIter->first).getType();
                for (std::map<IDInfo, std::pair<TotalOutput, TotalOutput> >::const_iterator cIter(rIter->second.begin()), cEndIter(rIter->second.end()); cIter != cEndIter; ++cIter)
                {
                    os << "\n\t " << safeGetCityName(cIter->first) << ": delta = " << cIter->second.first << ", base = " << cIter->second.second
                       << ", diff = " << cIter->second.first - cIter->second.second;
                }
            }
        }

        if (!dependentBuilds.empty())
        {
            os << "\ndep build items: ";
            for (std::set<BuildQueueItem>::const_iterator bIter(dependentBuilds.begin()), bEndIter(dependentBuilds.end()); bIter != bEndIter; ++bIter)
            {
                debugBuildItem(*bIter, os);
            }
        }

        if (!smallCultureBuildings.empty())
        {
            os << "\n\tSmall culture buildings: ";
        }
        for (std::set<CultureBuildingValue>::const_iterator ci(smallCultureBuildings.begin()), ciEnd(smallCultureBuildings.end());
            ci != ciEnd; ++ci)
        {
            os << "\n\t";
            ci->debug(os);
        }

        if (!largeCultureBuildings.empty())
        {
            os << "\n\tLarge culture buildings: ";
        }
        for (std::set<CultureBuildingValue>::const_iterator ci(largeCultureBuildings.begin()), ciEnd(largeCultureBuildings.end());
            ci != ciEnd; ++ci)
        {
            os << "\n\t";
            ci->debug(os);
        }

        if (!economicBuildings.empty())
        {
            os << "\n\tEconomic buildings: ";
        }
        for (std::set<EconomicBuildingValue>::const_iterator ci(economicBuildings.begin()), ciEnd(economicBuildings.end());
            ci != ciEnd; ++ci)
        {
            os << "\n\t";
            ci->debug(os);
        }

        if (!spreadReligionUnits.empty())
        {
            os << "\n\tSpread religion Units: ";
            for (std::map<UnitTypes, ReligionUnitValue>::const_iterator rIter(spreadReligionUnits.begin()), rEndIter(spreadReligionUnits.end()); rIter != rEndIter; ++rIter)
            {
                os << "\n\t" << gGlobals.getUnitInfo(rIter->first).getType() << " t = " << rIter->second.nTurns;
            }
        }

        if (!settledSpecialists.empty())
        {
            os << "\n\tSettled specialists: ";
        }
        for (std::multiset<SettledSpecialistValue>::const_iterator ci(settledSpecialists.begin()), ciEnd(settledSpecialists.end());
            ci != ciEnd; ++ci)
        {
            ci->debug(os);
        }

        for (std::map<BuildingTypes, std::set<BuildingTypes> >::const_iterator ci(buildingsCityCanAssistWith.begin()), ciEnd(buildingsCityCanAssistWith.end());
            ci != ciEnd; ++ci)
        {
            os << "\n\t can build: " << gGlobals.getBuildingInfo(ci->first).getType() << " to assist with: ";
            for (std::set<BuildingTypes>::const_iterator bIter(ci->second.begin()), bEndIter(ci->second.end()); bIter != bEndIter; ++bIter)
            {
                if (bIter != ci->second.begin()) os << ", ";
                os << gGlobals.getBuildingInfo(*bIter).getType();
            }
        }

        for (std::map<BuildingTypes, std::set<BuildingTypes> >::const_iterator ci(dependentBuildings.begin()), ciEnd(dependentBuildings.end());
            ci != ciEnd; ++ci)
        {
            os << "\n\t can build: " << gGlobals.getBuildingInfo(ci->first).getType() << " which is depended on by: ";
            for (std::set<BuildingTypes>::const_iterator bIter(ci->second.begin()), bEndIter(ci->second.end()); bIter != bEndIter; ++bIter)
            {
                if (bIter != ci->second.begin()) os << ", ";
                os << gGlobals.getBuildingInfo(*bIter).getType();
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

        if (!civicValues.empty())
        {
            os << "\n\tCivics: ";
        }
        for (std::list<CivicValue>::const_iterator ci(civicValues.begin()), ciEnd(civicValues.end()); ci != ciEnd; ++ci)
        {
            ci->debug(os);
        }

        debugUnitTacticSet(cityDefenceUnits, os, "City defence units:");
        debugUnitTacticSet(thisCityDefenceUnits, os, "This City defence units:");
        debugUnitTacticSet(cityAttackUnits, os, "City attack units:");
        debugUnitTacticSet(fieldAttackUnits, os, "Field attack units:");
        debugUnitTacticSet(fieldDefenceUnits, os, "Field defence units:");
        debugUnitTacticSet(collateralUnits, os, "CollateralUnits units:");
        debugUnitTacticSet(seaCombatUnits, os, "Sea combat units:");
        debugUnitTacticSet(scoutUnits, os, "Scout units:");

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

        if (!potentialResourceOutputDeltas.empty())
        {
            os << "\n\tPotential resources: ";
        }
        for (std::map<BonusTypes, std::pair<TotalOutput, TotalOutput> >::const_iterator ci(potentialResourceOutputDeltas.begin()), ciEnd(potentialResourceOutputDeltas.end());
            ci != ciEnd; ++ci)
        {
            os << "\n\t " << gGlobals.getBonusInfo(ci->first).getType() << " base = " << ci->second.first << ", delta = " << ci->second.second;
        }

        /*if (!potentialAcceleratedBuildings.empty())
        {
            os << "\n\tPotentially accelerated buildings: ";
        }
        for (std::map<BonusTypes, std::vector<BuildingTypes> >::const_iterator ci(potentialAcceleratedBuildings.begin()), ciEnd(potentialAcceleratedBuildings.end());
            ci != ciEnd; ++ci)
        {
            os << "\n\t for resource: " << gGlobals.getBonusInfo(ci->first).getType() << " - buildings: ";
            for (size_t i = 0, count = ci->second.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                if (!(i % 10)) os << "\n\t";
                os << gGlobals.getBuildingInfo(ci->second[i]).getType();
            }
        }*/

        for (size_t i = 0, count = cultureSources.size(); i < count; ++i)
        {
            cultureSources[i].debug(os);
        }

        if (!exclusions.empty())
        {
            os << "\n\tExcluded buildings: ";
        }
        for (std::set<BuildingTypes>::const_iterator ci(exclusions.begin()), ciEnd(exclusions.end()); ci != ciEnd; ++ci)
        {
            os << gGlobals.getBuildingInfo(*ci).getType() << ", ";
        }

        if (!enabledUnits.empty())
        {
            os << "\n\tEnabled units: ";
        }
        for (std::set<UnitTypes>::const_iterator ci(enabledUnits.begin()), ciEnd(enabledUnits.end()); ci != ciEnd; ++ci)
        {
            os << gGlobals.getUnitInfo(*ci).getType() << ", ";
        }
       
        if (!isEmpty(cityImprovementsDelta))
        {
            os << "\n\tCity Improvements Delta = " << cityImprovementsDelta;
        }

        for (std::map<ProcessTypes, TotalOutput>::const_iterator ci(processOutputsMap.begin()), ciEnd(processOutputsMap.end());
            ci != ciEnd; ++ci)
        {
            os << "\n\tProcess: " << gGlobals.getProcessInfo(ci->first).getType() << " output = " << ci->second;
        }

        for (std::map<BuildingTypes, TechTypes>::const_iterator ci(possibleFreeTechs.begin()), ciEnd(possibleFreeTechs.end());
            ci != ciEnd; ++ci)
        {
            os << "\n\tBuilding: " << gGlobals.getBuildingInfo(ci->first).getType() << " gives free tech, possible choice = " << gGlobals.getTechInfo(ci->second).getType(); 
        }

        if (possibleFreeTech != NO_TECH)
        {
            os << "\n\tGives free tech, possible choice = " << gGlobals.getTechInfo(possibleFreeTech).getType();
        }

        if (!isEmpty(baselineDelta))
        {
            os << "\n\tBaseline delta output = " << baselineDelta;
        }

        if (!isEmpty(resourceOutput))
        {
            os << "\n\tResource output = " << resourceOutput;
        }
        os << "\n tactic selection data end.";
#endif
    }
}