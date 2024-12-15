#include "AltAI.h"

#include "./religion_tactics.h"
#include "./player_analysis.h"
#include "./building_tactics_deps.h"
#include "./city_unit_tactics.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./city_data.h"
#include "./modifiers_helper.h"
#include "./religion_helper.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./civ_log.h"

namespace AltAI
{
    namespace
    {
        struct IsCityMatcher
        {
            explicit IsCityMatcher(const IDInfo city_) : city(city_) {}
            bool operator() (const std::pair<IDInfo, int>& cityAndBuildTime) const
            {
                return cityAndBuildTime.first == city;
            }
            const IDInfo city;
        };
    }
    ReligionAnalysis::ReligionAnalysis(Player& player) : player_(player)
    {
    }

    void ReligionAnalysis::init()
    {
        CityIter cityIter(*player_.getCvPlayer());

        while (CvCity* pLoopCity = cityIter())
        {
            addCity(pLoopCity);
        }
    }

    void ReligionAnalysis::addCity(const CvCity* pCity)
    {
        for (int religionIndex = 0, religionCount = gGlobals.getNumReligionInfos(); religionIndex < religionCount; ++religionIndex)
        {
            if (pCity->isHasReligion((ReligionTypes)religionIndex))
            {
                addReligion(pCity, (ReligionTypes)religionIndex, true);
            }
        }
    }

    void ReligionAnalysis::addReligion(const CvCity* pCity, ReligionTypes religionType, bool newValue)
    {    
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
        if (newValue)
        {
            ourReligions_.insert(religionType);
            cityReligions_[pCity->getIDInfo()].insert(religionType);
#ifdef ALTAI_DEBUG
            os << "\n added religion: " << gGlobals.getReligionInfo(religionType).getType() << " for city: " << safeGetCityName(pCity);
#endif
        }
        else
        {
            cityReligions_[pCity->getIDInfo()].erase(religionType);
#ifdef ALTAI_DEBUG
            os << "\n removed religion: " << gGlobals.getReligionInfo(religionType).getType() << " for city: " << safeGetCityName(pCity);
#endif

            int thisReligionCityCount = 0;
            for (std::map<IDInfo, std::set<ReligionTypes> >::iterator cityReligionsIter(cityReligions_.begin()), cityReligionsEndIter(cityReligions_.end()); 
                cityReligionsIter != cityReligionsEndIter; ++cityReligionsIter)
            {
                if (cityReligionsIter->second.find(religionType) != cityReligionsIter->second.end())
                {
                    ++thisReligionCityCount;
                    break;
                }
            }

            if (thisReligionCityCount == 0)
            {
#ifdef ALTAI_DEBUG
                os << "\n removed religion for civ: " << gGlobals.getReligionInfo(religionType).getType();
#endif
                ourReligions_.erase(religionType);
            }
        }
    }

    void ReligionAnalysis::update()
    {
        for (std::set<ReligionTypes>::const_iterator rIter(ourReligions_.begin()), rEndIter(ourReligions_.end()); rIter != rEndIter; ++rIter)
        {
            PlayerTactics::ReligionTacticsMap::const_iterator religionTacticIter = player_.getAnalysis()->getPlayerTactics()->religionTacticsMap_.find(*rIter);
            if (religionTacticIter != player_.getAnalysis()->getPlayerTactics()->religionTacticsMap_.end())
            {
                religionTacticIter->second->update(player_);
            }
        }
    }

    void ReligionAnalysis::update(City& city)
    {
        for (std::set<ReligionTypes>::const_iterator rIter(ourReligions_.begin()), rEndIter(ourReligions_.end()); rIter != rEndIter; ++rIter)
        {
            PlayerTactics::ReligionTacticsMap::const_iterator religionTacticIter = player_.getAnalysis()->getPlayerTactics()->religionTacticsMap_.find(*rIter);
            if (religionTacticIter != player_.getAnalysis()->getPlayerTactics()->religionTacticsMap_.end())
            {
                religionTacticIter->second->update(player_, city);
            }
        }
    }

    void ReligionAnalysis::apply(TacticSelectionData& selectionData)
    {
        for (std::set<ReligionTypes>::const_iterator rIter(ourReligions_.begin()), rEndIter(ourReligions_.end()); rIter != rEndIter; ++rIter)
        {
            PlayerTactics::ReligionTacticsMap::const_iterator religionTacticIter = player_.getAnalysis()->getPlayerTactics()->religionTacticsMap_.find(*rIter);
            if (religionTacticIter != player_.getAnalysis()->getPlayerTactics()->religionTacticsMap_.end())
            {
                religionTacticIter->second->apply(selectionData);
            }
        }
    }

    void ReligionAnalysis::apply(TacticSelectionDataMap& selectionDataMap, int depTacticFlags)
    {
        for (std::set<ReligionTypes>::const_iterator rIter(ourReligions_.begin()), rEndIter(ourReligions_.end()); rIter != rEndIter; ++rIter)
        {
            PlayerTactics::ReligionTacticsMap::const_iterator religionTacticIter = player_.getAnalysis()->getPlayerTactics()->religionTacticsMap_.find(*rIter);
            if (religionTacticIter != player_.getAnalysis()->getPlayerTactics()->religionTacticsMap_.end())
            {
                religionTacticIter->second->apply(selectionDataMap, depTacticFlags);
            }
        }
    }
   
    ReligionUnitRequestData ReligionAnalysis::getUnitRequestBuild(const CvCity* pCity, PlayerTactics& playerTactics)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\n" << __FUNCTION__ << " for city: " << safeGetCityName(pCity);
        debugRequestsMap(os);
#endif
        ReligionUnitRequestData religionUnitRequestData;
        
		std::map<IDInfo, std::set<ReligionTypes> >::const_iterator cityReligionsIter = cityReligions_.find(pCity->getIDInfo());
        // have at least one religion present in this city...
		if (cityReligionsIter != cityReligions_.end() && !cityReligionsIter->second.empty())
		{
            int bestUnitTrainValue = 0, bestDepBuildingValue = 0;
            ReligionUnitRequestData bestUnitTrainReq, bestDepBuildingReq;
            // loop over city requests map
            // find best value request for a religion we have present
            // track best value for case where we need to build the dep building first and when we can train the requested unit immediately
			for (std::map<IDInfo, std::map<ReligionTypes, std::pair<int, ReligionUnitRequestData> > >::const_iterator reqIter(religionRequestsMap_.begin()), reqEndIter(religionRequestsMap_.end());
				reqIter != reqEndIter; ++reqIter)
			{
                if (pCity->getIDInfo() == reqIter->first)
                {
                    continue;  // skip trying to satisfy this city's own requests
                }

				for (std::map<ReligionTypes, std::pair<int, ReligionUnitRequestData> >::const_iterator cityReqIter(reqIter->second.begin()), cityReqEndIter(reqIter->second.end());
					cityReqIter != cityReqEndIter; ++cityReqIter)
				{
					// do we have this religion in this city?
					if (cityReligionsIter->second.find(cityReqIter->first) != cityReligionsIter->second.end())
					{
                        const ReligionUnitRequestData& thisRequestData = cityReqIter->second.second;
#ifdef ALTAI_DEBUG
                        os << "\n\t checking request: ";
                        thisRequestData.debug(os);
#endif
                        if (cityReqIter->second.first > bestUnitTrainValue || cityReqIter->second.first > bestDepBuildingValue)
                        {
                            // can we train the requested unit?
                            UnitTacticsPtr pUnitTactics = playerTactics.getUnitTactics(thisRequestData.unitType);
                            if (pUnitTactics->areTechDependenciesSatisfied(player_))
                            {
                                CityUnitTacticsPtr pCityUnitTactics = pUnitTactics->getCityTactics(pCity->getIDInfo());
                                if (pCityUnitTactics)
                                {
                                    if (pCityUnitTactics->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                                    {
#ifdef ALTAI_DEBUG
                                        os << "\n\t checking " << gGlobals.getReligionInfo(cityReqIter->first).getType() << " = " << cityReqIter->second.first;
#endif
                                        if (cityReqIter->second.first > bestUnitTrainValue)
                                        {
                                            bestUnitTrainValue = cityReqIter->second.first;
                                            bestUnitTrainReq = thisRequestData;
#ifdef ALTAI_DEBUG
                                            os << "\n\t set new bestUnitTrainValue = " << bestUnitTrainValue << ' ';
                                            bestUnitTrainReq.debug(os);
#endif
                                        }
                                    }
                                    else if (cityReqIter->second.first > bestDepBuildingValue) 
                                    {
#ifdef ALTAI_DEBUG
                                        os << " checking dep building value: " << cityReqIter->second.first << ' ';
                                        
#endif
                                        const TacticSelectionData& selectionData = playerTactics.getBaseTacticSelectionData();
                                        std::vector<std::pair<IDInfo, int> > cityBuildTimes = selectionData.getCityBuildTimes(thisRequestData.depBuildingType);
#ifdef ALTAI_DEBUG
                                        for (size_t i = 0, count = cityBuildTimes.size(); i < count; ++i)
                                        {
                                            if (i > 0) os << ", ";
                                            else os << " ";
                                            os << safeGetCityName(cityBuildTimes[i].first) << " = " << cityBuildTimes[i].second;
                                        }
#endif
                                        std::vector<std::pair<IDInfo, int> >::const_iterator buildIter = std::find_if(cityBuildTimes.begin(), cityBuildTimes.end(), IsCityMatcher(pCity->getIDInfo()));
                                        // city can build and build time is not more than twice shortest build time
                                        if (buildIter != cityBuildTimes.end() && buildIter->second <= 2 * cityBuildTimes[0].second)
                                        {
                                            bestDepBuildingValue = cityReqIter->second.first;
                                            bestDepBuildingReq = thisRequestData;
#ifdef ALTAI_DEBUG
                                            os << "\n\t set new bestDepBuildingValue ";
                                            bestDepBuildingReq.debug(os);
#endif
                                        }                                        
                                    }
                                }
                            }
                        }
					}
				}

			}

            if (bestUnitTrainValue > 0)
            {
                religionUnitRequestData = bestUnitTrainReq;
#ifdef ALTAI_DEBUG
                os << "\n\t set final unit train request: ";
                religionUnitRequestData.debug(os);
#endif
            }
            else if (bestDepBuildingValue > 0)
            {
                religionUnitRequestData = bestDepBuildingReq;
#ifdef ALTAI_DEBUG
                os << "\n\t set final unit dep building request: ";
                religionUnitRequestData.debug(os);
#endif
            }
		}

        return religionUnitRequestData;
    }

    void ReligionAnalysis::setUnitRequestBuild(const CvCity* pCity, PlayerTactics& playerTactics)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\n" << __FUNCTION__ << " for city: " << safeGetCityName(pCity);
        debugRequestsMap(os);
#endif
        const int numSimTurns = playerTactics.player.getAnalysis()->getNumSimTurns();
        const int timeHorizon = playerTactics.player.getAnalysis()->getTimeHorizon();
        const TacticSelectionDataMap& globalSelectionDataMap = playerTactics.tacticSelectionDataMap;
        const TacticSelectionData& baseGlobalSelectionData = playerTactics.getBaseTacticSelectionData();  // globalSelectionDataMap[nodeps]
        const TacticSelectionData& cityPlayerTactics = playerTactics.getCityTacticSelectionData(pCity->getIDInfo());

        religionRequestsMap_[pCity->getIDInfo()].clear();

        // if best value of religion based buildings is greater than best value of economic buildings
        // or best value of spreading religion is better than best economic building
        // set dep build item as request for this city

        TotalOutput baseCityOutput = playerTactics.player.getCity(pCity->getID()).getBaseOutputProjection().getOutput();
        TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 1, 1), unitWeights = makeOutputW(1, 1, 1, 1, 0, 0);
        TotalOutputValueFunctor valueF(weights);
        // get best economic value:
        int totalEconomicValue = 0;
        int bestEconomicScaledValue = 0;
        boost::tie(bestEconomicScaledValue, totalEconomicValue) = cityPlayerTactics.calculateBestAndTotalEconomicBuildingValues(numSimTurns, timeHorizon);

        std::map<ReligionTypes, int> cityReligionsValueMap, cityReligionsBestBuildingValueMap;
        std::map<ReligionTypes, TotalOutput> cityBaseReligionValueMap;
        std::map<ReligionTypes, std::pair<UnitTypes, BuildingTypes> > religionDepBuildItemsMap;

        IsBuildItemType isUnitBuildItemType(UnitItem);
        for (std::set<ReligionTypes>::const_iterator rIter(ourReligions_.begin()), rEndIter(ourReligions_.end()); rIter != rEndIter; ++rIter)
        {
            ReligionTypes depReligion = *rIter;
            DependencyItemSet depItem;
            depItem.insert(std::make_pair(ReligiousDependency::ID, depReligion));
            TacticSelectionDataMap::const_iterator tacticMapIter(globalSelectionDataMap.find(depItem));
            if (tacticMapIter != globalSelectionDataMap.end())
            {
#ifdef ALTAI_DEBUG
                os << "\n";
                debugDepItemSet(depItem, os);
                tacticMapIter->second.debug(os);
#endif
                UnitTypes spreadUnitType = playerTactics.religionTacticsMap_[depReligion]->getSpreadUnitType();
                BuildingTypes spreadBuildingType = playerTactics.religionTacticsMap_[depReligion]->getSpreadBuildingType();
                if (spreadUnitType != NO_UNIT || spreadBuildingType != NO_BUILDING)
                {
                    religionDepBuildItemsMap[depReligion] = std::make_pair(spreadUnitType, spreadBuildingType);
                }

                // find output change for this city if it acquires this religion
                int thisReligionScaledValue = 0;
                TotalOutput baseReligionCityOutput;
                boost::tie(thisReligionScaledValue, baseReligionCityOutput) = baseGlobalSelectionData.calculateReligionSpreadValue(depReligion, pCity->getIDInfo(), numSimTurns, timeHorizon);
                cityBaseReligionValueMap[depReligion] = baseReligionCityOutput;
                if (thisReligionScaledValue > 0)
                {
                    cityReligionsValueMap[depReligion] = thisReligionScaledValue;
                }

                // get best building dependent on this religion
                BuildingTypes bestReligionBuilding = NO_BUILDING;
                int bestReligionBuildingValue = 0;
                boost::tie(bestReligionBuilding, bestReligionBuildingValue) = tacticMapIter->second.calculateBestAndTotalEconomicReligionBuildingValues(
                    pCity->getIDInfo(), cityBaseReligionValueMap[depReligion] - baseCityOutput, numSimTurns, timeHorizon);

                if (bestReligionBuildingValue > 0)
                {
                    cityReligionsBestBuildingValueMap[depReligion] = bestReligionBuildingValue;
                }
            }
        }

#ifdef ALTAI_DEBUG
        os << "\n\t base religion spread values: ";
        for (std::map<ReligionTypes, TotalOutput>::const_iterator rIter(cityBaseReligionValueMap.begin()), rEndIter(cityBaseReligionValueMap.end()); rIter != rEndIter; ++rIter)
        {
            os << "\n\t " << gGlobals.getReligionInfo(rIter->first).getType() << " = " << rIter->second << " % of total: " << asPercentageOf(rIter->second, baseCityOutput);
        }

        os << "\n\t total economic building value: " << totalEconomicValue << ", bestEconomicScaledValue: " << bestEconomicScaledValue;

        for (std::map<ReligionTypes, int>::const_iterator rIter(cityReligionsValueMap.begin()), rEndIter(cityReligionsValueMap.end()); rIter != rEndIter; ++rIter)
        {
            os << "\n\t " << gGlobals.getReligionInfo(rIter->first).getType() << ", city spread value = " << rIter->second;
        }
#endif            

        ReligionUnitRequestData unitRequestData;
        ReligionTypes bestSpreadReligion = NO_RELIGION, bestBuildingReligion = NO_RELIGION;
        int bestReligionScaledValue = 0;
        int bestReligionBuildingScaledValue = 0;
        // see if just spreading the religion alone is worth it
        for (std::map<ReligionTypes, int>::const_iterator rIter(cityReligionsValueMap.begin()), rEndIter(cityReligionsValueMap.end()); rIter != rEndIter; ++rIter)
        {
            if (rIter->second > bestEconomicScaledValue)
            {
                if (rIter->second > bestReligionScaledValue)
                {
                    bestReligionScaledValue = rIter->second;
                    unitRequestData.unitType = religionDepBuildItemsMap[rIter->first].first;
                    unitRequestData.depBuildingType = religionDepBuildItemsMap[rIter->first].second;
                    bestSpreadReligion = rIter->first;
                    religionRequestsMap_[pCity->getIDInfo()][rIter->first] = std::make_pair(bestReligionScaledValue, unitRequestData);
#ifdef ALTAI_DEBUG
                    os << "\n\t best religion value: " << bestReligionScaledValue
                        << " for religion: " << gGlobals.getReligionInfo(rIter->first).getType();
#endif
                }
            }
        }

        for (std::map<ReligionTypes, int>::const_iterator rIter(cityReligionsBestBuildingValueMap.begin()), rEndIter(cityReligionsBestBuildingValueMap.end()); rIter != rEndIter; ++rIter)
        {
            if (rIter->second > bestEconomicScaledValue && rIter->second > bestReligionScaledValue)
            {
                if (rIter->second > bestReligionBuildingScaledValue)
                {
                    bestReligionBuildingScaledValue = rIter->second;
                    unitRequestData.unitType = religionDepBuildItemsMap[rIter->first].first;
                    unitRequestData.depBuildingType = religionDepBuildItemsMap[rIter->first].second;
                    bestBuildingReligion = rIter->first;
#ifdef ALTAI_DEBUG
                    os << "\n\t best religion building value: " << bestReligionBuildingScaledValue
                        << " for religion: " << gGlobals.getReligionInfo(rIter->first).getType();
#endif
                }
            }
        }

        if (!isEmpty(unitRequestData.unitType) || !isEmpty(unitRequestData.depBuildingType)) // selected a missionary unit to request
        {
            UnitTacticsPtr pUnitTactics = playerTactics.getUnitTactics(unitRequestData.unitType);
            // have a valid tactic for this unit
            if (pUnitTactics)
            {
                if (!pUnitTactics->areTechDependenciesSatisfied(playerTactics.player))
                {
#ifdef ALTAI_DEBUG
                    os << "\n\t failed tech dep check for unit: " << gGlobals.getUnitInfo(unitRequestData.unitType).getType();
#endif
                    unitRequestData.unitType = NO_UNIT;
                    unitRequestData.depBuildingType = NO_BUILDING;
                }
                else
                {
                    // see which cities can train it
                    CityIter cityIter(*playerTactics.player.getCvPlayer());
                    int bestBuildTime = MAX_INT;
                    IDInfo bestCity;
                    while (CvCity* pLoopCity = cityIter())
                    {
                        const TacticSelectionData& citySelectionData = playerTactics.getCityTacticSelectionData(pLoopCity->getIDInfo());
                        std::map<UnitTypes, ReligionUnitValue>::const_iterator unitIter = citySelectionData.spreadReligionUnits.find(unitRequestData.unitType);
                        if (unitIter != citySelectionData.spreadReligionUnits.end())
                        {
                            if (unitIter->second.nTurns < bestBuildTime)
                            {
                                bestBuildTime = unitIter->second.nTurns;
                                bestCity = pLoopCity->getIDInfo();
                            }
#ifdef ALTAI_DEBUG
                            os << "\n\t (tactic data) city: " << safeGetCityName(pLoopCity) << " can build unit: " << gGlobals.getUnitInfo(unitRequestData.unitType).getType()
                                << " in: " << unitIter->second.nTurns << " turns";
#endif
                        }
                        else if (!isEmpty(unitRequestData.depBuildingType))
                        {
                            // can't train unit - check depBuilding
                            // no need to make this generic to any dep type as xml structure only allows for a building dep
                            ICityBuildingTacticsPtr pCityDepBuildingTactic = playerTactics.getCityBuildingTactics(pLoopCity->getIDInfo(), unitRequestData.depBuildingType);
                            if (pCityDepBuildingTactic)
                            {
#ifdef ALTAI_DEBUG
                                os << "\n dep building tactic: ";
                                pCityDepBuildingTactic->debug(os);
#endif
                            }
                        }
                    }

                    if (!isEmpty(bestCity))
                    {
#ifdef ALTAI_DEBUG
                        os << " best city: " << safeGetCityName(bestCity) << " " << bestBuildTime << "t";
#endif
                        unitRequestData.bestCity = bestCity;
                        unitRequestData.bestCityBuildTime = bestBuildTime;
                    }
                }

                if (unitRequestData.unitType != NO_UNIT)
                {
                    ReligionTypes requestedReligion = bestSpreadReligion;
                    if (bestReligionBuildingScaledValue > bestReligionScaledValue)
                    {
                        requestedReligion = bestBuildingReligion;
                    }
                    religionRequestsMap_[pCity->getIDInfo()][requestedReligion] = std::make_pair(std::max<int>(bestReligionScaledValue, bestReligionBuildingScaledValue), unitRequestData);
#ifdef ALTAI_DEBUG
                    os << "\n\t best religion value: " << std::max<int>(bestReligionScaledValue, bestReligionBuildingScaledValue)
                        << " - setting unit request data for religion: " << gGlobals.getReligionInfo(requestedReligion).getType()
                        << " to: " << gGlobals.getUnitInfo(unitRequestData.unitType).getType();
#endif

                }
            }
        }
    }

    void ReligionAnalysis::updateMissionaryMission(CvUnitAI* pUnit)
    {
        const CvUnitInfo& unitInfo = pUnit->getUnitInfo();
        ReligionTypes spreadReligion = NO_RELIGION;
        for (int religionIndex = 0, religionCount = gGlobals.getNumReligionInfos(); religionIndex < religionCount; ++religionIndex)
        {
            if (unitInfo.getReligionSpreads(religionIndex) > 0)
            {
                spreadReligion = (ReligionTypes)religionIndex;
                break;
            }
        }

        if (spreadReligion == NO_RELIGION)
        {
            FAssertMsg(spreadReligion != NO_RELIGION, "trying to spread religion with invalid unit?");
            return;
        }

        const CvCity* pTargetCity = NULL;
        std::map<IDInfo, IDInfo>::iterator missionIter = missionaryMissionMap_.find(pUnit->getIDInfo());

        if (missionIter != missionaryMissionMap_.end())
        {
            pTargetCity = ::getCity(missionIter->second);
            // city could have been lost or captured since we set the request
            // also, city may have acquired target religion in the interim
            // either way - need a new target
            if (!pTargetCity || pTargetCity->isHasReligion(spreadReligion))  
            {
                missionaryMissionMap_.erase(missionIter);
                missionIter = missionaryMissionMap_.end();
            }
        }

        // find a mission target
        if (missionIter == missionaryMissionMap_.end())
        {            
            // try and highest value unassigned request for unit's religion type            
            int highestSpreadValue = 0;
            IDInfo bestSpreadTargetCity;
            for (std::map<IDInfo, std::map<ReligionTypes, std::pair<int, ReligionUnitRequestData> > >::const_iterator requestsIter(religionRequestsMap_.begin()), requestsEndIter(religionRequestsMap_.end());
                requestsIter != requestsEndIter; ++requestsIter)
            {
                const CvCity* pPossibleTargetCity = ::getCity(requestsIter->first);
                if (pPossibleTargetCity && !pPossibleTargetCity->isHasReligion(spreadReligion))  // religion not already present in city
                {
                    if (!cityHasRequest_(requestsIter->first, spreadReligion))  // no existing request to spread this religion to this city
                    {
                        std::map<ReligionTypes, std::pair<int, ReligionUnitRequestData> >::const_iterator cityRequestIter = requestsIter->second.find(spreadReligion);
                        if (cityRequestIter != requestsIter->second.end())
                        {
                            //find highest value target that isn't already in missionaryMissionMap_
                            if (cityRequestIter->second.first > highestSpreadValue)
                            {
                                highestSpreadValue = cityRequestIter->second.first;
                                bestSpreadTargetCity = requestsIter->first;
                            }
                        }
                    }
                }
            }

            if (isEmpty(bestSpreadTargetCity))
            {
                // find largest city which doesn't have this religion
                CityIter cityIter(*player_.getCvPlayer());
                int maxPopulation = 0;

                while (CvCity* pLoopCity = cityIter())
                {
                    if (!pLoopCity->isHasReligion(spreadReligion))
                    {
                        if (pLoopCity->getPopulation() > maxPopulation)
                        {
                            maxPopulation = pLoopCity->getPopulation();
                            bestSpreadTargetCity = pLoopCity->getIDInfo();
                        }
                    }
                }
            }
            // todo: if not set a mission consider whether we can spread this religion to another civ
            // needs open borders and not in theocracy

            if (!isEmpty(bestSpreadTargetCity))
            {
                missionIter = missionaryMissionMap_.insert(std::make_pair(pUnit->getIDInfo(), bestSpreadTargetCity)).first;
                pTargetCity = ::getCity(bestSpreadTargetCity);
            }
        }

        if (missionIter != missionaryMissionMap_.end())
        {
            // try and find path to target city if not already there
            if (pTargetCity)
            {
                if (!pUnit->atPlot(pTargetCity->plot()))
                {
                    UnitPathData unitPathData;
                    unitPathData.calculate(pUnit->getGroup(), pTargetCity->plot(), MOVE_MAX_MOVES);
                    if (unitPathData.valid)
                    {
                        const CvPlot* pMoveToPlot = getNextMovePlot(player_, pUnit->getGroup(), pTargetCity->plot());
                        if (pMoveToPlot && !pUnit->at(pMoveToPlot->getX(), pMoveToPlot->getY()))
                        {
                            pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMoveToPlot->getX(), pMoveToPlot->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_SPREAD, (CvPlot*)pMoveToPlot, 0, __FUNCTION__);
                        }
                    }
                }
                else
                {
                    pUnit->getGroup()->pushMission(MISSION_SPREAD, spreadReligion, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                }
            }
        }        
    }

    std::set<DependencyItemSet, DependencyItemsComp> ReligionAnalysis::getReligionDeps() const
    {
        std::set<DependencyItemSet, DependencyItemsComp> depItems;
        for (std::set<ReligionTypes>::const_iterator rIter(ourReligions_.begin()), rEndIter(ourReligions_.end()); rIter != rEndIter; ++rIter)
        {
            DependencyItemSet depItem;
            depItem.insert(std::make_pair(ReligiousDependency::ID, *rIter));
            depItems.insert(depItem);
        }
        return depItems;
    }

    bool ReligionAnalysis::cityHasRequest_(IDInfo city, ReligionTypes religionType) const
    {
        for (std::map<IDInfo, IDInfo>::const_iterator missionsIter(missionaryMissionMap_.begin()), missionsEndIter(missionaryMissionMap_.end());
            missionsIter != missionsEndIter; ++missionsIter)
        {
            if (missionsIter->second == city)
            {
                const CvUnit* pUnit = ::getUnit(missionsIter->first);
                if (pUnit && pUnit->getUnitInfo().getReligionType() == religionType)
                {
                    return true;
                }
            }
        }
        return false;
    }

    void ReligionAnalysis::debugRequestsMap(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\t requests map: \n\t";
        for (std::map<IDInfo, std::map<ReligionTypes, std::pair<int, ReligionUnitRequestData> > >::const_iterator reqIter(religionRequestsMap_.begin()), reqEndIter(religionRequestsMap_.end());
            reqIter != reqEndIter; ++reqIter)
        {
            os << safeGetCityName(reqIter->first);
            for (std::map<ReligionTypes, std::pair<int, ReligionUnitRequestData> >::const_iterator cityReqIter(reqIter->second.begin()), cityReqEndIter(reqIter->second.end());
                cityReqIter != cityReqEndIter; ++cityReqIter)
            {
                os << " " << gGlobals.getReligionInfo(cityReqIter->first).getType() << ", value = " << cityReqIter->second.first << ' ';
                cityReqIter->second.second.debug(os);
            }
            os << "\n\t";
        }
#endif
    }


    ReligionTactics::ReligionTactics(PlayerTypes playerType, ReligionTypes religionType) : playerType_(playerType), religionType_(religionType), lastTurnCalculated_(-1)
    {
        for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
        {
            UnitTypes unitType = getPlayerVersion(playerType, (UnitClassTypes)i);
            if (unitType != NO_UNIT)
            {
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);
                if (unitInfo.getReligionSpreads(religionType) > 0)
                {
                    spreadUnitType_ = unitType;  // todo - support multiple units
                    BuildingTypes preReqBuilding = (BuildingTypes)unitInfo.getPrereqBuilding();
                    if (preReqBuilding != NO_BUILDING)
                    {
                        spreadBuildingType_ = preReqBuilding;
                    }
                    break;
                }
            }
        }
    }

    void ReligionTactics::addTactic(const IReligionTacticPtr& pReligionTactic)
    {
        religionTactics_.push_back(pReligionTactic);
    }

    void ReligionTactics::setTechDependency(const ResearchTechDependencyPtr& pTechDependency)
    {
        techDependency_ = pTechDependency;
    }

    void ReligionTactics::update(Player& player)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nUpdating religion tactic for: " << gGlobals.getReligionInfo(religionType_).getType() << " turn = " << gGlobals.getGame().getGameTurn();
        /*CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {   
            os << "\n\nbase city output: " << narrow(pCity->getName()) << ' ';
            player.getCity(pCity->getID()).getBaseOutputProjection().debug(os);
        }
        os << '\n';*/
#endif
        for (std::list<IReligionTacticPtr>::const_iterator tacticIter(religionTactics_.begin()), tacticEndIter(religionTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->update(shared_from_this(), player);
        }

        lastTurnCalculated_ = gGlobals.getGame().getGameTurn();
    }

    void ReligionTactics::update(Player& player, City& city)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nUpdating religion tactic for: " << gGlobals.getReligionInfo(religionType_).getType() << " turn = " << gGlobals.getGame().getGameTurn()
            << " and city: " << safeGetCityName(city.getCvCity()) ;
        /*CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {   
            os << "\n\nbase city output: " << narrow(pCity->getName()) << ' ';
            player.getCity(pCity->getID()).getBaseOutputProjection().debug(os);
        }
        os << '\n';*/
#endif
        for (std::list<IReligionTacticPtr>::const_iterator tacticIter(religionTactics_.begin()), tacticEndIter(religionTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->update(shared_from_this(), city);
        }

        lastTurnCalculated_ = gGlobals.getGame().getGameTurn();
    }

    void ReligionTactics::updateDependencies(const Player& player)
    {
        if (IsNotRequired(player)(techDependency_))
        {
            techDependency_.reset();
        }
    }

    void ReligionTactics::apply(TacticSelectionDataMap& selectionDataMap, int depTacticFlags)
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

        for (std::list<IReligionTacticPtr>::const_iterator tacticIter(religionTactics_.begin()), tacticEndIter(religionTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->apply(shared_from_this(), techSelectionData);
        }
    }

    void ReligionTactics::apply(TacticSelectionData& selectionData)
    {
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);

        if (pPlayer->getTechResearchDepth(techDependency_->getResearchTech()) > 2)
        {
            return;
        }

        for (std::list<IReligionTacticPtr>::const_iterator tacticIter(religionTactics_.begin()), tacticEndIter(religionTactics_.end());
            tacticIter != tacticEndIter; ++tacticIter)
        {
            (*tacticIter)->apply(shared_from_this(), selectionData);
        }
    }

    ReligionTypes ReligionTactics::getReligionType() const
    {
        return religionType_;
    }

    PlayerTypes ReligionTactics::getPlayerType() const
    {
        return playerType_;
    }

    UnitTypes ReligionTactics::getSpreadUnitType() const
    {
        return spreadUnitType_;
    }

    BuildingTypes ReligionTactics::getSpreadBuildingType() const
    {
        return spreadBuildingType_;
    }

    ResearchTechDependencyPtr ReligionTactics::getTechDependency() const
    {
        return techDependency_;
    }

    int ReligionTactics::getTurnLastUpdated() const
    {
        return lastTurnCalculated_;
    }

    void ReligionTactics::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tresource tactics: " << (religionType_ == NO_RELIGION ? " none? " : gGlobals.getReligionInfo(religionType_).getType());
        if (techDependency_)
        {
            techDependency_->debug(os);
        }
        for (std::list<IReligionTacticPtr>::const_iterator ci(religionTactics_.begin()), ciEnd(religionTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->debug(os);
        }        
#endif
    }

    void ReligionTactics::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ReligionTacticsID);
        pStream->Write(religionType_);
    }

    void ReligionTactics::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&religionType_);
    }

    ReligionTacticsPtr ReligionTactics::factoryRead(FDataStreamBase* pStream)
    {
        ReligionTacticsPtr pReligionTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pReligionTactics = ReligionTacticsPtr(new ReligionTactics());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in ReligionTactics::factoryRead");
            break;
        }

        pReligionTactics->read(pStream);
        return pReligionTactics;
    }

    void EconomicReligionTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tGeneral economic resource tactic ";
#endif
    }

    void EconomicReligionTactic::update(const ReligionTacticsPtr& pReligionTactics, City& city)
    {
        cityProjections_.erase(city.getCvCity()->getIDInfo());
        if (city.getCvCity()->isHasReligion(pReligionTactics->getReligionType()))
        {
            return;
        }

        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(city.getCvCity()->getOwner());

        ProjectionLadder base = gGlobals.getGame().getAltAI()->getPlayer(city.getCvCity()->getOwner())->getCity(city.getID()).getBaseOutputProjection();

        std::vector<IProjectionEventPtr> events;
        CityDataPtr pCityData = city.getCityData()->clone();
        pCityData->getReligionHelper()->setHasReligion(*pCityData, pReligionTactics->getReligionType(), true);
        pCityData->recalcOutputs();

        ProjectionLadder delta = getProjectedOutput(*pPlayer, pCityData, pPlayer->getAnalysis()->getNumSimTurns(), events, ConstructItem(), __FUNCTION__, false, false);
        cityProjections_[city.getCvCity()->getIDInfo()] = std::make_pair(delta, base);

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
        std::map<IDInfo, std::pair<ProjectionLadder, ProjectionLadder> >::const_iterator ci = cityProjections_.find(city.getCvCity()->getIDInfo());
        os << '\n' << __FUNCTION__ << ' ' << narrow(pPlayer->getCity(ci->first.iID).getCvCity()->getName()) << " = " << ci->second.first.getOutput() - ci->second.second.getOutput();

        /*os << "\n\tbase projection: ";
        ci->second.second.debug(os);
        os << "\n\tbase projection diffs: ";
        ci->second.second.debugDiffs(os);
        
        os << "\n\tdelta projection: ";
        ci->second.first.debug(os);
        os << "\n\tdelta projection diffs: ";
        ci->second.first.debugDiffs(os);*/
        
#endif

    }

    void EconomicReligionTactic::update(const ReligionTacticsPtr& pReligionTactics, Player& player)
    {
        cityProjections_.clear();
        CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {
            update(pReligionTactics, player.getCity(pCity->getID()));
        }
    }

    void EconomicReligionTactic::apply(const ReligionTacticsPtr& pReligionTactics, TacticSelectionData& selectionData)
    {
        //TotalOutput accumulatedBase, accumulatedDelta;
        for (std::map<IDInfo, std::pair<ProjectionLadder, ProjectionLadder> >::const_iterator ci(cityProjections_.begin()), ciEnd(cityProjections_.end()); ci != ciEnd; ++ci)
        {
            if (!::getCity(ci->first))  // city may have been deleted since update was last called
            {
                selectionData.potentialReligionOutputDeltas[pReligionTactics->getReligionType()].erase(ci->first);
            }
            else
            {
                selectionData.potentialReligionOutputDeltas[pReligionTactics->getReligionType()][ci->first] = std::make_pair(ci->second.first.getOutput(), ci->second.second.getOutput());
                //accumulatedDelta += ci->second.first.getOutput();
                //accumulatedBase += ci->second.second.getOutput();
            }
        }
    }

    void EconomicReligionTactic::write(FDataStreamBase* pStream) const
    {
        // todo - write projection ladders
        pStream->Write(ID);
    }

    void EconomicReligionTactic::read(FDataStreamBase* pStream)
    {
    }

    void UnitReligionTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tUnit religion tactic. ";
#endif
    }

    void UnitReligionTactic::update(const ReligionTacticsPtr& pReligionTactics, City& city)
    {
        if (city.getCvCity()->isHasReligion(pReligionTactics->getReligionType()))
        {
            return;
        }
        // else... todo
    }

    void UnitReligionTactic::update(const ReligionTacticsPtr& pReligionTactics, Player& player)
    {
        CityIter cityIter(*player.getCvPlayer());
        while (CvCity* pCity = cityIter())
        {
            if (pCity->isHasReligion(pReligionTactics->getReligionType()))
            {
                continue;
            }
            // else... todo
        }        
    }

    void UnitReligionTactic::apply(const ReligionTacticsPtr& pReligionTactics, TacticSelectionData& selectionData)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pReligionTactics->getPlayerType()))->getStream();
        os << "\n\tApplying unit religion tactic for: " << gGlobals.getReligionInfo(pReligionTactics->getReligionType()).getType();
#endif
    }

    void UnitReligionTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void UnitReligionTactic::read(FDataStreamBase* pStream)
    {
    }

    bool doMissionaryMove(Player& player, CvUnitAI* pUnit)
    {
        player.getAnalysis()->getReligionAnalysis()->updateMissionaryMission(pUnit);
        return true;
    }
}
