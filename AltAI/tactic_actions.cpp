#include "./tactic_actions.h"
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

    void ResearchTech::merge(const ResearchTech& other)
    {
        if (other.techType != techType)
        {
            return;
        }

        techFlags |= other.techFlags;
        economicFlags |= other.economicFlags;
        militaryFlags |= other.militaryFlags;
        workerFlags |= other.workerFlags;

        mergeVectors(possibleBuildings, other.possibleBuildings);
        mergeVectors(possibleUnits, other.possibleUnits);
        mergeVectors(possibleCivics, other.possibleCivics);
        mergeVectors(possibleBonuses, other.possibleBonuses);
        mergeVectors(possibleImprovements, other.possibleImprovements);
        mergeVectors(possibleProcesses, other.possibleProcesses);
        mergeVectors(possibleRemovableFeatures, other.possibleRemovableFeatures);

        for (ResearchTech::WorkerTechDataMap::const_iterator ci(other.workerTechDataMap.begin()), ciEnd(other.workerTechDataMap.end()); ci != ciEnd; ++ci)
        {
            ResearchTech::WorkerTechDataMap::const_iterator iter = workerTechDataMap.find(ci->first);
            if (iter == workerTechDataMap.end())
            {
                workerTechDataMap[ci->first] = ci->second;
            }
            else
            {
                iter->second->removableFeatureCounts = ci->second->removableFeatureCounts;
            }
        }
    }

    bool ResearchTech::hasNewImprovements(IDInfo city) const
    {
        WorkerTechDataMap::const_iterator ci = workerTechDataMap.find(city);
        return ci == workerTechDataMap.end() ? false : !ci->second->newImprovements.empty();
    }

    void ConstructItem::merge(const ConstructItem& other)
    {
        if (other.unitType != unitType || other.buildingType != buildingType || other.processType != processType || other.improvementType != improvementType)
        {
            return;
        }

        economicFlags |= other.economicFlags;
        militaryFlags |= other.militaryFlags;
        buildingFlags |= other.buildingFlags;

        mergeVectors(positiveBonuses, other.positiveBonuses);

        for (std::map<BuildTypes, int>::const_iterator ci(other.possibleBuildTypes.begin()), ciEnd(other.possibleBuildTypes.end()); ci != ciEnd; ++ci)
        {
            possibleBuildTypes.insert(*ci);
        }
    }

    void WorkerTechCityData::write(FDataStreamBase* pStream) const
    {
        writeMap(pStream, removableFeatureCounts);
        CityImprovementManager::writeImprovements(pStream, newImprovements);
    }

    void WorkerTechCityData::read(FDataStreamBase* pStream)
    {
        readMap<FeatureTypes, int, int, int>(pStream, removableFeatureCounts);
        CityImprovementManager::readImprovements(pStream, newImprovements);
    }

    void ResearchTech::write(FDataStreamBase* pStream) const
    {
        pStream->Write(techType);
        pStream->Write(targetTechType);
        pStream->Write(depth);
        pStream->Write(techFlags);
        pStream->Write(economicFlags);
        pStream->Write(militaryFlags);
        pStream->Write(workerFlags);

        writeVector(pStream, possibleBuildings);
        writeVector(pStream, possibleUnits);
        writeVector(pStream, possibleCivics);
        writeVector(pStream, possibleBonuses);
        writeVector(pStream, possibleImprovements);
        writeVector(pStream, possibleRemovableFeatures);
        writeVector(pStream, possibleProcesses);

        pStream->Write(workerTechDataMap.size());
        for (WorkerTechDataMap::const_iterator ci(workerTechDataMap.begin()), ciEnd(workerTechDataMap.end()); ci != ciEnd; ++ci)
        {
            if (ci->second)
            {
                ci->first.write(pStream);
                ci->second->write(pStream);
            }
        }
    }

    void ResearchTech::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&techType);
        pStream->Read((int*)&targetTechType);
        pStream->Read((int*)&depth);
        pStream->Read(&techFlags);
        pStream->Read(&economicFlags);
        pStream->Read(&militaryFlags);
        pStream->Read(&workerFlags);

        readVector<BuildingTypes, int>(pStream, possibleBuildings);
        readVector<UnitTypes, int>(pStream, possibleUnits);
        readVector<CivicTypes, int>(pStream, possibleCivics);
        readVector<BonusTypes, int>(pStream, possibleBonuses);
        readVector<ImprovementTypes, int>(pStream, possibleImprovements);
        readVector<FeatureTypes, int>(pStream, possibleRemovableFeatures);
        readVector<ProcessTypes, int>(pStream, possibleProcesses);

        size_t size;
        pStream->Read(&size);
        workerTechDataMap.clear();
        for (size_t i = 0; i < size; ++i)
        {
            IDInfo city;
            city.read(pStream);
            WorkerTechDataMap::iterator iter = workerTechDataMap.insert(std::make_pair(city, boost::shared_ptr<WorkerTechCityData>(new WorkerTechCityData()))).first;
            iter->second->read(pStream);
        }
    }

    void ConstructItem::write(FDataStreamBase* pStream) const
    {
        pStream->Write(buildingType);
        pStream->Write(unitType);
        pStream->Write(improvementType);
        pStream->Write(processType);

        writeVector(pStream, positiveBonuses);
        writeVector(pStream, requiredTechs);
        writeMap(pStream, possibleBuildTypes); 

        const size_t size = militaryFlagValuesMap.size();
        pStream->Write(size);
        for (MilitaryFlagValuesMap::const_iterator ci(militaryFlagValuesMap.begin()), ciEnd(militaryFlagValuesMap.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            pStream->Write(ci->second.first);
            pStream->Write(ci->second.second);
        }

        pStream->Write(economicFlags);
        pStream->Write(militaryFlags);
        pStream->Write(buildingFlags);
    }

    void ConstructItem::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&buildingType);
        pStream->Read((int*)&unitType);
        pStream->Read((int*)&improvementType);
        pStream->Read((int*)&processType);

        readVector<BonusTypes, int>(pStream, positiveBonuses);
        readVector<TechTypes, int>(pStream, requiredTechs);
        readMap<BuildTypes, int, int, int>(pStream, possibleBuildTypes); 
        
        size_t size;
        pStream->Read(&size);
        militaryFlagValuesMap.clear();

        for (size_t i = 0; i < size; ++i)
        {
            MilitaryFlags::Flags key;
            std::pair<int, int> value;
            pStream->Read((int*)&key);
            pStream->Read((int*)&value.first);
            pStream->Read((int*)&value.second);
            militaryFlagValuesMap.insert(std::make_pair(key, value));
        }

        pStream->Read(&economicFlags);
        pStream->Read(&militaryFlags);
        pStream->Read(&buildingFlags);
    }
}