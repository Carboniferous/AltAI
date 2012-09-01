#include "./tactics_interfaces.h"
#include "./building_tactics_deps.h"
#include "./building_tactics_items.h"
#include "./unit_tactics_items.h"
#include "./city_building_tactics.h"
#include "./city_unit_tactics.h"
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

    IUnitTacticsPtr IUnitTactics::factoryRead(FDataStreamBase* pStream)
    {
        IUnitTacticsPtr pUnitTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pUnitTactics = IUnitTacticsPtr(new UnitTactic());
            break;
        default:
            break;
        }

        pUnitTactics->read(pStream);
        return pUnitTactics;
    }
}