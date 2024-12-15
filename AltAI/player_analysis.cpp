#include "AltAI.h"

#include "CyArgsList.h"

#include "./plot_info.h"
#include "./resource_info.h"
#include "./resource_info_visitors.h"
#include "./unit_info_visitors.h"
#include "./tech_info_visitors.h"
#include "./civic_info_visitors.h"
#include "./building_info_visitors.h"
#include "./project_info_visitors.h"
#include "./spec_info_visitors.h"
#include "./player_analysis.h"
#include "./religion_tactics.h"
#include "./military_tactics.h"
#include "./map_analysis.h"
#include "./worker_tactics.h"
#include "./unit_analysis.h"
#include "./great_people_tactics.h"
#include "./modifiers_helper.h"
#include "./gamedata_analysis.h"
#include "./settler_manager.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./unit.h"
#include "./iters.h"
#include "./civ_helper.h"
#include "./civ_log.h"
#include "./error_log.h"
#include "./tictacs.h"
#include "./helper_fns.h"
#include "./save_utils.h"

namespace AltAI
{
    namespace
    {
        int getRequiredUnitExperience_(PlayerTypes playerType, int level)
        {
            int iExperienceNeeded = 0;
	        long lExperienceNeeded = 0;
        	CyArgsList argsList;
	        argsList.add(level);
            argsList.add(playerType);

	        gDLL->getPythonIFace()->callFunction(PYGameModule, "getExperienceNeeded", argsList.makeFunctionArgs(), &lExperienceNeeded);
	        iExperienceNeeded = (int)lExperienceNeeded;
            return iExperienceNeeded;
        }
    }

    PlayerAnalysis::PlayerAnalysis(Player& player) : player_(player), pMapAnalysis_(boost::shared_ptr<MapAnalysis>(new MapAnalysis(player))), timeHorizon_(0)
    {
        int nTurnsLeft = gGlobals.getGame().getMaxTurns() - gGlobals.getGame().getElapsedGameTurns();
        timeHorizon_ = std::min<int>(std::max<int>(gGlobals.getGame().getMaxTurns() / 10, 20), nTurnsLeft);

        pMapDelta_ = boost::shared_ptr<PlotUpdates>(new PlotUpdates(player_));
        playerTactics_ = boost::shared_ptr<PlayerTactics>(new PlayerTactics(player_));
        pWorkerAnalysis_ = boost::shared_ptr<WorkerAnalysis>(new WorkerAnalysis(player));
        pMilitaryAnalysis_ = MilitaryAnalysisPtr(new MilitaryAnalysis(player));
        pUnitAnalysis_ = boost::shared_ptr<UnitAnalysis>(new UnitAnalysis(player_));
        pGreatPeopleAnalysis_ = boost::shared_ptr<GreatPeopleAnalysis>(new GreatPeopleAnalysis(player_));
        pReligionAnalysis_ = boost::shared_ptr<ReligionAnalysis>(new ReligionAnalysis(player_));
    }

    // init 'static' data, i.e. xml based stuff
    void PlayerAnalysis::init()
    {
        analyseUnits_();
        analyseBuildings_();
        analyseProjects_();
        analyseTechs_();
        analyseCivics_();
        analyseResources_();
        analyseExperienceLevels_();

        recalcTechDepths();
    }

    // this initialisation depends on cities having being initialised
    void PlayerAnalysis::postCityInit()
    {
        analyseSpecialists();

        pUnitAnalysis_->init();
        pUnitAnalysis_->debug();

        playerTactics_->init();
        pReligionAnalysis_->init();
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
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);
                boost::shared_ptr<UnitInfo> pUnitInfo;
                if (!unitInfo.isAnimal())
                {
                    pUnitInfo = makeUnitInfo(unitType, playerType);
                    unitsInfo_.insert(std::make_pair(unitType, pUnitInfo));
                }
                else
                {
//#ifdef ALTAI_DEBUG
//                    std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
//                    os << "\nSkipping unit: " << gGlobals.getUnitInfo(unitType).getType();                    
//#endif
                    continue;
                }

                int productionMultiplier = 0;
                SpecialUnitTypes specialUnitType = (SpecialUnitTypes)unitInfo.getSpecialUnitType();
                for (int j = 0, count = gGlobals.getNumTraitInfos(); j < count; ++j)
                {
                    if (player_.getCvPlayer()->hasTrait((TraitTypes)j))
                    {
                        productionMultiplier = unitInfo.getProductionTraits(j);

                        if (specialUnitType != NO_SPECIALUNIT)
                        {
                            productionMultiplier += gGlobals.getSpecialUnitInfo(specialUnitType).getProductionTraits(j);
                        }
                    }
                }

                if (productionMultiplier != 0)
                {
                    unitProductionModifiersMap_[unitType] = productionMultiplier;
                }

                // UNIT_SCIENTIST -> BUILDING_ACADEMY, BUILDING_CORPORATION_3, BUILDING_CORPORATION_6
                // UNIT_MERCHANT -> BUILDING_CORPORATION_1, BUILDING_CORPORATION_2, etc...
                std::vector<BuildingTypes> unitSpecialBuildings = getUnitSpecialBuildings(pUnitInfo);
                for (size_t buildingIndex = 0, buildingCount = unitSpecialBuildings.size(); buildingIndex < buildingCount; ++buildingIndex)
                {
                    unitSpecialBuildingsMap_[unitSpecialBuildings[buildingIndex]].push_back(unitType);
                }
            }
        }

#ifdef ALTAI_DEBUG
        // debug
        /*{
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
            for (std::map<UnitTypes, boost::shared_ptr<UnitInfo> >::const_iterator ci(unitsInfo_.begin()), ciEnd(unitsInfo_.end()); ci != ciEnd; ++ci)
            {
                os << "\n" << gGlobals.getUnitInfo(ci->first).getType() << ": ";
                streamUnitInfo(os, ci->second);
            }
            os << "\n";
        }*/
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
                const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);
                if (buildingInfo.getProductionCost() > 0)
                {
                    buildingsInfo_.insert(std::make_pair(buildingType, makeBuildingInfo(buildingType, playerType)));
                }
                else
                {
                    // buildings which we can't build directly (e.g. need great people to build)
                    specialBuildingsInfo_.insert(std::make_pair(buildingType, makeBuildingInfo(buildingType, playerType)));
                }

                int productionMultiplier = 0;
                SpecialBuildingTypes specialBuildingType = (SpecialBuildingTypes)buildingInfo.getSpecialBuildingType();
                for (int j = 0, count = gGlobals.getNumTraitInfos(); j < count; ++j)
                {
                    if (player_.getCvPlayer()->hasTrait((TraitTypes)j))
                    {
                        productionMultiplier += buildingInfo.getProductionTraits(j);

                        if (specialBuildingType != NO_SPECIALBUILDING)
                        {
                            productionMultiplier += gGlobals.getSpecialBuildingInfo(specialBuildingType).getProductionTraits(j);
                        }
                    }
                }

                if (productionMultiplier != 0)
                {
                    buildingProductionModifiersMap_[buildingType] = productionMultiplier;
                }
            }
        }

//#ifdef ALTAI_DEBUG
//        // debug
//        {
//            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
//            for (std::map<BuildingTypes, boost::shared_ptr<BuildingInfo> >::const_iterator ci(buildingsInfo_.begin()), ciEnd(buildingsInfo_.end()); ci != ciEnd; ++ci)
//            {
//                os << "\n" << gGlobals.getBuildingInfo(ci->first).getType() << ": ";
//                streamBuildingInfo(os, ci->second);
//
//                std::map<BuildingTypes, int>::const_iterator modifiersIter = buildingProductionModifiersMap_.find(ci->first);
//                if (modifiersIter != buildingProductionModifiersMap_.end())
//                {
//                    os << " production modifier = " << modifiersIter->second;
//                }
//            }
//            os << "\n\nSpecial buildings:\n";
//            for (std::map<BuildingTypes, boost::shared_ptr<BuildingInfo> >::const_iterator ci(specialBuildingsInfo_.begin()), ciEnd(specialBuildingsInfo_.end()); ci != ciEnd; ++ci)
//            {
//                os << "\n" << gGlobals.getBuildingInfo(ci->first).getType() << ": ";
//                streamBuildingInfo(os, ci->second);
//
//                std::map<BuildingTypes, int>::const_iterator modifiersIter = buildingProductionModifiersMap_.find(ci->first);
//                if (modifiersIter != buildingProductionModifiersMap_.end())
//                {
//                    os << " production modifier = " << modifiersIter->second;
//                }
//            }
//        }
//#endif
    }

    void PlayerAnalysis::analyseProjects_()
    {
        for (int i = 0, count = gGlobals.getNumProjectInfos(); i < count; ++i)
        {
            PlayerTypes playerType = player_.getPlayerID();
            projectsInfo_.insert(std::make_pair((ProjectTypes)i, makeProjectInfo((ProjectTypes)i, playerType)));
        }

#ifdef ALTAI_DEBUG
        // debug
        {
            //std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
            //for (std::map<ProjectTypes, boost::shared_ptr<ProjectInfo> >::const_iterator ci(projectsInfo_.begin()), ciEnd(projectsInfo_.end()); ci != ciEnd; ++ci)
            //{
            //    os << "\n" << gGlobals.getProjectInfo(ci->first).getType() << ": ";
            //    streamProjectInfo(os, ci->second);
            //}
            //os << "\n";
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

//#ifdef ALTAI_DEBUG
//        // debug
//        {
//            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
//            for (std::map<TechTypes, boost::shared_ptr<TechInfo> >::const_iterator ci(techsInfo_.begin()), ciEnd(techsInfo_.end()); ci != ciEnd; ++ci)
//            {
//                os << "\n" << gGlobals.getTechInfo(ci->first).getType() << ": ";
//                streamTechInfo(os, ci->second);
//            }
//            os << "\n";
//        }
//#endif
    }

    void PlayerAnalysis::analyseCivics_()
    {        for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
        {
            PlayerTypes playerType = player_.getPlayerID();
            civicsInfo_.insert(std::make_pair((CivicTypes)i, makeCivicInfo((CivicTypes)i, playerType)));
        }

//#ifdef ALTAI_DEBUG
//        // debug
//        {
//            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
//            for (std::map<CivicTypes, boost::shared_ptr<CivicInfo> >::const_iterator ci(civicsInfo_.begin()), ciEnd(civicsInfo_.end()); ci != ciEnd; ++ci)
//            {
//                os << "\n" << gGlobals.getCivicInfo(ci->first).getType() << ": ";
//                streamCivicInfo(os, ci->second);
//            }
//            os << "\n";
//        }
//#endif
    }

    void PlayerAnalysis::analyseResources_()
    {
        for (int i = 0, count = gGlobals.getNumBonusInfos(); i < count; ++i)
        {
            PlayerTypes playerType = player_.getPlayerID();
            resourcesInfo_.insert(std::make_pair((BonusTypes)i, makeResourceInfo((BonusTypes)i, playerType)));
        }

//#ifdef ALTAI_DEBUG
//        // debug
//        {
//            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
//            for (std::map<BonusTypes, boost::shared_ptr<ResourceInfo> >::const_iterator ci(resourcesInfo_.begin()), ciEnd(resourcesInfo_.end()); ci != ciEnd; ++ci)
//            {
//                os << "\n" << gGlobals.getBonusInfo(ci->first).getType() << ": ";
//                streamResourceInfo(os, ci->second);
//            }
//            os << "\n";
//        }
//#endif
    }

    void PlayerAnalysis::analyseExperienceLevels_()
    {
        static const int maxLevel = 12;

        levelExperienceMap_.clear();
        experienceLevelsMap_.clear();
        experienceLevelsMap_[0] = 0;

        for (int i = 1; i <= maxLevel; ++i)
        {
            int iExperienceNeeded = getRequiredUnitExperience_(player_.getPlayerID(), i);

            levelExperienceMap_[i] = iExperienceNeeded; // e.g. {1, 2}, {2, 5}, {3, 10}, {4, 17}
            experienceLevelsMap_[iExperienceNeeded] = i;  // e.g. {2, 1}, {4, 2}, {8, 3}, {13, 4} (if charismatic)
        }
    }

    void PlayerAnalysis::analyseSpecialists()
    {
        TotalOutputWeights outputWeights = makeOutputW(1, 1, 1, 1, 1, 1);
        const CvCity* pCity = player_.getCvPlayer()->getCapitalCity();

        for (int i = 0; i < NUM_OUTPUT_TYPES; ++i)
        {
            MixedTotalOutputOrderFunctor valueF(makeTotalOutputSinglePriority((OutputTypes)i), outputWeights);

            if (pCity)
            {
                const CityDataPtr& pCityData = player_.getCity(pCity).getCityData();

                YieldModifier yieldModifier = pCityData->getModifiersHelper()->getTotalYieldModifier(*pCityData);
                CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);
                bestSpecialistTypesMap_[(OutputTypes)i] = AltAI::getBestSpecialist(player_, yieldModifier, commerceModifier, valueF);
            }
            else
            {
                bestSpecialistTypesMap_[(OutputTypes)i] = AltAI::getBestSpecialist(player_, valueF);
            }
        }

        if (pCity)
        {
            std::vector<OutputTypes> outputTypes;
            outputTypes.push_back(OUTPUT_PRODUCTION);
            outputTypes.push_back(OUTPUT_RESEARCH);
            outputTypes.push_back(OUTPUT_GOLD);

            mixedSpecialistTypes_ = getBestSpecialists(outputTypes, 4);
        }

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player_.getPlayerID()))->getStream();
        os << "\nBest specialists:";
        for (std::map<OutputTypes, SpecialistTypes>::const_iterator ci(bestSpecialistTypesMap_.begin()), ciEnd(bestSpecialistTypesMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nOutput type: " << ci->first << " = " << (ci->second == NO_SPECIALIST ? "NO_SPECIALIST" : gGlobals.getSpecialistInfo(ci->second).getType());
        }
#endif
    }

    SpecialistTypes PlayerAnalysis::getBestSpecialist(OutputTypes outputType) const
    {
        std::map<OutputTypes, SpecialistTypes>::const_iterator ci = bestSpecialistTypesMap_.find(outputType);
        return  ci != bestSpecialistTypesMap_.end() ? ci->second : NO_SPECIALIST;
    }

    SpecialistTypes PlayerAnalysis::getBestSpecialist(const std::vector<OutputTypes>& outputTypes) const
    {
        TotalOutputWeights outputWeights = makeOutputW(1, 1, 1, 1, 1, 1);
        const CvCity* pCity = player_.getCvPlayer()->getCapitalCity();
        TotalOutputPriority outputPriorities = makeTotalOutputPriorities(outputTypes);
        MixedWeightedOutputOrderFunctor<TotalOutput> valueF(outputPriorities, outputWeights);
        SpecialistTypes bestSpecialistType = NO_SPECIALIST;

        if (pCity)
        {
            const CityDataPtr& pCityData = player_.getCity(pCity).getCityData();

            YieldModifier yieldModifier = pCityData->getModifiersHelper()->getTotalYieldModifier(*pCityData);
            CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);
            bestSpecialistType = AltAI::getBestSpecialist(player_, yieldModifier, commerceModifier, valueF);
        }
        else
        {
            bestSpecialistType = AltAI::getBestSpecialist(player_, valueF);
        }

        return bestSpecialistType;
    }

    std::vector<SpecialistTypes> PlayerAnalysis::getBestSpecialists(const std::vector<OutputTypes>& outputTypes, size_t count) const
    {
        TotalOutputWeights outputWeights = makeOutputW(1, 1, 1, 1, 1, 1);
        const CvCity* pCity = player_.getCvPlayer()->getCapitalCity();
        TotalOutputPriority outputPriorities = makeTotalOutputPriorities(outputTypes);
        MixedWeightedOutputOrderFunctor<TotalOutput> valueF(outputPriorities, outputWeights);
        const CityDataPtr& pCityData = player_.getCity(pCity).getCityData();

        YieldModifier yieldModifier = pCityData->getModifiersHelper()->getTotalYieldModifier(*pCityData);
        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

        return AltAI::getBestSpecialists(player_, yieldModifier, commerceModifier, count, valueF);
    }

    std::vector<SpecialistTypes> PlayerAnalysis::getMixedSpecialistTypes() const
    {
        return mixedSpecialistTypes_;
    }

    int PlayerAnalysis::getPlayerUnitProductionModifier(UnitTypes unitType) const
    {
        std::map<UnitTypes, int>::const_iterator ci = unitProductionModifiersMap_.find(unitType);
        return ci == unitProductionModifiersMap_.end() ? 0 : ci->second;
    }

    int PlayerAnalysis::getPlayerBuildingProductionModifier(BuildingTypes buildingType) const
    {
        std::map<BuildingTypes, int>::const_iterator ci = buildingProductionModifiersMap_.find(buildingType);
        return ci == buildingProductionModifiersMap_.end() ? 0 : ci->second;
    }

    std::vector<UnitTypes> PlayerAnalysis::getSpecialBuildingUnits(BuildingTypes buildingType) const
    {
        std::map<BuildingTypes, std::vector<UnitTypes> >::const_iterator ci = unitSpecialBuildingsMap_.find(buildingType);
        if (ci != unitSpecialBuildingsMap_.end())
        {
            return ci->second;
        }
        else
        {
            return std::vector<UnitTypes>();
        }
    }

    int PlayerAnalysis::getUnitLevel(int experience) const
    {
        // std::map<int, int> experienceLevelsMap_;  // exp -> level
        // returns first value which is greater than passed in experience, e.g. 11 -> {17, 4}. So need to go back one to get current level
        std::map<int, int>::const_iterator levelIter = experienceLevelsMap_.upper_bound(experience);
        if (levelIter == experienceLevelsMap_.begin())  // unless at beginning - e.g. passed in 1 or 0 -> {2, 1} - treat this as level 0 (no promotions available)
        {
            return 0;
        }
        else if (levelIter == experienceLevelsMap_.end())  // past the end - just treat as max level we analysed (any unit at this level {145, 12} is crazy enough anyway)
        {
            return experienceLevelsMap_.rbegin()->second;
        }
        else
        {
            return (--levelIter)->second;
        }
    }

    int PlayerAnalysis::getRequiredUnitExperience(int level) const
    {
        std::map<int, int>::const_iterator expIter = levelExperienceMap_.find(level);
        if (expIter != levelExperienceMap_.end())
        {
            return expIter->second;
        }
        else
        {
            return getRequiredUnitExperience_(player_.getPlayerID(), level);
        }
    }

    boost::shared_ptr<UnitInfo> PlayerAnalysis::getUnitInfo(UnitTypes unitType) const
    {
        std::map<UnitTypes, boost::shared_ptr<UnitInfo> >::const_iterator unitsIter = unitsInfo_.find(unitType);

        if (unitsIter != unitsInfo_.end())
        {
            return unitsIter->second;
        }

        std::ostream& os = ErrorLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nFailed to find unit information for unit: " << gGlobals.getUnitInfo(unitType).getType();
        
        return boost::shared_ptr<UnitInfo>();
    }

    boost::shared_ptr<BuildingInfo> PlayerAnalysis::getBuildingInfo(BuildingTypes buildingType) const
    {
        std::map<BuildingTypes, boost::shared_ptr<BuildingInfo> >::const_iterator buildingsIter = buildingsInfo_.find(buildingType);

        if (buildingsIter != buildingsInfo_.end())
        {
            return buildingsIter->second;
        }
        
#ifdef ALTAI_DEBUG
        if (!getSpecialBuildingInfo(buildingType))
        {
            std::ostream& os = ErrorLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nFailed to find building information for building: " << gGlobals.getBuildingInfo(buildingType).getType();
        }
#endif

        return boost::shared_ptr<BuildingInfo>();
    }

    boost::shared_ptr<BuildingInfo> PlayerAnalysis::getSpecialBuildingInfo(BuildingTypes buildingType) const
    {
        std::map<BuildingTypes, boost::shared_ptr<BuildingInfo> >::const_iterator buildingsIter = specialBuildingsInfo_.find(buildingType);

        if (buildingsIter != specialBuildingsInfo_.end())
        {
            return buildingsIter->second;
        }
        
        std::ostream& os = ErrorLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nFailed to find special building information for building: " << gGlobals.getBuildingInfo(buildingType).getType();

        return boost::shared_ptr<BuildingInfo>();
    }

    boost::shared_ptr<ProjectInfo> PlayerAnalysis::getProjectInfo(ProjectTypes projectType) const
    {
        std::map<ProjectTypes, boost::shared_ptr<ProjectInfo> >::const_iterator projectsIter = projectsInfo_.find(projectType);

        if (projectsIter != projectsInfo_.end())
        {
            return projectsIter->second;
        }
        
        std::ostream& os = ErrorLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nFailed to find project information for project: " << gGlobals.getProjectInfo(projectType).getType();

        return boost::shared_ptr<ProjectInfo>();
    }

    boost::shared_ptr<TechInfo> PlayerAnalysis::getTechInfo(TechTypes techType) const
    {
        std::map<TechTypes, boost::shared_ptr<TechInfo> >::const_iterator techsIter = techsInfo_.find(techType);

        if (techsIter != techsInfo_.end())
        {
            return techsIter->second;
        }
        
        std::ostream& os = ErrorLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nFailed to find tech information for tech: " << gGlobals.getTechInfo(techType).getType();

        return boost::shared_ptr<TechInfo>();
    }

    boost::shared_ptr<CivicInfo> PlayerAnalysis::getCivicInfo(CivicTypes civicType) const
    {
        std::map<CivicTypes, boost::shared_ptr<CivicInfo> >::const_iterator civicsIter = civicsInfo_.find(civicType);

        if (civicsIter != civicsInfo_.end())
        {
            return civicsIter->second;
        }
        
        std::ostream& os = ErrorLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nFailed to find civic information for civic: " << gGlobals.getCivicInfo(civicType).getType();

        return boost::shared_ptr<CivicInfo>();
    }

    boost::shared_ptr<ResourceInfo> PlayerAnalysis::getResourceInfo(BonusTypes bonusType) const
    {
        std::map<BonusTypes, boost::shared_ptr<ResourceInfo> >::const_iterator resourcesIter = resourcesInfo_.find(bonusType);

        if (resourcesIter != resourcesInfo_.end())
        {
            return resourcesIter->second;
        }
        
        std::ostream& os = ErrorLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nFailed to find resource information for resource: " << gGlobals.getBonusInfo(bonusType).getType();

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
                if (getTechResearchDepth((TechTypes)i) <= depth)
                {
                    techs.push_back((TechTypes)i);
                }
            }
        }
        return techs;
    }

    CivicTypes PlayerAnalysis::chooseCivic(CivicOptionTypes civicOptionType)
    {
        return playerTactics_->chooseCivic(civicOptionType);
    }

    int PlayerAnalysis::getNumSimTurns() const
    {
        // 1500, 750, 500, 330 => 90, 45, 30, 19
        return (6 * gGlobals.getGame().getMaxTurns()) / 100;
    }

    ResearchTech PlayerAnalysis::getResearchTech(TechTypes ignoreTechType)
    {
        ResearchTech tacticsTech = playerTactics_->getResearchTech(ignoreTechType);
        if (tacticsTech.techType != NO_TECH && sanityCheckTech_(tacticsTech.techType))
        {
            return tacticsTech;
        }
        return ResearchTech();
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
        os << "\nTech = " << gGlobals.getTechInfo(techType).getType() << " cost = " << cost << ", rate = " << rate << ", turns = " << approxTurns;
#endif

        /*if (approxTurns > (4 * gGlobals.getGame().getMaxTurns() / 100))
        {
            return false;
        }*/

        return true;
    }

    void PlayerAnalysis::updateTechDepths_()
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//        os << "\nTech depths:";
//        std::multimap<int, TechTypes> techDepthsMap;
//#endif
        const int count = gGlobals.getNumTechInfos();
        techDepths_.resize(count);

        for (int i = 0; i < count; ++i)
        {
            techDepths_[i] = calculateTechResearchDepth((TechTypes)i, player_.getPlayerID());
//#ifdef ALTAI_DEBUG
//            techDepthsMap.insert(std::make_pair(techDepths_[i], (TechTypes)i));
//#endif
        }
//#ifdef ALTAI_DEBUG
//        int currentDepth = -1, techCount = 0;
//        for (std::multimap<int, TechTypes>::const_iterator ci(techDepthsMap.begin()), ciEnd(techDepthsMap.end()); ci != ciEnd; ++ci)
//        {
//            if (ci->first != currentDepth)
//            {
//                os << "\ndepth = " << ci->first << " ";
//                currentDepth = ci->first;
//                techCount = 0;
//            }
//
//            os << gGlobals.getTechInfo(ci->second).getType() << ", ";
//            if (!(++techCount % 10)) os << "\n\t";
//        }
//#endif
    }

    void PlayerAnalysis::write(FDataStreamBase* pStream) const
    {
        playerTactics_->write(pStream);
        pWorkerAnalysis_->write(pStream);
        pMilitaryAnalysis_->write(pStream);
    }

    void PlayerAnalysis::read(FDataStreamBase* pStream)
    {
        playerTactics_->read(pStream);
        pWorkerAnalysis_->read(pStream);
        pMilitaryAnalysis_->read(pStream);
    }
}