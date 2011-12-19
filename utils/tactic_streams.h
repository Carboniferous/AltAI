#pragma once

#include "./tactic_actions.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const ConstructItem& node);
    std::ostream& operator << (std::ostream& os, const ResearchTech& node);
    
    std::ostream& operator << (std::ostream& os, EconomicFlags economicFlags);
    std::ostream& operator << (std::ostream& os, MilitaryFlags militaryFlags);
    std::ostream& operator << (std::ostream& os, TechFlags techFlags);
    std::ostream& operator << (std::ostream& os, WorkerFlags workerFlags);

    void streamEconomicFlags(std::ostream& os, int flags);
    void streamMilitaryFlags(std::ostream& os, int flags);
    void streamTechFlags(std::ostream& os, int flags);
    void streamWorkerFlags(std::ostream& os, int flags);
}