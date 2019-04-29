#pragma once

#include "./utils.h"

namespace AltAI
{
    class Player;
    class UnitInfo;

    typedef std::set<PromotionTypes> Promotions;

    struct UnitOddsData
    {
        UnitOddsData() : E_HP_Att(0.0f), E_HP_Def(0.0f), E_HP_Att_Withdraw(0.0f), E_HP_Att_Victory(0.0f), E_HP_Att_Retreat(0.0f), E_HP_Def_Withdraw(0.0f), E_HP_Def_Defeat(0.0f),
            AttackerKillOdds(0.0f), PullOutOdds(0.0f), RetreatOdds(0.0f), DefenderKillOdds(0.0f)
        {
        }

        void debug(std::ostream& os) const;

        float E_HP_Att;
        float E_HP_Def;
        float E_HP_Att_Withdraw;
        float E_HP_Att_Victory;
        float E_HP_Att_Retreat;
        float E_HP_Def_Withdraw;
        float E_HP_Def_Defeat;

        float AttackerKillOdds;
        float PullOutOdds; // Withdraw odds
        float RetreatOdds;
        float DefenderKillOdds;
    };

    struct UnitData
    {
        struct CombatDetails
        {
            enum Flags
            {
                None = 0, CityAttack = (1 << 0), FortifyDefender = (1 << 1)
            };

            CombatDetails() : flags(None), plotIsHills(false), plotTerrain(NO_TERRAIN), plotFeature(NO_FEATURE), cultureDefence(0), buildingDefence(0)
            {
            }

            explicit CombatDetails(const CvPlot* pPlot);

            int flags;
            bool plotIsHills;
            TerrainTypes plotTerrain;
            FeatureTypes plotFeature;
            int cultureDefence, buildingDefence;
        };

        UnitData();
        explicit UnitData(const CvUnit* pUnit);
        explicit UnitData(UnitTypes unitType_, const Promotions& promotions_ = Promotions());

        void UnitData::promote(int level, const Promotions& freePromotions);
        void applyPromotion(PromotionTypes promotionType);

        void debug(std::ostream& os) const;

        // calculate our strength as defender
        int calculateStrength(const UnitData& other, const CombatDetails& combatDetails = CombatDetails()) const;

        // calculate our strength as attacker
        int calculateStrength(const CombatDetails& combatDetails = CombatDetails()) const;

        const CvUnitInfo* pUnitInfo;
        UnitTypes unitType;

        int hp, maxhp;
        int baseCombat, extraCombat, firstStrikes, chanceFirstStrikes, firePower, combatLimit, withdrawalProb;
        int cityAttackPercent, cityDefencePercent, fortifyPercent;
        int hillsAttackPercent, hillsDefencePercent;
        bool immuneToFirstStrikes;
        int extraCollateralDamage, collateralDamageProtection;
        std::map<FeatureTypes, int> unitFeatureAttackModifiers, unitFeatureDefenceModifiers;
        std::map<TerrainTypes, int> unitTerrainAttackModifiers, unitTerrainDefenceModifiers;
        std::map<UnitCombatTypes, int> unitCombatModifiers;

        std::vector<PromotionTypes> promotions;
    };

    struct UnitTypeP
    {
        explicit UnitTypeP(UnitTypes unitType_) : unitType(unitType_) {}
        bool operator() (const UnitData& unitData) const
        {
            return unitData.unitType == unitType;
        }
        UnitTypes unitType;
    };

    class UnitAnalysis
    {
    public:
        explicit UnitAnalysis(const Player& player);

        void init();
        void debug();

        int getCurrentUnitValue(UnitTypes unitType) const;
        int getCityAttackUnitValue(UnitTypes unitType, int level) const;
        int getCityDefenceUnitValue(UnitTypes unitType) const;
        int getAttackUnitValue(UnitTypes unitType) const;
        int getDefenceUnitValue(UnitTypes unitType) const;
        int getUnitCounterValue(UnitTypes unitType, UnitCombatTypes unitCombatType) const;
        
        typedef int (CvPromotionInfo::*CvPromotionInfoIntFnPtr)(void) const;
        typedef int (CvPromotionInfo::*CvPromotionInfoUnitCombatIntFnPtr)(int) const;

        typedef std::multimap<int, PromotionTypes, std::greater<int> > PromotionsMap;
        typedef std::pair<int, Promotions> RemainingLevelsAndPromotions;
        typedef std::pair<UnitTypes, RemainingLevelsAndPromotions> UnitAndRequiredPromotions;
        typedef std::multimap<int, UnitAndRequiredPromotions, std::greater<int> > UnitValuesMap;
        typedef std::vector<UnitValuesMap> UnitLevels;

        RemainingLevelsAndPromotions getCityAttackPromotions(UnitTypes unitType, int level) const;
        RemainingLevelsAndPromotions getCityDefencePromotions(UnitTypes unitType, int level) const;
        RemainingLevelsAndPromotions getCombatPromotions(UnitTypes unitType, int level) const;
        RemainingLevelsAndPromotions getCollateralPromotions(UnitTypes unitType, int level) const;
        RemainingLevelsAndPromotions getCombatCounterPromotions(UnitTypes unitType, UnitCombatTypes unitCombatType, int level) const;

        void promote(UnitData& unit, const UnitData::CombatDetails& combatDetails, bool isAttacker, int level, const Promotions& freePromotions) const;

        std::pair<int, int> getOdds(UnitTypes unit1Type, UnitTypes unit2Type, int unit1Level, int unit2Level) const;

        // isAttacker indicates if the first unit is attacking each of the units in the second list
        std::vector<int> getOdds(UnitTypes unitType, const std::vector<UnitTypes>& units, 
            int ourLevel, int theirLevel, const UnitData::CombatDetails& combatDetails, bool isAttacker, const Promotions& freePromotions = Promotions()) const;

        std::vector<int> getOdds(const UnitData& unit, const std::vector<UnitTypes>& units, int ourLevel, const UnitData::CombatDetails& combatDetails, bool isAttacker) const;

        std::vector<int> getOdds(const UnitData& unit, const std::vector<UnitData>& units, const UnitData::CombatDetails& combatDetails, bool isAttacker) const;

        UnitOddsData getCombatOddsDetail(const UnitData& attacker, const UnitData& defender, const UnitData::CombatDetails& combatDetails = UnitData::CombatDetails()) const;

        std::vector<int> getCollateralDamage(const UnitData& attacker, const std::vector<UnitData>& units, size_t skipIndex, const UnitData::CombatDetails& combatDetails) const;

    private:

        static const int maxPromotionSearchDepth_ = 5;

        void analysePromotions_();
        void analyseUnits_();

        template <typename ValueF>
            std::pair<int, RemainingLevelsAndPromotions>
                calculateBestPromotions_(const PromotionsMap& promotionsMap, int baseValue, const boost::shared_ptr<UnitInfo>& pUnitInfo,
                    ValueF valueF, int level, const Promotions& existingPromotions = Promotions()) const;

        void analyseAsCityAttackUnit_(UnitTypes unitType);
        void analyseAsCityDefenceUnit_(UnitTypes unitType);
        void analyseAsCombatUnit_(UnitTypes unitType);
        void analyseAsCounterUnit_(UnitTypes unitType);

        struct CombatData
        {
            CombatData() : attackOdds(0), defenceOdds(0), cityAttackOdds(0), cityDefenceOdds(0)
            {
            }

            int attackOdds, defenceOdds, cityAttackOdds, cityDefenceOdds;
        };

        typedef std::map<UnitTypes, CombatData> UnitCombatDataMap;
        typedef std::map<UnitTypes, UnitCombatDataMap> CombatDataMap;

        const Player& player_;
        CombatDataMap combatDataMap_;

        PromotionsMap cityAttackPromotions_, cityDefencePromotions_, combatPromotions_, firstStrikePromotions_, movementPromotions_, collateralPromotions_;
        std::map<UnitCombatTypes, PromotionsMap> unitCounterPromotionsMap_;
        
        UnitLevels cityAttackUnits_, cityDefenceUnits_, combatUnits_, firstStrikeUnits_, fastUnits_, collateralUnits_;
        std::map<UnitCombatTypes, UnitLevels> unitCounterUnits_;

        typedef std::map<UnitAITypes, std::pair<int, int> > UnitAITypesCombatOddsMap;
        typedef std::map<UnitTypes, UnitAITypesCombatOddsMap> UnitCombatOddsMap;
        typedef std::map<int, UnitCombatOddsMap> UnitCombatLevelsMap;
        typedef std::map<UnitTypes, UnitCombatLevelsMap> UnitCombatInfoMap;

        UnitCombatInfoMap attackUnitValues_, defenceUnitValues_;
        UnitCombatInfoMap attackUnitCounterValues_, defenceUnitCounterValues_;
        UnitCombatInfoMap cityAttackUnitValues_, cityDefenceUnitValues_;
    };
}