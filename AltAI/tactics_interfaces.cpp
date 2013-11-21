#include "AltAI.h"

#include "./tactics_interfaces.h"
#include "./building_tactics_deps.h"
#include "./building_tactics_items.h"
#include "./unit_tactics_items.h"
#include "./city_building_tactics.h"
#include "./city_unit_tactics.h"
#include "./player_tech_tactics.h"
#include "./tech_tactics_items.h"

namespace AltAI
{
    IDependentTacticPtr IDependentTactic::factoryRead(FDataStreamBase* pStream)
    {
        IDependentTacticPtr pDependentTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pDependentTactic = IDependentTacticPtr(new ResearchTechDependency());
            break;
        case 1:
            pDependentTactic = IDependentTacticPtr(new CityBuildingDependency());
            break;
        case 2:
            pDependentTactic = IDependentTacticPtr(new CivBuildingDependency());
            break;
        case 3:
            pDependentTactic = IDependentTacticPtr(new ReligiousDependency());
            break;
        case 4:
            pDependentTactic = IDependentTacticPtr(new CityBonusDependency());
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
        case 0:
            pBuildingTactic = ICityBuildingTacticPtr(new EconomicBuildingTactic());
            break;
        case 1:
            pBuildingTactic = ICityBuildingTacticPtr(new FoodBuildingTactic());
            break;
        case 2:
            pBuildingTactic = ICityBuildingTacticPtr(new HappyBuildingTactic());
            break;
        case 3:
            pBuildingTactic = ICityBuildingTacticPtr(new HealthBuildingTactic());
            break;
        case 4:
            pBuildingTactic = ICityBuildingTacticPtr(new ScienceBuildingTactic());
            break;
        case 5:
            pBuildingTactic = ICityBuildingTacticPtr(new GoldBuildingTactic());
            break;
        case 6:
            pBuildingTactic = ICityBuildingTacticPtr(new CultureBuildingTactic());
            break;
        case 7:
            pBuildingTactic = ICityBuildingTacticPtr(new EspionageBuildingTactic());
            break;
        case 8:
            pBuildingTactic = ICityBuildingTacticPtr(new SpecialistBuildingTactic());
            break;
        case 9:
            pBuildingTactic = ICityBuildingTacticPtr(new GovCenterTactic());
            break;
        case 10:
            pBuildingTactic = ICityBuildingTacticPtr(new UnitExperienceTactic());
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
                depItems.push_back(deps[i]->getDependencyItem());
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
                depItems.push_back(techDeps[i]->getDependencyItem());
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

    std::vector<DependencyItem> ICityUnitTactics::getDepItems(int ignoreFlags) const
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
                depItems.push_back(deps[i]->getDependencyItem());
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
        case 0:
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
        case 0:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new EconomicImprovementTactic());
            break;
        case 1:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new RemoveFeatureTactic());
            break;
        case 2:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new ProvidesResourceTactic());
            break;
        case 3:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new HappyImprovementTactic());
            break;
        case 4:
            pWorkerBuildTactic = IWorkerBuildTacticPtr(new HealthImprovementTactic());
            break;
        case 5:
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
        case 0:
            pGlobalBuildingTactics = ILimitedBuildingTacticsPtr(new GlobalBuildingTactic());
            break;
        case 1:
            pGlobalBuildingTactics = ILimitedBuildingTacticsPtr(new NationalBuildingTactic());
            break;
        default:
            break;
        }

        pGlobalBuildingTactics->read(pStream);
        return pGlobalBuildingTactics;
    }

    ICityImprovementTacticsPtr ICityImprovementTactics::factoryRead(FDataStreamBase* pStream)
    {
        ICityImprovementTacticsPtr pCityImprovementTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pCityImprovementTactics = ICityImprovementTacticsPtr(new CityImprovementTactics());
            break;
        default:
            break;
        }

        pCityImprovementTactics->read(pStream);
        return pCityImprovementTactics;
    }

    IProcessTacticsPtr IProcessTactics::factoryRead(FDataStreamBase* pStream)
    {
        IProcessTacticsPtr pProcessTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
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
        case 0:
            pCityUnitTactic = ICityUnitTacticPtr(new CityDefenceUnitTactic());
            break;
        case 1:
            pCityUnitTactic = ICityUnitTacticPtr(new CityAttackUnitTactic());
            break;
        case 2:
            pCityUnitTactic = ICityUnitTacticPtr(new CollateralUnitTactic());
            break;
        case 3:
            pCityUnitTactic = ICityUnitTacticPtr(new FieldDefenceUnitTactic());
            break;
        case 4:
            pCityUnitTactic = ICityUnitTacticPtr(new FieldAttackUnitTactic());
            break;
        case 5:
            pCityUnitTactic = ICityUnitTacticPtr(new BuildCityUnitTactic());
            break;
        case 6:
            pCityUnitTactic = ICityUnitTacticPtr(new BuildImprovementsUnitTactic());
            break;
        case 7:
            pCityUnitTactic = ICityUnitTacticPtr(new SeaAttackUnitTactic());
            break;
        default:
            break;
        }

        pCityUnitTactic->read(pStream);
        return pCityUnitTactic;
    }

    ICityUnitTacticsPtr ICityUnitTactics::factoryRead(FDataStreamBase* pStream)
    {
        ICityUnitTacticsPtr pCityUnitTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pCityUnitTactics = ICityUnitTacticsPtr(new CityUnitTactic());
            break;
        default:
            break;
        }

        pCityUnitTactics->read(pStream);
        return pCityUnitTactics;
    }

    IUnitTacticPtr IUnitTactic::factoryRead(FDataStreamBase* pStream)
    {
        IUnitTacticPtr pUnitTactic;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pUnitTactic = IUnitTacticPtr(new DiscoverTechUnitTactic());
            break;
        case 1:
            pUnitTactic = IUnitTacticPtr(new BuildSpecialBuildingUnitTactic());
            break;
        case 2:
            pUnitTactic = IUnitTacticPtr(new CreateGreatWorkUnitTactic());
            break;
        case 3:
            pUnitTactic = IUnitTacticPtr(new TradeMissionUnitTactic());
            break;
        default:
            break;
        }

        pUnitTactic->read(pStream);
        return pUnitTactic;
    }

    IUnitTacticsPtr IUnitTactics::factoryRead(FDataStreamBase* pStream)
    {
        IUnitTacticsPtr pUnitTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case -1:
            return IUnitTacticsPtr();  // Null tactic - valid for unit tactics if no city can build the unit currently
        case 0:
            pUnitTactics = IUnitTacticsPtr(new UnitTactic());
            break;
        default:
            break;
        }

        pUnitTactics->read(pStream);
        return pUnitTactics;
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
        case 0:
            pTechTactic = ITechTacticPtr(new FreeTechTactic());
            break;
        case 1:
            pTechTactic = ITechTacticPtr(new FoundReligionTechTactic());
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
        default:
            break;
        case 0:
            pTechTactics = ITechTacticsPtr(new PlayerTechTactics());
            break;
        }

        pTechTactics->read(pStream);
        return pTechTactics;
    }
}