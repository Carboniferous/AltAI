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
        default:
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
            os << gGlobals.getTechInfo((TechTypes)depItem.second).getType();
            break;
        case CityBuildingDependency::ID:
            os << gGlobals.getBuildingInfo((BuildingTypes)depItem.second).getType();
            break;
        case CivBuildingDependency::ID:
            os << gGlobals.getBuildingInfo((BuildingTypes)depItem.second).getType();
            break;
        case ReligiousDependency::ID:
            os << gGlobals.getReligionInfo((ReligionTypes)depItem.second).getType();
            break;
        case CityBonusDependency::ID:
            os << (depItem.second != NO_BONUS ? gGlobals.getBonusInfo((BonusTypes)depItem.second).getType() : "");
            break;
        case CivUnitDependency::ID:
            os << (depItem.second != NO_UNIT ? gGlobals.getUnitInfo((UnitTypes)depItem.second).getType() : "");
            break;
        case -1:
            os << "No Dependencies ";
            break;
        default:
            os << "UNKNOWN Dependency?";
            break;
        }
    }

    std::vector<DependencyItem> ICityBuildingTactics::getDepItems(int ignoreFlags) const
    {
        std::vector<DependencyItem> depItems;
        const CvCity* pCity = ::getCity(getCity());
        if (!pCity)
        {
            return depItems;
        }
        bool allDependenciesSatisfied = true;

        const std::vector<IDependentTacticPtr> deps = getDependencies();      
        for (size_t i = 0, count = deps.size(); i < count; ++i)
        {
            if (!deps[i]->required(pCity, ignoreFlags))
            {
                const std::vector<DependencyItem>& thisDepItems = deps[i]->getDependencyItems();
                std::copy(thisDepItems.begin(), thisDepItems.end(), std::back_inserter(depItems));
            }
            else
            {
                allDependenciesSatisfied = false;
            }
        }

        const std::vector<ResearchTechDependencyPtr> techDeps = getTechDependencies();
        for (size_t i = 0, count = techDeps.size(); i < count; ++i)
        {
            if (!techDeps[i]->required(pCity, ignoreFlags))
            {
                const std::vector<DependencyItem>& thisDepItems = techDeps[i]->getDependencyItems();
                std::copy(thisDepItems.begin(), thisDepItems.end(), std::back_inserter(depItems));
            }
            else
            {
                allDependenciesSatisfied = false;
            }
        }

        if (allDependenciesSatisfied && depItems.empty())
        {
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
        default:
            break;
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
            return boost::shared_ptr<PlayerTechTactics>();  // Null tactic - valid for tech tactics - some techs have no tactics of type ITechTactic
        case PlayerTechTactics::ID:
            pTechTactics = ITechTacticsPtr(new PlayerTechTactics());
            break;
        default:
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
        default:
            break;
        }

        pResourceTactic->read(pStream);
        return pResourceTactic;
    }
}