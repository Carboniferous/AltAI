#include "./culture_helper.h"
#include "./city_data.h"
#include "./city_simulator.h"

namespace AltAI
{
    CultureHelper::CultureHelper(const CvCity* pCity, CityData& data) : data_(data)
    {
        owner_ = pCity->getOwner();
        cityCulture_ = pCity->getCulture(owner_);
        cultureLevel_ = pCity->getCultureLevel();
        CITY_FREE_CULTURE_GROWTH_FACTOR_ = gGlobals.getDefineINT("CITY_FREE_CULTURE_GROWTH_FACTOR");
    }

    void CultureHelper::advanceTurn(bool includeUnclaimedPlots)
    {
        const int cultureOutput = data_.getOutput()[OUTPUT_CULTURE];
        cityCulture_ += cultureOutput / 100;

        CultureLevelTypes oldCultureLevel = cultureLevel_;

        bool culturalLevelChange = checkCulturalLevel_();

#ifdef ALTAI_DEBUG
        // debug
        {
            //if (culturalLevelChange)
            //{
            //    boost::shared_ptr<CityLog> pLog = CityLog::getLog(data.pCity);
            //    pLog->logCultureLevelChange(oldCultureLevel, cultureLevel_);
            //}
        }
#endif

        bool plotOwnersChanged = false;
        std::vector<std::pair<PlayerTypes, XYCoords> > changedPlotsData;

        updatePlot_(data_.getCityPlotOutput(), culturalLevelChange, data_.getCity(), cultureOutput);

        PlotDataListIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end());
        while (iter != endIter)
        {
            if (iter->isActualPlot())
            {
                bool thisPlotChanged = updatePlot_(*iter, culturalLevelChange, data_.getCity(), cultureOutput);

                if (thisPlotChanged)
                {
                    if (!(includeUnclaimedPlots && iter->controlled))
                    {
                        changedPlotsData.push_back(std::make_pair(iter->cultureData.ownerAndCultureTrumpFlag.first, iter->coords));
                        PlotDataListIter removeIter(iter++);
                        data_.getUnworkablePlots().splice(data_.getUnworkablePlots().begin(), data_.getPlotOutputs(), removeIter);
                    }
                }
                else
                {
                    ++iter;
                }

                plotOwnersChanged = plotOwnersChanged || thisPlotChanged;
            }
            else
            {
                ++iter;
            }
        }

        iter = data_.getUnworkablePlots().begin(), endIter = data_.getUnworkablePlots().end();
        while (iter != endIter)
        {
            if (iter->isActualPlot())
            {
                bool thisPlotChanged = updatePlot_(*iter, culturalLevelChange, data_.getCity(), cultureOutput);

                if (thisPlotChanged)
                {
                    changedPlotsData.push_back(std::make_pair(iter->cultureData.ownerAndCultureTrumpFlag.first, iter->coords));
                    PlotDataListIter removeIter(iter++);
                    data_.getPlotOutputs().splice(data_.getPlotOutputs().begin(), data_.getUnworkablePlots(), removeIter);
                }
                else
                {
                    ++iter;
                }

                plotOwnersChanged = plotOwnersChanged || thisPlotChanged;
            }
            else
            {
                ++iter;
            }
        }


        if (culturalLevelChange)
        {
            data_.pushEvent(CitySimulationEventPtr(new CultureBorderExpansion()));

#ifdef ALTAI_DEBUG
            // debug
            //{
            //    boost::shared_ptr<CityLog> pLog = CityLog::getLog(data_.pCity);
            //    pLog->logCultureData(data_);
            //}
#endif
        }

        if (plotOwnersChanged)
        {
#ifdef ALTAI_DEBUG
            // debug
            {
                boost::shared_ptr<CityLog> pLog = CityLog::getLog(data_.getCity());
                pLog->logPlotControlChange(changedPlotsData);   
            }
#endif
            data_.pushEvent(CitySimulationEventPtr(new PlotControlChange(changedPlotsData)));
        }
    }

    bool CultureHelper::checkCulturalLevel_()
    {
        const CvGameAI& theGame = gGlobals.getGame();

        CultureLevelTypes level = NO_CULTURELEVEL;
        for (int i = (gGlobals.getNumCultureLevelInfos() - 1); i > 0; --i)
		{
			if (cityCulture_ >= theGame.getCultureThreshold((CultureLevelTypes)i))
			{
				level = (CultureLevelTypes)i;
                break;
			}
		}

        if (level != cultureLevel_)
        {
            cultureLevel_ = level;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool CultureHelper::updatePlot_(PlotData& plotData, bool culturalLevelChange, const CvCity* pCity, int cultureOutput)
    {
        typedef PlotData::CultureData::CultureSourcesMap CultureSourcesMap;
        typedef CultureSourcesMap::iterator Iter;

        bool ownerChanged = false;

        const int cityX = pCity->getX(), cityY = pCity->getY();
        const int range = std::max<int>(1, plotDistance(cityX, cityY, plotData.coords.iX, plotData.coords.iY));

        CultureSourcesMap& cultureSourcesMap(plotData.cultureData.cultureSourcesMap);

        Iter iter(cultureSourcesMap.find(owner_));
        bool foundOurself = false;

        if (iter != cultureSourcesMap.end())
        {
            // look through source cities and update ourselves, if found
            for (size_t sourceIndex = 0, sourceCount = iter->second.second.size(); sourceIndex < sourceCount; ++sourceIndex)
            {
                if (iter->second.second[sourceIndex].city == pCity->getIDInfo())
                {
                    foundOurself = true;
                    if (culturalLevelChange)
                    {
                        // calculate new culture level
                        iter->second.second[sourceIndex].range = cultureLevel_ - range;
                    }
                    // update source's output
                    iter->second.second[sourceIndex].output = cultureOutput / 100;
                }
            }
        }

        // check if need to add ourselves as a new source
        if (culturalLevelChange && !foundOurself && range <= cultureLevel_)  
        {
            cultureSourcesMap[owner_].second.push_back(PlotData::CultureData::CultureSource(cultureOutput / 100, cultureLevel_ - range, pCity));
        }

        // now loop over all sources and update plot's culture for each contributing player
        for (Iter iter(cultureSourcesMap.begin()), endIter(cultureSourcesMap.end()); iter != endIter; ++iter)
        {
            for (size_t sourceIndex = 0, sourceCount = iter->second.second.size(); sourceIndex < sourceCount; ++sourceIndex)
            {
                iter->second.first += iter->second.second[sourceIndex].range * CITY_FREE_CULTURE_GROWTH_FACTOR_ + iter->second.second[sourceIndex].output + 1;
            }
        }

        // owner not overridden by factors other than culture
        if (!plotData.cultureData.ownerAndCultureTrumpFlag.second)
        {
            int highestCulture = 0;
            PlayerTypes highestCulturePlayer = NO_PLAYER, oldHighestCulturePlayer = (PlayerTypes)plotData.cultureData.ownerAndCultureTrumpFlag.first;

            for (CultureSourcesMap::iterator iter(cultureSourcesMap.begin()), endIter(cultureSourcesMap.end()); iter != endIter; ++iter)
            {
                int playerCulture = iter->second.first;
                if (playerCulture > highestCulture)
                {
                    highestCulture = playerCulture;
                    highestCulturePlayer = iter->first;
                }
            }

            if (oldHighestCulturePlayer != highestCulturePlayer)
            {
                plotData.controlled = highestCulturePlayer == owner_;
                plotData.cultureData.ownerAndCultureTrumpFlag.first = highestCulturePlayer;
                ownerChanged = true;
            }
        }
        return ownerChanged;
    }
}
