#pragma once

#include "./utils.h"

namespace AltAI
{
    class Player;
    class UnitInfo;

	typedef std::set<PromotionTypes> Promotions;

    class UnitAnalysis
    {
    public:
        explicit UnitAnalysis(const Player& player);

        void init();
        void debug();

        int getCurrentUnitValue(UnitTypes unitType) const;
        int getCityAttackUnitValue(UnitTypes unitType) const;
        int getCityDefenceUnitValue(UnitTypes unitType) const;
        int getAttackUnitValue(UnitTypes unitType) const;
        int getDefenceUnitValue(UnitTypes unitType) const;
        int getUnitCounterValue(UnitTypes unitType, UnitCombatTypes unitCombatType) const;

        std::pair<int, int> getOdds(const CvUnitInfo& unit1, const CvUnitInfo& unit2, int unit1Level, int unit2Level) const;

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
        RemainingLevelsAndPromotions getCombatCounterPromotions(UnitTypes unitType, UnitCombatTypes unitCombatType, int level) const;

    private:

        static const int maxPromotionSearchDepth_ = 5;

        void analysePromotions_();
        void analyseUnits_();

		template <typename ValueF>
            std::pair<int, RemainingLevelsAndPromotions>
                calculateBestPromotions_(const PromotionsMap& promotionsMap, int baseValue, const boost::shared_ptr<UnitInfo>& pUnitInfo,
					ValueF valueF, int level, const Promotions& existingPromotions = Promotions()) const;

		void analyseAsCityAttackUnit_(UnitTypes unitType);
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

        PromotionsMap cityAttackPromotions_, cityDefencePromotions_, combatPromotions_, firstStrikePromotions_, movementPromotions_;
        std::map<UnitCombatTypes, PromotionsMap> unitCounterPromotionsMap_;
        
        UnitLevels cityAttackUnits_, cityDefenceUnits_, combatUnits_, firstStrikeUnits_, fastUnits_;
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