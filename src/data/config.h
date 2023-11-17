#pragma once

#include <string>

namespace data
{
    struct config
    {
        struct signatures
        {
            std::string fishlog{};
            std::string spear_fishlog{};
            std::string object_table{};
        };

        void setup();

    private:
        signatures _signatures{};

    public:
        signatures signatures()
        {
            return _signatures;
        }
    };

    inline config config{};
}
