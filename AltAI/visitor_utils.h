#pragma once

#include "boost/variant.hpp"

namespace AltAI
{
    template <typename T>
        class IsVariantOfType : public boost::static_visitor<bool>
    {
    public:
        template <typename U> 
            bool operator() (const U&) const
        {
            return false;
        }

        template <>
            bool operator() (const T&) const
        {
            return true;
        }
    };
}