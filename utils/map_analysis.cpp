#include <fstream>

#include "./map_analysis.h"
#include "./gamedata_analysis.h"
#include "./plot_events.h"
#include "./plot_info_visitors.h"
#include "./plot_info_visitors_streams.h"
#include "./building_info_visitors.h"
#include "./player.h"
#include "./city.h"
#include "./unit.h"
#include "./sub_area.h"
#include "./iters.h"
#include "./city_optimiser.h"
#include "./city_simulator.h"
#include "./map_log.h"
#include "./civ_log.h"
#include "./error_log.h"
#include "./save_utils.h"

namespace AltAI
{
    MapAnalysis::MapAnalysis(Player& player) : init_(false), player_(player)
    {
    }

    void MapAnalysis::init()
    {
        PlayerTypes playerType = player_.getPlayerID();
        TeamTypes teamType = player_.getTeamID();

        if (playerType == NO_PLAYER)
        {
            return;  // todo - does this still happen?
        }

        const CvMap& theMap = gGlobals.getMap();
        std::vector<XYCoords> cities;
        
        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
	    {
            theMap.plotByIndex(i)->setFoundValue(playerType, 0);

		    const CvPlot* pPlot = theMap.plotByIndex(i);
            if (pPlot->isRevealed(teamType, false))
            {
                if (pPlot->isCity() && pPlot->getPlotCity()->getOwner() == playerType)
                {
                    cities.push_back(XYCoords(pPlot->getX(), pPlot->getY()));
                }
                updatePlotRevealed(pPlot);
            }
        }

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "Player: " << player_.getPlayerID() << " knows: " << revealedAreaDataMap_.size() << " areas\n";
            for (AreaMapConstIter ci(revealedAreaDataMap_.begin()), ciEnd(revealedAreaDataMap_.end()); ci != ciEnd; ++ci)
            {
                os << "Area: " << ci->first << " knows: " << ci->second.knownTileCount << " plots (out of " << gGlobals.getMap().getArea(ci->first)->getNumTiles() << ")\n";
            }
        }
#endif

        /*PlotGroupIter iter(player_.getCvPlayer());
        for (CvPlotGroup* pPlotGroup = iter(); pPlotGroup != NULL; pPlotGroup = iter())
        {
            if (pPlotGroup->getOwner() == playerType)
            {
                PlotGroupPlotIter plotIter(pPlotGroup);
                std::vector<XYCoords> coords;
                for (CLLNode<XYCoords>* pNode = plotIter(); pNode != NULL; pNode = plotIter())
                {
                    coords.push_back(pNode->m_data);
                }
                plotGroupCounts_.insert(std::make_pair(pPlotGroup->getID(), coords));
            }
        }*/

        std::vector<YieldTypes> yieldTypes = boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE);
        for (CitiesImprovements::iterator iter(citiesImprovements_.begin()), endIter(citiesImprovements_.end()); iter != endIter; ++iter)
        {
            const CvCity* pCity = getCity(iter->getCity());
            const int targetSize = 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel());
            iter->calcImprovements(yieldTypes, targetSize);
        }

//#ifdef ALTAI_DEBUG
//        {
//            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//            os << "Player: " << player_.getPlayerID() << " has: " << plotGroupCounts_.size() << " plot groups\n";
//            for (std::map<int, std::vector<XYCoords> >::const_iterator ci(plotGroupCounts_.begin()), ciEnd(plotGroupCounts_.end()); ci != ciEnd; ++ci)
//            {
//                os << "PlotGroup: " << ci->first << " has: " << ci->second.size() << " plots\n";
//                for (int i = 0, count = ci->second.size(); i < count; ++i)
//                {
//                    os << ci->second[i] << " ";
//                    if (!((i + 1) % 10)) os << "\n";
//                }
//                os << "\n";
//            }
//        }
//#endif

        init_ = true;
    }

    void MapAnalysis::reinitDotMap()
    {
        const CvMap& theMap = gGlobals.getMap();
        TeamTypes teamType = player_.getTeamID();

        plotInfoMap_.clear();
        plotValues_.plotValueMap.clear();
        plotValues_.keysValueMap.clear();

        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
	    {
		    const CvPlot* pPlot = theMap.plotByIndex(i);

            if (pPlot->isRevealed(teamType, false))
            {
                PlayerTypes owner = pPlot->getOwner();

                PlotInfo plotInfo(pPlot, player_.getPlayerID());
                int key = plotInfo.getKey();

                PlotInfoMap::iterator iter = plotInfoMap_.find(key);
                if (iter == plotInfoMap_.end())
                {
                    iter = plotInfoMap_.insert(std::make_pair(key, plotInfo.getInfo())).first;
                }
    
                updateKeysValueMap_(key, plotInfo);

                if (owner == NO_PLAYER || owner == player_.getPlayerID())
                {
                    addDotMapPlot_(pPlot, plotInfo);
                }
            }
        }
    }

    void MapAnalysis::update(const boost::shared_ptr<IPlotEvent>& pEvent)
    {
        if (!init_)
        {
            init();
        }
        pEvent->handle(*this);
    }

    void MapAnalysis::updatePlotRevealed(const CvPlot* pPlot)
    {
        const XYCoords coords(pPlot->getX(), pPlot->getY());
        const int areaID = pPlot->getArea();
        const int subAreaID = pPlot->getSubArea();

        int knownAreaCount = updateAreaCounts_(areaID);
        int knownSubAreaCount = updateSubAreaCounts_(subAreaID);

        updateAreaBorders_(pPlot, areaID, subAreaID);

        if (gGlobals.getMap().getArea(areaID)->getNumTiles() == knownAreaCount)
        {
            updateAreaComplete_(areaID);
        }

        if (gGlobals.getMap().getSubArea(subAreaID)->getNumTiles() == knownSubAreaCount)
        {
            updateSubAreaComplete_(subAreaID);
        }

        PlotInfo plotInfo(pPlot, player_.getPlayerID());
        int key = plotInfo.getKey();

        {
            /*if (key == 5389245)
            {
                std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
                os << "\nPlot: " << XYCoords(pPlot->getX(), pPlot->getY()) << " has key = " << key;
            }*/
            /*if (XYCoords(pPlot->getX(), pPlot->getY()) == XYCoords(54, 39))
            {
                std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
                os << "\n(54, 39) plot info = " << plotInfo.getInfo();
            }*/
        }

        PlotInfoMap::iterator iter = plotInfoMap_.find(key);
        if (iter == plotInfoMap_.end())
        {
            iter = plotInfoMap_.insert(std::make_pair(key, plotInfo.getInfo())).first;
        }

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nPlot = " << XYCoords(pPlot->getX(), pPlot->getY()) << ", plot key = " << key << ", " << iter->second << "\n";
        }
#endif

        updateKeysValueMap_(key, plotInfo);

        PlayerTypes owner = pPlot->getOwner();
        // consider plots we or nobody owns
        if (owner == NO_PLAYER || owner == player_.getPlayerID())
        {
            addDotMapPlot_(pPlot, plotInfo);
        }

        // update resource data
        updateResourceData_(pPlot);

        // build data for shared plots
        if (pPlot->isCity() && owner == player_.getPlayerID())
        {
            const CvCity* pCity = pPlot->getPlotCity();
            CitySharedPlots citySharedPlots(pCity->getIDInfo());

            // iterate over all plots within city's workable radius
            for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            {
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(player_.getTeamID(), false) && pLoopPlot->getOwner() == player_.getPlayerID())
                    {
                        SharedPlotItemPtr sharedPlot(new SharedPlotItem());
                        sharedPlot->coords = XYCoords(pLoopPlot->getX(), pLoopPlot->getY());

                        for (int j = 1; j <= CITY_PLOTS_RADIUS; ++j)
                        {
                            CultureRangePlotIter loopPlotIter(pLoopPlot, (CultureLevelTypes)j);
                            while (IterPlot pCityLoopPlot = loopPlotIter())
                            {
                                if (pCityLoopPlot.valid() && pCityLoopPlot->getOwner() == player_.getPlayerID())
                                {
                                    const CvCity* pLoopCity = pCityLoopPlot->getPlotCity();
                                    if (pLoopCity && !(pLoopCity->getIDInfo() == citySharedPlots.city))
                                    {
                                        sharedPlot->possibleCities.push_back(pLoopCity->getIDInfo());
                                    }
                                }
                            }
                        }

                        if (!sharedPlot->possibleCities.empty())
                        {
                            sharedPlot->possibleCities.push_back(citySharedPlots.city); // add ourselves if we have at least one other city
                            sharedPlot->assignedCity = pLoopPlot->getWorkingCity() ? pLoopPlot->getWorkingCity()->getIDInfo() : IDInfo();
                            citySharedPlots.sharedPlots.push_back(sharedPlot);
                        }
                    }
                }
            }
            sharedPlots_.insert(citySharedPlots);

            citiesImprovements_.insert(CityImprovementManager(pCity->getIDInfo()));
        }
    }

    void MapAnalysis::addDotMapPlot_(const CvPlot* pPlot, const PlotInfo& plotInfo)
    {
        int key = plotInfo.getKey();

        if (hasPossibleYield(plotInfo.getInfo(), player_.getPlayerID()))
        {
            for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            {
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    // consider plots we have seen and can theoretically found a city at
                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(player_.getTeamID(), false))
                    {
                        if (player_.getCvPlayer()->canFound(pLoopPlot->getX(), pLoopPlot->getY()))                    
                        {
                            plotValues_.plotValueMap[pLoopPlot->getSubArea()][XYCoords(pLoopPlot->getX(), pLoopPlot->getY())][key].insert(XYCoords(pPlot->getX(), pPlot->getY()));
                        }
                    }
                }
            }
        }
    }

    const MapAnalysis::PlotValues& MapAnalysis::getPlotValues() const
    {
        return plotValues_;
    }

    void MapAnalysis::updatePlotFeature(const CvPlot* pPlot, FeatureTypes oldFeatureType)
    {
        XYCoords coords(pPlot->getX(), pPlot->getY());
        // update key for this plot and add to plotInfoMap_ if required
        PlotInfo plotInfo(pPlot, player_.getPlayerID());
        const int key = plotInfo.getKey();
        const int oldKey = plotInfo.getKeyWithFeature(oldFeatureType);  // can be NO_FEATURE
        const int subAreaID = pPlot->getSubArea();

        PlotInfoMap::iterator iter = plotInfoMap_.find(key);
        if (iter == plotInfoMap_.end())
        {
            iter = plotInfoMap_.insert(std::make_pair(key, plotInfo.getInfo())).first;
        }

#ifdef ALTAI_DEBUG
        { // debug
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\n Updated plot: (" << pPlot->getX() << ", " << pPlot->getY() << ")";
        }
#endif

        updateKeysValueMap_(key, plotInfo);

        PlayerTypes owner = pPlot->getOwner();
        if (owner == NO_PLAYER || owner == player_.getPlayerID())
        {
            for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            {
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    // consider plots we have seen and can theoretically found a city at
                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(player_.getTeamID(), false))
                    {
                        // don't test if we can found city - as this might have changed in the meantime
                        PlotValues::PlotValueMap::iterator pIter = plotValues_.plotValueMap.find(pLoopPlot->getSubArea());
                        if (pIter != plotValues_.plotValueMap.end())
                        {
                            PlotValues::SubAreaPlotValueMap::iterator iter = pIter->second.find(XYCoords(pLoopPlot->getX(), pLoopPlot->getY()));
                            if (iter != pIter->second.end())
                            {
                                std::map<int /* plot key */, std::set<XYCoords> >::iterator keysIter = iter->second.find(oldKey);
                                if (keysIter != iter->second.end())
                                {
                                    std::set<XYCoords>::iterator coordsIter = keysIter->second.find(coords);
                                    if (coordsIter != keysIter->second.end())
                                    {
                                        keysIter->second.erase(coordsIter);
                                    }
                                }
                                iter->second[key].insert(coords);
                            }
                        }
                    }
                }
            }
        }
    }

    void MapAnalysis::updatePlotCulture(const CvPlot* pPlot, bool remove)
    {
        XYCoords coords(pPlot->getX(), pPlot->getY());
        PlotInfo plotInfo(pPlot, player_.getPlayerID());
        const int key = plotInfo.getKey();
        const int subAreaID = pPlot->getSubArea();

#ifdef ALTAI_DEBUG
        { // debug
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\n " << (remove ? "Removing " : " Adding ") << "plot: (" << pPlot->getX() << ", " << pPlot->getY() << ")";
        }
#endif

        if (remove && pPlot->getOwner() != NO_PLAYER)
        {
            for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            {
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    if (pLoopPlot.valid())
                    {
                        // don't test if we can found city - as this might have changed in the meantime
                        PlotValues::PlotValueMap::iterator pIter = plotValues_.plotValueMap.find(pLoopPlot->getSubArea());
                        if (pIter != plotValues_.plotValueMap.end())
                        {
                            PlotValues::SubAreaPlotValueMap::iterator iter = pIter->second.find(XYCoords(pLoopPlot->getX(), pLoopPlot->getY()));
                            if (iter != pIter->second.end())
                            {
                                std::map<int /* plot key */, std::set<XYCoords> >::iterator keysIter = iter->second.find(key);
                                if (keysIter != iter->second.end())
                                {
                                    std::set<XYCoords>::iterator coordsIter = keysIter->second.find(coords);
                                    if (coordsIter != keysIter->second.end())
                                    {
                                        keysIter->second.erase(coordsIter);
                                    }
                                }
                            }
                        }

                        if (pLoopPlot->isCity() && pLoopPlot->getPlotCity()->getOwner() == player_.getPlayerID())
                        {
                            const CvCity* pCity = pLoopPlot->getPlotCity();
                            player_.setCityDirty(pCity->getIDInfo());
                            player_.getCity(pCity->getID()).setFlag(City::NeedsImprovementCalcs);
                        }
                    }
                }
            }
        }
        else
        {
            // if we are here, means we own the plot (unowned plots should be added through updatePlotRevealed)
            if (hasPossibleYield(plotInfo.getInfo(), player_.getPlayerID()))
            {
                for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
                {
                    CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                    while (IterPlot pLoopPlot = plotIter())
                    {
                        // consider plots we have seen and can theoretically found a city at
                        if (pLoopPlot.valid() && pLoopPlot->isRevealed(player_.getTeamID(), false))
                        {
                            if (player_.getCvPlayer()->canFound(pLoopPlot->getX(), pLoopPlot->getY()))                    
                            {
                                plotValues_.plotValueMap[pLoopPlot->getSubArea()][XYCoords(pLoopPlot->getX(), pLoopPlot->getY())][key].insert(coords);
                            }

                            if (pLoopPlot->isCity() && pLoopPlot->getPlotCity()->getOwner() == player_.getPlayerID())
                            {
                                const CvCity* pCity = pLoopPlot->getPlotCity();
                                player_.setCityDirty(pCity->getIDInfo());
                                player_.getCity(pCity->getID()).setFlag(City::NeedsImprovementCalcs);
                            }
                        }
                    }
                }
            }

            // just update if plot is being added - only get called if plot is visible to us (our team) (think this works correctly!)
            updateResourceData_(pPlot);
        }
    }

    void MapAnalysis::updatePlotCanFound(const CvPlot* pPlot, bool remove)
    {
        // todo
    }

    void MapAnalysis::updateResourceData(const std::vector<BonusTypes>& revealedBonusTypes)
    {
        PlayerTypes playerType = player_.getPlayerID();
        TeamTypes teamType = player_.getTeamID();
        const CvMap& theMap = gGlobals.getMap();
        std::vector<XYCoords> cities;
        
        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
	    {
		    const CvPlot* pPlot = theMap.plotByIndex(i);
            if (pPlot->isRevealed(teamType, false))
            {
                BonusTypes bonusType = pPlot->getBonusType(teamType);
                if (bonusType != NO_BONUS && std::find(revealedBonusTypes.begin(), revealedBonusTypes.end(), bonusType) != revealedBonusTypes.end())
                {
                    updateResourceData_(pPlot);
                }
            }
        }
    }

    void MapAnalysis::updateResourceData_(const CvPlot* pPlot)
    {
        const int subAreaID = pPlot->getSubArea();
        const XYCoords coords(pPlot->getX(), pPlot->getY());
        PlayerTypes owner = pPlot->getOwner();

        // update resource data
        ResourcesMapIter resourceSubAreaIter = resourcesMap_.find(subAreaID);
        if (resourceSubAreaIter == resourcesMap_.end())
        {
            resourceSubAreaIter = resourcesMap_.insert(std::make_pair(subAreaID, ResourceData(subAreaID))).first;
        }

        BonusTypes bonusType = pPlot->getBonusType(player_.getTeamID());
        if (bonusType != NO_BONUS)
        {
            std::map<BonusTypes, ResourceData::ResourcePlotData>::iterator resourceItemIter = resourceSubAreaIter->second.subAreaResourcesMap.find(bonusType);
            if (resourceItemIter == resourceSubAreaIter->second.subAreaResourcesMap.end())
            {
                resourceSubAreaIter->second.subAreaResourcesMap.insert(std::make_pair(bonusType, ResourceData::ResourcePlotData(1, std::make_pair(coords, owner))));
            }
            else
            {
                bool found = false;
                for (size_t i = 0, count = resourceItemIter->second.size(); i < count; ++i)
                {
                    if (resourceItemIter->second[i].first == coords)
                    {
                        found = true;
                        resourceItemIter->second[i].second = owner;
                    }
                }
                if (!found)
                {
                    resourceItemIter->second.push_back(std::make_pair(coords, owner));
                }
            }
        }
    }

    void MapAnalysis::updateKeysValueMap_(int key, const PlotInfo& plotInfo)
    {
        if (plotValues_.keysValueMap.find(key) == plotValues_.keysValueMap.end())
        {
            // 2 = lookahead tech depth
            //std::pair<PlotValues::KeysValueMap::iterator, bool> iterBoolPair = plotValues_.keysValueMap.insert(
            //    std::make_pair(key, std::vector<std::pair<PlotYield, ImprovementTypes> >(1, getMaxYield(plotInfo.getInfo(), player_.getPlayerID(), 2))));

            //std::vector<std::pair<PlotYield, ImprovementTypes> > yields(getYields(plotInfo.getInfo(), player_.getPlayerID(), 3));  // 3 = lookahead tech depth
            //std::copy(yields.begin(), yields.end(), std::back_inserter(iterBoolPair.first->second));

            std::vector<std::pair<PlotYield, ImprovementTypes> > possibleImprovements(getYields(plotInfo.getInfo(), player_.getPlayerID(), 3));

            //BonusTypes bonusType = boost::get<PlotInfo::BaseNode>(plotInfo.getInfo()).bonusType;
            //if (bonusType != NO_BONUS)
            //{
                /*if (possibleImprovements.size() < 2)
                {
                    boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(*player_.getCvPlayer());
                    std::ostream& os = pErrorLog->getStream();
                    os << "\nUnexpected number of improvements for plot key: " << key
                       << " (bonus = " << gGlobals.getBonusInfo(bonusType).getType() << ")\n"
                       << plotInfo.getInfo();
                }*/
            //}

            if (!possibleImprovements.empty())
            {
                plotValues_.keysValueMap.insert(std::make_pair(key, possibleImprovements));
            }
        }
    }

    int MapAnalysis::updateAreaCounts_(int areaID)
    {
        AreaMapIter iter(revealedAreaDataMap_.find(areaID));
        if (iter == revealedAreaDataMap_.end())
        {
            iter = revealedAreaDataMap_.insert(std::make_pair(areaID, AreaDetail(areaID))).first;
        }
        else
        {
            ++iter->second.knownTileCount;
        }
        return iter->second.knownTileCount;
    }

    int MapAnalysis::updateSubAreaCounts_(int subAreaID)
    {
        SubAreaMapIter iter(revealedSubAreaDataMap_.find(subAreaID));
        if (iter == revealedSubAreaDataMap_.end())
        {
            iter = revealedSubAreaDataMap_.insert(std::make_pair(subAreaID, AreaDetail(subAreaID))).first;
        }
        else
        {
            ++iter->second.knownTileCount;
        }
        return iter->second.knownTileCount;
    }

    void MapAnalysis::updateAreaBorders_(const CvPlot* pPlot, int areaID, int subAreaID)
    {
        AreaMapIter areaIter(revealedAreaDataMap_.find(areaID));
        TeamTypes teamType = player_.getTeamID();
        NeighbourPlotIter iter(pPlot);

        while (IterPlot pLoopPlot = iter())
        {
            if (pLoopPlot.valid() && pLoopPlot->isRevealed(teamType, false))
            {
                int loopPlotAreaID = pLoopPlot->getArea();
                if (loopPlotAreaID != areaID)
                {
                    areaIter->second.borderingAreas.insert(loopPlotAreaID);
                }
            }
        }
    }

    void MapAnalysis::updateAreaComplete_(int areaID)
    {
        AreaMapIter iter(revealedAreaDataMap_.find(areaID));
        if (iter != revealedAreaDataMap_.end())
        {
            iter->second.allKnown = true;

#ifdef ALTAI_DEBUG
            {
                std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
                os << "Player: " << player_.getPlayerID() << " knows all of area: " << areaID << " (" << iter->second.knownTileCount << ") tiles\n";
            }
#endif
            // update from Map's area graph
        }
    }

    void MapAnalysis::updateSubAreaComplete_(int subAreaID)
    {
        SubAreaMapIter iter(revealedSubAreaDataMap_.find(subAreaID));
        if (iter != revealedSubAreaDataMap_.end())
        {
            iter->second.allKnown = true;

#ifdef ALTAI_DEBUG
            {
                std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
                os << "Player: " << player_.getPlayerID() << " knows all of sub area: " << subAreaID << " (" << iter->second.knownTileCount << ") tiles\n";
            }
#endif

            SubAreaGraphNode node = gGlobals.getMap().getSubAreaGraph()->getNode(subAreaID);
            if (node.ID != FFreeList::INVALID_INDEX)
            {
                iter->second.borderingAreas = node.borderingSubAreas;
                iter->second.enclosedAreas = node.enclosedSubAreas;
            }
        }
    }

    MapAnalysis::AreaDetail::AreaDetail(int ID_) : ID(ID_), knownTileCount(1), allKnown(false)
    {
    }

    PlotInfo::PlotInfoNode MapAnalysis::getPlotInfoNode(const CvPlot* pPlot) const
    {
        PlotInfo plotInfo(pPlot, player_.getPlayerID());
        int key = plotInfo.getKey();

        PlotInfoMap::const_iterator iter = plotInfoMap_.find(key);
        if (iter != plotInfoMap_.end())
        {
            return iter->second;
        }
        else
        {
            // TODO this is really an error - probably due to the plot not being revealed yet
            return PlotInfo::PlotInfoNode();
        }
    }

    void MapAnalysis::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n";
        for (AreaMapConstIter ci(revealedAreaDataMap_.begin()), ciEnd(revealedAreaDataMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nArea: " << ci->first << " knows: " << ci->second.knownTileCount << " plots, out of: " << gGlobals.getMap().getArea(ci->first)->getNumTiles()
               << " enclosed by: " << ci->second.enclosedAreas.size() << " areas, borders: " << ci->second.borderingAreas.size() << " areas.";
        }
        
        os << "\n";
        for (SubAreaMapConstIter ci(revealedSubAreaDataMap_.begin()), ciEnd(revealedSubAreaDataMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nSub Area: " << ci->first << " knows: " << ci->second.knownTileCount << " plots , out of: " << gGlobals.getMap().getSubArea(ci->first)->getNumTiles()
               << " enclosed by: " << ci->second.enclosedAreas.size() << " subareas, borders: " << ci->second.borderingAreas.size() << " subareas.";
        }

        os << "\n";
#endif
    }

    void MapAnalysis::debugResourceData() const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();

        for (ResourcesMapConstIter ci(resourcesMap_.begin()), ciEnd(resourcesMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nSub Area = " << ci->first;
            for (std::map<BonusTypes, ResourceData::ResourcePlotData>::const_iterator bi(ci->second.subAreaResourcesMap.begin()), biEnd(ci->second.subAreaResourcesMap.end()); bi != biEnd; ++bi)
            {
                os << "\nBonus = " << gGlobals.getBonusInfo(bi->first).getType();
                int ourCount = 0, otherCount = 0, unownedCount = 0;
                for (size_t i = 0, count = bi->second.size(); i < count; ++i)
                {
                    if (bi->second[i].second == player_.getPlayerID())
                    {
                        ++ourCount;
                    }
                    else if (bi->second[i].second == NO_PLAYER)
                    {
                        ++unownedCount;
                    }
                    else
                    {
                        ++otherCount;
                    }
                }
                os << " ours = " << ourCount << " others = " << otherCount << " unowned = " << unownedCount;
            }
        }
#endif
    }

    int MapAnalysis::getControlledResourceCount(BonusTypes bonusType) const
    {
        int resourceCount = 0;

        for (ResourcesMapConstIter ci(resourcesMap_.begin()), ciEnd(resourcesMap_.end()); ci != ciEnd; ++ci)
        {
            std::map<BonusTypes, ResourceData::ResourcePlotData>::const_iterator bi(ci->second.subAreaResourcesMap.find(bonusType));

            if (bi != ci->second.subAreaResourcesMap.end())
            {
                for (size_t i = 0, count = bi->second.size(); i < count; ++i)
                {
                    if (bi->second[i].second == player_.getPlayerID())
                    {
                        ++resourceCount;
                    }
                }
            }
        }
        return resourceCount;
    }

    IDInfo MapAnalysis::setWorkingCity(const CvPlot* pPlot, const CvCity* pOldCity, const CvCity* pNewCity)
    {
        XYCoords coords(pPlot->getX(), pPlot->getY());

        if (!pNewCity)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nAbout to clear plot override.";
#endif
        }

        const CvCity* pCity = pNewCity ? pNewCity : pOldCity;

        if (pCity)
        {
            CitySharedPlots thisCity(pCity->getIDInfo());
            SharedPlots::iterator iter(sharedPlots_.find(thisCity));
            if (iter != sharedPlots_.end())
            {
                for (CitySharedPlots::SharedPlotsConstIter plotsIter(iter->sharedPlots.begin()), endIter(iter->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
                {
                    if ((*plotsIter)->coords == coords)
                    {
                        IDInfo origAssignedCity = (*plotsIter)->assignedCity;
                        // slightly inefficient, as set this city's entry for the plot's assigned city in this loop whch means another find()
                        for (SharedPlotItem::PossibleCitiesConstIter citiesIter((*plotsIter)->possibleCities.begin()), endIter((*plotsIter)->possibleCities.end()); citiesIter != endIter; ++citiesIter)
                        {
                            setWorkingCity_(*citiesIter, coords, pNewCity ? pNewCity->getIDInfo() : IDInfo());
                        }
                        return origAssignedCity;
                    }
                }
            }
        }
        return IDInfo();
    }

    void MapAnalysis::setWorkingCity_(IDInfo possibleCity, XYCoords coords, IDInfo assignedCity)
    {
        CitySharedPlots citySharedPlots(possibleCity);
        SharedPlots::iterator iter(sharedPlots_.find(citySharedPlots));
        if (iter != sharedPlots_.end())
        {
            for (CitySharedPlots::SharedPlotsIter plotsIter(iter->sharedPlots.begin()), endIter(iter->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
            {
                if ((*plotsIter)->coords == coords)
                {
                    (*plotsIter)->assignedCity = assignedCity;
                    break;
                }
            }
        }
    }

    const CvCity* MapAnalysis::getWorkingCity(IDInfo city, XYCoords coords) const
    {
        if (sharedPlots_.empty())  // not init() yet - needed as cities init() first
        {
            return gGlobals.getMap().plot(coords.iX, coords.iY)->getWorkingCity();
        }

        CitySharedPlots thisCity(city);
        SharedPlots::const_iterator ci(sharedPlots_.find(thisCity));
        if (ci != sharedPlots_.end())
        {
            for (CitySharedPlots::SharedPlotsConstIter plotsIter(ci->sharedPlots.begin()), endIter(ci->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
            {
                if ((*plotsIter)->coords == coords)
                {
                    return getCity((*plotsIter)->assignedCity);
                }
            }
        }
        return getCity(city);
    }

    // analyse all shared plots for this player
    void MapAnalysis::analyseSharedPlots()
    {
        std::set<XYCoords> processedCoords;
        for (SharedPlots::iterator iter(sharedPlots_.begin()), ciEnd(sharedPlots_.end()); iter != ciEnd; ++iter)
        {
            for (CitySharedPlots::SharedPlotsIter plotsIter(iter->sharedPlots.begin()), endIter(iter->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
            {
                if (processedCoords.find((*plotsIter)->coords) == processedCoords.end())
                {
                    analyseSharedPlot_(iter, plotsIter);
                    processedCoords.insert((*plotsIter)->coords);
                }
            }
        }

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nCompleted analysis of all shared plots";
        }
        debugSharedPlots();
#endif
    }

    // reanalyse plots shared by these cities
    void MapAnalysis::analyseSharedPlots(const std::set<IDInfo>& cities)
    {
        std::set<XYCoords> processedCoords;
        for (std::set<IDInfo>::const_iterator ci(cities.begin()), ciEnd(cities.end()); ci != ciEnd; ++ci)
        {
            SharedPlots::iterator iter = sharedPlots_.find(CitySharedPlots(*ci));

            if (iter != sharedPlots_.end())
            {
                for (CitySharedPlots::SharedPlotsIter plotsIter(iter->sharedPlots.begin()), endIter(iter->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
                {
                    if (processedCoords.find((*plotsIter)->coords) == processedCoords.end())
                    {
                        analyseSharedPlot_(iter, plotsIter);
                        processedCoords.insert((*plotsIter)->coords);
                    }
                }
            }
        }

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nCompleted analysis of shared plots for cities: ";

            for (std::set<IDInfo>::const_iterator ci(cities.begin()), ciEnd(cities.end()); ci != ciEnd; ++ci)
            {
                CvCity* pCity = getCity(*ci);
                if (pCity)
                {
                    os << narrow(pCity->getName()) << ", ";
                }
            }
        }
        //debugSharedPlots();
#endif
    }

    void MapAnalysis::calcImprovements(const std::set<IDInfo>& cities)
    {
        std::vector<YieldTypes> yieldTypes = boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE);
        for (std::set<IDInfo>::const_iterator ci(cities.begin()), ciEnd(cities.end()); ci != ciEnd; ++ci)
        {
            CitiesImprovements::iterator iter(citiesImprovements_.find(CityImprovementManager(*ci)));
            if (iter != citiesImprovements_.end())
            {
                const CvCity* pCity = getCity(iter->getCity());
                const int targetSize = 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel());
                iter->calcImprovements(yieldTypes, targetSize);
            }
        }
    }

    void MapAnalysis::calcImprovements(IDInfo city)
    {
        std::vector<YieldTypes> yieldTypes = boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE);
        CitiesImprovements::iterator iter(citiesImprovements_.find(CityImprovementManager(city)));
        if (iter != citiesImprovements_.end())
        {
            const CvCity* pCity = getCity(iter->getCity());
            const int targetSize = 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel());
            iter->calcImprovements(yieldTypes, targetSize);
        }
    }

    // todo - this won't mark other player's plots - even if they are in our team
    void MapAnalysis::markIrrigationChainSourcePlot(XYCoords coords, ImprovementTypes improvementType)
    {
        // check neighbouring plots to see if they need to be marked as part of an irrigation chain
        // logic is to mark them if they are a source of freshwater, and are irrigated (and not already marked)
        // only mark plots we control (even if source is coming from a farm in another player's territory open borders)
        // TODO: it is possible to have more than one source plot - in which case we could skip all but one
        const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nChecking coords: " << coords << " for source of irrigation chain";
#endif

        NeighbourPlotIter iter(pPlot);

        while (IterPlot pLoopPlot = iter())
        {
            if (pLoopPlot.valid() && pLoopPlot->isRevealed(player_.getTeamID(), false) && pLoopPlot->getSubArea() == pPlot->getSubArea() && pLoopPlot->getOwner() == player_.getPlayerID())
            {
                if (pLoopPlot->isIrrigated() && pLoopPlot->isFreshWater())
                {
//#ifdef ALTAI_DEBUG
//                    {   // debug
//                        os << "\nChecking plot: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY());
//                    }
//#endif
                    const CvCity* pLoopCity = pLoopPlot->getWorkingCity();
                    if (pLoopCity && pLoopCity->getOwner() == pPlot->getOwner())
                    {
                        const XYCoords loopCoords(pLoopPlot->getX(), pLoopPlot->getY());
//#ifdef ALTAI_DEBUG
//                        {   // debug
//                            os << "\nChecking city: " << narrow(pLoopCity->getName()) << " for: " << coords << " for source of irrigation chain";
//                        }
//#endif
                        CityImprovementManager& improvementManager = getImprovementManager(pLoopCity->getIDInfo());
                        std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();
                        for (size_t i = 0, count = improvements.size(); i < count; ++i)
                        {
                            if (boost::get<0>(improvements[i]) == loopCoords)
                            {
#ifdef ALTAI_DEBUG
                                {   // debug
                                    os << "\nMarked plot: " << loopCoords << " as possible source of irrigation chain";
                                }
#endif
                                boost::get<6>(improvements[i]) |= CityImprovementManager::IrrigationChainPlot;
                                break;
                            }
                        }
                    }
                }
            }
        }

//        for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
//        {
//            CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
//            while (IterPlot pLoopPlot = plotIter())
//            {
//                const CvCity* pLoopCity = pLoopPlot->getPlotCity();
//                if (pLoopCity && pLoopCity->getOwner() == pPlot->getOwner())
//                {
//                    CityImprovementManager& improvementManager = getImprovementManager(pLoopCity->getIDInfo());
//                    std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();
//                    bool found = false;
//                    for (size_t j = 0, count = improvements.size(); j < count; ++j)
//                    {
//                        if (boost::get<0>(improvements[j]) == coords)
//                        {
//                            found = true;
//                            if (boost::get<2>(improvements[j]) != improvementType)
//                            {
//                                boost::get<2>(improvements[j]) = improvementType;
//                                boost::get<4>(improvements[j]) = TotalOutput();
//                            }
//
//                            // yield might change, even if improvement doesn't
//                            boost::get<3>(improvements[j]) = getYield(PlotInfo(pPlot, pPlot->getOwner()).getInfo(), pPlot->getOwner(), improvementType, NO_FEATURE, pPlot->getRouteType());
//                            //boost::get<6>(improvements[j]) |= CityImprovementManager::IrrigationChainPlot;
//#ifdef ALTAI_DEBUG
//                            //{   // debug
//                            //    std::ostream& os = CityLog::getLog(pLoopCity)->getStream();
//                            //    os << "\nMarked plot: " << coords << " as as part of irrigation chain with improvement: "
//                            //       << (improvementType == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo(improvementType).getType());
//                            //}
//#endif
//                            return;
//                        }
//                    }
//
//                    if (!found)
//                    {
//                        PlotYield thisYield = getYield(PlotInfo(pPlot, pPlot->getOwner()).getInfo(), pPlot->getOwner(), improvementType, NO_FEATURE, pPlot->getRouteType());
//                        improvements.push_back(boost::make_tuple(coords, NO_FEATURE, improvementType, thisYield, TotalOutput(), CityImprovementManager::Not_Built, CityImprovementManager::IrrigationChainPlot));
//
//#ifdef ALTAI_DEBUG
//                        {   // debug
//                            std::ostream& os = CityLog::getLog(pLoopCity)->getStream();
//                            os << "\nAdded plot: " << coords << " as as part of irrigation chain with improvement: "
//                                << (improvementType == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo(improvementType).getType());
//                        }
//#endif
//                    }
//                }
//            }
//        }
    }

    CityImprovementManager& MapAnalysis::getImprovementManager(IDInfo city)
    {
        CitiesImprovements::iterator iter(citiesImprovements_.find(CityImprovementManager(city)));
        if (iter != citiesImprovements_.end())
        {
            return *iter;
        }
        else
        {
            std::vector<YieldTypes> yieldTypes = boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE);
            // todo - clean this up and make initial calc explicit
            CityImprovementManager& improvementManager = *citiesImprovements_.insert(CityImprovementManager(city)).first; // add new one (probably actually an error?)

            const CvCity* pCity = getCity(city);
            const int targetSize = 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel());
            improvementManager.calcImprovements(yieldTypes, targetSize);
            return improvementManager;
        }
    }

    void MapAnalysis::addCity(const CvCity* pCity)
    {
        const CvPlot* pPlot = pCity->plot();

        CitySharedPlots citySharedPlots(pCity->getIDInfo());

        // iterate over all plots within city's workable radius
        for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
        {
            CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
            while (IterPlot pLoopPlot = plotIter())
            {
                if (pLoopPlot.valid() && pLoopPlot->isRevealed(player_.getTeamID(), false) && pLoopPlot->getOwner() == player_.getPlayerID())
                {
                    SharedPlotItemPtr sharedPlot(new SharedPlotItem());
                    sharedPlot->coords = XYCoords(pLoopPlot->getX(), pLoopPlot->getY());

                    for (int j = 1; j <= CITY_PLOTS_RADIUS; ++j)
                    {
                        CultureRangePlotIter loopPlotIter(pLoopPlot, (CultureLevelTypes)j);
                        while (IterPlot pCityLoopPlot = loopPlotIter())
                        {
                            if (pCityLoopPlot.valid() && pCityLoopPlot->getOwner() == player_.getPlayerID())
                            {
                                const CvCity* pLoopCity = pCityLoopPlot->getPlotCity();
                                if (pLoopCity && !(pLoopCity->getIDInfo() == citySharedPlots.city))
                                {
                                    sharedPlot->possibleCities.push_back(pLoopCity->getIDInfo());
                                    addSharedPlotToCity_(pLoopCity->getIDInfo(), sharedPlot->coords, pCity->getIDInfo());
                                }
                            }
                        }
                    }

                    if (!sharedPlot->possibleCities.empty())
                    {
                        sharedPlot->possibleCities.push_back(citySharedPlots.city); // add ourselves if we have at least one other city
                        sharedPlot->assignedCity = pLoopPlot->getWorkingCity() ? pLoopPlot->getWorkingCity()->getIDInfo() : IDInfo();
                        citySharedPlots.sharedPlots.push_back(sharedPlot);
                    }
                }
            }
        }
        sharedPlots_.insert(citySharedPlots);

#ifdef ALTAI_DEBUG
        debugSharedPlots();
#endif
    }

    void MapAnalysis::deleteCity(const CvCity* pCity)
    {
        const CvPlot* pPlot = pCity->plot();

        SharedPlots::iterator iter = sharedPlots_.find(CitySharedPlots(pCity->getIDInfo()));
        if (iter != sharedPlots_.end())
        {
            std::set<IDInfo> sharedCities;
            for (CitySharedPlots::SharedPlotsConstIter plotsIter(iter->sharedPlots.begin()), endIter(iter->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
            {
                for (SharedPlotItem::PossibleCitiesConstIter citiesIter((*plotsIter)->possibleCities.begin()), endIter((*plotsIter)->possibleCities.end()); citiesIter != endIter; ++citiesIter)
                {
                    if (!(*citiesIter == pCity->getIDInfo()))
                    {
                        sharedCities.insert(*citiesIter);
                    }
                }
            }

            for (std::set<IDInfo>::const_iterator ci(sharedCities.begin()), ciEnd(sharedCities.end()); ci != ciEnd; ++ci)
            {
                deleteSharedCity_(*ci, pCity->getIDInfo());
            }
            sharedPlots_.erase(iter);
        }
    }

    void MapAnalysis::deleteSharedCity_(IDInfo city, IDInfo sharedCity)
    {
        SharedPlots::iterator iter = sharedPlots_.find(CitySharedPlots(city));

        if (iter != sharedPlots_.end())
        {
            CitySharedPlots::SharedPlotsIter plotsIter(iter->sharedPlots.begin()), endIter(iter->sharedPlots.end());
            while (plotsIter != endIter)
            {
                SharedPlotItem::PossibleCitiesIter citiesIter = std::find((*plotsIter)->possibleCities.begin(), (*plotsIter)->possibleCities.end(), sharedCity);
                if (citiesIter != (*plotsIter)->possibleCities.end()) // found plot for this city which was shared with sharedCity
                {
                    (*plotsIter)->possibleCities.erase(citiesIter);
                }

                if ((*plotsIter)->possibleCities.size() < 2) // this was the only city we were sharing with
                {
                    iter->sharedPlots.erase(plotsIter++);  // no longer a shared plot
                }
                else
                {
                    ++plotsIter;
                }
            }
        }
    }

    void MapAnalysis::addSharedPlotToCity_(IDInfo city, XYCoords coords, IDInfo otherCity)
    {
        SharedPlots::iterator iter = sharedPlots_.find(CitySharedPlots(city));

        if (iter != sharedPlots_.end())
        {
            bool found = false;
            for (CitySharedPlots::SharedPlotsIter plotsIter(iter->sharedPlots.begin()), endIter(iter->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
            {
                if ((*plotsIter)->coords == coords)
                {
                    (*plotsIter)->possibleCities.push_back(city);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                SharedPlotItemPtr sharedPlot(new SharedPlotItem());

                sharedPlot->coords = coords;
                sharedPlot->possibleCities.push_back(city);
                sharedPlot->possibleCities.push_back(otherCity);

                CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                sharedPlot->assignedCity = pPlot->getWorkingCity() ? pPlot->getWorkingCity()->getIDInfo() : IDInfo();

                iter->sharedPlots.push_back(sharedPlot);
            }
        }
        else // need to add this city too as this city had no shared plots until now - todo - is this really possible?
        {
            SharedPlotItemPtr sharedPlot(new SharedPlotItem());

            sharedPlot->coords = coords;
            sharedPlot->possibleCities.push_back(city);
            sharedPlot->possibleCities.push_back(otherCity);

            CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
            sharedPlot->assignedCity = pPlot->getWorkingCity() ? pPlot->getWorkingCity()->getIDInfo() : IDInfo();

            CitySharedPlots citySharedPlots(city);
            citySharedPlots.sharedPlots.push_back(sharedPlot);

            sharedPlots_.insert(citySharedPlots);
        }
    }

    void MapAnalysis::reassignUnworkedSharedPlots(IDInfo city)
    {
        SharedPlots::iterator iter = sharedPlots_.find(CitySharedPlots(city));
        
        if (iter != sharedPlots_.end())
        {
            const CvCity* pCity = getCity(city);
            const CvMap& theMap = gGlobals.getMap();

            for (CitySharedPlots::SharedPlotsConstIter plotsIter(iter->sharedPlots.begin()), endIter(iter->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
            {
                CvPlot* pPlot = theMap.plot((*plotsIter)->coords.iX, (*plotsIter)->coords.iY);
                if (!pCity->isWorkingPlot(pPlot))  // and can work?
                {
                    for (SharedPlotItem::PossibleCitiesConstIter citiesIter((*plotsIter)->possibleCities.begin()), endIter((*plotsIter)->possibleCities.end()); citiesIter != endIter; ++citiesIter)
                    {
                        if (!(*citiesIter == city))
                        {
                            pPlot->setWorkingCityOverride(getCity(*citiesIter));  // give another city a chance to choose the plot
                            break;
                        }
                    }
                }
            }
        }
    }

    // assigns the chosen improvements by this city's improvements manager to any shared plots
    void MapAnalysis::assignSharedPlotImprovements(IDInfo city)
    {
        SharedPlots::iterator sharedPlotsIter = sharedPlots_.find(CitySharedPlots(city));
        CitiesImprovements::iterator improvementsIter = citiesImprovements_.find(CityImprovementManager(city));
        
        if (sharedPlotsIter != sharedPlots_.end() && improvementsIter != citiesImprovements_.end())
        {
            std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementsIter->getImprovements();

            // go through each shared plot for this city
            for (CitySharedPlots::SharedPlotsIter plotsIter(sharedPlotsIter->sharedPlots.begin()), endIter(sharedPlotsIter->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
            {
                // try and find the improvement for this shared plot
                for (size_t i = 0, count = improvements.size(); i < count; ++i)
                {
                    if (boost::get<0>(improvements[i]) == (*plotsIter)->coords)
                    {
                        // update this plot's improvement info for this city in this shared plot's data
                        (*plotsIter)->assignedImprovement.second = 
                            boost::make_tuple(boost::get<2>(improvements[i]), boost::get<1>(improvements[i]), boost::get<3>(improvements[i]), boost::get<6>(improvements[i]));
                        break;
                    }
                }
            }
        }
    }

    SharedPlotItemPtr MapAnalysis::getSharedPlot(IDInfo city, XYCoords coords) const
    {
        SharedPlots::const_iterator ci = sharedPlots_.find(CitySharedPlots(city));

        if (ci != sharedPlots_.end())
        {
            bool found = false;
            for (CitySharedPlots::SharedPlotsConstIter plotsIter(ci->sharedPlots.begin()), endIter(ci->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
            {
                if ((*plotsIter)->coords == coords)
                {
                    return *plotsIter;
                }
            }
        }

        return SharedPlotItemPtr();
    }

    void MapAnalysis::analyseSharedPlot_(MapAnalysis::SharedPlots::iterator iter, CitySharedPlots::SharedPlotsIter plotsIter)
    {
        CvPlot* pPlot = gGlobals.getMap().plot((*plotsIter)->coords.iX, (*plotsIter)->coords.iY);
        IDInfo origAssignedCity = (*plotsIter)->assignedCity;
        std::vector<std::pair<IDInfo, std::pair<TotalOutput, TotalOutputWeights> > > deltas;
        CvCity* pFirstCity = getCity(*(*plotsIter)->possibleCities.begin());
        CvCity* pOtherCity = (*plotsIter)->possibleCities.size() == 1 ? NULL : getCity(*(*plotsIter)->possibleCities.rbegin());

        for (SharedPlotItem::PossibleCitiesConstIter citiesIter((*plotsIter)->possibleCities.begin()), endIter((*plotsIter)->possibleCities.end()); citiesIter != endIter; ++citiesIter)
        {
            CvCity* pCity = getCity(*citiesIter);

            TotalOutput output, baseline;
            TotalOutputWeights weights;
            
            //setWorkingCity_((*plotsIter)->possibleCities[j], (*plotsIter)->coords, (*plotsIter)->possibleCities[j]);
            pPlot->setWorkingCityOverride(pCity);

            if (pCity && !pCity->isOccupation() && pCity->canWork(pPlot))
            {
                {
                    boost::shared_ptr<CityData> pCityData(new CityData(pCity));
                    CitySimulation simulation(pCity, pCityData);
                    SimulationOutput simOutput = simulation.simulateAsIs(10);

                    output = *simOutput.cumulativeOutput.rbegin();
                    weights = simulation.getCityOptimiser()->getMaxOutputWeights();
#ifdef ALTAI_DEBUG
                    {  // debug
                        CityLog::getLog(pCityData->pCity)->getStream() << "\nPlots with shared plot: " << (*plotsIter)->coords << "\n";
                        simulation.logPlots(true);
                    }
#endif
                }

                //setWorkingCity_((*plotsIter)->possibleCities[j], (*plotsIter)->coords, IDInfo());
                pPlot->setWorkingCityOverride(pOtherCity);
                //plotsIter->assignedCity = IDInfo();
                {
                    boost::shared_ptr<CityData> pCityData(new CityData(pCity));
                    CitySimulation simulation(pCity, pCityData);
                    SimulationOutput simOutput = simulation.simulateAsIs(10);
                    baseline = *simOutput.cumulativeOutput.rbegin();
                    weights = mergeMax(simulation.getCityOptimiser()->getMaxOutputWeights(), weights);
#ifdef ALTAI_DEBUG
                    {  // debug
                        CityLog::getLog(pCityData->pCity)->getStream() << "\nPlots without shared plot: " << (*plotsIter)->coords << "\n";
                        simulation.logPlots(true);
                    }
#endif
                }
                deltas.push_back(std::make_pair(*citiesIter, std::make_pair(output - baseline, weights)));
            }
            pOtherCity = pFirstCity;
        }

        // todo - if best value is 0, see if another city had a positive component to its value (e.g. increased hammer output, but less science, etc...)
        int bestCityValue = std::numeric_limits<int>::min();
        IDInfo bestCity;
        for (size_t k = 0, count = deltas.size(); k < count; ++k)
        {
            const CvCity* pCity = getCity(deltas[k].first);
            TotalOutputWeights outputWeights = player_.getCity(pCity->getID()).getMaxOutputWeights();
            TotalOutputValueFunctor valueF(deltas[k].second.second);

            if (valueF(deltas[k].second.first) > bestCityValue)
            {
                bestCityValue = valueF(deltas[k].second.first);
                bestCity = deltas[k].first;
            }
        }
        // actually set override
        CvCity* pBestCity = getCity(bestCity);
        pPlot->setWorkingCityOverride(pBestCity);  // can be no city

        // changed working city
        if (pBestCity && !(pBestCity->getIDInfo() == origAssignedCity))
        {
            CitiesImprovements::iterator improvementsIter = citiesImprovements_.find(CityImprovementManager(origAssignedCity));
            CitiesImprovements::iterator newImprovementsIter = citiesImprovements_.find(CityImprovementManager(bestCity));
            if (improvementsIter != citiesImprovements_.end() && newImprovementsIter != citiesImprovements_.end())
            {
                std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementsIter->getImprovements();

                // try and find the current improvement for this shared plot
                for (size_t i = 0, count = improvements.size(); i < count; ++i)
                {
                    if (boost::get<0>(improvements[i]) == (*plotsIter)->coords)
                    {
                        bool found = false;
                        std::vector<CityImprovementManager::PlotImprovementData>& newImprovements = newImprovementsIter->getImprovements();
                        for (size_t j = 0, count = newImprovements.size(); j < count; ++j)
                        {
                            if (boost::get<0>(newImprovements[j]) == (*plotsIter)->coords)
                            {
                                found = true;
                                newImprovements[j] = improvements[i];
#ifdef ALTAI_DEBUG
                                {   // debug
                                    std::ostream& os = CityLog::getLog(pBestCity)->getStream();
                                    os << "\nAssigned shared plot: " << (*plotsIter)->coords << " with improvement data: ";
                                    newImprovementsIter->logImprovement(os, newImprovements[j]);
                                    os << "\n";
                                    newImprovementsIter->logImprovements();
                                }
#endif
                                break;
                            }
                        }

                        if (!found)
                        {
                            newImprovementsIter->getImprovements().push_back(improvements[i]);
#ifdef ALTAI_DEBUG
                            {   // debug
                                std::ostream& os = CityLog::getLog(pBestCity)->getStream();
                                os << "\nAdded shared plot: " << (*plotsIter)->coords << " with improvement data: ";
                                newImprovementsIter->logImprovement(os, *newImprovementsIter->getImprovements().rbegin());
                                os << "\n";
                                newImprovementsIter->logImprovements();
                            }
#endif
                        }
                        break;
                    }
                }
            }
        }

        // call this too - as we may not be called back from CvPlot fn if it thinks the city hasn't changed (and we've been changing the city behind its back)
        //setWorkingCity(pPlot, getCity(origAssignedCity), getCity(bestCity));
#ifdef ALTAI_DEBUG
        {
            PlotInfo plotInfo(pPlot, player_.getPlayerID());

            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nCoords = " << (*plotsIter)->coords
                << " (yield = " << getYield(plotInfo.getInfo(), player_.getPlayerID(), pPlot->getImprovementType(), pPlot->getFeatureType(), pPlot->getRouteType()) << ") ";
            for (size_t k = 0, count = deltas.size(); k < count; ++k)
            {
                if (k > 0) os << ", ";
                const CvCity* pCity = getCity(deltas[k].first);
                //TotalOutputWeights outputWeights = player_.getCity(pCity->getID()).getMaxOutputWeights();
                TotalOutputValueFunctor valueF(deltas[k].second.second);

                os << " city: " << narrow(pCity->getName()) << " = " << deltas[k].second.first << " value = " << valueF(deltas[k].second.first) << " (weights = " << deltas[k].second.second << ")";
            }
            os << " best city = " << (pBestCity ? narrow(pBestCity->getName()) : "NONE") << " current plot setting = "
                << (pPlot->getWorkingCity() ? narrow(pPlot->getWorkingCity()->getName()) : " none ") << "\n";
        }
#endif
    }

    // save/load functions
    void MapAnalysis::write(FDataStreamBase* pStream) const
    {
        writeComplexSet(pStream, citiesImprovements_);
    }

    void MapAnalysis::read(FDataStreamBase* pStream)
    {
        readComplexSet(pStream, citiesImprovements_);
    }

    void MapAnalysis::debugSharedPlots() const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();

        for (SharedPlots::const_iterator ci(sharedPlots_.begin()), ciEnd(sharedPlots_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->city);
            if (pCity)
            {
                os << "\nCity: " << narrow(pCity->getName()) << " shared plots:\n";
                for (CitySharedPlots::SharedPlotsConstIter plotsIter(ci->sharedPlots.begin()), endIter(ci->sharedPlots.end()); plotsIter != endIter; ++plotsIter)
                {
                    os << "\tPlot: " << (*plotsIter)->coords << " shared with: ";
                    for (SharedPlotItem::PossibleCitiesConstIter citiesIter((*plotsIter)->possibleCities.begin()), endIter((*plotsIter)->possibleCities.end()); citiesIter != endIter; ++citiesIter)
                    {
                        if (citiesIter != (*plotsIter)->possibleCities.begin()) os << ", ";

                        const CvCity* pCity = getCity(*citiesIter);
                        if (pCity)
                        {
                            os << narrow(pCity->getName());
                        }
                        else
                        {
                            os << "unknown city?";
                        }
                    }

                    if (!((*plotsIter)->assignedCity == IDInfo()))
                    {
                        const CvCity* pCity = getCity((*plotsIter)->assignedCity);
                        if (pCity)
                        {
                            os << " worked by: " << narrow(pCity->getName());
                        }
                        else
                        {
                            os << " worked by unknown city? (not controlled by AltAI?)";
                        }
                    }
                    else
                    {
                        os << " not worked ";
                    }

                    CvPlot* pPlot = gGlobals.getMap().plot((*plotsIter)->coords.iX, (*plotsIter)->coords.iY);
                    os << " (Plot = " << (pPlot->getWorkingCity() ? narrow(pPlot->getWorkingCity()->getName()) : " none ") << ")\n";
                }
            }
        }
#endif
    }
}