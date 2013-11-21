#pragma once

#include "./utils.h"
#include "./plot_info.h"
#include "./dot_map.h"
#include "./shared_plot.h"
#include "./city_improvements.h"

#include "boost/enable_shared_from_this.hpp"

namespace AltAI
{
    class IPlotEvent;
    class Player;

    class MapAnalysis : public boost::enable_shared_from_this<MapAnalysis>
    {
    public:
        static const int DotMapTechDepth = 3, BarbDotMapTechDepth = 1;

        struct AreaDetail
        {
            explicit AreaDetail(int ID_);
            int ID;
            int knownTileCount;
            bool allKnown;
            std::set<int> enclosedAreas;
            std::set<int> borderingAreas;
        };

        struct ResourceData
        {
            ResourceData() : subAreaID(FFreeList::INVALID_INDEX) {}
            explicit ResourceData(int subAreaID_) : subAreaID(subAreaID_) {}
            int subAreaID;
            typedef std::vector<std::pair<XYCoords, PlayerTypes> > ResourcePlotData;
            std::map<BonusTypes, ResourcePlotData> subAreaResourcesMap;
        };

        struct PlotValues
        {
            // map of plots to possible squares a city on that plot could use, the squares are stored by their plot key, so similar squares will be together in a set
            typedef std::map<int /* plot key */, std::set<XYCoords> > PlotKeyCoordsMap;
            typedef std::map<XYCoords, PlotKeyCoordsMap> SubAreaPlotValueMap;
            typedef std::map<int /* subarea id */, SubAreaPlotValueMap> PlotValueMap;
            PlotValueMap plotValueMap;
            // map of plot keys to possible improvements and their yields
            typedef std::vector<std::pair<PlotYield, ImprovementTypes> > ImprovementsAndYields;
            typedef std::map<int /* plot key */, ImprovementsAndYields> KeysValueMap;
            KeysValueMap keysValueMap;
        };

        explicit MapAnalysis(Player& player);
        void init();

        void reinitDotMap();
        
        void pushPlotEvent(const boost::shared_ptr<IPlotEvent>& pEvent);
        void update();

        void updatePlotRevealed(const CvPlot* pPlot);
        void updatePlotFeature(const CvPlot* pPlot, FeatureTypes oldFeatureType);
        void updatePlotCulture(const CvPlot* pPlot, bool remove);
        void updateResourceData(const std::vector<BonusTypes>& revealedBonusTypes);
        void updatePlotBonus(const CvPlot* pPlot, BonusTypes bonusType);

        void debugResourceData() const;
        std::vector<int> getAccessibleSubAreas(DomainTypes domainType) const;
        int getControlledResourceCount(BonusTypes bonusType) const;
        std::vector<CvPlot*> getResourcePlots(const std::vector<BonusTypes>& bonusTypes, int subArea) const;

        const PlotInfo::PlotInfoNode& getPlotInfoNode(const CvPlot* pPlot);
        const Player& getPlayer() const { return player_; }

        const PlotValues& getPlotValues() const;

        void analyseSharedPlots();
        void analyseSharedPlots(const std::set<IDInfo>& cities);
        void addCity(const CvCity* pCity);
        void deleteCity(const CvCity* pCity);
        void reassignUnworkedSharedPlots(IDInfo city);
        void assignSharedPlotImprovements(IDInfo city);
        SharedPlotItemPtr getSharedPlot(IDInfo city, XYCoords coords) const;
        void markIrrigationChainSourcePlot(XYCoords coords, ImprovementTypes improvementType);

        void calcImprovements(const std::set<IDInfo>& cities);
        void calcImprovements(IDInfo city);

        CityImprovementManager& getImprovementManager(IDInfo city);

        void debug(std::ostream& os) const;
        void debugSharedPlots() const;

        IDInfo setWorkingCity(const CvPlot* pPlot, const CvCity* pOldCity, const CvCity* pNewCity);
        const CvCity* getWorkingCity(IDInfo city, XYCoords coords) const;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        typedef std::map<int, AreaDetail> AreaMap;
        typedef std::map<int, AreaDetail>::iterator AreaMapIter;
        typedef std::map<int, AreaDetail>::const_iterator AreaMapConstIter;
        typedef std::map<int, AreaDetail> SubAreaMap;
        typedef std::map<int, AreaDetail>::iterator SubAreaMapIter;
        typedef std::map<int, AreaDetail>::const_iterator SubAreaMapConstIter;

        int updateAreaCounts_(int areaID);
        int updateSubAreaCounts_(int subAreaID);
        void updateAreaBorders_(const CvPlot* pPlot, int areaID, int subAreaID);
        void updateAreaComplete_(int areaID);
        void updateSubAreaComplete_(int subAreaID);

        void updateResourceData_(const CvPlot* pPlot);

        void addDotMapPlot_(const CvPlot* pPlot, const PlotInfo::PlotInfoNode& plotInfo);
        void updateKeysValueMap_(int key, const PlotInfo::PlotInfoNode& plotInfo);

        void updatePlotValueKey_(const CvPlot* pPlot, int oldKey, int newKey);
        void removePlotValuePlot_(const CvPlot* pPlot);

        void setWorkingCity_(IDInfo possibleCity, XYCoords coords, IDInfo assignedCity);

        bool init_;
        Player& player_;
        
        AreaMap revealedAreaDataMap_;
        SubAreaMap revealedSubAreaDataMap_;
        // key = sub area id
        typedef std::map<int, ResourceData> ResourcesMap;
        typedef ResourcesMap::iterator ResourcesMapIter;
        typedef ResourcesMap::const_iterator ResourcesMapConstIter;
        ResourcesMap resourcesMap_;

        // value is plot key
        typedef std::map<XYCoords, int> KeyInfoMap;
        KeyInfoMap keyInfoMap_;

        // key is plot key
        typedef std::map<int, PlotInfo::PlotInfoNode> PlotInfoMap;
        PlotInfoMap plotInfoMap_;

        std::pair<int, PlotInfoMap::iterator> addPlotInfo_(const CvPlot* pPlot);
        std::pair<int, PlotInfoMap::iterator> updatePlotInfo_(const CvPlot* pPlot);

        PlotValues plotValues_, previousPlotValues_;

        typedef std::set<DotMapItem> DotMap;
        DotMap dotMap_;
        std::map<int, XYCoords, std::greater<int> > bestSites_;

        typedef std::set<CitySharedPlots> SharedPlots;
        SharedPlots sharedPlots_;

        typedef std::set<CityImprovementManager> CitiesImprovements;
        CitiesImprovements citiesImprovements_;

        void addSharedPlotToCity_(IDInfo city, XYCoords coords, IDInfo otherCity);
        void deleteSharedCity_(IDInfo city, IDInfo sharedCity);
        void analyseSharedPlot_(SharedPlots::iterator iter, CitySharedPlots::SharedPlotsIter plotsIter);
    };
}