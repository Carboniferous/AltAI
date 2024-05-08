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

        static const int ID = 0;

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

        static const int ID = 1;

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

        static const int ID = 2;

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

        static const int ID = 3;

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

        static const int ID = 4;

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

        static const int ID = 5;

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

        static const int ID = 6;
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

        static const int ID = 7;

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

        static const int ID = 8;

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

        static const int ID = 9;

    private:
        Promotions promotions_;
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

        static const int ID = 0;
    };

    class BuildSpecialBuildingUnitTactic : public IBuiltUnitTactic
    {
    public:
        BuildSpecialBuildingUnitTactic() {}
        explicit BuildSpecialBuildingUnitTactic(BuildingTypes buildingType);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 1;

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

        static const int ID = 2;
    };

    class TradeMissionUnitTactic : public IBuiltUnitTactic
    {
    public:
        TradeMissionUnitTactic() {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 3;
    };

    class JoinCityUnitTactic : public IBuiltUnitTactic
    {
    public:
        JoinCityUnitTactic() {}
        explicit JoinCityUnitTactic(SpecialistTypes specType);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 4;
    private:
        SpecialistTypes specType_;
    };

    class HurryBuildingUnitTactic : public IBuiltUnitTactic
    {
    public:
        HurryBuildingUnitTactic() {}
        HurryBuildingUnitTactic(int baseHurry, int multiplier);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const UnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 5;
    private:
        int baseHurry_, multiplier_;
    };
}