#include "AltAI.h"

#include "./tactics_interfaces.h"
#include "./building_tactics_deps.h"
#include "./building_tactics_items.h"
#include "./unit_tactics_items.h"
#include "./city_building_tactics.h"
#include "./city_unit_tactics.h"
#include "./player_tech_tactics.h"
#include "./tech_tactics_items.h"
#include "./civic_tactics.h"
#include "./resource_tactics.h"

namespace AltAI
{
    std::string ignoreFlagToString(int ignoreFlags)
    {
        std::ostringstream oss;
        if (ignoreFlags != 0)
        {
            if (ignoreFlags & IDependentTactic::Ignore_Techs)
            {
                oss << " ignore techs ";
            }
            if (ignoreFlags & IDependentTactic::Ignore_City_Buildings)
            {
                oss << " ignore city buildings ";
            }
            if (ignoreFlags & IDependentTactic::Ignore_Civ_Buildings)
            {
                oss << " ignore civ buildings ";
            }
            if (ignoreFlags & IDependentTactic::Ignore_Religions)
            {
                oss << " ignore religions ";
            }
            if (ignoreFlags & IDependentTactic::Ignore_Resources)
            {
                oss << " ignore resources ";
            }
            if (ignoreFlags & IDependentTactic::Ignore_CivUnits)
            {
                oss << " ignore civ units ";
            }
        }
        else
        {
            oss << " none ";
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
        case ResouceProductionBonusDependency::ID:
            pDependentTactic = IDependentTacticPtr(new ResouceProductionBonusDependency());
            break;
        default:
            FAssertMsg(true, "Unexpected ID in IDependentTactic::factoryRead");
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
        default:
            FAssertMsg(true, "Unexpected ID in ICityBuildingTactic::factoryRead");
            break;
        }

        pBuildingTactic->read(pStream);
        return pBuildingTactic;
    }

    void debugDepItem(const DependencyItem& depItem, std::ostream& os)
    {
        switch (depItem.first)
        {
        case ResearchTechDependency::ID:
            os << "ResearchTechDependency: " << gGlobals.getTechInfo((TechTypes)depItem.second).getType();
            break;
        case CityBuildingDependency::ID:
            os << "CityBuildingDependency: " << gGlobals.getBuildingInfo((BuildingTypes)depItem.second).getType();
            break;
        case CivBuildingDependency::ID:
            os << "CivBuildingDependency: " << gGlobals.getBuildingInfo((BuildingTypes)depItem.second).getType();
            break;
        case ReligiousDependency::ID:
            os << "ReligiousDependency: " << gGlobals.getReligionInfo((ReligionTypes)depItem.second).getType();
            break;
        case CityBonusDependency::ID:
            os << "CityBonusDependency: " << (depItem.second != NO_BONUS ? gGlobals.getBonusInfo((BonusTypes)depItem.second).getType() : "none?");
            break;
        case CivUnitDependency::ID:
            os << "CivUnitDependency: " << (depItem.second != NO_UNIT ? gGlobals.getUnitInfo((UnitTypes)depItem.second).getType() : "none?");
            break;
        case ResouceProductionBonusDependency::ID:
            os << "ResouceProductionBonusDependency: " << (depItem.second != NO_BONUS ? gGlobals.getBonusInfo((BonusTypes)depItem.second).getType() : "none?");
            break;
        case -1:
            os << "No Dependencies";
            break;
        default:
            os << "UNKNOWN Dependency?";
            break;
        }
        os << ' ';
    }

    // basically - get dependent items for given set of ignore flags
    // if ignore flags is set to none then dependent items will be added if not required (i.e. known techs, present dependent buildings)
    // if ignore flags are set - those items will be added instead as if they were required regardless
    // a special 'no dep' item is returned if all dependencies are required/satisfied (allowing for ignore flags) but none have been explicitly added
    std::vector<DependencyItem> ICityBuildingTactics::getDepItems(int ignoreFlags) const
    {
        std::vector<DependencyItem> depItems;
        const CvCity* pCity = ::getCity(getCity());
        if (!pCity)
        {
            return depItems;
        }
        bool allDependenciesSatisfied = true;

        const std::vector<IDependentTacticPtr>& deps = getDependencies();      
        for (size_t i = 0, count = deps.size(); i < count; ++i)
        {
            // e.g. if a dependency such as a religion present in city is not required or relevant ignore flag is set
            if (!deps[i]->required(pCity, ignoreFlags))
            {
                const std::vector<DependencyItem> thisDepItems = deps[i]->getDependencyItems();
                std::copy(thisDepItems.begin(), thisDepItems.end(), std::back_inserter(depItems));
            }
            else
            {
                allDependenciesSatisfied = false;
            }
        }

        const std::vector<ResearchTechDependencyPtr>& techDeps = getTechDependencies();
        for (size_t i = 0, count = techDeps.size(); i < count; ++i)
        {
            // i.e. if tech is not needed (so we know it) or tech ignore flag is set
            if (!techDeps[i]->required(pCity, ignoreFlags))
            {
                const std::vector<DependencyItem> thisDepItems = techDeps[i]->getDependencyItems();
                std::copy(thisDepItems.begin(), thisDepItems.end(), std::back_inserter(depItems));
            }
            else
            {
                allDependenciesSatisfied = false;
            }
        }

        // i.e. for the given ignore flags, all dependencies were satisfied and no dependent items exist for dependencies 
        if (allDependenciesSatisfied && depItems.empty()) 
        {
            // add a special 'no dependency' dependency so we can use it as a map key with other tactics
            depItems.push_back(std::make_pair(-1, -1));
        }

        return depItems;
    }
    
    ICityBuildingTacticsPtr ICityBuildingTactics::factoryRead(FDataStreamBase* pStream)
    {
        ICityBuildingTacticsPtr pCityBuildingTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case CityBuildingTactic::ID:
            pCityBuildingTactics = ICityBuildingTacticsPtr(new CityBuildingTactic());
            break;
        default:
            FAssertMsg(true, "Unexpected ID in ICityBuildingTactics::factoryRead");
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
            FAssertMsg(true, "Unexpected ID in IWorkerBuildTactic::factoryRead");
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
        case LimitedBuildingTactic::ID:
            pGlobalBuildingTactics = ILimitedBuildingTacticsPtr(new LimitedBuildingTactic());
            break;
        default:
            FAssertMsg(true, "Unexpected ID in IGlobalBuildingTactics::factoryRead");
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
        case ProcessTactic::ID:
            pProcessTactics = IProcessTacticsPtr(new ProcessTactic());
            break;
        default:
            FAssertMsg(true, "Unexpected ID in IProcessTactics::factoryRead");
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
        default:
            FAssertMsg(true, "Unexpected ID in ICityUnitTactic::factoryRead");
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
            FAssertMsg(true, "Unexpected ID in IBuiltUnitTactic::factoryRead");
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
            FAssertMsg(true, "Unexpected ID in ITechTactic::factoryRead");
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
        case PlayerTechTactics::ID:
            pTechTactics = ITechTacticsPtr(new PlayerTechTactics());
            break;
        default:
            FAssertMsg(true, "Unexpected ID in ITechTactics::factoryRead");
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
        default:
            FAssertMsg(true, "Unexpected ID in ICivicTactic::factoryRead");
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
            FAssertMsg(true, "Unexpected ID in IResourceTactic::factoryRead");
            break;
        }

        pResourceTactic->read(pStream);
        return pResourceTactic;
    }
}