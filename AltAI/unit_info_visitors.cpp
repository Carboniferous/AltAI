#include "./unit_info_visitors.h"
#include "./unit_info.h"
#include "./unit_info_streams.h"
#include "./civ_helper.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./city.h"
#include "./iters.h"
#include "./civ_log.h"

namespace AltAI
{
    boost::shared_ptr<UnitInfo> makeUnitInfo(UnitTypes unitType, PlayerTypes playerType)
    {
        return boost::shared_ptr<UnitInfo>(new UnitInfo(unitType, playerType));
    }

    void streamUnitInfo(std::ostream& os, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        os << pUnitInfo->getInfo();
    }

    std::vector<TechTypes> getRequiredTechs(const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        const UnitInfo::BaseNode& node = boost::get<UnitInfo::BaseNode>(pUnitInfo->getInfo());
        return node.techTypes;
    }

    class CouldConstructUnitVisitor : public boost::static_visitor<bool>
    {
    public:
        CouldConstructUnitVisitor(const Player& player, int lookaheadDepth) : player_(player), lookaheadDepth_(lookaheadDepth)
        {
            civHelper_ = player.getCivHelper();
            pAnalysis_ = player.getAnalysis();
        }

        template <typename T>
            bool operator() (const T&) const
        {
            return false;
        }

        bool operator() (const UnitInfo::ReligionNode& node) const
        {
            if (node.prereqReligion != NO_RELIGION)
            {
                CityIter cityIter(*player_.getCvPlayer());
                CvCity* pCity;

                while (pCity = cityIter())
                {
                    if (pCity->isHasReligion(node.prereqReligion))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        bool operator() (const UnitInfo::BaseNode& node) const
        {
#ifdef ALTAI_DEBUG
            //std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif

            for (size_t i = 0, count = node.techTypes.size(); i < count; ++i)
            {
                // if we don't we have the tech and its depth is deeper than our lookaheadDepth, return false
                if (!civHelper_->hasTech(node.techTypes[i]) &&
                    (lookaheadDepth_ == 0 ? true : pAnalysis_->getTechResearchDepth(node.techTypes[i]) > lookaheadDepth_))
                {
                    return false;
                }
            }

            // todo - add religion and any other checks
            bool passedAreaCheck = !(node.minAreaSize > -1), passedBonusCheck = node.andBonusTypes.empty() && node.orBonusTypes.empty();
            bool passedBuildingCheck = node.prereqBuildingType == NO_BUILDING;
            if (!passedBuildingCheck)
            {
                SpecialBuildingTypes specialBuildingType = (SpecialBuildingTypes)gGlobals.getBuildingInfo(node.prereqBuildingType).getSpecialBuildingType();
                if (specialBuildingType != NO_SPECIALBUILDING)
                {
                    passedBuildingCheck = player_.getCivHelper()->getSpecialBuildingNotRequiredCount(specialBuildingType) > 0;
                }
            }
#ifdef ALTAI_DEBUG
            //os << "\narea check = " << passedAreaCheck << ", bonus check = " << passedBonusCheck << ", building check = " << passedBuildingCheck;
#endif

            CityIter cityIter(*player_.getCvPlayer());
            CvCity* pCity;

            while (pCity = cityIter())
            {
                if (!passedAreaCheck)
                {
                    if (node.domainType == DOMAIN_SEA)
                    {
                        if (pCity->isCoastal(node.minAreaSize))
                        {
                            passedAreaCheck = true;
                        }
                    }
                    else if (node.domainType == DOMAIN_LAND)
                    {
                        if (pCity->area()->getNumTiles() >= node.minAreaSize)
                        {
                            passedAreaCheck = true;
                        }
                    }
                }

                if (!passedBonusCheck)
                {
                    bool foundAllAndBonuses = true, foundOrBonus = node.orBonusTypes.empty();
                    for (size_t i = 0, count = node.andBonusTypes.size(); i < count; ++i)
                    {
                        if (!pCity->hasBonus(node.andBonusTypes[i]))
                        {
                            foundAllAndBonuses = false;
                            break;
                        }
                    }

                    if (foundAllAndBonuses)
                    {
                        for (size_t i = 0, count = node.orBonusTypes.size(); i < count; ++i)
                        {
                            if (pCity->hasBonus(node.orBonusTypes[i]))
                            {
                                foundOrBonus = true;
                                break;
                            }
                        }
                    }

                    if (foundAllAndBonuses && foundOrBonus)
                    {
                        passedBonusCheck = true;
                    }
                }

                if (!passedBuildingCheck)
                {
                    passedBuildingCheck = pCity->getNumBuilding(node.prereqBuildingType) > 0;
                }
            }
            return passedAreaCheck && passedBonusCheck && passedBuildingCheck;
        }

    private:
        const Player& player_;
        int lookaheadDepth_;
        boost::shared_ptr<CivHelper> civHelper_;
        boost::shared_ptr<PlayerAnalysis> pAnalysis_;
    };

    bool couldConstructUnit(const Player& player, int lookaheadDepth, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        CouldConstructUnitVisitor visitor(player, lookaheadDepth);
        return boost::apply_visitor(visitor, pUnitInfo->getInfo());
    }

    class PromotionDepthVisitor : public boost::static_visitor<int>
    {
    public:
        PromotionDepthVisitor(const Player& player, PromotionTypes promotionType, const std::set<PromotionTypes>& existingPromotions)
            : player_(player), existingPromotions_(existingPromotions)
        {
            promotionTypes_.push(promotionType);
            civHelper_ = player.getCivHelper();
        }

        template <typename T>
            result_type operator() (const T&)
        {
            return result_type();
        }

        int operator() (const UnitInfo::PromotionsNode& node)
        {
            std::set<UnitInfo::PromotionsNode::Promotion>::const_iterator ci = node.promotions.find(UnitInfo::PromotionsNode::Promotion(promotionTypes_.top()));

            if (ci != node.promotions.end())
            {
                if (ci->level == 0)
                {
                    return 0;
                }

                if (ci->techType != NO_TECH && !civHelper_->hasTech(ci->techType))
                {
                    return -1;
                }

                int depth = 0;

                bool needAndPromotion = ci->andPromotion != NO_PROMOTION && existingPromotions_.find(ci->andPromotion) == existingPromotions_.end();
                if (needAndPromotion)
                {
                    int andDepth = visit_(node, ci->andPromotion);

                    if (andDepth < 0)  // unreachable promotion
                    {
                        return -1;
                    }
                    depth += andDepth;
                }

                bool needOrPromotion1 = ci->orPromotion1 != NO_PROMOTION && existingPromotions_.find(ci->orPromotion1) == existingPromotions_.end();
                bool needOrPromotion2 = ci->orPromotion2 != NO_PROMOTION && existingPromotions_.find(ci->orPromotion2) == existingPromotions_.end();

                if (!needOrPromotion1 && !needOrPromotion2)
                {
                    return depth;
                }

                int minOrDepth = MAX_INT;
                PromotionTypes bestOrPromotion = NO_PROMOTION;
                if (needOrPromotion1)
                {
                    int thisOrDepth = visit_(node, ci->orPromotion1);
                    if (thisOrDepth > 0)
                    {
                        minOrDepth = thisOrDepth;
                        bestOrPromotion = ci->orPromotion1;
                    }
                }
                if (needOrPromotion2)
                {
                    int thisOrDepth = visit_(node, ci->orPromotion2);
                    if (thisOrDepth > 0)
                    {
                        minOrDepth = thisOrDepth;
                        bestOrPromotion = ci->orPromotion2;
                    }
                }

                if (minOrDepth > 0 && minOrDepth < MAX_INT)
                {
                    depth += minOrDepth;
                }
                else
                {
                    return -1;
                }

                return depth;
            }
            return -1;
        }

    private:

        int visit_(const UnitInfo::PromotionsNode& node, PromotionTypes promotionType)
        {
            promotionTypes_.push(promotionType);
            int result = (*this)(node);
            promotionTypes_.pop();
            return result;
        }

        const Player& player_;
        boost::shared_ptr<CivHelper> civHelper_;
        std::stack<PromotionTypes> promotionTypes_;
        std::set<PromotionTypes> existingPromotions_;
    };

    typedef std::list<PromotionTypes> PromotionsList;

    class CanGainPromotionsVisitor : public boost::static_visitor<bool>
    {
    public:
        CanGainPromotionsVisitor(const Player& player, PromotionTypes promotionType, const std::set<PromotionTypes>& existingPromotions)
            : player_(player), promotionType_(promotionType), existingPromotions_(existingPromotions), promotionCount_(0)
        {
            civHelper_ = player.getCivHelper();
        }

        template <typename T>
            bool operator() (const T&)
        {
            return false;
        }

        bool operator() (const UnitInfo::PromotionsNode& node)
        {
            std::set<UnitInfo::PromotionsNode::Promotion>::const_iterator ci = node.promotions.find(UnitInfo::PromotionsNode::Promotion(promotionType_));

            // can get this promotion for this unit type
            if (ci != node.promotions.end())
            {
                // free promotion
                if (ci->level == 0)
                {
                    return true;
                }

                // check for tech
                if (ci->techType != NO_TECH && !civHelper_->hasTech(ci->techType))
                {
                    return false;
                }

                bool needAndPromotion = ci->andPromotion != NO_PROMOTION && existingPromotions_.find(ci->andPromotion) == existingPromotions_.end();
                if (needAndPromotion)
                {
                    promotionType_ = ci->andPromotion;
                    if ((*this)(node))
                    {
                        existingPromotions_.insert(ci->andPromotion);
                        ++promotionCount_;
                    }
                }

                // or promotions...
                bool needOrPromotion1 = ci->orPromotion1 != NO_PROMOTION && existingPromotions_.find(ci->orPromotion1) == existingPromotions_.end();
                bool needOrPromotion2 = ci->orPromotion2 != NO_PROMOTION && existingPromotions_.find(ci->orPromotion2) == existingPromotions_.end();

                if (!needOrPromotion1 && !needOrPromotion2)
                {
                    ++promotionCount_;
                    return true;
                }

                PromotionTypes bestOrPromotion = NO_PROMOTION;
                int minOrDepth = MAX_INT;

                if (needOrPromotion1)
                {
                    PromotionDepthVisitor depthVisitor(player_, ci->orPromotion1, existingPromotions_);
                    int depth = depthVisitor(node);
                    if (depth < minOrDepth)
                    {
                        minOrDepth = depth;
                        bestOrPromotion = ci->orPromotion1;
                    }
                }
                if (needOrPromotion2)
                {
                    PromotionDepthVisitor depthVisitor(player_, ci->orPromotion2, existingPromotions_);
                    int depth = depthVisitor(node);
                    if (depth < minOrDepth)
                    {
                        minOrDepth = depth;
                        bestOrPromotion = ci->orPromotion2;
                    }
                }

                if (bestOrPromotion != NO_PROMOTION)
                {
                    promotionType_ = bestOrPromotion;
                    if ((*this)(node))
                    {
                        existingPromotions_.insert(bestOrPromotion);
                        ++promotionCount_;
                        return true;
                    }
                }
            }
            return false;
        }

        bool operator() (const UnitInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                if (boost::apply_visitor(*this, node.nodes[i]))
                {
                    return true;
                }
            }

            return false;
        }

        const std::set<PromotionTypes>& getPromotions() const
        {
            return existingPromotions_;
        }

        int getPromotionCount() const
        {
            return promotionCount_;
        }

    private:
        const Player& player_;
        PromotionTypes promotionType_;
        std::set<PromotionTypes> existingPromotions_;
        int promotionCount_;
        boost::shared_ptr<CivHelper> civHelper_;
    };

    class FreePromotionVisitor : public boost::static_visitor<>
    {
    public:
        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::PromotionsNode& node)
        {
            for (std::set<UnitInfo::PromotionsNode::Promotion>::const_iterator ci(node.promotions.begin()), ciEnd(node.promotions.end()); ci != ciEnd; ++ci)
            {
                if (ci->level == 0)
                {
                    freePromotions.insert(ci->promotionType);
                }
            }
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }
        }

        const Promotions& getFreePromotions() const
        {
            return freePromotions;
        }

    private:
        Promotions freePromotions;
    };

    boost::tuple<bool, int, std::set<PromotionTypes> > canGainPromotion(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo, PromotionTypes promotionType,
        const std::set<PromotionTypes>& existingPromotions)
    {
        CanGainPromotionsVisitor visitor(player, promotionType, existingPromotions);
        bool result = boost::apply_visitor(visitor, pUnitInfo->getInfo());
        const std::set<PromotionTypes>& promotions = visitor.getPromotions();

        // debug
        /*{
            const std::set<PromotionTypes>& promotions = visitor.getPromotions();

            std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
            os << "\nPromotion: " << gGlobals.getPromotionInfo(promotionType).getType() << " requires: ";
            for (std::set<PromotionTypes>::const_iterator ci(promotions.begin()), ciEnd(promotions.end()); ci != ciEnd; ++ci)
            {
                os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
            }
        }*/

        return boost::make_tuple(result, visitor.getPromotionCount(), promotions);
    }

    Promotions getFreePromotions(const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        FreePromotionVisitor visitor;
        boost::apply_visitor(visitor, pUnitInfo->getInfo());

        return visitor.getFreePromotions();
    }
}
