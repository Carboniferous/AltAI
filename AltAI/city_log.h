#pragma once

#include "./utils.h"

namespace AltAI
{
    struct CityLogFileHandles;
    class CityData;
    struct HurryData;
    class CityOptimiser;
    struct BuildingSimulationResults;

    class CityLog : boost::noncopyable
    {
    public:
        friend struct CityLogFileHandles;

        static boost::shared_ptr<CityLog> getLog(const CvCity* pCity);

        void logBuilding(BuildingTypes buildingType);
        void logHurryBuilding(BuildingTypes buildingType, const HurryData& hurryData);
        void logTurn(int turn, TotalOutput output);
        void logCityData(const boost::shared_ptr<CityData>& pCityData);
        void logCultureData(const CityData& cityData);
        void logUpgradeData(const CityData& cityData);
        void logCultureLevelChange(CultureLevelTypes oldLevel, CultureLevelTypes newLevel);
        void logPlotControlChange(const std::vector<std::pair<PlayerTypes, XYCoords> >& data);
        void logPlots(const CityOptimiser& cityOptimiser, bool printAllPlots = false);
        void logOptimisedWeights(TotalOutput maxOutputs, TotalOutputWeights optWeights);
        void logBestBuilding(BuildingTypes buildingType, TotalOutput output, const std::string& optType);
        void logSimulationResults(const BuildingSimulationResults& results);

        std::ofstream& getStream() { return logFile_; }

    private:
        explicit CityLog(const CvCity* pCity);

        std::ofstream logFile_;
    };
}