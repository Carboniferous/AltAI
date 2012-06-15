#include "./unit_analysis.h"
#include "./player_analysis.h"
#include "./unit_info_visitors.h"
#include "./player.h"
#include "./city.h"
#include "./helper_fns.h"
#include "./civ_log.h"
#include "./unit_info.h"

namespace AltAI
{
    namespace
    {
        struct UnitData
        {
            enum Flags
            {
                None = 0, CityAttack = (1 << 0)
            };

            explicit UnitData(const CvUnitInfo& unitInfo_) :
                unitInfo(unitInfo_),
                combat(100 * unitInfo.getCombat()), 
                firstStrikes(unitInfo.getFirstStrikes()),
                chanceFirstStrikes(unitInfo.getChanceFirstStrikes()),
                firePower(unitInfo.getCombat()),
                combatLimit(unitInfo.getCombatLimit()),
                cityAttackPercent(unitInfo.getCityAttackModifier()), cityDefencePercent(unitInfo.getCityDefenseModifier()),
                immuneToFirstStrikes(unitInfo.isFirstStrikeImmune())
            {
            }

            void applyPromotion(const CvPromotionInfo& promotion)
            {
                combat += unitInfo.getCombat() * promotion.getCombatPercent();
                firstStrikes += promotion.getFirstStrikesChange();
                chanceFirstStrikes += promotion.getChanceFirstStrikesChange();
                if (promotion.isImmuneToFirstStrikes())
                {
                    immuneToFirstStrikes = true;
                }

                cityAttackPercent += promotion.getCityAttackPercent();
                cityDefencePercent += promotion.getCityDefensePercent();

                for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
                {
                    if (promotion.getUnitCombatModifierPercent(i) != 0)
                    {
                        unitCombatModifiers[(UnitCombatTypes)i] += promotion.getUnitCombatModifierPercent(i);
                    }
                }
            }

			// calculate our strength as defender
            int calculateStrength(const UnitData& other, int flags = 0) const
            {
				// unit class type modifiers - add defence, subtract the other unit's attack modifier
                int modifier = unitInfo.getUnitClassDefenseModifier(other.unitInfo.getUnitClassType());
                modifier -= other.unitInfo.getUnitClassAttackModifier(unitInfo.getUnitClassType());
				// unit type modifiers - add defence, subtract other's attack bonus
                modifier += unitInfo.getUnitCombatModifier(other.unitInfo.getUnitCombatType());
                modifier -= other.unitInfo.getUnitCombatModifier(unitInfo.getUnitCombatType());

                std::map<UnitCombatTypes, int>::const_iterator ci = unitCombatModifiers.find((UnitCombatTypes)other.unitInfo.getUnitCombatType());
                if (ci != unitCombatModifiers.end())
                {
                    modifier += ci->second;
                }

                ci = other.unitCombatModifiers.find((UnitCombatTypes)unitInfo.getUnitCombatType());
                if (ci != other.unitCombatModifiers.end())
                {
                    modifier -= ci->second;
                }

				// if attacker is attacking a city (so we are the defender)
                if (flags & CityAttack)
                {
					// subtract attacker's city attack bonus and add our city defence bonus
                    modifier -= other.cityAttackPercent;
                    modifier += cityDefencePercent;
                }

                int strength = combat;
                if (modifier > 0)
	            {
		            strength = strength * (modifier + 100);
	            }
	            else
	            {
		            strength = (strength * 10000) / (100 - modifier);
	            }
                return strength / 100;
            }

            // calculate our strength as attacker
            int calculateStrength(int flags) const
            {
                return combat;
            }

            const CvUnitInfo& unitInfo;

            int combat, firstStrikes, chanceFirstStrikes, firePower, combatLimit;
            int cityAttackPercent, cityDefencePercent;
            bool immuneToFirstStrikes;
            std::map<UnitCombatTypes, int> unitCombatModifiers;
        };

        int getCombatOdds(const UnitData& attacker, const UnitData& defender, int flags = 0)
        {
            const int attHP = gGlobals.getMAX_HIT_POINTS();
            const int defHP = attHP;
            const int attMaxHP = gGlobals.getMAX_HIT_POINTS();
            const int defMaxHP = attMaxHP;

            const int attackerStrength = attacker.calculateStrength(flags);
            const int defenderStrength = defender.calculateStrength(attacker, flags);

            return ::getCombatOdds(attackerStrength, attacker.firstStrikes, attacker.chanceFirstStrikes, attacker.immuneToFirstStrikes,
                attacker.firePower, attHP, attMaxHP, attacker.combatLimit,
                defenderStrength, defender.firstStrikes, defender.chanceFirstStrikes, defender.immuneToFirstStrikes,
                defender.firePower, defHP, defMaxHP);
        }

        void debugPromotion(std::ostream& os, PromotionTypes promotionType, const Promotions& requiredPromotions, int level)
        {
#ifdef ALTAI_DEBUG
            os << "\nPromotion: " << gGlobals.getPromotionInfo(promotionType).getType() << " level = " << level;
            for (Promotions::const_iterator ci(requiredPromotions.begin()), ciEnd(requiredPromotions.end()); ci != ciEnd; ++ci)
            {
                os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
            }
#endif
        }

        void debugPromotions(std::ostream& os, const UnitAnalysis::PromotionsMap& promotions, const std::string& descriptor)
        {
#ifdef ALTAI_DEBUG
            os << "\n" << descriptor << ":";
            for (UnitAnalysis::PromotionsMap::const_iterator ci(promotions.begin()), ciEnd(promotions.end()); ci != ciEnd; ++ci)
            {
                os << gGlobals.getPromotionInfo(ci->second).getType() << " = " << ci->first << " ";
            }
#endif
        }

        void debugUnits(std::ostream& os, const UnitAnalysis::UnitLevels& unitLevels, const std::string& descriptor)
        {
#ifdef ALTAI_DEBUG
            os << "\n" << descriptor<< ": ";
            for (size_t i = 0, count = unitLevels.size(); i < count; ++i)
            {
                os << "\nlevel = " << i;
                for (UnitAnalysis::UnitValuesMap::const_iterator ci(unitLevels[i].begin()), ciEnd(unitLevels[i].end()); ci != ciEnd; ++ci)
                {
                    os << "\n\t" << gGlobals.getUnitInfo(ci->second.first).getType() << " = " << ci->first << ", promotions = ";
                    for (Promotions::const_iterator pi(ci->second.second.second.begin()), piEnd(ci->second.second.second.end()); pi != piEnd; ++pi)
                    {
                        os << gGlobals.getPromotionInfo(*pi).getType() << ", ";
                    }
					os << " (remaining levels = " << ci->second.second.first << ")";
                }
            }
#endif
        }

        void combinePromotions(Promotions& combined, const Promotions& additional)
        {
            combined.insert(additional.begin(), additional.end());
        }

        struct PromotionValueFunctor
        {
            explicit PromotionValueFunctor(UnitAnalysis::CvPromotionInfoIntFnPtr funcPtr_) : funcPtr(funcPtr_)
            {
            }

            int operator() (PromotionTypes promotionType) const
            {
                return ((gGlobals.getPromotionInfo(promotionType)).*funcPtr)();
            }

            UnitAnalysis::CvPromotionInfoIntFnPtr funcPtr;
        };

        struct UnitCombatPromotionValueFunctor
        {
            UnitCombatPromotionValueFunctor(UnitAnalysis::CvPromotionInfoUnitCombatIntFnPtr funcPtr_, UnitCombatTypes unitCombatType_)
                : unitCombatType(unitCombatType_), funcPtr(funcPtr_)
            {
            }

            int operator() (PromotionTypes promotionType) const
            {
                return ((gGlobals.getPromotionInfo(promotionType)).*funcPtr)(unitCombatType);
            }

            UnitCombatTypes unitCombatType;
            UnitAnalysis::CvPromotionInfoUnitCombatIntFnPtr funcPtr;
        };

        struct FirstStrikesPromotionValueFunctor
        {
            FirstStrikesPromotionValueFunctor()
            {
            }

            int operator() (PromotionTypes promotionType) const
            {
                const CvPromotionInfo& promotionInfo = gGlobals.getPromotionInfo(promotionType);
                return 2 * promotionInfo.getFirstStrikesChange() + promotionInfo.getChanceFirstStrikesChange();
            }
        };
    }

    UnitAnalysis::UnitAnalysis(const Player& player) : player_(player)
    {
    }

    void UnitAnalysis::init()
    {
		analysePromotions_();
        analyseUnits_();

        const int attHP = gGlobals.getMAX_HIT_POINTS();
        const int defHP = attHP;
        const int attMaxHP = gGlobals.getMAX_HIT_POINTS();
        const int defMaxHP = attMaxHP;

        for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
        {
            PlayerTypes playerType = player_.getPlayerID();
            UnitTypes unitType = getPlayerVersion(playerType, (UnitClassTypes)i);
            if (unitType != NO_UNIT)
            {
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

                if (unitInfo.getProductionCost() >= 0 && unitInfo.getCombat() > 0)
                {
                    analyseAsCombatUnit_(unitType);
                    analyseAsCounterUnit_(unitType);

					if (unitInfo.getUnitAIType(UNITAI_ATTACK_CITY) || unitInfo.getUnitAIType(UNITAI_ATTACK))
					{
						analyseAsCityAttackUnit_(unitType);
					}
                    
                    int attBaseCombat = 100 * unitInfo.getCombat();
                    int attFS = unitInfo.getFirstStrikes();
                    int attChanceFS = unitInfo.getChanceFirstStrikes();
                    bool attImmuneToFS = unitInfo.isFirstStrikeImmune();
                    int attFirePower = unitInfo.getCombat();
                    int attCombatLimit = unitInfo.getCombatLimit();

                    for (int j = 0, count = gGlobals.getNumUnitClassInfos(); j < count; ++j)
                    {
                        UnitTypes defaultUnit = (UnitTypes)gGlobals.getUnitClassInfo((UnitClassTypes)j).getDefaultUnitIndex();

                        const CvUnitInfo& otherUnitInfo = gGlobals.getUnitInfo(defaultUnit);

                        int defBaseCombat = 100 * otherUnitInfo.getCombat();
                        int defCombatLimit = otherUnitInfo.getCombatLimit();

                        if (otherUnitInfo.getProductionCost() >= 0 && defBaseCombat > 0 && unitInfo.getDomainType() == otherUnitInfo.getDomainType())
                        {
                            int defFS = otherUnitInfo.getFirstStrikes();
                            int defChanceFS = otherUnitInfo.getChanceFirstStrikes();
                            bool defImmuneToFS = otherUnitInfo.isFirstStrikeImmune();
                            int defFirePower = otherUnitInfo.getCombat();

                            CombatData combatData;

                            // attack odds
                            {
                                int modifier = otherUnitInfo.getUnitClassDefenseModifier(i);
                                modifier -= unitInfo.getUnitClassAttackModifier(j);
                                modifier += otherUnitInfo.getUnitCombatModifier(unitInfo.getUnitCombatType());
                                modifier -= unitInfo.getUnitCombatModifier(otherUnitInfo.getUnitCombatType());

                                int defCombat = defBaseCombat / 100;
                                if (modifier > 0)
	                            {
		                            defCombat = defCombat * (modifier + 100);
	                            }
	                            else
	                            {
		                            defCombat = (defCombat * 10000) / (100 - modifier);
	                            }

                                combatData.attackOdds = ::getCombatOdds(attBaseCombat, attFS, attChanceFS, attImmuneToFS, attFirePower, attHP, attMaxHP, attCombatLimit,
                                    defCombat, defFS, defChanceFS, defImmuneToFS, defFirePower, defHP, defMaxHP);
                            }

                            // defence odds
                            {
                                int modifier = unitInfo.getUnitClassDefenseModifier(j);
                                modifier -= otherUnitInfo.getUnitClassAttackModifier(i);
                                modifier += unitInfo.getUnitCombatModifier(otherUnitInfo.getUnitCombatType());
                                modifier -= otherUnitInfo.getUnitCombatModifier(unitInfo.getUnitCombatType());

                                int attCombat = attBaseCombat / 100;
                                if (modifier > 0)
	                            {
		                            attCombat = attCombat * (modifier + 100);
	                            }
	                            else
	                            {
		                            attCombat = (attCombat * 10000) / (100 - modifier);
	                            }

                                // temp
                                combatData.cityAttackOdds = defBaseCombat;
                                combatData.cityDefenceOdds = attCombat;

                                combatData.defenceOdds = 1000 - ::getCombatOdds(defBaseCombat, defFS, defChanceFS, defImmuneToFS, defFirePower, defHP, defMaxHP, defCombatLimit,
                                    attCombat, attFS, attChanceFS, attImmuneToFS, attFirePower, attHP, attMaxHP);
                            }

                            if (combatData.attackOdds > 0 || combatData.defenceOdds > 0)
                            {
                                combatDataMap_[unitType][defaultUnit] = combatData;
                            }
                        }
                    }
                }
            }
        }

        
    }

    std::pair<int, int> UnitAnalysis::getOdds(const CvUnitInfo& unit1Info, const CvUnitInfo& unit2Info, int unit1Level, int unit2Level) const
    {
        const int attHP = gGlobals.getMAX_HIT_POINTS();
        const int defHP = attHP;
        const int attMaxHP = gGlobals.getMAX_HIT_POINTS();
        const int defMaxHP = attMaxHP;

        int attackOdds = 0, defenceOdds = 0;

        UnitData unit1(unit1Info), unit2(unit2Info);

        // todo - handle all air combat separately
        if (unit2Info.getProductionCost() >= 0 && unit2.combat > 0 && unit1Info.getDomainType() == unit2Info.getDomainType())
        {
            //Promotions combatPromotions = getCombatPromotions(unit1.getUnitClassType, unit1Level);

            attackOdds = getCombatOdds(unit1, unit2);
            defenceOdds = 1000 - getCombatOdds(unit2, unit1);
        }

        return std::make_pair(attackOdds, defenceOdds);
    }

    int UnitAnalysis::getCurrentUnitValue(UnitTypes unitType) const
    {
        int value = 0, maxValue = 0;
        CombatDataMap::const_iterator ci = combatDataMap_.find(unitType);
        if (ci != combatDataMap_.end())
        {
            for (UnitCombatDataMap::const_iterator iter = ci->second.begin(), iterEnd = ci->second.end(); iter != iterEnd; ++iter)
            {
                if (iter->second.attackOdds > 200)
                {
                    value += iter->second.attackOdds;
                    maxValue += 1000;
                }

                if (iter->second.defenceOdds > 200)
                {
                    value += iter->second.defenceOdds;
                    maxValue += 1000;
                }                
            }
        }

        return (100 * value) / std::max<int>(1, maxValue);
    }

    int UnitAnalysis::getCityAttackUnitValue(UnitTypes unitType) const
    {
        int value = 0, maxValue = 0;
        UnitCombatInfoMap::const_iterator ci = cityAttackUnitValues_.find(unitType);
        if (ci != cityAttackUnitValues_.end())
        {
            UnitCombatLevelsMap::const_iterator levelsIter = ci->second.find(1);

            if (levelsIter != ci->second.end())
            {
                for (UnitCombatOddsMap::const_iterator AIIter(levelsIter->second.begin()), AIEndIter(levelsIter->second.end()); AIIter != AIEndIter; ++AIIter)
                {
                    for (UnitAITypesCombatOddsMap::const_iterator oddsIter(AIIter->second.begin()), oddsEndIter(AIIter->second.end()); oddsIter != oddsEndIter; ++oddsIter)
                    {
                        value += oddsIter->second.first;
                        maxValue += 1000;
                    }
                }
            }
        }

        int finalValue = (100 * value) / std::max<int>(1, maxValue);
        return finalValue == 0 && value > 0 ? 1 : finalValue;
    }

    int UnitAnalysis::getCityDefenceUnitValue(UnitTypes unitType) const
    {
        int value = 0, maxValue = 0;
        UnitCombatInfoMap::const_iterator ci = cityDefenceUnitValues_.find(unitType);
        if (ci != cityDefenceUnitValues_.end())
        {
            UnitCombatLevelsMap::const_iterator levelsIter = ci->second.find(1);

            if (levelsIter != ci->second.end())
            {
                for (UnitCombatOddsMap::const_iterator AIIter(levelsIter->second.begin()), AIEndIter(levelsIter->second.end()); AIIter != AIEndIter; ++AIIter)
                {
                    for (UnitAITypesCombatOddsMap::const_iterator oddsIter(AIIter->second.begin()), oddsEndIter(AIIter->second.end()); oddsIter != oddsEndIter; ++oddsIter)
                    {
                        value += oddsIter->second.first;
                        maxValue += 1000;
                    }
                }
            }
        }

        int finalValue = (100 * value) / std::max<int>(1, maxValue);
        return finalValue == 0 && value > 0 ? 1 : finalValue;
    }

    int UnitAnalysis::getAttackUnitValue(UnitTypes unitType) const
    {
        int value = 0, maxValue = 0;
        UnitCombatInfoMap::const_iterator ci = attackUnitValues_.find(unitType);
        if (ci != attackUnitValues_.end())
        {
            UnitCombatLevelsMap::const_iterator levelsIter = ci->second.find(1);

            if (levelsIter != ci->second.end())
            {
                for (UnitCombatOddsMap::const_iterator AIIter(levelsIter->second.begin()), AIEndIter(levelsIter->second.end()); AIIter != AIEndIter; ++AIIter)
                {
                    for (UnitAITypesCombatOddsMap::const_iterator oddsIter(AIIter->second.begin()), oddsEndIter(AIIter->second.end()); oddsIter != oddsEndIter; ++oddsIter)
                    {
                        value += oddsIter->second.first;
                        maxValue += 1000;
                    }
                }
            }
        }

        int finalValue = (100 * value) / std::max<int>(1, maxValue);
        return finalValue == 0 && value > 0 ? 1 : finalValue;
    }

    int UnitAnalysis::getDefenceUnitValue(UnitTypes unitType) const
    {
        int value = 0, maxValue = 0;
        UnitCombatInfoMap::const_iterator ci = defenceUnitValues_.find(unitType);
        if (ci != defenceUnitValues_.end())
        {
            UnitCombatLevelsMap::const_iterator levelsIter = ci->second.find(1);

            if (levelsIter != ci->second.end())
            {
                for (UnitCombatOddsMap::const_iterator AIIter(levelsIter->second.begin()), AIEndIter(levelsIter->second.end()); AIIter != AIEndIter; ++AIIter)
                {
                    for (UnitAITypesCombatOddsMap::const_iterator oddsIter(AIIter->second.begin()), oddsEndIter(AIIter->second.end()); oddsIter != oddsEndIter; ++oddsIter)
                    {
                        value += oddsIter->second.first;
                        maxValue += 1000;
                    }
                }
            }
        }

        int finalValue = (100 * value) / std::max<int>(1, maxValue);
        return finalValue == 0 && value > 0 ? 1 : finalValue;
    }

    int UnitAnalysis::getUnitCounterValue(UnitTypes unitType, UnitCombatTypes unitCombatType) const
    {
        int value = 0, maxValue = 0;
        UnitCombatInfoMap::const_iterator ci = attackUnitCounterValues_.find(unitType);
        if (ci != attackUnitCounterValues_.end())
        {
            UnitCombatLevelsMap::const_iterator levelsIter = ci->second.find(1);

            if (levelsIter != ci->second.end())
            {
                for (UnitCombatOddsMap::const_iterator AIIter(levelsIter->second.begin()), AIEndIter(levelsIter->second.end()); AIIter != AIEndIter; ++AIIter)
                {
                    if (gGlobals.getUnitInfo(AIIter->first).getUnitCombatType() == unitCombatType)
                    {
                        for (UnitAITypesCombatOddsMap::const_iterator oddsIter(AIIter->second.begin()), oddsEndIter(AIIter->second.end()); oddsIter != oddsEndIter; ++oddsIter)
                        {
                            value += oddsIter->second.first;
                            maxValue += 1000;
                        }
                    }
                }
            }
        }

        
        int finalValue = (100 * value) / std::max<int>(1, maxValue);
        return finalValue == 0 && value > 0 ? 1 : finalValue;
    }

    UnitAnalysis::RemainingLevelsAndPromotions UnitAnalysis::getCityAttackPromotions(UnitTypes unitType, int level) const
    {
        level = std::max<int>(0, level);
        level = std::min<int>(level, (int)cityAttackUnits_.size());

        for (UnitValuesMap::const_iterator ci(cityAttackUnits_[level].begin()), ciEnd(cityAttackUnits_[level].end()); ci != ciEnd; ++ci)
        {
            if (ci->second.first == unitType)
            {
                return ci->second.second;
            }
        }
        return std::make_pair(level, Promotions());
    }

	UnitAnalysis::RemainingLevelsAndPromotions UnitAnalysis::getCityDefencePromotions(UnitTypes unitType, int level) const
    {
        level = std::max<int>(0, level);
		level = std::min<int>(level, (int)cityDefenceUnits_.size());

        for (UnitValuesMap::const_iterator ci(cityDefenceUnits_[level].begin()), ciEnd(cityDefenceUnits_[level].end()); ci != ciEnd; ++ci)
        {
            if (ci->second.first == unitType)
            {
                return ci->second.second;
            }
        }
        return std::make_pair(level, Promotions());
    }

    UnitAnalysis::RemainingLevelsAndPromotions UnitAnalysis::getCombatPromotions(UnitTypes unitType, int level) const
    {
        level = std::max<int>(0, level);
        level = std::min<int>(level, (int)combatUnits_.size());

        for (UnitValuesMap::const_iterator ci(combatUnits_[level].begin()), ciEnd(combatUnits_[level].end()); ci != ciEnd; ++ci)
        {
            if (ci->second.first == unitType)
            {
                return ci->second.second;
            }
        }
        return std::make_pair(level, Promotions());
    }

    UnitAnalysis::RemainingLevelsAndPromotions UnitAnalysis::getCombatCounterPromotions(UnitTypes unitType, UnitCombatTypes unitCombatType, int level) const
    {
        std::map<UnitCombatTypes, UnitLevels>::const_iterator combatTypesIter = unitCounterUnits_.find(unitCombatType);
        if (combatTypesIter != unitCounterUnits_.end())
        {
            level = std::max<int>(0, level);
            level = std::min<int>(level, (int)combatTypesIter->second.size());

            for (UnitValuesMap::const_iterator ci(combatTypesIter->second[level].begin()), ciEnd(combatTypesIter->second[level].end()); ci != ciEnd; ++ci)
            {
                if (ci->second.first == unitType)
                {
                    return ci->second.second;
                }
            }
        }
        return std::make_pair(level, Promotions());
    }

    void UnitAnalysis::analysePromotions_()
    {
        for (int i = 0, count = gGlobals.getNumPromotionInfos(); i < count; ++i)
        {
            const CvPromotionInfo& promotionInfo = gGlobals.getPromotionInfo((PromotionTypes)i);

            if (!promotionInfo.isLeader())
            {
                int cityAttackPercent = promotionInfo.getCityAttackPercent();
                if (cityAttackPercent > 0)
                {
                    cityAttackPromotions_.insert(std::make_pair(cityAttackPercent, (PromotionTypes)i));
                }

                int cityDefencePercent = promotionInfo.getCityDefensePercent();
                if (promotionInfo.getCityDefensePercent() > 0)
                {
                    cityDefencePromotions_.insert(std::make_pair(cityDefencePercent, (PromotionTypes)i));
                }

                int combatPercent = promotionInfo.getCombatPercent();

                if (combatPercent > 0)
                {
                    combatPromotions_.insert(std::make_pair(combatPercent, (PromotionTypes)i));
                }

                int nFirstStrikes = promotionInfo.getFirstStrikesChange();
                int nFirstStrikeChances = promotionInfo.getChanceFirstStrikesChange();
                if (nFirstStrikes + nFirstStrikeChances > 0)
                {
                    firstStrikePromotions_.insert(std::make_pair(2 * nFirstStrikes + nFirstStrikeChances, (PromotionTypes)i));
                }

                for (int j = 0, count = gGlobals.getNumUnitCombatInfos(); j < count; ++j)
                {
                    int unitCombatModifierPercent = promotionInfo.getUnitCombatModifierPercent(j);
                    if (unitCombatModifierPercent > 0)
                    {
                        unitCounterPromotionsMap_[(UnitCombatTypes)j].insert(std::make_pair(unitCombatModifierPercent, (PromotionTypes)i));
                    }
                }

                int moves = promotionInfo.getMovesChange();
                if (moves > 0)
                {
                    movementPromotions_.insert(std::make_pair(moves, (PromotionTypes)i));
                }
            }
        }
    }

    template <typename ValueF>
        std::pair<int, UnitAnalysis::RemainingLevelsAndPromotions> UnitAnalysis::calculateBestPromotions_(const UnitAnalysis::PromotionsMap& promotionsMap, int baseValue,
            const boost::shared_ptr<UnitInfo>& pUnitInfo, ValueF valueF, int level, const Promotions& existingPromotions) const
    {
        // debug
        //std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();

        Promotions bestPromotions(existingPromotions);
        int currentValue = baseValue;
        int bestValue = baseValue;
        int currentLevel = level;

        while (currentLevel >= 0)
        {
            Promotions newPromotions;
            int bestPromotionLevelCost = 0;
                        
            for (PromotionsMap::const_iterator ci(promotionsMap.begin()), ciEnd(promotionsMap.end()); ci != ciEnd; ++ci)
            {
                if (bestPromotions.find(ci->second) != bestPromotions.end())
                {
                    continue;
                }

                Promotions requiredPromotions;
                bool available = false;
                int levels = 0;
#ifdef ALTAI_DEBUG
                //os << "\nChecking for promotion: " << gGlobals.getPromotionInfo(ci->second).getType() << " for level = " << currentLevel;
#endif
                boost::tie(available, levels, requiredPromotions) = canGainPromotion(player_, pUnitInfo, ci->second, bestPromotions);
#ifdef ALTAI_DEBUG
                //os << " available = " << (available ? "true" : "false") << " levels = " << levels;
#endif
                if (available && levels <= currentLevel)
                {
                    int finalValue = currentValue;
                    Promotions::iterator pi(requiredPromotions.begin()), piEnd(requiredPromotions.end());
                    while (pi != piEnd)
                    {
                        if (bestPromotions.find(*pi) != bestPromotions.end())
                        {
                            requiredPromotions.erase(pi++);
                        }
                        else
                        {
                            finalValue += valueF(*pi);
                            ++pi;
                        }
                    }
                    finalValue += ci->first;

                    if (finalValue > bestValue)
                    {
                        newPromotions = requiredPromotions;
                        newPromotions.insert(ci->second);
                        bestValue = finalValue;
                        bestPromotionLevelCost = levels;
#ifdef ALTAI_DEBUG
                        /*os << "\nlevel = " << level << " value = " << bestValue << " current = (" << currentValue << ")";
                        os << "\nExisting promotions: ";
                        for (Promotions::const_iterator pi(bestPromotions.begin()), piEnd(bestPromotions.end()); pi != piEnd; ++pi)
                        {
                            os << gGlobals.getPromotionInfo(*pi).getType() << ", ";
                        }
                        os << "\nNew promotions: ";
                        for (Promotions::const_iterator pi(newPromotions.begin()), piEnd(newPromotions.end()); pi != piEnd; ++pi)
                        {
                            os << gGlobals.getPromotionInfo(*pi).getType() << ", ";
                        }*/
#endif
                    }
                }
            }

            if (newPromotions.empty())
            {
                break;
            }
            bestPromotions.insert(newPromotions.begin(), newPromotions.end());
            currentLevel -= bestPromotionLevelCost;
            currentValue = bestValue;
        }
#ifdef ALTAI_DEBUG
        // debug
        /*os << "\nUnit: " << gGlobals.getUnitInfo(pUnitInfo->getUnitType()).getType() <<  " best promotions for level: "
           << level << " value = " << bestValue << ", final level = " << currentLevel << "\n";
        for (Promotions::const_iterator pi(bestPromotions.begin()), piEnd(bestPromotions.end()); pi != piEnd; ++pi)
        {
            os << gGlobals.getPromotionInfo(*pi).getType() << ", ";
        }*/
#endif
        return std::make_pair(bestValue, std::make_pair(currentLevel, bestPromotions));
    }

    void UnitAnalysis::analyseUnits_()
    {
        // debug
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();

        cityAttackUnits_.resize(1 + maxPromotionSearchDepth_, UnitValuesMap());
        cityDefenceUnits_.resize(1 + maxPromotionSearchDepth_, UnitValuesMap());
        combatUnits_.resize(1 + maxPromotionSearchDepth_, UnitValuesMap());
        firstStrikeUnits_.resize(1 + maxPromotionSearchDepth_, UnitValuesMap());
        fastUnits_.resize(1 + maxPromotionSearchDepth_, UnitValuesMap());

        for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
        {
            unitCounterUnits_[(UnitCombatTypes)i].resize(1 + maxPromotionSearchDepth_, UnitValuesMap());
        }

        boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis = player_.getAnalysis();
        for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
        {
            UnitTypes unitType = getPlayerVersion(player_.getPlayerID(), (UnitClassTypes)i);

            if (unitType != NO_UNIT)
            {
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);
                if (unitInfo.getProductionCost() < 0 || unitInfo.getUnitCombatType() == NO_UNITCOMBAT)
                {
                    continue;
                }
                
                boost::shared_ptr<UnitInfo> pUnitInfo = pPlayerAnalysis->getUnitInfo(unitType);

                if (pUnitInfo)
                {
                    Promotions freePromotions = getFreePromotions(pUnitInfo);

                    for (int j = 0; j <= maxPromotionSearchDepth_; ++j)
                    {
                        RemainingLevelsAndPromotions requiredPromotions;

                        int baseCityAttack = unitInfo.getCityAttackModifier();
                        int baseCityDefence = unitInfo.getCityDefenseModifier();

                        int bestCityAttack = 0, additionalCityAttack = 0;

                        boost::tie(bestCityAttack, requiredPromotions) = calculateBestPromotions_(cityAttackPromotions_, unitInfo.getCityAttackModifier(), 
                            pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getCityAttackPercent), j);

					    // still have more promotions available
					    if (requiredPromotions.first > 0)
					    {
						    boost::tie(additionalCityAttack, requiredPromotions) = calculateBestPromotions_(combatPromotions_, 0, 
							    pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getCombatPercent), requiredPromotions.first, requiredPromotions.second);

						    if (requiredPromotions.first > 0)
						    {
							    boost::tie(additionalCityAttack, requiredPromotions) = calculateBestPromotions_(firstStrikePromotions_, 2 * unitInfo.getFirstStrikes() + unitInfo.getChanceFirstStrikes(), 
								    pUnitInfo, FirstStrikesPromotionValueFunctor(), requiredPromotions.first, requiredPromotions.second);
						    }
					    }

                        if (bestCityAttack > 0)
                        {
                            combinePromotions(requiredPromotions.second, freePromotions);
						    cityAttackUnits_[j].insert(std::make_pair(bestCityAttack, std::make_pair(unitType, requiredPromotions)));
                        }

                        int bestCityDefence = 0, additionalCityDefence = 0;
                    
                        boost::tie(bestCityDefence, requiredPromotions) = calculateBestPromotions_(cityDefencePromotions_, unitInfo.getCityDefenseModifier(),
                            pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getCityDefensePercent), j);

					    // still have more promotions available
					    if (requiredPromotions.first > 0)
					    {
						    boost::tie(additionalCityDefence, requiredPromotions) = calculateBestPromotions_(combatPromotions_, 0, 
							    pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getCombatPercent), requiredPromotions.first, requiredPromotions.second);

						    if (requiredPromotions.first > 0)
						    {
							    boost::tie(additionalCityDefence, requiredPromotions) = calculateBestPromotions_(firstStrikePromotions_, 2 * unitInfo.getFirstStrikes() + unitInfo.getChanceFirstStrikes(), 
								    pUnitInfo, FirstStrikesPromotionValueFunctor(), requiredPromotions.first, requiredPromotions.second);
						    }
					    }

                        if (bestCityDefence > 0)
                        {
                            combinePromotions(requiredPromotions.second, freePromotions);
						    cityDefenceUnits_[j].insert(std::make_pair(bestCityDefence, std::make_pair(unitType, requiredPromotions)));
                        }

                        int bestCombatPercent = 0, additionalCombat = 0;
                    
                        boost::tie(bestCombatPercent, requiredPromotions) = calculateBestPromotions_(combatPromotions_, 0, pUnitInfo,
                            PromotionValueFunctor(&CvPromotionInfo::getCombatPercent), j);

					    // still have more promotions available
					    if (requiredPromotions.first > 0)
					    {
						    boost::tie(additionalCombat, requiredPromotions) = calculateBestPromotions_(firstStrikePromotions_, 2 * unitInfo.getFirstStrikes() + unitInfo.getChanceFirstStrikes(), 
							    pUnitInfo, FirstStrikesPromotionValueFunctor(), requiredPromotions.first, requiredPromotions.second);
					    }

                        if (bestCombatPercent > 0)
                        {
                            combinePromotions(requiredPromotions.second, freePromotions);
						    combatUnits_[j].insert(std::make_pair(bestCombatPercent, std::make_pair(unitType, requiredPromotions)));
                        }

                        int bestFirstStrikes = 0, additionalFSCombat = 0;
                    
                        boost::tie(bestFirstStrikes, requiredPromotions) = calculateBestPromotions_(firstStrikePromotions_, 2 * unitInfo.getFirstStrikes() + unitInfo.getChanceFirstStrikes(),
                            pUnitInfo, FirstStrikesPromotionValueFunctor(), j);

					    if (requiredPromotions.first > 0)
					    {
						    boost::tie(additionalFSCombat, requiredPromotions) = calculateBestPromotions_(combatPromotions_, 0, 
							    pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getCombatPercent), requiredPromotions.first, requiredPromotions.second);
					    }

                        if (bestFirstStrikes > 0)
                        {
                            combinePromotions(requiredPromotions.second, freePromotions);
						    firstStrikeUnits_[j].insert(std::make_pair(bestFirstStrikes, std::make_pair(unitType, requiredPromotions)));
                        }

                        for (std::map<UnitCombatTypes, PromotionsMap>::const_iterator ci(unitCounterPromotionsMap_.begin()), ciEnd(unitCounterPromotionsMap_.end()); ci != ciEnd; ++ci)
                        {
                            int baseCounter = unitInfo.getUnitCombatModifier(ci->first);
                            int bestCounterValue = 0, extraValue;
                        
                            boost::tie(bestCounterValue, requiredPromotions) = calculateBestPromotions_(ci->second, baseCounter, pUnitInfo,
                                UnitCombatPromotionValueFunctor(&CvPromotionInfo::getUnitCombatModifierPercent, ci->first), j);

                            // still have more promotions available
					        if (requiredPromotions.first > 0)
					        {
						        boost::tie(extraValue, requiredPromotions) = calculateBestPromotions_(combatPromotions_, 0, 
							        pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getCombatPercent), requiredPromotions.first, requiredPromotions.second);

						        if (requiredPromotions.first > 0)
						        {
							        boost::tie(extraValue, requiredPromotions) = calculateBestPromotions_(firstStrikePromotions_, 2 * unitInfo.getFirstStrikes() + unitInfo.getChanceFirstStrikes(), 
								        pUnitInfo, FirstStrikesPromotionValueFunctor(), requiredPromotions.first, requiredPromotions.second);
						        }
					        }

                            if (bestCounterValue > 0)
                            {
                                combinePromotions(requiredPromotions.second, freePromotions);
                                unitCounterUnits_[ci->first][j].insert(std::make_pair(bestCounterValue, std::make_pair(unitType, requiredPromotions)));
                            }
                        }

                        int mostMoves = 0, extras = 0;
                        boost::tie(mostMoves, requiredPromotions) = calculateBestPromotions_(movementPromotions_, unitInfo.getMoves(), pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getMovesChange), j);

                        // still have more promotions available
					    if (requiredPromotions.first > 0)
					    {
						    boost::tie(extras, requiredPromotions) = calculateBestPromotions_(combatPromotions_, 0, 
							    pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getCombatPercent), requiredPromotions.first, requiredPromotions.second);

						    if (requiredPromotions.first > 0)
						    {
							    boost::tie(extras, requiredPromotions) = calculateBestPromotions_(firstStrikePromotions_, 2 * unitInfo.getFirstStrikes() + unitInfo.getChanceFirstStrikes(), 
								    pUnitInfo, FirstStrikesPromotionValueFunctor(), requiredPromotions.first, requiredPromotions.second);
						    }
					    }

                        if (mostMoves > 1)
                        {
                            combinePromotions(requiredPromotions.second, freePromotions);
                            fastUnits_[j].insert(std::make_pair(mostMoves, std::make_pair(unitType, requiredPromotions)));
                        }
                    }
                }
            }
        }
    }

	void UnitAnalysis::analyseAsCityAttackUnit_(UnitTypes unitType)
	{
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
		const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        if (unitInfo.isOnlyDefensive())
        {
#ifdef ALTAI_DEBUG
            os << "\nSkipping unit as only defensive: " << unitInfo.getType();
#endif
            return;
        }
#ifdef ALTAI_DEBUG
        os << "\nAnalysing unit: " << unitInfo.getType();
#endif
		for (int i = 1; i <= maxPromotionSearchDepth_; ++i)
		{
#ifdef ALTAI_DEBUG
            //os << "\n\tfor level: " << i;
#endif
			RemainingLevelsAndPromotions attackPromotions = getCityAttackPromotions(unitType, i);

			if (attackPromotions.second.empty())
			{
				attackPromotions = getCombatPromotions(unitType, i);
			}
#ifdef ALTAI_DEBUG
            //os << " attack promotions = ";
#endif
			UnitData attacker(unitInfo);
			for (Promotions::const_iterator ci(attackPromotions.second.begin()), ciEnd(attackPromotions.second.end()); ci != ciEnd; ++ci)
			{
#ifdef ALTAI_DEBUG
                //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
#endif
				attacker.applyPromotion(gGlobals.getPromotionInfo(*ci));
			}

			for (int j = 0, count = gGlobals.getNumUnitClassInfos(); j < count; ++j)
            {
                UnitTypes defaultUnit = (UnitTypes)gGlobals.getUnitClassInfo((UnitClassTypes)j).getDefaultUnitIndex();

                const CvUnitInfo& otherUnitInfo = gGlobals.getUnitInfo(defaultUnit);

                if (otherUnitInfo.getProductionCost() >= 0 && otherUnitInfo.getCombat() > 0 && unitInfo.getDomainType() == otherUnitInfo.getDomainType())
				{
					if (otherUnitInfo.getUnitAIType(UNITAI_CITY_DEFENSE) || (otherUnitInfo.getUnitAIType(UNITAI_COUNTER) && !otherUnitInfo.isNoDefensiveBonus()))
					{
						RemainingLevelsAndPromotions defencePromotions = getCityDefencePromotions(defaultUnit, i);

						if (defencePromotions.second.empty())
						{
							defencePromotions = getCombatPromotions(defaultUnit, i);
						}
#ifdef ALTAI_DEBUG
                        //os << "\n defender promotions = ";
#endif
						UnitData defender(otherUnitInfo);
						for (Promotions::const_iterator ci(defencePromotions.second.begin()), ciEnd(defencePromotions.second.end()); ci != ciEnd; ++ci)
						{
#ifdef ALTAI_DEBUG
                            //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
#endif
							defender.applyPromotion(gGlobals.getPromotionInfo(*ci));
						}

						int attackOdds = getCombatOdds(attacker, defender, UnitData::CityAttack);
#ifdef ALTAI_DEBUG
                        /*os << "\n\t\tv. unit: " << gGlobals.getUnitInfo(defaultUnit).getType() << " attack str = " << attacker.calculateStrength(UnitData::CityAttack) 
                           << " defence str = " << defender.calculateStrength(attacker, UnitData::CityAttack)
                           << " attacker combat = " << attacker.combat << " defender combat = " << defender.combat
                           << " city attack % = " << attacker.cityAttackPercent; */
#endif
						int defenceOdds = getCombatOdds(defender, attacker, UnitData::CityAttack);

						if (attackOdds > 0)
						{
							cityAttackUnitValues_[unitType][i][defaultUnit].insert(std::make_pair(UNITAI_CITY_DEFENSE, std::make_pair(attackOdds, 1000 - defenceOdds)));
						}
						if (defenceOdds > 0)
						{
                            cityDefenceUnitValues_[defaultUnit][i][unitType].insert(std::make_pair(UNITAI_ATTACK_CITY, std::make_pair(defenceOdds, 1000 - attackOdds)));
						}
					}
				}
			}
		}
#ifdef ALTAI_DEBUG
        os << "\n final city attack value = " << getCityAttackUnitValue(unitType);
        os << "\n final city defence value = " << getCityDefenceUnitValue(unitType);
#endif
	}


    void UnitAnalysis::analyseAsCombatUnit_(UnitTypes unitType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
		const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        if (unitInfo.isOnlyDefensive())
        {
#ifdef ALTAI_DEBUG
            os << "\nSkipping unit as only defensive: " << unitInfo.getType();
#endif
            return;
        }
#ifdef ALTAI_DEBUG
        os << "\nAnalysing unit: " << unitInfo.getType();
#endif
		for (int i = 1; i <= maxPromotionSearchDepth_; ++i)
		{
#ifdef ALTAI_DEBUG
            //os << "\n\tfor level: " << i;
#endif
			RemainingLevelsAndPromotions combatPromotions = getCombatPromotions(unitType, i);
#ifdef ALTAI_DEBUG
            //os << " attack promotions = ";
#endif
			UnitData attacker(unitInfo);
			for (Promotions::const_iterator ci(combatPromotions.second.begin()), ciEnd(combatPromotions.second.end()); ci != ciEnd; ++ci)
			{
#ifdef ALTAI_DEBUG
                //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
#endif
				attacker.applyPromotion(gGlobals.getPromotionInfo(*ci));
			}

			for (int j = 0, count = gGlobals.getNumUnitClassInfos(); j < count; ++j)
            {
                UnitTypes defaultUnit = (UnitTypes)gGlobals.getUnitClassInfo((UnitClassTypes)j).getDefaultUnitIndex();

                const CvUnitInfo& otherUnitInfo = gGlobals.getUnitInfo(defaultUnit);

                if (otherUnitInfo.getProductionCost() >= 0 && otherUnitInfo.getCombat() > 0 && unitInfo.getDomainType() == otherUnitInfo.getDomainType())
				{
                    if (otherUnitInfo.getUnitAIType(UNITAI_COUNTER) || otherUnitInfo.getUnitAIType(UNITAI_ATTACK_SEA) || otherUnitInfo.getUnitAIType(UNITAI_ASSAULT_SEA))
					{
						RemainingLevelsAndPromotions defencePromotions = getCombatPromotions(defaultUnit, i);
#ifdef ALTAI_DEBUG
                        //os << "\n defender promotions = ";
#endif
						UnitData defender(otherUnitInfo);
						for (Promotions::const_iterator ci(defencePromotions.second.begin()), ciEnd(defencePromotions.second.end()); ci != ciEnd; ++ci)
						{
#ifdef ALTAI_DEBUG
                            //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
#endif
							defender.applyPromotion(gGlobals.getPromotionInfo(*ci));
						}

						int attackOdds = getCombatOdds(attacker, defender);
#ifdef ALTAI_DEBUG
                        /*os << "\n\t\tv. unit: " << gGlobals.getUnitInfo(defaultUnit).getType() << " attack str = " << attacker.calculateStrength(0) 
                           << " defence str = " << defender.calculateStrength(attacker, 0)
                           << " attacker combat = " << attacker.combat << " defender combat = " << defender.combat;*/
#endif
						int defenceOdds = getCombatOdds(defender, attacker);

						if (attackOdds > 0)
						{
                            attackUnitValues_[unitType][i][defaultUnit].insert(std::make_pair(UNITAI_ATTACK, std::make_pair(attackOdds, 1000 - defenceOdds)));
						}
						if (defenceOdds > 0)
						{
                            defenceUnitValues_[defaultUnit][i][unitType].insert(std::make_pair(UNITAI_COUNTER, std::make_pair(defenceOdds, 1000 - attackOdds)));
						}
					}
				}
			}
		}
#ifdef ALTAI_DEBUG
        os << "\n final attack value = " << getAttackUnitValue(unitType);
        os << "\n final defence value = " << getDefenceUnitValue(unitType);
#endif
    }

    void UnitAnalysis::analyseAsCounterUnit_(UnitTypes unitType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
		const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        if (unitInfo.isOnlyDefensive())
        {
#ifdef ALTAI_DEBUG
            os << "\nSkipping unit as only defensive: " << unitInfo.getType();
#endif
            return;
        }
#ifdef ALTAI_DEBUG
        os << "\nAnalysing unit: " << unitInfo.getType();
#endif
		for (int i = 1; i <= maxPromotionSearchDepth_; ++i)
		{
#ifdef ALTAI_DEBUG
            //os << "\n\tfor level: " << i;
#endif
			for (int j = 0, count = gGlobals.getNumUnitClassInfos(); j < count; ++j)
            {
                UnitTypes defaultUnit = (UnitTypes)gGlobals.getUnitClassInfo((UnitClassTypes)j).getDefaultUnitIndex();

                const CvUnitInfo& otherUnitInfo = gGlobals.getUnitInfo(defaultUnit);

                if (otherUnitInfo.getProductionCost() >= 0 && otherUnitInfo.getCombat() > 0 && unitInfo.getDomainType() == otherUnitInfo.getDomainType())
				{
                    RemainingLevelsAndPromotions combatPromotions = getCombatCounterPromotions(unitType, (UnitCombatTypes)otherUnitInfo.getUnitCombatType(), i);
#ifdef ALTAI_DEBUG
                    //os << " attack promotions = ";
#endif
			        UnitData attacker(unitInfo);
			        for (Promotions::const_iterator ci(combatPromotions.second.begin()), ciEnd(combatPromotions.second.end()); ci != ciEnd; ++ci)
			        {
#ifdef ALTAI_DEBUG
                        //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
#endif
				        attacker.applyPromotion(gGlobals.getPromotionInfo(*ci));
			        }

                    {
    					RemainingLevelsAndPromotions defencePromotions = getCombatPromotions(defaultUnit, i);
#ifdef ALTAI_DEBUG
                        //os << "\n defender promotions = ";
#endif
	    				UnitData defender(otherUnitInfo);
		    			for (Promotions::const_iterator ci(defencePromotions.second.begin()), ciEnd(defencePromotions.second.end()); ci != ciEnd; ++ci)
			    		{
#ifdef ALTAI_DEBUG
                            //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
#endif
					    	defender.applyPromotion(gGlobals.getPromotionInfo(*ci));
					    }

					    int attackOdds = getCombatOdds(attacker, defender);
                        int defenceOdds = getCombatOdds(defender, attacker);
#ifdef ALTAI_DEBUG
                        /*os << "\n\t\tv. unit: " << gGlobals.getUnitInfo(defaultUnit).getType() << " attack str = " << attacker.calculateStrength(0) 
                            << " defence str = " << defender.calculateStrength(attacker, 0)
                            << " attacker combat = " << attacker.combat << " defender combat = " << defender.combat;*/
#endif
                        if (attackOdds > 0)
						{
                            attackUnitCounterValues_[unitType][i][defaultUnit].insert(std::make_pair(UNITAI_ATTACK, std::make_pair(attackOdds, 1000 - defenceOdds)));
						}
                    }
				}
			}
		}
#ifdef ALTAI_DEBUG
        for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
        {
            os << "\n final counter value (" << gGlobals.getUnitCombatInfo((UnitCombatTypes)i).getType() << ") = " << getUnitCounterValue(unitType, (UnitCombatTypes)i);
        }
#endif
    }

    void UnitAnalysis::debug()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();

        for (CombatDataMap::const_iterator ci(combatDataMap_.begin()), ciEnd(combatDataMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nUnit: " << gGlobals.getUnitInfo(ci->first).getType();
            for (UnitCombatDataMap::const_iterator ci2(ci->second.begin()), ci2End(ci->second.end()); ci2 != ci2End; ++ci2)
            {
                os << "\n\tv. " << gGlobals.getUnitInfo(ci2->first).getType() << " att = " << ci2->second.attackOdds << " def = " << ci2->second.defenceOdds
                    << " city att = " << ci2->second.cityAttackOdds << " city def = " << ci2->second.cityDefenceOdds;
            }
        }

        debugPromotions(os, cityAttackPromotions_, "City attack promotions");
        debugPromotions(os, cityDefencePromotions_, "City defence promotions");
        debugPromotions(os, combatPromotions_, "Combat promotions");
        debugPromotions(os, firstStrikePromotions_, "First strike promotions");
        debugPromotions(os, movementPromotions_, "Faster movement promotions");

        for (std::map<UnitCombatTypes, PromotionsMap>::const_iterator ci(unitCounterPromotionsMap_.begin()), ciEnd(unitCounterPromotionsMap_.end()); ci != ciEnd; ++ci)
        {
            std::ostringstream oss;
            oss << "Unit combat v. " << gGlobals.getUnitCombatInfo(ci->first).getType() << " promotions";
            debugPromotions(os, ci->second, oss.str());
        }

        debugUnits(os, cityAttackUnits_, "City attack units");
        debugUnits(os, cityDefenceUnits_, "City defence units");
        debugUnits(os, combatUnits_, "Combat units");
        debugUnits(os, firstStrikeUnits_, "First strike units");
        debugUnits(os, fastUnits_, "Fast units");

        os << "\nUnit counters: ";
        for (std::map<UnitCombatTypes, UnitLevels>::const_iterator ci(unitCounterUnits_.begin()), ciEnd(unitCounterUnits_.end()); ci != ciEnd; ++ci)
        {
            std::ostringstream oss;
            oss << "counter to: " << gGlobals.getUnitCombatInfo(ci->first).getType();
            debugUnits(os, ci->second, oss.str());
        }

		os << "\nCity attack values: ";
		for (UnitCombatInfoMap::const_iterator ci(cityAttackUnitValues_.begin()), ciEnd(cityAttackUnitValues_.end()); ci != ciEnd; ++ci)
		{
			os << "\nUnit = " << gGlobals.getUnitInfo(ci->first).getType();
			for (UnitCombatLevelsMap::const_iterator levelIter(ci->second.begin()), levelEndIter(ci->second.end()); levelIter != levelEndIter; ++levelIter)
			{
				os << "\n\tLevel = " << levelIter->first;
				for (UnitCombatOddsMap::const_iterator unitCounterIter(levelIter->second.begin()), unitCounterEndIter(levelIter->second.end()); unitCounterIter != unitCounterEndIter; ++unitCounterIter)
				{
					os << "\n\tv. " << gGlobals.getUnitInfo(unitCounterIter->first).getType();
					for (UnitAITypesCombatOddsMap::const_iterator unitAIIter(unitCounterIter->second.begin()), unitAIEndIter(unitCounterIter->second.end()); unitAIIter != unitAIEndIter; ++unitAIIter)
					{
						CvWString AITypeString;
						getUnitAIString(AITypeString, unitAIIter->first);

						os << " AI Type = " << narrow(AITypeString) << " att = " << unitAIIter->second.first << ", def = " << unitAIIter->second.second;
					}
				}
			}
		}

		os << "\nCity defence values: ";
		for (UnitCombatInfoMap::const_iterator ci(cityDefenceUnitValues_.begin()), ciEnd(cityDefenceUnitValues_.end()); ci != ciEnd; ++ci)
		{
			os << "\nUnit = " << gGlobals.getUnitInfo(ci->first).getType();
			for (UnitCombatLevelsMap::const_iterator levelIter(ci->second.begin()), levelEndIter(ci->second.end()); levelIter != levelEndIter; ++levelIter)
			{
				os << "\n\tLevel = " << levelIter->first;
				for (UnitCombatOddsMap::const_iterator unitCounterIter(levelIter->second.begin()), unitCounterEndIter(levelIter->second.end()); unitCounterIter != unitCounterEndIter; ++unitCounterIter)
				{
					os << "\n\tv. " << gGlobals.getUnitInfo(unitCounterIter->first).getType();
					for (UnitAITypesCombatOddsMap::const_iterator unitAIIter(unitCounterIter->second.begin()), unitAIEndIter(unitCounterIter->second.end()); unitAIIter != unitAIEndIter; ++unitAIIter)
					{
						CvWString AITypeString;
						getUnitAIString(AITypeString, unitAIIter->first);

						os << " AI Type = " << narrow(AITypeString) << " att = " << unitAIIter->second.first << ", def = " << unitAIIter->second.second;
					}
				}
			}
		}
#endif
    }
}