#include "AltAI.h"

#include "./tactic_actions.h"
#include "./tactics_interfaces.h"
#include "./save_utils.h"

namespace AltAI
{
    namespace
    {
        template <typename  T>
            void mergeVectors(std::vector<T>& destination, const std::vector<T>& source)
        {
            for (size_t i = 0, count = source.size(); i < count; ++i)
            {
                if (std::find(destination.begin(), destination.end(), source[i]) == destination.end())
                {
                    destination.push_back(source[i]);
                }
            }
        }
    }

    //void ResearchTech::merge(const ResearchTech& other)
    //{
    //    // can only merge if NO_TECH set yet or techs match
    //    if (other.techType != techType && !(other.techType == NO_TECH || techType == NO_TECH))
    //    {
    //        return;
    //    }

    //    techFlags |= other.techFlags;
    //    economicFlags |= other.economicFlags;
    //    militaryFlags |= other.militaryFlags;
    //    workerFlags |= other.workerFlags;
    //    victoryFlags |= other.victoryFlags;

    //    mergeVectors(possibleBuildings, other.possibleBuildings);
    //    mergeVectors(possibleProjects, other.possibleProjects);
    //    mergeVectors(possibleUnits, other.possibleUnits);
    //    mergeVectors(possibleCivics, other.possibleCivics);
    //    mergeVectors(possibleBonuses, other.possibleBonuses);
    //    mergeVectors(possibleImprovements, other.possibleImprovements);
    //    mergeVectors(possibleProcesses, other.possibleProcesses);
    //    mergeVectors(possibleRemovableFeatures, other.possibleRemovableFeatures);

    //    for (ResearchTech::WorkerTechDataMap::const_iterator ci(other.workerTechDataMap.begin()), ciEnd(other.workerTechDataMap.end()); ci != ciEnd; ++ci)
    //    {
    //        ResearchTech::WorkerTechDataMap::const_iterator iter = workerTechDataMap.find(ci->first);
    //        if (iter == workerTechDataMap.end())
    //        {
    //            workerTechDataMap[ci->first] = ci->second;
    //        }
    //        else
    //        {
    //            iter->second->removableFeatureCounts = ci->second->removableFeatureCounts;
    //        }
    //    }

    //    if (techType == NO_TECH && other.techType != NO_TECH)
    //    {
    //        techType = other.techType; 
    //    }
    //}

    //bool ResearchTech::hasNewImprovements(IDInfo city) const
    //{
    //    WorkerTechDataMap::const_iterator ci = workerTechDataMap.find(city);
    //    return ci == workerTechDataMap.end() ? false : !ci->second->newImprovements.empty();
    //}

    //void ConstructItem::merge(const ConstructItem& other)
    //{
    //    if (other.unitType != unitType || other.buildingType != buildingType || other.projectType != projectType ||
    //        other.processType != processType || other.improvementType != improvementType)
    //    {
    //        return;
    //    }

    //    economicFlags |= other.economicFlags;
    //    militaryFlags |= other.militaryFlags;
    //    buildingFlags |= other.buildingFlags;
    //    victoryFlags |= other.victoryFlags;

    //    mergeVectors(positiveBonuses, other.positiveBonuses);
    //    mergeVectors(religionTypes, other.religionTypes);

    //    for (std::map<BuildTypes, int>::const_iterator ci(other.possibleBuildTypes.begin()), ciEnd(other.possibleBuildTypes.end()); ci != ciEnd; ++ci)
    //    {
    //        possibleBuildTypes.insert(*ci);
    //    }
    //}

    //void WorkerTechCityData::write(FDataStreamBase* pStream) const
    //{
    //    writeMap(pStream, removableFeatureCounts);
    //    CityImprovementManager::writeImprovements(pStream, newImprovements);
    //}

    //void WorkerTechCityData::read(FDataStreamBase* pStream)
    //{
    //    readMap<FeatureTypes, int, int, int>(pStream, removableFeatureCounts);
    //    CityImprovementManager::readImprovements(pStream, newImprovements);
    //}

    void ResearchTech::write(FDataStreamBase* pStream) const
    {
        pStream->Write(techType);
        pStream->Write(targetTechType);
        pStream->Write(depth);
    //    pStream->Write(techFlags);
    //    pStream->Write(economicFlags);
    //    pStream->Write(militaryFlags);
    //    pStream->Write(workerFlags);
    //    pStream->Write(victoryFlags);

    //    writeVector(pStream, possibleBuildings);
    //    writeVector(pStream, possibleProjects);
    //    writeVector(pStream, possibleUnits);
    //    writeVector(pStream, possibleCivics);
    //    writeVector(pStream, possibleBonuses);
    //    writeVector(pStream, possibleImprovements);
    //    writeVector(pStream, possibleRemovableFeatures);
    //    writeVector(pStream, possibleProcesses);

    //    pStream->Write(workerTechDataMap.size());
    //    for (WorkerTechDataMap::const_iterator ci(workerTechDataMap.begin()), ciEnd(workerTechDataMap.end()); ci != ciEnd; ++ci)
    //    {
    //        if (ci->second)
    //        {
    //            ci->first.write(pStream);
    //            ci->second->write(pStream);
    //        }
    //    }
    }

    void ResearchTech::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&techType);
        pStream->Read((int*)&targetTechType);
        pStream->Read((int*)&depth);
    //    pStream->Read(&techFlags);
    //    pStream->Read(&economicFlags);
    //    pStream->Read(&militaryFlags);
    //    pStream->Read(&workerFlags);
    //    pStream->Read(&victoryFlags);

    //    readVector<BuildingTypes, int>(pStream, possibleBuildings);
    //    readVector<ProjectTypes, int>(pStream, possibleProjects);
    //    readVector<UnitTypes, int>(pStream, possibleUnits);
    //    readVector<CivicTypes, int>(pStream, possibleCivics);
    //    readVector<BonusTypes, int>(pStream, possibleBonuses);
    //    readVector<ImprovementTypes, int>(pStream, possibleImprovements);
    //    readVector<FeatureTypes, int>(pStream, possibleRemovableFeatures);
    //    readVector<ProcessTypes, int>(pStream, possibleProcesses);

    //    size_t size;
    //    pStream->Read(&size);
    //    workerTechDataMap.clear();
    //    for (size_t i = 0; i < size; ++i)
    //    {
    //        IDInfo city;
    //        city.read(pStream);
    //        WorkerTechDataMap::iterator iter = workerTechDataMap.insert(std::make_pair(city, boost::shared_ptr<WorkerTechCityData>(new WorkerTechCityData()))).first;
    //        iter->second->read(pStream);
    //    }
    }

    void ConstructItem::write(FDataStreamBase* pStream) const
    {
        pStream->Write(buildingType);
        pStream->Write(unitType);
        pStream->Write(projectType);
        pStream->Write(improvementType);
        pStream->Write(processType);

        pStream->Write(buildTarget.iX);
        pStream->Write(buildTarget.iY);

    //    writeVector(pStream, religionTypes);
    //    writeVector(pStream, positiveBonuses);
    //    writeVector(pStream, requiredTechs);
    //    writeMap(pStream, possibleBuildTypes); 

    //    const size_t size = militaryFlagValuesMap.size();
    //    pStream->Write(size);
    //    for (MilitaryFlagValuesMap::const_iterator ci(militaryFlagValuesMap.begin()), ciEnd(militaryFlagValuesMap.end()); ci != ciEnd; ++ci)
    //    {
    //        pStream->Write(ci->first);
    //        pStream->Write(ci->second.first);
    //        pStream->Write(ci->second.second);
    //    }

    //    pStream->Write(economicFlags);
    //    pStream->Write(militaryFlags);
    //    pStream->Write(buildingFlags);
    //    pStream->Write(victoryFlags);

    //    writeComplexVector(pStream, prerequisites);
    }

    void ConstructItem::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&buildingType);
        pStream->Read((int*)&unitType);
        pStream->Read((int*)&projectType);
        pStream->Read((int*)&improvementType);
        pStream->Read((int*)&processType);

        pStream->Read(&buildTarget.iX);
        pStream->Read(&buildTarget.iY);        

    //    readVector<ReligionTypes, int>(pStream, religionTypes);
    //    readVector<BonusTypes, int>(pStream, positiveBonuses);
    //    readVector<TechTypes, int>(pStream, requiredTechs);
    //    readMap<BuildTypes, int, int, int>(pStream, possibleBuildTypes); 
    //    
    //    size_t size;
    //    pStream->Read(&size);
    //    militaryFlagValuesMap.clear();

    //    for (size_t i = 0; i < size; ++i)
    //    {
    //        MilitaryFlags::Flags key;
    //        std::pair<int, int> value;
    //        pStream->Read((int*)&key);
    //        pStream->Read((int*)&value.first);
    //        pStream->Read((int*)&value.second);
    //        militaryFlagValuesMap.insert(std::make_pair(key, value));
    //    }

    //    pStream->Read(&economicFlags);
    //    pStream->Read(&militaryFlags);
    //    pStream->Read(&buildingFlags);
    //    pStream->Read(&victoryFlags);

    //    readComplexVector<ConstructItem>(pStream, prerequisites);
    }

    void ConstructItem::debug(std::ostream& os) const
    {
        if (buildingType != NO_BUILDING)
        {
            os << " building: " << gGlobals.getBuildingInfo(buildingType).getType();
        }
        if (unitType != NO_UNIT)
        {
            os << " unit: " << gGlobals.getUnitInfo(unitType).getType();
        }
        if (projectType != NO_PROJECT)
        {
            os << " project: " << gGlobals.getProjectInfo(projectType).getType();
        }
        if (processType != NO_PROCESS)
        {
            os << " process: " << gGlobals.getProcessInfo(processType).getType();
        }
        if (improvementType != NO_IMPROVEMENT)
        {
            os << " improvement: " << gGlobals.getImprovementInfo(improvementType).getType();
        }
        if (buildTarget != XYCoords())
        {
            os << " build target: " << buildTarget;
        }
    }
}