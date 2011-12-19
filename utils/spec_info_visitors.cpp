#include "./spec_info_visitors.h"
#include "./spec_info.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./civ_helper.h"
#include "./iters.h"
#include "./gamedata_analysis.h"

namespace AltAI
{
    namespace
    {
        SpecialistTypes getBestSpecialistHelper(PlayerTypes playerType, MixedTotalOutputOrderFunctor valueF,
            YieldModifier yieldModifier, CommerceModifier commerceModifier, CommerceModifier commercePercent)
        {
            TotalOutput bestOutput;
            SpecialistTypes bestSpecialist = NO_SPECIALIST;

            for (size_t i = 0, count = gGlobals.getNumSpecialistInfos(); i < count; ++i)
            {
                GreatPersonOutput outputAndUnitType = GameDataAnalysis::getSpecialistUnitTypeAndOutput((SpecialistTypes)i, playerType);

                if (outputAndUnitType.unitType == NO_UNIT || outputAndUnitType.output == 0)
                {
                    continue;
                }

                std::pair<PlotYield, Commerce> yieldAndCommerce = getYieldAndCommerce((SpecialistTypes)i, playerType);

                // need to produce one of the output types that are priorities
                bool canConsider = false;
                for (size_t j = 0, count = valueF.priorities.size(); j < count; ++j)
                {
                    if (valueF.priorities[j] != NO_OUTPUT)
                    {
                        switch (valueF.priorities[j])
                        {
                            case OUTPUT_FOOD:
                                canConsider = yieldAndCommerce.first[YIELD_FOOD] > 0;
                                break;
                            case OUTPUT_PRODUCTION:
                                canConsider = yieldAndCommerce.first[YIELD_PRODUCTION] > 0;
                                break;
                            case OUTPUT_GOLD:
                                canConsider = yieldAndCommerce.first[YIELD_COMMERCE] > 0 || yieldAndCommerce.second[COMMERCE_GOLD] > 0;
                                break;
                            case OUTPUT_RESEARCH:
                                canConsider = yieldAndCommerce.first[YIELD_COMMERCE] > 0 || yieldAndCommerce.second[COMMERCE_RESEARCH] > 0;
                                break;
                            case OUTPUT_CULTURE:
                                canConsider = yieldAndCommerce.first[YIELD_COMMERCE] > 0 || yieldAndCommerce.second[COMMERCE_CULTURE] > 0;
                                break;
                            case OUTPUT_ESPIONAGE:
                                canConsider = yieldAndCommerce.first[YIELD_COMMERCE] > 0 || yieldAndCommerce.second[COMMERCE_ESPIONAGE] > 0;
                                break;
                            default:
                                break;
                        }
                        if (canConsider)
                        {
                            break;
                        }
                    }
                }

                if (!canConsider)
                {
                    continue;
                }

                TotalOutput thisOutput = makeOutput(yieldAndCommerce.first, yieldAndCommerce.second, yieldModifier, commerceModifier, commercePercent);
                if (valueF(thisOutput, bestOutput))
                {
                    bestOutput = thisOutput;
                    bestSpecialist = (SpecialistTypes)i;
                }
            }

            return bestSpecialist;
        }
    }

    // TODO - needs completing
    class YieldAndCommerceVisitor : public boost::static_visitor<std::pair<PlotYield, Commerce> >
    {
    public:
        explicit YieldAndCommerceVisitor(PlayerTypes playerType) : playerType_(playerType)
        {
        }

        template <typename T> result_type operator() (const T& node) const
        {
            return result_type();
        }

        result_type operator() (const SpecInfo::BaseNode& node) const
        {
            result_type result(node.yield, node.commerce);

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                result_type nodeResult = boost::apply_visitor(*this, node.nodes[i]);
                result.first += nodeResult.first;
                result.second += nodeResult.second;
            }

            return result;
        }

        result_type operator() (const SpecInfo::CivicNode& node) const
        {
            result_type result;

            boost::shared_ptr<Player> player = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
            for (size_t i = 0, count = node.civicsExtraCommerce.size(); i < count; ++i)
            {
                if (player->getCivHelper()->isInCivic(node.civicsExtraCommerce[i].first))
                {
                    result.second += node.civicsExtraCommerce[i].second;
                }
            }

            return result;
        }

        result_type operator() (const SpecInfo::BuildingNode& node) const
        {
            result_type result;
            const CvPlayer& player = CvPlayerAI::getPlayer(playerType_);

            for (size_t i = 0, count = node.buildingExtraYields.size(); i < count; ++i)
            {
                CityIter iter(player);
                while (CvCity* pCity = iter())
                {
                    int count = pCity->getNumBuilding(node.buildingExtraYields[i].first);
                    if (count > 0)
                    {
                        result.first += count * node.buildingExtraYields[i].second;
                    }
                }
            }

            for (size_t i = 0, count = node.buildingsExtraCommerce.size(); i < count; ++i)
            {
                CityIter iter(player);
                while (CvCity* pCity = iter())
                {
                    int count = pCity->getNumBuilding(node.buildingsExtraCommerce[i].first);
                    if (count > 0)
                    {
                        result.second += count * node.buildingsExtraCommerce[i].second;
                    }
                }
            }

            return result;
        }

    private:
        PlayerTypes playerType_;
    };


    std::pair<PlotYield, Commerce> getYieldAndCommerce(SpecialistTypes specialistType, PlayerTypes playerType)
    {
        SpecInfo specInfo(specialistType, playerType);
        return boost::apply_visitor(YieldAndCommerceVisitor(playerType), specInfo.getInfo());
    }

    SpecialistTypes getBestSpecialist(const Player& player, const City& city, MixedTotalOutputOrderFunctor valueF)
    {
        TotalOutput bestOutput;
        SpecialistTypes bestSpecialist = NO_SPECIALIST;

        YieldModifier yieldModifier = city.getCityData()->getYieldModifier();
        CommerceModifier commerceModifier = city.getCityData()->getCommerceModifier(), commercePercent = city.getCityData()->getCommercePercent();

        return getBestSpecialistHelper(player.getPlayerID(), valueF, yieldModifier, commerceModifier, commercePercent);
    }

    SpecialistTypes getBestSpecialist(const Player& player, MixedTotalOutputOrderFunctor valueF)
    {
        TotalOutput bestOutput;
        SpecialistTypes bestSpecialist = NO_SPECIALIST;

        CommerceModifier commercePercent;
        for (int commerceType = 0; commerceType < NUM_COMMERCE_TYPES; ++commerceType)
        {
            commercePercent[commerceType] = player.getCvPlayer()->getCommercePercent((CommerceTypes)commerceType);
        }

        return getBestSpecialistHelper(player.getPlayerID(), valueF, makeYield(100, 100, 100), makeCommerce(100, 100, 100, 100), commercePercent);
    }
}