#include "AltAI.h"

#include "./unit_analysis.h"
#include "./player_analysis.h"
#include "./unit_info_visitors.h"
#include "./player.h"
#include "./helper_fns.h"
#include "./save_utils.h"
#include "./civ_log.h"
#include "./unit_info.h"
#include "./iters.h"

namespace AltAI
{
    namespace
    {
        int getCombatOdds(const UnitData& attacker, const UnitData& defender, const UnitData::CombatDetails& combatDetails = UnitData::CombatDetails())
        {
            const int attHP = attacker.hp;
            const int defHP = defender.hp;
            const int attMaxHP = attacker.maxhp;
            const int defMaxHP = defender.maxhp;

            const int maxAttackerStrength = attacker.calculateStrength();
            const int maxDefenderStrength = defender.calculateStrength(attacker, combatDetails);

            const int attackerStrength = maxAttackerStrength * attHP / attMaxHP;
            const int defenderStrength = maxDefenderStrength * defHP / defMaxHP;

            const int attackerFirepower = (1 + maxAttackerStrength + attackerStrength) / 2;
            const int defenderFirepower = (1 + maxDefenderStrength + defenderStrength) / 2;

            return ::getCombatOdds(attackerStrength, attacker.firstStrikes, attacker.chanceFirstStrikes, 
                attacker.immuneToFirstStrikes, attackerFirepower, attHP, attMaxHP, attacker.combatLimit,
                defenderStrength, defender.firstStrikes, defender.chanceFirstStrikes, 
                defender.immuneToFirstStrikes, defenderFirepower, defHP, defMaxHP);
        }

        struct OddsForwarder
        {
            OddsForwarder(const int attStrength_, const int attFirstStrikes_, const int attChanceFirstStrikes_, 
                const int attFirepower_, const int attHP_, const int attMaxHP_, 
                const int attCombatLimit_, const int attWithdrawalProb_,
                const int defStrength_, const int defFirstStrikes_, const int defChanceFirstStrikes_, 
                const int defFirepower_, const int defHP_, const int defMaxHP_,
                const bool attImmuneToFirstStrikes_, const bool defImmuneToFirstStrikes_)
                    : attStrength(attStrength_), attFirstStrikes(attFirstStrikes_), attChanceFirstStrikes(attChanceFirstStrikes_),
                      attFirepower(attFirepower_), attHP(attHP_), attMaxHP(attMaxHP_), 
                      attCombatLimit(attCombatLimit_), attWithdrawalProb(attWithdrawalProb_),
                      defStrength(defStrength_), defFirstStrikes(defFirstStrikes_), defChanceFirstStrikes(defChanceFirstStrikes_),
                      defFirepower(defFirepower_), defHP(defHP_), defMaxHP(defMaxHP_),
                      attImmuneToFirstStrikes(attImmuneToFirstStrikes_), defImmuneToFirstStrikes(defImmuneToFirstStrikes_)
            {
            }            

            float operator() (const int attRounds, const int defRounds) const
            {
                return ::getCombatOdds(attStrength, attFirstStrikes, attChanceFirstStrikes, attImmuneToFirstStrikes,
                        attFirepower, attHP, attMaxHP, attCombatLimit, attWithdrawalProb,
                        defStrength, defFirstStrikes, defChanceFirstStrikes, defImmuneToFirstStrikes,
                        defFirepower, defHP, defMaxHP, attRounds, defRounds);
            }

            const int attStrength, attFirstStrikes, attChanceFirstStrikes, attFirepower, attHP, attMaxHP, attCombatLimit, attWithdrawalProb;            
            const int defStrength, defFirstStrikes, defChanceFirstStrikes, defFirepower, defHP, defMaxHP;
            const bool attImmuneToFirstStrikes, defImmuneToFirstStrikes;
        };

        bool canAttack(const UnitData& attacker, const UnitData& defender, const UnitData::CombatDetails& combatDetails = UnitData::CombatDetails())
        {
            if (attacker.pUnitInfo->isOnlyDefensive() || (attacker.hasAttacked && !attacker.isBlitz))
            {
                return false;
            }
            if (defender.maxhp - defender.hp > attacker.combatLimit)
            {
                return false;
            }
            // todo - add check for amphibious collateral units
            return true;
        }

        UnitOddsData getCombatOddsDetail_(const UnitData& attacker, const UnitData& defender, const UnitData::CombatDetails& combatDetails = UnitData::CombatDetails())
        {
            static const int COMBAT_DIE_SIDES = gGlobals.getDefineINT("COMBAT_DIE_SIDES");  // 1000
            static const int COMBAT_DAMAGE = gGlobals.getDefineINT("COMBAT_DAMAGE");  // 20

            UnitOddsData odds;

            const int attHP = attacker.hp;
            const int defHP = defender.hp;
            const int attMaxHP = attacker.maxhp;
            const int defMaxHP = defender.maxhp;

            const int maxAttackerStrength = attacker.calculateStrength();
            const int maxDefenderStrength = defender.calculateStrength(attacker, combatDetails);

            const int attackerStrength = maxAttackerStrength * attHP / attMaxHP;
            const int defenderStrength = maxDefenderStrength * defHP / defMaxHP;

            const int attackerFirepower = (1 + maxAttackerStrength + attackerStrength) / 2;
            const int defenderFirepower = (1 + maxDefenderStrength + defenderStrength) / 2;

            const int iStrengthFactor = ((attackerFirepower + defenderFirepower + 1) / 2);
            const int iDamageToAttacker = std::max<int>(1, (COMBAT_DAMAGE * (defenderFirepower + iStrengthFactor)) / (attackerFirepower + iStrengthFactor));
            const int iDamageToDefender = std::max<int>(1, (COMBAT_DAMAGE * (attackerFirepower + iStrengthFactor)) / (defenderFirepower + iStrengthFactor));
            const int iDefenderHitLimit = defMaxHP - attacker.combatLimit;

            const int iNeededRoundsAttacker = (defHP - defMaxHP + attacker.combatLimit - (attacker.combatLimit == defMaxHP ? 1 : 0)) / iDamageToDefender + 1;
            const int iNeededRoundsDefender = (attHP - 1) / iDamageToAttacker + 1;

            OddsForwarder oddsForwarder(attackerStrength, attacker.firstStrikes, attacker.chanceFirstStrikes,
                        attackerFirepower, attHP, attMaxHP, attacker.combatLimit, attacker.withdrawalProb,
                        defenderStrength, defender.firstStrikes, defender.chanceFirstStrikes,
                        defenderFirepower, defHP, defMaxHP, attacker.immuneToFirstStrikes, defender.immuneToFirstStrikes);

            for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++)
            {
                odds.E_HP_Att += (attHP - n_A * iDamageToAttacker) * oddsForwarder(n_A, iNeededRoundsAttacker);
            }

            odds.E_HP_Att_Victory = odds.E_HP_Att;         

            if (attacker.withdrawalProb > 0)
            {
                odds.E_HP_Att_Withdraw = odds.E_HP_Att;
                for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++)
                {
                    odds.E_HP_Att += (attHP - (iNeededRoundsDefender - 1) * iDamageToAttacker) * oddsForwarder(iNeededRoundsAttacker - 1, n_D);
                }
            }

            if (attacker.combatLimit < defMaxHP)
            {
                odds.E_HP_Att_Retreat = (float)(attHP - (iNeededRoundsDefender - 1) * iDamageToAttacker);
            }

            for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++)
            {
                odds.E_HP_Def += (defHP - n_D * iDamageToDefender) * oddsForwarder(iNeededRoundsDefender, n_D) + oddsForwarder(iNeededRoundsDefender - 1, n_D);
            }

            odds.E_HP_Def_Defeat = odds.E_HP_Def;

            if (attacker.combatLimit < defMaxHP) // if attacker has a combatLimit (eg. catapult)
            {
                // code specific to case where the last successful hit by an attacker will do 0 damage, 
                // and doing either iNeededRoundsAttacker or iNeededRoundsAttacker - 1 will cause the same damage
                // seems to be the same in original - so removed here for now
                for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++)
                {
                    odds.E_HP_Def += (float)iDefenderHitLimit * oddsForwarder(n_A, iNeededRoundsAttacker);
                    odds.E_HP_Def_Withdraw += (float)iDefenderHitLimit * oddsForwarder(n_A, iNeededRoundsAttacker);
                }
            }

            if (attacker.combatLimit == defMaxHP) // ie. we can kill the defender...
            {
                for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++)
                {
                    odds.AttackerKillOdds += oddsForwarder(n_A, iNeededRoundsAttacker);
                }
            }
            else
            {
                // else we cannot kill the defender (eg. catapults attacking)
                for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++)
                {
                    odds.PullOutOdds += oddsForwarder(n_A, iNeededRoundsAttacker);
                }
            }

            if (attacker.withdrawalProb > 0)
            {
                for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++)
                {
                    odds.RetreatOdds += oddsForwarder(iNeededRoundsDefender - 1, n_D);
                }
            }

            for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++)
            {
                odds.DefenderKillOdds += oddsForwarder(iNeededRoundsDefender, n_D);
            }

            return odds;
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
            os << "\n" << descriptor << ": ";
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

    void UnitOddsData::debug(std::ostream& os) const
    {
        os << " E_HP_Att: " << E_HP_Att << ", E_HP_Def: " << E_HP_Def;
        if (E_HP_Att_Withdraw > 0.0)
        {
            os << ", E_HP_Att_Withdraw: " << E_HP_Att_Withdraw;
        }
        if (E_HP_Att_Retreat > 0.0)
        {
            os << ", E_HP_Att_Retreat: " << E_HP_Att_Retreat;
        }
        os << ", E_HP_Att_Victory: " << E_HP_Att_Victory << ", E_HP_Def_Defeat: " << E_HP_Def_Defeat;
        if (E_HP_Def_Withdraw > 0.0)
        {
            os << ", E_HP_Def_Withdraw: " << E_HP_Def_Withdraw;
        }

        os << ". AttackerKillOdds: " << AttackerKillOdds;
        if (PullOutOdds > 0.0)
        {
            os << ", PullOutOdds: " << PullOutOdds;
        }
        if (RetreatOdds > 0.0)
        {
            os << ", RetreatOdds: " << RetreatOdds;
        }
        os << ", DefenderKillOdds: " << DefenderKillOdds;
    }

    UnitData::CombatDetails::CombatDetails(const CvPlot* pPlot) : 
        flags(None), plotIsHills(pPlot->isHills()), plotTerrain(pPlot->getTerrainType()), plotFeature(pPlot->getFeatureType()),
        cultureDefence(0), buildingDefence(0), plotDefence(0), attackDirection(NO_DIRECTION)
    {
        if (pPlot->isCity())  // assumes we can see the city for real if we're checking combat odds
        {
            flags |= CombatDetails::CityAttack;
            flags |= CombatDetails::FortifyDefender;  // todo - make this more dynamic from our view of the plot, and unit states
            cultureDefence = pPlot->getPlotCity()->getDefenseModifier(true);
            buildingDefence = pPlot->getPlotCity()->getDefenseModifier(false);
        }

        plotDefence = plotFeature == NO_FEATURE ? 
            gGlobals.getTerrainInfo(plotTerrain).getDefenseModifier() : gGlobals.getFeatureInfo(plotFeature).getDefenseModifier();
        /*if (plotIsHills)  // added in calculateStrength (need flag as can affect attacker too)
        {
            plotDefence += gGlobals.getHILLS_EXTRA_DEFENSE();
        }*/
        NeighbourPlotIter plotIter(pPlot);
        const bool isWater = pPlot->isWater();
        while (IterPlot pLoopPlot = plotIter())
        {
            DirectionTypes d = directionXY(pLoopPlot, pPlot);
            isRiverCrossing[d] = pLoopPlot->isRiverCrossing(d);
            if (!isWater)
            {
                isAmphibious[d] = pLoopPlot->isWater();
            }
        }
    }

    void UnitData::CombatDetails::write(FDataStreamBase* pStream) const
    {
        pStream->Write(flags);
        pStream->Write(plotIsHills);
        pStream->Write(plotTerrain);
        pStream->Write(plotFeature);
        pStream->Write(cultureDefence);
        pStream->Write(buildingDefence);
        pStream->Write(plotDefence);
        // todo - isRiverCrossing, isAmphibious, unitAttackDirectionsMap, attackDirection
    }

    void UnitData::CombatDetails::read(FDataStreamBase* pStream)
    {
        pStream->Read(&flags);
        pStream->Read(&plotIsHills);
        pStream->Read((int*)&plotTerrain);
        pStream->Read((int*)&plotFeature);
        pStream->Read(&cultureDefence);
        pStream->Read(&buildingDefence);
        pStream->Read(&plotDefence);
        // todo - isRiverCrossing, isAmphibious, unitAttackDirectionsMap, attackDirection
    }

    void UnitData::CombatDetails::debug(std::ostream& os) const
    {
        os << (flags & CityAttack ? " city attack " : "") << (flags & FortifyDefender ? " fortity def " : "");
        if (plotIsHills) os << " is hills ";
        os << " terrain = " << (plotTerrain != NO_TERRAIN ? gGlobals.getTerrainInfo(plotTerrain).getType() : " none? ");
        if (plotFeature != NO_FEATURE) os << " plot feature = " << gGlobals.getFeatureInfo(plotFeature).getType();
        if (cultureDefence != 0) os << " culture def = " << cultureDefence;
        if (buildingDefence != 0) os << " building def = " << buildingDefence;
        os << " plot def = " << plotDefence;
    }

    DirectionTypes UnitData::CombatDetails::getAttackDirection(const IDInfo& unit) const
    {
        if (unit == IDInfo())
        {
            return attackDirection;
        }
        else
        {
            std::map<IDInfo, DirectionTypes>::const_iterator attackDirIter = unitAttackDirectionsMap.find(unit);
            return attackDirIter != unitAttackDirectionsMap.end() ? attackDirIter->second : attackDirection;
        }
    }

    UnitData::UnitData() :
        pUnitInfo(NULL),
        unitType(NO_UNIT),
        moves(0),
        hp(0), maxhp(0),
        baseCombat(0), extraCombat(0), firstStrikes(0), chanceFirstStrikes(0), firePower(0), combatLimit(0), withdrawalProb(0),
        cityAttackPercent(0), cityDefencePercent(0), fortifyPercent(0),
        hillsAttackPercent(0), hillsDefencePercent(0),
        extraCollateralDamage(0), collateralDamageProtection(0),
        immuneToFirstStrikes(false), isBlitz(false), isRiver(false), isAmphib(false), isBarbarian(false), isAnimal(false),
        hasAttacked(false),
        animalModifier(gGlobals.getHandicapInfo(gGlobals.getGame().getHandicapType()).getAIAnimalCombatModifier()),
        barbModifier(gGlobals.getHandicapInfo(gGlobals.getGame().getHandicapType()).getAIBarbarianCombatModifier()),
        featureDoubleMoves(gGlobals.getNumFeatureInfos(), 0), terrainDoubleMoves(gGlobals.getNumTerrainInfos(), 0)
    {
    }

    UnitData::UnitData(const CvUnit* pUnit) :
        pUnitInfo(&pUnit->getUnitInfo()),
        unitType(pUnit->getUnitType()),
        unitId(pUnit->getIDInfo()),
        moves(pUnitInfo->getMoves()),
        hp(pUnit->currHitPoints()),
        maxhp(pUnit->maxHitPoints()),
        baseCombat(pUnitInfo->getCombat()), 
        extraCombat(pUnit->getExtraCombatPercent()),
        firstStrikes(pUnit->firstStrikes()),
        chanceFirstStrikes(pUnit->chanceFirstStrikes()),
        firePower(100 * pUnitInfo->getCombat()),
        combatLimit(pUnitInfo->getCombatLimit()), withdrawalProb(pUnit->withdrawalProbability()),
        cityAttackPercent(pUnit->cityAttackModifier()), cityDefencePercent(pUnit->cityDefenseModifier()), fortifyPercent(pUnit->fortifyModifier()),
        hillsAttackPercent(pUnit->hillsAttackModifier()), hillsDefencePercent(pUnit->hillsDefenseModifier()),
        extraCollateralDamage(pUnit->getExtraCollateralDamage()), collateralDamageProtection(pUnit->getCollateralDamageProtection()),
        immuneToFirstStrikes(pUnit->immuneToFirstStrikes()), isBlitz(pUnit->isBlitz()),
        isRiver(pUnit->isRiver()), isAmphib(pUnit->isAmphib()),
        isBarbarian(pUnit->isBarbarian()), isAnimal(pUnit->isAnimal()),
        hasAttacked(false),
        animalModifier(gGlobals.getHandicapInfo(gGlobals.getGame().getHandicapType()).getAIAnimalCombatModifier()),
        barbModifier(gGlobals.getHandicapInfo(gGlobals.getGame().getHandicapType()).getAIBarbarianCombatModifier())
    {
        for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
        {
            if (pUnit->unitCombatModifier((UnitCombatTypes)i) != 0)
            {
                unitCombatModifiers[(UnitCombatTypes)i] = pUnit->unitCombatModifier((UnitCombatTypes)i);
            }
        }

        for (int i = 0, count = gGlobals.getNumTerrainInfos(); i < count; ++i)
        {
            if (pUnit->terrainAttackModifier((TerrainTypes)i) != 0)
            {
                unitTerrainAttackModifiers[(TerrainTypes)i] = pUnit->terrainAttackModifier((TerrainTypes)i);
            }
            if (pUnit->terrainDefenseModifier((TerrainTypes)i) != 0)
            {
                unitTerrainDefenceModifiers[(TerrainTypes)i] = pUnit->terrainDefenseModifier((TerrainTypes)i);
            }
        }

        for (int i = 0, count = gGlobals.getNumFeatureInfos(); i < count; ++i)
        {
            if (pUnit->featureAttackModifier((FeatureTypes)i) != 0)
            {
                unitFeatureAttackModifiers[(FeatureTypes)i] = pUnit->featureAttackModifier((FeatureTypes)i);
            }
            if (pUnit->featureDefenseModifier((FeatureTypes)i) != 0)
            {
                unitFeatureDefenceModifiers[(FeatureTypes)i] = pUnit->featureDefenseModifier((FeatureTypes)i);
            }
        }

        for (int i = 0, count = gGlobals.getNumPromotionInfos(); i < count; ++i)
        {
            if (pUnit->isHasPromotion((PromotionTypes)i))
            {
                promotions.push_back((PromotionTypes)i);
            }
        }

        for (int i = 0, infoCount = gGlobals.getNumFeatureInfos(); i < infoCount; ++i)
        {
            featureDoubleMoves.push_back(pUnit->isFeatureDoubleMove((FeatureTypes)i));
        }
        for (int i = 0, infoCount = gGlobals.getNumTerrainInfos(); i < infoCount; ++i)
        {
            terrainDoubleMoves.push_back(pUnit->isTerrainDoubleMove((TerrainTypes)i));
        }
    }

    UnitData::UnitData(UnitTypes unitType_, const Promotions& promotions_) :
        pUnitInfo(&gGlobals.getUnitInfo(unitType_)),
        unitType(unitType_),
        moves(pUnitInfo->getMoves()),
        hp(gGlobals.getMAX_HIT_POINTS()),
        maxhp(gGlobals.getMAX_HIT_POINTS()),
        baseCombat(pUnitInfo->getCombat()), 
        extraCombat(0),
        firstStrikes(pUnitInfo->getFirstStrikes()),
        chanceFirstStrikes(pUnitInfo->getChanceFirstStrikes()),
        firePower(100 * pUnitInfo->getCombat()),
        combatLimit(pUnitInfo->getCombatLimit()), withdrawalProb(pUnitInfo->getWithdrawalProbability()),
        cityAttackPercent(pUnitInfo->getCityAttackModifier()), cityDefencePercent(pUnitInfo->getCityDefenseModifier()), fortifyPercent(0),
        hillsAttackPercent(pUnitInfo->getHillsAttackModifier()), hillsDefencePercent(pUnitInfo->getHillsDefenseModifier()),
        extraCollateralDamage(0), collateralDamageProtection(0),
        immuneToFirstStrikes(pUnitInfo->isFirstStrikeImmune()), isBlitz(false), isBarbarian(false), isAnimal(pUnitInfo->isAnimal()),
        hasAttacked(false),
        animalModifier(gGlobals.getHandicapInfo(gGlobals.getGame().getHandicapType()).getAIAnimalCombatModifier()),
        barbModifier(gGlobals.getHandicapInfo(gGlobals.getGame().getHandicapType()).getAIBarbarianCombatModifier()),
        featureDoubleMoves(gGlobals.getNumFeatureInfos(), 0), terrainDoubleMoves(gGlobals.getNumTerrainInfos(), 0)
    {
        for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
        {
            if (pUnitInfo->getUnitCombatModifier(i) != 0)
            {
                unitCombatModifiers[(UnitCombatTypes)i] = pUnitInfo->getUnitCombatModifier(i);
            }
        }

        for (int i = 0, count = gGlobals.getNumTerrainInfos(); i < count; ++i)
        {
            if (pUnitInfo->getTerrainAttackModifier(i) != 0)
            {
                unitTerrainAttackModifiers[(TerrainTypes)i] = pUnitInfo->getTerrainAttackModifier(i);
            }
            if (pUnitInfo->getTerrainDefenseModifier(i) != 0)
            {
                unitTerrainDefenceModifiers[(TerrainTypes)i] = pUnitInfo->getTerrainDefenseModifier(i);
            }
        }

        for (int i = 0, count = gGlobals.getNumFeatureInfos(); i < count; ++i)
        {
            if (pUnitInfo->getFeatureAttackModifier(i) != 0)
            {
                unitFeatureAttackModifiers[(FeatureTypes)i] = pUnitInfo->getFeatureAttackModifier(i);
            }
            if (pUnitInfo->getFeatureDefenseModifier(i) != 0)
            {
                unitFeatureDefenceModifiers[(FeatureTypes)i] = pUnitInfo->getFeatureDefenseModifier(i);
            }
        }

        for (Promotions::const_iterator ci(promotions_.begin()), ciEnd(promotions_.end()); ci != ciEnd; ++ci)
        {
            applyPromotion(*ci);
        }
    }

    void UnitData::applyPromotion(PromotionTypes promotionType)
    {
        if (std::find(promotions.begin(), promotions.end(), promotionType) != promotions.end())
        {
            return;
        }
        const CvPromotionInfo& promotion = gGlobals.getPromotionInfo(promotionType);
        promotions.push_back(promotionType);

        extraCombat += promotion.getCombatPercent();
        firstStrikes += promotion.getFirstStrikesChange();
        chanceFirstStrikes += promotion.getChanceFirstStrikesChange();
        if (promotion.isImmuneToFirstStrikes())
        {
            immuneToFirstStrikes = true;
        }

        if (!isBlitz)
        {
            isBlitz = promotion.isBlitz();
        }

        if (!isRiver)
        {
            isRiver = promotion.isRiver();
        }

        if (!isAmphib)
        {
            isAmphib = promotion.isAmphib();
        }

        withdrawalProb += promotion.getWithdrawalChange();

        cityAttackPercent += promotion.getCityAttackPercent();
        cityDefencePercent += promotion.getCityDefensePercent();

        hillsAttackPercent += promotion.getHillsAttackPercent();
        hillsDefencePercent += promotion.getHillsDefensePercent();

        for (int i = 0, count = gGlobals.getNumFeatureInfos(); i < count; ++i)
        {
            if (promotion.getFeatureAttackPercent(i) != 0)
            {
                unitFeatureAttackModifiers[(FeatureTypes)i] += promotion.getFeatureAttackPercent(i);
            }
            if (promotion.getFeatureDefensePercent(i) != 0)
            {
                unitFeatureDefenceModifiers[(FeatureTypes)i] += promotion.getFeatureDefensePercent(i);
            }
        }

        for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
        {
            if (promotion.getUnitCombatModifierPercent(i) != 0)
            {
                unitCombatModifiers[(UnitCombatTypes)i] += promotion.getUnitCombatModifierPercent(i);
            }
        }

        if (hp < maxhp)
        {
            hp += (maxhp - hp) / 2;
        }
    }

    void UnitData::debugPromotions(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        for (size_t i = 0, count = promotions.size(); i < count; ++i)
        {
            if (i == 0) os << " (";
            else os << ", ";
            os << gGlobals.getPromotionInfo(promotions[i]).getType();
        }
        if (!promotions.empty()) os << ")";
#endif  
    }

    // calculate our strength as defender
    int UnitData::calculateStrength(const UnitData& other, const UnitData::CombatDetails& combatDetails) const
    {
        // unit class type modifiers - add defence, subtract the other unit's attack modifier
        // don't bother to precalculate as there are no promotions which affect them
        int modifier = pUnitInfo->getUnitClassDefenseModifier(other.pUnitInfo->getUnitClassType());
        const bool noDefensiveBonus = pUnitInfo->isNoDefensiveBonus();
        modifier -= other.pUnitInfo->getUnitClassAttackModifier(pUnitInfo->getUnitClassType());
        modifier += extraCombat;

        modifier += fortifyPercent;

        if (other.isAnimal)
        {
            modifier += pUnitInfo->getAnimalCombatModifier();
            modifier -= animalModifier;
        }

        if (isAnimal)
        {
            modifier -= other.pUnitInfo->getAnimalCombatModifier();
            modifier += other.animalModifier;
        }

        if (other.isBarbarian)
        {
            modifier -= barbModifier;
        }

        if (isBarbarian)
        {
            modifier += other.barbModifier;
        }

        if (!noDefensiveBonus)
        {
            modifier += combatDetails.plotDefence;
        }

        // unit type modifiers - add defence, subtract other's attack bonus
        std::map<UnitCombatTypes, int>::const_iterator ci = unitCombatModifiers.find((UnitCombatTypes)other.pUnitInfo->getUnitCombatType());
        if (ci != unitCombatModifiers.end())
        {
            modifier += ci->second;
        }

        ci = other.unitCombatModifiers.find((UnitCombatTypes)pUnitInfo->getUnitCombatType());
        if (ci != other.unitCombatModifiers.end())
        {
            modifier -= ci->second;
        }

        if (combatDetails.plotIsHills)
        {
            if (!noDefensiveBonus)
            {
                modifier += hillsDefencePercent;
            }
            modifier -= other.hillsAttackPercent;
        }

        DirectionTypes attackDirection = combatDetails.getAttackDirection(other.unitId);

        if (attackDirection != NO_DIRECTION)
        {
            if (!other.isRiver && combatDetails.isRiverCrossing[attackDirection])
            {
                modifier -= gGlobals.getRIVER_ATTACK_MODIFIER();  // -(-25)
            }

            if (!other.isAmphib && combatDetails.isAmphibious[attackDirection])
            {
                modifier -= gGlobals.getAMPHIB_ATTACK_MODIFIER();  // -(-50)
            }
        }

        if (combatDetails.plotTerrain != NO_TERRAIN)
        {
            std::map<TerrainTypes, int>::const_iterator fi = unitTerrainDefenceModifiers.find(combatDetails.plotTerrain);
            if (fi != unitTerrainDefenceModifiers.end())
            {
                modifier += fi->second;
            }

            fi = unitTerrainAttackModifiers.find(combatDetails.plotTerrain);
            if (fi != unitTerrainAttackModifiers.end())
            {
                modifier -= fi->second;
            }
        }

        if (combatDetails.plotFeature != NO_FEATURE)
        {
            std::map<FeatureTypes, int>::const_iterator fi = unitFeatureDefenceModifiers.find(combatDetails.plotFeature);
            if (fi != unitFeatureDefenceModifiers.end())
            {
                modifier += fi->second;
            }

            fi = unitFeatureAttackModifiers.find(combatDetails.plotFeature);
            if (fi != unitFeatureAttackModifiers.end())
            {
                modifier -= fi->second;
            }
        }

        // if attacker is attacking a city (so we are the defender)
        if (combatDetails.flags & CombatDetails::CityAttack)
        {
            // subtract attacker's city attack bonus and add our city defence bonus
            modifier -= other.cityAttackPercent;
            modifier += cityDefencePercent;

            if (!noDefensiveBonus)
            {
                modifier += std::max<int>(combatDetails.cultureDefence, other.pUnitInfo->isIgnoreBuildingDefense() ? 0 : combatDetails.buildingDefence);
            }
        }

        if (combatDetails.flags & CombatDetails::FortifyDefender)
        {
            if (!noDefensiveBonus)
            {
                modifier += gGlobals.getDefineINT("MAX_FORTIFY_TURNS") * gGlobals.getFORTIFY_MODIFIER_PER_TURN();
            }
        }

        int strength = baseCombat;
        if (modifier > 0)
        {
            strength = strength * (modifier + 100);
        }
        else
        {
            strength = (strength * 10000) / (100 - modifier);
        }
        return strength;
    }

    // calculate our strength as attacker
    int UnitData::calculateStrength() const
    {
        int modifier = extraCombat;
        int strength = baseCombat;

        if (modifier > 0)
	    {
		    strength = strength * (modifier + 100);
	    }
	    else
	    {
		    strength = (strength * 10000) / (100 - modifier);
	    }
        return strength;
    }

    void UnitData::write(FDataStreamBase* pStream) const
    {
        pStream->Write(unitType);        
        unitId.write(pStream);
        pStream->Write(moves);
        pStream->Write(hp);
        pStream->Write(maxhp);
        pStream->Write(baseCombat);
        pStream->Write(extraCombat);
        pStream->Write(firstStrikes);
        pStream->Write(chanceFirstStrikes);
        pStream->Write(firePower);
        pStream->Write(combatLimit);
        pStream->Write(withdrawalProb);
        pStream->Write(cityAttackPercent);
        pStream->Write(cityDefencePercent);
        pStream->Write(fortifyPercent);
        pStream->Write(hillsAttackPercent);
        pStream->Write(hillsDefencePercent);
        pStream->Write(immuneToFirstStrikes);
        pStream->Write(isBlitz);
        pStream->Write(isRiver);
        pStream->Write(isAmphib);
        pStream->Write(isBarbarian);
        pStream->Write(isAnimal);
        pStream->Write(extraCollateralDamage);
        pStream->Write(collateralDamageProtection);
        writeMap(pStream, unitFeatureAttackModifiers);
        writeMap(pStream, unitFeatureDefenceModifiers);
        writeMap(pStream, unitTerrainAttackModifiers);
        writeMap(pStream, unitTerrainDefenceModifiers);
        writeMap(pStream, unitCombatModifiers);
        pStream->Write(hasAttacked);
        writeVector(pStream, promotions);
        pStream->Write(animalModifier);
        pStream->Write(barbModifier);
    }

    void UnitData::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&unitType);
		if (unitType != NO_UNIT)
        {
            pUnitInfo = &gGlobals.getUnitInfo(unitType);
        }
        unitId.read(pStream);
        pStream->Read(&moves);
        pStream->Read(&hp);
        pStream->Read(&maxhp);
        pStream->Read(&baseCombat);
        pStream->Read(&extraCombat);
        pStream->Read(&firstStrikes);
        pStream->Read(&chanceFirstStrikes);
        pStream->Read(&firePower);
        pStream->Read(&combatLimit);
        pStream->Read(&withdrawalProb);
        pStream->Read(&cityAttackPercent);
        pStream->Read(&cityDefencePercent);
        pStream->Read(&fortifyPercent);
        pStream->Read(&hillsAttackPercent);
        pStream->Read(&hillsDefencePercent);
        pStream->Read(&immuneToFirstStrikes);
        pStream->Read(&isBlitz);
        pStream->Read(&isRiver);
        pStream->Read(&isAmphib);
        pStream->Read(&isBarbarian);
        pStream->Read(&isAnimal);
        pStream->Read(&extraCollateralDamage);
        pStream->Read(&collateralDamageProtection);
        readMap<FeatureTypes, int, int, int>(pStream, unitFeatureAttackModifiers);
        readMap<FeatureTypes, int, int, int>(pStream, unitFeatureDefenceModifiers);
        readMap<TerrainTypes, int, int, int>(pStream, unitTerrainAttackModifiers);
        readMap<TerrainTypes, int, int, int>(pStream, unitTerrainDefenceModifiers);
        readMap<UnitCombatTypes, int, int, int>(pStream, unitCombatModifiers);
        pStream->Read(&hasAttacked);
        readVector<PromotionTypes, int>(pStream, promotions);
        pStream->Read(&animalModifier);
        pStream->Read(&barbModifier);
    }

    CombatData::CombatData(const CvPlot* pPlot) : combatDetails(pPlot)
    {
    }

    void CombatData::write(FDataStreamBase* pStream) const
    {
        combatDetails.write(pStream);
        writeComplexVector(pStream, attackers);
        writeComplexVector(pStream, defenders);
    }

    void CombatData::read(FDataStreamBase* pStream)
    {
        combatDetails.read(pStream);
        readComplexVector(pStream, attackers);
        readComplexVector(pStream, defenders);
    }

    std::vector<UnitData> makeUnitData(const std::set<IDInfo>& units)
    {
        std::vector<UnitData> unitData;
        for (std::set<IDInfo>::const_iterator ci(units.begin()), ciEnd(units.end()); ci != ciEnd; ++ci)
        {
            const CvUnit* pUnit = ::getUnit(*ci);
            if (pUnit)
            {
                unitData.push_back(UnitData(pUnit));
            }
        }
        return unitData;
    }

    void debugUnitDataVector(std::ostream& os, const std::vector<UnitData>& units)
    {
        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            // todo - align with list version below
            units[i].debugPromotions(os);
        }
    }

    void debugUnitDataLists(std::ostream& os, const std::vector<std::list<UnitData> >& unitLists)
    {
        for (size_t i = 0, count = unitLists.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << "{";
            for (std::list<UnitData>::const_iterator choiceIter(unitLists[i].begin()), choiceEndIter(unitLists[i].end());
                choiceIter != choiceEndIter; ++choiceIter)
            {
                os << " " << choiceIter->pUnitInfo->getType();
                choiceIter->debugPromotions(os); 
            }
            os << " }";
        }
    }

    bool eraseUnitById(std::vector<UnitData>& units, IDInfo unitId)
    {
        std::vector<UnitData>::iterator iter = std::remove_if(units.begin(), units.end(), UnitDataIDInfoP(unitId));
        bool found = iter != units.end();
        units.erase(iter, units.end());
        return found;
    }

    void updateUnitData(std::vector<UnitData>& units)
    {
        std::vector<UnitData> newUnitData;
        newUnitData.reserve(units.size());

        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            const CvUnit* pUnit = ::getUnit(units[i].unitId);
            if (pUnit)
            {
                newUnitData.push_back(units[i]);
                newUnitData.rbegin()->hp = pUnit->currHitPoints();
            }
        }
        units = newUnitData;
    }

    std::vector<PromotionTypes> getAvailablePromotions(const CvUnit* pUnit)
    {
        std::vector<PromotionTypes> availablePromotions;

        for (int i = 0, count = gGlobals.getNumPromotionInfos(); i < count; ++i)
        {
            if (pUnit->canAcquirePromotion((PromotionTypes)i) && ::isPromotionValid((PromotionTypes)i, pUnit->getUnitType(), false))
            {
                availablePromotions.push_back((PromotionTypes)i);
            }
        }
        return availablePromotions;
    }

    UnitAnalysis::UnitAnalysis(const Player& player) : player_(player)
    {
    }

    void UnitAnalysis::init()
    {
        calculatePromotionDepths_();
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

                    if (unitInfo.getUnitAIType(UNITAI_ATTACK_CITY) || unitInfo.getUnitAIType(UNITAI_ATTACK) || unitInfo.getUnitAIType(UNITAI_COUNTER))
                    {
                        analyseAsCityAttackUnit_(unitType);
                    }

                    if (unitInfo.getUnitAIType(UNITAI_CITY_DEFENSE) || unitInfo.getUnitAIType(UNITAI_CITY_COUNTER) || unitInfo.getUnitAIType(UNITAI_COUNTER))
                    {
                        analyseAsCityDefenceUnit_(unitType);
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

                            CombatDataOdds combatData;

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
    
    void UnitAnalysis::promote(UnitData& unit, const UnitData::CombatDetails& combatDetails, bool isAttacker, int level, const Promotions& freePromotions) const
    {
        RemainingLevelsAndPromotions ourPromotions;

        if (combatDetails.flags & UnitData::CombatDetails::CityAttack)
        {
            if (isAttacker)
            {
                ourPromotions = getCityAttackPromotions(unit.unitType, level);
            }
            else
            {
                ourPromotions = getCityDefencePromotions(unit.unitType, level);
            }
        }

        // todo - check for free promotions which match ones given here and adjust our level appropriately
        if (ourPromotions.second.empty())
        {
            ourPromotions = getCombatPromotions(unit.unitType, level);
        }       
        combinePromotions(ourPromotions.second, freePromotions);

        for (Promotions::const_iterator ci(ourPromotions.second.begin()), ciEnd(ourPromotions.second.end()); ci != ciEnd; ++ci)
        {
            unit.applyPromotion(*ci);
        }
    }

    std::pair<int, int> UnitAnalysis::getOdds(UnitTypes unit1Type, UnitTypes unit2Type, int unit1Level, int unit2Level) const
    {
        const int attHP = gGlobals.getMAX_HIT_POINTS();
        const int defHP = attHP;
        const int attMaxHP = gGlobals.getMAX_HIT_POINTS();
        const int defMaxHP = attMaxHP;

        int attackOdds = 0, defenceOdds = 0;

        UnitData unit1(unit1Type), unit2(unit2Type);

        // todo - handle all air combat separately
        const CvUnitInfo& unit1Info = gGlobals.getUnitInfo(unit1Type), &unit2Info = gGlobals.getUnitInfo(unit2Type);
        if (unit2Info.getProductionCost() >= 0 && unit2.baseCombat > 0 && unit1Info.getDomainType() == unit2Info.getDomainType())
        {
            //Promotions combatPromotions = getCombatPromotions(unit1.getUnitClassType, unit1Level);

            attackOdds = getCombatOdds(unit1, unit2);
            defenceOdds = 1000 - getCombatOdds(unit2, unit1);
        }

        return std::make_pair(attackOdds, defenceOdds);
    }

    std::vector<int> UnitAnalysis::getOdds(UnitTypes unitType, const std::vector<UnitTypes>& units, int ourLevel, int theirLevel, 
        const UnitData::CombatDetails& combatDetails, bool isAttacker, const Promotions& freePromotions) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
        //os << "\ngetOdds():\n";
#endif
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        // we are attacker - so can't be defensive only
        if (isAttacker && unitInfo.isOnlyDefensive())
        {
            return std::vector<int>(units.size(), -1);
        }

        UnitData us(unitType);
        promote(us, combatDetails, isAttacker, ourLevel, freePromotions);
        return getOdds(us, units, theirLevel, combatDetails, isAttacker);
    }

    std::vector<int> UnitAnalysis::getOdds(const UnitData& unit, const std::vector<UnitTypes>& units, int level, const UnitData::CombatDetails& combatDetails, bool isAttacker) const
    {
        std::vector<int> odds;
        RemainingLevelsAndPromotions promotions;
        bool defensiveOnly = unit.pUnitInfo->isOnlyDefensive();

        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            const CvUnitInfo& otherUnitInfo = gGlobals.getUnitInfo(units[i]);

            bool otherUnitIsDefensiveOnly = otherUnitInfo.isOnlyDefensive();
            // units[i] are defending v. unit (isAttacker true), or units[i] are attacking and therefore they can't be defensive only
            if ((isAttacker && !defensiveOnly) || (!isAttacker && !otherUnitIsDefensiveOnly))
            {
                if (otherUnitInfo.getCombat() > 0 && unit.pUnitInfo->getDomainType() == otherUnitInfo.getDomainType())
                {
                    UnitData them(units[i]);
                    promote(them, combatDetails, !isAttacker, level, Promotions());

                    int ourOdds;

                    if (isAttacker)
                    {
                        ourOdds = getCombatOdds(unit, them, combatDetails);
#ifdef ALTAI_DEBUG
                        //os << "\n\t\tv. unit: " << otherUnitInfo.getType() << " our attack str = " << us.calculateStrength() 
                        //    << " their defence str = " << them.calculateStrength(us)
                        //    << " attacker combat = " << us.combat << " defender combat = " << them.combat << ", odds = " << ourOdds;
#endif
                    }
                    else
                    {
                        ourOdds = 1000 - getCombatOdds(them, unit, combatDetails);
#ifdef ALTAI_DEBUG
                        //os << "\n\t\tv. unit: " << otherUnitInfo.getType() << " their attack str = " << them.calculateStrength() 
                        //    << " our defence str = " << us.calculateStrength(them)
                        //    << " attacker combat = " << them.combat << " defender combat = " << us.combat << ", odds = " << ourOdds;
#endif
                    }

                    odds.push_back(ourOdds);
                    continue;
                }
            }
            odds.push_back(isAttacker ? 0 : (defensiveOnly ? 0 : 1000));  // add entry for each unit regardless of whether they could actually fight
            //odds.push_back(0);  // add entry for each unit regardless of whether they could actually fight
        }

        return odds;
    }

    std::vector<int> UnitAnalysis::getOdds(const UnitData& unit, const std::vector<UnitData>& units, const UnitData::CombatDetails& combatDetails, bool isAttacker) const
    {
        std::vector<int> odds;
        bool defensiveOnly = unit.pUnitInfo->isOnlyDefensive();

        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            // units[i] are defending v. unit (isAttacker true), or units[i] are attacking and therefore they can't be defensive only
            // or attacker has combat limit which exceeds defender's current hp
            if ((isAttacker ? canAttack(unit, units[i], combatDetails) : canAttack(units[i], unit, combatDetails))
                && units[i].pUnitInfo->getCombat() > 0 && unit.pUnitInfo->getDomainType() == units[i].pUnitInfo->getDomainType())
            {
                int ourOdds;
                if (isAttacker)
                {
                    ourOdds = getCombatOdds(unit, units[i], combatDetails);
                }
                else
                {
                    ourOdds = 1000 - getCombatOdds(units[i], unit, combatDetails);
                }
                odds.push_back(ourOdds);
            }
            else
            {
                // add zero for case of both attacker and defender being defensive only units
                // for case of defender being defensive only, above check will only skip odds calc
                // if attacker also happens to be defensive only, and then we don't want to add 1000 odds for a battle which can't occur
                //odds.push_back(isAttacker ? 0 : (defensiveOnly ? 0 : 1000));  // add entry for each unit regardless of whether they could actually fight
                odds.push_back(isAttacker ? 0 : 1000); // keep symmetry - need to be sure this doesn't break in the case of defendensive only units (and collateral edge cases)
            }
        }

        return odds;
    }

    UnitOddsData UnitAnalysis::getCombatOddsDetail(const UnitData& attacker, const UnitData& defender, const UnitData::CombatDetails& combatDetails) const
    {
        return getCombatOddsDetail_(attacker, defender, combatDetails);
    }

    std::vector<int> UnitAnalysis::getCollateralDamage(const UnitData& attacker, const std::vector<UnitData>& units, size_t skipIndex, const UnitData::CombatDetails& combatDetails) const
    {
        static const int COLLATERAL_COMBAT_DAMAGE = gGlobals.getDefineINT("COLLATERAL_COMBAT_DAMAGE");
        static const int MAX_HIT_POINTS = gGlobals.getMAX_HIT_POINTS();

        std::vector<int> damage(units.size(), 0);        
        
        const int numPossibleTargets = std::min<int>(attacker.pUnitInfo->getCollateralDamageMaxUnits(), units.size());
        if (numPossibleTargets <= 0)
        {
            return damage;
        }

		const int collateralStr = attacker.baseCombat * attacker.pUnitInfo->getCollateralDamage();
        const int collateralDamageLimit = attacker.pUnitInfo->getCollateralDamageLimit() * MAX_HIT_POINTS / 100;

		CvGameAI& refgame = gGlobals.getGame();
        std::multimap<int, size_t, std::greater<int> > potentialTargets;
        for (size_t i = 0, count = units.size(); i < count; ++i)
        {
            if (i == skipIndex) continue;
            // assumes all units in the list can defend and are visible
            potentialTargets.insert(std::make_pair((1 + refgame.getSorenRandNum(10000, "CollateralSim")) * units[i].hp, i));
        }

        std::multimap<int, size_t, std::greater<int> >::iterator targetIter = potentialTargets.begin();
        for (int targetIndex = 0; targetIndex < numPossibleTargets; ++targetIndex)
        {
            if (targetIter == potentialTargets.end())
            {
                break;
            }

            if (!units[targetIter->second].pUnitInfo->getUnitCombatCollateralImmune(attacker.pUnitInfo->getUnitCombatType()))
            {
                int iTheirStrength = units[targetIter->second].baseCombat;

				int iStrengthFactor = (collateralStr + iTheirStrength + 1) / 2;

				int iCollateralDamage = (COLLATERAL_COMBAT_DAMAGE * (collateralStr + iStrengthFactor)) / (iTheirStrength + iStrengthFactor);

                iCollateralDamage *= 100 + attacker.extraCollateralDamage;

				iCollateralDamage *= std::max<int>(0, 100 - units[targetIter->second].collateralDamageProtection);
				iCollateralDamage /= 100;

                // todo - add air modifier for city attacks from bombers (bunker gives protection)				

				iCollateralDamage /= 100;

				iCollateralDamage = std::max<int>(0, iCollateralDamage);

				int iMaxDamage = std::min<int>(collateralDamageLimit, (collateralDamageLimit * (collateralStr + iStrengthFactor)) / (iTheirStrength + iStrengthFactor));
                int iUnitDamage = std::max<int>(units[targetIter->second].maxhp - units[targetIter->second].hp, 
                    std::min<int>(units[targetIter->second].maxhp - units[targetIter->second].hp + iCollateralDamage, iMaxDamage));

				damage[targetIter->second] = iUnitDamage;
            }

            potentialTargets.erase(targetIter++);
        }

		return damage;
    }

    int UnitAnalysis::getCurrentUnitValue(UnitTypes unitType) const
    {
        int value = 0, maxValue = 0;
        const int minValue = 600;
        CombatDataMap::const_iterator ci = combatDataMap_.find(unitType);
        if (ci != combatDataMap_.end())
        {
            for (UnitCombatDataMap::const_iterator iter = ci->second.begin(), iterEnd = ci->second.end(); iter != iterEnd; ++iter)
            {
                maxValue += 2000;

                if (iter->second.attackOdds > minValue)
                {
                    value += iter->second.attackOdds;                 
                }                

                if (iter->second.defenceOdds > minValue)
                {
                    value += iter->second.defenceOdds;
                }                
            }
        }

        return std::max<int>(1, (1000 * value) / std::max<int>(1, maxValue));
    }

    int UnitAnalysis::getCityAttackUnitValue(UnitTypes unitType, int level) const
    {
        int value = 0, maxValue = 0;
        const int minValue = 600;
        UnitCombatInfoMap::const_iterator ci = cityAttackUnitValues_.find(unitType);
        if (ci != cityAttackUnitValues_.end())
        {
            UnitCombatLevelsMap::const_iterator levelsIter = ci->second.find(level);

            if (levelsIter != ci->second.end())
            {
                for (UnitCombatOddsMap::const_iterator AIIter(levelsIter->second.begin()), AIEndIter(levelsIter->second.end()); AIIter != AIEndIter; ++AIIter)
                {
                    for (UnitAITypesCombatOddsMap::const_iterator oddsIter(AIIter->second.begin()), oddsEndIter(AIIter->second.end()); oddsIter != oddsEndIter; ++oddsIter)
                    {
                        if (oddsIter->second.first > minValue)
                        {
                            value += oddsIter->second.first;
                        }
                        maxValue += 1000;
                    }
                }
            }
        }

        return std::max<int>(1, (1000 * value) / std::max<int>(1, maxValue));
    }

    int UnitAnalysis::getCityDefenceUnitValue(UnitTypes unitType) const
    {
        int value = 0, maxValue = 0;
        const int minValue = 600;
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
                        if (oddsIter->second.first > minValue)
                        {
                            value += oddsIter->second.first;
                        }
                        maxValue += 1000;
                    }
                }
            }
        }

        return std::max<int>(1, (1000 * value) / std::max<int>(1, maxValue));
    }

    int UnitAnalysis::getAttackUnitValue(UnitTypes unitType) const
    {
        int value = 0, maxValue = 0;
        const int minValue = 600;
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
                        if (oddsIter->second.first > minValue)
                        {
                            value += oddsIter->second.first;
                        }
                        maxValue += 1000;
                    }
                }
            }
        }

        return std::max<int>(1, (1000 * value) / std::max<int>(1, maxValue));
    }

    int UnitAnalysis::getDefenceUnitValue(UnitTypes unitType) const
    {
        int value = 0, maxValue = 0;
        const int minValue = 600;
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
                        if (oddsIter->second.first > minValue)
                        {
                            value += oddsIter->second.first;
                        }
                        maxValue += 1000;
                    }
                }
            }
        }

        return std::max<int>(1, (1000 * value) / std::max<int>(1, maxValue));
    }

    int UnitAnalysis::getUnitCounterValue(UnitTypes unitType, UnitCombatTypes unitCombatType) const
    {
        int value = 0, maxValue = 0;
        const int minValue = 600;
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
                            if (oddsIter->second.first > minValue)
                            {
                                value += oddsIter->second.first;
                            }
                            maxValue += 1000;
                        }
                    }
                }
            }
        }

        return (1000 * value) / std::max<int>(1, maxValue);
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

    UnitAnalysis::RemainingLevelsAndPromotions UnitAnalysis::getCollateralPromotions(UnitTypes unitType, int level) const
    {
        level = std::max<int>(0, level);
        level = std::min<int>(level, (int)collateralUnits_.size());

        for (UnitValuesMap::const_iterator ci(collateralUnits_[level].begin()), ciEnd(collateralUnits_[level].end()); ci != ciEnd; ++ci)
        {
            if (ci->second.first == unitType)
            {
                return ci->second.second;
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

                int collateral = promotionInfo.getCollateralDamageChange();
                if (collateral > 0)
                {
                    collateralPromotions_.insert(std::make_pair(collateral, (PromotionTypes)i));
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

    void UnitAnalysis::calculatePromotionDepths_()
    {
        std::stack<PromotionTypes> openList;
        promotionDepths_.resize(gGlobals.getNumPromotionInfos(), -1);

        for (int i = 0, count = gGlobals.getNumPromotionInfos(); i < count; ++i)
        {   
            openList.push((PromotionTypes)i);

            while (!openList.empty())
            {
                const CvPromotionInfo& promotionInfo = gGlobals.getPromotionInfo(openList.top());

                PromotionTypes andPrereq = (PromotionTypes)promotionInfo.getPrereqPromotion(), 
                    orPrereq1 = (PromotionTypes)promotionInfo.getPrereqOrPromotion1(),
                    orPrereq2 = (PromotionTypes)promotionInfo.getPrereqOrPromotion2();
                
                int depth = 0;
                int andDepth = -1, orDepth1 = -1, orDepth2 = -1;
                bool haveDepth = true;
                if (andPrereq != NO_PROMOTION)
                {
                    andDepth = promotionDepths_[andPrereq];
                    if (andDepth == -1)
                    {
                        haveDepth = false;
                        openList.push(andPrereq);
                    }
                }
                if (orPrereq1 != NO_PROMOTION)
                {
                    orDepth1 = promotionDepths_[orPrereq1];
                    if (orDepth1 == -1)
                    {
                        haveDepth = false;
                        openList.push(orPrereq1);
                    }
                }
                if (orPrereq2 != NO_PROMOTION)
                {
                    orDepth2 = promotionDepths_[orPrereq2];
                    if (orDepth2 == -1)
                    {
                        haveDepth = false;
                        openList.push(orPrereq2);
                    }
                }

                if (haveDepth)
                {
                    if (orPrereq1 != NO_PROMOTION)
                    {
                        if (orPrereq2 != NO_PROMOTION)
                        {
                            depth = 1 + std::max<int>(andDepth, std::min<int>(orDepth1, orDepth2));
                        }
                        else
                        {
                            // even if only an or prereq (no and one) - this will still work
                            depth = 1 + std::max<int>(andDepth, orDepth1);
                        }
                    }
                    else if (andPrereq != NO_PROMOTION)
                    {
                        depth = 1 + andDepth;
                    }
                    
                    promotionDepths_[(PromotionTypes)openList.top()] = depth;
                    openList.pop();
                }
            }
        }
    }

    int UnitAnalysis::getPromotionDepth(PromotionTypes promotionType) const
    {
        int depth = -1;
        if (promotionType != NO_PROMOTION)
        {
            depth = promotionDepths_[promotionType];
        }
        return depth;
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
        collateralUnits_.resize(1 + maxPromotionSearchDepth_, UnitValuesMap());

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

                        // city attack
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

                        // city defence
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

                        // combat
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

                        // first strikes
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

                        // collateral
                        int bestCollateral = 0, additionalCollateralCombat = 0;

                        boost::tie(bestCollateral, requiredPromotions) = calculateBestPromotions_(cityAttackPromotions_, unitInfo.getCollateralDamage(), 
                            pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getCollateralDamageChange), j);

                        // still have more promotions available
                        if (requiredPromotions.first > 0)
                        {
                            boost::tie(additionalCollateralCombat, requiredPromotions) = calculateBestPromotions_(combatPromotions_, 0, 
                                pUnitInfo, PromotionValueFunctor(&CvPromotionInfo::getCombatPercent), requiredPromotions.first, requiredPromotions.second);
                        }

                        if (bestCollateral > 0)
                        {
                            combinePromotions(requiredPromotions.second, freePromotions);
                            collateralUnits_[j].insert(std::make_pair(bestCollateral, std::make_pair(unitType, requiredPromotions)));
                        }

                        // unit counters
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

                        // 'fast' units
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
        //std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        if (unitInfo.isOnlyDefensive())
        {
//#ifdef ALTAI_DEBUG
//            os << "\nSkipping unit as only defensive: " << pUnitInfo->getType();
//#endif
            return;
        }
#ifdef ALTAI_DEBUG
        //os << "\nAnalysing unit: " << pUnitInfo->getType();
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
            UnitData attacker(unitType);
            for (Promotions::const_iterator ci(attackPromotions.second.begin()), ciEnd(attackPromotions.second.end()); ci != ciEnd; ++ci)
            {
#ifdef ALTAI_DEBUG
                //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
#endif
                attacker.applyPromotion(*ci);
            }

            UnitData::CombatDetails combatDetails;
            combatDetails.flags = UnitData::CombatDetails::CityAttack | UnitData::CombatDetails::FortifyDefender;

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
                        UnitData defender(defaultUnit);
                        for (Promotions::const_iterator ci(defencePromotions.second.begin()), ciEnd(defencePromotions.second.end()); ci != ciEnd; ++ci)
                        {
#ifdef ALTAI_DEBUG
                            //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
#endif
                            defender.applyPromotion(*ci);
                        }
                        
                        int attackOdds = getCombatOdds(attacker, defender, combatDetails);
                        int defenceOdds = getCombatOdds(defender, attacker, combatDetails);
#ifdef ALTAI_DEBUG
                        //os << "\n\t\tv. unit: " << gGlobals.getUnitInfo(defaultUnit).getType() << " attack str = " << attacker.calculateStrength(UnitData::CityAttack | UnitData::FortifyDefender) 
                        //   << " defence str = " << defender.calculateStrength(attacker, combatDetails)
                        //   << " attacker combat = " << attacker.combat << " defender combat = " << defender.combat
                        //   << " city attack % = " << attacker.cityAttackPercent
                        //   << "\n\t attack = " << attackOdds << ", defence = " << defenceOdds;
#endif                        
                        //UnitOddsData odds = getCombatOddsDetail_(attacker, defender, combatDetails);
#ifdef ALTAI_DEBUG
                        //odds.debug(os);
#endif

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
//#ifdef ALTAI_DEBUG
//        os << "\n final city attack value = ";
//        for (int i = 0; i < 5; ++i)
//        {
//            os << getCityAttackUnitValue(unitType, i) << ", ";
//        }
//        os << "\n final city defence value = " << getCityDefenceUnitValue(unitType);
//#endif
    }

    void UnitAnalysis::analyseAsCityDefenceUnit_(UnitTypes unitType)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//#endif
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

//#ifdef ALTAI_DEBUG
//        os << "\nAnalysing unit: " << pUnitInfo->getType();
//#endif
        for (int i = 1; i <= maxPromotionSearchDepth_; ++i)
        {
//#ifdef ALTAI_DEBUG
//            os << "\n\tfor level: " << i;
//#endif
            RemainingLevelsAndPromotions defencePromotions = getCityDefencePromotions(unitType, i);

            if (defencePromotions.second.empty())
            {
                defencePromotions = getCombatPromotions(unitType, i);
            }
//#ifdef ALTAI_DEBUG
//            os << " defence promotions = ";
//#endif
            UnitData defender(unitType);
            for (Promotions::const_iterator ci(defencePromotions.second.begin()), ciEnd(defencePromotions.second.end()); ci != ciEnd; ++ci)
            {
//#ifdef ALTAI_DEBUG
//                os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
//#endif
                defender.applyPromotion(*ci);
            }

            UnitData::CombatDetails combatDetails;
            combatDetails.flags = UnitData::CombatDetails::CityAttack | UnitData::CombatDetails::FortifyDefender;

            for (int j = 0, count = gGlobals.getNumUnitClassInfos(); j < count; ++j)
            {
                UnitTypes defaultUnit = (UnitTypes)gGlobals.getUnitClassInfo((UnitClassTypes)j).getDefaultUnitIndex();

                const CvUnitInfo& otherUnitInfo = gGlobals.getUnitInfo(defaultUnit);

                if (otherUnitInfo.getProductionCost() >= 0 && otherUnitInfo.getCombat() > 0 && unitInfo.getDomainType() == otherUnitInfo.getDomainType())
                {
                    if (otherUnitInfo.getUnitAIType(UNITAI_ATTACK_CITY) || otherUnitInfo.getUnitAIType(UNITAI_COUNTER))
                    {
                        RemainingLevelsAndPromotions attackPromotions = getCityAttackPromotions(defaultUnit, i);

                        if (attackPromotions.second.empty())
                        {
                            attackPromotions = getCombatPromotions(defaultUnit, i);
                        }
//#ifdef ALTAI_DEBUG
//                        os << "\n attacker promotions = ";
//#endif
                        UnitData attacker(defaultUnit);
                        for (Promotions::const_iterator ci(attackPromotions.second.begin()), ciEnd(attackPromotions.second.end()); ci != ciEnd; ++ci)
                        {
//#ifdef ALTAI_DEBUG
//                            os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
//#endif
                            attacker.applyPromotion(*ci);
                        }

                        int attackOdds = getCombatOdds(attacker, defender, combatDetails);
//#ifdef ALTAI_DEBUG
//                        os << "\n\t\tv. unit: " << gGlobals.getUnitInfo(defaultUnit).getType() << " attack str = " << attacker.calculateStrength(UnitData::CityAttack | UnitData::FortifyDefender) 
//                           << " defence str = " << defender.calculateStrength(attacker, combatDetails)
//                           << " attacker combat = " << attacker.combat << " defender combat = " << defender.combat
//                           << " city attack % = " << attacker.cityAttackPercent << " city def % = " << defender.cityDefencePercent
//                           << " att fs = " << attacker.firstStrikes << " att fs chances = " << attacker.chanceFirstStrikes
//                           << " def fs = " << defender.firstStrikes << " def fs chances = " << defender.chanceFirstStrikes;
//#endif
                        int defenceOdds = getCombatOdds(defender, attacker, combatDetails);
//#ifdef ALTAI_DEBUG
//                        os << "\n\t\t\t" << " attack str = " << defender.calculateStrength(0) 
//                           << " defence str = " << attacker.calculateStrength(defender, 0)
//                           << " attacker combat = " << defender.combat << " defender combat = " << attacker.combat
//                           << " city attack % = " << defender.cityAttackPercent << " city def % = " << attacker.cityDefencePercent
//                           << " att fs = " << defender.firstStrikes << " att fs chances = " << defender.chanceFirstStrikes
//                           << " def fs = " << attacker.firstStrikes << " def fs chances = " << attacker.chanceFirstStrikes;
//#endif

//#ifdef ALTAI_DEBUG
//                        os << "\n\t attack = " << attackOdds << ", defence = " << defenceOdds;
//#endif
                    }
                }
            }
        }
    }

    void UnitAnalysis::analyseAsCombatUnit_(UnitTypes unitType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
#endif
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        if (unitInfo.isOnlyDefensive())
        {
//#ifdef ALTAI_DEBUG
//            os << "\nSkipping unit as only defensive: " << pUnitInfo->getType();
//#endif
            return;
        }
//#ifdef ALTAI_DEBUG
//        os << "\nAnalysing unit: " << pUnitInfo->getType();
//#endif
        for (int i = 1; i <= maxPromotionSearchDepth_; ++i)
        {
//#ifdef ALTAI_DEBUG
//        os << "\n\tfor level: " << i;
//#endif
            RemainingLevelsAndPromotions combatPromotions = getCombatPromotions(unitType, i);
//#ifdef ALTAI_DEBUG
//            os << " attack promotions = ";
//#endif
            UnitData attacker(unitType);
            for (Promotions::const_iterator ci(combatPromotions.second.begin()), ciEnd(combatPromotions.second.end()); ci != ciEnd; ++ci)
            {
//#ifdef ALTAI_DEBUG
//                os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
//#endif
                attacker.applyPromotion(*ci);
            }

            UnitData::CombatDetails combatDetails;

            for (int j = 0, count = gGlobals.getNumUnitClassInfos(); j < count; ++j)
            {
                UnitTypes defaultUnit = (UnitTypes)gGlobals.getUnitClassInfo((UnitClassTypes)j).getDefaultUnitIndex();

                const CvUnitInfo& otherUnitInfo = gGlobals.getUnitInfo(defaultUnit);

                if (otherUnitInfo.getProductionCost() >= 0 && otherUnitInfo.getCombat() > 0 && unitInfo.getDomainType() == otherUnitInfo.getDomainType())
                {
                    if (otherUnitInfo.getUnitAIType(UNITAI_COUNTER) || otherUnitInfo.getUnitAIType(UNITAI_ATTACK_SEA) || otherUnitInfo.getUnitAIType(UNITAI_ASSAULT_SEA))
                    {
                        RemainingLevelsAndPromotions defencePromotions = getCombatPromotions(defaultUnit, i);
//#ifdef ALTAI_DEBUG
//                        os << "\n defender promotions = ";
//#endif
                        UnitData defender(defaultUnit);
                        for (Promotions::const_iterator ci(defencePromotions.second.begin()), ciEnd(defencePromotions.second.end()); ci != ciEnd; ++ci)
                        {
//#ifdef ALTAI_DEBUG
//                            os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
//#endif
                            defender.applyPromotion(*ci);
                        }

                        int attackOdds = getCombatOdds(attacker, defender);
                        int defenceOdds = getCombatOdds(defender, attacker);

//#ifdef ALTAI_DEBUG
//                        os << "\n\t\tv. unit: " << gGlobals.getUnitInfo(defaultUnit).getType() << " attack str = " << attacker.calculateStrength(combatDetails) 
//                           << " defence str = " << defender.calculateStrength(attacker, combatDetails)
//                           << " attacker combat = " << attacker.combat << " defender combat = " << defender.combat
//                           << "\n\t attack = " << attackOdds << ", defence = " << defenceOdds;
//#endif

                        UnitOddsData odds = getCombatOddsDetail_(attacker, defender);
//#ifdef ALTAI_DEBUG
//                        odds.debug(os);
//#endif

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
//#ifdef ALTAI_DEBUG
//        os << "\n final attack value = " << getAttackUnitValue(unitType);
//        os << "\n final defence value = " << getDefenceUnitValue(unitType);
//#endif
    }

    void UnitAnalysis::analyseAsCounterUnit_(UnitTypes unitType)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//#endif
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        if (unitInfo.isOnlyDefensive())
        {
//#ifdef ALTAI_DEBUG
//            os << "\nSkipping unit as only defensive: " << pUnitInfo->getType();
//#endif
            return;
        }
//#ifdef ALTAI_DEBUG
//        os << "\nAnalysing unit: " << pUnitInfo->getType();
//#endif
        for (int i = 1; i <= maxPromotionSearchDepth_; ++i)
        {
//#ifdef ALTAI_DEBUG
//            //os << "\n\tfor level: " << i;
//#endif
            for (int j = 0, count = gGlobals.getNumUnitClassInfos(); j < count; ++j)
            {
                UnitTypes defaultUnit = (UnitTypes)gGlobals.getUnitClassInfo((UnitClassTypes)j).getDefaultUnitIndex();

                const CvUnitInfo& otherUnitInfo = gGlobals.getUnitInfo(defaultUnit);

                if (otherUnitInfo.getProductionCost() >= 0 && otherUnitInfo.getCombat() > 0 && otherUnitInfo.getDomainType() == otherUnitInfo.getDomainType())
                {
                    RemainingLevelsAndPromotions combatPromotions = getCombatCounterPromotions(unitType, (UnitCombatTypes)otherUnitInfo.getUnitCombatType(), i);
//#ifdef ALTAI_DEBUG
//                    //os << " attack promotions = ";
//#endif
                    UnitData attacker(unitType);
                    for (Promotions::const_iterator ci(combatPromotions.second.begin()), ciEnd(combatPromotions.second.end()); ci != ciEnd; ++ci)
                    {
//#ifdef ALTAI_DEBUG
//                        //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
//#endif
                        attacker.applyPromotion(*ci);
                    }

                    {
                        RemainingLevelsAndPromotions defencePromotions = getCombatPromotions(defaultUnit, i);
//#ifdef ALTAI_DEBUG
//                        //os << "\n defender promotions = ";
//#endif
                        UnitData defender(defaultUnit);
                        for (Promotions::const_iterator ci(defencePromotions.second.begin()), ciEnd(defencePromotions.second.end()); ci != ciEnd; ++ci)
                        {
//#ifdef ALTAI_DEBUG
//                            //os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
//#endif
                            defender.applyPromotion(*ci);
                        }

                        int attackOdds = getCombatOdds(attacker, defender);
                        int defenceOdds = getCombatOdds(defender, attacker);
//#ifdef ALTAI_DEBUG
//                        /*os << "\n\t\tv. unit: " << gGlobals.getUnitInfo(defaultUnit).getType() << " attack str = " << attacker.calculateStrength(0) 
//                            << " defence str = " << defender.calculateStrength(attacker, 0)
//                            << " attacker combat = " << attacker.combat << " defender combat = " << defender.combat;*/
//#endif
                        if (attackOdds > 0)
                        {
                            attackUnitCounterValues_[unitType][i][defaultUnit].insert(std::make_pair(UNITAI_ATTACK, std::make_pair(attackOdds, 1000 - defenceOdds)));
                        }
                    }
                }
            }
        }
//#ifdef ALTAI_DEBUG
//        for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
//        {
//            os << "\n final counter value (" << gGlobals.getUnitCombatInfo((UnitCombatTypes)i).getType() << ") = " << getUnitCounterValue(unitType, (UnitCombatTypes)i);
//        }
//#endif
    }

    void UnitAnalysis::debug()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();

        os << "\nPromotion depths: ";
        for (size_t i = 0, count = promotionDepths_.size(); i < count; ++i)
        {
            os << "\n\t" << gGlobals.getPromotionInfo((PromotionTypes)i).getType() << " = " << promotionDepths_[i];
        }

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