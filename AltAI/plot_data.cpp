#include "AltAI.h"

#include "./plot_data.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./plot_info.h"
#include "./save_utils.h"
#include "./civ_log.h"

namespace AltAI
{
    PlotData::PlotData() : improvementType(NO_IMPROVEMENT), bonusType(NO_BONUS), featureType(NO_FEATURE), ableToWork(true), controlled(true), isWorked(false)
    {
    }

    PlotData::PlotData(const PlotInfoPtr& pPlotInfo_, PlotYield plotYield_, Commerce commerce_, TotalOutput output_, const GreatPersonOutput& greatPersonOutput_,
                       XYCoords coords_, ImprovementTypes improvementType_, BonusTypes bonusType_, FeatureTypes featureType_, 
                       RouteTypes routeType_, CultureData cultureData_)
        : pPlotInfo(pPlotInfo_), plotYield(plotYield_), commerce(commerce_), output(output_), actualOutput(output_), greatPersonOutput(greatPersonOutput_),
          coords(coords_), improvementType(improvementType_), bonusType(bonusType_), featureType(featureType_), routeType(routeType_),
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

    void PlotData::debugSummary(std::ostream& os) const
    {
        if (isActualPlot())
        {
            const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
            std::string featureString = shortFeatureType(featureType);
            std::string improvementString = shortImprovementType(improvementType);
            std::string bonusString = shortBonusType(bonusType);

            os << shortTerrainType(pPlot->getTerrainType()) << (featureString.empty() ? "" : " ") << featureString
                << (bonusString.empty() ? "" : " ") << bonusString
                << (pPlot->isHills() ? " hills" : "");
            if (!improvementString.empty())
            {
                os << " [" << improvementString;
                if (!upgradeData.upgrades.empty())
                {
                    os << " -> " << shortImprovementType(upgradeData.upgrades.begin()->improvementType) << " "
                        << upgradeData.timeHorizon - upgradeData.upgrades.begin()->remainingTurns << "t]";
                }
                else
                {
                    os << "]";
                }
            }

            os << " " << plotYield;
            //<< " {" << actualOutput << "},";
        }
        else
        {
            const CvSpecialistInfo& specInfo = gGlobals.getSpecialistInfo((SpecialistTypes)coords.iY);
            os << " " << specInfo.getType() << " " << actualOutput << ",";
        }
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

    PlotData::UpgradeData::Upgrade PlotData::UpgradeData::advanceTurn(int nTurns)
    {
        // can only get one upgrade - should give latest version if we somehow call with a greater interval than the upgrade time for an improvement
        Upgrade upgrade;
        PlotData::UpgradeData::UpgradeListIter iter(upgrades.begin()), endIter(upgrades.end());
        
        while (iter != endIter)
        {
            iter->remainingTurns = std::min<int>(timeHorizon, iter->remainingTurns + nTurns);
            if (iter->remainingTurns == timeHorizon)
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

    void PlotData::write(FDataStreamBase* pStream) const
    {
        plotYield.write(pStream);
        commerce.write(pStream);
        output.write(pStream);
        actualOutput.write(pStream);
        greatPersonOutput.write(pStream);
        coords.write(pStream);
        pStream->Write(improvementType);
        pStream->Write(featureType);
        pStream->Write(routeType);
        upgradeData.write(pStream);
        cultureData.write(pStream);
        pStream->Write(ableToWork);
        pStream->Write(controlled);
        pStream->Write(isWorked);
    }

    void PlotData::read(FDataStreamBase* pStream)
    {
        plotYield.read(pStream);
        commerce.read(pStream);
        output.read(pStream);
        actualOutput.read(pStream);
        greatPersonOutput.read(pStream);
        coords.read(pStream);
        pStream->Read((int*)&improvementType);
        pStream->Read((int*)&featureType);
        pStream->Read((int*)&routeType);
        upgradeData.read(pStream);
        cultureData.read(pStream);
        pStream->Read(&ableToWork);
        pStream->Read(&controlled);
        pStream->Read(&isWorked);
    }

    void PlotData::UpgradeData::write(FDataStreamBase* pStream) const
    {
        pStream->Write(timeHorizon);
        writeComplexList(pStream, upgrades);
    }

    void PlotData::UpgradeData::read(FDataStreamBase* pStream)
    {
        pStream->Read(&timeHorizon);
        readComplexList<Upgrade>(pStream, upgrades);
    }

    void PlotData::UpgradeData::Upgrade::write(FDataStreamBase* pStream) const
    {
        pStream->Write(improvementType);
        pStream->Write(remainingTurns);
        extraYield.write(pStream);
    }

    void PlotData::UpgradeData::Upgrade::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&improvementType);
        pStream->Read(&remainingTurns);
        extraYield.read(pStream);
    }

    void PlotData::CultureData::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ownerAndCultureTrumpFlag.first);
        pStream->Write(ownerAndCultureTrumpFlag.second);

        pStream->Write(cultureSourcesMap.size());
        for (CultureSourcesMap::const_iterator ci(cultureSourcesMap.begin()), ciEnd(cultureSourcesMap.end());
            ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            pStream->Write(ci->second.first);
            pStream->Write(ci->second.second.size());
            for (size_t i = 0, count = ci->second.second.size(); i < count; ++i)
            {
                ci->second.second[i].write(pStream);
            }
        }
    }

    void PlotData::CultureData::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&ownerAndCultureTrumpFlag.first);
        pStream->Read(&ownerAndCultureTrumpFlag.second);

        size_t cultureSourcesMapSize;
        pStream->Read(&cultureSourcesMapSize);
        for (size_t i = 0; i < cultureSourcesMapSize; ++i)
        {
            PlayerTypes playerType;
            int playerPlotCulture;            
            size_t cultureSourceCount;

            pStream->Read((int*)&playerType);
            pStream->Read(&playerPlotCulture);
            pStream->Read(&cultureSourceCount);

            std::vector<CultureSource> cultureSources;
            
            for (size_t j = 0; j < cultureSourceCount; ++j)
            {
                CultureData::CultureSource cultureSource;
                cultureSource.read(pStream);
                cultureSources.push_back(cultureSource);
            }

            cultureSourcesMap.insert(std::make_pair(playerType, std::make_pair(playerPlotCulture, cultureSources)));
        }
    }

    void PlotData::CultureData::CultureSource::read(FDataStreamBase* pStream)
    {
        pStream->Read(&output);
        pStream->Read(&range);
        city.read(pStream);
    }

    void PlotData::CultureData::CultureSource::write(FDataStreamBase* pStream) const
    {
        pStream->Write(output);
        pStream->Write(range);
        city.write(pStream);
    }

    void GreatPersonOutput::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&unitType);
        pStream->Read(&output);
    }

    void GreatPersonOutput::write(FDataStreamBase* pStream) const
    {
        pStream->Write(unitType);
        pStream->Write(output);
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