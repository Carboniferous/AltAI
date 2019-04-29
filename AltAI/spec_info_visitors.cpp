#include "AltAI.h"

#include "./spec_info_visitors.h"
#include "./spec_info.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./civ_helper.h"
#include "./modifiers_helper.h"
#include "./iters.h"
#include "./gamedata_analysis.h"

namespace AltAI
{
    namespace
    {
        template <typename P>
            struct SpecialistDataAdaptor
        {
            typedef typename P Pred;
            SpecialistDataAdaptor(P pred_) : pred(pred_) {}

            bool operator () (const std::pair<SpecialistTypes, TotalOutput>& p1, const std::pair<SpecialistTypes, TotalOutput>& p2) const
            {
                return pred(p1.second, p2.second);
            }
            P pred;
        };

        template <typename P>
            std::vector<SpecialistTypes> getBestSpecialistHelper(PlayerTypes playerType, P valueF,
                YieldModifier yieldModifier, CommerceModifier commerceModifier, CommerceModifier commercePercent, size_t selectionCount)
        {
            std::vector<SpecialistTypes> bestSpecialists;
            std::vector<std::pair<SpecialistTypes, TotalOutput> > specialistOutputs;

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
                specialistOutputs.push_back(std::make_pair((SpecialistTypes)i, thisOutput));
            }

            std::sort(specialistOutputs.begin(), specialistOutputs.end(), SpecialistDataAdaptor<P>(valueF));

            for (size_t i = 0; i < selectionCount && i < specialistOutputs.size(); ++i)
            {
                bestSpecialists.push_back(specialistOutputs[i].first);
            }

            return bestSpecialists;
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

            PlayerPtr player = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
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

    template <typename P>
        SpecialistTypes getBestSpecialist(const Player& player, YieldModifier yieldModifier, CommerceModifier commerceModifier, P valueF)
    {
        TotalOutput bestOutput;
        SpecialistTypes bestSpecialist = NO_SPECIALIST;

        CommerceModifier commercePercent = player.getCommercePercentages();

        std::vector<SpecialistTypes> bestSpecialists = getBestSpecialistHelper(player.getPlayerID(), valueF, yieldModifier, commerceModifier, commercePercent, 1);
        return (bestSpecialists.empty() ? NO_SPECIALIST : bestSpecialists[0]);
    }

    template <typename P>
        SpecialistTypes getBestSpecialist(const Player& player, P valueF)
    {
        TotalOutput bestOutput;
        SpecialistTypes bestSpecialist = NO_SPECIALIST;

        CommerceModifier commercePercent;
        for (int commerceType = 0; commerceType < NUM_COMMERCE_TYPES; ++commerceType)
        {
            commercePercent[commerceType] = player.getCvPlayer()->getCommercePercent((CommerceTypes)commerceType);
        }

        std::vector<SpecialistTypes> bestSpecialists = getBestSpecialistHelper(player.getPlayerID(), valueF, makeYield(100, 100, 100), makeCommerce(100, 100, 100, 100), commercePercent, 1);
        return (bestSpecialists.empty() ? NO_SPECIALIST : bestSpecialists[0]);
    }

    template <typename P>
        std::vector<SpecialistTypes> getBestSpecialists(const Player& player, YieldModifier yieldModifier, CommerceModifier commerceModifier, size_t count, P valueF)
    {
        TotalOutput bestOutput;
        SpecialistTypes bestSpecialist = NO_SPECIALIST;

        CommerceModifier commercePercent = player.getCommercePercentages();

        return getBestSpecialistHelper(player.getPlayerID(), valueF, yieldModifier, commerceModifier, commercePercent, count);
    }

    template SpecialistTypes getBestSpecialist<MixedTotalOutputOrderFunctor>(const Player&, YieldModifier, CommerceModifier, MixedTotalOutputOrderFunctor);
    template SpecialistTypes getBestSpecialist<MixedTotalOutputOrderFunctor>(const Player&, MixedTotalOutputOrderFunctor);

    template SpecialistTypes getBestSpecialist<MixedWeightedOutputOrderFunctor<TotalOutput> >(const Player&, YieldModifier, CommerceModifier, MixedWeightedOutputOrderFunctor<TotalOutput>);
    template SpecialistTypes getBestSpecialist<MixedWeightedOutputOrderFunctor<TotalOutput> >(const Player&, MixedWeightedOutputOrderFunctor<TotalOutput>);

    template std::vector<SpecialistTypes> 
        getBestSpecialists<MixedWeightedOutputOrderFunctor<TotalOutput> >(const Player&, YieldModifier, CommerceModifier, size_t, MixedWeightedOutputOrderFunctor<TotalOutput>);
}