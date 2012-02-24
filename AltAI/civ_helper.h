#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class CivHelper
    {
    public:
        explicit CivHelper(const CvPlayer& player);
        explicit CivHelper(PlayerTypes playerType);
        CivHelper(const CivHelper& other);

        bool hasTech(TechTypes techType) const;
        void addTech(TechTypes techType);
        void removeTech(TechTypes techType);
        void addAllTechs();

        bool isResearchingTech(TechTypes techType);
        void pushResearchTech(TechTypes techType);
        void clearResearchTechs();
        const std::list<TechTypes>& getResearchTechs() const;

        bool isInCivic(CivicTypes civicType) const;
        CivicTypes currentCivic(CivicOptionTypes civicOptionType) const;
        bool civicIsAvailable(CivicTypes civicType);
        void addCivic(CivicTypes civicType);
        void adoptCivic(CivicTypes civicType);
        void makeAllCivicsAvailable();

    private:
        void init_();

        const CvPlayer& player_;
        std::set<TechTypes> techs_;
        std::list<TechTypes> techsToResearch_;
        std::set<CivicTypes> availableCivics_;
        std::vector<CivicTypes> currentCivics_;
    };

    typedef boost::shared_ptr<CivHelper> CivHelperPtr;
}