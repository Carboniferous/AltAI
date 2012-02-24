#pragma once

#include "./project_info.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const ProjectInfo::NullNode& node);
    std::ostream& operator << (std::ostream& os, const ProjectInfo::VictoryNode& node);
    std::ostream& operator << (std::ostream& os, const ProjectInfo::BonusNode& node);
    std::ostream& operator << (std::ostream& os, const ProjectInfo::PrereqNode& node);
    std::ostream& operator << (std::ostream& os, const ProjectInfo::MiscNode& node);
    std::ostream& operator << (std::ostream& os, const ProjectInfo::BaseNode& node);
}
