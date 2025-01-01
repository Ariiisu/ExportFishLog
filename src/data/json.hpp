#pragma once

#include <glaze/json.hpp>
#include <glaze/core/macros.hpp>

namespace pastry_fish
{
    struct Main
    {
        std::vector<std::uint32_t> completed;

        GLZ_LOCAL_META(Main, completed);
    };
}
