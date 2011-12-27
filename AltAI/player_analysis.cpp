#include "./plot_info.h"
#include "./resource_info.h"
#include "./resource_info_visitors.h"
#include "./unit_info_visitors.h"
#include "./tech_info_visitors.h"
#include "./civic_info_visitors.h"
#include "./building_info_visitors.h"
#include "./spec_info_visitors.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./unit_analysis.h"
#include "./gamedata_analysis.h"
#include "./settler_manager.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./unit.h"
#include "./iters.h"
#include "./civ_helper.h"
#include "./civ_log.h"
#include "./tictacs.h"
#include "./helper_fns.h"
#include "./save_utils.h"

namespace AltAI
{
    PlayerAnalysis::PlayerAnalysis(Player& player) : player_(player), pMapAnalysis_(boost::shared_ptr<MapAnalysis>(new MapAnalysis(player))), timeHorizon_(0)
    {
        int nTurnsLeft = gGlobals.getGame().getMaxTurns() - gGlobals.getGame().getElapsedGameTurns();
        timeHorizon_ = std::min<int>(std::max<int>(gGlobals.getGame().getMaxTurns() / 10, 20), nTurnsLeft);

        playerTactics_ = boost::shared_ptr<PlayerTactics>(new PlayerTactics(player_));
        pUnitAnalysis_ = boost::shared_ptr<UnitAnalysis>(new UnitAnalysis(player_));
    }

    // init 'static' data, i.e. xml based stuff
    void PlayerAnalysis::init()
    {
        analyseUnits_();
        analyseBuildings_();
        analyseTechs_();
        analyseCivics_();
        analyseResources_();

        recalcTechDepths();
    }

    // this initialisation depends on cities having being initialised
    void PlayerAnalysis::postCityInit()
    {
        analyseSpecialists_();

        pUnitAnalysis_->init();
        pUnitAnalysis_->debug();

        playerTactics_->init();
    }


    void PlayerAnalysis::analyseUnits_()
    {
        for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
        {
            PlayerTypes playerType = player_.getPlayerID();
            UnitTypes unitType = getPlayerVersion(playerType, (UnitClassTypes)i);
            if (unitType != NO_UNIT)
            {
                // skip 'special' units (not built by normal means, such as great people; also excludes barb animals)
                // need the >= as settlers cost 0 in the xml and have a weird, undocumented mechanism to determine their cost
                if (gGlobals.getUnitInfo(unitType).getProductionCost() >= 0)
                {
                    unitsInfo_.insert(std::make_pair(unitType, makeUnitInfo(unitType, playerType)));
                }
                //else
                //{
                //    std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
                //    os << "\nSkipping unit: " << gGlobals.getUnitInfo(unitType).getType();
                //}
            }
        }

#ifdef ALTAI_DEBUG
        // debug
        {
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
            for (std::map<UnitTypes, boost::shared_ptr<UnitInfo> >::const_iterator ci(unitsInfo_.begin()), ciEnd(unitsInfo_.end()); ci != ciEnd; ++ci)
            {
                os << "\n" << gGlobals.getUnitInfo(ci->first).getType() << ": ";
                streamUnitInfo(os, ci->second);
            }
            os << "\n";
        }
#endif
    }

    void PlayerAnalysis::analyseBuildings_()
    {
        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            PlayerTypes playerType = player_.getPlayerID();
            BuildingTypes buildingType = getPlayerVersion(playerType, (BuildingClassTypes)i);
            if (buildingType != NO_BUILDING)
            {
                if (gGlobals.getBuildingInfo(buildingType).getProductionCost() > 0)
                {
                    buildingsInfo_.insert(std::make_pair(buildingType, makeBuildingInfo(buildingType, playerType)));
                }
                else
                {
#ifdef ALTAI_DEBUG
                    std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
                    os << "\nSkipping building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
                }
            }
        }

#ifdef ALTAI_DEBUG
        // debug
        {
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
            for (std::map<BuildingTypes, boost::shared_ptr<BuildingInfo> >::const_iterator ci(buildingsInfo_.begin()), ciEnd(buildingsInfo_.end()); ci != ciEnd; ++ci)
            {
                os << "\n" << gGlobals.getBuildingInfo(ci->first).getType() << ": ";
                streamBuildingInfo(os, ci->second);
            }
            os << "\n";
        }
#endif
    }

    void PlayerAnalysis::analyseTechs_()
    {
        for (int i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            PlayerTypes playerType = player_.getPlayerID();
            techsInfo_.insert(std::make_pair((TechTypes)i, makeTechInfo((TechTypes)i, playerType)));
        }

#ifdef ALTAI_DEBUG
        // debug
        {
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
            for (std::map<TechTypes, boost::shared_ptr<TechInfo> >::const_iterator ci(techsInfo_.begin()), ciEnd(techsInfo_.end()); ci != ciEnd; ++ci)
            {
                os << "\n" << gGlobals.getTechInfo(ci->first).getType() << ": ";
                streamTechInfo(os, ci->second);
            }
            os << "\n";
        }
#endif
    }

    void PlayerAnalysis::analyseCivics_()
    {
        for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
        {
            PlayerTypes playerType = player_.getPlayerID();
            civicsInfo_.insert(std::make_pair((CivicTypes)i, makeCivicInfo((CivicTypes)i, playerType)));
        }

#ifdef ALTAI_DEBUG
        // debug
        {
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
            for (std::map<CivicTypes, boost::shared_ptr<CivicInfo> >::const_iterator ci(civicsInfo_.begin()), ciEnd(civicsInfo_.end()); ci != ciEnd; ++ci)
            {
                os << "\n" << gGlobals.getCivicInfo(ci->first).getType() << ": ";
                streamCivicInfo(os, ci->second);
            }
            os << "\n";
        }
#endif
    }

    void PlayerAnalysis::analyseResources_()
    {
        for (int i = 0, count = gGlobals.getNumBonusInfos(); i < count; ++i)
        {
            PlayerTypes playerType = player_.getPlayerID();
            resourcesInfo_.insert(std::make_pair((BonusTypes)i, makeResourceInfo((BonusTypes)i, playerType)));
        }

#ifdef ALTAI_DEBUG
        // debug
        {
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
            for (std::map<BonusTypes, boost::shared_ptr<ResourceInfo> >::const_iterator ci(resourcesInfo_.begin()), ciEnd(resourcesInfo_.end()); ci != ciEnd; ++ci)
            {
                os << "\n" << gGlobals.getBonusInfo(ci->first).getType() << ": ";
                streamResourceInfo(os, ci->second);
            }
            os << "\n";
        }
#endif
    }

    void PlayerAnalysis::analyseSpecialists_()
    {
        TotalOutputWeights outputWeights = makeOutputW(1, 1, 1, 1, 1, 1);
        const CvCity* pCity = player_.getCvPlayer()->getCapitalCity();

        for (int i = 0; i < NUM_OUTPUT_TYPES; ++i)
        {
            MixedTotalOutputOrderFunctor valueF(makeTotalOutputSinglePriority((OutputTypes)i), outputWeights);

            if (pCity)
            {
                bestSpecialistTypesMap_[(OutputTypes)i] = AltAI::getBestSpecialist(player_, player_.getCity(pCity->getID()), valueF);
            }
            else
            {
                bestSpecialistTypesMap_[(OutputTypes)i] = AltAI::getBestSpecialist(player_, valueF);
            }
        }

        // debug
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
        for (std::map<OutputTypes, SpecialistTypes>::const_iterator ci(bestSpecialistTypesMap_.begin()), ciEnd(bestSpecialistTypesMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\n" << ci->first << " = " << (ci->second == NO_SPECIALIST ? "NO_SPECIALIST" : gGlobals.getSpecialistInfo(ci->second).getType());
        }
    }

    SpecialistTypes PlayerAnalysis::getBestSpecialist(OutputTypes outputType) const
    {
        std::map<OutputTypes, SpecialistTypes>::const_iterator ci = bestSpecialistTypesMap_.find(outputType);
        return  ci != bestSpecialistTypesMap_.end() ? ci->second : NO_SPECIALIST;
    }

    boost::shared_ptr<UnitInfo> PlayerAnalysis::getUnitInfo(UnitTypes unitType) const
    {
        std::map<UnitTypes, boost::shared_ptr<UnitInfo> >::const_iterator unitsIter = unitsInfo_.find(unitType);

        if (unitsIter != unitsInfo_.end())
        {
            return unitsIter->second;
        }
        
        return boost::shared_ptr<UnitInfo>();
    }

    boost::shared_ptr<BuildingInfo> PlayerAnalysis::getBuildingInfo(BuildingTypes buildingType) const
    {
        std::map<BuildingTypes, boost::shared_ptr<BuildingInfo> >::const_iterator buildingsIter = buildingsInfo_.find(buildingType);

        if (buildingsIter != buildingsInfo_.end())
        {
            return buildingsIter->second;
        }
        
        return boost::shared_ptr<BuildingInfo>();
    }

    boost::shared_ptr<TechInfo> PlayerAnalysis::getTechInfo(TechTypes techType) const
    {
        std::map<TechTypes, boost::shared_ptr<TechInfo> >::const_iterator techsIter = techsInfo_.find(techType);

        if (techsIter != techsInfo_.end())
        {
            return techsIter->second;
        }
        
        return boost::shared_ptr<TechInfo>();
    }

    boost::shared_ptr<CivicInfo> PlayerAnalysis::getCivicInfo(CivicTypes civicType) const
    {
        std::map<CivicTypes, boost::shared_ptr<CivicInfo> >::const_iterator civicsIter = civicsInfo_.find(civicType);

        if (civicsIter != civicsInfo_.end())
        {
            return civicsIter->second;
        }
        
        return boost::shared_ptr<CivicInfo>();
    }

    boost::shared_ptr<ResourceInfo> PlayerAnalysis::getResourceInfo(BonusTypes bonusType) const
    {
        std::map<BonusTypes, boost::shared_ptr<ResourceInfo> >::const_iterator resourcesIter = resourcesInfo_.find(bonusType);

        if (resourcesIter != resourcesInfo_.end())
        {
            return resourcesIter->second;
        }
        
        return boost::shared_ptr<ResourceInfo>();
    }

    void PlayerAnalysis::update(const boost::shared_ptr<IEvent<NullRecv> >& event)
    {
        // pass through to strategies which are interested in this event
    }

    void PlayerAnalysis::recalcTechDepths()
    {
        updateTechDepths_();
    }

    int PlayerAnalysis::getTechResearchDepth(TechTypes techType) const
    {
        return techType == NO_TECH ? 0 : techDepths_[techType];
    }

    std::vector<TechTypes> PlayerAnalysis::getTechsWithDepth(int depth) const
    {
        std::vector<TechTypes> techs;
        boost::shared_ptr<CivHelper> pCivHelper = player_.getCivHelper();

        for (int i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            if (!pCivHelper->hasTech((TechTypes)i))
            {
                if (getTechResearchDepth((TechTypes)i) == depth)
                {
                    techs.push_back((TechTypes)i);
                }
            }
        }
        return techs;
    }

    ResearchTech PlayerAnalysis::getResearchTech(TechTypes ignoreTechType)
    {
        ResearchTech tacticsTech = playerTactics_->getResearchTech(ignoreTechType);
        if (tacticsTech.techType != NO_TECH && sanityCheckTech_(tacticsTech.techType))
        {
            return tacticsTech;
        }

//        CvPlayer* pPlayer = player_.getCvPlayer();
//        PlayerTypes playerType = player_.getPlayerID();
//
//#ifdef ALTAI_DEBUG
//        // debug
//        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer);
//        std::ostream& os = pCivLog->getStream();
//#endif
//
//        std::map<TechTypes, WorkerTechData> workerTechData = PlayerAnalysis::getWorkerTechData_();
//
//        std::map<TechTypes, int, std::greater<int> > workerBonusTechMap, workerImprovementTechMap;
//        TechTypes bestBonusTech = NO_TECH, bestImprovementTech = NO_TECH;
//        int bestBonusValue = 0;
//        int bestImprovementValue = 0;  // not likely to have equal best values for different techs (no need for map to keep track of values)
//
//        typedef std::multimap<int, TechTypes, std::greater<int> > MostChosenTechsMap;
//        MostChosenTechsMap mostChosenTechs;
//
//        for (std::map<TechTypes, WorkerTechData>::const_iterator ci(workerTechData.begin()), ciEnd(workerTechData.end()); ci != ciEnd; ++ci)
//        {
//            int thisBonusValue = 0;
//            int thisImprovementValue = 0;
//
//            // new bonuses we can improve (todo? distinguish between bonus types we can access and those we can't? (bit of an edge case, as usually get the wheel quite early))
//            for (std::map<BonusTypes, int>::const_iterator iter(ci->second.newBonusAccessCounts.begin()), endIter(ci->second.newBonusAccessCounts.end()); iter != endIter; ++iter)
//            {
//                boost::shared_ptr<ResourceInfo> pResourceInfo = getResourceInfo(iter->first);
//                std::pair<int, int> unitCounts = getResourceMilitaryUnitCount(pResourceInfo);
//
//                ResourceHappyInfo happyInfo = getResourceHappyInfo(pResourceInfo);
//                ResourceHealthInfo healthInfo = getResourceHealthInfo(pResourceInfo);
//
//                thisBonusValue += 1 + happyInfo.actualHappy + healthInfo.actualHealth + 2 * unitCounts.first + unitCounts.second;
//            }
//
//            // new bonuses we can connect
//            for (std::set<BonusTypes>::const_iterator iter(ci->second.newConnectableBonuses.begin()), endIter(ci->second.newConnectableBonuses.end()); iter != endIter; ++iter)
//            {
//                boost::shared_ptr<ResourceInfo> pResourceInfo = getResourceInfo(*iter);
//
//                std::pair<int, int> unitCounts = getResourceMilitaryUnitCount(pResourceInfo);
//
//                ResourceHappyInfo happyInfo = getResourceHappyInfo(pResourceInfo);
//                ResourceHealthInfo healthInfo = getResourceHealthInfo(pResourceInfo);
//
//                thisBonusValue += 1 + happyInfo.actualHappy + healthInfo.actualHealth + 2 * unitCounts.first + unitCounts.second;
//            }
//
//            // new bonuses we can potentially access through new cities (todo? - include ones from border expansions? - useful for v. early?) 
//            for (std::set<BonusTypes>::const_iterator iter(ci->second.newPotentialWorkableBonuses.begin()), endIter(ci->second.newPotentialWorkableBonuses.end()); iter != endIter; ++iter)
//            {
//                boost::shared_ptr<ResourceInfo> pResourceInfo = getResourceInfo(*iter);
//
//                std::pair<int, int> unitCounts = getResourceMilitaryUnitCount(pResourceInfo);
//
//                thisBonusValue += 1 + unitCounts.first;
//            }
//
//            for (std::map<ImprovementTypes, std::pair<TotalOutput, int> >::const_iterator iter(ci->second.newValuedImprovements.begin()), endIter(ci->second.newValuedImprovements.end());
//                iter != endIter; ++iter)
//            {
//                thisImprovementValue += iter->second.second;
//            }
//
//            workerBonusTechMap.insert(std::make_pair(ci->first, thisBonusValue));
//            mostChosenTechs.insert(std::make_pair(thisBonusValue, ci->first));
//            workerImprovementTechMap.insert(std::make_pair(ci->first, thisImprovementValue));
//
//            if (thisBonusValue > bestBonusValue)
//            {
//                bestBonusTech = ci->first;
//                bestBonusValue = thisBonusValue;
//            }
//
//            if (thisImprovementValue > bestImprovementValue)
//            {
//                bestImprovementTech = ci->first;
//                bestImprovementValue = thisImprovementValue;
//            }
//#ifdef ALTAI_DEBUG
//            os << "\n" << gGlobals.getTechInfo(ci->first).getType() << ": bonus value = " << thisBonusValue << ", improvement value = " << thisImprovementValue;
//#endif
//        }
//
//        if (!mostChosenTechs.empty())
//        {
//            std::pair<MostChosenTechsMap::const_iterator, MostChosenTechsMap::const_iterator> itPair = mostChosenTechs.equal_range(mostChosenTechs.begin()->first);
//
//            std::vector<TechTypes> possibleTechs;
//            for (; itPair.first != itPair.second; ++itPair.first)
//            {
//                possibleTechs.push_back(itPair.first->second);
//            }
//
//            if (possibleTechs.size() > 1)
//            {
//                int bestImprovementValueOfBonusTechs = 0;
//                TechTypes bestImprovementTechOfBonusTechs = NO_TECH;
//
//                for (size_t i = 0, count = possibleTechs.size(); i < count; ++i)
//                {
//                    std::map<TechTypes, int, std::greater<int> >::const_iterator ci(workerImprovementTechMap.find(possibleTechs[i]));
//                    if (ci != workerImprovementTechMap.end())
//                    {
//                        if (ci->second > bestImprovementValueOfBonusTechs)
//                        {
//                            bestImprovementValueOfBonusTechs = ci->second;
//                            bestImprovementTechOfBonusTechs = possibleTechs[i];
//                        }
//                    }
//
//                }
//                if (bestImprovementTechOfBonusTechs != NO_TECH)
//                {
//                    bestBonusTech = bestImprovementTechOfBonusTechs;
//                }
//                else
//                {
//                    bestBonusTech = possibleTechs[0];
//                }
//            }
//            else
//            {
//                bestBonusTech = possibleTechs[0];
//            }
//        }
//
//        if (bestBonusTech != NO_TECH)
//        {
//#ifdef ALTAI_DEBUG
//            os << "\nBest worker tech (bonuses) = " << gGlobals.getTechInfo(bestBonusTech).getType() << ": bonus value = " << bestBonusValue;
//#endif
//        }
//
//        if (bestImprovementTech != NO_TECH)
//        {
//#ifdef ALTAI_DEBUG
//            os << "\nBest worker tech (improvements) = " << gGlobals.getTechInfo(bestImprovementTech).getType() << ": improvement value = " << bestImprovementValue;
//#endif
//        }
//
//        if (bestBonusTech != NO_TECH && sanityCheckTech_(bestBonusTech))
//        {
//            const CvTechInfo& techInfo = gGlobals.getTechInfo(bestBonusTech);
//            std::vector<TechTypes> orTechs = getOrTechs(getTechInfo(bestBonusTech));
//            bool haveOrTech = false;
//            int bestOrBonusValue = 0, bestOrImprovementValue = 0;
//            TechTypes bestOrBonusTech = NO_TECH, bestOrImprovementTech = NO_TECH;
//            if (orTechs.size() > 1)
//            {
//                for (int i = 0, count = orTechs.size(); i < count; ++i)
//                {
//                    if (CvTeamAI::getTeam(player_.getTeamID()).isHasTech(orTechs[i]))
//                    {
//                        haveOrTech = true;
//                        break;
//                    }
//                    else
//                    {
//                        int thisOrBonusValue = workerBonusTechMap[orTechs[i]];
//                        if (thisOrBonusValue > bestOrBonusValue)
//                        {
//                            bestOrBonusTech = orTechs[i];
//                        }
//                        int thisOrImprovementValue = workerImprovementTechMap[orTechs[i]];
//                        if (thisOrImprovementValue > bestOrImprovementValue)
//                        {
//                            bestOrImprovementTech = orTechs[i];
//                        }
//                    }
//                }
//            }
//
//            if (!haveOrTech)
//            {
//                if (bestOrBonusTech != NO_TECH)
//                {
//                    if (sanityCheckTech_(bestOrBonusTech))
//                    {
//                        pPlayer->pushResearch(bestOrBonusTech);
//                    }
//                }
//                else if (bestOrImprovementTech != NO_TECH)
//                {
//                    if (sanityCheckTech_(bestOrImprovementTech))
//                    {
//                        pPlayer->pushResearch(bestOrImprovementTech);
//                    }
//                }
//            }
//            return sanityCheckTech_(bestBonusTech) ? bestBonusTech : NO_TECH;
//
//        }
//        else if (bestImprovementTech != NO_TECH && bestImprovementValue > 10000 * pPlayer->getNumCities())
//        {
//            return sanityCheckTech_(bestImprovementTech) ? bestImprovementTech : NO_TECH;
//        }

        return ResearchTech();
    }

    std::map<TechTypes, PlayerAnalysis::WorkerTechData> PlayerAnalysis::getWorkerTechData_()
    {
        const CvPlayer* pPlayer = player_.getCvPlayer();

#ifdef ALTAI_DEBUG
        // debug
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*pPlayer);
        std::ostream& os = pCivLog->getStream();
#endif

        std::vector<TechTypes> availableWorkerTechs;
        std::vector<TechTypes> availableTechs(getTechsWithDepth(1)), oneAwayTechs(getTechsWithDepth(2));

        for (int i = 0, count = availableTechs.size(); i < count; ++i)
        {
            if (techIsWorkerTech(getTechInfo(availableTechs[i])))
            {
                availableWorkerTechs.push_back(availableTechs[i]);
            }
        }

        for (int i = 0, count = oneAwayTechs.size(); i < count; ++i)
        {
            if (techIsWorkerTech(getTechInfo(oneAwayTechs[i])))
            {
                availableWorkerTechs.push_back(oneAwayTechs[i]);
            }
        }

#ifdef ALTAI_DEBUG
        // debug
        {
            os << "\nAvailable worker techs = ";
            for (size_t i = 0, count = availableWorkerTechs.size(); i < count; ++i)
            {
                os << (!(i % 4) ? "\n" : "") << gGlobals.getTechInfo(availableWorkerTechs[i]).getType() << " (depth = " << getTechResearchDepth(availableWorkerTechs[i]) << "),";
            }
        }
#endif

        boost::shared_ptr<CivHelper> civHelper = gGlobals.getGame().getAltAI()->getPlayer(player_.getPlayerID())->getCivHelper();
        std::set<BonusTypes> potentialBonusTypes = gGlobals.getGame().getAltAI()->getPlayer(player_.getPlayerID())->getSettlerManager()->getBonusesForSites(2);

        std::map<TechTypes, WorkerTechData> techData;

        for (size_t i = 0, count = availableWorkerTechs.size(); i < count; ++i)
        {
            std::vector<BonusTypes> workableBonuses = getWorkableBonuses(getTechInfo(availableWorkerTechs[i]));

            for (size_t bonusIndex = 0, bonusCount = workableBonuses.size(); bonusIndex < bonusCount; ++bonusIndex)
            {
                if (potentialBonusTypes.find(workableBonuses[bonusIndex]) != potentialBonusTypes.end())
                {
                    std::map<TechTypes, WorkerTechData>::iterator iter = techData.find(availableWorkerTechs[i]);
                    if (iter == techData.end())
                    {
                        iter = techData.insert(std::make_pair(availableWorkerTechs[i], WorkerTechData())).first;
                        iter->second.techType = availableWorkerTechs[i];
                    }

                    iter->second.newPotentialWorkableBonuses.insert(workableBonuses[bonusIndex]);
                }
            }
        }

        CityIter cityIter(*pPlayer);
        CvCity* pCity;
        while (pCity = cityIter())
        {
            CityImprovementManager improvementManager(pCity->getIDInfo(), true);
            const City& city = gGlobals.getGame().getAltAI()->getPlayer(player_.getPlayerID())->getCity(pCity->getID());
            TotalOutputWeights outputWeights = city.getPlotAssignmentSettings().outputWeights;

            improvementManager.simulateImprovements(outputWeights);
            std::vector<CityImprovementManager::PlotImprovementData> baseImprovements = improvementManager.getImprovements();

            

#ifdef ALTAI_DEBUG
            // debug
            os << "\nBase improvements:";
            for (size_t index = 0, count = baseImprovements.size(); index < count; ++index)
            {
                improvementManager.logImprovement(os, baseImprovements[index]);
            }
#endif

            for (size_t i = 0, count = availableWorkerTechs.size(); i < count; ++i)
            {
                civHelper->addTech(availableWorkerTechs[i]);
                bool canBuildRoute = !availableRoutes(getTechInfo(availableWorkerTechs[i])).empty();

                std::map<TechTypes, WorkerTechData>::iterator iter = techData.find(availableWorkerTechs[i]);
                if (iter == techData.end())
                {
                    iter = techData.insert(std::make_pair(availableWorkerTechs[i], WorkerTechData())).first;
                    iter->second.techType = availableWorkerTechs[i];
                }

                // check for existing bonus improvements which we might be able to connect now
                for (size_t index = 0, count = baseImprovements.size(); index < count; ++index)
                {
                    if (boost::get<6>(baseImprovements[index]) & CityImprovementManager::ImprovementMakesBonusValid)
                    {
                        XYCoords coords = boost::get<0>(baseImprovements[index]);
                        const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
    
                        if (canBuildRoute && pPlot->getOwner() == player_.getPlayerID() && !pPlot->isConnectedTo(pCity))
                        {
                            BonusTypes bonusType = pPlot->getBonusType(player_.getTeamID());
                            iter->second.newConnectableBonuses.insert(bonusType);
                        }
                    }
                }

                improvementManager.simulateImprovements(outputWeights);
                std::vector<CityImprovementManager::PlotImprovementData> newImprovements = improvementManager.getImprovements();

#ifdef ALTAI_DEBUG
                // debug
                os << "\nNew improvements with tech: " << gGlobals.getTechInfo(availableWorkerTechs[i]).getType();
                for (size_t index = 0, count = newImprovements.size(); index < count; ++index)
                {
                    improvementManager.logImprovement(os, newImprovements[index]);
                }
#endif

                std::vector<CityImprovementManager::PlotImprovementData> delta = findNewImprovements(baseImprovements, newImprovements);

#ifdef ALTAI_DEBUG
                // debug
                os << "\nDifferent/new improvements with tech: " << gGlobals.getTechInfo(availableWorkerTechs[i]).getType();
#endif
                for (size_t index = 0, count = delta.size(); index < count; ++index)
                {
#ifdef ALTAI_DEBUG
                    improvementManager.logImprovement(os, delta[index]);
#endif
                    if (boost::get<6>(delta[index]) & CityImprovementManager::ImprovementMakesBonusValid)
                    {
                        XYCoords coords = boost::get<0>(delta[index]);
                        const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                        BonusTypes bonusType = pPlot->getBonusType(player_.getTeamID());
                        ++iter->second.newBonusAccessCounts[bonusType];

                        if (canBuildRoute && !pPlot->isConnectedTo(pCity))
                        {
                            iter->second.newConnectableBonuses.insert(bonusType);
                        }
                    }

                    // selected and has TotalOutput value - indicating simulation was positive
                    if (boost::get<5>(delta[index]) == CityImprovementManager::Not_Built)
                    {
                        if (!isEmpty(boost::get<4>(delta[index])))
                        {
                            // add its TotalOutput to total for this improvement type
                            iter->second.newValuedImprovements[boost::get<2>(delta[index])].first += boost::get<4>(delta[index]);
                            iter->second.newValuedImprovements[boost::get<2>(delta[index])].second += TotalOutputValueFunctor(outputWeights)(boost::get<4>(delta[index]));
                        }
                        else
                        {
                            // count improvement
                            ++iter->second.newUnvaluedImprovements[boost::get<2>(delta[index])];
                        }
                    }
                    else if (boost::get<5>(delta[index]) == CityImprovementManager::Not_Selected)
                    {
                        // count improvement
                        ++iter->second.newUnvaluedImprovements[boost::get<2>(delta[index])];
                    }
                }

                civHelper->removeTech(availableWorkerTechs[i]);
            }

#ifdef ALTAI_DEBUG
            for (std::map<TechTypes, WorkerTechData>::const_iterator ci(techData.begin()), ciEnd(techData.end()); ci != ciEnd; ++ci)
            {
                ci->second.debug(os);
            }
#endif
        }

        return techData;
    }

    // todo - team logic for pushing research?
    bool PlayerAnalysis::sanityCheckTech_(TechTypes techType) const
    {
        const CvPlayer* player = player_.getCvPlayer();
        const CvTeamAI& team = CvTeamAI::getTeam(player_.getTeamID());

#ifdef ALTAI_DEBUG
        // debug
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player);
        std::ostream& os = pCivLog->getStream();
#endif

        const int cost = team.getResearchLeft(techType);
        const int rate = player->calculateResearchRate(techType);

        const int approxTurns = std::max<int>(1, cost / rate);

#ifdef ALTAI_DEBUG
        os << "\nTech = " << gGlobals.getTechInfo(techType).getType() << "cost = " << cost << ", rate = " << rate << ", turns = " << approxTurns;
#endif

        if (approxTurns > (4 * gGlobals.getGame().getMaxTurns() / 100))
        {
            return false;
        }

        return true;
    }

    void PlayerAnalysis::updateTechDepths_()
    {
#ifdef ALTAI_DEBUG
        // debug
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player_.getCvPlayer());
        std::ostream& os = pCivLog->getStream();
        os << "\nTech depths = ";
#endif
        const int count = gGlobals.getNumTechInfos();
        techDepths_.resize(count);

        for (int i = 0; i < count; ++i)
        {
            techDepths_[i] = calculateTechResearchDepth((TechTypes)i, player_.getPlayerID());
#ifdef ALTAI_DEBUG
            os << gGlobals.getTechInfo((TechTypes)i).getType() << "=" << techDepths_[i] << ", ";
            if (!(i % 10))
            {
                os << "\n";
            }
#endif
        }
    }

    void PlayerAnalysis::write(FDataStreamBase* pStream) const
    {
        playerTactics_->write(pStream);
    }

    void PlayerAnalysis::read(FDataStreamBase* pStream)
    {
        playerTactics_->read(pStream);
    }

    void PlayerAnalysis::WorkerTechData::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        if (techType == NO_TECH)
        {
            os << "\nNo tech?";
            return;
        }

        os << "\n" << gGlobals.getTechInfo(techType).getType();

        for (std::set<BonusTypes>::const_iterator ci(newPotentialWorkableBonuses.begin()), ciEnd(newPotentialWorkableBonuses.end()); ci != ciEnd; ++ci)
        {
            if (ci != newPotentialWorkableBonuses.begin()) os << ", ";
            else os << "\nnew potential resource types: ";
            os << gGlobals.getBonusInfo(*ci).getType();
        }

        for (std::set<BonusTypes>::const_iterator ci(newConnectableBonuses.begin()), ciEnd(newConnectableBonuses.end()); ci != ciEnd; ++ci)
        {
            if (ci != newConnectableBonuses.begin()) os << ", ";
            else os << "\nnew connectable resource types: ";
            os << gGlobals.getBonusInfo(*ci).getType();
        }

        for (std::map<BonusTypes, int>::const_iterator ci(newBonusAccessCounts.begin()), ciEnd(newBonusAccessCounts.end()); ci != ciEnd; ++ci)
        {
            if (ci != newBonusAccessCounts.begin()) os << ", ";
            else os << "\nnew resource types:\n";

            os << gGlobals.getBonusInfo(ci->first).getType() << ", count = " << ci->second;
        }

        for (std::map<ImprovementTypes, std::pair<TotalOutput, int> >::const_iterator ci(newValuedImprovements.begin()), ciEnd(newValuedImprovements.end()); ci != ciEnd; ++ci)
        {
            if (ci != newValuedImprovements.begin()) os << ", ";
            else os << "\n";

            os << gGlobals.getImprovementInfo(ci->first).getType() << ", output = " << ci->second.first << ", total value = " << ci->second.second;
        }
        
        for (std::map<ImprovementTypes, int>::const_iterator ci(newUnvaluedImprovements.begin()), ciEnd(newUnvaluedImprovements.end()); ci != ciEnd; ++ci)
        {
            if (ci != newUnvaluedImprovements.begin()) os << ", ";
            else os << "\n";

            os << gGlobals.getImprovementInfo(ci->first).getType() << ", count = " << ci->second;
        }
        os << "\n";
#endif
    }
}