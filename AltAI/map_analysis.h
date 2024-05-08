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

        struct ResourcePlotData
        {
            ResourcePlotData() : coords(-1, -1), owner(NO_PLAYER), imp(NO_IMPROVEMENT) {}
            ResourcePlotData(XYCoords coords_, PlayerTypes owner_, ImprovementTypes imp_) : coords(coords_), owner(owner_), imp(imp_) {}
            XYCoords coords;
            PlayerTypes owner;
            ImprovementTypes imp;
        };

        struct ResourceData
        {
            ResourceData() : subAreaID(FFreeList::INVALID_INDEX) {}
            explicit ResourceData(int subAreaID_) : subAreaID(subAreaID_) {}
            int subAreaID;
            std::map<BonusTypes, std::vector<ResourcePlotData> > subAreaResourcesMap;
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
        void recalcPlotInfo();
        void update();

        void updatePlotInfo(const CvPlot* pPlot, bool isNew, const std::string& caller);
        bool updatePlotRevealed(const CvPlot* pPlot, bool isNew);
        void updatePlotFeature(const CvPlot* pPlot, FeatureTypes oldFeatureType);
        void updatePlotImprovement(const CvPlot* pPlot, ImprovementTypes oldImprovementType);
        void updatePlotCulture(const CvPlot* pPlot, PlayerTypes previousRevealedOwner, PlayerTypes newRevealedOwner);
        void updateResourceData(const std::vector<BonusTypes>& revealedBonusTypes);
        void updatePlotBonus(const CvPlot* pPlot, BonusTypes bonusType);

        void debugResourceData() const;
        std::vector<int> getAccessibleSubAreas(DomainTypes domainType) const;
        bool isAreaComplete(int areaID) const;
        bool isSubAreaComplete(int subAreaID) const;
        int getControlledResourceCount(BonusTypes bonusType) const;
        std::vector<CvPlot*> getResourcePlots(int subArea, const std::vector<BonusTypes>& bonusTypes = std::vector<BonusTypes>(), PlayerTypes playerType = NO_PLAYER, const int lookaheadTurns = 0) const;
        void getResources(std::set<BonusTypes>& owned, std::set<BonusTypes>& unowned) const;
        const CvPlot* getClosestCity(const CvPlot* pPlot, int subArea, bool includeActsAsCity, IDInfo& closestCity = IDInfo()) const;
        bool getTurnsToOwnership(const CvPlot* pPlot, bool includeOtherPlayers, IDInfo& owningCity, int& turns) const;
        std::vector<XYCoords> getGoodyHuts(int subArea) const;

        const std::map<int /* sub area id */, std::set<XYCoords> >& getBorderMap() const;
        const std::map<int /* sub area id */, std::set<XYCoords> >& getUnrevealedBorderMap() const;
        int getUnrevealedBorderCount(int subAreaId) const;
        bool isOurBorderPlot(int subAreaId, XYCoords coords) const;
        std::vector<int /* area id */> getAreasBorderingArea(int areaId) const;
        std::vector<int /* sub area id */ > getSubAreasInArea(int areaId) const;
        std::map<int /* sub area id */, std::vector<IDInfo> > getSubAreaCityMap() const;

        const PlotInfo::PlotInfoNode& getPlotInfoNode(const CvPlot* pPlot);
        const Player& getPlayer() const { return player_; }

        const PlotValues& getPlotValues();
        bool plotValuesDirty() const;

        void analyseSharedPlots(const std::set<IDInfo>& cities);
        void addCity(const CvCity* pCity);
        void deleteCity(const CvCity* pCity);
        void reassignUnworkedSharedPlots(IDInfo city);
        std::set<XYCoords> getCitySharedPlots(IDInfo city) const;
 
        bool isSharedPlot(XYCoords coords) const;
        IDInfo getSharedPlotAssignedCity(XYCoords coords) const;
        IDInfo getSharedPlot(IDInfo city, XYCoords coords) const;
        IDInfo getImprovementOwningCity(IDInfo city, XYCoords coords) const;
        IDInfo setImprovementOwningCity(IDInfo city, XYCoords coords);
        void markIrrigationChainSourcePlot(XYCoords coords, ImprovementTypes improvementType);

        void debug(std::ostream& os) const;
        void debugSharedPlots() const;

        IDInfo setWorkingCity(const CvPlot* pPlot, const CvCity* pOldCity, const CvCity* pNewCity);
        //const CvCity* getWorkingCity(IDInfo city, XYCoords coords) const;

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

        void processUpdatedPlots_();
        void addDotMapPlot_(const CvPlot* pPlot, const PlotInfo::PlotInfoNode& plotInfo);
        void updateKeysValueMap_(int key, const PlotInfo::PlotInfoNode& plotInfo, int oldKey);
        void updateKeysValueYields_();

        void updatePlotValueKey_(const CvPlot* pPlot, int oldKey, int newKey);
        void removePlotValuePlot_(const CvPlot* pPlot);

        void updateBorderPlots_(const CvPlot* pPlot, bool isAdding);

        void setWorkingCity_(XYCoords coords, IDInfo assignedCity);

        bool init_;
        Player& player_;
        
        AreaMap revealedAreaDataMap_;
        SubAreaMap revealedSubAreaDataMap_;
        std::map<int /* area id */, std::set<int /* sub area id */> > areaSubAreaMap_;

        std::map<int /* sub area id */, std::set<XYCoords> > impAsCityMap_;

        std::map<int /* sub area id */, std::set<XYCoords> > unrevealedBorderMap_, ourBorderMap_;
        // key = sub area id
        typedef std::map<int, ResourceData> ResourcesMap;
        typedef ResourcesMap::iterator ResourcesMapIter;
        typedef ResourcesMap::const_iterator ResourcesMapConstIter;
        ResourcesMap resourcesMap_;

        // value is plot key
        typedef std::map<XYCoords, int> KeyInfoMap;
        KeyInfoMap keyInfoMap_;

        // key is plot key
        typedef std::map<int, std::set<XYCoords> > KeyCoordsMap;
        KeyCoordsMap keyCoordsMap_;

        // key is plot key
        typedef std::map<int, PlotInfo::PlotInfoNode> PlotInfoMap;
        PlotInfoMap plotInfoMap_;

        std::pair<int, PlotInfoMap::iterator> updatePlotInfo_(const CvPlot* pPlot, bool isNew, bool forceKeyUpdate = false);

        PlotValues plotValues_, previousPlotValues_;
        
        typedef std::set<DotMapItem> DotMap;
        std::map<int, XYCoords, std::greater<int> > bestSites_;

        // set of shared plots for each city
        typedef std::map<IDInfo, CitySharedPlots> CitySharedPlotsMap;
        CitySharedPlotsMap citySharedPlots_;

        CitySharedPlotsMap::iterator getCitySharedPlots_(IDInfo city);

        // set of all shared plots
        typedef std::map<XYCoords, SharedPlot> SharedPlots;
        SharedPlots sharedPlots_;

        SharedPlots::iterator getSharedPlot_(XYCoords coords);

        std::map<IDInfo, XYCoords> seenCities_;
        std::set<XYCoords> goodyHuts_;
        PlotSet updatedPlots_;

        void analyseSharedPlot_(const std::set<XYCoords>& sharedCoords);
    };
}