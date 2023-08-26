#pragma once

#include <cpr/cpr.h>

namespace data
{
    class network
    {
    public:
        void setup();

        std::unordered_map<std::uint32_t, std::uint32_t> get_fishlog_map()
        {
            return _fishlog_map;
        }

        std::unordered_map<std::uint32_t, std::uint32_t> get_spear_fishlog_map()
        {
            return _spear_fishlog_map;
        }

    private:
        cpr::Session _session{};
        std::unordered_map<std::uint32_t, std::uint32_t> _fishlog_map;
        std::unordered_map<std::uint32_t, std::uint32_t> _spear_fishlog_map;
    };

    inline network network{};
}
