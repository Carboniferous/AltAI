#include "AltAI.h"

#include <fstream>

#include "./map_analysis.h"
#include "./player_analysis.h"
#include "./gamedata_analysis.h"
#include "./helper_fns.h"
#include "./plot_info_visitors.h"
#include "./plot_info_visitors_streams.h"
#include "./building_info_visitors.h"
#include "./game.h"
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

                    if (pLoopPlot && pLoopPlot->isCity() && pLoopPlot->getPlotCity()->wasRevealed(player.getTeamID(), false) && pLoopPlot->area() == pPlot->area())
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
    }

    void MapAnalysis::read(FDataStreamBase* pStream)
    {
    }

    // getters
    const PlotInfo::PlotInfoNode& MapAnalysis::getPlotInfoNode(const CvPlot* pPlot)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
        XYCoords coords(pPlot->getCoords());
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
                    os << "\n(getPlotInfoNode): Inconsistent plot keys for coords: " << coords << " keys: stored = " << keyIter->second << ", calculated = " << key
                       << "\n" << iter->second << "\n" << plotInfo.getInfo();
                }
                else if (!(iter->second == plotInfo.getInfo()))
                {
                    os << "\n(getPlotInfoNode): Inconsistent plot info for coords: " << coords << " keys: stored = " << keyIter->second << ", calculated = " << key
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
        return updatePlotInfo_(pPlot, false).second->second;
    }

    const MapAnalysis::PlotValues& MapAnalysis::getPlotValues()
    {
        // need to update dot map once we've processed the complete set of plot updates
        processUpdatedPlots_();

        return plotValues_;
    }

    bool MapAnalysis::plotValuesDirty() const
    {
        return !updatedPlots_.empty();
    }

    IDInfo MapAnalysis::getSharedPlot(IDInfo city, XYCoords coords) const
    {
        CitySharedPlotsMap::const_iterator citySharedPlotsIter = citySharedPlots_.find(city);
        if (citySharedPlotsIter != citySharedPlots_.end())
        {
            SharedPlots::const_iterator sharedPlotsIter = sharedPlots_.find(coords);
            if (sharedPlotsIter != sharedPlots_.end())
            {
                return sharedPlotsIter->second.assignedCity;
            }
        }

        return city;
    }

    IDInfo MapAnalysis::getImprovementOwningCity(IDInfo city, XYCoords coords) const
    {
        CitySharedPlotsMap::const_iterator citySharedPlotsIter = citySharedPlots_.find(city);
        if (citySharedPlotsIter != citySharedPlots_.end())
        {
            SharedPlots::const_iterator sharedPlotsIter = sharedPlots_.find(coords);
            if (sharedPlotsIter != sharedPlots_.end())
            {
                return sharedPlotsIter->second.assignedImprovementCity;
            }
        }

        return IDInfo();
    }

    IDInfo MapAnalysis::setImprovementOwningCity(IDInfo city, XYCoords coords)
    {
        CitySharedPlotsMap::iterator citySharedPlotsIter = citySharedPlots_.find(city);
        if (citySharedPlotsIter != citySharedPlots_.end())
        {
            SharedPlots::iterator sharedPlotsIter = sharedPlots_.find(coords);
            if (sharedPlotsIter != sharedPlots_.end())
            {
                IDInfo oldImpCity = sharedPlotsIter->second.assignedImprovementCity;
                sharedPlotsIter->second.assignedImprovementCity = city;
                return oldImpCity;
            }
        }
        return IDInfo();
    }

    std::set<XYCoords> MapAnalysis::getCitySharedPlots(IDInfo city) const
    {
        CitySharedPlotsMap::const_iterator citySharedPlotsIter = citySharedPlots_.find(city);
        if (citySharedPlotsIter != citySharedPlots_.end())
        {
            return citySharedPlotsIter->second.sharedPlots;
        }

        return std::set<XYCoords>();
    }

    const CvCity* MapAnalysis::getWorkingCity(IDInfo city, XYCoords coords) const
    {
        if (sharedPlots_.empty())  // not init() yet - needed as cities init() first
        {
            return gGlobals.getMap().plot(coords.iX, coords.iY)->getWorkingCity();
        }
        
        return ::getCity(getSharedPlot(city, coords));
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
                    if (boost::get<1>(bi->second[i]) == player_.getPlayerID())
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

    std::vector<CvPlot*> MapAnalysis::getResourcePlots(int subArea, const std::vector<BonusTypes>& bonusTypes, PlayerTypes playerType) const
    {
        std::vector<CvPlot*> resourcePlots;
        const CvMap& theMap = gGlobals.getMap();

        ResourcesMapConstIter ci = resourcesMap_.find(subArea);
        if (ci != resourcesMap_.end())
        {
            if (!bonusTypes.empty())
            {
                for (size_t i = 0, count = bonusTypes.size(); i < count; ++i)
                {
                    std::map<BonusTypes, ResourceData::ResourcePlotData>::const_iterator bi(ci->second.subAreaResourcesMap.find(bonusTypes[i]));
                    if (bi != ci->second.subAreaResourcesMap.end())
                    {
                        for (size_t i = 0, count = bi->second.size(); i < count; ++i)
                        {
                            if (boost::get<1>(bi->second[i]) == playerType)
                            {
                                XYCoords coords = boost::get<0>(bi->second[i]);
                                resourcePlots.push_back(theMap.plot(coords.iX, coords.iY));
                            }              
                        }
                    }
                }
            }
            else  // report all bonuses
            {
                for (std::map<BonusTypes, ResourceData::ResourcePlotData>::const_iterator ri(ci->second.subAreaResourcesMap.begin()),
                    riEnd(ci->second.subAreaResourcesMap.end()); ri != riEnd; ++ri)
                {
                    for (size_t i = 0, count = ri->second.size(); i < count; ++i)
                    {
                        if (boost::get<1>(ri->second[i]) == playerType)
                        {
                            XYCoords coords = boost::get<0>(ri->second[i]);
                            resourcePlots.push_back(theMap.plot(coords.iX, coords.iY));
                        }              
                    }
                }
            }
        }

        // todo - check for duplicates
        return resourcePlots;
    }

    const CvPlot* MapAnalysis::getClosestCity(const CvPlot* pPlot, int subArea, bool includeActsAsCity) const
    {
        const CvMap& theMap = gGlobals.getMap();
        const bool isWaterSubArea = theMap.getSubArea(subArea)->isWater();
        const TeamTypes teamType = player_.getTeamID();

        int bestStepDistance = MAX_INT;
        const CvPlot* pClosestCityPlot = (const CvPlot*)0;

        TeamCityIter cityIter(teamType);
        while (CvCity* pCity = cityIter())
        {
            bool mayBeReachable = false;
            if (isWaterSubArea)
            {
                // try and account for coastal cities
                // this won't always work for units in impassable areas, as the unit may be in an impassable sub area (e.g. sub under ice)
                // which doesn't border the city (a city could border an impassable sub area directly though)
                std::vector<int> borderingSubAreas = getBorderingSubAreas(teamType, pCity->plot());
                if (std::find(borderingSubAreas.begin(), borderingSubAreas.end(), subArea) != borderingSubAreas.end())
                {
                    mayBeReachable = true;
                }
            }
            else
            {
                mayBeReachable = pCity->plot()->getSubArea() == subArea;
            }

            if (mayBeReachable)
            {
                int thisStepDistance = stepDistance(pPlot->getX(), pPlot->getY(), pCity->getX(), pCity->getY());
                if (thisStepDistance < bestStepDistance)
                {
                    bestStepDistance = thisStepDistance;
                    pClosestCityPlot = pCity->plot();
                }
            }
        }

        if (includeActsAsCity)
        {
            std::set<int> subAreasToSearch;
            if (isWaterSubArea)
            {   
                std::vector<int> borderingAreas = getAreasBorderingArea(pPlot->getArea());
                for (size_t i = 0; i < borderingAreas.size(); ++i)
                {
                    std::vector<int> subAreas = getSubAreasInArea(borderingAreas[i]);
                    for (size_t j = 0; j < subAreas.size(); ++j)
                    {
                        boost::shared_ptr<SubArea> pBorderSubArea = theMap.getSubArea(subAreas[j]);
                        if (!pBorderSubArea->isImpassable() && !pBorderSubArea->isWater())
                        {
                            subAreasToSearch.insert(subAreas[j]);
                        }
                    }                        
                }
            }
            else
            {
                subAreasToSearch.insert(subArea);
            }

            for (std::set<int>::const_iterator searchIter(subAreasToSearch.begin()), searchEndIter(subAreasToSearch.end()); searchIter != searchEndIter; ++searchIter)
            {
                std::map<int /* sub area id */, std::set<XYCoords> >::const_iterator fortIter = impAsCityMap_.find(*searchIter);
                if (fortIter != impAsCityMap_.end())
                {
                    for (std::set<XYCoords>::const_iterator ci(fortIter->second.begin()), ciEnd(fortIter->second.end()); ci != ciEnd; ++ci)
                    {
                        const CvPlot* pLoopPlot = theMap.plot(ci->iX, ci->iY);
                        TeamTypes plotTeam = pLoopPlot->getRevealedTeam(teamType, false);

                        if (plotTeam != NO_TEAM && plotTeam != teamType)
                        {
                            continue;  // only consider friendly forts
                        }

                        bool mayBeReachable = true;
                        if (isWaterSubArea)
                        {
                            // try and account for coastal cities
                            // this won't always work for units in impassable areas, as the unit may be in an impassable sub area (e.g. sub under ice)
                            // which doesn't border the city (a city could border an impassable sub area directly though)
                            std::vector<int> borderingSubAreas = getBorderingSubAreas(teamType, pLoopPlot);
                            if (std::find(borderingSubAreas.begin(), borderingSubAreas.end(), subArea) == borderingSubAreas.end())
                            {
                                mayBeReachable = false;
                            }
                        }

                        if (mayBeReachable)
                        {
                            int thisStepDistance = stepDistance(pPlot->getX(), pPlot->getY(), ci->iX, ci->iY);
                            if (thisStepDistance < bestStepDistance)
                            {
                                bestStepDistance = thisStepDistance;
                                pClosestCityPlot = pLoopPlot;
                            }
                        }
                    }
                }
            }
        }

        return pClosestCityPlot;
    }

    const std::map<int /* sub area id */, std::set<XYCoords> >& MapAnalysis::getUnrevealedBorderMap() const
    {
        return unrevealedBorderMap_;
    }

    std::vector<int /* area id */> MapAnalysis::getAreasBorderingArea(int areaId) const
    {
        std::vector<int /* area id */> knownNeighbourAreas;

        AreaMapConstIter areaIter(revealedAreaDataMap_.find(areaId));
        if (areaIter != revealedAreaDataMap_.end())
        {
            std::copy(areaIter->second.borderingAreas.begin(), areaIter->second.borderingAreas.end(), std::back_inserter(knownNeighbourAreas));
        }

        return knownNeighbourAreas;
    }

    std::vector<int /* sub area id */ > MapAnalysis::getSubAreasInArea(int areaId) const
    {
        std::vector<int /* area id */> knownSubAreas;
        std::map<int /* area id */, std::set<int /* sub area id */> >::const_iterator iter(areaSubAreaMap_.find(areaId));
        if (iter != areaSubAreaMap_.end())
        {
             std::copy(iter->second.begin(), iter->second.end(), std::back_inserter(knownSubAreas));
        }
        return knownSubAreas;
    }

    std::map<int /* sub area id */, std::vector<IDInfo> > MapAnalysis::getSubAreaCityMap() const
    {
        std::map<int /* sub area id */, std::vector<IDInfo> > citySubAreaMap;
        CityIter iter(*player_.getCvPlayer());
        while (CvCity* pCity = iter())
        {
            citySubAreaMap[pCity->plot()->getSubArea()].push_back(pCity->getIDInfo());
        }
        return citySubAreaMap;
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
                    cities.push_back(pPlot->getCoords());
                    addCity(pPlot->getPlotCity());
                }
                else if (pPlot->isCity(true, teamType))
                {
                    impAsCityMap_[pPlot->getSubArea()].insert(pPlot->getCoords());
                }
                updatePlotRevealed(pPlot, true);
                updatePlotInfo(pPlot, true, __FUNCTION__);
            }
        }

#ifdef ALTAI_DEBUG
        os << "Player: " << player_.getPlayerID() << " knows: " << revealedAreaDataMap_.size() << " areas\n";
        for (AreaMapConstIter ci(revealedAreaDataMap_.begin()), ciEnd(revealedAreaDataMap_.end()); ci != ciEnd; ++ci)
        {
            os << "Area: " << ci->first << " knows: " << ci->second.knownTileCount << " plots (out of " << gGlobals.getMap().getArea(ci->first)->getNumTiles() << ")\n";
        }
#endif

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

        updateKeysValueYields_();

        // need to update dot map once we've processed the complete set of plot updates
        processUpdatedPlots_();

        previousPlotValues_ = plotValues_;
        plotValues_.plotValueMap.clear();
        plotValues_.keysValueMap.clear();

        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
        {
            const CvPlot* pPlot = theMap.plotByIndex(i);

            if (pPlot->isRevealed(teamType, false))
            {
                PlayerTypes owner = pPlot->getRevealedOwner(teamType, false);

                std::pair<int, MapAnalysis::PlotInfoMap::iterator> keyAndIter = updatePlotInfo_(pPlot, false, true);

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

    void MapAnalysis::recalcPlotInfo()
    {
        PlayerTypes playerType = player_.getPlayerID();
        TeamTypes teamType = player_.getTeamID();
        const CvMap& theMap = gGlobals.getMap();

        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
        {
            const CvPlot* pPlot = theMap.plotByIndex(i);
            if (pPlot->isRevealed(teamType, false))
            {
                updatePlotRevealed(pPlot, false);
            }
        }
    }

    void MapAnalysis::update()
    {
        if (!init_)
        {
            init();
        }
    }

    void MapAnalysis::updatePlotInfo(const CvPlot* pPlot, bool isNew, const std::string& caller)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nupdatePlotInfo(): plot: " << pPlot->getCoords() << " is new = " << isNew << " caller = " << caller
            << " owner = " << pPlot->getRevealedOwner(player_.getTeamID(), false)
            << ", turn = " << gGlobals.getGame().getGameTurn();
#endif
        std::pair<int, MapAnalysis::PlotInfoMap::iterator> keyAndIter = updatePlotInfo_(pPlot, isNew);
        
        updatedPlots_.insert(pPlot);

        if (pPlot->isCity())
        {
            std::map<IDInfo, XYCoords>::iterator seenCitiesIter = seenCities_.find(pPlot->getPlotCity()->getIDInfo());
            if (seenCitiesIter != seenCities_.end())
            {
                if (seenCitiesIter->second != pPlot->getCoords())
                {
                    ErrorLog::getLog(*player_.getCvPlayer())->getStream() << "\nInconsistent city coords for city: " << seenCitiesIter->first
                       << " stored coords = " << seenCitiesIter->second << ", new coords = " << pPlot->getCoords();
                }
            }
            else
            {                
                addCity(pPlot->getPlotCity());
            }
        }
        else if (pPlot->isCity(true, player_.getTeamID()))
        {
            impAsCityMap_[pPlot->getSubArea()].insert(pPlot->getCoords());
        }
    }

    void MapAnalysis::updatePlotRevealed(const CvPlot* pPlot, bool isNew)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nupdatePlotRevealed(): updating plot: " << pPlot->getCoords() << " owner = " << pPlot->getRevealedOwner(player_.getTeamID(), false)
            << ", turn = " << gGlobals.getGame().getGameTurn();
        PlotInfo plotInfo(pPlot, player_.getPlayerID());
        os << "\n" << plotInfo.getInfo();
#endif
        const XYCoords coords(pPlot->getCoords());

        if (isNew)
        {
            const int areaID = pPlot->getArea();
            const int subAreaID = pPlot->getSubArea();

            int knownAreaCount = updateAreaCounts_(areaID), totalAreaCount = gGlobals.getMap().getArea(areaID)->getNumTiles();
            int knownSubAreaCount = updateSubAreaCounts_(subAreaID), totalSubAreaCount = gGlobals.getMap().getSubArea(subAreaID)->getNumTiles();

#ifdef ALTAI_DEBUG
            os << "\nNow know " << knownAreaCount << " out of " << totalAreaCount << " for area: " << areaID;
            os << "\nNow know " << knownSubAreaCount << " out of " << totalSubAreaCount << " for subarea: " << subAreaID;
#endif
            areaSubAreaMap_[areaID].insert(subAreaID);
            updateAreaBorders_(pPlot, areaID, subAreaID);        

            if (totalAreaCount == knownAreaCount)
            {
                updateAreaComplete_(areaID);
            }

            if (totalSubAreaCount == knownSubAreaCount)
            {
                updateSubAreaComplete_(subAreaID);
            }
        }

        updateResourceData_(pPlot);
    }

    void MapAnalysis::updatePlotFeature(const CvPlot* pPlot, FeatureTypes oldFeatureType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nUpdating feature type for plot : " << pPlot->getCoords()
           << " old feature = " << (oldFeatureType == NO_FEATURE ? "NO_FEATURE" : gGlobals.getFeatureInfo(oldFeatureType).getType())
           << " new feature = " << (pPlot->getFeatureType() == NO_FEATURE ? "NO_FEATURE" : gGlobals.getFeatureInfo(pPlot->getFeatureType()).getType());
#endif
        updatePlotInfo_(pPlot, false);
    }

    void MapAnalysis::updatePlotImprovement(const CvPlot* pPlot, ImprovementTypes oldImprovementType)
    {
        ImprovementTypes newImprovementType = pPlot->getImprovementType();
        XYCoords plotCoords(pPlot->getCoords());

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nUpdating improvement type for plot : " << plotCoords
           << " old improvement = " << (oldImprovementType == NO_IMPROVEMENT ? "NO_IMPROVEMENT" : gGlobals.getImprovementInfo(oldImprovementType).getType())
           << " new improvement = " << (newImprovementType == NO_IMPROVEMENT ? "NO_IMPROVEMENT" : gGlobals.getImprovementInfo(newImprovementType).getType());
#endif
        if (oldImprovementType != NO_IMPROVEMENT && newImprovementType != oldImprovementType)
        {
            if (gGlobals.getImprovementInfo(oldImprovementType).isActsAsCity())
            {
                // might not exist if we never controlled the plot and it was never netural either
                impAsCityMap_[pPlot->getSubArea()].erase(pPlot->getCoords());
            }
        }

        if (newImprovementType == NO_IMPROVEMENT)
        {
            // todo handle spies destroying improvments - how is that notifed?
            if (pPlot->isVisibleEnemyUnit(player_.getPlayerID()))
            {
#ifdef ALTAI_DEBUG
                os << " (improvement was pillaged) ";
#endif
            }

            BonusTypes bounsType = pPlot->getBonusType(player_.getTeamID());
            if (bounsType != NO_BONUS)
            {
                const int subAreaId = pPlot->getSubArea();
                ResourcesMapIter resourceMapIter = resourcesMap_.find(subAreaId);
                if (resourceMapIter != resourcesMap_.end())
                {
                    std::map<BonusTypes, ResourceData::ResourcePlotData>::iterator iter = resourceMapIter->second.subAreaResourcesMap.find(bounsType);
                    if (iter != resourceMapIter->second.subAreaResourcesMap.end())
                    {
                        for (size_t bonusIndex = 0, bonusCount = iter->second.size(); bonusIndex < bonusCount; ++bonusIndex)
                        {
                            XYCoords bonusCoords = boost::get<0>(iter->second[bonusIndex]);
                            if (bonusCoords == plotCoords)
                            {
                                boost::get<2>(iter->second[bonusIndex]) = newImprovementType;
                                break;
                            }
                        }
                    }
                }
            }
        }
        //else
        //{
            // conditional riverside improvements are added if possible - although actual imp may depend on changing surrounding ones which compete
            // otherwise we would have to change the plot key and record the configuration for evey possible riverside imp (although there's only watermills in the standard game)            
            /*const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(newImprovementType);
            if (improvementInfo.isRequiresRiverSide())
            {
                CardinalPlotIter plotIter(pPlot);
                while (IterPlot iterPlot = plotIter())
                {
                    if (iterPlot.valid())
                    {
                        DirectionTypes directionType = directionXY(pPlot, iterPlot);
                        if (pPlot->isRiverCrossing(directionType))
                        {
                            updatePlotInfo_(iterPlot);
                        }
                    }
                }
            }*/
        //}
    }

    void MapAnalysis::updatePlotBonus(const CvPlot* pPlot, BonusTypes bonusType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nUpdating bonus type for plot : " << pPlot->getCoords()
           << " bonus = " << (bonusType == NO_BONUS ? "NO_BONUS" : gGlobals.getBonusInfo(bonusType).getType());
#endif
        updatePlotInfo_(pPlot, false);
    }

    void MapAnalysis::updatePlotCulture(const CvPlot* pPlot, PlayerTypes previousRevealedOwner, PlayerTypes newRevealedOwner)
    {
        XYCoords coords(pPlot->getCoords());

        KeyInfoMap::const_iterator keyIter = keyInfoMap_.find(coords);

        if (keyIter == keyInfoMap_.end())
        {
            return;
        }

        const int key = keyIter->second;
        const int subAreaID = pPlot->getSubArea();

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nupdatePlotCulture: " << coords << " with old owner: " << previousRevealedOwner << ", new owner: " << newRevealedOwner;
#endif      
        // this is a plot we no longer control (unless it was from a city which was razed, and is now uncontrolled)
        if ((previousRevealedOwner == player_.getPlayerID() || previousRevealedOwner == NO_PLAYER) && newRevealedOwner != player_.getPlayerID() && newRevealedOwner != NO_PLAYER)
        {
#ifdef ALTAI_DEBUG
            os << "\n " << "Removing plot: " << pPlot->getCoords();
#endif
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
        else if (newRevealedOwner == player_.getPlayerID())
        {
#ifdef ALTAI_DEBUG
            os << "\n Adding plot: " << pPlot->getCoords();
#endif
            const PlotInfo::PlotInfoNode& plotInfo = getPlotInfoNode(pPlot);
            std::vector<IDInfo> possibleWorkingCities;
            // if we are here, means we own the plot (unowned plots should be added through updatePlotRevealed)
            if (hasPossibleYield(plotInfo, player_.getPlayerID()))
            {
                updatedPlots_.insert(pPlot);

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
                                possibleWorkingCities.push_back(pCity->getIDInfo());
                            }
                        }
                    }
                }

                // new shared plot
                if (possibleWorkingCities.size() > 1)
                {
                    for (size_t i = 0, count = possibleWorkingCities.size(); i < count; ++i)
                    {
                        CitySharedPlotsMap::iterator citySharedPlotsIter = getCitySharedPlots_(possibleWorkingCities[i]);
                        citySharedPlotsIter->second.sharedPlots.insert(pPlot->getCoords());
#ifdef ALTAI_DEBUG
                        os << "\nAdding shared plot: " << pPlot->getCoords() << " for city: " << safeGetCityName(possibleWorkingCities[i]);
#endif
                    }

                    MapAnalysis::SharedPlots::iterator sharedPlotsIter = getSharedPlot_(pPlot->getCoords());
                    sharedPlotsIter->second.possibleCities.insert(possibleWorkingCities.begin(), possibleWorkingCities.end());
                    // not setting a working city, as I think at this point, the plot will not have been added to the cities' working plots
#ifdef ALTAI_DEBUG
                    os << "\nAdding shared plot: " << pPlot->getCoords();
#endif
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

    void MapAnalysis::processUpdatedPlots_()
    {
        // need to update dot map once we've processed the complete set of plot updates
        for (std::set<const CvPlot*>::iterator iter(updatedPlots_.begin()); iter != updatedPlots_.end();)
        {
            PlayerTypes owner = (*iter)->getRevealedOwner(player_.getTeamID(), false);
            // consider plots we or nobody owns
            if (owner == NO_PLAYER || owner == player_.getPlayerID())
            {
                addDotMapPlot_(*iter, getPlotInfoNode(*iter));
            }
            else
            {
                removePlotValuePlot_(*iter);
            }
            updatedPlots_.erase(iter++);
        }
    }

    void MapAnalysis::removePlotValuePlot_(const CvPlot* pPlot)
    {
        const XYCoords coords(pPlot->getCoords());
        KeyInfoMap::const_iterator keyIter = keyInfoMap_.find(coords);
        if (keyIter == keyInfoMap_.end())
        {
            return;
        }
        const int key = keyIter->second;

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nRemoving plot: " << coords << ", key = " << key << " from plot values map.";
#endif        

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
                        PlotValues::SubAreaPlotValueMap::iterator plotIter = subAreaIter->second.find(pLoopPlot->getCoords());
                        if (plotIter != subAreaIter->second.end())
                        {
                            std::map<int /* plot key */, std::set<XYCoords> >::iterator plotKeyIter = plotIter->second.find(key);
                            if (plotKeyIter != plotIter->second.end())
                            {
                                std::set<XYCoords>::iterator coordsIter = plotKeyIter->second.find(coords);
                                if (coordsIter != plotKeyIter->second.end())
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nRemoving entry for plot key: " << key << " for potential city plot: " << pLoopPlot->getCoords();
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
                os << "\nRemoving entries for potential city plot: " << pPlot->getCoords();
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

//#ifdef ALTAI_DEBUG
//        debugPlotValueMap(plotValues_, os);
//#endif
    }

    // plot update helper functions
    std::pair<int, MapAnalysis::PlotInfoMap::iterator> MapAnalysis::updatePlotInfo_(const CvPlot* pPlot, bool isNew, bool forceKeyUpdate)
    {
        const XYCoords coords(pPlot->getCoords());
        PlotInfo plotInfo(pPlot, player_.getPlayerID());
        int oldKey = -1;
        const int newKey = plotInfo.getKey();
        MapAnalysis::PlotInfoMap::iterator plotInfoIter;
        PlayerTypes revealedOwner = pPlot->getRevealedOwner(player_.getTeamID(), false);

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\n" << plotInfo.getInfo() << "\n";
#endif        

        if (isNew)
        {
            KeyInfoMap::iterator keyIter = keyInfoMap_.find(coords);
            if (keyIter != keyInfoMap_.end())
            {
                oldKey = keyIter->second;
#ifdef ALTAI_DEBUG
                os << "\n(updatePlotInfo_): key already exists for coords: " << coords << " - existing key = " << oldKey << ", new key = " << newKey;
#endif
                if (oldKey != newKey)
                {
                    keyIter->second = newKey;
                    keyCoordsMap_[oldKey].erase(coords);
                    keyCoordsMap_[newKey].insert(coords);
                    plotInfoMap_.erase(oldKey);
                    plotInfoIter = plotInfoMap_.insert(std::make_pair(newKey, plotInfo.getInfo())).first;
                }
                else
                {
                    plotInfoIter = plotInfoMap_.find(newKey);
                }
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\n(updatePlotInfo_): adding key: " << newKey << " for coords: " << coords;
#endif
                keyInfoMap_[coords] = newKey;
                keyCoordsMap_[newKey].insert(coords);
                plotInfoIter = plotInfoMap_.insert(std::make_pair(newKey, plotInfo.getInfo())).first;
            }
        }
        else  // updating existing plot
        {
            KeyInfoMap::iterator keyIter = keyInfoMap_.find(coords);
            if (keyIter == keyInfoMap_.end())  // plot info missing altogether
            {
                keyIter = keyInfoMap_.insert(std::make_pair(coords, newKey)).first;
                keyCoordsMap_[newKey].insert(coords);
                plotInfoIter = plotInfoMap_.insert(std::make_pair(newKey, plotInfo.getInfo())).first;

#ifdef ALTAI_DEBUG
                os << "\n(updatePlotInfo_): Missing plot info?: key = " << newKey << ", coords = " << pPlot->getCoords()
                   << "\n" << plotInfo.getInfo();
#endif
            }
            else  // found existing key for these coords
            {
                oldKey = keyIter->second;
                if (oldKey != newKey)
                {
                    KeyCoordsMap::iterator keyCoordsIter = keyCoordsMap_.find(oldKey);
                    if (keyCoordsIter != keyCoordsMap_.end())
                    {
                        keyCoordsIter->second.erase(coords);
                        if (keyCoordsIter->second.empty())
                        {
                            keyCoordsMap_.erase(keyCoordsIter);
                            plotInfoMap_.erase(oldKey);  // no entries for this key any more
                        }
                    }
                    
                    keyIter->second = newKey;
                    keyCoordsMap_[newKey].insert(coords);
                    plotInfoIter = plotInfoMap_.insert(std::make_pair(newKey, plotInfo.getInfo())).first;
#ifdef ALTAI_DEBUG
                    os << "\n(updatePlotInfo_): Updating plot info: key = " << newKey << ", old key = " << oldKey << ", coords = " << pPlot->getCoords();
#endif
                }
                else
                {
                    plotInfoIter = plotInfoMap_.find(newKey);
                }
            }
        }

        if (!isNew && oldKey != newKey)
        {
            if (hasPossibleYield(plotInfo.getInfo(), player_.getPlayerID()) && (revealedOwner == NO_PLAYER || revealedOwner == player_.getPlayerID()))
            {
                updatePlotValueKey_(pPlot, oldKey, newKey);
            }
        }

        if ((forceKeyUpdate || oldKey != newKey) && (revealedOwner == NO_PLAYER || revealedOwner == player_.getPlayerID()))
        {
            updateKeysValueMap_(newKey, plotInfo.getInfo(), oldKey);
        }

        return std::make_pair(newKey, plotInfoIter);
    }

    void MapAnalysis::addDotMapPlot_(const CvPlot* pPlot, const PlotInfo::PlotInfoNode& plotInfo)
    {
        const XYCoords coords(pPlot->getCoords());
        const int key = keyInfoMap_[coords];
        const TeamTypes teamType = player_.getTeamID();

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();       
#endif
        PlayerTypes plotOwner = pPlot->getRevealedOwner(player_.getTeamID(), false);
        if (plotOwner != NO_PLAYER && plotOwner != player_.getPlayerID())
        {
#ifdef ALTAI_DEBUG
            os << "\nSkipping plot for dot map: " << coords << ", owner = " << plotOwner;
#endif
            return;
        }

        if (hasPossibleYield(plotInfo, player_.getPlayerID()))
        {
            for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            {
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    // consider plots we have seen and can theoretically found a city at
                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(teamType, false))
                    {
                        if (couldFoundAtPlot(pLoopPlot, player_))
                        {
#ifdef ALTAI_DEBUG
                            os << "\nAdding plot: " << coords << " with key = " << key << " to plot value map for sub area: " << pLoopPlot->getSubArea()
                               << " for potential city: " << pLoopPlot->getCoords();
#endif
                            plotValues_.plotValueMap[pLoopPlot->getSubArea()][pLoopPlot->getCoords()][key].insert(coords);
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
                            const int thisKey = keyInfoMap_[pLoopPlot->getCoords()];

                            if (hasPossibleYield(thisNode, player_.getPlayerID()))
                            {
#ifdef ALTAI_DEBUG
                                os << "\nAdding plot: " << pLoopPlot->getCoords() << " with key = " << thisKey << " to plot value map for sub area: " << pPlot->getSubArea()
                                   << " for potential city: " << pPlot->getCoords();
#endif
                                plotValues_.plotValueMap[pPlot->getSubArea()][pPlot->getCoords()][thisKey].insert(pLoopPlot->getCoords());
                            }
                        }
                    }
                }
            }
        }
    }

    void MapAnalysis::updatePlotValueKey_(const CvPlot* pPlot, int oldKey, int newKey)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nUpdating plot keys for plot: " << pPlot->getCoords() << " from: " << oldKey << " to: " << newKey;
#endif
        const TeamTypes teamType = player_.getTeamID();
        const PlayerTypes playerType = player_.getPlayerID();
        const XYCoords coords(pPlot->getCoords());

        // update plot keys
        for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
        {
            CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
            while (IterPlot pLoopPlot = plotIter())
            {
                if (pLoopPlot.valid() && pLoopPlot->isRevealed(teamType, false))
                {
                    const PlayerTypes plotOwner = pLoopPlot->getRevealedOwner(teamType, false);

                    if (plotOwner == NO_PLAYER || plotOwner == player_.getPlayerID())  // could consider vassals' plots here if being v. aggressive
                    {
                        PlotValues::PlotValueMap::iterator subAreaIter = plotValues_.plotValueMap.find(pLoopPlot->getSubArea());
                        if (subAreaIter != plotValues_.plotValueMap.end())
                        {
                            PlotValues::SubAreaPlotValueMap::iterator plotIter = subAreaIter->second.find(pLoopPlot->getCoords());

                            if (plotIter != subAreaIter->second.end())
                            {
                                PlotValues::PlotKeyCoordsMap::iterator plotKeyIter = plotIter->second.find(oldKey);
                                bool erased = false;
                                if (plotKeyIter != plotIter->second.end())
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nErasing entry for plot key:" << oldKey << " for potential city plot: " << pLoopPlot->getCoords();
#endif
                                    // erase entry for plot with old plot key
                                    plotKeyIter->second.erase(coords);

                                    if (plotKeyIter->second.empty())
                                    {
                                        plotIter->second.erase(plotKeyIter);
                                    }
                                    erased = true;
                                }
                                if (erased)
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nAdding entry for plot key: " << newKey << " for potential city plot: " << pLoopPlot->getCoords();
#endif
                                    // insert new key - only add if had existing entry (new entries are added separately and should automatically get the correct key)
                                    subAreaIter->second[pLoopPlot->getCoords()][newKey].insert(coords);
                                }
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
        const XYCoords coords(pPlot->getCoords());
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
                resourceSubAreaIter->second.subAreaResourcesMap.insert(std::make_pair(bonusType, ResourceData::ResourcePlotData(1, boost::make_tuple(coords, owner, pPlot->getImprovementType()))));
            }
            else
            {
                bool found = false;
                for (size_t i = 0, count = resourceItemIter->second.size(); i < count; ++i)
                {
                    if (boost::get<0>(resourceItemIter->second[i]) == coords)
                    {
                        found = true;
                        boost::get<1>(resourceItemIter->second[i]) = owner;
                    }
                }
                if (!found)
                {
                    resourceItemIter->second.push_back(boost::make_tuple(coords, owner, pPlot->getImprovementType()));
                }
            }
        }
    }

    void MapAnalysis::updateKeysValueMap_(int key, const PlotInfo::PlotInfoNode& plotInfo, int oldKey)
    {
        if (oldKey != -1 && keyCoordsMap_[oldKey].empty())
        {
            plotValues_.keysValueMap.erase(oldKey);
        }

        plotValues_.keysValueMap[key] = getYields(plotInfo, player_.getPlayerID(),
            player_.getCvPlayer()->isBarbarian() ? BarbDotMapTechDepth : DotMapTechDepth);
    }

    void MapAnalysis::updateKeysValueYields_()
    {
        for (MapAnalysis::PlotValues::KeysValueMap::iterator keyIter(plotValues_.keysValueMap.begin()), keyEndIter(plotValues_.keysValueMap.end());
                keyIter != keyEndIter; ++keyIter)
        {
            PlotInfoMap::const_iterator plotInfoIter = plotInfoMap_.find(keyIter->first);
            if (plotInfoIter != plotInfoMap_.end())
            {
                keyIter->second = getYields(plotInfoIter->second, player_.getPlayerID(),
                    player_.getCvPlayer()->isBarbarian() ? BarbDotMapTechDepth : DotMapTechDepth);
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
        AreaMapIter areaIter(revealedAreaDataMap_.find(areaID)), subAreaIter(revealedSubAreaDataMap_.find(subAreaID));
        TeamTypes teamType = player_.getTeamID();
        NeighbourPlotIter iter(pPlot);
        bool hasUnrevealedNeighbours = false;
        std::vector<const CvPlot*> possiblePlotsToRemoveFromBorder;

        while (IterPlot pLoopPlot = iter())
        {
            if (pLoopPlot.valid())
            {
                if (pLoopPlot->isRevealed(teamType, false))
                {
                    int loopPlotAreaID = pLoopPlot->getArea();
                    if (loopPlotAreaID != areaID)
                    {
                        areaIter->second.borderingAreas.insert(loopPlotAreaID);
                    }

                    int loopPlotSubAreaID = pLoopPlot->getSubArea();
                    if (loopPlotSubAreaID != subAreaID)
                    {
                        subAreaIter->second.borderingAreas.insert(loopPlotSubAreaID);
                    }

                    // does this plot have any remaining unrevealed neighbours, if we're adding a new border plot?
                    if (unrevealedBorderMap_[pLoopPlot->getSubArea()].find(pLoopPlot->getCoords()) != unrevealedBorderMap_[pLoopPlot->getSubArea()].end())
                    {
                        possiblePlotsToRemoveFromBorder.push_back(pLoopPlot);
                    }
                }
                else
                {
                    hasUnrevealedNeighbours = true;
                }
            }
        }

        if (hasUnrevealedNeighbours)
        {
            unrevealedBorderMap_[pPlot->getSubArea()].insert(pPlot->getCoords());
        }
        
        // check this even if we didn't add a new border plot - could be filling in an interior region of border...
        for (size_t i = 0, count = possiblePlotsToRemoveFromBorder.size(); i < count; ++i)
        {
            hasUnrevealedNeighbours = false;
            NeighbourPlotIter neighbourIter(possiblePlotsToRemoveFromBorder[i]);
            while (IterPlot pLoopPlot = neighbourIter())
            {
                if (pLoopPlot.valid() && !pLoopPlot->isRevealed(teamType, false))
                {
                    hasUnrevealedNeighbours = true;
                    break;
                }
            }

            if (!hasUnrevealedNeighbours)
            {
                unrevealedBorderMap_[possiblePlotsToRemoveFromBorder[i]->getSubArea()].erase(possiblePlotsToRemoveFromBorder[i]->getCoords());
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
                os << "\nPlayer: " << player_.getPlayerID() << " knows all of area: " << areaID << " (" << iter->second.knownTileCount << ") tiles\n";
            }
#endif
            // update from Map's area graph
        }
    }

    bool MapAnalysis::isAreaComplete(int areaID) const
    {
        AreaMapConstIter iter(revealedAreaDataMap_.find(areaID));
        if (iter != revealedAreaDataMap_.end())
        {
            return iter->second.allKnown;
        }
        else
        {
            return false;
        }
    }

    bool MapAnalysis::isSubAreaComplete(int subAreaID) const
    {
        AreaMapConstIter iter(revealedSubAreaDataMap_.find(subAreaID));
        if (iter != revealedSubAreaDataMap_.end())
        {
            return iter->second.allKnown;
        }
        else
        {
            return false;
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
                os << "\nPlayer: " << player_.getPlayerID() << " knows all of sub area: " << subAreaID << " (" << iter->second.knownTileCount << ") tiles\n";
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

    // reanalyse plots shared by these cities
    void MapAnalysis::analyseSharedPlots(const std::set<IDInfo>& cities)
    {
        std::set<XYCoords> sharedCoords;

        for (std::set<IDInfo>::const_iterator ci(cities.begin()), ciEnd(cities.end()); ci != ciEnd; ++ci)
        {
            CitySharedPlotsMap::const_iterator citySharedPlotsIter = citySharedPlots_.find(*ci);
            if (citySharedPlotsIter != citySharedPlots_.end())
            {
                sharedCoords.insert(citySharedPlotsIter->second.sharedPlots.begin(), citySharedPlotsIter->second.sharedPlots.end());
            }
        }

        analyseSharedPlot_(sharedCoords);

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
        debugSharedPlots();
#endif
    }

    void MapAnalysis::reassignUnworkedSharedPlots(IDInfo city)
    {
        CitySharedPlotsMap::const_iterator citySharedPlotsIter = citySharedPlots_.find(city);
        if (citySharedPlotsIter != citySharedPlots_.end())
        {
            const CvMap& theMap = gGlobals.getMap();
            const CvCity* pCity = getCity(city);

            for (std::set<XYCoords>::const_iterator plotIter(citySharedPlotsIter->second.sharedPlots.begin()), plotEndIter(citySharedPlotsIter->second.sharedPlots.end());
                plotIter != plotEndIter; ++plotIter)
            {
                SharedPlots::iterator sharedPlotsIter = sharedPlots_.find(*plotIter);
                if (sharedPlotsIter != sharedPlots_.end())
                {
                    CvPlot* pPlot = theMap.plot(sharedPlotsIter->first.iX, sharedPlotsIter->first.iY);
                    if (!pCity->isWorkingPlot(pPlot) && pCity->canWork(pPlot))
                    {
                        for (SharedPlot::PossibleCitiesIter citiesIter(sharedPlotsIter->second.possibleCities.begin()), citiesEndIter(sharedPlotsIter->second.possibleCities.end());
                            citiesIter != citiesEndIter; ++citiesIter)
                        {
                            if (*citiesIter != city)
                            {
                                pPlot->setWorkingCityOverride(getCity(*citiesIter));  // give another city a chance to choose the plot
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    IDInfo MapAnalysis::setWorkingCity(const CvPlot* pPlot, const CvCity* pOldCity, const CvCity* pNewCity)
    {
        XYCoords coords(pPlot->getCoords());

        if (!pNewCity)
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nTurn: " << gGlobals.getGame().getGameTurn() << " - about to clear plot override back to: " << safeGetCityName(pOldCity)
                << " for plot: " << coords;
#endif
        }
        else
        {
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
            os << "\nTurn: " << gGlobals.getGame().getGameTurn() << " - about to set plot override back from: " << safeGetCityName(pOldCity)
                << " to: " << safeGetCityName(pNewCity) << " for plot: " << coords;
#endif
        }

        const CvCity* pCity = pNewCity ? pNewCity : pOldCity;

        IDInfo assignedCity = getSharedPlot(pCity->getIDInfo(), pPlot->getCoords());
        setWorkingCity_(coords, pNewCity ? pNewCity->getIDInfo() : IDInfo());
        
        return assignedCity;
    }

    void MapAnalysis::analyseSharedPlot_(const std::set<XYCoords>& sharedCoords)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
        TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1));

        for (std::set<XYCoords>::const_iterator coordsIter(sharedCoords.begin()), coordsEndIter(sharedCoords.end()); coordsIter != coordsEndIter; ++coordsIter)
        {
            SharedPlots::iterator sharedPlotIter = sharedPlots_.find(*coordsIter);
            if (sharedPlotIter != sharedPlots_.end())
            {
                IDInfo origAssignedCity = sharedPlotIter->second.assignedCity;
                CvPlot* pPlot = gGlobals.getMap().plot(coordsIter->iX, coordsIter->iY);
                CvCity* pBestCity = NULL;
                int worstDelta = 0;

                for (SharedPlot::PossibleCitiesIter citiesIter(sharedPlotIter->second.possibleCities.begin()), citiesEndIter(sharedPlotIter->second.possibleCities.end());
                    citiesIter != citiesEndIter; ++citiesIter)
                {
                    SharedPlot::PossibleCitiesIter assignedCityIter = citiesIter;

                    CvCity* pCity = getCity(*assignedCityIter);
                    sharedPlotIter->second.assignedCity = *assignedCityIter;

                    CityDataPtr pCityData(new CityData(pCity));
                    const City& city = player_.getCity(pCity);

                    city.optimisePlots(pCityData, city.getConstructItem());

                    PlotDataListConstIter plotsIter = pCityData->findPlot(pPlot->getCoords());

                    if (plotsIter != pCityData->getPlotOutputs().end())
                    {
                        if (plotsIter->isWorked)
                        {
                            TotalOutput withYield = pCityData->getOutput();
#ifdef ALTAI_DEBUG
                            os << "\nPlot: " <<  pPlot->getCoords() << " ";
                            plotsIter->debugSummary(os);
                            os << " worked by city: " << narrow(pCity->getName()) << ", total output = " << withYield;
                            pCityData->debugSummary(os);
#endif
                            ++assignedCityIter;
                            if (assignedCityIter == citiesEndIter)
                            {
                                assignedCityIter = sharedPlotIter->second.possibleCities.begin();
                            }

                            sharedPlotIter->second.assignedCity = *assignedCityIter;
                            pCityData = CityDataPtr(new CityData(pCity));
                            city.optimisePlots(pCityData, city.getConstructItem());

                            TotalOutput withoutYield = pCityData->getOutput();

                            int thisDelta = valueF(withYield) - valueF(withoutYield);
                            if (thisDelta > worstDelta || !pBestCity)
                            {
                                worstDelta = thisDelta;
                                pBestCity = pCity;
                            }
#ifdef ALTAI_DEBUG
                            os << "\nPlot: " <<  pPlot->getCoords() << " when not worked by city: " << narrow(pCity->getName())
                               << ", total output = " << withoutYield << ", delta value = " << thisDelta;
                            pCityData->debugSummary(os);
#endif
                        }
                        else
                        {
#ifdef ALTAI_DEBUG
                            os << "\nPlot: " <<  pPlot->getCoords() << " not worked by city: " << narrow(pCity->getName());
#endif
                        }
                    }
                }
                if (pBestCity)
                {
#ifdef ALTAI_DEBUG
                    os << "\nSetting plot override for plot: " << *coordsIter << " to city: " << safeGetCityName(pBestCity);
#endif
                
                    sharedPlotIter->second.assignedCity = pBestCity->getIDInfo();
                    setWorkingCity_(pPlot->getCoords(), pBestCity->getIDInfo());
                    pPlot->setWorkingCityOverride(pBestCity);
                }
                else
                {
                    sharedPlotIter->second.assignedCity = origAssignedCity;
#ifdef ALTAI_DEBUG
                    os << "\nNot setting plot override for plot: " << *coordsIter << " current city: "
                        << safeGetCityName(pPlot->getWorkingCityOverride());
#endif
                }
            }
        }

    }

    MapAnalysis::CitySharedPlotsMap::iterator MapAnalysis::getCitySharedPlots_(IDInfo city)
    {
        CitySharedPlotsMap::iterator citySharedPlotsIter = citySharedPlots_.find(city);
        if (citySharedPlotsIter == citySharedPlots_.end())
        {
            CitySharedPlots citySharedPlots(city);
            citySharedPlotsIter = citySharedPlots_.insert(std::make_pair(city, citySharedPlots)).first;
        }
        return citySharedPlotsIter;
    }

    MapAnalysis::SharedPlots::iterator MapAnalysis::getSharedPlot_(XYCoords coords)
    {
        SharedPlots::iterator sharedPlotsIter = sharedPlots_.find(coords);
        if (sharedPlotsIter == sharedPlots_.end())
        {
            SharedPlot sharedPlot(coords);
            sharedPlotsIter = sharedPlots_.insert(std::make_pair(coords, sharedPlot)).first;
        }
        return sharedPlotsIter;
    }

    void MapAnalysis::setWorkingCity_(XYCoords coords, IDInfo assignedCity)
    {
        SharedPlots::iterator sharedPlotsIter = sharedPlots_.find(coords);
        if (sharedPlotsIter != sharedPlots_.end())
        {
            if (sharedPlotsIter->second.possibleCities.find(assignedCity) != sharedPlotsIter->second.possibleCities.end())
            {
#ifdef ALTAI_DEBUG
                if (assignedCity != sharedPlotsIter->second.assignedCity)
                {
                    std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
                    os << "\nMismatched city overrides for : " << coords
                        << " have: " << safeGetCityName(sharedPlotsIter->second.assignedCity)
                        << " setting: " << safeGetCityName(assignedCity);
                }
#endif
                sharedPlotsIter->second.assignedCity = assignedCity;
            }
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
//                        os << "\nChecking plot: " << pLoopPlot->getCoords();
//                    }
//#endif
                    const CvCity* pLoopCity = pLoopPlot->getWorkingCity();
                    if (pLoopCity && pLoopCity->getOwner() == pPlot->getOwner())
                    {
                        const XYCoords loopCoords(pLoopPlot->getCoords());
//#ifdef ALTAI_DEBUG
//                        {   // debug
//                            os << "\nChecking city: " << narrow(pLoopCity->getName()) << " for: " << coords << " for source of irrigation chain";
//                        }
//#endif
                        std::vector<PlotImprovementData>& improvements = player_.getCity(pLoopCity).getCityImprovementManager()->getImprovements();
                        for (size_t i = 0, count = improvements.size(); i < count; ++i)
                        {
                            if (improvements[i].coords == loopCoords)
                            {
#ifdef ALTAI_DEBUG
                                {   // debug
                                    os << "\nMarked plot: " << loopCoords << " as possible source of irrigation chain";
                                }
#endif
                                improvements[i].flags |= PlotImprovementData::IrrigationChainPlot;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // add/remove cities... (doesn't have to be ours - want to update dotmap regardless)
    void MapAnalysis::addCity(const CvCity* pCity)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        os << "\nMapAnalysis::addCity - adding city: " << narrow(pCity->getName());
#endif
        const CvPlot* pPlot = pCity->plot();
        const XYCoords coords(pPlot->getCoords());
        IDInfo thisCity = pCity->getIDInfo();

        seenCities_[pPlot->getPlotCity()->getIDInfo()] = coords;

        if (pCity->getOwner() == player_.getPlayerID())
        {
            // iterate over all plots within city's workable radius
            for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            {
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(player_.getTeamID(), false) && pLoopPlot->getOwner() == player_.getPlayerID())
                    {
                        std::vector<IDInfo> possibleCities;
                        IDInfo improvementOwningCity;

                        for (int j = 1; j <= CITY_PLOTS_RADIUS; ++j)
                        {
                            CultureRangePlotIter loopPlotIter(pLoopPlot, (CultureLevelTypes)j);
                            while (IterPlot pCityLoopPlot = loopPlotIter())
                            {
                                if (pCityLoopPlot.valid() && pCityLoopPlot->getOwner() == player_.getPlayerID())
                                {
                                    const CvCity* pLoopCity = pCityLoopPlot->getPlotCity();
                                    if (pLoopCity && pLoopCity->getIDInfo() != thisCity)
                                    {
                                        possibleCities.push_back(pLoopCity->getIDInfo());
#ifdef ALTAI_DEBUG
                                        os << "\nAdding shared plot: " << pLoopPlot->getCoords() << " for city: " << safeGetCityName(pLoopCity)
                                            << " shared with: " << safeGetCityName(pCity);
#endif
                                        CitySharedPlotsMap::iterator citySharedPlotsIter = getCitySharedPlots_(pLoopCity->getIDInfo());
                                        citySharedPlotsIter->second.sharedPlots.insert(pLoopPlot->getCoords());

                                        // see if this city has assigned and selected (not necessarily built) an improvement to this plot
                                        bool foundImprovement;
                                        PlotImprovementData* pImprovementData;
                                        boost::tie(foundImprovement, pImprovementData) = 
                                            player_.getCity(pLoopCity).getCityImprovementManager()->findImprovement(pLoopPlot->getCoords());
                                        if (foundImprovement && pImprovementData->simulationData.firstTurnWorked > -1)
                                        {
                                            improvementOwningCity = pLoopCity->getIDInfo();
#ifdef ALTAI_DEBUG
                                            os << "\nSetting shared improvement owner for coords: " << pLoopPlot->getCoords() << " to city: " << safeGetCityName(pLoopCity);
#endif
                                        }
                                    }
                                }
                            }
                        }

                        if (!possibleCities.empty())
                        {
                            possibleCities.push_back(pCity->getIDInfo());

                            CitySharedPlotsMap::iterator citySharedPlotsIter = getCitySharedPlots_(pCity->getIDInfo());
                            citySharedPlotsIter->second.sharedPlots.insert(pLoopPlot->getCoords());

                            MapAnalysis::SharedPlots::iterator sharedPlotsIter = getSharedPlot_(pLoopPlot->getCoords());
                            sharedPlotsIter->second.possibleCities.insert(possibleCities.begin(), possibleCities.end());
                            if (sharedPlotsIter->second.assignedCity == IDInfo())
                            {
                                sharedPlotsIter->second.assignedCity = pLoopPlot->getWorkingCity() ? 
                                    pLoopPlot->getWorkingCity()->getIDInfo() : IDInfo();
                            }

                            // now we've added the shared plot data, can set any imp ownership info
                            if (improvementOwningCity != IDInfo())
                            {
                                sharedPlotsIter->second.assignedImprovementCity = improvementOwningCity;
                            }
#ifdef ALTAI_DEBUG
                            os << "\nAdding shared plot: " << pLoopPlot->getCoords() << " for city: " << safeGetCityName(pCity);
#endif
                        }
                    }
                }
            }

#ifdef ALTAI_DEBUG
            debugSharedPlots();
#endif
        }

        // remove plots from plotValues_
        // the city plot...
#ifdef ALTAI_DEBUG
        os << "\nRemoving plot: " << pPlot->getCoords() << " from plot values map for sub area: (new city) " << pPlot->getSubArea();
#endif
        plotValues_.plotValueMap[pPlot->getSubArea()].erase(pPlot->getCoords());
        // neightbouring plots which are now invalid (must be same area)
        NeighbourPlotIter plotIter(pPlot, 2, 2);
        while (IterPlot pLoopPlot = plotIter())
        {
            if (!pLoopPlot.valid())
            {
                continue;
            }
#ifdef ALTAI_DEBUG
            os << "\nChecking plot: " << pLoopPlot->getCoords() << " for removal from plotValues map";
#endif
            if (pLoopPlot->getArea() == pPlot->getArea())
            {
                PlotValues::PlotValueMap::iterator iter = plotValues_.plotValueMap.find(pLoopPlot->getSubArea());
                if (iter != plotValues_.plotValueMap.end())
                {
                    PlotValues::SubAreaPlotValueMap::iterator plotValueIter = iter->second.find(pLoopPlot->getCoords());
                    if (plotValueIter != iter->second.end())
                    {
#ifdef ALTAI_DEBUG
                        os << "\nRemoving plot: " << pLoopPlot->getCoords() << " from plot values map for sub area: " << pLoopPlot->getSubArea();
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
        IDInfo city = pCity->getIDInfo();
        std::map<IDInfo, XYCoords>::iterator seenCitiesIter = seenCities_.find(city);
        if (seenCitiesIter != seenCities_.end())
        {
            seenCities_.erase(seenCitiesIter);
        }
        else
        {
            ErrorLog::getLog(*player_.getCvPlayer())->getStream() << "\nErasing unseen city: " << city << " at: " << pCity->plot()->getCoords();
        }

        const CvPlot* pPlot = pCity->plot();

        CitySharedPlotsMap::iterator cityIter = citySharedPlots_.find(city);
        if (cityIter != citySharedPlots_.end())
        {
            for (std::set<XYCoords>::const_iterator citySharedPlotsIter(cityIter->second.sharedPlots.begin()), citySharedPlotsEndIter(cityIter->second.sharedPlots.end());
                citySharedPlotsIter != citySharedPlotsEndIter; ++citySharedPlotsIter)
            {
                SharedPlots::iterator sharedPlotsIter = sharedPlots_.find(*citySharedPlotsIter);
                if (sharedPlotsIter != sharedPlots_.end())
                {
                    sharedPlotsIter->second.possibleCities.erase(city);
                    if (sharedPlotsIter->second.possibleCities.size() < 2)
                    {
                        sharedPlots_.erase(*citySharedPlotsIter);
                    }
                    else
                    {
                        if (sharedPlotsIter->second.assignedCity == city)
                        {
                            sharedPlotsIter->second.assignedCity = IDInfo();
                        }
                        if (sharedPlotsIter->second.assignedImprovementCity == city)
                        {
                            sharedPlotsIter->second.assignedImprovementCity = IDInfo();
                        }
                    }
                }
            }
        }

        citySharedPlots_.erase(city);
        updatePlotInfo_(pPlot, false);
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
                    PlayerTypes owner = boost::get<1>(bi->second[i]);
                    if (owner == player_.getPlayerID())
                    {
                        ++ourCount;
                    }
                    else if (owner == NO_PLAYER)
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
            os << "\nPlot: " << ci->first << " ";

            for (SharedPlot::PossibleCitiesConstIter citiesIter(ci->second.possibleCities.begin()), citiesEndIter(ci->second.possibleCities.end());
                citiesIter != citiesEndIter; ++citiesIter)
            {
                if (citiesIter != ci->second.possibleCities.begin()) os << ", ";
                os << safeGetCityName(*citiesIter);
            }

            os << " worked by: " << safeGetCityName(ci->second.assignedCity) << ", imp assigned to: " << safeGetCityName(ci->second.assignedImprovementCity);

            CvPlot* pPlot = gGlobals.getMap().plot(ci->first.iX, ci->first.iY);
            os << " (Plot = " << (pPlot->getWorkingCity() ? narrow(pPlot->getWorkingCity()->getName()) : " none ") << ")\n";
        }

        for (CitySharedPlotsMap::const_iterator ci(citySharedPlots_.begin()), ciEnd(citySharedPlots_.end()); ci != ciEnd; ++ci)
        {
            os << "\nCity: " << safeGetCityName(ci->first) << " shared plots: ";
            for (std::set<XYCoords>::const_iterator coordsIter(ci->second.sharedPlots.begin()), coordsEndIter(ci->second.sharedPlots.end()); coordsIter != coordsEndIter; ++coordsIter)
            {
                if (coordsIter != ci->second.sharedPlots.begin()) os << ", ";
                os << *coordsIter << " ";
            }
        }
#endif
    }
}