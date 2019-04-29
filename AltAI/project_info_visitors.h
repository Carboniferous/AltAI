#pragma once

#include "./utils.h"

namespace AltAI
{
    class ProjectInfo;
    class Player;
    class City;

    boost::shared_ptr<ProjectInfo> makeProjectInfo(ProjectTypes projectType, PlayerTypes playerType);

    void streamProjectInfo(std::ostream& os, const boost::shared_ptr<ProjectInfo>& pProjectInfo);

    TechTypes getRequiredTech(const boost::shared_ptr<ProjectInfo>& pProjectInfo);

    bool couldConstructProject(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<ProjectInfo>& pProjectInfo);
}