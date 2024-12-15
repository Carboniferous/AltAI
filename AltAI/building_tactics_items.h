#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class EconomicBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::EconomicBuildingTacticID;
    };

    class FoodBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::FoodBuildingTacticID;
    };

    class HappyBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::HappyBuildingTacticID;
    };

    class HealthBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::HealthBuildingTacticID;
    };

    class ScienceBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::ScienceBuildingTacticID;
    };

    class GoldBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::GoldBuildingTacticID;
    };

    class CultureBuildingTactic : public ICityBuildingTactic
    {
    public:
        CultureBuildingTactic() : baseCommerce(0), baseGlobalCommerce(0) {}
        CultureBuildingTactic(int baseCommerce_, int baseGlobalCommerce_)
            : baseCommerce(baseCommerce_), baseGlobalCommerce(baseGlobalCommerce_)
        {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::CultureBuildingTacticID;
        int baseCommerce, baseGlobalCommerce;
    };

    class EspionageBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::EspionageBuildingTacticID;
    };

    class SpecialistBuildingTactic : public ICityBuildingTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::SpecialistBuildingTacticID;
    };

    class GovCenterTactic : public ICityBuildingTactic
    {
    public:
        GovCenterTactic() : isNewCenter_(false) {}
        explicit GovCenterTactic(bool isNewCenter);
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::GovCenterTacticID;

    private:
        bool isNewCenter_;
    };

    class UnitExperienceTactic : public ICityBuildingTactic
    {
    public:
        UnitExperienceTactic() : freeExperience(0), globalFreeExperience(0), freePromotion(NO_PROMOTION) {}
        UnitExperienceTactic(int freeExperience_, int globalFreeExperience_, 
            const std::vector<std::pair<DomainTypes, int> >& domainFreeExperience_,
            const std::vector<std::pair<UnitCombatTypes, int> >& combatTypeFreeExperience_,
            PromotionTypes freePromotion_);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::UnitExperienceTacticID;

    private:
        int freeExperience, globalFreeExperience;
        std::map<DomainTypes, int> domainFreeExperience;
        std::map<UnitCombatTypes, int> combatTypeFreeExperience;
        PromotionTypes freePromotion;
    };

    class CityDefenceBuildingTactic : public ICityBuildingTactic
    {
    public:
        CityDefenceBuildingTactic() : cityDefence(0), globalCityDefence(0), bombardDefence(0) {}
        CityDefenceBuildingTactic(int cityDefence_, int globalCityDefence_, int bombardDefence_)
         : cityDefence(cityDefence_), globalCityDefence(globalCityDefence_), bombardDefence(bombardDefence_)
        {
        }

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::CityDefenceBuildingTacticID;

    private:
        int cityDefence, globalCityDefence, bombardDefence;
    };

    class FreeTechBuildingTactic : public ICityBuildingTactic
    {
    public:
        FreeTechBuildingTactic() {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::FreeTechBuildingTacticID;
    };

    class CanTrainUnitBuildingTactic : public ICityBuildingTactic
    {
    public:
        CanTrainUnitBuildingTactic() : enabledUnitType_(NO_UNIT) {}
        explicit CanTrainUnitBuildingTactic(UnitTypes enabledUnitType) : enabledUnitType_(enabledUnitType) {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityBuildingTactic::CanTrainUnitBuildingTacticID;

    private:
        UnitTypes enabledUnitType_;
    };
}