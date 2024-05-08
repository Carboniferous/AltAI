#pragma once

#include "./player.h"

namespace AltAI
{
    class OpponentsAnalysis
    {
    public:
        explicit OpponentsAnalysis(Player& player);

    private:
        Player& player_;
    };
}