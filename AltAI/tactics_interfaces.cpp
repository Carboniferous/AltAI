#include "AltAI.h"

#include "./tactics_interfaces.h"
#include "./building_tactics_deps.h"
#include "./building_tactics_items.h"
#include "./unit_tactics_items.h"
#include "./city_building_tactics.h"
#include "./city_unit_tactics.h"
#include "./player_tech_tactics.h"
#include "./tech_tactics_items.h"
#include "./civic_tactics_items.h"
#include "./resource_tactics.h"
#include "./religion_tactics.h"
#include "./city.h"
#include "./helper_fns.h"
#include "./civ_log.h"

namespace AltAI
{
    std::string depTacticFlagsToString(int dep_tactic_flags)
    {
        std::ostringstream oss;
        if (dep_tactic_flags != 0)
        {
            if (dep_tactic_flags & IDependentTactic::Tech_Dep)
            {
                oss << " tech dep ";
            }
            if (dep_tactic_flags & IDependentTactic::City_Buildings_Dep)
            {
                oss << " city buildings dep ";
            }
            if (dep_tactic_flags & IDependentTactic::Civ_Buildings_Dep)
            {
                oss << " civ buildings dep ";
            }
            if (dep_tactic_flags & IDependentTactic::Religion_Dep)
            {
                oss << " religions dep ";
            }
            if (dep_tactic_flags & IDependentTactic::Resource_Dep)
            {
                oss << " resources dep ";
            }
            if (dep_tactic_flags & IDependentTactic::CivUnits_Dep)
            {
                oss << " civ units dep ";
            }
        }
        else
        {
            oss << " no dep flag ";
        }
        return oss.str();
    }

    IDependentTacticPtr IDependentTactic::factoryRead(FDataStreamBase* pStream)
    {
        IDependentTacticPtr pDependentTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case ResearchTechDependency::ID:
            pDependentTactic = IDependentTacticPtr(new ResearchTechDependency());
            break;
        case CityBuildingDependency::ID:
            pDependentTactic = IDependentTacticPtr(new CityBuildingDependency());
            break;
        case CivBuildingDependency::ID:
            pDependentTactic = IDependentTacticPtr(new CivBuildingDependency());
            break;
        case ReligiousDependency::ID:
            pDependentTactic = IDependentTacticPtr(new ReligiousDependency());
            break;
        case StateReligionDependency::ID:
            pDependentTactic = IDependentTacticPtr(new StateReligionDependency());
            break;
        case CityBonusDependency::ID:
            pDependentTactic = IDependentTacticPtr(new CityBonusDependency());
            break;
        case CivUnitDependency::ID:
            pDependentTactic = IDependentTacticPtr(new CivUnitDependency());
            break;
        case ResourceProductionBonusDependency::ID:
            pDependentTactic = IDependentTacticPtr(new ResourceProductionBonusDependency());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in IDependentTactic::factoryRead");
            break;
        }

        pDependentTactic->read(pStream);
        return pDependentTactic;
    }

    ICityBuildingTacticPtr ICityBuildingTactic::factoryRead(FDataStreamBase* pStream)
    {
        ICityBuildingTacticPtr pBuildingTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case EconomicBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new EconomicBuildingTactic());
            break;
        case FoodBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new FoodBuildingTactic());
            break;
        case HappyBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new HappyBuildingTactic());
            break;
        case HealthBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new HealthBuildingTactic());
            break;
        case ScienceBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new ScienceBuildingTactic());
            break;
        case GoldBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new GoldBuildingTactic());
            break;
        case CultureBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new CultureBuildingTactic());
            break;
        case EspionageBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new EspionageBuildingTactic());
            break;
        case SpecialistBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new SpecialistBuildingTactic());
            break;
        case GovCenterTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new GovCenterTactic());
            break;
        case UnitExperienceTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new UnitExperienceTactic());
            break;
        case CityDefenceBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new CityDefenceBuildingTactic());
            break;
        case FreeTechBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new FreeTechBuildingTactic());
            break;
        case CanTrainUnitBuildingTactic::ID:
            pBuildingTactic = ICityBuildingTacticPtr(new CanTrainUnitBuildingTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in ICityBuildingTactic::factoryRead");
            break;
        }

        pBuildingTactic->read(pStream);
        return pBuildingTactic;
    }

    void debugDepItemSet(const DependencyItemSet& depItemSet, std::ostream& os)
    {
        for (DependencyItemSet::const_iterator di(depItemSet.begin()), diEnd(depItemSet.end()); di != diEnd; ++di)
        {
            if (di != depItemSet.begin()) os << "+";
            debugDepItem(*di, os);                            
        }
    }

    void debugDepItem(const DependencyItem& depItem, std::ostream& os)
    {
        switch (depItem.first)
        {
        case ResearchTechDependency::ID:
            os << " ResearchTechDependency: " << gGlobals.getTechInfo((TechTypes)depItem.second).getType();
            break;
        case CityBuildingDependency::ID:
            os << " CityBuildingDependency: " << gGlobals.getBuildingInfo((BuildingTypes)depItem.second).getType();
            break;
        case CivBuildingDependency::ID:
            os << " CivBuildingDependency: " << gGlobals.getBuildingInfo((BuildingTypes)depItem.second).getType();
            break;
        case ReligiousDependency::ID:
            os << " ReligiousDependency: " << gGlobals.getReligionInfo((ReligionTypes)depItem.second).getType();
            break;
        case CityBonusDependency::ID:
            os << " CityBonusDependency: " << (depItem.second != NO_BONUS ? gGlobals.getBonusInfo((BonusTypes)depItem.second).getType() : "none?");
            break;
        case CivUnitDependency::ID:
            os << " CivUnitDependency: " << (depItem.second != NO_UNIT ? gGlobals.getUnitInfo((UnitTypes)depItem.second).getType() : "none?");
            break;
        case ResourceProductionBonusDependency::ID:
            os << " ResourceProductionBonusDependency: " << (depItem.second != NO_BONUS ? gGlobals.getBonusInfo((BonusTypes)depItem.second).getType() : "none?");
            break;
        //case -1:
        //    os << " No Dependencies";
        //    break;
        default:
            FAssertMsg(false, "Unknown Dep Item");
            os << " UNKNOWN Dependency?";
            break;
        }
        os << ' ';
    }

    void debugBuildItem(const BuildQueueItem& buildQueueItem, std::ostream& os)
    {
        switch (buildQueueItem.first)
        {
        case NoItem:
            os << " (no item)";
            break;
        case BuildingItem:
            os << " building: " << gGlobals.getBuildingInfo((BuildingTypes)buildQueueItem.second).getType();
            break;
        case UnitItem: 
            os << " unit: " << gGlobals.getUnitInfo((UnitTypes)buildQueueItem.second).getType();
            break;
        case ProjectItem:
            os << " project: " << gGlobals.getProjectInfo((ProjectTypes)buildQueueItem.second).getType();
            break;
        case ProcessItem:
            os << " process: " << gGlobals.getProcessInfo((ProcessTypes)buildQueueItem.second).getType();
            break;
        default:
            os << " unknown build queue item: " << buildQueueItem.first << ", " << buildQueueItem.second;
        }
    }

    DependencyItemSet getUnsatisfiedDeps(const DependencyItemSet& depItemSet, Player& player, City& city)
    {
        DependencyItemSet unsatisifedDeps;
        for (DependencyItemSet::const_iterator di(depItemSet.begin()), diEnd(depItemSet.end()); di != diEnd; ++di)
        {
            switch (di->first)
            {
            case ResearchTechDependency::ID:
                if (!CvTeamAI::getTeam(player.getTeamID()).isHasTech((TechTypes)(di->second)))
                {
                    unsatisifedDeps.insert(*di);
                }
                break;
            case CityBuildingDependency::ID:
                if (city.getCvCity()->getNumBuilding((BuildingTypes)(di->second)) == 0)
                {
                    unsatisifedDeps.insert(*di);
                }
                break;
            case CivBuildingDependency::ID:
                // todo - only store require building type - not its count or source building type (e.g. (6) courthouses for forbidden palace)
                unsatisifedDeps.insert(*di);
                break;
            case ReligiousDependency::ID:
                if (!city.getCvCity()->isHasReligion((ReligionTypes)(di->second)))
                {
                    unsatisifedDeps.insert(*di);
                }
                break;
            case CityBonusDependency::ID:
            case ResourceProductionBonusDependency::ID:
                if (!city.getCvCity()->hasBonus((BonusTypes)(di->second)))
                {
                    unsatisifedDeps.insert(*di);
                }
                break;
            case CivUnitDependency::ID:
                // todo
                unsatisifedDeps.insert(*di);
                break;
            default:
                FAssertMsg(false, "Unknown Dep Item");
                break;
            }
        }

        return unsatisifedDeps;
    }

    bool DependencyItemsComp::operator() (const DependencyItemSet& first, const DependencyItemSet& second) const
    {
        if (first.size() != second.size())
        {
            return first.size() < second.size();
        }
        else
        {
            for (DependencyItemSet::const_iterator ci1(first.begin()), ci2(second.begin()), ci1End(first.end());
                ci1 != ci1End; ++ci1, ++ci2)
            {
                if (*ci1 != *ci2)
                {
                    return *ci1 < *ci2;
                }
            }
            return false;
        }
    }

    bool IsNotRequired::operator() (const IDependentTacticPtr& pDependentTactic) const
    {
        return pDependentTactic->removeable() && (pCity ? !pDependentTactic->required(pCity, depTacticFlags) : !pDependentTactic->required(player, depTacticFlags));
    }

    // basically - get dependent items for given set of flags
    // if ignore flags is set to none then dependent items will be added if not required (i.e. known techs, present dependent buildings)
    // (they will not exist if they were not required at all) - so all existing deps will be added
    // if ignore flags are set - those items will be added instead as if they were required regardless
    // no longer add flag - just return empty list of deps
    // /*a special 'no dep' item is returned if all dependencies are required/satisfied (allowing for ignore flags) but none have been explicitly added*/
    std::vector<DependencyItem> ICityBuildingTactics::getDepItems(int depTacticFlags) const
    {
        std::vector<DependencyItem> depItems;
        const CvCity* pCity = ::getCity(getCity());
        if (!pCity)
        {
            return depItems;
        }

#ifdef ALTAI_DEBUG
        const CvPlayerAI& player = CvPlayerAI::getPlayer(pCity->getOwner());
        std::ostream& os = CivLog::getLog(player)->getStream();
        os << "\n\t calling getDepItems with dep flags: " << depTacticFlagsToString(depTacticFlags) << " for city: " << safeGetCityName(pCity)
           << " and building: " << gGlobals.getBuildingInfo(getBuildingType()).getType();
#endif

        bool allDependenciesSatisfied = true;

        const std::vector<IDependentTacticPtr>& deps = getDependencies();      
        for (size_t i = 0, count = deps.size(); i < count; ++i)
        {
            if (deps[i]->matches(depTacticFlags))
            {
#ifdef ALTAI_DEBUG
                os << " matched dep: ";
                deps[i]->debug(os);
#endif
                const std::vector<DependencyItem> thisDepItems = deps[i]->getDependencyItems();
                std::copy(thisDepItems.begin(), thisDepItems.end(), std::back_inserter(depItems));
            }
#ifdef ALTAI_DEBUG
            else
            {
                os << " unmatched dep flags: ";
                deps[i]->debug(os);
            }
#endif
        }

        const std::vector<ResearchTechDependencyPtr>& techDeps = getTechDependencies();
        for (size_t i = 0, count = techDeps.size(); i < count; ++i)
        {
            // i.e. if tech is not needed (so we know it) or tech ignore flag is set
            if (techDeps[i]->matches(depTacticFlags))
            {
#ifdef ALTAI_DEBUG
                os << " match tech deps: ";
                techDeps[i]->debug(os);
#endif
                const std::vector<DependencyItem> thisDepItems = techDeps[i]->getDependencyItems();
                std::copy(thisDepItems.begin(), thisDepItems.end(), std::back_inserter(depItems));
            }
#ifdef ALTAI_DEBUG
            else
            {
                os << " unmatched tech dep: ";
                techDeps[i]->debug(os);
            }
#endif
        }

//        if (depItems.empty()) 
//        {
//#ifdef ALTAI_DEBUG
//            os << " no deps matched - adding noItem dep...";
//#endif
//            // add a special 'no dependency' dependency so we can use it as a map key with other tactics
//            depItems.push_back(std::make_pair(-1, -1));
//        }
#ifdef ALTAI_DEBUG
        os << " done.";
#endif

        return depItems;
    }

    std::vector<BuildQueueItem > ICityBuildingTactics::getBuildItems(int depTacticFlags) const
    {
        std::vector<BuildQueueItem > buildItems;

        const CvCity* pCity = ::getCity(getCity());
        if (!pCity)
        {
            return buildItems;
        }

        // only do non-tech deps (no build items for tech deps)
        const std::vector<IDependentTacticPtr>& deps = getDependencies();      
        for (size_t i = 0, count = deps.size(); i < count; ++i)
        {
            // e.g. required() returns true if a dependency such as a another building or city religion is required or false if relevant ignore flag is set
            if (deps[i]->required(pCity, depTacticFlags))
            {
                BuildQueueItem buildItem = deps[i]->getBuildItem();
                if (buildItem.first != NoItem)
                {
                    buildItems.push_back(buildItem);
                }
            }
        }

        return buildItems;
    }
    
    ICityBuildingTacticsPtr ICityBuildingTactics::factoryRead(FDataStreamBase* pStream)
    {
        ICityBuildingTacticsPtr pCityBuildingTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case CityBuildingTactic::CityBuildingTacticID:
            pCityBuildingTactics = ICityBuildingTacticsPtr(new CityBuildingTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in ICityBuildingTactics::factoryRead");
            break;
        }

        pCityBuildingTactics->read(pStream);
        return pCityBuildingTactics;
    }

    IWorkerBuildTacticPtr IWorkerBuildTactic::factoryRead(FDataStreamBase* pStream)
    {
        IWorkerBuildTacticPtr pWorkerBuildTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case EconomicImprovementTactic::ID:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new EconomicImprovementTactic());
            break;
        case RemoveFeatureTactic::ID:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new RemoveFeatureTactic());
            break;
        case ProvidesResourceTactic::ID:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new ProvidesResourceTactic());
            break;
        case HappyImprovementTactic::ID:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new HappyImprovementTactic());
            break;
        case HealthImprovementTactic::ID:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new HealthImprovementTactic());
            break;
        case MilitaryImprovementTactic::ID:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new MilitaryImprovementTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in IWorkerBuildTactic::factoryRead");
            break;
        }

        pWorkerBuildTactic->read(pStream);
        return pWorkerBuildTactic;
    }

    ILimitedBuildingTacticsPtr IGlobalBuildingTactics::factoryRead(FDataStreamBase* pStream)
    {
        ILimitedBuildingTacticsPtr pGlobalBuildingTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case LimitedBuildingTactic::LimitedBuildingTacticID:
            pGlobalBuildingTactics = ILimitedBuildingTacticsPtr(new LimitedBuildingTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in IGlobalBuildingTactics::factoryRead");
            break;
        }

        pGlobalBuildingTactics->read(pStream);
        return pGlobalBuildingTactics;
    }

    IProcessTacticsPtr IProcessTactics::factoryRead(FDataStreamBase* pStream)
    {
        IProcessTacticsPtr pProcessTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case ProcessTactic::ProcessTacticID:
            pProcessTactics = IProcessTacticsPtr(new ProcessTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in IProcessTactics::factoryRead");
            break;
        }

        pProcessTactics->read(pStream);
        return pProcessTactics;
    }

    ICityUnitTacticPtr ICityUnitTactic::factoryRead(FDataStreamBase* pStream)
    {
        ICityUnitTacticPtr pCityUnitTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case CityDefenceUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new CityDefenceUnitTactic());
            break;
        case ThisCityDefenceUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new ThisCityDefenceUnitTactic());
            break;
        case CityAttackUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new CityAttackUnitTactic());
            break;
        case CollateralUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new CollateralUnitTactic());
            break;
        case FieldDefenceUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new FieldDefenceUnitTactic());
            break;
        case FieldAttackUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new FieldAttackUnitTactic());
            break;
        case BuildCityUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new BuildCityUnitTactic());
            break;
        case BuildImprovementsUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new BuildImprovementsUnitTactic());
            break;
        case SeaAttackUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new SeaAttackUnitTactic());
            break;
        case ScoutUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new ScoutUnitTactic());
            break;
        case SpreadReligionUnitTactic::ID:
            pCityUnitTactic = ICityUnitTacticPtr(new SpreadReligionUnitTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in ICityUnitTactic::factoryRead");
            break;
        }

        pCityUnitTactic->read(pStream);
        return pCityUnitTactic;
    }

    IBuiltUnitTacticPtr IBuiltUnitTactic::factoryRead(FDataStreamBase* pStream)
    {
        IBuiltUnitTacticPtr pUnitTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case DiscoverTechUnitTactic::ID:
            pUnitTactic = IBuiltUnitTacticPtr(new DiscoverTechUnitTactic());
            break;
        case BuildSpecialBuildingUnitTactic::ID:
            pUnitTactic = IBuiltUnitTacticPtr(new BuildSpecialBuildingUnitTactic());
            break;
        case CreateGreatWorkUnitTactic::ID:
            pUnitTactic = IBuiltUnitTacticPtr(new CreateGreatWorkUnitTactic());
            break;
        case TradeMissionUnitTactic::ID:
            pUnitTactic = IBuiltUnitTacticPtr(new TradeMissionUnitTactic());
            break;
        case JoinCityUnitTactic::ID:
            pUnitTactic = IBuiltUnitTacticPtr(new JoinCityUnitTactic());
            break;
        case HurryBuildingUnitTactic::ID:
            pUnitTactic = IBuiltUnitTacticPtr(new HurryBuildingUnitTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in IBuiltUnitTactic::factoryRead");
            break;
        }

        pUnitTactic->read(pStream);
        return pUnitTactic;
    }

    ITechTacticPtr ITechTactic::factoryRead(FDataStreamBase* pStream)
    {
        ITechTacticPtr pTechTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case FreeTechTactic::ID:
            pTechTactic = ITechTacticPtr(new FreeTechTactic());
            break;
        case FoundReligionTechTactic::ID:
            pTechTactic = ITechTacticPtr(new FoundReligionTechTactic());
            break;
        case ConnectsResourcesTechTactic::ID:
            pTechTactic = ITechTacticPtr(new ConnectsResourcesTechTactic());
            break;
        case ConstructBuildingTechTactic::ID:
            pTechTactic = ITechTacticPtr(new ConstructBuildingTechTactic());
            break;
        case ProvidesResourceTechTactic::ID:
            pTechTactic = ITechTacticPtr(new ProvidesResourceTechTactic());
            break;
        case EconomicTechTactic::ID:
            pTechTactic = ITechTacticPtr(new EconomicTechTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in ITechTactic::factoryRead");
            break;
        }

        pTechTactic->read(pStream);
        return pTechTactic;
    }

    ITechTacticsPtr ITechTactics::factoryRead(FDataStreamBase* pStream)
    {
        ITechTacticsPtr pTechTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case -1:
            return ITechTacticsPtr();  // Null tactic - valid for tech tactics - some techs have no tactics of type ITechTactic
        case PlayerTechTactics::PlayerTechTacticsID:
            pTechTactics = ITechTacticsPtr(new PlayerTechTactics());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in ITechTactics::factoryRead");
            break;
        
        }

        pTechTactics->read(pStream);
        return pTechTactics;
    }

    ICivicTacticPtr ICivicTactic::factoryRead(FDataStreamBase* pStream)
    {
        ICivicTacticPtr pCivicTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case EconomicCivicTactic::ID:
            pCivicTactic = ICivicTacticPtr(new EconomicCivicTactic());
            break;
        case HurryCivicTactic::ID:
            pCivicTactic = ICivicTacticPtr(new HurryCivicTactic());
            break;
        case HappyPoliceCivicTactic::ID:
            pCivicTactic = ICivicTacticPtr(new HappyPoliceCivicTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in ICivicTactic::factoryRead");
            break;
        }

        pCivicTactic->read(pStream);
        return pCivicTactic;
    }

    IResourceTacticPtr IResourceTactic::factoryRead(FDataStreamBase* pStream)
    {
        IResourceTacticPtr pResourceTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case EconomicResourceTactic::ID:
            pResourceTactic = IResourceTacticPtr(new EconomicResourceTactic());
            break;
        case UnitResourceTactic::ID:
            pResourceTactic = IResourceTacticPtr(new UnitResourceTactic());
            break;
        case BuildingResourceTactic::ID:
            pResourceTactic = IResourceTacticPtr(new BuildingResourceTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in IResourceTactic::factoryRead");
            break;
        }

        pResourceTactic->read(pStream);
        return pResourceTactic;
    }

    IReligionTacticPtr IReligionTactic::factoryRead(FDataStreamBase* pStream)
    {
        IReligionTacticPtr pReligionTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case EconomicResourceTactic::ID:
            pReligionTactic = IReligionTacticPtr(new EconomicReligionTactic());
            break;
        case UnitResourceTactic::ID:
            pReligionTactic = IReligionTacticPtr(new UnitReligionTactic());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in IReligionTactic::factoryRead");
            break;
        }

        pReligionTactic->read(pStream);
        return pReligionTactic;
    }

    bool ICityBuildingTacticsBuildingComp::operator() (const ICityBuildingTacticsPtr& pFirstTactic, const ICityBuildingTacticsPtr& pSecondTactic) const
    {
        return pFirstTactic->getBuildingType() < pSecondTactic->getBuildingType();
    }
}