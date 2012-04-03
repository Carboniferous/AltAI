#include "./plot_data.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./civ_log.h"

namespace AltAI
{
    PlotData::PlotData() : improvementType(NO_IMPROVEMENT), featureType(NO_FEATURE), ableToWork(true), controlled(true), isWorked(false)
    {
    }

    PlotData::PlotData(PlotYield plotYield_, Commerce commerce_, TotalOutput output_, const GreatPersonOutput& greatPersonOutput_,
                       XYCoords coords_, ImprovementTypes improvementType_, FeatureTypes featureType_, RouteTypes routeType_, CultureData cultureData_)
        : plotYield(plotYield_), commerce(commerce_), output(output_), actualOutput(output_), greatPersonOutput(greatPersonOutput_),
          coords(coords_), improvementType(improvementType_), featureType(featureType_), routeType(routeType_),
          cultureData(cultureData_), ableToWork(true), controlled(true), isWorked(false)
    {
    }

    PlotData::CultureData::CultureData(const CvPlot* pPlot, PlayerTypes player, const CvCity* pOriginalCity)
    {
        TeamTypes teamID = PlayerIDToTeamID(player);

        for (int rangeIndex = 0, count = gGlobals.getNumCultureLevelInfos(); rangeIndex < count; ++rangeIndex)
        {
            CultureRangePlotIter iter(pPlot, (CultureLevelTypes)rangeIndex);  // iterate over all plots at given culture radius
            while (IterPlot pLoopPlot = iter())
            {
                if (pLoopPlot.valid())
                {
                    if (pLoopPlot->isCity())  // TODO - check plot owner = city owner even if not majority culture
                    {
                        const CvCity* pCity = pLoopPlot->getPlotCity();
                        PlayerTypes playerIndex = pLoopPlot->getOwner();

                        CultureLevelTypes sourceLevel = pCity->getCultureLevel();

                        if (sourceLevel - rangeIndex >= 0)
                        {
#ifdef ALTAI_DEBUG
                            //if (pCity->area() != pOriginalCity->area())  // debug
                            //{
                            //    std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player))->getStream();
                            //    os << "\n" << narrow(pOriginalCity->getName()) << ": plot = " << pPlot->getX() << ", " << pPlot->getY() << " distance to " << narrow(pCity->getName()) << " = "
                            //       << plotDistance(pPlot->getX(), pPlot->getY(), pCity->getX(), pCity->getY()) << " range = " << rangeIndex << "\n";
                            //}
#endif
                            if (!pPlot->isPotentialCityWorkForArea(pCity->area()))
                            {
#ifdef ALTAI_DEBUG
                                // debug
                                //{
                                //    std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(player))->getStream();
                                //    os << "\n" << narrow(pOriginalCity->getName()) << ": plot = " << pPlot->getX() << ", " << pPlot->getY() << " distance to " << narrow(pCity->getName()) << " = "
                                //       << plotDistance(pPlot->getX(), pPlot->getY(), pCity->getX(), pCity->getY()) << " range = " << rangeIndex << "\n";
                                //}
#endif
                                continue;
                            }
                            
                            // TODO if have city visibility - get actual culture output - otherwise infer it
                            //if (pLoopPlot->isVisible(teamID, false))
                            //{
                                cultureSourcesMap[playerIndex].second.push_back(CultureSource(pCity->getCommerceRate(COMMERCE_CULTURE), sourceLevel - rangeIndex, pCity));
                            //}
                            //else  // infer culture from distance => min. cultural level
                            //{
    
                            //}
                        }
                    }
                }
            }
        }

        int highestCulture = 0;
        PlayerTypes highestCulturePlayer = NO_PLAYER;

        for (CultureSourcesMap::iterator iter(cultureSourcesMap.begin()), endIter(cultureSourcesMap.end()); iter != endIter; ++iter)
        {
            int playerCulture = pPlot->getCulture(iter->first);
            if (playerCulture > highestCulture)
            {
                highestCulture = playerCulture;
                highestCulturePlayer = iter->first;
            }
            iter->second.first = playerCulture;
        }

        PlayerTypes owner = pPlot->getOwner();
        if (owner != highestCulturePlayer)
        {
            ownerAndCultureTrumpFlag = std::make_pair(owner, true);
        }
        else
        {
            ownerAndCultureTrumpFlag = std::make_pair(highestCulturePlayer, false);
        }
    }

    void PlotData::debug(std::ostream& os, bool includeUpgradeData, bool includeCultureData) const
    {
#ifdef ALTAI_DEBUG
        os << "\n " << coords;

        os << " Yield = " << plotYield << ", Commerce = " << commerce << ", total = " << output << ", actual = " << actualOutput;

        os << (controlled ? " controlled," : " not controlled,") << (ableToWork ? " can work," : " can't work,") << (isWorked ? " worked," : " not worked,");

        if (!isEmpty(greatPersonOutput.output))
        {
            os << ", GPP output = " << greatPersonOutput.output << " from: " << gGlobals.getUnitInfo(greatPersonOutput.unitType).getType();
        }

        if (improvementType != NO_IMPROVEMENT)
        {
            os << ", " << gGlobals.getImprovementInfo(improvementType).getType();
        }

        if (featureType != NO_FEATURE)
        {
            os << ", " << gGlobals.getFeatureInfo(featureType).getType();
        }

        if (includeUpgradeData && !upgradeData.upgrades.empty() && upgradeData.upgrades.begin()->improvementType != NO_IMPROVEMENT)
        {
            os << " upgrade: ";
            upgradeData.debug(os);
        }

        if (includeCultureData)
        {
            cultureData.debug(os);
        }
#endif
    }

    TotalOutput PlotData::UpgradeData::getExtraOutput(YieldModifier yieldModifier, CommerceModifier commerceModifier, CommerceModifier commercePercent)
    {
        TotalOutput extraOutput;

        for (PlotData::UpgradeData::UpgradeListConstIter iter(upgrades.begin()), endIter(upgrades.end()); iter != endIter; ++iter)
        {
            if (iter->remainingTurns <= 0)
            {
                break;
            }
            int scaleFactor = (100 * iter->remainingTurns) / timeHorizon;
            extraOutput += makeOutput(iter->extraYield, yieldModifier, commerceModifier, commercePercent, scaleFactor);
        }
        return extraOutput;
    }

    PlotData::UpgradeData::Upgrade PlotData::UpgradeData::advanceTurn()
    {
        // can only get one upgrade
        Upgrade upgrade;

        PlotData::UpgradeData::UpgradeListIter iter(upgrades.begin()), endIter(upgrades.end());
        
        while (iter != endIter)
        {
            if (++iter->remainingTurns == timeHorizon)
            {
                upgrade = *iter;
                upgrades.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }
        return upgrade;
    }

    void PlotData::UpgradeData::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << " horizon = " << timeHorizon << " ";
        for (PlotData::UpgradeData::UpgradeListConstIter iter(upgrades.begin()), endIter(upgrades.end()); iter != endIter; ++iter)
        {
            os << "\n" << gGlobals.getImprovementInfo(iter->improvementType).getType() << " remaining turns = " << iter->remainingTurns << " extra yield = " << iter->extraYield;
        }
#endif
    }

    void PlotData::CultureData::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << " owner = " << ownerAndCultureTrumpFlag.first;

        if (ownerAndCultureTrumpFlag.second)
        {
            os << " (owner trumps culture) ";
        }
        os << "\n";

        for (CultureSourcesMap::const_iterator ci(cultureSourcesMap.begin()), ciEnd(cultureSourcesMap.end()); ci != ciEnd; ++ci)
        {
            os << " player: " << ci->first << " culture = " << ci->second.first;
            for (size_t i = 0, count = ci->second.second.size(); i < count; ++i)
            {
                os << " city = " << narrow(getCity(ci->second.second[i].city)->getName()) << " output = " << ci->second.second[i].output << " range = " << ci->second.second[i].range;
            }
            os << "\n";
        }
#endif
    }

    std::ostream& operator << (std::ostream& os, const GreatPersonOutput& output)
    {
        return os << output.unitType << output.output;
    }

    std::ostream& operator << (std::ostream& os, const PlotData& plotData)
    {
        os << plotData.coords << plotData.controlled << plotData.isWorked << plotData.ableToWork;
        os << plotData.plotYield << plotData.commerce << plotData.output << plotData.actualOutput;
        os << plotData.improvementType << plotData.featureType;
        return os << plotData.cultureData << plotData.greatPersonOutput << plotData.upgradeData;
    }

    std::ostream& operator << (std::ostream& os, const PlotData::UpgradeData& upgradeData)
    {
        os << upgradeData.timeHorizon << upgradeData.upgrades.size();
        for (PlotData::UpgradeData::UpgradeListConstIter iter(upgradeData.upgrades.begin()), endIter(upgradeData.upgrades.end()); iter != endIter; ++iter)
        {
            os << iter->improvementType << iter->remainingTurns << iter->extraYield;
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const PlotData::CultureData& cultureData)
    {
        typedef PlotData::CultureData::CultureSourcesMap CultureSourcesMap;

        os << cultureData.ownerAndCultureTrumpFlag.first << cultureData.ownerAndCultureTrumpFlag.second;
        os << cultureData.cultureSourcesMap.size();

        for (CultureSourcesMap::const_iterator ci(cultureData.cultureSourcesMap.begin()), ciEnd(cultureData.cultureSourcesMap.end()); ci != ciEnd; ++ci)
        {
            os << ci->first << ci->second.first << ci->second.second.size();
            for (size_t i = 0, count = ci->second.second.size(); i < count; ++i)
            {
                os << ci->second.second[i].city.eOwner << ci->second.second[i].city.iID << ci->second.second[i].output << ci->second.second[i].range;
            }
        }
        return os;
    }
}