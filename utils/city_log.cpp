#include "./city_log.h"
#include "./city_data.h"
#include "./city_optimiser.h"
#include "./city_simulator.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        std::string makeFileName(const CvCity* pCity)
        {
            std::ostringstream oss;
            oss << getLogDirectory() << narrow(pCity->getName()) << pCity->getOwner() << ".txt";
            return oss.str();
        }
    }

    struct CityLogFileHandles
    {
        typedef std::map<std::string, boost::shared_ptr<CityLog> > HandleMap;
        static HandleMap logFiles;

        static boost::shared_ptr<CityLog> getHandle(const CvCity* pCity)
        {
            const std::string name(makeFileName(pCity));

            HandleMap::iterator iter(logFiles.find(name));
            if (iter == logFiles.end())
            {
                return logFiles.insert(std::make_pair(name, boost::shared_ptr<CityLog>(new CityLog(pCity)))).first->second;
            }
            else
            {
                return iter->second;
            }
        }
    };

    CityLogFileHandles::HandleMap CityLogFileHandles::logFiles;
    

    boost::shared_ptr<CityLog> CityLog::getLog(const CvCity* pCity) 
    {
        return CityLogFileHandles::getHandle(pCity);
    }

    void CityLog::logBuilding(BuildingTypes buildingType)
    {
#ifdef ALTAI_DEBUG
        if (buildingType != NO_BUILDING)
        {
            const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);
            logFile_ << "\n" << buildingInfo.getType() << "\n";
        }
        else
        {
            logFile_ << "\nNO_BUILDING\n";
        }
#endif
    }

    void CityLog::logHurryBuilding(BuildingTypes buildingType, const HurryData& hurryData)
    {
#ifdef ALTAI_DEBUG
        const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);
        const CvHurryInfo& hurryInfo = gGlobals.getHurryInfo(hurryData.hurryType);

        logFile_ << "\n" << buildingInfo.getType() << " (" << hurryInfo.getType() << " cost = ";
        if (hurryData.hurryPopulation > 0)
        {
            logFile_ << hurryData.hurryPopulation << " pop ";
        }
        if (hurryData.hurryGold > 0)
        {
            logFile_ << hurryData.hurryGold << " gold ";
        }
        logFile_ << ")\n";
#endif
    }

    void CityLog::logTurn(int turn, TotalOutput output)
    {
#ifdef ALTAI_DEBUG
        logFile_ << "\n" << turn << " = " << output << " ";
#endif
    }

    void CityLog::logCityData(const boost::shared_ptr<CityData>& pCityData)
    {
#ifdef ALTAI_DEBUG
        pCityData->debugBasicData(logFile_);
#endif
    }

    void CityLog::logCultureData(const CityData& cityData)
    {
#ifdef ALTAI_DEBUG
        cityData.debugCultureData(logFile_);
#endif
    }

    void CityLog::logUpgradeData(const CityData& cityData)
    {
#ifdef ALTAI_DEBUG
        cityData.debugUpgradeData(logFile_);
#endif
    }

    void CityLog::logCultureLevelChange(CultureLevelTypes oldLevel, CultureLevelTypes newLevel)
    {
#ifdef ALTAI_DEBUG
        logFile_ << "\nCulture level change to level: " << newLevel << " from level: " << oldLevel << "\n";
#endif
    }

    void CityLog::logPlotControlChange(const std::vector<std::pair<PlayerTypes, XYCoords> >& data)
    {
#ifdef ALTAI_DEBUG
        for (size_t i = 0, count = data.size(); i < count; ++i)
        {
            logFile_ << "\nPlot: " << data[i].second << " now controlled by player: " << data[i].first << "\n";
        }
#endif
    }

    void CityLog::logPlots(const CityOptimiser& cityOptimiser, bool printAllPlots)
    {
#ifdef ALTAI_DEBUG
        logFile_ << "\n";
        cityOptimiser.debug(logFile_, printAllPlots);
#endif
    }

    void CityLog::logOptimisedWeights(TotalOutput maxOutputs, TotalOutputWeights optWeights)
    {
#ifdef ALTAI_DEBUG
        logFile_ << "\nWeights = " << optWeights << ", max output = " << maxOutputs << "\n";
#endif
    }

    void CityLog::logBestBuilding(BuildingTypes buildingType, TotalOutput output, const std::string& optType)
    {
#ifdef ALTAI_DEBUG
        logFile_ << "\nBuilding (" << optType << ") = ";
        if (buildingType == NO_BUILDING)
        {
            logFile_ << "None";
        }
        else
        {
            logFile_ << gGlobals.getBuildingInfo(buildingType).getType() << ", output = " << output;
        }
        logFile_ << "\n";
#endif
    }

    void CityLog::logSimulationResults(const BuildingSimulationResults& results)
    {
#ifdef ALTAI_DEBUG
        results.debugResults(logFile_);
#endif
    }

    CityLog::CityLog(const CvCity* pCity) : logFile_(makeFileName(pCity).c_str())
    {
    }
}