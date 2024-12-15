#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"
#include "./city_improvements.h"

namespace AltAI
{
    typedef std::set<PromotionTypes> Promotions;

    class CityDefenceUnitTactic : public ICityUnitTactic
    {
    public:
        CityDefenceUnitTactic() {}
        explicit CityDefenceUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::CityDefenceUnitTacticID;

    private:
        Promotions promotions_;
    };

    class ThisCityDefenceUnitTactic : public ICityUnitTactic
    {
    public:
        ThisCityDefenceUnitTactic() {}
        explicit ThisCityDefenceUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::ThisCityDefenceUnitTacticID;

    private:
        Promotions promotions_;
    };

    class CityAttackUnitTactic : public ICityUnitTactic
    {
    public:
        CityAttackUnitTactic() {}
        explicit CityAttackUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::CityAttackUnitTacticID;

    private:
        Promotions promotions_;
    };

    class CollateralUnitTactic : public ICityUnitTactic
    {
    public:
        CollateralUnitTactic() {}
        explicit CollateralUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::CollateralUnitTacticID;

    private:
        Promotions promotions_;
    };

    class FieldDefenceUnitTactic : public ICityUnitTactic
    {
    public:
        FieldDefenceUnitTactic() {}
        explicit FieldDefenceUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::FieldDefenceUnitTacticID;

    private:
        Promotions promotions_;
    };

    class FieldAttackUnitTactic : public ICityUnitTactic
    {
    public:
        FieldAttackUnitTactic() {}
        explicit FieldAttackUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::FieldAttackUnitTacticID;

    private:
        Promotions promotions_;
    };

    class BuildCityUnitTactic : public ICityUnitTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::BuildCityUnitTacticID;
    };   

    struct WorkerUnitValue;

    class BuildImprovementsUnitTactic : public ICityUnitTactic
    {
    public:
        BuildImprovementsUnitTactic() : hasConsumedBuilds_(false) {}
        explicit BuildImprovementsUnitTactic(const std::vector<BuildTypes>& buildTypes);
        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::BuildImprovementsUnitTacticID;

    private:
        void applyBuilds_(WorkerUnitValue& unitValue, IDInfo city, const std::vector<PlotImprovementData>& improvements, 
            TotalOutput totalDelta = TotalOutput(), const std::vector<TechTypes>& impTechs = std::vector<TechTypes>());

        std::vector<BuildTypes> buildTypes_;
        bool hasConsumedBuilds_;
    };

    class SeaAttackUnitTactic : public ICityUnitTactic
    {
    public:
        SeaAttackUnitTactic() {}
        explicit SeaAttackUnitTactic(const Promotions& promotions);
        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::SeaAttackUnitTacticID;

    private:
        Promotions promotions_;
    };

    class ScoutUnitTactic : public ICityUnitTactic
    {
    public:
        ScoutUnitTactic() {}
        explicit ScoutUnitTactic(const Promotions& promotions);
        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::ScoutUnitTacticID;

    private:
        Promotions promotions_;
    };

    class SpreadReligionUnitTactic : public ICityUnitTactic
    {
    public:
        SpreadReligionUnitTactic() {}
        SpreadReligionUnitTactic(const std::vector<std::pair<ReligionTypes, int> >& religionSpreads);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);
        virtual std::vector<XYCoords> getPossibleTargets(Player& player, IDInfo city);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = ICityUnitTactic::SpreadReligionUnitTacticID;
    private:
        std::vector<std::pair<ReligionTypes, int> > religionSpreads_;
    };

    // specialist tactics:

    class DiscoverTechUnitTactic : public IBuiltUnitTactic
    {
    public:
        DiscoverTechUnitTactic() {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IBuiltUnitTactic::DiscoverTechUnitTacticID;
    };

    class BuildSpecialBuildingUnitTactic : public IBuiltUnitTactic
    {
    public:
        BuildSpecialBuildingUnitTactic() : buildingType_(NO_BUILDING) {}
        explicit BuildSpecialBuildingUnitTactic(BuildingTypes buildingType);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IBuiltUnitTactic::BuildSpecialBuildingUnitTacticID;

    private:
        BuildingTypes buildingType_;
    };
    
    class CreateGreatWorkUnitTactic : public IBuiltUnitTactic
    {
    public:
        CreateGreatWorkUnitTactic() {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IBuiltUnitTactic::CreateGreatWorkUnitTacticID;
    };

    class TradeMissionUnitTactic : public IBuiltUnitTactic
    {
    public:
        TradeMissionUnitTactic() {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IBuiltUnitTactic::TradeMissionUnitTacticID;
    };

    class JoinCityUnitTactic : public IBuiltUnitTactic
    {
    public:
        JoinCityUnitTactic() : specType_(NO_SPECIALIST) {}
        explicit JoinCityUnitTactic(SpecialistTypes specType);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IBuiltUnitTactic::JoinCityUnitTacticID;
    private:
        SpecialistTypes specType_;
    };

    class HurryBuildingUnitTactic : public IBuiltUnitTactic
    {
    public:
        HurryBuildingUnitTactic() : baseHurry_(0), multiplier_(0) {}
        HurryBuildingUnitTactic(int baseHurry, int multiplier);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IBuiltUnitTactic::HurryBuildingUnitTacticID;
    private:
        int baseHurry_, multiplier_;
    };

    
}