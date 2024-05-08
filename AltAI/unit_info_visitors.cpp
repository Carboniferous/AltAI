#include "AltAI.h"

#include "./unit_info_visitors.h"
#include "./unit_info.h"
#include "./unit_info_streams.h"
#include "./civ_helper.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./city.h"
#include "./city_data.h"
#include "./specialist_helper.h"
#include "./iters.h"
#include "./helper_fns.h"
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

    void updateRequestData(CityData& data, SpecialistTypes specialistType)
    {
        data.getSpecialistHelper()->changeFreeSpecialistCount(specialistType, 1);
        data.recalcOutputs();
    }

    class CouldConstructUnitVisitor : public boost::static_visitor<bool>
    {
    public:
        CouldConstructUnitVisitor(const Player& player, int lookaheadDepth, bool ignoreRequiredResources, bool ignoreRequiredBuildings)
            : player_(player), pCity_(NULL), lookaheadDepth_(lookaheadDepth),
              ignoreRequiredResources_(ignoreRequiredResources), ignoreRequiredBuildings_(ignoreRequiredBuildings)
        {
            civHelper_ = player.getCivHelper();
            pAnalysis_ = player.getAnalysis();
        }

        CouldConstructUnitVisitor(const Player& player, const CvCity* pCity, int lookaheadDepth, bool ignoreRequiredResources, bool ignoreRequiredBuildings)
            : player_(player), pCity_(pCity), lookaheadDepth_(lookaheadDepth),
              ignoreRequiredResources_(ignoreRequiredResources), ignoreRequiredBuildings_(ignoreRequiredBuildings)
        {
            civHelper_ = player.getCivHelper();
            pAnalysis_ = player.getAnalysis();
        }

        template <typename T>
            bool operator() (const T&) const
        {
            return true;
        }

        bool operator() (const UnitInfo::ReligionNode& node) const
        {
            if (node.prereqReligion != NO_RELIGION)
            {
                if (pCity_)
                {
                    return pCity_->isHasReligion(node.prereqReligion);
                }

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

            return node.prereqReligion == NO_RELIGION;
        }

        bool operator() (const UnitInfo::CorporationNode& node) const
        {
            if (node.prereqCorporation != NO_CORPORATION)
            {
                if (pCity_)
                {
                    return pCity_->isHasCorporation(node.prereqCorporation);
                }

                CityIter cityIter(*player_.getCvPlayer());
                CvCity* pCity;

                while (pCity = cityIter())
                {
                    if (pCity->isHasCorporation(node.prereqCorporation))
                    {
                        return true;
                    }
                }
            }

            return node.prereqCorporation == NO_CORPORATION;
        }

        bool operator() (const UnitInfo::BaseNode& node) const
        {
#ifdef ALTAI_DEBUG
            //std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
            // includes great people
            if (node.cost < 1)
            {
#ifdef ALTAI_DEBUG
                //os << "\nskipping unit: " << gGlobals.getUnitInfo(node.unitType).getType() << " cost = " << node.cost;
#endif
                // do great people if city can produce the right type of great people points from specialists
                return false;
            }

            for (size_t i = 0, count = node.techTypes.size(); i < count; ++i)
            {
                // if we don't we have the tech and its depth is deeper than our lookaheadDepth, return false
                if (!civHelper_->hasTech(node.techTypes[i]) &&
                    (lookaheadDepth_ == 0 || pAnalysis_->getTechResearchDepth(node.techTypes[i]) > lookaheadDepth_))
                {
                    return false;
                }
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                if (!boost::apply_visitor(*this, node.nodes[i]))
                {
                    return false;
                }
            }

            if (node.specialUnitType != NO_SPECIALUNIT)
            {
                if (!gGlobals.getGame().isSpecialUnitValid(node.specialUnitType))
                {
                    return false;
                }
            }

            // todo - add religion and any other checks
            bool passedAreaCheck = node.minAreaSize < 0 && !node.requireAreaBonuses;
            bool passedBonusCheck = ignoreRequiredResources_ || (node.andBonusTypes.empty() && node.orBonusTypes.empty());
            bool passedBuildingCheck = ignoreRequiredBuildings_ || node.prereqBuildingType == NO_BUILDING;

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

            if (pCity_)
            {
                checkCity_(pCity_, node, passedAreaCheck, passedBonusCheck, passedBuildingCheck);
                return passedAreaCheck && passedBonusCheck && passedBuildingCheck;
            }
            else
            {
                CityIter cityIter(*player_.getCvPlayer());
                CvCity* pCity;

                while (pCity = cityIter())
                {
                    // have to pass all tests in one city - so copy flags to avoid setting some to true in one city and some in another
                    // which could lead to the wrong answer
                    bool cityPassedAreaCheck(passedAreaCheck), cityPassedBonusCheck(passedBonusCheck), cityPassedBuildingCheck(passedBuildingCheck);
                    checkCity_(pCity, node, cityPassedAreaCheck, cityPassedBonusCheck, cityPassedBuildingCheck);
                    if (cityPassedAreaCheck && cityPassedBonusCheck && cityPassedBuildingCheck)
                    {
                        return true;  // found a city we can build the unit
                    }
                }
                // if have no cities yet - still allow tactic to be created if pass required checks - since some are not city dependant
                // this is useful as it allow creation of some unit tactics based on initial techs, etc... so we can determine city guard units
                return passedAreaCheck && passedBonusCheck && passedBuildingCheck && player_.getCvPlayer()->getNumCities() == 0;
            }
        }

    private:
        void checkCity_(const CvCity* pCity, const UnitInfo::BaseNode& node, 
                        bool& passedAreaCheck, bool& passedBonusCheck, bool& passedBuildingCheck) const
        {
            if (!passedAreaCheck)
            {
                // assume ok, unless find otherwise - we know there is something to check or wouldn't be here
                passedAreaCheck = true;  
                if (node.domainType == DOMAIN_SEA)
                {
                    if (node.minAreaSize > 0 && !pCity->isCoastal(node.minAreaSize))
                    {
                        passedAreaCheck = false;
                    }

                    if (node.requireAreaBonuses)
                    {
                        // by definition, adjacent areas for a land city must be water
                        std::vector<const CvArea*> adjacentAreas = pCity->plot()->getAdjacentAreas();
                        bool foundBonuses = false;
                        for (size_t i = 0, count = adjacentAreas.size(); i < count; ++i)
                        {
                            // this is how the original AI works out if a city can build workboats
                            // arguably this is a cheat, since you could work out that an area has no resources
                            // by the inability to build a workboat. In practice, the existing AI doesn't exploit
                            // this information - and nor does AltAI - this code is just filtering builds that 
                            // would fail from a check to canTrain anyway.
                            foundBonuses = adjacentAreas[i]->getNumTotalBonuses() > 0;
                            if (foundBonuses)
                            {
                                break;
                            }
                        }
                        if (!foundBonuses)
                        {
                            passedAreaCheck = false;
                        }
                    }
                }
                else if (node.domainType == DOMAIN_LAND)
                {
                    if (pCity->area()->getNumTiles() >= node.minAreaSize)
                    {
                        passedAreaCheck = true;
                    }
                    // don't need to worry about requireAreaBonuses for land workers - since none of them are consumable
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

                    if (foundOrBonus)
                    {
                        passedBonusCheck = true;
                    }
                }
            }

            if (!passedBuildingCheck)
            {
                passedBuildingCheck = pCity->getNumBuilding(node.prereqBuildingType) > 0;
            }
        }

        const Player& player_;
        const CvCity* pCity_;
        int lookaheadDepth_;
        bool ignoreRequiredResources_;
        bool ignoreRequiredBuildings_;
        boost::shared_ptr<CivHelper> civHelper_;
        boost::shared_ptr<PlayerAnalysis> pAnalysis_;
    };

    bool couldConstructUnit(const Player& player, int lookaheadDepth, const boost::shared_ptr<UnitInfo>& pUnitInfo, bool ignoreRequiredResources, bool ignoreRequiredBuildings)
    {
        CouldConstructUnitVisitor visitor(player, lookaheadDepth, ignoreRequiredResources, ignoreRequiredBuildings);
        return boost::apply_visitor(visitor, pUnitInfo->getInfo());
    }

    bool couldConstructUnit(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<UnitInfo>& pUnitInfo, bool ignoreRequiredResources, bool ignoreRequiredBuildings)
    {
        CouldConstructUnitVisitor visitor(player, city.getCvCity(), lookaheadDepth, ignoreRequiredResources, ignoreRequiredBuildings);
        return boost::apply_visitor(visitor, pUnitInfo->getInfo());
    }

    bool couldEverConstructUnit(const Player& player, const City& city, const boost::shared_ptr<UnitInfo>& pUnitInfo, int lookAheadDepth)
    {
        CouldConstructUnitVisitor visitor(player, city.getCvCity(), lookAheadDepth, true, true);
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

    class SettledSpecialistsVisitor : public boost::static_visitor<>
    {
    public:
        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }
        }

        void operator() (const UnitInfo::MiscAbilityNode& node)
        {
            for (size_t i = 0, count = node.settledSpecialists.size(); i < count; ++i)
            {
                specialists_.push_back(node.settledSpecialists[i]);
            }
        }

        const std::vector<SpecialistTypes> & getSpecTypes() const
        {
            return specialists_;
        }

    private:
        std::vector<SpecialistTypes> specialists_;
    };

    std::vector<SpecialistTypes> getCityJoinSpecs(const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        SettledSpecialistsVisitor visitor;
        boost::apply_visitor(visitor, pUnitInfo->getInfo());

        return visitor.getSpecTypes();
    }

    class UpgradeVisitor : public boost::static_visitor<bool>
    {
    public:
        explicit UpgradeVisitor(const Player& player) : player_(player)
        {
        }

        template <typename T>
            bool operator() (const T&) const
        {
            return false;
        }

        bool operator() (const UnitInfo::BaseNode& node) const
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

        bool operator() (const UnitInfo::UpgradeNode& node) const
        {
            for (size_t i = 0, count = node.upgrades.size(); i < count; ++i)
            {
                UnitTypes upgradeUnit = getPlayerVersion(player_.getPlayerID(), node.upgrades[i]);
                if (upgradeUnit == NO_UNIT)  // can't upgrade
                {
                    return false;
                }

                if (!couldConstructUnit(player_, 0, player_.getAnalysis()->getUnitInfo(upgradeUnit), false, false))
                {
                    return false;
                }
            }
            return !node.upgrades.empty();  // can construct all upgrades, and there are some
        }

    private:
        const Player& player_;
    };

    bool isUnitObsolete(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        UpgradeVisitor visitor(player);
        return boost::apply_visitor(visitor, pUnitInfo->getInfo());
    }

    class UnitSpecialBuildingsVisitor : public boost::static_visitor<>
    {
    public:
        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }
        }

        void operator() (const UnitInfo::MiscAbilityNode& node)
        {
            for (size_t i = 0, count = node.specialBuildings.buildings.size(); i < count; ++i)
            {
                specialBuildings_.push_back(node.specialBuildings.buildings[i]);
            }
        }

        const std::vector<BuildingTypes> & getSpecialBuildings() const
        {
            return specialBuildings_;
        }

    private:
        std::vector<BuildingTypes> specialBuildings_;
    };

    std::vector<BuildingTypes> getUnitSpecialBuildings(const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        UnitSpecialBuildingsVisitor visitor;
        boost::apply_visitor(visitor, pUnitInfo->getInfo());

        return visitor.getSpecialBuildings();
    }
}
