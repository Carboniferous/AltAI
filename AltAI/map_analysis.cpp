#include "AltAI.h"

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
    namespace
    {
        struct ImprovementsAndYieldsFinder
        {
            ImprovementsAndYieldsFinder(const std::pair<PlotYield, ImprovementTypes>& yieldAndImprovement_)
                : yieldAndImprovement(yieldAndImprovement_)
            {
            }

            bool operator() (const std::pair<PlotYield, ImprovementTypes>& other) const
            {
                return yieldAndImprovement.first == other.first && yieldAndImprovement.second == other.second;
            }

            std::pair<PlotYield, ImprovementTypes> yieldAndImprovement;
        };

        bool debugImprovementLists(const MapAnalysis::PlotValues::ImprovementsAndYields& first, const MapAnalysis::PlotValues::ImprovementsAndYields& second, std::ostream& os, bool reverse, int key)
        {
            bool differ = false;
            for (size_t i = 0, count = first.size(); i < count; ++i)
            {
                if (std::find_if(second.begin(), second.end(), ImprovementsAndYieldsFinder(first[i])) == second.end())
                {
                    os << "\nKey: " << key << " - failed to find: " << first[i].first << ", " 
                       << (first[i].second == NO_IMPROVEMENT ? "NO_IMPROVEMENT" : gGlobals.getImprovementInfo(first[i].second).getType())
                       << " in " << (reverse ? "old" : "new") << " improvements map";
                    differ = true;
                }
            }
            return differ;
        }

        bool comparePlotValueMaps(const MapAnalysis::PlotValues& first, const MapAnalysis::PlotValues& second, std::ostream& os, bool reverse)
        {
            bool differ = false;
            for (MapAnalysis::PlotValues::PlotValueMap::const_iterator plotValueIter1(first.plotValueMap.begin()), plotValueEndIter1(first.plotValueMap.end());
                plotValueIter1 != plotValueEndIter1; ++plotValueIter1)
            {
                MapAnalysis::PlotValues::PlotValueMap::const_iterator plotValueIter2(second.plotValueMap.find(plotValueIter1->first)), plotValueEndIter2(second.plotValueMap.end());
                if (plotValueIter2 == plotValueEndIter2)
                {
                    os << "\nFailed to find sub area in " << (reverse ? "old" : "new") << " map: " << plotValueIter1->first;
                    differ = true;
                }
                else
                {
                    for (MapAnalysis::PlotValues::SubAreaPlotValueMap::const_iterator subAreaIter1(plotValueIter1->second.begin()), subAreaEndIter1(plotValueIter1->second.end());
                         subAreaIter1 != subAreaEndIter1; ++subAreaIter1)
                    {
                        MapAnalysis::PlotValues::SubAreaPlotValueMap::const_iterator subAreaIter2(plotValueIter2->second.find(subAreaIter1->first)), subAreaEndIter2(plotValueIter2->second.end());
                        if (subAreaIter2 == subAreaEndIter2)
                        {
                            os << "\nFailed to find potential city plot: " << subAreaIter1->first << " in " << (reverse ? "old" : "new") << " map for sub area: " << plotValueIter1->first;
                            differ = true;
                        }
                        else
                        {
                            for (MapAnalysis::PlotValues::PlotKeyCoordsMap::const_iterator plotsIter1(subAreaIter1->second.begin()), plotsEndIter1(subAreaIter1->second.end());
                                plotsIter1 != plotsEndIter1; ++plotsIter1)
                            {
                                MapAnalysis::PlotValues::PlotKeyCoordsMap::const_iterator plotsIter2(subAreaIter2->second.find(plotsIter1->first)), plotsEndIter2(subAreaIter2->second.end());
                                if (plotsIter2 == plotsEndIter2)
                                {
                                    os << "\nFailed to find plot key: " << plotsIter1->first << " in " << (reverse ? "old" : "new") << " map for potential city plot: "
                                       << subAreaIter1->first << " and sub area: " << plotValueIter1->first;
                                    differ = true;
                                }
                                else
                                {
                                    for (std::set<XYCoords>::const_iterator plotCoordsIter1(plotsIter1->second.begin()), plotCoordsEndIter1(plotsIter1->second.end());
                                        plotCoordsIter1 != plotCoordsEndIter1; ++plotCoordsIter1)
                                    {
                                        std::set<XYCoords>::const_iterator plotCoordsIter2(plotsIter2->second.find(*plotCoordsIter1)), plotCoordsEndIter2(plotsIter2->second.end());
                                        if (plotCoordsIter2 == plotCoordsEndIter2)
                                        {
                                            os << "\nFailed to find coord: " << *plotCoordsIter1 << " in " << (reverse ? "old" : "new") << " map for key: "
                                               << plotsIter1->first << " for potential city plot: " << subAreaIter1->first
                                               << " and sub area: " << plotValueIter1->first;
                                            differ = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            for (MapAnalysis::PlotValues::KeysValueMap::const_iterator keyIter1(first.keysValueMap.begin()), keyEndIter1(first.keysValueMap.end());
                keyIter1 != keyEndIter1; ++keyIter1)
            {
                MapAnalysis::PlotValues::KeysValueMap::const_iterator keyIter2 = second.keysValueMap.find(keyIter1->first);
                if (keyIter2 == second.keysValueMap.end())
                {
                    os << "\nFailed to find key: " << keyIter1->first << " in " << (reverse ? "old" : "new") << " plot improvements map";
                    differ = true;
                }
                else
                {
                    if (keyIter1->second.size() != keyIter2->second.size())
                    {
                        os << "\nDifferent improvement lists for key: " << keyIter1->first
                           << (reverse ? " old" : " new") << " size = " << keyIter2->second.size()
                           << (reverse ? " new" : " old") << " size = " << keyIter1->second.size();
                    }

                    bool impDiffer = debugImprovementLists(keyIter1->second, keyIter2->second, os, reverse, keyIter1->first);
                    differ = impDiffer || differ;
                }
            }

            return differ;
        }

        bool comparePlotValueMaps(const MapAnalysis::PlotValues& first, const MapAnalysis::PlotValues& second, std::ostream& os)
        {
            os << "\n(comparePlotValueMaps): Turn = " << gGlobals.getGame().getGameTurn();

            bool differ1 = comparePlotValueMaps(first, second, os, false);
            bool differ2 = comparePlotValueMaps(second, first, os, true);
            return differ1 || differ2;
        }

        void debugPlotValueMap(const MapAnalysis::PlotValues& map, std::ostream& os)
        {
            os << "\n(debugPlotValueMap): Turn = " << gGlobals.getGame().getGameTurn();

            for (MapAnalysis::PlotValues::PlotValueMap::const_iterator plotValueIter(map.plotValueMap.begin()), plotValueEndIter(map.plotValueMap.end());
                plotValueIter != plotValueEndIter; ++plotValueIter)
            {
                os << "\nSub area: " << plotValueIter->first;

                for (MapAnalysis::PlotValues::SubAreaPlotValueMap::const_iterator subAreaIter(plotValueIter->second.begin()), subAreaEndIter(plotValueIter->second.end());
                     subAreaIter != subAreaEndIter; ++subAreaIter)
                {
                    os << "\n\tplot: " << subAreaIter->first;

                    for (MapAnalysis::PlotValues::PlotKeyCoordsMap::const_iterator plotsIter(subAreaIter->second.begin()), plotsEndIter(subAreaIter->second.end());
                        plotsIter != plotsEndIter; ++plotsIter)
                    {
                        os << "\n\t\t(key = " << plotsIter->first;

                        for (std::set<XYCoords>::const_iterator plotCoordsIter(plotsIter->second.begin()), plotCoordsEndIter(plotsIter->second.end());
                            plotCoordsIter != plotCoordsEndIter; ++plotCoordsIter)
                        {
                            if (plotCoordsIter != plotCoordsEndIter) os << ", ";
                            os << *plotCoordsIter;
                        }
                        os << ")";
                    }
                }
            }

            for (MapAnalysis::PlotValues::KeysValueMap::const_iterator keyIter(map.keysValueMap.begin()), keyEndIter(map.keysValueMap.end());
                keyIter != keyEndIter; ++keyIter)
            {
                os << "\nKey = " << keyIter->first;

                for (size_t i = 0, count = keyIter->second.size(); i < count; ++i)
                {
                    if (i > 0) os << ",";

                    os << " (";
                    if (keyIter->second[i].second != NO_IMPROVEMENT)
                    {
                        os << gGlobals.getImprovementInfo(keyIter->second[i].second).getType();
                    }
                    else
                    {
                        os << "NO_IMPROVEMENT";
                    }
                    os << ", " << keyIter->second[i].first << ")";
                }
            }
        }

        // like canFound, but uses this player's view of the map, rather than the actual map (less of a cheat)
        bool couldFoundAtPlot(const CvPlot* pPlot, const Player& player)
        {
            if (pPlot->isImpassable())
        	{
		        return false;
	        }

	        if (pPlot->getFeatureType() != NO_FEATURE)
	        {
		        if (GC.getFeatureInfo(pPlot->getFeatureType()).isNoCity())
		        {
			        return false;
		        }
	        }

            PlayerTypes plotOwner = pPlot->getRevealedOwner(player.getTeamID(), false);
            if (plotOwner != NO_PLAYER && plotOwner != player.getPlayerID())
	        {
		        return false;
	        }

	        bool validPlot = false;
            const CvTerrainInfo& terrainInfo = gGlobals.getTerrainInfo(pPlot->getTerrainType());

            if (terrainInfo.isFound() || terrainInfo.isFoundCoast() || (terrainInfo.isFoundFreshWater() && pPlot->isFreshWater()))
	        {
		        validPlot = true;
	        }

            // note we aren't bothering with canFoundCitiesOnWater() python callback
            if (pPlot->isWater())
            {
                return false;
            }

	    	int range = gGlobals.getMIN_CITY_RANGE();

		    for (int iDX = -range; iDX <= range; iDX++)
		    {
			    for (int iDY = -range; iDY <= range; iDY++)
			    {
				    const CvPlot* pLoopPlot = plotXY(pPlot->getX(), pPlot->getY(), iDX, iDY);

				    if (pLoopPlot && pLoopPlot->isCity() && pLoopPlot->area() == pPlot->area())
                    {
                        return false;
    				}
	    		}
		    }

            return validPlot;
        }
    }

    MapAnalysis::MapAnalysis(Player& player) : init_(false), player_(player)
    {
    }

    MapAnalysis::AreaDetail::AreaDetail(int ID_) : ID(ID_), knownTileCount(1), allKnown(false)
    {
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

    // getters
    const PlotInfo::PlotInfoNode& MapAnalysis::getPlotInfoNode(const CvPlot* pPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
        XYCoords coords(pPlot->getX(), pPlot->getY());
        KeyInfoMap::const_iterator keyIter = keyInfoMap_.find(coords);

        if (keyIter != keyInfoMap_.end())
        {
            PlotInfoMap::iterator iter = plotInfoMap_.find(keyIter->second);
            if (iter != plotInfoMap_.end())
            {
#ifdef ALTAI_DEBUG
                PlotInfo plotInfo(pPlot, player_.getPlayerID());
                int key = plotInfo.getKey();
                if (key != keyIter->second)
                {
                    os << "\n(getPlotInfoNode): Inconsistent plot keys for coords: " << coords << " keys = " << key << ", " << keyIter->second
                       << "\n" << iter->second << "\n" << plotInfo.getInfo();
                }
                else if (!(iter->second == plotInfo.getInfo()))
                {
                    os << "\n(getPlotInfoNode): Inconsistent plot info for coords: " << coords << " keys = " << key << ", " << keyIter->second
                       << "\n" << iter->second << "\n" << plotInfo.getInfo();
                }
#endif
                return iter->second;
            }
        }

        // add key
#ifdef ALTAI_DEBUG
        os << "\nPlotInfo missing for coords: " << coords;
#endif
        return addPlotInfo_(pPlot).second->second;
    }

    const MapAnalysis::PlotValues& MapAnalysis::getPlotValues() const
    {
        return plotValues_;
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

    std::vector<int> MapAnalysis::getAccessibleSubAreas(DomainTypes domainType) const
    {
        std::vector<int> accessibleSubAreas;
        const CvMap& theMap = gGlobals.getMap();

        for (SubAreaMapConstIter ci(revealedSubAreaDataMap_.begin()), ciEnd(revealedSubAreaDataMap_.end()); ci != ciEnd; ++ci)
        {
            boost::shared_ptr<SubArea> pSubArea = theMap.getSubArea(ci->first);
            if (!pSubArea->isImpassable())
            {
                // add domain type to sub area class?
                if (pSubArea->isWater())
                {
                    if (domainType == DOMAIN_SEA)
                    {
                        accessibleSubAreas.push_back(ci->first);
                    }
                }
                else if (domainType == DOMAIN_LAND)
                {
                    accessibleSubAreas.push_back(ci->first);
                }
            }
        }

        return accessibleSubAreas;
    }

    std::vector<CvPlot*> MapAnalysis::getResourcePlots(const std::vector<BonusTypes>& bonusTypes, int subArea) const
    {
        std::vector<CvPlot*> resourcePlots;
        const CvMap& theMap = gGlobals.getMap();

        ResourcesMapConstIter ci = resourcesMap_.find(subArea);
        if (ci != resourcesMap_.end())
        {
            for (size_t i = 0, count = bonusTypes.size(); i < count; ++i)
            {
                std::map<BonusTypes, ResourceData::ResourcePlotData>::const_iterator bi(ci->second.subAreaResourcesMap.find(bonusTypes[i]));
                if (bi != ci->second.subAreaResourcesMap.end())
                {
                    for (size_t i = 0, count = bi->second.size(); i < count; ++i)
                    {
                        if (bi->second[i].second == player_.getPlayerID())
                        {
                            resourcePlots.push_back(theMap.plot(bi->second[i].first.iX, bi->second[i].first.iY));
                        }              
                    }
                }
            }
        }

        // todo - check for duplicates
        return resourcePlots;
    }

    // initialisation functions
    void MapAnalysis::init()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
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
#ifdef ALTAI_DEBUG
        debugPlotValueMap(plotValues_, os);
#endif
        init_ = true;
    }

    void MapAnalysis::reinitDotMap()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
        const CvMap& theMap = gGlobals.getMap();
        TeamTypes teamType = player_.getTeamID();

        previousPlotValues_ = plotValues_;
        plotValues_.plotValueMap.clear();
        plotValues_.keysValueMap.clear();

        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
	    {
		    const CvPlot* pPlot = theMap.plotByIndex(i);

            if (pPlot->isRevealed(teamType, false))
            {
                PlayerTypes owner = pPlot->getRevealedOwner(teamType, false);

                std::pair<int, MapAnalysis::PlotInfoMap::iterator> keyAndIter = updatePlotInfo_(pPlot);

                updateKeysValueMap_(keyAndIter.first, keyAndIter.second->second);

                if (owner == NO_PLAYER || owner == player_.getPlayerID())
                {
                    addDotMapPlot_(pPlot, keyAndIter.second->second);
                }
            }
        }

#ifdef ALTAI_DEBUG
        if (comparePlotValueMaps(previousPlotValues_, plotValues_, os))
        {
            debugPlotValueMap(previousPlotValues_, os);
            debugPlotValueMap(plotValues_, os);
        }
#endif
    }

    void MapAnalysis::update()
    {
        if (!init_)
        {
            init();
        }
    }

    // plot update functions
    void MapAnalysis::pushPlotEvent(const boost::shared_ptr<IPlotEvent>& pEvent)
    {
        if (!init_)
        {
            init();
        }

        pEvent->handle(*this);
    }

    void MapAnalysis::updatePlotRevealed(const CvPlot* pPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nupdatePlotRevealed(): updating plot: " << XYCoords(pPlot->getX(), pPlot->getY());
#endif
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

        std::pair<int, MapAnalysis::PlotInfoMap::iterator> keyAndIter = addPlotInfo_(pPlot);

        updateKeysValueMap_(keyAndIter.first, keyAndIter.second->second);

        PlayerTypes owner = pPlot->getRevealedOwner(player_.getTeamID(), false);
        // consider plots we or nobody owns
        if (owner == NO_PLAYER || owner == player_.getPlayerID())
        {
            addDotMapPlot_(pPlot, keyAndIter.second->second);
        }

        // update resource data
        updateResourceData_(pPlot);

        // build data for shared plots - can we do this from addCity?
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

    void MapAnalysis::updatePlotFeature(const CvPlot* pPlot, FeatureTypes oldFeatureType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nUpdating feature type for plot : " << XYCoords(pPlot->getX(), pPlot->getY())
           << " old feature = " << (oldFeatureType == NO_FEATURE ? "NO_FEATURE" : gGlobals.getFeatureInfo(oldFeatureType).getType())
           << " new feature = " << (pPlot->getFeatureType() == NO_FEATURE ? "NO_FEATURE" : gGlobals.getFeatureInfo(pPlot->getFeatureType()).getType());
#endif

        XYCoords coords(pPlot->getX(), pPlot->getY());
        // update key for this plot and add to plotInfoMap_ if required
        const int oldKey = keyInfoMap_[coords];

        std::pair<int, MapAnalysis::PlotInfoMap::iterator> keyAndIter = updatePlotInfo_(pPlot);
        
        const int subAreaID = pPlot->getSubArea();

        updatePlotValueKey_(pPlot, oldKey, keyAndIter.first);
        updateKeysValueMap_(keyAndIter.first, keyAndIter.second->second);
    }

    void MapAnalysis::updatePlotBonus(const CvPlot* pPlot, BonusTypes bonusType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nUpdating bonus type for plot : " << XYCoords(pPlot->getX(), pPlot->getY())
           << " bonus = " << (bonusType == NO_BONUS ? "NO_BONUS" : gGlobals.getBonusInfo(bonusType).getType());
#endif

        XYCoords coords(pPlot->getX(), pPlot->getY());
        // update key for this plot and add to plotInfoMap_ if required
        const int oldKey = keyInfoMap_[coords];

        std::pair<int, MapAnalysis::PlotInfoMap::iterator> keyAndIter = updatePlotInfo_(pPlot);
       
        updatePlotValueKey_(pPlot, oldKey, keyAndIter.first);
        updateKeysValueMap_(keyAndIter.first, keyAndIter.second->second);
    }

    void MapAnalysis::updatePlotCulture(const CvPlot* pPlot, bool remove)
    {
        XYCoords coords(pPlot->getX(), pPlot->getY());

        const PlotInfo::PlotInfoNode& plotInfo = getPlotInfoNode(pPlot);
        const int key = keyInfoMap_[coords];
        const int subAreaID = pPlot->getSubArea();

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\n " << (remove ? "Removing " : " Adding ") << "plot: (" << pPlot->getX() << ", " << pPlot->getY() << ")";
#endif

        // ok to use getOwner and not getRevealedOwner as this call is made to teams which can see the plot
        if (remove && pPlot->getOwner() != NO_PLAYER)
        {
            removePlotValuePlot_(pPlot);

            for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            {
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    if (pLoopPlot.valid())
                    {
                        if (pLoopPlot->isCity() && pLoopPlot->getPlotCity()->getOwner() == player_.getPlayerID())
                        {
                            const CvCity* pCity = pLoopPlot->getPlotCity();
                            player_.setCityDirty(pCity->getIDInfo());
                            player_.getCity(pCity).setFlag(City::NeedsImprovementCalcs);
                        }
                    }
                }
            }
        }
        else
        {
            // if we are here, means we own the plot (unowned plots should be added through updatePlotRevealed)
            if (hasPossibleYield(plotInfo, player_.getPlayerID()))
            {
                for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
                {
                    CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                    while (IterPlot pLoopPlot = plotIter())
                    {
                        // consider plots we have seen and can theoretically found a city at
                        if (pLoopPlot.valid() && pLoopPlot->isRevealed(player_.getTeamID(), false))
                        {
                            if (pLoopPlot->isCity() && pLoopPlot->getPlotCity()->getOwner() == player_.getPlayerID())
                            {
                                const CvCity* pCity = pLoopPlot->getPlotCity();
                                player_.setCityDirty(pCity->getIDInfo());
                                player_.getCity(pCity).setFlag(City::NeedsImprovementCalcs);
                            }
                        }
                    }
                }
            }

            // just update if plot is being added - only get called if plot is visible to us (our team) (think this works correctly!)
            updateResourceData_(pPlot);
        }
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

    void MapAnalysis::removePlotValuePlot_(const CvPlot* pPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nRemoving plot: " << XYCoords(pPlot->getX(), pPlot->getY()) << " from plot values map.";
#endif

        const XYCoords coords(pPlot->getX(), pPlot->getY());
        const int key = keyInfoMap_[coords];

        for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
        {
            CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
            while (IterPlot pLoopPlot = plotIter())
            {
                if (pLoopPlot.valid())
                {
                    // don't test if we can found city - as this might have changed in the meantime
                    PlotValues::PlotValueMap::iterator subAreaIter = plotValues_.plotValueMap.find(pLoopPlot->getSubArea());
                    if (subAreaIter != plotValues_.plotValueMap.end())
                    {
                        PlotValues::SubAreaPlotValueMap::iterator plotIter = subAreaIter->second.find(XYCoords(pLoopPlot->getX(), pLoopPlot->getY()));
                        if (plotIter != subAreaIter->second.end())
                        {
                            std::map<int /* plot key */, std::set<XYCoords> >::iterator plotKeyIter = plotIter->second.find(key);
                            if (plotKeyIter != plotIter->second.end())
                            {
                                std::set<XYCoords>::iterator coordsIter = plotKeyIter->second.find(coords);
                                if (coordsIter != plotKeyIter->second.end())
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nRemoving entry for plot key: " << key << " for potential city plot: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY());
#endif
                                    plotKeyIter->second.erase(coordsIter);
                                }

                                if (plotKeyIter->second.empty())
                                {
                                    plotIter->second.erase(plotKeyIter);
                                }
                            }
                        }
                    }
                }
            }
        }

        PlotValues::PlotValueMap::iterator subAreaIter = plotValues_.plotValueMap.find(pPlot->getSubArea());

        if (subAreaIter != plotValues_.plotValueMap.end())
        {
            PlotValues::SubAreaPlotValueMap::iterator plotIter = subAreaIter->second.find(coords);
            if (plotIter != subAreaIter->second.end())
            {
#ifdef ALTAI_DEBUG
                os << "\nRemoving entries for potential city plot: " << XYCoords(pPlot->getX(), pPlot->getY());
#endif
                subAreaIter->second.erase(plotIter);
            }

            if (subAreaIter->second.empty())
            {
#ifdef ALTAI_DEBUG
                os << "\nRemoving entries for sub area: " << pPlot->getSubArea();
#endif
                plotValues_.plotValueMap.erase(subAreaIter);
            }
        }
    }

    // plot update helper functions
    std::pair<int, MapAnalysis::PlotInfoMap::iterator> MapAnalysis::addPlotInfo_(const CvPlot* pPlot)
    {
        PlotInfo plotInfo(pPlot, player_.getPlayerID());
        int key = plotInfo.getKey();
        keyInfoMap_[XYCoords(pPlot->getX(), pPlot->getY())] = key;

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//        os << "\n(addPlotInfo_): Adding plot info for key value = " << key << ", plot: " << XYCoords(pPlot->getX(), pPlot->getY()) << "\n" << plotInfo.getInfo();
#endif

        PlotInfoMap::iterator iter = plotInfoMap_.find(key);
        if (iter == plotInfoMap_.end())
        {
            iter = plotInfoMap_.insert(std::make_pair(key, plotInfo.getInfo())).first;
        }
        else
        {
            if (!(plotInfo.getInfo() == iter->second))
            {
#ifdef ALTAI_DEBUG
                os << "\n(addPlotInfo_): Inconsistent plot keys:\n" << plotInfo.getInfo() << "\n" << iter->second;
#endif
                iter->second = plotInfo.getInfo();
            }
        }
        return std::make_pair(key, iter);
    }

    std::pair<int, MapAnalysis::PlotInfoMap::iterator> MapAnalysis::updatePlotInfo_(const CvPlot* pPlot)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//#endif
        const XYCoords coords(pPlot->getX(), pPlot->getY());

        PlotInfo plotInfo(pPlot, player_.getPlayerID());
        int key = plotInfo.getKey();

        keyInfoMap_[coords] = key;

        PlotInfoMap::iterator iter = plotInfoMap_.find(key);
        if (iter == plotInfoMap_.end())
        {
            iter = plotInfoMap_.insert(std::make_pair(key, plotInfo.getInfo())).first;

//#ifdef ALTAI_DEBUG
//            os << "\n(updatePlotInfo_): Adding plot info: " << key << ", for coords = " << XYCoords(pPlot->getX(), pPlot->getY())
//               << "\n" << plotInfo.getInfo();
//#endif
        }
        else
        {
            iter->second = plotInfo.getInfo();

//#ifdef ALTAI_DEBUG
//            os << "\n(updatePlotInfo_): Updating plot info: " << key << ", for coords = " << XYCoords(pPlot->getX(), pPlot->getY())
//               << "\n" << plotInfo.getInfo();
//#endif
        }

        return std::make_pair(key, iter);
    }

    void MapAnalysis::addDotMapPlot_(const CvPlot* pPlot, const PlotInfo::PlotInfoNode& plotInfo)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();       
//#endif
        const XYCoords coords(pPlot->getX(), pPlot->getY());
        const int key = keyInfoMap_[coords];
        const TeamTypes teamType = player_.getTeamID();

        if (hasPossibleYield(plotInfo, player_.getPlayerID()))
        {
            for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            {
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    // consider plots we have seen and can theoretically found a city at
                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(teamType, false) && !pLoopPlot->isCity())
                    {
                        if (couldFoundAtPlot(pLoopPlot, player_))
                        {
//#ifdef ALTAI_DEBUG
//                            os << "\nAdding plot: " << coords << " with key = " << key << " to plot value map for sub area: " << pLoopPlot->getSubArea()
//                               << " for potential city: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY());
//#endif
                            plotValues_.plotValueMap[pLoopPlot->getSubArea()][XYCoords(pLoopPlot->getX(), pLoopPlot->getY())][key].insert(coords);
                        }
                    }
                }
            }
        }

        if (couldFoundAtPlot(pPlot, player_))
        {
            for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            {
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    // consider plots we have seen and can theoretically found a city at
                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(teamType, false))
                    {
                        PlayerTypes playerType = pLoopPlot->getRevealedOwner(teamType, false);

                        if (playerType == NO_PLAYER || playerType == player_.getPlayerID())
                        {
                            const PlotInfo::PlotInfoNode& thisNode = getPlotInfoNode(pLoopPlot);
                            const int thisKey = keyInfoMap_[XYCoords(pLoopPlot->getX(), pLoopPlot->getY())];

                            if (hasPossibleYield(thisNode, player_.getPlayerID()))
                            {
//#ifdef ALTAI_DEBUG
//                                os << "\nAdding plot: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY()) << " with key = " << thisKey << " to plot value map for sub area: " << pPlot->getSubArea()
//                                   << " for potential city: " << XYCoords(pPlot->getX(), pPlot->getY());
//#endif
                                plotValues_.plotValueMap[pPlot->getSubArea()][XYCoords(pPlot->getX(), pPlot->getY())][thisKey].insert(XYCoords(pLoopPlot->getX(), pLoopPlot->getY()));
                            }
                        }
                    }
                }
            }
        }
    }

    void MapAnalysis::updatePlotValueKey_(const CvPlot* pPlot, int oldKey, int newKey)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//        os << "\nUpdating plot keys for plot: " << XYCoords(pPlot->getX(), pPlot->getY()) << " from: " << oldKey << " to: " << newKey;
//#endif
        const TeamTypes teamType = player_.getTeamID();
        const PlayerTypes playerType = player_.getPlayerID();
        const XYCoords coords(pPlot->getX(), pPlot->getY());

        // update plot keys
        for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
        {
            CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
            while (IterPlot pLoopPlot = plotIter())
            {
                if (pLoopPlot.valid() && pLoopPlot->isRevealed(teamType, false))
                {
                    const PlayerTypes plotOwner = pLoopPlot->getRevealedOwner(teamType, false);

                    if (plotOwner == NO_PLAYER || plotOwner == player_.getPlayerID())
                    {
                        PlotValues::PlotValueMap::iterator subAreaIter = plotValues_.plotValueMap.find(pLoopPlot->getSubArea());
                        if (subAreaIter != plotValues_.plotValueMap.end())
                        {
                            PlotValues::SubAreaPlotValueMap::iterator plotIter = subAreaIter->second.find(XYCoords(pLoopPlot->getX(), pLoopPlot->getY()));

                            if (plotIter != subAreaIter->second.end())
                            {
                                PlotValues::PlotKeyCoordsMap::iterator plotKeyIter = plotIter->second.find(oldKey);
                                if (plotKeyIter != plotIter->second.end())
                                {
//#ifdef ALTAI_DEBUG
//                                    os << "\nErasing entry for plot key for potential city plot: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY());
//#endif
                                    // erase entry for bonus plot with old plot key
                                    plotKeyIter->second.erase(coords);

                                    if (plotKeyIter->second.empty())
                                    {
                                        plotIter->second.erase(plotKeyIter);
                                    }
                                }
//#ifdef ALTAI_DEBUG
//                                os << "\nAdding entry for plot key for potential city plot: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY());
//#endif
                                // insert new key - only add if had existing entry (new entries are added separately and should automatically get the correct key)
                                subAreaIter->second[XYCoords(pLoopPlot->getX(), pLoopPlot->getY())][newKey].insert(coords);
                            }
                        }
                    }
                }
            }
        }
    }

    void MapAnalysis::updateResourceData_(const CvPlot* pPlot)
    {
        const int subAreaID = pPlot->getSubArea();
        const XYCoords coords(pPlot->getX(), pPlot->getY());
        PlayerTypes owner = pPlot->getRevealedOwner(player_.getTeamID(), false);

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

    void MapAnalysis::updateKeysValueMap_(int key, const PlotInfo::PlotInfoNode& plotInfo)
    {
        plotValues_.keysValueMap[key] = getYields(plotInfo, player_.getPlayerID(),
            player_.getCvPlayer()->isBarbarian() ? BarbDotMapTechDepth : DotMapTechDepth);
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

    // shared plot functions
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
                    CityDataPtr pCityData(new CityData(pCity));
                    CitySimulation simulation(pCity, pCityData);
                    SimulationOutput simOutput = simulation.simulateAsIs(10);

                    output = *simOutput.cumulativeOutput.rbegin();
                    weights = makeOutputW(3, 4, 3, 3, 1, 1);//simulation.getCityOptimiser()->getMaxOutputWeights();
#ifdef ALTAI_DEBUG
                    {  // debug
                        CityLog::getLog(pCityData->getCity())->getStream() << "\nPlots with shared plot: " << (*plotsIter)->coords << "\n";
                        simulation.logPlots(true);
                    }
#endif
                }

                //setWorkingCity_((*plotsIter)->possibleCities[j], (*plotsIter)->coords, IDInfo());
                pPlot->setWorkingCityOverride(pOtherCity);
                //plotsIter->assignedCity = IDInfo();
                {
                    CityDataPtr pCityData(new CityData(pCity));
                    CitySimulation simulation(pCity, pCityData);
                    SimulationOutput simOutput = simulation.simulateAsIs(10);
                    baseline = *simOutput.cumulativeOutput.rbegin();
                    //weights = mergeMax(simulation.getCityOptimiser()->getMaxOutputWeights(), weights);
#ifdef ALTAI_DEBUG
                    {  // debug
                        CityLog::getLog(pCityData->getCity())->getStream() << "\nPlots without shared plot: " << (*plotsIter)->coords << "\n";
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
            TotalOutputWeights outputWeights = player_.getCity(pCity).getMaxOutputWeights();
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
            const PlotInfo::PlotInfoNode& plotInfo = getPlotInfoNode(pPlot);

            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nCoords = " << (*plotsIter)->coords
                << " (yield = " << getYield(plotInfo, player_.getPlayerID(), pPlot->getImprovementType(), pPlot->getFeatureType(), pPlot->getRouteType()) << ") ";
            for (size_t k = 0, count = deltas.size(); k < count; ++k)
            {
                if (k > 0) os << ", ";
                const CvCity* pCity = getCity(deltas[k].first);
                //TotalOutputWeights outputWeights = player_.getCity(pCity).getMaxOutputWeights();
                TotalOutputValueFunctor valueF(deltas[k].second.second);

                os << " city: " << narrow(pCity->getName()) << " = " << deltas[k].second.first << " value = " << valueF(deltas[k].second.first) << " (weights = " << deltas[k].second.second << ")";
            }
            os << " best city = " << (pBestCity ? narrow(pBestCity->getName()) : "NONE") << " current plot setting = "
                << (pPlot->getWorkingCity() ? narrow(pPlot->getWorkingCity()->getName()) : " none ") << "\n";
        }
#endif
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

    // city improvements functions:
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

    // add/remove cities...
    void MapAnalysis::addCity(const CvCity* pCity)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
        const CvPlot* pPlot = pCity->plot();
        const XYCoords coords(pPlot->getX(), pPlot->getY());

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
        // update plot info data
        const int oldKey = keyInfoMap_[coords];
        std::pair<int, MapAnalysis::PlotInfoMap::iterator> keyAndIter = updatePlotInfo_(pPlot);
        
#ifdef ALTAI_DEBUG
        os << "\n(MapAnalysis::addCity) Updating plot key for new city at: " << coords << " from: " << oldKey << " to: " << keyAndIter.first
           << "\n" << keyAndIter.second->second;
#endif

        // remove plots from plotValues_
        // the city plot...
#ifdef ALTAI_DEBUG
        os << "\nRemoving plot: " << XYCoords(pPlot->getX(), pPlot->getY()) << " from plot values map for sub area: " << pPlot->getSubArea();
#endif
        plotValues_.plotValueMap[pPlot->getSubArea()].erase(XYCoords(pPlot->getX(), pPlot->getY()));
        // neightbouring plots which are now invalid (must be same area)
        NeighbourPlotIter plotIter(pPlot, 2, 2);
        while (IterPlot pLoopPlot = plotIter())
        {
            if (!pLoopPlot.valid())
            {
                continue;
            }
#ifdef ALTAI_DEBUG
            os << "\nChecking plot: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY()) << " for removal from plotValues map";
#endif
            if (pLoopPlot->getArea() == pPlot->getArea())
            {
                PlotValues::PlotValueMap::iterator iter = plotValues_.plotValueMap.find(pLoopPlot->getSubArea());
                if (iter != plotValues_.plotValueMap.end())
                {
                    PlotValues::SubAreaPlotValueMap::iterator plotValueIter = iter->second.find(XYCoords(pLoopPlot->getX(), pLoopPlot->getY()));
                    if (plotValueIter != iter->second.end())
                    {
#ifdef ALTAI_DEBUG
                        os << "\nRemoving plot: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY()) << " from plot values map for sub area: " << pLoopPlot->getSubArea();
#endif
                        iter->second.erase(plotValueIter);
                    }

                    if (iter->second.empty())
                    {
                        plotValues_.plotValueMap.erase(iter);
                    }
                }
            }
        }
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
   
    // debug functions
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