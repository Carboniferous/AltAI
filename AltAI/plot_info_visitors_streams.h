#pragma once

#include "./plot_info.h"
#include "./city_data.h"

namespace AltAI
{
    // plot info debug data
    std::string getPlotDebugString(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType);


    std::ostream& operator << (std::ostream& os, const PlotInfo::NullNode& node);
    std::ostream& operator << (std::ostream& os, const PlotInfo::BaseNode& node);
    std::ostream& operator << (std::ostream& os, const PlotInfo::ImprovementNode& node);
    std::ostream& operator << (std::ostream& os, const PlotInfo::UpgradeNode& node);
    std::ostream& operator << (std::ostream& os, const PlotInfo::FeatureRemovedNode& node);

    std::ostream& operator << (std::ostream& os, const PlotInfo::HasTech& node);
    std::ostream& operator << (std::ostream& os, const PlotInfo::BuildOrCondition& node);

}